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
| 11 | 8-Channel MOSFET Module | IRF540N or equivalent, 12V | 1 |
| 12 | 470µF Electrolytic Cap | 16V, MOSFET input filter | 2 |
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
| 19 | Canister Filter | Controlled via AC relay | 1 |

---

## 📡 Sensors

| # | Component | Specification | Qty |
|---|---|---|---|
| 20 | JSN-SR04T | Waterproof ultrasonic, 5V | 1 |
| 21 | Capacitive Level Sensor | XKC-Y25-NPN, non-contact, 3.3–5V (GPIO 4) | 1 |
| 22 | Float Switch | Reservoir level (GPIO 35) | 1 |
| 23 | DS3231 | RTC module I2C (SDA 21 / SCL 22) | 1 |
| 24 | 0.96" OLED Display | SSD1306, I2C, 128×64 | 1 |

---

## 🛡️ Protection & Filtering

| # | Component | Specification | Qty |
|---|---|---|---|
| 25 | FR154 Diode | Fast Recovery, pump flyback | 7 |
| 26 | 1N5822 Diode | Schottky, solenoid flyback | 1 |
| 27 | 1kΩ Resistor | ¼W, ECHO voltage divider (R1) | 1 |
| 28 | 2kΩ Resistor | ¼W, ECHO voltage divider (R2) | 1 |
| 29 | 10µF / 22µF Capacitor | 50V, sensor decoupling | 2–4 |

---

## 🔌 Connectors & Accessories

| # | Component | Specification | Qty |
|---|---|---|---|
| 30 | RJ45 Female Connector | Panel mount, remote sensors | 2–4 |
| 31 | RJ45 Patch Cable | Cat5e, 1.2m (sensors) | 2–4 |
| 32 | KRE / Wago Terminal | Power connections | ~20 |
| 33 | Heat-Shrink Tubing | NTC and solder insulation | 1m |
| 34 | 22 AWG Wire | Signal / sensors | ~5m |
| 35 | 18 AWG Wire | Power / pumps | ~10m |

---

## 📊 Component Summary

| Category | Items |
|---|---|
| Electrolytic Capacitors | 6–10 |
| Diodes (flyback) | 8 |
| Resistors | 2 |
| Motors / Pumps | 7 |
| Sensors | 4 |
| Modules (ESP32, LM2596, MOSFET, Relay, RTC, OLED) | 6 |
