# Proteção Elétrica dos Sensores

Guia em linguagem simples sobre por que a boia do reservatório queimou o pino do ESP32, e o que instalar para não acontecer de novo.

> **Status:** documento de referência. Nada aqui foi implementado ainda.

---

## O problema em uma frase

O ESP32 é delicado: seus pinos aceitam de **0 V a 3,3 V** e nada fora disso. Passar disso, mesmo por um milionésimo de segundo, danifica o pino — e o dano se acumula até ele parar de funcionar de vez.

A boia já matou dois pinos assim: primeiro o GPIO5, depois o GPIO19.

---

## Por que o pino queimou

### Causa 1 — martelo hidráulico, mas elétrico

Você conhece o efeito de fechar uma torneira de golpe: o cano treme e faz barulho. A água estava em movimento e não quer parar.

Eletricidade em bomba e solenoide faz exatamente a mesma coisa. Ao desligar, a corrente não quer parar e gera um pico de **centenas de volts** por um instante. Esse pico "salta" para fios vizinhos, como o cabo da boia — que corre justamente na mesma canaleta dos canos da solenoide e da bomba de recalque.

### Causa 2 — o zero que não é o mesmo zero (a mais provável)

Todo sinal elétrico é medido **em relação a um ponto de referência** — o "zero", o GND.

O ESP32 mede tudo em relação ao **pino GND dele**. Esse é o único zero que ele conhece.

A boia tem 2 fios: um vai no GPIO19, o outro no GND. E hoje esse segundo fio está ligado **direto no borne da fonte** — que é um zero *diferente*, em outro ponto do circuito.

Analogia: duas pessoas medem a altura da água. Uma está no chão firme; a outra, num barco. Quando o barco sobe e desce, a segunda jura que a água mudou de nível — mas foi o **ponto de referência dela** que se moveu.

O borne da fonte é o barco. É para lá que toda a corrente de retorno das bombas desagua. Quando a bomba de recalque parte, ela puxa uma corrente enorme por ali, e aquele ponto "afunda" cerca de **1 volt negativo** em relação ao GND do ESP32.

Aí, quando a boia fecha o contato, ela **liga o GPIO19 direto nesse ponto que afundou**. O pino recebe −1 V. O limite dele é −0,3 V.

Não é ruído vazando pelo ar. É conduzido, por fio, direto no pino. Cada partida de bomba dá mais um golpe, até o pino abrir de vez.

E o pior: **a boia é lida exatamente enquanto a solenoide e a bomba estão ligadas**. O sensor apanha justo na hora em que ele é crítico.

---

## O que já está certo no projeto

Nada disso precisa mudar:

- ✅ **8 diodos flyback**, um por canal com motor (`HARDWARE.md`)
- ✅ Instalados **na ponta do fio, junto ao motor** — que é a posição correta
- ✅ Capacitores de 1000 µF (saída 5 V, alimentando o ESP32) e 470 µF (entrada 12 V do MOSFET) amortecendo a fonte
- ✅ Capacitores de 10 µF / 22 µF previstos para desacoplamento **na ponta dos sensores**, nos conectores GX12 (`BOM.md` item 28)
- ✅ No software: filtro de mediana no ultrassônico e exigência de 5 leituras iguais na boia

> Atenção: esses capacitores seguram **queda de tensão**, que é um problema real e diferente. Eles não fazem nada contra a Causa 2, porque o problema lá não está na linha de energia e sim no caminho de retorno.

---

## O plano, sensor a sensor

Os dois sensores sofrem do mesmo problema de referência de GND, mas a proteção difere: a boia é liga/desliga, e o ultrassônico transmite dados.

| Sensor | GPIO | Tipo | Proteção |
|---|---|---|---|
| **Boia do reservatório** | 19 | Contato seco, 2 fios | Optoacoplador (opcional) |
| **Ultrassônico A02YYUW** | 34 | UART 9600, 4 fios | Desacoplamento + pull-up |

O sensor capacitivo XKC-Y25 que ocupava o GPIO4 **foi removido do projeto** — nunca chegou a ser instalado. A proteção de nível máximo passou a ser um **reed switch em série com o fio de sinal** entre o GPIO33 e a entrada IN7 do módulo MOSFET, que corta a bomba de recalque sem passar pelo firmware. Ver [`HARDWARE.md`](HARDWARE.md).

Mais duas medidas que valem para os dois sensores restantes: o **retorno de GND** e o **roteamento do cabo**.

---

## Passo 1 — o retorno de GND (vale para os dois)

**Não é acrescentar fio.** É mudar **onde o fio de GND encosta**.

Como está hoje:

```
SENSOR ──sinal──────────────► GPIO (ESP32)
       └─GND────────────────► borne V− da FONTE   ← ponto barulhento
```

