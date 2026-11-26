// Posições dos mapas: tamanho da arte na tela, paredes, portas e spawns (sala e corredor).
#ifndef UTOPIA_MAPAS_H
#define UTOPIA_MAPAS_H

#include "raylib.h"
#include "config.h"

// =============================================================
// POSIÇÕES DOS MAPAS
// =============================================================

// Sala inicial: arte do mapa (imagem quadrada 1024x1024)
// Se o arquivo não existir, o jogo usa o cenário simples antigo.
#define ARQUIVO_MAPA_SALA "resources/maps/mapa1_introducao.png"

// ---- TAMANHO E POSIÇÃO DO MAPA NA TELA (AJUSTE AQUI) ----
// Tudo que é "medido na imagem" (paredes, porta, ponto de save, Luci, spawn)
// acompanha estes valores sozinho: aumentou o mapa, tudo vai junto.
#define MAPA_SALA_X          100.0f
#define MAPA_SALA_Y            0.0f
#define MAPA_SALA_LARGURA    600.0f
#define MAPA_SALA_ALTURA     600.0f
#define TAM_IMAGEM_MAPA_SALA 1024.0f   // lado da imagem em pixels

// Converte uma medida da imagem (pixels, de 0 a 1024) para a tela
#define MAPA_X(px)  (MAPA_SALA_X + (px) * MAPA_SALA_LARGURA / TAM_IMAGEM_MAPA_SALA)
#define MAPA_Y(py)  (MAPA_SALA_Y + (py) * MAPA_SALA_ALTURA / TAM_IMAGEM_MAPA_SALA)
#define MAPA_L(pl)  ((pl) * MAPA_SALA_LARGURA / TAM_IMAGEM_MAPA_SALA)
#define MAPA_A(pa)  ((pa) * MAPA_SALA_ALTURA / TAM_IMAGEM_MAPA_SALA)

// Retângulo medido na imagem: x, y, largura, altura
#define RETANGULO_MAPA(x, y, l, a)  { MAPA_X(x), MAPA_Y(y), MAPA_L(l), MAPA_A(a) }

static const Rectangle AREA_MAPA_SALA = { MAPA_SALA_X, MAPA_SALA_Y, MAPA_SALA_LARGURA, MAPA_SALA_ALTURA };

// Limites do centro do personagem no cenário simples (sem a imagem do mapa)
static const Rectangle LIMITES_SALA_SIMPLES = {80, 80, 640, 440};

// ---- PAREDES INVISÍVEIS DA SALA INICIAL ----
#define PAREDES_INVISIVEIS_ATIVAS   1   // 1 = bloqueiam o personagem | 0 = desligadas
#define MOSTRAR_PAREDES_INVISIVEIS  0   // 1 = desenha as paredes (para calibrar) | 0 = escondidas

// Cada linha é uma parede: { x, y, largura, altura } medidos na imagem.
// Para criar outra, é só acrescentar uma linha aqui e aumentar NUM_PAREDES_SALA.
static const Rectangle PAREDES_SALA[] = {
    RETANGULO_MAPA(   0,   0, 1024, 270),   // parede de cima (com o arco da porta)
    RETANGULO_MAPA(   0,   0,   55, 1024),  // parede da esquerda
    RETANGULO_MAPA( 985,   0,   39, 1024),  // parede da direita
    RETANGULO_MAPA(   0, 975, 1024,   49),  // parede de baixo
};
#define NUM_PAREDES_SALA 4

// Sala inicial: porta na parede de CIMA (o arco da imagem)
static const Rectangle PORTA_SALA      = RETANGULO_MAPA(464, 101, 116, 159);
static const Rectangle ZONA_PORTA_SALA = RETANGULO_MAPA(447, 154, 150, 145);

// ---- CORREDOR DO BLOCO 1: arte "mapa2_terminais.png" (1350 x 801) ----
// A imagem é reduzida no carregamento para a largura da tela, SEM deformar (800 x 474,7),
// e fica logo abaixo do HUD. Tudo do corredor é medido NA IMAGEM e convertido com
// CORR_X / CORR_Y: portas, paredes, terminal e spawns acompanham o mapa sozinhos.
// (Os monitores ficam na parede de cima, em monitores[].area, que já cai na parede da arte.)
#define ARQUIVO_MAPA_CORREDOR   "resources/maps/mapa2_terminais.png"
#define TAM_IMG_CORREDOR_L      1350.0f
#define TAM_IMG_CORREDOR_A       801.0f
#define MAPA_CORREDOR_X            0.0f
#define MAPA_CORREDOR_Y          100.0f      // o HUD do corredor fica em cima (y 62 e 82)
#define MAPA_CORREDOR_LARGURA    800.0f
#define ESCALA_MAPA_CORREDOR    (MAPA_CORREDOR_LARGURA / TAM_IMG_CORREDOR_L)
#define MAPA_CORREDOR_ALTURA    (TAM_IMG_CORREDOR_A * ESCALA_MAPA_CORREDOR)

