#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <time.h>
#include "../../include/core/fonte.h"
#include "../../include/core/mapas.h"
#include "../../include/audio/audio.h"
#include "../../include/bloco1/batalha.h"
#include "../../include/bloco1/batalha_monitores.h"
#include "../../include/bloco1/corredor.h"
#include "../../include/bloco1/minigame_visual.h"
#include "../../include/bloco1/objetivo.h"
#include "../../include/core/save.h"
#include "../../include/core/telemetria.h"
#include "../../include/core/texto.h"
#include "../../include/dados/conteudo_bloco1.h"
#include "../../include/entidades/jogador.h"
#include "../../include/entidades/luci_companion.h"
#include "../../include/telas/intro.h"
#include "../../include/telas/pause.h"
#include "../../include/telas/ranking.h"
#include "../../include/telas/resolucao.h"
#include "../../include/bloco2/biblioteca.h"

// ---------- Desenho dos inimigos da expansão ----------
// Cada um tem uma silhueta própria (não depende só da cor) e um aviso antes de atacar.

// Spammer: uma janela pop-up com barra de título, "x" e o selo de notificação
static void DesenharSpammer(const Inimigo *e, float escala)
{
    float s = escala;
    Vector2 p = { e->pos.x, e->pos.y + sinf(e->anim * 9.0f) * 1.5f };

    if (((int)(e->anim * 7.0f + e->lado * 3.0f)) % 9 == 0)
        p.x += 2.0f;                                        // tremidinha de glitch

    Rectangle janela = { p.x - 11 * s, p.y - 8 * s, 22 * s, 16 * s };
    DrawRectangleRec(janela, (Color){ 45, 25, 30, 255 });
    DrawRectangle((int)janela.x, (int)janela.y, (int)janela.width, (int)(4 * s), (Color){ 255, 120, 60, 255 });
    DrawRectangleLinesEx(janela, 1, (Color){ 255, 170, 120, 255 });
    DrawLine((int)(p.x + 6 * s), (int)(p.y - 8 * s), (int)(p.x + 9 * s), (int)(p.y - 5 * s), WHITE);    // "x"
    DrawLine((int)(p.x + 9 * s), (int)(p.y - 8 * s), (int)(p.x + 6 * s), (int)(p.y - 5 * s), WHITE);
    DrawRectangle((int)(p.x - 7 * s), (int)(p.y - 1 * s), (int)(12 * s), (int)(2 * s), Fade(WHITE, 0.7f));
    DrawRectangle((int)(p.x - 7 * s), (int)(p.y + 3 * s), (int)(8 * s), (int)(2 * s), Fade(WHITE, 0.7f));

    // Selo de notificação ("!")
    DrawCircle((int)(p.x - 11 * s), (int)(p.y - 8 * s), 5 * s, RED);
    DrawText("!", (int)(p.x - 12 * s), (int)(p.y - 13 * s), (int)(10 * s), WHITE);
}

// Fragmento de spam: um envelopinho
static void DesenharSpam(const Inimigo *e)
{
    Vector2 p = e->pos;
    float alfa = fminf(1.0f, e->vida / 0.6f);                // some piscando no fim

    DrawRectangle((int)p.x - 6, (int)p.y - 4, 12, 8, Fade((Color){ 255, 220, 180, 255 }, alfa));
    DrawLine((int)p.x - 6, (int)p.y - 4, (int)p.x, (int)p.y + 1, Fade(RED, alfa));
    DrawLine((int)p.x + 6, (int)p.y - 4, (int)p.x, (int)p.y + 1, Fade(RED, alfa));
}

// Phisher: camuflado, um arquivo "GRÁTIS" (com um anzol que brilha de vez em quando);
// revelado, o arquivo escurece, abre os olhos e mostra o anzol. A cópia falsa não tem
// sombra e pisca (é assim que se descobre o verdadeiro).
static void DesenharPhisher(const Inimigo *e, float escala)
{
    bool isca = (e->tipo == INIMIGO_ISCA);
    bool revelado = (e->estado != 0);
    float s = escala;
    Vector2 p = { e->pos.x, e->pos.y + sinf(e->anim * 2.0f) * 2.0f };

    if (isca && ((int)(e->anim * 12.0f)) % 5 == 0)
        return;                                             // a cópia falha por um instante

    if (!isca)
        DrawEllipse((int)e->pos.x, (int)e->pos.y + 16, 12 * s, 4 * s, Fade(BLACK, 0.45f));

    if (e->estado == 1)                                      // mirando: treme
        p.x += (float)GetRandomValue(-2, 2);

    Color papel = revelado ? (Color){ 70, 55, 80, 255 } : (Color){ 235, 228, 205, 255 };
    Rectangle r = { p.x - 10 * s, p.y - 13 * s, 20 * s, 26 * s };

    DrawRectangleRec(r, papel);
    DesenharTrianguloB2((Vector2){ r.x + r.width - 7 * s, r.y }, (Vector2){ r.x + r.width, r.y + 7 * s },
                        (Vector2){ r.x + r.width - 7 * s, r.y + 7 * s }, revelado ? GOLD : (Color){ 190, 180, 150, 255 });
    DrawRectangleLinesEx(r, 1, revelado ? GOLD : (Color){ 150, 140, 120, 255 });

    if (!revelado) {
        // "Prêmio" para chamar atenção, e o anzol escondido aparecendo de vez em quando
        DrawText("$", (int)p.x - MeasureText("$", 14) / 2, (int)p.y - LinhaTexto(8), 14, (Color){ 40, 140, 60, 255 });
        if (fmodf(e->anim, 2.6f) < 0.25f)
            DrawCircleLines((int)(r.x + r.width), (int)(r.y + r.height), 3, GOLD);
        const char *rotulo = "GRÁTIS";
        DrawText(rotulo, (int)p.x - MeasureText(rotulo, 8) / 2, (int)(r.y + r.height) + 3, 8, GOLD);
        return;
    }

    // Revelado: olhos e o anzol em cima
    DrawRectangle((int)(p.x - 6 * s), (int)(p.y - 4 * s), (int)(4 * s), (int)(2 * s), RED);
    DrawRectangle((int)(p.x + 2 * s), (int)(p.y - 4 * s), (int)(4 * s), (int)(2 * s), RED);
    DrawLine((int)p.x, (int)(r.y - 12 * s), (int)p.x, (int)r.y, LIGHTGRAY);
    DrawRing((Vector2){ p.x + 4 * s, r.y - 12 * s }, 3 * s, 4.5f * s, 180.0f, 360.0f, 8, LIGHTGRAY);

    if (e->estado == 1)
        DrawText("!", (int)p.x - 2, (int)(r.y - 30), 16, GOLD);     // o corredor do bote é desenhado no chão (DesenharAvisoInimigo)
}

// Firewall: robô octogonal com cadeado; escudo hexagonal na frente, saída de ar
// (ponto fraco) atrás. Barreira de energia: aviso tracejado, depois o muro.
static void DesenharFirewall(const Inimigo *e, float escala, float t)
{
    float s = escala;
    Vector2 p = e->pos;
    float ang = atan2f(e->frente.y, e->frente.x) * RAD2DEG;
    Vector2 tras = Vector2Subtract(p, Vector2Scale(e->frente, e->raio * 0.75f * s));
    Vector2 lado = { -e->frente.y, e->frente.x };

    DrawEllipse((int)p.x, (int)p.y + 20, 20 * s, 5 * s, Fade(BLACK, 0.4f));
    DrawPoly(p, 8, 20 * s, 22.5f, (Color){ 30, 60, 45, 255 });
    DrawPolyLinesEx(p, 8, 20 * s, 22.5f, 2, (Color){ 120, 220, 150, 255 });

    // Cadeado
    DrawRing((Vector2){ p.x, p.y - 3 * s }, 4 * s, 6 * s, 180.0f, 360.0f, 10, LIGHTGRAY);
    DrawRectangle((int)(p.x - 6 * s), (int)(p.y - 3 * s), (int)(12 * s), (int)(9 * s), GOLD);
    DrawRectangle((int)(p.x - 1 * s), (int)p.y, (int)(2 * s), (int)(3 * s), (Color){ 60, 40, 0, 255 });

    // Saída de ar atrás (ponto fraco), pulsando
    for (int k = -1; k <= 1; k++) {
        Vector2 a = Vector2Add(tras, Vector2Scale(lado, k * 5.0f * s));
        DrawLineEx(Vector2Add(a, Vector2Scale(e->frente, -3 * s)), Vector2Add(a, Vector2Scale(e->frente, 3 * s)), 2,
                   Fade(ORANGE, 0.5f + 0.4f * sinf(t * 6.0f)));
    }

    // Escudo frontal (com as divisões hexagonais)
    DrawRing(p, e->raio + 4, e->raio + 9, ang - FIREWALL_ANGULO_FRENTE, ang + FIREWALL_ANGULO_FRENTE, 16, Fade((Color){ 0, 228, 48, 255 }, 0.85f));
    for (int k = -2; k <= 2; k++) {
        float a = (ang + k * FIREWALL_ANGULO_FRENTE / 2.5f) * DEG2RAD;
        DrawLineEx((Vector2){ p.x + cosf(a) * (e->raio + 4), p.y + sinf(a) * (e->raio + 4) },
                   (Vector2){ p.x + cosf(a) * (e->raio + 9), p.y + sinf(a) * (e->raio + 9) }, 1, (Color){ 20, 60, 30, 255 });
    }

    // Barreira de energia
    if (e->estado == 1 || e->estado == 2) {
        Vector2 a, c;
        RogueBarreiraFirewall(e, &a, &c);
        Color verde = { 0, 228, 48, 255 };
        if (e->estado == 1) {
            float q = Clamp(1.0f - e->timer / FIREWALL_AVISO_BARREIRA, 0.0f, 1.0f);
            Vector2 meio = Vector2Lerp(a, c, 0.5f);
            for (int k = 0; k < 8; k += 2)
                DrawLineEx(Vector2Lerp(a, c, k / 8.0f), Vector2Lerp(a, c, (k + 1) / 8.0f), 2, Fade(verde, 0.35f + 0.35f * sinf(t * 20.0f)));
            DrawLineEx(Vector2Lerp(meio, a, q), Vector2Lerp(meio, c, q), 3.0f + 4.0f * q, Fade(verde, 0.20f + 0.35f * q));   // a parede se formando
            DrawCircleV(a, 3.0f, Fade(verde, 0.8f));
            DrawCircleV(c, 3.0f, Fade(verde, 0.8f));
        } else {
            float falha = (e->timer < 0.45f && sinf(t * 35.0f) < 0.0f) ? 0.35f : 1.0f;       // perto de cair, a barreira falha
            DrawLineEx(a, c, 9.0f, Fade(verde, 0.22f * falha));
            DrawLineEx(a, c, 5.0f, Fade(verde, 0.35f * falha));
            DrawLineEx(a, c, 2.0f, Fade((Color){ 150, 255, 170, 255 }, falha));
            for (int k = 1; k < 6; k++)
                DrawPolyLines(Vector2Lerp(a, c, k / 6.0f), 6, 5, t * 60.0f, Fade((Color){ 150, 255, 170, 255 }, 0.8f * falha));
            float u = fmodf(t * 1.6f, 1.0f);                                                     // pulso correndo pela parede
            DrawCircleV(Vector2Lerp(a, c, u), 3.0f, Fade(WHITE, 0.7f * falha));
        }
    }
}

