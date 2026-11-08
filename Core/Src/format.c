/**
  ******************************************************************************
  * @file    format.c
  * @brief   Formatage de flottants sans l'option _printf_float.
  ******************************************************************************
  */
#include "format.h"

#include <stdint.h>
#include <stdio.h>

/* Decompose la valeur en signe, partie entiere et centiemes, avec arrondi
   au plus proche dans les deux sens. */
static void decomposer(float valeur, char *signe, long *entier, long *centiemes)
{
  int32_t total;

  if (valeur >= 0.0f)
  {
    total = (int32_t)(valeur * 100.0f + 0.5f);
    *signe = '+';
  }
  else
  {
    total = (int32_t)(valeur * 100.0f - 0.5f);
    *signe = '-';
  }

  if (total < 0)
  {
    total = -total;
  }

  *entier = (long)(total / 100);
  *centiemes = (long)(total % 100);
}

void format_float(float valeur, char *texte, int taille)
{
  char signe;
  long entier;
  long centiemes;

  decomposer(valeur, &signe, &entier, &centiemes);

  /* Une espace a la place du "+" : les colonnes restent alignees quel que
     soit le signe. */
  snprintf(texte, taille, "%c%ld.%02ld",
           (signe == '-') ? '-' : ' ', entier, centiemes);
}

void format_float_brut(float valeur, char *texte, int taille)
{
  char signe;
  long entier;
  long centiemes;

  decomposer(valeur, &signe, &entier, &centiemes);

  if (signe == '-')
  {
    snprintf(texte, taille, "-%ld.%02ld", entier, centiemes);
  }
  else
  {
    snprintf(texte, taille, "%ld.%02ld", entier, centiemes);
  }
}
