"""Route carrier.kicad_pcb with Freerouting and pour GND.

Run with KiCad's bundled Python after gen_pcb.py:

    route.py export   -> writes carrier.kicad_pro (net classes) and carrier.dsn
    (run Freerouting on carrier.dsn -> carrier.ses)
    route.py import   -> applies carrier.ses, fills GND zones, saves the board

Net classes follow current, not taste: the 12 V bus carries every pump at
once (fused at 5 A in the box), each channel at most 0.75 A, the 5 V/3V3
rails a few hundred mA. Widths are for 1 oz outer copper with ~10 C rise.
"""
import json
import os
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
BASE = os.path.join(HERE, "..", "carrier")
PCB, PRO, DSN, SES = BASE + ".kicad_pcb", BASE + ".kicad_pro", BASE + ".dsn", BASE + ".ses"

CLASSES = [
    # name, track mm, clearance mm, via dia, via drill, patterns
    ("Default", 0.25, 0.2, 0.6, 0.3, []),
    ("Bus12V", 1.0, 0.25, 0.9, 0.5, ["+12V", "+12V_SW", "+12V_PUMP"]),
    ("Pump", 0.8, 0.25, 0.8, 0.4, ["CH*_POS", "CH*_NEG", "REED_A"]),
    ("Bridge", 0.5, 0.2, 0.8, 0.4, ["CH9_VM", "CH9_OUT1", "CH9_OUT2", "CH9_A"]),
    ("Supply", 0.6, 0.2, 0.7, 0.35, ["+5V", "+5V_DK", "SW5", "+3V3", "GND"]),
]


def write_project():
    classes = []
    for name, tw, cl, vd, vdr, _ in CLASSES:
        classes.append({
            "name": name, "track_width": tw, "clearance": cl,
            "via_diameter": vd, "via_drill": vdr,
            "microvia_diameter": 0.3, "microvia_drill": 0.1,
            "diff_pair_width": 0.2, "diff_pair_gap": 0.25, "diff_pair_via_gap": 0.25,
            "wire_width": 6, "bus_width": 12, "line_style": 0, "priority": 2147483647 if name == "Default" else 0,
            "schematic_color": "rgba(0, 0, 0, 0.000)", "pcb_color": "rgba(0, 0, 0, 0.000)",
        })
    # schematic label nets are named "/NAME"; accept both spellings
    patterns = [{"netclass": name, "pattern": pre + p} for name, *_, pats in CLASSES
                for p in pats for pre in ("", "/")]
    pro = {
        "board": {"design_settings": {"rules": {
            # JLCPCB standard 2-layer capabilities, with margin
            "min_clearance": 0.15, "min_track_width": 0.15, "min_via_diameter": 0.5,
            "min_through_hole_diameter": 0.3, "min_copper_edge_clearance": 0.3,
            "min_hole_to_hole": 0.5, "min_hole_clearance": 0.25}}},
        "meta": {"filename": "carrier.kicad_pro", "version": 3},
        "net_settings": {"classes": classes, "meta": {"version": 4}, "netclass_patterns": patterns},
    }
    with open(PRO, "w") as f:
        json.dump(pro, f, indent=2)


def add_gnd_zones(board):
    """Caller must start from a board without GND zones (finalize.py strips them)."""
    gnd = board.FindNet("/GND") or board.FindNet("GND")
    for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
        z = pcbnew.ZONE(board)
        z.SetLayer(layer)
        z.SetNet(gnd)
        z.SetLocalClearance(pcbnew.FromMM(0.3))
        z.SetMinThickness(pcbnew.FromMM(0.25))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
        z.SetThermalReliefGap(pcbnew.FromMM(0.4))
        z.SetThermalReliefSpokeWidth(pcbnew.FromMM(0.5))
        o = z.Outline()
        o.NewOutline()
        for x, y in ((0.5, 0.5), (99.5, 0.5), (99.5, 99.5), (0.5, 99.5)):
            o.Append(pcbnew.FromMM(x), pcbnew.FromMM(y))
        board.Add(z)