// Overclock: engrenagem com núcleo superaquecido. Carregando, o núcleo fica branco
// e a linha da investida cresce; superaquecido, solta fumaça e a engrenagem para.
static void DesenharOverclock(const Inimigo *e, float escala, float t)
{
    float s = escala;
    Vector2 p = e->pos;
    float k = (e->estado == 1) ? Clamp(e->carga / OVERCLOCK_CARGA_MAX, 0.0f, 1.0f) : 0.0f;

    if (e->estado == 1)
        p.x += (float)GetRandomValue(-1, 1) * (1.0f + 2.0f * k);

    // Investida: rastro de imagens
    if (e->estado == 2)
        for (int r = 1; r <= 3; r++)
            DrawCircleV(Vector2Subtract(p, Vector2Scale(Vector2Normalize(e->vel), r * 14.0f)), 13 * s, Fade(ORANGE, 0.25f / r));

    float giro = (e->estado == 3) ? 0.0f : e->anim * (e->estado == 1 ? 12.0f + 20.0f * k : 4.0f);
    Color metal = (e->estado == 3) ? (Color){ 90, 90, 110, 255 } : (Color){ 150, 110, 70, 255 };

    for (int d = 0; d < 8; d++) {
        float a = giro + d * (PI / 4.0f);
        DrawRectanglePro((Rectangle){ p.x + cosf(a) * 15 * s, p.y + sinf(a) * 15 * s, 7 * s, 7 * s }, (Vector2){ 3.5f * s, 3.5f * s }, a * RAD2DEG, metal);
    }
    DrawCircleV(p, 14 * s, metal);

    Color nucleo = (e->estado == 3) ? (Color){ 60, 90, 140, 255 }
                 : ColorLerp((Color){ 255, 120, 0, 255 }, WHITE, k);
    if (e->estado != 3)
        DrawCircleV(p, (9 + 8 * k + sinf(t * 10.0f) * 1.5f) * s, Fade(nucleo, 0.35f));
    DrawCircleV(p, 7 * s, nucleo);

    if (e->estado == 3) {
        // Superaquecido: rachaduras (é a hora de atacar)
        DrawLine((int)p.x - 6, (int)p.y - 6, (int)p.x + 2, (int)p.y + 1, BLACK);
        DrawLine((int)p.x + 2, (int)p.y + 1, (int)p.x + 6, (int)p.y - 3, BLACK);
        DrawText("!!", (int)p.x - 5, (int)p.y - 32, 12, SKYBLUE);
    }

    // Aviso da investida: o anel da carga (o corredor, que cresce com a carga, fica no chão)
    if (e->estado == 1)
        DrawRing(e->pos, 20 * s, 23 * s, -90.0f, -90.0f + 360.0f * Clamp(e->carga / e->cargaAlvo, 0.0f, 1.0f), 20, ORANGE);
}

// Botnet: núcleo com três nós orbitando, ligados por fios
static void DesenharBotnet(const Inimigo *e, float escala, float t)
{
    float s = escala;
    Vector2 p = { e->pos.x, e->pos.y + sinf(t * 3.0f + e->pos.x) * 2.0f };
    Color ciano = { 0, 228, 255, 255 };

    if (e->estado == 1)
        DrawCircleV(p, (e->raio + 6) * s, Fade(ciano, 0.25f + 0.25f * sinf(t * 25.0f)));

    for (int n = 0; n < 3; n++) {
        float a = t * 2.0f + n * (2.0f * PI / 3.0f);
        Vector2 q = { p.x + cosf(a) * 16 * s, p.y + sinf(a) * 16 * s };
        DrawLineEx(p, q, 1, Fade(ciano, 0.7f));
        DrawRectangle((int)(q.x - 3 * s), (int)(q.y - 3 * s), (int)(6 * s), (int)(6 * s), (Color){ 20, 60, 80, 255 });
        DrawRectangleLines((int)(q.x - 3 * s), (int)(q.y - 3 * s), (int)(6 * s), (int)(6 * s), ciano);
    }

    DrawPoly(p, 6, 9 * s, t * 40.0f, (Color){ 10, 40, 60, 255 });
    DrawPolyLinesEx(p, 6, 9 * s, t * 40.0f, 2, ciano);
    DrawCircleV(p, 3 * s, WHITE);
}

// Keylogger: corpo fino de teclado (com teclas) e o "REC" piscando
static void DesenharKeylogger(const Inimigo *e, float escala, float t)
{
    float s = escala;
    Vector2 p = { e->pos.x, e->pos.y + sinf(t * 2.5f + e->pos.y) * 2.0f };
    Rectangle r = { p.x - 8 * s, p.y - 18 * s, 16 * s, 36 * s };
    static const char *teclas[6] = { "Q", "W", "E", "A", "S", "D" };

    DrawEllipse((int)e->pos.x, (int)e->pos.y + 20, 9 * s, 3 * s, Fade(BLACK, 0.4f));
    DrawRectangleRec(r, (Color){ 40, 40, 48, 255 });
    DrawRectangleLinesEx(r, 1, (e->estado >= 1) ? RED : LIGHTGRAY);

    for (int k = 0; k < 6; k++) {
        int cx = (int)(r.x + 2 * s + (k % 2) * 7 * s), cy = (int)(r.y + 3 * s + (k / 2) * 10 * s);
        DrawRectangle(cx, cy, (int)(6 * s), (int)(7 * s), (Color){ 200, 200, 210, 255 });
        if (s > 0.9f) DrawText(teclas[k], cx + 1, cy, 8, (Color){ 40, 40, 48, 255 });
    }

    // REC
    if (((int)(t * 2.0f)) % 2 == 0 || e->estado >= 1) {
        DrawCircle((int)(p.x - 9 * s), (int)(r.y - 6 * s), 2.5f * s, RED);
        DrawText("REC", (int)(p.x - 5 * s), (int)(r.y - 10 * s), 8, RED);
    }
}

// Mira do Keylogger no chão (onde ele acha que o jogador vai estar)
static void DesenharMiraKeylogger(const Inimigo *e, float t)
{
    if (e->estado != 1 && e->estado != 2)
        return;

    Vector2 m = e->miraPrevista;
    float travou = (e->estado == 2 || e->timer <= 0.15f) ? 1.0f : 0.0f;
    float raio = 16.0f - 6.0f * travou;

    DrawLineEx(e->pos, m, 1.0f, Fade(RED, 0.35f));
    DrawCircleLines((int)m.x, (int)m.y, raio, Fade(RED, 0.6f + 0.4f * sinf(t * 20.0f)));
    DrawLine((int)(m.x - raio - 4), (int)m.y, (int)(m.x - raio + 4), (int)m.y, RED);
    DrawLine((int)(m.x + raio - 4), (int)m.y, (int)(m.x + raio + 4), (int)m.y, RED);
    DrawLine((int)m.x, (int)(m.y - raio - 4), (int)m.x, (int)(m.y - raio + 4), RED);
    DrawLine((int)m.x, (int)(m.y + raio - 4), (int)m.x, (int)(m.y + raio + 4), RED);
}

// Cache: pilha de pentes de memória com o núcleo mostrando a energia guardada
static void DesenharCache(const Inimigo *e, float escala, float t)
{
    float s = escala;
    Vector2 p = e->pos;
    bool brilho = (e->estado == 1);

    DrawEllipse((int)p.x, (int)p.y + 20, 18 * s, 5 * s, Fade(BLACK, 0.4f));

    for (int k = 0; k < 3; k++) {
        float y = p.y - 14 * s + k * 10 * s + sinf(t * 2.0f + k) * 1.5f;
        Rectangle pente = { p.x - 16 * s, y, 32 * s, 8 * s };
        DrawRectangleRec(pente, brilho ? (Color){ 120, 220, 140, 255 } : (Color){ 20, 90, 50, 255 });
        DrawRectangleLinesEx(pente, 1, (Color){ 0, 228, 48, 255 });
        for (int c = 0; c < 4; c++)
            DrawRectangle((int)(pente.x + 3 * s + c * 8 * s), (int)(y + 2 * s), (int)(5 * s), (int)(4 * s), (Color){ 10, 30, 20, 255 });
        for (int pino = 0; pino < 6; pino++)
            DrawRectangle((int)(pente.x + 2 * s + pino * 5 * s), (int)(y + 7 * s), (int)(2 * s), (int)(2 * s), GOLD);
    }

    // Núcleo: a energia guardada
    float k = e->energia / CACHE_ENERGIA_MAX;
    DrawCircleV(p, 6 * s, (Color){ 10, 30, 20, 255 });
    DrawRing(p, 6 * s, 9 * s, -90.0f, -90.0f + 360.0f * k, 16, (Color){ 0, 228, 255, 255 });

    // Recuperando: um anel verde enchendo e cruzinhas subindo
    if (e->recuperando > 0.0f) {
        float feito = 1.0f - e->recuperando / CACHE_RECUPERAR;
        DrawRing(p, e->raio + 6, e->raio + 9, -90.0f, -90.0f + 360.0f * feito, 24, LIME);
        for (int c = 0; c < 3; c++) {
            float sobe = fmodf(t * 30.0f + c * 12.0f, 30.0f);
            DrawText("+", (int)(p.x - 14 + c * 12), (int)(p.y - 22 - sobe), 12, Fade(LIME, 1.0f - sobe / 30.0f));
        }
    }
}

// ---------- Avisos dos inimigos (no padrão da luta da Luci) ----------
// Bote e investida: o CORREDOR enche; quando a mira trava, as bordas acendem em branco e as setas correm.
// Anel, leque e tiro: uma LINHA para cada bala, crescendo até o disparo (dá para ver a brecha antes).
void DesenharCorredorAvisoIni(Vector2 ini, Vector2 dir, float alcance, float largura, float k, bool travou, Color cor, float t)
{
    Vector2 n = { -dir.y, dir.x };
    float pisca = (sinf(t * 40.0f) > 0.0f) ? 1.0f : 0.35f;
    Vector2 fim = Vector2Add(ini, Vector2Scale(dir, alcance));
    Vector2 cheio = Vector2Lerp(ini, fim, Clamp(k, 0.0f, 1.0f));
    Vector2 a1 = Vector2Add(ini, Vector2Scale(n, largura)), a2 = Vector2Subtract(ini, Vector2Scale(n, largura));
    Vector2 b1 = Vector2Add(fim, Vector2Scale(n, largura)), b2 = Vector2Subtract(fim, Vector2Scale(n, largura));
    Vector2 c1 = Vector2Add(cheio, Vector2Scale(n, largura)), c2 = Vector2Subtract(cheio, Vector2Scale(n, largura));
    DesenharTrianguloB2(a1, a2, b2, Fade(cor, 0.07f));
    DesenharTrianguloB2(a1, b2, b1, Fade(cor, 0.07f));
    DesenharTrianguloB2(a1, a2, c2, Fade(cor, travou ? 0.28f : 0.16f));
    DesenharTrianguloB2(a1, c2, c1, Fade(cor, travou ? 0.28f : 0.16f));
    Color borda = travou ? Fade(WHITE, 0.55f + 0.45f * pisca) : Fade(cor, 0.55f);
    DrawLineEx(a1, b1, travou ? 2.0f : 1.5f, borda);
    DrawLineEx(a2, b2, travou ? 2.0f : 1.5f, borda);
    DrawLineEx(b1, b2, 1.5f, borda);
    if (travou)
        for (int s = 0; s < 2; s++) {                             // setas correndo para frente
            float u = fmodf(t * 3.5f + (float)s / 2.0f, 1.0f);
            float l = fminf(largura * 0.8f, 8.0f);
            Vector2 c = Vector2Lerp(ini, fim, u), ponta = Vector2Add(c, Vector2Scale(dir, 6.0f));
            DrawLineEx(Vector2Add(c, Vector2Scale(n, l)), ponta, 2.0f, Fade(WHITE, 0.8f * (1.0f - u)));
            DrawLineEx(Vector2Subtract(c, Vector2Scale(n, l)), ponta, 2.0f, Fade(WHITE, 0.8f * (1.0f - u)));
        }
}

void DesenharLinhasBalasIni(Vector2 p, float raio, int n, float angIni, float passo, float k, Color cor, float t)
{
    k = Clamp(k, 0.0f, 1.0f);
    bool quase = k > 0.75f;
    float pisca = (sinf(t * 30.0f) > 0.0f) ? 1.0f : 0.5f;
    float comp = 14.0f + 40.0f * k;
    float anel = raio + 6.0f + 30.0f * (1.0f - k);                // o anel fecha até a hora do disparo
    DrawRing(p, anel, anel + 2.0f, 0.0f, 360.0f, 32, Fade(cor, 0.12f + 0.40f * k));
    for (int i = 0; i < n; i++) {
        float a = angIni + passo * (float)i;
        Vector2 d = { cosf(a), sinf(a) };
        Vector2 a0 = Vector2Add(p, Vector2Scale(d, raio + 4.0f));
        Vector2 a1 = Vector2Add(a0, Vector2Scale(d, comp));
        DrawLineEx(a0, a1, quase ? 2.5f : 1.5f, quase ? Fade(WHITE, 0.45f + 0.4f * pisca) : Fade(cor, 0.25f + 0.4f * k));
        DrawCircleV(a1, quase ? 2.5f : 1.8f, Fade(cor, 0.4f + 0.5f * k));
    }
}

