"""Post-routing clean-up of carrier.kicad_pcb.

Run with KiCad's bundled Python after `route.py import`:

  1. delete dangling track stubs and vias Freerouting leaves behind, found by
     DRC (repeated, since removing one stub can expose the next);
  2. pour wide +12V and +12V_PUMP copper where the bus current flows, above
     the GND pours in priority;
  3. refill zones and save.
"""
import json
import os
import subprocess

import sys

import pcbnew

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import route  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
PCB = os.path.join(HERE, "..", "carrier.kicad_pcb")
CLI = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
TMP = os.path.join(HERE, "..", "drc.json")


def load():
    """LoadBoard occasionally hands back a bare SwigPyObject; retry."""
    for _ in range(5):
        b = pcbnew.LoadBoard(PCB)
        if hasattr(b, "GetTracks"):
            return b
    raise SystemExit("could not load " + PCB)


def drc():
    subprocess.run([CLI, "pcb", "drc", "--format", "json", "--severity-all", "--refill-zones",
                    "-o", TMP, PCB], capture_output=True)
    data = json.load(open(TMP))
    os.remove(TMP)
    return data


def step(name):
    """pcbnew 10 loads a board only once per process here (a second
    LoadBoard after SaveBoard returns junk), so each step runs in its own."""
    subprocess.run([sys.executable, os.path.abspath(__file__), name], check=True)


def clean_dangling():
    for _ in range(10):
        data = drc()
        if not any(v["type"] in ("track_dangling", "via_dangling") for v in data.get("violations", [])):
            return
        step("clean-once")


def clean_once():
    data = drc()
    ids = {it["uuid"] for v in data["violations"] if v["type"] in ("track_dangling", "via_dangling")
           for it in v["items"]}
    board = load()
    gone = [t for t in board.GetTracks() if t.m_Uuid.AsString() in ids]
    for t in gone:
        board.Remove(t)
    pcbnew.SaveBoard(PCB, board)
    print("removed %d dangling track/via items" % len(gone))


def zone(board, net, layer, pts, priority):
    z = pcbnew.ZONE(board)
    z.SetLayer(layer)
    z.SetNet(board.FindNet("/" + net) or board.FindNet(net))
    z.SetAssignedPriority(priority)
    z.SetLocalClearance(pcbnew.FromMM(0.3))
    z.SetMinThickness(pcbnew.FromMM(0.25))
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
    z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
    o = z.Outline()
    o.NewOutline()
    for x, y in pts:
        o.Append(pcbnew.FromMM(x), pcbnew.FromMM(y))
    board.Add(z)


def rect(x0, y0, x1, y1):
    return ((x0, y0), (x1, y0), (x1, y1), (x0, y1))


def strip_zones():
    """Drop every top-level (zone ...) block from the file. Removing zones
    through the Python API crashes pcbnew 10 here, so do it as text."""
    text = open(PCB).read()
    out, i = [], 0
    while True:
        j = text.find("\n\t(zone", i)
        if j < 0:
            out.append(text[i:])
            break
        out.append(text[i:j])
        depth, k = 0, j + 2
        while True:
            c = text[k]
            if c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
                if depth == 0:
                    break
            elif c == '"':
                k = text.index('"', k + 1)
            k += 1
        i = k + 1
    open(PCB, "w").write("".join(out))


def link_to_schematic(board):
    """Make the board match the schematic exactly: "/NAME" net names, full
    library footprint IDs, symbol paths and the LCSC field, so KiCad's
    schematic-parity check and "Update PCB from Schematic" line up."""
    import gen_sch  # noqa: F401  (same deterministic UUIDs as the schematic)
    import circuit
    for ni in board.GetNetInfo().NetsByName().values():
        name = ni.GetNetname()
        if name and not name.startswith("/"):
            ni.SetNetname("/" + name)
    parts = {p["ref"]: p for p in circuit.PARTS}
    for fp in board.GetFootprints():
        p = parts.get(fp.GetReference())
        if not p:
            continue
        lib, name = p["footprint"].split(":")
        fp.SetFPID(pcbnew.LIB_ID(lib, name))
        fp.SetPath(pcbnew.KIID_PATH("/" + gen_sch.U("sym", p["ref"])))
        if p["lcsc"]:
            fp.SetField("LCSC", p["lcsc"])
            fp.GetField("LCSC").SetVisible(False)


def add_power_pours():
    strip_zones()
    board = load()
    link_to_schematic(board)
    route.add_gnd_zones(board)
    # DevKit GND pins sit between tracks that starve their thermal spokes, and
    # the DRV8871 exposed pad is its heat path: tie them solid to the pour
    for fp in board.GetFootprints():
        if fp.GetValue().startswith("DevKit") or fp.GetValue() == "DRV8871DDAR":
            for pad in fp.Pads():
                if pad.GetNetname() == "GND":
                    pad.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)
    # 12 V entry: terminal, TVS, bulk cap and the P-MOSFET source, top layer
    zone(board, "+12V", pcbnew.F_Cu,
         ((0.8, 56.0), (9.0, 56.0), (9.0, 66.5), (16.5, 66.5), (16.5, 99.2), (0.8, 99.2)), 2)
        # P-MOSFET drain up to the top pad of the current shunt
    zone(board, "+12V_SW", pcbnew.F_Cu,
         ((10.2, 56.3), (17.6, 56.3), (17.6, 59.7), (13.0, 59.7), (13.0, 65.8), (10.2, 65.8)), 2)
    # pump bus: shunt bottom pad to the row of PTC inputs
    zone(board, "+12V_PUMP", pcbnew.F_Cu,
         ((13.6, 60.4), (18.4, 60.4), (18.4, 59.0), (96.0, 59.0), (96.0, 63.2), (17.6, 63.2),
          (17.6, 65.4), (13.6, 65.4)), 2)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    pcbnew.SaveBoard(PCB, board)


def fix_drc_leftovers():
    """GND pads whose thermal spokes got starved by nearby tracks: tie them
    solid to the pour instead."""
    data = drc()
    starved = [it for v in data.get("violations", []) if v["type"] == "starved_thermal"
               for it in v["items"] if not it["description"].startswith("Zon")]
    if not starved:
        return
    board = load()
    for it in starved:
        for fp in board.GetFootprints():
            for pad in fp.Pads():
                if pad.m_Uuid.AsString() == it["uuid"]:
                    pad.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)
                    print("solid GND tie:", fp.GetReference(), pad.GetNumber())
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    pcbnew.SaveBoard(PCB, board)


if __name__ == "__main__":
    if len(sys.argv) > 1:
        {"clean-once": clean_once, "pours": add_power_pours, "fix": fix_drc_leftovers}[sys.argv[1]]()
        sys.exit(0)
    clean_dangling()
    step("pours")
    clean_dangling()
    step("fix")
    data = drc()
    kinds = {}
    for v in data.get("violations", []):
        kinds[v["type"]] = kinds.get(v["type"], 0) + 1
    print("violations:", kinds or "none")
    print("unconnected:", len(data.get("unconnected_items", [])))
