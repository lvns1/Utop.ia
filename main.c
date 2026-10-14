#include "raylib.h"
#include "core/config.h"
#include "core/game.h"

int main(void) {
    InitWindow(LARGURA_TELA, ALTURA_TELA, "UtopIA - Game");
    SetTargetFPS(60);

    Jogo jogo;
    JogoInicializar(&jogo);

    while (!WindowShouldClose() && !jogo.fecharJogo) {
        JogoAtualizar(&jogo);
        JogoDesenhar(&jogo);
    }

    JogoFinalizar(&jogo);
    CloseWindow();

    return 0;
}
