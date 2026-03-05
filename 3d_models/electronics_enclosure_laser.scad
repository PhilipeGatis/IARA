// ============================================================
// IARA - Caixa de Eletrônica (Versão Corte a Laser - DXF)
// Arquivo: electronics_enclosure_laser.scad
// Descrição: Painéis planificados 2D com finger-joints para
//            fabricação por corte a laser (acrílico/MDF 3mm)
// ============================================================
// Uso:
//   1. Abrir no OpenSCAD
//   2. Renderizar (F6)
//   3. Exportar: File > Export > Export as DXF
// ============================================================

$fn = 50;

// ============================================================
// PARÂMETROS GERAIS DA CAIXA
// ============================================================

box_width = 250; // mm (X)
box_depth = 220; // mm (Y)
box_height = 62; // mm (Z)
wall = 3; // mm - espessura das paredes
base_height = 62; // mm - altura da caixa
corner_r = 5; // mm - raio dos cantos arredondados

// -- Tampa de acrílico --
acrylic_thickness = 3; // mm
acrylic_tolerance = 0.5; // mm - folga para encaixe

// -- Furos de fixação da tampa --
screw_d = 3.2; // mm - M3

// ============================================================
// PARÂMETROS DOS COMPONENTES INTERNOS
// ============================================================

psu_w = 199;
psu_d = 98;
psu_h = 42;

esp32_w = 51;
esp32_d = 28;

mosfet_w = 99;
mosfet_d = 52;

lm2596_w = 61;
lm2596_d = 34;

ssr_w = 25;
ssr_d = 34;

rtc_w = 38;
rtc_d = 22;

tft_screen_w = 35;
tft_screen_h = 28;
tft_board_w = 56;
tft_board_h = 34;
tft_mount_holes_spacing_w = 52;
tft_mount_holes_spacing_h = 30;
tft_mount_d = 2.2;

ultra_w = 41;
ultra_d = 28.5;

// ============================================================
// PARÂMETROS DOS CONECTORES
// ============================================================

nbr_outlet_d = 37;
pump_conn_d = 8;
pump_conn_qty = 8;

rj45_w = 16;
rj45_h = 14;
rj45_qty = 3;

// -- Botão de painel (push button momentâneo 12mm) --
btn_panel_d = 12; // mm - diâmetro do furo de montagem

// -- Ventilação --
vent_slot_w = 2;
vent_slot_l = 20;
vent_spacing = 4;
vent_qty = 8;

// ============================================================
// PARÂMETROS DE FINGER-JOINT
// ============================================================

mat_t = wall; // espessura do material (= 3mm)
fj_tooth = 10; // comprimento de cada dente
fj_kerf = 0.1; // compensação de kerf do laser (cada lado)

// ============================================================
// MÓDULOS UTILITÁRIOS
// ============================================================

// --- Gerador de borda com finger-joints (2D) ---
module fj_edge(edge_len, teeth = "tabs") {
  n_teeth = floor(edge_len / (fj_tooth * 2));
  actual_tooth = edge_len / (2 * n_teeth + 1);
  k = fj_kerf;

  if (teeth == "slots") {
    for (i = [0:n_teeth - 1]) {
      translate([actual_tooth + i * 2 * actual_tooth - k, -k])
        square([actual_tooth + 2 * k, mat_t + 2 * k]);
    }
  } else {
    for (i = [0:n_teeth - 1]) {
      translate([i * 2 * actual_tooth - k, -k])
        square([actual_tooth + 2 * k, mat_t + 2 * k]);
    }
    translate([n_teeth * 2 * actual_tooth - k, -k])
      square([actual_tooth + 2 * k, mat_t + 2 * k]);
  }
}

