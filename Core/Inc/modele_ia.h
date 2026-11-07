/**
  ******************************************************************************
  * @file    modele_ia.h
  * @brief   TP3 §3.b - inference du reseau de neurones de classification meteo.
  *
  *  FICHIER GENERE AUTOMATIQUEMENT par TP3_IA/4_generer_c.py
  *  Ne pas editer a la main : reexecuter le script apres reentrainement.
  *
  *  Architecture : 3 -> 16 -> 8 -> 4 (ReLU, ReLU, softmax)
  *  Parametres   : 236 poids, 944 octets en float32
  *  Exactitude   : 0.4847 (equilibree 0.4179)
  ******************************************************************************
  */
#ifndef MODELE_IA_H
#define MODELE_IA_H

#define IA_NB_ENTREES   3
#define IA_NB_CLASSES   4

/* Ordre des entrees brutes attendu par ia_predire() :
 *   [0] temp  en degC
 *   [1] rhum  en %RH
 *   [2] pres  en hPa
 */

/* Noms des classes, dans l'ordre des sorties */
extern const char *const ia_noms_classes[IA_NB_CLASSES];

/**
  * @brief  Classe une observation meteo.
  * @param  entrees_brutes  valeurs physiques non normalisees (voir ci-dessus)
  * @param  probabilites    sortie softmax, somme = 1 (peut etre NULL)
  * @retval indice de la classe la plus probable
  */
int ia_predire(const float entrees_brutes[IA_NB_ENTREES],
               float probabilites[IA_NB_CLASSES]);

/**
  * @brief  Rejoue des vecteurs de test dont la reference a ete calculee par
  *         onnxruntime sur le PC, et compare les sorties.
  * @retval nombre de vecteurs en echec (0 = portage fidele)
  */
int ia_autotest(void);

#endif /* MODELE_IA_H */
