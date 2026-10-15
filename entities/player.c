// player.c - Implementação do jogador.

#include "player.h"
#include "raymath.h"

// Lê W A S D, move o jogador (sem ficar mais rápido na diagonal)
// e devolve para qual lado o sprite deve olhar.
Direcao MoverJogador(Vector2 *pos, Direcao direcaoAtual) {
    Vector2 direcao = {0};
    Direcao nova = direcaoAtual;

    if (IsKeyDown(KEY_D)) { direcao.x += 1.0f; nova = DIR_DIREITA; }
    if (IsKeyDown(KEY_A)) { direcao.x -= 1.0f; nova = DIR_ESQUERDA; }
    if (IsKeyDown(KEY_S)) { direcao.y += 1.0f; nova = DIR_FRENTE; }
    if (IsKeyDown(KEY_W)) { direcao.y -= 1.0f; nova = DIR_COSTAS; }  // para cima = de costas

    if (Vector2Length(direcao) > 0.0f) {
        direcao = Vector2Normalize(direcao);
        *pos = Vector2Add(*pos, Vector2Scale(direcao, VELOCIDADE_JOGADOR * GetFrameTime()));
    }
    return nova;
}

// Hitbox menor, usada para colidir com a Luci
Rectangle HitboxJogador(Vector2 p) {
    return (Rectangle){ p.x - TAM_SPRITE / 4, p.y - TAM_SPRITE / 4, TAM_SPRITE / 2, TAM_SPRITE / 2 };
}

// Área usada para portas e monitores
Rectangle AreaJogador(Vector2 p) {
    return (Rectangle){ p.x - 18, p.y - 22, 36, 44 };
}

// Desenha o sprite da direção atual centrado em 'pos' (esquerda = sprite do lado espelhado).
void DesenharJogador(Texture2D texFrente, Texture2D texCostas, Texture2D texLado, Vector2 pos, Direcao direcao) {
    Texture2D texAtual;
    float flip = 1.0f;                      // 1 = normal, -1 = espelhado (usado p/ olhar p/ esquerda)

    switch (direcao) {
        case DIR_FRENTE:   texAtual = texFrente; break;
        case DIR_COSTAS:   texAtual = texCostas; break;
        case DIR_DIREITA:  texAtual = texLado;   break;
        case DIR_ESQUERDA: texAtual = texLado; flip = -1.0f; break;
        default:           texAtual = texFrente; break;
    }

    Rectangle sourceRec = { 0.0f, 0.0f, (float)texAtual.width * flip, (float)texAtual.height };
    Rectangle destRec = { pos.x, pos.y, TAM_SPRITE, TAM_SPRITE };
    Vector2 origin = { TAM_SPRITE / 2, TAM_SPRITE / 2 };

    DrawTexturePro(texAtual, sourceRec, destRec, origin, 0.0f, WHITE);
}
