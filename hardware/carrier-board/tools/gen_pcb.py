"""Place the carrier-board parts on a 100 x 100 mm board (carrier.kicad_pcb).

Run with KiCad's bundled Python (it ships the pcbnew module). Nets come from
carrier.net, so pads carry the same connectivity as the schematic. This is a
placement pass: no tracks yet. Layout plan:

    top edge     sensor/lid headers, DevKit sockets (USB over the right edge)
    left column  12 V entry, 5 V buck, heartbeat pump switch
    middle       RTC + CR2032, sensor input protection
    bottom edge  12V IN + 8 channel terminals + REED, each channel's parts in
                 a column directly above its terminal
"""
import os
import sys

import pcbnew

sys.path.insert(0, os.path.dirname(__file__))
from sexpr import find, find1, parse  # noqa: E402
import circuit  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "carrier.kicad_pcb")
NETLIST = os.path.join(HERE, "..", "carrier.net")
FPDIR = "/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints/"
W, H = 100.0, 100.0

board = pcbnew.BOARD()
mm = pcbnew.FromMM


def pt(x, y):
    return pcbnew.VECTOR2I(mm(x), mm(y))


# ------------------------------------------------------------------ nets
tree = parse(open(NETLIST).read())
pad_net = {}
for n in find(find1(tree, "nets"), "net"):
    name = find1(n, "name")[1].lstrip("/")
    if name.startswith("unconnected"):
        continue
    ni = pcbnew.NETINFO_ITEM(board, name)
    board.Add(ni)
    for node in find(n, "node"):
        pad_net[(find1(node, "ref")[1], find1(node, "pin")[1])] = ni

# ------------------------------------------------------------- footprints
fps = {}
for p in circuit.PARTS:
    lib, name = p["footprint"].split(":")
    fp = pcbnew.FootprintLoad(FPDIR + lib + ".pretty", name)
    if fp is None:
        raise SystemExit("footprint not found: " + p["footprint"])
    fp.SetReference(p["ref"])
    fp.SetValue(p["value"])
    fp.Reference().SetVisible(False)
    fp.Value().SetVisible(False)
    for pad in fp.Pads():
        ni = pad_net.get((p["ref"], pad.GetNumber()))
        if ni:
            pad.SetNet(ni)
    board.Add(fp)
    fps[p["ref"]] = fp


def by_block(block):
    return [p for p in circuit.PARTS if p["block"] == block]


def ref_of(block, value, nth=0):
    hits = [p["ref"] for p in by_block(block) if p["value"] == value]
    return hits[nth]


def place(ref, x, y, rot=0):
    fp = fps[ref]
    fp.SetOrientationDegrees(rot)
    fp.SetPosition(pt(x, y))


def place_center(ref, x, y, rot=0):
    """Place so the courtyard/body bbox centre lands on (x, y)."""
    place(ref, 0, 0, rot)
    bb = fps[ref].GetBoundingBox(False)
    c = bb.GetCenter()
    place(ref, x - pcbnew.ToMM(c.x), y - pcbnew.ToMM(c.y), rot)


def width(ref, rot=0):
    place(ref, 0, 0, rot)
    return pcbnew.ToMM(fps[ref].GetBoundingBox(False).GetWidth())


# ------------------------------------------------------------- outline
def edge(x0, y0, x1, y1):
    s = pcbnew.PCB_SHAPE(board)
    s.SetShape(pcbnew.SHAPE_T_SEGMENT)
    s.SetStart(pt(x0, y0))
    s.SetEnd(pt(x1, y1))
    s.SetLayer(pcbnew.Edge_Cuts)
    s.SetWidth(mm(0.1))
    board.Add(s)


for a, b in (((0, 0), (W, 0)), ((W, 0), (W, H)), ((W, H), (0, H)), ((0, H), (0, 0))):
    edge(a[0], a[1], b[0], b[1])

holes = [p["ref"] for p in by_block("mech")]
for ref, (x, y) in zip(holes, ((3.5, 3.5), (W - 3.5, 3.5), (3.5, H - 3.5), (W - 3.5, H - 3.5))):
    place(ref, x, y)

# --------------------------------------------- bottom edge: power terminals
# terminal openings face the bottom edge (rotation 180)
order = [ref_of("power", "12V IN")]
for n in range(1, 9):
    order.append(ref_of("ch%d" % n, [p["value"] for p in by_block("ch%d" % n) if p["value"].startswith("CH%d " % n)][0]))
    if n == 7:
        order.append(ref_of("ch7", "REED"))
