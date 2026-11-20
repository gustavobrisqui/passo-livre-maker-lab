// ===================================================================
// passo_livre_model.h  -  GERADO por ml/train_model.py. NÃO EDITE À MÃO.
// MLP 7 -> 16 (ReLU) -> 6 (softmax), 230 parâmetros.
// Acurácia no conjunto de teste (simulado): 0.9947
// ===================================================================
#pragma once
#include <math.h>

#define PL_WINDOW     10
#define PL_N_FEATURES 7
#define PL_N_HIDDEN   16
#define PL_N_CLASSES  6

enum PlClass { PL_LIVRE = 0, PL_OBSTACULO = 1, PL_PESSOA = 2, PL_BURACO = 3, PL_DEGRAU = 4, PL_RAMPA = 5 };
static const char* const PL_CLASS_NAMES[PL_N_CLASSES] = { "LIVRE", "OBSTACULO", "PESSOA", "BURACO", "DEGRAU", "RAMPA" };

static const float PL_FEAT_MEAN[7] = { 193.25255412f, 167.59858884f, 25.60989721f, -25.96321390f, 14.44888518f, 2.72604130f, 3.47985241f };
static const float PL_FEAT_STD[7] = { 115.69309313f, 124.90832232f, 37.83373576f, 91.24719067f, 13.79150579f, 6.07279076f, 3.38909009f };
static const float PL_W1[112] = { -0.42010443f, 0.54258501f, -0.84689091f, 0.88915935f, -1.70912424f, 2.07629162f, 0.46123284f, 1.43532716f, -0.96428327f, -0.23568319f, -0.04221310f, 1.43172127f, -1.63894906f, 1.09772015f, 1.22636804f, -0.98048924f, 0.06092854f, 0.31024666f, -2.74113013f, -1.46576468f, -2.05493512f, -0.39719322f, 0.33190643f, 0.64269702f, 1.67862562f, 0.32355417f, -0.61974730f, -0.36664009f, -0.42653687f, 0.25591074f, 0.21623929f, 0.32622170f, -3.10503979f, 1.24738010f, -0.92581275f, -0.57781683f, 0.02615456f, -2.38654451f, 1.27110991f, 1.76795277f, -0.33547157f, -0.05550327f, -2.01822982f, 0.01866632f, 0.66664907f, 0.24646984f, 2.13307084f, -0.66417554f, -0.15374523f, 0.89808904f, 0.07102936f, -0.18770482f, 0.82071065f, -2.87270328f, 0.76858779f, 0.15002456f, -0.10170967f, -1.12280779f, -1.29210596f, -0.92026965f, -2.94281475f, 0.26625523f, -0.17998149f, -1.43486337f, 3.08282638f, -0.19888416f, -0.70466808f, 2.05794646f, 0.96125856f, -0.69404725f, -0.21462678f, -0.14683612f, -2.37021192f, 1.21898467f, 1.75678672f, -0.34297132f, 0.04458240f, 0.65223341f, 1.06264583f, 1.32102833f, 0.57114921f, 0.94772536f, 0.66451050f, 1.15725957f, 0.93861340f, 1.00318593f, -1.03403621f, 0.31370856f, 0.31242585f, 0.50711669f, -1.22645807f, -2.04899095f, 1.07382397f, 0.17103949f, -0.46835819f, -3.05897306f, 0.40877995f, -0.31540959f, 0.00334920f, 0.65847683f, -0.13319916f, 0.71021047f, -2.53035408f, -0.22945926f, 1.00449700f, -0.17020656f, -0.84611723f, -0.01236976f, 0.19740209f, 1.81778938f, -2.73486885f, -2.29316717f };
static const float PL_B1[16] = { 0.33574117f, 2.01711546f, 0.85941758f, 0.60618091f, 0.21891985f, 0.20515077f, -0.58774268f, 0.64654803f, 0.72448383f, 1.91076965f, 0.47125153f, 0.92314820f, 1.92885821f, -0.15308238f, 0.47461097f, 1.65737046f };
static const float PL_W2[96] = { -2.72111545f, 1.46903247f, 1.01886115f, 0.63137163f, -1.04924315f, 0.46366120f, -1.22156545f, 0.36827099f, -2.99341018f, 2.20657612f, 1.52716710f, -1.91513008f, -1.08782487f, -0.00445628f, 0.22834286f, 0.35757567f, -0.50341084f, -2.80760972f, -1.17206437f, 0.90284268f, -0.32797992f, 1.61771219f, 1.63966560f, -3.39620324f, -0.45064071f, -0.02596545f, 1.00879896f, 0.53776396f, -3.11828231f, 1.28966344f, -0.70740614f, 0.58796593f, 0.79386545f, 0.98575430f, 0.80815342f, -1.18334624f, 0.14125420f, -6.20637580f, -0.18513689f, 0.07889332f, 1.87036519f, -2.13887976f, -5.41272932f, -0.48168200f, 0.70022888f, -2.12809166f, -0.12350307f, 0.86040241f, 0.68168000f, -0.21762772f, -3.90154637f, 1.61990267f, -0.31378931f, 1.66939112f, -0.88141090f, -3.10198219f, -2.26142764f, 2.99181527f, 1.38060750f, 1.66851898f, 1.38553057f, -2.96715114f, -4.47147828f, 1.27663674f, 1.02448282f, -4.07455730f, 1.16103200f, -1.59589198f, 3.62801911f, -1.69368389f, -1.13527317f, 2.69664302f, 0.42223328f, -1.67303559f, -3.03076813f, -0.47219348f, 0.65800113f, 2.43694092f, 1.11712209f, -3.21716699f, 1.83305935f, 2.46345661f, -3.72954992f, -1.36715005f, -2.92129287f, -0.18314436f, -1.75211750f, 0.42469177f, 0.25662617f, -1.71584012f, 1.13423321f, 2.45659949f, -2.55634670f, -1.32972802f, 2.57180684f, -2.82580313f };
static const float PL_B2[6] = { 0.05671807f, -1.32116741f, 0.49737814f, 1.52719243f, -0.06992475f, 0.90916273f };

