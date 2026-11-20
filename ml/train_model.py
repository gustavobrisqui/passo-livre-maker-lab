"""
Passo Livre - Maker Lab
Treino do classificador TinyML que roda dentro do ESP32.

O que este script faz:
  1. Gera um dataset SIMULADO de janelas de 1 s (10 leituras a 10 Hz) dos três
     sensores do robô: ultrassônico frontal, ultrassônico de piso e MPU6050.
     A geração segue a física do problema e os limites da NBR 9050 (ver
     docs/dataset.md); a GS aceita dados simulados desde que a geração seja
     justificada e documentada.
  2. Extrai as MESMAS 7 features que o firmware calcula (pl_extract_features).
  3. Treina uma MLP minúscula (7 -> 16 -> 6, ~230 parâmetros).
  4. Exporta pesos + normalização para firmware/passo_livre_model.h.
  5. Grava tests/parity_windows.csv para o teste de paridade Python x C.

Uso:  python ml/train_model.py
"""
from pathlib import Path
import json

import numpy as np
from sklearn.metrics import classification_report, confusion_matrix
from sklearn.model_selection import train_test_split
from sklearn.neural_network import MLPClassifier

ROOT = Path(__file__).resolve().parents[1]
RNG = np.random.default_rng(42)

WINDOW = 10            # 10 amostras = 1 s a 10 Hz
FLOOR_NOMINAL = 10.0   # sensor de piso montado a 10 cm do chão, apontado para baixo/à frente
MAX_CM = 400.0         # alcance do HC-SR04; timeout de eco vira 400
NBR9050_RAMP_DEG = np.degrees(np.arctan(0.0833))  # 8,33 % = 4,76 graus

CLASSES = ["LIVRE", "OBSTACULO", "PESSOA", "BURACO", "DEGRAU", "RAMPA"]
N_PER_CLASS = 3000


# ---------------------------------------------------------------- geração
def _noise(n, sigma):
    return RNG.normal(0.0, sigma, n)


def _dropouts(x, p=0.02):
    """Eco perdido no HC-SR04: o firmware converte timeout em MAX_CM."""
    mask = RNG.random(len(x)) < p
    x = x.copy()
    x[mask] = MAX_CM
    return x


def _ramp(start, delta):
    return start + np.linspace(0.0, delta, WINDOW)


def _floor_ok(sigma):
    return FLOOR_NOMINAL + _noise(WINDOW, sigma)


def _pitch_ok():
    # inclinações dentro da norma (até ~4,3 graus) continuam LIVRE
    return RNG.uniform(-4.3, 4.3) + _noise(WINDOW, RNG.uniform(0.0, 0.8))


def _front_free(sigma):
    return _ramp(RNG.uniform(90, MAX_CM), RNG.uniform(-25, 0)) + _noise(WINDOW, sigma)


def gen_window(label):
    s = RNG.uniform(0.0, 1.2)  # ruído do sensor varia por janela (inclui janelas "limpas" como no Wokwi)
    if label == "LIVRE":
        r = RNG.random()
        if r < 0.25:
            front = np.full(WINDOW, MAX_CM)
        elif r < 0.45:
            # algo SAINDO da frente (pedestre passou, carro saiu): distância sobe = caminho liberando
            k = RNG.integers(1, WINDOW)
            front = np.full(WINDOW, RNG.uniform(8, 80)) + _noise(WINDOW, s)
            front[k:] = RNG.uniform(150, MAX_CM) + _noise(WINDOW - k, s)
        else:
            front = _front_free(s)
        floor, pitch = _floor_ok(s * 0.5), _pitch_ok()
    elif label == "OBSTACULO":
        # carro na calçada, entulho, lixeira: parado. Robô parado (0) ou andando a ~20 cm/s
        front = _ramp(RNG.uniform(8, 60), RNG.uniform(-25, 0)) + _noise(WINDOW, s)
        floor, pitch = _floor_ok(s * 0.5), _pitch_ok()
    elif label == "PESSOA":
        # pedestre se aproximando: 0,5 a 2 m/s relativos -> queda de 50 a 200 cm em 1 s
        if RNG.random() < 0.35:
            # alguém ENTRA na frente de lado: distância despenca de uma amostra para outra
            k = RNG.integers(1, WINDOW)
            front = np.full(WINDOW, RNG.uniform(150, MAX_CM)) + _noise(WINDOW, s)
            front[k:] = RNG.uniform(15, 100) + _noise(WINDOW - k, s * 2)
        else:
            start = RNG.uniform(80, 320)
            delta = -min(RNG.uniform(50, 200), start - 15)
            front = _ramp(start, delta) + _noise(WINDOW, s * 2)
        floor, pitch = _floor_ok(s * 0.5), _pitch_ok()
    elif label == "BURACO":
        k = RNG.integers(0, 5)  # borda do buraco aparece no meio da janela
        floor = _floor_ok(s * 0.5)
        floor[k:] = RNG.uniform(16, 80) + _noise(WINDOW - k, s * 3)
        front, pitch = _front_free(s), _pitch_ok()
    elif label == "DEGRAU":
        k = RNG.integers(0, 5)  # guia/degrau de 4 a 8 cm sem rampa
        floor = _floor_ok(s * 0.5)
        floor[k:] = RNG.uniform(2.5, 6.0) + _noise(WINDOW - k, s * 0.3)
        front = _ramp(RNG.uniform(30, MAX_CM), RNG.uniform(-25, 0)) + _noise(WINDOW, s)
        pitch = _pitch_ok()
    elif label == "RAMPA":
        # acima de 8,33 % (4,76 graus), subida ou descida
        sign = 1 if RNG.random() > 0.5 else -1
        pitch = sign * RNG.uniform(5.3, 15.0) + _noise(WINDOW, RNG.uniform(0.0, 0.8))
        front, floor = _front_free(s), _floor_ok(s * 0.8)
    else:
        raise ValueError(label)

    front = np.clip(_dropouts(front), 2, MAX_CM)
    floor = np.clip(floor, 2, MAX_CM)
    return front, floor, pitch


