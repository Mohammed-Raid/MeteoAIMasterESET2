/**
  ******************************************************************************
  * @file    baseline_arbre.c
  * @brief   Reference deterministe : arbre de decision de profondeur 5.
  *
  *  FICHIER GENERE AUTOMATIQUEMENT par TP3_IA/6_baseline.py
  *  Ne pas editer a la main.
  *
  *  A comparer avec modele_ia.c : meme probleme, memes donnees, meme jeu de
  *  test, mais aucune multiplication ni exponentielle. L'arbre travaille
  *  directement sur les grandeurs physiques, sans normalisation.
  ******************************************************************************
  */
#include "baseline_arbre.h"

const char *const arbre_noms_classes[ARBRE_NB_CLASSES] = { "Clair", "Partiellement nuageux", "Couvert", "Precipitations" };

int arbre_predire(const float entrees[ARBRE_NB_ENTREES])
{
  if (entrees[1] <= 86.500000f)  /* rhum en %RH */
  {
    if (entrees[2] <= 1015.549988f)  /* pres en hPa */
    {
      if (entrees[1] <= 51.500000f)  /* rhum en %RH */
      {
        if (entrees[1] <= 32.500000f)  /* rhum en %RH */
        {
          if (entrees[0] <= 30.950000f)  /* temp en degC */
          {
            return 0;  /* Clair (339 releves) */
          }
          else
          {
            return 0;  /* Clair (374 releves) */
          }
        }
        else
        {
          if (entrees[0] <= 25.750000f)  /* temp en degC */
          {
            return 1;  /* Partiellement nuageux (2690 releves) */
          }
          else
          {
            return 0;  /* Clair (1427 releves) */
          }
        }
      }
      else
      {
        if (entrees[2] <= 1009.649994f)  /* pres en hPa */
        {
          if (entrees[1] <= 73.500000f)  /* rhum en %RH */
          {
            return 1;  /* Partiellement nuageux (4725 releves) */
          }
          else
          {
            return 3;  /* Precipitations (6824 releves) */
          }
        }
        else
        {
          if (entrees[1] <= 69.500000f)  /* rhum en %RH */
          {
            return 1;  /* Partiellement nuageux (5769 releves) */
          }
          else
          {
            return 1;  /* Partiellement nuageux (8479 releves) */
          }
        }
      }
    }
    else
    {
      if (entrees[1] <= 62.500000f)  /* rhum en %RH */
      {
        if (entrees[2] <= 1020.950012f)  /* pres en hPa */
        {
          if (entrees[1] <= 39.500000f)  /* rhum en %RH */
          {
            return 0;  /* Clair (1938 releves) */
          }
          else
          {
            return 0;  /* Clair (7899 releves) */
          }
        }
        else
        {
          if (entrees[1] <= 43.500000f)  /* rhum en %RH */
          {
            return 0;  /* Clair (2261 releves) */
          }
          else
          {
            return 0;  /* Clair (5641 releves) */
          }
        }
      }
      else
      {
        if (entrees[2] <= 1020.049988f)  /* pres en hPa */
        {
          if (entrees[0] <= 12.950000f)  /* temp en degC */
          {
            return 1;  /* Partiellement nuageux (4375 releves) */
          }
          else
          {
            return 1;  /* Partiellement nuageux (6462 releves) */
          }
        }
        else
        {
          if (entrees[0] <= 11.850000f)  /* temp en degC */
          {
            return 0;  /* Clair (9779 releves) */
          }
          else
          {
            return 0;  /* Clair (6274 releves) */
          }
        }
      }
    }
  }
  else
  {
    if (entrees[1] <= 94.500000f)  /* rhum en %RH */
    {
      if (entrees[2] <= 1015.750000f)  /* pres en hPa */
      {
        if (entrees[2] <= 1009.649994f)  /* pres en hPa */
        {
          if (entrees[2] <= 996.950012f)  /* pres en hPa */
          {
            return 3;  /* Precipitations (1438 releves) */
          }
          else
          {
            return 3;  /* Precipitations (5983 releves) */
          }
        }
        else
        {
          if (entrees[0] <= 8.650000f)  /* temp en degC */
          {
            return 3;  /* Precipitations (1833 releves) */
          }
          else
          {
            return 3;  /* Precipitations (4379 releves) */
          }
        }
      }
      else
      {
        if (entrees[0] <= 2.850000f)  /* temp en degC */
        {
          if (entrees[1] <= 90.500000f)  /* rhum en %RH */
          {
            return 0;  /* Clair (933 releves) */
          }
          else
          {
            return 3;  /* Precipitations (1497 releves) */
          }
        }
        else
        {
          if (entrees[1] <= 91.500000f)  /* rhum en %RH */
          {
            return 1;  /* Partiellement nuageux (6544 releves) */
          }
          else
          {
            return 3;  /* Precipitations (4129 releves) */
          }
        }
      }
    }
    else
    {
      if (entrees[0] <= 6.250000f)  /* temp en degC */
      {
        if (entrees[1] <= 97.500000f)  /* rhum en %RH */
        {
          if (entrees[2] <= 1001.649994f)  /* pres en hPa */
          {
            return 3;  /* Precipitations (326 releves) */
          }
          else
          {
            return 3;  /* Precipitations (2422 releves) */
          }
        }
        else
        {
          if (entrees[1] <= 99.500000f)  /* rhum en %RH */
          {
            return 3;  /* Precipitations (1089 releves) */
          }
          else
          {
            return 3;  /* Precipitations (1023 releves) */
          }
        }
      }
      else
      {
        if (entrees[2] <= 1012.549988f)  /* pres en hPa */
        {
          if (entrees[2] <= 1006.350006f)  /* pres en hPa */
          {
            return 3;  /* Precipitations (1318 releves) */
          }
          else
          {
            return 3;  /* Precipitations (1475 releves) */
          }
        }
        else
        {
          if (entrees[1] <= 96.500000f)  /* rhum en %RH */
          {
            return 3;  /* Precipitations (1990 releves) */
          }
          else
          {
            return 3;  /* Precipitations (2061 releves) */
          }
        }
      }
    }
  }
}
