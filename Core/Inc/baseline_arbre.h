/**
  ******************************************************************************
  * @file    baseline_arbre.h
  * @brief   Reference deterministe : arbre de decision de profondeur 5.
  *
  *  FICHIER GENERE AUTOMATIQUEMENT par TP3_IA/6_baseline.py
  *
  *  Sert a repondre a la question posee par le polycopie du module (§2.5) :
  *  le reseau de neurones apporte-t-il un benefice reel face a une methode
  *  deterministe ? Cet arbre est entraine sur les memes donnees et evalue
  *  sur le meme jeu de test que le reseau.
  *
  *  Exactitude : 0.4816 (equilibree 0.4164)
  *  Cout        : au plus 5 comparaisons de flottants, aucune multiplication
  ******************************************************************************
  */
#ifndef BASELINE_ARBRE_H
#define BASELINE_ARBRE_H

#define ARBRE_NB_ENTREES   3
#define ARBRE_NB_CLASSES   4

extern const char *const arbre_noms_classes[ARBRE_NB_CLASSES];

/**
  * @brief  Classe une observation avec l'arbre de decision.
  * @param  entrees  valeurs physiques brutes : temperature degC, humidite %RH,
  *                  pression hPa. Aucune normalisation n'est necessaire.
  * @retval indice de la classe predite
  */
int arbre_predire(const float entrees[ARBRE_NB_ENTREES]);

#endif /* BASELINE_ARBRE_H */
