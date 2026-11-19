"""Circuit definition for the IARA carrier board.

Single source of truth for parts, values, footprints and nets. gen_sch.py turns
it into a KiCad schematic; gen_pcb.py places the same parts on the board.

Pin keys are pin numbers (strings) or pin names; the value is the net name.
A value of None marks the pin as intentionally unconnected.
"""

R0603 = "Resistor_SMD:R_0603_1608Metric"
C0603 = "Capacitor_SMD:C_0603_1608Metric"
C1206 = "Capacitor_SMD:C_1206_3216Metric"
CP10 = "Capacitor_SMD:CP_Elec_10x10"
CP8 = "Capacitor_SMD:CP_Elec_8x10"
SOT23 = "Package_TO_SOT_SMD:SOT-23"
TERM35 = "TerminalBlock_Phoenix:TerminalBlock_Phoenix_PT-1,5-2-3.5-H_1x02_P3.50mm_Horizontal"
TERM35_3 = "TerminalBlock_Phoenix:TerminalBlock_Phoenix_PT-1,5-3-3.5-H_1x03_P3.50mm_Horizontal"
TERM508 = "TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal"
HDR = "Connector_PinHeader_2.54mm:PinHeader_1x%02d_P2.54mm_Vertical"

PARTS = []


def part(prefix, lib_id, value, footprint, pins, block, lcsc="", note="", bom=True):
    PARTS.append(dict(prefix=prefix, lib_id=lib_id, value=value, footprint=footprint,
                      pins=pins, block=block, lcsc=lcsc, note=note, bom=bom))


def R(value, a, b, block, note=""):
    part("R", "Device:R", value, R0603, {"1": a, "2": b}, block, note=note)


def C(value, a, b, block, fp=C0603, note=""):
    part("C", "Device:C", value, fp, {"1": a, "2": b}, block, note=note)


def CP(value, pos, neg, block, fp=CP10, note=""):
    part("C", "Device:C_Polarized", value, fp, {"1": pos, "2": neg}, block, note=note)


def clamp(net, block):
    """BAT54S: COM on the signal, one diode to GND, the other to +3V3."""
    part("D", "Diode:BAT54S", "BAT54S", SOT23, {"A": "GND", "K": "+3V3", "COM": net}, block)


# ---------------------------------------------------------------- power input
B = "power"
part("J", "Connector:Screw_Terminal_01x02", "12V IN", TERM508,
     {"1": "+12V", "2": "GND"}, B, note="Vem do fusivel T5A que ja existe na caixa")
part("D", "Device:D_Zener", "SMBJ15A", "Diode_SMD:D_SMB", {"K": "+12V", "A": "GND"}, B,
     note="Surto e polaridade invertida: conduz e abre o fusivel T5A da caixa")
CP("470uF 25V", "+12V", "GND", B, fp="Capacitor_SMD:CP_Elec_10x10.5")
C("10uF 50V", "+12V", "GND", B, fp=C1206)
C("100nF", "+12V", "GND", B)

# 12 V -> 5 V buck
part("U", "Regulator_Switching:AP63205WU", "AP63205WU", "Package_TO_SOT_SMD:TSOT-23-6",
     {"IN": "+12V", "EN": "+12V", "GND": "GND", "SW": "SW5", "BST": "BST5", "FB": "+5V"}, B)
C("100nF", "BST5", "SW5", B, note="Bootstrap")
part("L", "Device:L", "4.7uH", "Inductor_SMD:L_Changjiang_FXL0630", {"1": "SW5", "2": "+5V"}, B)
C("22uF 25V", "+5V", "GND", B, fp=C1206)
C("22uF 25V", "+5V", "GND", B, fp=C1206)
CP("470uF 16V", "+5V", "GND", B, fp="Capacitor_SMD:CP_Elec_6.3x7.7")
part("D", "Diode:SS34", "SS34", "Diode_SMD:D_SMA", {"A": "+5V", "K": "+5V_DK"}, B,
     note="Impede que o 5 V da placa alimente a USB do PC")

# test points (bare pads, not in the BOM)
for net in ("+12V", "+5V", "+3V3", "GND"):
    part("TP", "Connector:TestPoint", net, "TestPoint:TestPoint_THTPad_D1.5mm_Drill0.7mm",
         {"1": net}, B, bom=False)

