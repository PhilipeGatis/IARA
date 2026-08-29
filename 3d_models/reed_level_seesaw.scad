// ============================================================
// IARA - Gangorra do Reed Switch (trava de nivel maximo)
// Unidades: milimetros
// ============================================================
//
// PRINCIPIO
// ---------
// Alavanca apoiada na borda do vidro. Um braco entra no aquario e
// termina numa boia; o outro fica do lado de fora e carrega o ima. O
// reed switch, dentro de um tubo de acrilico, fica em pe no topo de uma
// coluna, fora da agua, e nunca encosta nela.
//
// A polaridade eletrica segue o que ja esta em HARDWARE.md: reed NA
// (normalmente aberto), fechado no nivel normal, aberto no nivel maximo.
//
//   Nivel normal : boia fora d'agua -> ponta da boia desce ->
//                  ponta do ima sobe -> ima perto do reed -> FECHADO
//   Nivel maximo : boia flutua      -> ponta da boia sobe  ->
//                  ponta do ima desce -> ima longe do reed -> ABERTO
//
// Quem mantem o contato fechado no nivel normal e o PESO do conjunto
// boia + contrapeso, nao a agua. Por isso existem os furos de
// contrapeso: a peca so funciona depois de calibrada.
//
// PERFIL (vista lateral, X+ para fora do aquario, Z+ para cima)
// -------------------------------------------------------------
//
//                                    | | <- tubo do reed, em pe
//                                   [===] <- garra do tubo
//                                     |
//                        ___________[o]__   <- pa com o ima deitado
//        dois discos    /             |
//        dentados -->  (O)            |     <- coluna
//                     /  |####|=======|     <- garra no vidro + viga
//                    /   |####|
//                   /    |vidro|
//              contrapeso
//                 /
//            ~~ boia ~~~~~~ nivel da agua ~~~~~~
//
// OS DOIS DISCOS DENTADOS
// -----------------------
// Cada braco termina num disco dentado. Os dois encaixam face a face,
// dente de um no vao do outro, e um unico parafuso M3 no centro aperta
// os dois -- esse mesmo parafuso e o eixo da gangorra, apoiado nas
// orelhas do suporte. A cabeca e a porca ficam em rebaixos nas orelhas,
// entao apertar o dentado nao prende as orelhas: a alavanca continua
// girando.
//
// Os dentes sao RETANGULARES, nao triangulares. Flanco reto sai da
// impressora com a medida certa; ponta triangular vira uma aresta que
// o bico de 0,4 nao consegue fechar e o encaixe fica frouxo. Com
// teeth_n = 12 o passo e de 30 graus e cada dente tem quase 2 mm de
// largura na ponta -- grosso, para imprimir sem drama. Para passo mais
// fino aumente teeth_n, mas conferindo se o dente nao fica mais estreito
// que tres filetes do bico.
//
// O disco tem so 18 mm de diametro: e a peca que fica mais a vista, e a
// ideia e ela sumir dentro do aquario.
//
// AJUSTE GROSSO E AJUSTE FINO
// ---------------------------
// Sao dois ajustes:
//
//   1. dentado do pivo, de 30 em 30 graus: poe a boia na regiao certa,
//      errando no maximo 22 mm, que e menos que o raio da boia
//   2. contrapeso: escolhe o nivel exato de disparo, com alcance de
//      quase a altura inteira da boia
//
// O segundo e o que realmente manda. Por isso o dentado pode ser grosso
// e o disco do pivo, pequeno.
//
// POR QUE O ANGULO ENTRE OS BRACOS IMPORTA
// ----------------------------------------
// O pivo esta 28 mm ACIMA da borda do vidro. Com os dois bracos em
// linha reta a ponta da boia tambem fica acima da borda e nunca alcanca
// a agua. E o dentado que resolve: dobrando o braco da boia para baixo,
// a ponta desce.
//
//   profundidade da boia = arm_in * sin(ang_v) - pivot_z
//
// Com arm_in = 60 e ang_v = 60 graus: 60*0,866 - 28 = 24 mm abaixo da
// borda, e 30 mm para dentro do aquario. Se a sua lamina d'agua estiver
// mais funda, aumente arm_in ou ang_v.
//
// AMPLIFICACAO
// ------------
// O braco util e a projecao HORIZONTAL, nao o comprimento da peca:
//
//   braco da boia = arm_in * cos(ang_v) = 60*0,5  = 30 mm
//   braco do ima  = arm_out             =          60 mm
//   ganho = 60/30 = 2,0x
//
// Ou seja, 10 mm de variacao da agua viram 20 mm de curso do ima.
// Puxar a boia para dentro do rasgo encurta o braco dela e AUMENTA o
// ganho: com a boia no fim de dentro o braco vira 46 e o ganho, 2,6x. Isso
// importa porque HARDWARE.md exige curso de 2 a 3 vezes a distancia de
// liberacao do reed. Os batentes limitam o giro em +-15 graus, o que da
// 2*60*sin(15) = 31 mm de curso do ima.
//
// Repare o efeito colateral: aumentar ang_v encurta o braco da boia e
// aumenta o ganho, mas cobra mais empuxo da boia.
//
// FORCA DA BOIA E CONTRAPESO
// --------------------------
// O empuxo, no braco da boia, precisa vencer o desequilibrio do
// conjunto. Uma bolinha de ping-pong (40 mm) submersa da ~33 gf; menos
// o peso dela, sobram ~30 gf uteis. O braco do ima mais o ima pesam na
// casa de 6 g e ficam mais longe do pivo, entao a peca nua tende a cair
// para o lado do ima -- que e o lado errado.
//
// Por isso existem tres furos M3 no braco da boia. Atravesse um M3 com
// porcas empilhadas ate que, com a boia SECA, a ponta da boia desca
// sozinha e encoste no batente. Trocar de furo e o ajuste grosso; somar
// ou tirar porcas e o ajuste fino.
//
// BATENTES
// --------
// Duas linguetas saem da orelha do lado do ima, para dentro do vao, e o
// braco do ima encosta nelas nos dois extremos. Elas pegam so a faixa
// em Y do braco do ima, entao o braco da boia passa livre por baixo
// seja qual for o ang_v escolhido.
//
// POR QUE O REED NAO FICA EM CIMA DO IMA
// --------------------------------------
// Os dois estao deitados: o ima numa pa horizontal, com a face virada
// para cima, e o tubo do reed na horizontal, apontando na direcao do
// braco. So que reed so fecha com campo ao longo do proprio eixo, e o
// campo de um ima deitado sai VERTICAL bem em cima dele. Reed deitado
// em cima do ima nao fecha nunca.
//
// O campo de um ima deitado so fica horizontal numa linha especifica:
// 54,7 graus a partir do eixo, ou seja, onde o afastamento horizontal e
// raiz(2) vezes a altura. Por isso os contatos do reed ficam
// reed_dy = 9,9 mm PARA O LADO e reed_dz = 7 mm acima da face do ima --
// nessa posicao o campo aponta certinho ao longo do tubo.
//
// O tubo corre em Y, paralelo a parede do aquario, e passa por cima do
// ima. Quem precisa estar deslocado nao e o tubo, e o REED dentro dele:
// escorregue o tubo ate os contatos ficarem a ~10 mm do eixo do ima.
//
// Ima de 6x2 mm N35 nessa posicao, a 12,1 mm de distancia: da ~43 gauss
// ao longo do eixo do reed. Reed comum fecha com 10-25 gauss, entao
// sobra fator 3 a 7. No batente de baixo a distancia passa de 32 mm, o
// campo cai com o cubo dela e ainda fica mais vertical: cerca de
// 1 gauss no eixo do reed, abre com folga.
//
// Se nao fechar de jeito nenhum, o caminho e ima maior (8x3 ou 10x3) e
// nao encostar mais o tubo: abaixo de reed_dz = 6 a pa do ima comeca a
// raspar nos labios da garra no fim do curso.
//
// So existe UM ajuste, e de proposito: escorregar o tubo em Y. O dz e
// cota de projeto, com furo redondo, porque dz e dy sao acoplados --
// mudar a altura sem recalcular o dy tira o reed da linha de 54,7 e o
// contato para de fechar. Se o seu reed for surdo demais para fechar em
// qualquer posicao do tubo, o caminho e ima maior (8x3 ou 10x3) e
// atualizar mag_d/mag_h aqui, que o arquivo recalcula o resto.
//
// O reed escorrega dentro do tubo. Os contatos (o meio do bulbo) tem
// que ficar em cima do ima. Marque o tubo por fora antes de encaixar e
// trave o reed com uma gota de cola quente nas duas pontas: isso tambem
// veda contra respingo e alivia tracao nos fios.
//
// CALIBRACAO (multimetro em continuidade, antes de ligar no ESP32)
// ---------------------------------------------------------------
// 0. Escolha o angulo entre os bracos no dentado e aperte o M3 central.
//    Confira antes na tela mexendo em ang_v.
// 1. Monte seco. Ajuste o contrapeso ate a ponta da boia descer sozinha
//    e encostar no batente.
// 2. Escorregue o tubo de um lado para o outro ate achar o ponto em que
//    o contato FECHA. A janela fica em torno de 10 mm de deslocamento a
//    partir do eixo do ima, para cada lado. Achado o ponto, marque o
//    tubo e trave o reed dentro dele com cola quente.
// 3. Levante a boia com a mao ate o outro batente. O contato tem que
//    ABRIR e nao pode voltar a fechar em nenhum ponto do meio do curso.
//    Se nao abrir, o ima esta forte demais: suba o tubo. Se nunca
//    fechar, use um ima maior e atualize mag_d/mag_h aqui.
// 4. Com o aquario no nivel maximo desejado, ajuste ang_v e a boia ate
//    o contato abrir nesse nivel.
//
// ONDA DE SUPERFICIE: bomba ligada faz a lamina oscilar e a boia segue a
// onda, o que pode fazer o reed piscar perto do ponto de disparo. Se
// acontecer, envolva a boia num tubo de PVC vertical com furos embaixo
// (tubo de calmaria) ou aumente o contrapeso.
//
// MATERIAL: PETG ou ASA. PLA absorve umidade e deforma perto d'agua.
//
// IMPRESSAO
// ---------
// Nao mande as pecas na posicao de montagem para a fatiadora. Use
// part = "plate" (as tres na mesa, 155 x 90 mm) ou uma das print_*:
// elas ja saem giradas e assentadas em z = 0.
//
// bracket     : deitado de lado, ou seja, girado 90 graus em X para o
//               eixo Y ficar na vertical. Assim o rasgo do vidro e o
//               furo de 5 mm do tubo do reed ficam com parede vertical.
//               Precisa de suporte em dois lugares, os dois pelo mesmo
//               motivo: a peca tem duas paredes laterais e a de cima
//               comeca no ar. Sao a segunda orelha do pivo e o segundo
//               dedo do reed. Os dois sao vaos abertos e o suporte sai
//               com o dedo. De pe seria pior: os dedos sobem em rampa
//               de 19 graus e a face de baixo deles viraria overhang.
// arm_mag     : deitado, dentes do pivo para CIMA. O alojamento do ima
//               fica numa parede vertical: rebaixo raso de 2,25 mm, sai
//               redondo o bastante. 3 perimetros.
// arm_float   : deitado, dentes do pivo para CIMA. Sem suporte.
// float_tray  : chapada na mesa, tubo do encaixe para cima. O teto do
//               tubo e uma ponte de 4,3 mm, que a fatiadora resolve.
//
// As traves do reed fazem parte do suporte: nao ha peca separada nem
// parafuso ali.
//
// FERRAGEM
// --------
// 1x M3 x 20 + porca nyloc   (eixo do pivo, que tambem aperta o dentado)
// 1x M3 x 25 + porcas de sobra (contrapeso)
// 1x ima de neodimio 6 x 2 mm, redondo
// 1x reed NA dentro de tubo de acrilico de 5 mm de diametro externo
// Abracadeiras finas: 1 aperta a garra do tubo, 2 amarram a boia.
// ============================================================