static void DesenharAvisoInimigo(const Inimigo *e, float t)
{
    Color cor = DEF_INIMIGOS[e->tipo].cor;

    switch (e->tipo) {
        case INIMIGO_FRAGMENTO:
            if (e->estado == 1) {
                float k = 1.0f - e->timer / FRAGMENTO_PREPARO;
                DesenharCorredorAvisoIni(e->pos, e->alvo, FRAGMENTO_BOTE_VEL * FRAGMENTO_BOTE_DURACAO + e->raio, e->raio, k, k > 0.55f, RED, t);
            }
            break;
        case INIMIGO_PHISHER:
        case INIMIGO_ISCA:              // a cópia falsa avisa igual (é para enganar)
            if (e->estado == 1)
                DesenharCorredorAvisoIni(e->pos, e->alvo, PHISHER_BOTE_VEL * PHISHER_BOTE_DURACAO + e->raio, e->raio,
                                         1.0f - e->timer / PHISHER_PREPARO, e->timer <= 0.12f, GOLD, t);
            break;
        case INIMIGO_OVERCLOCK:
            if (e->estado == 1) {
                float alcance = (OVERCLOCK_INVESTIDA_MIN + (OVERCLOCK_INVESTIDA_MAX - OVERCLOCK_INVESTIDA_MIN) * RogueCargaOverclock(e)) * OVERCLOCK_INVESTIDA_DURACAO;
                DesenharCorredorAvisoIni(e->pos, e->alvo, alcance, e->raio, Clamp(e->carga / e->cargaAlvo, 0.0f, 1.0f),
                                         e->cargaAlvo - e->carga <= 0.15f, (Color){ 255, 90, 40, 255 }, t);
            }
            break;
        case INIMIGO_GUARDIAO:
            if (e->estado == 1) {
                int leque = e->elite ? GUARDIAO_LEQUE + 2 : GUARDIAO_LEQUE;
                float passo = GUARDIAO_ABERTURA * DEG2RAD;
                float base = atan2f(e->alvo.y, e->alvo.x) - passo * (float)(leque - 1) / 2.0f;
                DesenharLinhasBalasIni(e->pos, e->raio, leque, base, passo, 1.0f - e->timer / GUARDIAO_PREPARO, ORANGE, t);
            }
            break;
        case INIMIGO_BOTNET:
            if (e->estado == 1)
                DesenharLinhasBalasIni(e->pos, e->raio, 1, atan2f(e->alvo.y, e->alvo.x), 0.0f, 1.0f - e->timer / BOTNET_PREPARO, (Color){ 0, 228, 255, 255 }, t);
            break;
        case INIMIGO_ECO: {
            float k = 1.0f - e->timer / ECO_TEMPO;
            if (k > 0.35f)              // só no fim: as balas vão sair por estas linhas
                DesenharLinhasBalasIni(e->pos, e->raio, ECO_BALAS, e->anim + e->timer, 2.0f * PI / ECO_BALAS, (k - 0.35f) / 0.65f, MAGENTA, t);
        } break;
        case INIMIGO_CACHE:
            if (e->estado == 1 && e->recuperando <= 0.0f) {
                int balas = e->elite ? CACHE_BALAS + 4 : CACHE_BALAS;
                DesenharLinhasBalasIni(e->pos, e->raio, balas, e->anim + e->timer, 2.0f * PI / (float)balas, 1.0f - e->timer / CACHE_PREPARO, cor, t);
            }
            break;
        case INIMIGO_NUCLEO:
            if (e->estado == 1)
                DesenharLinhasBalasIni(e->pos, e->raio, NUCLEO_BALAS, (e->anim + e->timer) * 0.7f, 2.0f * PI / NUCLEO_BALAS, 1.0f - e->timer / NUCLEO_PREPARO, cor, t);
            break;
        case INIMIGO_SENTINELA:         // a trava: o feixe vai sair por este corredor (até a estante)
            if (e->estado == 1)
                DesenharCorredorAvisoIni(e->pos, e->alvo, Vector2Distance(e->pos, e->fimFeixe), SENTINELA_FEIXE_LARGURA / 2.0f + 2.0f,
                                         1.0f - e->timer / SENTINELA_TRAVA, true, RED, t);
            break;
        case INIMIGO_RETRO:             // rebobinando: um corredor até cada marca (é por onde os golpes voltam)
            if (e->estado == 1) {
                float k = 1.0f - e->timer / RETRO_AVISO;
                for (int r = 0; r < e->numRegistros; r++) {
                    Vector2 d = Vector2Subtract(e->registro[r], e->pos);
                    float l = Vector2Length(d);
                    if (l > 1.0f) DesenharCorredorAvisoIni(e->pos, Vector2Scale(d, 1.0f / l), l, 6.0f, k, k > 0.7f, RED, t);
                }
            }
            break;
        case INIMIGO_JANELA:            // a resposta: o leque mirado no que ela lembra
            if (e->estado == 1 && e->carga <= 0.5f) {
                int nRel = e->tiros;
                int balas = (3 + 2 * nRel > 11) ? 11 : 3 + 2 * nRel;
                float arco = (50.0f / (1.0f + 0.4f * (float)nRel)) * DEG2RAD;
                Vector2 d = Vector2Subtract(e->alvo, e->pos);
                float base = (Vector2Length(d) > 1.0f) ? atan2f(d.y, d.x) : PI / 2.0f;
                DesenharLinhasBalasIni(e->pos, e->raio, balas, base - arco / 2.0f, (balas > 1) ? arco / (float)(balas - 1) : 0.0f,
                                       1.0f - e->timer / 0.8f, GOLD, t);
            }
            break;
        case INIMIGO_VERBOSO:
            if (e->estado == 1 || e->estado == 2) {
                int balas = (e->hp < e->hpMax * 0.5f) ? 14 : 10;
                float giro = (e->estado == 2) ? PI / (float)balas : 0.0f;
                float k = (e->estado == 1) ? 1.0f - e->timer / 0.85f : 1.0f - e->timer / 0.4f;
                DesenharLinhasBalasIni(e->pos, e->raio, balas, giro, 2.0f * PI / (float)balas, k, (Color){ 255, 90, 70, 255 }, t);
            }
            break;
        default:
            break;
    }
}

// Por baixo dos inimigos: escudo de rede dos Firewalls e as conexões dos Botnets
void DesenharSinergiasChao(const Biblioteca *b, float t)
{
    Color verde = { 0, 228, 48, 255 }, ciano = { 0, 228, 255, 255 };

    for (int i = 0; i < MAX_INIMIGOS; i++) {

        const Inimigo *e = &b->ini[i];

        if (!e->ativo || e->surgindo > 0.0f)
            continue;

        if (e->tipo == INIMIGO_FIREWALL) {
            DrawCircleV(e->pos, FIREWALL_ZONA, Fade(verde, 0.05f));
            for (int k = 0; k < 12; k += 2) {
                float a1 = t * 0.3f + k * (PI / 6.0f), a2 = a1 + PI / 6.0f;
                DrawLineEx((Vector2){ e->pos.x + cosf(a1) * FIREWALL_ZONA, e->pos.y + sinf(a1) * FIREWALL_ZONA },
                           (Vector2){ e->pos.x + cosf(a2) * FIREWALL_ZONA, e->pos.y + sinf(a2) * FIREWALL_ZONA }, 2, Fade(verde, 0.35f));
            }
        }

        if (e->tipo == INIMIGO_BOTNET) {
            for (int k = 0; k < BOTNET_CONEXOES; k++) {
                if (e->conexao[k] < 0) continue;
                Vector2 alvo = b->ini[e->conexao[k]].pos;
                DrawLineEx(e->pos, alvo, 1.5f, Fade(ciano, 0.45f));
                float f = fmodf(t * 1.5f + k * 0.33f, 1.0f);         // pacote correndo pelo fio
                DrawCircleV(Vector2Lerp(e->pos, alvo, f), 2.5f, WHITE);
            }
        }

        if (e->tipo == INIMIGO_KEYLOGGER)
            DesenharMiraKeylogger(e, t);

        if (e->atordoado <= 0.0f)
            DesenharAvisoInimigo(e, t);
    }
}

// Por cima: quem está protegido (hexágono) e quem está conectado (antena)
void DesenharSinergiasMarcas(const Biblioteca *b, float t)
{
    for (int i = 0; i < MAX_INIMIGOS; i++) {

        const Inimigo *e = &b->ini[i];

        if (!e->ativo || e->surgindo > 0.0f || RogueInimigoPassageiro(e->tipo) || (e->tipo == INIMIGO_PHISHER && e->estado == 0))
            continue;           // o Phisher camuflado não ganha marca (não entrega o disfarce)

        if (e->tipo != INIMIGO_FIREWALL && RogueNaZonaFirewall(b, e))
            DrawPolyLines(e->pos, 6, e->raio + 6, t * 30.0f, Fade((Color){ 0, 228, 48, 255 }, 0.8f));

        if (e->conectado) {
            Vector2 a = { e->pos.x + e->raio, e->pos.y - e->raio };
            DrawCircleV(a, 2, (Color){ 0, 228, 255, 255 });
            DrawRing(a, 4, 5, -80.0f, -10.0f, 6, (Color){ 0, 228, 255, 255 });
            DrawRing(a, 7, 8, -80.0f, -10.0f, 6, Fade((Color){ 0, 228, 255, 255 }, 0.6f + 0.4f * sinf(t * 8.0f)));
        }
    }
}


// =============================================================
// EXPANSÃO DA PROGRESSÃO: DESENHO DOS ENCONTROS ESPECIAIS
// =============================================================

// SENTINELA DE ATENÇÃO: lente (olho) num casco hexagonal, com três "cabeças de atenção" em órbita.
// A linha de atenção engrossa com o foco; travada, vira uma faixa vermelha; no disparo, um feixe branco.
void DesenharSentinela(const Inimigo *e, float escala, float t)
{
    Color cor = DEF_INIMIGOS[INIMIGO_SENTINELA].cor;
    float r = e->raio * escala;
    bool vulneravel = (e->estado == 3 || e->distraida > 0.0f);

    // Linha de atenção (só quando vê o jogador) e a mira que fecha em volta dele
    if (e->estado == 0 && e->visao && e->foco > 0.02f) {
        DrawLineEx(e->pos, e->miraPrevista, 1.0f + 3.0f * e->foco, Fade(cor, 0.25f + 0.5f * e->foco));
        float rm = 12.0f + 30.0f * (1.0f - e->foco);
        DrawCircleLines((int)e->miraPrevista.x, (int)e->miraPrevista.y, rm, Fade(RED, 0.4f + 0.5f * e->foco));
        for (int k = 0; k < 4; k++) {
            float a = (float)k * PI / 2.0f + t;
            DrawLineEx((Vector2){ e->miraPrevista.x + cosf(a) * rm, e->miraPrevista.y + sinf(a) * rm },
                       (Vector2){ e->miraPrevista.x + cosf(a) * (rm + 6), e->miraPrevista.y + sinf(a) * (rm + 6) }, 2, Fade(RED, 0.7f));
        }
    }
    // Travada: o corredor do feixe é desenhado no chão (DesenharAvisoInimigo)
    // O feixe: brilho largo, núcleo branco tremendo, clarão na lente e o anel do impacto na estante
    if (e->estado == 2) {
        float q = Clamp(e->timer / SENTINELA_FEIXE, 0.0f, 1.0f);                // 1 -> 0
        float w = SENTINELA_FEIXE_LARGURA * (0.6f + 0.4f * q) + 2.0f * sinf(t * 60.0f);
        DrawLineEx(e->pos, e->fimFeixe, w + 18, Fade(cor, 0.06f + 0.14f * q));
        DrawLineEx(e->pos, e->fimFeixe, w + 6, Fade(cor, 0.45f));
        DrawLineEx(e->pos, e->fimFeixe, w * 0.6f, Fade(WHITE, 0.95f));
        DrawCircleV(e->fimFeixe, 10 + 6 * q, Fade(WHITE, 0.6f));
        DrawRing(e->fimFeixe, 14 + 18 * (1 - q), 17 + 18 * (1 - q), 0.0f, 360.0f, 24, Fade(cor, 0.7f * q));
        DrawCircleV(e->pos, r * 0.9f, Fade(WHITE, 0.35f * q));
    }

    // Cabeças de atenção em órbita (mais rápidas quanto mais foco)
    for (int k = 0; k < 3; k++) {
        float a = t * (1.2f + 3.0f * e->foco) + (float)k * 2.0f * PI / 3.0f;
        Vector2 c = { e->pos.x + cosf(a) * (r + 12), e->pos.y + sinf(a) * (r + 12) };
        DrawTriangle((Vector2){ c.x - 5, c.y + 4 }, (Vector2){ c.x + 5, c.y + 4 }, (Vector2){ c.x, c.y - 6 }, vulneravel ? GRAY : cor);
    }

    // Casco e lente
    DrawPoly(e->pos, 6, r, t * 20.0f, (Color){ 30, 40, 60, 255 });
    DrawPolyLinesEx(e->pos, 6, r, t * 20.0f, 2, vulneravel ? GOLD : cor);
    Vector2 olhar = (e->distraida > 0.0f) ? (Vector2){ sinf(t * 6.0f), 0 } : e->alvo;
    if (e->estado == 0 && e->visao) olhar = Vector2Normalize(Vector2Subtract(e->miraPrevista, e->pos));
    if (e->estado == 3) {
        DrawLineEx((Vector2){ e->pos.x - r * 0.5f, e->pos.y }, (Vector2){ e->pos.x + r * 0.5f, e->pos.y }, 3, GRAY);   // lente fechada
    } else {
        DrawCircleV(e->pos, r * 0.55f, (Color){ 230, 245, 255, 255 });
        DrawCircleV((Vector2){ e->pos.x + olhar.x * r * 0.25f, e->pos.y + olhar.y * r * 0.25f }, r * 0.28f,
                    (e->estado == 1 || e->estado == 2) ? RED : (Color){ 20, 90, 160, 255 });
    }
    if (vulneravel) {
        DrawCircleLines((int)e->pos.x, (int)e->pos.y, r + 4 + 2 * sinf(t * 12.0f), GOLD);
        DrawText((e->distraida > 0.0f && e->estado != 3) ? "?" : "!", (int)e->pos.x - 3, (int)(e->pos.y - r - 30), 16, GOLD);
    }
}

