/**
  ******************************************************************************
  * @file    partie1_chenillard.c
  * @brief   Partie 1 : LED blink, chenillard 3 s et logs console.
  ******************************************************************************
  */
#include "main.h"

#define TEMPO_CHENILLARD_MS   3000

void partie1_lancer(void)
{
  printf("<debut d'application>\r\n");

  /* a. Allumer les trois LEDs */
  led_allumer(COULEUR_ROUGE);
  led_allumer(COULEUR_VERTE);
  led_allumer(COULEUR_BLEUE);
  printf("Les trois LEDs sont allumees\r\n");
  HAL_Delay(TEMPO_CHENILLARD_MS);

  /* b. + c. Chenillard avec temporisation de 3 s et logs */
  while (1)
  {
    printf("<debut de chenillard>\r\n");

    leds_eteindre_tout();
    led_allumer(COULEUR_ROUGE);
    printf("<ROUGE>\r\n");
    HAL_Delay(TEMPO_CHENILLARD_MS);

    leds_eteindre_tout();
    led_allumer(COULEUR_VERTE);
    printf("<VERT>\r\n");
    HAL_Delay(TEMPO_CHENILLARD_MS);

    leds_eteindre_tout();
    led_allumer(COULEUR_BLEUE);
    printf("<BLEU>\r\n");
    HAL_Delay(TEMPO_CHENILLARD_MS);
  }
}
