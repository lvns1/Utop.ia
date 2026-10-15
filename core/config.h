// config.h - Constantes globais: telas, sprites, tempos, batalha e posições dos mapas.
// Para ajustar o balanceamento do jogo (tempo do quiz, fade, etc.), mexa aqui.

#ifndef CONFIG_H
#define CONFIG_H

#include "raylib.h"

// Definimos os "canais" (telas) do nosso jogo
typedef enum { MENU, INTRO, JOGANDO, OPCOES, CORREDOR, BATALHA } TelaAtual;
typedef enum Direcao { DIR_FRENTE, DIR_COSTAS, DIR_DIREITA, DIR_ESQUERDA } Direcao;

#define LARGURA_TELA 800
#define ALTURA_TELA  600

// Sprites
// Andando para CIMA o personagem tem que aparecer DE COSTAS para a câmera.
// O jogo procura primeiro "person.costas.png" (personagem visto por trás).
// Se esse arquivo não existir, usa o "person.cima.png" antigo.
#define SPRITE_FRENTE         "resources/sprites/player/person.frente.png"
#define SPRITE_COSTAS         "resources/sprites/player/person.costas.png"
#define SPRITE_COSTAS_RESERVA "resources/sprites/player/person.cima.png"
#define SPRITE_LADO           "resources/sprites/player/person.lado.png"

// Sprites dos monitores (opcionais). Enquanto os arquivos não existirem,
// o jogo desenha os monitores com formas simples.
#define SPRITE_MONITOR_NEUTRO "resources/sprites/corredor/monitor.neutro.png"
#define SPRITE_MONITOR_ATIVO  "resources/sprites/corredor/monitor.ativo.png"
#define SPRITE_MONITOR_ACERTO "resources/sprites/corredor/monitor.acerto.png"
#define SPRITE_MONITOR_ERRO   "resources/sprites/corredor/monitor.erro.png"

#define TAM_SPRITE          65.0f           // tamanho do personagem na tela
#define VELOCIDADE_JOGADOR  300.0f          // pixels por segundo (= 5 px por frame a 60 FPS)

// Corredor & Diálogo
#define TOTAL_MONITORES      4
#define PONTOS_POR_ACERTO    10
#define TEMPO_QUIZ           60.0f          // segundos da barra de tempo de cada quiz (AJUSTE AQUI)
#define ATRASO_RESPOSTA      0.4f           // evita responder sem querer apertando ENTER rápido demais
#define QUIZ_EM_ORDEM        1              // 1 = monitores na ordem 1-2-3-4 / 0 = qualquer ordem
#define VELOCIDADE_TEXTO     50.0f          // caracteres por segundo (máquina de escrever)
#define DURACAO_FADE         0.35f          // fade entre salas
#define DURACAO_FADE_BATALHA 0.2f           // fade rápido para entrar/sair da batalha

#define MAX_PAGINAS          8
#define NUM_OPCOES           4

#define FONTE_CAIXA          18
#define LARGURA_TEXTO_CAIXA  640
#define FONTE_PERGUNTA       18
#define LARGURA_PERGUNTA     680
#define LARGURA_OPCAO        650

// Conversas que não são de monitor
#define OBJ_NENHUM  -1
#define OBJ_LUCI    -2

// Batalha
#define MAX_BALAS            96
#define HP_MAX_BATALHA       20
#define VELOCIDADE_ALMA      170.0f
#define TEMPO_INVENCIVEL     1.0f
#define RAIO_BALA            7.0f
#define RAIO_ALMA            6.0f
#define DURACAO_MINIGAME     7.0f           // segundos de esquiva quando ERRA a resposta
#define DURACAO_COMBATE      11.0f          // segundos de esquiva quando o TEMPO ACABA
#define DURACAO_ANIM_CAIXA   0.35f
#define FONTE_BATALHA        20

// Geometria dos Mapas
// Sala inicial: porta na parede de CIMA
static const Rectangle PORTA_SALA            = {360, 42, 80, 36};
static const Rectangle ZONA_PORTA_SALA       = {360, 42, 80, 52};
// Corredor: porta de baixo volta para a sala / porta da direita é a saída
static const Rectangle PORTA_VOLTAR_CORREDOR = {100, 406, 90, 18};
static const Rectangle PORTA_FIM_CORREDOR    = {722, 265, 28, 140};

static const Vector2 SPAWN_SALA              = {400, 300};
static const Vector2 SPAWN_SALA_VOLTA        = {400, 115};  // logo abaixo da porta de cima
static const Vector2 SPAWN_CORREDOR          = {145, 368};  // logo acima da porta de baixo

// Caixa da batalha: larga para texto, estreita para desviar (como no Undertale)
static const Rectangle CAIXA_TEXTO_BATALHA   = {50, 270, 700, 160};
static const Rectangle CAIXA_DESVIO          = {275, 270, 250, 160};

#endif
