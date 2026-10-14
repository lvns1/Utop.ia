#ifndef TRANSITION_H
#define TRANSITION_H

#include <stdbool.h>
#include "config.h"

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
