#!/usr/bin/env python3
"""Regenerate the wiring diagrams in docs/diagrams/ — run: python3 scripts/gen_diagrams.py

The diagrams are generated rather than drawn so the three language sets cannot
drift: a wiring change is edited once, in the layout function, and the labels for
each language live in TEXT below. Editing an SVG by hand puts it out of sync with
the other two the moment the next change comes.

The drawing language: dark blocks are boards and modules, outlined blocks are
discrete components, wire colour says what the wire carries (live, neutral,
earth, ground, signal, passive). Every file is self-contained — no fonts, no
scripts, no external references — and carries its own light and dark palette, so
GitHub renders it correctly in either theme.
"""
import os

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "docs", "diagrams")

MONO = "ui-monospace, SFMono-Regular, Menlo, Consolas, monospace"
SANS = "system-ui, -apple-system, 'Segoe UI', Roboto, sans-serif"

STYLE = """
  svg {
    --bg: #ffffff;
    --ink: #16201a;
    --muted: #5b6a61;
    --edge: #c8d2c8;
    --node: #1d3a2c;
    --node-ink: #dceae1;
    --live: #b4362c;
    --neutral: #22598f;
    --earth: #2c7a51;
    --sig: #6a4b9c;
    --warm: #a96a09;
    --gnd: #55665c;
  }
  @media (prefers-color-scheme: dark) {
    svg {
      --bg: #11160f;
      --ink: #e6ece6;
      --muted: #9aa89f;
      --edge: #333f37;
      --node: #1f4130;
      --node-ink: #d8e6dd;
      --live: #e8776a;
      --neutral: #6ea8e0;
      --earth: #6cc493;
      --sig: #b294e6;
      --warm: #e5a63f;
      --gnd: #9aaaa0;
    }
  }
  .n { fill: var(--node); }
  .nt { fill: var(--node-ink); font-family: %s; font-size: 12px; font-weight: 600; }
  .ns { fill: var(--node-ink); font-family: %s; font-size: 11px; opacity: .72; }
  .c { fill: none; stroke: var(--edge); stroke-width: 1.5; }
  .ct { fill: var(--ink); font-family: %s; font-size: 11.5px; }
  .l { fill: var(--muted); font-family: %s; font-size: 11.5px; }
  .lb { fill: var(--ink); font-family: %s; font-size: 12px; font-weight: 600; }
  .w { fill: none; stroke-width: 2; stroke-linecap: round; stroke-linejoin: round; }
  .dot { stroke: none; }
""" % (MONO, SANS, SANS, SANS, SANS)


def textw(t, mono=True, size=12):
    """Rough advance width. Mono 12px is ~7.25 px/char; the sans labels are
    narrower, and CJK glyphs are full-width."""
    per = (0.60 if mono else 0.52) * size
    wide = sum(1 for c in t if ord(c) > 0x2E80)
    return (len(t) - wide) * per + wide * size


def boxw(title, sub=None, pad=30, minimum=110):
    w = textw(title, True, 12)
    if sub:
        w = max(w, textw(sub, False, 11))
    return max(minimum, round(w + pad))


def svg(width, height, body, aria):
    return (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" '
        f'width="{width}" height="{height}" role="img" aria-label="{esc(aria)}">\n'
        f'<style>{STYLE}</style>\n'
        f'<rect x="0" y="0" width="{width}" height="{height}" fill="var(--bg)"/>\n'
        f'{body}\n</svg>\n'
    )


def esc(t):
    return (t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
             .replace('"', "&quot;"))


def node(x, y, w, h, title, sub=None):
    """Dark block: a board or a module."""
    out = f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="6" class="n"/>'
    if sub:
        out += (f'<text x="{x + w/2}" y="{y + h/2 - 2}" text-anchor="middle" class="nt">{esc(title)}</text>'
                f'<text x="{x + w/2}" y="{y + h/2 + 13}" text-anchor="middle" class="ns">{esc(sub)}</text>')
    else:
        out += f'<text x="{x + w/2}" y="{y + h/2 + 4}" text-anchor="middle" class="nt">{esc(title)}</text>'
    return out


