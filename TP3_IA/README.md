# TP3 ETRS923 — Cloud versus Edge AI

Un seul modèle, exécuté à deux endroits : dans le Cloud (MATLAB sur
ThingSpeak) et sur la carte NUCLEO-N657X0-Q. C'est ce qui rend la comparaison
Cloud/Edge honnête : les deux solutions calculent la même chose, seuls le
lieu et le chemin des données changent.

## Qui a fait quoi

| Élément | Auteur |
| --- | --- |
| `TP3_Meteo_Colab.ipynb` — données, entraînement, étude du compromis, exports | binôme (partie IA) |
| `matlab/meteo_onnx_test.m`, `matlab/ThingSpeak_MeteoAnalysis.m` | binôme (partie IA) |
| `Core/Src/meteo_ia.c`, `Core/Src/meteo_talkback.c` — prétraitement, inférence, Talkback | binôme (partie IA) |
| Capteurs, intégration sur la carte, client ThingSpeak, tableau de bord, mesures | partie capteurs |
| `executer_notebook_local.py`, `generer_c.py`, `baseline_arbre.py` | partie capteurs |

## État de chaque point du sujet

**validé** = exécuté et vérifié ; **écrit** = code présent mais jamais
exécuté faute de compte ou de matériel ; **à faire** = absent.

| Point du sujet | État | Où |
| --- | --- | --- |
| §1.a — modèle TensorFlow sur Meteostat | validé | notebook, `export/` |
| §1.b — compromis classes / taille / accuracy, hyperparamètres | validé | `export/etude_compromis.csv` |
| §2.a — TensorFlow → ONNX | validé, écart 1,2e-07 | `export/meteo_model.onnx` |
| §2.a/b — ONNX → MATLAB, inférence | **écrit**, jamais exécuté | `matlab/meteo_onnx_test.m` |
| §2.c — mesures de la carte sur ThingSpeak, résultat sur un canal | **écrit**, jamais exécuté | `cloud_thingspeak.c`, `matlab/ThingSpeak_MeteoAnalysis.m` |
| §2.d — résultat affiché sur la carte via Talkback | **écrit**, jamais exécuté | `meteo_talkback.c`, `partie5_cloud.c` |
| §3.a — ONNX → C | validé, **sans X-CUBE-AI** (voir plus bas) | `generer_c.py` → `Core/Src/modele_ia.c` |
| §3.b — inférence en C | validé sur la carte, autotest 8/8 | `Core/Src/meteo_ia.c` |
| §3.c — capteurs réels et temps d'exécution | validé sur la carte, ~103 µs | `Core/Src/partie4_ia.c` |
| §3.c — comparaison avec le Cloud | **écrit**, dépend du §2 | `Core/Src/partie5_cloud.c` |
| §3.d — composants coupés, programme témoin | validé sur la carte | `make PARTIE=4 CONSO=1 [TEMOIN=1]` |
| §3.d — mesure du courant | **à faire** | manipulation, avec les encadrants |

Ce qui manque pour le §2 : un compte ThingSpeak, MATLAB Online, et la carte
branchée sur un réseau qui lui donne accès à Internet.

## Le modèle

Entraîné par le notebook sur 12 stations Meteostat à moins de 120 km du campus
du Bourget-du-Lac, de 2016 à 2025 (922 794 heures), dont la station de
Chambéry-Aix voisine du campus.

- **Entrées**, dans cet ordre : température (°C), humidité (%RH), pression
  ramenée au niveau de la mer (hPa), tendance de pression sur 3 h (hPa).
- **Classes** : Clair, Nuageux, Pluie, Neige.
- **Architecture** : `4 → 32 → 16 → 4`, ReLU, dropout 0,1, softmax ;
  756 paramètres, 3 Ko en float32.
- **Découpage temporel** : apprentissage 2016–2023, validation 2024, test
  2025. Un découpage aléatoire mettrait des heures quasi identiques dans
  l'apprentissage et le test, et gonflerait les scores.
- **Poids de classes** « balanced » adoucis par une racine, pour que la pluie
  et la neige, rares, soient apprises.

Résultat sur 2025, année jamais vue :

| | accuracy | balanced accuracy | macro-F1 |
| --- | --- | --- | --- |
| Réseau | 0,560 | 0,631 | 0,484 |
| Réponse constante « Nuageux » | 0,568 | 0,250 | ≈ 0,18 |

**L'accuracy est sous la référence majoritaire, et c'est voulu** : les poids
de classes échangent un peu d'accuracy contre la détection des classes rares
(rappel 0,71 sur la pluie, 0,73 sur la neige, au lieu de presque 0). La
métrique à lire est la balanced accuracy ou la macro-F1. Présenter
l'accuracy seule serait trompeur.

