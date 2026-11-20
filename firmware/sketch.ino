/*
 * ===================================================================
 *  PASSO LIVRE - Robô de vistoria de passagens acessíveis (Maker Lab)
 *  FIAP Global Solution 2026.2 - CitizenTech - Engenharia de Software 4º ano
 * ===================================================================
 *  O robô percorre um trecho de calçada, lê três sensores e um modelo
 *  TinyML embarcado (passo_livre_model.h) classifica cada janela de 1 s:
 *
 *     LIVRE | OBSTACULO | PESSOA | BURACO | DEGRAU | RAMPA
 *
 *  Barreiras confirmadas viram RELATOS georreferenciados, publicados via
 *  MQTT e exibidos no mapa colaborativo do Passo Livre.
 *
 *  Sensores
 *    - HC-SR04 frontal  : obstáculo/pessoa à frente
 *    - HC-SR04 de piso  : (SENSOR ADICIONAL) desnível - buraco ou degrau sem rampa
 *    - MPU6050          : (SENSOR ADICIONAL) inclinação - rampa acima de 8,33 % (NBR 9050)
 *  Atuadores: motores (pinos L298N), LEDs de estado, buzzer, OLED.
 * ===================================================================
 */
#include <WiFi.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>
#include <time.h>
#include "passo_livre_model.h"

// ------------------------------------------------------------ configuração
#define DEVICE_ID     "robo-01"
#define WIFI_SSID     "Wokwi-GUEST"      // no carrinho real: rede do campus
#define WIFI_PASS     ""
#define MQTT_HOST     "broker.hivemq.com"
#define MQTT_PORT     1883
#define TOPIC_BASE    "passolivre/gs2026-4es"   // troque pelo ID da equipe
// Tópicos: <base>/relatos  (barreiras)   <base>/telemetria/<device> (posição/estado)

// ------------------------------------------------------------ pinos
#define PIN_FRONT_TRIG 5
#define PIN_FRONT_ECHO 18
#define PIN_FLOOR_TRIG 19
#define PIN_FLOOR_ECHO 23
#define PIN_LED_GREEN  4
#define PIN_LED_YELLOW 15
#define PIN_LED_RED    13
#define PIN_BUZZER     25
// L298N (carrinho físico). No Wokwi o movimento é simulado pela odometria.
#define PIN_MOTOR_L_FWD 26
#define PIN_MOTOR_L_REV 27
#define PIN_MOTOR_R_FWD 32
#define PIN_MOTOR_R_REV 33

#define MPU_ADDR 0x68
#define OLED_ADDR 0x3C

// ------------------------------------------------------------ parâmetros
const uint32_t SAMPLE_MS        = 100;    // 10 Hz -> janela de 1 s
const float    MAX_CM           = 400.0;
const float    SPEED_MPS        = 0.6;    // velocidade simulada de patrulha
const float    SPEED_SLOW_MPS   = 0.25;   // em rampa
const float    MIN_CONFIDENCE   = 0.70;
const int      CONFIRM_WINDOWS  = 3;      // classe precisa se repetir 3x seguidas (~300 ms)
const float    SAFETY_FLOOR_HI  = 18.0;   // reflexo: piso sumiu -> para JÁ, sem esperar a IA
const float    SAFETY_FLOOR_LO  = 5.0;    // reflexo: degrau à frente
const float    SAFETY_FRONT_CM  = 12.0;   // reflexo: colisão iminente
const uint32_t OBSTACLE_REPORT_MS = 2000; // obstáculo parado 2 s = barreira, não pedestre
const float    DEDUP_RADIUS_M   = 5.0;    // não repete o mesmo relato no mesmo lugar
const int      QUEUE_SIZE       = 20;     // relatos guardados se a rede cair

// Trecho simulado: calçada norte da Av. Paulista, entre a Al. Casa Branca e a R. Itapeva (~268 m)
const double ROUTE_A_LAT = -23.561224, ROUTE_A_LON = -46.656248;
const double ROUTE_B_LAT = -23.562969, ROUTE_B_LON = -46.654435;
const float  ROUTE_LEN_M = 268.0;

// ------------------------------------------------------------ estado
Adafruit_SSD1306 oled(128, 64, &Wire, -1);
WiFiClient net;
PubSubClient mqtt(net);

