#ifndef PROJECTILE_H
#define PROJECTILE_H

#include <stdbool.h>
#include "raylib.h"

typedef struct {
    bool ativa;
    Vector2 pos;
    Vector2 vel;
    char glifo;
} Bala;

void DesenharBala(const Bala *b);

#endif