def comp(x, y, w, h, title, sub=None, color="var(--edge)"):
    """Outlined block: a discrete component."""
    out = (f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="4" class="c" '
           f'style="stroke: {color}"/>')
    if sub:
        out += (f'<text x="{x + w/2}" y="{y + h/2 - 2}" text-anchor="middle" class="ct">{esc(title)}</text>'
                f'<text x="{x + w/2}" y="{y + h/2 + 12}" text-anchor="middle" class="l">{esc(sub)}</text>')
    else:
        out += f'<text x="{x + w/2}" y="{y + h/2 + 4}" text-anchor="middle" class="ct">{esc(title)}</text>'
    return out


def wire(points, color, dash=None):
    d = " ".join(f"{x},{y}" for x, y in points)
    extra = f' stroke-dasharray="{dash}"' if dash else ""
    return f'<polyline points="{d}" class="w" style="stroke: {color}"{extra}/>'


def dot(x, y, color):
    return f'<circle cx="{x}" cy="{y}" r="3.5" class="dot" style="fill: {color}"/>'


def label(x, y, text, anchor="start", cls="l"):
    return f'<text x="{x}" y="{y}" text-anchor="{anchor}" class="{cls}">{esc(text)}</text>'


# ---------------------------------------------------------------- diagrams

def d_ac(L):
    b = []
    b.append(node(24, 300, boxw(L["iec"], L["iec_sub"]), 54, L["iec"], L["iec_sub"]))
    b.append(wire([(24 + boxw(L["iec"], L["iec_sub"]), 327), (200, 327)], "var(--muted)"))
    b.append(wire([(200, 70), (200, 340)], "var(--muted)"))

    def pair(y, key_lbl, key_a, key_b, color, up=24, down=24):
        """One conductor: chip label, then a split to two destinations."""
        out = [wire([(200, y), (330, y)], color), dot(200, y, color),
               label(210, y - 10, L[key_lbl], cls="lb")]
        wa, wb = boxw(L[key_a], L.get("psu_sub")), boxw(L[key_b])
        out.append(wire([(330, y), (330, y - up), (560, y - up)], color))
        out.append(wire([(330, y), (330, y + down), (560, y + down)], color))
        out.append(dot(330, y, color))
        out.append(node(560, y - up - 24, wa, 48, L[key_a], L["psu_sub"]))
        out.append(node(560, y + down - 21, wb, 42, L[key_b]))
        return out

    b += pair(70, "earth", "psu_e", "out_e", "var(--earth)", up=34, down=54)
    b += pair(210, "neutral", "psu_n", "out_n", "var(--neutral)", up=34, down=54)

    # live: fuse, then a split to the supply and to the relay
    b.append(wire([(200, 340), (330, 340)], "var(--live)"))
    b.append(dot(200, 340, "var(--live)"))
    b.append(label(210, 330, L["live"], cls="lb"))
    wf = boxw(L["fuse"], minimum=90)
    b.append(comp(330, 322, wf, 36, L["fuse"], color="var(--live)"))
    jx = 330 + wf + 50
    b.append(wire([(330 + wf, 340), (jx, 340)], "var(--live)"))
    b.append(wire([(jx, 306), (jx, 400)], "var(--live)"))
    b.append(dot(jx, 340, "var(--live)"))
    b.append(wire([(jx, 306), (560, 306)], "var(--live)"))
    b.append(node(560, 282, boxw(L["psu_l"], L["psu_sub"]), 48, L["psu_l"], L["psu_sub"]))

    ws = boxw(L["ssr"], L["ssr_sub"])
    b.append(wire([(jx, 400), (jx + 30, 400)], "var(--live)"))
    b.append(comp(jx + 30, 376, ws, 48, L["ssr"], L["ssr_sub"], color="var(--live)"))
    wo = boxw(L["out_l"])
    b.append(wire([(jx + 30 + ws, 400), (jx + 70 + ws, 400)], "var(--live)"))
    b.append(node(jx + 70 + ws, 379, wo, 42, L["out_l"]))

    b.append(label(24, 462, L["note"], cls="l"))
    width = max(880, jx + 70 + ws + wo + 24, 560 + boxw(L["psu_e"], L["psu_sub"]) + 24)
    return svg(width, 482, "\n".join(b), L["aria"])


