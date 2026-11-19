/**
  ******************************************************************************
  * @file    meteo_entrees.h
  * @brief   Vecteur d'entree du modele meteo, construit a partir des capteurs.
  *
  *  Le modele (TP3_Meteo_Colab.ipynb) attend, dans cet ordre :
  *    temperature (degC), humidite relative (%RH),
  *    pression ramenee au niveau de la mer (hPa), tendance de pression sur 3 h.
  *  Le pretraitement (pression mer, historique, tendance) est celui de
  *  meteo_ia.c ; ce module se contente de l'appliquer aux mesures.
  ******************************************************************************
  */
#ifndef METEO_ENTREES_H
#define METEO_ENTREES_H

#include <stdint.h>

#include "capteurs.h"
#include "meteo_ia.h"

/* Valeurs intermediaires, utiles a l'affichage */
typedef struct
{
  float pression_mesuree_hpa;   /* LPS22DF, a l'altitude de la carte */
  float pression_mer_hpa;       /* ramenee au niveau de la mer */
  float tendance_3h_hpa;        /* 0 tant que l'historique fait moins de 3 h */
  int   tendance_disponible;    /* 1 des que 3 h d'historique sont accumulees */
} meteo_entrees_info_t;

/**
  * @brief  Remplit le vecteur d'entree du modele.
  * @param  m              mesures issues de capteurs_lire()
  * @param  maintenant_ms  HAL_GetTick(), pour l'historique de pression
  * @param  entrees        vecteur a remplir, dans l'ordre du modele
  * @param  info           valeurs intermediaires (peut etre NULL)
  * @retval 1 si les mesures necessaires sont valides, 0 sinon
  */
int meteo_entrees_construire(const mesures_t *m, uint32_t maintenant_ms,
                             float entrees[METEO_NB_FEATURES],
                             meteo_entrees_info_t *info);

#endif /* METEO_ENTREES_H */
