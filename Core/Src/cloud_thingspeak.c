/**
  ******************************************************************************
  * @file    cloud_thingspeak.c
  * @brief   Client HTTP ThingSpeak (TP3 §2.c et §2.d).
  *
  *  Reutilise les sockets BSD de LWIP deja en place pour la partie 3. Aucune
  *  bibliotheque HTTP n'est ajoutee : une requete GET tient en quelques
  *  lignes et evite d'embarquer du code dont on n'utiliserait qu'un pour cent.
  *
  *  Les tampons sont statiques et non locaux : ce code s'execute dans une
  *  tache FreeRTOS a pile fixe, ou un tampon de 512 octets sur la pile est
  *  une cause classique de debordement. Les deux fonctions publiques sont
  *  appelees depuis la meme tache, il n'y a donc pas de reentrance.
  ******************************************************************************
  */
#include "main.h"
#include "format.h"
#include "cloud_thingspeak.h"
#include "config_cloud.h"

#include <string.h>
#include <stdio.h>

#include "lwip/sockets.h"
#include "lwip/netdb.h"

#define TAILLE_REQUETE   512
#define TAILLE_REPONSE   768

static char requete[TAILLE_REQUETE];
static char reponse[TAILLE_REPONSE];

/* ============================================================================
 *  Outils
 * ==========================================================================*/


const char *cloud_message_erreur(int code)
{
  switch (code)
  {
    case CLOUD_OK:                return "ok";
    case CLOUD_NON_CONFIGURE:     return "cle d'API absente (config_cloud_local.h)";
    case CLOUD_ERREUR_DNS:        return "resolution DNS impossible";
    case CLOUD_ERREUR_SOCKET:     return "creation de socket impossible";
    case CLOUD_ERREUR_CONNEXION:  return "connexion refusee ou injoignable";
    case CLOUD_ERREUR_ENVOI:      return "echec de l'envoi";
    case CLOUD_ERREUR_REPONSE:    return "reponse HTTP invalide";
    default:                      return "erreur inconnue";
  }
}

int cloud_est_configure(void)
{
  return (THINGSPEAK_WRITE_API_KEY[0] != '\0') ? 1 : 0;
}

/* ============================================================================
 *  Echange HTTP : resolution, connexion, envoi, lecture de la reponse.
 *  Retourne CLOUD_OK et place le corps de la reponse dans "corps".
 * ==========================================================================*/
static int echange_http(const char *chemin, char **corps, uint32_t *latence_ms)
{
  struct hostent *hote;
  struct sockaddr_in destination;
  struct timeval delai;
  uint32_t debut;
  int douille;
  int longueur;
  int recus;
  int total = 0;
  char *separateur;
  int resultat = CLOUD_OK;

  debut = HAL_GetTick();

  hote = gethostbyname(THINGSPEAK_HOTE);
  if ((hote == NULL) || (hote->h_addr_list[0] == NULL))
  {
    return CLOUD_ERREUR_DNS;
  }

  douille = socket(AF_INET, SOCK_STREAM, 0);
  if (douille < 0)
  {
    return CLOUD_ERREUR_SOCKET;
  }

  /* Sans delai de garde, une absence de reponse bloquerait la tache */
  delai.tv_sec = THINGSPEAK_TIMEOUT_S;
  delai.tv_usec = 0;
  setsockopt(douille, SOL_SOCKET, SO_RCVTIMEO, &delai, sizeof(delai));
  setsockopt(douille, SOL_SOCKET, SO_SNDTIMEO, &delai, sizeof(delai));

  memset(&destination, 0, sizeof(destination));
  destination.sin_family = AF_INET;
  destination.sin_port = lwip_htons(THINGSPEAK_PORT);
  memcpy(&destination.sin_addr, hote->h_addr_list[0], sizeof(destination.sin_addr));

  if (connect(douille, (struct sockaddr *)&destination, sizeof(destination)) != 0)
  {
    lwip_close(douille);
    return CLOUD_ERREUR_CONNEXION;
  }

  longueur = snprintf(requete, sizeof(requete),
                      "GET %s HTTP/1.1\r\n"
                      "Host: " THINGSPEAK_HOTE "\r\n"
                      "User-Agent: STM32N6-ETRS923\r\n"
                      "Connection: close\r\n"
                      "\r\n",
                      chemin);

  if (send(douille, requete, longueur, 0) != longueur)
  {
    lwip_close(douille);
    return CLOUD_ERREUR_ENVOI;
  }

  /* "Connection: close" : on lit jusqu'a la fermeture par le serveur */
  while (total < (int)(sizeof(reponse) - 1))
  {
    recus = recv(douille, reponse + total, sizeof(reponse) - 1 - total, 0);
    if (recus <= 0)
    {
      break;
    }
    total += recus;
  }
  reponse[total] = '\0';
  lwip_close(douille);

  if (latence_ms != NULL)
  {
    *latence_ms = HAL_GetTick() - debut;
  }

  if (total == 0)
  {
    return CLOUD_ERREUR_REPONSE;
  }

  if (strncmp(reponse, "HTTP/1.1 200", 12) != 0 &&
      strncmp(reponse, "HTTP/1.0 200", 12) != 0)
  {
    resultat = CLOUD_ERREUR_REPONSE;
  }

  /* Le corps commence apres la ligne vide qui termine les en-tetes */
  separateur = strstr(reponse, "\r\n\r\n");
  *corps = (separateur != NULL) ? (separateur + 4) : (reponse + total);

  return resultat;
}

