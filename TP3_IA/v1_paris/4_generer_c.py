"""
TP3 ETRS923 - etape 4 : generation du code C d'inference.

TP3 §3.b : "Implementer un code C qui tourne le reseau des neurones pour le
            probleme MeteoStat en mode inference."

Le sujet propose de passer par X-CUBE-AI (§3.a). Le telechargement de cet
outil exige un compte ST. Le modele ne comptant que 236 poids et trois
couches denses, on genere ici directement le C equivalent, ce qui a deux
avantages : aucune dependance a un runtime externe, et une empreinte
minimale, ce qui sert la mesure de consommation du §3.d.

La fidelite n'est pas supposee : le script embarque des vecteurs de test
avec les sorties de reference calculees par onnxruntime, que la carte
rejoue au demarrage (ia_autotest).

Usage :  py -3.12 4_generer_c.py
"""
import json
from pathlib import Path

import numpy as np
import onnxruntime as ort
import tensorflow as tf

DOSSIER = Path(__file__).parent
PROJET = DOSSIER.parent
MODELE = DOSSIER / "modele_meteo.keras"
ONNX = DOSSIER / "modele_meteo.onnx"
META = DOSSIER / "modele_meteo.json"

SORTIE_H = PROJET / "Core" / "Inc" / "modele_ia.h"
SORTIE_C = PROJET / "Core" / "Src" / "modele_ia.c"

NB_TESTS = 12


def tableau_c(nom: str, valeurs: np.ndarray) -> str:
    """Formate un tableau de float en C, 6 valeurs par ligne."""
    plats = valeurs.flatten()
    lignes = []
    for debut in range(0, len(plats), 6):
        morceau = plats[debut:debut + 6]
        lignes.append("  " + " ".join(f"{v:+.8e}f," for v in morceau))
    corps = "\n".join(lignes).rstrip(",")
    return f"static const float {nom}[{len(plats)}] =\n{{\n{corps}\n}};\n"