$fn = 48;

/* [Peca a exportar] */
// all = vista montada de conferencia
// Para imprimir use plate, ou uma das print_*: elas ja saem giradas e
// assentadas em z = 0. As sem prefixo estao na posicao de montagem.
part = "all"; // [all, plate, print_bracket, print_arm_mag, print_arm_float, print_float_tray, dims, dims_reed, dentes, coroas, bracket, arm_mag, arm_float, float_tray]
// Posicao da alavanca so na vista montada (+ = boia sobe = nivel maximo)
view_angle = -15; // [-15:1:15]

/* [Vidro] */
glass_t   = 4.9;   // Espessura do vidro do aquario
glass_tol = 0.6;   // Folga do rasgo da garra
clamp_wall  = 4.0; // Parede de cada perna da garra
clamp_depth = 30;  // Quanto a perna de FORA desce agarrando o vidro
// A perna de DENTRO e curta de proposito: com o braco da boia dobrado,
// a ponta dele passa rente ao vidro no fim do curso, e uma perna longa
// ficaria no caminho. A garra continua firme porque quem faz forca e o
// conjunto das duas pernas sobre a borda.
clamp_depth_in = 14;
clamp_w     = 24;  // Largura da garra (ao longo da parede do vidro)
bridge_h    = 8;   // Espessura da ponte que apoia sobre a borda

