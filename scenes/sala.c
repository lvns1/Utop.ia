// sala.c - Sala inicial: conversa com a Luci e porta de cima para o corredor.
#include "raylib.h"
#include "raymath.h"
#include "sala.h"
#include "../entities/player.h"
#include "../entities/luci.h"
#include "../systems/collision.h"
#include "../data/dialogues.h"

// Sala inicial: conversa com a Luci (tecla E); depois dela a porta de cima leva ao corredor.
void SalaAtualizar(Jogo *g, float dt) {
    g->companionAngulo += dt * 2.5f;
    g->pertoDaLuci = false;

    // ---------- CONVERSA EM ANDAMENTO (o personagem fica parado) ----------
    if (g->dlg.estado != DLG_NENHUM) {
        ResultadoDialogo r = DialogoAtualizar(&g->dlg);

        // Terminou a conversa com a Luci: ela passa a acompanhar o jogador
        if (r == RES_FIM && g->dlg.objeto == OBJ_LUCI)
            g->luciAcompanhando = true;
    } else {
        // ---------- MOVIMENTAÇÃO DO PERSONAGEM ----------
        Vector2 posAnterior = g->playerPos;
        g->direcaoAtual = MoverJogador(&g->playerPos, g->direcaoAtual);

        bool andando =
        Vector2Distance(posAnterior, g->playerPos) > 0.1f;

        AtualizarAnimacao(
            &g->animJogador,
            andando,
            dt
        );

        // Colisão com a Luci (não deixa atravessar)
        if (!g->luciAcompanhando && CheckCollisionRecs(HitboxJogador(g->playerPos), g->luciHitbox))
            g->playerPos = posAnterior;

        // ---------- LIMITES DO MAPA ----------
        if (g->playerPos.x < 80)  g->playerPos.x = 80;
        if (g->playerPos.x > 720) g->playerPos.x = 720;
        if (g->playerPos.y < 80)  g->playerPos.y = 80;
        if (g->playerPos.y > 520) g->playerPos.y = 520;

        // ---------- INTERAÇÃO (tecla E) ----------

        // pequeno retângulo na frente do jogador, na direção que ele está olhando
        Rectangle interacaoBox = CaixaInteracaoJogador(g->playerPos, g->direcaoAtual, g->interacaoAlcance);
        g->pertoDaLuci = !g->luciAcompanhando && CheckCollisionRecs(interacaoBox, g->luciHitbox);
        bool pertoDaPorta = CheckCollisionRecs(AreaJogador(g->playerPos), ZONA_PORTA_SALA);

        if (IsKeyPressed(KEY_E)) {
            if (g->pertoDaLuci) {
                DialogoIniciar(&g->dlg, &dialogoLuciSala, OBJ_LUCI);
                g->pertoDaLuci = false;
            } else if (pertoDaPorta) {
                // A porta de cima leva ao corredor (depois de falar com a Luci)
                if (g->luciAcompanhando)
                    IniciarTransicao(&g->transicao, CORREDOR, DURACAO_FADE);
                else
                    DialogoIniciar(&g->dlg, &dialogoPortaSalaTrancada, OBJ_NENHUM);
            }
        }
    }
}

// Desenha a sala: mapa, porta, Luci (NPC ou companion), jogador, HUD e a conversa por cima.
void SalaDesenhar(const Jogo *g) {
    ClearBackground((Color){8, 6, 18, 255});

    // ---------- MAPA SIMPLES ----------
    Rectangle mapa = {50, 50, 700, 500};
    DrawRectangleRec(mapa, (Color){18, 15, 30, 255});  // chão
    DrawRectangleLinesEx(mapa, 4, Fade(PURPLE, 0.55f));  // bordas

    // ---------- PORTA (parede de cima) ----------
    bool salaLiberada = g->luciAcompanhando;
    DrawRectangleRec(PORTA_SALA, (Color){35, 25, 55, 255});
    DrawRectangleLinesEx(PORTA_SALA, 3, salaLiberada ? PURPLE : MAROON);

    // Maçaneta
    DrawCircle((int)(PORTA_SALA.x + PORTA_SALA.width - 12), (int)(PORTA_SALA.y + PORTA_SALA.height / 2), 4, GOLD);

    DrawText(
        "CORREDOR",
        (int)(PORTA_SALA.x + PORTA_SALA.width / 2) - MeasureText("CORREDOR", 10) / 2,
        (int)PORTA_SALA.y - 14, 10, salaLiberada ? LIGHTGRAY : RED
    );

    bool livre = (g->dlg.estado == DLG_NENHUM && !g->transicao.ativa);
    if (livre && CheckCollisionRecs(AreaJogador(g->playerPos), ZONA_PORTA_SALA)) {
        DrawText(
            "PRESSIONE E",
            (int)(PORTA_SALA.x + PORTA_SALA.width) + 10,
            (int)PORTA_SALA.y + 12, 12, GOLD
        );
    }

    // ---------- LUCI (NPC enquanto não conversou) ----------
    if (!g->luciAcompanhando) {
        DesenharLuciNPC(g->luciHitbox);
    }

    // ---------- PERSONAGEM ----------
    DesenharJogador(
    g->spritesJogador,
    g->playerPos,
    g->direcaoAtual,
    g->animJogador.quadro
    );
    if (g->luciAcompanhando)
        DesenharCompanion(g->playerPos, g->companionAngulo);

    // ---------- INSTRUÇÕES E HUD ----------
    DrawText("W A S D  -  MOVIMENTAR", 65, 65, 12, LIGHTGRAY);
    DrawText("E  -  INTERAGIR", 65, 82, 12, LIGHTGRAY);
    DrawText(g->nickname, 700 - MeasureText(g->nickname, 14), 65, 14, WHITE);

    const char *textoPontos = TextFormat("PONTOS: %d", g->progresso.pontos);
    DrawText(textoPontos, 700 - MeasureText(textoPontos, 12), 84, 12, GOLD);

    if (livre && g->pertoDaLuci) {
        // aviso de interação
        const char *aviso = "Aperte E para falar com a Luci";
        DrawText(aviso, LARGURA_TELA / 2 - MeasureText(aviso, 20) / 2, ALTURA_TELA - 40, 20, RAYWHITE);
    }

    // Conversa por cima de tudo
    DialogoDesenhar(&g->dlg);
}
