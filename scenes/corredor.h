// corredor.h - Cena do corredor dos monitores.

#ifndef SCENE_CORREDOR_H
#define SCENE_CORREDOR_H

#include "../core/game.h"

void DesenharPortaCorredor(Rectangle porta, Color corBorda);
void CorredorAtualizar(Jogo *g, float dt);
void CorredorDesenhar(const Jogo *g);

#endif
