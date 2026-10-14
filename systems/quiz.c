#include "quiz.h"
#include "../utils/text.h"

int ContarConcluidos(const ProgressoCorredor *p) {
    int total = 0;
    for (int i = 0; i < TOTAL_MONITORES; i++)
        if (p->estado[i] == MON_ACERTOU || p->estado[i] == MON_ERROU)
            total++;
    return total;
}

int ProximoMonitor(const ProgressoCorredor *p) {
    for (int i = 0; i < TOTAL_MONITORES; i++)
        if (p->estado[i] == MON_NEUTRO || p->estado[i] == MON_ATIVO)
            return i;
    return -1;
}

int AlturaPainelPergunta(const SessaoDialogo *d) {
    int h = 62;
    h += ContarLinhas(d->texto) * 24 + 14;
    for (int i = 0; i < NUM_OPCOES; i++)
        h += ContarLinhas(d->opcoesTexto[i]) * (d->fonteOpcao + 5) + 10;
    h += 30;
    return h;
}

void DialogoMostrarPergunta(SessaoDialogo *d) {
    static const int fontes[] = {16, 15, 14, 13};

    d->estado = DLG_ESCOLHA;
    d->selecao = 0;
    d->tempoRestante = TEMPO_QUIZ;

    DialogoMostrarTexto(d, d->conteudo.pergunta, LARGURA_PERGUNTA, FONTE_PERGUNTA);

    for (int f = 0; f < 4; f++) {
        d->fonteOpcao = fontes[f];
        for (int i = 0; i < NUM_OPCOES; i++) {
            QuebrarTexto(
                TextFormat("%c) %s", 'A' + i, d->conteudo.opcoes[i]),
                LARGURA_OPCAO,
                d->fonteOpcao,
                d->opcoesTexto[i],
                (int)sizeof(d->opcoesTexto[i])
            );
        }
        if (AlturaPainelPergunta(d) <= ALTURA_TELA - 30)
            break;
    }
}
