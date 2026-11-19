"""
Execute TP3_Meteo_Colab.ipynb en local, sans Colab ni Jupyter.

Le notebook reste la source : ce script se contente d'en executer les
cellules de code dans l'ordre, dans le dossier TP3_IA/, et produit le meme
dossier export/ qu'une execution dans Colab.

Trois adaptations seulement, toutes hors du notebook :
  - les lignes "!pip ..." sont ignorees (les paquets sont deja installes) ;
  - display() est remplace par print() ;
  - le garde-fou de meteostat 2.1 qui bloque les requetes horaires de plus
    de 3 ans est desactive. Sans cela, les 10 ans demandes par le notebook
    levent une exception, chaque station est ignoree en silence, et la
    cellule echoue faute de donnees.

A la fin, les jeux d'apprentissage et de test du modele final sont
enregistres en grandeurs physiques dans export/donnees_split.npz, pour que
l'arbre de decision de reference (baseline_arbre.py) soit entraine et evalue
exactement sur les memes donnees que le reseau.

Usage :  py -3.12 TP3_IA/executer_notebook_local.py
Prerequis : tensorflow meteostat pandas scikit-learn tf2onnx onnx onnxruntime
            matplotlib pyarrow
"""
import json
import os
from pathlib import Path

import matplotlib

matplotlib.use("Agg")   # pas de fenetre : les figures sont enregistrees en PNG

import meteostat  # noqa: E402
import numpy as np  # noqa: E402

meteostat.config.block_large_requests = False

DOSSIER = Path(__file__).resolve().parent
NOTEBOOK = DOSSIER / "TP3_Meteo_Colab.ipynb"


def main() -> None:
    os.chdir(DOSSIER)
    cellules = json.loads(NOTEBOOK.read_text(encoding="utf-8"))["cells"]
    espace = {"__name__": "__main__", "display": print}

    for numero, cellule in enumerate(cellules):
        if cellule["cell_type"] != "code":
            continue
        lignes = "".join(cellule["source"]).splitlines()
        code = "\n".join(l for l in lignes if not l.lstrip().startswith("!"))
        print(f"\n===== cellule {numero} =====", flush=True)
        exec(compile(code, f"<cellule {numero}>", "exec"), espace)

    # Donnees du modele final, en grandeurs physiques, pour l'arbre de reference
    parties, moyenne, ecart, classes = espace["preparer"](espace["FEATURES"], espace["SCHEMA"])
    brut = {nom: (X * ecart + moyenne, y) for nom, (X, y) in parties.items()}
    np.savez(DOSSIER / "export" / "donnees_split.npz",
             X_train=brut["train"][0], y_train=brut["train"][1],
             X_test=brut["test"][0], y_test=brut["test"][1],
             features=np.array(espace["FEATURES"]), classes=np.array(classes))
    print("\nexport/donnees_split.npz ecrit")


if __name__ == "__main__":
    main()
