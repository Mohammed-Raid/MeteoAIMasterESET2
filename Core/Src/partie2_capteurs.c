/**
  ******************************************************************************
  * @file    partie2_capteurs.c
  * @brief   Partie 2 : lecture des capteurs de la X-NUCLEO-IKS4A1 par I2C.
  *          LED ROUGE = occupe (lecture capteurs)
  *          LED VERTE = disponible, en attente de la prochaine lecture
  *
  *  La lecture (capteurs_lire) est separee de la restitution
  *  (capteurs_afficher / capteurs_afficher_csv) : les mesures transitent par
  *  une structure mesures_t. On peut ainsi les envoyer ailleurs (reseau,
  *  modele d'IA) sans toucher au code I2C.
  *
  *  Deux sorties sont produites a chaque cycle :
  *    - un affichage lisible pour la console du TP ;
  *    - une ligne CSV prefixee "CSV;", a filtrer pour obtenir un fichier
  *      directement exploitable (pandas.read_csv(sep=';')).
  ******************************************************************************
  */
#include "main.h"
#include "capteurs.h"

#define PERIODE_LECTURE_MS   2000

void partie2_lancer(void)
{
  mesures_t mesures;

  printf("<debut d'application>\r\n");

  leds_eteindre_tout();
  led_allumer(COULEUR_ROUGE);

  capteurs_init();
  capteurs_afficher_csv_entete();

  while (1)
  {
    /* Occupe : lecture des capteurs */
    led_eteindre(COULEUR_VERTE);
    led_allumer(COULEUR_ROUGE);
    printf("<Lecture Interface Capteurs>\r\n");

    capteurs_lire(&mesures);
    capteurs_afficher(&mesures);
    capteurs_afficher_csv(&mesures);

    /* Disponible : attente de la prochaine lecture */
    led_eteindre(COULEUR_ROUGE);
    led_allumer(COULEUR_VERTE);
    attendre_ms(PERIODE_LECTURE_MS);
  }
}