def main() -> None:
    meta = json.loads(META.read_text(encoding="utf-8"))
    modele = tf.keras.models.load_model(MODELE)

    features = meta["features"]
    classes = meta["classes"]
    moyenne = np.array(meta["normalisation"]["moyenne"], dtype=np.float32)
    ecart = np.array(meta["normalisation"]["ecart_type"], dtype=np.float32)

    denses = [c for c in modele.layers if c.get_weights()]
    poids = [c.get_weights() for c in denses]
    tailles = [features.__len__()] + [w[1].shape[0] for w in poids]

    print(f"Architecture : {' -> '.join(str(t) for t in tailles)}")
    print(f"Entrees      : {features}")
    print(f"Classes      : {classes}")

    # ------------------------------------------------- vecteurs de reference
    # Entrees brutes plausibles, couvrant le domaine des donnees reelles
    rng = np.random.default_rng(7)
    bruts = np.stack([
        rng.uniform(-5.0, 35.0, NB_TESTS),      # temperature degC
        rng.uniform(20.0, 100.0, NB_TESTS),     # humidite %RH
        rng.uniform(985.0, 1040.0, NB_TESTS),   # pression hPa
    ], axis=1).astype(np.float32)

    normalises = ((bruts - moyenne) / ecart).astype(np.float32)

    session = ort.InferenceSession(str(ONNX), providers=["CPUExecutionProvider"])
    nom_entree = session.get_inputs()[0].name
    attendus = session.run(None, {nom_entree: normalises})[0].astype(np.float32)

    # ------------------------------------------------------------- en-tete .h
    h = f'''/**
  ******************************************************************************
  * @file    modele_ia.h
  * @brief   TP3 §3.b - inference du reseau de neurones de classification meteo.
  *
  *  FICHIER GENERE AUTOMATIQUEMENT par TP3_IA/4_generer_c.py
  *  Ne pas editer a la main : reexecuter le script apres reentrainement.
  *
  *  Architecture : {' -> '.join(str(t) for t in tailles)} (ReLU, ReLU, softmax)
  *  Parametres   : {meta["parametres"]} poids, {meta["parametres"] * 4} octets en float32
  *  Exactitude   : {meta["exactitude"]:.4f} (equilibree {meta["exactitude_equilibree"]:.4f})
  ******************************************************************************
  */
#ifndef MODELE_IA_H
#define MODELE_IA_H

#define IA_NB_ENTREES   {len(features)}
#define IA_NB_CLASSES   {len(classes)}

/* Ordre des entrees brutes attendu par ia_predire() :
'''
    for i, f in enumerate(features):
        unite = {"temp": "degC", "rhum": "%RH", "pres": "hPa"}.get(f, "")
        h += f" *   [{i}] {f:5s} en {unite}\n"
    h += f''' */

/* Noms des classes, dans l'ordre des sorties */
extern const char *const ia_noms_classes[IA_NB_CLASSES];

/**
  * @brief  Classe une observation meteo.
  * @param  entrees_brutes  valeurs physiques non normalisees (voir ci-dessus)
  * @param  probabilites    sortie softmax, somme = 1 (peut etre NULL)
  * @retval indice de la classe la plus probable
  */
int ia_predire(const float entrees_brutes[IA_NB_ENTREES],
               float probabilites[IA_NB_CLASSES]);

/**
  * @brief  Rejoue des vecteurs de test dont la reference a ete calculee par
  *         onnxruntime sur le PC, et compare les sorties.
  * @retval nombre de vecteurs en echec (0 = portage fidele)
  */
int ia_autotest(void);

#endif /* MODELE_IA_H */
'''
    SORTIE_H.write_text(h, encoding="utf-8")

    # ------------------------------------------------------------- source .c
    c = f'''/**
  ******************************************************************************
  * @file    modele_ia.c
  * @brief   TP3 §3.b - inference du reseau de neurones de classification meteo.
  *
  *  FICHIER GENERE AUTOMATIQUEMENT par TP3_IA/4_generer_c.py
  *  Ne pas editer a la main : reexecuter le script apres reentrainement.
  *
  *  Le reseau est un perceptron multicouche entraine sous TensorFlow sur les
  *  releves Meteostat (TP3 §1). Les poids ci-dessous sont ceux du modele
  *  exporte en ONNX, dont la fidelite a ete verifiee a 1,2e-07 pres.
  ******************************************************************************
  */
#include "modele_ia.h"

#include <math.h>
#include <stddef.h>

/* ===========================================================================
 *  Normalisation centree-reduite.
 *  Constantes calculees sur l'ensemble d'APPRENTISSAGE uniquement.
 *  Elles doivent etre identiques a celles utilisees a l'entrainement :
 *  c'est la premiere cause d'ecart entre un modele Python et son portage C.
 * =========================================================================*/
'''
    c += tableau_c("ia_moyenne", moyenne)
    c += tableau_c("ia_ecart_type", ecart)

    c += "\n/* ===========================================================================\n"
    c += " *  Poids du reseau. Les noyaux sont ranges [entree][sortie].\n"
    c += " * =========================================================================*/\n"
    for i, (noyau, biais) in enumerate(poids):
        c += f"\n/* couche {i + 1} : {noyau.shape[0]} -> {noyau.shape[1]} */\n"
        c += tableau_c(f"ia_noyau{i + 1}", noyau)
        c += tableau_c(f"ia_biais{i + 1}", biais)

    noms = ", ".join(f'"{n}"' for n in classes)
    c += f'''
const char *const ia_noms_classes[IA_NB_CLASSES] = {{ {noms} }};

/* ===========================================================================
 *  Couche dense : sortie[j] = biais[j] + somme_i entree[i] * noyau[i][j]
 * =========================================================================*/
static void dense(const float *entree, int n_entree,
                  const float *noyau, const float *biais,
                  float *sortie, int n_sortie, int relu)
{{
  int i;
  int j;

  for (j = 0; j < n_sortie; j++)
  {{
    float somme = biais[j];

    for (i = 0; i < n_entree; i++)
    {{
      somme += entree[i] * noyau[i * n_sortie + j];
    }}

    if (relu && somme < 0.0f)
    {{
      somme = 0.0f;
    }}
    sortie[j] = somme;
  }}
}}

/* Softmax numeriquement stable : on retranche le maximum avant l'exponentielle
   pour eviter tout debordement. */
static void softmax(float *valeurs, int n)
{{
  int i;
  float maximum = valeurs[0];
  float total = 0.0f;

  for (i = 1; i < n; i++)
  {{
    if (valeurs[i] > maximum)
    {{
      maximum = valeurs[i];
    }}
  }}

  for (i = 0; i < n; i++)
  {{
    valeurs[i] = expf(valeurs[i] - maximum);
    total += valeurs[i];
  }}

  for (i = 0; i < n; i++)
  {{
    valeurs[i] /= total;
  }}
}}

int ia_predire(const float entrees_brutes[IA_NB_ENTREES],
               float probabilites[IA_NB_CLASSES])
{{
  float normalisees[IA_NB_ENTREES];
  float couche1[{tailles[1]}];
  float couche2[{tailles[2]}];
  float sortie[IA_NB_CLASSES];
  int i;
  int meilleure = 0;

  for (i = 0; i < IA_NB_ENTREES; i++)
  {{
    normalisees[i] = (entrees_brutes[i] - ia_moyenne[i]) / ia_ecart_type[i];
  }}

  dense(normalisees, IA_NB_ENTREES, ia_noyau1, ia_biais1, couche1, {tailles[1]}, 1);
  dense(couche1, {tailles[1]}, ia_noyau2, ia_biais2, couche2, {tailles[2]}, 1);
  dense(couche2, {tailles[2]}, ia_noyau3, ia_biais3, sortie, IA_NB_CLASSES, 0);
  softmax(sortie, IA_NB_CLASSES);

  for (i = 1; i < IA_NB_CLASSES; i++)
  {{
    if (sortie[i] > sortie[meilleure])
    {{
      meilleure = i;
    }}
  }}

  if (probabilites != NULL)
  {{
    for (i = 0; i < IA_NB_CLASSES; i++)
    {{
      probabilites[i] = sortie[i];
    }}
  }}

  return meilleure;
}}

/* ===========================================================================
 *  Auto-test : vecteurs dont la reference vient d'onnxruntime sur PC.
 *  Tolerance 1e-4 : largement au-dessus du bruit d'arrondi float32, et
 *  largement en-dessous de tout ecart qui changerait une decision.
 * =========================================================================*/
#define IA_NB_TESTS   {NB_TESTS}

'''
    c += tableau_c("ia_test_entrees", bruts)
    c += tableau_c("ia_test_attendus", attendus)

    c += '''
int ia_autotest(void)
{
  float probabilites[IA_NB_CLASSES];
  int echecs = 0;
  int t;
  int i;

  for (t = 0; t < IA_NB_TESTS; t++)
  {
    ia_predire(&ia_test_entrees[t * IA_NB_ENTREES], probabilites);

    for (i = 0; i < IA_NB_CLASSES; i++)
    {
      float ecart = probabilites[i] - ia_test_attendus[t * IA_NB_CLASSES + i];

      if (ecart < 0.0f)
      {
        ecart = -ecart;
      }
      if (ecart > 1.0e-4f)
      {
        echecs++;
        break;
      }
    }
  }

  return echecs;
}
'''
    SORTIE_C.write_text(c, encoding="utf-8")

    print(f"\nGenere : {SORTIE_H}")
    print(f"Genere : {SORTIE_C}")
    print(f"\n{NB_TESTS} vecteurs de test embarques, reference onnxruntime.")
    print("\nApercu des cas de test :")
    for t in range(4):
        cls = int(attendus[t].argmax())
        print(f"  T={bruts[t][0]:6.1f} degC  RH={bruts[t][1]:5.1f}%  P={bruts[t][2]:7.1f} hPa"
              f"  -> {classes[cls]} ({attendus[t][cls]:.3f})")


if __name__ == "__main__":
    main()
