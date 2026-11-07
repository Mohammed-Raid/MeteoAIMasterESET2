/**
  ******************************************************************************
  * @file    capteurs.c
  * @brief   Partie 2 du TP : interface entre les drivers ST "STMems_Standard_C"
  *          (fichiers xxx_reg.c / xxx_reg.h) et l'I2C HAL du STM32N6.
  *
  *  CARTE CAPTEURS REELLEMENT UTILISEE : X-NUCLEO-IKS4A1
  *  ----------------------------------------------------
  *  Le sujet du TP decrit la X-NUCLEO-IKS01A3 (LSM6DSO, LIS2DW12, HTS221,
  *  LPS22HH, STTS751, LIS2MDL). La carte dont nous disposons est une IKS4A1,
  *  qui ne porte pas les memes composants. Le diagnostic a ete etabli par
  *  lecture des registres WHO_AM_I : chaque valeur "inattendue" correspondait
  *  exactement a l'identifiant d'un capteur de l'IKS4A1.
  *
  *    adresse 7 bits | attendu par le sujet | reellement present | WHO_AM_I
  *    ---------------+----------------------+--------------------+---------
  *    0x6B           | LSM6DSO    (0x6C)    | LSM6DSV16X         | 0x70
  *    0x6A           | -                    | LSM6DSO16IS        | 0x22
  *    0x19           | LIS2DW12   (0x44)    | LIS2DUXS12         | 0x47
  *    0x5D           | LPS22HH    (0xB3)    | LPS22DF            | 0xB4
  *    0x38           | -                    | STTS22H            | 0xA0
  *    0x44           | -                    | SHT40AD1B          | (aucun)
  *    0x1E           | LIS2MDL    (0x40)    | LIS2MDL            | 0x40
  *
  *  Le LIS2MDL est le seul capteur commun aux deux cartes : c'est pour cela
  *  qu'il etait le seul a fonctionner avec le code d'origine.
  *
  *  ADRESSES I2C 7 BITS OU 8 BITS (choix documente, cf. sujet)
  *  ----------------------------------------------------------
  *  Les datasheets donnent l'adresse sur 7 bits (ex. LSM6DSV16X = 0x6B).
  *  La HAL attend cette adresse decalee d'un bit vers la gauche (format
  *  8 bits) : 0x6B << 1 = 0xD6, le bit 0 (lecture/ecriture) etant gere par
  *  la HAL elle-meme. Les macros des drivers ST (ex. LSM6DSV16X_I2C_ADD_H
  *  = 0xD7) sont deja au format 8 bits, bit 0 a 1 : on peut donc les passer
  *  directement a la HAL, qui ignore ce bit de poids faible.
  *  CHOIX RETENU : on manipule partout l'adresse 8 bits issue des macros ST.
  *
  *  Les deux centrales inertielles partagent les memes adresses possibles
  *  (0xD5 / 0xD7). On ne peut donc pas les distinguer par l'adresse : elles
  *  sont identifiees par leur WHO_AM_I.
  ******************************************************************************
  */
#include "main.h"
#include "capteurs.h"
#include <string.h>

#include "lsm6dsv16x_reg.h"
#include "lsm6dso16is_reg.h"
#include "lis2duxs12_reg.h"
#include "lis2mdl_reg.h"
#include "lps22df_reg.h"
#include "stts22h_reg.h"

#define I2C_TIMEOUT_MS      100

/* Le SHT40AD1B n'a pas de registres : il se pilote par commandes brutes.
   Adresse 7 bits 0x44, soit 0x88 au format 8 bits attendu par la HAL. */
#define SHT40_ADRESSE_8BITS   0x88
#define SHT40_CMD_MESURE      0xFD   /* mesure T + RH, haute precision */

/* Adresse I2C de chaque capteur, format 8 bits (0 = capteur absent) ---------*/
static uint16_t adr_dsv16x  = 0;
static uint16_t adr_dso16is = 0;
static uint16_t adr_duxs12  = 0;
static uint16_t adr_lis2mdl = 0;
static uint16_t adr_lps22df = 0;
static uint16_t adr_stts22h = 0;
static uint8_t  sht40_present = 0;