/* [Alavanca] */
// O braco do ima e quem compra margem de desarme: o curso do ima e
// 2*arm_out*sin(max_ang), e o campo no batente de baixo cai com o cubo
// da distancia. Encurtar aqui e barato ate certo ponto e depois fica
// caro, porque o ganho cai junto:
//
//   60 mm -> curso 31,1  ganho 2,00x  aberto com 1,3 gauss
//   45 mm -> curso 23,3  ganho 1,50x  aberto com 2,4 gauss
//   30 mm -> curso 15,5  ganho 1,00x  aberto com 5,1 gauss
//
// Reed comum solta perto de 10 a 15 gauss quando fecha com 43, entao
// ate 30 mm ainda desarma. Mas em 30 o ganho vira 1,0 e some a folga.
arm_out  = 45;  // Pivo -> ima, do lado de fora
arm_in   = 50;  // Pivo -> boia, medido ao longo do braco
arm_h    = 6;   // Altura da secao dos bracos
plate_t  = 4;   // Espessura da chapa de cada braco
max_ang  = 15;  // Curso maximo em graus, definido pelos batentes
// O pivo puxa todo o resto: quanto mais baixo, mais curto o braco da
// boia para alcancar a mesma profundidade e mais baixa a coluna, porque
// o ima sobe a partir dele. O piso sao os 8 mm da viga externa mais a
// meia altura do braco: abaixo de 18 a alavanca raspa nela antes de
// chegar no canal.
pivot_z  = 20;  // Altura do pivo acima da borda do vidro
pivot_d  = 3.2; // Furo do eixo M3
ear_t    = 7;   // Espessura de cada orelha do pivo

/* [Discos dentados] */
// A largura do dente na ponta e (pi * teeth_ro * teeth_fit) / teeth_n.
// Ela nao pode cair abaixo de uns 1,2 mm, que sao tres filetes de um
// bico de 0,4: abaixo disso a fatiadora borra o dente e o encaixe nao
// trava. Isso amarra o passo ao tamanho do disco:
//
//   passo 30 graus (n=12) -> coroa raio  7 -> disco 17 mm
//   passo 20 graus (n=18) -> coroa raio  9 -> disco 21 mm
//   passo 15 graus (n=24) -> coroa raio 11 -> disco 25 mm
//   passo  5 graus (n=72) -> coroa raio 32 -> disco 70 mm, inviavel
//
// Ficou em 30 graus, o menor disco possivel, porque passo fino aqui NAO
// e necessario: quem define o nivel de disparo e o contrapeso, nao o
// angulo. O angulo so precisa por a boia perto da superficie -- erra no
// maximo 22 mm entre dois dentes, dentro do raio da propria boia -- e o
// rasgo da boia cobre mais 12 mm de forma continua. O ajuste fino de
// verdade e somar ou tirar porca no contrapeso.
teeth_n   = 12;   // 12 dentes = um passo a cada 30 graus
teeth_ri  = 4.0;  // Raio interno da coroa
teeth_ro  = 7.0;  // Raio externo da coroa
teeth_h   = 1.4;  // Altura do dente
teeth_fit = 0.85; // Largura do dente / meio passo (folga de encaixe)
disc_r    = 8.5;  // Raio dos dois discos (coroa + 1,5 mm de aba)
// Angulo entre os bracos, escolhido no dentado. 0 = bracos em linha
// reta. Positivo abaixa a ponta da boia. Multiplo de 360/teeth_n.
ang_v = 60; // [0:30:90]