float bufFront[PL_WINDOW], bufFloor[PL_WINDOW], bufPitch[PL_WINDOW];
int   bufCount = 0;
float lastFront = MAX_CM, lastFloor = 10, lastPitch = 0;

enum RobotState { PATRULHANDO, DEVAGAR, AGUARDANDO_PESSOA, PARADO_BARREIRA, RECUANDO, DESVIANDO };
const char* STATE_NAMES[] = { "PATRULHANDO", "DEVAGAR", "AGUARD.PESSOA", "BARREIRA", "RECUANDO", "DESVIANDO" };
RobotState state = PATRULHANDO;
uint32_t stateSince = 0;

int   rawClass = PL_LIVRE, candClass = PL_LIVRE, candCount = 0;
int   confirmedClass = PL_LIVRE;
float confirmedConf = 1.0;
float probs[PL_N_CLASSES];

float posM = 0;      // posição no trecho (odometria)
int   direction = 1; // 1 = A->B, -1 = B->A
bool  safetyStop = false;
bool  obstacleReported = false;

struct Report { int cls; float posM; uint32_t at; };
Report lastReports[8]; int lastReportIdx = 0;
String queue[QUEUE_SIZE]; int qHead = 0, qCount = 0;
uint32_t reportSeq = 0, reportsSent = 0, bootId = 0;

// ------------------------------------------------------------ sensores
float readUltrasonic(int trig, int echo) {
  digitalWrite(trig, LOW);  delayMicroseconds(2);
  digitalWrite(trig, HIGH); delayMicroseconds(10);
  digitalWrite(trig, LOW);
  unsigned long us = pulseIn(echo, HIGH, 25000UL);     // 25 ms ~ 4 m
  if (us == 0) return MAX_CM;                          // eco perdido = nada ao alcance
  float cm = us * 0.0343f / 2.0f;
  return constrain(cm, 2.0f, MAX_CM);
}

void mpuInit() {
  Wire.beginTransmission(MPU_ADDR); Wire.write(0x6B); Wire.write(0); Wire.endTransmission(); // acorda
}

float readPitchDeg() {
  Wire.beginTransmission(MPU_ADDR); Wire.write(0x3B); Wire.endTransmission(false);
  if (Wire.requestFrom(MPU_ADDR, 6) != 6) return lastPitch;
  int16_t ax = (Wire.read() << 8) | Wire.read();
  int16_t ay = (Wire.read() << 8) | Wire.read();
  int16_t az = (Wire.read() << 8) | Wire.read();
  // eixo X apontado para a frente do robô
  return atan2f((float)ax, sqrtf((float)ay * ay + (float)az * az)) * 180.0f / PI;
}

void pushSample(float f, float fl, float p) {
  if (bufCount < PL_WINDOW) {
    bufFront[bufCount] = f; bufFloor[bufCount] = fl; bufPitch[bufCount] = p; bufCount++;
    return;
  }
  for (int i = 1; i < PL_WINDOW; i++) {
    bufFront[i - 1] = bufFront[i]; bufFloor[i - 1] = bufFloor[i]; bufPitch[i - 1] = bufPitch[i];
  }
  bufFront[PL_WINDOW - 1] = f; bufFloor[PL_WINDOW - 1] = fl; bufPitch[PL_WINDOW - 1] = p;
}

// ------------------------------------------------------------ atuadores
void motors(int left, int right) {   // -1 ré, 0 parado, 1 frente
  digitalWrite(PIN_MOTOR_L_FWD, left > 0);  digitalWrite(PIN_MOTOR_L_REV, left < 0);
  digitalWrite(PIN_MOTOR_R_FWD, right > 0); digitalWrite(PIN_MOTOR_R_REV, right < 0);
}

void leds(bool g, bool y, bool r) {
  digitalWrite(PIN_LED_GREEN, g); digitalWrite(PIN_LED_YELLOW, y); digitalWrite(PIN_LED_RED, r);
}

void beep(int freq, int ms) { tone(PIN_BUZZER, freq, ms); }

void setState(RobotState s) {
  if (s == state) return;
  state = s; stateSince = millis();
  Serial.printf("[estado] -> %s\n", STATE_NAMES[s]);
  switch (s) {
    case PATRULHANDO:       leds(1, 0, 0); break;
    case DEVAGAR:           leds(1, 1, 0); beep(1200, 120); break;
    case AGUARDANDO_PESSOA: leds(0, 1, 0); beep(1800, 80); break;   // aviso curto e gentil
    case PARADO_BARREIRA:   leds(0, 0, 1); beep(600, 600); break;   // alerta longo
    case RECUANDO:          leds(0, 1, 1); break;
    case DESVIANDO:         leds(0, 1, 1); break;
  }
}

