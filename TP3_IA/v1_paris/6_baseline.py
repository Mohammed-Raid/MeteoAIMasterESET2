"""
TP3 ETRS923 - etape 6 : l'IA est-elle justifiee ici ?

Le polycopie du module pose la question centrale (§2.5) :
    "l'IA ne doit etre utilisee que si elle apporte un benefice clair par
     rapport a une methode deterministe"

Il la tranche pour le sinus (la fonction native gagne : 17 mA contre 36 mA),
et affirme que pour Meteostat l'IA est justifiee parce que "la relation entre
temperature, humidite et pression n'est pas analytique" (§3.4).

Cette affirmation n'est pas verifiee dans le poly. Ce script la met a
l'epreuve : on entraine des arbres de decision de profondeur croissante sur
exactement les memes donnees et le meme decoupage que le reseau de neurones,
et on compare. Un arbre de profondeur 3, c'est au plus 7 comparaisons de
flottants a l'execution, contre 208 multiplications-accumulations et 4
exponentielles pour le reseau.

Le gagnant est ensuite genere en C pour etre mesure sur la carte.

Usage :  py -3.12 6_baseline.py
"""
import json
from pathlib import Path

import numpy as np
import pandas as pd
from sklearn.metrics import balanced_accuracy_score
from sklearn.model_selection import train_test_split
from sklearn.tree import DecisionTreeClassifier, _tree

DOSSIER = Path(__file__).parent
PROJET = DOSSIER.parent
DONNEES = DOSSIER / "meteostat_paris.csv"
META = DOSSIER / "modele_meteo.json"

SORTIE_H = PROJET / "Core" / "Inc" / "baseline_arbre.h"
SORTIE_C = PROJET / "Core" / "Src" / "baseline_arbre.c"

FEATURES = ["temp", "rhum", "pres"]
UNITES = {"temp": "degC", "rhum": "%RH", "pres": "hPa"}
GRAINE = 42

REGROUPEMENT = {
    "Clair": [1, 2],
    "Partiellement nuageux": [3],
    "Couvert": [4],
    "Precipitations": list(range(5, 28)),
}

PROFONDEUR_RETENUE = 5


def donnees():
    df = pd.read_csv(DONNEES)
    code_vers_classe = {}
    for indice, nom in enumerate(REGROUPEMENT):
        for code in REGROUPEMENT[nom]:
            code_vers_classe[code] = indice

    sous = df[df["coco"].isin(code_vers_classe)]
    X = sous[FEATURES].to_numpy(dtype=np.float32)
    y = sous["coco"].map(code_vers_classe).to_numpy(dtype=np.int32)

    # Meme decoupage que 2_entrainement.py : la comparaison n'a de sens que
    # sur le meme jeu de test.
    return train_test_split(X, y, test_size=0.2, random_state=GRAINE, stratify=y)


def arbre_vers_c(arbre: DecisionTreeClassifier, classes: list) -> str:
    """Traduit l'arbre en C, sous forme de if/else imbriques."""
    t = arbre.tree_
    lignes = []

    def descendre(noeud: int, indentation: int) -> None:
        marge = "  " * indentation

        if t.feature[noeud] == _tree.TREE_UNDEFINED:
            classe = int(np.argmax(t.value[noeud][0]))
            effectif = int(t.n_node_samples[noeud])
            lignes.append(f"{marge}return {classe};  "
                          f"/* {classes[classe]} ({effectif} releves) */")
            return

        nom = FEATURES[t.feature[noeud]]
        seuil = t.threshold[noeud]
        lignes.append(f"{marge}if (entrees[{t.feature[noeud]}] <= {seuil:.6f}f)  "
                      f"/* {nom} en {UNITES[nom]} */")
        lignes.append(f"{marge}{{")
        descendre(t.children_left[noeud], indentation + 1)
        lignes.append(f"{marge}}}")
        lignes.append(f"{marge}else")
        lignes.append(f"{marge}{{")
        descendre(t.children_right[noeud], indentation + 1)
        lignes.append(f"{marge}}}")

    descendre(0, 1)
    return "\n".join(lignes)