/* [Ima] */
mag_d = 6.0;    // Diametro do ima de neodimio
mag_h = 2.0;    // Espessura do ima
mag_tol = 0.25; // Folga do alojamento (encaixe por pressao)
mag_pad_w = 10; // Largura da pa que segura o ima (em Y)
mag_pad_l = 10; // Comprimento da pa (em X)
mag_pad_z = 4;  // Altura da face de cima da pa, a partir do eixo do braco

/* [Reed] */
tube_od  = 5.0;   // Diametro externo do tubo de acrilico
tube_tol = 0.3;   // Folga da garra (o tubo desliza para ajustar o alcance)
// Posicao dos contatos do reed em relacao a face do ima, no batente de
// cima. NAO e em cima do ima: veja a explicacao no cabecalho. A conta e
// reed_dx = raiz(2) * reed_dz, que poe o reed na linha de 54,7 graus,
// onde o campo de um ima deitado e horizontal.
reed_dz  = 7.0;   // Quanto o eixo do tubo fica acima da face do ima
reed_dy  = 9.9;   // Quanto os contatos ficam deslocados de lado
// A coluna fica 7 mm alem da ponta do braco. O vao mais apertado nao e
// no fim do curso e sim perto do meio dele, com a alavanca a uns 5
// graus: ali a ponta chega a 49,7 mm e a face da coluna esta em 57.
col_x0   = 57;    // Face interna da coluna do reed
col_t    = 10;    // Espessura da coluna
col_w    = 24;    // Largura da coluna e da viga externa

/* [Boia] */
// A boia assenta numa BANDEJA horizontal, peca separada, que entra por
// pressao na ponta do braco. Deitada assim o empuxo empurra contra a
// face inteira, em vez de torcer uma lamina em pe.
//
// Separar as duas resolve a impressao: bandeja e braco sao
// perpendiculares entre si, entao numa peca so uma das duas sairia em
// pe na mesa, com metade pendurada no ar. Soltas, cada uma deita e
// nenhuma pede suporte.
//
// O encaixe e um tubo retangular na bandeja, 0,3 mm maior que a secao
// do braco e fechado no fundo. Entra empurrando. Se afrouxar com o
// tempo, ha um furo transversal para um M3 ou um pedaco de arame.
//
// Os dois furos das pontas nao sao de parafuso: sao chaves de cola. A
// cola atravessa e endurece dentro deles, e ai o isopor fica ancorado e
// nao so grudado na superficie -- que e o que falha primeiro quando a
// junta e so de topo.
//
// Nao ha rasgo de ajuste aqui: com a roda no proprio braco nao existe
// nada para deslizar. O ajuste fino do nivel de disparo e o contrapeso,
// que tem alcance de quase a altura inteira da boia.
float_plate_l = 24;  // Comprimento da bandeja, ao longo do braco
float_plate_w = 12;  // Largura da bandeja, transversal ao braco
float_plate_t = 4;   // Espessura da bandeja
float_key_d   = 3;   // Diametro das chaves de cola
tray_tol      = 0.3; // Folga do encaixe por pressao
tray_wall     = 2;   // Parede do tubo do encaixe
tray_socket_l = 14;  // Comprimento do encaixe

/* [Diversos] */
tol = 0.4;
m3  = 3.4;

gs      = glass_t + glass_tol;
clamp_x = gs/2 + clamp_wall;
arm_gap = 2*plate_t + teeth_h + 0.4;  // vao entre as orelhas
beam_h  = 8;                          // viga externa, do vidro a coluna

// ------------------------------------------------------------
// GEOMETRIA DERIVADA
// ------------------------------------------------------------
// Plano de encaixe dos dentes em y = 0. Cada chapa fica de um lado.
function mag_y0()  =  teeth_h/2;              // chapa do braco do ima
function flo_y0()  = -teeth_h/2 - plate_t;    // chapa do braco da boia

// Onde a face do ima passa no batente de cima, ou seja, no nivel
// normal, com o contato fechado. Tudo do reed para cima sai daqui.
function mag_up_x()   = arm_out*cos(max_ang) - mag_pad_z*sin(max_ang);
function mag_face_z() = pivot_z + arm_out*sin(max_ang)
                                + mag_pad_z*cos(max_ang);
// E no batente de baixo, com o contato aberto
function mag_dn_z()   = pivot_z - arm_out*sin(max_ang)
                                + mag_pad_z*cos(max_ang);

