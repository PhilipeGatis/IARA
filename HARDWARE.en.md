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
│  • EMI Filter (Y)   │     │  • Cap Bank         │     │  • Sensors (GX12)   │
│  • Canister Relay   │     │  • 8-Ch MOSFET      │     │  • Pumps + Valve    │
└─────────────────────┘     └─────────────────────┘     └─────────────────────┘
```

---

## ⚡ Layer 1: AC Input & Protection (Infrastructure)

The power input architecture focuses on simplicity and maximum electrical safety, delegating heavy filtering (like EMI filter and NTC/Inrush limits) to the professional circuits already built into the Switching Power Supply.

### Components

| Component | Function |
|---|---|
| **AC Glass Fuse** (3A to 5A) | Mandatory item: Protection against hard short circuits and fire risks |
| **SSR Relay Module 1CH** | Independent control via ESP32 for the Canister filter (AC) |

### Wiring Diagram

```text
[ IEC C14 INLET ]
      │
      ├─── [ GROUND PIN (Green) ] ────────┬───────► [ PSU GROUND (G) Terminal ]
      │                                   │
      │                                   └───────► [ Canister Outlet GROUND ]
      │         
      ├─── [ NEUTRAL PIN (Blue) ] ────────┬───────► [ PSU NEUTRAL (N) Terminal ]
      │                                   │
      │                                   └───────► [ Canister Outlet NEUTRAL ]
      │
      └─── [ LIVE PIN (Brown) ] ─── [ FUSE ] ─────┐
                                                  │
             ┌────────────────────────────────────┘
             │                                  
             ├───────────────────► [ PSU LIVE (L) Terminal ]
             │
             └───► [ SSR Relay Screw 1 ] ── SWITCH ──► [ SSR Relay Screw 2 ] ──► [ Canister Outlet LIVE ]
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
| **1× 470µF 16V** | In parallel at 12V MOSFET input (Prevents voltage dips on pump startup) |
| **4× 1000µF 10V** | In parallel at 5V output (Acts as a "UPS" for ESP32 to withstand line fluctuations and spikes) |

### Wiring Diagram

```text
[ PSU 12.53V ]
   │              │
 (V+)           (V─) ──────────────────────────────┐ (STAR GND)
   │              │                                │
[T5A FUSE]        │                                │
   │              │                                │
   ├──────────────┼────────────────┐               │
   │              │                │               │
[LM2596 IN+]  [LM2596 IN─]    [MOSFET VIN]   [MOSFET GND]
   │              │                │               │
(Output 5.1V)     │         (1× 470µF 16V)         │
   │              │                │               │
(4× 1000µF 10V)   │                │               │
   │              │                │               │
[ESP32 VIN]   [ESP32 GND]          │               │
   │              │         [ 8-CH MOSFET ]        │
   │              └────────────────┴───────────────┘
```

> [!IMPORTANT]
> **POWER GND — star at the PSU**: **Power** negative references (MOSFET module, LM2596, pumps, solenoid) must return directly to the PSU V− terminal, to avoid ground loops.

> [!CAUTION]
> **SIGNAL GND — the opposite rule**: The return (GND) of the **sensors** — float switch and ultrasonic — must go to the **ESP32 GND pin**, and **never** straight to the PSU terminal.
>
> The ESP32 measures every input relative to *its own* GND pin. A sensor referenced to the PSU terminal hands the GPIO the potential difference between those two points. During pump startup that difference exceeds **1 V negative** — far beyond the ESP32's −0.3 V absolute minimum — and the damage accumulates until the pin fails open.
>
> This is exactly how GPIO5 and GPIO19 burned out. See [`PROTECAO_ELETRICA.md`](PROTECAO_ELETRICA.md) (Portuguese only).

---

## 🌊 Layer 3: Peripherals & Suppression (Edge)

Responsible for mitigating inductive noise (Flyback) and stabilizing sensor readings on long cables (1.2m).

### Components

| Component | Application | Qty |
|---|---|---|
| **FR154 Diodes** (Fast Recovery) | Refill and Drain Pumps | 2 |
| **FR154 Diodes** (Fast Recovery) | Peristaltic Pumps (4 Fert + 1 Prime) | 5 |
| **1N5822 Diode** (Schottky) | Solenoid Valve (Channel 8) | 1 |
| **10µF / 22µF 50V Capacitors** | Sensor decoupling at cable ends (GX12) | — |

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

### Wiring Diagram — Ultrasonic Sensor (A02YYUW, UART)

