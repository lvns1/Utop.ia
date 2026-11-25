// Biblioteca dos Ecos, sala da Mestre e Bloco 3 (a lógica está em src/bloco2 e src/bloco3).
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../../include/core/fonte.h"
#include "../../include/core/mapas.h"
#include "../../include/telas/jogo.h"

void TelaBibliotecaAtualizar(Jogo *jg)
{
    jg->companionAngulo += jg->dt * 2.5f;

    EventoRogue evento = RogueAtualizar(&jg->biblioteca, &jg->progresso, &jg->dlg, jg->dt * (1.0f - jg->biblioteca.lentidao));   // câmera lenta do 1º parry

    if (evento == ROGUE_SAIR)
        IniciarTransicao(&jg->transicao, CORREDOR, DURACAO_FADE);

    // Venceu a ALUCINAÇÃO MESTRE: fim do Bloco 1 (ranking) e o Thomas já está no átrio do Bloco 2
    if (evento == ROGUE_MESTRE_VENCIDO) {
        jg->chefeVencido = true;
        jg->progresso.fimAnunciado = true;
        jg->pedirAutoSave = true;
        // O ranking só aparece no fim do jogo: aqui a história segue direto para a Biblioteca
    }

    // Venceu o Alucinador: fim da Biblioteca, com o ranking
    if (evento == ROGUE_VITORIA) {
        jg->pedirAutoSave = true;
        AbrirTelaRanking(&jg->telaRanking, &jg->ranking, "FIM DA BIBLIOTECA DOS ECOS", jg->nickname, jg->progresso.pontos, jg->idPartida);
    }

    // Usou as 3 vidas: a partida acaba. O save é apagado (CONTINUAR some), os pontos vão
    // para o ranking e, ao fechar, o menu
    if (evento == ROGUE_SEM_VIDAS) {
        ApagarSave();
        jg->existeSave = false;
        jg->pedirAutoSave = false;
        AbrirTelaRanking(&jg->telaRanking, &jg->ranking, "FIM DA PARTIDA", jg->nickname, jg->progresso.pontos, jg->idPartida);
        jg->voltarAoMenuDepoisDoRanking = true;
    }

    // Fim de UtopIA (Bloco 3): ranking e, ao fechar, o menu
    if (evento == ROGUE_FIM_JOGO) {
        jg->pedirAutoSave = true;
        AbrirTelaRanking(&jg->telaRanking, &jg->ranking, "FIM DE UTOPIA", jg->nickname, jg->progresso.pontos, jg->idPartida);
        jg->voltarAoMenuDepoisDoRanking = true;
    }
}

void TelaBibliotecaDesenhar(Jogo *jg)
{
    RogueDesenhar(&jg->biblioteca, jg->spritesJogador, jg->companionAngulo, jg->tempo, jg->nickname, jg->progresso.pontos);
    DialogoDesenhar(&jg->dlg);
    DesenharTelaRanking(&jg->telaRanking, &jg->ranking, jg->tempo);
    DesenharAvisoSave(jg->avisoSave, jg->saveFalhou, jg->tempo);
}