// RETROPROPAGADOR: um "nabla" (triângulo invertido) com camadas de neurônios.
// As marcas no chão mostram para onde os golpes vão voltar.
void DesenharRetro(const Inimigo *e, float escala, float t)
{
    Color cor = DEF_INIMIGOS[INIMIGO_RETRO].cor;
    float r = e->raio * escala;
    bool recalibrando = (e->estado == 3);

    // Marcas: de onde vieram os golpes
    for (int k = 0; k < e->numRegistros; k++) {
        Vector2 m = e->registro[k];
        float pulso = (e->estado == 1) ? 0.5f + 0.5f * sinf(t * 20.0f) : 0.6f;
        DrawLineEx((Vector2){ m.x - 7, m.y - 7 }, (Vector2){ m.x + 7, m.y + 7 }, 2, Fade(RED, pulso));
        DrawLineEx((Vector2){ m.x + 7, m.y - 7 }, (Vector2){ m.x - 7, m.y + 7 }, 2, Fade(RED, pulso));
        DrawCircleLines((int)m.x, (int)m.y, 10, Fade(cor, 0.5f * pulso));      // o caminho de volta fica no chão (DesenharAvisoInimigo)
    }

    // Corpo: nabla com três camadas de pontos ligadas
    Color corpo = recalibrando ? (Color){ 90, 70, 60, 255 } : (Color){ 70, 25, 15, 255 };
    Vector2 a = { e->pos.x - r, e->pos.y - r * 0.7f }, b2 = { e->pos.x + r, e->pos.y - r * 0.7f }, c = { e->pos.x, e->pos.y + r };
    DrawTriangle(a, c, b2, corpo);
    DrawTriangleLines(a, c, b2, recalibrando ? GRAY : cor);
    for (int camada = 0; camada < 3; camada++) {
        int n = 3 - camada;
        float y = e->pos.y - r * 0.45f + camada * r * 0.45f;
        for (int k = 0; k < n; k++) {
            float x = e->pos.x + ((float)k - (n - 1) / 2.0f) * r * 0.5f;
            float brilho = (e->estado == 2) ? 1.0f : 0.5f + 0.5f * sinf(t * 4.0f + camada + k);
            DrawCircleV((Vector2){ x, y }, 2.5f, Fade(recalibrando ? GRAY : ORANGE, brilho));
        }
    }

    // Contador de golpes guardados
    if (e->numRegistros > 0 && !recalibrando) {
        const char *txt = TextFormat("%d%s", e->numRegistros, (e->excesso > 0) ? "+" : "");
        DrawText(txt, (int)e->pos.x - MeasureText(txt, 12) / 2, (int)(e->pos.y - r - 26), 12, RED);
    }
    if (recalibrando)
        DrawCircleLines((int)e->pos.x, (int)e->pos.y, r + 5 + 2 * sinf(t * 12.0f), GOLD);
}

// JANELA DE CONTEXTO: uma janela de aplicativo com linhas de texto correndo. Com PROTEGER, escudo
// hexagonal; sem ele, a moldura racha e o núcleo aparece.
void DesenharJanelaChefe(const Inimigo *e, float escala, float t)
{
    Color cor = DEF_INIMIGOS[INIMIGO_JANELA].cor;
    bool aberta = (e->estado == 3);
    float w = 70.0f * escala, h = 52.0f * escala;
    Vector2 p = { e->pos.x, e->pos.y + sinf(t * 1.6f) * 3.0f };
    Rectangle r = { p.x - w / 2, p.y - h / 2, w, h };

    // Telegrafia da resposta: linhas até onde ele LEMBRA que o jogador estava
    if (e->estado == 1) {
        bool pisca = ((int)(t * 12.0f)) % 2 == 0;
        if (e->carga > 0.5f) {
            DrawText("?", (int)p.x - 6, (int)(r.y - 30), 24, Fade(LIGHTGRAY, pisca ? 1.0f : 0.5f));
        } else {                                              // as linhas do leque ficam no chão (DesenharAvisoInimigo)
            DrawCircleLines((int)e->alvo.x, (int)e->alvo.y, 14, Fade(GOLD, pisca ? 0.9f : 0.5f));
            DrawText("LEMBRA DAQUI", (int)e->alvo.x - MeasureText("LEMBRA DAQUI", 8) / 2, (int)e->alvo.y + 16, 8, GOLD);
        }
    }

    DrawEllipse((int)p.x, (int)(r.y + h + 10), w * 0.5f, 7, Fade(BLACK, 0.4f));

    if (!aberta) {
        float pulso = 0.5f + 0.5f * sinf(t * 3.0f);
        DrawPoly(p, 6, w * 0.78f, t * 10.0f, Fade((Color){ 0, 228, 255, 255 }, 0.08f + 0.06f * pulso));
        DrawPolyLinesEx(p, 6, w * 0.78f, t * 10.0f, 2, Fade((Color){ 0, 228, 255, 255 }, 0.6f));
    }

    // Corpo: a janela
    DrawRectangleRec(r, (Color){ 22, 20, 44, 255 });
    DrawRectangle((int)r.x, (int)r.y, (int)r.width, 9, aberta ? MAGENTA : cor);
    for (int k = 0; k < 3; k++) DrawCircle((int)(r.x + r.width - 8 - k * 7), (int)r.y + 4, 2, (Color){ 22, 20, 44, 255 });
    // Linhas de texto correndo. O sorteio usa a largura FIXA do corpo e só o resultado é escalado:
    // surgindo, a escala começa em 0 (a largura escalada viraria um "% 0", divisão por zero que fechava o jogo)
    for (int k = 0; k < 5; k++) {
        float fase = fmodf(t * 18.0f + k * 9.0f, fmaxf(1.0f, h - 14.0f));
        int lw = (int)((float)(12 + (int)(Ruido((unsigned int)(k * 13 + (int)(t * 2.0f))) % 42u)) * escala);
        DrawRectangle((int)r.x + 6, (int)(r.y + 12 + fase), lw, 2, Fade(cor, 0.6f));
    }
    DrawRectangleLinesEx(r, 2, aberta ? MAGENTA : cor);

    if (aberta) {
        // Rachaduras e o núcleo exposto
        DrawLineEx((Vector2){ r.x + 8, r.y + 12 }, (Vector2){ p.x, p.y }, 2, MAGENTA);
        DrawLineEx((Vector2){ r.x + r.width - 6, r.y + h - 6 }, (Vector2){ p.x + 4, p.y + 2 }, 2, MAGENTA);
        DrawCircleV(p, 10 + 3 * sinf(t * 10.0f), MAGENTA);
        DrawCircleV(p, 5, WHITE);
    }
    if (e->estado == 2)
        DrawText("RESUMINDO", (int)p.x - MeasureText("RESUMINDO", 10) / 2, (int)(r.y - 16), 10, cor);
}

// Dado dourado: o que a Janela sabe sobre o jogador (dá para destruir com um tiro)
// ALUCINAÇÃO: um fantasma de imagem duplicada que treme antes de sumir
static void DesenharAlucinacaoInimigo(const Inimigo *e, float escala, float t)
{
    float s = e->raio * escala;
    Vector2 p = e->pos;
    Color cor = DEF_INIMIGOS[INIMIGO_ALUCINACAO].cor;
    if (e->estado == 1) { p.x += (float)GetRandomValue(-2, 2); p.y += (float)GetRandomValue(-2, 2); }
    float alfa = (e->estado == 1 && ((int)(t * 20.0f)) % 2 == 0) ? 0.25f : 0.8f;

    DrawCircleV((Vector2){ p.x - 4, p.y }, s, Fade(SKYBLUE, 0.35f));      // a "imagem duplicada" (erro de renderização)
    DrawCircleV((Vector2){ p.x + 4, p.y }, s, Fade(RED, 0.35f));
    DrawCircleV(p, s, Fade(cor, alfa));
    for (int k = 0; k < 3; k++) {                                          // barra do fantasma, ondulando
        float x = p.x - s + s * (0.35f + 0.65f * (float)k);
        DrawCircleV((Vector2){ x, p.y + s * 0.8f + sinf(t * 8.0f + (float)k) * 2.0f }, s * 0.35f, Fade(cor, alfa));
    }
    DrawCircleV((Vector2){ p.x - s * 0.35f, p.y - s * 0.15f }, s * 0.22f, WHITE);
    DrawCircleV((Vector2){ p.x + s * 0.35f, p.y - s * 0.15f }, s * 0.22f, WHITE);
    DrawText("?", (int)p.x - 3, (int)(p.y - s - 12), 10, Fade(WHITE, alfa));
}

