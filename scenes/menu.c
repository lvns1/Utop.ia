#include "menu.h"
#include <math.h>

static const Rectangle btnJogar  = {300, 270, 200, 50};
static const Rectangle btnOpcoes = {300, 340, 200, 50};
static const Rectangle btnSair   = {300, 410, 200, 50};

void MenuAtualizar(Jogo *g) {
    Vector2 mouse = GetMousePosition();

    if (CheckCollisionPointRec(mouse, btnJogar) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        JogoReiniciarPartida(g);
        g->tela = INTRO;
    }
    if (CheckCollisionPointRec(mouse, btnOpcoes) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        g->tela = OPCOES;
    }
    if (CheckCollisionPointRec(mouse, btnSair) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        g->fecharJogo = true;
    }
}

void MenuDesenhar(const Jogo *g) {
    ClearBackground(g->corFundoMenu);

    float amplitude = 8.0f;
    float velocidade = 3.0f;
    float y  = 100.0f + sinf(g->tempo * velocidade) * amplitude + sinf(g->tempo * 2.1f) * 4.0f;
    float y1 = 100.0f + sinf((g->tempo - 0.25f) * velocidade) * amplitude + sinf((g->tempo - 0.25f) * 2.1f) * 4.0f;
    float y2 = 100.0f + sinf((g->tempo - 0.45f) * velocidade) * amplitude + sinf((g->tempo - 0.45f) * 2.1f) * 4.0f;

    DrawTexture(g->imagemTitulo, 215, (int)y, WHITE);
    DrawTexture(g->imagemTitulo, 215, (int)y1, Fade(PURPLE, 0.1f));
    DrawTexture(g->imagemTitulo, 215, (int)y2, Fade(PURPLE, 0.1f));

    Vector2 mouse = GetMousePosition();
    Color corBtnJogar  = CheckCollisionPointRec(mouse, btnJogar)  ? GREEN     : DARKGRAY;
    Color corBtnOpcoes = CheckCollisionPointRec(mouse, btnOpcoes) ? LIGHTGRAY : DARKGRAY;
    Color corBtnSair   = CheckCollisionPointRec(mouse, btnSair)   ? RED       : DARKGRAY;

    DrawRectangleRec(btnJogar, corBtnJogar);
    DrawRectangleRec(btnOpcoes, corBtnOpcoes);
    DrawRectangleRec(btnSair, corBtnSair);

    DrawText("JOGAR", 365, 285, 20, WHITE);
    DrawText("OPÇÕES", 360, 355, 20, WHITE);
    DrawText("SAIR", 375, 425, 20, WHITE);
}
