/**
  ******************************************************************************
  * @file    ethernetif.c
  * @brief   Pilote Ethernet entre lwIP et la HAL ETH du STM32N6.
  *
  *  - Reception "zero-copy" : le DMA ecrit directement dans des pbuf lwIP
  *    pris dans le pool RX_POOL (callbacks HAL_ETH_RxAllocateCallback /
  *    HAL_ETH_RxLinkCallback).
  *  - Une tache "EthIf" attend le semaphore donne par l'interruption de
  *    reception et passe les trames a lwIP.
  *  - Une tache "EthLink" surveille le PHY LAN8742 (cable branche ou non,
  *    vitesse 10/100, duplex) toutes les 500 ms.
  *
  *  Le D-cache n'est pas active (cf. main.c) : aucune operation de cache n'est
  *  necessaire sur les buffers DMA.
  *  Adaptation du modele ethernetif.c genere par CubeMX (familles H7/H5, meme
  *  API HAL ETH que le N6).
  ******************************************************************************
  */
#include "main.h"
#include "lwip/opt.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/pbuf.h"
#include "lwip/timeouts.h"
#include "lwip/tcpip.h"
#include "netif/ethernet.h"
#include "netif/etharp.h"
#include "ethernetif.h"
#include "lan8742.h"
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* ---- Parametres ------------------------------------------------------------*/
#define IFNAME0                     's'
#define IFNAME1                     't'
#define ETH_RX_BUFFER_SIZE          1536U    /* = heth1.Init.RxBuffLen      */
#define ETH_RX_BUFFER_CNT           32U      /* buffers de reception. 16 se sont
                                               reveles trop peu sur un reseau
                                               /16 charge en diffusions.    */
#define ETH_TX_TIMEOUT_MS           2000U
#define TACHE_ETHIF_PILE_MOTS       1024U    /* cf. sujet : 1024 a 2048     */
#define TACHE_ETHIF_PRIORITE        (configMAX_PRIORITIES - 1)

/* ---- Pool de buffers de reception (pbuf "custom") --------------------------*/
typedef struct
{
  struct pbuf_custom pbuf_custom;
  uint8_t buff[(ETH_RX_BUFFER_SIZE + 31U) & ~31U] __attribute__((aligned(32)));
} RxBuff_t;

LWIP_MEMPOOL_DECLARE(RX_POOL, ETH_RX_BUFFER_CNT, sizeof(RxBuff_t), "RX pool zero-copy");

#define RX_ALLOC_OK      0
#define RX_ALLOC_ERREUR  1
static volatile uint8_t etat_allocation_rx = RX_ALLOC_OK;

/* Diagnostic : buffers du pool actuellement detenus par lwIP */
static volatile unsigned rx_alloues = 0;

/* ---- Descripteurs DMA (2 canaux en reception et en emission sur le N6) ----*/
ETH_DMADescTypeDef DMARxDscrTab[ETH_DMA_RX_CH_CNT][ETH_RX_DESC_CNT] __attribute__((aligned(32)));
ETH_DMADescTypeDef DMATxDscrTab[ETH_DMA_TX_CH_CNT][ETH_TX_DESC_CNT] __attribute__((aligned(32)));

/* ---- Variables -------------------------------------------------------------*/
ETH_HandleTypeDef heth1;
static ETH_TxPacketConfigTypeDef TxConfig;
static lan8742_Object_t LAN8742;
static SemaphoreHandle_t semaphore_rx = NULL;
static SemaphoreHandle_t semaphore_tx = NULL;
static uint8_t adresse_mac[6];
static int ethernet_demarre = 0;

static void pbuf_free_custom(struct pbuf *p);
static void ethernetif_input(void *argument);

/* ============================================================================
 *  Fonctions d'acces au PHY (registres MDIO) pour le driver LAN8742
 * ==========================================================================*/
static int32_t ETH_PHY_IO_Init(void)
{
  HAL_ETH_SetMDIOClockRange(&heth1);
  return 0;
}

static int32_t ETH_PHY_IO_DeInit(void)
{
  return 0;
}

static int32_t ETH_PHY_IO_ReadReg(uint32_t adresse_phy, uint32_t registre, uint32_t *valeur)
{
  if (HAL_ETH_ReadPHYRegister(&heth1, adresse_phy, registre, valeur) != HAL_OK)
  {
    return -1;
  }
  return 0;
}

static int32_t ETH_PHY_IO_WriteReg(uint32_t adresse_phy, uint32_t registre, uint32_t valeur)
{
  if (HAL_ETH_WritePHYRegister(&heth1, adresse_phy, registre, valeur) != HAL_OK)
  {
    return -1;
  }
  return 0;
}

static int32_t ETH_PHY_IO_GetTick(void)
{
  return (int32_t)HAL_GetTick();
}

