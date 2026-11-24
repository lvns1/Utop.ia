// Histórico das falas da Luci (issue #30): guarda as últimas falas dela e mostra numa tela de consulta.
#ifndef UTOPIA_HISTORICO_LUCI_H
#define UTOPIA_HISTORICO_LUCI_H

#include <stdbool.h>
#include "raylib.h"
#include "../core/config.h"

// =============================================================
// HISTÓRICO DAS FALAS DA LUCI
//   Toda fala dela (caixa de diálogo, balão do topo, dicas e comentários das batalhas,
//   laboratórios) entra aqui. R abre a tela (o jogo pausa); ela também está no menu de pause.
// =============================================================

#define MAX_HISTORICO_LUCI      40      // quantas falas ficam guardadas (as mais antigas saem)
#define TAM_FALA_HISTORICO      320
#define TECLA_HISTORICO_LUCI    KEY_R   // R de "rever" (fora do diálogo e também durante ele)

// ----- Funções -----
void HistoricoLuciAdicionar(const char *texto);
void HistoricoLuciLimpar(void);
int  HistoricoLuciTotal(void);

// Tela do histórico (dentro do pause). Devolve true quando o jogador fecha (BACKSPACE, ENTER, E ou o X).
// O ESC fica com o main (direto = aberta pelo R: ESC e fechar voltam para o jogo; senão voltam ao menu de pause).
bool AtualizarHistoricoLuci(float *rolagem);
void DesenharHistoricoLuci(Texture2D fundo, float rolagem, bool direto);

#endif // UTOPIA_HISTORICO_LUCI_H
