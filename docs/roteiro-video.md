# Roteiro do vídeo (máx. 6 min · alvo 5 min)

Gravação em **um take**. Leia as falas em voz natural; o que está em *itálico* é ação, não é para falar.

**Abas na tela gravada (nesta ordem):**
1. Foto de calçada com barreira
2. README no GitHub
3. Wokwi (metade esquerda) + `mapa/index.html` (metade direita)

**Antes de gravar:** Wokwi parado, mapa aberto e "Conectado", Não Perturbe ligado.

---

## Cena 1 · Problema (0:00–0:35) — aba: foto da calçada

> Olá, somos a equipe do Passo Livre, da Global Solution CitizenTech da FIAP.
> Nas calçadas de São Paulo é comum encontrar carro estacionado, entulho, buraco, guia sem rebaixo e rampa íngreme demais. Para quem usa cadeira de rodas, bengala ou andador, qualquer uma dessas barreiras interrompe o caminho. E hoje quase nenhuma delas é registrada: alguém precisa parar, fotografar e abrir um chamado.
> O Passo Livre é um mapa colaborativo dessas barreiras. E este robô é a parte física do produto: ele percorre a calçada, identifica as barreiras sozinho e manda para o mapa.

## Cena 2 · Evolução do robô (0:35–1:10) — aba: README, seções 2 e 3

*Rolar até a tabela "Evolução em relação ao robô anterior".*

> O nosso robô anterior tinha só um sensor ultrassônico na frente. Nesta versão adicionamos dois sensores.
> Um segundo ultrassônico, apontado para o chão, que mede a distância até o piso: se ela aumenta, é buraco; se diminui, é degrau sem rampa.
> E um acelerômetro MPU6050, que mede a inclinação. A norma de acessibilidade, a NBR 9050, permite no máximo 8,33 por cento de inclinação numa rampa, e o robô verifica isso.

*Rolar até a tabela de componentes.*

> Tudo roda num ESP32, com tela OLED, LEDs, buzzer e ponte H para os motores.

## Cena 3 · Inteligência Artificial (1:10–1:50) — aba: README, seção 4

*Rolar até "4. Integração sensores × IA".*

> A cada 100 milissegundos o robô lê os três sensores e guarda o último segundo de leituras. Desse segundo saem 7 características, como a distância média à frente, o quanto ela variou e a inclinação.
> Essas características entram numa rede neural pequena, TinyML, que roda dentro do próprio ESP32, sem internet, e classifica a situação em seis classes: livre, pessoa, obstáculo, buraco, degrau e rampa.

*Rolar até "4.4 Da classe à ação".*

> E a IA não decide sozinha: se o chão some na frente, um reflexo de segurança para o robô na hora, antes mesmo da IA confirmar. E a classe só é aceita quando aparece três vezes seguidas com pelo menos 70 por cento de confiança, o que evita alarme falso.

## Cena 4 · Calçada livre (1:50–2:05) — aba: Wokwi + mapa

*Clicar em ▶ no Wokwi. Esperar o OLED mostrar `PATRULHANDO` e o ponto verde andar no mapa.*

> Vamos ver funcionando. À esquerda, a simulação no Wokwi; à direita, o mapa ao vivo. Com a calçada livre, o LED verde fica aceso, o robô anda e manda a posição para o mapa a cada dois segundos.

## Cena 5 · Pedestre (2:05–2:30)

*Clicar no sensor ultrassônico **da frente**. Arrastar o slider de ~300 até ~60 cm em mais ou menos 1 segundo.*

> Agora uma pessoa vindo na direção do robô.

*Esperar o LED amarelo e o bip. Depois arrastar o slider de volta até 400.*

> Ele reconhece que é uma pessoa pela velocidade com que a distância diminui, para e dá passagem. Repare que nenhum ponto apareceu no mapa: pessoas nunca viram registro, por causa da LGPD.

## Cena 6 · Carro na calçada (2:30–3:05)

*Sensor da frente em ~30 cm e soltar.*

> Agora algo parado na frente, como um carro em cima da calçada.

*Esperar ~3 s: aparece `BARREIRA`, depois `DESVIANDO` e um ponto laranja no mapa.*

> Primeiro ele trata como se fosse uma pessoa e espera. Como o objeto continua ali depois de dois segundos, ele entende que é uma barreira, registra no mapa, ponto laranja, e desvia.

*Sensor da frente de volta a 400.*

## Cena 7 · Buraco e degrau (3:05–3:35)

*Clicar no sensor ultrassônico **do piso**. Colocar em ~35 cm.*

> Agora um buraco: a distância até o chão aumentou de 10 para 35 centímetros. O reflexo de segurança para o robô na hora, a IA confirma que é buraco, ele registra, ponto vermelho, e dá ré.

*Piso de volta a 10. Esperar 2 s. Piso em ~4 cm.*

> E aqui uma guia sem rebaixo: o chão ficou mais perto. Registro de degrau sem rampa, ponto roxo.

*Piso de volta a 10.*

## Cena 8 · Rampa (3:35–4:05)

*Clicar no **MPU6050**. Colocar accel X em 0,15.*

> Agora o acelerômetro: uma rampa com cerca de 15 por cento de inclinação, acima da norma. O robô reduz a velocidade e registra, ponto amarelo.

*Accel X de volta a 0. Depois accel X em 0,05.*

> Já uma rampa de 5 por cento está dentro da norma: o robô segue normalmente e não registra nada.

*Accel X de volta a 0.*

## Cena 9 · Falhas (4:05–4:25)

*Sensor da frente em 400.*

> Quando o sensor não recebe eco, por exemplo num espaço aberto, a leitura vai para 400 centímetros e o robô não inventa um obstáculo.
> E se a internet cair, os registros ficam guardados numa fila de até 20 itens e são enviados quando a conexão volta, sem duplicar.

## Cena 10 · Integração (4:25–4:50) — Serial Monitor do Wokwi

*Rolar o Serial Monitor até uma linha `[relato] {...}`. No mapa, clicar num ponto para abrir o balão.*

> Cada registro sai do robô neste formato, com tipo, confiança da IA, localização e medidas, e vai por MQTT para o mapa e para a API do aplicativo. Ele chega como "pendente de validação": a automação do Passo Livre confirma, prioriza e abre o chamado na prefeitura.

## Cena 11 · Encerramento (4:50–5:15) — aba: README, topo (integrantes)

> Como limitações, o modelo foi treinado com dados simulados e a posição é estimada pela velocidade. Os próximos passos são coletar leituras reais com o carrinho e usar GPS.
> O código, a simulação e a documentação estão neste repositório. Obrigado!

---

### Se algo der errado durante a gravação

- Errou uma fala ou um slider: volte o sensor ao normal (frente 400, piso 10, accel X 0) e siga.
- O ponto não apareceu no mapa: siga mesmo assim e mostre o `[relato]` no Serial Monitor.
- Pedestre virou obstáculo: o slider ficou parado tempo demais; volte a frente para 400 e continue.
