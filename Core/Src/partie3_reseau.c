/**
  ******************************************************************************
  * @file    partie3_reseau.c
  * @brief   Partie 3 : FreeRTOS + lwIP (DHCP, ICMP, DNS, sockets BSD).
  *
  *  Taches FreeRTOS :
  *    "App"     : cycle Lecture capteurs -> Communication reseau -> attente
  *    "Echo"    : serveur TCP echo (socket BSD : socket/bind/listen/accept)
  *    "tcpip"   : pile lwIP                        (creee par tcpip_init)
  *    "EthIf"   : reception des trames Ethernet    (ethernetif.c)
  *    "EthLink" : surveillance du cable / PHY      (ethernetif.c)
  *
  *  LEDs : ROUGE = lecture capteurs, BLEUE = communication reseau,
  *         VERTE = disponible / en attente
  ******************************************************************************
  */
#include "main.h"
#include "capteurs.h"
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/opt.h"
#include "lwip/tcpip.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/dns.h"
#include "lwip/ip4_addr.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/icmp.h"
#include "lwip/inet_chksum.h"
#include "lwip/prot/ip4.h"
#include "ethernetif.h"

#define PERIODE_CYCLE_MS        5000
#define DELAI_DHCP_MAX_MS       20000
#define NOMBRE_PINGS            3
#define TAILLE_DONNEES_PING     32
#define IDENTIFIANT_PING        0xAFAF
#define TIMEOUT_PING_S          2

#define PILE_TACHE_APP          2048   /* en mots de 32 bits */
#define PILE_TACHE_ECHO         1024
#define PILE_TACHE_LIEN         512
#define PRIORITE_TACHE_APP      2
#define PRIORITE_TACHE_ECHO     2
#define PRIORITE_TACHE_LIEN     3

static struct netif interface_eth;
static uint16_t numero_sequence_ping = 0;

/* ============================================================================
 *  Initialisation de la pile reseau
 * ==========================================================================*/
static void reseau_init(void)
{
  ip4_addr_t ip;
  ip4_addr_t masque;
  ip4_addr_t passerelle;
  ip_addr_t dns_secours;

  /* Adresse 0.0.0.0 : elle sera fournie par le serveur DHCP */
  ip4_addr_set_zero(&ip);
  ip4_addr_set_zero(&masque);
  ip4_addr_set_zero(&passerelle);

  /* Cree la tache "tcpip" de lwIP */
  tcpip_init(NULL, NULL);

  LOCK_TCPIP_CORE();
  netif_add(&interface_eth, &ip, &masque, &passerelle, NULL, ethernetif_init, tcpip_input);
  netif_set_default(&interface_eth);
  netif_set_up(&interface_eth);

  /* Serveur DNS de secours si le DHCP n'en fournit qu'un (ou aucun) */
  IP_ADDR4(&dns_secours, 8, 8, 8, 8);
  dns_setserver(1, &dns_secours);

  /* Client DHCP : demarre des que le cable est branche */
  dhcp_start(&interface_eth);
  UNLOCK_TCPIP_CORE();

  /* Surveillance du lien Ethernet */
  xTaskCreate(ethernet_link_thread, "EthLink", PILE_TACHE_LIEN, &interface_eth,
              PRIORITE_TACHE_LIEN, NULL);
}

static void afficher_configuration_ip(void)
{
  char texte[16];
  const ip_addr_t *dns;

  LOCK_TCPIP_CORE();
  ip4addr_ntoa_r(netif_ip4_addr(&interface_eth), texte, sizeof(texte));
  printf("  adresse IP : %s\r\n", texte);
  ip4addr_ntoa_r(netif_ip4_netmask(&interface_eth), texte, sizeof(texte));
  printf("  masque     : %s\r\n", texte);
  ip4addr_ntoa_r(netif_ip4_gw(&interface_eth), texte, sizeof(texte));
  printf("  passerelle : %s\r\n", texte);
  dns = dns_getserver(0);
  ip4addr_ntoa_r(ip_2_ip4(dns), texte, sizeof(texte));
  printf("  DNS 1      : %s\r\n", texte);
  dns = dns_getserver(1);
  ip4addr_ntoa_r(ip_2_ip4(dns), texte, sizeof(texte));
  printf("  DNS 2      : %s\r\n", texte);
  UNLOCK_TCPIP_CORE();
}

