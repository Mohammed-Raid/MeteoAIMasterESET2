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

**Le geste à faire, vers 2:20** : débrancher le câble réseau en plein pitch.
La colonne Cloud tombe, la page reste, l'inférence locale continue à 37 µs.
Rebrancher. C'est la démonstration de la thèse du module en cinq secondes.

> « Sept capteurs MEMS lus en I2C. Température, humidité, pression partent
> dans un réseau de neurones entraîné sur onze ans de relevés Meteostat,
> 142 000 observations. Inférence en 37 microsecondes sur le Cortex-M55. »

Montrer la ligne `Autotest du modele : OK`.

> « Cette ligne vérifie, à chaque démarrage, que le portage C donne les mêmes
> sorties que le modèle Python, à 1e-4 près. »

## 0:50 — 1:30 · La compétence : on ne fait pas confiance, on vérifie

> « Au premier essai, cinq des six capteurs annonçaient un WHO_AM_I faux.
> Plutôt que de conclure à une panne, nous avons lu les identifiants : chaque
> valeur “fausse” était l'identifiant **correct** d'un autre composant. Nous
> n'avions pas la carte IKS01A3 du sujet, mais une IKS4A1. »

> « Validation croisée : cinq thermomètres de technologies différentes
> s'accordent à 1,2 °C, et les trois accéléromètres mesurent la pesanteur à
> moins de 2 % près. Ce ne sont pas des chiffres qui s'affichent, ce sont des
> mesures justes. »

## 1:30 — 2:20 · Le résultat · **le cœur du pitch**

> « Revenons à la question de départ. Nous avons entraîné un arbre de
> décision sur exactement les mêmes données et le même jeu de test. »

Afficher le tableau :

| Méthode | Exactitude | Mesuré sur la carte |
| --- | --- | --- |
| Arbre de décision, profondeur 5 | 0,4816 | **332 cycles — 0,55 µs** |
| Réseau de neurones 3-16-8-4 | 0,4847 | **22 358 cycles — 37,26 µs** |

> « Le réseau gagne trois millièmes d'exactitude. Trois millièmes. Pour
> **soixante-sept fois plus de cycles**, mesurés sur la carte, pas estimés.
> Et sur tous les relevés observés, les deux méthodes prédisent la même
> classe. Sur ce problème, l'IA ne se justifie pas — et le cours affirmait
> l'inverse. »

Pointer la ligne `accord des deux methodes : oui` sur la console en direct.

> « Mieux : l'arbre est lisible. “Si l'humidité dépasse 86 %, alors
> précipitations.” Un humain comprend la décision. Le réseau, lui, est 236
> poids opaques. »

## 2:20 — 2:45 · La démarche de sobriété

> « Nous avons aussi testé la quantification int8, que le cours présente
> comme obligatoire. Résultat mesuré : le modèle **grossit**, de 3 140 à
> 3 496 octets, parce que le surcoût du format dépasse les 944 octets de
> poids. À cette échelle, la bonne pratique ne s'applique pas. »

> « Nous livrons deux binaires identiques à l'inférence près, pour mesurer au
> jumper le coût énergétique exact du réseau. »

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
- `TP3_IA/comparaison.csv` et `TP3_IA/baseline.csv` ouverts, au cas où on
  demande les chiffres bruts.

## Questions probables, et les réponses

**« Pourquoi seulement 48 % d'exactitude ? »**
Parce que la référence n'est pas 100 %, c'est 32,4 % — le score d'un
classifieur qui répondrait toujours la classe majoritaire. Nous gagnons 16
points sur cette référence. Et avec trois grandeurs seulement, le plafond est
bas : nous l'avons vérifié en multipliant la taille du réseau par 41 sans
gagner un point.

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
