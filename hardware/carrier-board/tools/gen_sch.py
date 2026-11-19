"""Generate carrier.kicad_sch from circuit.py.

Every pin gets a net label at its connection point instead of a drawn wire.
Labels with the same text are the same net, so connectivity is exact and the
sheet stays readable block by block.
"""
import copy
import os
import sys
import uuid

sys.path.insert(0, os.path.dirname(__file__))
from sexpr import Sym, dump, find, find1, parse  # noqa: E402
import circuit  # noqa: E402

SYMDIR = "/Applications/KiCad/KiCad.app/Contents/SharedSupport/symbols/"
OUT = os.path.join(os.path.dirname(__file__), "..", "carrier.kicad_sch")
NS = uuid.UUID("6f1b7c1e-4d0a-4a8e-9a39-1a2b3c4d5e6f")
ROOT = str(uuid.uuid5(NS, "root"))
PROJECT = "carrier"
G = 1.27  # schematic grid

_libs = {}


def U(*key):
    return str(uuid.uuid5(NS, "/".join(str(k) for k in key)))


def snap(v):
    return round(v / G) * G


def lib_symbol(lib_id):
    lib, name = lib_id.split(":")
    if lib not in _libs:
        tree = parse(open(SYMDIR + lib + ".kicad_sym").read())
        _libs[lib] = {s[1]: s for s in find(tree, "symbol")}
    syms = _libs[lib]
    s = syms[name]
    ext = find1(s, "extends")
    if not ext:
        out = copy.deepcopy(s)
    else:
        parent = syms[ext[1]]
        out = copy.deepcopy(parent)
        child_props = {p[1]: p for p in find(s, "property")}
        for i, x in enumerate(out):
            if isinstance(x, list) and x and x[0] == "property" and x[1] in child_props:
                out[i] = copy.deepcopy(child_props.pop(x[1]))
        for p in child_props.values():
            out.insert(2, copy.deepcopy(p))
        for x in out:
            if isinstance(x, list) and x and x[0] == "symbol":
                x[1] = x[1].replace(ext[1], name, 1)
    out[1] = lib_id
    return out


def pins_of(sym):
    pins = []
    for sub in find(sym, "symbol"):
        # sub-symbol names end in _<unit>_<style>; skip De Morgan style 2
        parts = sub[1].rsplit("_", 2)
        if len(parts) == 3 and parts[2] == "2":
            continue
        for p in find(sub, "pin"):
            at = find1(p, "at")
            pins.append(dict(num=find1(p, "number")[1], name=find1(p, "name")[1],
                             x=float(at[1]), y=float(at[2]), rot=int(float(at[3])) if len(at) > 3 else 0,
                             kind=p[1]))
    return pins


def label_angle(rot):
    return {0: 180, 180: 0, 90: 270, 270: 90}[rot]


def text_len(s):
    return len(s) * 1.05 + 2.5


def extents(p, pins):
    """Space a part needs around its origin: (left, right, up, down) in mm."""
    left = right = up = down = 2.54
    for pin in pins:
        net = net_for(p, pin)
        ln = text_len(net) if net else 2
        x, y = pin["x"], -pin["y"]
        if pin["rot"] == 0:
            left = max(left, -x + ln)
        elif pin["rot"] == 180:
            right = max(right, x + ln)
        elif pin["rot"] == 90:
            down = max(down, y + ln)
        else:
            up = max(up, -y + ln)
        left, right = max(left, -x + 1), max(right, x + 1)
        up, down = max(up, -y + 1), max(down, y + 1)
    return left, right + 9, up + 3, down + 3


def net_for(p, pin):
    pins = p["pins"]
    for key in (pin["num"], pin["name"]):
        if key in pins:
            return pins[key]
    return None


