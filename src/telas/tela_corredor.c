// Corredor dos Monitores do Bloco 1.
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../../include/core/fonte.h"
#include "../../include/core/mapas.h"
#include "../../include/telas/jogo.h"

void TelaCorredorAtualizar(Jogo *jg)
{
    // O companion continua flutuando mesmo durante as conversas
    jg->companionAngulo += jg->dt * 2.5f;

    // -------------------------------------------------
    // CARTÃO DE RESUMO (depois de cada monitor)
    // -------------------------------------------------

    // (fechou o cartão do 3º monitor: a ALUCINAÇÃO MESTRE aparece logo abaixo, em EXPLORAÇÃO)
    if (jg->cartao.ativo) {

        AtualizarCartao(&jg->cartao, &jg->dlg);
    }

    else

    // -------------------------------------------------
    // CONVERSA / QUIZ EM ANDAMENTO (o personagem fica parado)
    // -------------------------------------------------

    if (jg->dlg.estado != DLG_NENHUM) {

        ResultadoDialogo r = DialogoAtualizar(&jg->dlg);
        int m = jg->dlg.objeto;

        // TAB no "saber mais" do cartão: o cartão volta
        if (r == RES_VOLTAR_CARTAO) {
            jg->cartao.ativo = true;
            jg->cartao.inicio = GetTime();
        }

        // Terminal de atalho (Bloco 1 -> Bloco 2): SIM vai ao átrio da Biblioteca sem marcar nada do
        // Bloco 1 como concluído (a porta normal continua exigindo a vitória sobre a Mestre)
        if (m == OBJ_ATALHO_BLOCO2 && (r == RES_SIM || r == RES_NAO)) {
            Telemetria("atalho_bloco1_bloco2", (r == RES_SIM) ? "sim" : "nao", (float)ContarConcluidos(&jg->progresso), jg->chefeVencido ? 1 : 0);
            if (r == RES_SIM)
                IniciarTransicao(&jg->transicao, BIBLIOTECA, DURACAO_FADE);
        }

        // Errou ou o tempo acabou: a alucinação ataca
        if ((r == RES_ERROU || r == RES_TEMPO) && m >= 0) {

            jg->progresso.alucinacao++;
            jg->quizDaBatalha = m;
            jg->motivoBatalha = r;

            BatalhaIniciar(
                &jg->batalha,
                (r == RES_TEMPO) ? BAT_COMBATE : BAT_MINIGAME,
                m,
                jg->progresso.alucinacao,
                HpDoCorredor(&jg->progresso)
            );

            IniciarTransicao(&jg->transicao, BATALHA, DURACAO_FADE_BATALHA);
        }

        else if (r == RES_FIM) {

            // Fim do quiz de um monitor: registra o resultado
            if (m >= 0 && jg->dlg.respondeu && jg->progresso.estado[m] == MON_ATIVO) {

                if (jg->dlg.acertou) {

                    jg->progresso.estado[m] = MON_ACERTOU;
                    jg->progresso.pontos += PONTOS_POR_ACERTO;

                } else {

                    jg->progresso.estado[m] = MON_ERROU;
                }

                jg->pedirAutoSave = true;     // AUTO SAVE: terminou um monitor
            }

            // Fim da fala da Luci sobre o chefe: começa a ALUCINAÇÃO MESTRE
            // (versão de treino: a sala dela, com a arma e o tutorial de combate)
            if (m == OBJ_CHEFE && MESTRE_TREINO_COMBATE) {

                jg->entrarSalaMestre = true;
                IniciarTransicao(&jg->transicao, BIBLIOTECA, DURACAO_FADE_BATALHA);
            }
            else if (m == OBJ_CHEFE) {

                BatalhaB2IniciarChefe(&jg->batalhaB2, jg->progresso.alucinacao);
                BatalhaB2LigarLuci(&jg->batalhaB2, &jg->estabilidadeLuci, &jg->ajudasPedidas);
                jg->salaDaBatalhaB2 = CORREDOR;
                jg->motivoBatalhaB2 = MOTIVO_CHEFE;
                IniciarTransicao(&jg->transicao, BATALHA_B2, DURACAO_FADE_BATALHA);
            }
        }
    }

    // -------------------------------------------------
    // PRIMEIRA ENTRADA: Luci explica o corredor
    // -------------------------------------------------

    else if (!jg->progresso.introVista) {

        jg->progresso.introVista = true;
        DialogoIniciar(&jg->dlg, &dialogoIntroCorredor, OBJ_NENHUM);
    }

    // -------------------------------------------------
    // FIM DO BLOCO 1: os 3 monitores resolvidos chamam a ALUCINAÇÃO MESTRE
    // e, vencida, o ranking (depois dele a Luci avisa que a saída abriu)
    // -------------------------------------------------

    else if (ContarConcluidos(&jg->progresso) == TOTAL_MONITORES && !jg->chefeVencido && !jg->chefeAnunciado) {

        jg->chefeAnunciado = true;
        DialogoIniciar(&jg->dlg, MESTRE_TREINO_COMBATE ? &dialogoChefeTreino : &dialogoChefe, OBJ_CHEFE);
    }

    else if (jg->chefeVencido && !jg->progresso.fimAnunciado) {

        AbrirTelaRanking(&jg->telaRanking, &jg->ranking, "FIM DO BLOCO 1", jg->nickname, jg->progresso.pontos, jg->idPartida);
    }

    // -------------------------------------------------
    // EXPLORAÇÃO LIVRE
    // -------------------------------------------------

    else {

        Vector2 posAntesCorredor = jg->playerPos;
        jg->direcaoAtual = MoverJogador(&jg->playerPos, jg->direcaoAtual);

        // Paredes do mapa: os pés batem nelas (e o jogador desliza junto da parede)
        ColidirComLista(&jg->playerPos, posAntesCorredor, PAREDES_CORREDOR, NUM_PAREDES_CORREDOR);

        Rectangle jogador = AreaJogador(jg->playerPos);

        // Interações (tecla E)
        if (IsKeyPressed(KEY_E)) {

            // Porta de baixo: volta para a sala
            if (CheckCollisionRecs(jogador, PORTA_VOLTAR_CORREDOR)) {

                IniciarTransicao(&jg->transicao, JOGANDO, DURACAO_FADE);
            }

            // Porta da direita: trancada até resolver tudo
            else if (CheckCollisionRecs(jogador, PORTA_FIM_CORREDOR)) {

                if (jg->chefeVencido) {

                    // Bloco 2: Biblioteca dos Ecos
                    IniciarTransicao(&jg->transicao, BIBLIOTECA, DURACAO_FADE);

                } else if (ContarConcluidos(&jg->progresso) == TOTAL_MONITORES) {

                    // A ALUCINAÇÃO MESTRE escapou da última vez: revanche
                    DialogoIniciar(&jg->dlg, &dialogoChefeVolta, OBJ_CHEFE);

                } else {

                    DialogoIniciar(&jg->dlg, &dialogoPortaTrancada, OBJ_NENHUM);
                }
            }

            // Terminal de atalho para o Bloco 2 (pede confirmação)
            else if (CheckCollisionRecs(jogador, ZONA_TERMINAL_CORREDOR)) {

                DialogoConfirmar(&jg->dlg, TEXTO_ATALHO_BLOCO2, OBJ_ATALHO_BLOCO2);
                Telemetria("atalho_bloco2_terminal", "", (float)ContarConcluidos(&jg->progresso), jg->chefeVencido ? 1 : 0);
            }

            // Monitores
            else {

                bool achouMonitor = false;

                for (int i = 0; i < TOTAL_MONITORES; i++) {

                    if (!CheckCollisionRecs(jogador, ZonaInteracao(monitores[i].area)))
                        continue;

                    achouMonitor = true;

                    int proximo = ProximoMonitor(&jg->progresso);

                    if (jg->progresso.estado[i] == MON_ACERTOU || jg->progresso.estado[i] == MON_ERROU) {

                        // Monitor já resolvido: mostra de novo o cartão do que foi aprendido
                        AbrirCartao(&jg->cartao, i, jg->progresso.estado[i] == MON_ACERTOU,
                                    TITULOS_RESUMO_MONITOR[i], FRASES_RESUMO_MONITOR[i], &saberMaisMonitores[i]);

                    } else if (QUIZ_EM_ORDEM && i != proximo) {

                        DialogoFalaAvulsa(&jg->dlg, TextFormat("Luci: Comece pelo MONITOR %d!", proximo + 1));

                    } else {

                        // NEUTRO -> ATIVO: a alucinação do monitor vira uma batalha
                        jg->progresso.estado[i] = MON_ATIVO;
                        BatalhaB2IniciarMonitor(&jg->batalhaB2, i, jg->progresso.alucinacao, HpDoCorredor(&jg->progresso));
                        BatalhaB2LigarLuci(&jg->batalhaB2, &jg->estabilidadeLuci, &jg->ajudasPedidas);
                        jg->salaDaBatalhaB2 = CORREDOR;
                        jg->motivoBatalhaB2 = MOTIVO_MONITOR;
                        IniciarTransicao(&jg->transicao, BATALHA_B2, DURACAO_FADE_BATALHA);
                    }

                    break;
                }

                if (!achouMonitor)
                    g_luciPedirDica = true;     // nada por perto: a Luci repete a dica
            }
        }
    }
}

