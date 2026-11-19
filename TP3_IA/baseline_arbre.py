"""
TP3 ETRS923 - l'IA est-elle justifiee ? Reference deterministe.

Le polycopie du module pose la question (§2.5) : "l'IA ne doit etre utilisee
que si elle apporte un benefice clair par rapport a une methode
deterministe". Il la tranche pour le sinus et affirme que, pour Meteostat,
l'IA est justifiee (§3.4). Ce script le verifie.

Des arbres de decision de profondeur croissante sont entraines sur EXACTEMENT
les memes donnees que le reseau du notebook : memes entrees, meme decoupage
temporel (export/donnees_split.npz), memes poids de classes. Ils sont evalues
sur la meme annee de test, avec les memes metriques. L'arbre retenu est le
plus petit dont la macro-F1 est a moins de 0,01 de celle du reseau ; il est
genere en C et s'execute sur la carte a cote du reseau.

Usage :  py -3.12 TP3_IA/baseline_arbre.py   (apres executer_notebook_local.py)
"""
import json
from pathlib import Path

import numpy as np
import pandas as pd
import tensorflow as tf
from sklearn.metrics import accuracy_score, balanced_accuracy_score, f1_score
from sklearn.tree import DecisionTreeClassifier, _tree
from sklearn.utils.class_weight import compute_class_weight

DOSSIER = Path(__file__).resolve().parent
PROJET = DOSSIER.parent
EXPORT = DOSSIER / "export"

SORTIE_H = PROJET / "Core" / "Inc" / "baseline_arbre.h"
SORTIE_C = PROJET / "Core" / "Src" / "baseline_arbre.c"
SORTIE_CSV = DOSSIER / "baseline_arbre.csv"

PROFONDEURS = [1, 2, 3, 4, 5, 6, 8, 10]
TOLERANCE_MACRO_F1 = 0.01
UNITES = {"temp": "degC", "rhum": "%RH", "pres": "hPa (niveau mer)", "dp3h": "hPa en 3 h"}


def poids_classes(y: np.ndarray, n: int) -> dict:
    """Memes poids que le notebook : 'balanced' adouci par une racine."""
    presentes = np.unique(y)
    w = compute_class_weight("balanced", classes=presentes, y=y)
    poids = {i: 1.0 for i in range(n)}
    poids.update({int(c): float(np.sqrt(v)) for c, v in zip(presentes, w)})
    return poids


def metriques(y, pred, n) -> dict:
    return {"accuracy": accuracy_score(y, pred),
            "balanced_acc": balanced_accuracy_score(y, pred),
            "macro_F1": f1_score(y, pred, average="macro", labels=np.arange(n), zero_division=0)}


def arbre_vers_c(arbre, features, classes) -> str:
    t = arbre.tree_
    lignes = []

    def descendre(noeud: int, retrait: int) -> None:
        marge = "  " * retrait
        if t.feature[noeud] == _tree.TREE_UNDEFINED:
            classe = int(np.argmax(t.value[noeud][0]))
            lignes.append(f"{marge}return {classe};  /* {classes[classe]} */")
            return
        f = int(t.feature[noeud])
        lignes.append(f"{marge}if (entrees[{f}] <= {t.threshold[noeud]:.6f}f)  "
                      f"/* {features[f]} en {UNITES.get(features[f], '')} */")
        lignes.append(f"{marge}{{")
        descendre(t.children_left[noeud], retrait + 1)
        lignes.append(f"{marge}}}")
        lignes.append(f"{marge}else")
        lignes.append(f"{marge}{{")
        descendre(t.children_right[noeud], retrait + 1)
        lignes.append(f"{marge}}}")

    descendre(0, 1)
    return "\n".join(lignes)


