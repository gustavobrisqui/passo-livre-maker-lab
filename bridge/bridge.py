"""
Ponte MQTT -> API do Passo Livre.

Assina <TOPIC_BASE>/relatos, valida o JSON do robô e faz POST no endpoint
de relatos do app (o mesmo que recebe relatos de cidadãos e da visão
computacional). O relato_id vai como Idempotency-Key, então reenvios do
robô (fila offline) não duplicam no banco; a deduplicação espacial fica
com o bot de RPA.

Variáveis de ambiente (nada de credencial no código):
  MQTT_HOST           default broker.hivemq.com
  MQTT_PORT           default 1883
  TOPIC_BASE          default passolivre/gs2026-4es
  PASSO_LIVRE_API_URL ex.: https://passolivre.exemplo.com/api/v1/relatos
  PASSO_LIVRE_API_KEY chave do dispositivo (header X-Device-Key)
  DRY_RUN=1           só imprime, não chama a API

Uso:  pip install -r requirements.txt && python bridge.py
"""
import json
import os
import sys
import urllib.request

import paho.mqtt.client as mqtt

MQTT_HOST = os.getenv("MQTT_HOST", "broker.hivemq.com")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
TOPIC = os.getenv("TOPIC_BASE", "passolivre/gs2026-4es") + "/relatos"
API_URL = os.getenv("PASSO_LIVRE_API_URL", "")
API_KEY = os.getenv("PASSO_LIVRE_API_KEY", "")
DRY_RUN = os.getenv("DRY_RUN", "1" if not API_URL else "0") == "1"

TIPOS = {"OBSTACULO_CALCADA", "BURACO", "DEGRAU_SEM_RAMPA", "RAMPA_INCLINACAO_EXCESSIVA"}
REQUIRED = {"relato_id", "device_id", "tipo", "confianca", "lat", "lon"}


def validate(r: dict) -> str | None:
    missing = REQUIRED - r.keys()
    if missing:
        return f"campos faltando: {sorted(missing)}"
    if r["tipo"] not in TIPOS:
        return f"tipo desconhecido: {r['tipo']}"
    if not (-24.1 < r["lat"] < -23.3 and -47.0 < r["lon"] < -46.3):
        return "fora da cidade de São Paulo"
    if not 0 <= r["confianca"] <= 1:
        return "confiança inválida"
    return None


def to_api(r: dict) -> dict:
    """Contrato do app: mesmo formato de um relato de cidadão, com origem=robo."""
    return {
        "origem": "robo_ultrassonico",
        "dispositivo": r["device_id"],
        "categoria": r["tipo"],
        "confianca_ia": r["confianca"],
        "localizacao": {"lat": r["lat"], "lon": r["lon"]},
        "medidas": r.get("medidas", {}),
        "status": "pendente_validacao",   # pessoa no circuito: moderação/RPA confirma antes de abrir chamado
        "registrado_em": r.get("ts") or None,
    }


def post(r: dict) -> None:
    body = json.dumps(to_api(r)).encode()
    req = urllib.request.Request(API_URL, data=body, method="POST", headers={
        "Content-Type": "application/json",
        "X-Device-Key": API_KEY,
        "Idempotency-Key": r["relato_id"],
    })
    with urllib.request.urlopen(req, timeout=10) as resp:
        print(f"  -> API {resp.status}")


def on_message(_c, _u, msg):
    try:
        r = json.loads(msg.payload)
    except json.JSONDecodeError:
        print("payload inválido, ignorado")
        return
    err = validate(r)
    if err:
        print(f"rejeitado {r.get('relato_id')}: {err}")
        return
    print(f"relato {r['relato_id']} {r['tipo']} ({r['confianca']:.0%}) @ {r['lat']:.5f},{r['lon']:.5f}")
    if DRY_RUN:
        print("  (DRY_RUN) " + json.dumps(to_api(r), ensure_ascii=False))
        return
    try:
        post(r)
    except Exception as e:  # API fora do ar: o robô já guarda fila; aqui só registramos
        print(f"  !! falha ao enviar: {e}", file=sys.stderr)


def main():
    c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"passolivre-bridge-{os.getpid()}")
    c.on_connect = lambda cl, *_: (cl.subscribe(TOPIC), print(f"assinando {TOPIC} em {MQTT_HOST}"))
    c.on_message = on_message
    c.connect(MQTT_HOST, MQTT_PORT, 60)
    c.loop_forever()


if __name__ == "__main__":
    main()
