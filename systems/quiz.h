// quiz.h - Progresso do corredor (monitores, pontos, alucinação) e painel de pergunta.

#ifndef QUIZ_H
#define QUIZ_H

#include <stdbool.h>
#include "../core/config.h"
#include "../entities/monitor.h"
#include "dialogue.h"

// Progresso no corredor: estado de cada monitor, pontos e nível de alucinação.
typedef struct {
    EstadoMonitor estado[TOTAL_MONITORES];
    int pontos;
    int alucinacao;                         // cresce a cada erro (README: errar fortalece a alucinação)
    bool introVista;
    bool fimAnunciado;
} ProgressoCorredor;

int ContarConcluidos(const ProgressoCorredor *p);
int ProximoMonitor(const ProgressoCorredor *p);
int AlturaPainelPergunta(const SessaoDialogo *d);
void DialogoMostrarPergunta(SessaoDialogo *d);

#endif