// Suporte do tubo do reed: duas traves furadas, nada mais.
// O tubo corre em Y, paralelo a parede do aquario: nao avanca sobre a
// agua e some contra a borda.
function ch_d()        = tube_od + tube_tol;
function reed_axis_z() = mag_face_z() + reed_dz;
function reed_y()      = reed_dy;   // onde ficam os contatos, em Y
function tab_t()       = 3;         // espessura de cada trave
// As traves ficam FORA da faixa em Y que a pa do ima ocupa, para o ima
// poder chegar perto do tubo pelado no meio do vao
// Os dedos correm por fora da pa do ima, com 1,5 mm de folga, e vao ate
// a borda externa da orelha
function fing_y0()     = mag_pad_w/2 + 1.5;
function fing_t()      = arm_gap/2 + ear_t - fing_y0();
function fing_x1()     = mag_up_x() + 6;
// Base da peca: o tanto que a trave desce abaixo do eixo do tubo
function reed_lip()    = 4.7;
function reed_base_z() = reed_axis_z() - reed_lip();
// A altura do tubo e cota de projeto, sem ajuste, de proposito: dz e dy
// sao acoplados (dy = 1,41 x dz), entao mudar a altura sem mexer no
// tubo tira o reed da linha onde o campo e horizontal. Quem ajusta e o
// tubo, escorregando em Y dentro das traves.
function col_top()     = reed_axis_z() + 3;   // ponto mais alto de tudo

// Batentes: linguetas na orelha do lado do ima
function stop_x()   = disc_r + 4;
function stop_top() = pivot_z - (arm_h/2)/cos(max_ang) - stop_x()*tan(max_ang);

// Onde a boia fica, medido da borda do vidro. Negativo = dentro d'agua.
// arm_in vai ate o centro da roda, que e onde a boia fica.
function float_z() = pivot_z - arm_in*sin(ang_v);
function float_x() = -arm_in*cos(ang_v);

// Canal da viga: precisa passar a pa do ima, que e mais larga que a chapa
function chan_w() = mag_pad_w + 1.5;

// Pontas da alavanca
function lever_x1() = arm_out + mag_pad_l/2;

// ------------------------------------------------------------
// DENTES RETANGULARES
// ------------------------------------------------------------
// Um dente e um setor de coroa circular: flancos retos, topo plano.
// Ocupa metade do passo vezes teeth_fit; a sobra e a folga de encaixe.
// Duas coroas iguais, uma defasada meio passo, encaixam uma na outra.
module tooth_2d(ri, ro, a) {
    intersection() {
        difference() { circle(r = ro); circle(r = ri); }
        polygon([[0, 0],
                 [ro*2*cos(-a), ro*2*sin(-a)],
                 [ro*2, 0],
                 [ro*2*cos( a), ro*2*sin( a)]]);
    }
}

module crown(n, ri, ro, h) {
    a = (90/n)*teeth_fit;          // meia largura angular do dente
    linear_extrude(h)
        for (i = [0 : n - 1]) rotate([0, 0, i*360/n]) tooth_2d(ri, ro, a);
}

// Coroa do braco da boia: dentes de -teeth_h/2 ate +teeth_h/2, para +Y
module crown_up() {
    translate([0, -teeth_h/2 - 0.05, 0]) rotate([-90, 0, 0])
        crown(teeth_n, teeth_ri, teeth_ro, teeth_h + 0.05);
}
// Coroa do braco do ima: mesma coroa virada e defasada meio passo
module crown_dn() {
    translate([0, teeth_h/2 + 0.05, 0]) rotate([90, 0, 0])
        rotate([0, 0, 180/teeth_n])
            crown(teeth_n, teeth_ri, teeth_ro, teeth_h + 0.05);
}

// Bandeja da boia. Peca separada, entra por pressao na ponta do braco.
// Origem no eixo do encaixe, que e o eixo da lamina do braco.
function tray_iy()  = plate_t + tray_tol;          // vao interno, em Y
function tray_iz()  = arm_h + tray_tol;            // vao interno, em Z
function tray_top() = -(tray_iz()/2 + tray_wall);  // face de cima da bandeja
module float_tray() {
    difference() {
        union() {
            // Tubo do encaixe
            translate([-tray_socket_l/2, -(tray_iy()/2 + tray_wall),
                       -(tray_iz()/2 + tray_wall)])
                cube([tray_socket_l, tray_iy() + 2*tray_wall,
                      tray_iz() + 2*tray_wall]);
            // Bandeja, logo abaixo do tubo. Cantos arredondados so para
            // nao virar faca dentro do aquario.
            translate([0, 0, tray_top() - float_plate_t])
                linear_extrude(float_plate_t)
                    hull() for (sx = [-1, 1], sy = [-1, 1])
                        translate([sx*(float_plate_l/2 - 3),
                                   sy*(float_plate_w/2 - 3)]) circle(r = 3);
        }
        // Vao do encaixe, aberto so do lado do pivo
        translate([-tray_socket_l/2 + 4, -tray_iy()/2, -tray_iz()/2])
            cube([tray_socket_l, tray_iy(), tray_iz()]);
        // Furo transversal, casa com o do braco
        translate([tray_socket_l/2 - 5, -20, 0]) rotate([-90, 0, 0])
            cylinder(d = m3, h = 40);
        // Chaves de cola, nas pontas, fora da sombra do tubo
        for (sx = [-1, 1])
            translate([sx*(float_plate_l/2 - 2.5), 0, -30])
                cylinder(d = float_key_d, h = 60);
    }
}

// Chapa: perfil 2D no plano XZ, espessura t a partir de y0
module plate(y0, t) {
    translate([0, y0, 0]) rotate([-90, 0, 0]) linear_extrude(t) children();
}
// Furo passante em Y
module ybore(x, z, d) {
    translate([x, -30, z]) rotate([-90, 0, 0]) cylinder(d = d, h = 60);
}

