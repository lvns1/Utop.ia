#ifndef MONITOR_H
#define MONITOR_H

#include "raylib.h"
#include "../systems/dialogue.h"

typedef enum { MON_NEUTRO, MON_ATIVO, MON_ACERTOU, MON_ERROU } EstadoMonitor;

typedef struct {
    const char *nome;
    Rectangle area;
    Dialogo quiz;
} MonitorQuiz;

void DesenharMonitor(int indice, Rectangle a, EstadoMonitor estado, float tempo, const Texture2D sprites[4]);

#endif