/* Contexte de chaque driver ST ----------------------------------------------*/
static stmdev_ctx_t ctx_dsv16x;
static stmdev_ctx_t ctx_dso16is;
static stmdev_ctx_t ctx_duxs12;
static stmdev_ctx_t ctx_lis2mdl;
static stmdev_ctx_t ctx_lps22df;
static stmdev_ctx_t ctx_stts22h;

/* Donnees privees exigees par certains drivers (gestion de l'auto-increment) */
static lis2duxs12_priv_t priv_duxs12;
static stts22h_priv_t    priv_stts22h;

/* Parametres de conversion du LIS2DUXS12 : lis2duxs12_xl_data_get() en a
   besoin pour appliquer la bonne pleine echelle */
static lis2duxs12_md_t md_duxs12;

/* ============================================================================
 *  1. FONCTIONS BAS NIVEAU (platform_read / platform_write)
 *     Les drivers ST appellent ces fonctions pour lire/ecrire un registre.
 *     handle = pointeur vers l'adresse I2C du capteur concerne.
 * ==========================================================================*/
static int32_t platform_write(void *handle, uint8_t reg, const uint8_t *buf, uint16_t len)
{
  uint16_t adresse = *(uint16_t *)handle;

  if (HAL_I2C_Mem_Write(&hi2c1, adresse, reg, I2C_MEMADD_SIZE_8BIT,
                        (uint8_t *)buf, len, I2C_TIMEOUT_MS) == HAL_OK)
  {
    return 0;
  }
  return -1;
}

static int32_t platform_read(void *handle, uint8_t reg, uint8_t *buf, uint16_t len)
{
  uint16_t adresse = *(uint16_t *)handle;

  if (HAL_I2C_Mem_Read(&hi2c1, adresse, reg, I2C_MEMADD_SIZE_8BIT,
                       buf, len, I2C_TIMEOUT_MS) == HAL_OK)
  {
    return 0;
  }
  return -1;
}

static void platform_delay(uint32_t ms)
{
  attendre_ms(ms);
}

/* ============================================================================
 *  2. SHT40AD1B : capteur Sensirion, pilote par commandes et non par registres
 * ==========================================================================*/

/* Envoie la commande de mesure, attend la conversion, lit les 6 octets.
   Format recu : T_MSB T_LSB T_CRC RH_MSB RH_LSB RH_CRC
   Retourne 0 si tout s'est bien passe. */
static int32_t sht40_mesurer(float *temperature_c, float *humidite_rh)
{
  uint8_t commande = SHT40_CMD_MESURE;
  uint8_t recu[6];
  uint16_t ticks_t;
  uint16_t ticks_rh;

  if (HAL_I2C_Master_Transmit(&hi2c1, SHT40_ADRESSE_8BITS, &commande, 1,
                              I2C_TIMEOUT_MS) != HAL_OK)
  {
    return -1;
  }

  /* La mesure haute precision demande environ 9 ms */
  attendre_ms(12);

  if (HAL_I2C_Master_Receive(&hi2c1, SHT40_ADRESSE_8BITS, recu, sizeof(recu),
                             I2C_TIMEOUT_MS) != HAL_OK)
  {
    return -1;
  }

  ticks_t  = (uint16_t)((recu[0] << 8) | recu[1]);
  ticks_rh = (uint16_t)((recu[3] << 8) | recu[4]);

  /* Formules de conversion de la datasheet SHT4x */
  *temperature_c = -45.0f + 175.0f * ((float)ticks_t / 65535.0f);
  *humidite_rh   = -6.0f + 125.0f * ((float)ticks_rh / 65535.0f);

  /* L'humidite relative physique est bornee a 0-100 % */
  if (*humidite_rh < 0.0f)
  {
    *humidite_rh = 0.0f;
  }
  if (*humidite_rh > 100.0f)
  {
    *humidite_rh = 100.0f;
  }

  return 0;
}

/* ============================================================================
 *  3. OUTILS D'AFFICHAGE
 * ==========================================================================*/

/* Convertit un float en texte avec 2 decimales, sans utiliser printf("%f")
   (evite l'option _printf_float et les allocations memoire de newlib) */
