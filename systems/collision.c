#include "collision.h"

Rectangle ZonaInteracao(Rectangle a) {
    return (Rectangle){ a.x - 16, a.y, a.width + 32, a.height + 90 };
}

Rectangle CaixaInteracaoJogador(Vector2 playerPos, Direcao direcaoAtual, float alcance) {
    Rectangle box = { playerPos.x - alcance / 2, playerPos.y - alcance / 2, alcance, alcance };
    switch (direcaoAtual) {
        case DIR_FRENTE:   box.y += TAM_SPRITE / 2; break;
        case DIR_COSTAS:   box.y -= TAM_SPRITE / 2; break;
        case DIR_DIREITA:  box.x += TAM_SPRITE / 2; break;
        case DIR_ESQUERDA: box.x -= TAM_SPRITE / 2; break;
    }
    return box;
}
