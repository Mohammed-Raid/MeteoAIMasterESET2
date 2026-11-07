# TP2 ETRS923 — Interface capteurs & STM32 — NUCLEO-N657X0-Q

Projet unique couvrant les 3 parties du TP. La partie exécutée est choisie par `PARTIE_TP` dans `Core/Inc/main.h` (valeur par défaut : 3).

## Matériel et câblage

- NUCLEO-N657X0-Q + X-NUCLEO-IKS4A1 enfichée sur les connecteurs Arduino.
- Câble USB sur le connecteur ST-LINK : alimentation, débogage et console série.
- Partie 3 : câble RJ45 vers un réseau avec serveur DHCP et accès Internet (box, prise réseau de salle).

## Mapping utilisé (vérifié sur les sources ST de la carte)

- LEDs : bleue PG8, rouge PG10, verte PG0, actives à l'état bas.
- Console : USART1, PE5 (TX) / PE6 (RX), Virtual COM Port du ST-LINK, 115200 bauds 8N1.
- Capteurs : I2C1 sur le connecteur Arduino, PH9 = SCL (D15), PC1 = SDA (D14), AF4, 100 kHz.
- Ethernet : ETH1 en RMII, PHY LAN8742 (adresse 0), broches PF4/PF5/PF7/PF10–PF15/PG11.

Le sujet indique « PA12 -> I2C2_SCL, PA11 -> I2C2_SDA » : ce mapping ne correspond pas à cette carte.

## Compiler

### STM32CubeIDE

1. File > Import > General > Existing Projects into Workspace.
2. Sélectionner le dossier `TP2_NUCLEO_N657`. Ne pas cocher « Copy projects into workspace » si le dossier est déjà à son emplacement définitif.
3. Ouvrir `Core/Inc/main.h` et régler `#define PARTIE_TP` à 1, 2 ou 3.
4. Project > Build Project (marteau).

### Ligne de commande (arm-none-eabi-gcc)

```
make            (partie définie dans main.h)
make PARTIE=2   (force la partie 2)
make clean
```

Sorties : `build/TP2_NUCLEO_N657.elf`, `.bin`, `.hex`.

Des binaires déjà compilés sont fournis dans `binaires/` (`TP2_partieX.elf` et `TP2_partieX.bin`).

## Charger le programme sur la carte

Le STM32N657 n'a pas de Flash interne. Deux modes existent.

### Mode développement (chargement en RAM par le débogueur)

1. Carte hors tension : cavalier **JP2 (BOOT1) en position 2**, JP1 (BOOT0) laissé en position 1. JP2 n'a que deux positions ; celle du mode développement est celle qui n'est pas sérigraphiée.
2. Brancher l'USB ST-LINK.
3. Dans CubeIDE : Run > Debug As > STM32 C/C++ Application, valider la configuration par défaut (ST-LINK GDB server, SWD), puis Resume (F8).

Le programme est perdu à la coupure d'alimentation.

### Boot depuis la Flash externe (programme permanent)

1. Ajouter l'en-tête de boot au binaire avec l'outil fourni avec STM32CubeProgrammer :
   `STM32_SigningTool_CLI -bin TP2_NUCLEO_N657.bin -nk -of 0x80000000 -t fsbl -o TP2-trusted.bin -hv 2.3 -align`
   (option `-align` à partir de CubeProgrammer 2.21).
2. Carte en mode développement (JP2 en position 2). Dans STM32CubeProgrammer, sélectionner le loader externe de la NUCLEO-N6 (Flash MX25UM51245G), puis programmer `TP2-trusted.bin` à l'adresse 0x70000000.
3. Carte hors tension : JP1 et JP2 tous deux en position 1, puis remettre sous tension ou appuyer sur RESET.

## Console

Ouvrir le port COM « STMicroelectronics STLink Virtual COM Port » à 115200 bauds, 8N1 (Docklight, PuTTY, Tera Term, ou le terminal de CubeIDE).

## Partie 1 — LED blink et chenillard