// VERBOSO: um balão de fala que não para de se encher de texto; incha e fica vermelho antes de soltar as balas
static void DesenharVerboso(const Inimigo *e, float escala, float t)
{
    Color cor = DEF_INIMIGOS[INIMIGO_VERBOSO].cor;
    float carga = (e->estado == 1) ? (1.0f - e->timer / 0.85f) : ((e->estado == 2) ? 1.0f : 0.0f);
    float s = e->raio * escala * (1.0f + 0.25f * carga);
    Vector2 p = e->pos;
    if (e->estado == 1) { p.x += (float)GetRandomValue(-1, 1); p.y += (float)GetRandomValue(-1, 1); }
    p.y += sinf(t * 2.5f + e->pos.x) * 2.0f;                                   // flutua
    Color borda = (carga > 0.0f) ? (Color){ 255, (unsigned char)(200 - 150 * carga), (unsigned char)(120 - 90 * carga), 255 } : cor;

    // Balão: retângulo arredondado com a "cauda" apontando para baixo
    float w = s * 2.5f, h = s * 1.8f;
    Rectangle fora = { p.x - w / 2 - 3, p.y - h / 2 - 3, w + 6, h + 6 };
    Rectangle dentro = { p.x - w / 2, p.y - h / 2, w, h };
    DrawRectangleRounded(fora, 0.45f, 8, Fade(borda, 0.25f + 0.35f * carga));
    DrawRectangleRounded(dentro, 0.4f, 8, (Color){ 235, 238, 250, 255 });
    DrawTriangle((Vector2){ p.x - s * 0.55f, p.y + h / 2 - 2 }, (Vector2){ p.x - s * 0.1f, p.y + h / 2 - 2 }, (Vector2){ p.x - s * 0.6f, p.y + h / 2 + s * 0.65f }, (Color){ 235, 238, 250, 255 });
    DrawLineEx((Vector2){ p.x - s * 0.6f, p.y + h / 2 + s * 0.65f }, (Vector2){ p.x - s * 0.1f, p.y + h / 2 - 2 }, 2, borda);
    DrawLineEx((Vector2){ p.x - s * 0.6f, p.y + h / 2 + s * 0.65f }, (Vector2){ p.x - s * 0.55f, p.y + h / 2 - 2 }, 2, borda);

    // Linhas de texto que correm sem parar (cada linha é mais comprida e rápida que a anterior)
    for (int k = 0; k < 4; k++) {
        float ly = p.y - h * 0.32f + (float)k * h * 0.2f;
        float comp = w * (0.55f + 0.3f * fabsf(sinf(t * (1.2f + 0.5f * (float)k) + (float)k * 1.7f)));
        Color tinta = (carga > 0.0f) ? (Color){ (unsigned char)(70 + 170 * carga), 40, 60, 255 } : (Color){ 60, 70, 120, 255 };
        DrawRectangle((int)(p.x - w * 0.42f), (int)ly, (int)(comp * 0.84f), 3, tinta);
        DrawRectangle((int)(p.x - w * 0.42f + comp * 0.84f + 3), (int)ly, 3, 3, tinta);       // "pontinho" final, como uma palavra a mais
    }

    // Carregando: o balão enche de "!!!"; parado: as reticências ficam piscando no canto
    if (e->estado == 0) {
        for (int k = 0; k < 3; k++)
            if (((int)(t * 3.0f) % 4) > k)
                DrawCircleV((Vector2){ p.x + w * 0.28f + (float)k * 6.0f, p.y + h * 0.36f }, 2, borda);
    } else {
        DrawText("!!!", (int)(p.x - MeasureText("!!!", 12) / 2), (int)(p.y - s * 0.28f), 12, Fade(RED, 0.5f + 0.5f * sinf(t * 30.0f)));
    }
}

static void DesenharToken(const Inimigo *e, float t)
{
    float s = 7.0f + sinf(t * 8.0f + e->pos.x) * 1.0f;
    DrawPoly(e->pos, 4, s + 4, 45.0f, Fade(GOLD, 0.25f));
    DrawPoly(e->pos, 4, s, 45.0f + t * 90.0f, GOLD);
    DrawCircleV(e->pos, 2, (Color){ 60, 40, 0, 255 });
}

// Barra do mini-chefe: vida e a janela de contexto (o mais antigo à esquerda)
void DesenharBarraJanela(const Biblioteca *b, float tempo)
{
    const JanelaContexto *jc = &b->janela;
    const Inimigo *e = (jc->indice >= 0 && jc->indice < MAX_INIMIGOS) ? &b->ini[jc->indice] : NULL;
    if (e == NULL || !e->ativo || e->tipo != INIMIGO_JANELA)
        return;

    // Vida
    int x = 220, y = 42, w = 360;
    DrawRectangle(x - 4, y - 2, w + 8, 14, Fade(BLACK, 0.8f));
    DrawRectangle(x, y + 1, w, 8, (Color){ 30, 26, 50, 255 });
    DrawRectangle(x, y + 1, (int)(w * e->hp / e->hpMax), 8, (jc->vulneravel > 0.0f) ? MAGENTA : DEF_INIMIGOS[INIMIGO_JANELA].cor);
    DrawRectangle(x + (int)(w * JANELA_FASE2) - 1, y - 1, 2, 12, WHITE);
    DrawRectangle(x + (int)(w * JANELA_FASE3) - 1, y - 1, 2, 12, WHITE);
    DrawRectangle(x + (int)(w * JANELA_FASE4) - 1, y - 1, 2, 12, WHITE);
    const char *nome = TextFormat("JANELA DE CONTEXTO  F%d", jc->fase);
    int wn = MeasureText(nome, 10);
    DrawRectangle(x - wn - 16, y - 2, wn + 10, 14, Fade(BLACK, 0.8f));
    DrawText(nome, x - wn - 11, y, 10, DEF_INIMIGOS[INIMIGO_JANELA].cor);

    // Janela de contexto: capacidade em blocos
    int lado = 52, alto = 22, gap = 4;
    int total = jc->capacidade * lado + (jc->capacidade - 1) * gap;
    int x0 = LARGURA_TELA / 2 - total / 2, y0 = 60;
    DrawRectangle(x0 - 70, y0 - 4, total + 140, alto + 8, Fade(BLACK, 0.75f));
    DrawText("esquece <", x0 - 66, y0 + 6, 10, Fade(RED, 0.8f));
    DrawText("< entra", x0 + total + 8, y0 + 6, 10, GOLD);

    for (int i = 0; i < jc->capacidade; i++) {
        Rectangle s = { (float)(x0 + i * (lado + gap)), (float)y0, (float)lado, (float)alto };
        if (i >= jc->num) {
            DrawRectangleLinesEx(s, 1, Fade(GRAY, 0.4f));
            continue;
        }
        int tipo = jc->slot[i];
        Color c = (tipo == CTX_SISTEMA) ? (Color){ 0, 228, 255, 255 } : ((tipo == CTX_RELEVANTE) ? GOLD : (Color){ 110, 110, 120, 255 });
        const char *rot = (tipo == CTX_SISTEMA) ? "PROTEGER" : ((tipo == CTX_RELEVANTE) ? "DADO" : "ruído");
        bool novo = (i == jc->num - 1 && jc->entrouAnim > 0.0f);
        bool proximo = (i == 0 && jc->num == jc->capacidade);       // o próximo a ser esquecido
        DrawRectangleRec(s, Fade(c, novo ? 0.6f : 0.3f));
        DrawRectangleLinesEx(s, proximo ? 2.0f : 1.0f, proximo ? ((((int)(tempo * 8.0f)) % 2) ? RED : c) : c);
        DrawText(rot, (int)(s.x + s.width / 2) - MeasureText(rot, 10) / 2, (int)s.y + 6, 10, (tipo == CTX_RUIDO) ? LIGHTGRAY : WHITE);
    }

    // O bloco que acabou de sair (cai para fora da janela)
    if (jc->esquecidoAnim > 0.0f) {
        float k = 1.0f - jc->esquecidoAnim / 0.9f;
        int tipo = jc->esquecidoTipo;
        Color c = (tipo == CTX_SISTEMA) ? (Color){ 0, 228, 255, 255 } : ((tipo == CTX_RELEVANTE) ? GOLD : GRAY);
        Rectangle s = { (float)(x0 - lado - 10) - 20.0f * k, (float)y0 + 30.0f * k, (float)lado, (float)alto };
        DrawRectangleRec(s, Fade(c, 0.5f * (1.0f - k)));
        DrawText("ESQUECIDO", (int)s.x, (int)(s.y + alto + 2), 10, Fade(RED, 1.0f - k));
    }

    // Escudo aberto: o tempo que resta
    if (jc->vulneravel > 0.0f) {
        DrawRectangle(x0, y0 + alto + 4, total, 4, Fade(BLACK, 0.6f));
        DrawRectangle(x0, y0 + alto + 4, (int)(total * jc->vulneravel / JANELA_VULNERAVEL), 4, MAGENTA);
    }

    // Dica curta e o aviso de que os blocos são uma simplificação
    const char *dica = jc->dica;
    int wd = MeasureText(dica, 10);
    DrawRectangle(LARGURA_TELA / 2 - wd / 2 - 6, y0 + alto + 10, wd + 12, 26, Fade(BLACK, 0.6f));
    DrawText(dica, LARGURA_TELA / 2 - wd / 2, y0 + alto + 12, 10, (Color){ 200, 195, 255, 255 });
    const char *nota = "(cada bloco é uma unidade simplificada de contexto, não uma palavra)";
    DrawText(nota, LARGURA_TELA / 2 - MeasureText(nota, 8) / 2, y0 + alto + 25, 8, GRAY);
}

void DesenharInimigo(const Inimigo *e)
{
    float escala = (e->surgindo > 0.0f) ? 1.0f - e->surgindo / TEMPO_SURGINDO : 1.0f;

    if (e->surgindo > 0.0f)
        DrawCircleLines((int)e->pos.x, (int)e->pos.y, e->raio + 12 * (1.0f - escala), Fade(MAGENTA, 0.7f));

    for (int vez = 0; vez < ((e->flash > 0.0f) ? 3 : 1); vez++) {

        // Levou dano: desenha de novo somando a cor (fica branco)
        if (vez == 1) BeginBlendMode(BLEND_ADDITIVE);

        float t = (float)GetTime();

        switch (e->tipo) {
            case INIMIGO_FRAGMENTO: DesenharFragmentoAlucinado(e, escala); break;
            case INIMIGO_GUARDIAO:  DesenharGuardiao(e, escala);           break;
            case INIMIGO_ECO:       DesenharEco(e);                        break;
            case INIMIGO_SPAMMER:   DesenharSpammer(e, escala);            break;
            case INIMIGO_SPAM:      DesenharSpam(e);                       break;
            case INIMIGO_PHISHER:
            case INIMIGO_ISCA:      DesenharPhisher(e, escala);            break;
            case INIMIGO_FIREWALL:  DesenharFirewall(e, escala, t);        break;
            case INIMIGO_OVERCLOCK: DesenharOverclock(e, escala, t);       break;
            case INIMIGO_BOTNET:    DesenharBotnet(e, escala, t);          break;
            case INIMIGO_KEYLOGGER: DesenharKeylogger(e, escala, t);       break;
            case INIMIGO_NUCLEO:    DesenharNucleoCorrupcao(e, escala, t); break;
            case INIMIGO_SENTINELA: DesenharSentinela(e, escala, t);       break;
            case INIMIGO_RETRO:     DesenharRetro(e, escala, t);           break;
            case INIMIGO_JANELA:    DesenharJanelaChefe(e, escala, t);     break;
            case INIMIGO_TOKEN:     DesenharToken(e, t);                   break;
            case INIMIGO_ALUCINACAO: DesenharAlucinacaoInimigo(e, escala, t);     break;
            case INIMIGO_VERBOSO:   DesenharVerboso(e, escala, t);         break;
            default:                DesenharCache(e, escala, t);           break;
        }
    }

    if (e->flash > 0.0f)
        EndBlendMode();

    if (e->selado && e->surgindo <= 0.0f)
        DesenharSelo(e, (float)GetTime());

    if (e->surgindo <= 0.0f)
        DesenharMarcasReformulacao(e, (float)GetTime());        // elite, alvo prioritário, rótulo

    // Barra de vida quando já levou dano (o Phisher camuflado não se entrega)
    bool semBarra = RogueInimigoPassageiro(e->tipo) || (e->tipo == INIMIGO_PHISHER && e->estado == 0) || e->tipo == INIMIGO_JANELA;

    if (e->hp < e->hpMax && !semBarra) {
        int w = (int)(e->raio * 2.4f);
        int x = (int)e->pos.x - w / 2, y = (int)(e->pos.y - e->raio - 12);
        DrawRectangle(x, y, w, 4, (Color){ 40, 30, 55, 255 });
        DrawRectangle(x, y, (int)(w * e->hp / e->hpMax), 4, DEF_INIMIGOS[e->tipo].cor);

        // Cache: a energia guardada (a cura que ele pode fazer) aparece embaixo
        if (e->tipo == INIMIGO_CACHE && e->energia > 0.0f)
            DrawRectangle(x, y + 5, (int)(w * e->energia / CACHE_ENERGIA_MAX), 2, (Color){ 0, 228, 255, 255 });
    }
}

// Ícone redondo com a recarga (Pulso / Esquiva)
void DesenharRecargaRogue(int x, int y, float falta, float total, const char *tecla, Color cor, bool disponivel)
{
    DrawCircle(x, y, 14, (Color){ 20, 16, 36, 255 });

    if (falta > 0.0f)
        DrawCircleSector((Vector2){ (float)x, (float)y }, 14, -90.0f, -90.0f + 360.0f * (1.0f - falta / total), 24, Fade(cor, 0.5f));
    else
        DrawCircle(x, y, 14, Fade(cor, disponivel ? 0.8f : 0.25f));

    DrawCircleLines(x, y, 14, cor);
    // Pronto (círculo cheio e claro): letra escura; recarregando (círculo escuro): letra branca
    bool cheio = falta <= 0.0f && disponivel;
    DrawText(tecla, x - MeasureText(tecla, 10) / 2, y - LinhaTexto(6), 10, cheio ? (Color){ 20, 16, 36, 255 } : WHITE);
}

