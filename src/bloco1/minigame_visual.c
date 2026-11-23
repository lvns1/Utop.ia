#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <time.h>
#include "../../include/core/fonte.h"
#include "../../include/core/mapas.h"
#include "../../include/bloco1/minigame_visual.h"
#include "../../include/audio/audio.h"
#include "../../include/core/save.h"
#include "../../include/core/telemetria.h"
#include "../../include/core/texto.h"
#include "../../include/entidades/jogador.h"

const Color COR_PATCH_CURA  = { 60, 235, 140, 255 };
const Color COR_FATO_ATAQUE = { 255, 203, 60, 255 };

// A Luci na caixa de esquiva. "rastro" (pode ser NULL) são as últimas posições, do mais novo ao mais velho.
void DesenharAlmaLuci(Vector2 p, const Vector2 *rastro, int numRastro)
{
    float t = (float)GetTime();

    for (int k = numRastro - 1; k >= 0; k--) {                     // rastro de luz das asas
        float f = 1.0f - (float)(k + 1) / (float)(numRastro + 1);
        DrawCircleV(rastro[k], 2.0f + 4.0f * f, Fade(SKYBLUE, 0.28f * f));
    }

    DrawCircleV(p, RAIO_ALMA + 6.0f + 1.5f * sinf(t * 8.0f), Fade(SKYBLUE, 0.14f));
    DesenharLuciTamanho(p, ALTURA_LUCI_ALMA);
    DrawCircleV(p, 2.0f, WHITE);                                    // o núcleo: é aqui que os dados acertam
}

// Cursor das escolhas do Bloco 1 (no lugar do coração)
void DesenharCursorLuci(float cx, float cy)
{
    float t = (float)GetTime();
    DesenharLuciTamanho((Vector2){ cx + sinf(t * 5.0f) * 1.5f, cy + sinf(t * 3.0f) * 1.0f }, ALTURA_LUCI_CURSOR);
}

// PATCH DE CORREÇÃO: cápsula/chip verde com uma cruz de "restaurar" (e = escala; 1 = tamanho do minigame)
void DesenharPatchCuraEscala(Vector2 p, float e, float alfa, float t)
{
    float pulso = 0.5f + 0.5f * sinf(t * 6.0f + p.x * 0.07f);
    Color escuro = { 8, 46, 30, 255 };
    Rectangle corpo = { p.x - 9 * e, p.y - 7 * e, 18 * e, 14 * e };
    float pino = fmaxf(1.0f, 2.0f * e);

    DrawCircleV(p, (12.0f + 2.0f * pulso) * e, Fade(COR_PATCH_CURA, 0.16f * alfa));
    for (int k = -1; k <= 1; k += 2) {                              // pinos de chip nas laterais
        DrawRectangleV((Vector2){ p.x - 12 * e, p.y + (k * 3 - 1) * e }, (Vector2){ 3 * e, pino }, Fade(COR_PATCH_CURA, alfa));
        DrawRectangleV((Vector2){ p.x + 9 * e,  p.y + (k * 3 - 1) * e }, (Vector2){ 3 * e, pino }, Fade(COR_PATCH_CURA, alfa));
    }
    DrawRectangleRounded(corpo, 0.45f, 4, Fade(escuro, alfa));
    DrawRectangleRoundedLines(corpo, 0.45f, 4, Fade(COR_PATCH_CURA, alfa));
    DrawRectangleV((Vector2){ p.x - 1.5f * e, p.y - 5.5f * e }, (Vector2){ 3 * e, 11 * e }, Fade(WHITE, alfa));
    DrawRectangleV((Vector2){ p.x - 5.5f * e, p.y - 1.5f * e }, (Vector2){ 11 * e, 3 * e }, Fade(WHITE, alfa));
}

static void DesenharPatchCura(Vector2 p, float alfa, float t)
{
    DesenharPatchCuraEscala(p, 1.0f, alfa, t);
}

// NÚCLEO DE INTEGRIDADE (vida máxima): hexágono verde com um núcleo de luz (como o da Luci) e uma cruz
void DesenharNucleoIntegridade(Vector2 c, float e, float t)
{
    float pulso = 0.5f + 0.5f * sinf(t * 4.0f);
    DrawCircleV(c, 13 * e, Fade(COR_PATCH_CURA, 0.12f + 0.06f * pulso));
    DrawPoly(c, 6, 11 * e, 30, (Color){ 8, 46, 30, 255 });
    DrawPolyLinesEx(c, 6, 11 * e, 30, fmaxf(1.0f, 2.0f * e), COR_PATCH_CURA);
    DrawCircleV(c, 6.0f * e, Fade(SKYBLUE, 0.55f + 0.25f * pulso));
    DrawCircleV(c, 3.5f * e, (Color){ 230, 250, 255, 255 });
    DrawRectangleV((Vector2){ c.x - 1 * e, c.y - 4.5f * e }, (Vector2){ 2 * e, 9 * e }, (Color){ 8, 46, 30, 255 });
    DrawRectangleV((Vector2){ c.x - 4.5f * e, c.y - 1 * e }, (Vector2){ 9 * e, 2 * e }, (Color){ 8, 46, 30, 255 });
}

