// =============================================================
// UtopIA - BLOCO 1
//   Sala inicial (Luci) -> porta de cima -> Corredor dos Monitores
//   Cada monitor tem um quiz (conteúdo do "Quizz - missão 1.pdf").
//   Tempo esgotado -> COMBATE estilo Undertale
//   Resposta errada -> MINIGAME de esquiva estilo Undertale
//   Resposta certa  -> ganha pontos
//
// main.c: só abre a janela e roda o game loop.
// Toda a lógica fica em core/game.c e nas cenas (scenes/).
// =============================================================

#include "raylib.h"
#include "core/config.h"
#include "core/game.h"

int main(void) {
    // 1. INICIALIZAÇÃO
    InitWindow(LARGURA_TELA, ALTURA_TELA, "UtopIA - Game");
    SetTargetFPS(60);

    Jogo jogo;
    JogoInicializar(&jogo);

    // 2. GAME LOOP
    while (!WindowShouldClose() && !jogo.fecharJogo) {
        JogoAtualizar(&jogo);
        JogoDesenhar(&jogo);
    }

    // 3. FINALIZAÇÃO
    // (as texturas precisam ser liberadas ANTES de fechar a janela)
    JogoFinalizar(&jogo);
    CloseWindow();

    return 0;
}
