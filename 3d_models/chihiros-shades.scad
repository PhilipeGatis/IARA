// Arquivo gerado automaticamente
// =========================================================
// MODO DE VISUALIZAÇÃO E IMPRESSÃO
// =========================================================
modo_impressao = true; // true = Sólido de impressão limpo, false = Wireframe de debug
comprimento = 200; // Comprimento total da peça em mm (ex: 300 = 30cm, 200 = 20cm)

y_ini = 0.5;
y_fim = comprimento - 0.5;
y_clip_ini = 12.5; // Início da transição do clipe (fixo, perto da borda)
y_clip_fim = comprimento - 12.5; // Fim da transição do clipe (espelhado)
y_clip_reto_ini = 15.5; // Início do miolo reto do clipe
y_clip_reto_fim = comprimento - 15.5; // Fim do miolo reto do clipe

pontos = [
  // [0, 1] Ponta esquerda extrema (Vermelho)
  [0.4216, y_ini, 4.3713],
  [0.4216, y_fim, 4.3713],
  // [2, 3] Base interna (Verde)
  [4.2929, y_ini, 0.5000],
  [4.2929, y_fim, 0.5000],
  // [4, 5] Base externa (Azul)
  [81.7426, y_ini, 0.5000],
  [81.7426, y_fim, 0.5000],
  // [6, 7] Lábio externo topo (Amarelo)
  [81.7426, y_ini, 2.0000],
  [81.7426, y_fim, 2.0000],
  // [8, 9] Lábio externo base (Magenta)
  [80.2426, y_ini, 2.0000],
  [80.2426, y_fim, 2.0000],
  // [10, 11] Chanfro externo topo (Azul Claro / Cyan)
  [78.2071, y_ini, 5.5355],
  [78.2071, y_fim, 5.5355],
  // [12, 13] Chanfro externo plano (Laranja)
  [76.7071, y_ini, 5.5355],
  [76.7071, y_fim, 5.5355],
  // [14, 15] Parede vertical base (Roxo)
  [17.1213, y_ini, 2.0000],
  [17.1213, y_fim, 2.0000],
  // [16, 17] Parede vertical topo (exterior) (Marrom)
  [17.1213, y_ini, 11.8787],
  [17.1213, y_fim, 11.8787],
  // [18, 19] Parede vertical topo (interior / início da rampa) (Pink)
  [15.0000, y_ini, 11.8787],
  [15.0000, y_fim, 11.8787],
  // [20, 21] Clip face de encaixe (base do dente) (Lime)
  [6.5355, y_clip_reto_ini, 8.3640],
  [6.5355, y_clip_reto_fim, 8.3640],
  // [22, 23] Clip face de encaixe (topo do dente) (Navy)
  [5.4749, y_clip_reto_ini, 9.4246],
  [5.4749, y_clip_reto_fim, 9.4246],
  // [24, 25] Transição do Clip (Gold)
  [4.5910, y_clip_ini, 6.4194],
  [4.5910, y_clip_fim, 6.4194],
  // [26, 27] Transição do Clip (Teal)
  [3.5303, y_clip_ini, 7.4801],
  [3.5303, y_clip_fim, 7.4801],
  // [28, 29] Transição do Clip (Maroon)
  [2.6464, y_clip_ini, 4.4749],
  [2.6464, y_clip_fim, 4.4749],
  // [30, 31] Pé da rampa (Olive)
  [5.1213, y_ini, 2.0000],
  [5.1213, y_fim, 2.0000],
  // [32, 33, 34, 35] Chanfro interno topo (Black)
  [1.5858, y_ini, 5.5355],
  [1.5858, y_clip_ini, 5.5355],
  [1.5858, y_clip_fim, 5.5355],
  [1.5858, y_fim, 5.5355],

  // NOVOS PONTOS PARA A CAVA DO ACRÍLICO (PAREDE DIREITA RETA)
  // [36, 37] Parede Direita - Topo Externo
  [81.7426, y_ini, 4.0000],
  [81.7426, y_fim, 4.0000],
  // [38, 39] Parede Direita - Topo Interno
  [80.2426, y_ini, 4.0000],
  [80.2426, y_fim, 4.0000],
];

// =========================================================
// DEFINIÇÃO DE MÓDULOS (OpenSCAD exige que módulos fiquem fora de if/else)
// =========================================================

module objeto_final_impressao(tol_clip = 0, aumento_diag = 0, aumento_rampa = 0, cor_corpo = "LightGray", cor_clipe = "HotPink") {
  function aplicar_tolerancia(p, t) =
    [
      [p[0][0] + t * 0.7071, p[0][1] - t * 0.7071], // Maroon 
      p[1], // Black 
      p[2], // Navy/Teal 
      [p[3][0] + t * 0.7071, p[3][1] - t * 0.7071], // Lime/Gold 
    ];

  module cunha_transicao(y_start, y_end, p1, p2) {
    hull() {
      translate([0, y_start, 0]) rotate([90, 0, 0]) linear_extrude(0.01) polygon(aplicar_tolerancia(p1, tol_clip));
      translate([0, y_end, 0]) rotate([90, 0, 0]) linear_extrude(0.01) polygon(aplicar_tolerancia(p2, tol_clip));
    }
  }

