# Pitch de 3 minutes, en français simple

À dire avec l'image `docs/schema_projet.png` projetée, la carte qui tourne à
côté. Phrases courtes, un chiffre à la fois. **A** = toi (capteurs, carte,
démo), **B** = Amine (modèle, Cloud). Si tu passes seul, tu dis tout.

Version détaillée, avec les chiffres bruts : `PITCH.md`.

---

## 0:00 – 0:20 · La question (A)

> « Notre question est simple : pour reconnaître le temps qu'il fait, vaut-il
> mieux faire calculer l'IA **directement sur la carte**, ou l'envoyer
> **dans le Cloud** ? On a construit les deux, avec le même modèle, et on les
> a mesurés. »

## 0:20 – 0:50 · ① Mesurer (A)

Montrer la colonne ① de l'image, puis la carte.

> « La carte STM32 lit sept capteurs. L'IA en utilise trois : la
> température, l'humidité et la pression. »

> « Une anecdote : au début, notre IA annonçait de la pluie en permanence.
> Le modèle a appris avec la pression **au niveau de la mer**, mais ici on
> est à 235 mètres, donc le capteur mesure moins. Pour l'IA, c'était une
> tempête permanente. On corrige la pression avant de la donner au modèle,
> et le problème a disparu. »

## 0:50 – 1:20 · ⓪ Le modèle (B)

Montrer la bande violette en haut de l'image.

> « On a entraîné le modèle sur Google Colab, avec dix ans de météo réelle :
> douze stations autour du Bourget-du-Lac, 920 000 heures. Il reconnaît
> quatre temps : clair, nuageux, pluie, neige. C'est un petit réseau de
> neurones, 756 paramètres. »

> « Il ne devine pas au hasard : en moyenne, il reconnaît bien chaque type
> de temps 63 fois sur 100. Le hasard ferait 25. Avec seulement trois
> capteurs, on ne peut pas faire beaucoup mieux : on ne voit pas les
> nuages. »

## 1:20 – 2:00 · ② et ③ Les deux façons de décider (A puis B)

Montrer la console et le tableau de bord.

> **A** : « Sur la carte, le modèle répond en **105 microsecondes**. Au
> démarrage, la carte rejoue huit tests calculés sur PC et trouve
> exactement les mêmes résultats : ici, 8 sur 8. »

> **B** : « Côté Cloud, la carte envoie ses mesures sur ThingSpeak. Le même
> modèle tourne dans MATLAB, et renvoie son verdict à la carte. Aller-retour :
> environ **450 millisecondes**. »

## 2:00 – 2:30 · Le résultat (A)

Montrer le bas de l'image.

> « Même verdict des deux côtés. Mais la carte est environ **4 000 fois
> plus rapide**. »

**Le geste** : couper le Wi-Fi du PC.

> « Je coupe Internet. Le Cloud ne répond plus. La carte, elle, continue de
> décider. » *(Rallumer le Wi-Fi.)*

## 2:30 – 3:00 · La leçon (B)

> « Dernière chose : on a aussi essayé un simple **arbre de décision**,
> quelques questions du type « l'humidité est-elle au-dessus de 75 % ? ».
> Il fait presque aussi bien que le réseau de neurones, avec environ **190
> fois moins de calcul**. »

> « Notre conclusion : l'IA sur la carte est plus rapide et plus sobre que le
> Cloud. Mais avant de mettre de l'IA, il faut **mesurer** si une méthode
> simple ne suffit pas. Merci. »

---

## Si le prof pose une question

**« Pourquoi pas tout dans le Cloud ? »**
Le Cloud est 4 000 fois plus lent, dépend d'Internet, et envoie les données
ailleurs. La carte décide seule, tout de suite, sans réseau.

**« Pourquoi le Cloud alors ? »**
Pour comparer, c'est la question du TP. Et le Cloud garde l'historique des
mesures, ce qui sert à analyser après coup.

**« Votre IA tourne sur le NPU ? »**
Non, sur le processeur (Cortex-M55). Le modèle est minuscule, 756
paramètres : le processeur le fait en 105 µs. Le NPU sert aux gros modèles,
par exemple pour les images.

**« Pourquoi seulement 63 % ? »**
Trois capteurs ne voient pas le ciel. Un réseau 40 fois plus gros ne gagne
pas un point : la limite vient des capteurs, pas du modèle.

**« La température est celle de la salle, non ? »**
Oui, la carte est à l'intérieur : ça ne compte pas comme une vraie prévision.
Mais la carte et le Cloud reçoivent exactement les mêmes mesures, donc la
comparaison entre les deux reste juste.

**« Et la consommation d'énergie ? »**
Le programme de mesure est prêt : il fait 100 000 classifications d'affilée,
puis se repose. Mesuré sur la carte : 103 µs par classification, dont 99 %
pour le réseau de neurones. La mesure du courant se fait avec l'ampèremètre de
la salle.
