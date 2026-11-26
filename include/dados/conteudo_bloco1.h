// Bloco 1: falas da sala inicial e os quizzes dos monitores. Para editar perguntas/respostas, mexa no conteudo_bloco1.c.
#ifndef UTOPIA_CONTEUDO_BLOCO1_H
#define UTOPIA_CONTEUDO_BLOCO1_H

#include <stdbool.h>
#include "raylib.h"
#include "raymath.h"
#include "../core/config.h"
#include "../core/tipos.h"


// ----- Variáveis compartilhadas -----
extern const Dialogo dialogoLuciSala;
extern const Dialogo dialogoPortaSalaTrancada;
extern const MonitorQuiz monitores[TOTAL_MONITORES];
extern const Dialogo dialogoIntroCorredor;
extern const Dialogo dialogoFimCorredor;
extern const Dialogo dialogoPortaTrancada;
extern const Dialogo dialogoChefe;
extern const Dialogo dialogoChefeVolta;

// ----- Funções -----

#endif // UTOPIA_CONTEUDO_BLOCO1_H