def main() -> None:
    X_tr, X_te, y_tr, y_te = donnees()
    meta = json.loads(META.read_text(encoding="utf-8"))
    classes = meta["classes"]

    print(f"Jeu de test : {len(X_te)} releves, identique a celui du reseau\n")
    print(f"{'methode':<28} {'exactitude':>11} {'equilibree':>11} {'operations':>12}")
    print("-" * 66)

    lignes = []
    retenu = None

    for profondeur in [1, 2, 3, 4, 5, 8]:
        arbre = DecisionTreeClassifier(max_depth=profondeur, random_state=GRAINE)
        arbre.fit(X_tr, y_tr)
        pred = arbre.predict(X_te)

        exact = float((pred == y_te).mean())
        equil = float(balanced_accuracy_score(y_te, pred))
        # Une comparaison de flottants par niveau traverse
        operations = f"<= {profondeur} comp."

        print(f"{'Arbre profondeur ' + str(profondeur):<28} "
              f"{exact:>11.4f} {equil:>11.4f} {operations:>12}")
        lignes.append({"methode": f"Arbre profondeur {profondeur}",
                       "exactitude": exact, "exactitude_equilibree": equil})

        if profondeur == PROFONDEUR_RETENUE:
            retenu = arbre

    # Le reseau de neurones, mesure dans 2_entrainement.py
    print(f"{'Reseau 3-16-8-4':<28} {meta['exactitude']:>11.4f} "
          f"{meta['exactitude_equilibree']:>11.4f} {'208 MAC + 4 exp':>12}")
    lignes.append({"methode": "Reseau 3-16-8-4",
                   "exactitude": meta["exactitude"],
                   "exactitude_equilibree": meta["exactitude_equilibree"]})

    pd.DataFrame(lignes).to_csv(DOSSIER / "baseline.csv", index=False)

    # ------------------------------------------------------------- verdict
    arbre_exact = [l for l in lignes if l["methode"] == f"Arbre profondeur {PROFONDEUR_RETENUE}"][0]
    ecart = meta["exactitude"] - arbre_exact["exactitude"]
    print("\n" + "=" * 66)
    print(f"Ecart reseau - arbre(profondeur {PROFONDEUR_RETENUE}) : "
          f"{ecart:+.4f} point d'exactitude")
    print("=" * 66)

    # --------------------------------------------------- generation du C
    code = arbre_vers_c(retenu, classes)
    noms = ", ".join(f'"{n}"' for n in classes)

    SORTIE_H.write_text(f'''/**
  ******************************************************************************
  * @file    baseline_arbre.h
  * @brief   Reference deterministe : arbre de decision de profondeur {PROFONDEUR_RETENUE}.
  *
  *  FICHIER GENERE AUTOMATIQUEMENT par TP3_IA/6_baseline.py
  *
  *  Sert a repondre a la question posee par le polycopie du module (§2.5) :
  *  le reseau de neurones apporte-t-il un benefice reel face a une methode
  *  deterministe ? Cet arbre est entraine sur les memes donnees et evalue
  *  sur le meme jeu de test que le reseau.
  *
  *  Exactitude : {arbre_exact["exactitude"]:.4f} (equilibree {arbre_exact["exactitude_equilibree"]:.4f})
  *  Cout        : au plus {PROFONDEUR_RETENUE} comparaisons de flottants, aucune multiplication
  ******************************************************************************
  */
#ifndef BASELINE_ARBRE_H
#define BASELINE_ARBRE_H

#define ARBRE_NB_ENTREES   {len(FEATURES)}
#define ARBRE_NB_CLASSES   {len(classes)}

extern const char *const arbre_noms_classes[ARBRE_NB_CLASSES];

/**
  * @brief  Classe une observation avec l'arbre de decision.
  * @param  entrees  valeurs physiques brutes : temperature degC, humidite %RH,
  *                  pression hPa. Aucune normalisation n'est necessaire.
  * @retval indice de la classe predite
  */
int arbre_predire(const float entrees[ARBRE_NB_ENTREES]);

#endif /* BASELINE_ARBRE_H */
''', encoding="utf-8")

    SORTIE_C.write_text(f'''/**
  ******************************************************************************
  * @file    baseline_arbre.c
  * @brief   Reference deterministe : arbre de decision de profondeur {PROFONDEUR_RETENUE}.
  *
  *  FICHIER GENERE AUTOMATIQUEMENT par TP3_IA/6_baseline.py
  *  Ne pas editer a la main.
  *
  *  A comparer avec modele_ia.c : meme probleme, memes donnees, meme jeu de
  *  test, mais aucune multiplication ni exponentielle. L'arbre travaille
  *  directement sur les grandeurs physiques, sans normalisation.
  ******************************************************************************
  */
#include "baseline_arbre.h"

const char *const arbre_noms_classes[ARBRE_NB_CLASSES] = {{ {noms} }};

int arbre_predire(const float entrees[ARBRE_NB_ENTREES])
{{
{code}
}}
''', encoding="utf-8")

    print(f"\nGenere : {SORTIE_H}")
    print(f"Genere : {SORTIE_C}")


if __name__ == "__main__":
    main()