- a. Les trois LEDs s'allument pendant 3 s.
- b. Chenillard rouge → vert → bleu, 3 s par LED.
- c. Logs : `<debut d'application>`, `<debut de chenillard>`, `<ROUGE>`, `<VERT>`, `<BLEU>`, puis à nouveau `<debut de chenillard>`.

Fichier : `Core/Src/partie1_chenillard.c`.

## Partie 2 — Capteurs I2C

### La carte capteurs n'est pas celle du sujet

Le sujet décrit la **X-NUCLEO-IKS01A3**. La carte réellement utilisée est une
**X-NUCLEO-IKS4A1**, qui ne porte pas les mêmes composants. Le diagnostic a été
établi en lisant les registres WHO_AM_I : chaque valeur « inattendue » était en
fait l'identifiant correct d'un capteur de l'IKS4A1.

| Adresse 7 bits | Attendu par le sujet | Réellement présent | WHO_AM_I lu |
| --- | --- | --- | --- |
| 0x6B | LSM6DSO (0x6C) | LSM6DSV16X | 0x70 |
| 0x6A | — | LSM6DSO16IS | 0x22 |
| 0x19 | LIS2DW12 (0x44) | LIS2DUXS12 | 0x47 |
| 0x5D | LPS22HH (0xB3) | LPS22DF | 0xB4 |
| 0x38 | — | STTS22H | 0xA0 |
| 0x44 | — | SHT40AD1B | (aucun) |
| 0x1E | LIS2MDL (0x40) | LIS2MDL | 0x40 |

Le LIS2MDL est le seul capteur commun aux deux cartes : c'était donc le seul à
fonctionner avec le code écrit pour l'IKS01A3.

### Fonctionnement

Au démarrage :
- scan du bus I2C (adresses 0x08 à 0x77) ;
- détection de chaque capteur par lecture de son registre d'identification (WHO_AM_I).

Toutes les 2 s, les 7 capteurs sont lus et affichés :
- LSM6DSV16X : accélération en mg et en m/s², vitesse angulaire en dps, température en °C ;
- LSM6DSO16IS : accélération en mg, vitesse angulaire en dps ;
- LIS2DUXS12 : accélération en mg, température en °C ;
- LIS2MDL : champ magnétique en µT ;
- LPS22DF : pression en hPa et température en °C ;
- STTS22H : température en °C ;
- SHT40AD1B : humidité en %RH et température en °C.

LEDs : rouge = lecture en cours, verte = disponible, en attente de la prochaine lecture.

Fichiers :
- `Core/Src/capteurs.c` : fonctions bas niveau `platform_read` / `platform_write`, détection, initialisation, lecture et conversions ;
- `Core/Inc/capteurs.h` : structure `mesures_t` et interface publique ;
- `Core/Src/partie2_capteurs.c` : boucle de la partie 2 ;
- `Drivers/STMems/` : paires `xxx_reg.c` / `xxx_reg.h` du dépôt STMems_Standard_C_drivers ;
- `Drivers/STMems_IKS01A3_inutilise/` : drivers de l'IKS01A3, conservés mais hors compilation.

### Sortie CSV

En plus de l'affichage lisible, chaque cycle émet une ligne préfixée `CSV;`
contenant un horodatage en millisecondes et 26 mesures, séparées par des
points-virgules. Pour obtenir un fichier exploitable, filtrer la console :

```
grep "^CSV;" console.txt | cut -c5- > mesures.csv
```

Puis, côté Python : `pandas.read_csv("mesures.csv", sep=";")`. Une mesure
invalide produit un champ vide, lu comme `NaN`.

### Choix documentés

