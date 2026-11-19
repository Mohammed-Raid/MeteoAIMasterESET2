/* meteo_ia.c - Classification météo sur STM32N6 (TP3 ETRS923) */
#include <stdio.h>
#include <math.h>
#include "meteo_ia.h"

/* Deux moteurs d'inference possibles pour le meme modele :
 *  - X-CUBE-AI, des que son code genere (network.h) est dans le projet ;
 *  - sinon le code C genere par TP3_IA/generer_c.py (modele_ia.h).
 * Normalisation, chronometrage et autotest sont communs aux deux. */
#if defined(__has_include)
#  if __has_include("network.h")
#    define METEO_AVEC_XCUBEAI 1
#  endif
#endif
#ifndef METEO_AVEC_XCUBEAI
#  define METEO_AVEC_XCUBEAI 0
#endif

#if METEO_AVEC_XCUBEAI
#include "network.h"          /* généré par X-CUBE-AI */
#include "network_data.h"     /* généré par X-CUBE-AI */
#else
#include "modele_ia.h"        /* généré par TP3_IA/generer_c.py */
#endif
#include "main.h"             /* SystemCoreClock, registres CMSIS (DWT) */

/* Mettre 0 pour le programme témoin de la mesure de consommation (partie 3.d) :
 * tout est exécuté (normalisation, copie, argmax) SAUF le réseau de neurones. */
#ifndef AVEC_IA
#define AVEC_IA 1
#endif

/* =====================================================================
 * 1. Pression ramenée au niveau de la mer
 * ===================================================================== */
float meteo_pression_mer(float p_mesuree_hpa, float temp_c, float altitude_m)
{
    float k = 0.0065f * altitude_m;
    float rapport = 1.0f - k / (temp_c + k + 273.15f);
    return p_mesuree_hpa * powf(rapport, -5.257f);
}

/* =====================================================================
 * 2. Historique de pression pour la tendance sur 3 h
 *    Une valeur toutes les 10 min, 19 cases = 3 h + la valeur actuelle.
 * ===================================================================== */
#define HIST_PERIODE_MS  (10UL * 60UL * 1000UL)
#define HIST_TAILLE      19

static float    hist_pression[HIST_TAILLE];
static int      hist_nb = 0;            /* nombre de cases remplies */
static int      hist_pos = 0;           /* prochaine case à écrire */
static uint32_t hist_dernier_ms = 0;

void meteo_historique_ajouter(float p_mer_hpa, uint32_t maintenant_ms)
{
    if (hist_nb > 0 && (maintenant_ms - hist_dernier_ms) < HIST_PERIODE_MS) {
        return;                         /* pas encore 10 min depuis la dernière valeur */
    }
    hist_pression[hist_pos] = p_mer_hpa;
    hist_pos = hist_pos + 1;
    if (hist_pos == HIST_TAILLE) {
        hist_pos = 0;                   /* buffer circulaire */
    }
    if (hist_nb < HIST_TAILLE) {
        hist_nb = hist_nb + 1;
    }
    hist_dernier_ms = maintenant_ms;
}

int meteo_tendance_3h(float *dp3h)
{
    if (hist_nb < HIST_TAILLE) {
        *dp3h = 0.0f;                   /* moins de 3 h de mesures */
        return 0;
    }
    int idx_recent = hist_pos - 1;
    if (idx_recent < 0) {
        idx_recent = HIST_TAILLE - 1;
    }
    int idx_ancien = hist_pos;          /* la plus ancienne = la prochaine à écraser */
    *dp3h = hist_pression[idx_recent] - hist_pression[idx_ancien];
    return 1;
}

/* =====================================================================
 * 3. Réseau de neurones (API X-CUBE-AI "legacy")
 * ===================================================================== */
#if METEO_AVEC_XCUBEAI
static ai_handle  reseau = AI_HANDLE_NULL;
AI_ALIGNED(32) static ai_u8    activations[AI_NETWORK_DATA_ACTIVATIONS_SIZE];
AI_ALIGNED(32) static ai_float entree_ia[AI_NETWORK_IN_1_SIZE];
AI_ALIGNED(32) static ai_float sortie_ia[AI_NETWORK_OUT_1_SIZE];
static ai_buffer *ai_entree;
static ai_buffer *ai_sortie;
#else
static float entree_ia[IA_NB_ENTREES];
static float sortie_ia[IA_NB_CLASSES];
#endif

static void compteur_cycles_init(void)
{
    DCB->DEMCR |= DCB_DEMCR_TRCENA_Msk;     /* active le bloc de trace */
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;    /* démarre le compteur de cycles CPU */
}

