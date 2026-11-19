/* meteo_ia.h - Classification météo sur STM32N6 (TP3 ETRS923)
 * Partie IA : X-CUBE-AI (Cortex-M55) + prétraitement des mesures capteurs.
 */
#ifndef METEO_IA_H
#define METEO_IA_H

#include <stdint.h>
#include "meteo_model_params.h"   /* généré par le notebook Colab */

/* ---- Prétraitement ---- */
float meteo_pression_mer(float p_mesuree_hpa, float temp_c, float altitude_m);
void  meteo_historique_ajouter(float p_mer_hpa, uint32_t maintenant_ms);
int   meteo_tendance_3h(float *dp3h);          /* 1 si dispo, 0 sinon (dp3h = 0) */

/* ---- Inférence ---- */
int   meteo_ia_init(void);                     /* 0 si OK */
int   meteo_ia_classer(const float entrees[METEO_NB_FEATURES],
                       float proba[METEO_NB_CLASSES],
                       int *classe, uint32_t *duree_us);
void  meteo_ia_autotest(void);                 /* compare avec les vecteurs de test Python */

#endif
