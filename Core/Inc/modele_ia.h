/**
  ******************************************************************************
  * @file    modele_ia.h
  * @brief   Reseau de neurones meteo en C, utilise par meteo_ia.c quand le
  *          code de X-CUBE-AI n'est pas present dans le projet.
  *
  *  FICHIER GENERE par TP3_IA/generer_c.py a partir de
  *  TP3_IA/export/meteo_model.keras. Ne pas editer a la main.
  *
  *  Architecture : 4 -> 32 -> 16 -> 4 (ReLU, softmax en sortie)
  *  Parametres   : 756, soit 3024 octets en float32
  ******************************************************************************
  */
#ifndef MODELE_IA_H
#define MODELE_IA_H

#define IA_NB_ENTREES   4
#define IA_NB_CLASSES   4

/**
  * @brief  Execute le reseau, comme ai_network_run de X-CUBE-AI.
  * @param  entree_normalisee  entrees DEJA normalisees (z-score) par l'appelant
  * @param  probabilites       sortie softmax, somme = 1
  */
void ia_reseau_executer(const float entree_normalisee[IA_NB_ENTREES],
                        float probabilites[IA_NB_CLASSES]);

#endif /* MODELE_IA_H */