// --- Furos de standoff para módulos (2D) ---
module standoff_holes_2d(w, d, hole_d = 2.5) {
  inset = 2.5;
  if (w < 40) {
    positions = [
      [-w / 2 + inset, -d / 2 + inset],
      [w / 2 - inset, d / 2 - inset],
    ];
    for (pos = positions)
      translate([pos[0], pos[1]])
        circle(d=hole_d, $fn=20);
  } else {
    positions = [
      [-w / 2 + inset, -d / 2 + inset],
      [w / 2 - inset, -d / 2 + inset],
      [-w / 2 + inset, d / 2 - inset],
      [w / 2 - inset, d / 2 - inset],
    ];
    for (pos = positions)
      translate([pos[0], pos[1]])
        circle(d=hole_d, $fn=20);
  }
}

// ============================================================
// POSIÇÕES DOS PARAFUSOS
// ============================================================
function screw_positions() =
  [
    [-box_width / 2 + 10, -box_depth / 2 + 10],
    [box_width / 2 - 10, -box_depth / 2 + 10],
    [-box_width / 2 + 10, box_depth / 2 - 10],
    [box_width / 2 - 10, box_depth / 2 - 10],
  ];

// ============================================================
// PAINEL DO FUNDO (base_2d_bottom)
// ============================================================
module base_2d_bottom() {
  difference() {
    square([box_width, box_depth], center=true);

    // --- Finger-joint slots nas 4 bordas ---
    translate([-box_width / 2, box_depth / 2 - mat_t])
      fj_edge(box_width, "slots");
    translate([-box_width / 2, -box_depth / 2])
      fj_edge(box_width, "slots");
    translate([-box_width / 2 + mat_t, -box_depth / 2])
      rotate([0, 0, 90])
        fj_edge(box_depth, "slots");
    translate([box_width / 2, -box_depth / 2])
      rotate([0, 0, 90])
        fj_edge(box_depth, "slots");

    // --- Furos de montagem dos módulos (espaçadores M2.5) ---
    translate([-70, 48])
      standoff_holes_2d(mosfet_d, mosfet_w);
    translate([40, 45])
      standoff_holes_2d(esp32_w, esp32_d);
    translate([-20, 55])
      standoff_holes_2d(lm2596_d, lm2596_w);
    translate([40, 85])
      standoff_holes_2d(ultra_w, ultra_d);
    translate([90, 30])
      standoff_holes_2d(ssr_d, ssr_w);
    translate([40, 8])
      standoff_holes_2d(rtc_w, rtc_d);

    // --- Furos parafusos tampa M3 ---
    for (pos = screw_positions())
      translate([pos[0], pos[1]])
        circle(d=screw_d + 0.3, $fn=20);

    // --- Ventilação no fundo (sob a fonte) ---
    for (i = [0:7]) {
      tw = 8 * vent_slot_w + 7 * vent_spacing;
      translate(
        [
          -tw / 2 + i * (vent_slot_w + vent_spacing),
          -box_depth / 2 + wall + 2 + psu_d / 2 - vent_slot_l / 2,
        ]
      )
        square([vent_slot_w, vent_slot_l]);
    }

    // --- Furos abas de montagem na parede ---
    translate([-box_width / 2 - 15 / 2, 0])
      circle(d=4.5, $fn=20);
    translate([box_width / 2 + 15 / 2, 0])
      circle(d=4.5, $fn=20);
  }

  // --- Abas de montagem (integradas ao fundo) ---
  mount_tab_w = 20;
  mount_tab_h = 15;
  difference() {
    translate([-box_width / 2 - mount_tab_h, -mount_tab_w / 2])
      square([mount_tab_h, mount_tab_w]);
    translate([-box_width / 2 - mount_tab_h / 2, 0])
      circle(d=4.5, $fn=20);
  }
  difference() {
    translate([box_width / 2, -mount_tab_w / 2])
      square([mount_tab_h, mount_tab_w]);
    translate([box_width / 2 + mount_tab_h / 2, 0])
      circle(d=4.5, $fn=20);
  }
}

