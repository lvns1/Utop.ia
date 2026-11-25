// Menu de pause (continuar, opções, sair).
#ifndef UTOPIA_PAUSE_H
#define UTOPIA_PAUSE_H

#include <stdbool.h>
#include "raylib.h"
#include "raymath.h"
#include "../core/config.h"
#include "../core/tipos.h"

// =============================================================
// MENU DE PAUSE
// =============================================================

#define DESFOQUE_PAUSA  4        // quanto maior, mais desfocado fica o fundo (2 a 8)

// PAUSA_HISTORICO: as últimas falas da Luci (issue #30; o R abre a mesma tela direto do jogo)
// PAUSA_OPCOES: a mesma tela de opções do menu principal, por cima do jogo pausado
typedef enum { PAUSA_CONTINUAR, PAUSA_HISTORICO, PAUSA_OPCOES, PAUSA_LOBBY, PAUSA_MENU, PAUSA_SAIR, NUM_OPCOES_PAUSA } OpcaoPausa;

// ----- Variáveis compartilhadas -----

// ----- Funções -----
Texture2D CapturarFundoPausa(RenderTexture2D alvo);
int AtualizarPausa(int *selecao, bool temLobby);
void DesenharFundoPausa(Texture2D fundo);
void DesenharPausa(Texture2D fundo, int selecao, bool temLobby, int opConfirmando);

#endif // UTOPIA_PAUSE_H