static void float_vers_texte(float valeur, char *texte, int taille)
{
  int32_t centiemes;
  char signe = ' ';

  if (valeur >= 0.0f)
  {
    centiemes = (int32_t)(valeur * 100.0f + 0.5f);
  }
  else
  {
    centiemes = (int32_t)(valeur * 100.0f - 0.5f);
  }

  if (centiemes < 0)
  {
    signe = '-';
    centiemes = -centiemes;
  }

  snprintf(texte, taille, "%c%ld.%02ld", signe,
           (long)(centiemes / 100), (long)(centiemes % 100));
}

static void afficher_xyz(const char *nom, const mesure_xyz_t *m, const char *unite)
{
  char tx[16];
  char ty[16];
  char tz[16];

  if (!m->valide)
  {
    printf("  %-28s ERREUR I2C\r\n", nom);
    return;
  }

  float_vers_texte(m->x, tx, sizeof(tx));
  float_vers_texte(m->y, ty, sizeof(ty));
  float_vers_texte(m->z, tz, sizeof(tz));
  printf("  %-28s X=%10s  Y=%10s  Z=%10s %s\r\n", nom, tx, ty, tz, unite);
}

static void afficher_valeur(const char *nom, const mesure_t *m, const char *unite)
{
  char t[16];

  if (!m->valide)
  {
    printf("  %-28s ERREUR I2C\r\n", nom);
    return;
  }

  float_vers_texte(m->valeur, t, sizeof(t));
  printf("  %-28s %10s %s\r\n", nom, t, unite);
}

/* ============================================================================
 *  4. DETECTION DES CAPTEURS
 * ==========================================================================*/

/* Lit un registre d'identification a une adresse donnee.
   Retourne 1 si la lecture a reussi. */
static int lire_id(uint16_t adresse_8bits, uint8_t registre, uint8_t *id)
{
  return (HAL_I2C_Mem_Read(&hi2c1, adresse_8bits, registre, I2C_MEMADD_SIZE_8BIT,
                           id, 1, I2C_TIMEOUT_MS) == HAL_OK) ? 1 : 0;
}

/* Cherche un capteur parmi plusieurs adresses possibles, en validant son
   WHO_AM_I. Retourne l'adresse 8 bits trouvee, ou 0 si absent. */
static uint16_t trouver_capteur(const uint16_t *adresses, int nombre,
                                uint8_t registre_id, uint8_t id_attendu)
{
  uint8_t id = 0;
  int i;

  for (i = 0; i < nombre; i++)
  {
    if (lire_id(adresses[i], registre_id, &id) && id == id_attendu)
    {
      return adresses[i];
    }
  }
  return 0;
}

void capteurs_scanner_bus_i2c(void)
{
  uint16_t adresse7;
  int nombre = 0;

  printf("Scan du bus I2C1 (PH9=SCL, PC1=SDA) :\r\n");
  for (adresse7 = 0x08; adresse7 < 0x78; adresse7++)
  {
    /* HAL : adresse 7 bits decalee d'un bit a gauche */
    if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(adresse7 << 1), 3, 10) == HAL_OK)
    {
      printf("  peripherique trouve : 0x%02X (7 bits) / 0x%02X (8 bits)\r\n",
             adresse7, adresse7 << 1);
      nombre++;
    }
  }
  if (nombre == 0)
  {
    printf("  AUCUN peripherique ! Verifier : carte IKS4A1 bien enfichee,\r\n");
    printf("  cavaliers d'alimentation, pull-up I2C, broches PH9/PC1.\r\n");
  }
  else
  {
    printf("  %d peripherique(s) detecte(s)\r\n", nombre);
  }
}

/* ============================================================================
 *  5. INITIALISATION DES CAPTEURS
 * ==========================================================================*/

/* Les deux IMU partagent les adresses 0xD5 et 0xD7 : on lit WHO_AM_I sur
   chacune et on attribue l'adresse selon l'identifiant trouve. */