### §1.b — ce que dit l'étude du compromis

| Facteur | Constat (macro-F1) |
| --- | --- |
| Taille du réseau | 76 paramètres : 0,486 ; 756 : 0,488 ; 2 996 : 0,486. **Aucun effet.** |
| Nombre de classes | 2 : 0,709 ; 3 : 0,564 ; 4 : 0,483 ; 5 : 0,451 ; 8 : 0,290 |
| Entrées | pression seule : 0,356 ; T/HR/P : 0,487 ; + tendance 3 h : 0,486 ; + point de rosée et heure : 0,512 |

Le facteur limitant est l'information contenue dans les capteurs, pas la
taille du réseau. Et contrairement à ce qu'on attendait, **la tendance de
pression sur 3 h n'apporte rien** sur ces données ; c'est le point de rosée
et l'heure qui aident. Le modèle déployé garde la tendance, choix du
notebook, sans perte mesurable.

## Côté carte

`meteo_ia.c` choisit son moteur à la compilation :

- si le code généré par **X-CUBE-AI** (`network.h`) est dans le projet, il
  l'utilise, comme le prévoit le sujet ;
- sinon il exécute `modele_ia.c`, produit par `generer_c.py` à partir du même
  modèle. Le téléchargement de X-CUBE-AI exige un compte ST.

La normalisation, le chronométrage et l'autotest sont communs aux deux. Sur
la carte, sans X-CUBE-AI :

```
[IA] Reseau pret (code C genere, sans X-CUBE-AI) : 4 entrees, 4 classes
[TEST] 8/8 identiques, ecart max = 0 e-6
entrees : T= 23.64 degC  RH= 46.28 %  P mesuree= 982.33  P mer= 1009.20 hPa
<Inference IA> classe = Nuageux (reseau seul : 101 us)
```

**La correction de pression est indispensable.** Le capteur mesure la
pression à son altitude, environ 27 hPa sous la pression au niveau de la mer
sur laquelle le modèle a appris. Sans correction, le modèle voit une
dépression permanente et prédit de la pluie : c'était le défaut de notre
première version (voir `v1_paris/`). L'altitude se règle dans `main.h`
(`ALTITUDE_CARTE_M`, 235 m au campus) et doit être reportée dans le script
ThingSpeak.

**La tendance sur 3 h vaut 0 pendant les 3 premières heures** après le
démarrage, le temps d'accumuler l'historique. La console le signale.

## Le réseau est-il justifié ? La référence déterministe

Le polycopié (§2.5) demande de vérifier que l'IA apporte un bénéfice clair
face à une méthode déterministe, et affirme que c'est le cas pour Meteostat.
`baseline_arbre.py` entraîne des arbres de décision sur exactement les mêmes
données, le même découpage et les mêmes poids de classes :

| Méthode | accuracy | balanced accuracy | macro-F1 | Coût |
| --- | --- | --- | --- | --- |
| Réseau | 0,560 | **0,632** | 0,484 | ~62 000 cycles |
| Arbre de profondeur 5 | **0,564** | 0,602 | **0,487** | ~300 cycles |

L'arbre égale le réseau en macro-F1 et en accuracy, le réseau garde trois
points de balanced accuracy, pour environ **190 fois plus de cycles** mesurés
sur la carte. L'arbre tourne à côté du réseau à chaque cycle, en parties 4
et 5 ; la console affiche le rapport de cycles et l'accord des deux.

## §3.d — mesure de consommation

Méthode du guide : une classification ne se voit pas à l'ampèremètre, on la
répète 100 000 fois (la rafale), puis 10 s de repos donnent un niveau de
référence. Le programme témoin exécute la même boucle — normalisation, copie,
argmax, chronométrage — **sauf le réseau**. LEDs éteintes, capteurs lus une
seule fois avant la première rafale, console muette pendant la rafale.

```
make PARTIE=4 CONSO=1            -> binaires/partie4_conso_avec_IA.bin
make PARTIE=4 CONSO=1 TEMOIN=1   -> binaires/partie4_conso_temoin.bin
```

Sous STM32CubeIDE : `MESURE_CONSO` et `TEMOIN_SANS_IA` dans `main.h`, sous
`PARTIE_TP`.

Mesuré sur la carte :

| Programme | Durée d'une rafale | Par classification |
| --- | --- | --- |
| avec IA | 10 388 ms | 103,88 µs |
| témoin | 86 ms | 0,86 µs |
| **réseau seul** | | **103,0 µs** |

