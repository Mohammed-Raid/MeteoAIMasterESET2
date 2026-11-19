# Fiche de démo — ETRS923, TP3

La carte branchée sur le PC, le PC qui partage son Wi-Fi : c'est le montage
testé le 19/11/2026, et le seul où la page de la carte et ThingSpeak
marchent en même temps, quel que soit le réseau de la salle.

## La veille au soir (10 min)

- [ ] Appliquer le réglage qui empêche Windows de couper le Wi-Fi pendant le
      partage (PowerShell **administrateur**, voir `TP3_IA/THINGSPEAK.md`) :
      `New-ItemProperty -Path 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\WcmSvc\GroupPolicy' -Name fMinimizeConnections -PropertyType DWord -Value 0 -Force`,
      puis redémarrer le PC.
- [ ] Captures de secours, prises pendant que tout marche : la console avec
      `envoi ThingSpeak : ok` et `[CLOUD] ...`, le tableau de bord, les
      canaux *Mesures STm32* et *Meteo IA Cloud*, le TalkBack.
- [ ] PC chargé, câble USB et câble RJ45 dans le sac.

## Installation en salle (5 min)

1. Carte : cavalier **JP2 (BOOT1) en position 2**, USB ST-LINK et câble RJ45
   **branchés sur le PC**, pas sur la prise murale.
2. PC connecté au Wi-Fi de l'école, en passant la page de connexion s'il y
   en a une.
3. `ncpa.cpl` → clic droit **Wi-Fi** → *Propriétés* → *Partage* : case
   cochée, connexion domestique **Ethernet**.
4. La carte perd son programme hors tension : le recharger. Dans
   `Core/Inc/main.h`, ligne 29, `#define PARTIE_TP 5`, puis dans CubeIDE
   *Build* (marteau) et *Run*. Les clés ThingSpeak sont lues dans
   `config_cloud_local.h`, déjà en place sur ce PC.
5. Docklight sur **COM3, 115200**. CubeIDE charge par le ST-LINK : Docklight
   peut rester ouvert.
6. Relever l'adresse sur la console, `adresse IP : 192.168.137.x` (elle
   change à chaque démarrage), et l'ouvrir dans le navigateur.

## Déroulé (ce qu'on montre, dans l'ordre)

| Quand | À l'écran | À dire |
| --- | --- | --- |
| Démarrage | `[TEST] 8/8 identiques` | la carte rejoue les cas de test du notebook, même résultat au millionième |
| 0:20 | tableau de bord | page servie par la carte elle-même, 7 capteurs MEMS |
| 0:50 | `Edge : Clair 105 us`, `Arbre : Clair` | réseau 4-32-16-4 sur le Cortex-M55, l'arbre fait presque aussi bien en 190 fois moins de cycles |
| 1:30 | `envoi ThingSpeak : ok`, `[CLOUD] Clair (49 %)` | même modèle dans MATLAB sur ThingSpeak, verdict renvoyé par TalkBack |
| 2:00 | ligne `Cloud : ... aller-retour HTTP ~450 ms` | **105 µs contre ~450 ms**, environ 4 000 fois plus lent, pour le même verdict |
| 2:20 | couper le **Wi-Fi du PC** | la colonne Cloud tombe, la page et l'inférence locale continuent ; rallumer |

Le geste de 2:20 n'a pas été répété : le tester une fois sur place avant de
passer. Après avoir rallumé le Wi-Fi, vérifier que la case du partage est
toujours cochée.

## Si ça coince

| Symptôme | Que faire |
| --- | --- |
| Pas d'`adresse IP` sur la console | partage pas actif : refaire l'étape 3, attendre une minute, sinon relancer *Run* (pas le bouton RESET : en mode développement, il efface le programme) |
| `statut HTTP 302`, `redirige vers` | le Wi-Fi du PC n'a pas passé la page de connexion : l'ouvrir dans le navigateur |
| `resolution DNS impossible` | le PC n'a pas Internet : vérifier son Wi-Fi |
| `Talkback vide` en boucle, verdict qui vieillit | le script MATLAB ne répond pas : *Apps → MATLAB Analysis → Meteo IA Cloud → Run* juste avant de montrer |
| Page « Carte injoignable » | mauvaise adresse : relire `adresse IP` sur la console |
| CubeIDE ne charge pas | débrancher et rebrancher l'USB, relancer *Run* |
| Rien ne marche côté réseau | montrer la partie locale (capteurs, Edge, arbre) et les captures de la veille |
