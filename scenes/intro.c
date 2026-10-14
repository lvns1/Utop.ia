#include "intro.h"
#include "../data/dialogues.h"

void IntroAtualizar(Jogo *g) {
    Vector2 mouse = GetMousePosition();
    const int larguraConteudo = 500;
    const int xConteudo = (800 - larguraConteudo) / 2;

    Rectangle btnContinuar = {
        xConteudo + larguraConteudo - 125,
        (g->etapaIntro == 3) ? 475 : 457,
        125,
        30
    };

    bool podeContinuar = (g->etapaIntro < 3) || (g->nicknamePos == 3);
    if (CheckCollisionPointRec(mouse, btnContinuar) && podeContinuar && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (g->etapaIntro < 3) {
            g->etapaIntro++;
        } else if (g->nicknamePos == 3) {
            g->etapaIntro = 0;
            g->tela = JOGANDO;
        }
        return;
    }

    if (g->etapaIntro == 3) {
        int tecla = GetKeyPressed();
        if (tecla >= KEY_A && tecla <= KEY_Z && g->nicknamePos < 3) {
            g->nickname[g->nicknamePos] = (char)('A' + (tecla - KEY_A));
            g->nicknamePos++;
        }

        if (IsKeyPressed(KEY_BACKSPACE)) {
            if (g->nicknamePos > 0) {
                g->nicknamePos--;
                g->nickname[g->nicknamePos] = '_';
            } else {
                g->etapaIntro = 0;
                g->tela = MENU;
            }
        }

        if (IsKeyPressed(KEY_ENTER) && g->nicknamePos == 3) {
            g->etapaIntro = 0;
            g->tela = JOGANDO;
        }
    } else {
        if (IsKeyPressed(KEY_ENTER)) {
            g->etapaIntro++;
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            g->etapaIntro = 0;
            g->tela = MENU;
        }
    }
}

void IntroDesenhar(const Jogo *g) {
    ClearBackground(g->corFundoMenu);

    const int larguraConteudo = 500;
    const int xConteudo = (800 - larguraConteudo) / 2;

    int espacamento = 14;
    int larguraIndicador = 3 * espacamento;
    int xIndicador = 400 - larguraIndicador / 2;

    for (int i = 0; i < 4; i++) {
        Color corEtapa = (i == g->etapaIntro) ? WHITE : Fade(LIGHTGRAY, 0.35f);
        DrawCircle(xIndicador + i * espacamento, 48, 3, corEtapa);
    }

    const char *textoEtapa = TextFormat("Etapa %d de 4", g->etapaIntro + 1);
    DrawText(textoEtapa, 400 - MeasureText(textoEtapa, 9) / 2, 58, 9, GRAY);

    Rectangle painelImagem = { xConteudo, 85, larguraConteudo, 255 };
    DrawRectangleRec(painelImagem, (Color){8, 6, 18, 255});
    DrawRectangleLinesEx(painelImagem, 1, Fade(LIGHTGRAY, 0.65f));

    DrawCircle(xConteudo + 415, 140, 30, Fade(PURPLE, 0.30f));
    DrawCircle(xConteudo + 415, 140, 19, Fade(PURPLE, 0.45f));
    DrawRectangle(xConteudo + 55, 270, 390, 3, Fade(LIGHTGRAY, 0.25f));
    DrawRectangle(xConteudo + 95, 230, 90, 40, Fade(DARKPURPLE, 0.55f));
    DrawRectangle(xConteudo + 205, 210, 150, 60, Fade(DARKPURPLE, 0.35f));

    const char *textoImagem = "IMAGEM DA HISTORIA DO JOGO";
    DrawText(textoImagem, 400 - MeasureText(textoImagem, 11) / 2, 245, 11, Fade(WHITE, 0.65f));

    Rectangle caixaNarrador = {
        xConteudo, 355, larguraConteudo, (g->etapaIntro == 3) ? 110 : 80
    };

    DrawRectangleRec(caixaNarrador, (Color){245, 245, 245, 255});
    DrawRectangleLinesEx(caixaNarrador, 1, Fade(LIGHTGRAY, 0.6f));

    if (g->etapaIntro < 3) {
        DrawRectangle(xConteudo + 12, 349, 48, 16, LIGHTGRAY);
        DrawText("Narrador", xConteudo + 16, 353, 8, BLACK);
        DrawText(textosIntro[g->etapaIntro], xConteudo + 12, 380, 11, DARKGRAY);
    } else {
        DrawRectangle(xConteudo + 12, 349, 58, 16, LIGHTGRAY);
        DrawText("Marcador", xConteudo + 16, 353, 8, BLACK);
        DrawText("LUCI // REGISTRO DE IDENTIDADE!", xConteudo + 12, 377, 9, BLACK);
        DrawText("Digite seu codigo de identificacao de 3 letras:", xConteudo + 12, 393, 10, DARKGRAY);

        for (int i = 0; i < 3; i++) {
            Rectangle caixaLetra = { 245 + i * 55, 405, 45, 45 };
            Color corCaixa = (i == g->nicknamePos && g->nicknamePos < 3)
                ? (Color){225, 225, 225, 255}
                : (Color){238, 238, 238, 255};

            DrawRectangleRec(caixaLetra, corCaixa);
            const char letra[2] = { g->nickname[i], '\0' };
            DrawText(letra, (int)caixaLetra.x + 12, (int)caixaLetra.y + 5, 32, BLACK);
        }
    }

    Rectangle btnContinuar = {
        xConteudo + larguraConteudo - 125,
        (g->etapaIntro == 3) ? 475 : 457,
        125,
        30
    };

    Vector2 mouse = GetMousePosition();
    bool podeContinuar = (g->etapaIntro < 3) || (g->nicknamePos == 3);
    Color corContinuar = (CheckCollisionPointRec(mouse, btnContinuar) && podeContinuar) ? DARKGRAY : BLACK;

    DrawRectangleRec(btnContinuar, corContinuar);
    DrawText("CONTINUAR", (int)btnContinuar.x + 22, (int)btnContinuar.y + 9, 8, WHITE);
    DrawText(">", (int)btnContinuar.x + 102, (int)btnContinuar.y + 7, 12, WHITE);

    const char *instrucao1 = (g->etapaIntro == 3) ? "Digite A-Z | ENTER para confirmar" : "ENTER para continuar";
    const char *instrucao2 = (g->etapaIntro == 3) ? "BACKSPACE apaga a ultima letra"    : "BACKSPACE para voltar";

    DrawText(instrucao1, 400 - MeasureText(instrucao1, 10) / 2, 510, 10, GRAY);
    DrawText(instrucao2, 400 - MeasureText(instrucao2, 10) / 2, 528, 10, GRAY);
}
