/**
  ******************************************************************************
  * @file    partie5_cloud.c
  * @brief   TP3 - Cloud versus Edge AI : la comparaison.
  *
  *  C'est le coeur du TP3. A chaque cycle, la meme observation est classee
  *  deux fois :
  *    - en local, par le reseau embarque (§3.b), chronometre au cycle pres ;
  *    - dans le Cloud, en envoyant les mesures sur ThingSpeak (§2.c) et en
  *      relisant le verdict par l'API Talkback (§2.d), chronometre en
  *      millisecondes.
  *
  *  Les deux latences sont affichees cote a cote : c'est la reponse au §3.c.
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
#include "modele_ia.h"
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

/* ============================================================================
 *  Outils
 * ==========================================================================*/
static void dwt_demarrer(void)
{
  DCB->DEMCR |= DCB_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static float cycles_vers_us(uint32_t cycles)
{
  return (float)cycles * 1000000.0f / (float)SystemCoreClock;
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

  float entrees[IA_NB_ENTREES];
  char texte[16];
  uint32_t compteur = 0;
  int classe_edge;
  int classe_arbre;
  uint32_t cycles_edge;
  uint32_t latence_envoi_ms;
  uint32_t latence_talkback_ms;
  int code;

  (void)argument;

  printf("<debut d'application>\r\n");
  dwt_demarrer();

  /* --- Capteurs --------------------------------------------------------*/
  leds_eteindre_tout();
  led_allumer(COULEUR_ROUGE);
  capteurs_init();
  led_eteindre(COULEUR_ROUGE);

  if (ia_autotest() == 0)
  {
    printf("Autotest du modele : OK (portage C fidele a l'ONNX)\r\n");
  }
  else
  {
    printf("Autotest du modele : ECHEC - portage incorrect !\r\n");
  }

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

    if (!mesures.sht40_temp_c.valide ||
        !mesures.sht40_humidite_rh.valide ||
        !mesures.df_pression_hpa.valide)
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

    entrees[0] = mesures.sht40_temp_c.valeur;
    entrees[1] = mesures.sht40_humidite_rh.valeur;
    entrees[2] = mesures.df_pression_hpa.valeur;

    /* --- 2. Inference embarquee ----------------------------------------*/
    {
      uint32_t debut = DWT->CYCCNT;
      classe_edge = ia_predire(entrees, NULL);
      cycles_edge = DWT->CYCCNT - debut;
    }
    classe_arbre = arbre_predire(entrees);

    etat.classe_edge = ia_noms_classes[classe_edge];
    etat.cycles_edge = cycles_edge;
    etat.duree_edge_us = cycles_vers_us(cycles_edge);
    etat.classe_arbre = arbre_noms_classes[classe_arbre];
    etat.nb_cycles++;
    if (classe_edge == classe_arbre)
    {
      etat.nb_accords_arbre++;
    }

    format_float(entrees[0], texte, sizeof(texte));
    printf("T=%s", texte);
    format_float(entrees[1], texte, sizeof(texte));
    printf("  RH=%s", texte);
    format_float(entrees[2], texte, sizeof(texte));
    printf("  P=%s", texte);
    format_float(etat.duree_edge_us, texte, sizeof(texte));
    printf("  Edge: %-22s %s us", ia_noms_classes[classe_edge], texte);

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

      latence_envoi_ms = 0;
      latence_talkback_ms = 0;
      talkback[0] = '\0';
      etat.cloud_joignable = 0;

      code = cloud_envoyer_mesures(&mesures, classe_edge, &latence_envoi_ms);
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
          snprintf(etat.classe_cloud, sizeof(etat.classe_cloud), "%s",
                   (talkback[0] != '\0') ? talkback : "en attente");

          printf("  verdict Cloud    : %s (aller-retour %lu ms)\r\n",
                 etat.classe_cloud, (unsigned long)etat.latence_cloud_ms);
        }
        else
        {
          printf("  Talkback         : %s\r\n", cloud_message_erreur(code));
        }
      }
      else
      {
        printf("  envoi ThingSpeak : %s\r\n", cloud_message_erreur(code));
      }

      /* --- 4. La comparaison, objet du TP3 §3.c ------------------------*/
      printf("<Comparaison Cloud / Edge>\r\n");

      format_float(etat.duree_edge_us, texte, sizeof(texte));
      printf("  Edge   : %-22s %s us (%lu cycles)\r\n",
             ia_noms_classes[classe_edge], texte, (unsigned long)cycles_edge);
      printf("  Arbre  : %-22s (reference deterministe)\r\n",
             arbre_noms_classes[classe_arbre]);

      if (etat.cloud_joignable && (etat.duree_edge_us > 0.0f))
      {
        printf("  Cloud  : %-22s %lu ms\r\n", etat.classe_cloud,
               (unsigned long)etat.latence_cloud_ms);
        printf("  -> le Cloud est %lu fois plus lent que l'inference locale\r\n",
               (unsigned long)((float)etat.latence_cloud_ms * 1000.0f
                               / etat.duree_edge_us));
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
