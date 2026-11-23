// Tela da introdução e do nickname.
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../../include/core/fonte.h"
#include "../../include/core/mapas.h"
#include "../../include/telas/jogo.h"

void TelaIntroAtualizar(Jogo *jg)
{
    // =================================================
    // ETAPA FINAL: REGISTRO DO NICKNAME
    // =================================================

    if (jg->etapaIntro == 3) {

        // Lê uma letra por vez (A-Z)
        int tecla = GetKeyPressed();

        if (tecla >= KEY_A && tecla <= KEY_Z && jg->nicknamePos < 3) {

            jg->nickname[jg->nicknamePos] = (char)('A' + (tecla - KEY_A));
            jg->nicknamePos++;
        }

        // BACKSPACE apaga a última letra digitada.
        // Se não houver nenhuma letra, volta para o menu.
        if (IsKeyPressed(KEY_BACKSPACE)) {

            if (jg->nicknamePos > 0) {

                jg->nicknamePos--;
                jg->nickname[jg->nicknamePos] = '_';

            } else {

                jg->etapaIntro = 0;
                jg->tela = MENU;
            }
        }

        // ENTER (ou o botão CONTINUAR) só avança com as 3 letras preenchidas
        // e com um nome que ainda não está no ranking.
        if ((IsKeyPressed(KEY_ENTER) || jg->cliqueConfirmarNome) && jg->nicknamePos == 3) {

            if (NicknameDisponivel(&jg->ranking, jg->nickname)) {

                jg->etapaIntro = 0;
                jg->tela = JOGANDO;
                jg->avisoNomeRepetido = 0.0f;

            } else {

                jg->avisoNomeRepetido = 3.0f;
                jg->nickname[0] = jg->nickname[1] = jg->nickname[2] = '_';
                jg->nicknamePos = 0;
                TocarSom(sfxErro);
            }
        }

        jg->cliqueConfirmarNome = false;

        if (jg->avisoNomeRepetido > 0.0f)
            jg->avisoNomeRepetido -= jg->dt;
    }

    // =================================================
    // ETAPAS NORMAIS DA INTRODUÇÃO
    // =================================================

    else {

        // ENTER avança a introdução
        if (IsKeyPressed(KEY_ENTER)) {

            jg->etapaIntro++;
        }

        // BACKSPACE volta para o menu
        if (IsKeyPressed(KEY_BACKSPACE)) {

            jg->etapaIntro = 0;
            jg->tela = MENU;
        }
    }
}

