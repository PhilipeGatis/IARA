# BOM — Bill of Materials

Lista completa de componentes do sistema IARA.

---

## 🧠 Controlador

| # | Componente | Especificação | Qtd |
|---|---|---|---|
| 1 | ESP32 DevKit V1 | 38 pinos, WROOM-32 | 1 |

---

## ⚡ Camada 1 — Entrada AC e Proteção

| # | Componente | Especificação | Qtd |
|---|---|---|---|
| 2 | Conector IEC C14 | Macho painel com fusível | 1 |
| 3 | Fusível AC | 3A–5A, 250V | 1 |
| 4 | NTC 5D-11 | Limitador inrush, 5Ω a frio | 1 |
| 5 | Capacitor NDF 222M | Capacitor Y, Fase–Terra | 1 |
| 6 | Capacitor NDF 222M | Capacitor Y, Neutro–Terra | 1 |
| 7 | Módulo Relé SSR 1CH | SSR, controle do Canister (AC) | 1 |

---

## 🔋 Camada 2 — Barramento DC e Lógica

| # | Componente | Especificação | Qtd |
|---|---|---|---|
| 8 | Fonte Colmeia 180W | 12V (ajustada 12.53V) | 1 |
| 9 | Fusível T5AL250V | 5A, proteção DC | 1 |
| 10 | Módulo LM2596 | Step-down ajustável (→ 5.1V) | 1 |
| 11 | Módulo MOSFET 8 canais | Entrada optoacoplada, **versão 3,3 V**, PWM em todos os canais, 12V. MOSFET não identificado — ver nota abaixo | 1 |
| 12 | Capacitor eletrolítico 470µF | 16V, filtro entrada MOSFET | 1 |
| 13 | Capacitor eletrolítico 1000µF | 10V, filtro saída 5V (ESP32) | 4 |

---

## 🌊 Camada 3 — Atuadores

| # | Componente | Especificação | Qtd |
|---|---|---|---|
| 14 | Bomba peristáltica 12V | Fertilizantes (CH1–CH4) | 4 |
| 15 | Bomba peristáltica 12V | Prime / desclorificante (CH5) | 1 |
| 16 | Bomba submersível 12V | Esgoto / drenagem (CH6) | 1 |
| 17 | Bomba submersível 12V | Recalque / refill (CH7) | 1 |
| 18 | Válvula solenoide 12V | Normalmente fechada (CH8) | 1 |
| 19 | Filtro canister | Controlado via relé AC | 1 |

---

## 📡 Sensores

| # | Componente | Especificação | Qtd |
|---|---|---|---|
| 20 | A02YYUW | Ultrassônico à prova d'água, UART, **3.3V** | 1 |
| 21 | Reed switch + ímã | Trava de nível máx., NA, em série GPIO33→IN7 | 1 |
| 22 | Boia float switch | Nível do reservatório (GPIO 19) | 1 |
| 23 | DS3231 | Módulo RTC I2C (SDA 21 / SCL 22) | 1 |
| 24 | Display TFT 1.8" | ST7735, SPI, 128×160 | 1 |
| 24b| Botão Push/Tactile | Navegação do display (GPIO 0) | 1 |

---

## 🛡️ Proteção e Filtragem

| # | Componente | Especificação | Qtd |
|---|---|---|---|
| 25 | Diodo FR154 | Fast Recovery, flyback bombas | 7 |
| 26 | Diodo 1N5822 | Schottky, flyback solenoide | 1 |
| 27 | Resistor 10kΩ | ¼W, pull-up do GPIO34 (TX do A02YYUW) | 1 |
| 28 | Capacitor 10µF / 22µF | 50V, desacoplamento sensores | 2–4 |

---

## 🔌 Conectores e Acessórios

| # | Componente | Especificação | Qtd |
|---|---|---|---|
| 29 | Conector GX12 (Aviação) | Painel fêmea/macho, para sensores | 2–4 |
| 30 | Cabo blindado multi-vias | 4 a 5 vias, soldável no GX12, 1.2m | 2–4 |
| 31 | Borne KRE / Wago | Conexões de potência | ~20 |
| 32 | Espaguete termo-retrátil | Isolamento NTC e soldas | 1m |
| 33 | Fio 22 AWG | Sinal / sensores | ~5m |
| 34 | Fio 18 AWG | Potência / bombas | ~10m |


