# Hardware Architecture Documentation: Aquarium Management System

## 📋 Overview

The system uses a **Defense in Depth** topology with noise filtering at the input (AC), stabilization on the main bus (DC), and inductive spike suppression at the edge (Actuators).

```
┌─────────────────────┐     ┌─────────────────────┐     ┌─────────────────────┐
│  LAYER 1: AC        │ ──► │  LAYER 2: DC        │ ──► │  LAYER 3: EDGE      │
│  Input/Protection   │     │  Bus/Logic          │     │  Peripherals/Suppr. │
│                     │     │                     │     │                     │
│  • AC Fuse          │     │  • 12.53V PSU       │     │  • Flyback Diodes   │
│  • NTC (Inrush)     │     │  • LM2596 → 5.1V    │     │  • Decoupling Caps  │
│  • EMI Filter (Y)   │     │  • Cap Bank         │     │  • Sensors (RJ45)   │
│  • Canister Relay   │     │  • 8-Ch MOSFET      │     │  • Pumps + Valve    │
└─────────────────────┘     └─────────────────────┘     └─────────────────────┘
```

---

## ⚡ Layer 1: AC Input & Protection (Infrastructure)

Responsible for filtering power line interference and protecting the PSU against inrush current surges at startup.

### Components

| Component | Function |
|---|---|
| **AC Fuse** (3A to 5A) | Protection against hard short circuits |
| **NTC 5D-11** | Inrush current limiter (in series with Live) |
| **NDF 222M** (Y Capacitors) | EMI filter (Phase–Ground and Neutral–Ground) |
| **SSR Relay Module 1CH** | Independent control for the Canister filter (AC) |

### Wiring Diagram

```
[ IEC C14 INLET ]
      │
      ├─── [ GROUND PIN ] ──────────────────────┬───► [ PSU G Terminal ]
      │         │                               │
      │         ├───[ NDF 222M ]─── (to L)      │
      │         └───[ NDF 222M ]─── (to N)      │
      │                                         │
      ├─── [ NEUTRAL PIN ] ─────────────────────┼───► [ PSU N Terminal ]
      │                                         └───► [ Relay COMMON ]
      │
      └─── [ LIVE PIN ] ─── [ AC FUSE ] ──────┐
                                               │
              ┌────────────────────────────────┘
              │                                │
        [ NTC 5D-11 ]                   [ RELAY INPUT ]
        (In series)                     (Live/NO pin)
              │                                │
        [ PSU L Terminal ]              [ CANISTER OUTLET ]
```

---

## 🔋 Layer 2: DC Bus & Logic (Distribution)

Responsible for converting power to logic levels and maintaining ESP32 stability during heavy load switching.

### Components

| Component | Function |
|---|---|
| **180W Switching PSU** | Adjusted to 12.53V |
| **T5AL250V Fuse** | Physical firewall for 8 pumps and sensors |
| **LM2596** | Step-down adjusted to 5.1V (ESP32 power) |
| **2× 470µF 16V** | In parallel at 12V MOSFET input |
| **4× 1000µF 10V** | In parallel at 5V output (near ESP32) |

### Wiring Diagram

```
[ PSU 12.53V ]
   │              │
 (V+)           (V─) ─────────────────────────┐ (STAR GND)
   │              │                            │
[T5A FUSE]        │                            │
   │              │                            │
   ├──────────────┼────────────┐               │
   │              │            │               │
[LM2596 IN+]  [LM2596 IN─]  [MOSFET VIN]  [MOSFET GND]
   │              │            │               │
(Output 5.1V)     │      (2× 470µF 16V)       │
   │              │            │               │
[ESP32 VIN]   [ESP32 GND]     │               │
   │              │      [ 8-CH MOSFET ]      │
(4× 1000µF 10V)  │            │               │
   │              └────────────┴───────────────┘
```

> [!IMPORTANT]
> **Star GND**: All negative references must return directly to the PSU V− terminal to avoid ground loops.

---

## 🌊 Layer 3: Peripherals & Suppression (Edge)

Responsible for mitigating inductive noise (Flyback) and stabilizing sensor readings on long cables (1.2m).

