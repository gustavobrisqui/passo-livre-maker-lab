# Passo Livre · Maker Lab — Robô de vistoria de passagens acessíveis

> **FIAP Global Solution 2026.2 · CitizenTech · Engenharia de Software 4º ano**
> Disciplina: Project-Based Maker Lab — *Evolução de um robô inteligente para CitizenTech*
> Componente do MVP: **Físico**

**Integrantes**

| Nome | RM |
|---|---|
| Gustavo Brisqui Martinez | 97969 |
| Matheus Brisqui Martinez | 97892 |
| Guilherme Resende Bezerra | 98508 |

📹 **Vídeo demonstrativo (até 6 min):** _link do YouTube_
🔌 **Simulação no Wokwi:** <https://wokwi.com/projects/481129630702885889>
🗺️ **Mapa ao vivo:** `mapa/index.html` (abrir no navegador ou GitHub Pages)

---

## 1. Problema

Calçadas de São Paulo têm carros estacionados, entulho, buracos, guias sem rebaixo e rampas mais íngremes que o permitido. Para quem usa cadeira de rodas, bengala ou andador, qualquer uma dessas barreiras interrompe o trajeto. Hoje o mapeamento depende de alguém parar, fotografar e abrir um chamado no SP156, e a maioria das barreiras nunca é registrada.

O **Passo Livre** é um mapa colaborativo de obstáculos em calçadas e pontos de ônibus. Este repositório é a peça **física** do produto: um robô que percorre uma passagem, **detecta e classifica as barreiras sozinho** e as envia para o mesmo mapa que recebe relatos de cidadãos e da visão computacional.

## 2. O que o robô faz

```
 HC-SR04 frente ─┐                ┌─ LIVRE      → segue patrulhando
 HC-SR04 piso  ──┼─ janela 1 s ─► │  PESSOA     → para, aviso sonoro curto, dá passagem (não registra)
 MPU6050       ─┘  (10 leituras)  │  OBSTACULO  → para; se continuar lá por 2 s, RELATA e desvia
                   7 features     │  BURACO     → para na hora, RELATA, dá ré e desvia
                   MLP TinyML     │  DEGRAU     → para na hora, RELATA, dá ré e desvia
                                  └─ RAMPA      → reduz velocidade e RELATA (inclinação > 8,33 %)
                                          │
                              relato JSON georreferenciado
                                          │ MQTT
                         ┌────────────────┴────────────────┐
                    mapa/index.html                  bridge/bridge.py
                  (pinos ao vivo)            → POST /api/v1/relatos do app
                                              → RPA deduplica, prioriza e abre chamado
```

### Evolução em relação ao robô anterior

| | Robô anterior (CP1/CP2) | Robô Passo Livre (GS) |
|---|---|---|
| Sensores | ultrassônico frontal | **+ ultrassônico de piso** (desnível) **+ MPU6050** (inclinação) |
| IA | — | **MLP TinyML embarcada** com 6 classes, roda no ESP32 sem internet |
| Decisão | comando manual / regra fixa | máquina de estados autônoma + **reflexo de segurança** independente da IA |
| Saída | local | **relato georreferenciado** no mapa do Passo Livre via MQTT, com fila offline |

## 3. Componentes

| Componente | Função | Pinos ESP32 |
|---|---|---|
| ESP32 DevKit v1 | processamento, IA, Wi-Fi | — |
| HC-SR04 frontal | obstáculo e aproximação de pessoa | TRIG 5 · ECHO 18 |
| HC-SR04 de piso **(sensor adicional)** | buraco / degrau sem rampa (montado a 10 cm do chão, apontado para baixo e à frente) | TRIG 19 · ECHO 23 |
| MPU6050 **(sensor adicional)** | inclinação da rampa (NBR 9050: máx. 8,33 %) | SDA 21 · SCL 22 |
| OLED SSD1306 128×64 | estado, classe da IA, medidas, posição | SDA 21 · SCL 22 (0x3C) |
| LEDs verde / amarelo / vermelho | andando / atenção / barreira | 4 · 15 · 13 |
| Buzzer | alerta sonoro | 25 |
| Ponte H L298N + 2 motores DC | tração (carrinho físico) | 26, 27, 32, 33 |

> **No carrinho físico:** o ECHO do HC-SR04 sai em 5 V. Use divisor resistivo (1 kΩ / 2 kΩ) antes do GPIO do ESP32.
> **No Wokwi:** não existe motor DC; o deslocamento é simulado por odometria (0,6 m/s) ao longo de um trecho de ~268 m da calçada da Av. Paulista, e os pinos do L298N continuam sendo acionados.

## 4. Integração sensores × IA

### 4.1 Features (iguais no Python e no C)

A cada 100 ms o robô lê os três sensores e mantém uma janela deslizante de 10 amostras (1 s). Dela saem 7 features:

