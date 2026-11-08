# TP3 ETRS923 — Cloud versus Edge AI

Partie IA embarquée : entraînement du modèle de classification météo, export,
et exécution sur la carte NUCLEO-N657X0-Q à partir des capteurs réels.

## Ce qui est couvert

| Point du sujet | État | Où |
| --- | --- | --- |
| §1.a — modèle entraîné en Python/TensorFlow sur Meteostat | fait | `2_entrainement.py` |
| §1.b — compromis classes / taille / exactitude | fait | `comparaison.csv` |
| §3.a — TensorFlow → ONNX | fait et vérifié | `3_export_onnx.py` |
| §3.a — ONNX → C | fait **sans X-CUBE-AI** (voir plus bas) | `4_generer_c.py` |
| §3.b — code C d'inférence | fait | `Core/Src/modele_ia.c` |
| §3.c — données réelles des capteurs + temps d'exécution | fait | `Core/Src/partie4_ia.c` |
| §3.c — comparaison avec le Cloud | **à faire** | dépend du §2 (ThingSpeak) |
| §3.d — programme témoin | fait | `make PARTIE=4 TEMOIN=1` |
| §3.d — mesure du courant | **à faire** | manipulation matérielle |
| §2 — MATLAB / ThingSpeak / Talkback | **non traité** | licence et compte requis |

## Pourquoi pas X-CUBE-AI

Le sujet (§3.a) indique « TensorFlow -> ONNX -> C (via X Cube AI) ». X-CUBE-AI
supporte bien le STM32N6 à partir de la version 10.0.0, NPU Neural-ART compris,
mais son téléchargement exige un compte STMicroelectronics.

Le modèle retenu ne comptant que **236 poids** répartis sur trois couches
denses, le code C équivalent est généré directement par `4_generer_c.py`. Deux
avantages pour ce TP : aucune dépendance à un runtime externe, et une empreinte
minimale, ce qui sert la mesure de consommation du §3.d.

La fidélité du portage n'est pas supposée : douze vecteurs de test, dont les
sorties de référence sont calculées par `onnxruntime` sur PC, sont embarqués
dans le binaire et rejoués au démarrage par `ia_autotest()`.

Pour revenir à la voie du sujet, installer X-CUBE-AI ≥ 10.0.0 via
`Help > Manage Embedded Software Packages` dans STM32CubeIDE, puis importer
`modele_meteo.onnx`.

## Le modèle

Entrées, dans cet ordre : **température (°C), humidité relative (%RH),
pression (hPa)**. Ce sont les trois grandeurs que la carte X-NUCLEO-IKS4A1
sait mesurer, contrainte imposée par le §3.c.

Architecture : `3 → 16 (ReLU) → 8 (ReLU) → 4 (softmax)`, 236 paramètres,
944 octets de poids en float32.

Classes de sortie : `Clair`, `Partiellement nuageux`, `Couvert`,
`Précipitations`.

Hyperparamètres, et pourquoi :

- **ReLU** sur les couches cachées : convergence rapide, pas de saturation du
  gradient, et opération élémentaire bien supportée par les générateurs de code.
- **softmax** en sortie : le sujet demande une distribution de probabilité.
- **adam** : converge vite sans réglage manuel du pas.
- **sparse_categorical_crossentropy** : les étiquettes sont des entiers et non
  du one-hot, exactement le cas décrit dans le tableau du TP1 §3.

## §1.b — compromis classes / taille / exactitude

Données : 142 120 relevés horaires, stations Paris-Orly, Montsouris et
Le Bourget, 2015–2025. Découpage 80/20 stratifié.

| Classes | Archi | Paramètres | Octets | Exactitude | Équilibrée | Majorité | Gain |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 3 | 8 | 59 | 236 | 0,5469 | 0,5271 | 0,4421 | +0,105 |
| 3 | 16-8 | 227 | 908 | 0,5543 | 0,5262 | 0,4421 | +0,112 |
| 3 | 32-16 | 707 | 2828 | 0,5550 | 0,5253 | 0,4421 | +0,113 |
| 3 | 64-32 | 2435 | 9740 | 0,5536 | 0,5217 | 0,4421 | +0,111 |
| 4 | 8 | 68 | 272 | 0,4805 | 0,4139 | 0,3239 | +0,157 |
| **4** | **16-8** | **236** | **944** | **0,4847** | **0,4179** | 0,3239 | **+0,161** |
| 4 | 32-16 | 724 | 2896 | 0,4845 | 0,4173 | 0,3239 | +0,161 |
| 4 | 64-32 | 2468 | 9872 | 0,4862 | 0,4182 | 0,3239 | +0,162 |
| 6 | 16-8 | 254 | 1016 | 0,4109 | 0,3509 | 0,3239 | +0,087 |
| 9 | 64-32 | 2633 | 10532 | 0,4024 | 0,2640 | 0,3239 | +0,078 |

Trois enseignements :

1. **La taille du modèle n'a quasiment aucun effet.** Passer de 59 à 2435
   paramètres, un facteur 41, fait gagner moins d'un point d'exactitude. Le
   facteur limitant n'est pas la capacité du réseau mais l'information contenue
   dans trois grandeurs seulement. On retient donc le plus petit modèle utile.
