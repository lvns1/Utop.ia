// transition.c - Implementação do fade entre telas.

#include "transition.h"
#include "raylib.h"

// Começa um fade até a tela 'destino'. Se já houver um fade em andamento, ignora.
void IniciarTransicao(Transicao *t, TelaAtual destino, float duracao) {
    if (t->ativa) return;
    t->ativa = true;
    t->alfa = 0.0f;
    t->fase = 0;
    t->duracao = duracao;
    t->destino = destino;
}

// ---------- TRANSIÇÃO ENTRE TELAS (FADE) ----------

// Avança o fade. Devolve true no frame em que a tela é trocada (tela toda preta);
// nesse caso 'origemOut' recebe a tela de onde o jogador veio.
bool AtualizarTransicao(Transicao *t, TelaAtual *telaAtual, float dt, TelaAtual *origemOut) {
    if (!t->ativa) return false;

    float passo = dt / t->duracao;
    bool trocouTela = false;

    if (t->fase == 0) {
        // Escurecendo
        t->alfa += passo;
        if (t->alfa >= 1.0f) {
            t->alfa = 1.0f;
            if (origemOut) *origemOut = *telaAtual;
            *telaAtual = t->destino;
            t->fase = 1;
            trocouTela = true;
        }
    } else {
        // Clareando
        t->alfa -= passo;
        if (t->alfa <= 0.0f) {
            t->alfa = 0.0f;
            t->ativa = false;
        }
    }
    return trocouTela;
}

// Retângulo preto por cima de tudo, com a opacidade do fade.
void DesenharTransicao(const Transicao *t) {
    if (t->ativa) {
        DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, t->alfa));
    }
}
