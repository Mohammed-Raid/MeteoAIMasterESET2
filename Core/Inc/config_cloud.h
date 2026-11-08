/**
  ******************************************************************************
  * @file    config_cloud.h
  * @brief   Identifiants ThingSpeak (TP3 §2.c et §2.d).
  *
  *  NE METTEZ PAS VOS VRAIES CLES DANS CE FICHIER : il est versionne.
  *
  *  Creez a cote un fichier config_cloud_local.h, qui est ignore par Git
  *  (voir .gitignore), avec vos valeurs :
  *
  *      #define THINGSPEAK_WRITE_API_KEY   "XXXXXXXXXXXXXXXX"
  *      #define THINGSPEAK_TALKBACK_ID     "12345"
  *      #define THINGSPEAK_TALKBACK_KEY    "YYYYYYYYYYYYYYYY"
  *
  *  Si ce fichier est absent, les valeurs par defaut ci-dessous laissent le
  *  projet compiler et tourner : la partie Cloud se desactive proprement en
  *  affichant un message, au lieu de faire echouer la compilation.
  ******************************************************************************
  */
#ifndef CONFIG_CLOUD_H
#define CONFIG_CLOUD_H

#if defined(__has_include)
#if __has_include("config_cloud_local.h")
#include "config_cloud_local.h"
#endif
#endif

/* --- Serveur ---------------------------------------------------------------
 * HTTP et non HTTPS : LWIP n'embarque pas de pile TLS dans ce projet, et en
 * ajouter une (mbedTLS) couterait plusieurs dizaines de ko de RAM pour un
 * benefice nul dans le cadre du TP. ThingSpeak accepte le port 80.
 * Limite a documenter : la cle d'API circule en clair sur le reseau.
 * -------------------------------------------------------------------------*/
#define THINGSPEAK_HOTE            "api.thingspeak.com"
#define THINGSPEAK_PORT            80

/* Cle d'ecriture du canal (onglet "API Keys" de votre canal ThingSpeak) */
#ifndef THINGSPEAK_WRITE_API_KEY
#define THINGSPEAK_WRITE_API_KEY   ""
#endif

/* Talkback : identifiant numerique et cle d'API (TP3 §2.d) */
#ifndef THINGSPEAK_TALKBACK_ID
#define THINGSPEAK_TALKBACK_ID     ""
#endif

#ifndef THINGSPEAK_TALKBACK_KEY
#define THINGSPEAK_TALKBACK_KEY    ""
#endif

/* --- Cadence ---------------------------------------------------------------
 * Le compte ThingSpeak gratuit refuse les ecritures espacees de moins de
 * 15 secondes. On prend une marge.
 * -------------------------------------------------------------------------*/
#define THINGSPEAK_PERIODE_MIN_MS  16000

/* Delai au-dela duquel on considere le serveur injoignable */
#define THINGSPEAK_TIMEOUT_S       10

#endif /* CONFIG_CLOUD_H */
