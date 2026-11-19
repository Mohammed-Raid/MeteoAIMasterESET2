"""
TP3 ETRS923 - code C du reseau, sans X-CUBE-AI.

Le sujet (§3.a) prevoit la chaine TensorFlow -> ONNX -> C via X-CUBE-AI. Le
code de la carte (Core/Src/meteo_ia.c) appelle X-CUBE-AI quand son code genere
est present dans le projet. Tant qu'il ne l'est pas - son telechargement
exige un compte ST - meteo_ia.c execute a la place le code produit ici :
les memes couches denses, avec les poids du meme modele.

Le contrat est celui de ai_network_run : l'entree est DEJA normalisee par
meteo_ia.c, avec les constantes de meteo_model_params.h. Il n'y a donc
qu'une seule source pour la normalisation, quel que soit le chemin.

La fidelite n'est pas supposee : meteo_ia_autotest() rejoue sur la carte les
vecteurs de test du notebook et compare aux probabilites de Keras.

Usage :  py -3.12 TP3_IA/generer_c.py   (apres executer_notebook_local.py)
"""
from pathlib import Path

import numpy as np
import tensorflow as tf

DOSSIER = Path(__file__).resolve().parent
PROJET = DOSSIER.parent
MODELE = DOSSIER / "export" / "meteo_model.keras"

SORTIE_H = PROJET / "Core" / "Inc" / "modele_ia.h"
SORTIE_C = PROJET / "Core" / "Src" / "modele_ia.c"


def tableau_c(nom: str, valeurs: np.ndarray) -> str:
    plats = np.asarray(valeurs, dtype=np.float32).flatten()
    lignes = ["  " + " ".join(f"{v:+.8e}f," for v in plats[i:i + 6])
              for i in range(0, len(plats), 6)]
    corps = "\n".join(lignes).rstrip(",")
    return f"static const float {nom}[{len(plats)}] =\n{{\n{corps}\n}};\n"


def propagation_numpy(couches, x: np.ndarray) -> np.ndarray:
    """Meme calcul que le C genere, pour le comparer a Keras avant la carte."""
    h = x.astype(np.float32)
    for rang, (noyau, biais) in enumerate(couches):
        h = h @ noyau + biais
        if rang < len(couches) - 1:
            h = np.maximum(h, 0.0)
    e = np.exp(h - h.max())
    return e / e.sum()