# ---------------------------------------------------------------- features
# ATENÇÃO: idêntico a pl_extract_features() no header C. Mudou aqui, muda lá.
FEATURES = ["front_mean", "front_min", "front_std", "front_delta",
            "floor_mean", "floor_std", "pitch_abs_mean"]


def extract_features(front, floor, pitch):
    return np.array([
        front.mean(),
        front.min(),
        front.std(),
        front[-1] - front[0],
        floor.mean(),
        floor.std(),
        abs(pitch.mean()),
    ], dtype=np.float64)


def build_dataset():
    X, y, raw = [], [], []
    for ci, name in enumerate(CLASSES):
        for _ in range(N_PER_CLASS):
            f, fl, p = gen_window(name)
            X.append(extract_features(f, fl, p))
            y.append(ci)
            raw.append(np.concatenate([f, fl, p]))
    return np.array(X), np.array(y), np.array(raw)


# ---------------------------------------------------------------- export
def c_array(name, arr):
    flat = ", ".join(f"{v:.8f}f" for v in np.asarray(arr).ravel())
    return f"static const float {name}[{np.asarray(arr).size}] = {{ {flat} }};"


def export_header(model, mu, sigma, path, metrics):
    W1 = model.coefs_[0].T        # (hidden, in)
    b1 = model.intercepts_[0]
    W2 = model.coefs_[1].T        # (out, hidden)
    b2 = model.intercepts_[1]
    n_in, n_hid, n_out = W1.shape[1], W1.shape[0], W2.shape[0]
    names = ", ".join(f'"{c}"' for c in CLASSES)

    header = f"""// ===================================================================
// passo_livre_model.h  -  GERADO por ml/train_model.py. NÃO EDITE À MÃO.
// MLP {n_in} -> {n_hid} (ReLU) -> {n_out} (softmax), {model.coefs_[0].size + n_hid + model.coefs_[1].size + n_out} parâmetros.
// Acurácia no conjunto de teste (simulado): {metrics['accuracy']:.4f}
// ===================================================================
#pragma once
#include <math.h>

#define PL_WINDOW     {WINDOW}
#define PL_N_FEATURES {n_in}
#define PL_N_HIDDEN   {n_hid}
#define PL_N_CLASSES  {n_out}

enum PlClass {{ {", ".join(f"PL_{c} = {i}" for i, c in enumerate(CLASSES))} }};
static const char* const PL_CLASS_NAMES[PL_N_CLASSES] = {{ {names} }};

{c_array("PL_FEAT_MEAN", mu)}
{c_array("PL_FEAT_STD", sigma)}
{c_array("PL_W1", W1)}
{c_array("PL_B1", b1)}
{c_array("PL_W2", W2)}
{c_array("PL_B2", b2)}

// Mesmas features de ml/train_model.py::extract_features
static inline void pl_extract_features(const float* front, const float* floor_,
                                       const float* pitch, int n, float* out) {{
  float fs = 0, fmin = front[0], fl = 0, ps = 0;
  for (int i = 0; i < n; i++) {{
    fs += front[i]; fl += floor_[i]; ps += pitch[i];
    if (front[i] < fmin) fmin = front[i];
  }}
  float fmean = fs / n, flmean = fl / n;
  float fvar = 0, flvar = 0;
  for (int i = 0; i < n; i++) {{
    fvar += (front[i] - fmean) * (front[i] - fmean);
    flvar += (floor_[i] - flmean) * (floor_[i] - flmean);
  }}
  out[0] = fmean;
  out[1] = fmin;
  out[2] = sqrtf(fvar / n);
  out[3] = front[n - 1] - front[0];
  out[4] = flmean;
  out[5] = sqrtf(flvar / n);
  out[6] = fabsf(ps / n);
}}

// Retorna a classe e preenche probs[PL_N_CLASSES]
static inline int pl_predict(const float* feat, float* probs) {{
  float x[PL_N_FEATURES], h[PL_N_HIDDEN];
  for (int i = 0; i < PL_N_FEATURES; i++) x[i] = (feat[i] - PL_FEAT_MEAN[i]) / PL_FEAT_STD[i];
  for (int j = 0; j < PL_N_HIDDEN; j++) {{
    float a = PL_B1[j];
    for (int i = 0; i < PL_N_FEATURES; i++) a += PL_W1[j * PL_N_FEATURES + i] * x[i];
    h[j] = a > 0 ? a : 0;
  }}
  float mx = -1e30f;
  for (int k = 0; k < PL_N_CLASSES; k++) {{
    float a = PL_B2[k];
    for (int j = 0; j < PL_N_HIDDEN; j++) a += PL_W2[k * PL_N_HIDDEN + j] * h[j];
    probs[k] = a;
    if (a > mx) mx = a;
  }}
  float sum = 0; int best = 0;
  for (int k = 0; k < PL_N_CLASSES; k++) {{ probs[k] = expf(probs[k] - mx); sum += probs[k]; }}
  for (int k = 0; k < PL_N_CLASSES; k++) {{ probs[k] /= sum; if (probs[k] > probs[best]) best = k; }}
  return best;
}}
"""
    path.write_text(header, encoding="utf-8")