Waterproof ultrasonic sensor that reports distance over **UART**. No TRIG/ECHO, and no voltage divider needed. It transmits on its own without being commanded: one frame every ~100 ms.

| Wire | Function | Connection | Note |
|---|---|---|---|
| **VCC** (red) | Supply | ESP32 3.3V | ⚠️ Never 5V — see warning below |
| **GND** (black) | Return | **ESP32 GND pin** | ⚠️ Never the PSU terminal |
| **TX** | Sensor data output | GPIO34 (RX2) | Needs a 10kΩ pull-up |
| **RX** | Control input | 3.3V | Unused by the firmware — see note |

> The colors of the two data wires vary between batches; only red and black are consistent. To identify the output, power the sensor and measure: **TX** idles at ~3.3V and shows activity; RX stays inert.

```
   ESP32 3.3V ──────┬──────────────────►  VCC  (red)
                    │
                [ 10 kΩ ]                        A02YYUW
                    │                           (waterproof)
   ESP32 GPIO34 ────┴──────────────────►  TX   (data output)
    (RX2)

   ESP32 3.3V ─────────────────────────►  RX   (control, unused)

   ESP32 GND ──────────────────────────►  GND  (black)
    (board pin, never the PSU terminal)
```

> [!WARNING]
> **Power it at 3.3V, never 5V.** The A02YYUW accepts 3.3–5V, but its output logic level follows VCC. At 5V the TX line would drive 5V into GPIO34 and permanently damage it. Running at 3.3V is precisely what makes the direct connection possible without a divider.

> [!IMPORTANT]
> **10kΩ pull-up between GPIO34 and 3.3V.** GPIO34 is input-only and has **no internal pull-up**. Without the external resistor, a loose cable or unpowered sensor leaves the line floating and the firmware reads noise as data. With the pull-up the line idles high — the UART idle state — and missing data becomes a clean failure, caught by the 2 s timeout in `SafetyWatchdog::readUltrasonic()`.

> [!NOTE]
> **The control wire (sensor RX) is unused.** Nothing in the firmware writes to `Serial2`; the sensor transmits on its own. That wire should go to **3.3V** to keep continuous-transmission mode — what must be avoided is leaving it floating.
>
> That wire sits on **GPIO18**. Nothing in the firmware writes to `Serial2`, but the pin stays declared as the UART TX precisely so the line is held idle-high, which is what the sensor needs to stay in continuous mode.

> [!TIP]
> **Protocol:** 9600 baud, 8N1. 4-byte frame — `0xFF`, `DataH`, `DataL`, `Checksum`, where distance is in **millimeters** (`(DataH << 8) | DataL`) and the checksum is `(0xFF + DataH + DataL) & 0xFF`. The firmware discards frames with an invalid checksum and applies a 5-sample median filter.

> [!CAUTION]
> **Local decoupling.** At 3.3V the sensor runs at the bottom of its range, which makes it sensitive to the voltage sag when pumps start — the symptom is corrupted readings during exactly the drain and refill steps. Fit **100 µF + 100 nF at the sensor connector**, not on the board. See [`PROTECAO_ELETRICA.md`](PROTECAO_ELETRICA.md) (Portuguese only).

### Wiring Diagram — Max-Level Interlock (Reed Switch)

#### Why the interlock is on the refill pump, not the solenoid

At first glance the solenoid looks like the obvious target: it is fed from the water mains and on its own would flood the house. But it **already has a mechanical float valve** in the reservoir — a valve that physically shuts the flow when the water rises, with no electronics involved. Same idea as a cistern ballcock.

The refill pump has no such thing and cannot have one: it pushes water *into* the aquarium, and no mechanical valve on the discharge side stops a pump. If it does not stop, the water goes on the floor. So the reed protects the channel that had nothing.

Water interlocks, strongest first:

| Actuator | Physical stop | Electrical stop | Firmware |
|---|---|---|---|
| Solenoid (CH8) | **Mechanical float valve in the reservoir** | — | Float switch on GPIO19 + 8-minute timeout |
| Refill pump (CH7) | — (not possible) | **Reed in series with the pump's +12 V** | Ultrasonic + flow-sized timeout |

The max-level cutoff **does not go through the firmware**. It is a reed switch in series with the refill pump's **+12 V** wire. If the water reaches max level the contact opens, the pump loses power and stops — even with the ESP32 hung, rebooting, the ultrasonic silent, **or the MOSFET shorted**.

That last case is why the reed sits in the power line and not in the signal. The MOSFETs on these modules fail shorted drain to source, and in that failure cutting the gate does nothing: the pump keeps running, and neither the firmware nor a reed on the signal can stop it.

