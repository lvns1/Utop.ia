#include "battle.h"
#include "../utils/text.h"
#include "../utils/effects.h"
#include "raymath.h"
#include <string.h>
#include <math.h>

static void BatalhaTexto(Batalha *b, const char *texto) {
    QuebrarTexto(texto, 640, FONTE_BATALHA, b->texto, (int)sizeof(b->texto));
    b->visiveis = 0.0f;
}

void BatalhaIniciar(Batalha *b, TipoBatalha tipo, int quiz, int nivelAlucinacao) {
    memset(b, 0, sizeof(*b));

    b->tipo = tipo;
    b->quiz = quiz;
    b->fase = FASE_INTRO;
    b->hp = HP_MAX_BATALHA;

    int nivel = (nivelAlucinacao > 10) ? 10 : nivelAlucinacao;
    float forca = 1.0f + 0.06f * (float)nivel;

    if (tipo == BAT_COMBATE) {
        b->duracao = DURACAO_COMBATE;
        b->intervaloBala = 0.20f / forca;
        b->velocidadeBala = 150.0f * forca;
        b->dano = 3;
        b->padrao = 0;

        BatalhaTexto(b,
            "* O tempo acabou... A ALUCINAÇÃO saiu do monitor e bloqueia o seu caminho!\n"
            "* Desvie dos dados corrompidos até ela perder a força.");
    } else {
        b->duracao = DURACAO_MINIGAME;
        b->intervaloBala = 0.32f / forca;
        b->velocidadeBala = 125.0f * forca;
        b->dano = 3;
        b->padrao = quiz % 3;

        BatalhaTexto(b, TextFormat(
            "* Resposta errada! Um fragmento de alucinação escapou do monitor %d.\n"
            "* Desvie dos dados corrompidos!", quiz + 1));
    }
}

void BatalhaCriarBala(Batalha *b, Rectangle c) {
    static const char glifos[] = "01?!#%";
    int livre = -1;

    for (int i = 0; i < MAX_BALAS; i++) {
        if (!b->balas[i].ativa) {
            livre = i;
            break;
        }
    }
    if (livre < 0) return;

    Bala *bl = &b->balas[livre];
    float v = b->velocidadeBala * (0.85f + GetRandomValue(0, 30) / 100.0f);

    int xMin = (int)c.x + 12, xMax = (int)(c.x + c.width) - 12;
    int yMin = (int)c.y + 12, yMax = (int)(c.y + c.height) - 12;

    bl->ativa = true;
    bl->glifo = glifos[GetRandomValue(0, (int)sizeof(glifos) - 2)];

    switch (b->padrao) {
        case 0: {
            bl->pos = (Vector2){ (float)GetRandomValue(xMin, xMax), c.y - 12 };
            bl->vel = (Vector2){ 0, v };
        } break;

        case 1: {
            bool daEsquerda = (GetRandomValue(0, 1) == 0);
            bl->pos = (Vector2){ daEsquerda ? c.x - 12 : c.x + c.width + 12, (float)GetRandomValue(yMin, yMax) };
            bl->vel = (Vector2){ daEsquerda ? v : -v, 0 };
        } break;

        default: {
            Vector2 p;
            switch (GetRandomValue(0, 3)) {
                case 0:  p = (Vector2){ (float)GetRandomValue(xMin, xMax), c.y - 12 }; break;
                case 1:  p = (Vector2){ (float)GetRandomValue(xMin, xMax), c.y + c.height + 12 }; break;
                case 2:  p = (Vector2){ c.x - 12, (float)GetRandomValue(yMin, yMax) }; break;
                default: p = (Vector2){ c.x + c.width + 12, (float)GetRandomValue(yMin, yMax) }; break;
            }
            bl->pos = p;
            bl->vel = Vector2Scale(Vector2Normalize(Vector2Subtract(b->alma, p)), v * 0.9f);
        } break;
    }
}

