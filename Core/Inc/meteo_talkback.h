/* meteo_talkback.h - Lecture de la commande TalkBack "METEO:<classe>:<confiance>" */
#ifndef METEO_TALKBACK_H
#define METEO_TALKBACK_H

/* Cherche "METEO:" dans la réponse HTTP de ThingSpeak.
 * Retourne 1 et remplit classe / confiance si trouvé, 0 sinon (file TalkBack vide). */
int meteo_talkback_lire(const char *reponse_http, int *classe, int *confiance);

#endif