static void init_imus(void)
{
  static const uint16_t adresses[2] = { 0xD7, 0xD5 };
  uint8_t id = 0;
  int i;

  for (i = 0; i < 2; i++)
  {
    if (!lire_id(adresses[i], LSM6DSV16X_WHO_AM_I, &id))
    {
      continue;
    }
    if (id == LSM6DSV16X_ID)
    {
      adr_dsv16x = adresses[i];
    }
    else if (id == LSM6DSO16IS_ID)
    {
      adr_dso16is = adresses[i];
    }
  }

  /* --- LSM6DSV16X : accelerometre + gyroscope + temperature --- */
  if (adr_dsv16x != 0)
  {
    ctx_dsv16x.write_reg = platform_write;
    ctx_dsv16x.read_reg  = platform_read;
    ctx_dsv16x.mdelay    = platform_delay;
    ctx_dsv16x.handle    = &adr_dsv16x;

    lsm6dsv16x_sw_por(&ctx_dsv16x);
    attendre_ms(20);
    lsm6dsv16x_block_data_update_set(&ctx_dsv16x, PROPERTY_ENABLE);
    lsm6dsv16x_xl_full_scale_set(&ctx_dsv16x, LSM6DSV16X_2g);
    lsm6dsv16x_gy_full_scale_set(&ctx_dsv16x, LSM6DSV16X_2000dps);
    lsm6dsv16x_xl_data_rate_set(&ctx_dsv16x, LSM6DSV16X_ODR_AT_15Hz);
    lsm6dsv16x_gy_data_rate_set(&ctx_dsv16x, LSM6DSV16X_ODR_AT_15Hz);

    printf("  LSM6DSV16X  : OK, adresse 0x%02X (7 bits 0x%02X), WHO_AM_I = 0x%02X\r\n",
           adr_dsv16x & 0xFE, adr_dsv16x >> 1, LSM6DSV16X_ID);
  }
  else
  {
    printf("  LSM6DSV16X  : absent\r\n");
  }

  /* --- LSM6DSO16IS : seconde centrale inertielle --- */
  if (adr_dso16is != 0)
  {
    ctx_dso16is.write_reg = platform_write;
    ctx_dso16is.read_reg  = platform_read;
    ctx_dso16is.mdelay    = platform_delay;
    ctx_dso16is.handle    = &adr_dso16is;

    lsm6dso16is_software_reset(&ctx_dso16is);
    attendre_ms(20);
    lsm6dso16is_block_data_update_set(&ctx_dso16is, PROPERTY_ENABLE);
    lsm6dso16is_xl_full_scale_set(&ctx_dso16is, LSM6DSO16IS_2g);
    lsm6dso16is_gy_full_scale_set(&ctx_dso16is, LSM6DSO16IS_2000dps);
    lsm6dso16is_xl_data_rate_set(&ctx_dso16is, LSM6DSO16IS_XL_ODR_AT_26Hz_HP);
    lsm6dso16is_gy_data_rate_set(&ctx_dso16is, LSM6DSO16IS_GY_ODR_AT_26Hz_HP);

    printf("  LSM6DSO16IS : OK, adresse 0x%02X (7 bits 0x%02X), WHO_AM_I = 0x%02X\r\n",
           adr_dso16is & 0xFE, adr_dso16is >> 1, LSM6DSO16IS_ID);
  }
  else
  {
    printf("  LSM6DSO16IS : absent\r\n");
  }
}

static void init_duxs12(void)
{
  static const uint16_t adresses[2] = { LIS2DUXS12_I2C_ADD_H, LIS2DUXS12_I2C_ADD_L };

  adr_duxs12 = trouver_capteur(adresses, 2, LIS2DUXS12_WHO_AM_I, LIS2DUXS12_ID);
  if (adr_duxs12 == 0)
  {
    printf("  LIS2DUXS12  : absent\r\n");
    return;
  }

  ctx_duxs12.write_reg = platform_write;
  ctx_duxs12.read_reg  = platform_read;
  ctx_duxs12.mdelay    = platform_delay;
  ctx_duxs12.handle    = &adr_duxs12;
  ctx_duxs12.priv_data = &priv_duxs12;

  lis2duxs12_exit_deep_power_down(&ctx_duxs12);
  attendre_ms(25);
  lis2duxs12_sw_reset(&ctx_duxs12);
  attendre_ms(25);
  lis2duxs12_init_set(&ctx_duxs12);

  md_duxs12.fs  = LIS2DUXS12_4g;
  md_duxs12.bw  = LIS2DUXS12_ODR_div_4;
  md_duxs12.odr = LIS2DUXS12_25Hz_LP;
  lis2duxs12_mode_set(&ctx_duxs12, &md_duxs12);

  printf("  LIS2DUXS12  : OK, adresse 0x%02X (7 bits 0x%02X), WHO_AM_I = 0x%02X\r\n",
         adr_duxs12 & 0xFE, adr_duxs12 >> 1, LIS2DUXS12_ID);
}