void TelaIntroDesenhar(Jogo *jg)
{
    // Mesmo fundo do menu
    ClearBackground(jg->cor_de_fundo_menu);

    // -------------------------------------------------
    // ÁREA CENTRAL
    // -------------------------------------------------

    const int larguraConteudo = 500;
    const int xConteudo = (800 - larguraConteudo) / 2;

    // =================================================
    // INDICADOR DE ETAPAS
    // =================================================

    int espacamento = 14;
    int larguraIndicador = 3 * espacamento;

    int xIndicador = 400 - larguraIndicador / 2;

    for (int i = 0; i < 4; i++) {

        Color corEtapa;

        if (i == jg->etapaIntro)
            corEtapa = WHITE;
        else
            corEtapa = Fade(LIGHTGRAY, 0.35f);

        DrawCircle(xIndicador + i * espacamento, 48, 3, corEtapa);
    }

    const char *textoEtapa = TextFormat("Etapa %d de 4", jg->etapaIntro + 1);

    DrawText(textoEtapa, 400 - MeasureText(textoEtapa, 9) / 2, 58, 9, GRAY);

    // =================================================
    // PAINEL DA IMAGEM
    // =================================================

    Rectangle painelImagem = { xConteudo, 85, larguraConteudo, 255 };

    // Cena animada que ilustra o texto do narrador desta etapa
    DesenharCenaIntro(jg->etapaIntro, painelImagem, jg->tempo);

    // =================================================
    // CAIXA DO NARRADOR
    // =================================================

    Rectangle caixaNarrador = {
        xConteudo,
        355,
        larguraConteudo,
        (jg->etapaIntro == 3) ? 110 : 80
    };

    DrawRectangleRec(caixaNarrador, (Color){245, 245, 245, 255});
    DrawRectangleLinesEx(caixaNarrador, 1, Fade(LIGHTGRAY, 0.6f));

    // Conteúdo da caixa de narrativa
    if (jg->etapaIntro < 3) {

        DrawRectangle(xConteudo + 12, 349, 48, 16, LIGHTGRAY);
        DrawText("Narrador", xConteudo + 16, 353, 8, BLACK);
        DrawText(jg->textosIntro[jg->etapaIntro], xConteudo + 12, 380, 11, DARKGRAY);

    } else {

        // Etiqueta da etapa final
        DrawRectangle(xConteudo + 12, 349, 58, 16, LIGHTGRAY);
        DrawText("Marcador", xConteudo + 16, 353, 8, BLACK);
        DrawText("LUCI // REGISTRO DE IDENTIDADE!", xConteudo + 12, 377, 9, BLACK);
        DrawText("Digite seu codigo de identificacao de 3 letras:", xConteudo + 12, 393, 10, DARKGRAY);

        // Três campos do nickname, como no print
        for (int i = 0; i < 3; i++) {

            Rectangle caixaLetra = { 245 + i * 55, 405, 45, 45 };

            Color corCaixa =
                (i == jg->nicknamePos && jg->nicknamePos < 3)
                ? (Color){225, 225, 225, 255}
                : (Color){238, 238, 238, 255};

            DrawRectangleRec(caixaLetra, corCaixa);

            const char letra[2] = { jg->nickname[i], '\0' };

            DrawText(letra, (int)caixaLetra.x + 12, (int)caixaLetra.y + 5, 32, BLACK);
        }

        if (jg->avisoNomeRepetido > 0.0f)
            DrawText("Esse nome já está no ranking! Escolha outro.", xConteudo + 12, 453, 10, RED);
    }

    // =================================================
    // BOTÃO CONTINUAR
    // =================================================

    Rectangle btnContinuar = {
        xConteudo + larguraConteudo - 125,
        (jg->etapaIntro == 3) ? 475 : 457,
        125,
        30
    };

    Color corContinuar = BLACK;

    // Na última etapa, o botão só pode continuar com 3 letras.
    bool podeContinuar = (jg->etapaIntro < 3) || (jg->nicknamePos == 3);

    if (CheckCollisionPointRec(jg->mouse, btnContinuar) && podeContinuar) {

        corContinuar = DARKGRAY;

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {

            if (jg->etapaIntro < 3) {

                jg->etapaIntro++;

            } else if (jg->nicknamePos == 3) {

                jg->cliqueConfirmarNome = true;     // a lógica confere o nome no próximo quadro
            }
        }
    }

    DrawRectangleRec(btnContinuar, corContinuar);

    DrawText("CONTINUAR", (int)btnContinuar.x + 22, (int)btnContinuar.y + 9, 8, WHITE);
    DrawText(">", (int)btnContinuar.x + 102, (int)btnContinuar.y + 7, 12, WHITE);

    // =================================================
    // BOTÃO VOLTAR (o mesmo que BACKSPACE: na etapa do nome apaga a última letra)
    // =================================================

    Rectangle btnVoltarIntro = { (float)xConteudo, btnContinuar.y, 125, 30 };
    bool apagarLetra = (jg->etapaIntro == 3 && jg->nicknamePos > 0);
    Color corVoltar = BLACK;

    if (CheckCollisionPointRec(jg->mouse, btnVoltarIntro)) {

        corVoltar = DARKGRAY;

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {

            if (apagarLetra) {
                jg->nicknamePos--;
                jg->nickname[jg->nicknamePos] = '_';
            } else {
                jg->etapaIntro = 0;
                jg->tela = MENU;
            }
        }
    }

    DrawRectangleRec(btnVoltarIntro, corVoltar);
    DrawText("<", (int)btnVoltarIntro.x + 14, (int)btnVoltarIntro.y + 7, 12, WHITE);
    DrawText(apagarLetra ? "APAGAR" : "VOLTAR", (int)btnVoltarIntro.x + 32, (int)btnVoltarIntro.y + 9, 8, WHITE);

    // =================================================
    // INSTRUÇÕES
    // =================================================

    const char *instrucao1;
    const char *instrucao2;

    if (jg->etapaIntro == 3) {

        instrucao1 = "Digite A-Z | ENTER ou CONTINUAR para confirmar";
        instrucao2 = "BACKSPACE ou APAGAR apaga a ultima letra";

    } else {

        instrucao1 = "ENTER ou CONTINUAR para continuar";
        instrucao2 = "BACKSPACE ou VOLTAR para voltar ao menu";
    }

    DrawText(instrucao1, 400 - MeasureText(instrucao1, 10) / 2, 510, 10, GRAY);
    DrawText(instrucao2, 400 - MeasureText(instrucao2, 10) / 2, 528, 10, GRAY);
}