def main() -> None:
    if not MODELE.exists():
        raise SystemExit(f"Modele absent : {MODELE}\nLancer d'abord executer_notebook_local.py")

    modele = tf.keras.models.load_model(MODELE)
    # Les couches Dropout n'ont pas de poids et sont inactives en inference
    couches = [tuple(c.get_weights()) for c in modele.layers if c.get_weights()]
    tailles = [couches[0][0].shape[0]] + [b.shape[0] for _, b in couches]
    n_params = int(sum(n.size + b.size for n, b in couches))

    # Controle prealable : le calcul a reproduire en C doit egaler Keras
    x = np.random.default_rng(0).normal(0, 1.5, (300, tailles[0])).astype(np.float32)
    ref = modele.predict(x, verbose=0)
    ecart = max(float(np.abs(propagation_numpy(couches, xi) - ri).max())
                for xi, ri in zip(x, ref))
    if ecart > 1e-5:
        raise SystemExit(f"Calcul de reference different de Keras (ecart {ecart:.2e})")

    archi = " -> ".join(str(t) for t in tailles)
    SORTIE_H.write_text(f'''/**
  ******************************************************************************
  * @file    modele_ia.h
  * @brief   Reseau de neurones meteo en C, utilise par meteo_ia.c quand le
  *          code de X-CUBE-AI n'est pas present dans le projet.
  *
  *  FICHIER GENERE par TP3_IA/generer_c.py a partir de
  *  TP3_IA/export/meteo_model.keras. Ne pas editer a la main.
  *
  *  Architecture : {archi} (ReLU, softmax en sortie)
  *  Parametres   : {n_params}, soit {n_params * 4} octets en float32
  ******************************************************************************
  */
#ifndef MODELE_IA_H
#define MODELE_IA_H

#define IA_NB_ENTREES   {tailles[0]}
#define IA_NB_CLASSES   {tailles[-1]}

/**
  * @brief  Execute le reseau, comme ai_network_run de X-CUBE-AI.
  * @param  entree_normalisee  entrees DEJA normalisees (z-score) par l'appelant
  * @param  probabilites       sortie softmax, somme = 1
  */
void ia_reseau_executer(const float entree_normalisee[IA_NB_ENTREES],
                        float probabilites[IA_NB_CLASSES]);

#endif /* MODELE_IA_H */
''', encoding="utf-8")

    c = f'''/**
  ******************************************************************************
  * @file    modele_ia.c
  * @brief   Reseau de neurones meteo en C ({archi}).
  *
  *  FICHIER GENERE par TP3_IA/generer_c.py. Ne pas editer a la main :
  *  reexecuter le script apres tout reentrainement du notebook.
  *
  *  Calcul controle contre Keras avant generation (ecart {ecart:.1e}), puis
  *  sur la carte par meteo_ia_autotest().
  ******************************************************************************
  */
#include "modele_ia.h"

#include <math.h>

/* Poids, ranges [entree][sortie] */
'''
    for rang, (noyau, biais) in enumerate(couches, start=1):
        c += f"\n/* couche {rang} : {noyau.shape[0]} -> {noyau.shape[1]} */\n"
        c += tableau_c(f"noyau{rang}", noyau)
        c += tableau_c(f"biais{rang}", biais)

    c += '''
/* sortie[j] = biais[j] + somme_i entree[i] * noyau[i][j], puis ReLU si demande */
static void dense(const float *entree, int n_entree,
                  const float *noyau, const float *biais,
                  float *sortie, int n_sortie, int relu)
{
  int i;
  int j;

  for (j = 0; j < n_sortie; j++)
  {
    float somme = biais[j];

    for (i = 0; i < n_entree; i++)
    {
      somme += entree[i] * noyau[i * n_sortie + j];
    }
    sortie[j] = (relu && somme < 0.0f) ? 0.0f : somme;
  }
}

/* Softmax stable : on retranche le maximum avant l'exponentielle */
static void softmax(float *valeurs, int n)
{
  int i;
  float maximum = valeurs[0];
  float total = 0.0f;

  for (i = 1; i < n; i++)
  {
    if (valeurs[i] > maximum)
    {
      maximum = valeurs[i];
    }
  }
  for (i = 0; i < n; i++)
  {
    valeurs[i] = expf(valeurs[i] - maximum);
    total += valeurs[i];
  }
  for (i = 0; i < n; i++)
  {
    valeurs[i] /= total;
  }
}

void ia_reseau_executer(const float entree_normalisee[IA_NB_ENTREES],
                        float probabilites[IA_NB_CLASSES])
{
'''
    for rang in range(1, len(couches)):
        c += f"  float couche{rang}[{tailles[rang]}];\n"
    precedent, taille_prec = "entree_normalisee", "IA_NB_ENTREES"
    for rang in range(1, len(couches) + 1):
        derniere = rang == len(couches)
        sortie = "probabilites" if derniere else f"couche{rang}"
        taille = "IA_NB_CLASSES" if derniere else str(tailles[rang])
        c += (f"\n  dense({precedent}, {taille_prec}, noyau{rang}, biais{rang}, "
              f"{sortie}, {taille}, {0 if derniere else 1});")
        precedent, taille_prec = sortie, taille
    c += "\n  softmax(probabilites, IA_NB_CLASSES);\n}\n"

    SORTIE_C.write_text(c, encoding="utf-8")
    print(f"Architecture : {archi}, {n_params} parametres")
    print(f"Ecart calcul de reference / Keras : {ecart:.2e}")
    print(f"Genere : {SORTIE_H}")
    print(f"Genere : {SORTIE_C}")


if __name__ == "__main__":
    main()
