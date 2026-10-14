#ifndef PLAYER_H
#define PLAYER_H

#include "raylib.h"
#include "../core/config.h"

Direcao MoverJogador(Vector2 *pos, Direcao direcaoAtual);
Rectangle HitboxJogador(Vector2 p);
Rectangle AreaJogador(Vector2 p);
void DesenharJogador(Texture2D texFrente, Texture2D texCostas, Texture2D texLado, Vector2 pos, Direcao direcao);

#endif
