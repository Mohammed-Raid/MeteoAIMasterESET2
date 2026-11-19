/**
  ******************************************************************************
  * @file    cloud_thingspeak.h
  * @brief   Client HTTP ThingSpeak (TP3 §2.c et §2.d).
  *
  *  §2.c : envoi des mesures reelles des capteurs vers un canal ThingSpeak.
  *  §2.d : recuperation du verdict du Cloud via l'API Talkback.
  *
  *  Chaque appel mesure sa propre latence aller-retour, qui est le chiffre a
  *  opposer aux 37 us de l'inference embarquee pour repondre au §3.c.
  ******************************************************************************
  */
#ifndef CLOUD_THINGSPEAK_H
#define CLOUD_THINGSPEAK_H

#include <stdint.h>

#include "capteurs.h"

/* Codes de retour communs aux deux fonctions */
#define CLOUD_OK                 0
#define CLOUD_NON_CONFIGURE     -1   /* cle d'API absente */
#define CLOUD_ERREUR_DNS        -2
#define CLOUD_ERREUR_SOCKET     -3
#define CLOUD_ERREUR_CONNEXION  -4
#define CLOUD_ERREUR_ENVOI      -5
#define CLOUD_ERREUR_REPONSE    -6   /* statut HTTP autre que 200 */
#define CLOUD_ECRITURE_REFUSEE  -7   /* statut 200, mais ecriture refusee */

/* Vrai si une cle d'ecriture a ete fournie dans config_cloud_local.h */
int cloud_est_configure(void);

/* Code de statut HTTP de la derniere reponse, 0 si aucune. Pour un envoi,
   400 signale une cle invalide ; pour le TalkBack, 401 un mauvais
   identifiant ou une mauvaise cle. */
int cloud_dernier_statut_http(void);

/* Destination de la derniere redirection (statut 3xx), chaine vide sinon */
const char *cloud_derniere_redirection(void);

/**
  * @brief  Envoie les mesures et le resultat Edge vers ThingSpeak.
  * @param  m              mesures issues de capteurs_lire()
  * @param  classe_edge    indice de la classe predite par le modele embarque
  * @param  confiance_pct  probabilite de cette classe, en %
  * @param  duree_edge_us  duree de l'inference embarquee, en microsecondes
  * @param  latence_ms     duree aller-retour en millisecondes (peut etre NULL)
  * @retval CLOUD_OK ou un code d'erreur
  */
int cloud_envoyer_mesures(const mesures_t *m, int classe_edge,
                          int confiance_pct, uint32_t duree_edge_us,
                          uint32_t *latence_ms);

/**
  * @brief  Lit la commande en tete de file Talkback, et la retire.
  *         C'est par ce canal que le Cloud renvoie sa classification (§2.d).
  * @param  commande    tampon de sortie, chaine terminee par zero
  * @param  taille      taille du tampon
  * @param  latence_ms  duree aller-retour en millisecondes (peut etre NULL)
  * @retval CLOUD_OK ou un code d'erreur. Une file vide renvoie CLOUD_OK avec
  *         une chaine vide.
  */
int cloud_lire_talkback(char *commande, int taille, uint32_t *latence_ms);

/* Libelle lisible d'un code de retour, pour les messages console */
const char *cloud_message_erreur(int code);

#endif /* CLOUD_THINGSPEAK_H */
