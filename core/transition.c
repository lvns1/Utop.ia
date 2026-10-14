#include "transition.h"
#include "raylib.h"

void IniciarTransicao(Transicao *t, TelaAtual destino, float duracao) {
    if (t->ativa) return;
    t->ativa = true;
    t->alfa = 0.0f;
    t->fase = 0;
    t->duracao = duracao;
    t->destino = destino;
}

bool AtualizarTransicao(Transicao *t, TelaAtual *telaAtual, float dt, TelaAtual *origemOut) {
    if (!t->ativa) return false;

    float passo = dt / t->duracao;
    bool trocouTela = false;

    if (t->fase == 0) {
        t->alfa += passo;
        if (t->alfa >= 1.0f) {
            t->alfa = 1.0f;
            if (origemOut) *origemOut = *telaAtual;
            *telaAtual = t->destino;
            t->fase = 1;
            trocouTela = true;
        }
    } else {
        t->alfa -= passo;
        if (t->alfa <= 0.0f) {
            t->alfa = 0.0f;
            t->ativa = false;
        }
    }
    return trocouTela;
}

void DesenharTransicao(const Transicao *t) {
    if (t->ativa) {
        DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, t->alfa));
    }
}
