"""
TP3 ETRS923 - etape 2 : entrainement du modele de classification meteo.

TP3 §1.a : reseau de neurones dense (MLP) entraine en Python/TensorFlow sur
           les donnees Meteostat, avec des classes issues du systeme de
           classification meteo standard donne dans le sujet.
TP3 §1.b : etude du compromis entre nombre de classes, taille du modele et
           exactitude.

Entrees du modele : temperature, humidite relative, pression.
Sortie : distribution de probabilite sur les classes (softmax).

Usage :  py -3.12 2_entrainement.py
"""
import json
from pathlib import Path

import numpy as np
import pandas as pd
import tensorflow as tf
from sklearn.metrics import balanced_accuracy_score
from sklearn.model_selection import train_test_split

DOSSIER = Path(__file__).parent
DONNEES = DOSSIER / "meteostat_paris.csv"
SORTIE_MODELE = DOSSIER / "modele_meteo.keras"
SORTIE_META = DOSSIER / "modele_meteo.json"

FEATURES = ["temp", "rhum", "pres"]
GRAINE = 42

# ---------------------------------------------------------------------------
# Regroupements des codes COCO de Meteostat vers les classes du sujet.
#
# Le sujet liste treize libelles. Plusieurs sont inexploitables ici :
#   - "Vent fort" n'a pas de code COCO (il faudrait la vitesse du vent) ;
#   - grele, eclairs, pluie verglacante comptent moins de 30 releves sur
#     onze ans, donc trop rares pour etre appris.
# On construit donc quatre granularites croissantes, ce qui est exactement
# l'objet de l'etude demandee au §1.b.
# ---------------------------------------------------------------------------
REGROUPEMENTS = {
    3: {
        "Degage": [1, 2],
        "Nuageux": [3, 4],
        "Precipitations": [5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                           17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27],
    },
    4: {
        "Clair": [1, 2],
        "Partiellement nuageux": [3],
        "Couvert": [4],
        "Precipitations": [5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                           17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27],
    },
    6: {
        "Clair": [1],
        "Peu nuageux": [2],
        "Partiellement nuageux": [3],
        "Couvert": [4],
        "Brouillard": [5, 6],
        "Precipitations": [7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                           17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27],
    },
    9: {
        "Clair": [1],
        "Peu nuageux": [2],
        "Partiellement nuageux": [3],
        "Couvert": [4],
        "Brouillard": [5, 6],
        "Pluie": [7, 8, 9, 10, 11],
        "Averses": [17, 18, 19, 20],
        "Neige": [12, 13, 14, 15, 16, 21, 22],
        "Orage": [23, 24, 25, 26, 27],
    },
}

# Architectures testees. Avec seulement trois entrees, un reseau large
# n'apporte rien : on le verifie plutot que de le supposer.
ARCHITECTURES = {
    "8": [8],
    "16-8": [16, 8],
    "32-16": [32, 16],
    "64-32": [64, 32],
}


def charger_donnees() -> pd.DataFrame:
    if not DONNEES.exists():
        raise SystemExit(f"Fichier absent : {DONNEES}\nLancer d'abord 1_donnees.py")
    return pd.read_csv(DONNEES)


def etiqueter(df: pd.DataFrame, regroupement: dict) -> tuple[np.ndarray, np.ndarray, list]:
    """Transforme les codes COCO en indices de classe selon le regroupement."""
    code_vers_classe = {}
    noms = list(regroupement.keys())
    for indice, nom in enumerate(noms):
        for code in regroupement[nom]:
            code_vers_classe[code] = indice

    masque = df["coco"].isin(code_vers_classe)
    sous = df[masque]

    X = sous[FEATURES].to_numpy(dtype=np.float32)
    y = sous["coco"].map(code_vers_classe).to_numpy(dtype=np.int32)
    return X, y, noms


def construire(couches: list, n_entrees: int, n_classes: int) -> tf.keras.Model:
    """MLP dense : entree -> couches cachees ReLU -> softmax.

    Choix des hyperparametres (TP3 §1.b) :
      - ReLU sur les couches cachees : convergence rapide, pas de saturation
        du gradient, et operation elementaire bien supportee par X-CUBE-AI ;
      - softmax en sortie : le sujet demande une distribution de probabilite ;
      - optimiseur adam : converge vite sans reglage manuel du pas ;
      - sparse_categorical_crossentropy : nos etiquettes sont des entiers et
        non du one-hot, c'est exactement le cas decrit dans le tableau du TP1.
    """
    modele = tf.keras.Sequential(name=f"meteo_{n_classes}c")
    modele.add(tf.keras.layers.Input(shape=(n_entrees,)))
    for n in couches:
        modele.add(tf.keras.layers.Dense(n, activation="relu"))
    modele.add(tf.keras.layers.Dense(n_classes, activation="softmax"))
    modele.compile(
        optimizer="adam",
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"],
    )
    return modele