// ------------------------------------------------------------ posição
void currentLatLon(double& lat, double& lon) {
  double t = posM / ROUTE_LEN_M;
  lat = ROUTE_A_LAT + (ROUTE_B_LAT - ROUTE_A_LAT) * t;
  lon = ROUTE_A_LON + (ROUTE_B_LON - ROUTE_A_LON) * t;
}

void advance(float mps, float dt) {
  posM += direction * mps * dt;
  if (posM >= ROUTE_LEN_M) { posM = ROUTE_LEN_M; direction = -1; }
  if (posM <= 0)           { posM = 0;           direction = 1;  }
}

// ------------------------------------------------------------ relatos
const char* reportType(int cls) {
  switch (cls) {
    case PL_OBSTACULO: return "OBSTACULO_CALCADA";
    case PL_BURACO:    return "BURACO";
    case PL_DEGRAU:    return "DEGRAU_SEM_RAMPA";
    case PL_RAMPA:     return "RAMPA_INCLINACAO_EXCESSIVA";
    default:           return nullptr;   // LIVRE e PESSOA não viram relato (LGPD: robô não registra pessoas)
  }
}

bool isDuplicate(int cls) {
  for (auto& r : lastReports)
    if (r.at && r.cls == cls && fabsf(r.posM - posM) < DEDUP_RADIUS_M && millis() - r.at < 120000) return true;
  return false;
}

void isoNow(char* out, size_t n) {
  struct tm t;
  if (getLocalTime(&t, 10)) strftime(out, n, "%Y-%m-%dT%H:%M:%SZ", &t);
  else if (n) out[0] = '\0';                          // sem NTP: backend usa a hora de chegada
}

void enqueue(const String& payload) {
  int idx = (qHead + qCount) % QUEUE_SIZE;
  if (qCount == QUEUE_SIZE) { qHead = (qHead + 1) % QUEUE_SIZE; qCount--; } // descarta o mais antigo
  queue[idx] = payload; qCount++;
}

void flushQueue() {
  while (qCount > 0 && mqtt.connected()) {
    if (!mqtt.publish(TOPIC_BASE "/relatos", queue[qHead].c_str())) break;
    reportsSent++;
    qHead = (qHead + 1) % QUEUE_SIZE; qCount--;
  }
}

void report(int cls, float conf) {
  const char* type = reportType(cls);
  if (!type || isDuplicate(cls)) return;

  double lat, lon; currentLatLon(lat, lon);
  char ts[25]; isoNow(ts, sizeof(ts));
  float slopePct = tanf(fabsf(lastPitch) * PI / 180.0f) * 100.0f;
  // relato_id único entre reinícios: hora UTC (NTP) ou, sem NTP, um número aleatório sorteado no boot
  time_t epoch = time(nullptr);
  char id[40]; snprintf(id, sizeof(id), "%s-%lu-%04lu", DEVICE_ID,
                        (unsigned long)(epoch > 1700000000 ? (uint32_t)epoch : bootId), (unsigned long)++reportSeq);

  char buf[512];
  snprintf(buf, sizeof(buf),
    "{\"schema\":\"passolivre.relato.v1\",\"relato_id\":\"%s\",\"device_id\":\"%s\","
    "\"origem\":\"robo_ultrassonico\",\"tipo\":\"%s\",\"confianca\":%.2f,"
    "\"lat\":%.6f,\"lon\":%.6f,\"posicao_m\":%.1f,"
    "\"medidas\":{\"frente_cm\":%.1f,\"piso_cm\":%.1f,\"inclinacao_pct\":%.1f},"
    "\"status\":\"pendente_validacao\",\"ts\":\"%s\",\"uptime_ms\":%lu}",
    id, DEVICE_ID, type, conf, lat, lon, posM,
    lastFront, lastFloor, slopePct, ts, (unsigned long)millis());

  lastReports[lastReportIdx] = { cls, posM, millis() };
  lastReportIdx = (lastReportIdx + 1) % 8;

  Serial.printf("[relato] %s\n", buf);
  enqueue(String(buf));
  flushQueue();
}

