// quiz.c - Funções do progresso do corredor e montagem do painel de pergunta.

#include "quiz.h"
#include "../utils/text.h"

// Quantos monitores já foram respondidos (acertando ou errando).
int ContarConcluidos(const ProgressoCorredor *p) {
    int total = 0;
    for (int i = 0; i < TOTAL_MONITORES; i++)
        if (p->estado[i] == MON_ACERTOU || p->estado[i] == MON_ERROU)
            total++;
    return total;
}

// Primeiro monitor que ainda não foi resolvido (-1 = todos resolvidos)
int ProximoMonitor(const ProgressoCorredor *p) {
    for (int i = 0; i < TOTAL_MONITORES; i++)
        if (p->estado[i] == MON_NEUTRO || p->estado[i] == MON_ATIVO)
            return i;
    return -1;
}

// Altura do painel da pergunta (cabeçalho + pergunta + alternativas + rodapé): serve para saber se cabe na tela.
int AlturaPainelPergunta(const SessaoDialogo *d) {
    int h = 62;                             // cabeçalho + barra de tempo
    h += ContarLinhas(d->texto) * 24 + 14;  // pergunta
    for (int i = 0; i < NUM_OPCOES; i++)
        h += ContarLinhas(d->opcoesTexto[i]) * (d->fonteOpcao + 5) + 10;
    h += 30;                                // rodapé
    return h;
}

// Abre a pergunta de múltipla escolha: zera a seleção e a barra de tempo e ajusta a fonte das alternativas.
void DialogoMostrarPergunta(SessaoDialogo *d) {
    static const int fontes[] = {16, 15, 14, 13};

    d->estado = DLG_ESCOLHA;
    d->selecao = 0;
    d->tempoRestante = TEMPO_QUIZ;

    DialogoMostrarTexto(d, d->conteudo.pergunta, LARGURA_PERGUNTA, FONTE_PERGUNTA);

    // As alternativas do PDF são longas: se não couberem, a fonte diminui um pouco
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