Assembly: **NO** (normally open) reed with a magnet held nearby by an EVA float, out of the water. At normal level the magnet is present and the contact closed. As the water rises, the float carries the magnet away and the contact opens.

| Situation | Magnet | Reed | 12 V | Pump |
|---|---|---|---|---|
| Normal level | present | closed | passes | may run |
| Max level | away | open | broken | **stopped** |
| Broken wire | — | — | broken | **stopped** |
| MOSFET shorted | away | open | broken | **stopped** |

```
  +12V ──────[ REED ]────────── refill pump (+)

  pump (−) ──────────────────── OUT− of the MOSFET module (channel 7)

  ESP32 GPIO33 ──────────────── IN7 (MOSFET module, channel 7)
                                 │
                             [ 1 kΩ ]
                                 │
                             [ 100 nF ]
                                 │
                            module GND
```

> [!CAUTION]
> **The 1 kΩ resistor still applies, now for a different reason.** With the reed out of the signal wire that line is never open in operation — but GPIO33 floats during the ESP32's boot and reset. The MOSFET input works by stored charge: without the resistor, the floating gate holds its charge and can keep the MOSFET partially conducting.
>
> Before building, measure the resistance between IN7 and the module GND. If it already reads between 10 kΩ and 100 kΩ, the module has its own pull-down and the external resistor is unnecessary.

> [!WARNING]
> **The reed may sit in the power line only because of this pump.** Reed contacts are rated for 0.5–1 A and 10 W. The pump in this project is 12 V, 5 W, 400 mA maximum, which puts 4.8 W on the contact — a little over 2× margin.
>
> Swap in a larger pump and that stops being true. With several amps at startup the contact arcs and eventually welds closed, a silent failure that voids the interlock. In that case the reed drives a **relay coil** instead (tens of mA), and the relay contact, rated for the load, carries the pump.

> [!CAUTION]
> **What hurts the contact is closing, not opening.** The pump is brushless with an epoxy-sealed board, meaning an input capacitor. A discharged capacitor looks like a short the instant the contact closes, and that surge is what welds reeds. Breaking 400 mA is uneventful.
>
> In practice the risk is small: after a cutoff the pump stops on the timeout, and the water only falls again through evaporation, hours later, with everything already off. The bad case is the float bobbing on the surface with the pump running, chattering the contact under load. During the continuity test, confirm the transition is **clean**, with no chatter near the trip point.

> [!TIP]
> **Test the release distance before sealing the assembly.** A reed has hysteresis: it closes at one distance and only opens at a larger one. With the multimeter on continuity, pull the magnet away until the contact opens and note the distance — the float's travel must be 2 to 3 times that value. Also confirm it does not close again at any intermediate position along the travel.

> [!IMPORTANT]
> **The firmware cannot see this cutoff.** When the reed acts, the state machine stays in `REFILLING` until the timeout expires and ends in `ERROR`. In normal operation the ultrasonic reaches the setpoint first, so this does not happen — the reed is a safety net, not a routine stop.
>
> **GPIO4**, previously the XKC-Y25 capacitive sensor (never installed, dropped from the project), is free. Wiring a second reed to it would give the firmware that visibility back, but requires a code change.

#### Holding the magnet and the reed: the printed seesaw

The part above describes the circuit. The mechanics that carry it are in [`3d_models/reed_level_seesaw.scad`](3d_models/reed_level_seesaw.scad).

It is a seesaw resting on the glass rim. The inner arm ends in a tray where a block of foam is glued; the outer arm carries the 6 × 2 mm magnet **lying flat** on a horizontal pad. The reed lives inside a 5 mm acrylic tube that threads through two fingers growing out of the pivot ears. No part of the circuit touches the water.

| | |
|---|---|
| Glass | 4.9 mm |
| Magnet travel | 23.3 mm |
| Field at the reed — closed / open | ~43 / ~2.4 gauss |
| Highest point above the rim | 45.5 mm |
| Printed parts | 4 |

> [!IMPORTANT]
> **The reed is not above the magnet, and that is not an oversight.** A magnet lying flat throws a vertical field straight above itself, and a reed lying flat cannot see a vertical field — in that position it never closes. The field only turns horizontal on the 54.7° line from the magnet's axis, which is why the contacts sit 9.9 mm to the side and 7 mm above its face. The two dimensions are coupled (`dy = 1.41 × dz`): moving one alone takes the reed off that line. That is why the height is a round hole with no adjustment — the tube is what you adjust, sliding it through the two fingers.

