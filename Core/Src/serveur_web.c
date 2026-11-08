/**
  ******************************************************************************
  * @file    serveur_web.c
  * @brief   Tableau de bord HTTP servi par la carte.
  *
  *  Un serveur HTTP minimal sur les sockets BSD de LWIP. Deux ressources :
  *    GET /           la page, un fichier statique embarque dans la Flash
  *    GET /api/etat   l'etat courant en JSON, interroge chaque seconde
  *
  *  La page n'a aucune dependance externe : ni police, ni bibliotheque, ni
  *  CDN. Elle s'affiche donc meme si la carte n'a pas d'acces Internet, ce
  *  qui est exactement la demonstration recherchee : debrancher la liaison
  *  Internet fait tomber la colonne Cloud et laisse l'inference locale
  *  intacte.
  *
  *  Les tampons sont statiques : ce code tourne dans une tache FreeRTOS a
  *  pile fixe, ou un tampon de plusieurs kilo-octets sur la pile provoquerait
  *  un debordement.
  ******************************************************************************
  */
#include "main.h"
#include "serveur_web.h"
#include "format.h"

#include <string.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "lwip/sockets.h"

#define PILE_TACHE_WEB       2048
#define PRIORITE_TACHE_WEB   2
#define TAILLE_REQUETE       512
#define TAILLE_JSON          2048

static etat_systeme_t etat_partage;
static SemaphoreHandle_t mutex_etat;
static char requete[TAILLE_REQUETE];
static char json[TAILLE_JSON];

/* ============================================================================
 *  La page. Guillemets simples dans le HTML pour eviter de les echapper en C.
 * ==========================================================================*/
static const char page_html[] =
"<!DOCTYPE html><html lang='fr'><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>NUCLEO-N6 Cloud vs Edge</title><style>"
":root{--bg:#0e1116;--card:#171b22;--line:#262b34;--fg:#e6edf3;--dim:#8b949e;"
"--edge:#3fb950;--cloud:#d29922;--tree:#58a6ff;--ko:#f85149}"
"*{box-sizing:border-box}"
"body{margin:0;padding:16px;background:var(--bg);color:var(--fg);"
"font:15px/1.5 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}"
"h1{font-size:19px;margin:0 0 2px}"
".sub{color:var(--dim);font-size:13px;margin-bottom:18px}"
".grid{display:grid;gap:12px;grid-template-columns:repeat(auto-fit,minmax(210px,1fr))}"
".card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:14px}"
".lab{font-size:11px;letter-spacing:.09em;text-transform:uppercase;color:var(--dim)}"
".val{font-size:19px;font-weight:600;margin:7px 0 3px;min-height:28px}"
".lat{font-size:13px;color:var(--dim);font-variant-numeric:tabular-nums}"
".edge .val{color:var(--edge)}.cloud .val{color:var(--cloud)}.tree .val{color:var(--tree)}"
".ko{color:var(--ko)}"
"h2{font-size:12px;letter-spacing:.09em;text-transform:uppercase;color:var(--dim);"
"margin:22px 0 6px;font-weight:600}"
"table{width:100%;border-collapse:collapse;font-size:14px}"
"td{padding:7px 0;border-bottom:1px solid var(--line)}"
"td:last-child{text-align:right;font-variant-numeric:tabular-nums}"
"td.u{color:var(--dim);width:60px}"
"footer{margin-top:20px;color:var(--dim);font-size:12px;line-height:1.7}"
"</style></head><body>"
"<h1>NUCLEO-N657X0-Q &middot; Cloud versus Edge</h1>"
"<div class='sub'>Page servie par la carte elle-meme &mdash; ETRS923</div>"
"<div class='grid'>"
"<div class='card edge'><div class='lab'>Edge &mdash; reseau embarque</div>"
"<div class='val' id='e'>&mdash;</div><div class='lat' id='el'>&mdash;</div></div>"
"<div class='card tree'><div class='lab'>Arbre deterministe</div>"
"<div class='val' id='a'>&mdash;</div><div class='lat' id='al'>&mdash;</div></div>"
"<div class='card cloud'><div class='lab'>Cloud &mdash; ThingSpeak</div>"
"<div class='val' id='c'>&mdash;</div><div class='lat' id='cl'>&mdash;</div></div>"
"</div>"
"<h2>Capteurs</h2><table id='t'></table>"
"<footer id='f'>Connexion...</footer>"
"<script>"
"const $=i=>document.getElementById(i);"
"function maj(d){"
"$('e').textContent=d.edge||'\\u2014';"
"$('el').textContent=d.edge_us?d.edge_us+' us  ('+d.edge_cy+' cycles)':'\\u2014';"
"$('a').textContent=d.arbre||'\\u2014';"
"$('al').textContent=d.arbre?'5 comparaisons':'\\u2014';"
"if(!d.cloud_cfg){$('c').innerHTML=\"<span class='ko'>non configure</span>\";"
"$('cl').textContent='cle ThingSpeak absente';}"
"else if(!d.cloud_ok){$('c').innerHTML=\"<span class='ko'>injoignable</span>\";"
"$('cl').textContent='aucune reponse';}"
"else{$('c').textContent=d.cloud||'en attente';"
"$('cl').textContent=d.cloud_ms+' ms  (aller-retour)';}"
"let h='';for(const r of d.cap){h+='<tr><td>'+r[0]+'</td><td>'+r[1]+"
"'</td><td class=\"u\">'+r[2]+'</td></tr>';}"
"$('t').innerHTML=h;"
"let f=d.n+' cycles, '+d.acc+' accords entre le reseau et l\\u2019arbre';"
"if(d.n>0){f+=' ('+Math.round(100*d.acc/d.n)+' %)';}"
"if(d.edge_us&&d.cloud_ok&&d.cloud_ms>0){"
"f+='<br>Le Cloud est '+Math.round(d.cloud_ms*1000/parseFloat(d.edge_us))+"
"' fois plus lent que l\\u2019inference locale.';}"
"$('f').innerHTML=f;}"
"async function tic(){try{const r=await fetch('/api/etat',{cache:'no-store'});"
"maj(await r.json());}catch(e){$('f').textContent='Carte injoignable.';}}"
"setInterval(tic,1000);tic();"
"</script></body></html>";