def scenario_checks(model, mu, sigma):
    """Cenários 'de slider' do Wokwi: valores constantes, sem ruído."""
    flat = lambda v: np.full(WINDOW, float(v))
    cases = {
        "nada à frente (400 cm), piso 10, plano": (flat(400), flat(10), flat(0), "LIVRE"),
        "parede a 150 cm": (flat(150), flat(10), flat(0), "LIVRE"),
        "carro a 30 cm": (flat(30), flat(10), flat(0), "OBSTACULO"),
        "pessoa vindo 250->60 cm": (np.linspace(250, 60, WINDOW), flat(10), flat(0), "PESSOA"),
        "slider saltando 400->30 (algo surge)": (np.r_[np.full(5, 400.0), np.full(5, 30.0)], flat(10), flat(0), "PESSOA"),
        "obstáculo saindo 30->400": (np.r_[np.full(5, 30.0), np.full(5, 400.0)], flat(10), flat(0), "LIVRE"),
        "buraco: piso a 35 cm": (flat(400), flat(35), flat(0), "BURACO"),
        "degrau: piso a 4 cm": (flat(400), flat(4), flat(0), "DEGRAU"),
        "rampa 9 graus (~16 %)": (flat(400), flat(10), flat(9), "RAMPA"),
        "rampa 3 graus (dentro da NBR)": (flat(400), flat(10), flat(3), "LIVRE"),
    }
    ok = True
    print("\nCenários do Wokwi:")
    for desc, (f, fl, p, exp) in cases.items():
        x = (extract_features(f, fl, p) - mu) / sigma
        pr = model.predict_proba([x])[0]
        got = CLASSES[int(pr.argmax())]
        flag = "OK " if got == exp else "ERRO"
        ok &= got == exp
        print(f"  [{flag}] {desc:38s} -> {got:9s} ({pr.max():.2f})  esperado {exp}")
    return ok


def main():
    X, y, raw = build_dataset()
    Xtr, Xte, ytr, yte, _, raw_te = train_test_split(
        X, y, raw, test_size=0.2, stratify=y, random_state=42)

    mu, sigma = Xtr.mean(axis=0), Xtr.std(axis=0) + 1e-6
    model = MLPClassifier(hidden_layer_sizes=(16,), activation="relu",
                          max_iter=800, random_state=42)
    model.fit((Xtr - mu) / sigma, ytr)

    pred = model.predict((Xte - mu) / sigma)
    rep = classification_report(yte, pred, target_names=CLASSES, digits=4, output_dict=True)
    print(classification_report(yte, pred, target_names=CLASSES, digits=4))
    cm = confusion_matrix(yte, pred)
    print("Matriz de confusão (linhas = real):")
    print("            " + " ".join(f"{c[:6]:>7s}" for c in CLASSES))
    for c, row in zip(CLASSES, cm):
        print(f"  {c:9s} " + " ".join(f"{v:7d}" for v in row))

    scen_ok = scenario_checks(model, mu, sigma)

    export_header(model, mu, sigma, ROOT / "firmware" / "passo_livre_model.h", rep)

    # paridade Python x C: 300 janelas do teste com as probabilidades do sklearn
    probs = model.predict_proba((Xte[:300] - mu) / sigma)
    rows = np.hstack([raw_te[:300], probs])
    np.savetxt(ROOT / "tests" / "parity_windows.csv", rows, delimiter=",", fmt="%.6f")

    (ROOT / "ml" / "metrics.json").write_text(json.dumps({
        "accuracy": rep["accuracy"],
        "per_class_f1": {c: rep[c]["f1-score"] for c in CLASSES},
        "confusion_matrix": cm.tolist(),
        "classes": CLASSES,
        "features": FEATURES,
        "scenario_checks_passed": bool(scen_ok),
        "n_windows": int(len(X)),
        "nbr9050_ramp_limit_deg": round(float(NBR9050_RAMP_DEG), 2),
    }, indent=2, ensure_ascii=False), encoding="utf-8")
    print("\nHeader exportado em firmware/passo_livre_model.h")


if __name__ == "__main__":
    main()