# ------------------------------------------------------------ DevKit sockets
B = "devkit"
J1 = ["+3V3", "+3V3", "EN", "CH1_CTL", "CH2_CTL", "CH3_CTL", "CH4_CTL", "CH5_CTL",
      "CH6_CTL", "CH7_CTL", "CH8_CTL", "CAN_CTL", None, None, "BTN", "TFT_CS",
      "TFT_MOSI_MCU", "TFT_SCK_MCU", "TFT_DC", "HB", "+5V_DK", "GND"]
J3 = ["GND", None, None, "US_RX", "US_TX", "FLOAT", "REED_SENSE", "I2C_SCL", "I2C_SDA",
      None, None, None, None, None, None, "CH9_B_CTL", "CH9_A_CTL", "PUMP_SENSE", None, None, "GND", "GND"]
part("J", "Connector_Generic:Conn_01x22", "DevKit J1", "Connector_PinSocket_2.54mm:PinSocket_1x22_P2.54mm_Vertical",
     {str(i + 1): n for i, n in enumerate(J1)}, B,
     note="ESP32-S3-DevKitC-1, lado J1: 3V3 3V3 RST 4 5 6 7 15 16 17 18 8 3 46 9 10 11 12 13 14 5V GND")
part("J", "Connector_Generic:Conn_01x22", "DevKit J3", "Connector_PinSocket_2.54mm:PinSocket_1x22_P2.54mm_Vertical",
     {str(i + 1): n for i, n in enumerate(J3)}, B,
     note="ESP32-S3-DevKitC-1, lado J3: GND TX RX 1 2 42 41 40 39 38 37 36 35 0 45 48 47 21 20 19 GND GND")
CP("1000uF 10V", "+3V3", "GND", B, fp="Capacitor_SMD:CP_Elec_8x10.5", note="Colado no soquete: correcao do brownout")
C("100nF", "+3V3", "GND", B)

# ---------------------------------------------------------------- channels
CHANNELS = [
    (1, "FERT1", "0.75A"), (2, "FERT2", "0.75A"), (3, "FERT3", "0.75A"),
    (4, "FERT4", "0.75A"), (5, "PRIME", "0.75A"), (6, "DRAIN", "0.75A"),
    (7, "REFILL", "0.5A"), (8, "SOLENOID", "0.75A"),
]
for n, name, hold in CHANNELS:
    B = "ch%d" % n
    pos = "CH%d_POS" % n
    neg = "CH%d_NEG" % n
    ptc_out = "REED_A" if n == 7 else pos
    R("100R", "CH%d_CTL" % n, "CH%d_G" % n, B)
    R("10k", "CH%d_G" % n, "GND", B, note="Pull-down: canal desligado no boot")
    part("Q", "Transistor_FET:AO3400A", "AO3400A", SOT23,
         {"G": "CH%d_G" % n, "S": "GND", "D": neg}, B)
    part("F", "Device:Polyfuse", "PTC %s hold" % hold, "Fuse:Fuse_1812_4532Metric",
         {"1": "+12V_PUMP", "2": ptc_out}, B)
    part("D", "Diode:SS34", "SS34", "Diode_SMD:D_SMA", {"K": pos, "A": neg}, B,
         note="Flyback na placa (o da ponta do fio continua)")
    R("4.7k", pos, "CH%d_LED" % n, B)
    part("D", "Device:LED", "LED green", "LED_SMD:LED_0805_2012Metric",
         {"A": "CH%d_LED" % n, "K": neg}, B, note="Aceso = canal energizado")
    part("J", "Connector:Screw_Terminal_01x02", "CH%d %s" % (n, name), TERM35,
         {"1": pos, "2": neg}, B, note="1 = +12V, 2 = retorno (MOSFET)")

# reed: the only path from the CH7 PTC to the refill pump's +12 V
B = "ch7"
part("J", "Connector:Screw_Terminal_01x02", "REED", TERM35,
     {"1": "REED_A", "2": "CH7_POS"}, B, note="Reed NA em serie com o +12V do recalque")
R("10k", "CH7_POS", "REED_DIV", B, note="Divisor 12,5 V -> 3,1 V")
R("3.3k", "REED_DIV", "GND", B)
C("100nF", "REED_DIV", "GND", B)
clamp("REED_DIV", B)
R("1k", "REED_DIV", "REED_SENSE", B)

