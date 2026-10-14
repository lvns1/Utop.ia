#ifndef QUIZ_H
#define QUIZ_H

#include <stdbool.h>
#include "../core/config.h"
#include "../entities/monitor.h"
#include "dialogue.h"

typedef struct {
    EstadoMonitor estado[TOTAL_MONITORES];
    int pontos;
    int alucinacao;
    bool introVista;
    bool fimAnunciado;
} ProgressoCorredor;

int ContarConcluidos(const ProgressoCorredor *p);
int ProximoMonitor(const ProgressoCorredor *p);
int AlturaPainelPergunta(const SessaoDialogo *d);
void DialogoMostrarPergunta(SessaoDialogo *d);

#endif