- **Adresses I2C** : les datasheets donnent l'adresse sur 7 bits (LSM6DSV16X = 0x6B). La HAL attend l'adresse décalée d'un bit à gauche (0x6B << 1 = 0xD6) ; elle gère elle-même le bit lecture/écriture. Les macros des drivers ST (`LSM6DSV16X_I2C_ADD_H` = 0xD7) sont déjà sur 8 bits et sont passées directement à la HAL. Le programme teste les adresses possibles (broche SA0 à 0 ou à 1) et garde celle dont le WHO_AM_I correspond.
- **Identification par WHO_AM_I et non par adresse** : les deux centrales inertielles (LSM6DSV16X et LSM6DSO16IS) partagent les mêmes adresses possibles 0xD5 / 0xD7. Elles ne peuvent donc être distinguées que par leur identifiant.
- **SHT40AD1B** : ce capteur Sensirion n'a ni registres ni WHO_AM_I. Il se pilote par commandes brutes (`HAL_I2C_Master_Transmit` / `Receive`), avec les formules de conversion de la datasheet SHT4x. Sa présence se constate en tentant une mesure.
- **Séparation lecture / affichage** : `capteurs_lire()` remplit une structure `mesures_t`, `capteurs_afficher()` la restitue. Les mesures peuvent ainsi être envoyées sur le réseau ou données à un modèle d'IA sans toucher au code I2C.
- **Erreurs I2C** : tous les codes de retour des drivers sont testés. Une mesure qui échoue est signalée `ERREUR I2C` au lieu de réafficher silencieusement la valeur précédente.
- **Affichage des flottants** : pas d'option `_printf_float`. Les valeurs sont converties en entiers (partie entière + 2 décimales), ce qui évite les allocations mémoire de newlib, dangereuses sous RTOS.
- **Timing I2C1** : 0x40D2B6B9 pour 100 kHz avec une horloge I2C de 200 MHz (PCLK1). Valeur calculée avec l'algorithme du BSP ST. Pour 400 kHz : 0x60911523.

## Partie 3 — FreeRTOS + lwIP

### Contexte

Le package officiel STM32CubeN6 ne fournit ni FreeRTOS ni LwIP : ST livre ThreadX/NetXDuo pour le N6. Les deux piles sont donc intégrées à la main :
- FreeRTOS-Kernel V11.1.0, portage `GCC/ARM_CM55_NTZ` : Cortex-M55 sans TrustZone, programme exécuté en état sécurisé uniquement (`configRUN_FREERTOS_SECURE_ONLY = 1`) ;
- lwIP 2.2.1 ;
- couche OS `LWIP/Target/sys_arch.c` et pilote `LWIP/Target/ethernetif.c` écrits pour ce projet.

### Tâches

- **App** : cycle répété
  1. `<Lecture Interface Capteurs>`, LED rouge ;
  2. `<Communication Reseau>`, LED bleue, 3 pings vers google.com ;
  3. attente de 5 s, LED verte.
- **Echo** : serveur TCP echo en sockets BSD (socket, bind, listen, accept, recv, send) sur le port 7.
- **tcpip** : pile lwIP.
- **EthIf** : réception des trames Ethernet. Pile de 1024 mots, conformément à la recommandation du sujet (1024 à 2048).
- **EthLink** : surveillance du câble et du PHY toutes les 500 ms.

### Réseau

- **Adresse IP** : obtenue par DHCP. Sans réponse sous 20 s, l'adresse fixe 192.168.1.50/24 est appliquée (modifiable dans `main.h`).
- **DNS** : serveur fourni par le DHCP, avec 8.8.8.8 en secours.
- **Ping** : socket BSD brute (`SOCK_RAW`, protocole ICMP). Requête ICMP construite à la main, somme de contrôle par `inet_chksum`.

### Réponses aux points du sujet