def main() -> None:
    d = np.load(EXPORT / "donnees_split.npz")
    Xtr, ytr, Xte, yte = d["X_train"], d["y_train"], d["X_test"], d["y_test"]
    features = [str(f) for f in d["features"]]
    classes = [str(c) for c in d["classes"]]
    n = len(classes)

    # Le reseau du notebook, evalue ici avec exactement les memes metriques
    poids = json.loads((EXPORT / "meteo_weights.json").read_text(encoding="utf-8"))
    moyenne, ecart = np.array(poids["mean"]), np.array(poids["std"])
    reseau = tf.keras.models.load_model(EXPORT / "meteo_model.keras")
    pred_reseau = reseau.predict(((Xte - moyenne) / ecart).astype(np.float32),
                                 batch_size=4096, verbose=0).argmax(axis=1)
    ref = metriques(yte, pred_reseau, n)
    n_params = int(reseau.count_params())

    lignes = [{"methode": f"Reseau ({n_params} parametres)", **ref}]
    arbres = {}
    for p in PROFONDEURS:
        arbre = DecisionTreeClassifier(max_depth=p, class_weight=poids_classes(ytr, n),
                                       random_state=42).fit(Xtr, ytr)
        arbres[p] = arbre
        lignes.append({"methode": f"Arbre profondeur {p}",
                       **metriques(yte, arbre.predict(Xte), n)})

    tableau = pd.DataFrame(lignes).round(4)
    tableau.to_csv(SORTIE_CSV, index=False)
    print(f"Test : {len(yte)} heures de l'annee jamais vue | classes : {classes}\n")
    print(tableau.to_string(index=False))

    retenue = next((p for p in PROFONDEURS
                    if metriques(yte, arbres[p].predict(Xte), n)["macro_F1"]
                    >= ref["macro_F1"] - TOLERANCE_MACRO_F1), PROFONDEURS[-1])
    arbre = arbres[retenue]
    m = metriques(yte, arbre.predict(Xte), n)
    print(f"\nArbre retenu : profondeur {retenue} (au plus {retenue} comparaisons), "
          f"macro-F1 {m['macro_F1']:.4f} contre {ref['macro_F1']:.4f} pour le reseau")

    SORTIE_H.write_text(f'''/**
  ******************************************************************************
  * @file    baseline_arbre.h
  * @brief   Reference deterministe : arbre de decision de profondeur {retenue}.
  *
  *  FICHIER GENERE par TP3_IA/baseline_arbre.py. Ne pas editer a la main.
  *
  *  Entraine sur les memes donnees, avec le meme decoupage temporel et les
  *  memes poids de classes que le reseau du notebook, et evalue sur la meme
  *  annee de test :
  *    arbre  : macro-F1 {m['macro_F1']:.4f}, accuracy {m['accuracy']:.4f}
  *    reseau : macro-F1 {ref['macro_F1']:.4f}, accuracy {ref['accuracy']:.4f}
  *
  *  Memes entrees que le reseau, en grandeurs physiques (aucune
  *  normalisation) : {", ".join(features)}.
  *  Les classes sont celles de METEO_CLASS_NAMES (meteo_model_params.h).
  ******************************************************************************
  */
#ifndef BASELINE_ARBRE_H
#define BASELINE_ARBRE_H

#define ARBRE_NB_ENTREES   {len(features)}
#define ARBRE_NB_CLASSES   {n}

/* Retourne l'indice de la classe predite */
int arbre_predire(const float entrees[ARBRE_NB_ENTREES]);

#endif /* BASELINE_ARBRE_H */
''', encoding="utf-8")

    SORTIE_C.write_text(f'''/**
  ******************************************************************************
  * @file    baseline_arbre.c
  * @brief   Reference deterministe : arbre de decision de profondeur {retenue}.
  *
  *  FICHIER GENERE par TP3_IA/baseline_arbre.py. Ne pas editer a la main.
  *  Aucune multiplication, aucune exponentielle : au plus {retenue} comparaisons.
  ******************************************************************************
  */
#include "baseline_arbre.h"

int arbre_predire(const float entrees[ARBRE_NB_ENTREES])
{{
{arbre_vers_c(arbre, features, classes)}
}}
''', encoding="utf-8")
    print(f"Genere : {SORTIE_H}\nGenere : {SORTIE_C}\nTableau : {SORTIE_CSV}")


if __name__ == "__main__":
    main()
