# Fiche de démo — ETRS923, TP3

Deux montages possibles :

- **A. La carte sur la switch de la salle** (prévu). Le réseau de l'école
  n'a pas été testé : il peut ne pas donner d'adresse à la carte, avoir une
  page de connexion, ou imposer un proxy. Le script le dit tout de suite.
- **B. La carte branchée sur le PC**, qui partage son Wi-Fi. C'est le montage
  testé le 19/11/2026 : page et ThingSpeak marchent. C'est le plan B si A
  échoue.

## La veille au soir (10 min)

- [ ] Captures de secours, prises pendant que tout marche : la console avec
      `envoi ThingSpeak : ok` et `[CLOUD] ...`, le tableau de bord, les
      canaux *Mesures STm32* et *Meteo IA Cloud*, le TalkBack.
- [ ] Pour le montage B seulement : le réglage qui empêche Windows de couper
      le Wi-Fi pendant le partage (PowerShell **administrateur**, voir
      `TP3_IA/THINGSPEAK.md`), puis redémarrer le PC.
- [ ] PC chargé, câble USB, **deux** câbles RJ45 (carte et PC) dans le sac.

## Installation en salle, montage A : la switch (5 min)

1. Carte : cavalier **JP2 (BOOT1) en position 2**, câble USB sur le PC,
   câble RJ45 sur la **switch**.
2. PC : **partage de connexion désactivé** (`ncpa.cpl` → clic droit
   **Wi-Fi** → *Propriétés* → *Partage* : case décochée). Sinon le PC
   distribue des adresses à toute la salle.
3. PC branché lui aussi **sur la même switch** en RJ45 : en Wi-Fi, il est
   souvent sur un autre réseau et ne verrait pas la page de la carte.
4. **Fermer Docklight** (le script lit la console), puis, **au moins
   5 minutes avant de passer**, dans PowerShell depuis le dossier du projet :

   ```
   powershell -ExecutionPolicy Bypass -File outils\demo.ps1
   ```

   Le script charge la partie 5 (compilée avec les clés, dans `build/`),
   lit sur la console l'adresse obtenue et le résultat du premier envoi vers
   ThingSpeak, puis ouvre la page de la carte.
5. Lire ce qu'il affiche sous **« Acces a ThingSpeak »** :
   - `envoi ThingSpeak : ok` : tout va bien, rouvrir Docklight sur **COM3,
     115200** ;
   - toute autre ligne : le script dit pourquoi. Passer au montage B.

Le premier verdict Cloud est arrivé environ 2 min 30 après le chargement le
19/11/2026, d'où les 5 minutes d'avance.

## Installation en salle, montage B : le partage de connexion

1. Câble RJ45 de la carte **sur le PC**. PC en Wi-Fi sur le réseau de l'école,
   page de connexion passée s'il y en a une.
2. `ncpa.cpl` → clic droit **Wi-Fi** → *Propriétés* → *Partage* : case
   cochée, connexion domestique **Ethernet**.
3. Fermer Docklight, relancer `outils\demo.ps1`.

Dernier recours si le script échoue : dans `Core/Inc/main.h`, ligne 29,
`#define PARTIE_TP 5`, puis CubeIDE *Build* (marteau) et *Run*, et ouvrir
l'adresse `adresse IP : ...` lue sur la console. Pas testé avec la partie 5.

## Déroulé (ce qu'on montre, dans l'ordre)

| Quand | À l'écran | À dire |
| --- | --- | --- |
| Démarrage | `[TEST] 8/8 identiques` | la carte rejoue les cas de test du notebook, même résultat au millionième |
| 0:20 | tableau de bord | page servie par la carte elle-même, 7 capteurs MEMS |
| 0:50 | `Edge : Clair 105 us`, `Arbre : Clair` | réseau 4-32-16-4 sur le Cortex-M55, l'arbre fait presque aussi bien en 190 fois moins de cycles |
| 1:30 | `envoi ThingSpeak : ok`, `[CLOUD] Clair (49 %)` | même modèle dans MATLAB sur ThingSpeak, verdict renvoyé par TalkBack |
| 2:00 | ligne `Cloud : ... aller-retour HTTP ~450 ms` | **105 µs contre ~450 ms**, environ 4 000 fois plus lent, pour le même verdict |
| 2:20 | couper Internet (voir ci-dessous) | le Cloud tombe, la carte continue de décider |
| 2:40 | page en ligne et son QR code | « scannez : vous suivez la carte en direct sur votre téléphone » |

**La page en ligne** : <https://mohammed-raid.github.io/MeteoAIMasterESET2/>.
Elle lit les canaux ThingSpeak (publics) : verdicts carte et Cloud, frise des
verdicts, nombre d'accords entre les deux, courbes des capteurs, QR code. Elle
marche sans la carte sur le même réseau, mais n'affiche du direct que si la
carte envoie à ThingSpeak. Mise à jour toutes les 15 s.

**Le souffle** : souffler quelques secondes sur la carte fait monter
l'humidité. D'après le modèle, au-delà de 60 % il passe à « Nuageux », vers
75 % à « Pluie ». La page locale le montre en 2 s ; la page en ligne affiche
ensuite « La carte a vu Pluie à … Le Cloud l'a vu … s plus tard ». Pas encore
essayé sur la carte : le tester une fois avant de passer.

Le geste de 2:20 dépend du montage. Il n'a pas été répété : le tester une
fois sur place avant de passer.

- **Montage A** : débrancher le câble RJ45 **de la carte**. La page se fige
  (« Carte injoignable »), mais **Docklight** montre que la carte continue :
  `Edge : Clair 105 us` à chaque cycle, et l'envoi vers ThingSpeak en erreur.
  Montrer Docklight à ce moment-là, pas la page. Rebrancher.
- **Montage B** : couper le **Wi-Fi du PC**. La page reste, la colonne Cloud
  tombe, l'inférence locale continue. Rallumer le Wi-Fi et vérifier que la
  case du partage est toujours cochée.

## Si ça coince

| Symptôme | Que faire |
| --- | --- |
| « La carte n'a pas obtenu d'adresse » | le réseau de la salle ne lui en donne pas : montage B |
| `302`, `redirige vers` | le réseau a une page de connexion : montage B |
| `resolution DNS impossible` | pas d'accès Internet direct (proxy) : montage B |
| `connexion refusee ou injoignable` | le réseau bloque le port 80 : montage B |
| « le PC ne la joint pas » | le PC n'est pas sur le même réseau : le brancher sur la même switch |
| « console COM3 : occupee » | fermer Docklight et relancer le script |
| « la carte ne repond pas » | débrancher et rebrancher l'USB, vérifier le cavalier JP2, relancer |
| `Talkback vide` en boucle, verdict qui vieillit | le script MATLAB ne répond pas : *Apps → MATLAB Analysis → Meteo IA Cloud → Run* juste avant de montrer |
| Rien ne marche côté réseau | montrer la partie locale (capteurs, Edge, arbre) et les captures de la veille |

Jamais le bouton RESET de la carte : en mode développement, il efface le
programme. Relancer le script à la place.