def d_dc(L):
    b = []
    b.append(node(24, 24, 280, 56, L["psu"], L["psu_sub"]))
    # V+ rail
    b.append(wire([(80, 80), (80, 120)], "var(--live)"))
    b.append(label(90, 106, "V+", cls="lb"))
    wf = boxw(L["fuse"], minimum=110)
    b.append(comp(80 - wf / 2, 120, wf, 38, L["fuse"], color="var(--live)"))
    b.append(wire([(80, 158), (80, 200)], "var(--live)"))
    b.append(wire([(80, 200), (450, 200)], "var(--live)"))
    b.append(dot(80, 200, "var(--live)"))

    # V- rail down to the star bus, clear of the left-hand column
    b.append(wire([(250, 80), (250, 500)], "var(--gnd)"))
    b.append(label(260, 106, "V−", cls="lb"))
    b.append(wire([(150, 500), (450 + max(210, boxw(L["mosfet"], L["mosfet_sub"])) / 2 + 40 + boxw(L["pumps"], minimum=90) / 2, 500)], "var(--gnd)"))
    b.append(dot(250, 500, "var(--gnd)"))
    b.append(label(150, 526, L["star"], cls="l"))

    # 5 V branch: buck, bulk capacitance, ESP32
    b.append(wire([(80, 200), (80, 240)], "var(--live)"))
    b.append(node(24, 240, 190, 50, L["buck"], L["buck_sub"]))
    b.append(wire([(80, 290), (80, 322)], "var(--live)"))
    b.append(comp(24, 322, 190, 40, L["caps5v"], color="var(--warm)"))
    b.append(wire([(80, 362), (80, 400)], "var(--live)"))
    b.append(node(24, 400, 190, 50, L["esp"], L["esp_sub"]))
    b.append(wire([(150, 450), (150, 500)], "var(--gnd)"))
    b.append(dot(150, 500, "var(--gnd)"))

    # 12 V branch: local bulk capacitance, MOSFET module, pumps
    wm = max(210, boxw(L["mosfet"], L["mosfet_sub"]))
    b.append(wire([(450, 200), (450, 250)], "var(--live)"))
    b.append(comp(450 - 95, 250, 190, 40, L["cap12v"], color="var(--warm)"))
    b.append(wire([(450, 290), (450, 340)], "var(--live)"))
    b.append(node(450 - wm / 2, 340, wm, 62, L["mosfet"], L["mosfet_sub"]))
    b.append(wire([(450, 402), (450, 500)], "var(--gnd)"))
    b.append(dot(450, 500, "var(--gnd)"))

    wp = boxw(L["pumps"], minimum=90)
    px = 450 + wm / 2 + 40
    b.append(wire([(450 + wm / 2, 371), (px, 371)], "var(--live)"))
    b.append(node(px, 340, wp, 62, L["pumps"]))
    b.append(wire([(px + wp / 2, 402), (px + wp / 2, 500)], "var(--gnd)"))
    b.append(dot(px + wp / 2, 500, "var(--gnd)"))

    b.append(label(24, 556, L["sig_gnd"], cls="l"))
    return svg(max(760, px + wp + 24), 576, "\n".join(b), L["aria"])


def d_act(L):
    b = []
    b.append(node(24, 60, boxw(L["ch"], L["ch_sub"], minimum=168), 52, L["ch"], L["ch_sub"]))
    b.append(wire([(192, 86), (330, 86)], "var(--live)"))
    b.append(label(196, 72, L["wire12"], cls="l"))
    b.append(wire([(330, 86), (470, 86)], "var(--live)"))
    b.append(dot(330, 86, "var(--live)"))
    wp = max(130, boxw(L["pump"], L["pump_sub"]))
    b.append(node(470, 60, wp, 108, L["pump"], L["pump_sub"]))
    b.append(label(462, 82, "+", anchor="end", cls="lb"))
    b.append(label(462, 232, "−", anchor="end", cls="lb"))

    # flyback between the two rails, at the motor end
    b.append(wire([(330, 86), (330, 140)], "var(--warm)"))
    b.append(comp(330 - boxw(L["diode"], minimum=150)/2, 140, boxw(L["diode"], minimum=150), 40, L["diode"], color="var(--warm)"))
    b.append(wire([(330, 180), (330, 236)], "var(--warm)"))
    b.append(dot(330, 236, "var(--gnd)"))

    b.append(node(24, 210, boxw(L["gnd"], L["gnd_sub"], minimum=168), 52, L["gnd"], L["gnd_sub"]))
    b.append(wire([(192, 236), (470, 236)], "var(--gnd)"))
    b.append(wire([(470, 236), (470, 140)], "var(--gnd)"))

    b.append(label(24, 300, L["note"], cls="l"))
    return svg(max(640, 470 + wp + 24), 320, "\n".join(b), L["aria"])


