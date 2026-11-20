// Teste de paridade: o modelo em C (o mesmo que roda no ESP32) precisa
// dar as mesmas probabilidades que o sklearn deu no treino.
// gcc -O2 -I firmware tests/parity_test.c -lm -o parity && ./parity tests/parity_windows.csv
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "passo_livre_model.h"

int main(int argc, char** argv) {
  FILE* f = fopen(argc > 1 ? argv[1] : "tests/parity_windows.csv", "r");
  if (!f) { perror("csv"); return 2; }
  int n = 0, argmax_mismatch = 0; double max_err = 0;
  float front[PL_WINDOW], floor_[PL_WINDOW], pitch[PL_WINDOW], ref[PL_N_CLASSES];
  for (;;) {
    int ok = 1;
    for (int i = 0; i < PL_WINDOW && ok; i++) ok = fscanf(f, "%f,", &front[i]) == 1;
    for (int i = 0; i < PL_WINDOW && ok; i++) ok = fscanf(f, "%f,", &floor_[i]) == 1;
    for (int i = 0; i < PL_WINDOW && ok; i++) ok = fscanf(f, "%f,", &pitch[i]) == 1;
    for (int k = 0; k < PL_N_CLASSES && ok; k++) ok = fscanf(f, "%f,", &ref[k]) == 1;
    if (!ok) break;
    float feat[PL_N_FEATURES], probs[PL_N_CLASSES];
    pl_extract_features(front, floor_, pitch, PL_WINDOW, feat);
    int c = pl_predict(feat, probs);
    int rc = 0;
    for (int k = 0; k < PL_N_CLASSES; k++) {
      double e = fabs(probs[k] - ref[k]);
      if (e > max_err) max_err = e;
      if (ref[k] > ref[rc]) rc = k;
    }
    if (c != rc) argmax_mismatch++;
    n++;
  }
  printf("janelas: %d | classes divergentes: %d | erro máx. de probabilidade: %.6f\n",
         n, argmax_mismatch, max_err);
  return (n > 0 && argmax_mismatch == 0 && max_err < 1e-3) ? 0 : 1;
}
