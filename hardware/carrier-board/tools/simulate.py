"""SPICE checks of the carrier board's safety-relevant sub-circuits.

Drives KiCad's bundled libngspice through ctypes, so nothing extra needs
installing. Component values are copied from circuit.py; MOSFET models are
level-1 fits to the datasheet R_DS(on) points, run at typical and worst-case
threshold. Writes PNG plots to ../sim/ and prints a pass/fail line per check.

    python3 tools/simulate.py
"""
import ctypes
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "sim")
LIB = "/Applications/KiCad/KiCad.app/Contents/Frameworks/libngspice.0.dylib"
VIN = 12.53  # measured supply

os.makedirs(OUT, exist_ok=True)
ng = ctypes.CDLL(LIB)
_log = []
PRINTF = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_void_p)
STAT = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_void_p)
EXIT = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_int, ctypes.c_bool, ctypes.c_bool, ctypes.c_int, ctypes.c_void_p)
_cb = (PRINTF(lambda s, i, p: _log.append(s.decode(errors="replace")) or 0),
       STAT(lambda s, i, p: 0), EXIT(lambda *a: 0))
ng.ngSpice_Init(_cb[0], _cb[1], _cb[2], None, None, None, None)

MODELS = """
.model BAT54 D(Is=2e-7 N=1.04 Rs=2.4 Cjo=10p Bv=30 Ibv=100u)
.model SS34 D(Is=5e-5 N=1.1 Rs=0.03 Cjo=200p Bv=40)
* AO3400A: Rds(on) 48 mOhm @ Vgs 2.5 V, 33 mOhm @ 4.5 V
.model AO3400A NMOS(Level=1 Vto={VTN} Kp=14 Lambda=0.01 Cgso=400p Cgdo=60p)
* AO4407A: Rds(on) ~14 mOhm @ Vgs -10 V
.model AO4407A PMOS(Level=1 Vto=-1.9 Kp=12 Lambda=0.01 Cgso=2.2n Cgdo=300p)
"""


def run(netlist, vectors):
    """Load a netlist, run it, return {vector: [values]} including the scale."""
    lines = ["* sim"] + netlist.strip().splitlines() + MODELS.strip().splitlines() + [".end"]
    arr = (ctypes.c_char_p * (len(lines) + 1))(*[l.encode() for l in lines], None)
    ng.ngSpice_Command(b"remcirc")
    ng.ngSpice_Circ(arr)
    ng.ngSpice_Command(b"run")
    path = os.path.join(OUT, "_tmp.txt")
    ng.ngSpice_Command(("wrdata %s %s" % (path, " ".join(vectors))).encode())
    data = {"x": []}
    for v in vectors:
        data[v] = []
    for row in open(path):
        cols = [float(c) for c in row.split()]
        if not cols:
            continue
        data["x"].append(cols[0])
        for i, v in enumerate(vectors):
            data[v].append(cols[2 * i + 1])
    os.remove(path)
    return data


results = []


def check(ok, msg):
    results.append(ok)
    print(("OK   " if ok else "FAIL ") + msg)


def first_cross(xs, ys, level, rising, after=0.0):
    for x, a, b in zip(xs[1:], ys, ys[1:]):
        if x > after and ((rising and a < level <= b) or (not rising and a > level >= b)):
            return x
    return None


