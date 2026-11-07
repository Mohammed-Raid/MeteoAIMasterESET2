"""
TP3 ETRS923 - etape 5 : export TFLite et quantification int8.

Le polycopie du module (§2.3) presente la quantification comme une technique
obligatoire et non optionnelle pour l'embarque, et le §3.1 decrit la chaine
"TensorFlow -> TFLite -> X-CUBE-AI". Ce script produit les deux variantes et
mesure ce que la quantification coute en exactitude et rapporte en memoire.

Trois modeles sont compares :
  - float32 TFLite      : reference
  - int8 "dynamic range": poids en int8, activations en float
  - int8 integral       : poids ET activations en int8, calibre sur un
                          echantillon representatif

Usage :  py -3.12 5_quantification.py
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
META = DOSSIER / "modele_meteo.json"
SAVED_MODEL = DOSSIER / "saved_model"

TFLITE_F32 = DOSSIER / "modele_meteo_float32.tflite"
TFLITE_DYN = DOSSIER / "modele_meteo_int8_dynamique.tflite"
TFLITE_INT8 = DOSSIER / "modele_meteo_int8_integral.tflite"

FEATURES = ["temp", "rhum", "pres"]
GRAINE = 42

# Memes regroupements que 2_entrainement.py pour le modele a 4 classes
REGROUPEMENT = {
    "Clair": [1, 2],
    "Partiellement nuageux": [3],
    "Couvert": [4],
    "Precipitations": list(range(5, 28)),
}


def jeu_de_test():
    """Reconstruit exactement le decoupage de 2_entrainement.py."""
    df = pd.read_csv(DONNEES)
    meta = json.loads(META.read_text(encoding="utf-8"))

    code_vers_classe = {}
    for indice, nom in enumerate(REGROUPEMENT):
        for code in REGROUPEMENT[nom]:
            code_vers_classe[code] = indice

    sous = df[df["coco"].isin(code_vers_classe)]
    X = sous[FEATURES].to_numpy(dtype=np.float32)
    y = sous["coco"].map(code_vers_classe).to_numpy(dtype=np.int32)

    X_tr, X_te, y_tr, y_te = train_test_split(
        X, y, test_size=0.2, random_state=GRAINE, stratify=y
    )

    moyenne = np.array(meta["normalisation"]["moyenne"], dtype=np.float32)
    ecart = np.array(meta["normalisation"]["ecart_type"], dtype=np.float32)

    return ((X_tr - moyenne) / ecart).astype(np.float32), \
           ((X_te - moyenne) / ecart).astype(np.float32), y_te, meta


def evaluer_tflite(chemin: Path, X: np.ndarray, y: np.ndarray) -> tuple[float, float]:
    """Execute le modele TFLite sur tout le jeu de test."""
    interpreteur = tf.lite.Interpreter(model_path=str(chemin))
    interpreteur.allocate_tensors()
    entree = interpreteur.get_input_details()[0]
    sortie = interpreteur.get_output_details()[0]

    predictions = np.empty(len(X), dtype=np.int32)

    for i in range(len(X)):
        echantillon = X[i:i + 1]

        # Un modele integralement quantifie attend des entiers : on applique
        # l'echelle et le zero-point calcules a la conversion.
        if entree["dtype"] == np.int8:
            echelle, zero = entree["quantization"]
            echantillon = np.round(echantillon / echelle + zero)
            echantillon = np.clip(echantillon, -128, 127).astype(np.int8)

        interpreteur.set_tensor(entree["index"], echantillon)
        interpreteur.invoke()
        predictions[i] = int(interpreteur.get_tensor(sortie["index"])[0].argmax())

    return float((predictions == y).mean()), float(balanced_accuracy_score(y, predictions))


def main() -> None:
    X_tr, X_te, y_te, meta = jeu_de_test()
    print(f"Jeu de test : {len(X_te)} releves\n")

    convertisseur_base = lambda: tf.lite.TFLiteConverter.from_saved_model(str(SAVED_MODEL))

    # --- float32 ----------------------------------------------------------
    conv = convertisseur_base()
    TFLITE_F32.write_bytes(conv.convert())

    # --- int8 dynamic range : seuls les poids sont quantifies -------------
    conv = convertisseur_base()
    conv.optimizations = [tf.lite.Optimize.DEFAULT]
    TFLITE_DYN.write_bytes(conv.convert())

    # --- int8 integral : poids ET activations -----------------------------
    # La calibration a besoin d'un echantillon representatif des entrees
    # reelles, sinon les echelles de quantification sont fausses.
    def echantillon_representatif():
        indices = np.random.default_rng(GRAINE).choice(len(X_tr), 500, replace=False)
        for i in indices:
            yield [X_tr[i:i + 1]]

    conv = convertisseur_base()
    conv.optimizations = [tf.lite.Optimize.DEFAULT]
    conv.representative_dataset = echantillon_representatif
    conv.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    conv.inference_input_type = tf.int8
    conv.inference_output_type = tf.int8
    TFLITE_INT8.write_bytes(conv.convert())

    # --- comparaison ------------------------------------------------------
    print("=" * 72)
    print("Compromis quantification : memoire contre exactitude")
    print("=" * 72)
    print(f"{'variante':<22} {'octets':>8} {'exactitude':>11} {'equilibree':>11}")

    reference = None
    lignes = []
    for nom, chemin in [("TFLite float32", TFLITE_F32),
                        ("TFLite int8 dynamique", TFLITE_DYN),
                        ("TFLite int8 integral", TFLITE_INT8)]:
        taille = chemin.stat().st_size
        exact, equil = evaluer_tflite(chemin, X_te, y_te)
        if reference is None:
            reference = (taille, exact)
        print(f"{nom:<22} {taille:>8} {exact:>11.4f} {equil:>11.4f}")
        lignes.append({"variante": nom, "octets": taille,
                       "exactitude": exact, "exactitude_equilibree": equil})

    # Le code C genere a la main sert de point de comparaison
    poids_c = meta["parametres"] * 4
    print(f"{'Code C genere (poids)':<22} {poids_c:>8} {meta['exactitude']:>11.4f} "
          f"{meta['exactitude_equilibree']:>11.4f}")
    lignes.append({"variante": "Code C genere (poids seuls)", "octets": poids_c,
                   "exactitude": meta["exactitude"],
                   "exactitude_equilibree": meta["exactitude_equilibree"]})

    pd.DataFrame(lignes).to_csv(DOSSIER / "quantification.csv", index=False)

    gain = 100.0 * (1.0 - TFLITE_INT8.stat().st_size / TFLITE_F32.stat().st_size)
    print(f"\nReduction int8 integral vs float32 : {gain:.1f} % de memoire")
    print(f"Tableau ecrit : {DOSSIER / 'quantification.csv'}")


if __name__ == "__main__":
    main()
