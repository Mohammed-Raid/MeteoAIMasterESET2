/**
  ******************************************************************************
  * @file    partie5_cloud.c
  * @brief   TP3 - Cloud versus Edge AI : la comparaison.
  *
  *  C'est le coeur du TP3. La meme observation est classee deux fois, par le
  *  meme modele (TP3_Meteo_Colab.ipynb) :
  *    - en local, par meteo_ia.c (§3.b), chronometre au cycle pres ;
  *    - dans le Cloud : la carte envoie ses mesures et son resultat Edge sur
  *      ThingSpeak (§2.c), le script TP3_IA/matlab/ThingSpeak_MeteoAnalysis.m
  *      classe la mesure et depose "METEO:<classe>:<confiance>" dans la file
  *      Talkback, que la carte relit (§2.d).
  *
  *  Le verdict Cloud lu a un echange correspond au mieux a la mesure envoyee
  *  a l'echange precedent : le script MATLAB est declenche APRES l'ecriture.
  *  Il ne repond pas non plus a chaque envoi : le dernier verdict est donc
  *  garde et affiche avec son age.
  *
  *  L'arbre de decision deterministe tourne aussi, pour garder sous les yeux
  *  ce que couterait la solution la plus sobre.
  *
  *  LEDs : ROUGE = capteurs, BLEUE = communication reseau, VERTE = attente.
  ******************************************************************************
  */
#include "main.h"
#include "format.h"
#include "capteurs.h"
#include "meteo_ia.h"
#include "meteo_entrees.h"
#include "meteo_talkback.h"
#include "baseline_arbre.h"
#include "reseau.h"
#include "cloud_thingspeak.h"
#include "config_cloud.h"
#include "serveur_web.h"
#include "ethernetif.h"

#include "FreeRTOS.h"
#include "task.h"

#define PILE_TACHE_APP       4096   /* en mots : sockets + DNS + capteurs */
#define PRIORITE_TACHE_APP   2

/* La boucle locale est rapide, pour que le tableau de bord reste vivant.
   Les echanges avec ThingSpeak, eux, sont espaces : le compte gratuit
   refuse les ecritures separees de moins de 15 secondes. */
#define PERIODE_LOCALE_MS    2000
#define CYCLES_PAR_ENVOI     (THINGSPEAK_PERIODE_MIN_MS / PERIODE_LOCALE_MS)

#if ARBRE_NB_ENTREES != METEO_NB_FEATURES
#error "L'arbre de reference et le modele n'ont pas les memes entrees : relancer TP3_IA/baseline_arbre.py"
#endif

static float cycles_vers_us(uint32_t cycles)
{
  return (float)cycles * 1000000.0f / (float)SystemCoreClock;
}

/* Dernier verdict recu du Cloud. Le script MATLAB ne repond pas a chaque
   envoi : constate le 19/11/2026, des verdicts a presque tous les cycles,
   mais aussi des trous de plusieurs minutes juste apres la creation du
   React. La commande
   Talkback etant retiree de la file a la lecture, on garde le verdict et
   l'heure de sa reception, au lieu de l'effacer au cycle suivant. */
static int dernier_verdict_classe = -1;      /* -1 : aucun verdict recu */
static int dernier_verdict_confiance = 0;
static uint32_t dernier_verdict_tick = 0;

/* Libelle du dernier verdict avec son age, pour la console et le tableau
   de bord : "Clair (49 %), il y a 32 s". */
static void libelle_dernier_verdict(char *texte, int taille)
{
  uint32_t age_s;

  if (dernier_verdict_classe < 0)
  {
    snprintf(texte, taille, "en attente du premier verdict");
    return;
  }
  age_s = (HAL_GetTick() - dernier_verdict_tick) / 1000u;
  if (age_s < 120u)
  {
    snprintf(texte, taille, "%s (%d %%), il y a %lu s",
             METEO_CLASS_NAMES[dernier_verdict_classe],
             dernier_verdict_confiance, (unsigned long)age_s);
  }
  else
  {
    snprintf(texte, taille, "%s (%d %%), il y a %lu min",
             METEO_CLASS_NAMES[dernier_verdict_classe],
             dernier_verdict_confiance, (unsigned long)(age_s / 60u));
  }
}

/* Affiche une erreur Cloud, avec le statut HTTP quand le serveur a repondu :
   une cle invalide (400 ou 401) et une ecriture trop rapide (200, refusee)
   ne se corrigent pas de la meme facon. */
static void afficher_erreur_cloud(const char *quoi, int code)
{
  int statut = cloud_dernier_statut_http();

  printf("  %-16s : %s", quoi, cloud_message_erreur(code));
  if (statut != 0)
  {
    printf(" (statut HTTP %d)", statut);
  }
  printf("\r\n");
  if (cloud_derniere_redirection()[0] != '\0')
  {
    printf("    redirige vers : %s\r\n", cloud_derniere_redirection());
  }
}