def main() -> None:
    tf.keras.utils.set_random_seed(GRAINE)
    df = charger_donnees()
    print(f"Jeu de donnees : {len(df)} releves, entrees = {FEATURES}\n")

    resultats = []
    meilleur = None

    for n_classes, regroupement in REGROUPEMENTS.items():
        X, y, noms = etiqueter(df, regroupement)

        X_tr, X_te, y_tr, y_te = train_test_split(
            X, y, test_size=0.2, random_state=GRAINE, stratify=y
        )

        # Normalisation centree-reduite, calculee UNIQUEMENT sur
        # l'apprentissage pour ne pas laisser fuiter le test.
        moyenne = X_tr.mean(axis=0)
        ecart = X_tr.std(axis=0)
        X_tr_n = (X_tr - moyenne) / ecart
        X_te_n = (X_te - moyenne) / ecart

        for nom_archi, couches in ARCHITECTURES.items():
            modele = construire(couches, len(FEATURES), n_classes)
            modele.fit(
                X_tr_n, y_tr,
                validation_split=0.1,
                epochs=30,
                batch_size=256,
                verbose=0,
                callbacks=[tf.keras.callbacks.EarlyStopping(
                    patience=5, restore_best_weights=True, monitor="val_loss")],
            )

            proba = modele.predict(X_te_n, verbose=0)
            pred = proba.argmax(axis=1)

            exactitude = float((pred == y_te).mean())
            # Avec des classes tres desequilibrees, l'exactitude brute flatte
            # le modele : on mesure aussi l'exactitude equilibree.
            equilibree = float(balanced_accuracy_score(y_te, pred))
            params = int(modele.count_params())

            resultats.append({
                "classes": n_classes,
                "architecture": nom_archi,
                "parametres": params,
                "octets_float32": params * 4,
                "exactitude": exactitude,
                "exactitude_equilibree": equilibree,
            })

            print(f"  {n_classes} classes | {nom_archi:>6} | "
                  f"{params:5d} params | {params*4:6d} o | "
                  f"exactitude {exactitude:.4f} | equilibree {equilibree:.4f}")

            # On garde le modele a 4 classes le plus simple qui tienne la
            # route : c'est celui qui partira sur la carte.
            if n_classes == 4 and nom_archi == "16-8":
                meilleur = {
                    "modele": modele,
                    "moyenne": moyenne,
                    "ecart": ecart,
                    "noms": noms,
                    "exactitude": exactitude,
                    "exactitude_equilibree": equilibree,
                    "architecture": nom_archi,
                    "parametres": params,
                }

        print()

    # ----------------------------------------------------------------- bilan
    print("=" * 78)
    print("TP3 §1.b - compromis nombre de classes / taille / exactitude")
    print("=" * 78)
    tableau = pd.DataFrame(resultats)
    print(tableau.to_string(index=False))

    tableau.to_csv(DOSSIER / "comparaison.csv", index=False)
    print(f"\nTableau ecrit : {DOSSIER / 'comparaison.csv'}")

    # --------------------------------------------------- sauvegarde du modele
    modele = meilleur["modele"]
    modele.save(SORTIE_MODELE)

    meta = {
        "features": FEATURES,
        "normalisation": {
            "moyenne": [float(v) for v in meilleur["moyenne"]],
            "ecart_type": [float(v) for v in meilleur["ecart"]],
        },
        "classes": meilleur["noms"],
        "architecture": meilleur["architecture"],
        "parametres": meilleur["parametres"],
        "exactitude": meilleur["exactitude"],
        "exactitude_equilibree": meilleur["exactitude_equilibree"],
    }
    SORTIE_META.write_text(json.dumps(meta, indent=2, ensure_ascii=False), encoding="utf-8")

    print(f"Modele retenu  : {SORTIE_MODELE}")
    print(f"Metadonnees    : {SORTIE_META}")
    print("\nNormalisation a reproduire a l'identique en C :")
    for i, f in enumerate(FEATURES):
        print(f"  {f:5s} : moyenne {meilleur['moyenne'][i]:9.4f}  "
              f"ecart-type {meilleur['ecart'][i]:9.4f}")


if __name__ == "__main__":
    main()
