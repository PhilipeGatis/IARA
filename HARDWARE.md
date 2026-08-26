# Documentação de Arquitetura de Hardware: Sistema de Gestão de Aquário

## 📋 Visão Geral

O sistema utiliza uma topologia de **Defesa em Profundidade**, com filtragem de ruído na entrada (AC), estabilização no barramento principal (DC) e supressão de picos indutivos na borda (Atuadores).

```
┌─────────────────────┐     ┌─────────────────────┐     ┌─────────────────────┐
│  CAMADA 1: AC       │ ──► │  CAMADA 2: DC       │ ──► │  CAMADA 3: BORDA    │
│  Entrada/Proteção   │     │  Barramento/Lógica  │     │  Periféricos/Supres.│
│                     │     │                     │     │                     │
│  • Fusível AC       │     │  • Fonte 12.53V     │     │  • Diodos Flyback   │
│  • NTC (Inrush)     │     │  • LM2596 → 5.1V    │     │  • Caps desacopl.   │
│  • Filtro EMI (Y)   │     │  • Banco de Caps    │     │  • Sensores (GX12)  │
│  • Relé Canister    │     │  • MOSFET 8 canais  │     │  • Bombas + Válvula │
└─────────────────────┘     └─────────────────────┘     └─────────────────────┘
```

---

## ⚡ Camada 1: Entrada e Proteção AC (Infraestrutura)

A arquitetura da entrada de energia foca em simplicidade e máxima segurança elétrica, delegando as proteções e filtragens (como Filtro EMI e Termistor NTC/Inrush) para os circuitos profissionais já embutidos na Fonte Colmeia.

### Componentes

| Componente | Função |
|---|---|
| **Fusível de Vidro AC** (3A a 5A) | Item obrigatório: Proteção contra curto-circuito bruto e risco de incêndio |
| **Módulo Relé SSR 1CH** | Controle independente via ESP32 para o filtro Canister (AC) |

### Esquema de Ligação

```text
[ TOMADA IEC C14 ]
      │
      ├─── [ PINO TERRA (Verde) ] ────────┬─────────► [ Borne G (TERRA) da Fonte ]
      │                                   │
      │                                   └─────────► [ Pino TERRA - Tomada Canister ]
      │         
      ├─── [ PINO NEUTRO (Azul) ] ────────┬─────────► [ Borne N (NEUTRO) da Fonte ]
      │                                   │
      │                                   └─────────► [ Pino NEUTRO - Tomada Canister ]
      │
      └─── [ PINO FASE (Marrom) ] ─── [ FUSÍVEL ] ──┐
                                                    │
             ┌──────────────────────────────────────┘
             │                                  
             ├───────────────────► [ Borne L (FASE) da Fonte ]
             │
             └───► [ Parafuso 1 do Relé SSR ] ── INTERRUPTOR ──► [ Parafuso 2 do Relé SSR ] ──► [ Pino FASE - Tomada Canister ]
```

---

## 🔋 Camada 2: Barramento DC e Lógica (Distribuição)

Responsável por converter a potência para os níveis lógicos e manter a estabilidade do ESP32 durante o chaveamento das cargas pesadas.

### Componentes

| Componente | Função |
|---|---|
| **Fonte Colmeia 180W** | Ajustada para 12.53V |
| **Fusível T5AL250V** | Firewall físico para as 8 bombas e sensores |
| **LM2596** | Step-down ajustado para 5.1V (alimentação ESP32) |
| **1× 470µF 16V** | Em paralelo na entrada 12V do MOSFET (Evita queda de tensão quando as bombas ligam) |
| **4× 1000µF 10V** | Em paralelo na saída 5V (Atua como "Nobreak" para o ESP32 aguentar flutuações e picos da rede) |

### Esquema de Ligação

```text
[ FONTE 12.53V ]
   │              │
 (V+)           (V─) ──────────────────────────────┐ (GND ESTRELA)
   │              │                                │
[FUSÍVEL T5A]     │                                │
   │              │                                │
   ├──────────────┼────────────────┐               │
   │              │                │               │
[LM2596 IN+]  [LM2596 IN─]    [MOSFET VIN]   [MOSFET GND]
   │              │                │               │
(Saída 5.1V)      │         (1× 470µF 16V)         │
   │              │                │               │
(4× 1000µF 10V)   │                │               │
   │              │                │               │
[ESP32 VIN]   [ESP32 GND]          │               │
   │              │         [ 8 CANAIS MOSFET ]    │
   │              └────────────────┴───────────────┘
```

> [!IMPORTANT]
> **GND de POTÊNCIA — estrela na fonte**: As referências negativas de **potência** (módulo MOSFET, LM2596, bombas, solenoide) devem retornar diretamente ao borne V− da fonte colmeia, para evitar loops de terra.