bool BatalhaAtualizar(Batalha *b) {
    float dt = GetFrameTime();
    bool avancar = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_Z);

    int total = (int)strlen(b->texto);
    if (b->visiveis < (float)total) {
        b->visiveis += VELOCIDADE_TEXTO * dt;
        if (b->visiveis > (float)total)
            b->visiveis = (float)total;
    }
    bool terminou = (b->visiveis >= (float)total);

    switch (b->fase) {
        case FASE_INTRO: {
            if (avancar) {
                if (!terminou) b->visiveis = (float)total;
                else           b->fase = FASE_ENCOLHENDO;
            }
        } break;

        case FASE_ENCOLHENDO: {
            b->anim += dt / DURACAO_ANIM_CAIXA;
            if (b->anim >= 1.0f) {
                b->anim = 1.0f;
                b->fase = FASE_DESVIO;
                b->alma = (Vector2){ CAIXA_DESVIO.x + CAIXA_DESVIO.width / 2, CAIXA_DESVIO.y + CAIXA_DESVIO.height / 2 };
            }
        } break;

        case FASE_DESVIO: {
            Rectangle c = CAIXA_DESVIO;
            b->tempoDesvio += dt;

            if (b->invencivel > 0.0f) b->invencivel -= dt;
            if (b->tremor > 0.0f)     b->tremor -= dt;

            Vector2 dir = {0};
            if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    dir.y -= 1.0f;
            if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  dir.y += 1.0f;
            if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  dir.x -= 1.0f;
            if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) dir.x += 1.0f;

            if (Vector2Length(dir) > 0.0f)
                b->alma = Vector2Add(b->alma, Vector2Scale(Vector2Normalize(dir), VELOCIDADE_ALMA * dt));

            float margem = 12.0f;
            b->alma.x = Clamp(b->alma.x, c.x + margem, c.x + c.width - margem);
            b->alma.y = Clamp(b->alma.y, c.y + margem, c.y + c.height - margem);

            if (b->tipo == BAT_COMBATE)
                b->padrao = ((int)(b->tempoDesvio / (b->duracao / 3.0f))) % 3;

            b->cronoBala -= dt;
            if (b->cronoBala <= 0.0f) {
                b->cronoBala += b->intervaloBala;
                BatalhaCriarBala(b, c);
            }

            for (int i = 0; i < MAX_BALAS; i++) {
                Bala *bl = &b->balas[i];
                if (!bl->ativa) continue;

                bl->pos = Vector2Add(bl->pos, Vector2Scale(bl->vel, dt));

                if (bl->pos.x < c.x - 30 || bl->pos.x > c.x + c.width + 30 ||
                    bl->pos.y < c.y - 30 || bl->pos.y > c.y + c.height + 30) {
                    bl->ativa = false;
                    continue;
                }

                if (b->invencivel <= 0.0f &&
                    Vector2Distance(bl->pos, b->alma) < RAIO_BALA + RAIO_ALMA) {
                    b->hp -= b->dano;
                    b->invencivel = TEMPO_INVENCIVEL;
                    b->tremor = 0.2f;
                    b->danosRecebidos++;
                    bl->ativa = false;
                }
            }

            if (b->hp <= 0) {
                b->hp = 0;
                b->derrota = true;
            }

            if (b->derrota || b->tempoDesvio >= b->duracao) {
                for (int i = 0; i < MAX_BALAS; i++)
                    b->balas[i].ativa = false;
                b->fase = FASE_EXPANDINDO;
            }
        } break;

        case FASE_EXPANDINDO: {
            b->anim -= dt / DURACAO_ANIM_CAIXA;
            if (b->anim <= 0.0f) {
                b->anim = 0.0f;
                b->fase = FASE_FIM;

                if (b->derrota)
                    BatalhaTexto(b,
                        "* A alucinação te dominou por um instante...\n"
                        "* No Bloco 1 você não perde vidas, mas ela ficou mais forte.");
                else if (b->danosRecebidos == 0)
                    BatalhaTexto(b,
                        "* Esquiva perfeita! A alucinação se desfaz em ruído.\n"
                        "* Luci quer conversar sobre a resposta.");
                else
                    BatalhaTexto(b,
                        "* Você resistiu! A alucinação se desfaz em ruído.\n"
                        "* Luci quer conversar sobre a resposta.");
            }
        } break;

        case FASE_FIM: {
            if (avancar) {
                if (!terminou) b->visiveis = (float)total;
                else           return true;
            }
        } break;

        default: break;
    }
    return false;
}

static void DesenharAlucinacao(Vector2 centro, float tempo, bool forte) {
    float s = forte ? 1.25f : 0.9f;
    Vector2 p = { centro.x, centro.y + sinf(tempo * 2.0f) * 6.0f };

    Color cores[3] = { Fade(RED, 0.45f), Fade(SKYBLUE, 0.45f), (Color){ 200, 170, 255, 255 } };
    float desloc[3] = { -4.0f, 4.0f, 0.0f };

    for (int k = 0; k < 3; k++) {
        float cx = p.x + desloc[k] * (forte ? 1.5f : 1.0f);
        DrawCircle((int)cx, (int)(p.y - 10 * s), 42 * s, cores[k]);
        DrawRectangle((int)(cx - 42 * s), (int)(p.y - 10 * s), (int)(84 * s), (int)(50 * s), cores[k]);

        for (int j = 0; j < 5; j++)
            DrawCircle((int)(cx - 34 * s + j * 17 * s), (int)(p.y + 40 * s + sinf(tempo * 6.0f + j) * 3.0f), 9 * s, cores[k]);
    }

    DrawCircle((int)(p.x - 15 * s), (int)(p.y - 14 * s), 7 * s, BLACK);
    DrawCircle((int)(p.x + 15 * s), (int)(p.y - 14 * s), 7 * s, BLACK);

    if (forte) {
        DrawCircle((int)(p.x - 15 * s), (int)(p.y - 14 * s), 2.5f * s, RED);
        DrawCircle((int)(p.x + 15 * s), (int)(p.y - 14 * s), 2.5f * s, RED);
    }

    for (int j = 0; j < 5; j++) {
        Vector2 a = { p.x - 18 * s + j * 7.2f * s,       p.y + 8 * s + (j % 2) * 6 * s };
        Vector2 b = { p.x - 18 * s + (j + 1) * 7.2f * s, p.y + 8 * s + ((j + 1) % 2) * 6 * s };
        DrawLineEx(a, b, 2, BLACK);
    }

    unsigned int quadro = (unsigned int)(tempo * 10.0f);
    int fatias = forte ? 5 : 3;

    for (int k = 0; k < fatias; k++) {
        unsigned int r = Ruido(quadro * 31u + (unsigned int)k);
        if ((r & 3u) == 0) continue;

        int fy = (int)(p.y - 50 * s) + (int)(r % (unsigned int)(100 * s));
        int fx = (int)(p.x - 50 * s) + (int)((r >> 8) % 21) - 10;
        DrawRectangle(fx, fy, (int)(100 * s), 3, Fade((k % 2) ? SKYBLUE : MAGENTA, 0.55f));
    }

    const char *simbolos[4] = { "?", "404", "01", "!" };
    for (int j = 0; j < 4; j++) {
        float ang = tempo * 1.2f + j * (PI / 2.0f);
        int sx = (int)(p.x + cosf(ang) * 85 * s);
        int sy = (int)(p.y + sinf(ang) * 30 * s);
        DrawText(simbolos[j], sx - MeasureText(simbolos[j], 14) / 2, sy - 7, 14, Fade(WHITE, 0.6f));
    }
}