void TelaCorredorDesenhar(Jogo *jg)
{
    ClearBackground((Color){8, 6, 18, 255});

    DesenharCenarioCorredor(jg->tempo);

    // -------------------------------------------------
    // PORTAS
    // -------------------------------------------------

    int concluidos = ContarConcluidos(&jg->progresso);
    bool saidaLiberada = jg->chefeVencido;
    bool chefeNaPorta = (concluidos == TOTAL_MONITORES && !jg->chefeVencido);

    float pulsoPorta = 0.5f + 0.5f * sinf(jg->tempo * 4.0f);

    Color bordaSaida = saidaLiberada ? Fade(GREEN, 0.55f + 0.45f * pulsoPorta)
                     : (chefeNaPorta ? Fade(MAGENTA, 0.55f + 0.45f * pulsoPorta) : MAROON);

    DesenharPortaMapaCorredor(PORTA_VOLTAR_CORREDOR, PURPLE, false, jg->tempo);
    DesenharPortaMapaCorredor(PORTA_FIM_CORREDOR, bordaSaida, !saidaLiberada, jg->tempo);

    DrawText(
        "SALA",
        (int)(PORTA_VOLTAR_CORREDOR.x + PORTA_VOLTAR_CORREDOR.width / 2) - MeasureText("SALA", 10) / 2,
        (int)(PORTA_VOLTAR_CORREDOR.y + PORTA_VOLTAR_CORREDOR.height) + 6,
        10,
        LIGHTGRAY
    );

    const char *textoSaida = saidaLiberada ? "SAIDA" : (chefeNaPorta ? "CHEFE" : "TRANCADA");

    DrawText(
        textoSaida,
        (int)(PORTA_FIM_CORREDOR.x + PORTA_FIM_CORREDOR.width) - MeasureText(textoSaida, 10) - 8,
        (int)PORTA_FIM_CORREDOR.y - 16,
        10,
        saidaLiberada ? GREEN : (chefeNaPorta ? MAGENTA : RED)
    );

    // Terminal de atalho para o Bloco 2 (parede de baixo, com a trilha de setas até a saída)
    DesenharTerminalAtalho(TERMINAL_ATALHO_CORREDOR, "PULAR PARA O BLOCO 2", CentroRetangulo(PORTA_FIM_CORREDOR), true, jg->tempo);

    // -------------------------------------------------
    // MONITORES
    // -------------------------------------------------

    Rectangle jogador = AreaJogador(jg->playerPos);

    // Só mostra os destaques quando o jogador está livre para agir
    bool explorando = (jg->dlg.estado == DLG_NENHUM && jg->progresso.introVista && !jg->transicao.ativa);

    for (int i = 0; i < TOTAL_MONITORES; i++) {

        Rectangle a = monitores[i].area;
        EstadoMonitor est = jg->progresso.estado[i];

        DesenharMonitor(i, a, est, jg->tempo, jg->spritesMonitor);

        // Nome embaixo, com a cor do estado
        const char *nome = monitores[i].nome;
        Color corNome = LIGHTGRAY;

        if (est == MON_ACERTOU)     { nome = TextFormat("%s (OK)", monitores[i].nome);   corNome = GREEN; }
        else if (est == MON_ERROU)  { nome = TextFormat("%s (ERRO)", monitores[i].nome); corNome = RED; }
        else if (est == MON_ATIVO)  { corNome = GOLD; }

        DrawText(
            nome,
            (int)(a.x + a.width / 2) - MeasureText(nome, 10) / 2,
            (int)(a.y + a.height) + 32,
            10,
            corNome
        );

        // Destaque quando o jogador está perto: a tecla E desenhada e o que ela faz
        if (explorando && CheckCollisionRecs(jogador, ZonaInteracao(a))) {

            DrawRectangleLinesEx(
                (Rectangle){ a.x - 4, a.y - 4, a.width + 8, a.height + 8 },
                2,
                GOLD
            );

            DesenharDicaAgora((Vector2){ a.x + a.width / 2, a.y - 30 }, "E", false, true, "ENFRENTAR", jg->tempo);
        }
        // Longe: uma seta pulando em cima do PRÓXIMO monitor (é para lá)
        else if (explorando && i == ProximoMonitor(&jg->progresso)) {
            DesenharSetaTutorial((Vector2){ a.x + a.width / 2, a.y - 10 }, 10.0f, GOLD, jg->tempo);
        }
    }

    // Dicas "PRESSIONE E" nas portas
    if (explorando) {

        if (CheckCollisionRecs(jogador, PORTA_VOLTAR_CORREDOR)) {

            DrawText(
                "PRESSIONE E",
                (int)(PORTA_VOLTAR_CORREDOR.x + PORTA_VOLTAR_CORREDOR.width) + 10,
                (int)PORTA_VOLTAR_CORREDOR.y,
                12,
                GOLD
            );
        }

        if (CheckCollisionRecs(jogador, PORTA_FIM_CORREDOR)) {

            DrawText(
                "PRESSIONE E",
                (int)(PORTA_FIM_CORREDOR.x + PORTA_FIM_CORREDOR.width) - MeasureText("PRESSIONE E", 12) - 8,
                (int)PORTA_FIM_CORREDOR.y - 32,
                12,
                GOLD
            );
        }
        else if (chefeNaPorta || saidaLiberada)      // monitores resolvidos: a seta vai para a porta da direita
            DesenharSetaTutorial((Vector2){ PORTA_FIM_CORREDOR.x + PORTA_FIM_CORREDOR.width / 2, PORTA_FIM_CORREDOR.y - 22 }, 10.0f,
                                 chefeNaPorta ? MAGENTA : GREEN, jg->tempo);

        // Terminal de atalho
        if (CheckCollisionRecs(jogador, ZONA_TERMINAL_CORREDOR)) {

            const char *txt = "PRESSIONE E: ATALHO";
            DrawRectangle((int)TERMINAL_ATALHO_CORREDOR.x - MeasureText(txt, 12) / 2 - 6, (int)TERMINAL_ATALHO_CORREDOR.y - 80, MeasureText(txt, 12) + 12, 18, Fade(BLACK, 0.75f));
            DrawText(txt, (int)TERMINAL_ATALHO_CORREDOR.x - MeasureText(txt, 12) / 2, (int)TERMINAL_ATALHO_CORREDOR.y - 77, 12, COR_ATALHO);
        }
    }

    // -------------------------------------------------
    // PERSONAGEM E COMPANION
    // -------------------------------------------------

    DesenharJogadorComLuci(jg->spritesJogador, jg->playerPos, jg->direcaoAtual, jg->animJogador.quadro,
                           true, jg->companionAngulo);

    // Calibração do mapa (MOSTRAR_COLISOES_CORREDOR = 1)
    if (MOSTRAR_COLISOES_CORREDOR) {
        for (int i = 0; i < NUM_PAREDES_CORREDOR; i++)
            DrawRectangleLinesEx(PAREDES_CORREDOR[i], 1, RED);
        for (int i = 0; i < TOTAL_MONITORES; i++)
            DrawRectangleLinesEx(ZonaInteracao(monitores[i].area), 1, GOLD);
        DrawRectangleLinesEx(ZONA_TERMINAL_CORREDOR, 1, COR_ATALHO);
        DrawRectangleLinesEx(PORTA_VOLTAR_CORREDOR, 1, GREEN);
        DrawRectangleLinesEx(PORTA_FIM_CORREDOR, 1, GREEN);
        DrawRectangleLinesEx(PesJogador(jg->playerPos), 1, SKYBLUE);
        DrawRectangleLinesEx(AreaJogador(jg->playerPos), 1, Fade(SKYBLUE, 0.5f));
    }

    // -------------------------------------------------
    // HUD
    // -------------------------------------------------

    DrawText(TextFormat("MONITORES: %d/%d", concluidos, TOTAL_MONITORES), 65, 62, 14, WHITE);
    DrawText(TextFormat("PONTOS: %d", jg->progresso.pontos), 230, 62, 14, GOLD);
    DrawText(
        TextFormat("ALUCINAÇÃO: %d", jg->progresso.alucinacao),
        360,
        62,
        14,
        (jg->progresso.alucinacao == 0) ? LIGHTGRAY : (jg->progresso.alucinacao < 3 ? ORANGE : RED)
    );
    DrawText("CORREDOR DO HISTÓRICO", 65, 82, 10, GRAY);
    DrawText(jg->nickname, 700 - MeasureText(jg->nickname, 14), 62, 14, WHITE);

    if (jg->dlg.estado == DLG_NENHUM) {

        DrawText("W A S D  -  MOVIMENTAR", 65, 582, 12, LIGHTGRAY);
    }

    // Ambiente, objetivo e marcador
    DesenharGlitchAmbiente(jg->progresso.alucinacao, jg->tempo);
    DesenharObjetivoHUD(&jg->objetivo);

    // Conversa por cima de tudo
    DialogoDesenhar(&jg->dlg);
    DesenharCartao(&jg->cartao, jg->tempo);
    DesenharTelaRanking(&jg->telaRanking, &jg->ranking, jg->tempo);
    DesenharAvisoSave(jg->avisoSave, jg->saveFalhou, jg->tempo);
}