def edge_keepout(board):
    """A 0.9 mm no-track/no-via frame along the outline. Freerouting does not
    see KiCad's copper-to-edge rule, and once put a via 0.2 mm from the edge.
    finalize.py strips it with the other zones; pours keep their own margin."""
    w = 0.9
    for x0, y0, x1, y1 in ((0, 0, 100, w), (0, 100 - w, 100, 100), (0, 0, w, 100), (100 - w, 0, 100, 100)):
        z = pcbnew.ZONE(board)
        z.SetIsRuleArea(True)
        z.SetDoNotAllowTracks(True)
        z.SetDoNotAllowVias(True)
        z.SetDoNotAllowPads(False)
        z.SetDoNotAllowFootprints(False)
        z.SetDoNotAllowZoneFills(False)
        ls = pcbnew.LSET()
        ls.AddLayer(pcbnew.F_Cu)
        ls.AddLayer(pcbnew.B_Cu)
        z.SetLayerSet(ls)
        o = z.Outline()
        o.NewOutline()
        for x, y in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)):
            o.Append(pcbnew.FromMM(x), pcbnew.FromMM(y))
        board.Add(z)


def export():
    write_project()
    # GND is routed as ordinary tracks (a plane in the DSN lets Freerouting
    # assume connectivity the poured copper may not deliver); pours come after.
    import finalize  # zones go as text: Remove() then Add() crashes pcbnew 10
    finalize.strip_zones()
    board = pcbnew.LoadBoard(PCB)
    edge_keepout(board)
    if "--keep-tracks" not in sys.argv:
        for t in list(board.GetTracks()):
            board.Remove(t)
    pcbnew.SaveBoard(PCB, board)
    board = pcbnew.LoadBoard(PCB)  # reload so the project net classes apply
    ok = pcbnew.ExportSpecctraDSN(board, DSN)
    print("dsn", ok, DSN)


def text(board, s, x, y, size=1.0, layer=None):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(s)
    t.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y)))
    t.SetLayer(layer if layer is not None else pcbnew.F_SilkS)
    t.SetTextSize(pcbnew.VECTOR2I(pcbnew.FromMM(size), pcbnew.FromMM(size)))
    t.SetTextThickness(pcbnew.FromMM(size * 0.15))
    board.Add(t)


def add_labels(board):
    """Silkscreen names next to every connector, from the footprint values."""
    items = [(fp.GetValue(), fp.GetBoundingBox(False)) for fp in board.GetFootprints()]
    for t in [d for d in board.GetDrawings() if d.GetClass() == "PCB_TEXT"]:
        board.Remove(t)
    for v, bb in items:
        cx, top, bot = pcbnew.ToMM(bb.GetCenter().x), pcbnew.ToMM(bb.GetTop()), pcbnew.ToMM(bb.GetBottom())
        if v.startswith("CH9 "):                     # top-edge terminal: label below it
            text(board, "CH9", 11.5, 3.2, 1.0)
            text(board, "PONTE H", 11.5, 5.0, 0.8)
        elif v.startswith("CH") and " " in v:          # channel terminals: "CH1 FERT1"
            n, name = v.split(" ", 1)
            text(board, n, cx, top - 3.2, 1.0)
            text(board, name, cx, top - 1.6, 0.8)
        elif v in ("12V IN", "REED"):
            text(board, v, cx, top - 1.6, 1.0)
        elif v in ("TFT ST7735", "BOTAO", "I2C EXP"):
            text(board, v.split(" ")[0], cx, top - 0.9, 0.8)
        elif v in ("ULTRASSOM A02", "BOIA"):
            text(board, v.split(" ")[0], cx, bot + 1.0, 0.8)
        elif v == "SSR CANISTER":
            text(board, "SSR CANISTER  V+ SIG GND", cx, bot + 1.0, 0.8)
        elif v == "SSR V SEL":
            text(board, "5V | SSR | 3V3", cx, top - 1.0, 0.8)
        elif v == "CR2032":
            cy = pcbnew.ToMM(bb.GetCenter().y)
            text(board, "CR2032 (+ para cima)", cx, cy + 5.0, 0.8)
    text(board, "IARA carrier v0.3  ESP32-S3-DevKitC-1", 70.0, 62.0, 1.0)
    text(board, "USB ->", 93.0, 26.0, 0.9)


def do_import():
    board = pcbnew.LoadBoard(PCB)
    ok = pcbnew.ImportSpecctraSES(board, SES)
    add_gnd_zones(board)
    add_labels(board)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    pcbnew.SaveBoard(PCB, board)
    print("ses", ok, "tracks", len(board.GetTracks()))


if __name__ == "__main__":
    {"export": export, "import": do_import}[sys.argv[1]]()
