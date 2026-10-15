// collision.c - Implementação das áreas de colisão e interação.

#include "collision.h"

// Área em que o jogador pode apertar E (o monitor + uma folga para
// os lados e para baixo, já que ele fica "em frente" dele).
Rectangle ZonaInteracao(Rectangle a) {
    return (Rectangle){ a.x - 16, a.y, a.width + 32, a.height + 90 };
}

// Quadrado de lado 'alcance' à frente do jogador, na direção em que ele olha.
Rectangle CaixaInteracaoJogador(Vector2 playerPos, Direcao direcaoAtual, float alcance) {
    Rectangle box = { playerPos.x - alcance / 2, playerPos.y - alcance / 2, alcance, alcance };
    switch (direcaoAtual) {
        case DIR_FRENTE:   box.y += TAM_SPRITE / 2; break;  // olhando pra baixo
        case DIR_COSTAS:   box.y -= TAM_SPRITE / 2; break;  // olhando pra cima
        case DIR_DIREITA:  box.x += TAM_SPRITE / 2; break;
        case DIR_ESQUERDA: box.x -= TAM_SPRITE / 2; break;
    }
    return box;
}
