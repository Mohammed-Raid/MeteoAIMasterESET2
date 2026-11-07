/**
  ******************************************************************************
  * @file    leds.c
  * @brief   Commande des 3 LEDs utilisateur (actives a l'etat bas).
  ******************************************************************************
  */
#include "main.h"

void led_allumer(int couleur)
{
  if (couleur == COULEUR_ROUGE)
  {
    HAL_GPIO_WritePin(LED_ROUGE_GPIO_Port, LED_ROUGE_Pin, LED_ALLUMEE);
  }
  else if (couleur == COULEUR_VERTE)
  {
    HAL_GPIO_WritePin(LED_VERTE_GPIO_Port, LED_VERTE_Pin, LED_ALLUMEE);
  }
  else if (couleur == COULEUR_BLEUE)
  {
    HAL_GPIO_WritePin(LED_BLEUE_GPIO_Port, LED_BLEUE_Pin, LED_ALLUMEE);
  }
}

void led_eteindre(int couleur)
{
  if (couleur == COULEUR_ROUGE)
  {
    HAL_GPIO_WritePin(LED_ROUGE_GPIO_Port, LED_ROUGE_Pin, LED_ETEINTE);
  }
  else if (couleur == COULEUR_VERTE)
  {
    HAL_GPIO_WritePin(LED_VERTE_GPIO_Port, LED_VERTE_Pin, LED_ETEINTE);
  }
  else if (couleur == COULEUR_BLEUE)
  {
    HAL_GPIO_WritePin(LED_BLEUE_GPIO_Port, LED_BLEUE_Pin, LED_ETEINTE);
  }
}

void leds_eteindre_tout(void)
{
  led_eteindre(COULEUR_ROUGE);
  led_eteindre(COULEUR_VERTE);
  led_eteindre(COULEUR_BLEUE);
}
