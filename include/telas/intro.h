// Cenas animadas da introdução.
#ifndef UTOPIA_INTRO_H
#define UTOPIA_INTRO_H

#include <stdbool.h>
#include "raylib.h"
#include "raymath.h"
#include "../core/config.h"
#include "../core/tipos.h"


// ----- Variáveis compartilhadas -----

// ----- Funções -----
void DesenharCenaIntro(int etapa, Rectangle painel, float tempo);

#endif // UTOPIA_INTRO_H