// ============================================================
// PAINEL FRONTAL (base_2d_front) — Sensores
// ============================================================
module base_2d_front() {
  panel_w = box_width;
  panel_h = base_height - mat_t;

  difference() {
    square([panel_w, panel_h], center=true);

    // --- Finger-joints bordas laterais ---
    translate([-panel_w / 2 + mat_t, -panel_h / 2])
      rotate([0, 0, 90])
        fj_edge(panel_h, "tabs");
    translate([panel_w / 2, -panel_h / 2])
      rotate([0, 0, 90])
        fj_edge(panel_h, "tabs");

    // --- Finger-joints borda inferior ---
    translate([-panel_w / 2, -panel_h / 2])
      fj_edge(panel_w, "tabs");

    // --- Recortes sensores ---
    rj45_spacing = rj45_w + 8;
    sensor_z_2d = psu_h - 4;

    translate([-(rj45_spacing) - rj45_w / 2, sensor_z_2d - panel_h / 2])
      square([rj45_w, rj45_h]);
    translate([-8 / 2, sensor_z_2d - panel_h / 2])
      square([8, 6]);
    translate([rj45_spacing - rj45_w / 2, sensor_z_2d - panel_h / 2])
      square([rj45_w, rj45_h]);

    // --- Furo botão de painel (12mm) ---
    btn_x = rj45_spacing + rj45_w / 2 + 20;
    btn_z = sensor_z_2d + rj45_h / 2 - panel_h / 2;
    translate([btn_x, btn_z])
      circle(d=btn_panel_d, $fn=40);
  }
}

// ============================================================
// PAINEL TRASEIRO (base_2d_back) — Ventilação
// ============================================================
module base_2d_back() {
  panel_w = box_width;
  panel_h = base_height - mat_t;

  difference() {
    square([panel_w, panel_h], center=true);

    // --- Finger-joints bordas laterais ---
    translate([-panel_w / 2 + mat_t, -panel_h / 2])
      rotate([0, 0, 90])
        fj_edge(panel_h, "tabs");
    translate([panel_w / 2, -panel_h / 2])
      rotate([0, 0, 90])
        fj_edge(panel_h, "tabs");

    // --- Finger-joints borda inferior ---
    translate([-panel_w / 2, -panel_h / 2])
      fj_edge(panel_w, "tabs");

    // --- Slots de ventilação traseira ---
    for (row = [0:2]) {
      vent_z = 8 + row * 14;
      translate([-60, vent_z - panel_h / 2])
        square([120, 3]);
    }
  }
}

// ============================================================
// PAINEL ESQUERDO (base_2d_left) — Bombas DC
// ============================================================
module base_2d_left() {
  panel_w = box_depth;
  panel_h = base_height - mat_t;

  difference() {
    square([panel_w, panel_h], center=true);

    // --- Finger-joints borda inferior (encaixa no fundo) ---
    translate([-panel_w / 2, -panel_h / 2])
      fj_edge(panel_w, "tabs");

    // --- Finger-joints bordas verticais (encaixam no frontal/traseiro) ---
    translate([-panel_w / 2 + mat_t, -panel_h / 2])
      rotate([0, 0, 90])
        fj_edge(panel_h, "slots");
    translate([panel_w / 2, -panel_h / 2])
      rotate([0, 0, 90])
        fj_edge(panel_h, "slots");

    // --- Furos 8× bombas P4 ---
    cols = 2;
    rows = 4;
    col_spacing = 25;
    row_spacing = pump_conn_d + 6;
    start_z = 10;

    for (col = [0:cols - 1])
      for (row = [0:rows - 1]) {
        y_offset = -pump_conn_qty / rows * row_spacing / 2 + row * row_spacing + row_spacing / 2;
        z_offset = start_z + col * col_spacing;
        translate([y_offset, z_offset - panel_h / 2])
          circle(d=pump_conn_d, $fn=30);
      }
  }
}

