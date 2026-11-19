# Mise en place de ThingSpeak — TP3 §2

Ce qui est déjà prêt dans le dépôt :

- `export/ThingSpeak_MeteoAnalysis_pret.m` : le script MATLAB de l'application,
  avec les poids du réseau déjà insérés. Il ne reste que six valeurs à remplir.
  Son calcul a été vérifié hors ligne contre les vecteurs de test du notebook :
  8/8 classes identiques, écart maximal 2,4e-5.
- `Core/Inc/config_cloud_local.h` : le fichier des clés de la carte, ignoré par
  Git.
- Le client HTTP de la carte, en HTTP/1.0 : en HTTP/1.1, ThingSpeak découpe la
  réponse Talkback en morceaux, et la carte lirait la taille du morceau au lieu
  de la commande.

Compter une demi-heure. Garder un fichier texte ouvert pour noter les
identifiants et les clés au fur et à mesure.

## 1. Le compte

Aller sur <https://thingspeak.mathworks.com> et se connecter avec le compte
MathWorks **de l'université** : il donne l'accès académique, avec un temps de
calcul MATLAB plus long que le compte gratuit.

## 2. Canal « Mesures STM32 » — rempli par la carte

*Channels → My Channels → New Channel*. Nom : `Mesures STM32`. Cocher et
nommer six champs, **dans cet ordre** :

| Champ | Nom | Contenu envoyé par la carte |
| --- | --- | --- |
| Field 1 | temperature | °C |
| Field 2 | humidite | %RH |
| Field 3 | pression_mesuree | hPa, **brute**, le script la corrige |
| Field 4 | classe_edge | classe prédite par la carte, de 0 à 3 |
| Field 5 | confiance_edge | % |
| Field 6 | duree_edge_us | durée de l'inférence sur la carte, en µs |

*Save Channel*. Noter : **Channel ID**, et dans l'onglet *API Keys* la
**Write API Key** et la **Read API Key**.

## 3. Canal « Meteo IA Cloud » — rempli par MATLAB

*New Channel*. Nom : `Meteo IA Cloud`. Huit champs :

| Champ | Nom |
| --- | --- |
| Field 1 | classe |
| Field 2 | confiance_pct |
| Field 3 | inference_ms |
| Field 4 | age_mesure_s |
| Field 5 | p_Clair |
| Field 6 | p_Nuageux |
| Field 7 | p_Pluie |
| Field 8 | p_Neige |

Noter : **Channel ID** et **Write API Key**.

## 4. Le TalkBack

*Apps → TalkBack → New TalkBack*. Nom : `Meteo STM32`, sans canal de journal.
Noter le **TalkBack ID** (un nombre) et son **API Key**.

## 5. L'application MATLAB Analysis

*Apps → MATLAB Analysis → New → Custom (no starter code) → Create*.
Nom : `Meteo IA Cloud`.

1. Ouvrir `TP3_IA/export/ThingSpeak_MeteoAnalysis_pret.m` dans le Bloc-notes,
   tout copier, et coller dans la zone *MATLAB Code* après l'avoir vidée.
2. Remplir la section CONFIGURATION, en haut. Les identifiants sans
   apostrophes, les clés entre apostrophes :

   | Variable | Valeur |
   | --- | --- |
   | `CANAL_MESURES` | Channel ID de **Mesures STM32** |
   | `CLE_LECTURE` | **Read** API Key de **Mesures STM32** |
   | `CANAL_RESULTAT` | Channel ID de **Meteo IA Cloud** |
   | `CLE_ECRITURE` | **Write** API Key de **Meteo IA Cloud** |
   | `TALKBACK_ID` | TalkBack ID |
   | `TALKBACK_CLE` | API Key du TalkBack |

   Les deux pièges : mettre la Write Key de *Mesures STM32* à la place de sa
   Read Key, ou à la place de la Write Key de *Meteo IA Cloud*.
3. *Save* (pas encore *Run*).

Ne pas toucher à `ALTITUDE_M` : 235 m, la même valeur que dans `main.h`. Si
elle change d'un côté, la changer de l'autre.

## 6. Tester sans la carte

Le script lit les 200 dernières minutes du canal de mesures : il lui faut des
mesures récentes. Coller dans la barre d'adresse du navigateur, avec la
**Write API Key de Mesures STM32** :

```
https://api.thingspeak.com/update?api_key=WRITE_KEY_MESURES&field1=22&field2=45&field3=985
```

La page affiche un nombre : c'est le numéro de la mesure enregistrée. `0`
veut dire refusé, le plus souvent parce que la mesure précédente date de
moins de 15 secondes. Faire trois envois espacés de 20 secondes.

Puis *Save and Run* sous le script. Sortie attendue :

```
Attention : moins de 3 h d'historique, dp3h forcé à 0
Entrées : T=22.0°C HR=45% P=1012.x hPa dP3h=+0.0 hPa
Résultat : <classe> (xx %) | inférence x.xxx ms | âge de la mesure xx s
TalkBack : METEO:<classe>:<confiance> | durée totale du script x.xx s
```

