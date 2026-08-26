# BOM — Bill of Materials

Complete component list for the IARA system.

---

## 🧠 Controller

| # | Component | Specification | Qty |
|---|---|---|---|
| 1 | ESP32 DevKit V1 | 38 pins, WROOM-32 | 1 |

---

## ⚡ Layer 1 — AC Input & Protection

| # | Component | Specification | Qty |
|---|---|---|---|
| 2 | IEC C14 Connector | Male panel-mount with fuse | 1 |
| 3 | AC Fuse | 3A–5A, 250V | 1 |
| 4 | NTC 5D-11 | Inrush current limiter, 5Ω cold | 1 |
| 5 | NDF 222M Capacitor | Y capacitor, Phase–Ground | 1 |
| 6 | NDF 222M Capacitor | Y capacitor, Neutral–Ground | 1 |
| 7 | SSR Relay Module 1CH | SSR, Canister filter control (AC) | 1 |

---

## 🔋 Layer 2 — DC Bus & Logic

| # | Component | Specification | Qty |
|---|---|---|---|
| 8 | 180W Switching PSU | 12V (adjusted to 12.53V) | 1 |
| 9 | T5AL250V Fuse | 5A, DC protection | 1 |
| 10 | LM2596 Module | Adjustable step-down (→ 5.1V) | 1 |
| 11 | 8-Channel MOSFET Module | Optocoupled inputs, **3.3 V version**, PWM on every channel, 12V. MOSFET part not identified — see the note below | 1 |
| 12 | 470µF Electrolytic Cap | 16V, MOSFET input filter | 1 |
| 13 | 1000µF Electrolytic Cap | 10V, 5V output filter (ESP32) | 4 |

---

## 🌊 Layer 3 — Actuators

| # | Component | Specification | Qty |
|---|---|---|---|
| 14 | 12V Peristaltic Pump | Fertilizers (CH1–CH4) | 4 |
| 15 | 12V Peristaltic Pump | Prime / dechlorinator (CH5) | 1 |
| 16 | 12V Submersible Pump | Drain / sewage (CH6) | 1 |
| 17 | 12V Submersible Pump | Refill / top-up (CH7) | 1 |
| 18 | 12V Solenoid Valve | Normally closed (CH8) | 1 |
| 18b| Mechanical float valve | Physically shuts the reservoir inlet — independent of any electronics | 1 |
| 19 | Canister Filter | Controlled via AC relay | 1 |

---

## 📡 Sensors

| # | Component | Specification | Qty |
|---|---|---|---|
| 20 | A02YYUW | Waterproof ultrasonic, UART, **3.3V** | 1 |
| 21 | Reed switch + magnet | Max-level interlock, NO, in series GPIO33→IN7 | 1 |
| 22 | Float Switch | Reservoir level (GPIO 19) | 1 |
| 23 | DS3231 | RTC module I2C (SDA 21 / SCL 22) | 1 |
| 24 | 1.8" TFT Display | ST7735, SPI, 128×160 | 1 |
| 24b| Push/Tactile Button | Display navigation (GPIO 0) | 1 |

---

## 🛡️ Protection & Filtering

| # | Component | Specification | Qty |
|---|---|---|---|
| 25 | FR154 Diode | Fast Recovery, pump flyback | 7 |
| 26 | 1N5822 Diode | Schottky, solenoid flyback | 1 |
| 27 | 10kΩ Resistor | ¼W, GPIO34 pull-up (A02YYUW TX) | 1 |
| 28 | 10µF / 22µF Capacitor | 50V, sensor decoupling | 2–4 |

---

## 🔌 Connectors & Accessories

| # | Component | Specification | Qty |
|---|---|---|---|
| 29 | GX12 Aviation Connector | Panel mount, for remote sensors | 2–4 |
| 30 | Shielded Multi-core Cable | 4-5 cores, solderable to GX12, 1.2m | 2–4 |
| 31 | KRE / Wago Terminal | Power connections | ~20 |
| 32 | Heat-Shrink Tubing | NTC and solder insulation | 1m |
| 33 | 22 AWG Wire | Signal / sensors | ~5m |
| 34 | 18 AWG Wire | Power / pumps | ~10m |


---

## 📝 Notes on the MOSFET module

The module in use is an **8-channel board with optocoupled inputs, 3.3 V version**, with PWM on every channel. Listing: AliExpress item `1005010404945969`.

**The MOSFET part number is not confirmed.** The BOM said `IRF540N or equivalent`, which was a guess — and a poor one: the IRF540N's V<sub>GS(th)</sub> is 2.0–4.0 V and its R<sub>DS(on)</sub> is specified at V<sub>GS</sub> = 10 V. A real IRF540N on a 3.3 V gate would sit in the linear region, dissipating watts in a TO-220 with no heatsink inside a sealed box. The module works, so it is probably **not** an IRF540N. Read the marking off the package and record it here — whoever replaces a blown channel will buy whatever this file says.

### Why this module needs no input pull-downs

On a direct-drive board the IN terminal goes to the MOSFET **gate**. A gate is capacitive and has no current threshold: stray voltage accumulates charge and the transistor starts conducting. That is why such boards need a 10 kΩ pull-down on every input — without one, ESP32 GPIO13 and GPIO14, which come out of reset with ~45 kΩ internal pull-ups, could run pumps during the ~300 ms bootloader window, on every reset and every flash.

Here the IN terminal drives an **LED** inside the optocoupler, and an LED is a threshold device — it needs 5 to 20 mA to light. The internal pull-up can supply at most `(3.3 − 1.2) / 45k ≈ 47 µA`, two to three orders of magnitude short. **The optocoupler already does the pull-down's job.**

Confirm with a multimeter in **diode mode** between IN and the control-side GND: ~1.0–1.3 V one way and open the other means it is an LED. If it reads a resistance both ways, the board is direct-drive and the pull-downs become mandatory again.

### Check whether the isolation is real

Cheap boards advertised as optocoupled often bridge the two grounds with a solder jumper or a trace, which makes the barrier cosmetic.

That matters a great deal here. All eight channels switch low-side, so every pump's return current flows through the module's GND terminal. If that screw goes high-resistance, several amps look for another path back to the supply negative — and the only one available is the signal wires, straight into the GPIOs. It is the explanation that best fits the burned GPIO5 and GPIO19: conducted, amps, and happening exactly when a pump starts.

**With the galvanic barrier intact that current has no path to the ESP32.** Measure with an ohmmeter, module disconnected, between the control-side GND and the load-side GND:

- **Open** — real isolation. Keep it that way: do not bond those two grounds anywhere.
- **~0 Ω** — shared grounds. Look for the solder jumper; cutting it restores the protection.

### PWM through an optocoupler

`FertManager` runs LEDC at 5 kHz on channels 1 to 5. A common optocoupler (PC817 and similar) turns off considerably more slowly than it turns on, so the duty reaching the gate is not exactly the one commanded, especially at low values. Flow calibration absorbs this because it is measured empirically — but if a channel will not turn at low PWM, that is the reason, not a weak pump.

---

## 📊 Component Summary

| Category | Items |
|---|---|
| Electrolytic Capacitors | 6–10 |
| Diodes (flyback) | 8 |
| Resistors | 1 |
| Motors / Pumps | 7 |
| Sensors | 4 |
| Modules (ESP32, LM2596, MOSFET, Relay, RTC, TFT) | 6 |
