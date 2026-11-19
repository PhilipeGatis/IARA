"""Project-specific checks on the exported netlist.

Run after `kicad-cli sch export netlist`. Each check states the rule it
enforces; a failure prints why and exits non-zero.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from sexpr import find, find1, parse  # noqa: E402

path = sys.argv[1]
tree = parse(open(path).read())
nets = {}
values = {}
for c in find(find1(tree, "components"), "comp"):
    values[find1(c, "ref")[1]] = find1(c, "value")[1]
for n in find(find1(tree, "nets"), "net"):
    name = find1(n, "name")[1].lstrip("/")
    nets[name] = {(find1(x, "ref")[1], find1(x, "pin")[1]) for x in find(n, "node")}

fails = []


def check(cond, msg):
    print(("OK   " if cond else "FAIL ") + msg)
    if not cond:
        fails.append(msg)


def refs(net):
    return {r for r, _ in nets.get(net, ())}


def pin_net(ref, pin):
    for name, nodes in nets.items():
        if (ref, pin) in nodes:
            return name
    return None


# 1. Reed is the only path from the CH7 PTC to the refill pump.
reed = [r for r, v in values.items() if v == "REED"][0]
ch7 = [r for r, v in values.items() if v.startswith("CH7 ")][0]
check(len(nets["REED_A"]) == 2 and reed in refs("REED_A"),
      "REED_A liga so o PTC do CH7 e o borne do reed: %s" % sorted(nets["REED_A"]))
check(pin_net(ch7, "1") == "CH7_POS" and pin_net(reed, "2") == "CH7_POS",
      "o +12V do recalque (%s.1) so chega pelo reed (%s.2)" % (ch7, reed))
check(not (refs("+12V") & refs("CH7_POS")),
      "nenhum componente liga +12V direto ao CH7_POS")

# 2. Every gate has a pull-down and a series resistor.
for n in range(1, 9):
    g = "CH%d_G" % n
    rs = [r for r in refs(g) if r.startswith("R")]
    pd = [r for r in rs if values[r] == "10k" and r in refs("GND")]
    check(len(pd) == 1, "%s tem pull-down 10k para GND" % g)
    check(any(r in refs("CH%d_CTL" % n) for r in rs), "%s tem resistor serie vindo do GPIO" % g)

# 3. GPIO map on the DevKit socket (ESP32-S3-DevKitC-1 header order).
J1 = ["3V3", "3V3", "RST", 4, 5, 6, 7, 15, 16, 17, 18, 8, 3, 46, 9, 10, 11, 12, 13, 14, "5V", "GND"]
J3 = ["GND", 43, 44, 1, 2, 42, 41, 40, 39, 38, 37, 36, 35, 0, 45, 48, 47, 21, 20, 19, "GND", "GND"]
sock1 = [r for r, v in values.items() if v == "DevKit J1"][0]
sock3 = [r for r, v in values.items() if v == "DevKit J3"][0]
gpio = {}
for sock, order in ((sock1, J1), (sock3, J3)):
    for i, g in enumerate(order):
        net = pin_net(sock, str(i + 1))
        if isinstance(g, int) and net and not net.startswith("unconnected"):
            gpio[g] = net
print("     mapa GPIO -> rede:", ", ".join("GPIO%d=%s" % kv for kv in sorted(gpio.items())))
for strap in (0, 3, 45, 46):
    check(strap not in gpio, "GPIO%d (strapping) livre" % strap)
for psram in (35, 36, 37):
    check(psram not in gpio, "GPIO%d (PSRAM octal) livre" % psram)
for usb in (19, 20):
    check(usb not in gpio, "GPIO%d (USB) livre" % usb)
check(pin_net(sock1, "21") == "+5V_DK", "5V da DevKit vem pelo diodo (+5V_DK)")
check(len(refs("+5V_DK")) == 2, "+5V_DK liga so o diodo e o pino 5V")

# 4. Inputs from outside the box are clamped.
for net in ("FLOAT", "US_RX", "REED_DIV"):
    check(any(values[r] == "BAT54S" for r in refs(net)), "%s tem clamp BAT54S" % net)

# 5. Every channel has flyback and PTC.
for n in range(1, 9):
    pos, neg = "CH%d_POS" % n, "CH%d_NEG" % n
    check(any(values[r] == "SS34" for r in refs(pos) & refs(neg)), "CH%d tem flyback" % n)

# 6. Heartbeat: pump +12V only through the P-MOSFET, gated by a capacitor-coupled pulse.
ptcs = [r for r, v in values.items() if v.startswith("PTC")]
check(all(pin_net(r, "1") == "+12V_PUMP" for r in ptcs), "todos os PTCs dos canais saem do +12V_PUMP")
pfet = [r for r, v in values.items() if v == "AO4407A"][0]
check(pin_net(pfet, "1") == "+12V" and pin_net(pfet, "5") == "+12V_SW", "AO4407A liga +12V -> +12V_SW")
check(refs("+12V") & refs("+12V_SW") == {pfet}, "so o AO4407A une +12V e +12V_SW")
check(not (refs("+12V") & refs("+12V_PUMP")), "nada liga +12V direto no +12V_PUMP")
pshunt = [r for r, v in values.items() if v == "0.05R 1W"][0]
check(refs("+12V_SW") & refs("+12V_PUMP") == {pshunt} | {r for r in refs("+12V_SW") & refs("+12V_PUMP") if values[r] == "INA219AIDR"},
      "toda a corrente das bombas passa pelo shunt de 0,05R")
pmon = [r for r in refs("+12V_SW") if values[r] == "INA219AIDR"]
check(len(pmon) == 1 and pin_net(pmon[0], "8") == "+12V_SW" and pin_net(pmon[0], "7") == "+12V_PUMP"
      and pin_net(pmon[0], "2") == "+3V3" and pin_net(pmon[0], "1") == "GND",
      "INA219 das bombas le o shunt, endereco 0x41")
check(gpio.get(14) == "HB" and gpio.get(21) == "PUMP_SENSE", "GPIO14 = HB, GPIO21 = PUMP_SENSE")
hb_r = refs("HB_R")
check(len(hb_r) == 2 and all(values[r] in ("100R", "1uF") for r in hb_r),
      "HB chega ao retificador so por capacitor (nivel DC nao passa)")
check(any(values[r] == "470k" for r in refs("HB_DC") & refs("GND")), "HB_DC descarrega por 470k")

# 7. Channel 9 H-bridge: off at boot, fed from the heartbeat-gated rail, current measured.
drv = [r for r, v in values.items() if v == "DRV8871DDAR"][0]
ina = [r for r in refs("CH9_A") if values[r] == "INA219AIDR"][0]
check(gpio.get(47) == "CH9_A_CTL" and gpio.get(48) == "CH9_B_CTL", "GPIO47 = CH9 IN1, GPIO48 = CH9 IN2")
for net in ("CH9_IN1", "CH9_IN2"):
    check(any(values[r] == "10k" for r in refs(net) & refs("GND")), "%s tem pull-down (ponte parada no boot)" % net)
vm_ptc = [r for r in refs("CH9_VM") if values[r].startswith("PTC")]
check(pin_net(drv, "5") == "CH9_VM" and len(vm_ptc) == 1 and pin_net(vm_ptc[0], "1") == "+12V_PUMP",
      "DRV8871 VM vem do +12V_PUMP por PTC (heartbeat corta o canal 9)")
shunt = [r for r, v in values.items() if v.startswith("0.5R")][0]
check({pin_net(shunt, "1"), pin_net(shunt, "2")} == {"CH9_OUT1", "CH9_A"}
      and pin_net(ina, "8") == "CH9_OUT1" and pin_net(ina, "7") == "CH9_A",
      "INA219 le o shunt em serie com a saida A")
check(pin_net(ina, "1") == "GND" and pin_net(ina, "2") == "GND", "INA219 no endereco 0x40 (DS3231 fica em 0x68)")

# 8. No single-pin nets.
lonely = [n for n, nodes in nets.items() if len(nodes) < 2 and not n.startswith("unconnected")]
check(not lonely, "nenhuma rede com um pino so: %s" % lonely)

print("\n%d falha(s)" % len(fails))
sys.exit(1 if fails else 0)