2. **L'exactitude décroît avec le nombre de classes**, et l'exactitude
   équilibrée bien plus vite encore : de 0,53 à 3 classes à 0,26 à 9 classes.
   Les classes rares (neige, orage) ne sont pratiquement jamais prédites.
3. **La configuration à 4 classes est celle qui apporte le plus par rapport à
   sa ligne de base** (+0,161 contre +0,112 à 3 classes), tout en distinguant
   quatre conditions utiles. C'est celle qui est déployée.

L'exactitude brute est trompeuse ici : les classes sont très déséquilibrées,
de 46 036 relevés « partiellement nuageux » à 98 « averses de neige ». La
colonne « majorité » donne le score qu'obtiendrait un classifieur répondant
toujours la classe la plus fréquente ; c'est à elle qu'il faut comparer.

Trois libellés du sujet ne sont pas exploitables et sont écartés : « Vent fort »
n'a pas de code COCO (il faudrait la vitesse du vent), et grêle, éclairs et
pluie verglaçante totalisent moins de 30 relevés sur onze ans.

## Enchaînement des scripts

```
py -3.12 1_donnees.py        # telecharge Meteostat -> meteostat_paris.csv
py -3.12 2_entrainement.py   # entraine, compare, ecrit modele_meteo.keras + .json
py -3.12 3_export_onnx.py    # exporte en ONNX et verifie la fidelite
py -3.12 4_generer_c.py      # genere Core/Inc/modele_ia.h et Core/Src/modele_ia.c
```

Les fichiers C sont **générés** : ne pas les éditer à la main, relancer
`4_generer_c.py` après tout réentraînement.

Dépendances : `py -3.12 -m pip install tensorflow meteostat pandas scikit-learn
tf2onnx onnx onnxruntime`. TensorFlow n'existe pas pour Python 3.14, d'où
l'usage explicite de Python 3.12.

## Vérification du portage

L'export ONNX est contrôlé contre le modèle Keras sur 500 entrées :

```
ecart absolu maximal sur les probabilites : 1.192e-07
accord sur la classe predite              : 100.0000%
```

Le code C est contrôlé sur la carte au démarrage, contre des références
`onnxruntime`, avec une tolérance de 1e-4. Le message `Autotest du modele : OK`
atteste que la normalisation et les poids ont été reportés correctement — c'est
la première cause d'écart entre un modèle Python et son portage embarqué.

## §3.c — résultats mesurés sur la carte

Relevés sur la NUCLEO-N657X0-Q, cœur Cortex-M55 à 600 MHz, modèle à 4 classes.

**Temps d'exécution d'une inférence :**

```
22 235 cycles = 37 us
```

Valeur rigoureusement constante d'un cycle à l'autre sur l'ensemble des
relevés, ce qui s'explique par un réseau de taille fixe, sans branchement
dépendant des données et entièrement en cache après le premier passage.

**Fidélité du portage, contrôlée sur les données réelles.** Les entrées lues
par la carte ont été rejouées sur PC avec `onnxruntime` et comparées aux
probabilités qu'elle a affichées :

| Entrées (T °C, RH %, P hPa) | Classe prédite | Écart carte / PC |
| --- | --- | --- |
| 28,82 / 35,24 / 997,00 | Précipitations | 0,0100 pt |
| 28,85 / 35,26 / 996,99 | Précipitations | 0,0081 pt |
| 28,79 / 35,04 / 997,02 | Précipitations | 0,0053 pt |
| 28,83 / 35,02 / 996,98 | Précipitations | 0,0086 pt |

L'écart maximal, 0,01 point de pourcentage, correspond exactement à l'arrondi
d'affichage à deux décimales de la carte. Le portage C est donc numériquement
identique au modèle exporté.

**Limite d'interprétation.** La carte est en intérieur : les 28,8 °C et 35 %RH
relevés sont ceux de la pièce, alors que le modèle a été entraîné sur des
relevés extérieurs. Seule la pression, 997 hPa, est commune à l'intérieur et à
l'extérieur. Les classes prédites ne sont donc pas à lire comme une prévision
météo valide. Cela ne gêne pas la comparaison demandée au §3.c : les solutions
embarquée et Cloud reçoivent les mêmes entrées et subissent donc le même biais.

## Quantification : la bonne pratique ne s'applique pas ici

Le polycopié du module (§2.3) présente la quantification comme une technique
« obligatoire, pas optionnelle », et le §3.1 décrit la chaîne
TensorFlow → TFLite → X-CUBE-AI. Nous l'avons appliquée et mesurée
(`5_quantification.py`).

| Variante | Octets | Exactitude | Équilibrée |
| --- | --- | --- | --- |
| TFLite float32 | 3 140 | 0,4847 | 0,4179 |
| TFLite int8 dynamique | 3 140 | 0,4847 | 0,4179 |
| TFLite int8 intégral | **3 496** | 0,4838 | 0,4170 |
| Code C généré (poids seuls) | **944** | 0,4847 | 0,4179 |