def d_us(L):
    b = []
    b.append(node(24, 40, 150, 220, L["esp"], L["esp_sub"]))
    b.append(node(470, 40, 156, 220, L["sensor"], L["sensor_sub"]))

    rows = [
        (80,  "3V3", L["vcc"], "var(--live)"),
        (140, "GPIO34", L["tx"], "var(--sig)"),
        (196, "3V3", L["rx"], "var(--live)"),
        (240, "GND", L["gnd"], "var(--gnd)"),
    ]
    for y, pin, target, color in rows:
        b.append(label(186, y - 6, pin, cls="lb"))
        b.append(wire([(174, y), (470, y)], color))
        b.append(label(462, y - 6, target, anchor="end", cls="l"))

    # pull-up between 3V3 and GPIO34
    b.append(wire([(300, 80), (300, 104)], "var(--warm)"))
    b.append(dot(300, 80, "var(--warm)"))
    b.append(comp(252, 104, 96, 36, "10 kΩ", color="var(--warm)"))
    b.append(dot(300, 140, "var(--warm)"))

    b.append(label(24, 296, L["n1"], cls="l"))
    b.append(label(24, 318, L["n2"], cls="l"))
    b.append(label(24, 340, L["n3"], cls="l"))
    return svg(650, 360, "\n".join(b), L["aria"])


def d_reed(L):
    b = []
    # power path
    b.append(node(24, 40, 120, 46, "+12 V", L["psu"]))
    b.append(wire([(144, 63), (214, 63)], "var(--live)"))
    wr = max(150, boxw(L["reed"], L["reed_sub"]))
    b.append(comp(214, 42, wr, 42, L["reed"], L["reed_sub"], color="var(--live)"))
    b.append(wire([(214 + wr, 63), (470, 63)], "var(--live)"))
    b.append(node(470, 34, max(150, boxw(L["pump"], L["pump_sub"])), 92, L["pump"], L["pump_sub"]))
    b.append(label(458, 52, "+", anchor="end", cls="lb"))
    b.append(label(458, 124, "−", anchor="end", cls="lb"))
    b.append(wire([(470, 110), (400, 110), (400, 168)], "var(--gnd)"))
    b.append(node(250, 168, max(200, boxw(L["mod_out"], L["mod_out_sub"])), 52, L["mod_out"], L["mod_out_sub"]))

    # signal path
    b.append(node(24, 262, 150, 50, L["esp"], "GPIO33"))
    b.append(wire([(174, 287), (330, 287)], "var(--sig)"))
    b.append(node(330, 262, max(160, boxw(L["in7"], L["in7_sub"])), 50, L["in7"], L["in7_sub"]))
    b.append(dot(266, 287, "var(--sig)"))
    b.append(wire([(266, 287), (266, 330)], "var(--sig)"))
    b.append(comp(214, 330, 104, 34, "1 kΩ", color="var(--warm)"))
    b.append(wire([(266, 364), (266, 386)], "var(--sig)"))
    b.append(comp(214, 386, 104, 34, "100 nF", color="var(--warm)"))
    b.append(wire([(266, 420), (266, 448)], "var(--gnd)"))
    b.append(label(276, 452, L["mod_gnd"], cls="l"))

    b.append(label(24, 500, L["note"], cls="l"))
    return svg(max(660, 470 + max(150, boxw(L["pump"], L["pump_sub"])) + 24), 520, "\n".join(b), L["aria"])


