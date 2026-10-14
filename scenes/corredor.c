#include "corredor.h"
#include "../entities/player.h"
#include "../entities/luci.h"
#include "../entities/monitor.h"
#include "../systems/collision.h"
#include "../data/dialogues.h"
#include "../data/quizzes.h"
#include <math.h>

void DesenharPortaCorredor(Rectangle porta, Color corBorda) {
    DrawRectangleRec(porta, (Color){35, 25, 55, 255});
    DrawRectangleLinesEx(porta, 3, corBorda);

    if (porta.width > porta.height)
        DrawCircle((int)(porta.x + porta.width - 12), (int)(porta.y + porta.height / 2), 3, GOLD);
    else
        DrawCircle((int)(porta.x + 9), (int)(porta.y + porta.height / 2), 4, GOLD);
}

void CorredorAtualizar(Jogo *g, float dt) {
    g->companionAngulo += dt * 2.5f;

    if (g->dlg.estado != DLG_NENHUM) {
        ResultadoDialogo r = DialogoAtualizar(&g->dlg);
        int m = g->dlg.objeto;

        if ((r == RES_ERROU || r == RES_TEMPO) && m >= 0) {
            g->progresso.alucinacao++;
            g->quizDaBatalha = m;
            g->motivoBatalha = r;

            BatalhaIniciar(
                &g->batalha,
                (r == RES_TEMPO) ? BAT_COMBATE : BAT_MINIGAME,
                m,
                g->progresso.alucinacao
            );

            IniciarTransicao(&g->transicao, BATALHA, DURACAO_FADE_BATALHA);
        } else if (r == RES_FIM) {
            if (m >= 0 && g->dlg.respondeu && g->progresso.estado[m] == MON_ATIVO) {
                if (g->dlg.acertou) {
                    g->progresso.estado[m] = MON_ACERTOU;
                    g->progresso.pontos += PONTOS_POR_ACERTO;
                } else {
                    g->progresso.estado[m] = MON_ERROU;
                }
            }

            if (ContarConcluidos(&g->progresso) == TOTAL_MONITORES && !g->progresso.fimAnunciado) {
                g->progresso.fimAnunciado = true;
                DialogoIniciar(&g->dlg, &dialogoFimCorredor, OBJ_NENHUM);
            }
        }
    } else if (!g->progresso.introVista) {
        g->progresso.introVista = true;
        DialogoIniciar(&g->dlg, &dialogoIntroCorredor, OBJ_NENHUM);
    } else {
        g->direcaoAtual = MoverJogador(&g->playerPos, g->direcaoAtual);

        if (g->playerPos.x < 80)  g->playerPos.x = 80;
        if (g->playerPos.x > 720) g->playerPos.x = 720;
        if (g->playerPos.y < 285) g->playerPos.y = 285;
        if (g->playerPos.y > 395) g->playerPos.y = 395;

        Rectangle jogador = AreaJogador(g->playerPos);

        if (IsKeyPressed(KEY_E)) {
            if (CheckCollisionRecs(jogador, PORTA_VOLTAR_CORREDOR)) {
                IniciarTransicao(&g->transicao, JOGANDO, DURACAO_FADE);
            } else if (CheckCollisionRecs(jogador, PORTA_FIM_CORREDOR)) {
                if (ContarConcluidos(&g->progresso) == TOTAL_MONITORES)
                    DialogoIniciar(&g->dlg, &dialogoPortaLiberada, OBJ_NENHUM);
                else
                    DialogoIniciar(&g->dlg, &dialogoPortaTrancada, OBJ_NENHUM);
            } else {
                for (int i = 0; i < TOTAL_MONITORES; i++) {
                    if (!CheckCollisionRecs(jogador, ZonaInteracao(monitores[i].area)))
                        continue;

                    const Dialogo *q = &monitores[i].quiz;
                    int proximo = ProximoMonitor(&g->progresso);

                    if (g->progresso.estado[i] == MON_ACERTOU) {
                        DialogoFalaAvulsa(&g->dlg, TextFormat("* O %s mostra um sinal verde. Lição concluída!", monitores[i].nome));
                    } else if (g->progresso.estado[i] == MON_ERROU) {
                        DialogoFalaAvulsa(&g->dlg, TextFormat("* O %s ainda chia um pouco. A resposta certa era a %c.", monitores[i].nome, 'A' + q->correta));
                    } else if (QUIZ_EM_ORDEM && i != proximo) {
                        DialogoFalaAvulsa(&g->dlg, TextFormat("Luci: Vamos na ordem, uma lição puxa a outra. Comece pelo monitor %d.", proximo + 1));
                    } else {
                        g->progresso.estado[i] = MON_ATIVO;
                        DialogoIniciar(&g->dlg, q, i);
                    }
                    break;
                }
            }
        }
    }
}

