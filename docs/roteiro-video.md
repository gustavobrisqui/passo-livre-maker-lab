# Roteiro do vídeo (máx. 6 min)

Tela dividida: **Wokwi** à esquerda, **mapa/index.html** à direita.

| Tempo | Cena | Fala (resumo) |
|---|---|---|
| 0:00–0:40 | Foto de calçada com carro/entulho/guia alta | Problema: barreiras em calçadas de SP que ninguém registra. Passo Livre = mapa colaborativo. Este robô é a peça física. |
| 0:40–1:30 | `diagram.json` no Wokwi + tabela de componentes | Evolução do robô: além do ultrassônico frontal, **ultrassônico de piso** (desnível) e **MPU6050** (inclinação). Mostrar onde cada um entra. |
| 1:30–2:20 | `passo_livre_model.h` e `train_model.py` | IA: janela de 1 s → 7 features → MLP TinyML com 6 classes, no próprio ESP32 e sem internet. Dataset simulado, teste de paridade C×Python. |
| 2:20–2:45 | ▶ Simulação, OLED em `PATRULHANDO`, ponto verde andando no mapa | Calçada livre: o robô anda e manda telemetria. |
| 2:45–3:10 | Slider frontal 300 → 60 cm rápido | **Pedestre**: para, bip curto, dá passagem. **Não vira relato** (LGPD). |
| 3:10–3:45 | Frontal 30 cm e soltar | **Carro na calçada**: primeiro trata como pedestre e espera; como não sai em 2 s, vira barreira, relata e desvia. Pino laranja no mapa. |
| 3:45–4:15 | Piso 35 cm | **Buraco**: reflexo de segurança para na hora, IA confirma, relata, dá ré. Pino vermelho. |
| 4:15–4:35 | Piso 4 cm | **Degrau sem rampa**: pino roxo. |
| 4:35–5:00 | MPU accelX 0,15 g, depois 0,05 g | **Rampa a ~15 %** relatada; rampa de ~5 % não (dentro da NBR 9050). |
| 5:00–5:30 | Serial com JSON + `bridge.py` em DRY_RUN | Integração: MQTT → mapa e → API do app; status `pendente_validacao`, o RPA prioriza e abre chamado. |
| 5:30–6:00 | README | Limitações (dados simulados, odometria) e próximos passos (dados reais, GPS). Nomes da equipe. |