- **API FreeRTOS** : API native plutôt que CMSIS-RTOS v1/v2. Le package N6 ne fournit pas de couche CMSIS-RTOS pour FreeRTOS, et l'API native évite le surcoût de pile signalé par le sujet pour CMSIS v2.
- **MEM_SIZE** : 10 ko au lieu des 5000 octets recommandés au départ, pour couvrir DNS + socket brute + socket TCP ouvertes en même temps (`LWIP/Target/lwipopts.h`).
- **Logs lwIP** : `#define LWIP_DEBUG` est placé dans la zone USER CODE de `lwipopts.h`. Les modules sont désactivés par défaut ; passer `DHCP_DEBUG`, `ICMP_DEBUG`, etc. à `LWIP_DBG_ON` pour les observer.
- **CRC** : la CRC Ethernet (FCS) est insérée par le MAC (`CRCPadCtrl = ETH_CRC_PAD_INSERT`). Les sommes de contrôle IP/ICMP/UDP/TCP sont calculées par logiciel.
- **Cache et DMA** : le D-cache est laissé désactivé (I-cache actif). Le DMA Ethernet travaille directement en RAM, aucune opération de nettoyage ou d'invalidation de cache n'est nécessaire.
- **Adresse MAC** : 00:80:E1 suivi de 3 octets dérivés de l'identifiant unique du microcontrôleur. Chaque carte de la salle a une adresse différente, ce qui évite les conflits DHCP/ARP.

### Tests

- **Ping vers la carte** : depuis un PC du même réseau, `ping <adresse_IP_affichée>`.
- **Echo TCP** : `nc <adresse_IP> 7` (ou PuTTY en mode Raw sur le port 7). Le texte tapé est renvoyé.

## Arborescence

- `Core/Inc`, `Core/Src` : application.
  - `main.c` : horloges, USART1, I2C1, RIF.
  - `leds.c`, `capteurs.c`.
  - `partie1_chenillard.c`, `partie2_capteurs.c`, `partie3_reseau.c`.
- `Core/Startup` : code de démarrage.
- `LWIP/Target` : configuration et portage lwIP (`lwipopts.h`, `sys_arch`, `ethernetif`).
- `Drivers` : HAL STM32N6, CMSIS, PHY LAN8742, drivers STMems.
- `Middlewares/Third_Party` : FreeRTOS-Kernel V11.1.0, lwIP 2.2.1.
- `STM32N657X0HXQ_AXISRAM2_fsbl.ld` : script de liens. Code à 0x34180400 (255 Ko), données à 0x341C0000 (256 Ko).

## Dépannage

- **LED rouge qui clignote vite** : `Error_Handler()` a été appelé (initialisation HAL en échec, faute processeur, débordement de pile FreeRTOS). Le message correspondant s'affiche sur la console quand c'est possible.
- **« AUCUN peripherique » au scan I2C** : vérifier que l'IKS4A1 est bien enfichée, ses cavaliers d'alimentation et les broches PH9/PC1.
- **Pas de lien Ethernet** : vérifier le câble ; les LEDs du connecteur RJ45 doivent s'allumer.
- **DNS en échec** : le réseau bloque peut-être les serveurs DNS publics. Tester un ping vers une IP locale (passerelle) en remplaçant `HOTE_A_PINGUER` dans `main.h`.
- **Rien sur la console** : vérifier le port COM et le débit de 115200 bauds. En mode développement, vérifier que JP2 (BOOT1) est bien en position 2.
- **Le programme ne démarre pas alors que le chargement a « réussi »** : charger en RAM **depuis STM32CubeIDE** (`Run > Debug As > STM32 C/C++ Application`, puis F8), et non en ligne de commande.

  Sur cette carte, `ST-LINK_gdbserver` échoue avec `Failed to halt target` / `Target not halted`, y compris avec l'option `-k`. La cause est lisible dans le registre DHCSR (0xE000EDF0) : le bit `C_DEBUGEN` reste à 0, donc aucun outil ne peut arrêter le cœur ni forcer le PC.

  Le piège est que `STM32_Programmer_CLI -d fichier.elf -g <adresse>` affiche malgré tout `Start operation achieved successfully` : CubeProgrammer écrit la RAM par le bus système (AHB-AP) **sans passer par le cœur**, donc le téléchargement réussit réellement et la relecture est conforme — mais le saut à l'adresse de démarrage n'a jamais lieu et le bootROM continue de tourner. Symptôme : console muette, et tous les registres des périphériques à leur valeur de reset.

- **Fichiers nouveaux ou supprimés ignorés par CubeIDE** : sélectionner le projet et appuyer sur **F5** (Refresh). Eclipse garde en cache l'arborescence des fichiers et tentera sinon de compiler des sources supprimées.