// ------------------------------------------------------------
// SUPORTE: garra no vidro + orelhas + viga + coluna
// ------------------------------------------------------------
module bracket() {
    difference() {
        union() {
            // Garra em U sobre a borda do vidro
            translate([-clamp_x, -clamp_w/2, -clamp_depth])
                cube([2*clamp_x, clamp_w, clamp_depth + bridge_h]);

            // Orelhas do pivo, uma de cada lado do vao
            for (s = [-1, 1])
                translate([0, s*(arm_gap/2), 0])
                    mirror([0, s < 0 ? 1 : 0, 0]) ear();

            // Linguetas de fim de curso. Ficam so na faixa em Y do
            // braco do ima, entao o braco da boia passa livre.
            for (s = [-1, 1])
                translate([s*stop_x() - (s > 0 ? 2 : 0), mag_y0(), 0])
                    cube([2, arm_gap/2 - mag_y0(), stop_top()]);

            // Os dois dedos que levam o tubo do reed. Saem das proprias
            // orelhas e passam POR CIMA do braco do ima, um de cada
            // lado dele. Substituem a viga no nivel da borda mais a
            // coluna: a estrutura toda vira garra + torre.
            for (sy = [-1, 1])
                translate([0, sy*fing_y0(), 0])
                    mirror([0, sy < 0 ? 1 : 0, 0]) finger();
        }

        // Canal para o braco da boia passar quando ang_v e grande e a
        // alavanca esta no fim do curso. Desce toda a perna de dentro:
        // com ang_v = 60 e a alavanca no batente de cima, o braco cruza
        // o nivel da borda a 7,5 mm do centro do vidro, rente a perna.
        // Some 5 mm de uma garra de 24 mm de largura, o que nao muda a
        // pegada dela na borda.
        translate([-clamp_x - 1, flo_y0() - 0.5, -clamp_depth_in - 1])
            cube([2*clamp_x + 2, plate_t + 1, clamp_depth_in + bridge_h + 2]);

        // Encurta a perna de dentro
        translate([-clamp_x - 1, -clamp_w/2 - 1, -clamp_depth - 1])
            cube([clamp_wall + 1, clamp_w + 2, clamp_depth - clamp_depth_in + 1]);

        // Rasgo do vidro, aberto embaixo
        translate([-gs/2, -clamp_w/2 - 1, -clamp_depth - 1])
            cube([gs, clamp_w + 2, clamp_depth + 1]);

        // Chanfro de entrada, ajuda a enfiar no vidro
        translate([0, 0, -clamp_depth]) rotate([-90, 0, 0])
            linear_extrude(clamp_w + 2, center = true)
                polygon([[-gs/2 - 2, 0], [gs/2 + 2, 0],
                         [gs/2, 2.5], [-gs/2, 2.5]]);

        // Furo do eixo M3
        ybore(0, pivot_z, pivot_d);

        // Rebaixos nas orelhas: a cabeca e a porca do parafuso do eixo
        // moram aqui. E o que deixa apertar o dentado dos dois bracos
        // sem prender as orelhas.
        for (sy = [-1, 1])
            translate([0, sy*(arm_gap/2 - 0.01), pivot_z])
                rotate([-90*sy, 0, 0]) cylinder(d = 8.5, h = 3.4);

        // Furo de 5 mm que atravessa os dois dedos. O tubo desliza
        // dentro dele: e assim que se acerta o deslocamento lateral dos
        // contatos em relacao ao ima.
        translate([mag_up_x(), -clamp_w, reed_axis_z()]) rotate([-90, 0, 0])
            cylinder(d = ch_d(), h = 2*clamp_w);
    }
}

// Dedo que sai da orelha e vai segurar o tubo do reed. A face de baixo
// e uma rampa: tem que passar acima do braco do ima no fim de curso, e
// com 2 mm de folga no ponto mais apertado, que fica perto da ponta.
module finger() {
    rotate([90, 0, 0]) translate([0, 0, -fing_t()]) linear_extrude(fing_t())
        polygon([[0, pivot_z + 2],
                 [fing_x1(), reed_axis_z() - 4],
                 [fing_x1(), reed_axis_z() + 3],
                 [0, pivot_z + 12]]);
}

// Placa lateral do pivo, topo arredondado em volta do furo.
// Desenhada no plano XZ e girada; ocupa y de 0 ate ear_t.
module ear() {
    rotate([90, 0, 0]) translate([0, 0, -ear_t]) linear_extrude(ear_t)
        hull() {
            translate([-stop_x() - 2, bridge_h]) square([2*stop_x() + 4, 8]);
            translate([0, pivot_z]) circle(d = 13);
        }
}

// ------------------------------------------------------------
// BRACO DO IMA: disco dentado + pa horizontal com o ima deitado
// ------------------------------------------------------------
module arm_mag() {
    difference() {
        union() {
            plate(mag_y0(), plate_t) {
                circle(r = disc_r);
                translate([0, -arm_h/2]) square([arm_out, arm_h]);
            }
            crown_dn();
            // Pa horizontal da ponta: o ima fica DEITADO nela, com a
            // face rente ao topo, olhando para cima, para o tubo do
            // reed que desce no mesmo eixo.
            translate([arm_out - mag_pad_l/2, -mag_pad_w/2, -arm_h/2])
                cube([mag_pad_l, mag_pad_w, mag_pad_z + arm_h/2]);
        }
        ybore(0, 0, pivot_d);   // eixo, que tambem aperta o dentado

        // Alojamento do ima: 6 mm de diametro, 2 mm de profundidade,
        // aberto para cima, eixo vertical.
        translate([arm_out, 0, mag_pad_z - (mag_h + mag_tol)])
            cylinder(d = mag_d + mag_tol, h = mag_h + mag_tol + 1);
        // Furo por baixo, para empurrar o ima para fora
        translate([arm_out, 0, -arm_h/2 - 1])
            cylinder(d = mag_d - 2.5, h = mag_pad_z + 2);
    }
}