static void init_lis2mdl(void)
{
  static const uint16_t adresses[1] = { LIS2MDL_I2C_ADD };

  adr_lis2mdl = trouver_capteur(adresses, 1, LIS2MDL_WHO_AM_I, LIS2MDL_ID);
  if (adr_lis2mdl == 0)
  {
    printf("  LIS2MDL     : absent\r\n");
    return;
  }

  ctx_lis2mdl.write_reg = platform_write;
  ctx_lis2mdl.read_reg  = platform_read;
  ctx_lis2mdl.mdelay    = platform_delay;
  ctx_lis2mdl.handle    = &adr_lis2mdl;

  /* Sequence deja validee sur la carte avec le code d'origine */
  lis2mdl_sw_reset(&ctx_lis2mdl);
  lis2mdl_block_data_update_set(&ctx_lis2mdl, PROPERTY_ENABLE);
  lis2mdl_data_rate_set(&ctx_lis2mdl, LIS2MDL_ODR_10Hz);
  lis2mdl_set_rst_mode_set(&ctx_lis2mdl, LIS2MDL_SENS_OFF_CANC_EVERY_ODR);
  lis2mdl_offset_temp_comp_set(&ctx_lis2mdl, PROPERTY_ENABLE);
  lis2mdl_operating_mode_set(&ctx_lis2mdl, LIS2MDL_CONTINUOUS_MODE);

  printf("  LIS2MDL     : OK, adresse 0x%02X (7 bits 0x%02X), WHO_AM_I = 0x%02X\r\n",
         adr_lis2mdl & 0xFE, adr_lis2mdl >> 1, LIS2MDL_ID);
}

static void init_lps22df(void)
{
  static const uint16_t adresses[2] = { LPS22DF_I2C_ADD_H, LPS22DF_I2C_ADD_L };
  lps22df_bus_mode_t bus_mode;
  lps22df_md_t md;

  adr_lps22df = trouver_capteur(adresses, 2, LPS22DF_WHO_AM_I, LPS22DF_ID);
  if (adr_lps22df == 0)
  {
    printf("  LPS22DF     : absent\r\n");
    return;
  }

  ctx_lps22df.write_reg = platform_write;
  ctx_lps22df.read_reg  = platform_read;
  ctx_lps22df.mdelay    = platform_delay;
  ctx_lps22df.handle    = &adr_lps22df;

  lps22df_init_set(&ctx_lps22df, LPS22DF_BOOT);
  attendre_ms(10);
  lps22df_init_set(&ctx_lps22df, LPS22DF_RESET);
  attendre_ms(10);
  lps22df_init_set(&ctx_lps22df, LPS22DF_DRV_RDY);

  bus_mode.filter    = LPS22DF_FILTER_AUTO;
  bus_mode.interface = LPS22DF_SEL_BY_HW;
  lps22df_bus_mode_set(&ctx_lps22df, &bus_mode);

  md.odr = LPS22DF_4Hz;
  md.avg = LPS22DF_16_AVG;
  md.lpf = LPS22DF_LPF_ODR_DIV_4;
  lps22df_mode_set(&ctx_lps22df, &md);

  printf("  LPS22DF     : OK, adresse 0x%02X (7 bits 0x%02X), WHO_AM_I = 0x%02X\r\n",
         adr_lps22df & 0xFE, adr_lps22df >> 1, LPS22DF_ID);
}

static void init_stts22h(void)
{
  static const uint16_t adresses[4] = { STTS22H_I2C_ADD_H, STTS22H_I2C_ADD_L,
                                        STTS22H_I2C_ADD_15K_OHM,
                                        STTS22H_I2C_ADD_56K_OHM };

  adr_stts22h = trouver_capteur(adresses, 4, STTS22H_WHOAMI, STTS22H_ID);
  if (adr_stts22h == 0)
  {
    printf("  STTS22H     : absent\r\n");
    return;
  }

  ctx_stts22h.write_reg = platform_write;
  ctx_stts22h.read_reg  = platform_read;
  ctx_stts22h.mdelay    = platform_delay;
  ctx_stts22h.handle    = &adr_stts22h;
  ctx_stts22h.priv_data = &priv_stts22h;

  /* Auto-increment : exige par le driver pour les lectures multi-octets */
  stts22h_auto_increment_set(&ctx_stts22h, 1);
  stts22h_temp_data_rate_set(&ctx_stts22h, STTS22H_25Hz);

  printf("  STTS22H     : OK, adresse 0x%02X (7 bits 0x%02X), WHO_AM_I = 0x%02X\r\n",
         adr_stts22h & 0xFE, adr_stts22h >> 1, STTS22H_ID);
}

