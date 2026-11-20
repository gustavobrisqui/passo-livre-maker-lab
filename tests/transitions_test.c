// Simula a janela deslizante + filtro temporal do firmware (3 janelas, conf >= 0,7)
// sobre sequências de leitura e confere a sequência de classes confirmadas.
// gcc -O2 -I firmware tests/transitions_test.c -lm -o transitions && ./transitions
#include <stdio.h>
#include <string.h>
#include "passo_livre_model.h"

static int run(const char* name, const float* front, int n, const char* expected) {
  float f[PL_WINDOW], fl[PL_WINDOW], p[PL_WINDOW], feat[PL_N_FEATURES], pr[PL_N_CLASSES];
  int cand = -1, cnt = 0, conf = PL_LIVRE; char got[256] = "LIVRE";
  for (int t = PL_WINDOW; t <= n; t++) {
    for (int i = 0; i < PL_WINDOW; i++) { f[i] = front[t - PL_WINDOW + i]; fl[i] = 10; p[i] = 0; }
    pl_extract_features(f, fl, p, PL_WINDOW, feat);
    int c = pl_predict(feat, pr);
    if (pr[c] >= 0.7f && c == cand) cnt++; else { cand = c; cnt = pr[c] >= 0.7f; }
    if (cnt >= 3 && cand != conf) { conf = cand; strcat(got, ">"); strcat(got, PL_CLASS_NAMES[conf]); }
  }
  int ok = strcmp(got, expected) == 0;
  printf("[%s] %-34s %s\n", ok ? "OK " : "ERRO", name, got);
  return ok;
}

int main(void) {
  float a[40], b[40], c[40], d[40];
  for (int i = 0; i < 40; i++) {
    a[i] = i < 15 ? 400 : 30;                                  // carro aparece e fica
    b[i] = i < 15 ? 30 : 400;                                  // obstáculo sai
    c[i] = i < 10 ? 400 : (i < 20 ? 300 - (i - 10) * 25 : 400); // pedestre vem e passa
    d[i] = 400;                                                // calçada livre
  }
  int ok = 1;
  ok &= run("carro surge e fica", a, 40, "LIVRE>PESSOA>OBSTACULO");
  ok &= run("obstáculo sai da frente", b, 40, "LIVRE>OBSTACULO>LIVRE");
  ok &= run("pedestre se aproxima e passa", c, 40, "LIVRE>PESSOA>LIVRE");
  ok &= run("calçada livre", d, 40, "LIVRE");
  return ok ? 0 : 1;
}