// Converte uma medida da imagem (pixels) para a tela
#define CORR_X(px)  (MAPA_CORREDOR_X + (px) * ESCALA_MAPA_CORREDOR)
#define CORR_Y(py)  (MAPA_CORREDOR_Y + (py) * ESCALA_MAPA_CORREDOR)
#define CORR_T(p)   ((p) * ESCALA_MAPA_CORREDOR)
#define RETANGULO_CORREDOR(x, y, l, a)  { CORR_X(x), CORR_Y(y), CORR_T(l), CORR_T(a) }

#define MOSTRAR_COLISOES_CORREDOR  0   // 1 = desenha paredes, portas e zonas de interação (para calibrar) | 0 = escondidas


// Portas = as aberturas da arte: a de baixo volta para a sala, a da direita é a saída
static const Rectangle PORTA_VOLTAR_CORREDOR = RETANGULO_CORREDOR( 602, 735, 158,  66);
static const Rectangle PORTA_FIM_CORREDOR    = RETANGULO_CORREDOR(1305, 402,  45, 118);

// Terminal de atalho do Bloco 1 (para o Bloco 2): encostado na parede de baixo, perto da saída da direita
#define TERMINAL_CORREDOR_X  CORR_X(1160)
#define TERMINAL_CORREDOR_Y  CORR_Y(725)
static const Vector2   TERMINAL_ATALHO_CORREDOR = { TERMINAL_CORREDOR_X, TERMINAL_CORREDOR_Y };
static const Rectangle ZONA_TERMINAL_CORREDOR   = { TERMINAL_CORREDOR_X - 38, TERMINAL_CORREDOR_Y - 58, 76, 64 };   // o Thomas encosta e aperta E

// Paredes do corredor: o que bate nos PÉS do jogador (a cabeça pode passar "por trás" da parede).
// Cabos, luzes, telas e marcas do chão são só desenho: não bloqueiam.
static const Rectangle PAREDES_CORREDOR[] = {
    RETANGULO_CORREDOR(   0,   0, 1350, 250),   // parede de cima (até a base dos tijolos)
    RETANGULO_CORREDOR(   0,   0,   42, 801),   // parede da esquerda
    RETANGULO_CORREDOR(1308,   0,   42, 400),   // parede da direita, acima da saída
    RETANGULO_CORREDOR(1308, 520,   42, 281),   // parede da direita, abaixo da saída
    RETANGULO_CORREDOR(   0, 765,  562,  36),   // parede de baixo, à esquerda da porta
    RETANGULO_CORREDOR( 800, 765,  550,  36),   // parede de baixo, à direita da porta
    RETANGULO_CORREDOR( 560, 680,   42, 121),   // batente esquerdo da porta de baixo
    RETANGULO_CORREDOR( 760, 680,   40, 121),   // batente direito da porta de baixo
    RETANGULO_CORREDOR(   0, 801, 1350,  60),   // fim do mapa embaixo (a porta não deixa sair da tela)
    RETANGULO_CORREDOR(1350,   0,   60, 801),   // fim do mapa à direita
    { TERMINAL_CORREDOR_X - 20, TERMINAL_CORREDOR_Y - 14, 40, 34 },   // o próprio terminal de atalho
};
#define NUM_PAREDES_CORREDOR ((int)(sizeof(PAREDES_CORREDOR) / sizeof(PAREDES_CORREDOR[0])))

static const Vector2 SPAWN_SALA       = { MAPA_X(512), MAPA_Y(512) };
static const Vector2 SPAWN_SALA_VOLTA = { MAPA_X(522), MAPA_Y(300) };   // logo abaixo da porta de cima
static const Vector2 SPAWN_CORREDOR   = { CORR_X(681), CORR_Y(655) };   // logo acima da porta de baixo

// Caixa da batalha: larga para texto, estreita para desviar (como no Undertale)
static const Rectangle CAIXA_TEXTO_BATALHA = {50, 270, 700, 160};
static const Rectangle CAIXA_DESVIO        = {275, 270, 250, 160};

#endif // UTOPIA_MAPAS_H