def d_display(L):
    b = []
    b.append(node(24, 40, 160, 300, "ESP32", L["esp_sub"]))
    b.append(node(470, 40, 170, 300, "ST7735", L["disp_sub"]))
    rows = [
        (78,  "3V3", "VCC + LED", "var(--live)"),
        (120, "GND", "GND", "var(--gnd)"),
        (162, "D15", "CS", "var(--warm)"),
        (204, "D4",  "SDA", "var(--warm)"),
        (246, "D16", "SCK", "var(--warm)"),
        (288, "D17", "A0 / DC", "var(--warm)"),
        (330, "EN",  "RESET", "var(--sig)"),
    ]
    for y, pin, target, color in rows:
        b.append(label(196, y - 6, pin, cls="lb"))
        b.append(wire([(184, y), (470, y)], color))
        b.append(label(462, y - 6, target, anchor="end", cls="l"))
    # the grouped block
    b.append('<path d="M 652 62 L 664 62 L 664 306 L 652 306" fill="none" '
             'stroke="var(--warm)" stroke-width="1.5"/>')
    b.append(f'<text x="682" y="184" text-anchor="middle" class="l" '
             f'style="fill: var(--warm)" transform="rotate(90 682 184)">{esc(L["block"])}</text>')
    b.append(label(24, 380, L["n1"], cls="l"))
    b.append(label(24, 402, L["n2"], cls="l"))
    return svg(710, 420, "\n".join(b), L["aria"])


def d_float(L):
    b = []
    b.append(node(24, 40, 160, 54, "ESP32", "GPIO19"))
    b.append(wire([(184, 67), (280, 67)], "var(--sig)"))
    wf = max(140, boxw(L["float"], L["float_sub"]))
    b.append(comp(280, 46, wf, 42, L["float"], L["float_sub"], color="var(--sig)"))
    b.append(wire([(280 + wf, 67), (280 + wf + 60, 67)], "var(--gnd)"))
    b.append(node(280 + wf + 60, 40, 160, 54, "ESP32 GND", L["gnd_sub"]))
    b.append(label(24, 130, L["n1"], cls="l"))
    b.append(label(24, 152, L["n2"], cls="l"))
    return svg(280 + wf + 60 + 160 + 24, 172, "\n".join(b), L["aria"])


DIAGRAMS = [
    ("ac-entrada", d_ac),
    ("barramento-dc", d_dc),
    ("atuadores", d_act),
    ("ultrassonico", d_us),
    ("reed-nivel-maximo", d_reed),
    ("display-tft", d_display),
    ("boia-reservatorio", d_float),
]

