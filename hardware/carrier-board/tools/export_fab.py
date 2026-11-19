"""Write the fabrication package to ../fab/.

    fab/carrier-gerbers.zip   copper, mask, paste, silk, outline, drills
    fab/carrier-bom.csv       JLCPCB BOM: Comment, Designator, Footprint, LCSC
    fab/carrier-cpl.csv       JLCPCB pick-and-place: Designator, Mid X/Y, Layer, Rotation

Run with plain python3 after finalize.py. Parts marked bom=False in
circuit.py (test points, mounting holes) are left out of BOM and CPL.
"""
import csv
import os
import shutil
import subprocess
import sys
import zipfile

sys.path.insert(0, os.path.dirname(__file__))
import circuit  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
PCB = os.path.join(ROOT, "carrier.kicad_pcb")
FAB = os.path.join(ROOT, "fab")
GER = os.path.join(FAB, "gerbers")
CLI = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
LAYERS = "F.Cu,B.Cu,F.Paste,B.Paste,F.SilkS,B.SilkS,F.Mask,B.Mask,Edge.Cuts"


def cli(*args):
    r = subprocess.run([CLI] + list(args), capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(r.stderr)


shutil.rmtree(GER, ignore_errors=True)
os.makedirs(GER)
cli("pcb", "export", "gerbers", "--layers", LAYERS, "--subtract-soldermask", "-o", GER + "/", PCB)
cli("pcb", "export", "drill", "--format", "excellon", "--drill-origin", "absolute",
    "--excellon-units", "mm", "--generate-map", "--map-format", "gerberx2", "-o", GER + "/", PCB)
zpath = os.path.join(FAB, "carrier-gerbers.zip")
with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
    for f in sorted(os.listdir(GER)):
        z.write(os.path.join(GER, f), f)
print("gerbers:", len(os.listdir(GER)), "files ->", zpath)

# ------------------------------------------------------------------- BOM
groups = {}
for p in circuit.PARTS:
    if not p["bom"]:
        continue
    key = (p["footprint"].split(":")[1], p["lcsc"])
    groups.setdefault(key, []).append(p)


def refkey(r):
    head = r.rstrip("0123456789")
    return head, int(r[len(head):])


with open(os.path.join(FAB, "carrier-bom.csv"), "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
    for (fp, lcsc), parts in sorted(groups.items(), key=lambda kv: min(refkey(p["ref"]) for p in kv[1])):
        values = sorted({p["value"] for p in parts})
        comment = values[0] if len(values) == 1 else "Borne parafuso 3.5mm 2P" if lcsc == "C466970" \
            else "Conector " + lcsc
        w.writerow([comment, ",".join(sorted((p["ref"] for p in parts), key=refkey)), fp, lcsc])
print("bom:", len(groups), "lines,", sum(len(r) for r in groups.values()), "parts")
groups_refs = {p["ref"] for ps in groups.values() for p in ps}

# ------------------------------------------------------------------- CPL
bom_refs = {p["ref"] for p in circuit.PARTS if p["bom"]}
tmp = os.path.join(FAB, "_pos.csv")
cli("pcb", "export", "pos", "--format", "csv", "--units", "mm", "--side", "both", "-o", tmp, PCB)
rows = list(csv.DictReader(open(tmp)))
os.remove(tmp)
with open(os.path.join(FAB, "carrier-cpl.csv"), "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
    n = 0
    for r in sorted(rows, key=lambda r: refkey(r["Ref"])):
        if r["Ref"] not in bom_refs:
            continue
        w.writerow([r["Ref"], "%.4f" % float(r["PosX"]), "%.4f" % float(r["PosY"]),
                    "Top" if r["Side"].lower().startswith("top") else "Bottom", "%.1f" % float(r["Rot"])])
        n += 1
missing = bom_refs - {r["Ref"] for r in rows}
print("cpl:", n, "placements", ("MISSING " + ",".join(sorted(missing))) if missing else "")