// Tecla "E" sobre o que dá para usar
void DesenharTeclaE(Vector2 c, float tempo)
{
    int y = (int)(c.y + sinf(tempo * 5.0f) * 2.0f);
    DrawRectangle((int)c.x - 9, y - 9, 18, 18, Fade(BLACK, 0.7f));
    DrawRectangleLines((int)c.x - 9, y - 9, 18, 18, GOLD);
    DrawText("E", (int)c.x - 3, y - 6, 12, GOLD);
}

// "APERTE [E] PARA ENTRAR" (ou SAIR) sobre a cabeça do Thomas, perto de uma porta
void DesenharAvisoPorta(Vector2 pos, const char *acao, float tempo)
{
    const char *antes = "APERTE";
    const char *depois = TextFormat("PARA %s", acao);
    int wa = MeasureText(antes, 10), wd = MeasureText(depois, 10);
    int w = wa + wd + 40;
    int x = (int)Clamp(pos.x - w / 2.0f, 8.0f, (float)(LARGURA_TELA - w - 8));
    int y = (int)(pos.y - 52.0f + sinf(tempo * 4.0f) * 1.5f);

    DrawRectangle(x, y, w, 20, Fade((Color){ 16, 12, 30, 255 }, 0.85f));
    DrawRectangleLines(x, y, w, 20, Fade(GOLD, 0.7f));
    DrawText(antes, x + 8, y + 5, 10, LIGHTGRAY);
    DrawRectangle(x + 14 + wa, y + 3, 14, 14, Fade(GOLD, 0.2f));
    DrawRectangleLines(x + 14 + wa, y + 3, 14, 14, GOLD);
    DrawText("E", x + 18 + wa, y + 5, 10, GOLD);
    DrawText(depois, x + 32 + wa, y + 5, 10, WHITE);
}

// Púlpito da enciclopédia no átrio
void DesenharPulpitoEnciclopedia(Vector2 c, float t)
{
    DrawRectangle((int)c.x - 6, (int)c.y - 4, 12, 18, (Color){ 70, 45, 30, 255 });
    DrawRectangle((int)c.x - 16, (int)c.y + 12, 32, 6, (Color){ 70, 45, 30, 255 });

    // Livro aberto brilhando
    DrawCircleV((Vector2){ c.x, c.y - 10 }, 18 + sinf(t * 2.0f) * 2, Fade((Color){ 0, 228, 255, 255 }, 0.12f));
    DesenharTrianguloB2((Vector2){ c.x, c.y - 6 }, (Vector2){ c.x - 16, c.y - 12 }, (Vector2){ c.x - 16, c.y - 2 }, (Color){ 230, 225, 200, 255 });
    DesenharTrianguloB2((Vector2){ c.x, c.y - 6 }, (Vector2){ c.x + 16, c.y - 12 }, (Vector2){ c.x + 16, c.y - 2 }, (Color){ 230, 225, 200, 255 });
    DrawRectangle((int)c.x - 16, (int)c.y - 12, 32, 10, (Color){ 230, 225, 200, 255 });
    DrawLine((int)c.x, (int)c.y - 12, (int)c.x, (int)c.y - 2, (Color){ 120, 100, 80, 255 });

    const char *nome = "ENCICLOPÉDIA";
    DrawText(nome, (int)c.x - MeasureText(nome, 10) / 2, (int)c.y + 22, 10, (Color){ 0, 228, 255, 255 });
}

// Tela do desafio educativo (por cima da sala)
void DesenharDesafioRogue(const Biblioteca *b, float tempo)
{
    const DesafioEdu *d = &DESAFIOS[b->desafio];

    DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, 0.65f));

    Rectangle painel = { 60, 70, 680, 440 };
    DrawRectangleRec(painel, (Color){ 18, 15, 30, 255 });
    DrawRectangleLinesEx(painel, 4, GOLD);

    int x = (int)painel.x + 24, y = (int)painel.y + 16;

    DrawText("GUARDIÃO SELADO // DESAFIO", x, y, 12, GOLD);
    const char *tipo = TextFormat("%s   //   %s", NOMES_TIPOS_DESAFIO[d->tipo], NOMES_CONCEITOS[d->conceito]);
    DrawText(tipo, x, y + 18, 10, Fade(SKYBLUE, 0.9f));

    // Cronômetro: barra que esvazia e passa do verde ao amarelo e ao vermelho (sem degraus)
    {
        float frac = Clamp(b->tempoRespostaSelo / TEMPO_RESPOSTA_SELO, 0.0f, 1.0f);
        Color verde = { 0, 228, 48, 255 }, amarelo = { 253, 249, 0, 255 }, vermelho = { 230, 41, 55, 255 };
        Color corT = (frac > 0.5f) ? ColorLerp(amarelo, verde, (frac - 0.5f) * 2.0f) : ColorLerp(vermelho, amarelo, frac * 2.0f);
        bool critico = frac < 0.25f && !b->respondido;
        if (critico && ((int)(tempo * 6.0f)) % 2 == 0) corT = ColorBrightness(corT, 0.35f);

        Rectangle barra = { painel.x + 404, painel.y + 18, 240, 12 };
        DrawRectangleRec(barra, (Color){ 30, 24, 48, 255 });
        DrawRectangleRec((Rectangle){ barra.x, barra.y, barra.width * frac, barra.height }, corT);
        DrawRectangleLinesEx(barra, 1, Fade(GOLD, 0.6f));
        const char *seg = TextFormat("%d s", (int)ceilf(b->tempoRespostaSelo));
        DrawText(seg, (int)(barra.x + barra.width) + 8, (int)barra.y + 1, 10, critico ? corT : LIGHTGRAY);
        DrawText("TEMPO", (int)barra.x - MeasureText("TEMPO", 10) - 8, (int)barra.y + 1, 10, Fade(GOLD, 0.8f));
    }

    char linhas[512];
    QuebrarTexto(d->enunciado, (int)painel.width - 48, 18, linhas, (int)sizeof(linhas));
    int nl = DesenharTextoLinhas(linhas, x, y + 42, 18, 24, WHITE);
    y += 42 + nl * EspacoLinhaTexto(24, 18) + 14;

    // Opções
    for (int i = 0; i < d->numOpcoes; i++) {

        bool marcada = (b->selecaoDesafio == i) && !b->respondido;
        bool riscada = b->opcaoRiscada[i];
        Color cor = riscada ? Fade(GRAY, 0.5f) : (marcada ? YELLOW : LIGHTGRAY);

        if (b->respondido && i == d->correta)                       cor = LIME;
        else if (b->respondido && i == b->selecaoDesafio && !b->acertouDesafio && !b->tempoEsgotadoSelo) cor = RED;

        char op[256];
        QuebrarTexto(TextFormat("%d) %s", i + 1, d->opcoes[i]), (int)painel.width - 90, 16, op, (int)sizeof(op));

        if (marcada)
            DesenharCursorLuci((float)x + 8, (float)y + 8);

        int n = DesenharTextoLinhas(op, x + 26, y, 16, 20, cor);
        if (riscada)
            DrawLine(x + 26, y + 8, x + 26 + MeasureText(op, 16), y + 8, Fade(GRAY, 0.8f));
        y += n * EspacoLinhaTexto(20, 16) + 8;
    }

    // Dica (Lente, Luci ou Manual)
    if (b->dicaDesafio[0] != '\0' && !b->respondido) {

        Color corDica = b->verificouDica ? (b->luciMentiu ? RED : LIME) : (b->dicaDaLuci ? (Color){ 0, 228, 255, 255 } : GOLD);
        char dica[256];
        QuebrarTexto(b->dicaDesafio, (int)painel.width - 90, 14, dica, (int)sizeof(dica));

        if (b->dicaDaLuci) {
            Vector2 luci = { (float)x + 12, (float)y + 14 };
            DesenharLuciTamanho(luci, 26.0f);
            if (b->luciMentiu)
                DrawRectangle(x + 34 + GetRandomValue(-2, 2), y + 2, 4, 14, Fade(MAGENTA, 0.6f));
        }

        DesenharTextoLinhas(dica, x + 36, y + 6, 14, 18, corDica);
    }

    // Resposta e explicação
    if (b->respondido) {

        int yr = (int)(painel.y + painel.height) - 110;
        DrawLine(x, yr - 8, (int)(painel.x + painel.width) - 24, yr - 8, Fade(GOLD, 0.4f));

        DrawText(b->acertouDesafio ? "CERTO! O selo se quebra."
                 : (b->tempoEsgotadoSelo ? "TEMPO ESGOTADO! O guardião prepara um ataque especial."
                                         : "ERRADO! O guardião prepara um ataque especial."),
                 x, yr, 18, b->acertouDesafio ? LIME : RED);

        char exp[384];
        QuebrarTexto(d->explicacao, (int)painel.width - 48, 14, exp, (int)sizeof(exp));
        DesenharTextoLinhas(exp, x, yr + 26, 14, 18, WHITE);

        if (b->dicaDaLuci && b->luciMentiu)
            DrawText("A dica da Luci estava errada. Ela também pode alucinar!", x, yr + 70, 12, MAGENTA);

        if (((int)(GetTime() * 2.0)) % 2 == 0)
            DesenharDicaAvancar((int)(painel.x + painel.width) - 90, (int)(painel.y + painel.height) - 24, 12);
        return;
    }

    // Teclas de ajuda (também clicáveis)
    const char *lente = RogueTextoLenteDesafio(b);
    const char *luci  = RogueTextoLuciDesafio(b);
    const char *manual = (b->j.itens[ITEM_MANUAL] > 0) ? TextFormat("Manual: segunda chance (x%d)", b->j.itens[ITEM_MANUAL]) : "";

    int yb = Y_AJUDA_DESAFIO;
    DesenharTextoClicavel(lente, X_LENTE_DESAFIO, yb, 12, (b->j.itens[ITEM_LENTE] > 0) ? GOLD : GRAY);
    DesenharTextoClicavel(luci, X_LUCI_DESAFIO, yb, 12, b->dicaDaLuci ? GRAY : (Color){ 0, 228, 255, 255 });
    DrawText(manual, x + 500, yb, 12, GOLD);
    DrawText("W/S, 1-4 ou MOUSE: escolher    E/ENTER ou CLIQUE: responder", x, yb + 18, 10, GRAY);
    (void)tempo;
}

// Enciclopédia aberta no átrio
void DesenharEnciclopediaRogue(const Biblioteca *b)
{
    DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, 0.6f));

    Rectangle painel = { 110, 50, 580, 490 };       // cabem os 4 conceitos
    DrawRectangleRec(painel, (Color){ 18, 15, 30, 255 });
    DrawRectangleLinesEx(painel, 4, (Color){ 0, 228, 255, 255 });

    DrawText("ENCICLOPÉDIA DA BIBLIOTECA", (int)painel.x + 24, (int)painel.y + 18, 20, (Color){ 0, 228, 255, 255 });
    DrawText("O conhecimento descoberto nunca se perde.", (int)painel.x + 24, (int)painel.y + 44, 10, LIGHTGRAY);

    for (int c = 0; c < NUM_CONCEITOS; c++) {

        int y = (int)painel.y + 76 + c * 96;
        bool sabe = (b->perm->conceitos & (1u << c)) != 0;
        bool marcado = (b->selecaoEnciclopedia == c);

        if (marcado) {
            DrawRectangle((int)painel.x + 12, y - 6, (int)painel.width - 24, 88, Fade((Color){ 0, 228, 255, 255 }, 0.10f));
            DesenharCursorLuci(painel.x + 30, (float)y + 8);
        }

        DrawText(TextFormat("CONCEITO %d: %s", c + 1, sabe ? NOMES_CONCEITOS[c] : "???"), (int)painel.x + 50, y, 16,
                 sabe ? (marcado ? YELLOW : WHITE) : GRAY);

        char resumo[256];
        QuebrarTexto(sabe ? RESUMOS_CONCEITOS[c] : "Ainda não descoberto. Procure na Biblioteca.", (int)painel.width - 90, 14,
                     resumo, (int)sizeof(resumo));
        DesenharTextoLinhas(resumo, (int)painel.x + 50, y + 24, 14, 18, sabe ? LIGHTGRAY : Fade(GRAY, 0.7f));
    }

    DesenharBotaoFechar(RET_FECHAR_ENCICLOPEDIA, (Color){ 0, 228, 255, 255 });

    const char *ajuda = "W/S ou MOUSE escolher    E / CLIQUE ler o texto completo    BACKSPACE ou X sair";
    DrawText(ajuda, LARGURA_TELA / 2 - MeasureText(ajuda, 10) / 2, (int)(painel.y + painel.height) - LinhaTexto(20), 10, GRAY);
}

