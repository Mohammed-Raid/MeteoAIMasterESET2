/**
  ******************************************************************************
  * @file    modele_ia.c
  * @brief   TP3 §3.b - inference du reseau de neurones de classification meteo.
  *
  *  FICHIER GENERE AUTOMATIQUEMENT par TP3_IA/4_generer_c.py
  *  Ne pas editer a la main : reexecuter le script apres reentrainement.
  *
  *  Le reseau est un perceptron multicouche entraine sous TensorFlow sur les
  *  releves Meteostat (TP3 §1). Les poids ci-dessous sont ceux du modele
  *  exporte en ONNX, dont la fidelite a ete verifiee a 1,2e-07 pres.
  ******************************************************************************
  */
#include "modele_ia.h"

#include <math.h>
#include <stddef.h>

/* ===========================================================================
 *  Normalisation centree-reduite.
 *  Constantes calculees sur l'ensemble d'APPRENTISSAGE uniquement.
 *  Elles doivent etre identiques a celles utilisees a l'entrainement :
 *  c'est la premiere cause d'ecart entre un modele Python et son portage C.
 * =========================================================================*/
static const float ia_moyenne[3] =
{
  +1.29586287e+01f, +7.49897919e+01f, +1.01640570e+03f
};
static const float ia_ecart_type[3] =
{
  +7.27653360e+00f, +1.78581276e+01f, +9.55475807e+00f
};

/* ===========================================================================
 *  Poids du reseau. Les noyaux sont ranges [entree][sortie].
 * =========================================================================*/

/* couche 1 : 3 -> 16 */
static const float ia_noyau1[48] =
{
  -4.87500429e-01f, -7.54672468e-01f, +4.81869102e-01f, -2.33004227e-01f, +4.40721065e-01f, +1.85387671e-01f,
  -3.38915914e-01f, +1.47752106e-01f, -4.24735248e-01f, +4.27035987e-01f, +2.80782282e-01f, +1.06648877e-01f,
  +1.81924403e-02f, +6.13173485e-01f, -3.98024827e-01f, +6.51505828e-01f, -7.90251493e-01f, +3.98598641e-01f,
  +2.37582754e-02f, -3.64444911e-01f, -7.24591734e-03f, -1.95572108e-01f, +3.87957573e-01f, -4.12413299e-01f,
  -3.24773610e-01f, +6.27762545e-03f, -4.11796391e-01f, +8.09322417e-01f, +8.36728573e-01f, +2.13875413e-01f,
  +3.00967991e-01f, +4.56633233e-02f, +6.19310029e-02f, -5.39306939e-01f, -1.61717683e-01f, +4.46182251e-01f,
  -3.95373315e-01f, -7.50896394e-01f, +3.49846572e-01f, -1.74891427e-01f, -3.83390069e-01f, -1.39943911e-02f,
  +1.67022690e-01f, +2.30355352e-01f, -1.66817904e-01f, +4.58138958e-02f, -7.47291818e-02f, +9.71946865e-02f
};
static const float ia_biais1[16] =
{
  +4.62574475e-02f, +4.35109049e-01f, -9.17617232e-02f, +1.88907743e-01f, -1.16781563e-01f, +4.50190991e-01f,
  -6.30599380e-01f, -6.47834063e-01f, -1.53689697e-01f, +2.22681567e-01f, -5.42833693e-02f, -6.19323432e-01f,
  -2.47440487e-01f, -1.63358808e-01f, -3.09296668e-01f, +2.25191295e-01f
};