int meteo_ia_init(void)
{
#if !METEO_AVEC_XCUBEAI
    /* Code C genere : meme controle de coherence avec meteo_model_params.h */
    if (IA_NB_ENTREES != METEO_NB_FEATURES || IA_NB_CLASSES != METEO_NB_CLASSES) {
        printf("[IA] ERREUR : modele_ia.h et meteo_model_params.h ne correspondent pas\r\n");
        return -1;
    }
    compteur_cycles_init();
    printf("[IA] Reseau pret (code C genere, sans X-CUBE-AI) : %d entrees, %d classes\r\n",
           IA_NB_ENTREES, IA_NB_CLASSES);
    return 0;
#else
    if (AI_NETWORK_IN_1_SIZE != METEO_NB_FEATURES || AI_NETWORK_OUT_1_SIZE != METEO_NB_CLASSES) {
        printf("[IA] ERREUR : modele X-CUBE-AI et meteo_model_params.h ne correspondent pas\r\n");
        return -1;
    }
    const ai_handle acts[] = { activations };
    ai_error err = ai_network_create_and_init(&reseau, acts, NULL);
    if (err.type != AI_ERROR_NONE) {
        printf("[IA] ERREUR init : type=%d code=%d\r\n", err.type, err.code);
        return -1;
    }
    ai_entree = ai_network_inputs_get(reseau, NULL);
    ai_sortie = ai_network_outputs_get(reseau, NULL);
    compteur_cycles_init();
    printf("[IA] Reseau pret : %d entrees, %d classes, %d octets d'activations\r\n",
           (int)AI_NETWORK_IN_1_SIZE, (int)AI_NETWORK_OUT_1_SIZE, (int)AI_NETWORK_DATA_ACTIVATIONS_SIZE);
    return 0;
#endif
}

int meteo_ia_classer(const float entrees[METEO_NB_FEATURES],
                     float proba[METEO_NB_CLASSES],
                     int *classe, uint32_t *duree_us)
{
    int i;

    /* a) normalisation z-score avec les valeurs du train */
    for (i = 0; i < METEO_NB_FEATURES; i++) {
        entree_ia[i] = (entrees[i] - METEO_MEAN[i]) / METEO_STD[i];
    }

    /* b) inférence chronométrée en cycles CPU */
#if METEO_AVEC_XCUBEAI
    ai_entree[0].data = AI_HANDLE_PTR(entree_ia);
    ai_sortie[0].data = AI_HANDLE_PTR(sortie_ia);
#endif
    uint32_t debut = DWT->CYCCNT;
#if AVEC_IA && METEO_AVEC_XCUBEAI
    ai_i32 nb = ai_network_run(reseau, &ai_entree[0], &ai_sortie[0]);
#elif AVEC_IA
    ia_reseau_executer(entree_ia, sortie_ia);
    int nb = 1;                         /* le code genere ne peut pas echouer */
#else
    int nb = 1;                         /* programme témoin : pas d'inférence */
#endif
    uint32_t cycles = DWT->CYCCNT - debut;
    *duree_us = cycles / (SystemCoreClock / 1000000UL);
    if (nb != 1) {
#if METEO_AVEC_XCUBEAI
        ai_error err = ai_network_get_error(reseau);
        printf("[IA] ERREUR run : type=%d code=%d\r\n", err.type, err.code);
#endif
        return -1;
    }

    /* c) recopie des probabilités et recherche du maximum (argmax) */
    *classe = 0;
    for (i = 0; i < METEO_NB_CLASSES; i++) {
        proba[i] = sortie_ia[i];
        if (proba[i] > proba[*classe]) {
            *classe = i;
        }
    }
    return 0;
}

void meteo_ia_autotest(void)
{
    float proba[METEO_NB_CLASSES];
    int classe, attendu, t, i, nb_ok = 0;
    uint32_t us;
    float ecart_max = 0.0f;

    for (t = 0; t < METEO_NB_TESTS; t++) {
        meteo_ia_classer(METEO_TEST_IN[t], proba, &classe, &us);
        attendu = 0;
        for (i = 0; i < METEO_NB_CLASSES; i++) {
            float e = fabsf(proba[i] - METEO_TEST_OUT[t][i]);
            if (e > ecart_max) ecart_max = e;
            if (METEO_TEST_OUT[t][i] > METEO_TEST_OUT[t][attendu]) attendu = i;
        }
        if (classe == attendu) nb_ok++;
        printf("[TEST %d] carte=%s attendu=%s\r\n", t, METEO_CLASS_NAMES[classe], METEO_CLASS_NAMES[attendu]);
    }
    /* pas de %f : ecart affiché en millionièmes */
    printf("[TEST] %d/%d identiques, ecart max = %ld e-6\r\n",
           nb_ok, METEO_NB_TESTS, (long)(ecart_max * 1000000.0f));
}
