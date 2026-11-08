/**
  ******************************************************************************
  * @file    format.h
  * @brief   Formatage de flottants sans l'option _printf_float.
  *
  *  Le projet n'active pas _printf_float : "%f" couterait plusieurs kilo-octets
  *  de code et declenche des allocations memoire dans newlib, ce qui est
  *  deconseille sous RTOS. Les valeurs sont donc converties a la main en
  *  partie entiere et deux decimales.
  ******************************************************************************
  */
#ifndef FORMAT_H
#define FORMAT_H

/**
  * @brief  Convertit un flottant en texte avec deux decimales, precede d'une
  *         espace quand il est positif. L'alignement des colonnes de la
  *         console en depend.
  */
void format_float(float valeur, char *texte, int taille);

/**
  * @brief  Idem, mais sans espace de tete : pour une URL, du JSON ou du CSV,
  *         ou une espace parasite serait au mieux inutile, au pire genante.
  */
void format_float_brut(float valeur, char *texte, int taille);

#endif /* FORMAT_H */
