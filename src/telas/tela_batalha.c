// Batalhas do Bloco 1 (quiz e monitores).
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../../include/core/fonte.h"
#include "../../include/core/mapas.h"
#include "../../include/telas/jogo.h"

void TelaBatalhaAtualizar(Jogo *jg)
{
    if (BatalhaAtualizar(&jg->batalha)) {

        GuardarHpDoCorredor(&jg->progresso, jg->batalha.hp, jg->batalha.derrota);

        if (jg->batalha.derrota)
            jg->progresso.alucinacao++;

        // Errou (ou o tempo acabou): perde pontos.
        // Se a vida zerou na batalha, perde mais.
        int perdidos = AlterarPontos(&jg->progresso,
            -(PENALIDADE_ERRO + (jg->batalha.derrota ? PENALIDADE_HP_ZERO : 0)));

        // De volta ao corredor: Luci explica a resposta certa
        const Dialogo *q = &monitores[jg->quizDaBatalha].quiz;

        jg->dlg.conteudo = *q;
        jg->dlg.objeto = jg->quizDaBatalha;
        jg->dlg.respondeu = true;
        jg->dlg.acertou = false;

        DialogoMostrarFeedback(
            &jg->dlg,
            TextFormat(
                "Luci: %s A resposta certa era a %c. %s",
                (jg->motivoBatalha == RES_TEMPO) ? "O tempo acabou!" : "Não foi dessa vez.",
                'A' + q->correta,
                q->explicacao
            ),
            ORANGE,
            perdidos            // aparece em vermelho na caixa: "-5 PONTOS"
        );

        IniciarTransicao(&jg->transicao, CORREDOR, DURACAO_FADE_BATALHA);
    }
}

void TelaBatalhaB2Atualizar(Jogo *jg)
{
    if (BatalhaB2Atualizar(&jg->batalhaB2))
        FimBatalhaB2(&jg->ctxB2);       // vitória, ou E na tela de game over (o monitor fica como erro)
}

void TelaBatalhaDesenhar(Jogo *jg)
{
    BatalhaDesenhar(&jg->batalha, jg->nickname, jg->tempo);
}

void TelaBatalhaB2Desenhar(Jogo *jg)
{
    BatalhaB2Desenhar(&jg->batalhaB2, jg->nickname, jg->tempo);
}