> [!CAUTION]
> **GND de SINAL — regra oposta**: O retorno (GND) dos **sensores** — boia e ultrassônico — deve ir para o **pino GND do ESP32**, e **nunca** direto ao borne da fonte.
>
> O ESP32 mede cada entrada em relação ao pino GND *dele*. Um sensor referenciado ao borne da fonte entrega ao GPIO a diferença de potencial entre os dois pontos. Na partida das bombas essa diferença passa de **1 V negativo** — muito além do mínimo absoluto de −0,3 V do ESP32 — e o dano se acumula até o pino abrir.
>
> Foi exatamente assim que os GPIO5 e GPIO19 queimaram. Ver [`PROTECAO_ELETRICA.md`](PROTECAO_ELETRICA.md).

---

## 🌊 Camada 3: Periféricos e Supressão (Borda)

Responsável por mitigar o ruído indutivo (Flyback) e estabilizar a leitura dos sensores em cabos longos (1,2m).

### Componentes

| Componente | Aplicação | Qtd |
|---|---|---|
| **Diodos FR154** (Fast Recovery) | Bombas de Recalque e Esgoto | 2 |
| **Diodos FR154** (Fast Recovery) | Bombas Peristálticas (4 Fert + 1 Prime) | 5 |
| **Diodo 1N5822** (Schottky) | Válvula Solenoide (Canal 8) | 1 |
| **Capacitores 10µF / 22µF 50V** | Desacoplamento na ponta dos sensores (GX12) | — |

> [!NOTE]
> **Total: 8 diodos flyback** — um para cada canal com motor (7× FR154 + 1× 1N5822). Todos instalados na ponta do fio, junto ao motor.

### Esquema de Ligação — Atuadores

```
[ CANAL MOSFET ] ──────────── (Fio 1.2m) ──────┬────────── (BOMBA +)
                                               │
                                         [ DIODO FLYBACK ]
                                         (Listra no POS+)
                                               │
[ GND BARRAMENTO ] ─────────── (Fio 1.2m) ─────┴────────── (BOMBA ─)
```

> [!CAUTION]
> **Diodos Flyback**: Devem ser instalados obrigatoriamente **na ponta do fio** (junto ao motor) para evitar que o cabo de 1,2m irradie ruído como uma antena.

### Esquema de Ligação — Sensor Ultrassônico (A02YYUW, UART)

Sensor ultrassônico à prova d'água que envia a distância por **UART**. Não usa TRIG/ECHO e não precisa de divisor de tensão. Transmite sozinho, sem receber comando: um frame a cada ~100 ms.

| Fio | Função | Conexão | Observação |
|---|---|---|---|
| **VCC** (vermelho) | Alimentação | 3.3V do ESP32 | ⚠️ Nunca 5V — ver aviso abaixo |
| **GND** (preto) | Retorno | **Pino GND do ESP32** | ⚠️ Nunca o borne da fonte |
| **TX** | Saída de dados do sensor | GPIO34 (RX2) | Precisa de pull-up de 10kΩ |
| **RX** | Entrada de controle | 3.3V | Não usado pelo firmware — ver nota |

> A cor dos dois fios de dados varia entre lotes; só vermelho e preto são consistentes. Para identificar a saída, alimente o sensor e meça: o **TX** repousa em ~3.3V e apresenta atividade; o RX fica inerte.

```
   ESP32 3.3V ──────┬──────────────────►  VCC  (vermelho)
                    │
                [ 10 kΩ ]                        A02YYUW
                    │                        (à prova d'água)
   ESP32 GPIO34 ────┴──────────────────►  TX   (saída de dados)
    (RX2)

   ESP32 3.3V ─────────────────────────►  RX   (controle, não usado)

   ESP32 GND ──────────────────────────►  GND  (preto)
    (pino da placa, nunca o borne da fonte)
```

> [!WARNING]
> **Alimente com 3.3V, nunca com 5V.** O A02YYUW aceita 3.3–5V, mas o nível lógico da saída acompanha o VCC. Em 5V a linha de TX entregaria 5V ao GPIO34 e o danificaria permanentemente. É justamente por operar em 3.3V que a ligação dispensa divisor de tensão.

> [!IMPORTANT]
> **Pull-up de 10kΩ entre GPIO34 e 3.3V.** O GPIO34 é input-only e **não possui pull-up interno**. Sem o resistor externo, cabo solto ou sensor sem alimentação deixam a linha flutuando, e o firmware lê ruído como se fosse dado. Com o pull-up a linha repousa em nível alto — que é o repouso do UART — e a ausência de dados vira uma falha limpa, detectada pelo timeout de 2 s em `SafetyWatchdog::readUltrasonic()`.

