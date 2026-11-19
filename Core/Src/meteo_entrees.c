/**
  ******************************************************************************
  * @file    meteo_entrees.c
  * @brief   Vecteur d'entree du modele meteo, construit a partir des capteurs.
  ******************************************************************************
  */
#include "main.h"
#include "meteo_entrees.h"

/* L'ordre ci-dessous est celui du notebook (FEATURES = temp, rhum, pres,
   dp3h). Si le notebook est relance avec d'autres variables, ce fichier doit
   suivre : la compilation s'arrete ici plutot que de donner au reseau des
   entrees dans le mauvais ordre, sans aucun message. */
#if METEO_NB_FEATURES != 4
#error "meteo_model_params.h ne correspond plus a temp, rhum, pres, dp3h : adapter meteo_entrees.c"
#endif

int meteo_entrees_construire(const mesures_t *m, uint32_t maintenant_ms,
                             float entrees[METEO_NB_FEATURES],
                             meteo_entrees_info_t *info)
{
  float pression_mesuree;
  float pression_mer;
  float tendance;
  int disponible;

  if (!m->sht40_temp_c.valide || !m->sht40_humidite_rh.valide ||
      !m->df_pression_hpa.valide)
  {
    return 0;
  }

  /* Temperature et humidite viennent du meme capteur, le SHT40AD1B */
  pression_mesuree = m->df_pression_hpa.valeur + OFFSET_PRESSION_HPA;
  pression_mer = meteo_pression_mer(pression_mesuree, m->sht40_temp_c.valeur,
                                    ALTITUDE_CARTE_M);

  /* L'historique n'enregistre qu'une valeur toutes les 10 min, on peut donc
     l'appeler a chaque mesure */
  meteo_historique_ajouter(pression_mer, maintenant_ms);
  disponible = meteo_tendance_3h(&tendance);

  entrees[0] = m->sht40_temp_c.valeur;
  entrees[1] = m->sht40_humidite_rh.valeur;
  entrees[2] = pression_mer;
  entrees[3] = tendance;

  if (info != NULL)
  {
    info->pression_mesuree_hpa = pression_mesuree;
    info->pression_mer_hpa = pression_mer;
    info->tendance_3h_hpa = tendance;
    info->tendance_disponible = disponible;
  }
  return 1;
}
