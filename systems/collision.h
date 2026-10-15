// collision.h - Áreas de colisão e de interação usadas pelas cenas.

#ifndef COLLISION_H
#define COLLISION_H

#include "raylib.h"
#include "../core/config.h"

Rectangle ZonaInteracao(Rectangle a);
Rectangle CaixaInteracaoJogador(Vector2 playerPos, Direcao direcaoAtual, float alcance);

#endif
