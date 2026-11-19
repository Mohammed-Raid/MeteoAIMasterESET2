/**
  ******************************************************************************
  * @file    baseline_arbre.h
  * @brief   Reference deterministe : arbre de decision de profondeur 5.
  *
  *  FICHIER GENERE par TP3_IA/baseline_arbre.py. Ne pas editer a la main.
  *
  *  Entraine sur les memes donnees, avec le meme decoupage temporel et les
  *  memes poids de classes que le reseau du notebook, et evalue sur la meme
  *  annee de test :
  *    arbre  : macro-F1 0.4871, accuracy 0.5639
  *    reseau : macro-F1 0.4838, accuracy 0.5599
  *
  *  Memes entrees que le reseau, en grandeurs physiques (aucune
  *  normalisation) : temp, rhum, pres, dp3h.
  *  Les classes sont celles de METEO_CLASS_NAMES (meteo_model_params.h).
  ******************************************************************************
  */
#ifndef BASELINE_ARBRE_H
#define BASELINE_ARBRE_H

#define ARBRE_NB_ENTREES   4
#define ARBRE_NB_CLASSES   4

/* Retourne l'indice de la classe predite */
int arbre_predire(const float entrees[ARBRE_NB_ENTREES]);

#endif /* BASELINE_ARBRE_H */
