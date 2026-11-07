# Contrat d'interface — brancher un autre modèle sur la carte

À lire si tu veux remplacer le modèle actuel par le tien, par exemple généré
avec X-CUBE-AI depuis ton propre entraînement.

**Tu ne touches qu'à un seul fichier : `Core/Src/modele_ia.c`.**

Rien d'autre ne bouge : ni la lecture des capteurs, ni les LEDs, ni la boucle
principale, ni le programme témoin. C'est exactement l'intérêt d'avoir séparé
l'acquisition de l'inférence.

## Les trois choses que ton code doit fournir

Le reste du projet appelle uniquement ces symboles, définis dans
`Core/Inc/modele_ia.h`. **Ne change pas leurs signatures.**

```c
#define IA_NB_ENTREES   3
#define IA_NB_CLASSES   4

extern const char *const ia_noms_classes[IA_NB_CLASSES];

int ia_predire(const float entrees_brutes[IA_NB_ENTREES],
               float probabilites[IA_NB_CLASSES]);

int ia_autotest(void);
```

### `ia_predire`

Reçoit les **grandeurs physiques brutes**, dans cet ordre imposé :

| Indice | Grandeur | Unité | Source sur la carte |
| --- | --- | --- | --- |
| 0 | température | °C | SHT40AD1B |
| 1 | humidité relative | %RH | SHT40AD1B |
| 2 | pression | hPa | LPS22DF |

C'est **à toi** de normaliser à l'intérieur de `ia_predire`, avec les
constantes de **ton** entraînement. Ne suppose pas que les nôtres
conviennent.

Remplit `probabilites` (somme = 1) si le pointeur n'est pas `NULL`, et
retourne l'indice de la classe la plus probable.

### `ia_noms_classes`

Les libellés, **dans le même ordre que les sorties de ton modèle**. Si ton
modèle a un nombre de classes différent, change `IA_NB_CLASSES` dans le `.h`
et adapte ce tableau. Le reste du code s'y adapte tout seul, il ne code en dur
aucun nombre de classes.

### `ia_autotest`

Retourne le nombre de vecteurs de test en échec, `0` si tout va bien. Appelé
une fois au démarrage, il s'affiche sur la console.

**Ne te contente pas de retourner 0.** Embarque de vrais vecteurs : prends
une dizaine d'entrées, calcule les sorties de référence sur PC avec
`onnxruntime` ou l'interpréteur TFLite, mets-les en dur dans le fichier, et
compare. C'est ce qui attrape l'erreur la plus fréquente de tout ce TP, une
normalisation mal reportée, qui donne un modèle silencieusement faux.

Regarde comment c'est fait dans le `modele_ia.c` actuel, fonction
`ia_autotest()`, et dans le générateur `TP3_IA/4_generer_c.py`.

## Si tu passes par X-CUBE-AI

Squelette de ce que devient `modele_ia.c` :

```c
#include "modele_ia.h"
#include "network.h"        /* genere par X-CUBE-AI */
#include "network_data.h"

static ai_handle reseau = AI_HANDLE_NULL;
static ai_float entree_ai[AI_NETWORK_IN_1_SIZE];
static ai_float sortie_ai[AI_NETWORK_OUT_1_SIZE];

/* Constantes de TON entrainement */
static const float moyenne[IA_NB_ENTREES]    = { /* ... */ };
static const float ecart_type[IA_NB_ENTREES] = { /* ... */ };

const char *const ia_noms_classes[IA_NB_CLASSES] = { /* ... */ };

int ia_predire(const float entrees_brutes[IA_NB_ENTREES],
               float probabilites[IA_NB_CLASSES])
{
  ai_buffer *entree;
  ai_buffer *sortie;
  int i;
  int meilleure = 0;

  if (reseau == AI_HANDLE_NULL)
  {
    ai_network_create_and_init(&reseau, NULL, NULL);
  }

  for (i = 0; i < IA_NB_ENTREES; i++)
  {
    entree_ai[i] = (entrees_brutes[i] - moyenne[i]) / ecart_type[i];
  }

  entree = ai_network_inputs_get(reseau, NULL);
  sortie = ai_network_outputs_get(reseau, NULL);
  entree[0].data = AI_HANDLE_PTR(entree_ai);
  sortie[0].data = AI_HANDLE_PTR(sortie_ai);

  ai_network_run(reseau, entree, sortie);

  for (i = 0; i < IA_NB_CLASSES; i++)
  {
    if (probabilites != NULL)
    {
      probabilites[i] = sortie_ai[i];
    }
    if (sortie_ai[i] > sortie_ai[meilleure])
    {
      meilleure = i;
    }
  }

  return meilleure;
}
```

Deux pièges à connaître :

**L'initialisation du réseau est coûteuse.** Fais-la une seule fois, pas à
chaque appel, sinon la mesure de latence du §3.c et celle de consommation du
§3.d n'ont plus aucun sens.

**Vérifie si ton modèle applique déjà le softmax.** Si X-CUBE-AI te rend des
logits bruts, il faut l'ajouter, sinon les « probabilités » affichées ne
sommeront pas à 1.

## Ce que tu n'as pas besoin de changer

- `Core/Src/partie4_ia.c` — la boucle, les LEDs, la mesure DWT, le programme
  témoin. Tout passe par les trois symboles ci-dessus.
- `Core/Src/capteurs.c` — l'acquisition.
- `Core/Src/baseline_arbre.c` — l'arbre de décision de référence. Laisse-le :
  c'est lui qui permet de dire si ton modèle vaut son coût énergétique.
- Le `Makefile` — il prend les nouveaux `.c` de `Core/Src/` automatiquement.

## Vérifier que ça marche

```
make PARTIE=4
```

Puis sur la console, au démarrage :

```
Autotest du modele : OK (portage C fidele a l'ONNX)
```

Si tu lis autre chose, ton portage est faux et il ne sert à rien d'aller plus
loin.
