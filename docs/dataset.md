# Dataset simulado — como e por que foi gerado

A GS aceita "dados simulados com geração justificada e documentada". Este documento justifica cada escolha de `ml/train_model.py`.

## Por que simulado

1. **Prazo e segurança:** coletar centenas de janelas reais de buracos, guias e rampas com o carrinho exigiria levá-lo para a rua, com risco de queda e sem autorização de uso da via.
2. **Reprodutibilidade:** o avaliador regenera exatamente o mesmo dataset (`seed = 42`) e o mesmo modelo.
3. **Física conhecida:** o comportamento do HC-SR04 e do MPU6050 é bem documentado, então a simulação é fiel o bastante para treinar a separação entre classes.

## Janela

10 leituras a 10 Hz (1 s) de: distância frontal (cm), distância ao piso (cm, sensor a 10 cm do chão) e inclinação (graus, eixo X do MPU6050).

## Regras por classe

| Classe | Frontal | Piso | Inclinação | Fundamento |
|---|---|---|---|---|
| LIVRE | 90–400 cm, sem eco (400), ou **algo saindo da frente** (salto de 8–80 para 150–400 cm) | 10 cm ± ruído | −4,3° a +4,3° | rampas dentro da NBR 9050 continuam livres |
| OBSTACULO | 8–60 cm, variação 0 a −25 cm/s | normal | normal | objeto parado; robô parado ou andando a ~20 cm/s |
| PESSOA | começa em 80–320 cm e cai 50–200 cm na janela, ou **alguém entra de lado** (salto de 150–400 para 15–100 cm) | normal | normal | pedestre a 0,5–2 m/s relativos ao robô |
| BURACO | livre | salta para 16–80 cm a partir de uma amostra aleatória (0–4) | normal | o robô "vê" a borda do buraco no meio da janela |
| DEGRAU | 30–400 cm | cai para 2,5–6 cm a partir de uma amostra aleatória | normal | guia/degrau de 4 a 8 cm sem rebaixo |
| RAMPA | livre | 10 cm ± ruído | 5,3° a 15°, subida ou descida | NBR 9050: inclinação máx. 8,33 % = 4,76°; margem de 0,5° evita rótulo ambíguo |

## Ruído e falhas incluídos

- Ruído gaussiano com σ sorteado por janela entre 0 e 1,2 cm (inclui janelas "limpas", como os sliders do Wokwi).
- Ruído maior para pessoas (corpo irregular reflete pior).
- 2 % de **ecos perdidos** viram 400 cm, igual ao timeout do firmware.
- Leituras limitadas a 2–400 cm, o alcance do HC-SR04.

## Resultado

- 18.000 janelas (3.000 por classe), 80/20 estratificado.
- Acurácia de teste: 99,5 % (ver `ml/metrics.json`); as confusões ficam entre LIVRE, OBSTACULO e PESSOA nas janelas de transição. Isso mostra que as classes, **como foram definidas**, são separáveis pelas 7 features. Não é uma estimativa de desempenho em campo.
- 10 cenários de slider do Wokwi e 4 sequências de transição verificados automaticamente.

## Próximo passo para campo

Gravar janelas reais com o carrinho (o firmware já imprime as features no Serial em cada mudança de classe), rotular, misturar com o simulado e retreinar. As faixas de limiar acima viram hipóteses a validar.