// ------------------------------------------------------------
// BRACO DA BOIA: disco dentado + pa furada na ponta
// ------------------------------------------------------------
module arm_float() {
    difference() {
        union() {
            plate(flo_y0(), plate_t) {
                circle(r = disc_r);
                // A lamina segue ate o fundo do encaixe da bandeja
                translate([-arm_in - tray_socket_l/2 + 4, -arm_h/2])
                    square([arm_in + tray_socket_l/2 - 4, arm_h]);
            }
            crown_up();
        }
        ybore(0, 0, pivot_d);
        // Furo transversal do encaixe da bandeja, para um M3 ou arame
        ybore(-arm_in + tray_socket_l/2 - 5, 0, m3);
        // Furos de contrapeso
        for (r = [20, 28, 36]) ybore(-r, 0, m3);
    }
}

// ------------------------------------------------------------
// SUPORTE DO REED: garra partida, com o tubo em pe
// ------------------------------------------------------------
// ------------------------------------------------------------
// MONTAGEM
// ------------------------------------------------------------
module assembly() {
    color("SkyBlue") bracket();
    translate([0, 0, pivot_z]) rotate([0, -view_angle, 0]) lever();
    // Tubo do reed, deitado em Y, so para conferir folga e alcance
    color("DimGray", 0.45)
        translate([mag_up_x(), -clamp_w, reed_axis_z()])
            rotate([-90, 0, 0]) cylinder(d = tube_od, h = 2*clamp_w);
    // Vidro
    color("Tomato", 0.3)
        translate([-glass_t/2, -60, -140]) cube([glass_t, 120, 140]);
}

// Vista de conferencia do dentado: os dois bracos afastados no eixo,
// para ver que os dentes SAEM da face de cada disco e caem no vao do
// outro. gap = 0 mostra eles encaixados.
module lever_exploded(gap = 16) {
    color("RoyalBlue") translate([0, gap, 0]) arm_mag();
    color("SteelBlue") translate([0, -gap, 0]) rotate([0, -ang_v, 0])
        arm_float();
}

// So as duas coroas, sem as chapas: a maneira mais clara de ver que os
// dentes de uma caem no vao da outra
module teeth_pair() {
    color("RoyalBlue") crown_dn();
    color("SteelBlue") crown_up();
}

// Alavanca montada: o braco da boia gira ang_v sobre o do ima
module lever() {
    color("RoyalBlue") arm_mag();
    color("SteelBlue") rotate([0, -ang_v, 0]) {
        arm_float();
        color("Orange") translate([-arm_in, flo_y0() + plate_t/2, 0])
            float_tray();
    }
}

// ------------------------------------------------------------
// COTAS: vistas so de conferencia, nao imprimem nada
// ------------------------------------------------------------
function mm(v) = str(round(v*10)/10, " mm");

module dline(x1, y1, x2, y2, t = 0.7) {
    hull() { translate([x1, y1]) circle(d = t); translate([x2, y2]) circle(d = t); }
}
module dim_h(x1, x2, y, txt, sz = 4.5) {
    dline(x1, y, x2, y);
    dline(x1, y - 2, x1, y + 2);
    dline(x2, y - 2, x2, y + 2);
    translate([(x1 + x2)/2, y + 2.5]) text(txt, size = sz, halign = "center");
}
module dim_v(y1, y2, x, txt, side = 1, sz = 4.5) {
    dline(x, y1, x, y2);
    dline(x - 2, y1, x + 2, y1);
    dline(x - 2, y2, x + 2, y2);
    translate([x + 3*side, (y1 + y2)/2 - sz/2])
        text(txt, size = sz, halign = side > 0 ? "left" : "right");
}
module ext(x1, y1, x2, y2) { dline(x1, y1, x2, y2, 0.35); }

module dims_general() {
    // Horizontais
    dim_h(float_x(), 0, -64, str("boia, ", mm(-float_x()), " para dentro"));
    dim_h(0, arm_out, -78, str("braco do ima  ", mm(arm_out)));
    dim_h(float_x() - float_plate_l/2, fing_x1(), -92,
          str("vao total  ", mm(fing_x1() - float_x() + float_plate_l/2)));
    ext(float_x(), float_z(), float_x(), -62);
    ext(0, pivot_z, 0, -76);
    ext(arm_out, mag_face_z(), arm_out, -76);
    ext(fing_x1(), 0, fing_x1(), -90);

    // Verticais, a direita
    dim_v(0, pivot_z, 100, str("pivo  ", mm(pivot_z)));
    dim_v(0, mag_face_z(), 118, str("face do ima, fechado  ", mm(mag_face_z())));
    dim_v(0, col_top(), 142, str("ponto mais alto  ", mm(col_top())));
    ext(0, 0, 140, 0);
    ext(fing_x1(), col_top(), 140, col_top());