static void init_sht40(void)
{
  float t;
  float rh;

  /* Pas de WHO_AM_I sur ce capteur : la presence se constate en mesurant */
  if (sht40_mesurer(&t, &rh) == 0)
  {
    sht40_present = 1;
    printf("  SHT40AD1B   : OK, adresse 0x%02X (7 bits 0x%02X), pas de WHO_AM_I\r\n",
           SHT40_ADRESSE_8BITS, SHT40_ADRESSE_8BITS >> 1);
  }
  else
  {
    sht40_present = 0;
    printf("  SHT40AD1B   : absent\r\n");
  }
}

void capteurs_init(void)
{
  capteurs_scanner_bus_i2c();

  printf("Initialisation des capteurs (X-NUCLEO-IKS4A1) :\r\n");
  init_imus();
  init_duxs12();
  init_lis2mdl();
  init_lps22df();
  init_stts22h();
  init_sht40();

  /* Laisse le temps aux capteurs les plus lents de produire une mesure */
  attendre_ms(500);
}

/* ============================================================================
 *  6. LECTURE
 * ==========================================================================*/
void capteurs_lire(mesures_t *m)
{
  int16_t brut3[3];
  int16_t brut;

  memset(m, 0, sizeof(*m));
  m->horodatage_ms = HAL_GetTick();

  /* --- LSM6DSV16X --- */
  if (adr_dsv16x != 0)
  {
    if (lsm6dsv16x_acceleration_raw_get(&ctx_dsv16x, brut3) == 0)
    {
      m->dsv16x_accel_mg.x = lsm6dsv16x_from_fs2_to_mg(brut3[0]);
      m->dsv16x_accel_mg.y = lsm6dsv16x_from_fs2_to_mg(brut3[1]);
      m->dsv16x_accel_mg.z = lsm6dsv16x_from_fs2_to_mg(brut3[2]);
      m->dsv16x_accel_mg.valide = 1;
    }

    if (lsm6dsv16x_angular_rate_raw_get(&ctx_dsv16x, brut3) == 0)
    {
      m->dsv16x_gyro_dps.x = lsm6dsv16x_from_fs2000_to_mdps(brut3[0]) / 1000.0f;
      m->dsv16x_gyro_dps.y = lsm6dsv16x_from_fs2000_to_mdps(brut3[1]) / 1000.0f;
      m->dsv16x_gyro_dps.z = lsm6dsv16x_from_fs2000_to_mdps(brut3[2]) / 1000.0f;
      m->dsv16x_gyro_dps.valide = 1;
    }

    if (lsm6dsv16x_temperature_raw_get(&ctx_dsv16x, &brut) == 0)
    {
      m->dsv16x_temp_c.valeur = lsm6dsv16x_from_lsb_to_celsius(brut);
      m->dsv16x_temp_c.valide = 1;
    }
  }

  /* --- LSM6DSO16IS --- */
  if (adr_dso16is != 0)
  {
    if (lsm6dso16is_acceleration_raw_get(&ctx_dso16is, brut3) == 0)
    {
      m->dso16is_accel_mg.x = lsm6dso16is_from_fs2g_to_mg(brut3[0]);
      m->dso16is_accel_mg.y = lsm6dso16is_from_fs2g_to_mg(brut3[1]);
      m->dso16is_accel_mg.z = lsm6dso16is_from_fs2g_to_mg(brut3[2]);
      m->dso16is_accel_mg.valide = 1;
    }

    if (lsm6dso16is_angular_rate_raw_get(&ctx_dso16is, brut3) == 0)
    {
      m->dso16is_gyro_dps.x = lsm6dso16is_from_fs2000dps_to_mdps(brut3[0]) / 1000.0f;
      m->dso16is_gyro_dps.y = lsm6dso16is_from_fs2000dps_to_mdps(brut3[1]) / 1000.0f;
      m->dso16is_gyro_dps.z = lsm6dso16is_from_fs2000dps_to_mdps(brut3[2]) / 1000.0f;
      m->dso16is_gyro_dps.valide = 1;
    }
  }

  /* --- LIS2DUXS12 --- */
  if (adr_duxs12 != 0)
  {
    lis2duxs12_xl_data_t data_xl;
    lis2duxs12_outt_data_t data_t;

    if (lis2duxs12_xl_data_get(&ctx_duxs12, &md_duxs12, &data_xl) == 0)
    {
      m->duxs12_accel_mg.x = data_xl.mg[0];
      m->duxs12_accel_mg.y = data_xl.mg[1];
      m->duxs12_accel_mg.z = data_xl.mg[2];
      m->duxs12_accel_mg.valide = 1;
    }

    if (lis2duxs12_outt_data_get(&ctx_duxs12, &data_t) == 0)
    {
      m->duxs12_temp_c.valeur = data_t.heat.deg_c;
      m->duxs12_temp_c.valide = 1;
    }
  }

  /* --- LIS2MDL --- */
  if (adr_lis2mdl != 0)
  {
    if (lis2mdl_magnetic_raw_get(&ctx_lis2mdl, brut3) == 0)
    {
      /* 1 milligauss = 0,1 microtesla */
      m->mdl_champ_ut.x = lis2mdl_from_lsb_to_mgauss(brut3[0]) / 10.0f;
      m->mdl_champ_ut.y = lis2mdl_from_lsb_to_mgauss(brut3[1]) / 10.0f;
      m->mdl_champ_ut.z = lis2mdl_from_lsb_to_mgauss(brut3[2]) / 10.0f;
      m->mdl_champ_ut.valide = 1;
    }
  }

  /* --- LPS22DF --- */
  if (adr_lps22df != 0)
  {
    lps22df_data_t data;

    if (lps22df_data_get(&ctx_lps22df, &data) == 0)
    {
      m->df_pression_hpa.valeur = data.pressure.hpa;
      m->df_pression_hpa.valide = 1;
      m->df_temp_c.valeur = data.heat.deg_c;
      m->df_temp_c.valide = 1;
    }
  }

  /* --- STTS22H --- */
  if (adr_stts22h != 0)
  {
    if (stts22h_temperature_raw_get(&ctx_stts22h, &brut) == 0)
    {
      m->stts22h_temp_c.valeur = stts22h_from_lsb_to_celsius(brut);
      m->stts22h_temp_c.valide = 1;
    }
  }

  /* --- SHT40AD1B --- */
  if (sht40_present)
  {
    float t;
    float rh;

    if (sht40_mesurer(&t, &rh) == 0)
    {
      m->sht40_temp_c.valeur = t;
      m->sht40_temp_c.valide = 1;
      m->sht40_humidite_rh.valeur = rh;
      m->sht40_humidite_rh.valide = 1;
    }
  }
}