TEXT = {
"pt": {
 "ac-entrada": dict(
   iec="TOMADA IEC C14", iec_sub="entrada AC",
   earth="TERRA (verde)", neutral="NEUTRO (azul)", live="FASE (marrom)",
   fuse="FUSÍVEL", ssr="RELÉ SSR", ssr_sub="parafuso 1 → interruptor → parafuso 2",
   psu_e="Borne G da fonte", psu_n="Borne N da fonte", psu_l="Borne L da fonte",
   psu_sub="fonte colmeia 12,53 V",
   out_e="Tomada canister · terra", out_n="Tomada canister · neutro",
   out_l="Tomada canister · fase",
   note="Todas as emendas de 110/220 V vão isoladas com termo-retrátil.",
   aria="Entrada AC: tomada IEC C14 dividindo terra, neutro e fase; a fase passa por fusível e pelo relé SSR antes da tomada do canister."),
 "barramento-dc": dict(
   psu="FONTE 12,53 V", psu_sub="colmeia",
   fuse="FUSÍVEL T5A", buck="LM2596", buck_sub="saída 5,1 V",
   caps5v="4 × 1000 µF 10 V", cap12v="1 × 470 µF 16 V",
   esp="ESP32", esp_sub="VIN / GND",
   mosfet="MÓDULO MOSFET", mosfet_sub="8 canais · VIN / GND",
   pumps="Bombas", star="GND de potência: estrela no borne V− da fonte",
   sig_gnd="GND de sinal é o oposto: boia e ultrassônico voltam ao pino GND do ESP32, nunca ao borne da fonte.",
   aria="Barramento DC: fonte de 12,53 V alimentando o LM2596 e o módulo MOSFET, com todos os retornos de potência em estrela no borne V menos."),
 "atuadores": dict(
   ch="CANAL MOSFET", ch_sub="saída do módulo",
   gnd="GND BARRAMENTO", gnd_sub="borne V− da fonte",
   pump="BOMBA", pump_sub="12 V",
   diode="DIODO FLYBACK", wire12="fio de 1,2 m",
   note="O diodo vai na ponta do fio, junto ao motor: no meio do cabo, o 1,2 m irradia o ruído como antena.",
   aria="Canal MOSFET e GND do barramento chegando à bomba por fios de 1,2 metro, com o diodo flyback instalado junto ao motor."),
 "ultrassonico": dict(
   esp="ESP32", esp_sub="3,3 V lógico",
   sensor="A02YYUW", sensor_sub="à prova d'água · UART",
   vcc="VCC (vermelho)", tx="TX (dados)", rx="RX (controle)", gnd="GND (preto)",
   n1="Nunca alimente com 5 V: o nível de saída acompanha o VCC e queima o GPIO34.",
   n2="O pull-up de 10 kΩ é obrigatório — GPIO34 é input-only e não tem pull-up interno.",
   n3="9600 8N1, frame de 4 bytes: 0xFF, DataH, DataL, checksum. Distância em milímetros.",
   aria="Sensor ultrassônico A02YYUW ligado ao ESP32: VCC em 3,3 volts, TX no GPIO34 com pull-up de 10 kilo-ohms, RX fixo em 3,3 volts e GND no pino da placa."),
 "reed-nivel-maximo": dict(
   psu="fonte", reed="REED NA", reed_sub="ímã presente = fechado",
   pump="BOMBA DE RECALQUE", pump_sub="12 V · 400 mA",
   mod_out="MÓDULO MOSFET · OUT−", mod_out_sub="canal 7",
   esp="ESP32", in7="IN7", in7_sub="entrada do canal 7",
   mod_gnd="GND do módulo",
   note="A trava fica na alimentação, não no sinal: MOSFET falha em curto, e aí cortar o gate não para a bomba.",
   aria="Reed switch normalmente aberto em série com o mais 12 volts da bomba de recalque, e o sinal do GPIO33 indo ao IN7 com resistor de 1 kilo-ohm e capacitor de 100 nanofarads para o GND do módulo."),
 "display-tft": dict(
   esp_sub="SPI por software", disp_sub="TFT 128×160 · 3,3 V",
   block="bloco de 6 pinos seguidos",
   n1="Os seis primeiros ficam lado a lado na base do header direito; só o RESET sai do bloco, no EN.",
   n2="O LED entra em ponte com o VCC: o backlight é fixo, sem GPIO.",
   aria="Display ST7735 ligado ao ESP32 com seis pinos seguidos na base do header direito e o RESET no pino EN."),
 "boia-reservatorio": dict(
   float="BOIA", float_sub="horizontal · reservatório",
   gnd_sub="pino da placa",
   n1="INPUT_PULLUP, ativo em LOW: reservatório cheio fecha o contato.",
   n2="Os dois fios saem juntos no mesmo cabo, o caminho todo, e o GND é o pino do ESP32 — nunca o borne da fonte.",
   aria="Boia do reservatório entre o GPIO19 e o pino GND do ESP32."),
},
"en": {
 "ac-entrada": dict(
   iec="IEC C14 INLET", iec_sub="AC input",
   earth="EARTH (green)", neutral="NEUTRAL (blue)", live="LIVE (brown)",
   fuse="FUSE", ssr="SSR RELAY", ssr_sub="screw 1 → switch → screw 2",
   psu_e="PSU terminal G", psu_n="PSU terminal N", psu_l="PSU terminal L",
   psu_sub="12.53 V supply",
   out_e="Canister socket · earth", out_n="Canister socket · neutral",
   out_l="Canister socket · live",
   note="Every 110/220 V joint goes inside heat-shrink.",
   aria="AC input: IEC C14 inlet splitting earth, neutral and live; live passes through the fuse and the SSR relay before the canister socket."),
 "barramento-dc": dict(
   psu="12.53 V SUPPLY", psu_sub="bench supply",
   fuse="T5A FUSE", buck="LM2596", buck_sub="5.1 V output",
   caps5v="4 × 1000 µF 10 V", cap12v="1 × 470 µF 16 V",
   esp="ESP32", esp_sub="VIN / GND",
   mosfet="MOSFET MODULE", mosfet_sub="8 channels · VIN / GND",
   pumps="Pumps", star="Power GND: star at the supply's V− terminal",
   sig_gnd="Signal GND is the opposite rule: the float and the ultrasonic return to the ESP32's own GND pin, never to the supply terminal.",
   aria="DC bus: the 12.53 volt supply feeding the LM2596 and the MOSFET module, with every power return starred at the V minus terminal."),
 "atuadores": dict(
   ch="MOSFET CHANNEL", ch_sub="module output",
   gnd="BUS GND", gnd_sub="supply V− terminal",
   pump="PUMP", pump_sub="12 V",
   diode="FLYBACK DIODE", wire12="1.2 m wire",
   note="The diode belongs at the far end, next to the motor: mid-cable, the 1.2 m run radiates the spike like an antenna.",
   aria="MOSFET channel and bus GND reaching the pump over 1.2 metre wires, with the flyback diode fitted at the motor."),
 "ultrassonico": dict(
   esp="ESP32", esp_sub="3.3 V logic",
   sensor="A02YYUW", sensor_sub="waterproof · UART",
   vcc="VCC (red)", tx="TX (data)", rx="RX (control)", gnd="GND (black)",
   n1="Never power it from 5 V: the output level follows VCC and would destroy GPIO34.",
   n2="The 10 kΩ pull-up is mandatory — GPIO34 is input-only and has no internal pull-up.",
   n3="9600 8N1, 4-byte frame: 0xFF, DataH, DataL, checksum. Distance in millimetres.",
   aria="A02YYUW ultrasonic sensor wired to the ESP32: VCC at 3.3 volts, TX on GPIO34 with a 10 kilo-ohm pull-up, RX tied to 3.3 volts and GND on the board pin."),
 "reed-nivel-maximo": dict(
   psu="supply", reed="REED NO", reed_sub="magnet present = closed",
   pump="REFILL PUMP", pump_sub="12 V · 400 mA",
   mod_out="MOSFET MODULE · OUT−", mod_out_sub="channel 7",
   esp="ESP32", in7="IN7", in7_sub="channel 7 input",
   mod_gnd="module GND",
   note="The interlock sits in the supply, not the signal: a MOSFET fails shorted, and cutting the gate then stops nothing.",
   aria="Normally-open reed switch in series with the refill pump's plus 12 volts, and the GPIO33 signal reaching IN7 with a 1 kilo-ohm resistor and a 100 nanofarad capacitor to module GND."),
 "display-tft": dict(
   esp_sub="software SPI", disp_sub="TFT 128×160 · 3.3 V",
   block="six pins in one run",
   n1="The first six sit side by side at the bottom of the right-hand header; only RESET leaves the group, on EN.",
   n2="LED bridges to VCC: the backlight is fixed, with no GPIO.",
   aria="ST7735 display wired to the ESP32 with six consecutive pins at the bottom of the right-hand header and RESET on the EN pin."),
 "boia-reservatorio": dict(
   float="FLOAT", float_sub="horizontal · reservoir",
   gnd_sub="board pin",
   n1="INPUT_PULLUP, active LOW: a full reservoir closes the contact.",
   n2="Both wires run together in the same cable the whole way, and GND is the ESP32's pin — never the supply terminal.",
   aria="Reservoir float switch between GPIO19 and the ESP32's GND pin."),
},
"ja": {
 "ac-entrada": dict(
   iec="IEC C14 インレット", iec_sub="AC入力",
   earth="アース（緑）", neutral="ニュートラル（青）", live="ライブ（茶）",
   fuse="ヒューズ", ssr="SSRリレー", ssr_sub="ネジ1 → スイッチ → ネジ2",
   psu_e="電源 G端子", psu_n="電源 N端子", psu_l="電源 L端子",
   psu_sub="12.53 V 電源",
   out_e="キャニスターコンセント・アース", out_n="キャニスターコンセント・ニュートラル",
   out_l="キャニスターコンセント・ライブ",
   note="110/220 Vの接続部はすべて熱収縮チューブで絶縁します。",
   aria="AC入力：IEC C14インレットからアース、ニュートラル、ライブへ分岐。ライブはヒューズとSSRリレーを経てキャニスターのコンセントへ。"),
 "barramento-dc": dict(
   psu="12.53 V 電源", psu_sub="スイッチング電源",
   fuse="T5A ヒューズ", buck="LM2596", buck_sub="出力 5.1 V",
   caps5v="4 × 1000 µF 10 V", cap12v="1 × 470 µF 16 V",
   esp="ESP32", esp_sub="VIN / GND",
   mosfet="MOSFETモジュール", mosfet_sub="8チャンネル・VIN / GND",
   pumps="ポンプ", star="電力系GND：電源のV−端子でスター接続",
   sig_gnd="信号系GNDは逆の規則です。フロートと超音波センサーはESP32のGNDピンへ戻し、電源端子へは決して戻しません。",
   aria="DCバス：12.53 V電源がLM2596とMOSFETモジュールに供給し、電力系の戻りはすべてV−端子でスター接続。"),
 "atuadores": dict(
   ch="MOSFETチャンネル", ch_sub="モジュール出力",
   gnd="バスGND", gnd_sub="電源 V−端子",
   pump="ポンプ", pump_sub="12 V",
   diode="フライバックダイオード", wire12="1.2 mのケーブル",
   note="ダイオードはモーター側の端に付けます。ケーブル途中では1.2 mがアンテナのようにノイズを放射します。",
   aria="MOSFETチャンネルとバスGNDが1.2 mのケーブルでポンプに届き、フライバックダイオードはモーター側に取り付けられている。"),
 "ultrassonico": dict(
   esp="ESP32", esp_sub="3.3 Vロジック",
   sensor="A02YYUW", sensor_sub="防水・UART",
   vcc="VCC（赤）", tx="TX（データ）", rx="RX（制御）", gnd="GND（黒）",
   n1="5 Vでは絶対に電源を取らないこと。出力レベルがVCCに追従し、GPIO34を破壊します。",
   n2="10 kΩのプルアップは必須です。GPIO34は入力専用で内部プルアップがありません。",
   n3="9600 8N1、4バイトフレーム：0xFF、DataH、DataL、チェックサム。距離はミリメートル。",
   aria="A02YYUW超音波センサーとESP32の配線：VCCは3.3 V、TXは10 kΩプルアップ付きでGPIO34、RXは3.3 V固定、GNDはボードのピン。"),
 "reed-nivel-maximo": dict(
   psu="電源", reed="リードスイッチ NO", reed_sub="磁石あり = 閉",
   pump="給水ポンプ", pump_sub="12 V・400 mA",
   mod_out="MOSFETモジュール・OUT−", mod_out_sub="チャンネル7",
   esp="ESP32", in7="IN7", in7_sub="チャンネル7入力",
   mod_gnd="モジュールGND",
   note="インターロックは信号ではなく電源側に入れます。MOSFETはショート状態で故障するため、ゲートを切っても止まりません。",
   aria="給水ポンプの+12 Vに直列に入ったノーマルオープンのリードスイッチと、1 kΩ抵抗と100 nFコンデンサを介してモジュールGNDへ落ちるGPIO33からIN7への信号。"),
 "display-tft": dict(
   esp_sub="ソフトウェアSPI", disp_sub="TFT 128×160・3.3 V",
   block="連続した6ピンのブロック",
   n1="最初の6本は右側ヘッダの下端に並びます。ブロックから出るのはENに行くRESETだけです。",
   n2="LEDはVCCに接続します。バックライトは常時点灯でGPIOを使いません。",
   aria="ST7735ディスプレイとESP32の配線：右側ヘッダ下端の連続した6ピンと、ENピンのRESET。"),
 "boia-reservatorio": dict(
   float="フロート", float_sub="水平型・リザーバー",
   gnd_sub="ボードのピン",
   n1="INPUT_PULLUP、LOWでアクティブ：リザーバーが満水になると接点が閉じます。",
   n2="2本の線は最後まで同じケーブルで通し、GNDはESP32のピンです。電源端子ではありません。",
   aria="リザーバーのフロートスイッチがGPIO19とESP32のGNDピンの間に接続されている。"),
},
}


def main():
    for lang, per_lang in TEXT.items():
        d = os.path.join(OUT, lang)
        os.makedirs(d, exist_ok=True)
        for name, fn in DIAGRAMS:
            content = fn(per_lang[name])
            with open(os.path.join(d, f"{name}.svg"), "w") as f:
                f.write(content)
            print(f"{lang}/{name}.svg  {len(content)} B")


if __name__ == "__main__":
    main()