BLOCKS = {
    # name: (x, y, width, title)
    "power": (20, 40, 400, "ENTRADA 12 V, PROTECAO E FONTE 5 V"),
    "devkit": (440, 40, 170, "SOQUETE ESP32-S3-DevKitC-1"),
    "rtc": (630, 40, 190, "RTC DS3231 + I2C"),
    "ssr": (20, 470, 150, "SSR DO CANISTER"),
    "tft": (180, 470, 150, "VISOR TFT (TAMPA)"),
    "btn": (340, 470, 110, "BOTAO (TAMPA)"),
    "us": (460, 470, 200, "ULTRASSOM A02YYUW (GX12)"),
    "float": (670, 470, 150, "BOIA (GX12)"),
    "mech": (20, 560, 150, "FIXACAO"),
    "ch9": (840, 40, 330, "CANAL 9 - PONTE H (eletrolise tipo Chihiros Doctor ou peristaltica reversivel)"),
    "hb": (20, 620, 800, "HEARTBEAT - CORTE DO 12V DAS BOMBAS SE O FIRMWARE TRAVAR + MONITOR DE CORRENTE"),
}
for i in range(8):
    BLOCKS["ch%d" % (i + 1)] = (20 + (i % 4) * 202, 180 + (i // 4) * 140, 195,
                                "CANAL %d" % (i + 1))
BLOCKS["ch7"] = BLOCKS["ch7"][:3] + ("CANAL 7 - RECALQUE + REED (trava de nivel maximo)",)


def effects(size=1.27, hide=False, justify=None):
    e = [Sym("effects"), [Sym("font"), [Sym("size"), size, size]]]
    if justify:
        e.append([Sym("justify")] + [Sym(j) for j in justify])
    if hide:
        e.append([Sym("hide"), Sym("yes")])
    return e


def prop(key, val, x, y, hide=False, size=1.27, justify=None):
    return [Sym("property"), key, val, [Sym("at"), snap(x), snap(y), 0], effects(size, hide, justify)]


def main():
    items = []
    lib_syms = {}
    labels = []
    ncs = []
    texts = []

    by_block = {}
    for p in circuit.PARTS:
        by_block.setdefault(p["block"], []).append(p)

    for bname, parts in by_block.items():
        bx, by, bw, title = BLOCKS[bname]
        texts.append((title, bx, by - 8, 2.2))
        cx, cy, row_h = bx, by, 0
        for p in parts:
            if p["lib_id"] not in lib_syms:
                lib_syms[p["lib_id"]] = lib_symbol(p["lib_id"])
            pins = pins_of(lib_syms[p["lib_id"]])
            known = {pin["num"] for pin in pins} | {pin["name"] for pin in pins}
            bad = [k for k in p["pins"] if k not in known]
            if bad:
                raise SystemExit("%s %s: pin keys %s match no pin" % (p["ref"], p["lib_id"], bad))
            for pin in pins:
                if pin["num"] not in p["pins"] and pin["name"] not in p["pins"] and p["pins"]:
                    raise SystemExit("%s: pin %s/%s has no net and no explicit None" % (p["ref"], pin["num"], pin["name"]))
            l, r, u, d = extents(p, pins)
            if p["note"]:
                d += 5
                r = max(r, len(p["note"]) * 0.75 - l + 4)
            if cx + l + r > bx + bw and cx > bx:
                cx, cy, row_h = bx, cy + row_h + 4, 0
            ox, oy = snap(cx + l), snap(cy + u)
            p["sch_pos"] = (ox, oy)
            cx += l + r
            row_h = max(row_h, u + d)
            ref = p["ref"]
            sym_uuid = U("sym", ref)
            p["uuid"] = sym_uuid
            inst = [Sym("symbol"), [Sym("lib_id"), p["lib_id"]], [Sym("at"), ox, oy, 0],
                    [Sym("unit"), 1], [Sym("exclude_from_sim"), Sym("no")],
                    [Sym("in_bom"), Sym("yes" if p["bom"] else "no")], [Sym("on_board"), Sym("yes")],
                    [Sym("dnp"), Sym("no")], [Sym("uuid"), sym_uuid],
                    prop("Reference", ref, ox + 2.54, oy - 1.27, justify=["left"]),
                    prop("Value", p["value"], ox + 2.54, oy + 1.27, justify=["left"]),
                    prop("Footprint", p["footprint"], ox, oy, hide=True),
                    prop("Datasheet", "", ox, oy, hide=True)]
            if p["lcsc"]:
                inst.append(prop("LCSC", p["lcsc"], ox, oy, hide=True))
            for pin in pins:
                inst.append([Sym("pin"), pin["num"], [Sym("uuid"), U("pin", ref, pin["num"])]])
            inst.append([Sym("instances"), [Sym("project"), PROJECT,
                                            [Sym("path"), "/" + ROOT, [Sym("reference"), ref], [Sym("unit"), 1]]]])
            items.append(inst)
            seen = set()
            for pin in pins:
                px, py = snap(ox + pin["x"]), snap(oy - pin["y"])
                if (px, py) in seen:
                    continue  # stacked pins (e.g. several GND on one point)
                seen.add((px, py))
                net = net_for(p, pin)
                if net is None:
                    ncs.append((px, py))
                else:
                    labels.append((net, px, py, label_angle(pin["rot"])))
            if p["note"]:
                texts.append((p["note"], ox - l + 1, oy + d - 3, 1.0))

    # power flags, placed in a row under the power block
    fx = 30
    for net in circuit.POWER_FLAGS:
        lid = "power:PWR_FLAG"
        if lid not in lib_syms:
            lib_syms[lid] = lib_symbol(lid)
        ref = "#FLG%02d" % (circuit.POWER_FLAGS.index(net) + 1)
        x, y = snap(fx), snap(150)
        items.append([Sym("symbol"), [Sym("lib_id"), lid], [Sym("at"), x, y, 0], [Sym("unit"), 1],
                      [Sym("exclude_from_sim"), Sym("no")], [Sym("in_bom"), Sym("no")],
                      [Sym("on_board"), Sym("yes")], [Sym("dnp"), Sym("no")], [Sym("uuid"), U("flag", net)],
                      prop("Reference", ref, x, y - 5, hide=True), prop("Value", "PWR_FLAG", x, y - 3.8),
                      prop("Footprint", "", x, y, hide=True), prop("Datasheet", "", x, y, hide=True),
                      [Sym("pin"), "1", [Sym("uuid"), U("flagpin", net)]],
                      [Sym("instances"), [Sym("project"), PROJECT,
                                          [Sym("path"), "/" + ROOT, [Sym("reference"), ref], [Sym("unit"), 1]]]]])
        labels.append((net, x, y, 270))
        fx += 22
    texts.append(("PWR_FLAG (so para o ERC)", 30, 140, 1.27))

    doc = [Sym("kicad_sch"), [Sym("version"), 20231120], [Sym("generator"), "eeschema"],
           [Sym("generator_version"), "8.0"], [Sym("uuid"), ROOT], [Sym("paper"), "A0"],
           [Sym("title_block"), [Sym("title"), "IARA - placa base (carrier) ESP32-S3"],
            [Sym("rev"), "0.3"], [Sym("company"), "automacaoAquario"],
            [Sym("comment"), 1, "Gerado por tools/gen_sch.py a partir de tools/circuit.py - nao editar a mao"]],
           [Sym("lib_symbols")] + list(lib_syms.values())]
    for net, x, y, ang in labels:
        just = {0: ["left", "bottom"], 180: ["right", "bottom"], 90: ["left", "bottom"], 270: ["right", "bottom"]}[ang]
        doc.append([Sym("label"), net, [Sym("at"), x, y, ang], [Sym("fields_autoplaced"), Sym("yes")],
                    effects(1.27, justify=just), [Sym("uuid"), U("label", net, x, y)]])
    for x, y in ncs:
        doc.append([Sym("no_connect"), [Sym("at"), x, y], [Sym("uuid"), U("nc", x, y)]])
    for t, x, y, size in texts:
        doc.append([Sym("text"), t, [Sym("exclude_from_sim"), Sym("no")], [Sym("at"), snap(x), snap(y), 0],
                    effects(size, justify=["left", "bottom"]), [Sym("uuid"), U("text", t, x, y)]])
    doc.extend(items)
    doc.append([Sym("sheet_instances"), [Sym("path"), "/", [Sym("page"), "1"]]])
    with open(OUT, "w") as f:
        f.write(dump(doc) + "\n")
    print("wrote", os.path.abspath(OUT), len(circuit.PARTS), "parts", len(labels), "labels")


if __name__ == "__main__":
    main()