# ----------------------------------------------------------------- 1. heartbeat
def heartbeat(freq, stop_at, vtn, tag):
    """GPIO14 toggles at `freq` Hz until `stop_at`, then holds high forever."""
    half = 0.5 / freq
    pts, t, lvl = ["0 0"], 0.0, 0.0
    while t + half < stop_at:
        t += half
        pts.append("%.6f %.1f" % (t, lvl))
        lvl = 3.3 - lvl
        pts.append("%.6f %.1f" % (t + 1e-6, lvl))
    pts.append("%.6f 3.3" % (t + 2e-6))
    net = """
.param VTN=%g
VIN vin 0 %g
VHB hb_src 0 PWL(%s)
RGPIO hb_src hb 25
R28 hb hb_r 100
C11 hb_r hb_ac 1u
DCL 0 hb_ac BAT54
DRC hb_ac hb_dc BAT54
C12 hb_dc 0 1u
R29 hb_dc 0 470k
M9 pull hb_dc 0 0 AO3400A
R30 pull pg 47k
R31 vin pg 100k
M10 pump pg vin vin AO4407A
C13 pump 0 10u
RLOAD pump 0 12
.tran 0.05m %g 0 0.05m
""" % (vtn, VIN, " ".join(pts), stop_at + 3.0)
    d = run(net, ["v(hb)", "v(hb_dc)", "v(pg)", "v(pump)"])
    on = first_cross(d["x"], d["v(pump)"], 0.9 * VIN, True)
    off = first_cross(d["x"], d["v(pump)"], 1.0, False, after=stop_at)
    steady = [v for x, v in zip(d["x"], d["v(pump)"]) if stop_at - 0.5 < x < stop_at]
    fig, ax = plt.subplots(3, 1, figsize=(10, 7), sharex=True)
    ax[0].plot(d["x"], d["v(hb)"], lw=0.5)
    ax[0].set_ylabel("GPIO14 (V)")
    ax[1].plot(d["x"], d["v(hb_dc)"], color="tab:orange")
    ax[1].axhline(vtn, ls="--", color="grey", lw=0.8)
    ax[1].set_ylabel("HB_DC (V)")
    ax[2].plot(d["x"], d["v(pump)"], color="tab:green")
    ax[2].set_ylabel("+12V_PUMP (V)")
    ax[2].set_xlabel("tempo (s)")
    for a in ax:
        a.axvline(stop_at, color="red", lw=0.8)
        a.grid(alpha=0.3)
    ax[0].set_title("Heartbeat %s: pisca a %g Hz ate %.0f s, depois trava em HIGH (linha vermelha)" % (tag, freq, stop_at))
    fig.tight_layout()
    fig.savefig(os.path.join(OUT, "heartbeat-%s.png" % tag), dpi=110)
    plt.close(fig)
    return on, off, (min(steady) if steady else 0.0)


print("\n1. HEARTBEAT (corte do 12V das bombas)")
for vtn, tag in ((1.05, "tipico"), (1.45, "pior-vth-alto"), (0.65, "pior-vth-baixo")):
    on, off, vmin = heartbeat(25, 3.0, vtn, tag)
    off_s = (off - 3.0) if off else None
    print("     Vth %.2f V: liga em %s, 12V_PUMP minimo pulsando %.2f V, corta %s apos travar" % (
        vtn, "%.0f ms" % (on * 1e3) if on else "NUNCA", vmin,
        "%.2f s" % off_s if off_s is not None else "NUNCA"))
    check(on is not None and on < 1.0, "Vth %.2f: bombas habilitam em menos de 1 s" % vtn)
    check(vmin > 0.95 * VIN, "Vth %.2f: 12V_PUMP estavel enquanto pisca (sem queda > 5%%)" % vtn)
    check(off_s is not None and off_s < 5.0, "Vth %.2f: corta em menos de 5 s com o GPIO parado" % vtn)

# a blocked loop that still toggles slowly must not hold the rail
on, off, vmin = heartbeat(1, 6.0, 1.05, "lento-1Hz")
check(vmin < 0.5 * VIN or on is None, "piscando a 1 Hz (loop travado) o 12V_PUMP nao se mantem: min %.2f V" % vmin)

