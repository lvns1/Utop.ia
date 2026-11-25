// Resolução: o jogo desenha em 800x600 e estica para a janela; opções de vídeo e volume.
#ifndef UTOPIA_RESOLUCAO_H
#define UTOPIA_RESOLUCAO_H

#include <stdbool.h>
#include "raylib.h"
#include "raymath.h"
#include "../core/config.h"
#include "../core/tipos.h"
#include "../core/preferencias.h"

// =============================================================
// RESOLUÇÃO
// =============================================================
// O jogo é sempre desenhado em 800x600 numa textura, que depois é esticada
// para o tamanho da janela (mantendo a proporção, com faixas pretas se
// sobrar espaço). Assim nenhuma coordenada do jogo precisa mudar.

#define ARQUIVO_CONFIG   "utopia.cfg"
#define MAX_RESOLUCOES   8               // itens no máximo em cada lista suspensa

typedef enum { EXIBICAO_JANELA, EXIBICAO_TELA_CHEIA, NUM_MODOS_EXIBICAO } ModoExibicao;

typedef struct {
    int largura;
    int altura;                           // 0 x 0 = o maior tamanho que cabe no monitor
} Resolucao;

typedef struct {
    ModoExibicao modo;
    Resolucao resolucao;
    bool textoGrande;                     // TAMANHO DO TEXTO: GRANDE (issue #31)
    Preferencias pref;                    // abas EFEITOS e SONS (preferencias.h)
} ConfigExibicao;

#define NUM_RESOLUCOES_4X3  ((int)(sizeof(RESOLUCOES_4X3) / sizeof(RESOLUCOES_4X3[0])))

typedef struct {
    Shader shader;
    int locEscala;                        // -1 = shader não carregou (usa o filtro simples)
    int locTamanho;
    int locCorrupcao;                     // efeitos de tela (-1 = só o filtro de nitidez)
    int locTempo;
    int locEfeitos;
} FiltroNitidez;

// -------------------------------------------------------------
// TELA DE OPÇÕES: três abas
//   VÍDEO   - listas suspensas (modo de exibição, resolução e tamanho do texto)
//   EFEITOS - liga/desliga os efeitos visuais
//   SONS    - volumes (geral, música e efeitos sonoros)
// -------------------------------------------------------------

typedef enum { ABA_VIDEO, ABA_EFEITOS, ABA_SONS, NUM_ABAS_OPCOES } AbaOpcoes;
typedef enum { CAMPO_MODO, CAMPO_RESOLUCAO, CAMPO_TEXTO, NUM_CAMPOS_OPCOES } CampoOpcoes;                  // aba VÍDEO
typedef enum { EF_TELA, EF_CORRUPCAO, EF_LUZ, EF_NUMEROS, EF_TREMOR, EF_CAMERA, NUM_CAMPOS_EFEITOS } CampoEfeitos;   // aba EFEITOS
typedef enum { SOM_GERAL, SOM_MUSICA, SOM_EFEITOS, NUM_CAMPOS_SONS } CampoSons;                           // aba SONS

typedef struct {
    int aba;                              // AbaOpcoes
    int foco;                             // FOCO_ABAS, um campo da aba ou o botão VOLTAR (= número de campos da aba)
    int aberta;                           // lista aberta (CampoOpcoes, só na aba VÍDEO) ou -1
    int marcado;                          // item marcado dentro da lista aberta
    bool sair;                            // o jogador escolheu VOLTAR (ao menu ou ao pause)
    bool naPausa;                         // aberta pelo menu de pause: o botão diz VOLTAR À PAUSA
} MenuOpcoes;

#define FOCO_ABAS  -1                     // o foco está na fileira das abas

// O que a AtualizarOpcoes mudou
typedef enum { OPCOES_NADA, OPCOES_VIDEO, OPCOES_PREFERENCIAS } MudancaOpcoes;

#define ALTURA_ITEM_LISTA  26

// ----- Variáveis compartilhadas -----

// ----- Funções -----
void GuardarMonitor(void);
void AplicarExibicao(ConfigExibicao c);
ConfigExibicao LerConfigExibicao(void);
void SalvarConfigExibicao(ConfigExibicao c);
void AjustarMouseAJanela(void);
FiltroNitidez CarregarFiltroNitidez(void);
void DefinirCorrupcaoTela(float alvo);
void DesenharNaJanela(RenderTexture2D alvo, FiltroNitidez filtro);
MudancaOpcoes AtualizarOpcoes(MenuOpcoes *m, ConfigExibicao *cfg);
void DesenharOpcoes(const MenuOpcoes *m, ConfigExibicao cfg, Color fundo);

#endif // UTOPIA_RESOLUCAO_H
