/**
  ******************************************************************************
  * @file    main.h
  * @brief   TP2 ETRS923 - NUCLEO-N657X0-Q + X-NUCLEO-IKS01A3
  *          Fichier d'en-tete commun a tout le projet.
  ******************************************************************************
  */
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32n6xx_hal.h"
#include <stdio.h>
#include <stdint.h>

/* ============================================================================
 *  CHOIX DE LA PARTIE DU TP A EXECUTER
 *    1 : LED blink + chenillard 3 s + logs console           (sans RTOS)
 *    2 : lecture des 7 capteurs IKS4A1 par I2C + LEDs etat   (sans RTOS)
 *    3 : FreeRTOS + lwIP (DHCP, ICMP, DNS, sockets BSD) + ping google.com
 *    4 : TP3 - inference du reseau de neurones meteo sur les capteurs reels
 *    5 : TP3 - comparaison Cloud (ThingSpeak) contre Edge, avec latences
 *  Changer la valeur puis recompiler (ou : make PARTIE=4 en ligne de commande)
 *  Programme temoin du TP3 §3.d : make PARTIE=4 TEMOIN=1
 * ==========================================================================*/
#ifndef PARTIE_TP
#define PARTIE_TP   4
#endif

/* ---------------------------------------------------------------------------
 *  LEDs utilisateur de la NUCLEO-N657X0-Q (source : BSP ST stm32n6xx_nucleo.h)
 *    LD1 bleue = PG8, LD2 rouge = PG10, LD3 verte = PG0
 *  ATTENTION : les LEDs sont actives a l'etat BAS (RESET = allumee)
 * -------------------------------------------------------------------------*/
#define LED_BLEUE_Pin        GPIO_PIN_8
#define LED_BLEUE_GPIO_Port  GPIOG
#define LED_ROUGE_Pin        GPIO_PIN_10
#define LED_ROUGE_GPIO_Port  GPIOG
#define LED_VERTE_Pin        GPIO_PIN_0
#define LED_VERTE_GPIO_Port  GPIOG

#define LED_ALLUMEE          GPIO_PIN_RESET
#define LED_ETEINTE          GPIO_PIN_SET

#define COULEUR_ROUGE        0
#define COULEUR_VERTE        1
#define COULEUR_BLEUE        2

/* ---------------------------------------------------------------------------
 *  Bus I2C des connecteurs Arduino (la carte X-NUCLEO-IKS01A3 s'y branche)
 *    D15 = PH9 = I2C1_SCL (AF4)
 *    D14 = PC1 = I2C1_SDA (AF4)
 *  Remarque : le sujet cite PA12/PA11 + I2C2, ce mapping ne correspond pas
 *  a la NUCLEO-N657X0-Q.
 * -------------------------------------------------------------------------*/
#define CAPTEURS_I2C_SCL_Pin       GPIO_PIN_9
#define CAPTEURS_I2C_SCL_GPIO_Port GPIOH
#define CAPTEURS_I2C_SDA_Pin       GPIO_PIN_1
#define CAPTEURS_I2C_SDA_GPIO_Port GPIOC

/* ---------------------------------------------------------------------------
 *  Reseau : adresse de secours si aucun serveur DHCP ne repond
 * -------------------------------------------------------------------------*/
#define IP_SECOURS_0   192
#define IP_SECOURS_1   168
#define IP_SECOURS_2   1
#define IP_SECOURS_3   50
#define MASQUE_0       255
#define MASQUE_1       255
#define MASQUE_2       255
#define MASQUE_3       0
#define PASSERELLE_0   192
#define PASSERELLE_1   168
#define PASSERELLE_2   1
#define PASSERELLE_3   1

#define HOTE_A_PINGUER       "google.com"
#define PORT_SERVEUR_ECHO    7

/* Handles globaux (definis dans main.c) */
extern UART_HandleTypeDef huart1;
extern I2C_HandleTypeDef  hi2c1;

/* Fonctions communes */
void Error_Handler(void);
void attendre_ms(uint32_t ms);

/* leds.c */
void led_allumer(int couleur);
void led_eteindre(int couleur);
void leds_eteindre_tout(void);

/* partie1_chenillard.c / partie2_capteurs.c / partie3_reseau.c / partie4_ia.c */
void partie1_lancer(void);
void partie2_lancer(void);
void partie3_lancer(void);
void partie4_lancer(void);   /* TP3 : IA embarquee */
void partie5_lancer(void);   /* TP3 : comparaison Cloud / Edge */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