static lan8742_IOCtx_t LAN8742_IOCtx = {
  ETH_PHY_IO_Init,
  ETH_PHY_IO_DeInit,
  ETH_PHY_IO_WriteReg,
  ETH_PHY_IO_ReadReg,
  ETH_PHY_IO_GetTick
};

/* ============================================================================
 *  Initialisation materielle (appelee par ethernetif_init)
 * ==========================================================================*/
static void low_level_init(struct netif *netif)
{
  uint32_t uid;
  int ch;

  /* Adresse MAC : prefixe ST 00:80:E1 + 3 octets tires de l'identifiant unique
     du microcontroleur -> deux cartes de la salle de TP n'ont jamais la meme
     adresse MAC (sinon conflits DHCP / ARP). */
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  uid = HAL_GetUIDw0() ^ HAL_GetUIDw1() ^ HAL_GetUIDw2();
  adresse_mac[0] = 0x00;
  adresse_mac[1] = 0x80;
  adresse_mac[2] = 0xE1;
  adresse_mac[3] = (uint8_t)(uid >> 16);
  adresse_mac[4] = (uint8_t)(uid >> 8);
  adresse_mac[5] = (uint8_t)(uid);

  /* Peripherique ETH1 en mode RMII */
  heth1.Instance = ETH1;
  heth1.Init.MACAddr = adresse_mac;
  heth1.Init.MediaInterface = HAL_ETH_RMII_MODE;
  for (ch = 0; ch < (int)ETH_DMA_CH_CNT; ch++)
  {
    heth1.Init.TxDesc[ch] = DMATxDscrTab[ch];
    heth1.Init.RxDesc[ch] = DMARxDscrTab[ch];
  }
  heth1.Init.RxBuffLen = ETH_RX_BUFFER_SIZE;
  if (HAL_ETH_Init(&heth1) != HAL_OK)
  {
    printf("[ETH] HAL_ETH_Init en echec\r\n");
    Error_Handler();
  }

  /* Configuration commune a toutes les trames emises :
     le MAC ajoute le bourrage et la CRC Ethernet (FCS) par materiel */
  memset(&TxConfig, 0, sizeof(TxConfig));
  TxConfig.Attributes = ETH_TX_PACKETS_FEATURES_CRCPAD;
  TxConfig.CRCPadCtrl = ETH_CRC_PAD_INSERT;
  TxConfig.ChecksumCtrl = ETH_CHECKSUM_DISABLE;

  LWIP_MEMPOOL_INIT(RX_POOL);

  /* Parametres de l'interface lwIP */
  netif->hwaddr_len = ETH_HWADDR_LEN;
  memcpy(netif->hwaddr, adresse_mac, ETH_HWADDR_LEN);
  netif->mtu = 1500;
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;

  /* Semaphores donnes par les interruptions de reception / d'emission */
  semaphore_rx = xSemaphoreCreateBinary();
  semaphore_tx = xSemaphoreCreateBinary();

  /* Tache de reception des trames */
  xTaskCreate(ethernetif_input, "EthIf", TACHE_ETHIF_PILE_MOTS, netif,
              TACHE_ETHIF_PRIORITE, NULL);

  /* PHY LAN8742 */
  LAN8742_RegisterBusIO(&LAN8742, &LAN8742_IOCtx);
  if (LAN8742_Init(&LAN8742) != LAN8742_STATUS_OK)
  {
    printf("[ETH] PHY LAN8742 non detecte sur le bus MDIO\r\n");
  }

  printf("[ETH] adresse MAC %02X:%02X:%02X:%02X:%02X:%02X\r\n",
         adresse_mac[0], adresse_mac[1], adresse_mac[2],
         adresse_mac[3], adresse_mac[4], adresse_mac[5]);
}

/* ============================================================================
 *  Emission d'une trame (appelee par lwIP)
 * ==========================================================================*/