// FATO VERIFICADO: losango dourado com um "certo", raios de luz girando em volta
static void DesenharFatoAtaque(Vector2 p, float alfa, float t)
{
    float giro = t * 120.0f + p.y * 2.0f;

    DrawCircleV(p, 13.0f, Fade(COR_FATO_ATAQUE, 0.14f * alfa));
    for (int k = 0; k < 4; k++) {
        float a = (giro + (float)k * 90.0f + 45.0f) * DEG2RAD;
        Vector2 d = { cosf(a), sinf(a) };
        DrawLineEx(Vector2Add(p, Vector2Scale(d, 11.0f)), Vector2Add(p, Vector2Scale(d, 15.0f)), 2.0f, Fade(COR_FATO_ATAQUE, 0.75f * alfa));
    }
    DrawPoly(p, 4, 10.0f, 0.0f, Fade(COR_FATO_ATAQUE, alfa));
    DrawPoly(p, 4, 7.0f, 0.0f, Fade((Color){ 110, 64, 0, 255 }, alfa));
    DrawLineEx((Vector2){ p.x - 4, p.y }, (Vector2){ p.x - 1, p.y + 3 }, 2.0f, Fade(WHITE, alfa));
    DrawLineEx((Vector2){ p.x - 1, p.y + 3 }, (Vector2){ p.x + 4, p.y - 3 }, 2.0f, Fade(WHITE, alfa));
}

// O item que se pega no minigame: no ATAQUE é o fato verificado, na DEFESA é o patch de correção.
// falsaAparecendo: a falsificação se mostra (falha em magenta com um "?")
void DesenharItemB1(Vector2 p, bool ataque, float alfa, bool falsaAparecendo)
{
    float t = (float)GetTime();

    if (falsaAparecendo)
        p.x += (float)GetRandomValue(-1, 1);

    if (ataque) DesenharFatoAtaque(p, alfa, t);
    else        DesenharPatchCura(p, alfa, t);

    if (falsaAparecendo) {
        DrawCircleV(p, 12.0f, Fade(MAGENTA, 0.45f * alfa));
        DrawRectangle((int)p.x - 13, (int)p.y - 4, 26, 2, Fade(MAGENTA, 0.8f * alfa));
        DrawRectangle((int)p.x - 10, (int)p.y + 3, 20, 2, Fade(MAGENTA, 0.8f * alfa));
        DrawText("?", (int)p.x - MeasureText("?", 16) / 2, (int)p.y - 8, 16, Fade(WHITE, alfa));
    }
}

// DADO CORROMPIDO (machuca): o símbolo da alucinação num bloquinho com falha de cor (vermelho e ciano deslocados)
void DesenharDadoCorrompido(Vector2 p, char glifo, Color cor, int indice)
{
    float t = (float)GetTime();
    char g[2] = { glifo, '\0' };
    bool falha = fmodf(t * 2.3f + (float)indice * 0.61f, 1.0f) < 0.12f;    // de vez em quando a imagem "pula"
    float dx = falha ? (float)((indice % 3) - 1) * 2.0f : 0.0f;
    Rectangle r = { p.x - 8 + dx, p.y - 8, 16, 16 };
    int tam = 14;
    int gx = (int)(p.x + dx) - MeasureText(g, tam) / 2, gy = (int)p.y - tam / 2;

    DrawRectangleRec(r, Fade((Color){ 22, 8, 32, 255 }, 0.92f));
    DrawRectangleLinesEx(r, 1, Fade(cor, 0.95f));
    DrawText(g, gx - 1, gy, tam, Fade(RED, 0.65f));
    DrawText(g, gx + 1, gy, tam, Fade(SKYBLUE, 0.65f));
    DrawText(g, gx, gy, tam, WHITE);
    if (falha)
        DrawRectangle((int)r.x - 2, (int)p.y - 1, 20, 2, Fade(cor, 0.8f));
}

// Luci voando em volta do jogador (depois da primeira conversa).
// Ela dá voltas numa elipse na altura do peito: quando está "lá atrás"
// (parte de cima da elipse) fica menor e é desenhada antes do personagem.

bool LuciAtrasDoJogador(float angulo)
{
    return sinf(angulo) < 0.0f;
}

// Ícone de emoção sobre a cabeça da Luci
void DesenharEmocaoLuci(Vector2 p, float altura)
{
    if (tempoEmocaoLuci <= 0.0f || emocaoLuci == EMOCAO_NENHUMA)
        return;

    float pulo = sinf((float)GetTime() * 8.0f) * 2.0f;
    Vector2 c = { p.x, p.y - altura / 2 - 12 + pulo };

    DrawCircleV(c, 8, Fade((Color){ 20, 16, 34, 255 }, 0.85f));

    if (emocaoLuci == EMOCAO_ALEGRE)
        DesenharCoracao(c.x, c.y, 1, (Color){ 255, 120, 170, 255 });
    else if (emocaoLuci == EMOCAO_DUVIDA)
        DrawText("?", (int)c.x - MeasureText("?", 14) / 2, (int)c.y - 7, 14, WHITE);
    else
        DrawText("!", (int)c.x - MeasureText("!", 14) / 2, (int)c.y - 7, 14, GOLD);
}