// Loja do Byteco por cima da sala: aba de itens (desta tentativa) e de melhorias (para sempre)
// (definidas mais abaixo, junto da tela de troca de arma)

void DesenharLojaRogue(const Biblioteca *b, float tempo)
{
    DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, 0.6f));

    Rectangle painel = { 150, 40, 500, 520 };
    DrawRectangleRec(painel, (Color){ 18, 15, 30, 255 });
    DrawRectangleLinesEx(painel, 4, GOLD);

    DrawText("LOJA DO BYTECO", (int)painel.x + 24, (int)painel.y + 18, 22, GOLD);

    const char *fc = TextFormat("SEUS FRAGMENTOS: %d", b->perm->fragmentos);
    DrawText(fc, (int)(painel.x + painel.width) - 24 - MeasureText(fc, 14), (int)painel.y + 24, 14, (Color){ 0, 228, 255, 255 });

    // Abas
    static const char *abas[3] = { "ITENS", "MELHORIAS (sempre)", "ARMAS" };
    for (int a = 0; a < 3; a++) {
        Rectangle r = { painel.x + 24 + a * 152, painel.y + 52, 146, 24 };
        bool ativa = (b->abaLoja == a);
        DrawRectangleRec(r, ativa ? Fade(GOLD, 0.25f) : Fade(GRAY, 0.1f));
        DrawRectangleLinesEx(r, 2, ativa ? GOLD : Fade(GRAY, 0.5f));
        DrawText(abas[a], (int)(r.x + r.width / 2) - MeasureText(abas[a], 12) / 2, (int)r.y + 6, 12, ativa ? GOLD : GRAY);
    }

    int linhas = (b->abaLoja == 0) ? NUM_ITENS_LOJA : ((b->abaLoja == 1) ? NUM_MELHORIAS : ARMAS_NA_LOJA);

    for (int i = 0; i <= linhas; i++) {

        int y = (int)painel.y + 90 + i * 46;
        bool marcado = (b->selecaoLoja == i);

        if (marcado)
            DrawRectangle((int)painel.x + 12, y - 4, (int)painel.width - 24, 40, Fade(GOLD, 0.12f));

        if (i == linhas) {
            DrawText("SAIR", (int)painel.x + 60, y + 8, 16, marcado ? YELLOW : LIGHTGRAY);
            if (marcado) DesenharCursorLuci(painel.x + 34, (float)y + 16);
            continue;
        }

        if (marcado) DesenharCursorLuci(painel.x + 24, (float)y + 16);

        if (b->abaLoja == 0) {

            const DefItem *it = &ITENS[i];
            int precoReal = PrecoItemLoja(b, i);
            bool cheio = it->passivo && b->j.itens[i] >= it->maxPilha;
            bool podePagar = !cheio && b->perm->fragmentos >= precoReal;
            bool oferta = (i == b->ofertaItem);

            DesenharIconeItem((TipoItem)i, (Vector2){ painel.x + 60, (float)y + 16 }, 1.0f, tempo);
            DrawText(it->nome, (int)painel.x + 84, y, 16, marcado ? YELLOW : WHITE);
            DrawText(it->efeito, (int)painel.x + 84, y + 19, 10, LIGHTGRAY);
            const char *preco = cheio ? "CHEIO" : TextFormat("%d FC", precoReal);
            int px = (int)(painel.x + painel.width) - 30 - MeasureText(preco, 16);

            // Selo de oferta: do lado do nome; se encostar no preço (nome longo, texto grande), vai para depois do efeito
            if (oferta && !cheio) {
                char selo[32];
                snprintf(selo, sizeof(selo), "-%d%% OFERTA", OFERTA_DESCONTO_PERC);
                int wSelo = MeasureText(selo, 8) + 8;
                int xAntes = px - MeasureText(TextFormat("%d", it->preco), 10) - 6;
                int xs = (int)painel.x + 94 + MeasureText(it->nome, 16), ys = y + 2;
                if (xs + wSelo > xAntes - 6) {
                    xs = (int)painel.x + 94 + MeasureText(it->efeito, 10);
                    ys = y + 18;
                }
                if (xs + wSelo <= (int)(painel.x + painel.width) - 96) {
                    DrawRectangle(xs, ys, wSelo, LinhaTexto(14), Fade(RED, 0.8f + 0.2f * sinf(tempo * 6.0f)));
                    DrawText(selo, xs + 4, ys + 1, 8, WHITE);
                }
            }
            DrawText(preco, px, y + 4, 16, cheio ? GOLD : (podePagar ? (Color){ 0, 228, 255, 255 } : RED));
            if (oferta && !cheio) {
                const char *antes = TextFormat("%d", it->preco);
                DrawText(antes, px - MeasureText(antes, 10) - 6, y + 7, 10, GRAY);
                DrawLine(px - MeasureText(antes, 10) - 6, y + 12, px - 6, y + 12, GRAY);
            }

            if (b->j.itens[i] > 0)
                DrawText(it->passivo ? TextFormat("tem %d/%d", b->j.itens[i], it->maxPilha) : TextFormat("tem %d", b->j.itens[i]), (int)(painel.x + painel.width) - 92, y + 22, 10, GRAY);

        } else if (b->abaLoja == 2) {

            // Armas desta volta ao átrio (a comprada vira a arma extra desta tentativa)
            TipoArma arma = b->estoqueArmas[i];
            const DefArma *a = &DEF_ARMAS[arma];
            bool sua = RogueTemArma(&b->j, arma);
            bool podePagar = b->perm->fragmentos >= a->preco;

            DesenharIconeArma(arma, (Vector2){ painel.x + 60, (float)y + 16 }, 1.0f, tempo);
            DrawText(a->nome, (int)painel.x + 84, y, 16, marcado ? YELLOW : WHITE);
            DrawText(TextFormat("%s - %s", NOMES_ALCANCE[a->alcanceTipo], a->efeito), (int)painel.x + 84, y + 19, 10, LIGHTGRAY);
            DrawText(NOMES_RARIDADE[a->raridade], (int)painel.x + 94 + MeasureText(a->nome, 16), y + 4, 10, CORES_RARIDADE[a->raridade]);

            const char *preco = sua ? "SUA" : TextFormat("%d FC", a->preco);
            DrawText(preco, (int)(painel.x + painel.width) - 30 - MeasureText(preco, 16), y + 4, 16,
                     sua ? GOLD : (podePagar ? (Color){ 0, 228, 255, 255 } : RED));

        } else {

            int nivel = PermNivel(b->perm, i);
            bool maximo = (nivel >= NIVEL_MAX_MELHORIA);

            DrawText(NOMES_MELHORIAS[i], (int)painel.x + 50, y, 16, marcado ? YELLOW : WHITE);
            DrawText(EFEITOS_MELHORIAS[i], (int)painel.x + 50, y + 19, 10, LIGHTGRAY);

            for (int k = 0; k < NIVEL_MAX_MELHORIA; k++) {
                Rectangle pip = { painel.x + 300 + k * 16, (float)y + 6, 12, 12 };
                if (k < nivel) DrawRectangleRec(pip, GOLD);
                DrawRectangleLinesEx(pip, 1, GOLD);
            }

            const char *preco = maximo ? "MÁXIMO" : TextFormat("%d FC", PRECO_MELHORIA[nivel]);
            bool podePagar = !maximo && b->perm->fragmentos >= PRECO_MELHORIA[nivel];
            DrawText(preco, (int)(painel.x + painel.width) - 30 - MeasureText(preco, 16), y + 4, 16,
                     maximo ? GOLD : (podePagar ? (Color){ 0, 228, 255, 255 } : RED));
        }
    }

    // Detalhes do que está marcado (aba dos itens)
    if (b->abaLoja == 0 && b->selecaoLoja < NUM_ITENS_LOJA) {
        const DefItem *it = &ITENS[b->selecaoLoja];
        Rectangle caixa = { painel.x + 16, painel.y + 90 + (float)(NUM_ITENS_LOJA + 1) * 46 + 2, painel.width - 32, 58 };
        DrawRectangleRec(caixa, (Color){ 28, 24, 46, 255 });
        DrawRectangleLinesEx(caixa, 1, Fade(GOLD, 0.6f));
        DrawText(it->nome, (int)caixa.x + 8, (int)caixa.y + 6, 12, GOLD);
        DrawText(it->passivo ? TextFormat("PASSIVO: vale enquanto estiver na mochila (máx. %d)", it->maxPilha)
                             : (it->temporario ? "TEMPORÁRIO: vale na hora em que é pego" : "CONSUMÍVEL: use pela tecla indicada no efeito"),
                 (int)caixa.x + 8, (int)caixa.y + 22, 10, LIGHTGRAY);
        {   // sem espaço (texto grande): fica só a oferta, sem o "muda a cada volta"
            const char *oferta = TextFormat("Oferta do Byteco: %s (-%d%%) — muda a cada volta ao átrio.", ITENS[b->ofertaItem].nome, OFERTA_DESCONTO_PERC);
            if (MeasureText(oferta, 10) > (int)caixa.width - 16)
                oferta = TextFormat("Oferta do Byteco: %s (-%d%%)", ITENS[b->ofertaItem].nome, OFERTA_DESCONTO_PERC);
            DrawText(oferta, (int)caixa.x + 8, (int)caixa.y + 38, 10, (Color){ 255, 120, 120, 255 });
        }
    }

    if (b->abaLoja == 1 && b->selecaoLoja < NUM_MELHORIAS) {
        int m = b->selecaoLoja, nv = PermNivel(b->perm, m);
        Rectangle caixa = { painel.x + 16, painel.y + 90 + (float)(NUM_MELHORIAS + 1) * 46 + 2, painel.width - 32, 58 };
        DrawRectangleRec(caixa, (Color){ 28, 24, 46, 255 });
        DrawRectangleLinesEx(caixa, 1, Fade(GOLD, 0.6f));
        DrawText(NOMES_MELHORIAS[m], (int)caixa.x + 8, (int)caixa.y + 6, 12, GOLD);
        DrawText(TextFormat("AGORA: %s", (nv > 0) ? TextoEfeitoMelhoria(m, nv) : "nada ainda"), (int)caixa.x + 8, (int)caixa.y + 22, 10, LIGHTGRAY);
        DrawText((nv >= NIVEL_MAX_MELHORIA) ? "NÍVEL MÁXIMO: vale para sempre, em todas as tentativas."
                                            : TextFormat("PRÓXIMO NÍVEL: %s", TextoEfeitoMelhoria(m, nv + 1)),
                 (int)caixa.x + 8, (int)caixa.y + 38, 10, (nv >= NIVEL_MAX_MELHORIA) ? GOLD : (Color){ 120, 255, 160, 255 });
    }

    // Detalhes da arma marcada: números, comparação com a que você está usando e o que vai acontecer ao comprar
    if (b->abaLoja == 2 && b->selecaoLoja < ARMAS_NA_LOJA) {
        TipoArma arma = b->estoqueArmas[b->selecaoLoja];
        TipoArma atual = RogueArmaAtual(&b->j);
        const DefArma *a = &DEF_ARMAS[arma];
        Rectangle caixa = { painel.x + 16, painel.y + 90 + (float)(ARMAS_NA_LOJA + 1) * 46 + 2, painel.width - 32, 150 };
        DrawRectangleRec(caixa, (Color){ 28, 24, 46, 255 });
        DrawRectangleLinesEx(caixa, 1, Fade(a->cor, 0.8f));
        DesenharIconeArma(arma, (Vector2){ caixa.x + 34, caixa.y + 44 }, 1.6f, tempo);
        DrawText(a->nome, (int)caixa.x + 70, (int)caixa.y + 8, 14, a->cor);
        DrawText(TextFormat("%s  -  %s", NOMES_RARIDADE[a->raridade], NOMES_ALCANCE[a->alcanceTipo]), (int)caixa.x + 70, (int)caixa.y + 26, 10, CORES_RARIDADE[a->raridade]);
        char linhas[256];
        QuebrarTexto(a->efeito, 190, 10, linhas, (int)sizeof(linhas));
        DesenharTextoLinhas(linhas, (int)caixa.x + 12, (int)caixa.y + 76, 10, 12, LIGHTGRAY);

        char txt[40];
        float bx = caixa.x + 230, bw = caixa.width - 240;
        DrawText(TextFormat("comparado com a sua: %s", DEF_ARMAS[atual].nome), (int)bx, (int)caixa.y + 8, 9, GRAY);
        snprintf(txt, sizeof(txt), "%.0f", DpsArma(arma));
        DesenharBarraArma("DANO/s",  bx, caixa.y + 28, bw, DpsArma(arma), 110.0f, txt, a->cor, DpsArma(atual));
        snprintf(txt, sizeof(txt), "%.1f/s", 1.0f / a->intervalo);
        DesenharBarraArma("CADÊNCIA", bx, caixa.y + 44, bw, 1.0f / a->intervalo, 6.0f, txt, a->cor, 1.0f / DEF_ARMAS[atual].intervalo);
        DesenharBarraArma("ALCANCE", bx, caixa.y + 60, bw, AlcanceArma(arma), 1.0f, "", a->cor, AlcanceArma(atual));

        char ups[200];
        RogueMelhoriasDaArma(&b->j, arma, ups, (int)sizeof(ups));
        float yExtra = caixa.y + 82;                              // a próxima linha começa depois do texto anterior
        if (ups[0]) {
            QuebrarTexto(TextFormat("Suas melhorias para ela voltam a valer: %s", ups), (int)bw, 9, linhas, (int)sizeof(linhas));
            int nl = DesenharTextoLinhas(linhas, (int)bx, (int)yExtra, 9, 11, (Color){ 120, 255, 160, 255 });
            yExtra += (float)(nl * EspacoLinhaTexto(11, 9) + 4);
        }
        if (arma == ARMA_RAIO || arma == ARMA_RAIO_CLASSICO) {
            QuebrarTexto((arma == ARMA_RAIO) ? "Gasta EP a cada sigilo. Acertar devolve EP." : "Gasta EP a cada disparo (sem EP, não atira).", (int)bw, 9, linhas, (int)sizeof(linhas));
            DesenharTextoLinhas(linhas, (int)bx, (int)yExtra, 9, 11, ORANGE);
        }

        const char *aviso = RogueTemArma(&b->j, arma) ? "Você já tem esta arma."
                          : ((b->j.armaExtra != ARMA_DISPARADOR) ? "Vagas cheias: você escolhe qual arma trocar (pagando só ao confirmar)."
                                                                 : "Vai para a sua 2ª vaga (TAB troca entre as duas).");
        DrawText(aviso, (int)caixa.x + 12, (int)(caixa.y + caixa.height) - 18, 10, YELLOW);
    }

    const char *ajuda = "A/D aba    W/S ou MOUSE escolher    E / CLIQUE comprar    BACKSPACE sair";
    DrawText(ajuda, LARGURA_TELA / 2 - MeasureText(ajuda, 10) / 2, (int)(painel.y + painel.height) - 20, 10, GRAY);
}