/* ============================================================================
 *  7. AFFICHAGE LISIBLE
 * ==========================================================================*/
void capteurs_afficher(const mesures_t *m)
{
  mesure_xyz_t ms2;

  if (adr_dsv16x != 0)
  {
    afficher_xyz("LSM6DSV16X acceleration", &m->dsv16x_accel_mg, "mg");

    /* Meme mesure convertie en m/s2 : 1 mg = 9,80665e-3 m/s2 */
    ms2.x = m->dsv16x_accel_mg.x * 0.00980665f;
    ms2.y = m->dsv16x_accel_mg.y * 0.00980665f;
    ms2.z = m->dsv16x_accel_mg.z * 0.00980665f;
    ms2.valide = m->dsv16x_accel_mg.valide;
    afficher_xyz("LSM6DSV16X acceleration", &ms2, "m/s2");

    afficher_xyz("LSM6DSV16X vit. angulaire", &m->dsv16x_gyro_dps, "dps");
    afficher_valeur("LSM6DSV16X temperature", &m->dsv16x_temp_c, "degC");
  }

  if (adr_dso16is != 0)
  {
    afficher_xyz("LSM6DSO16IS acceleration", &m->dso16is_accel_mg, "mg");
    afficher_xyz("LSM6DSO16IS vit. angulaire", &m->dso16is_gyro_dps, "dps");
  }

  if (adr_duxs12 != 0)
  {
    afficher_xyz("LIS2DUXS12 acceleration", &m->duxs12_accel_mg, "mg");
    afficher_valeur("LIS2DUXS12 temperature", &m->duxs12_temp_c, "degC");
  }

  if (adr_lis2mdl != 0)
  {
    afficher_xyz("LIS2MDL champ magnetique", &m->mdl_champ_ut, "uT");
  }

  if (adr_lps22df != 0)
  {
    afficher_valeur("LPS22DF pression", &m->df_pression_hpa, "hPa");
    afficher_valeur("LPS22DF temperature", &m->df_temp_c, "degC");
  }

  if (adr_stts22h != 0)
  {
    afficher_valeur("STTS22H temperature", &m->stts22h_temp_c, "degC");
  }

  if (sht40_present)
  {
    afficher_valeur("SHT40AD1B humidite", &m->sht40_humidite_rh, "%RH");
    afficher_valeur("SHT40AD1B temperature", &m->sht40_temp_c, "degC");
  }
}

