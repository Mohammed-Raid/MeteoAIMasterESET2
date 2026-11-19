/* Généré automatiquement par TP3_Meteo_Colab.ipynb le 2026-11-19 18:48 - ne pas modifier à la main */
#ifndef METEO_MODEL_PARAMS_H
#define METEO_MODEL_PARAMS_H

#define METEO_NB_FEATURES 4   /* ordre : temp, rhum, pres, dp3h */
#define METEO_NB_CLASSES  4

static const float METEO_MEAN[METEO_NB_FEATURES] = { 11.501699f, 70.190781f, 1017.795654f, -0.001726f };
static const float METEO_STD[METEO_NB_FEATURES]  = { 8.617645f, 18.975735f, 7.714952f, 1.328217f };
static const char *const METEO_CLASS_NAMES[METEO_NB_CLASSES] = { "Clair", "Nuageux", "Pluie", "Neige" };

/* Paramètres de quantification INT8 (uniquement pour l'option NPU) */
#define METEO_IN_SCALE  0.033068068f
#define METEO_IN_ZP     (-1)
#define METEO_OUT_SCALE 0.003906250f
#define METEO_OUT_ZP    (-128)

#define METEO_NB_TESTS 8
static const float METEO_TEST_IN[METEO_NB_TESTS][METEO_NB_FEATURES] = {
  { 24.000000f, 69.000000f, 1017.099976f, 1.000000f },
  { 10.000000f, 82.000000f, 1017.000000f, 2.000000f },
  { 15.000000f, 94.000000f, 1020.000000f, -1.000000f },
  { 0.200000f, 88.000000f, 1013.700012f, -1.400000f },
  { 11.300000f, 56.000000f, 1013.799988f, 0.700000f },
  { 18.299999f, 38.000000f, 1018.200012f, -2.900000f },
  { 8.600000f, 88.000000f, 1023.900024f, 1.300000f },
  { 7.700000f, 84.000000f, 1016.599976f, -0.400000f },
};
static const float METEO_TEST_OUT[METEO_NB_TESTS][METEO_NB_CLASSES] = {
  { 0.396836f, 0.452643f, 0.150521f, 0.000000f },
  { 0.100757f, 0.423829f, 0.475199f, 0.000216f },
  { 0.177360f, 0.432241f, 0.390399f, 0.000000f },
  { 0.079292f, 0.232184f, 0.067806f, 0.620717f },
  { 0.334428f, 0.519266f, 0.146020f, 0.000285f },
  { 0.775789f, 0.215567f, 0.008643f, 0.000000f },
  { 0.239922f, 0.516679f, 0.243312f, 0.000087f },
  { 0.167148f, 0.497835f, 0.334155f, 0.000862f },
};

#endif