// menu.c - Menu principal: título animado e botões JOGAR / OPÇÕES / SAIR.

#include "menu.h"
#include <math.h>

// ---------- ÁREAS DE CLIQUE DOS BOTÕES DO MENU ----------
static const Rectangle btnJogar  = {300, 270, 200, 50};
static const Rectangle btnOpcoes = {300, 340, 200, 50};
static const Rectangle btnSair   = {300, 410, 200, 50};

// Cliques nos botões: JOGAR (nova partida -> INTRO), OPÇÕES e SAIR.
void MenuAtualizar(Jogo *g) {
    Vector2 mouse = GetMousePosition();

    // JOGAR
    if (CheckCollisionPointRec(mouse, btnJogar) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        // Novo jogo: zera tudo
        JogoReiniciarPartida(g);
        g->tela = INTRO;
    }

    // OPÇÕES
    if (CheckCollisionPointRec(mouse, btnOpcoes) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        g->tela = OPCOES;
    }

    // SAIR
    if (CheckCollisionPointRec(mouse, btnSair) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        g->fecharJogo = true;
    }
}

// Fundo, título flutuando (com "rastros" roxos) e os três botões.
void MenuDesenhar(const Jogo *g) {
    ClearBackground(g->corFundoMenu);

    // ---------- ANIMAÇÃO DO TÍTULO ----------
    float amplitude = 8.0f;
    float velocidade = 3.0f;
    float y  = 100.0f + sinf(g->tempo * velocidade) * amplitude + sinf(g->tempo * 2.1f) * 4.0f;
    float y1 = 100.0f + sinf((g->tempo - 0.25f) * velocidade) * amplitude + sinf((g->tempo - 0.25f) * 2.1f) * 4.0f;
    float y2 = 100.0f + sinf((g->tempo - 0.45f) * velocidade) * amplitude + sinf((g->tempo - 0.45f) * 2.1f) * 4.0f;

    // Título original
    DrawTexture(g->imagemTitulo, 215, (int)y, WHITE);
    DrawTexture(g->imagemTitulo, 215, (int)y1, Fade(PURPLE, 0.1f));
    DrawTexture(g->imagemTitulo, 215, (int)y2, Fade(PURPLE, 0.1f));

    Vector2 mouse = GetMousePosition();
    Color corBtnJogar  = CheckCollisionPointRec(mouse, btnJogar)  ? GREEN     : DARKGRAY;
    Color corBtnOpcoes = CheckCollisionPointRec(mouse, btnOpcoes) ? LIGHTGRAY : DARKGRAY;
    Color corBtnSair   = CheckCollisionPointRec(mouse, btnSair)   ? RED       : DARKGRAY;

    // Botões
    DrawRectangleRec(btnJogar, corBtnJogar);
    DrawRectangleRec(btnOpcoes, corBtnOpcoes);
    DrawRectangleRec(btnSair, corBtnSair);

    // Textos
    DrawText("JOGAR", 365, 285, 20, WHITE);
    DrawText("OPÇÕES", 360, 355, 20, WHITE);
    DrawText("SAIR", 375, 425, 20, WHITE);
}
