// monitor.c - Desenho dos monitores nos 4 estados (com sprite ou com formas simples).

#include "monitor.h"
#include "../utils/effects.h"
#include <math.h>

// ---------- Desenho dos monitores (4 estados) ----------
// Quando a arte final ficar pronta, é só colocar os PNGs nos caminhos
// SPRITE_MONITOR_* lá em cima: o jogo passa a usá-los automaticamente.
void DesenharMonitor(int indice, Rectangle a, EstadoMonitor estado, float tempo, const Texture2D sprites[4]) {
    // Com sprite: desenha a imagem ocupando a tela + o pé do monitor
    if (sprites[estado].id > 0) {
        Texture2D t = sprites[estado];
        DrawTexturePro(
            t,
            (Rectangle){ 0, 0, (float)t.width, (float)t.height },
            (Rectangle){ a.x, a.y, a.width, a.height + 18 },
            (Vector2){ 0, 0 },
            0.0f,
            WHITE
        );
        return;
    }

    // Sem sprite: formas simples
    int cx = (int)(a.x + a.width / 2);
    float pulso = 0.5f + 0.5f * sinf(tempo * 5.0f);

    Color corBorda = GRAY;
    Color corLed = DARKGRAY;

    // Pé do monitor
    DrawRectangle(cx - 6, (int)(a.y + a.height), 12, 12, (Color){45, 40, 62, 255});
    DrawRectangle(cx - 24, (int)(a.y + a.height) + 12, 48, 6, (Color){45, 40, 62, 255});

    // Carcaça
    DrawRectangleRec(a, (Color){30, 28, 44, 255});

    Rectangle visor = { a.x + 7, a.y + 7, a.width - 14, a.height - 18 };
    int vx = (int)visor.x;
    int vy = (int)visor.y;
    int vw = (int)visor.width;
    int vh = (int)visor.height;
    int cy = vy + vh / 2;

    switch (estado) {
        // ---------- NEUTRO: desligado, esperando o jogador ----------
        case MON_NEUTRO: {
            DrawRectangleRec(visor, (Color){10, 12, 20, 255});
            const char *num = TextFormat("%d", indice + 1);
            DrawText(num, cx - MeasureText(num, 20) / 2, cy - 10, 20, (Color){55, 60, 78, 255});

            if (((int)(tempo * 1.5f)) % 2 == 0)
                DrawText("_", vx + 6, vy + vh - 14, 10, (Color){80, 90, 110, 255});

            corBorda = GRAY;
            corLed = (Color){70, 70, 70, 255};
        } break;

        // ---------- ATIVO: durante a interação / quiz ----------
        case MON_ATIVO: {
            DrawRectangleRec(visor, (Color){12, 40, 75, 255});

            // Linhas de varredura
            for (int yy = 0; yy < vh; yy += 4)
                DrawLine(vx, vy + yy, vx + vw, vy + yy, Fade(BLACK, 0.25f));

            float varre = fmodf(tempo * 40.0f, (float)vh);
            DrawRectangle(vx, vy + (int)varre, vw, 3, Fade(SKYBLUE, 0.35f));
            DrawText("?", cx - MeasureText("?", 28) / 2, cy - 14, 28, Fade(WHITE, 0.6f + 0.4f * pulso));

            corBorda = Fade(GOLD, 0.6f + 0.4f * pulso);
            corLed = (pulso > 0.5f) ? YELLOW : ORANGE;
        } break;

        // ---------- ACERTOU ----------
        case MON_ACERTOU: {
            DrawRectangleRec(visor, (Color){8, 48, 22, 255});
            DrawLineEx((Vector2){ (float)cx - 16, (float)cy }, (Vector2){ (float)cx - 4, (float)cy + 12 }, 5, LIME);
            DrawLineEx((Vector2){ (float)cx - 4, (float)cy + 12 }, (Vector2){ (float)cx + 18, (float)cy - 14 }, 5, LIME);

            corBorda = GREEN;
            corLed = LIME;
        } break;

        // ---------- ERROU: chiado de alucinação ----------
        case MON_ERROU: {
            DrawRectangleRec(visor, (Color){55, 8, 14, 255});
            unsigned int quadro = (unsigned int)(tempo * 12.0f) + (unsigned int)indice * 97u;

            for (int k = 0; k < 6; k++) {
                unsigned int r = Ruido(quadro * 7u + (unsigned int)k);
                int lw = 10 + (int)((r >> 8) % (unsigned int)(vw - 10));
                int lx = vx + (int)((r >> 16) % (unsigned int)(vw - lw + 1));
                int ly = vy + (int)(r % (unsigned int)vh);

                DrawRectangle(lx, ly, lw, 2, Fade(RED, 0.5f));
            }

            DrawLineEx((Vector2){ (float)cx - 12, (float)cy - 12 }, (Vector2){ (float)cx + 12, (float)cy + 12 }, 4, RED);
            DrawLineEx((Vector2){ (float)cx + 12, (float)cy - 12 }, (Vector2){ (float)cx - 12, (float)cy + 12 }, 4, RED);

            corBorda = RED;
            corLed = RED;
        } break;

        default: break;
    }

    DrawRectangleLinesEx(a, 3, corBorda);

    // LED de energia
    DrawCircle((int)(a.x + a.width - 10), (int)(a.y + a.height - 6), 2, corLed);
}
