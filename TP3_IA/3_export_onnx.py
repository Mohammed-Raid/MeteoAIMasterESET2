"""
TP3 ETRS923 - etape 3 : export du modele TensorFlow vers ONNX.

TP3 §3.a : "N'oubliez pas de transferer le modele TensorFlow -> ONNX -> C
            (via X Cube AI)". Ce script realise le premier transfert.

Le script verifie ensuite que l'ONNX produit les MEMES sorties que le modele
Keras d'origine. Sans cette verification, une conversion silencieusement
fausse ne se decouvre qu'une fois le code embarque sur la carte.

Usage :  py -3.12 3_export_onnx.py
"""
import json
import shutil
import subprocess
import sys
from pathlib import Path

import numpy as np
import tensorflow as tf

DOSSIER = Path(__file__).parent
MODELE = DOSSIER / "modele_meteo.keras"
META = DOSSIER / "modele_meteo.json"
SAVED_MODEL = DOSSIER / "saved_model"
ONNX = DOSSIER / "modele_meteo.onnx"

OPSET = 13  # largement supporte par X-CUBE-AI


def main() -> None:
    if not MODELE.exists():
        raise SystemExit(f"Modele absent : {MODELE}\nLancer d'abord 2_entrainement.py")

    meta = json.loads(META.read_text(encoding="utf-8"))
    modele = tf.keras.models.load_model(MODELE)
    n_entrees = len(meta["features"])

    # --- 1. Keras -> SavedModel -------------------------------------------
    # tf2onnx ne lit pas le format .keras de Keras 3 : il faut passer par un
    # SavedModel TensorFlow.
    if SAVED_MODEL.exists():
        shutil.rmtree(SAVED_MODEL)
    modele.export(str(SAVED_MODEL))
    print(f"SavedModel ecrit : {SAVED_MODEL}")

    # --- 2. SavedModel -> ONNX --------------------------------------------
    commande = [
        sys.executable, "-m", "tf2onnx.convert",
        "--saved-model", str(SAVED_MODEL),
        "--output", str(ONNX),
        "--opset", str(OPSET),
    ]
    resultat = subprocess.run(commande, capture_output=True, text=True)
    if resultat.returncode != 0:
        print(resultat.stdout[-3000:])
        print(resultat.stderr[-3000:])
        raise SystemExit("Echec de la conversion ONNX")
    print(f"ONNX ecrit       : {ONNX}  ({ONNX.stat().st_size} octets)")

    # --- 3. Verification numerique ----------------------------------------
    import onnxruntime as ort

    rng = np.random.default_rng(0)
    # Entrees normalisees plausibles : quelques ecarts-types autour de zero
    echantillon = rng.normal(0.0, 1.5, size=(500, n_entrees)).astype(np.float32)

    sortie_keras = modele.predict(echantillon, verbose=0)

    session = ort.InferenceSession(str(ONNX), providers=["CPUExecutionProvider"])
    nom_entree = session.get_inputs()[0].name
    sortie_onnx = session.run(None, {nom_entree: echantillon})[0]

    ecart_max = float(np.abs(sortie_keras - sortie_onnx).max())
    classes_keras = sortie_keras.argmax(axis=1)
    classes_onnx = sortie_onnx.argmax(axis=1)
    accord = float((classes_keras == classes_onnx).mean())

    print("\nVerification Keras vs ONNX sur 500 entrees :")
    print(f"  ecart absolu maximal sur les probabilites : {ecart_max:.3e}")
    print(f"  accord sur la classe predite              : {accord:.4%}")

    if ecart_max > 1e-5 or accord < 1.0:
        raise SystemExit("ECHEC : l'ONNX ne reproduit pas le modele Keras")
    print("  -> conversion fidele")

    # --- 4. Rappel des constantes pour l'embarque -------------------------
    print("\nA reporter dans le code C (normalisation centree-reduite) :")
    for i, f in enumerate(meta["features"]):
        moy = meta["normalisation"]["moyenne"][i]
        ect = meta["normalisation"]["ecart_type"][i]
        print(f"  {f:5s} : (valeur - {moy:9.4f}f) / {ect:8.4f}f")
    print("\nClasses de sortie, dans l'ordre :")
    for i, nom in enumerate(meta["classes"]):
        print(f"  {i} : {nom}")


if __name__ == "__main__":
    main()
