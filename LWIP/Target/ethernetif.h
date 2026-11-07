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

#endif /* ETHERNETIF_H */