Les deux rafales n'ont pas la même durée : on compare les énergies.

```
E_classification = (I_ia x T_ia - I_temoin x T_temoin) x V / 100 000
```

Point de mesure d'après le guide du binôme (manuel UM3417, §7.4.4) :
**VDDCORE**, cœur CPU et NPU, sur CN12 broches 3–4, après retrait de R40,
shunt de 50 mΩ ; mesurer la **tension** aux bornes du shunt. Retirer R40 est
une soudure sur la carte : **à valider avec les encadrants avant**, comme le
demande le sujet. À défaut, un wattmètre USB sur le câble ST-LINK mesure toute
la carte, moins précis mais sans soudure.

## Cloud — ce qu'il reste à brancher

La marche à suivre complète, clic par clic, est dans [THINGSPEAK.md](THINGSPEAK.md). En résumé :

1. Deux canaux ThingSpeak et un TalkBack. Canal de mesures, rempli par la
   carte : field1 température, field2 humidité, field3 pression **mesurée**
   (le script la corrige), field4 classe Edge, field5 confiance Edge en %,
   field6 durée Edge en µs. Canal de résultats, rempli par MATLAB : field1
   classe, field2 confiance, field3 durée d'inférence en ms, field4 âge de la
   mesure, field5 à 8 probabilités.
2. `export/ThingSpeak_MeteoAnalysis_pret.m` dans une application MATLAB
   Analysis : c'est le script du binôme avec les poids déjà insérés par
   `preparer_thingspeak.py`. Il calcule le réseau sans Deep Learning Toolbox,
   indisponible dans ThingSpeak. Son calcul a été vérifié hors ligne contre
   les vecteurs de test : 8/8 classes identiques, écart maximal 2,4e-5.
3. Une application React qui déclenche le script à chaque nouvelle mesure.
4. Les clés de la carte dans `Core/Inc/config_cloud_local.h`, ignoré par Git
   (modèle dans `config_cloud.h`).

Le verdict Cloud lu par la carte correspond à la mesure de l'envoi précédent,
puisque le script tourne après l'écriture : il a 16 à 32 s de retard.

## Régénérer

```
py -3.12 TP3_IA/executer_notebook_local.py   # le notebook, sans Colab : export/
py -3.12 TP3_IA/generer_c.py                 # Core/Src/modele_ia.c
py -3.12 TP3_IA/baseline_arbre.py            # Core/Src/baseline_arbre.c
py -3.12 TP3_IA/preparer_thingspeak.py       # export/ThingSpeak_MeteoAnalysis_pret.m
copy TP3_IA\export\meteo_model_params.h Core\Inc\
```

Le notebook s'exécute aussi tel quel dans Colab. En local, une adaptation est
nécessaire et se trouve dans `executer_notebook_local.py`, pas dans le
notebook : la version 2.1 de Meteostat bloque les requêtes horaires de plus de
3 ans. Sans la lever, chaque station est ignorée en silence et l'entraînement
échoue faute de données.

`export/meteo_model_params.h` est généré : ne pas le modifier à la main. Une
seule correction a été faite dans le générateur du notebook :
`METEO_CLASS_NAMES` est déclaré `static const char *const`, sans quoi chaque
fichier qui inclut l'en-tête sans utiliser les noms produisait un avertissement.

## Limites à connaître

- **La carte est à l'intérieur.** Température et humidité sont celles de la
  salle, pas de la météo. Seule la pression est commune à l'intérieur et à
  l'extérieur. Cela n'affecte pas la comparaison Cloud/Edge, qui porte sur les
  mêmes entrées, mais interdit de lire les classes comme une prévision.
- **HTTP et non HTTPS** vers ThingSpeak : lwIP n'a pas de pile TLS dans ce
  projet. La clé d'API circule en clair. Et **HTTP/1.0**, pas 1.1 : en 1.1,
  ThingSpeak découpe la réponse Talkback en morceaux, et la carte lirait la
  taille du morceau au lieu de la commande.
- **Charger en ligne de commande** exige un serveur GDB neuf à chaque
  chargement, connecté sous reset et sur le port d'accès 1 du N6
  (`-k -m 1`). Recharger sans reset bloque le nouveau programme avant même
  l'ouverture de la console. STM32CubeIDE fait cela de lui-même.

## Première version

`v1_paris/` contient notre premier pipeline : 3 stations parisiennes, entrées
T/HR/P, découpage aléatoire. Il est conservé pour l'historique et remplacé
pour deux raisons : la pression du capteur n'était pas ramenée au niveau de la
mer, et le découpage aléatoire surestimait les scores.