### Components

| Component | Application | Qty |
|---|---|---|
| **FR154 Diodes** (Fast Recovery) | Refill and Drain Pumps | 2 |
| **FR154 Diodes** (Fast Recovery) | Peristaltic Pumps (4 Fert + 1 Prime) | 5 |
| **1N5822 Diode** (Schottky) | Solenoid Valve (Channel 8) | 1 |
| **10µF / 22µF 50V Capacitors** | Sensor decoupling at cable ends (RJ45) | — |

> [!NOTE]
> **Total: 8 flyback diodes** — one per motor channel (7× FR154 + 1× 1N5822). All installed at the cable end, next to the motor.

### Wiring Diagram — Actuators

```
[ MOSFET CHANNEL ] ──────────── (1.2m Wire) ──────┬────────── (PUMP +)
                                                   │
                                             [ FLYBACK DIODE ]
                                             (Stripe on POS+)
                                                   │
[ BUS GND ] ─────────────────── (1.2m Wire) ───────┴────────── (PUMP ─)
```

> [!CAUTION]
> **Flyback Diodes**: Must be installed at the **cable end** (next to the motor) to prevent the 1.2m cable from radiating noise like an antenna.

### Wiring Diagram — Ultrasonic Sensor (JSN-SR04T)

The sensor operates at 5V, but the ESP32 supports a maximum of **3.3V** on GPIOs. The **ECHO** pin requires a resistive voltage divider to reduce 5V → 3.3V before entering GPIO19.

| Component | Function |
|---|---|
| **1kΩ Resistor** (R1) | In series between ECHO and GPIO19 |
| **2kΩ Resistor** (R2) | Pull-down between GPIO19 and GND |

**Formula:** `Vout = 5V × R2 / (R1 + R2) = 5 × 2k / 3k = 3.33V` ✅

```
                           ┌──────────────────┐
  ESP32 GPIO18 (TRIG) ─────┤ TRIG    JSN-SR04T│
                           │                  │
  ESP32 5V ────────────────┤ VCC              │
                           │                  │
  GND ─────────────────────┤ GND              │
                           │                  │
                           │ ECHO ────┐       │
                           └──────────┼───────┘
                                      │
                                 [ R1 = 1kΩ ]
                                      │
                    ESP32 GPIO19 ──────┤
                                      │
                                 [ R2 = 2kΩ ]
                                      │
                                    (GND)
```

> [!WARNING]
> **Never connect ECHO directly to the ESP32** without the voltage divider. The 5V ECHO signal can permanently damage the ESP32 GPIO.

### Wiring Diagram — Capacitive Level Sensor (XKC-Y25-NPN)

Non-contact sensor that detects liquid presence through glass/tube walls. NPN open-collector output, directly compatible with ESP32's 3.3V.

| Sensor Pin | Connection |
|---|---|
| **VCC** (red) | ESP32 3.3V |
| **GND** (black) | GND |
| **OUT** (yellow) | GPIO 4 (`INPUT_PULLUP`) |

```
  ESP32 3.3V ─────────── VCC (red)
                                            ┌────────────────┐
  ESP32 GPIO4 ─────────── OUT (yellow) ────┤ XKC-Y25-NPN    │
   (INPUT_PULLUP)                          │ (glued on the  │
                                            │  outside of    │
  GND ─────────────────── GND (black)  ────┤  glass/tube)   │
                                            └────────────────┘
```

> [!TIP]
> **No external resistor needed.** The ESP32's internal pull-up (~45kΩ) is sufficient for the NPN open-collector. The sensor is glued to the **outside** of the aquarium glass at the desired maximum level.

---

## 🛠️ Safe Implementation Notes

1. **Soldering Priority** — The NTC must be insulated with high-temperature heat-shrink tubing, as it heats up during operation.

2. **Capacitor Polarity** — Check the negative stripe on all electrolytics (especially the 1000µF 10V operating at 5.1V).

3. **Flyback Diodes** — Must be installed at the cable end (next to the motor) to prevent the 1.2m cable from radiating noise like an antenna.

4. **Star GND** — All negative references must return directly to the PSU V− terminal to avoid ground loops.