// O Alucinador: um núcleo de dados corrompidos com um olho que segue o jogador,
// páginas girando em volta e uma coroa de glifos. Na fase 2, o escudo de distorção.
void DesenharAlucinador(const ChefeRogue *c, Vector2 jogador, float t)
{
    float k = (c->morrendo > 0.0f) ? c->morrendo / 1.6f : 1.0f;     // some ao ser derrotado
    float r = CHEFE_RAIO * (0.6f + 0.4f * k);
    Vector2 p = { c->pos.x, c->pos.y + sinf(t * 1.8f) * 4.0f };

    // Investida: o corredor enche; nos últimos 0,2 s a mira trava (bordas brancas e setas)
    if (c->padrao == PADRAO_INVESTIDA && c->etapaInvestida == 0 && c->morrendo <= 0.0f)
        DesenharCorredorAvisoIni(p, c->dirInvestida, CHEFE_INVESTIDA_VEL * 0.45f + CHEFE_RAIO, CHEFE_RAIO * 0.8f,
                                 1.0f - c->tempoPadrao / 0.8f, c->tempoPadrao <= 0.2f, RED, t);

    // Leque: as linhas da próxima rajada (seguem o Thomas até o disparo)
    if (c->padrao == PADRAO_LEQUE && c->rajadas > 0 && c->morrendo <= 0.0f) {
        int n = 5 + 2 * c->surto;
        float passo = 12.0f * DEG2RAD;
        Vector2 d = Vector2Subtract(jogador, p);
        float base = atan2f(d.y, d.x) - passo * (float)(n - 1) / 2.0f;
        DesenharLinhasBalasIni(p, CHEFE_RAIO, n, base, passo, 1.0f - c->cronoTiro / 0.6f, MAGENTA, t);
    }

    DrawEllipse((int)c->pos.x, (int)c->pos.y + (int)CHEFE_RAIO + 8, r, 8, Fade(BLACK, 0.4f));

    for (int vez = 0; vez < ((c->flash > 0.0f) ? 3 : 1); vez++) {

        if (vez == 1) BeginBlendMode(BLEND_ADDITIVE);

        // Páginas girando
        for (int i = 0; i < 6; i++) {
            float a = t * 1.3f + i * (PI / 3.0f);
            Vector2 q = { p.x + cosf(a) * (r + 26), p.y + sinf(a) * (r + 12) };
            DrawRectangle((int)q.x - 7, (int)q.y - 9, 14, 18, Fade((Color){ 230, 225, 200, 255 }, 0.8f * k));
            DrawRectangle((int)q.x - 4, (int)q.y - 4, 8, 1, Fade(MAGENTA, k));
            DrawRectangle((int)q.x - 4, (int)q.y, 8, 1, Fade(MAGENTA, k));
        }

        // Corpo em três camadas deslocadas (glitch)
        Color cores[3] = { Fade(RED, 0.45f * k), Fade(SKYBLUE, 0.45f * k), Fade((Color){ 60, 10, 80, 255 }, k) };
        float desloc[3] = { -4.0f + sinf(t * 13.0f) * 2.0f, 4.0f, 0.0f };
        for (int i = 0; i < 3; i++)
            DrawCircleV((Vector2){ p.x + desloc[i], p.y }, r, cores[i]);
        DrawCircleLines((int)p.x, (int)p.y, r, Fade(MAGENTA, k));

        // Olho que segue o jogador
        Vector2 olhar = Vector2Normalize(Vector2Subtract(jogador, p));
        DrawEllipse((int)p.x, (int)p.y - 4, r * 0.55f, r * 0.35f, Fade(WHITE, k));
        DrawCircleV((Vector2){ p.x + olhar.x * 8, p.y - 4 + olhar.y * 5 }, r * 0.22f, Fade(MAGENTA, k));
        DrawCircleV((Vector2){ p.x + olhar.x * 9, p.y - 4 + olhar.y * 6 }, r * 0.1f, Fade(BLACK, k));

        // Coroa de glifos
        const char *glifos[5] = { "?", "!", "#", "404", "01" };
        for (int i = 0; i < 5; i++) {
            int gx = (int)(p.x - 40 + i * 20), gy = (int)(p.y - r - 22 + sinf(t * 3 + i) * 3);
            DrawText(glifos[i], gx - MeasureText(glifos[i], 12) / 2, gy, 12, Fade(MAGENTA, k));
        }
    }

    if (c->flash > 0.0f)
        EndBlendMode();

    // Fase 2: escudo de distorção (fechado) ou aberto, com o tempo que falta
    if (c->fase == 2 && c->morrendo <= 0.0f) {
        if (c->janela <= 0.0f) {
            for (int i = 0; i < 6; i++) {
                float a1 = -t * 0.8f + i * (PI / 3.0f), a2 = a1 + PI / 3.0f;
                float rs = r + 30;
                DrawLineEx((Vector2){ p.x + cosf(a1) * rs, p.y + sinf(a1) * rs },
                           (Vector2){ p.x + cosf(a2) * rs, p.y + sinf(a2) * rs }, 4, Fade(MAGENTA, 0.75f));
            }
            DrawCircleV(p, r + 30, Fade(MAGENTA, 0.10f + 0.05f * sinf(t * 6.0f)));
        } else {
            float falta = c->janela / CHEFE_JANELA;
            DrawRing(p, r + 26, r + 30, -90.0f, -90.0f + 360.0f * falta, 32, Fade(GOLD, 0.9f));
        }
    }
}

// Barra de vida do Alucinador (no alto, sobre a estante)
void DesenharBarraChefe(const ChefeRogue *c)
{
    int x = 200, y = 44, w = 400;

    DrawRectangle(x - 4, y - 2, w + 8, 16, Fade(BLACK, 0.8f));
    DrawRectangle(x, y + 2, w, 8, (Color){ 40, 20, 50, 255 });
    DrawRectangle(x, y + 2, (int)(w * c->hp / CHEFE_HP), 8, (c->fase == 1) ? MAGENTA : (c->janela > 0.0f ? GOLD : PURPLE));
    DrawRectangle(x + (int)(w * CHEFE_FASE2) - 1, y, 2, 12, WHITE);          // onde começa a fase 2
    DrawRectangleLines(x, y + 2, w, 8, GRAY);
    int wNome = MeasureText("ALUCINADOR", 10);
    DrawRectangle(x - wNome - 18, y - 2, wNome + 12, 16, Fade(BLACK, 0.8f));
    DrawText("ALUCINADOR", x - wNome - 12, y + 1, 10, MAGENTA);
}

// Faixa das afirmações (fase 2): o combate continua por baixo
void DesenharFaixaAfirmacao(const Biblioteca *b, float tempo)
{
    const ChefeRogue *c = &b->chefe;

    if (c->fase != 2 || c->afirmacaoAtual < 0 || c->morrendo > 0.0f)
        return;

    const AfirmacaoChefe *a = &AFIRMACOES_ALUCINADOR[c->afirmacaoAtual];
    Rectangle faixa = { 100, 66, 600, 100 };

    DrawRectangleRec(faixa, Fade((Color){ 18, 15, 30, 255 }, 0.88f));
    DrawRectangleLinesEx(faixa, 2, c->mostrandoResultado ? (c->acertouAfirm ? LIME : RED) : MAGENTA);

    int x = (int)faixa.x + 12, y = (int)faixa.y + 8;

    if (c->mostrandoResultado) {
        DrawText(c->acertouAfirm ? "CERTO! O escudo caiu." : "ERRADO! Ataque especial!", x, y, 16, c->acertouAfirm ? LIME : RED);
        char exp[256];
        QuebrarTexto(a->explicacao, (int)faixa.width - 24, 14, exp, (int)sizeof(exp));
        DesenharTextoLinhas(exp, x, y + 24, 14, 18, WHITE);
        return;
    }

    if (a->antes != NULL) {
        DrawText(TextFormat("ANTES ELE DISSE: \"%s\"", a->antes), x, y, 10, LIGHTGRAY);
        y += 14;
    }

    char frase[256];
    QuebrarTexto(TextFormat("ALUCINADOR: \"%s\"", a->texto), (int)faixa.width - 24, 16, frase, (int)sizeof(frase));
    int nl = DesenharTextoLinhas(frase, x, y, 16, 19, WHITE);
    y += nl * EspacoLinhaTexto(19, 16) + 2;

    if (c->dicaAfirm[0] != '\0') {
        Color cor = c->verificouAfirm ? (c->luciMentiuAfirm ? RED : LIME) : (c->dicaAfirmLuci ? (Color){ 0, 228, 255, 255 } : GOLD);
        DrawText(c->dicaAfirm, x, y, 10, cor);
    }

    // Tempo e teclas
    float resta = Clamp(c->tempoAfirm / TEMPO_AFIRMACAO, 0.0f, 1.0f);
    DrawRectangle((int)faixa.x, (int)(faixa.y + faixa.height) - 4, (int)(faixa.width * resta), 4, (resta > 0.3f) ? GOLD : RED);

    const char *teclas = "C = CONFIÁVEL    F = PROBLEMÁTICA    L = Lente    H = Luci    V = verificar";
    DrawText(teclas, (int)(faixa.x + faixa.width / 2) - MeasureText(teclas, 12) / 2, (int)(faixa.y + faixa.height) - 20, 12,
             Fade(GOLD, 0.75f + 0.25f * sinf(tempo * 5.0f)));
}
