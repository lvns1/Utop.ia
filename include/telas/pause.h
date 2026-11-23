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

typedef enum { PAUSA_CONTINUAR, PAUSA_LOBBY, PAUSA_MENU, PAUSA_SAIR, NUM_OPCOES_PAUSA } OpcaoPausa;

// ----- Variáveis compartilhadas -----

// ----- Funções -----
Texture2D CapturarFundoPausa(RenderTexture2D alvo);
int AtualizarPausa(int *selecao, bool temLobby);
void DesenharPausa(Texture2D fundo, int selecao, bool temLobby, int opConfirmando);

#endif // UTOPIA_PAUSE_H