/* Attend l'adresse DHCP (LED bleue clignotante). Sans reponse : IP fixe. */
static void reseau_attendre_adresse_ip(void)
{
  uint32_t temps = 0;
  int adresse_obtenue = 0;
  ip4_addr_t ip;
  ip4_addr_t masque;
  ip4_addr_t passerelle;

  printf("Attente d'une adresse IP (DHCP)...\r\n");
  while ((adresse_obtenue == 0) && (temps < DELAI_DHCP_MAX_MS))
  {
    HAL_GPIO_TogglePin(LED_BLEUE_GPIO_Port, LED_BLEUE_Pin);
    vTaskDelay(pdMS_TO_TICKS(250));
    temps = temps + 250;

    LOCK_TCPIP_CORE();
    if (dhcp_supplied_address(&interface_eth))
    {
      adresse_obtenue = 1;
    }
    UNLOCK_TCPIP_CORE();
  }
  led_eteindre(COULEUR_BLEUE);

  if (adresse_obtenue == 1)
  {
    printf("Adresse obtenue par DHCP :\r\n");
  }
  else
  {
    printf("Pas de reponse DHCP : adresse IP fixe de secours\r\n");
    IP4_ADDR(&ip, IP_SECOURS_0, IP_SECOURS_1, IP_SECOURS_2, IP_SECOURS_3);
    IP4_ADDR(&masque, MASQUE_0, MASQUE_1, MASQUE_2, MASQUE_3);
    IP4_ADDR(&passerelle, PASSERELLE_0, PASSERELLE_1, PASSERELLE_2, PASSERELLE_3);
    LOCK_TCPIP_CORE();
    dhcp_release_and_stop(&interface_eth);
    netif_set_addr(&interface_eth, &ip, &masque, &passerelle);
    UNLOCK_TCPIP_CORE();
  }
  afficher_configuration_ip();
}

/* ============================================================================
 *  PING avec une socket BSD "brute" (SOCK_RAW, protocole ICMP)
 * ==========================================================================*/