Como deve ficar:

```
SENSOR ──sinal──────────────► GPIO (ESP32)
       └─GND────────────────► pino GND do ESP32   ← mesma referência
```

Com os dois fios na mesma referência, quando o sensor acionar, o GPIO vai ser ligado exatamente no zero que o ESP32 usa para medir. A diferença passa a ser zero **por construção** — não há como haver desvio, faça a bomba o que fizer.

É trocar um fio de borne. Custo zero, e ataca a raiz. **Faça isso mesmo instalando o optoacoplador**: nos dois casos o lado do ESP32 continua precisando da referência certa.

### Verifique também o GND do módulo MOSFET

Existe um segundo caminho de retorno que costuma passar despercebido, e ele encaixa ainda melhor com o histórico de GPIO queimado.

Os 8 canais chaveiam pelo **lado baixo**, então a corrente de retorno de *todas* as bombas passa por um único parafuso: o terminal GND do módulo. Se esse borne afrouxar ou oxidar, vários ampères vão procurar outro caminho de volta ao negativo da fonte — e o único disponível são os oito fios de sinal de 22 AWG, entrando direto nos GPIOs.

Isso explica o padrão melhor do que ruído irradiado: é **conduzido**, é da ordem de ampères, e acontece exatamente no instante em que uma bomba parte.

**A boa notícia:** o módulo em uso tem entradas **optoacopladas**. Se a barreira galvânica estiver íntegra, essa corrente simplesmente não tem caminho até o ESP32. Mas placas baratas frequentemente unem os dois GNDs por um jumper de solda ou uma trilha, e aí a barreira é decorativa.

Meça com o ohmímetro, módulo totalmente desconectado, entre o **GND do lado de controle** e o **GND do lado de potência**:

- **Aberto** — isolamento real. Mantenha assim: **não una esses dois GNDs em lugar nenhum**. Isso é mais valioso do que qualquer outra proteção nesta lista.
- **~0 Ω** — GNDs compartilhados. Procure o jumper de solda; cortá-lo devolve a proteção de verdade.

Independente do resultado, **reaperte o borne de GND do módulo** e todos os bornes de potência depois da primeira hora de operação. Foi um parafuso solto que causou o falso alarme diagnosticado neste projeto.

---

## Passo 2 — optoacoplador na boia (opcional)

Um optoacoplador transmite o sinal **por luz**, não por eletricidade. Dentro dele há um LED de um lado e um sensor de luz do outro, sem contato elétrico entre as partes.

Analogia: em vez de gritar através de uma parede, você pisca uma lanterna. A mensagem passa; o barulho não.

**O efeito:** o cabo comprido que vai até o sensor **deixa de tocar o ESP32**. Qualquer pico que ele capte descarrega no circuito de 12 V — que é robusto e aguenta — em vez de entrar num pino de 3,3 V.

Bônus: o sinal passa a viajar em 12 V no cabo. Um ruído de 2 V é fatal para 3,3 V e irrelevante para 12 V.

**O lado do ESP32:**

```
   3,3V ──[ 10 kΩ ]──┬────────── GPIO (4 ou 19)
                     │
            coletor ─┘
            emissor ──────────── GND do ESP32
                  PC817
```

---

### Sensor 1 — Boia do reservatório (GPIO19)

Contato seco, 2 fios. É só uma chave: fecha quando o reservatório enche.

```
   +12V ───[ BOIA ]───[ 1,5 kΩ ]───┬────►|────┬─── GND (12V)
                                   │  LED     │
                                   └────|◄────┘
                                     1N4148
```

O 1N4148 fica **em antiparalelo com o LED** (catodo do LED ligado ao anodo do diodo). Serve para proteger o LED de tensão reversa vinda do cabo.

Corrente no LED: 12 V ÷ 1,5 kΩ ≈ **8 mA**. Folgado para o PC817, que trabalha bem entre 5 e 20 mA.

---

### Sensor 2 — Ultrassônico A02YYUW (GPIO34)

**Este não leva optoacoplador**, e vale explicar por quê.

O PC817 é ótimo para liga/desliga, mas lento demais para transmitir **dados**. O A02YYUW fala UART a 9600 baud, e o PC817 deformaria os bits. Existem optoacopladores rápidos que dariam conta, mas como o sensor é ativo e precisa de alimentação descendo pelo mesmo cabo, isolar só a linha de dados resolveria metade do problema — complicação sem retorno proporcional.

E o problema dele é outro: **ele não queima, ele mente.**

Alimentado em 3,3 V, o A02YYUW opera no piso da faixa dele. Quando a bomba parte e a tensão cai por um instante, o sensor reinicia e devolve leitura corrompida — justamente durante o dreno e o recalque, quando o nível precisa estar correto.

