/**
  ******************************************************************************
  * @file    ethernetif.h
  * @brief   Pilote Ethernet lwIP <-> HAL ETH du STM32N6 (PHY LAN8742)
  ******************************************************************************
  */
#ifndef ETHERNETIF_H
#define ETHERNETIF_H

#include "lwip/err.h"
#include "lwip/netif.h"

err_t ethernetif_init(struct netif *netif);
void  ethernet_link_thread(void *argument);

/* Diagnostic : nombre de buffers du pool de reception zero-copie encore
   disponibles, et drapeau de penurie. Si le drapeau reste a 1 et que le
   compte tombe a 0, la reception est bloquee : le DMA n'a plus de buffer
   libre et aucun n'est rendu. */
unsigned ethernetif_buffers_rx_libres(void);
unsigned ethernetif_penurie_rx(void);

#endif /* ETHERNETIF_H */