| # | Feature | Por quê |
|---|---|---|
| 0 | média da distância frontal | quão perto está o que há na frente |
| 1 | mínimo da distância frontal | robustez a ecos perdidos |
| 2 | desvio-padrão frontal | objeto em movimento varia mais |
| 3 | variação frontal na janela (último − primeiro) | **separa pessoa se aproximando (cai 50–200 cm/s) de obstáculo parado** |
| 4 | média da distância ao piso | buraco (> 16 cm) ou degrau (< 6 cm) |
| 5 | desvio-padrão do piso | borda de desnível dentro da janela |
| 6 | \|inclinação média\| | rampa acima de 4,76° (= 8,33 %) |

### 4.2 Modelo

- **MLP 7 → 16 (ReLU) → 6 (softmax)**, 230 parâmetros (< 1 KB), inferência em microssegundos no ESP32.
- Treinado com `ml/train_model.py` (scikit-learn) e exportado para `firmware/passo_livre_model.h`, que contém os pesos **e** as funções `pl_extract_features()` e `pl_predict()` em C puro.
- **Por que MLP própria e não Edge Impulse:** o modelo precisa rodar dentro do Wokwi para o avaliador reproduzir. Um header C sem dependências compila em qualquer ESP32, no Wokwi ou no carrinho, e o teste de paridade garante que o C dá o mesmo resultado que o Python.

### 4.3 Dataset

Dataset **simulado**, como a GS permite quando a geração é justificada. São 18.000 janelas (3.000 por classe) geradas a partir da física do problema; os detalhes estão em [`docs/dataset.md`](docs/dataset.md).

### 4.4 Da classe à ação: IA classifica, regra protege

1. **Reflexo de segurança (sem IA):** se o piso some (> 18 cm), aparece degrau (< 5 cm) ou há algo a menos de 12 cm, o robô para **antes** de a IA confirmar. Um robô que cai num buraco não serve para mapear buracos.
2. **Filtro temporal:** a classe só muda quando aparece em **3 janelas seguidas com confiança ≥ 70 %**, o que evita relato por ruído.
3. **Distinção pedestre × barreira:** algo à frente que **continua lá por 2 s** é barreira (carro, entulho); quem se aproxima e sai é pedestre. **Pessoas nunca viram relato** (LGPD: o robô não registra presença de pessoas).
4. **Deduplicação local:** o mesmo tipo no raio de 5 m em 2 min não é reenviado. A deduplicação entre fontes (cidadão, CV, robô) é feita pelo bot de RPA.
5. **Pessoa no circuito:** todo relato do robô chega como `pendente_validacao`. Moderação/RPA confirma antes de abrir chamado na prefeitura.

## 5. Contrato de integração com o Passo Livre

**Broker:** `broker.hivemq.com:1883` (TCP, firmware) · `wss://broker.hivemq.com:8884/mqtt` (navegador)

| Tópico | Conteúdo |
|---|---|
| `passolivre/gs2026-4es/relatos` | uma barreira confirmada |
| `passolivre/gs2026-4es/telemetria/robo-01` | posição e estado a cada 2 s |

**Relato (`passolivre.relato.v1`)**

```json
{
  "schema": "passolivre.relato.v1",
  "relato_id": "robo-01-1795099213-0003",
  "device_id": "robo-01",
  "origem": "robo_ultrassonico",
  "tipo": "BURACO",
  "confianca": 0.98,
  "lat": -23.561712, "lon": -46.655576,
  "posicao_m": 73.4,
  "medidas": { "frente_cm": 400.0, "piso_cm": 35.0, "inclinacao_pct": 0.0 },
  "status": "pendente_validacao",
  "ts": "2026-11-19T18:42:10Z",
  "uptime_ms": 128450
}
```

`tipo` ∈ `OBSTACULO_CALCADA` · `BURACO` · `DEGRAU_SEM_RAMPA` · `RAMPA_INCLINACAO_EXCESSIVA`. O ultrassom não distingue carro de entulho, então `OBSTACULO_CALCADA` é genérico; a foto do cidadão e o YOLO refinam a categoria no app.

**Resiliência:** sem Wi-Fi ou broker, os relatos ficam numa fila de 20 itens e são enviados quando a conexão volta. `relato_id` é chave de idempotência, então reenvios não duplicam.

**Ponte para a API do app** (`bridge/bridge.py`): assina `relatos`, valida (tipo, faixa de lat/lon de SP, confiança) e faz `POST` em `PASSO_LIVRE_API_URL` com `X-Device-Key` e `Idempotency-Key`. Credenciais apenas por variável de ambiente. Sem `PASSO_LIVRE_API_URL`, roda em `DRY_RUN` e só imprime.

## 6. Como executar

### 6.1 Simulação no Wokwi (recomendado para avaliação)

