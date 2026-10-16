// player.h - Jogador: movimento (WASD), hitboxes e desenho do sprite.

#ifndef PLAYER_H
#define PLAYER_H

#include "raylib.h"
#include "../core/config.h"

typedef struct {
    Texture2D textura;
    int quadros;
    bool espelhar;
} SpriteDirecao;

typedef struct {
    int quadro;
    float tempo;
} AnimacaoJogador;

SpriteDirecao CarregarSprite(
    const char *folha,
    const char *antigo,
    bool espelharAntigo
);

void AtualizarAnimacao(
    AnimacaoJogador *animacao,
    bool andando,
    float dt
);

Direcao MoverJogador(Vector2 *pos, Direcao direcaoAtual);

Rectangle HitboxJogador(Vector2 p);
Rectangle AreaJogador(Vector2 p);

void DesenharJogador(
    const SpriteDirecao sprites[4],
    Vector2 pos,
    Direcao direcao,
    int quadro
);

#endif
