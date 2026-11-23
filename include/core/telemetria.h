// Telemetria dos testes de usabilidade (utopia_teste.csv).
#ifndef UTOPIA_TELEMETRIA_H
#define UTOPIA_TELEMETRIA_H

#include <stdbool.h>
#include "raylib.h"
#include "raymath.h"
#include "../core/config.h"
#include "../core/tipos.h"

// =============================================================
// TELEMETRIA DOS TESTES DE USABILIDADE
//   Grava em "utopia_teste.csv" quanto tempo cada conversa ficou aberta,
//   quantas vezes o texto foi pulado, o resultado das batalhas e quando a
//   Luci precisou guiar o jogador. Abra no Excel/Planilhas para comparar.
//   Desligue com TELEMETRIA_ATIVA 0.
// =============================================================

#define TELEMETRIA_ATIVA     1
#define ARQUIVO_TELEMETRIA   "utopia_teste.csv"

// ----- Variáveis compartilhadas -----

// ----- Funções -----
void Telemetria(const char *evento, const char *detalhe, float valor, int quantidade);
void TelemetriaFechar(void);
float DtJogo(void);

#endif // UTOPIA_TELEMETRIA_H