1. Abra <https://wokwi.com/projects/481129630702885889> **ou** crie um projeto ESP32 em <https://wokwi.com/projects/new/esp32> e copie `firmware/sketch.ino`, `firmware/diagram.json` e `firmware/passo_livre_model.h` (menu ▾ → *New file*). Em *Library Manager*, adicione `PubSubClient`, `Adafruit SSD1306` e `Adafruit GFX Library`.
2. Clique em ▶. O Wi-Fi `Wokwi-GUEST` já tem internet.
3. Abra `mapa/index.html` em outra aba: o ponto verde é o robô andando no trecho.
4. Clique nos sensores durante a simulação e mova os sliders (ver §7).

### 6.2 Retreinar o modelo

```bash
pip install scikit-learn numpy
python ml/train_model.py                    # gera firmware/passo_livre_model.h e ml/metrics.json
gcc -O2 -I firmware tests/parity_test.c -lm -o parity && ./parity tests/parity_windows.csv
gcc -O2 -I firmware tests/transitions_test.c -lm -o transitions && ./transitions
```

### 6.3 Ponte para a API

```bash
pip install -r bridge/requirements.txt
export PASSO_LIVRE_API_URL=https://<app>/api/v1/relatos PASSO_LIVRE_API_KEY=<chave>
python bridge/bridge.py
```

### 6.4 Carrinho físico

Mesmo `sketch.ino` na Arduino IDE (placa *ESP32 Dev Module*), trocando `WIFI_SSID`/`WIFI_PASS`. Monte o sensor de piso a 10 cm do chão; se a altura for outra, ajuste `FLOOR_NOMINAL` em `ml/train_model.py` e retreine.

## 7. Testes e demonstração

| # | Cenário | Como simular no Wokwi | Esperado |
|---|---|---|---|
| 1 | Calçada livre | frente 400, piso 10, MPU plano | LED verde, `PATRULHANDO`, posição avançando |
| 2 | Pedestre vindo | arraste o slider frontal de 300 → 60 cm em ~1 s | `PESSOA`, LED amarelo, bip curto, **nenhum relato** |
| 3 | Carro na calçada | frontal em 30 cm e solte | primeiro `PESSOA` (algo surgiu), depois `OBSTACULO`; após 2 s parado, relato `OBSTACULO_CALCADA` e `DESVIANDO` |
| 3b | Obstáculo sai | frontal de volta a 400 cm | volta a `LIVRE` e `PATRULHANDO`, sem alerta |
| 4 | Buraco | piso em 35 cm | parada imediata (reflexo), relato `BURACO`, ré e desvio |
| 5 | Guia sem rebaixo | piso em 4 cm | relato `DEGRAU_SEM_RAMPA` |
| 6 | Rampa irregular | MPU accelX = 0,15 g (~8,5°) | `DEVAGAR`, relato `RAMPA_INCLINACAO_EXCESSIVA` com inclinação em % |
| 7 | Rampa dentro da norma | MPU accelX = 0,05 g (~3°) | continua `LIVRE` |
| 8 | Falha de rede | pare/reinicie a simulação sem internet | relatos ficam na fila (`Rel:x/fila` no OLED) e saem depois |
| 9 | Eco perdido | sensor fora de alcance | leitura = 400 cm, não dispara obstáculo falso |

Testes automatizados:

- `ml/train_model.py`: 10 cenários de slider do Wokwi.
- `tests/parity_test.c`: 300 janelas comparadas entre C e Python (0 divergências, erro máx. 1e-6).
- `tests/transitions_test.c`: janela deslizante + filtro temporal sobre sequências reais de leitura (carro surge e fica → PESSOA → OBSTACULO; obstáculo sai → LIVRE; pedestre passa → PESSOA → LIVRE).

Validado no Wokwi em 19/11/2026: Wi-Fi e MQTT conectados, relatos de BURACO, OBSTACULO_CALCADA e RAMPA_INCLINACAO_EXCESSIVA (15 %) recebidos no mapa ao vivo; rampa de 5 % mantida como LIVRE.

## 8. Limitações conhecidas

- A acurácia de 99,5 % é sobre dados **simulados** e mede só a separabilidade das classes como foram geradas. Em campo o desempenho será menor; o próximo passo é coletar janelas reais com o carrinho e retreinar.
- O ultrassom tem cone de ~15°: obstáculos estreitos (poste fino) podem passar despercebidos.
- A posição é odometria simulada sobre um trecho fixo. No carrinho real entraria GPS ou o celular do operador.
- Broker público é para demonstração; em produção, broker com TLS e usuário por dispositivo.

## 9. Estrutura

```
firmware/   sketch.ino · diagram.json · libraries.txt · passo_livre_model.h (gerado)
ml/         train_model.py · metrics.json
tests/      parity_test.c · parity_windows.csv · transitions_test.c
mapa/       index.html (Leaflet + MQTT over WebSocket)
bridge/     bridge.py · requirements.txt
docs/       dataset.md · roteiro-video.md
```
