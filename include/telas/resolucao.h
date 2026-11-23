// Resolução: o jogo desenha em 800x600 e estica para a janela; opções de vídeo e volume.
#ifndef UTOPIA_RESOLUCAO_H
#define UTOPIA_RESOLUCAO_H

#include <stdbool.h>
#include "raylib.h"
#include "raymath.h"
#include "../core/config.h"
#include "../core/tipos.h"

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
} ConfigExibicao;

#define NUM_RESOLUCOES_4X3  ((int)(sizeof(RESOLUCOES_4X3) / sizeof(RESOLUCOES_4X3[0])))

typedef struct {
    Shader shader;
    int locEscala;                        // -1 = shader não carregou (usa o filtro simples)
    int locTamanho;
} FiltroNitidez;

// -------------------------------------------------------------
// TELA DE OPÇÕES: duas listas suspensas (modo de exibição e resolução)
// -------------------------------------------------------------

typedef enum { CAMPO_MODO, CAMPO_RESOLUCAO, NUM_CAMPOS_OPCOES } CampoOpcoes;

typedef struct {
    int foco;                             // campo marcado (CampoOpcoes)
    int aberta;                           // lista aberta (CampoOpcoes) ou -1
    int marcado;                          // item marcado dentro da lista aberta
    bool sair;                            // o jogador escolheu VOLTAR AO MENU
} MenuOpcoes;

#define CAMPO_VOLTAR  NUM_CAMPOS_OPCOES     // o botão VOLTAR é o último "campo" do foco

#define ALTURA_ITEM_LISTA  26

// ----- Variáveis compartilhadas -----

// ----- Funções -----
void GuardarMonitor(void);
void AplicarExibicao(ConfigExibicao c);
ConfigExibicao LerConfigExibicao(void);
void SalvarConfigExibicao(ConfigExibicao c);
void AjustarMouseAJanela(void);
FiltroNitidez CarregarFiltroNitidez(void);
void DesenharNaJanela(RenderTexture2D alvo, FiltroNitidez filtro);
bool AtualizarOpcoes(MenuOpcoes *m, ConfigExibicao *cfg);
void DesenharOpcoes(const MenuOpcoes *m, ConfigExibicao cfg, Color fundo);

#endif // UTOPIA_RESOLUCAO_H
