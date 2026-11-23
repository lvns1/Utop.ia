// Fonte do jogo (DejaVu Sans Condensed Bold, com acentos).
// Depois deste header, DrawText e MeasureText usam a fonte do jogo em todo o código.
#ifndef UTOPIA_FONTE_H
#define UTOPIA_FONTE_H

#include "raylib.h"

void CarregarFonteJogo(void);
void DescarregarFonteJogo(void);
void DesenharTextoJogo(const char *texto, int x, int y, int tam, Color cor);
int MedirTextoJogo(const char *texto, int tam);

// A partir daqui, todo o jogo usa a fonte nova (fonte.c não liga as macros: ele chama as originais da Raylib)
#ifndef UTOPIA_FONTE_IMPL
#define DrawText     DesenharTextoJogo
#define MeasureText  MedirTextoJogo
#endif

#endif // UTOPIA_FONTE_H