/* couche 2 : 16 -> 8 */
static const float ia_noyau2[128] =
{
  -1.60200194e-01f, -5.12213707e-01f, -4.59950447e-01f, +2.51553744e-01f, +1.08472392e-01f, +5.58027387e-01f,
  +5.19187152e-01f, -3.89998049e-01f, +3.01926523e-01f, +1.09487817e-01f, +2.44803447e-02f, -2.46774539e-01f,
  +4.55846041e-01f, +7.05676734e-01f, +4.90702093e-01f, -9.06021833e-01f, +2.99963295e-01f, +2.40223587e-01f,
  +1.41357675e-01f, -4.29181099e-01f, +3.34354162e-01f, +2.08739862e-02f, +6.03115380e-01f, +2.65224457e-01f,
  +7.06526935e-01f, -6.22164011e-01f, -4.14693236e-01f, +1.62847653e-01f, -2.25084886e-01f, +3.46438661e-02f,
  -3.93698782e-01f, -1.05265081e-01f, -3.94231454e-02f, +1.38537526e-01f, -4.50395346e-01f, -5.06854057e-01f,
  +5.82596101e-02f, -1.67304933e-01f, -7.26137171e-03f, +2.84187227e-01f, -2.49630511e-02f, +1.40317753e-01f,
  +1.65017899e-02f, -1.40130475e-01f, +6.24600470e-01f, -6.77827179e-01f, +5.98330259e-01f, -3.16670120e-01f,
  -5.97606659e-01f, -3.23714554e-01f, -9.34547186e-02f, -1.54312778e+00f, +4.32557702e-01f, +1.32688314e-01f,
  -1.03493881e+00f, +1.24078877e-01f, +9.59056988e-02f, +6.77785277e-01f, +2.07525969e-01f, +5.55677652e-01f,
  -5.82518220e-01f, +9.61176634e-01f, -5.78668296e-01f, +1.38848948e+00f, +3.96782309e-01f, +6.07102752e-01f,
  -9.49267298e-02f, +4.24322098e-01f, -2.78427631e-01f, +6.77653626e-02f, -3.01141590e-01f, +3.99397969e-01f,
  -3.14518780e-01f, -5.22839487e-01f, -4.15053368e-01f, +6.91308677e-01f, +3.74082811e-02f, +7.96210840e-02f,
  +1.27132133e-01f, +1.06192261e-01f, +7.25721002e-01f, -3.23442519e-01f, -1.60928845e-01f, +6.96561158e-01f,
  +4.81778592e-01f, -1.67636946e-01f, -7.80186132e-02f, -3.31366807e-01f, -2.71779269e-01f, +1.97478995e-01f,
  -1.75196275e-01f, -1.47226915e-01f, +1.02105379e+00f, -3.82212520e-01f, -9.63197172e-01f, +5.03489226e-02f,
  -3.73607546e-01f, +6.34126514e-02f, -1.03469543e-01f, -4.07963365e-01f, +4.80771542e-01f, -3.90252829e-01f,
  +3.22038680e-01f, -7.24106729e-01f, -4.66193765e-01f, -2.86399424e-01f, +3.11662436e-01f, +3.01709175e-01f,
  +5.57878613e-01f, -3.42525125e-01f, +1.99352950e-01f, +2.93150842e-01f, -6.35422826e-01f, +4.07157630e-01f,
  -1.01411372e-01f, -6.81383312e-01f, +5.90794742e-01f, +3.82999152e-01f, +4.16451544e-01f, +9.09264237e-02f,
  +6.42147243e-01f, -4.26522464e-01f, -4.76861119e-01f, +2.67884940e-01f, -1.87587336e-01f, +5.95285058e-01f,
  -2.59170383e-01f, +3.55168879e-01f
};
static const float ia_biais2[8] =
{
  +3.92374367e-01f, -2.00751185e-01f, -3.03383395e-02f, +1.24350496e-01f, -5.13017066e-02f, +1.15727365e-01f,
  -5.40388711e-02f, -2.24662796e-01f
};

/* couche 3 : 8 -> 4 */
static const float ia_noyau3[32] =
{
  +4.05573606e-01f, +6.44778430e-01f, +9.27143618e-02f, -8.90437245e-01f, +6.29223108e-01f, -4.29327130e-01f,
  +3.90657485e-01f, -6.28682733e-01f, +2.60633290e-01f, +6.76527202e-01f, +5.57200126e-02f, -4.66372192e-01f,
  +6.55030489e-01f, -2.09791139e-02f, -7.82472014e-01f, -5.10944545e-01f, +8.83478224e-02f, +2.54559547e-01f,
  -6.92705587e-02f, +1.13037217e+00f, +1.31995961e-01f, -6.30087972e-01f, -4.75893199e-01f, -6.18381143e-01f,
  -4.11941618e-01f, +3.81097645e-01f, +2.73926735e-01f, +4.64142472e-01f, +4.89463098e-02f, -1.04210532e+00f,
  -5.21005869e-01f, +2.09483758e-01f
};
static const float ia_biais3[4] =
{
  -9.92902219e-02f, +1.14818975e-01f, +1.01231094e-02f, +3.94420251e-02f
};

const char *const ia_noms_classes[IA_NB_CLASSES] = { "Clair", "Partiellement nuageux", "Couvert", "Precipitations" };

/* ===========================================================================
 *  Couche dense : sortie[j] = biais[j] + somme_i entree[i] * noyau[i][j]
 * =========================================================================*/
static void dense(const float *entree, int n_entree,
                  const float *noyau, const float *biais,
                  float *sortie, int n_sortie, int relu)
{
  int i;
  int j;

  for (j = 0; j < n_sortie; j++)
  {
    float somme = biais[j];

    for (i = 0; i < n_entree; i++)
    {
      somme += entree[i] * noyau[i * n_sortie + j];
    }

    if (relu && somme < 0.0f)
    {
      somme = 0.0f;
    }
    sortie[j] = somme;
  }
}

/* Softmax numeriquement stable : on retranche le maximum avant l'exponentielle
   pour eviter tout debordement. */
static void softmax(float *valeurs, int n)
{
  int i;
  float maximum = valeurs[0];
  float total = 0.0f;

  for (i = 1; i < n; i++)
  {
    if (valeurs[i] > maximum)
    {
      maximum = valeurs[i];
    }
  }

  for (i = 0; i < n; i++)
  {
    valeurs[i] = expf(valeurs[i] - maximum);
    total += valeurs[i];
  }

  for (i = 0; i < n; i++)
  {
    valeurs[i] /= total;
  }
}