static void ping_hote(const char *nom_hote, int nombre)
{
  struct hostent *hote;
  struct sockaddr_in destination;
  struct sockaddr_in source;
  socklen_t taille_source;
  struct timeval timeout;
  uint8_t paquet[sizeof(struct icmp_echo_hdr) + TAILLE_DONNEES_PING];
  uint8_t reception[128];
  struct icmp_echo_hdr *echo;
  struct icmp_echo_hdr *reponse;
  struct ip_hdr *entete_ip;
  char texte_ip[16];
  int s;
  int i;
  int k;
  int n;
  int recu;
  int nb_recus = 0;
  uint32_t debut;

  /* 1. Resolution DNS du nom (ex. google.com) */
  hote = gethostbyname(nom_hote);
  if (hote == NULL)
  {
    printf("  DNS : impossible de resoudre %s\r\n", nom_hote);
    return;
  }
  memset(&destination, 0, sizeof(destination));
  destination.sin_family = AF_INET;
  memcpy(&destination.sin_addr, hote->h_addr_list[0], sizeof(destination.sin_addr));
  inet_ntoa_r(destination.sin_addr, texte_ip, sizeof(texte_ip));
  printf("  PING %s (%s) : %d octets de donnees\r\n", nom_hote, texte_ip, TAILLE_DONNEES_PING);

  /* 2. Socket brute ICMP */
  s = socket(AF_INET, SOCK_RAW, IP_PROTO_ICMP);
  if (s < 0)
  {
    printf("  socket() en echec\r\n");
    return;
  }
  timeout.tv_sec = TIMEOUT_PING_S;
  timeout.tv_usec = 0;
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

  for (i = 0; i < nombre; i++)
  {
    /* 3. Construction de la requete ICMP "echo request" */
    numero_sequence_ping++;
    echo = (struct icmp_echo_hdr *)paquet;
    ICMPH_TYPE_SET(echo, ICMP_ECHO);
    ICMPH_CODE_SET(echo, 0);
    echo->id = IDENTIFIANT_PING;
    echo->seqno = lwip_htons(numero_sequence_ping);
    for (k = 0; k < TAILLE_DONNEES_PING; k++)
    {
      paquet[sizeof(struct icmp_echo_hdr) + k] = (uint8_t)('a' + (k % 26));
    }
    echo->chksum = 0;
    echo->chksum = inet_chksum(paquet, sizeof(paquet));

    /* 4. Envoi */
    debut = HAL_GetTick();
    sendto(s, paquet, sizeof(paquet), 0, (struct sockaddr *)&destination, sizeof(destination));

    /* 5. Attente de la reponse "echo reply" correspondante */
    recu = 0;
    while (recu == 0)
    {
      taille_source = sizeof(source);
      n = recvfrom(s, reception, sizeof(reception), 0, (struct sockaddr *)&source, &taille_source);
      if (n <= 0)
      {
        break;   /* delai depasse */
      }
      /* La socket brute IPv4 recoit aussi l'en-tete IP */
      entete_ip = (struct ip_hdr *)reception;
      reponse = (struct icmp_echo_hdr *)(reception + IPH_HL_BYTES(entete_ip));
      if ((ICMPH_TYPE(reponse) == ICMP_ER) &&
          (reponse->id == IDENTIFIANT_PING) &&
          (reponse->seqno == lwip_htons(numero_sequence_ping)))
      {
        recu = 1;
      }
    }

    if (recu == 1)
    {
      nb_recus++;
      printf("  Reponse de %s : seq=%u temps=%lu ms TTL=%u\r\n", texte_ip,
             numero_sequence_ping, (unsigned long)(HAL_GetTick() - debut),
             IPH_TTL(entete_ip));
    }
    else
    {
      printf("  Delai d'attente depasse (seq=%u)\r\n", numero_sequence_ping);
    }

    if (i < nombre - 1)
    {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  closesocket(s);
  printf("  %d paquet(s) envoye(s), %d recu(s)\r\n", nombre, nb_recus);
}

/* ============================================================================
 *  Serveur TCP "echo" (port 7) : renvoie tout ce qu'il recoit
 *  Test depuis un PC : nc <ip_carte> 7   (ou telnet / Putty en mode raw)
 * ==========================================================================*/
static void tache_serveur_echo(void *argument)
{
  int serveur;
  int client;
  int n;
  struct sockaddr_in adresse;
  struct sockaddr_in adresse_client;
  socklen_t taille;
  char tampon[128];

  (void)argument;

  serveur = socket(AF_INET, SOCK_STREAM, 0);
  memset(&adresse, 0, sizeof(adresse));
  adresse.sin_family = AF_INET;
  adresse.sin_port = htons(PORT_SERVEUR_ECHO);
  adresse.sin_addr.s_addr = htonl(INADDR_ANY);

  if ((serveur < 0) ||
      (bind(serveur, (struct sockaddr *)&adresse, sizeof(adresse)) < 0) ||
      (listen(serveur, 2) < 0))
  {
    printf("[ECHO] impossible d'ouvrir le serveur\r\n");
    vTaskDelete(NULL);
  }
  printf("[ECHO] serveur TCP en ecoute sur le port %d\r\n", PORT_SERVEUR_ECHO);

  while (1)
  {
    taille = sizeof(adresse_client);
    client = accept(serveur, (struct sockaddr *)&adresse_client, &taille);
    if (client < 0)
    {
      continue;
    }
    printf("[ECHO] client connecte\r\n");

    n = recv(client, tampon, sizeof(tampon), 0);
    while (n > 0)
    {
      send(client, tampon, n, 0);
      n = recv(client, tampon, sizeof(tampon), 0);
    }

    closesocket(client);
    printf("[ECHO] client deconnecte\r\n");
  }
}

/* ============================================================================
 *  Tache principale de l'application
 * ==========================================================================*/
static void tache_application(void *argument)
{
  (void)argument;

  printf("<debut d'application>\r\n");

  /* Capteurs (LED rouge pendant l'initialisation) */
  leds_eteindre_tout();
  led_allumer(COULEUR_ROUGE);
  capteurs_init();
  led_eteindre(COULEUR_ROUGE);

  /* Reseau */
  reseau_init();
  reseau_attendre_adresse_ip();
  xTaskCreate(tache_serveur_echo, "Echo", PILE_TACHE_ECHO, NULL, PRIORITE_TACHE_ECHO, NULL);

  while (1)
  {
    /* Lecture des capteurs : LED ROUGE */
    leds_eteindre_tout();
    led_allumer(COULEUR_ROUGE);
    printf("<Lecture Interface Capteurs>\r\n");
    {
      /* static : mesures_t pese plusieurs centaines d'octets et on est ici
         dans une tache FreeRTOS a pile fixe. Un seul appelant, donc pas de
         probleme de reentrance. */
      static mesures_t mesures;

      capteurs_lire(&mesures);
      capteurs_afficher(&mesures);
    }

    /* Communication reseau : LED BLEUE */
    leds_eteindre_tout();
    led_allumer(COULEUR_BLEUE);
    printf("<Communication Reseau>\r\n");
    ping_hote(HOTE_A_PINGUER, NOMBRE_PINGS);

    /* Disponible, en attente du prochain cycle : LED VERTE */
    leds_eteindre_tout();
    led_allumer(COULEUR_VERTE);
    vTaskDelay(pdMS_TO_TICKS(PERIODE_CYCLE_MS));
  }
}

void partie3_lancer(void)
{
  if (xTaskCreate(tache_application, "App", PILE_TACHE_APP, NULL, PRIORITE_TACHE_APP, NULL) != pdPASS)
  {
    printf("Creation de la tache App impossible\r\n");
    Error_Handler();
  }

  /* Demarre l'ordonnanceur FreeRTOS : ne revient jamais */
  vTaskStartScheduler();

  Error_Handler();
}

/* ============================================================================
 *  Fonctions de diagnostic appelees par FreeRTOS
 * ==========================================================================*/
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
  (void)xTask;
  printf("\r\n!!! Debordement de pile dans la tache %s : augmenter sa pile\r\n", pcTaskName);
  Error_Handler();
}

void vApplicationMallocFailedHook(void)
{
  printf("\r\n!!! Tas FreeRTOS plein : augmenter configTOTAL_HEAP_SIZE\r\n");
  Error_Handler();
}

void vAssertCalled(const char *fichier, int ligne)
{
  printf("\r\n!!! configASSERT : %s ligne %d\r\n", fichier, ligne);
  Error_Handler();
}