# -------------------------------------------------------- 2. sense dividers
print("\n2. DIVISORES DE LEITURA (reed GPIO41, bombas GPIO21)")
for v12 in (VIN, 15.0):
    net = """
VIN v12 0 %g
V33 v33 0 3.3
R10 v12 reed_div 10k
R11 reed_div 0 3.3k
DA 0 reed_div BAT54
DK reed_div v33 BAT54
R12 reed_div gpio41 1k
RIN41 gpio41 0 10Meg
R33 v12 pump_div 22k
R34 pump_div 0 6.8k
R35 pump_div gpio21 1k
RIN21 gpio21 0 10Meg
.op
""" % v12
    lines = ["* op"] + net.strip().splitlines() + MODELS.replace("{VTN}", "1").strip().splitlines() + [".end"]
    arr = (ctypes.c_char_p * (len(lines) + 1))(*[l.encode() for l in lines], None)
    ng.ngSpice_Command(b"remcirc")
    ng.ngSpice_Circ(arr)
    ng.ngSpice_Command(b"run")
    _log.clear()
    ng.ngSpice_Command(b"print v(gpio41) v(gpio21)")
    vals = {}
    for l in _log:
        l = l.replace("stdout", "").strip()
        if "=" in l:
            k, v = l.split("=")
            vals[k.strip()] = float(v)
    g41, g21 = vals.get("v(gpio41)", 0), vals.get("v(gpio21)", 0)
    print("     12V = %.2f V: GPIO41 (reed) = %.2f V, GPIO21 (bombas) = %.2f V" % (v12, g41, g21))
    check(2.475 <= g41 <= 3.6, "reed com %.2f V: le HIGH com folga e abaixo do maximo do ESP (3,6 V)" % v12)
    check(2.475 <= g21 <= 3.6, "sensor de bombas com %.2f V: le HIGH e abaixo de 3,6 V" % v12)

# ------------------------------------------------- 3. channel gate + flyback
print("\n3. CANAL DE BOMBA (gate 3,3 V, PWM 5 kHz, flyback)")
for vtn, tag in ((1.05, "tipico"), (1.45, "pior")):
    net = """
.param VTN=%g
VIN vin 0 %g
VG g_src 0 PULSE(0 3.3 0 20n 20n 100u 200u)
RGPIO g_src ctl 25
R100 ctl g 100
R10K g 0 10k
RPTC vin pos 0.15
LPUMP pos m 1m
RPUMP m neg 16
DFLY neg pos SS34
M1 neg g 0 0 AO3400A
.tran 0.1u 4m 2m 0.05u
""" % (vtn, VIN)
    d = run(net, ["v(g)", "v(neg)", "i(vin)"])
    on_samples = sorted(v for v, g in zip(d["v(neg)"], d["v(g)"]) if g > 3.2)
    vds_on = on_samples[len(on_samples) // 2]  # median while the gate is fully on
    vpk = max(d["v(neg)"])
    i_pk = max(-i for i in d["i(vin)"])
    fig, ax = plt.subplots(2, 1, figsize=(10, 5), sharex=True)
    ax[0].plot([x * 1e3 for x in d["x"]], d["v(g)"])
    ax[0].set_ylabel("gate (V)")
    ax[1].plot([x * 1e3 for x in d["x"]], d["v(neg)"], color="tab:red")
    ax[1].set_ylabel("dreno (V)")
    ax[1].set_xlabel("tempo (ms)")
    ax[0].set_title("Canal de bomba, Vth %.2f V: PWM 5 kHz 50%%, bomba 16 ohm + 1 mH" % vtn)
    for a in ax:
        a.grid(alpha=0.3)
    fig.tight_layout()
    fig.savefig(os.path.join(OUT, "canal-%s.png" % tag), dpi=110)
    plt.close(fig)
    i_on = (VIN - vds_on) / 16.15
    print("     Vth %.2f V: Vds ligado %.0f mV (~%.0f mOhm), pico no dreno %.2f V, corrente de pico %.2f A" % (
        vtn, vds_on * 1e3, vds_on / max(i_on, 1e-3) * 1e3, vpk, i_pk))
    check(vds_on < 0.1, "Vth %.2f: MOSFET satura com 3,3 V no gate (Vds < 100 mV)" % vtn)
    check(vpk < VIN + 1.5, "Vth %.2f: flyback segura o pico no desligamento (< %.1f V, AO3400A aguenta 30 V)" % (vtn, VIN + 1.5))

print("\n%d de %d checagens OK. Graficos em %s" % (sum(results), len(results), os.path.abspath(OUT)))
sys.exit(0 if all(results) else 1)