    // Verticais, a esquerda
    dim_v(-clamp_depth_in, 0, -112, str(mm(clamp_depth_in), " garra"), -1, 3.5);
    dim_v(float_z(), 0, -60, str("boia ", mm(-float_z()), " abaixo"), -1, 3.5);

    dline(-118, 0, 152, 0, 0.35);
    translate([-118, 2]) text("borda do vidro = 0", size = 4);
    translate([-90, pivot_z + 40])
        text(str("discos dentados: ", teeth_n, " dentes retangulares, ",
                 "passo de ", 360/teeth_n, " graus"), size = 4);
    translate([-90, pivot_z + 33])
        text(str("angulo entre os bracos: ", ang_v, " graus"), size = 4);
    translate([-90, pivot_z + 26])
        text(str("ganho = ", round(arm_out/(arm_in*cos(ang_v))*100)/100, "x"),
             size = 4);
}

module dims_reed() {
    dim_v(mag_dn_z(), mag_face_z(), mag_up_x() + 30,
          str("curso do ima  ", mm(mag_face_z() - mag_dn_z())), 1, 3.5);
    ext(mag_up_x(), mag_face_z(), mag_up_x() + 28, mag_face_z());
    ext(mag_up_x(), mag_dn_z(), mag_up_x() + 28, mag_dn_z());

    // Altura do tubo sobre a face do ima
    dim_v(mag_face_z(), reed_axis_z(), mag_up_x() + 12,
          str("dz ", mm(reed_dz)), 1, 3.5);
    translate([mag_up_x() - 46, reed_axis_z() + 36])
        text("tubo em Y, por cima do ima. Os CONTATOS do reed", size = 3.5);
    translate([mag_up_x() - 46, reed_axis_z() + 31])
        text(str("ficam dy = ", mm(reed_dy), " para o lado, na linha de 54,7 graus:"),
             size = 3.5);
    translate([mag_up_x() - 46, reed_axis_z() + 26])
        text("e onde o campo do ima deitado fica horizontal", size = 3.5);

    // Ima em corte, deitado
    translate([mag_up_x() - 54, mag_face_z() - 30]) {
        dline(-mag_d/2, 0, mag_d/2, 0, 0.8);
        dline(-mag_d/2, -mag_h, mag_d/2, -mag_h, 0.8);
        dline(-mag_d/2, -mag_h, -mag_d/2, 0, 0.8);
        dline(mag_d/2, -mag_h, mag_d/2, 0, 0.8);
        dim_h(-mag_d/2, mag_d/2, 6, str("ima deitado, d ", mm(mag_d)), 3.5);
        dim_v(-mag_h, 0, 10, mm(mag_h), 1, 3.5);
        translate([-mag_d/2, -12])
            text(str("alojamento ", mag_d + mag_tol, " x ", mag_h + mag_tol),
                 size = 3.2);
    }
}

module annotate(mod = 0) {
    color("Black") translate([0, -30, 0]) rotate([90, 0, 0]) linear_extrude(1)
        if (mod == 0) dims_general(); else dims_reed();
}

// ------------------------------------------------------------
// VERSAO PARA IMPRIMIR
// ------------------------------------------------------------
// As pecas do modelo estao na posicao de MONTAGEM, que nao serve para
// fatiar. Os modulos abaixo giram cada uma para a posicao de impressao
// e assentam ela em z = 0, prontas para mandar para a fatiadora.
//
// A regra e a mesma nas tres: o eixo Y do modelo, que e a espessura das
// chapas, vira o eixo Z da impressora. Assim todo furo de eixo Y sai
// redondo e toda parede fica vertical.

// Suporte deitado de lado. Y do modelo (+-12) vira a altura.
module print_bracket() { translate([0, 0, clamp_w/2]) rotate([90, 0, 0]) bracket(); }

// Braco do ima com os dentes do pivo para cima
module print_arm_mag() { translate([0, 0, mag_pad_w/2]) rotate([-90, 0, 0]) arm_mag(); }

// Braco da boia com os dentes do pivo para cima
module print_arm_float() {
    translate([0, 0, -flo_y0()]) rotate([90, 0, 0]) arm_float();
}

// Bandeja assentada na mesa, tubo do encaixe para cima
module print_float_tray() {
    translate([0, 0, -tray_top() + float_plate_t]) float_tray();
}

// Mesa com as tres pecas posicionadas: 155 x 90 mm, cabe em qualquer
// mesa de 180 mm para cima. Os dois bracos ficam em pe ao lado do
// suporte, para o conjunto nao passar de 90 mm em Y.
module print_plate() {
    print_bracket();
    translate([58, -48, 0]) rotate([0, 0,  90]) print_arm_mag();
    translate([88, -48, 0]) rotate([0, 0, -90]) print_arm_float();
    translate([112, -40, 0]) print_float_tray();
}

if (part == "all") assembly();
else if (part == "plate") print_plate();
else if (part == "print_bracket") print_bracket();
else if (part == "print_arm_mag") print_arm_mag();
else if (part == "print_arm_float") print_arm_float();
else if (part == "dims") { assembly(); annotate(0); }
else if (part == "dims_reed") { assembly(); annotate(1); }
else if (part == "dentes") lever_exploded();
else if (part == "coroas") teeth_pair();
else if (part == "dentes_juntos") lever_exploded(0);
else if (part == "bracket") bracket();
else if (part == "arm_mag") arm_mag();
else if (part == "arm_float") arm_float();
else if (part == "float_tray") float_tray();
else if (part == "print_float_tray") print_float_tray();
