/**
  ******************************************************************************
  * @file    partie4_ia.c
  * @brief   TP3 - IA embarquee sur la carte STM32N6.
  *
  *  §3.b : execution du reseau de neurones en mode inference.
  *  §3.c : inference a partir des donnees reelles des capteurs de la carte
  *         fille, avec mesure du temps d'execution pour la comparaison avec
  *         la solution Cloud.
  *  §3.d : un programme temoin, qui execute exactement les memes etapes SANS
  *         l'inference, permet d'isoler la consommation du reseau. Il
  *         s'obtient en compilant avec -DTEMOIN_SANS_IA=1.
  *
  *  Entrees du modele : temperature et humidite du SHT40AD1B, pression du
  *  LPS22DF. Le SHT40 fournit les deux premieres, ce qui evite de melanger
  *  des capteurs dont les biais different.
  *
  *  LEDs : ROUGE = lecture des capteurs, BLEUE = inference, VERTE = attente.
  ******************************************************************************
  */
#include "main.h"
#include "format.h"
#include "capteurs.h"
#include "modele_ia.h"
#include "baseline_arbre.h"

#define PERIODE_CYCLE_MS   2000

/* TP3 §3.d : programme temoin, identique mais sans l'appel au reseau */
#ifndef TEMOIN_SANS_IA
#define TEMOIN_SANS_IA   0
#endif

/* ============================================================================
 *  Compteur de cycles DWT : mesure du temps d'execution (§3.c)
 * ==========================================================================*/
static void dwt_demarrer(void)
{
  DCB->DEMCR |= DCB_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

#if !TEMOIN_SANS_IA
/* Convertit un nombre de cycles en microsecondes.
   Le resultat est un flottant : l'arbre de decision s'execute en moins
   d'une microseconde, un entier afficherait "0 us" et donnerait l'impression
   d'une mesure ratee. */
static float cycles_vers_us(uint32_t cycles)
{
  return (float)cycles * 1000000.0f / (float)SystemCoreClock;
}
#endif

/* ============================================================================
 *  Affichage des flottants sans _printf_float
 *  (meme approche que dans capteurs.c, volontairement locale a ce fichier)
 * ==========================================================================*/

/* ============================================================================
 *  Boucle principale
 * ==========================================================================*/
void partie4_lancer(void)
{
  mesures_t mesures;
  float entrees[IA_NB_ENTREES];
  char texte[16];

  printf("<debut d'application>\r\n");

  dwt_demarrer();

  leds_eteindre_tout();
  led_allumer(COULEUR_ROUGE);
  capteurs_init();

#if TEMOIN_SANS_IA
  /* L'autotest execute lui aussi le reseau : on ne le lance pas en mode
     temoin, sinon le programme ne serait plus exempt d'inference. */
  printf("MODE TEMOIN : inference desactivee (TP3 §3.d)\r\n");
#else
  /* Verification du portage : les vecteurs de reference ont ete calcules par
     onnxruntime sur PC. Si cet autotest echoue, les poids ou la
     normalisation ne correspondent pas au modele entraine. */
  {
    int echecs = ia_autotest();

    if (echecs == 0)
    {
      printf("Autotest du modele : OK (portage C fidele a l'ONNX)\r\n");
    }
    else
    {
      printf("Autotest du modele : %d ECHEC(S) - portage incorrect !\r\n", echecs);
    }
  }
#endif

  printf("Modele : %d entrees, %d classes\r\n", IA_NB_ENTREES, IA_NB_CLASSES);

  while (1)
  {
    /* --- Lecture des capteurs : LED ROUGE --------------------------------*/
    leds_eteindre_tout();
    led_allumer(COULEUR_ROUGE);
    printf("<Lecture Interface Capteurs>\r\n");
    capteurs_lire(&mesures);

    /* Le modele attend temperature, humidite, pression dans cet ordre */
    entrees[0] = mesures.sht40_temp_c.valeur;
    entrees[1] = mesures.sht40_humidite_rh.valeur;
    entrees[2] = mesures.df_pression_hpa.valeur;

    if (!mesures.sht40_temp_c.valide ||
        !mesures.sht40_humidite_rh.valide ||
        !mesures.df_pression_hpa.valide)
    {
      printf("  Capteurs incomplets, pas de classification ce cycle\r\n");
      leds_eteindre_tout();
      led_allumer(COULEUR_VERTE);
      attendre_ms(PERIODE_CYCLE_MS);
      continue;
    }

    format_float(entrees[0], texte, sizeof(texte));
    printf("  entrees : T=%s degC", texte);
    format_float(entrees[1], texte, sizeof(texte));
    printf("  RH=%s %%", texte);
    format_float(entrees[2], texte, sizeof(texte));
    printf("  P=%s hPa\r\n", texte);

    /* --- Inference : LED BLEUE -------------------------------------------*/
    leds_eteindre_tout();
    led_allumer(COULEUR_BLEUE);

#if TEMOIN_SANS_IA
    /* Programme temoin du §3.d : on ne lance pas le reseau. Tout le reste
       du cycle est rigoureusement identique, de sorte que la difference de
       courant mesuree au jumper n'est imputable qu'a l'inference. */
    printf("<Temoin : inference non executee>\r\n");
#else
    {
      float probabilites[IA_NB_CLASSES];
      int i;
      uint32_t debut = DWT->CYCCNT;
      int classe = ia_predire(entrees, probabilites);
      uint32_t cycles = DWT->CYCCNT - debut;

      printf("<Inference IA> classe = %s\r\n", ia_noms_classes[classe]);

      for (i = 0; i < IA_NB_CLASSES; i++)
      {
        format_float(probabilites[i] * 100.0f, texte, sizeof(texte));
        printf("    %-24s %s %%\r\n", ia_noms_classes[i], texte);
      }

      format_float(cycles_vers_us(cycles), texte, sizeof(texte));
      printf("    duree : %lu cycles = %s us\r\n", (unsigned long)cycles, texte);

      /* --- Reference deterministe -----------------------------------------
         Le polycopie du module (§2.5) pose la question : le reseau apporte-t-il
         un benefice clair face a une methode deterministe ? On ne repond pas
         par une opinion, on mesure. L'arbre resout le meme probleme, entraine
         sur les memes donnees et evalue sur le meme jeu de test, avec au plus
         5 comparaisons de flottants contre 208 MAC et 4 exponentielles.
         Ecart d'exactitude mesure hors ligne : 0,0031 point en faveur du
         reseau. */
      {
        uint32_t debut_arbre = DWT->CYCCNT;
        int classe_arbre = arbre_predire(entrees);
        uint32_t cycles_arbre = DWT->CYCCNT - debut_arbre;

        printf("<Reference arbre> classe = %s\r\n",
               arbre_noms_classes[classe_arbre]);
        format_float(cycles_vers_us(cycles_arbre), texte, sizeof(texte));
        printf("    duree : %lu cycles = %s us\r\n",
               (unsigned long)cycles_arbre, texte);

        if (cycles_arbre > 0u)
        {
          printf("    le reseau coute %lu fois plus de cycles\r\n",
                 (unsigned long)(cycles / cycles_arbre));
        }
        printf("    accord des deux methodes : %s\r\n",
               (classe_arbre == classe) ? "oui" : "non");
      }
    }
#endif

    /* --- Attente : LED VERTE ---------------------------------------------*/
    leds_eteindre_tout();
    led_allumer(COULEUR_VERTE);
    attendre_ms(PERIODE_CYCLE_MS);
  }
}