/* ============================================================================
 *  8. AFFICHAGE CSV
 *     Format directement exploitable par pandas.read_csv(sep=';').
 *     Une mesure invalide donne un champ vide, lu comme NaN par pandas.
 * ==========================================================================*/
void capteurs_afficher_csv_entete(void)
{
  printf("CSV;ms;"
         "dsv16x_ax_mg;dsv16x_ay_mg;dsv16x_az_mg;"
         "dsv16x_gx_dps;dsv16x_gy_dps;dsv16x_gz_dps;dsv16x_temp_c;"
         "dso16is_ax_mg;dso16is_ay_mg;dso16is_az_mg;"
         "dso16is_gx_dps;dso16is_gy_dps;dso16is_gz_dps;"
         "duxs12_ax_mg;duxs12_ay_mg;duxs12_az_mg;duxs12_temp_c;"
         "mdl_x_ut;mdl_y_ut;mdl_z_ut;"
         "df_pression_hpa;df_temp_c;stts22h_temp_c;"
         "sht40_humidite_rh;sht40_temp_c\r\n");
}

/* Ecrit le separateur PUIS la valeur (champ vide si la mesure est invalide).
   Le separateur est emis avant et non apres, pour que la ligne ne se termine
   pas par un point-virgule surnumeraire : sinon la ligne de donnees compte un
   champ de plus que l'en-tete, et pandas promeut silencieusement la premiere
   colonne en index, ce qui decale toutes les autres. */
static void csv_valeur(const mesure_t *m)
{
  char t[16];
  const char *p;

  if (!m->valide)
  {
    printf(";");
    return;
  }

  float_vers_texte(m->valeur, t, sizeof(t));

  /* float_vers_texte aligne les nombres positifs avec une espace de tete :
     utile a l'affichage console, parasite dans un CSV. */
  p = (t[0] == ' ') ? t + 1 : t;
  printf(";%s", p);
}

static void csv_xyz(const mesure_xyz_t *m)
{
  mesure_t v;

  v.valide = m->valide;
  v.valeur = m->x;
  csv_valeur(&v);
  v.valeur = m->y;
  csv_valeur(&v);
  v.valeur = m->z;
  csv_valeur(&v);
}

void capteurs_afficher_csv(const mesures_t *m)
{
  /* Pas de separateur final ici : csv_valeur() emet le sien en tete */
  printf("CSV;%lu", (unsigned long)m->horodatage_ms);

  csv_xyz(&m->dsv16x_accel_mg);
  csv_xyz(&m->dsv16x_gyro_dps);
  csv_valeur(&m->dsv16x_temp_c);

  csv_xyz(&m->dso16is_accel_mg);
  csv_xyz(&m->dso16is_gyro_dps);

  csv_xyz(&m->duxs12_accel_mg);
  csv_valeur(&m->duxs12_temp_c);

  csv_xyz(&m->mdl_champ_ut);

  csv_valeur(&m->df_pression_hpa);
  csv_valeur(&m->df_temp_c);
  csv_valeur(&m->stts22h_temp_c);

  csv_valeur(&m->sht40_humidite_rh);
  csv_valeur(&m->sht40_temp_c);

  printf("\r\n");
}
