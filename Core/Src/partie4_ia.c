/**
  ******************************************************************************
  * @file    partie4_ia.c
  * @brief   TP3 §3 - IA embarquee sur la carte STM32N6.
  *
  *  Le modele est celui du notebook TP3_Meteo_Colab.ipynb : ses constantes
  *  sont dans meteo_model_params.h, et il est execute par meteo_ia.c (avec
  *  X-CUBE-AI quand son code est present, sinon avec le code C genere par
  *  TP3_IA/generer_c.py).
  *
  *  §3.b : execution du reseau en mode inference.
  *  §3.c : inference sur les donnees reelles des capteurs, avec la duree.
  *  §3.d : programme temoin (TEMOIN_SANS_IA a 1 dans main.h), qui execute
  *         tout sauf le reseau, et mode de mesure du courant (MESURE_CONSO).
  *
  *  L'arbre de decision de reference tourne a cote du reseau, sur les memes
  *  entrees : il repond a la question du polycopie, "l'IA apporte-t-elle un
  *  benefice clair face a une methode deterministe ?".
  *
  *  LEDs : ROUGE = lecture des capteurs, BLEUE = inference, VERTE = attente.
  ******************************************************************************
  */
#include "main.h"
#include "format.h"
#include "capteurs.h"
#include "meteo_ia.h"
#include "meteo_entrees.h"
#include "baseline_arbre.h"

#define PERIODE_CYCLE_MS   2000

#if ARBRE_NB_ENTREES != METEO_NB_FEATURES
#error "L'arbre de reference et le modele n'ont pas les memes entrees : relancer TP3_IA/baseline_arbre.py"
#endif

#if MESURE_CONSO
/* ============================================================================
 *  TP3 §3.d : mesure du courant d'alimentation
 *
 *  Une classification dure quelques dizaines de microsecondes : aucun
 *  instrument ne la voit passer. On la repete donc NB_REPETITIONS fois
 *  d'affilee (la rafale), puis on laisse la carte au repos REPOS_MS, ce qui
 *  donne un niveau de reference bien visible sur le voltmetre.
 *
 *  Les deux programmes executent la meme boucle : le programme avec IA, et
 *  le temoin (TEMOIN_SANS_IA a 1) qui fait tout - normalisation, copie,
 *  argmax, chronometrage - sauf le reseau. Leur rafale n'a donc pas la meme
 *  duree, et on compare les energies totales :
 *
 *    E_classification = (I_ia x T_ia - I_temoin x T_temoin) x V / NB_REPETITIONS
 *
 *  La duree de chaque rafale est affichee pendant le repos, jamais pendant
 *  la mesure. Les LEDs sont eteintes et les capteurs lus une seule fois,
 *  avant la premiere rafale : ils ne font pas partie de ce qu'on mesure.
 * ==========================================================================*/
#define NB_REPETITIONS   100000u
#define REPOS_MS         10000u

static void boucle_mesure_consommation(void)
{
  mesures_t mesures;
  float entrees[METEO_NB_FEATURES];
  float probabilites[METEO_NB_CLASSES];
  int classe;
  uint32_t duree_us;
  uint32_t debut;
  uint32_t duree_ms;
  uint32_t i;

  leds_eteindre_tout();

  /* Entrees figees pour toute la mesure. A defaut de capteurs valides, le
     premier vecteur de test du notebook fait l'affaire : seule compte
     l'egalite des entrees entre les deux programmes. */
  capteurs_lire(&mesures);
  if (!meteo_entrees_construire(&mesures, HAL_GetTick(), entrees, NULL))
  {
    for (i = 0; i < METEO_NB_FEATURES; i++)
    {
      entrees[i] = METEO_TEST_IN[0][i];
    }
  }

  printf("MODE MESURE DE CONSOMMATION (TP3 3.d)\r\n");
#if TEMOIN_SANS_IA
  printf("  programme TEMOIN : tout sauf le reseau de neurones\r\n");
#else
  printf("  programme AVEC IA\r\n");
#endif
  printf("  rafales de %lu classifications, repos de %lu ms, LEDs eteintes\r\n",
         (unsigned long)NB_REPETITIONS, (unsigned long)REPOS_MS);

  while (1)
  {
    debut = HAL_GetTick();
    for (i = 0; i < NB_REPETITIONS; i++)
    {
      (void)meteo_ia_classer(entrees, probabilites, &classe, &duree_us);
    }
    duree_ms = HAL_GetTick() - debut;

    printf("  rafale : %lu ms, soit %lu ns par classification\r\n",
           (unsigned long)duree_ms,
           (unsigned long)((uint64_t)duree_ms * 1000000u / NB_REPETITIONS));
    HAL_Delay(REPOS_MS);
  }
}
#endif /* MESURE_CONSO */