/* ============================================================================
 *  §2.c : envoi des mesures vers le canal
 * ==========================================================================*/
int cloud_envoyer_mesures(const mesures_t *m, int classe_edge,
                          uint32_t *latence_ms)
{
  char chemin[256];
  char t[16];
  char h[16];
  char p[16];
  char *corps;
  int code;

  if (!cloud_est_configure())
  {
    return CLOUD_NON_CONFIGURE;
  }

  format_float_brut(m->sht40_temp_c.valeur, t, sizeof(t));
  format_float_brut(m->sht40_humidite_rh.valeur, h, sizeof(h));
  format_float_brut(m->df_pression_hpa.valeur, p, sizeof(p));

  /* field1..field3 : les grandeurs physiques, que MATLAB relira cote Cloud.
     field4 : la classe predite en local, pour pouvoir comparer les deux
     verdicts directement dans le canal. */
  snprintf(chemin, sizeof(chemin),
           "/update?api_key=%s&field1=%s&field2=%s&field3=%s&field4=%d",
           THINGSPEAK_WRITE_API_KEY, t, h, p, classe_edge);

  code = echange_http(chemin, &corps, latence_ms);
  if (code != CLOUD_OK)
  {
    return code;
  }

  /* ThingSpeak repond l'identifiant de l'entree creee, ou "0" en cas de
     rejet (cadence trop rapide, cle invalide). */
  if ((corps == NULL) || (corps[0] == '0') || (corps[0] == '\0'))
  {
    return CLOUD_ERREUR_REPONSE;
  }

  return CLOUD_OK;
}

/* ============================================================================
 *  §2.d : recuperation du verdict du Cloud par l'API Talkback
 * ==========================================================================*/
int cloud_lire_talkback(char *commande, int taille, uint32_t *latence_ms)
{
  char chemin[192];
  char *corps;
  int code;
  int i;

  commande[0] = '\0';

  if ((THINGSPEAK_TALKBACK_ID[0] == '\0') || (THINGSPEAK_TALKBACK_KEY[0] == '\0'))
  {
    return CLOUD_NON_CONFIGURE;
  }

  /* "execute" retire la commande de la file et la renvoie */
  snprintf(chemin, sizeof(chemin),
           "/talkbacks/%s/commands/execute?api_key=%s",
           THINGSPEAK_TALKBACK_ID, THINGSPEAK_TALKBACK_KEY);

  code = echange_http(chemin, &corps, latence_ms);
  if (code != CLOUD_OK)
  {
    return code;
  }

  if (corps == NULL)
  {
    return CLOUD_OK;   /* file vide */
  }

  /* Recopie en s'arretant a la fin de ligne : la reponse est du texte brut */
  for (i = 0; (i < taille - 1) && (corps[i] != '\0') &&
              (corps[i] != '\r') && (corps[i] != '\n'); i++)
  {
    commande[i] = corps[i];
  }
  commande[i] = '\0';

  return CLOUD_OK;
}