La quantification intégrale **augmente** la taille du fichier au lieu de la
réduire. Le modèle ne pesant que 944 octets de poids, le surcoût du format
TFLite — métadonnées, descripteurs d'opérateurs, échelles et zero-points —
dépasse largement ce que la quantification économise. Le coût en exactitude
est en revanche négligeable, un millième.

Le conseil du polycopié reste juste pour des modèles de plusieurs centaines de
kilo-octets. À cette échelle, il ne s'applique pas. C'est une conclusion
mesurée, pas une opinion.

## L'IA est-elle justifiée ici ? La mesure

Le polycopié pose la question centrale (§2.5) :

> « l'IA ne doit être utilisée que si elle apporte un bénéfice clair par
> rapport à une méthode déterministe »

Il la tranche pour le sinus — la fonction native gagne, 17 mA contre 36 mA —
et affirme que pour Meteostat l'IA est justifiée car « la relation entre
température, humidité et pression n'est pas analytique » (§3.4). Cette
affirmation n'est pas vérifiée dans le polycopié. Nous l'avons mise à
l'épreuve (`6_baseline.py`), avec des arbres de décision entraînés sur les
mêmes données et évalués sur le même jeu de test.

| Méthode | Exactitude | Équilibrée | Coût par inférence |
| --- | --- | --- | --- |
| Arbre profondeur 1 | 0,4068 | 0,3592 | 1 comparaison |
| Arbre profondeur 2 | 0,4575 | 0,3962 | 2 comparaisons |
| Arbre profondeur 3 | 0,4585 | 0,3976 | 3 comparaisons |
| Arbre profondeur 4 | 0,4711 | 0,4000 | 4 comparaisons |
| **Arbre profondeur 5** | **0,4816** | **0,4164** | **5 comparaisons** |
| Arbre profondeur 8 | 0,4841 | 0,4167 | 8 comparaisons |
| **Réseau 3-16-8-4** | **0,4847** | **0,4179** | **208 MAC + 4 exp** |

Le réseau de neurones dépasse un arbre de profondeur 8 de **0,0006 point**, et
un arbre de profondeur 5 de **0,0031 point**, pour environ quarante fois plus
d'opérations arithmétiques.

Trois conséquences :

1. **Sur ce problème, le réseau de neurones ne se justifie pas.** Le
   polycopié affirmait l'inverse pour Meteostat ; la mesure le contredit.
2. **L'arbre est interprétable.** « Si l'humidité dépasse 86,5 %, alors
   précipitations » se lit et se discute. Les 236 poids du réseau, non.
3. **L'arbre n'a pas besoin de normalisation.** Il travaille directement sur
   les grandeurs physiques, ce qui supprime la principale source d'erreur de
   portage.

L'arbre retenu est généré en C dans `Core/Src/baseline_arbre.c` et s'exécute
sur la carte à côté du réseau, à chaque cycle, avec sa propre mesure de
cycles. La comparaison n'est donc pas théorique : elle est affichée sur la
console du prototype.

### Mesure sur la carte

Relevé sur la NUCLEO-N657X0-Q, Cortex-M55 à 600 MHz :

```
<Inference IA>     classe = Partiellement nuageux   duree : 22358 cycles = 37.26 us
<Reference arbre>  classe = Partiellement nuageux   duree :   332 cycles =  0.55 us
    le reseau coute 67 fois plus de cycles
    accord des deux methodes : oui
```

| Méthode | Cycles | Durée | Exactitude |
| --- | --- | --- | --- |
| Arbre profondeur 5 | **332** | 0,55 µs | 0,4816 |
| Réseau 3-16-8-4 | **22 358** | 37,26 µs | 0,4847 |
| Rapport | **× 67** | × 67 | **+0,0031** |

Les deux méthodes prédisent la même classe à chaque cycle sur l'ensemble des
relevés observés. Le réseau consomme 67 fois plus de cycles — donc, à
fréquence égale, environ 67 fois plus d'énergie de calcul — pour trois
millièmes de point d'exactitude obtenus hors ligne.

C'est la réponse mesurée à la question du §2.5 du polycopié, sur le cas que
celui-ci donnait précisément comme favorable à l'IA.

## Changer de modèle

Pour brancher un autre modèle, par exemple généré avec X-CUBE-AI, voir
[INTERFACE_MODELE.md](INTERFACE_MODELE.md). Un seul fichier est à modifier.

## §3.d — mesure de consommation

Deux binaires strictement identiques, à l'exécution du réseau près :

```
make PARTIE=4             -> binaires/TP3_partie4_avec_IA.bin   (47 456 o)
make PARTIE=4 TEMOIN=1    -> binaires/TP3_partie4_temoin.bin    (44 576 o)
```

En mode témoin, l'autotest de démarrage est également désactivé, puisqu'il
exécute lui aussi le réseau : le programme témoin est donc réellement exempt
de toute inférence.

Mesurer le courant au jumper d'alimentation pour chacun des deux programmes ;
la différence est imputable à l'inférence. **Lire la documentation de la carte
et valider la manipulation avec l'encadrant avant de la réaliser**, comme le
rappelle le sujet.