void CorredorDesenhar(const Jogo *g) {
    ClearBackground((Color){8, 6, 18, 255});
    Rectangle corredor = {50, 100, 700, 320};

    DrawRectangle(50, 100, 700, 140, (Color){26, 20, 46, 255});
    DrawRectangle(50, 240, 700, 180, (Color){18, 15, 30, 255});
    DrawRectangle(50, 236, 700, 6, Fade(PURPLE, 0.45f));

    for (int x = 50; x < 750; x += 70)
        DrawLine(x, 242, x, 420, Fade(PURPLE, 0.12f));

    for (int i = 0; i < 5; i++) {
        float pulso = 0.5f + 0.5f * sinf(g->tempo * 1.5f + i * 1.3f);
        int lx = 120 + i * 140;
        DrawCircle(lx, 108, 20, Fade(SKYBLUE, 0.04f + 0.08f * pulso));
        DrawRectangle(lx - 12, 102, 24, 5, Fade(SKYBLUE, 0.5f + 0.4f * pulso));
    }

    DrawRectangleLinesEx(corredor, 4, Fade(PURPLE, 0.55f));

    int concluidos = ContarConcluidos(&g->progresso);
    bool saidaLiberada = (concluidos == TOTAL_MONITORES);
    float pulsoPorta = 0.5f + 0.5f * sinf(g->tempo * 4.0f);
    Color bordaSaida = saidaLiberada ? Fade(GREEN, 0.55f + 0.45f * pulsoPorta) : MAROON;

    DesenharPortaCorredor(PORTA_VOLTAR_CORREDOR, PURPLE);
    DesenharPortaCorredor(PORTA_FIM_CORREDOR, bordaSaida);

    DrawText(
        "SALA",
        (int)(PORTA_VOLTAR_CORREDOR.x + PORTA_VOLTAR_CORREDOR.width / 2) - MeasureText("SALA", 10) / 2,
        (int)(PORTA_VOLTAR_CORREDOR.y + PORTA_VOLTAR_CORREDOR.height) + 6, 10, LIGHTGRAY
    );

    const char *textoSaida = saidaLiberada ? "SAIDA" : "TRANCADA";
    DrawText(
        textoSaida,
        (int)(PORTA_FIM_CORREDOR.x + PORTA_FIM_CORREDOR.width) - MeasureText(textoSaida, 10) - 8,
        (int)PORTA_FIM_CORREDOR.y - 16, 10, saidaLiberada ? GREEN : RED
    );

    Rectangle jogador = AreaJogador(g->playerPos);
    bool explorando = (g->dlg.estado == DLG_NENHUM && g->progresso.introVista && !g->transicao.ativa);

    for (int i = 0; i < TOTAL_MONITORES; i++) {
        Rectangle a = monitores[i].area;
        EstadoMonitor est = g->progresso.estado[i];

        DesenharMonitor(i, a, est, g->tempo, g->spritesMonitor);

        const char *nome = monitores[i].nome;
        Color corNome = LIGHTGRAY;
        if (est == MON_ACERTOU)    { nome = TextFormat("%s (OK)", monitores[i].nome);   corNome = GREEN; }
        else if (est == MON_ERROU) { nome = TextFormat("%s (ERRO)", monitores[i].nome); corNome = RED; }
        else if (est == MON_ATIVO) { corNome = GOLD; }

        DrawText(
            nome,
            (int)(a.x + a.width / 2) - MeasureText(nome, 10) / 2,
            (int)(a.y + a.height) + 32, 10, corNome
        );

        if (explorando && CheckCollisionRecs(jogador, ZonaInteracao(a))) {
            DrawRectangleLinesEx((Rectangle){ a.x - 4, a.y - 4, a.width + 8, a.height + 8 }, 2, GOLD);
            DrawText("PRESSIONE E", (int)(a.x + a.width / 2) - MeasureText("PRESSIONE E", 12) / 2, (int)a.y - 22, 12, GOLD);
        }
    }

    if (explorando) {
        if (CheckCollisionRecs(jogador, PORTA_VOLTAR_CORREDOR)) {
            DrawText("PRESSIONE E", (int)(PORTA_VOLTAR_CORREDOR.x + PORTA_VOLTAR_CORREDOR.width) + 10, (int)PORTA_VOLTAR_CORREDOR.y, 12, GOLD);
        }
        if (CheckCollisionRecs(jogador, PORTA_FIM_CORREDOR)) {
            DrawText("PRESSIONE E", (int)(PORTA_FIM_CORREDOR.x + PORTA_FIM_CORREDOR.width) - MeasureText("PRESSIONE E", 12) - 8, (int)PORTA_FIM_CORREDOR.y - 32, 12, GOLD);
        }
    }

    DesenharJogador(g->texFrente, g->texCostas, g->texLado, g->playerPos, g->direcaoAtual);
    DesenharCompanion(g->playerPos, g->companionAngulo);

    DrawText(TextFormat("MONITORES: %d/%d", concluidos, TOTAL_MONITORES), 65, 62, 14, WHITE);
    DrawText(TextFormat("PONTOS: %d", g->progresso.pontos), 230, 62, 14, GOLD);
    DrawText(
        TextFormat("ALUCINAÇÃO: %d", g->progresso.alucinacao),
        360, 62, 14,
        (g->progresso.alucinacao == 0) ? LIGHTGRAY : (g->progresso.alucinacao < 3 ? ORANGE : RED)
    );
    DrawText("CORREDOR DO HISTÓRICO", 65, 82, 10, GRAY);
    DrawText(g->nickname, 700 - MeasureText(g->nickname, 14), 62, 14, WHITE);

    if (g->dlg.estado == DLG_NENHUM) {
        DrawText("W A S D  -  MOVIMENTAR", 65, 452, 12, LIGHTGRAY);
        DrawText("E  -  INTERAGIR", 65, 469, 12, LIGHTGRAY);
    }

    DialogoDesenhar(&g->dlg);
}
