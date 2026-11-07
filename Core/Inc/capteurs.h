/**
  ******************************************************************************
  * @file    capteurs.h
  * @brief   Acces aux capteurs MEMS de la X-NUCLEO-IKS4A1 (bus I2C1)
  *
  *  La lecture et l'affichage sont volontairement separes : capteurs_lire()
  *  remplit une structure mesures_t, que l'on peut ensuite afficher, envoyer
  *  sur le reseau ou donner a un modele d'IA sans retoucher a l'I2C.
  ******************************************************************************
  */
#ifndef CAPTEURS_H
#define CAPTEURS_H

#include <stdint.h>

/* Une grandeur scalaire : la valeur n'a de sens que si valide == 1 */
typedef struct
{
  float   valeur;
  uint8_t valide;
} mesure_t;

/* Une grandeur sur trois axes */
typedef struct
{
  float   x;
  float   y;
  float   z;
  uint8_t valide;
} mesure_xyz_t;

/* Jeu complet de mesures a un instant donne */
typedef struct
{
  uint32_t horodatage_ms;

  mesure_xyz_t dsv16x_accel_mg;    /* LSM6DSV16X  : accelerometre */
  mesure_xyz_t dsv16x_gyro_dps;    /* LSM6DSV16X  : gyroscope */
  mesure_t     dsv16x_temp_c;      /* LSM6DSV16X  : temperature interne */

  mesure_xyz_t dso16is_accel_mg;   /* LSM6DSO16IS : accelerometre */
  mesure_xyz_t dso16is_gyro_dps;   /* LSM6DSO16IS : gyroscope */

  mesure_xyz_t duxs12_accel_mg;    /* LIS2DUXS12  : accelerometre */
  mesure_t     duxs12_temp_c;      /* LIS2DUXS12  : temperature interne */

  mesure_xyz_t mdl_champ_ut;       /* LIS2MDL     : champ magnetique */

  mesure_t     df_pression_hpa;    /* LPS22DF     : pression */
  mesure_t     df_temp_c;          /* LPS22DF     : temperature */

  mesure_t     stts22h_temp_c;     /* STTS22H     : temperature */

  mesure_t     sht40_humidite_rh;  /* SHT40AD1B   : humidite relative */
  mesure_t     sht40_temp_c;       /* SHT40AD1B   : temperature */
} mesures_t;

void capteurs_scanner_bus_i2c(void);
void capteurs_init(void);
void capteurs_lire(mesures_t *m);
void capteurs_afficher(const mesures_t *m);
void capteurs_afficher_csv_entete(void);
void capteurs_afficher_csv(const mesures_t *m);

#endif /* CAPTEURS_H */