void BatalhaDesenhar(const Batalha *b, const char *nickname, float tempo) {
    ClearBackground(BLACK);
    bool forte = (b->tipo == BAT_COMBATE);
    const char *nome = forte ? "ALUCINAÇÃO" : "FRAGMENTO DE ALUCINAÇÃO";

    DrawText(forte ? "COMBATE" : "MINIGAME", 20, 16, 10, forte ? RED : ORANGE);
    DrawText(nome, LARGURA_TELA / 2 - MeasureText(nome, 14) / 2, 34, 14, GRAY);

    DesenharAlucinacao((Vector2){ LARGURA_TELA / 2.0f, 150.0f }, tempo, forte);

    float t = b->anim * b->anim * (3.0f - 2.0f * b->anim);
    Rectangle caixa = {
        Lerp(CAIXA_TEXTO_BATALHA.x, CAIXA_DESVIO.x, t),
        Lerp(CAIXA_TEXTO_BATALHA.y, CAIXA_DESVIO.y, t),
        Lerp(CAIXA_TEXTO_BATALHA.width, CAIXA_DESVIO.width, t),
        Lerp(CAIXA_TEXTO_BATALHA.height, CAIXA_DESVIO.height, t)
    };

    if (b->tremor > 0.0f) {
        caixa.x += (float)GetRandomValue(-3, 3);
        caixa.y += (float)GetRandomValue(-3, 3);
    }

    DrawRectangleRec(caixa, BLACK);
    DrawRectangleLinesEx(caixa, 4, WHITE);

    if (b->fase == FASE_INTRO || b->fase == FASE_FIM) {
        char parcial[512];
        bool terminou = TextoParcial(b->texto, b->visiveis, parcial, (int)sizeof(parcial));
        DesenharTextoLinhas(parcial, (int)caixa.x + 24, (int)caixa.y + 22, FONTE_BATALHA, 30, WHITE);

        if (terminou && (((int)(GetTime() * 2.0)) % 2 == 0))
            DrawText("ENTER >", (int)(caixa.x + caixa.width - 90), (int)(caixa.y + caixa.height - 26), 12, GOLD);
    }

    if (b->fase == FASE_DESVIO) {
        BeginScissorMode((int)caixa.x + 4, (int)caixa.y + 4, (int)caixa.width - 8, (int)caixa.height - 8);
        for (int i = 0; i < MAX_BALAS; i++) {
            DesenharBala(&b->balas[i]);
        }
        if (b->invencivel <= 0.0f || (((int)(b->invencivel * 10.0f)) % 2 == 0))
            DesenharCoracao(b->alma.x, b->alma.y, 2, RED);
        EndScissorMode();

        float resta = 1.0f - b->tempoDesvio / b->duracao;
        if (resta < 0.0f) resta = 0.0f;
        DrawRectangle((int)caixa.x, (int)(caixa.y + caixa.height + 6), (int)(caixa.width * resta), 3, Fade(WHITE, 0.5f));
    }

    int yStatus = 452;
    DrawText(nickname, 60, yStatus, 20, WHITE);
    DrawText("LV 1", 150, yStatus, 20, WHITE);
    DrawText("HP", 250, yStatus + 4, 12, WHITE);

    DrawRectangle(275, yStatus, HP_MAX_BATALHA * 4, 20, RED);
    DrawRectangle(275, yStatus, b->hp * 4, 20, YELLOW);
    DrawText(TextFormat("%d / %d", b->hp, HP_MAX_BATALHA), 275 + HP_MAX_BATALHA * 4 + 14, yStatus, 20, WHITE);

    const char *instrucao = (b->fase == FASE_DESVIO)
        ? "W A S D ou SETAS  -  mova o coração e desvie!"
        : "ENTER  -  continuar";
    DrawText(instrucao, LARGURA_TELA / 2 - MeasureText(instrucao, 12) / 2, 540, 12, GRAY);
}