// Mesmas features de ml/train_model.py::extract_features
static inline void pl_extract_features(const float* front, const float* floor_,
                                       const float* pitch, int n, float* out) {
  float fs = 0, fmin = front[0], fl = 0, ps = 0;
  for (int i = 0; i < n; i++) {
    fs += front[i]; fl += floor_[i]; ps += pitch[i];
    if (front[i] < fmin) fmin = front[i];
  }
  float fmean = fs / n, flmean = fl / n;
  float fvar = 0, flvar = 0;
  for (int i = 0; i < n; i++) {
    fvar += (front[i] - fmean) * (front[i] - fmean);
    flvar += (floor_[i] - flmean) * (floor_[i] - flmean);
  }
  out[0] = fmean;
  out[1] = fmin;
  out[2] = sqrtf(fvar / n);
  out[3] = front[n - 1] - front[0];
  out[4] = flmean;
  out[5] = sqrtf(flvar / n);
  out[6] = fabsf(ps / n);
}

// Retorna a classe e preenche probs[PL_N_CLASSES]
static inline int pl_predict(const float* feat, float* probs) {
  float x[PL_N_FEATURES], h[PL_N_HIDDEN];
  for (int i = 0; i < PL_N_FEATURES; i++) x[i] = (feat[i] - PL_FEAT_MEAN[i]) / PL_FEAT_STD[i];
  for (int j = 0; j < PL_N_HIDDEN; j++) {
    float a = PL_B1[j];
    for (int i = 0; i < PL_N_FEATURES; i++) a += PL_W1[j * PL_N_FEATURES + i] * x[i];
    h[j] = a > 0 ? a : 0;
  }
  float mx = -1e30f;
  for (int k = 0; k < PL_N_CLASSES; k++) {
    float a = PL_B2[k];
    for (int j = 0; j < PL_N_HIDDEN; j++) a += PL_W2[k * PL_N_HIDDEN + j] * h[j];
    probs[k] = a;
    if (a > mx) mx = a;
  }
  float sum = 0; int best = 0;
  for (int k = 0; k < PL_N_CLASSES; k++) { probs[k] = expf(probs[k] - mx); sum += probs[k]; }
  for (int k = 0; k < PL_N_CLASSES; k++) { probs[k] /= sum; if (probs[k] > probs[best]) best = k; }
  return best;
}