void publishTelemetry() {
  if (!mqtt.connected()) return;
  double lat, lon; currentLatLon(lat, lon);
  char buf[320];
  snprintf(buf, sizeof(buf),
    "{\"device_id\":\"%s\",\"lat\":%.6f,\"lon\":%.6f,\"posicao_m\":%.1f,\"estado\":\"%s\","
    "\"classe\":\"%s\",\"confianca\":%.2f,\"frente_cm\":%.1f,\"piso_cm\":%.1f,\"inclinacao_graus\":%.1f,"
    "\"fila\":%d,\"enviados\":%lu}",
    DEVICE_ID, lat, lon, posM, STATE_NAMES[state], PL_CLASS_NAMES[confirmedClass], confirmedConf,
    lastFront, lastFloor, lastPitch, qCount, (unsigned long)reportsSent);
  mqtt.publish(TOPIC_BASE "/telemetria/" DEVICE_ID, buf);
}

// ------------------------------------------------------------ rede (não bloqueante)
void networkTick() {
  static uint32_t lastTry = 0;
  static bool wifiLogged = false;
  if (WiFi.status() != WL_CONNECTED) { wifiLogged = false; return; }  // WiFi.begin reconecta sozinho
  if (!wifiLogged) { wifiLogged = true; Serial.printf("[wifi] conectado, IP %s\n", WiFi.localIP().toString().c_str()); }
  if (mqtt.connected()) { mqtt.loop(); flushQueue(); return; }
  if (millis() - lastTry < 3000) return;
  lastTry = millis();
  String cid = String("passolivre-") + DEVICE_ID + "-" + String((uint32_t)esp_random(), HEX);
  if (mqtt.connect(cid.c_str())) Serial.println("[mqtt] conectado");
  else Serial.printf("[mqtt] falhou (rc=%d), tentando de novo\n", mqtt.state());
}

// ------------------------------------------------------------ IA + decisão
void classify() {
  float feat[PL_N_FEATURES];
  pl_extract_features(bufFront, bufFloor, bufPitch, PL_WINDOW, feat);
  rawClass = pl_predict(feat, probs);

  // filtro temporal: só muda de classe com confiança e repetição
  if (probs[rawClass] >= MIN_CONFIDENCE && rawClass == candClass) candCount++;
  else { candClass = rawClass; candCount = (probs[rawClass] >= MIN_CONFIDENCE) ? 1 : 0; }

  if (candCount >= CONFIRM_WINDOWS && candClass != confirmedClass) {
    confirmedClass = candClass;
    confirmedConf = probs[candClass];
    obstacleReported = false;
    Serial.printf("[ia] %s (%.0f%%)  feat=[%.0f %.0f %.1f %.0f %.1f %.2f %.1f]\n",
                  PL_CLASS_NAMES[confirmedClass], confirmedConf * 100,
                  feat[0], feat[1], feat[2], feat[3], feat[4], feat[5], feat[6]);
  } else if (candClass == confirmedClass) {
    confirmedConf = probs[candClass];
  }
}

void decide(float dt) {
  // 1) Reflexo de segurança: independe da IA (a IA classifica, a regra protege)
  safetyStop = (lastFloor > SAFETY_FLOOR_HI || lastFloor < SAFETY_FLOOR_LO || lastFront < SAFETY_FRONT_CM);

  uint32_t inState = millis() - stateSince;

  switch (state) {
    case RECUANDO:
      motors(-1, -1); advance(SPEED_SLOW_MPS, -dt);  // dá ré
      if (inState > 1000) setState(DESVIANDO);
      return;
    case DESVIANDO:                                  // gira para contornar
      motors(1, -1);
      if (inState > 1200 && !safetyStop && confirmedClass != PL_OBSTACULO
          && confirmedClass != PL_BURACO && confirmedClass != PL_DEGRAU) setState(PATRULHANDO);
      return;
    default: break;
  }

  if (safetyStop && state != PARADO_BARREIRA) { motors(0, 0); setState(PARADO_BARREIRA); }

  switch (confirmedClass) {
    case PL_LIVRE:
      if (!safetyStop) { setState(PATRULHANDO); motors(1, 1); advance(SPEED_MPS, dt); }
      break;

    case PL_RAMPA:  // continua, devagar, e registra a rampa fora da norma
      if (!safetyStop) { setState(DEVAGAR); motors(1, 1); advance(SPEED_SLOW_MPS, dt); }
      report(PL_RAMPA, confirmedConf);
      break;

    case PL_PESSOA: // dá passagem, avisa, não registra nada
      motors(0, 0); setState(AGUARDANDO_PESSOA);
      break;

    case PL_OBSTACULO:
      motors(0, 0); setState(PARADO_BARREIRA);
      // pedestre para e sai; carro/entulho fica. Só relata se continuar lá.
      if (!obstacleReported && millis() - stateSince > OBSTACLE_REPORT_MS) {
        report(PL_OBSTACULO, confirmedConf); obstacleReported = true; setState(DESVIANDO);
      }
      break;

    case PL_BURACO:
    case PL_DEGRAU:
      motors(0, 0);
      if (!obstacleReported) {
        setState(PARADO_BARREIRA);
        report(confirmedClass, confirmedConf); obstacleReported = true;
      } else if (millis() - stateSince > 800) {
        setState(RECUANDO);
      }
      break;
  }
}

