#ifndef CONFIG_H
#define CONFIG_H

#include "raylib.h"

typedef enum { MENU, INTRO, JOGANDO, OPCOES, CORREDOR, BATALHA } TelaAtual;
typedef enum Direcao { DIR_FRENTE, DIR_COSTAS, DIR_DIREITA, DIR_ESQUERDA } Direcao;

#define LARGURA_TELA 800
#define ALTURA_TELA  600

// Sprites
#define SPRITE_FRENTE         "resources/sprites/player/person.frente.png"
#define SPRITE_COSTAS         "resources/sprites/player/person.costas.png"
#define SPRITE_COSTAS_RESERVA "resources/sprites/player/person.cima.png"
#define SPRITE_LADO           "resources/sprites/player/person.lado.png"

#define SPRITE_MONITOR_NEUTRO "resources/sprites/corredor/monitor.neutro.png"
#define SPRITE_MONITOR_ATIVO  "resources/sprites/corredor/monitor.ativo.png"
#define SPRITE_MONITOR_ACERTO "resources/sprites/corredor/monitor.acerto.png"
#define SPRITE_MONITOR_ERRO   "resources/sprites/corredor/monitor.erro.png"

#define TAM_SPRITE          65.0f
#define VELOCIDADE_JOGADOR  300.0f

// Corredor & Diálogo
#define TOTAL_MONITORES      4
#define PONTOS_POR_ACERTO    10
#define TEMPO_QUIZ           60.0f
#define ATRASO_RESPOSTA      0.4f
#define QUIZ_EM_ORDEM        1
#define VELOCIDADE_TEXTO     50.0f
#define DURACAO_FADE         0.35f
#define DURACAO_FADE_BATALHA 0.2f

#define MAX_PAGINAS          8
#define NUM_OPCOES           4

#define FONTE_CAIXA          18
#define LARGURA_TEXTO_CAIXA  640
#define FONTE_PERGUNTA       18
#define LARGURA_PERGUNTA     680
#define LARGURA_OPCAO        650

#define OBJ_NENHUM  -1
#define OBJ_LUCI    -2

// Batalha
#define MAX_BALAS            96
#define HP_MAX_BATALHA       20
#define VELOCIDADE_ALMA      170.0f
#define TEMPO_INVENCIVEL     1.0f
#define RAIO_BALA            7.0f
#define RAIO_ALMA            6.0f
#define DURACAO_MINIGAME     7.0f
#define DURACAO_COMBATE      11.0f
#define DURACAO_ANIM_CAIXA   0.35f
#define FONTE_BATALHA        20

// Geometria dos Mapas
static const Rectangle PORTA_SALA            = {360, 42, 80, 36};
static const Rectangle ZONA_PORTA_SALA       = {360, 42, 80, 52};
static const Rectangle PORTA_VOLTAR_CORREDOR = {100, 406, 90, 18};
static const Rectangle PORTA_FIM_CORREDOR    = {722, 265, 28, 140};

static const Vector2 SPAWN_SALA              = {400, 300};
static const Vector2 SPAWN_SALA_VOLTA        = {400, 115};
static const Vector2 SPAWN_CORREDOR          = {145, 368};

static const Rectangle CAIXA_TEXTO_BATALHA   = {50, 270, 700, 160};
static const Rectangle CAIXA_DESVIO          = {275, 270, 250, 160};

#endif