widths = [width(r, 180) for r in order]
gap = (W - 15 - sum(widths)) / (len(order) - 1)
x = 7.5
centers = {}
for r, w in zip(order, widths):
    place_center(r, x + w / 2, H - 5.2, 180)
    centers[r] = x + w / 2
    x += w + gap

# ------------------------------------------- channel columns above terminals
for n in range(1, 9):
    blk = "ch%d" % n
    term = [p["ref"] for p in by_block(blk) if p["value"].startswith("CH%d " % n)][0]
    cx = centers[term]
    refs = {p["value"] + ("#%d" % i): p["ref"] for i, p in enumerate(by_block(blk))}
    get = lambda v: [p["ref"] for p in by_block(blk) if p["value"] == v]  # noqa: E731
    place_center([r for r in get("PTC 0.75A hold") + get("PTC 0.5A hold")][0], cx - 2.0, 66, 90)
    place_center(get("SS34")[0], cx + 2.2, 66, 90)
    place_center(get("AO3400A")[0], cx, 73.5, 0)
    place_center(get("100R")[0], cx - 2.2, 78.5, 90)
    place_center(get("10k")[0], cx, 78.5, 90)
    place_center(get("4.7k")[0], cx + 2.2, 78.5, 90)
    place_center(get("LED green")[0], cx, 83, 0)

