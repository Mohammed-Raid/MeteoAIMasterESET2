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
#include "capteurs.h"
#include "modele_ia.h"
#include "baseline_arbre.h"
#include "reseau.h"
#include "cloud_thingspeak.h"
#include "config_cloud.h"

#include "FreeRTOS.h"
#include "task.h"

#define PILE_TACHE_APP       4096   /* en mots : sockets + DNS + capteurs */
#define PRIORITE_TACHE_APP   2

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

static void float_vers_texte(float valeur, char *texte, int taille)
{
  int32_t centiemes;
  char signe = ' ';

  if (valeur >= 0.0f)
  {
    centiemes = (int32_t)(valeur * 100.0f + 0.5f);
  }
  else
  {
    centiemes = (int32_t)(valeur * 100.0f - 0.5f);
  }

  if (centiemes < 0)
  {
    signe = '-';
    centiemes = -centiemes;
  }

  snprintf(texte, taille, "%c%ld.%02ld", signe,
           (long)(centiemes / 100), (long)(centiemes % 100));
}

/* ============================================================================
 *  Tache applicative
 * ==========================================================================*/
static void tache_application(void *argument)
{
  static mesures_t mesures;      /* static : trop gros pour la pile d'une tache */
  static char talkback[64];

  float entrees[IA_NB_ENTREES];
  char texte[16];
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

  if (!cloud_est_configure())
  {
    printf("\r\n*** ThingSpeak non configure ***\r\n");
    printf("Creez Core/Inc/config_cloud_local.h avec votre cle d'ecriture.\r\n");
    printf("Voir Core/Inc/config_cloud.h pour le modele.\r\n");
    printf("La partie Cloud sera ignoree, l'inference locale continue.\r\n\r\n");
  }

  while (1)
  {
    /* --- 1. Acquisition : LED ROUGE ------------------------------------*/
    leds_eteindre_tout();
    led_allumer(COULEUR_ROUGE);
    printf("<Lecture Interface Capteurs>\r\n");
    capteurs_lire(&mesures);

    if (!mesures.sht40_temp_c.valide ||
        !mesures.sht40_humidite_rh.valide ||
        !mesures.df_pression_hpa.valide)
    {
      printf("  Capteurs incomplets, cycle ignore\r\n");
      leds_eteindre_tout();
      led_allumer(COULEUR_VERTE);
      vTaskDelay(pdMS_TO_TICKS(THINGSPEAK_PERIODE_MIN_MS));
      continue;
    }

    entrees[0] = mesures.sht40_temp_c.valeur;
    entrees[1] = mesures.sht40_humidite_rh.valeur;
    entrees[2] = mesures.df_pression_hpa.valeur;

    float_vers_texte(entrees[0], texte, sizeof(texte));
    printf("  T=%s degC", texte);
    float_vers_texte(entrees[1], texte, sizeof(texte));
    printf("  RH=%s %%", texte);
    float_vers_texte(entrees[2], texte, sizeof(texte));
    printf("  P=%s hPa\r\n", texte);

    /* --- 2. Inference embarquee ----------------------------------------*/
    {
      uint32_t debut = DWT->CYCCNT;
      classe_edge = ia_predire(entrees, NULL);
      cycles_edge = DWT->CYCCNT - debut;
    }
    classe_arbre = arbre_predire(entrees);

    /* --- 3. Cloud : LED BLEUE ------------------------------------------*/
    leds_eteindre_tout();
    led_allumer(COULEUR_BLEUE);
    printf("<Communication Reseau>\r\n");

    latence_envoi_ms = 0;
    latence_talkback_ms = 0;
    talkback[0] = '\0';

    code = cloud_envoyer_mesures(&mesures, classe_edge, &latence_envoi_ms);
    if (code == CLOUD_OK)
    {
      printf("  envoi ThingSpeak : ok en %lu ms\r\n",
             (unsigned long)latence_envoi_ms);

      code = cloud_lire_talkback(talkback, sizeof(talkback), &latence_talkback_ms);
      if (code == CLOUD_OK)
      {
        if (talkback[0] != '\0')
        {
          printf("  verdict Cloud    : %s (Talkback en %lu ms)\r\n",
                 talkback, (unsigned long)latence_talkback_ms);
        }
        else
        {
          printf("  Talkback vide    : le Cloud n'a pas encore repondu "
                 "(%lu ms)\r\n", (unsigned long)latence_talkback_ms);
        }
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

    /* --- 4. La comparaison, qui est l'objet du TP3 §3.c ----------------*/
    printf("<Comparaison Cloud / Edge>\r\n");

    float_vers_texte(cycles_vers_us(cycles_edge), texte, sizeof(texte));
    printf("  Edge   : %-22s %s us (%lu cycles)\r\n",
           ia_noms_classes[classe_edge], texte, (unsigned long)cycles_edge);

    printf("  Arbre  : %-22s (reference deterministe)\r\n",
           arbre_noms_classes[classe_arbre]);

    if (latence_envoi_ms > 0u)
    {
      uint32_t total_ms = latence_envoi_ms + latence_talkback_ms;
      uint32_t edge_us = (uint32_t)cycles_vers_us(cycles_edge);

      printf("  Cloud  : %-22s %lu ms (aller-retour complet)\r\n",
             (talkback[0] != '\0') ? talkback : "en attente",
             (unsigned long)total_ms);

      if (edge_us > 0u)
      {
        printf("  -> le Cloud est %lu fois plus lent que l'inference locale\r\n",
               (unsigned long)((total_ms * 1000u) / edge_us));
      }
    }

    /* --- 5. Attente : LED VERTE ----------------------------------------*/
    leds_eteindre_tout();
    led_allumer(COULEUR_VERTE);
    vTaskDelay(pdMS_TO_TICKS(THINGSPEAK_PERIODE_MIN_MS));
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