  translate([0, 0, -0.5])
    union() {
      // 1. CORPO PRINCIPAL (Extrusão do perfil frontal)
      // O retângulo base é esticado e engrossado movendo Red e Black na diagonal
      perfil_xy = [
        [pontos[0][0] - aumento_diag, pontos[0][2] + aumento_diag], // 0: Red
        [pontos[2][0], pontos[2][2]], // 2: Green (fixo)
        [pontos[4][0], pontos[4][2]], // 4: Blue
        [pontos[36][0], pontos[36][2]], // 36: Parede dir topo ext 
        [pontos[38][0], pontos[38][2]], // 38: Parede dir topo int 
        [pontos[8][0], pontos[8][2]], // 8: Magenta 
        [pontos[14][0], pontos[14][2]], // 14: Purple 
        [pontos[16][0], pontos[16][2]], // 16: Brown 
        [pontos[18][0], pontos[18][2]], // 18: Pink
        [pontos[30][0] + aumento_rampa, pontos[30][2] + aumento_rampa], // 30: Olive (Move na diagonal oposta para engrossar a rampa)
        [pontos[32][0] - aumento_diag + aumento_rampa, pontos[32][2] + aumento_diag + aumento_rampa], // 32: Black (Soma os dois movimentos)
      ];

      color(cor_corpo) {
        translate([0, y_fim, 0]) rotate([90, 0, 0]) linear_extrude(height=y_fim - y_ini) polygon(perfil_xy);
      }

      // 2. O CLIPE
      // O topo do clipe (Navy, Lime) acompanha apenas o alargamento base (aumento_diag)
      // O buraco e a base (Maroon, Black, Teal, Gold) acompanham também a rampa (aumento_rampa)
      // Isso encurta o clipe em exatos `aumento_rampa`!

      dx_base = -aumento_diag + aumento_rampa;
      dz_base = aumento_diag + aumento_rampa;

      dx_topo = -aumento_diag;
      dz_topo = aumento_diag;

      perfil_transicao = [
        [2.6464 + dx_base, 4.4749 + dz_base], // Maroon
        [1.5858 + dx_base, 5.5355 + dz_base], // Black
        [3.5303 + dx_base, 7.4801 + dz_base], // Teal
        [4.5910 + dx_base, 6.4194 + dz_base], // Gold
      ];

      perfil_topo = [
        [2.6464 + dx_base, 4.4749 + dz_base], // Projeção Maroon na rampa
        [1.5858 + dx_base, 5.5355 + dz_base], // Projeção Black na rampa
        [5.4749 + dx_topo, 9.4246 + dz_topo], // Navy
        [6.5355 + dx_topo, 8.3640 + dz_topo], // Lime
      ];

      color(cor_clipe) {
        // Como as coordenadas já foram transladadas, fazemos apenas o ajuste anti-non-manifold
        // Deslizando levemente na parede externa
        translate([-0.01, 0, -0.01]) {
          cunha_transicao(y_clip_ini, y_clip_reto_ini, perfil_transicao, perfil_topo);
          cunha_transicao(y_clip_fim, y_clip_reto_fim, perfil_transicao, perfil_topo); // Cunhas espelhadas

          // Miolo do clipe
          translate([0, y_clip_reto_fim, 0])
            rotate([90, 0, 0])
              linear_extrude(height=y_clip_reto_fim - y_clip_reto_ini) {
                polygon(aplicar_tolerancia(perfil_topo, tol_clip));
              }
        }
      }
    }
}

module desenhar_arestas(lista_pares) {
  for (par = lista_pares) {
    hull() {
      translate(pontos[par[0]]) sphere(r=0.2);
      translate(pontos[par[1]]) sphere(r=0.2);
    }
  }
}

module ligar_pontos_longitudinais(pts) {
  for (i = [0:len(pts) - 2]) {
    if (abs(pts[i][0] - pts[i + 1][0]) < 0.001 && abs(pts[i][2] - pts[i + 1][2]) < 0.001) {
      hull() {
        translate(pontos[i]) sphere(r=0.15);
        translate(pontos[i + 1]) sphere(r=0.15);
      }
    }
  }
}

// =========================================================
// RENDERIZAÇÃO
// =========================================================

if (modo_impressao) {
  // Renderiza a peça com o movimento diagonal base (0.75) e o engrossamento da rampa (1.5)
  objeto_final_impressao(0, 0.75, 1.5);
} else {
  cores_perfil = [
    "Red",
    "Green",
    "Blue",
    "Yellow",
    "Magenta",
    "Cyan",
    "Orange",
    "Purple",
    "Brown",
    "Pink",
    "Lime",
    "Navy",
    "Gold",
    "Teal",
    "Maroon",
    "Olive",
    "Black",
  ];

  translate([0, 0, -0.5]) {
    for (i = [0:len(pontos) - 1]) {
      if (i % 2 == 0 && i <= 32) {
        color(cores_perfil[i / 2]) translate(pontos[i]) sphere(r=0.5);
      } else {
        color("LightBlue") translate(pontos[i]) sphere(r=0.3);
      }
    }

    ligacoes_frente = [
      [2, 4],
      [10, 6],
      [10, 12],
      [12, 8],
      [6, 4],
      [8, 14],
      [14, 16],
      [16, 18],
      [18, 30],
      [30, 32],
      [32, 0],
      [0, 2],
      [24, 20],
      [20, 22],
      [22, 26],
      [24, 28],
      [28, 33],
      [33, 26],
    ];

    ligacoes_tras = [
      [3, 5],
      [11, 7],
      [11, 13],
      [13, 9],
      [7, 5],
      [9, 15],
      [15, 17],
      [17, 19],
      [19, 31],
      [31, 35],
      [35, 1],
      [1, 3],
      [25, 21],
      [21, 23],
      [23, 27],
      [25, 29],
      [29, 34],
      [34, 27],
    ];

    color("White") {
      desenhar_arestas(ligacoes_frente);
      desenhar_arestas(ligacoes_tras);
    }

    color("Red") ligar_pontos_longitudinais(pontos);
  }
}