# ---------------------------------------------------------------- heartbeat
# The pumps' +12 V exists only while the firmware keeps toggling GPIO14. The
# coupling capacitor passes edges, never a level: a pin stuck high, stuck low,
# in reset or in the bootloader leaves HB_DC to bleed down through 470k (tau 0.47 s),
# the AO3400A turns off and the 100k pulls the P-MOSFET gate to its source.
B = "hb"
R("100R", "HB", "HB_R", B, note="GPIO14: onda quadrada vinda do loop")
C("1uF", "HB_R", "HB_AC", B, note="So passa borda, nunca nivel parado")
part("D", "Diode:BAT54S", "BAT54S", SOT23, {"A": "GND", "COM": "HB_AC", "K": "HB_DC"}, B,
     note="Bomba de carga: retifica as bordas")
C("1uF", "HB_DC", "GND", B)
R("470k", "HB_DC", "GND", B, note="Sem borda por ~1 s: corta o 12V das bombas")
part("Q", "Transistor_FET:AO3400A", "AO3400A", SOT23, {"G": "HB_DC", "S": "GND", "D": "HB_PULL"}, B)
R("47k", "HB_PULL", "PUMP_G", B)
R("100k", "+12V", "PUMP_G", B, note="Vgs = -8,5 V ligado, 0 V desligado")
part("Q", "Transistor_FET:IRF7404", "AO4407A", "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm",
     {"S": "+12V", "G": "PUMP_G", "D": "+12V_SW"}, B,
     note="Chave geral do 12V dos 8 canais (P-MOSFET 30V 12A)")
C("10uF 50V", "+12V_SW", "GND", B, fp=C1206)
# total pump current: every channel's 12 V passes this shunt. Current with
# all channels off means a shorted MOSFET; the firmware then stops the
# heartbeat and the P-MOSFET above cuts the whole rail.
part("R", "Device:R", "0.05R 1W", "Resistor_SMD:R_2512_6332Metric",
     {"1": "+12V_SW", "2": "+12V_PUMP"}, B, note="Shunt: 320 mV = 6,4 A, resolucao 0,2 mA")
part("U", "Sensor_Energy:INA219AxD", "INA219AIDR", "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm",
     {"A1": "GND", "A0": "+3V3", "SDA": "I2C_SDA", "SCL": "I2C_SCL", "VS": "+3V3", "GND": "GND",
      "IN+": "+12V_SW", "IN-": "+12V_PUMP"}, B,
     note="I2C 0x41: corrente total das bombas + tensao do 12V")
C("100nF", "+3V3", "GND", B)
R("4.7k", "+12V_PUMP", "PUMP_LED", B)
part("D", "Device:LED", "LED green", "LED_SMD:LED_0805_2012Metric", {"A": "PUMP_LED", "K": "GND"}, B,
     note="Aceso = bombas habilitadas")
R("22k", "+12V_PUMP", "PUMP_DIV", B, note="GPIO21: firmware confere se o corte funciona")
R("6.8k", "PUMP_DIV", "GND", B)
C("100nF", "PUMP_DIV", "GND", B)
R("1k", "PUMP_DIV", "PUMP_SENSE", B)

# ------------------------------------------------------------- canister SSR
B = "ssr"
R("100R", "CAN_CTL", "SSR_SIG", B)
R("10k", "SSR_SIG", "GND", B, note="Mantem o nivel do boot igual ao GPIO2 de hoje")
part("J", "Connector_Generic:Conn_01x03", "SSR V SEL", HDR % 3,
     {"1": "+5V", "2": "SSR_V", "3": "+3V3"}, B,
     note="Jumper: 1-2 = SSR de 5V, 2-3 = SSR de 3,3V")
part("J", "Connector:Screw_Terminal_01x03", "SSR CANISTER", TERM35_3,
     {"1": "SSR_V", "2": "SSR_SIG", "3": "GND"}, B,
     note="Borne: 1 = V+ do SSR, 2 = sinal, 3 = GND")