> [!NOTE]
> **O fio de controle (RX do sensor) não é usado.** Nada no firmware escreve no `Serial2`; o sensor transmite por conta própria. Esse fio deve ir ao **3.3V** para manter o modo de transmissão contínua — o que não se deve fazer é deixá-lo flutuando.
>
> Esse fio fica no **GPIO18**. Nada no firmware escreve no `Serial2`, mas o pino segue declarado como TX do UART justamente para manter a linha em nível alto, que é o que o sensor precisa para permanecer em transmissão contínua.

> [!TIP]
> **Protocolo:** 9600 baud, 8N1. Frame de 4 bytes — `0xFF`, `DataH`, `DataL`, `Checksum`, onde a distância vem em **milímetros** (`(DataH << 8) | DataL`) e o checksum é `(0xFF + DataH + DataL) & 0xFF`. O firmware descarta frames com checksum inválido e aplica filtro de mediana de 5 amostras.

> [!CAUTION]
> **Desacoplamento local.** Alimentado em 3.3V o sensor opera no piso da faixa dele, o que o deixa sensível à queda de tensão quando as bombas partem — o sintoma é leitura corrompida justamente durante o dreno e o recalque. Instale **100 µF + 100 nF no conector do sensor**, não na placa. Ver [`PROTECAO_ELETRICA.md`](PROTECAO_ELETRICA.md).

### Esquema de Ligação — Trava de Nível Máximo (Reed Switch)

#### Por que a trava fica no recalque, e não na solenoide

À primeira vista a solenoide parece o alvo óbvio: ela é alimentada pela rede de água e, sozinha, encheria a casa. Mas ela **já tem uma boia mecânica** no reservatório — uma válvula que fecha o fluxo fisicamente quando a água sobe, sem passar por eletrônica nenhuma. É a mesma ideia da boia de caixa d'água.

A bomba de recalque não tem esse recurso e não pode ter: ela empurra água *para dentro* do aquário, e não existe válvula mecânica que interrompa uma bomba do lado da descarga. Se ela não parar, a água vai ao chão. Por isso o reed protege o canal que não tinha nada.

Resumo das travas de água, do mais forte para o mais fraco:

| Atuador | Trava física | Trava elétrica | Firmware |
|---|---|---|---|
| Solenoide (CH8) | **Boia mecânica no reservatório** | — | Boia elétrica no GPIO19 + timeout de 8 min |
| Bomba de recalque (CH7) | — (impossível) | **Reed em série com o sinal IN7** | Ultrassônico + timeout dimensionado pela vazão |

O corte de nível máximo **não passa pelo firmware**. É um reed switch em série com o fio de sinal entre o **GPIO33** e a entrada **IN7** do módulo MOSFET (canal da bomba de recalque). Se a água atingir o nível máximo, o contato abre, o comando se rompe e a bomba para — mesmo com o ESP32 travado, reiniciando ou com o ultrassônico mudo.

Montagem: reed **NA** (normalmente aberto) com um ímã mantido próximo por uma boia de EVA, fora da água. No nível normal o ímã está presente e o contato fechado. Quando a água sobe, a boia afasta o ímã e o contato abre.

| Situação | Ímã | Reed | Sinal | Bomba |
|---|---|---|---|---|
| Nível normal | presente | fechado | passa | pode funcionar |
| Nível máximo | afastado | aberto | cortado | **parada** |
| Fio rompido | — | — | cortado | **parada** |

```
  ESP32 GPIO33 ───[ REED ]───┬─── IN7 (módulo MOSFET, canal 7)
                             │
                         [ 1 kΩ ]
                             │
                         [ 100 nF ]
                             │
                       GND do módulo
```

> [!CAUTION]
> **O resistor de 1 kΩ é obrigatório e fica do lado do módulo**, depois do ponto onde o reed corta. A entrada do MOSFET funciona por carga acumulada: com o fio aberto e sem esse resistor, o gate fica flutuando, retém carga e pode manter o MOSFET parcialmente conduzindo — a bomba não desliga.
>
> Antes de montar, meça a resistência entre IN7 e o GND do módulo. Se já houver algo entre 10 kΩ e 100 kΩ, o módulo tem pull-down próprio e o resistor externo é dispensável.

> [!WARNING]
> **Não ligue o reed em série com a alimentação da bomba.** Contato de reed é especificado para 0,5–1 A, e a bomba de recalque puxa vários ampères na partida. O contato arcaria e acabaria soldando fechado — falha silenciosa que anula a trava. Nesta posição, no fio de sinal, ele conduz menos de 4 mA.

> [!TIP]
> **Teste a distância de liberação antes de fechar o conjunto.** Reed tem histerese: fecha a uma distância e só abre a uma distância maior. Com o multímetro em continuidade, afaste o ímã até o contato abrir e anote a distância — o curso da boia deve ser 2 a 3 vezes esse valor. Confirme também que ele não volta a fechar em nenhuma posição intermediária do curso.