La première ligne est normale avec de fausses mesures. Vérifier ensuite qu'un
point est apparu dans *Meteo IA Cloud*, et que la commande `METEO:...` est
dans *Apps → TalkBack → Meteo STM32*.

## 7. Le déclenchement automatique

*Apps → React → New React* :

- Condition Type : *Numeric* ; Test Frequency : *On Data Insertion* ;
- If channel : *Mesures STM32*, field 1, *is greater than* `-100` (toujours
  vrai) ;
- Action : *MATLAB Analysis* → `Meteo IA Cloud` ;
- Options : *Run action each time condition is met*.

*Save React*. Désormais, chaque envoi de la carte déclenche une
classification dans le Cloud.

## 8. La carte

1. Dans `Core/Inc/config_cloud_local.h`, renseigner la **Write API Key de
   Mesures STM32**, le **TalkBack ID** et la **clé du TalkBack**, puis enlever
   les `//` des trois lignes `#define`.
2. Dans `Core/Inc/main.h`, mettre `PARTIE_TP` à 5, puis recompiler et charger.
   Le binaire ainsi compilé contient tes clés en clair : **ne pas le copier
   dans `binaires/`**, qui est poussé sur GitHub. Les dossiers de compilation
   `build/` et `Debug/` sont, eux, ignorés par Git.
3. Brancher le câble RJ45 sur un réseau qui donne accès à Internet **sans
   page de connexion**. Voir la section suivante : c'est le piège le plus
   probable.

## Le piège : les réseaux à page de connexion

Testé le 19/11/2026 sur le réseau filaire d'une résidence Wifirst. La carte
obtient une adresse par DHCP et résout `api.thingspeak.com`, mais chaque
requête HTTP reçoit :

```
  envoi ThingSpeak : reponse HTTP invalide (statut HTTP 302)
    redirige vers : https://portal.wifirst.net/connect
```

C'est un **portail captif** : tant qu'un appareil ne s'est pas identifié sur
la page de connexion, le réseau redirige tout son trafic web vers cette page.
La carte n'a pas de navigateur, elle ne peut pas s'identifier. Ce n'est pas
un défaut du code : la même requête, envoyée depuis un PC déjà connecté,
reçoit bien la réponse de ThingSpeak.

Trois solutions :

- **Enregistrer la carte auprès du réseau.** La plupart des réseaux de
  résidence permettent d'ajouter un appareil sans navigateur (console, TV)
  par son adresse MAC, depuis l'espace client. Celle de la carte s'affiche au
  démarrage : `[ETH] adresse MAC 00:80:E1:2E:61:54`.
- **Partager la connexion du PC.** Le PC, déjà identifié en Wi-Fi, partage sa
  connexion sur son port Ethernet (*Paramètres → Réseau et Internet → Point
  d'accès mobile*, ou le partage de connexion de la carte Wi-Fi), et la carte
  se branche sur le PC.
- **Un autre réseau** : une box à la maison, ou une prise de salle de TP
  sans portail.

Sur la console, toutes les 16 secondes :

```
<Communication Reseau>
  envoi ThingSpeak : ok en xxx ms
[CLOUD] Nuageux (46 %), verdict de l'envoi precedent
<Comparaison Cloud / Edge>
  Edge   : Nuageux   104.xx us (62xxx cycles)
  Arbre  : Clair    (reference deterministe)
  Cloud  : Nuageux (46 %), aller-retour HTTP xxx ms, verdict en retard d'un envoi (16 s)
```

Au premier envoi, la file Talkback est encore vide : c'est normal, le script
MATLAB tourne après l'écriture, et son verdict arrive à l'envoi suivant.

## Ce qu'il faut capturer pour le rapport

- §2.c : le canal *Meteo IA Cloud* après quelques heures de fonctionnement.
- §2.d : la console série montrant les lignes `[CLOUD]`.
- §3.c : laisser tourner la carte au moins une demi-journée, puis remplir et
  exécuter la cellule 12 du notebook : accord Cloud/Edge, accuracy face à la
  météo réellement observée à Chambéry, temps de chaque côté.

## Si ça ne marche pas

| Symptôme | Cause probable |
| --- | --- |
| `envoi ThingSpeak : cle d'API absente` | `config_cloud_local.h` encore commenté, ou pas recompilé |
| `reponse HTTP invalide (statut HTTP 302)` suivi de `redirige vers` | portail captif du réseau : voir la section précédente |
| `envoi ThingSpeak : reponse HTTP invalide (statut HTTP 400)` | mauvaise Write Key |
| `envoi ThingSpeak : ecriture refusee` | envoi moins de 15 s après le précédent |
| `resolution DNS impossible` | pas d'accès Internet sur ce réseau |
| `Talkback : reponse HTTP invalide (statut HTTP 401)` | mauvais TalkBack ID ou mauvaise clé |
| `Talkback vide` qui ne change jamais | React pas créé, ou script MATLAB en erreur : regarder sa sortie |
| Le script répond toujours « Pluie » | pression déjà corrigée envoyée, ou altitude différente entre carte et script |
| `Poids absents` dans MATLAB | ne pas coller le script original, mais la version `_pret.m` |