# ------------------------------------------------------- channel 9: H-bridge
# Reversible output for an electrolysis sterilizer (Chihiros Doctor style:
# titanium MMO mesh, polarity flipped periodically) or a reversible
# peristaltic pump. IN1/IN2 = 10 forward, 01 reverse, 00 coast, 11 brake.
# The INA219 reads the signed load current over I2C (0x40), so the firmware
# can hold a set current in electrolysis mode and catch open/short loads.
B = "ch9"
R("100R", "CH9_A_CTL", "CH9_IN1", B, note="GPIO47")
R("10k", "CH9_IN1", "GND", B, note="Pull-down: ponte parada no boot")
R("100R", "CH9_B_CTL", "CH9_IN2", B, note="GPIO48 (o LED RGB da DevKit v1.1 fica no GPIO38)")
R("10k", "CH9_IN2", "GND", B)
part("F", "Device:Polyfuse", "PTC 0.75A hold", "Fuse:Fuse_1812_4532Metric",
     {"1": "+12V_PUMP", "2": "CH9_VM"}, B, note="Tambem cortado pelo heartbeat")
C("10uF 50V", "CH9_VM", "GND", B, fp=C1206)
C("100nF", "CH9_VM", "GND", B)
part("U", "Driver_Motor:DRV8871DDA", "DRV8871DDAR",
     "Package_SO:HTSOP-8-1EP_3.9x4.9mm_P1.27mm_EP2.4x3.2mm",
     {"1": "GND", "2": "CH9_IN2", "3": "CH9_IN1", "4": "CH9_ILIM", "5": "CH9_VM",
      "6": "CH9_OUT1", "7": "GND", "8": "CH9_OUT2", "9": "GND"}, B,
     note="Ponte H 45V 3,6A, protecao termica e de curto")
R("100k", "CH9_ILIM", "GND", B, note="Limite de corrente ~0,64 A (64/100k)")
part("R", "Device:R", "0.5R 1/2W", "Resistor_SMD:R_1206_3216Metric",
     {"1": "CH9_OUT1", "2": "CH9_A"}, B, note="Shunt: 320 mV = 640 mA, resolucao 20 uA")
part("U", "Sensor_Energy:INA219AxD", "INA219AIDR", "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm",
     {"A1": "GND", "A0": "GND", "SDA": "I2C_SDA", "SCL": "I2C_SCL", "VS": "+3V3", "GND": "GND",
      "IN+": "CH9_OUT1", "IN-": "CH9_A"}, B, note="Corrente com sinal (sentido da polaridade), I2C 0x40")
C("100nF", "+3V3", "GND", B)
part("J", "Connector:Screw_Terminal_01x02", "CH9 PONTE H", TERM35,
     {"1": "CH9_A", "2": "CH9_OUT2"}, B, note="Eletrodos ou peristaltica reversivel")

# ------------------------------------------------------------------ display
B = "tft"
R("33R", "TFT_MOSI_MCU", "TFT_MOSI", B)
R("33R", "TFT_SCK_MCU", "TFT_SCK", B)
part("J", "Connector_Generic:Conn_01x08", "TFT ST7735", HDR % 8,
     {"1": "+3V3", "2": "GND", "3": "TFT_CS", "4": "EN", "5": "TFT_DC",
      "6": "TFT_MOSI", "7": "TFT_SCK", "8": "+3V3"}, B,
     note="Mesma ordem do modulo: VCC GND CS RESET A0 SDA SCK LED")

# ------------------------------------------------------------------- button
B = "btn"
part("J", "Connector_Generic:Conn_01x02", "BOTAO", HDR % 2, {"1": "BTN_IN", "2": "GND"}, B)
R("1k", "BTN_IN", "BTN", B)
R("10k", "+3V3", "BTN", B)
C("100nF", "BTN", "GND", B)

# --------------------------------------------------------------- ultrasonic
B = "us"
part("J", "Connector_Generic:Conn_01x04", "ULTRASSOM A02", HDR % 4,
     {"1": "+3V3", "2": "GND", "3": "US_TXO", "4": "US_RXI"}, B,
     note="1 VCC, 2 GND, 3 TX do sensor, 4 RX (controle) do sensor")
R("1k", "US_TXO", "US_RX", B)
R("10k", "+3V3", "US_RX", B, note="Linha em repouso alto: sensor mudo vira timeout limpo")
clamp("US_RX", B)
R("1k", "US_TX", "US_RXI", B)
CP("100uF 16V", "+3V3", "GND", B, fp="Capacitor_SMD:CP_Elec_6.3x5.4")
C("100nF", "+3V3", "GND", B)

