// projectile.c - Desenho dos projéteis da batalha.

#include "projectile.h"

// Desenha a bala como um caractere; balas inativas não aparecem.
void DesenharBala(const Bala *b) {
    if (!b->ativa) return;
    char g[2] = { b->glifo, '\0' };
    DrawText(
        g,
        (int)b->pos.x - MeasureText(g, 20) / 2,
        (int)b->pos.y - 10,
        20,
        WHITE
    );
}