/* ============================================================================
 *  Construction du JSON
 * ==========================================================================*/

/* Ajoute une ligne ["nom","valeur","unite"] au tableau des capteurs */
static int ajouter_capteur(char *sortie, int reste, int *premier,
                           const char *nom, const mesure_t *m, const char *unite)
{
  char v[16];

  if (!m->valide)
  {
    return 0;
  }

  format_float_brut(m->valeur, v, sizeof(v));
  return snprintf(sortie, reste, "%s[\"%s\",\"%s\",\"%s\"]",
                  (*premier)-- > 0 ? "" : ",", nom, v, unite);
}

static int ajouter_capteur_xyz(char *sortie, int reste, int *premier,
                               const char *nom, const mesure_xyz_t *m,
                               const char *unite)
{
  char x[16];
  char y[16];
  char z[16];

  if (!m->valide)
  {
    return 0;
  }

  format_float_brut(m->x, x, sizeof(x));
  format_float_brut(m->y, y, sizeof(y));
  format_float_brut(m->z, z, sizeof(z));
  return snprintf(sortie, reste, "%s[\"%s\",\"%s / %s / %s\",\"%s\"]",
                  (*premier)-- > 0 ? "" : ",", nom, x, y, z, unite);
}

static int construire_json(char *sortie, int taille)
{
  etat_systeme_t e;
  char us[16];
  int n = 0;
  int premier;

  /* Copie sous mutex : la serialisation est longue, on ne garde pas le
     verrou pendant tout ce temps. */
  xSemaphoreTake(mutex_etat, portMAX_DELAY);
  e = etat_partage;
  xSemaphoreGive(mutex_etat);

  format_float_brut(e.duree_edge_us, us, sizeof(us));

  n += snprintf(sortie + n, taille - n,
                "{\"edge\":\"%s\",\"edge_us\":\"%s\",\"edge_cy\":%lu,"
                "\"arbre\":\"%s\",\"cloud\":\"%s\",\"cloud_ms\":%lu,"
                "\"cloud_cfg\":%u,\"cloud_ok\":%u,\"n\":%lu,\"acc\":%lu,\"cap\":[",
                (e.classe_edge != NULL) ? e.classe_edge : "",
                us,
                (unsigned long)e.cycles_edge,
                (e.classe_arbre != NULL) ? e.classe_arbre : "",
                e.classe_cloud,
                (unsigned long)e.latence_cloud_ms,
                (unsigned)e.cloud_configure,
                (unsigned)e.cloud_joignable,
                (unsigned long)e.nb_cycles,
                (unsigned long)e.nb_accords_arbre);

  premier = 1;
  n += ajouter_capteur(sortie + n, taille - n, &premier,
                       "SHT40AD1B temperature", &e.mesures.sht40_temp_c, "degC");
  n += ajouter_capteur(sortie + n, taille - n, &premier,
                       "SHT40AD1B humidite", &e.mesures.sht40_humidite_rh, "%RH");
  n += ajouter_capteur(sortie + n, taille - n, &premier,
                       "LPS22DF pression", &e.mesures.df_pression_hpa, "hPa");
  n += ajouter_capteur(sortie + n, taille - n, &premier,
                       "LPS22DF temperature", &e.mesures.df_temp_c, "degC");
  n += ajouter_capteur(sortie + n, taille - n, &premier,
                       "STTS22H temperature", &e.mesures.stts22h_temp_c, "degC");
  n += ajouter_capteur(sortie + n, taille - n, &premier,
                       "LSM6DSV16X temperature", &e.mesures.dsv16x_temp_c, "degC");
  n += ajouter_capteur_xyz(sortie + n, taille - n, &premier,
                           "LSM6DSV16X acceleration", &e.mesures.dsv16x_accel_mg, "mg");
  n += ajouter_capteur_xyz(sortie + n, taille - n, &premier,
                           "LSM6DSV16X gyroscope", &e.mesures.dsv16x_gyro_dps, "dps");
  n += ajouter_capteur_xyz(sortie + n, taille - n, &premier,
                           "LIS2DUXS12 acceleration", &e.mesures.duxs12_accel_mg, "mg");
  n += ajouter_capteur_xyz(sortie + n, taille - n, &premier,
                           "LIS2MDL champ magnetique", &e.mesures.mdl_champ_ut, "uT");

  n += snprintf(sortie + n, taille - n, "]}");
  return n;
}