/* ============================================================================
 *  Tache applicative
 * ==========================================================================*/
static void tache_application(void *argument)
{
  /* static : ces structures pesent plusieurs centaines d'octets, trop pour
     la pile d'une tache FreeRTOS. Une seule tache les utilise. */
  static mesures_t mesures;
  static etat_systeme_t etat;
  /* Meme taille que le champ du tableau de bord : lire plus que ce qu'on
     peut stocker n'aurait aucun sens. */
  static char talkback[TAILLE_LIBELLE_CLOUD];

  meteo_entrees_info_t info;
  float entrees[METEO_NB_FEATURES];
  float probabilites[METEO_NB_CLASSES];
  char texte[16];
  uint32_t compteur = 0;
  int classe_edge;
  int classe_arbre;
  int classe_cloud;
  int confiance_cloud;
  uint32_t duree_edge_us;
  uint32_t cycles_edge;
  uint32_t debut;
  uint32_t latence_envoi_ms;
  uint32_t latence_talkback_ms;
  int code;

  (void)argument;

  printf("<debut d'application>\r\n");

  /* --- Capteurs et modele ----------------------------------------------*/
  leds_eteindre_tout();
  led_allumer(COULEUR_ROUGE);
  capteurs_init();
  led_eteindre(COULEUR_ROUGE);

  /* Initialise aussi le compteur de cycles DWT */
  if (meteo_ia_init() != 0)
  {
    printf("Reseau de neurones indisponible : arret\r\n");
    Error_Handler();
  }
  meteo_ia_autotest();

  /* --- Reseau ----------------------------------------------------------*/
  reseau_init();
  reseau_attendre_adresse_ip();

  etat.cloud_configure = (uint8_t)cloud_est_configure();

  if (!cloud_est_configure())
  {
    printf("\r\n*** ThingSpeak non configure ***\r\n");
    printf("Creez Core/Inc/config_cloud_local.h avec votre cle d'ecriture.\r\n");
    printf("Voir Core/Inc/config_cloud.h pour le modele.\r\n");
    printf("La partie Cloud sera ignoree, l'inference locale continue.\r\n\r\n");
  }

  /* Tableau de bord : la page est servie meme sans acces Internet */
  if (serveur_web_demarrer() != 0)
  {
    printf("Serveur web : demarrage impossible\r\n");
  }

  while (1)
  {
    int cycle_cloud = ((compteur % CYCLES_PAR_ENVOI) == 0u);

    compteur++;

    /* --- 1. Acquisition : LED ROUGE ------------------------------------*/
    leds_eteindre_tout();
    led_allumer(COULEUR_ROUGE);
    capteurs_lire(&mesures);
    etat.mesures = mesures;

    if (!meteo_entrees_construire(&mesures, HAL_GetTick(), entrees, &info))
    {
      printf("  Capteurs incomplets, cycle ignore\r\n");
      etat.classe_edge = NULL;
      etat.classe_arbre = NULL;
      serveur_web_publier(&etat);
      leds_eteindre_tout();
      led_allumer(COULEUR_VERTE);
      vTaskDelay(pdMS_TO_TICKS(PERIODE_LOCALE_MS));
      continue;
    }

    /* --- 2. Inference embarquee ----------------------------------------*/
    debut = DWT->CYCCNT;
    if (meteo_ia_classer(entrees, probabilites, &classe_edge, &duree_edge_us) != 0)
    {
      printf("  Erreur d'inference, cycle ignore\r\n");
      vTaskDelay(pdMS_TO_TICKS(PERIODE_LOCALE_MS));
      continue;
    }
    cycles_edge = DWT->CYCCNT - debut;
    classe_arbre = arbre_predire(entrees);

    etat.classe_edge = METEO_CLASS_NAMES[classe_edge];
    etat.cycles_edge = cycles_edge;
    etat.duree_edge_us = cycles_vers_us(cycles_edge);
    etat.classe_arbre = METEO_CLASS_NAMES[classe_arbre];
    etat.nb_cycles++;
    if (classe_edge == classe_arbre)
    {
      etat.nb_accords_arbre++;
    }

    format_float(entrees[0], texte, sizeof(texte));
    printf("T=%s", texte);
    format_float(entrees[1], texte, sizeof(texte));
    printf("  RH=%s", texte);
    format_float(info.pression_mer_hpa, texte, sizeof(texte));
    printf("  Pmer=%s", texte);
    format_float(info.tendance_3h_hpa, texte, sizeof(texte));
    printf("  dP3h=%s%s", texte, info.tendance_disponible ? "" : "*");
    format_float(etat.duree_edge_us, texte, sizeof(texte));
    printf("  Edge: %-8s %s us", METEO_CLASS_NAMES[classe_edge], texte);

    /* Sante du pool de reception zero-copie. Si "libres" tombe a 0 et que
       "penurie" passe a 1, la carte ne recoit plus rien : le DMA n'a plus de
       buffer et aucun n'est rendu. */
    printf("  | RX libres %u%s\r\n",
           ethernetif_buffers_rx_libres(),
           ethernetif_penurie_rx() ? " PENURIE" : "");

    /* --- 3. Cloud : LED BLEUE ------------------------------------------
       Espace dans le temps : le compte ThingSpeak gratuit refuse les
       ecritures separees de moins de 15 secondes. La boucle locale, elle,
       reste rapide pour que le tableau de bord soit vivant. */
    if (cycle_cloud)
    {
      leds_eteindre_tout();
      led_allumer(COULEUR_BLEUE);
      printf("<Communication Reseau>\r\n");
      if (!info.tendance_disponible)
      {
        printf("  (* dP3h vaut 0 tant que 3 h de pression ne sont pas accumulees)\r\n");
      }

      latence_envoi_ms = 0;
      latence_talkback_ms = 0;
      talkback[0] = '\0';
      etat.cloud_joignable = 0;

      code = cloud_envoyer_mesures(&mesures, classe_edge,
                                   (int)(probabilites[classe_edge] * 100.0f + 0.5f),
                                   duree_edge_us, &latence_envoi_ms);
      if (code == CLOUD_OK)
      {
        printf("  envoi ThingSpeak : ok en %lu ms\r\n",
               (unsigned long)latence_envoi_ms);

        code = cloud_lire_talkback(talkback, sizeof(talkback),
                                   &latence_talkback_ms);
        if (code == CLOUD_OK)
        {
          etat.cloud_joignable = 1;
          etat.latence_cloud_ms = latence_envoi_ms + latence_talkback_ms;

          /* Commande deposee par le script MATLAB : "METEO:<classe>:<conf>".
             Le test de bornes protege METEO_CLASS_NAMES d'une commande
             mal formee. */
          if (meteo_talkback_lire(talkback, &classe_cloud, &confiance_cloud) &&
              (classe_cloud >= 0) && (classe_cloud < METEO_NB_CLASSES))
          {
            dernier_verdict_classe = classe_cloud;
            dernier_verdict_confiance = confiance_cloud;
            dernier_verdict_tick = HAL_GetTick();
            printf("[CLOUD] %s (%d %%), nouveau verdict du script MATLAB\r\n",
                   METEO_CLASS_NAMES[classe_cloud], confiance_cloud);
          }
          else
          {
            printf("  Talkback vide : le script MATLAB n'a pas encore repondu\r\n");
          }
          /* Dans les deux cas, on affiche le dernier verdict connu */
          libelle_dernier_verdict(etat.classe_cloud, sizeof(etat.classe_cloud));
        }
        else
        {
          afficher_erreur_cloud("Talkback", code);
        }
      }
      else
      {
        afficher_erreur_cloud("envoi ThingSpeak", code);
      }

      /* --- 4. La comparaison, objet du TP3 §3.c ------------------------*/
      printf("<Comparaison Cloud / Edge>\r\n");

      format_float(etat.duree_edge_us, texte, sizeof(texte));
      printf("  Edge   : %-8s %s us (%lu cycles)\r\n",
             METEO_CLASS_NAMES[classe_edge], texte, (unsigned long)cycles_edge);
      printf("  Arbre  : %-8s (reference deterministe)\r\n",
             METEO_CLASS_NAMES[classe_arbre]);

      if (etat.cloud_joignable)
      {
        printf("  Cloud  : %s, aller-retour HTTP %lu ms\r\n",
               etat.classe_cloud, (unsigned long)etat.latence_cloud_ms);
      }
    }

    /* --- 5. Publication et attente : LED VERTE -------------------------*/
    serveur_web_publier(&etat);
    leds_eteindre_tout();
    led_allumer(COULEUR_VERTE);
    vTaskDelay(pdMS_TO_TICKS(PERIODE_LOCALE_MS));
  }
}

void partie5_lancer(void)
{
  if (xTaskCreate(tache_application, "AppCloud", PILE_TACHE_APP, NULL,
                  PRIORITE_TACHE_APP, NULL) != pdPASS)
  {
    printf("Creation de la tache AppCloud impossible\r\n");
    Error_Handler();
  }

  vTaskStartScheduler();

  Error_Handler();
}
