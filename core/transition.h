// transition.h - Fade entre telas (a troca de tela acontece com a tela toda preta).

#ifndef TRANSITION_H
#define TRANSITION_H

#include <stdbool.h>
#include "config.h"

// Fade entre telas: escurece (fase 0), troca de tela, clareia (fase 1)
typedef struct {
    bool ativa;
    float alfa;
    int fase;
    float duracao;
    TelaAtual destino;
} Transicao;

void IniciarTransicao(Transicao *t, TelaAtual destino, float duracao);
bool AtualizarTransicao(Transicao *t, TelaAtual *telaAtual, float dt, TelaAtual *origemOut);
void DesenharTransicao(const Transicao *t);

#endif