// ============================================================
// PAINEL DIREITO (base_2d_right) — AC IN + CANISTER
// ============================================================
module base_2d_right() {
  panel_w = box_depth;
  panel_h = base_height - mat_t;

  difference() {
    square([panel_w, panel_h], center=true);

    // --- Finger-joints borda inferior (encaixa no fundo) ---
    translate([-panel_w / 2, -panel_h / 2])
      fj_edge(panel_w, "tabs");

    // --- Finger-joints bordas verticais (encaixam no frontal/traseiro) ---
    translate([-panel_w / 2 + mat_t, -panel_h / 2])
      rotate([0, 0, 90])
        fj_edge(panel_h, "slots");
    translate([panel_w / 2, -panel_h / 2])
      rotate([0, 0, 90])
        fj_edge(panel_h, "slots");

    // --- Tomada AC AS-08A (31.2 × 27.2mm) ---
    ac_x = box_depth / 4 + 25;
    ac_z = base_height / 2 + 2 - mat_t;
    translate([ac_x - 27.2 / 2, ac_z - panel_h / 2 - 31.2 / 2])
      square([27.2, 31.2]);

    // --- Tomada NBR 14136 CANISTER (40.5 × 21.7mm) ---
    can_x = 25;
    can_z = base_height / 2 + 2 - mat_t;
    translate([can_x - 40.5 / 2, can_z - panel_h / 2 - 21.7 / 2])
      square([40.5, 21.7]);
  }
}

// ============================================================
// TAMPA 2D (lid_2d) — Template para corte laser
// ============================================================
module lid_2d() {
  tft_x_offset = 40;
  tft_y_offset = 45;
  difference() {
    // Contorno com cantos arredondados
    hull() {
      for (
        x = [
          -(box_width - acrylic_tolerance) / 2 + corner_r,
          (box_width - acrylic_tolerance) / 2 - corner_r,
        ]
      )
        for (
          y = [
            -(box_depth - acrylic_tolerance) / 2 + corner_r,
            (box_depth - acrylic_tolerance) / 2 - corner_r,
          ]
        )
          translate([x, y]) circle(r=corner_r);
    }

    // Abertura TFT
    translate([tft_x_offset - tft_screen_w / 2, tft_y_offset - tft_screen_h / 2])
      square([tft_screen_w, tft_screen_h]);

    // Furos TFT M2
    for (dx = [-tft_mount_holes_spacing_w / 2, tft_mount_holes_spacing_w / 2])
      for (dy = [-tft_mount_holes_spacing_h / 2, tft_mount_holes_spacing_h / 2])
        translate([tft_x_offset + dx, tft_y_offset + dy])
          circle(d=tft_mount_d);

    // Furos M3 fixação
    for (pos = screw_positions())
      translate([pos[0], pos[1]]) circle(d=screw_d + 0.3);

    // Furo botão de painel (12mm) — ao lado do TFT
    translate([tft_x_offset + tft_screen_w / 2 + 15, tft_y_offset])
      circle(d=btn_panel_d, $fn=40);

    // Ventilação
    for (side = [1, -1]) {
      for (i = [0:vent_qty - 1]) {
        tw = vent_qty * vent_slot_w + (vent_qty - 1) * vent_spacing;
        translate(
          [
            side * box_width / 4 - tw / 2 + i * (vent_slot_w + vent_spacing),
            -box_depth / 4 + tft_board_h / 2 + 10 - vent_slot_l / 2,
          ]
        )
          square([vent_slot_w, vent_slot_l]);
      }
    }
  }
}

// ============================================================
// LAYOUT COMPACTO — BASE (5 painéis em linhas paralelas)
// ============================================================
// Peças organizadas para minimizar o bounding box da chapa.
// Layout:
//   Linha 1: Fundo (280 × 220mm com abas)
//   Linha 2: Frontal (250 × 59) + Esquerdo (220 × 59)
//   Linha 3: Traseiro (250 × 59) + Direito (220 × 59)
// ============================================================
module base_2d() {
  spacing = 8; // mm entre peças
  panel_h = base_height - mat_t; // 59mm — altura dos painéis laterais
  mount_tab_h = 15;
  bottom_total_w = box_width + 2 * mount_tab_h; // 280mm com abas