| Proteção | Para quê |
|---|---|
| **Capacitor 100 µF + 100 nF no conector do sensor** | Reserva local de energia; segura a queda de tensão. É a mais importante |
| **Resistor de 10 kΩ do GPIO34 para 3,3 V** | O GPIO34 não tem resistor interno. Sem ele, cabo solto = leitura de lixo. Com ele, cabo solto = silêncio limpo, que o timeout de 2 s do firmware já trata |
| **Resistores de 470 Ω nas linhas de dados** | Limita corrente em caso de falha |
| **Filtro de ferrite no cabo** | Bloqueia ruído captado ao longo do caminho |

> ⚠️ **Nunca alimente o A02YYUW com 5 V.** O nível lógico da saída dele acompanha a alimentação, e 5 V no GPIO34 destrói o pino. Se for usar um regulador dedicado, tem que ser de **3,3 V**.

---

## A lógica do firmware não muda

Vale conferir, porque é o que garante que nada precisa ser reprogramado:

| Sensor | Evento | LED do opto | GPIO | O que o firmware espera |
|---|---|---|---|---|
| Boia | reservatório cheio | acende | LOW | `isReservoirFull()` → `== LOW` ✅ |

Bate com o código atual. **Nenhuma linha muda.**

---

## O que acontece se um cabo romper

| Sensor | Cabo rompido | Consequência |
|---|---|---|
| **Boia** | GPIO fica alto → "reservatório nunca enche" | O enchimento estoura o tempo limite e o TPA para com erro. **Falha segura** |
| **Ultrassônico** | sem dado → `_sensorsConnected` vira falso após 2 s | As verificações por sensor são puladas. O recalque passa a depender só do tempo limite |
| **Reed da trava** | contato aberto = mesmo efeito de nível máximo | A bomba de recalque não liga. **Falha segura** |

O reed merece destaque: por ser NA com o ímã presente no repouso, fio rompido, conector solto e ímã caído levam todos ao mesmo resultado — bomba desligada. A trava falhando também para a bomba.

O caso que sobra é **mecânico**: se a boia de EVA emperrar e não afastar o ímã, o contato fica fechado e a trava não age. É o único caminho que ainda leva a transbordo, e nenhum componente elétrico cobre isso — só folga generosa na guia e o teste da distância de liberação.

## Proteção na fonte do ruído — TVS nos canais das bombas

Aqui está a metade que falta da proteção contra o martelo elétrico.

O diodo flyback fica junto ao motor e protege muito bem *aquele lado*. Mas o cabo de 1,2 m entre a placa e o motor também guarda energia, e essa parte o diodo não alcança — ela descarrega **do lado da placa**.

Um **TVS** é um componente que fica inerte até a tensão passar do limite, e então "curto-circuita" o excesso para o GND. Como um ladrão de pressão.

**Onde:** um em cada canal de bomba de dreno, bomba de recalque e solenoide.

---

## Roteamento, cabo e vedação — custo zero

- Afastar os cabos de sensor pelo menos **15 cm** dos cabos de força. Se precisar cruzar, cruzar em ângulo de 90°.
- **Drip loop**: deixar uma barriga para baixo no cabo antes do conector, para a água escorrer e pingar no chão em vez de correr para dentro do plugue.
- Vedar os conectores com termo-retrátil com cola.
- **Filtro de ferrite** no cabo, junto do ESP32. Ajuda independente do tipo de cabo e não exige nada dele.

**Sobre o cabo — não precisa ser par trançado.**

Um cabo com 2 condutores dentro da mesma capa já entrega quase todo o benefício, porque o que importa é os dois fios percorrerem o mesmo caminho, colados. A torção acrescenta pouco além disso. Não vale trocar de cabo por causa disso.

**Sobre a haste metálica de enrijecimento**, se o cabo tiver uma:

Solta, sem ligação em ponta nenhuma, ela é uma antena deitada ao lado dos fios de sinal. Duas opções:

- **Ligue ao pino GND do ESP32, só nessa ponta.** A outra ponta fica solta, sem encostar em nada. Assim a haste vira blindagem e passa a trabalhar a favor.
- **Se a haste tiver qualquer contato com a água do reservatório, não ligue** — corrente de fuga na água causa eletrólise. Nesse caso deixe solta mesmo.

Ligar nas **duas** pontas é o pior dos mundos: cria loop de terra e piora. Uma ponta só.

---

## Antes de comprar: faça este teste

Confirma a Causa 2 em dois minutos, com multímetro comum.