/* ============================================================================
 *  Reponses HTTP
 * ==========================================================================*/
static void repondre(int douille, const char *type, const char *corps, int longueur)
{
  char entete[160];
  int n;

  n = snprintf(entete, sizeof(entete),
               "HTTP/1.1 200 OK\r\n"
               "Content-Type: %s\r\n"
               "Content-Length: %d\r\n"
               "Cache-Control: no-store\r\n"
               "Connection: close\r\n"
               "\r\n",
               type, longueur);

  send(douille, entete, n, 0);
  send(douille, corps, longueur, 0);
}

static void repondre_404(int douille)
{
  static const char corps[] = "Ressource inconnue";

  repondre(douille, "text/plain; charset=utf-8", corps, sizeof(corps) - 1);
}

/* ============================================================================
 *  Tache du serveur
 * ==========================================================================*/
static void tache_serveur(void *argument)
{
  struct sockaddr_in adresse;
  int ecoute;
  int client;
  int recus;
  int option = 1;

  (void)argument;

  ecoute = socket(AF_INET, SOCK_STREAM, 0);
  if (ecoute < 0)
  {
    printf("Serveur web : creation de socket impossible\r\n");
    vTaskDelete(NULL);
    return;
  }

  /* Permet de relancer le serveur sans attendre l'expiration du TIME_WAIT */
  setsockopt(ecoute, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

  memset(&adresse, 0, sizeof(adresse));
  adresse.sin_family = AF_INET;
  adresse.sin_port = lwip_htons(SERVEUR_WEB_PORT);
  adresse.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(ecoute, (struct sockaddr *)&adresse, sizeof(adresse)) != 0)
  {
    printf("Serveur web : port %d indisponible\r\n", SERVEUR_WEB_PORT);
    lwip_close(ecoute);
    vTaskDelete(NULL);
    return;
  }

  listen(ecoute, 4);
  printf("Serveur web pret : http://<adresse IP de la carte>/\r\n");

  while (1)
  {
    client = accept(ecoute, NULL, NULL);
    if (client < 0)
    {
      continue;
    }

    recus = recv(client, requete, sizeof(requete) - 1, 0);
    if (recus > 0)
    {
      requete[recus] = '\0';

      if (strncmp(requete, "GET /api/etat", 13) == 0)
      {
        int n = construire_json(json, sizeof(json));

        repondre(client, "application/json; charset=utf-8", json, n);
      }
      else if ((strncmp(requete, "GET / ", 6) == 0) ||
               (strncmp(requete, "GET /index.html", 15) == 0))
      {
        repondre(client, "text/html; charset=utf-8",
                 page_html, (int)(sizeof(page_html) - 1));
      }
      else
      {
        repondre_404(client);
      }
    }

    lwip_close(client);
  }
}

/* ============================================================================
 *  Interface publique
 * ==========================================================================*/
int serveur_web_demarrer(void)
{
  if (mutex_etat == NULL)
  {
    mutex_etat = xSemaphoreCreateMutex();
    if (mutex_etat == NULL)
    {
      return -1;
    }
  }

  if (xTaskCreate(tache_serveur, "Web", PILE_TACHE_WEB, NULL,
                  PRIORITE_TACHE_WEB, NULL) != pdPASS)
  {
    return -1;
  }

  return 0;
}

void serveur_web_publier(const etat_systeme_t *etat)
{
  if (mutex_etat == NULL)
  {
    return;
  }

  xSemaphoreTake(mutex_etat, portMAX_DELAY);
  etat_partage = *etat;
  xSemaphoreGive(mutex_etat);
}
