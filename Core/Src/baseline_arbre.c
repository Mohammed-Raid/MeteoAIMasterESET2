/**
  ******************************************************************************
  * @file    baseline_arbre.c
  * @brief   Reference deterministe : arbre de decision de profondeur 5.
  *
  *  FICHIER GENERE par TP3_IA/baseline_arbre.py. Ne pas editer a la main.
  *  Aucune multiplication, aucune exponentielle : au plus 5 comparaisons.
  ******************************************************************************
  */
#include "baseline_arbre.h"

int arbre_predire(const float entrees[ARBRE_NB_ENTREES])
{
  if (entrees[1] <= 75.500000f)  /* rhum en %RH */
  {
    if (entrees[2] <= 1014.450012f)  /* pres en hPa (niveau mer) */
    {
      if (entrees[1] <= 49.500000f)  /* rhum en %RH */
      {
        if (entrees[1] <= 36.500000f)  /* rhum en %RH */
        {
          if (entrees[3] <= -0.850000f)  /* dp3h en hPa en 3 h */
          {
            return 0;  /* Clair */
          }
          else
          {
            return 0;  /* Clair */
          }
        }
        else
        {
          if (entrees[0] <= 21.450000f)  /* temp en degC */
          {
            return 1;  /* Nuageux */
          }
          else
          {
            return 0;  /* Clair */
          }
        }
      }
      else
      {
        if (entrees[0] <= 3.950000f)  /* temp en degC */
        {
          if (entrees[0] <= -1.550000f)  /* temp en degC */
          {
            return 0;  /* Clair */
          }
          else
          {
            return 1;  /* Nuageux */
          }
        }
        else
        {
          if (entrees[2] <= 1009.350006f)  /* pres en hPa (niveau mer) */
          {
            return 1;  /* Nuageux */
          }
          else
          {
            return 1;  /* Nuageux */
          }
        }
      }
    }
    else
    {
      if (entrees[1] <= 54.500000f)  /* rhum en %RH */
      {
        if (entrees[1] <= 38.500000f)  /* rhum en %RH */
        {
          if (entrees[2] <= 1021.149994f)  /* pres en hPa (niveau mer) */
          {
            return 0;  /* Clair */
          }
          else
          {
            return 0;  /* Clair */
          }
        }
        else
        {
          if (entrees[2] <= 1021.549988f)  /* pres en hPa (niveau mer) */
          {
            return 0;  /* Clair */
          }
          else
          {
            return 0;  /* Clair */
          }
        }
      }
      else
      {
        if (entrees[1] <= 64.500000f)  /* rhum en %RH */
        {
          if (entrees[0] <= 3.950000f)  /* temp en degC */
          {
            return 0;  /* Clair */
          }
          else
          {
            return 0;  /* Clair */
          }
        }
        else
        {
          if (entrees[0] <= -0.750000f)  /* temp en degC */
          {
            return 0;  /* Clair */
          }
          else
          {
            return 1;  /* Nuageux */
          }
        }
      }
    }
  }
  else
  {
    if (entrees[0] <= 1.750000f)  /* temp en degC */
    {
      if (entrees[2] <= 1023.750000f)  /* pres en hPa (niveau mer) */
      {
        if (entrees[1] <= 89.500000f)  /* rhum en %RH */
        {
          if (entrees[2] <= 1016.149994f)  /* pres en hPa (niveau mer) */
          {
            return 3;  /* Neige */
          }
          else
          {
            return 1;  /* Nuageux */
          }
        }
        else
        {
          if (entrees[0] <= 0.750000f)  /* temp en degC */
          {
            return 3;  /* Neige */
          }
          else
          {
            return 3;  /* Neige */
          }
        }
      }
      else
      {
        if (entrees[2] <= 1028.750000f)  /* pres en hPa (niveau mer) */
        {
          if (entrees[1] <= 93.500000f)  /* rhum en %RH */
          {
            return 1;  /* Nuageux */
          }
          else
          {
            return 3;  /* Neige */
          }
        }
        else
        {
          if (entrees[0] <= -0.650000f)  /* temp en degC */
          {
            return 0;  /* Clair */
          }
          else
          {
            return 1;  /* Nuageux */
          }
        }
      }
    }
    else
    {
      if (entrees[2] <= 1016.450012f)  /* pres en hPa (niveau mer) */
      {
        if (entrees[1] <= 87.500000f)  /* rhum en %RH */
        {
          if (entrees[2] <= 1010.549988f)  /* pres en hPa (niveau mer) */
          {
            return 2;  /* Pluie */
          }
          else
          {
            return 1;  /* Nuageux */
          }
        }
        else
        {
          if (entrees[2] <= 1005.649994f)  /* pres en hPa (niveau mer) */
          {
            return 2;  /* Pluie */
          }
          else
          {
            return 2;  /* Pluie */
          }
        }
      }
      else
      {
        if (entrees[1] <= 87.500000f)  /* rhum en %RH */
        {
          if (entrees[2] <= 1023.950012f)  /* pres en hPa (niveau mer) */
          {
            return 1;  /* Nuageux */
          }
          else
          {
            return 1;  /* Nuageux */
          }
        }
        else
        {
          if (entrees[2] <= 1021.149994f)  /* pres en hPa (niveau mer) */
          {
            return 2;  /* Pluie */
          }
          else
          {
            return 1;  /* Nuageux */
          }
        }
      }
    }
  }
}