static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
  ETH_BufferTypeDef buffers[ETH_TX_DESC_CNT];
  struct pbuf *q;
  uint32_t i = 0;
  err_t resultat = ERR_OK;

  (void)netif;

  /* Libere les trames deja emises par le DMA (appelle HAL_ETH_TxFreeCallback) */
  HAL_ETH_ReleaseTxPacket(&heth1);

  /* Une trame lwIP peut etre une chaine de pbuf : un buffer DMA par pbuf */
  memset(buffers, 0, sizeof(buffers));
  for (q = p; q != NULL; q = q->next)
  {
    if (i >= ETH_TX_DESC_CNT)
    {
      return ERR_IF;
    }
    buffers[i].buffer = q->payload;
    buffers[i].len = q->len;
    if (i > 0)
    {
      buffers[i - 1].next = &buffers[i];
    }
    buffers[i].next = NULL;
    i++;
  }

  TxConfig.Length = p->tot_len;
  TxConfig.TxBuffer = buffers;
  TxConfig.pData = p;

  /* lwIP liberera p au retour : on garde une reference jusqu'a la fin du DMA */
  pbuf_ref(p);

  do
  {
    if (HAL_ETH_Transmit_IT(&heth1, &TxConfig) == HAL_OK)
    {
      resultat = ERR_OK;
    }
    else if ((HAL_ETH_GetError(&heth1) & HAL_ETH_ERROR_BUSY) != 0)
    {
      /* Plus de descripteur libre : on attend la fin d'une emission */
      xSemaphoreTake(semaphore_tx, pdMS_TO_TICKS(ETH_TX_TIMEOUT_MS));
      HAL_ETH_ReleaseTxPacket(&heth1);
      resultat = ERR_BUF;
    }
    else
    {
      pbuf_free(p);
      resultat = ERR_IF;
    }
  } while (resultat == ERR_BUF);

  return resultat;
}

/* ============================================================================
 *  Reception
 * ==========================================================================*/
static struct pbuf *low_level_input(struct netif *netif)
{
  struct pbuf *p = NULL;

  (void)netif;
  if (etat_allocation_rx == RX_ALLOC_OK)
  {
    HAL_ETH_ReadData(&heth1, (void **)&p);
  }
  return p;
}

/* Tache "EthIf" : reveillee par l'interruption, transmet les trames a lwIP */
static void ethernetif_input(void *argument)
{
  struct netif *netif = (struct netif *)argument;
  struct pbuf *p;

  while (1)
  {
    if (xSemaphoreTake(semaphore_rx, portMAX_DELAY) == pdTRUE)
    {
      do
      {
        p = low_level_input(netif);
        if (p != NULL)
        {
          if (netif->input(p, netif) != ERR_OK)
          {
            pbuf_free(p);
          }
        }
      } while (p != NULL);
    }
  }
}

/* Liberation d'un buffer de reception (appelee par lwIP via pbuf_free) */
static void pbuf_free_custom(struct pbuf *p)
{
  struct pbuf_custom *custom_pbuf = (struct pbuf_custom *)p;

  LWIP_MEMPOOL_FREE(RX_POOL, custom_pbuf);

  if (rx_alloues > 0u)
  {
    rx_alloues--;
  }

  /* Si le DMA manquait de buffers, on relance la lecture */
  if (etat_allocation_rx == RX_ALLOC_ERREUR)
  {
    etat_allocation_rx = RX_ALLOC_OK;
    xSemaphoreGive(semaphore_rx);
  }
}

/* ============================================================================
 *  Diagnostic du pool de reception
 * ==========================================================================*/
unsigned ethernetif_buffers_rx_libres(void)
{
  return (unsigned)ETH_RX_BUFFER_CNT - rx_alloues;
}

unsigned ethernetif_penurie_rx(void)
{
  return (etat_allocation_rx == RX_ALLOC_ERREUR) ? 1u : 0u;
}

/* ============================================================================
 *  Callbacks de la HAL ETH
 * ==========================================================================*/

/* Le DMA demande un buffer de reception libre */
void HAL_ETH_RxAllocateCallback(uint8_t **buff)
{
  struct pbuf_custom *p = LWIP_MEMPOOL_ALLOC(RX_POOL);

  if (p != NULL)
  {
    rx_alloues++;
    *buff = (uint8_t *)p + offsetof(RxBuff_t, buff);
    p->custom_free_function = pbuf_free_custom;
    pbuf_alloced_custom(PBUF_RAW, 0, PBUF_REF, p, *buff, ETH_RX_BUFFER_SIZE);
  }
  else
  {
    etat_allocation_rx = RX_ALLOC_ERREUR;
    *buff = NULL;
  }
}

/* Une trame recue (eventuellement sur plusieurs buffers) : chainage des pbuf */
void HAL_ETH_RxLinkCallback(void **pStart, void **pEnd, uint8_t *buff, uint16_t Length)
{
  struct pbuf **ppStart = (struct pbuf **)pStart;
  struct pbuf **ppEnd = (struct pbuf **)pEnd;
  struct pbuf *p;

  p = (struct pbuf *)(buff - offsetof(RxBuff_t, buff));
  p->next = NULL;
  p->tot_len = 0;
  p->len = Length;

  if (*ppStart == NULL)
  {
    *ppStart = p;
  }
  else
  {
    (*ppEnd)->next = p;
  }
  *ppEnd = p;

  for (p = *ppStart; p != NULL; p = p->next)
  {
    p->tot_len = (u16_t)(p->tot_len + Length);
  }
}

/* Le DMA a fini d'emettre une trame : on libere la pbuf correspondante */
void HAL_ETH_TxFreeCallback(uint32_t *buff)
{
  pbuf_free((struct pbuf *)buff);
}

