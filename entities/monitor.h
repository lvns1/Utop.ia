// monitor.h - Monitores do corredor: estados, dados do quiz de cada um e desenho.

#ifndef MONITOR_H
#define MONITOR_H

#include "raylib.h"
#include "../systems/dialogue.h"

// NEUTRO = desligado | ATIVO = em quiz | ACERTOU = resposta certa | ERROU = resposta errada
typedef enum { MON_NEUTRO, MON_ATIVO, MON_ACERTOU, MON_ERROU } EstadoMonitor;

// Um monitor do corredor: nome, posição na tela e o quiz (conversa + pergunta) que ele mostra.
typedef struct {
    const char *nome;
    Rectangle area;                         // tela do monitor (o pé é desenhado embaixo)
    Dialogo quiz;
} MonitorQuiz;

void DesenharMonitor(int indice, Rectangle a, EstadoMonitor estado, float tempo, const Texture2D sprites[4]);

#endif