> [!TIP]
> **The magnet arm is what buys release margin, not the float arm.** Magnet travel is `2 × arm_out × sin(15°)`, and the field at the open stop falls with the cube of the distance. Making the float arm longer does the opposite of what it looks like: it is the denominator of the gain, so it trades travel for torque.

The angle between the two arms is chosen on a 12-tooth face coupling, in 30 degree steps. The step is coarse on purpose: the trip level is set by the counterweight, whose range is nearly the whole height of the float. The angle only has to put the float near the surface.

Calibration is the same hysteresis test as the tip above, with one extra step: slide the tube until the contact closes with the lever at the upper stop, and only then confirm it opens at the lower one without closing again anywhere in between.

Printing: four parts in PETG or ASA — PLA absorbs moisture and warps near water. The STLs come already rotated and seated at z = 0, and `reed_seesaw_mesa.stl` carries all four on a 138 × 86 mm plate. Only `bracket` needs support, in two open pockets.

### Wiring Diagram — TFT ST7735 Display (SPI)

1.8" color display, 128×160 pixels. Operates at **3.3V** — incompatible with 5V. Uses software SPI (bit-banging) on custom pins.

| Display Pin | ESP32 Connection | GPIO | Notes |
|---|---|---|---|
| **VCC** | 3.3V | — | ⚠️ Never use 5V |
| **GND** | GND | — | |
| **CS** | D15 | GPIO15 | Chip Select |
| **RESET** | EN pin | — | Hardware reset shared with ESP32 |
| **A0 (DC)** | TX2 | GPIO17 | Data/Command |
| **SDA (MOSI)** | D23 | GPIO23 | SPI Data |
| **SCK** | RX2 | GPIO16 | SPI Clock |
| **LED** | 3.3V | — | Fixed backlight (no free GPIO available) |

```
ESP32 3.3V  ────►  VCC
ESP32 GND   ────►  GND
ESP32 D15   ────►  CS
ESP32 EN    ────►  RESET
ESP32 TX2   ────►  A0 (DC)
ESP32 D23   ────►  SDA (MOSI)
ESP32 RX2   ────►  SCK
ESP32 3.3V  ────►  LED (fixed backlight)
```

> [!WARNING]
> The display **RESET** pin must connect to the ESP32 **EN** pin (not a GPIO). This ensures the display resets together with the microcontroller. With EN HIGH (normal operation), the display works normally.

> [!NOTE]
> On the ESP32 DevKit V1 breakout board, **D16** and **D17** are labeled **RX2** and **TX2** respectively. Use your terminal block labels to identify the correct positions.

---

### Wiring Diagram — Reservoir Float Switch

Horizontal float switch that signals "reservoir full". It closes the solenoid during the TPA `FILLING_RESERVOIR` state. The firmware requires 5 consecutive "full" reads (~250 ms debounce) before accepting the signal.

| Pin | Connection | GPIO | Configuration |
|---|---|---|---|
| **Terminal 1** | GPIO19 | GPIO19 | `INPUT_PULLUP` — active LOW |
| **Terminal 2** | **ESP32 GND pin** | — | ⚠️ Never the PSU terminal |

```
ESP32 GPIO19 ─────── [ FLOAT ] ─────── ESP32 GND (board pin)
         INPUT_PULLUP, active LOW (full = LOW)
         Both wires leave together, in the same cable, the whole way
```

> [!NOTE]
> The float switch originally used GPIO5, but that pin is an ESP32 strapping pin and suffered interference (~2.5 V at rest). It was moved to GPIO19, which previously drove the panel navigation button.

> [!IMPORTANT]
> **The panel navigation button is on GPIO5** (`PIN_BTN` in `include/Config.h`). It lost GPIO19 to the float switch. GPIO5 is an ESP32 strapping pin, so it must not be held down through power-on; a momentary press at any other time is harmless.

---

## 🛠️ Safe Implementation Notes

1. **AC Connections** — Insulate all 110V/220V solder joints and connections with heat-shrink tubing for maximum safety.

2. **Capacitor Polarity** — Check the negative stripe on all electrolytics (especially the 1000µF 10V operating at 5.1V).

3. **Flyback Diodes** — Must be installed at the cable end (next to the motor) to prevent the 1.2m cable from radiating noise like an antenna.

4. **Star GND** — All negative references must return directly to the PSU V− terminal to avoid ground loops.

5. **Siphon Effect (Hydraulics)** — To prevent continuous gravity draining of the aquarium after the drain pump is turned off, wire a solenoid valve in parallel with the drain pump (on the same MOSFET channel, ensuring a flyback diode for each) or make a breather hole in the hose.
