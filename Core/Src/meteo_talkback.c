/* meteo_talkback.c */
#include <stdio.h>
#include <string.h>
#include "meteo_talkback.h"

int meteo_talkback_lire(const char *reponse_http, int *classe, int *confiance)
{
    const char *debut = strstr(reponse_http, "METEO:");
    if (debut == NULL) {
        return 0;
    }
    if (sscanf(debut, "METEO:%d:%d", classe, confiance) != 2) {
        return 0;
    }
    return 1;
}