> [!IMPORTANT]
> **O firmware não enxerga esse corte.** Quando o reed agir, a máquina de estados continuará em `REFILLING` até estourar o timeout e terminar em `ERROR`. Em operação normal o ultrassônico atinge o setpoint antes, então isso não acontece — o reed é rede de proteção, não parada de rotina.
>
> O **GPIO4**, que era do sensor capacitivo XKC-Y25 (nunca instalado, removido do projeto), está livre. Ligar um segundo reed nele devolveria essa visibilidade ao firmware, mas exige mudança de código.

### Esquema de Ligação — Display TFT ST7735 (SPI)

Display colorido 1.8" 128×160 pixels. Opera em **3.3V** — incompatível com 5V. Utiliza SPI por software (bit-banging) em pinos customizados.

| Pino do Display | Conexão ESP32 | GPIO | Observação |
|---|---|---|---|
| **VCC** | 3.3V | — | ⚠️ Nunca usar 5V |
| **GND** | GND | — | |
| **CS** | D15 | GPIO15 | Chip Select |
| **RESET** | Pino EN | — | Reset hardware compartilhado com o ESP32 |
| **A0 (DC)** | TX2 | GPIO17 | Data/Command |
| **SDA (MOSI)** | D23 | GPIO23 | Dados SPI |
| **SCK** | RX2 | GPIO16 | Clock SPI |
| **LED** | 3.3V | — | Backlight fixo (sem GPIO livre disponível) |

```
ESP32 3.3V  ────►  VCC
ESP32 GND   ────►  GND
ESP32 D15   ────►  CS
ESP32 EN    ────►  RESET
ESP32 TX2   ────►  A0 (DC)
ESP32 D23   ────►  SDA (MOSI)
ESP32 RX2   ────►  SCK
ESP32 3.3V  ────►  LED (backlight fixo)
```

> [!WARNING]
> O pino **RESET** do display deve ir ao pino **EN** do ESP32 (não a um GPIO). Isso garante que o display é resetado junto com o microcontrolador. Com o pino EN em HIGH (nível normal de operação), o display funciona normalmente.

> [!NOTE]
> No breakout board ESP32 DevKit V1, os pinos **D16** e **D17** aparecem rotulados como **RX2** e **TX2** respectivamente. Use os rótulos do seu terminal para identificar a posição correta.

---

### Esquema de Ligação — Boia do Reservatório

Boia horizontal que indica reservatório cheio. Fecha o solenoide no estado `FILLING_RESERVOIR` do TPA. O firmware exige 5 leituras consecutivas de "cheio" (debounce ~250 ms) antes de aceitar o sinal.

| Pino | Conexão | GPIO | Configuração |
|---|---|---|---|
| **Terminal 1** | GPIO19 | GPIO19 | `INPUT_PULLUP` — ativo em LOW |
| **Terminal 2** | **Pino GND do ESP32** | — | ⚠️ Nunca no borne da fonte |

```
ESP32 GPIO19 ─────── [ BOIA ] ─────── ESP32 GND (pino da placa)
         INPUT_PULLUP, ativo em LOW (cheio = LOW)
         Os 2 fios saem juntos, no mesmo cabo, o caminho todo
```

> [!NOTE]
> A boia usava GPIO5 originalmente, mas aquele pino é um strapping pin do ESP32 e sofria interferência (~2,5 V em repouso). Foi movida para GPIO19, que antes era do botão de navegação do painel.

> [!IMPORTANT]
> **O botão de navegação do painel está no GPIO5** (`PIN_BTN` em `include/Config.h`). Ele perdeu o GPIO19 para a boia. O GPIO5 é strapping pin do ESP32, então não pode ficar pressionado durante a energização; um toque momentâneo em qualquer outro momento é inofensivo.

---

## 🛠️ Notas de Implementação Segura

1. **Conexões AC** — Isole todas as soldas e conexões de 110V/220V com espaguete termo-retrátil para segurança máxima.

2. **Polaridade dos Capacitores** — Verificar a listra negativa em todos os eletrolíticos (especialmente os de 1000µF 10V que estão operando em 5.1V).

3. **Diodos Flyback** — Devem ser instalados obrigatoriamente na ponta do fio (junto ao motor) para evitar que o cabo de 1,2m irradie ruído como uma antena.

4. **GND Estrela** — Todas as referências negativas devem retornar diretamente ao borne V− da fonte colmeia para evitar loops de terra.

5. **Efeito Sifão (Hidráulica)** — Para evitar o esvaziamento contínuo do aquário por gravidade após o desligamento da bomba de drenagem, ligue uma válvula solenoide em paralelo com a bomba (no mesmo canal MOSFET, garantindo um diodo flyback para cada) ou faça um furo de respiro na mangueira.