1. Multímetro em **tensão contínua (DC)**, escala de 2 V ou menor
2. Ponta preta no **pino GND do ESP32**
3. Ponta vermelha no **barramento de terra** onde as bombas retornam
4. Ligue e desligue a bomba de recalque algumas vezes

**Leitura acima de 0,2 V confirma o diagnóstico.** E o pico real é bem maior do que isso — o multímetro é lento demais para mostrar, ele só enxerga a média.

### Teste extra: o GPIO19 morreu mesmo?

Com a boia desconectada, meça o GPIO19 contra o GND.

- **~3,3 V** → o pino está vivo; o problema foi no cabo ou no conector
- **Perto de 0 V ou instável** → o pino se foi

---

## Lista de compras

Tudo são componentes comuns e baratos de loja de eletrônica.

**Para a trava de nível máximo:**

| Item | Quantidade | Para quê |
|---|---|---|
| Reed switch NA + ímã | 1 | Corte por hardware da bomba de recalque |
| Resistor 1 kΩ | 1 | Pull-down do IN7, **lado do módulo** |
| Capacitor 100 nF | 1 | Filtro na entrada do IN7 |

**Para o optoacoplador da boia** (opcional):

| Item | Quantidade | Para quê |
|---|---|---|
| Optoacoplador PC817 | 1 | Isolar a boia |
| Resistor 1,5 kΩ | 1 | Limitar corrente do LED |
| Resistor 10 kΩ | 1 | Pull-up da saída do opto |
| Diodo 1N4148 | 1 | Proteger o LED de tensão reversa |

**Para o ultrassônico:**

| Item | Quantidade | Para quê |
|---|---|---|
| Capacitor 100 µF | 1 | Reserva de energia no conector do sensor |
| Capacitor 100 nF | 1 | Em paralelo com o de 100 µF, para ruído rápido |
| Resistor 10 kΩ | 1 | Pull-up do GPIO34 |
| Resistor 470 Ω | 2 | Linhas de dados |

**Comum:**

| Item | Quantidade | Para quê |
|---|---|---|
| TVS SMAJ18A ou P6KE18A | 3 | Canais de dreno, recalque e solenoide |
| Filtro de ferrite clip-on | 3 | Um por cabo de sensor |

---

## Perguntas rápidas

**Existe um pino mais resistente no ESP32?**
Não. Todos têm a mesma fragilidade e o mesmo limite. Trocar de pino não resolve — a prova é que já saímos do 5 para o 19 e o 19 morreu igual.

**Então preciso mudar a boia de pino?**
Não. Com o optoacoplador instalado, o GPIO19 volta a servir perfeitamente, sem mudar uma linha de código.

**Por que o ultrassônico não leva optoacoplador?**
Porque ele transmite dados, não liga/desliga. O PC817 é lento demais para 9600 baud e deformaria os bits. E o problema dele é outro: ele não queima, ele devolve leitura errada quando a tensão cai. A proteção dele é o capacitor no conector.

**E a proteção contra transbordo?**
É o reed switch, em série com o fio de sinal entre o GPIO33 e o IN7 do módulo MOSFET. Ele corta a bomba de recalque **sem passar pelo firmware**, então funciona com o ESP32 travado ou com o ultrassônico devolvendo leitura errada. É mais forte que qualquer verificação em código, e por isso o sensor capacitivo foi removido do projeto.

**E se o GPIO19 estiver realmente morto?**
O **GPIO35** é a melhor opção livre. Precisa de um resistor de 10 kΩ para 3,3 V, porque ele não tem resistor interno. Evite o GPIO5 e o GPIO0 — são pinos especiais usados na inicialização do chip.

**Dá para nunca mais queimar o ESP32?**
A estratégia mais segura é tirar os sensores do ESP32 e ligá-los num **expansor PCF8574** — um chip pequeno, em soquete, que conversa com o ESP32 por dois fios curtos dentro da caixa. Se queimar, você troca uma peça de poucos reais em vez da placa inteira.

---

## Ordem de execução

1. **Fazer o teste do multímetro** — confirma o diagnóstico antes de gastar
2. **Mover o retorno de GND dos sensores** para o pino GND do ESP32 (custo zero)
3. **Reed switch** na linha GPIO33→IN7, com o resistor de 1 kΩ do lado do módulo
4. **Capacitor no conector do ultrassônico** — resolve leitura errada durante as bombas
5. **Optoacoplador na boia** (GPIO19) — opcional, se o ajuste de GND não bastar
6. **TVS nos três canais** de bomba e solenoide
7. **Roteamento e vedação** dos cabos

Os passos 2 e 3 cobrem o essencial: o primeiro protege o pino, o segundo protege o aquário. O resto é margem.

> Nada disso exige mudança no firmware. A lógica dos sensores continua ativa em LOW nos dois casos.
