#include "sala.h"
#include "../entities/player.h"
#include "../entities/luci.h"
#include "../systems/collision.h"
#include "../data/dialogues.h"

void SalaAtualizar(Jogo *g, float dt) {
    g->companionAngulo += dt * 2.5f;
    g->pertoDaLuci = false;

    if (g->dlg.estado != DLG_NENHUM) {
        ResultadoDialogo r = DialogoAtualizar(&g->dlg);
        if (r == RES_FIM && g->dlg.objeto == OBJ_LUCI)
            g->luciAcompanhando = true;
    } else {
        Vector2 posAnterior = g->playerPos;
        g->direcaoAtual = MoverJogador(&g->playerPos, g->direcaoAtual);

        if (!g->luciAcompanhando && CheckCollisionRecs(HitboxJogador(g->playerPos), g->luciHitbox))
            g->playerPos = posAnterior;

        if (g->playerPos.x < 80)  g->playerPos.x = 80;
        if (g->playerPos.x > 720) g->playerPos.x = 720;
        if (g->playerPos.y < 80)  g->playerPos.y = 80;
        if (g->playerPos.y > 520) g->playerPos.y = 520;

        Rectangle interacaoBox = CaixaInteracaoJogador(g->playerPos, g->direcaoAtual, g->interacaoAlcance);
        g->pertoDaLuci = !g->luciAcompanhando && CheckCollisionRecs(interacaoBox, g->luciHitbox);
        bool pertoDaPorta = CheckCollisionRecs(AreaJogador(g->playerPos), ZONA_PORTA_SALA);

        if (IsKeyPressed(KEY_E)) {
            if (g->pertoDaLuci) {
                DialogoIniciar(&g->dlg, &dialogoLuciSala, OBJ_LUCI);
                g->pertoDaLuci = false;
            } else if (pertoDaPorta) {
                if (g->luciAcompanhando)
                    IniciarTransicao(&g->transicao, CORREDOR, DURACAO_FADE);
                else
                    DialogoIniciar(&g->dlg, &dialogoPortaSalaTrancada, OBJ_NENHUM);
            }
        }
    }
}

void SalaDesenhar(const Jogo *g) {
    ClearBackground((Color){8, 6, 18, 255});

    Rectangle mapa = {50, 50, 700, 500};
    DrawRectangleRec(mapa, (Color){18, 15, 30, 255});
    DrawRectangleLinesEx(mapa, 4, Fade(PURPLE, 0.55f));

    bool salaLiberada = g->luciAcompanhando;
    DrawRectangleRec(PORTA_SALA, (Color){35, 25, 55, 255});
    DrawRectangleLinesEx(PORTA_SALA, 3, salaLiberada ? PURPLE : MAROON);
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

    if (!g->luciAcompanhando) {
        DesenharLuciNPC(g->luciHitbox);
    }

    DesenharJogador(g->texFrente, g->texCostas, g->texLado, g->playerPos, g->direcaoAtual);

    if (g->luciAcompanhando)
        DesenharCompanion(g->playerPos, g->companionAngulo);

    DrawText("W A S D  -  MOVIMENTAR", 65, 65, 12, LIGHTGRAY);
    DrawText("E  -  INTERAGIR", 65, 82, 12, LIGHTGRAY);
    DrawText(g->nickname, 700 - MeasureText(g->nickname, 14), 65, 14, WHITE);

    const char *textoPontos = TextFormat("PONTOS: %d", g->progresso.pontos);
    DrawText(textoPontos, 700 - MeasureText(textoPontos, 12), 84, 12, GOLD);

    if (livre && g->pertoDaLuci) {
        const char *aviso = "Aperte E para falar com a Luci";
        DrawText(aviso, LARGURA_TELA / 2 - MeasureText(aviso, 20) / 2, ALTURA_TELA - 40, 20, RAYWHITE);
    }

    DialogoDesenhar(&g->dlg);
}