  // --- Linha 1: Fundo (centro) ---
  base_2d_bottom();

  // --- Linha 2: Frontal + Esquerdo (acima do fundo) ---
  row2_y = box_depth / 2 + panel_h / 2 + spacing;

  // Frontal (250mm) — alinhado à esquerda
  translate([-(box_depth + spacing) / 4, row2_y])
    base_2d_front();

  // Esquerdo (220mm) — ao lado do frontal
  translate([box_width / 2 + spacing / 2 + box_depth / 2 - (box_depth + spacing) / 4, row2_y])
    base_2d_left();

  // --- Linha 3: Traseiro + Direito (abaixo do fundo) ---
  row3_y = -box_depth / 2 - panel_h / 2 - spacing;

  // Traseiro (250mm) — alinhado à esquerda
  translate([-(box_depth + spacing) / 4, row3_y])
    base_2d_back();

  // Direito (220mm) — ao lado do traseiro
  translate([box_width / 2 + spacing / 2 + box_depth / 2 - (box_depth + spacing) / 4, row3_y])
    base_2d_right();
}

// ============================================================
// LAYOUT OTIMIZADO — TUDO (mínimo de material)
// ============================================================
// Duas colunas lado a lado para minimizar chapa.
// Layout:
//   Col 1: Fundo (250×220) → Frontal (250×59) → Traseiro (250×59)
//   Col 2: Tampa (~250×220) → Esquerdo (220×59) → Direito (220×59)
// ============================================================
module all_2d() {
  spacing = 8; // mm entre peças
  panel_h = base_height - mat_t; // 59mm
  mount_tab_h = 15;

  // --- Coluna 1 (esquerda): Fundo + painéis maiores (250mm) ---
  col1_x = 0;

  // Fundo (em cima)
  translate([col1_x, 0])
    base_2d_bottom();

  // Frontal (abaixo do fundo)
  translate([col1_x, -box_depth / 2 - spacing - panel_h / 2])
    base_2d_front();

  // Traseiro (abaixo do frontal)
  translate([col1_x, -box_depth / 2 - spacing - panel_h - spacing - panel_h / 2])
    base_2d_back();

  // --- Coluna 2 (direita): Tampa + painéis menores (220mm) ---
  col2_x = box_width / 2 + mount_tab_h + spacing + (box_width - acrylic_tolerance) / 2;

  // Tampa (em cima, alinhada com o fundo)
  translate([col2_x, 0])
    lid_2d();

  // Esquerdo (abaixo da tampa)
  translate([col2_x, -box_depth / 2 - spacing - panel_h / 2])
    base_2d_left();

  // Direito (abaixo do esquerdo)
  translate([col2_x, -box_depth / 2 - spacing - panel_h - spacing - panel_h / 2])
    base_2d_right();
}

// ============================================================
// RENDERIZAR
// ============================================================
// Descomente a linha desejada e exporte como DXF:

// base_2d(); // Painéis da base (5 peças compactas)
// lid_2d();   // Somente a tampa
all_2d(); // Base + tampa juntos

// ============================================================
// NOTAS DE FABRICAÇÃO
// ============================================================
// Material: Acrílico 3mm ou MDF 3mm
// Kerf: ajustar fj_kerf conforme a máquina (~0.05 a 0.15mm)
// Montagem:
//   - Painéis laterais encaixam no fundo via finger-joints
//   - Cola: cloreto de metileno (acrílico) ou cola branca (MDF)
//   - Módulos: fixar com espaçadores M2.5 metálicos nos furos
//   - Tampa: 4× parafusos M3×8mm + porcas
// ============================================================
