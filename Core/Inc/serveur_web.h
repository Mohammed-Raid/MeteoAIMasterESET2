/**
  ******************************************************************************
  * @file    serveur_web.h
  * @brief   Tableau de bord servi par la carte elle-meme.
  *
  *  La NUCLEO-N657X0-Q devient son propre serveur web : on ouvre son adresse
  *  IP dans un navigateur et on voit, en direct, les capteurs, le verdict du
  *  modele embarque avec sa latence en microsecondes, celui de l'arbre
  *  deterministe, et celui du Cloud avec sa latence en millisecondes.
  *
  *  Tout le propos du TP3 tient alors dans un seul ecran, servi sans aucun
  *  service externe : debrancher le cable Internet laisse la page vivante et
  *  l'inference locale intacte, seul le Cloud tombe.
  *
  *  Deux taches se partagent l'etat : la tache applicative appelle
  *  serveur_web_publier(), la tache du serveur le lit. L'acces est protege
  *  par un mutex.
  ******************************************************************************
  */
#ifndef SERVEUR_WEB_H
#define SERVEUR_WEB_H

#include <stdint.h>

#include "capteurs.h"

#define SERVEUR_WEB_PORT           80
#define TAILLE_LIBELLE_CLOUD       48

/* Instantane publie a chaque cycle par la tache applicative */
typedef struct
{
  mesures_t mesures;

  const char *classe_edge;       /* libelle, ou NULL si pas encore calcule */
  uint32_t    cycles_edge;
  float       duree_edge_us;

  const char *classe_arbre;      /* reference deterministe */

  char        classe_cloud[TAILLE_LIBELLE_CLOUD];  /* vide si indisponible */
  uint32_t    latence_cloud_ms;
  uint8_t     cloud_configure;
  uint8_t     cloud_joignable;

  uint32_t    nb_cycles;         /* compteurs cumules depuis le demarrage */
  uint32_t    nb_accords_arbre;  /* cycles ou edge et arbre sont d'accord */
} etat_systeme_t;

/**
  * @brief  Cree la tache du serveur HTTP. A appeler une fois l'adresse IP
  *         obtenue.
  * @retval 0 si la tache a ete creee
  */
int serveur_web_demarrer(void);

/**
  * @brief  Publie l'etat courant. Copie la structure sous mutex, l'appelant
  *         peut donc reutiliser la sienne immediatement.
  */
void serveur_web_publier(const etat_systeme_t *etat);

#endif /* SERVEUR_WEB_H */