void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *heth)
{
  BaseType_t reveil = pdFALSE;

  (void)heth;
  xSemaphoreGiveFromISR(semaphore_rx, &reveil);
  portYIELD_FROM_ISR(reveil);
}

void HAL_ETH_TxCpltCallback(ETH_HandleTypeDef *heth)
{
  BaseType_t reveil = pdFALSE;

  (void)heth;
  xSemaphoreGiveFromISR(semaphore_tx, &reveil);
  portYIELD_FROM_ISR(reveil);
}

void HAL_ETH_ErrorCallback(ETH_HandleTypeDef *heth)
{
  BaseType_t reveil = pdFALSE;

  /* Plus de buffer de reception disponible : relancer la lecture */
  if ((HAL_ETH_GetDMAError(heth) & ETH_DMACxSR_RBU) == ETH_DMACxSR_RBU)
  {
    xSemaphoreGiveFromISR(semaphore_rx, &reveil);
    portYIELD_FROM_ISR(reveil);
  }
}

/* ============================================================================
 *  Initialisation de l'interface (passee a netif_add)
 * ==========================================================================*/
err_t ethernetif_init(struct netif *netif)
{
  netif->hostname = "nucleo-n657";
  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;
  netif->output = etharp_output;
  netif->linkoutput = low_level_output;

  low_level_init(netif);
  return ERR_OK;
}

/* ============================================================================
 *  Surveillance du lien Ethernet (tache "EthLink")
 * ==========================================================================*/
static void ethernet_verifier_lien(struct netif *netif)
{
  ETH_MACConfigTypeDef config_mac;
  int32_t etat_phy;
  uint32_t vitesse = 0;
  uint32_t duplex = 0;
  int lien_ok = 0;

  etat_phy = LAN8742_GetLinkState(&LAN8742);

  if (etat_phy == LAN8742_STATUS_100MBITS_FULLDUPLEX)
  {
    vitesse = ETH_SPEED_100M;
    duplex = ETH_FULLDUPLEX_MODE;
    lien_ok = 1;
  }
  else if (etat_phy == LAN8742_STATUS_100MBITS_HALFDUPLEX)
  {
    vitesse = ETH_SPEED_100M;
    duplex = ETH_HALFDUPLEX_MODE;
    lien_ok = 1;
  }
  else if (etat_phy == LAN8742_STATUS_10MBITS_FULLDUPLEX)
  {
    vitesse = ETH_SPEED_10M;
    duplex = ETH_FULLDUPLEX_MODE;
    lien_ok = 1;
  }
  else if (etat_phy == LAN8742_STATUS_10MBITS_HALFDUPLEX)
  {
    vitesse = ETH_SPEED_10M;
    duplex = ETH_HALFDUPLEX_MODE;
    lien_ok = 1;
  }

  if ((lien_ok == 0) && netif_is_link_up(netif))
  {
    /* Cable debranche */
    LOCK_TCPIP_CORE();
    netif_set_link_down(netif);
    UNLOCK_TCPIP_CORE();
    printf("[ETH] lien Ethernet perdu\r\n");
  }
  else if ((lien_ok == 1) && !netif_is_link_up(netif))
  {
    /* Cable branche : regler le MAC sur la vitesse negociee par le PHY */
    if (ethernet_demarre == 0)
    {
      HAL_ETH_GetMACConfig(&heth1, &config_mac);
      config_mac.DuplexMode = duplex;
      config_mac.Speed = vitesse;
      HAL_ETH_SetMACConfig(&heth1, &config_mac);
      HAL_ETH_Start_IT(&heth1);
      ethernet_demarre = 1;
    }
    else
    {
      /* DMA deja demarre : on change seulement vitesse/duplex dans MACCR
         (un Stop/Start reallouerait tous les buffers de reception) */
      MODIFY_REG(heth1.Instance->MACCR, ETH_MACCR_FES | ETH_MACCR_DM, vitesse | duplex);
    }

    LOCK_TCPIP_CORE();
    netif_set_link_up(netif);
    UNLOCK_TCPIP_CORE();

    if (vitesse == ETH_SPEED_100M)
    {
      printf("[ETH] lien Ethernet etabli : 100 Mbit/s");
    }
    else
    {
      printf("[ETH] lien Ethernet etabli : 10 Mbit/s");
    }
    if (duplex == ETH_FULLDUPLEX_MODE)
    {
      printf(" full duplex\r\n");
    }
    else
    {
      printf(" half duplex\r\n");
    }
  }
}

void ethernet_link_thread(void *argument)
{
  struct netif *netif = (struct netif *)argument;

  while (1)
  {
    ethernet_verifier_lien(netif);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}