// ------------------------------------------------------------ display
void drawOled() {
  double lat, lon; currentLatLon(lat, lon);
  oled.clearDisplay();
  oled.setTextSize(1); oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);
  oled.printf("PASSO LIVRE %s %s\n", WiFi.status() == WL_CONNECTED ? "W" : "-", mqtt.connected() ? "M" : "-");
  oled.printf("%s\n", STATE_NAMES[state]);
  oled.printf("IA:%s %d%%\n", PL_CLASS_NAMES[confirmedClass], (int)(confirmedConf * 100));
  oled.printf("Fr:%3.0f Pi:%4.1f cm\n", lastFront, lastFloor);
  oled.printf("Incl:%4.1f%% %s\n", tanf(fabsf(lastPitch) * PI / 180) * 100, safetyStop ? "STOP" : "");
  oled.printf("Pos:%5.1fm Rel:%lu/%d\n", posM, (unsigned long)reportsSent, qCount);
  int w = (int)(posM / ROUTE_LEN_M * 127);           // barra de progresso no trecho
  oled.drawRect(0, 56, 128, 8, SSD1306_WHITE);
  oled.fillRect(0, 56, w, 8, SSD1306_WHITE);
  oled.display();
}

// ------------------------------------------------------------ setup/loop
void setup() {
  Serial.begin(115200);
  pinMode(PIN_FRONT_TRIG, OUTPUT); pinMode(PIN_FRONT_ECHO, INPUT);
  pinMode(PIN_FLOOR_TRIG, OUTPUT); pinMode(PIN_FLOOR_ECHO, INPUT);
  int outs[] = { PIN_LED_GREEN, PIN_LED_YELLOW, PIN_LED_RED, PIN_BUZZER,
                 PIN_MOTOR_L_FWD, PIN_MOTOR_L_REV, PIN_MOTOR_R_FWD, PIN_MOTOR_R_REV };
  for (int p : outs) pinMode(p, OUTPUT);

  Wire.begin(21, 22);
  mpuInit();
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) Serial.println("[oled] não encontrado");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS, 6);   // canal 6 = conexão rápida no Wokwi
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(768);
  configTime(0, 0, "pool.ntp.org");

  bootId = esp_random();
  leds(1, 0, 0); stateSince = millis();
  Serial.println("Passo Livre - robô iniciado. Ajuste os sensores no Wokwi para simular a calçada.");
}

void loop() {
  static uint32_t lastSample = 0, lastTelemetry = 0, lastDraw = 0;
  uint32_t now = millis();

  networkTick();

  if (now - lastSample >= SAMPLE_MS) {
    float dt = (now - lastSample) / 1000.0f;
    lastSample = now;
    lastFront = readUltrasonic(PIN_FRONT_TRIG, PIN_FRONT_ECHO);
    lastFloor = readUltrasonic(PIN_FLOOR_TRIG, PIN_FLOOR_ECHO);
    lastPitch = readPitchDeg();
    pushSample(lastFront, lastFloor, lastPitch);
    if (bufCount >= PL_WINDOW) { classify(); decide(dt); }
  }
  if (now - lastTelemetry >= 2000) { lastTelemetry = now; publishTelemetry(); }
  if (now - lastDraw >= 250)       { lastDraw = now; drawOled(); }
}
