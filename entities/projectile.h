// projectile.h - Projéteis ("dados corrompidos") da batalha.

#ifndef PROJECTILE_H
#define PROJECTILE_H

#include <stdbool.h>
#include "raylib.h"

// Um "dado corrompido" da batalha: um caractere que se move pela caixa de desvio.
typedef struct {
    bool ativa;
    Vector2 pos;
    Vector2 vel;
    char glifo;
} Bala;

void DesenharBala(const Bala *b);

#endif