# reed column extras (divider, filter, clamp, series R) above the REED terminal
reed = ref_of("ch7", "REED")
cx = centers[reed]
extras = [p["ref"] for p in by_block("ch7") if p["value"] in ("3.3k", "100nF", "BAT54S", "1k")]
extras.insert(0, [p["ref"] for p in by_block("ch7") if p["value"] == "10k"][1])
for i, r in enumerate(extras):
    place_center(r, cx + (-1.6 if i % 2 == 0 else 1.6), 64 + (i // 2) * 4.5, 90 if fps[r].GetPadCount() == 2 else 0)

# ------------------------------------------------- DevKit sockets (top right)
# Rotation 90 runs the pins along +X. Pin 22 (GND/5V end, where the USB
# connectors are) sits near the right edge so the USB ports overhang it.
j1 = ref_of("devkit", "DevKit J1")
j3 = ref_of("devkit", "DevKit J3")
pitch = 2.54
x_pin22 = W - 4.0
x_pin1 = x_pin22 - 21 * pitch
for r, y in ((j1, 37.0), (j3, 37.0 - 22.86)):
    for rot in (90, 270):
        place(r, 0, 0, rot)
        pads = {pd.GetNumber(): pd.GetPosition() for pd in fps[r].Pads()}
        if pads["22"].x > pads["1"].x:
            break
    place(r, x_pin1, y, rot)
dk_caps = [p["ref"] for p in by_block("devkit") if p["prefix"] == "C"]
place_center(dk_caps[0], x_pin1 - 7.5, 31.0)   # 1000 uF next to the 3V3 pins
place_center(dk_caps[1], x_pin1 + 2.0, 40.6, 0)

# --------------------------------------------------- top strip: headers
hdr_y = 4.2
row = [ref_of("tft", "TFT ST7735"), ref_of("btn", "BOTAO"), ref_of("rtc", "I2C EXP")]
x = 30.0
for r in row:
    w = width(r, 90)
    place_center(r, x + w / 2, hdr_y, 90)
    x += w + 2.5
for i, r in enumerate(p["ref"] for p in by_block("tft") if p["prefix"] == "R"):
    place_center(r, 33 + i * 3, 8.6, 0)
for i, r in enumerate(p["ref"] for p in by_block("btn") if p["prefix"] != "J"):
    place_center(r, fps[row[1]].GetPosition().x / 1e6 - 1 + i * 2.2, 8.6, 90)

# SSR control terminal on the top edge, wire entry facing out
place_center(ref_of("ssr", "SSR CANISTER"), 84.0, 4.8, 0)

# --------------------------------- middle band: sensors, SSR, RTC + battery
mid_y = 46.0
row = [ref_of("us", "ULTRASSOM A02"), ref_of("float", "BOIA"), ref_of("ssr", "SSR V SEL")]
x = 47.0
for r in row:
    w = width(r, 90)
    place_center(r, x + w / 2, mid_y, 90)
    x += w + 3.0
small = []
for blk in ("us", "float", "ssr"):
    small += [p["ref"] for p in by_block(blk) if p["prefix"] != "J" and not p["value"].startswith("100uF")]
for i, r in enumerate(small):
    place_center(r, 46.0 + (i % 9) * 3.1, 51.5 + (i // 9) * 4.6, 90 if fps[r].GetPadCount() == 2 else 0)
place_center([p["ref"] for p in by_block("us") if p["value"].startswith("100uF")][0], 78.5, 56.5)

bat = ref_of("rtc", "CR2032")
place_center(bat, 30.0, 49.5, 0)
place_center(ref_of("rtc", "DS3231SN#"), 88.0, 47.0, 90)
rsmall = [p["ref"] for p in by_block("rtc") if fps[p["ref"]].GetPadCount() == 2 and p["ref"] != bat]
for i, r in enumerate(rsmall):
    place_center(r, 85.0 + i * 2.6, 56.0, 90)

# ------------------------------------------- left column: 12 V entry + buck
pw = {p["ref"]: p for p in by_block("power")}
get = lambda v: [r for r, p in pw.items() if p["value"] == v]  # noqa: E731
place_center(get("SMBJ15A")[0], 8.0, 84.5, 90)
place_center(get("470uF 25V")[0], 9.0, 73.0)
# buck in the top-left corner; its 10 uF + 100 nF input pair sits on the
# VIN/GND pins, since the switching loop is what radiates
place_center(get("AP63205WU")[0], 11.0, 13.0, 0)
place_center(get("10uF 50V")[0], 6.0, 13.0, 90)
place_center(get("100nF")[0], 11.0, 17.2, 0)
place_center(get("100nF")[1], 11.0, 9.0, 0)
place_center(get("4.7uH")[0], 18.5, 13.0, 0)
place_center(get("22uF 25V")[0], 27.0, 10.5, 0)
place_center(get("22uF 25V")[1], 27.0, 15.5, 0)
place_center(get("470uF 16V")[0], 13.0, 22.5, 0)
place_center(get("SS34")[0], 22.0, 22.0, 0)
for i, r in enumerate(get("+12V") + get("+5V") + get("+3V3") + get("GND")):
    place(r, 3.5 + i * 3.0, 30.0)

# heartbeat: pump switch near the 12 V entry, pump-charge parts above it
hb = [p["ref"] for p in by_block("hb")]
pfet = ref_of("hb", "AO4407A")
place_center(pfet, 9.0, 62.5, 0)
pshunt = ref_of("hb", "0.05R 1W")
pmon = ref_of("hb", "INA219AIDR")
pmon_cap = [p["ref"] for p in by_block("hb") if p["value"] == "100nF"][-1]
place_center(pshunt, 15.5, 61.0, 270)  # +12V_SW pad up, +12V_PUMP pad down; between the P-FET drain and the pump bus
place_center(pmon, 8.0, 53.2, 0)
place_center(pmon_cap, 13.5, 53.2, 90)
rest = [r for r in hb if r not in (pfet, pshunt, pmon, pmon_cap)]
for i, r in enumerate(rest):
    place_center(r, 2.6 + (i % 5) * 3.1, 38.0 + (i // 5) * 5.0, 90 if fps[r].GetPadCount() == 2 else 0)

# channel 9 H-bridge: terminal on the top edge left of the TFT header, the
# bridge and current monitor in the free pocket between the buck and the
# DevKit socket
c9 = {}
for p in by_block("ch9"):
    c9.setdefault(p["value"], []).append(p["ref"])
place_center(c9["CH9 PONTE H"][0], 20.0, 4.8, 0)
place_center(c9["PTC 0.75A hold"][0], 33.0, 12.3, 0)
place_center(c9["10uF 50V"][0], 34.0, 15.6, 180)  # VM pad faces DRV pin 5
place_center(c9["DRV8871DDAR"][0], 34.2, 21.2, 90)
place_center(c9["100nF"][0], 30.5, 19.0, 90)
place_center(c9["0.5R 1/2W"][0], 38.7, 19.0, 90)
place_center(c9["100k"][0], 38.7, 23.4, 90)
for i, r in enumerate([c9["100R"][0], c9["10k"][0], c9["100R"][1], c9["10k"][1]]):
    place_center(r, 26.75 + (i % 2) * 1.7, 18.8 + (i // 2) * 3.4, 90)
place_center(c9["INA219AIDR"][0], 25.8, 27.6, 90)
place_center(c9["100nF"][1], 21.6, 27.6, 90)

pcbnew.SaveBoard(OUT, board)
print("wrote", os.path.abspath(OUT), len(fps), "footprints")