int ia_predire(const float entrees_brutes[IA_NB_ENTREES],
               float probabilites[IA_NB_CLASSES])
{
  float normalisees[IA_NB_ENTREES];
  float couche1[16];
  float couche2[8];
  float sortie[IA_NB_CLASSES];
  int i;
  int meilleure = 0;

  for (i = 0; i < IA_NB_ENTREES; i++)
  {
    normalisees[i] = (entrees_brutes[i] - ia_moyenne[i]) / ia_ecart_type[i];
  }

  dense(normalisees, IA_NB_ENTREES, ia_noyau1, ia_biais1, couche1, 16, 1);
  dense(couche1, 16, ia_noyau2, ia_biais2, couche2, 8, 1);
  dense(couche2, 8, ia_noyau3, ia_biais3, sortie, IA_NB_CLASSES, 0);
  softmax(sortie, IA_NB_CLASSES);

  for (i = 1; i < IA_NB_CLASSES; i++)
  {
    if (sortie[i] > sortie[meilleure])
    {
      meilleure = i;
    }
  }

  if (probabilites != NULL)
  {
    for (i = 0; i < IA_NB_CLASSES; i++)
    {
      probabilites[i] = sortie[i];
    }
  }

  return meilleure;
}

/* ===========================================================================
 *  Auto-test : vecteurs dont la reference vient d'onnxruntime sur PC.
 *  Tolerance 1e-4 : largement au-dessus du bruit d'arrondi float32, et
 *  largement en-dessous de tout ecart qui changerait une decision.
 * =========================================================================*/
#define IA_NB_TESTS   12

static const float ia_test_entrees[36] =
{
  +2.00038185e+01f, +4.03895683e+01f, +9.86962402e+02f, +3.08885517e+01f, +5.56061058e+01f, +1.01331891e+03f,
  +2.60274277e+01f, +6.03638611e+01f, +1.01064136e+03f, +4.00828743e+00f, +6.42797852e+01f, +1.03544421e+03f,
  +7.00665140e+00f, +9.96400223e+01f, +1.01960742e+03f, +2.99421387e+01f, +8.34129562e+01f, +1.01327649e+03f,
  -4.78938770e+00f, +6.97743378e+01f, +1.01232806e+03f, +2.78491364e+01f, +9.91168137e+01f, +9.98613342e+02f,
  +2.68827763e+01f, +3.72246971e+01f, +9.85648682e+02f, +1.37173977e+01f, +3.28169632e+01f, +9.95582092e+02f,
  +7.12129688e+00f, +6.90031662e+01f, +1.02306177e+03f, +6.13702440e+00f, +2.35153599e+01f, +9.96033386e+02f
};
static const float ia_test_attendus[48] =
{
  +1.45208910e-01f, +4.80906934e-01f, +1.11904413e-01f, +2.61979729e-01f, +3.93363178e-01f, +3.19727808e-01f,
  +3.16870548e-02f, +2.55221993e-01f, +2.24611089e-01f, +4.11433965e-01f, +6.28724322e-02f, +3.01082522e-01f,
  +5.15546143e-01f, +3.30168277e-01f, +1.20470665e-01f, +3.38149518e-02f, +1.38492092e-01f, +1.35467306e-01f,
  +8.32910240e-02f, +6.42749548e-01f, +1.87215567e-01f, +2.24600405e-01f, +4.96498160e-02f, +5.38534164e-01f,
  +3.47495228e-01f, +3.78500491e-01f, +1.25347793e-01f, +1.48656547e-01f, +5.14083030e-03f, +6.74601719e-02f,
  +1.49261104e-02f, +9.12472785e-01f, +2.24653944e-01f, +2.11976975e-01f, +4.47734892e-02f, +5.18595576e-01f,
  +2.37417430e-01f, +5.18570483e-01f, +7.33602792e-02f, +1.70651823e-01f, +4.41912651e-01f, +3.43784124e-01f,
  +1.50871485e-01f, +6.34316877e-02f, +4.65757132e-01f, +4.59193975e-01f, +3.08530275e-02f, +4.41958420e-02f
};

int ia_autotest(void)
{
  float probabilites[IA_NB_CLASSES];
  int echecs = 0;
  int t;
  int i;

  for (t = 0; t < IA_NB_TESTS; t++)
  {
    ia_predire(&ia_test_entrees[t * IA_NB_ENTREES], probabilites);

    for (i = 0; i < IA_NB_CLASSES; i++)
    {
      float ecart = probabilites[i] - ia_test_attendus[t * IA_NB_CLASSES + i];

      if (ecart < 0.0f)
      {
        ecart = -ecart;
      }
      if (ecart > 1.0e-4f)
      {
        echecs++;
        break;
      }
    }
  }

  return echecs;
}