/* ============================================================================
 *  Boucle principale
 * ==========================================================================*/
void partie4_lancer(void)
{
  mesures_t mesures;
  meteo_entrees_info_t info;
  float entrees[METEO_NB_FEATURES];
  float probabilites[METEO_NB_CLASSES];
  char texte[16];
  int classe;
  int classe_arbre;
  uint32_t duree_us;
  uint32_t cycles_ia;
  uint32_t cycles_arbre;
  uint32_t debut;
  int i;

  printf("<debut d'application>\r\n");

  leds_eteindre_tout();
  led_allumer(COULEUR_ROUGE);
  capteurs_init();

  /* Initialise aussi le compteur de cycles DWT */
  if (meteo_ia_init() != 0)
  {
    printf("Reseau de neurones indisponible : arret\r\n");
    Error_Handler();
  }

#if MESURE_CONSO
  boucle_mesure_consommation();   /* ne revient jamais */
#endif

#if TEMOIN_SANS_IA
  /* L'autotest execute le reseau : il n'a pas sa place dans le temoin */
  printf("MODE TEMOIN : inference desactivee (TP3 3.d)\r\n");
#else
  /* Vecteurs de test du notebook : la carte doit retrouver les probabilites
     de Keras. C'est la preuve que le modele embarque est le bon. */
  meteo_ia_autotest();
#endif

  while (1)
  {
    /* --- Lecture des capteurs : LED ROUGE --------------------------------*/
    leds_eteindre_tout();
    led_allumer(COULEUR_ROUGE);
    printf("<Lecture Interface Capteurs>\r\n");
    capteurs_lire(&mesures);

    if (!meteo_entrees_construire(&mesures, HAL_GetTick(), entrees, &info))
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
    format_float(info.pression_mesuree_hpa, texte, sizeof(texte));
    printf("  P mesuree=%s", texte);
    format_float(info.pression_mer_hpa, texte, sizeof(texte));
    printf("  P mer=%s hPa", texte);
    format_float(info.tendance_3h_hpa, texte, sizeof(texte));
    printf("  dP3h=%s%s\r\n", texte,
           info.tendance_disponible ? "" : " (moins de 3 h d'historique)");

    /* --- Inference : LED BLEUE -------------------------------------------*/
    leds_eteindre_tout();
    led_allumer(COULEUR_BLEUE);

#if TEMOIN_SANS_IA
    printf("<Temoin : inference non executee>\r\n");
    (void)probabilites; (void)classe; (void)classe_arbre; (void)duree_us;
    (void)cycles_ia; (void)cycles_arbre; (void)debut; (void)i;
#else
    debut = DWT->CYCCNT;
    if (meteo_ia_classer(entrees, probabilites, &classe, &duree_us) != 0)
    {
      printf("  Erreur d'inference\r\n");
      attendre_ms(PERIODE_CYCLE_MS);
      continue;
    }
    cycles_ia = DWT->CYCCNT - debut;

    printf("<Inference IA> classe = %s (reseau seul : %lu us)\r\n",
           METEO_CLASS_NAMES[classe], (unsigned long)duree_us);
    for (i = 0; i < METEO_NB_CLASSES; i++)
    {
      format_float(probabilites[i] * 100.0f, texte, sizeof(texte));
      printf("    %-16s %s %%\r\n", METEO_CLASS_NAMES[i], texte);
    }

    /* --- Reference deterministe, sur les memes entrees ----------------- */
    debut = DWT->CYCCNT;
    classe_arbre = arbre_predire(entrees);
    cycles_arbre = DWT->CYCCNT - debut;

    printf("<Reference arbre> classe = %s\r\n", METEO_CLASS_NAMES[classe_arbre]);
    printf("    cycles : reseau %lu, arbre %lu",
           (unsigned long)cycles_ia, (unsigned long)cycles_arbre);
    if (cycles_arbre > 0u)
    {
      printf(" -> le reseau coute %lu fois plus", (unsigned long)(cycles_ia / cycles_arbre));
    }
    printf("\r\n    accord des deux methodes : %s\r\n",
           (classe_arbre == classe) ? "oui" : "non");
#endif

    /* --- Attente : LED VERTE ---------------------------------------------*/
    leds_eteindre_tout();
    led_allumer(COULEUR_VERTE);
    attendre_ms(PERIODE_CYCLE_MS);
  }
}