---

## 📝 Notas sobre o módulo MOSFET

O módulo em uso é um **8 canais com entrada optoacoplada, versão 3,3 V**, com PWM em todos os canais. Anúncio: item AliExpress `1005010404945969`.

**O part number do MOSFET não está confirmado.** A BOM dizia `IRF540N ou equivalente`, o que era palpite — e um palpite ruim: o IRF540N tem V<sub>GS(th)</sub> de 2,0–4,0 V e é especificado para R<sub>DS(on)</sub> com V<sub>GS</sub> = 10 V. Se fosse mesmo IRF540N num gate de 3,3 V, ele operaria na região linear, dissipando watts num TO-220 sem dissipador dentro de uma caixa fechada. O módulo funciona, então provavelmente **não** é IRF540N. Leia a marcação gravada no encapsulamento e registre aqui — quem repuser um canal queimado vai comprar pelo que estiver escrito neste arquivo.

### Por que este módulo dispensa pull-down nas entradas

Numa placa de acionamento direto, o terminal IN vai ao **gate** do MOSFET. Gate é capacitivo e não tem limiar: tensão parasita acumula carga e o transistor começa a conduzir. Por isso essas placas exigem um pull-down de 10 kΩ em cada entrada — sem ele, o GPIO13 e o GPIO14 do ESP32, que saem do reset com pull-up interno de ~45 kΩ, poderiam ligar bombas durante os ~300 ms de bootloader, a cada reset e a cada gravação.

Aqui o IN alimenta um **LED** dentro do optoacoplador, e LED é dispositivo de limiar — precisa de 5 a 20 mA para acender. O pull-up interno entrega no máximo `(3,3 − 1,2) / 45k ≈ 47 µA`, duas a três ordens de grandeza abaixo disso. **O optoacoplador já cumpre o papel do pull-down.**

Confirme com o multímetro em **modo diodo**, entre IN e o GND do lado de controle: ~1,0–1,3 V numa polaridade e aberto na outra significa LED. Se ler resistência nos dois sentidos, é acionamento direto e os pull-downs voltam a ser obrigatórios.

### Verifique se o isolamento é real

Placas baratas anunciadas "com optoacoplador" frequentemente unem os dois GNDs por um jumper de solda ou uma trilha, e aí a barreira é decorativa.

Isso importa muito neste projeto. Todos os 8 canais chaveiam pelo lado baixo, então a corrente de retorno de todas as bombas passa pelo terminal GND do módulo. Se esse parafuso afrouxar, vários ampères procuram outro caminho de volta ao negativo da fonte — e o único disponível são os fios de sinal, entrando direto nos GPIOs. É a explicação que melhor encaixa com os GPIO5 e GPIO19 queimados: é conduzida, é ampères, e acontece exatamente quando uma bomba parte.

**Com a barreira galvânica intacta essa corrente não tem caminho até o ESP32.** Meça com o ohmímetro, módulo desconectado, entre o GND do lado de controle e o GND do lado de potência:

- **Aberto** — isolamento real. Mantenha assim: não una esses dois GNDs em lugar nenhum.
- **~0 Ω** — GNDs compartilhados. Procure o jumper de solda; cortá-lo devolve a proteção.

### PWM através do optoacoplador

`FertManager` roda LEDC a 5 kHz nos canais 1 a 5. Um optoacoplador comum (PC817 e similares) desliga bem mais devagar do que liga, então o duty que chega ao gate não é exatamente o comandado, sobretudo em valores baixos. A calibração de vazão absorve isso, porque é medida empiricamente — mas se um canal não girar em PWM baixo, a causa é essa, não bomba fraca.

---

## 📊 Resumo de Quantidades

| Categoria | Itens |
|---|---|
| Capacitores eletrolíticos | 6–10 |
| Diodos (flyback) | 8 |
| Resistores | 1 |
| Motores / bombas | 7 |
| Sensores | 4 |
| Módulos (ESP32, LM2596, MOSFET, Relé, RTC, TFT) | 6 |
