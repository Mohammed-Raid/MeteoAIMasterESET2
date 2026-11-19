# Pitch de 3 minutes — ETRS923

Trame pour le critère **C_ORAL**, construite pour toucher aussi **P_TEDS**
(transition écologique) et **E_IMPLEMENT** (prototype fonctionnel).

L'angle : nous ne présentons pas « on a fait marcher une IA ». Nous
présentons **une mesure que personne d'autre n'aura faite**, qui répond à la
question posée par le cours lui-même.

---

## 0:00 — 0:20 · L'accroche

> « Le polycopié de ce module pose une question : une IA embarquée ne se
> justifie que si elle apporte un bénéfice clair face à une méthode
> déterministe. Pour le sinus, la réponse est non — 17 mA contre 36 mA. Pour
> Meteostat, le cours affirme que l'IA est justifiée. Nous avons voulu le
> vérifier. »

Ne pas annoncer le résultat. Le garder pour 2:00.

## 0:20 — 0:50 · Le prototype, en marche

Carte branchée, **navigateur projeté sur l'adresse IP de la carte** — le
tableau de bord est servi par le STM32 lui-même, pas par un service externe.

> « Cette page n'est hébergée nulle part. C'est la carte qui la sert. »

**Le geste à faire, vers 2:20** : couper le **Wi-Fi du PC** en plein pitch,
pas le câble de la carte. La carte est branchée sur le PC, qui lui partage
sa connexion : la page arrive par ce câble, Internet par le Wi-Fi. Wi-Fi
coupé, la colonne Cloud tombe, la page reste, l'inférence locale continue en
100 µs. Rallumer le Wi-Fi. C'est la démonstration de la thèse du module en
cinq secondes. Débrancher le câble, lui, couperait aussi la page. Voir
`DEMO.md`.

> « Sept capteurs MEMS lus en I2C. Température, humidité et pression
> partent dans un réseau de neurones entraîné sur dix ans de relevés de
> douze stations autour du campus, 920 000 heures. Inférence en
> 100 microsecondes sur le Cortex-M55. »

Montrer la ligne `[TEST] 8/8 identiques, ecart max = 0 e-6`.

> « À chaque démarrage, la carte rejoue huit cas de test calculés en Python
> et retrouve les mêmes probabilités, au millionième près. »

## 0:50 — 1:30 · La compétence : on ne fait pas confiance, on vérifie

> « Au premier essai, cinq des six capteurs annonçaient un WHO_AM_I faux.
> Plutôt que de conclure à une panne, nous avons lu les identifiants : chaque
> valeur “fausse” était l'identifiant **correct** d'un autre composant. Nous
> n'avions pas la carte IKS01A3 du sujet, mais une IKS4A1. »

> « Validation croisée : cinq thermomètres de technologies différentes
> s'accordent à 1,2 °C, et les trois accéléromètres mesurent la pesanteur à
> moins de 2 % près. Ce ne sont pas des chiffres qui s'affichent, ce sont des
> mesures justes. »

> « Et notre première version prédisait la pluie en permanence. Le modèle
> apprend sur la pression ramenée au niveau de la mer ; le capteur, lui,
> mesure 27 hPa de moins à notre altitude. Pour le réseau, c'était une
> dépression perpétuelle. Une ligne de correction, la même côté carte et
> côté Cloud, et le problème a disparu. »

## 1:30 — 2:20 · Le résultat · **le cœur du pitch**

> « Revenons à la question de départ. Nous avons entraîné un arbre de
> décision sur exactement les mêmes données, le même découpage et la même
> année de test que le réseau. »

Afficher le tableau :

| Méthode | macro-F1 | balanced accuracy | Mesuré sur la carte |
| --- | --- | --- | --- |
| Arbre de décision, profondeur 5 | **0,487** | 0,602 | **~300 cycles** |
| Réseau de neurones 4-32-16-4 | 0,484 | **0,632** | **~62 000 cycles** |

> « L'arbre fait jeu égal : il gagne en macro-F1, le réseau garde trois
> points de balanced accuracy. Pour **environ 190 fois plus de cycles**,
> mesurés sur la carte, pas estimés. Sur ce problème, le bénéfice du réseau
> est marginal — et le cours affirmait qu'il était justifié. »

Pointer la ligne `le reseau coute ... fois plus` sur la console en direct.
Ne pas affirmer que les deux méthodes sont toujours d'accord : elles ne le
sont pas, la ligne `accord des deux methodes` le montre.

> « Mieux : l'arbre est lisible. “Si l'humidité reste sous 75 % et la
> pression sous 1014 hPa…” Un humain suit la décision. Le réseau, lui, est
> 756 poids opaques. »

## 2:20 — 2:45 · La démarche de sobriété

> « Nous avons aussi mesuré la quantification int8, que le cours présente
> comme obligatoire. Les poids passent de 3 Ko à 0,75 Ko, mais le fichier
> TFLite ne perd que 536 octets, de 5 280 à 4 744 : à cette échelle, le
> format pèse plus que les poids. »

> « Pour l'énergie, nous livrons deux programmes identiques au réseau près.
> Mesuré sur la carte : 103,9 µs par classification avec le réseau, 0,9 µs
> sans. Le réseau représente 99 % du temps de calcul. »

## 2:45 — 3:00 · La chute

> « Notre conclusion n'est pas que l'IA embarquée est inutile. C'est qu'elle
> doit être **mesurée avant d'être adoptée**. Nous avons construit la chaîne
> complète, de Meteostat jusqu'au Cortex-M55 — et la même rigueur qui nous a
> fait identifier la bonne carte capteurs nous fait dire que, sur ce
> problème, cinq comparaisons suffisaient. »

---

## Ce qu'il faut avoir sous la main

- La carte branchée, partie 4 déjà chargée, console ouverte. **Testé avant
  de passer.** Le binaire `binaires/partie4.bin` permet de
  redémarrer sans recompiler.
- Le tableau arbre contre réseau, projeté.
- `TP3_IA/export/etude_compromis.csv` et `TP3_IA/baseline_arbre.csv`
  ouverts, au cas où on demande les chiffres bruts.

## Questions probables, et les réponses

**« Votre accuracy est sous celle d'une réponse constante. »**
Oui, 0,560 contre 0,568, et c'est voulu. Répondre toujours « Nuageux » donne
0,568 d'accuracy mais ne détecte jamais ni la pluie ni la neige : sa balanced
accuracy est de 0,25. Le nôtre détecte 71 % des heures de pluie et 73 % des
heures de neige, pour une balanced accuracy de 0,63. Avec quatre grandeurs
seulement, le plafond est bas : multiplier la taille du réseau par 40 ne fait
pas gagner un point.

**« Pourquoi pas X-CUBE-AI ? »**
Le sujet le demande et il supporte bien le N6 depuis la version 10.0.0. Son
téléchargement exige un compte ST. Nous avons généré le C directement, et
surtout nous avons **prouvé** l'équivalence avec l'ONNX plutôt que de la
supposer. L'interface est documentée pour basculer sur X-CUBE-AI sans toucher
au reste du code.

**« Vos mesures de température sont celles de la salle, pas de la météo. »**
Exact, et c'est écrit dans notre documentation. Seule la pression traverse les
murs. Cela ne fausse pas la comparaison embarqué contre Cloud, qui reçoivent
les mêmes entrées, mais cela interdit de lire les classes comme une prévision.

**« Qu'est-ce qui se passe si un capteur tombe ? »**
Chaque lecture teste son code de retour. Une mesure invalide affiche
`ERREUR I2C` et le cycle est sauté plutôt que de classer sur une valeur
périmée.