# -------------------------------------------------------------------- float
B = "float"
part("J", "Connector_Generic:Conn_01x02", "BOIA", HDR % 2, {"1": "FLOAT_IN", "2": "GND"}, B)
R("1k", "FLOAT_IN", "FLOAT", B, note="Limita a corrente se 12V chegar ao fio")
R("10k", "+3V3", "FLOAT", B)
clamp("FLOAT", B)
C("100nF", "FLOAT", "GND", B)

# ---------------------------------------------------------------- RTC / I2C
B = "rtc"
part("U", "Timer_RTC:DS3231M", "DS3231SN#", "Package_SO:SOIC-16W_7.5x10.3mm_P1.27mm",
     dict({"VCC": "+3V3", "VBAT": "VBAT", "SDA": "I2C_SDA", "SCL": "I2C_SCL",
           "32KHZ": None, "~{INT}/SQW": None, "~{RST}": None},
          **{str(p): "GND" for p in range(5, 14)}), B)
C("100nF", "+3V3", "GND", B)
part("BT", "Device:Battery_Cell", "CR2032", "Battery:BatteryHolder_MYOUNG_BS-07-A1BJ001_CR2032",
     {"+": "VBAT", "-": "GND"}, B)
C("100nF", "VBAT", "GND", B)
R("4.7k", "+3V3", "I2C_SDA", B)
R("4.7k", "+3V3", "I2C_SCL", B)
part("J", "Connector_Generic:Conn_01x04", "I2C EXP", HDR % 4,
     {"1": "+3V3", "2": "GND", "3": "I2C_SDA", "4": "I2C_SCL"}, B)

# --------------------------------------------------------------- mechanical
for _ in range(4):
    part("H", "Mechanical:MountingHole", "M3", "MountingHole:MountingHole_3.2mm_M3", {}, "mech", bom=False)

# nets with no driving pin of their own (ERC power flags)
POWER_FLAGS = ["+12V", "+5V", "+3V3", "GND", "VBAT", "CH9_VM"]


# JLCPCB/LCSC part numbers, checked on jlcpcb.com parts search 2026-11-11
# (heartbeat parts checked 2026-11-18)
LCSC = {
    "10k": "C25804", "4.7k": "C23162", "3.3k": "C22978", "1k": "C21190",
    "100R": "C22775", "33R": "C23140", "100nF": "C14663",
    "470k": "C23178", "100k": "C25803", "47k": "C25819", "22k": "C31850", "6.8k": "C23212",
    "1uF": "C15849", "AO4407A": "C16072",
    "DRV8871DDAR": "C75864", "0.05R 1W": "C105390", "INA219AIDR": "C138706", "0.5R 1/2W": "C859934",
    "22uF 25V": "C12891", "10uF 50V": "C13585",
    "470uF 25V": "C3445246", "470uF 16V": "C2977552", "1000uF 10V": "C48935400",
    "100uF 16V": "C2887276",
    "AP63205WU": "C2071056", "4.7uH": "C167220", "AO3400A": "C20917", "SS34": "C8678",
    "BAT54S": "C7420333", "SMBJ15A": "C19077569",
    "PTC 0.75A hold": "C369169", "PTC 0.5A hold": "C369168",
    "DS3231SN#": "C9866", "CR2032": "C2979167", "LED green": "C2297",
    "12V IN": "C474952", "REED": "C466970",
    "DevKit J1": "C41376161", "DevKit J3": "C41376161",
    "TFT ST7735": "C492407", "SSR CANISTER": "C474893", "SSR V SEL": "C2894926", "ULTRASSOM A02": "C2691448",
    "I2C EXP": "C2691448", "BOTAO": "C492401", "BOIA": "C492401",
}


def assign_refs():
    counters = {}
    for p in PARTS:
        counters[p["prefix"]] = counters.get(p["prefix"], 0) + 1
        p["ref"] = "%s%d" % (p["prefix"], counters[p["prefix"]])
        if p["bom"] and not p["lcsc"]:
            key = "CHAN" if p["value"].startswith("CH") else p["value"]
            p["lcsc"] = LCSC.get(key, "C466970" if key == "CHAN" else "")
            if not p["lcsc"]:
                raise SystemExit("sem codigo LCSC: %s %s" % (p["prefix"], p["value"]))
    return PARTS


assign_refs()
