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

// ---------- Desenho da Câmara da Inferência ----------

// Escreve uma fórmula lógica. A fonte padrão não tem os conectivos, então eles são
// desenhados com linhas:  & = ∧ (E)   | = ∨ (OU)   ~ = ¬ (NÃO)   > = → (SE...ENTÃO)   # = ↔ (SE E SÓ SE)
// Com desenhar = false, só mede. Devolve a largura em pixels.
static int DesenharLogica(const char *f, int x, int y, int tam, Color cor, bool desenhar)
{
    char trecho[128];
    int n = 0, cx = x;
    float g = fmaxf(1.0f, tam / 9.0f);

    for (const char *c = f; ; c++) {

        bool simbolo = (*c == '&' || *c == '|' || *c == '~' || *c == '>' || *c == '#');

        if (!simbolo && *c != '\0') {
            if (n < (int)sizeof(trecho) - 1) trecho[n++] = *c;
            continue;
        }

        if (n > 0) {
            trecho[n] = '\0';
            if (desenhar) DrawText(trecho, cx, y, tam, cor);
            cx += MeasureText(trecho, tam) + tam / 10;
            n = 0;
        }

        if (*c == '\0')
            break;

        float w = tam * 0.7f;
        float x0 = (float)cx, x1 = cx + w, xm = cx + w / 2.0f;
        float yt = y + tam * 0.15f, ym = y + tam * 0.5f, yb = y + tam * 0.85f, ponta = tam * 0.28f;

        if (desenhar) switch (*c) {
            case '&':
                DrawLineEx((Vector2){ x0, yb }, (Vector2){ xm, yt }, g, cor);
                DrawLineEx((Vector2){ xm, yt }, (Vector2){ x1, yb }, g, cor);
                break;
            case '|':
                DrawLineEx((Vector2){ x0, yt }, (Vector2){ xm, yb }, g, cor);
                DrawLineEx((Vector2){ xm, yb }, (Vector2){ x1, yt }, g, cor);
                break;
            case '~':
                DrawLineEx((Vector2){ x0, ym }, (Vector2){ x1, ym }, g, cor);
                DrawLineEx((Vector2){ x1, ym - g / 2 }, (Vector2){ x1, ym + tam * 0.3f }, g, cor);
                break;
            case '>':
                DrawLineEx((Vector2){ x0, ym }, (Vector2){ x1, ym }, g, cor);
                DrawLineEx((Vector2){ x1, ym }, (Vector2){ x1 - ponta, ym - ponta }, g, cor);
                DrawLineEx((Vector2){ x1, ym }, (Vector2){ x1 - ponta, ym + ponta }, g, cor);
                break;
            default:    // '#'
                DrawLineEx((Vector2){ x0, ym }, (Vector2){ x1, ym }, g, cor);
                DrawLineEx((Vector2){ x1, ym }, (Vector2){ x1 - ponta, ym - ponta }, g, cor);
                DrawLineEx((Vector2){ x1, ym }, (Vector2){ x1 - ponta, ym + ponta }, g, cor);
                DrawLineEx((Vector2){ x0, ym }, (Vector2){ x0 + ponta, ym - ponta }, g, cor);
                DrawLineEx((Vector2){ x0, ym }, (Vector2){ x0 + ponta, ym + ponta }, g, cor);
                break;
        }

        cx += (int)w + tam / 4;
    }

    return cx - x;
}

static void DesenharLogicaCentro(const char *f, int cx, int y, int tam, Color cor)
{
    DesenharLogica(f, cx - DesenharLogica(f, 0, 0, tam, cor, false) / 2, y, tam, cor, true);
}

// Disparo Dedutivo: P → Q depois do 1º acerto; P → Q e P depois do 2º
void DesenharMarcaDedutiva(int marca, Vector2 pos, float raio)
{
    if (marca == 0)
        return;

    int y = (int)(pos.y - raio - 34);
    int w = DesenharLogica("P > Q", 0, 0, 12, GOLD, false);

    DrawRectangle((int)pos.x - w / 2 - 4, y - 2, w + 8, (marca == 2) ? 30 : 16, Fade(BLACK, 0.6f));
    DesenharLogicaCentro("P > Q", (int)pos.x, y, 12, GOLD);

    if (marca == 2)
        DrawText("P", (int)pos.x - MeasureText("P", 12) / 2, y + 14, 12, GOLD);
}

// Um fragmento de afirmação: um pergaminho holográfico com a letra
void DesenharPecaSilogismo(Vector2 c, char letra, Color cor, float t)
{
    Vector2 p = { c.x, c.y + sinf(t * 3.0f + c.x) * 2.0f };

    DrawCircleV(p, 20, Fade(cor, 0.15f));
    DrawRectangle((int)p.x - 11, (int)p.y - 14, 22, 28, Fade((Color){ 20, 30, 60, 255 }, 0.9f));
    DrawRectangleLinesEx((Rectangle){ p.x - 11, p.y - 14, 22, 28 }, 2, cor);
    DrawRectangle((int)p.x - 13, (int)p.y - 16, 26, 4, cor);           // as pontas do pergaminho
    DrawRectangle((int)p.x - 13, (int)p.y + 12, 26, 4, cor);

    char txt[2] = { letra, '\0' };
    DrawText(txt, (int)p.x - MeasureText(txt, 18) / 2, (int)p.y - 9, 18, WHITE);
}

// Um fio do circuito em L (apagado, ou aceso com o valor V/F escrito)
static void DesenharFioLogico(Vector2 de, Vector2 ate, bool aceso, bool valor, float t)
{
    float mx = (de.x + ate.x) / 2.0f;
    Vector2 pts[4] = { de, { mx, de.y }, { mx, ate.y }, ate };
    Color cor = aceso ? (valor ? SKYBLUE : PURPLE) : Fade(GRAY, 0.35f);

    for (int i = 0; i < 3; i++)
        DrawLineEx(pts[i], pts[i + 1], aceso ? 3.0f : 2.0f, cor);

    if (!aceso)
        return;

    // Pulso de energia correndo no fio e o valor escrito no meio
    float k = fmodf(t * 0.8f, 1.0f);
    Vector2 q = (k < 0.5f) ? Vector2Lerp(pts[0], pts[1], k * 2.0f) : Vector2Lerp(pts[2], pts[3], (k - 0.5f) * 2.0f);
    DrawCircleV(q, 3, WHITE);
    DrawText(valor ? "V" : "F", (int)mx + 5, (int)((de.y + ate.y) / 2) - 5, 10, cor);
}

// O chão da câmara: diagramas geométricos girando devagar
void DesenharChaoCamara(float t)
{
    Vector2 c = { 400, 330 };
    Color ciano = { 0, 228, 255, 255 };

    DrawCircleLines((int)c.x, (int)c.y, 190, Fade(ciano, 0.07f));
    DrawCircleLines((int)c.x, (int)c.y, 130, Fade(PURPLE, 0.08f));

    for (int i = 0; i < 3; i++) {
        float a1 = t * 0.1f + i * (2.0f * PI / 3.0f), a2 = a1 + 2.0f * PI / 3.0f;
        DrawLineEx((Vector2){ c.x + cosf(a1) * 190, c.y + sinf(a1) * 190 }, (Vector2){ c.x + cosf(a2) * 190, c.y + sinf(a2) * 190 },
                   1.0f, Fade(ciano, 0.07f));
    }
}

// Mecanismo da montagem / do sofisma: P1 → P2 → C ligados à armadura do Silogista
void DesenharMecanismoSilogismo(const Silogista *s, float t)
{
    Color ciano = { 0, 228, 255, 255 };
    bool sofisma = (s->etapa == SIL_SOFISMA);
    Color linha = sofisma ? MAGENTA : ciano;

    for (int e = 0; e + 1 < NUM_ENCAIXES; e++) {
        Vector2 a = POS_ENCAIXES[e], b = POS_ENCAIXES[e + 1];
        Vector2 d = Vector2Normalize(Vector2Subtract(b, a));
        Vector2 ini = Vector2Add(a, Vector2Scale(d, 40)), fim = Vector2Subtract(b, Vector2Scale(d, 40));
        DrawLineEx(ini, fim, 2, Fade(linha, 0.5f));
        DrawLineEx(fim, Vector2Add(fim, Vector2Rotate(Vector2Scale(d, -8), 0.6f)), 2, Fade(linha, 0.5f));
        DrawLineEx(fim, Vector2Add(fim, Vector2Rotate(Vector2Scale(d, -8), -0.6f)), 2, Fade(linha, 0.5f));
    }

    // Energia do C até a armadura
    for (int i = 0; i < 10; i++) {
        float k = (i + fmodf(t * 2.0f, 1.0f)) / 10.0f;
        Vector2 q = Vector2Lerp(POS_ENCAIXES[ENCAIXE_C], s->pos, k);
        DrawCircleV(q, 2, Fade(linha, 0.4f));
    }

    for (int e = 0; e < NUM_ENCAIXES; e++) {

        Vector2 c = POS_ENCAIXES[e];
        bool erro = s->erro > 0.0f && ((int)(s->erro * 10.0f)) % 2 == 0;
        bool cheio = sofisma || s->encaixe[e] >= 0;
        Color cor = erro ? RED : (sofisma ? MAGENTA : (cheio ? ciano : Fade(ciano, 0.45f)));
        Rectangle r = { c.x - 36, c.y - 24, 72, 48 };

        if (sofisma && s->tremePeca[e] > 0.0f)
            r.x += (float)GetRandomValue(-2, 2);

        DrawRectangleRec(r, (Color){ 16, 20, 42, 255 });
        DrawRectangleLinesEx(r, 2, cor);
        DrawText(NOMES_ENCAIXES[e], (int)r.x + 5, (int)r.y + 4, 12, cor);

        const char *papel = PAPEIS_ENCAIXES[e];
        DrawText(papel, (int)c.x - MeasureText(papel, 10) / 2, (int)(r.y + r.height) + 4, 10, Fade(cor, 0.8f));

        if (sofisma) {
            // A peça do argumento corrompido, com a vida (some e volta se ninguém insistir)
            float k = s->hpPeca[e] / SIL_HP_PECA;
            DrawRing(c, 15, 19, -90.0f, -90.0f + 360.0f * k, 24, Fade(MAGENTA, 0.9f));
            DrawText("?", (int)c.x - 4, (int)c.y - 8, 16, WHITE);
            if (s->blindagem > 0.0f)
                DrawPolyLinesEx(c, 6, 30, t * 40.0f, 2, Fade(ciano, 0.8f));
        }
        else if (s->encaixe[e] >= 0) {
            char txt[2] = { s->letraPeca[s->encaixe[e]], '\0' };
            DrawText(txt, (int)c.x - MeasureText(txt, 22) / 2, (int)c.y - 10, 22, WHITE);
        }
        else {
            DrawCircleLines((int)c.x, (int)c.y + 2, 9 + sinf(t * 4.0f + e) * 2.0f, Fade(cor, 0.6f));
        }
    }
}

// Circuito proposicional no chão: terminais p e q, portas e núcleo
void DesenharCircuito(const Silogista *s, float t)
{
    const CircuitoDef *c = &CIRCUITO_SILOGISTA;
    Color ciano = { 0, 228, 255, 255 };
    bool final = (s->etapa == SIL_VULNERAVEL);

    // Fios: cada entrada de cada porta, e a última porta até o núcleo
    for (int k = 0; k < c->numPortas; k++) {
        const PortaLogica *p = &c->porta[k];
        int n = EntradasPorta(p);
        for (int i = 0; i < n; i++) {
            bool valor = false;
            bool aceso = SinalCircuito(s, p->entrada[i], &valor) || final;
            Vector2 de = SilPosElemento(p->entrada[i]);
            de.x += 24;
            Vector2 ate = { p->pos.x - 24, p->pos.y + ((n == 2) ? (i == 0 ? -10.0f : 10.0f) : 0.0f) };
            DesenharFioLogico(de, ate, aceso, valor, t);
        }
    }

    const PortaLogica *ultima = &c->porta[c->numPortas - 1];
    bool saiu = s->ativada[c->numPortas - 1] || final;
    DesenharFioLogico((Vector2){ ultima->pos.x + 24, ultima->pos.y }, (Vector2){ c->posNucleo.x - 34, c->posNucleo.y },
                      saiu, s->saida[c->numPortas - 1], t);

    // Terminais p e q (a letra V/F acompanha a cor)
    for (int e = 0; e < 2; e++) {
        Vector2 pos = c->posTerminal[e];
        bool v = s->valor[e];
        Color cor = v ? SKYBLUE : PURPLE;
        bool erro = s->erroElemento[e] > 0.0f;
        Rectangle r = { pos.x - 22, pos.y - 22, 44, 44 };

        DrawRectangleRec(r, v ? (Color){ 20, 50, 100, 255 } : (Color){ 55, 20, 70, 255 });
        DrawRectangleLinesEx(r, 3, erro ? RED : cor);
        DrawText(v ? "V" : "F", (int)pos.x - MeasureText(v ? "V" : "F", 22) / 2, (int)pos.y - 10, 22, WHITE);
        DrawText((e == 0) ? "p" : "q", (int)r.x - 14, (int)pos.y - 10, 20, cor);
        if (s->rodada == 1 && !final)
            DrawText("FIXO", (int)pos.x - MeasureText("FIXO", 10) / 2, (int)(r.y + r.height) + 4, 10, GRAY);
        else if (!final)
            DrawText("ATIRE: V/F", (int)pos.x - MeasureText("ATIRE: V/F", 10) / 2, (int)(r.y + r.height) + 4, 10, Fade(cor, 0.8f));
    }

    // Portas lógicas
    for (int k = 0; k < c->numPortas; k++) {
        const PortaLogica *p = &c->porta[k];
        bool ativa = s->ativada[k] || final;
        bool erro = s->erroElemento[2 + k] > 0.0f;
        Color cor = erro ? RED : (ativa ? (s->saida[k] ? SKYBLUE : PURPLE) : Fade(LIGHTGRAY, 0.7f));

        DrawCircleV(p->pos, 24, (Color){ 16, 20, 42, 255 });
        if (ativa) DrawCircleV(p->pos, 24, Fade(cor, 0.2f));
        DrawCircleLines((int)p->pos.x, (int)p->pos.y, 24, cor);
        DrawCircleLines((int)p->pos.x, (int)p->pos.y, 22, cor);
        DesenharLogicaCentro(SIMBOLOS_OPERADORES[p->op], (int)p->pos.x, (int)p->pos.y - 11, 22, WHITE);

        const char *nome = NOMES_OPERADORES[p->op];
        DrawText(nome, (int)p->pos.x - MeasureText(nome, 10) / 2, (int)p->pos.y + 28, 10, cor);

        // Saída: ? até a porta ser ativada
        const char *saida = ativa ? (s->saida[k] ? "V" : "F") : "?";
        DrawText(saida, (int)p->pos.x + 28, (int)p->pos.y - 22, 14, ativa ? cor : GRAY);
    }

    // Núcleo de saída
    Vector2 n = c->posNucleo;
    bool energia = s->nucleo > 0.0f || final;
    float pulso = 0.5f + 0.5f * sinf(t * 6.0f);

    if (energia) DrawCircleV(n, 40 + pulso * 6, Fade(ciano, 0.15f));
    DrawCircleV(n, 32, (Color){ 16, 20, 42, 255 });
    DrawPolyLinesEx(n, 8, 32, t * 20.0f, 3, (s->erro > 0.0f && !final) ? RED : (energia ? ciano : Fade(ciano, 0.4f)));

    const char *dentro = energia ? (s->saida[c->numPortas - 1] ? "V" : "F") : "?";
    DrawText(dentro, (int)n.x - MeasureText(dentro, 26) / 2, (int)n.y - 12, 26, energia ? WHITE : GRAY);
    DrawText("NÚCLEO", (int)n.x - MeasureText("NÚCLEO", 10) / 2, (int)n.y + 38, 10, ciano);

    if (!final) {
        const char *exige = TextFormat("EXIGE: %s", s->exigido ? "V" : "F");
        DrawText(exige, (int)n.x - MeasureText(exige, 12) / 2, (int)n.y - 52, 12, GOLD);
    }

    // Corte do Silogista: o elemento fica travado (X e o tempo que falta)
    for (int e = 0; e < SilNumElementos(); e++) {
        if (s->travado[e] <= 0.0f) continue;
        Vector2 q = SilPosElemento(e);
        DrawLineEx((Vector2){ q.x - 18, q.y - 18 }, (Vector2){ q.x + 18, q.y + 18 }, 4, Fade(RED, 0.85f));
        DrawLineEx((Vector2){ q.x + 18, q.y - 18 }, (Vector2){ q.x - 18, q.y + 18 }, 4, Fade(RED, 0.85f));
        DrawRing(q, 28, 31, -90.0f, -90.0f + 360.0f * s->travado[e] / SIL_CORTE_DURACAO, 24, Fade(PURPLE, 0.9f));
    }
}

// Avisos dos ataques (desenhados antes, embaixo de tudo) e as zonas ativas
void DesenharAtaquesSilogista(const Silogista *s, float t)
{
    for (int i = 0; i < MAX_ATAQUES_SIL; i++) {

        const AtaqueSil *a = &s->atq[i];

        if (!a->ativo)
            continue;

        float k = (a->aviso > 0.0f) ? 1.0f - a->aviso / SIL_AVISO : 1.0f;     // 0 = começou a avisar, 1 = vai disparar
        float pisca = 0.5f + 0.5f * sinf(t * 18.0f);

        switch (a->tipo) {

            case ATQ_LINHAS: {
                if (a->aviso <= 0.0f) break;
                // As balas que vão sair e o corredor verde da brecha
                for (int b = 0; b < SilBalasLinha(a); b++)
                    if (b < a->brecha || b >= a->brecha + SIL_LINHA_BRECHA) {
                        Vector2 q = SilPontoLinha(a, b);
                        Color cb = (k > 0.75f) ? WHITE : ORANGE;                         // no fim, acendem (vai sair)
                        DrawCircleLines((int)q.x, (int)q.y, 6, Fade(cb, 0.4f + 0.5f * k));
                        DrawLineEx(q, Vector2Add(q, Vector2Scale(a->dir, 14 + 12 * k)), 2, Fade(cb, 0.5f * k));
                    }
                Vector2 b0 = SilPontoLinha(a, a->brecha), b1 = SilPontoLinha(a, a->brecha + SIL_LINHA_BRECHA - 1);
                float meia = SIL_LINHA_ESPACO / 2.0f;
                Rectangle brecha = (a->dir.y != 0.0f)
                    ? (Rectangle){ b0.x - meia, ArenaRogue().y, b1.x - b0.x + 2 * meia, ArenaRogue().height }
                    : (Rectangle){ ArenaRogue().x, b0.y - meia, ArenaRogue().width, b1.y - b0.y + 2 * meia };
                DrawRectangleRec(brecha, Fade(GREEN, 0.08f + 0.06f * pisca));
            } break;

            case ATQ_ZONA:
                if (a->aviso > 0.0f) {
                    float rf = SIL_ZONA_RAIO + 40.0f * (1.0f - k);                       // anel fechando até a borda
                    DrawCircleV(a->pos, SIL_ZONA_RAIO * k, Fade(RED, 0.18f));
                    DrawCircleLines((int)a->pos.x, (int)a->pos.y, SIL_ZONA_RAIO, Fade(RED, 0.5f + 0.4f * pisca));
                    DrawRing(a->pos, rf, rf + 2.0f, 0.0f, 360.0f, 36, Fade((k > 0.75f) ? WHITE : RED, 0.3f + 0.5f * k));
                } else {
                    DrawCircleV(a->pos, SIL_ZONA_RAIO, Fade(RED, 0.30f + 0.1f * pisca));
                    DrawPolyLinesEx(a->pos, 6, SIL_ZONA_RAIO, t * 60.0f, 3, Fade(MAGENTA, 0.9f));
                    DrawText("BLOQUEIO", (int)a->pos.x - MeasureText("BLOQUEIO", 10) / 2, (int)a->pos.y - 5, 10, WHITE);
                }
                break;

            case ATQ_REFUTACAO:                     // corredor da rajada (trava nos últimos 0,25 s)
                if (a->aviso > 0.0f)
                    DesenharCorredorAvisoIni(s->pos, a->dir, 760.0f, 9.0f, k, a->aviso <= 0.25f, RED, t);
                break;

            case ATQ_CORTE: {
                Vector2 alvo = SilPosElemento(a->alvo);
                DrawLineEx(s->pos, alvo, 2.0f + 3.0f * k, Fade(PURPLE, 0.3f + 0.5f * pisca));
                DrawCircleLines((int)alvo.x, (int)alvo.y, 34 - 8 * k, Fade(RED, 0.8f));
            } break;

            default:    // ATQ_ANEL: as 16 linhas de saída e o anel se fechando no Silogista
                if (a->aviso > 0.0f)
                    DesenharLinhasBalasIni(s->pos, SIL_RAIO, 16, s->anim + a->aviso, 2.0f * PI / 16.0f, 1.0f - a->aviso / 0.7f, ORANGE, t);
                break;
        }
    }
}

// O Silogista: um sábio mecânico (cabeça hexagonal com visor, manto, livro holográfico)
// com os conectivos lógicos orbitando e a armadura lógica (uma camada por puzzle que falta)
void DesenharSilogista(const Silogista *s, Vector2 jogador, float t)
{
    float k = (s->morrendo > 0.0f) ? s->morrendo / 1.6f : 1.0f;
    Vector2 p = { s->pos.x, s->pos.y + sinf(t * 1.6f) * 4.0f };
    Color ciano = { 0, 228, 255, 255 }, lilas = { 170, 140, 255, 255 };

    DrawEllipse((int)s->pos.x, (int)s->pos.y + (int)SIL_RAIO + 10, 30 * k, 8, Fade(BLACK, 0.4f));

    // Diagrama atrás: triângulo dentro de um círculo
    DrawCircleLines((int)p.x, (int)p.y, 50, Fade(ciano, 0.25f * k));
    for (int i = 0; i < 3; i++) {
        float a1 = -t * 0.5f + i * (2.0f * PI / 3.0f), a2 = a1 + 2.0f * PI / 3.0f;
        DrawLineEx((Vector2){ p.x + cosf(a1) * 50, p.y + sinf(a1) * 50 }, (Vector2){ p.x + cosf(a2) * 50, p.y + sinf(a2) * 50 },
                   1.5f, Fade(lilas, 0.35f * k));
    }

    for (int vez = 0; vez < ((s->flash > 0.0f) ? 3 : 1); vez++) {

        if (vez == 1) BeginBlendMode(BLEND_ADDITIVE);

        // Manto
        Color manto = Fade((Color){ 40, 40, 110, 255 }, k);
        DesenharTrianguloB2((Vector2){ p.x - 14, p.y - 6 }, (Vector2){ p.x + 14, p.y - 6 }, (Vector2){ p.x - 28, p.y + 34 }, manto);
        DesenharTrianguloB2((Vector2){ p.x + 14, p.y - 6 }, (Vector2){ p.x + 28, p.y + 34 }, (Vector2){ p.x - 28, p.y + 34 }, manto);
        DrawLineEx((Vector2){ p.x - 28, p.y + 34 }, (Vector2){ p.x + 28, p.y + 34 }, 2, Fade(ciano, k));
        DrawLineEx((Vector2){ p.x, p.y - 6 }, (Vector2){ p.x, p.y + 34 }, 1, Fade(lilas, 0.7f * k));

        // Livro holográfico aberto na frente
        Color pagina = Fade(ciano, 0.45f * k);
        DesenharTrianguloB2((Vector2){ p.x, p.y + 14 }, (Vector2){ p.x - 18, p.y + 8 }, (Vector2){ p.x - 18, p.y + 20 }, pagina);
        DesenharTrianguloB2((Vector2){ p.x, p.y + 14 }, (Vector2){ p.x + 18, p.y + 8 }, (Vector2){ p.x + 18, p.y + 20 }, pagina);
        DrawRectangle((int)p.x - 18, (int)p.y + 14, 36, 8, pagina);

        // Cabeça: hexágono com um visor que segue o jogador
        Vector2 cab = { p.x, p.y - 22 };
        Vector2 olhar = Vector2Normalize(Vector2Subtract(jogador, cab));
        DrawPoly(cab, 6, 17, 0, Fade((Color){ 25, 25, 60, 255 }, k));
        DrawPolyLinesEx(cab, 6, 17, 0, 2, Fade(ciano, k));
        DrawRectangle((int)(cab.x - 10 + olhar.x * 2), (int)(cab.y - 4 + olhar.y * 2), 20, 6, Fade(ciano, k));

        // Barba de circuitos
        for (int i = 0; i < 3; i++) {
            float bx = cab.x - 6 + i * 6;
            DrawLine((int)bx, (int)cab.y + 8, (int)bx, (int)cab.y + 14 + (i % 2) * 5, Fade(lilas, k));
            DrawCircle((int)bx, (int)cab.y + 15 + (i % 2) * 5, 1.5f, Fade(lilas, k));
        }

        // Conectivos orbitando
        static const char *simbolos[5] = { "~", "&", "|", ">", "#" };
        for (int i = 0; i < 5; i++) {
            float a = t * 0.9f + i * (2.0f * PI / 5.0f);
            Vector2 q = { p.x + cosf(a) * 56, p.y - 6 + sinf(a) * 32 };
            DesenharLogicaCentro(simbolos[i], (int)q.x, (int)q.y - 7, 14, Fade((i % 2) ? ciano : lilas, k));
        }
    }

    if (s->flash > 0.0f)
        EndBlendMode();

    // Armadura lógica
    static const Color camadas[3] = { { 0, 228, 255, 255 }, { 102, 191, 255, 255 }, { 170, 140, 255, 255 } };
    int restam = 3 - (int)s->etapa;

    for (int c = 0; c < restam && s->morrendo <= 0.0f; c++)
        DrawPolyLinesEx(p, 6, SIL_RAIO + 12 + c * 8, t * ((c % 2) ? -35.0f : 35.0f), 2, Fade(camadas[c], 0.75f));
}

// Barra de vida e as camadas da armadura lógica
void DesenharBarraSilogista(const Silogista *s)
{
    int x = 200, y = 44, w = 400;
    int camadas = 3 - (int)s->etapa;

    DrawRectangle(x - 4, y - 2, w + 8, 16, Fade(BLACK, 0.8f));
    DrawRectangle(x, y + 2, w, 8, (Color){ 30, 25, 60, 255 });
    DrawRectangle(x, y + 2, (int)(w * s->hp / SIL_HP), 8, (camadas > 0) ? Fade((Color){ 120, 110, 255, 255 }, 0.45f) : (Color){ 120, 110, 255, 255 });
    DrawRectangleLines(x, y + 2, w, 8, GRAY);

    int wNome = MeasureText("O SILOGISTA", 10);
    DrawRectangle(x - wNome - 18, y - 2, wNome + 12, 16, Fade(BLACK, 0.8f));
    DrawText("O SILOGISTA", x - wNome - 12, y + 1, 10, (Color){ 170, 140, 255, 255 });

    // Uma casinha por camada da armadura (cheia = ainda protege)
    int wArm = MeasureText("ARMADURA", 10);
    DrawRectangle(x + w + 8, y - 2, wArm + 38, 16, Fade(BLACK, 0.8f));
    DrawText("ARMADURA", x + w + 12, y + 1, 10, LIGHTGRAY);
    for (int c = 0; c < 3; c++) {
        Rectangle r = { (float)(x + w + 18 + wArm + c * 8), (float)y + 1, 6, 10 };
        if (c < camadas) DrawRectangleRec(r, (Color){ 0, 228, 255, 255 });
        DrawRectangleLinesEx(r, 1, (Color){ 0, 228, 255, 255 });
    }
}

// Faixa no alto: o que está em jogo em cada puzzle (os textos não cabem na arena)
void DesenharFaixaSilogista(const Biblioteca *b, float tempo)
{
    const Silogista *s = &b->silogista;
    Color ciano = { 0, 228, 255, 255 };

    if (s->etapa == SIL_VULNERAVEL || s->morrendo > 0.0f)
        return;

    // Versão bullet-hell: as premissas da rodada e a prova (curtas: o texto não voa pela tela)
    if (s->modoBH) {
        const RodadaSilogistaBH *r = RodadaBH(s->bh.rodada);
        Rectangle faixa = { 60, 64, 680, 64 };
        DrawRectangleRec(faixa, Fade((Color){ 18, 15, 30, 255 }, 0.88f));
        DrawRectangleLinesEx(faixa, 2, ciano);

        int x = (int)faixa.x + 10, y = (int)faixa.y + 6;
        DrawText(TextFormat("RODADA %d/3  //  %s", s->bh.rodada + 1, r->titulo), x, y, 10, ciano);

        char linhas[320];
        QuebrarTexto(r->premissas, (int)faixa.width - 20, 12, linhas, (int)sizeof(linhas));
        DesenharTextoLinhas(linhas, x, y + 14, 12, 14, WHITE);
        DrawText(r->pergunta, x, y + 44, 10, Fade(GOLD, 0.75f + 0.25f * sinf(tempo * 5.0f)));

        int px = (int)(faixa.x + faixa.width) - 90;
        DrawText("PROVA", px - 44, y, 10, LIGHTGRAY);
        for (int c = 0; c < SIL_BH_META; c++) {
            Rectangle pip = { (float)(px + c * 14), (float)y, 10, 10 };
            if (c < s->bh.acertosRodada) DrawRectangleRec(pip, LIME);
            DrawRectangleLinesEx(pip, 1, LIME);
        }
        return;
    }

    Rectangle faixa = { 60, 64, 680, 64 };
    DrawRectangleRec(faixa, Fade((Color){ 18, 15, 30, 255 }, 0.88f));
    DrawRectangleLinesEx(faixa, 2, (s->etapa == SIL_SOFISMA) ? MAGENTA : ciano);

    int x = (int)faixa.x + 10, y = (int)faixa.y + 6;

    if (s->etapa == SIL_MONTAGEM) {

        DrawText("MONTE O SILOGISMO:   P1 = regra geral    P2 = caso particular    C = a conclusão que decorre das duas", x, y, 10, ciano);

        // Em ordem de letra (as letras foram sorteadas)
        for (char letra = 'X'; letra <= 'Z'; letra++) {
            int k = 0;
            while (k < NUM_ENCAIXES - 1 && s->letraPeca[k] != letra) k++;

            int onde = -1;
            for (int e = 0; e < NUM_ENCAIXES; e++)
                if (s->encaixe[e] == k) onde = e;

            int yy = y + 14 + (letra - 'X') * 15;
            Color cor = (s->carregando == k) ? YELLOW : ((onde >= 0) ? Fade(LIGHTGRAY, 0.6f) : WHITE);
            const char *txt = TextFormat("%c:  %s", letra, SILOGISMO_VALIDO.frase[k]);
            DrawText(txt, x, yy, 12, cor);

            int xr = x + MeasureText(txt, 12) + 10;
            if (s->carregando == k) DrawText("(na sua mão)", xr, yy, 12, YELLOW);
            else if (onde >= 0)     DrawText(TextFormat("(em %s)", NOMES_ENCAIXES[onde]), xr, yy, 12, ciano);
        }
    }
    else if (s->etapa == SIL_SOFISMA) {

        DrawText("ARGUMENTO DO SILOGISTA (ele jura que é válido). Destrua a tiros a peça que não se sustenta:", x, y, 10, MAGENTA);

        for (int e = 0; e < NUM_ENCAIXES; e++)
            DrawText(TextFormat("%s:  %s", NOMES_ENCAIXES[e], SOFISMA_SILOGISTA.frase[e]), x, y + 14 + e * 15, 12, WHITE);

        if (s->blindagem > 0.0f)
            DrawText("BLINDADO", (int)(faixa.x + faixa.width) - 70, y, 10, ciano);
    }
    else {

        const CircuitoDef *c = &CIRCUITO_SILOGISTA;

        DrawText(TextFormat("CIRCUITO PROPOSICIONAL  //  RODADA %d DE 2", s->rodada), x, y, 10, ciano);
        DesenharLogicaCentro(c->expressao, (int)(faixa.x + faixa.width / 2), y + 12, 22, WHITE);

        const char *exige = TextFormat("NÚCLEO EXIGE: %s", s->exigido ? "V" : "F");
        DrawText(exige, (int)(faixa.x + faixa.width) - 16 - MeasureText(exige, 14), y + 16, 14,
                 Fade(GOLD, 0.75f + 0.25f * sinf(tempo * 5.0f)));

        DesenharLogica("& = E     | = OU     ~ = NÃO", x, y + 40, 12, LIGHTGRAY, true);

        const char *nota = (s->rodada == 1) ? "p e q fixos: ative as portas na ordem certa (tiros)"
                                            : "atire em p e q para trocar V/F; depois ative as portas";
        DrawText(nota, (int)(faixa.x + faixa.width) - 16 - MeasureText(nota, 10), y + 42, 10, ciano);
    }
}

// Ícone de cada arma (pedestal, loja e HUD)
void DesenharIconeArma(TipoArma arma, Vector2 c, float e, float tempo)
{
    Color cor = DEF_ARMAS[arma].cor;
    Color metal = { 60, 70, 100, 255 };

    switch (arma) {

        case ARMA_VETOR:            // fuzil comprido com luneta
            DrawRectangle((int)(c.x - 13 * e), (int)(c.y - 1 * e), (int)(26 * e), (int)(3 * e), metal);
            DrawRectangle((int)(c.x - 13 * e), (int)(c.y + 1 * e), (int)(6 * e), (int)(6 * e), metal);
            DrawRectangle((int)(c.x - 4 * e), (int)(c.y - 5 * e), (int)(9 * e), (int)(3 * e), cor);
            DrawLineEx((Vector2){ c.x + 13 * e, c.y }, (Vector2){ c.x + 16 * e, c.y }, 2, Fade(cor, 0.5f + 0.5f * sinf(tempo * 6.0f)));
            break;

        case ARMA_ROTEADOR:         // roteador com antenas e um pacote fazendo curva
            DrawRectangle((int)(c.x - 10 * e), (int)(c.y + 1 * e), (int)(20 * e), (int)(7 * e), metal);
            DrawLineEx((Vector2){ c.x - 6 * e, c.y + 1 * e }, (Vector2){ c.x - 9 * e, c.y - 8 * e }, 2, LIGHTGRAY);
            DrawLineEx((Vector2){ c.x + 6 * e, c.y + 1 * e }, (Vector2){ c.x + 9 * e, c.y - 8 * e }, 2, LIGHTGRAY);
            DrawRing((Vector2){ c.x, c.y - 2 * e }, 6 * e, 7.5f * e, 200.0f, 340.0f, 8, cor);
            break;

        case ARMA_FLUXO:            // três canos e a rajada
            for (int k = -1; k <= 1; k++)
                DrawRectangle((int)(c.x - 10 * e), (int)(c.y + k * 4 * e - 1), (int)(16 * e), (int)(2 * e), metal);
            for (int k = 0; k < 3; k++)
                DrawCircle((int)(c.x + 9 * e + k * 4 * e), (int)c.y, 1.5f * e, cor);
            break;

        case ARMA_ORBE:             // esfera com um anel
            DrawCircleV(c, 7 * e, Fade(cor, 0.5f));
            DrawCircleV(c, 4 * e, cor);
            DrawRing(c, 9 * e, 10.5f * e, tempo * 90.0f, tempo * 90.0f + 270.0f, 12, cor);
            break;

        case ARMA_FRAGMENTADORA:    // cone de estilhaços
            DrawRectangle((int)(c.x - 12 * e), (int)(c.y - 3 * e), (int)(10 * e), (int)(6 * e), metal);
            for (int k = -2; k <= 2; k++)
                DrawLineEx((Vector2){ c.x, c.y }, (Vector2){ c.x + 13 * e, c.y + k * 4 * e }, 1.5f, cor);
            break;

        case ARMA_KATANA:           // lâmina diagonal, guarda dourada e cabo escuro
            DrawLineEx((Vector2){ c.x - 13 * e, c.y + 13 * e }, (Vector2){ c.x - 7 * e, c.y + 7 * e }, 3.5f, (Color){ 60, 40, 90, 255 });
            DrawLineEx((Vector2){ c.x - 10 * e, c.y + 4 * e }, (Vector2){ c.x - 4 * e, c.y + 10 * e }, 3, GOLD);
            DrawLineEx((Vector2){ c.x - 7 * e, c.y + 7 * e }, (Vector2){ c.x + 13 * e, c.y - 13 * e }, 3, cor);
            DrawLineEx((Vector2){ c.x - 6 * e, c.y + 5 * e }, (Vector2){ c.x + 12 * e, c.y - 13 * e }, 1, Fade(SKYBLUE, 0.6f + 0.4f * sinf(tempo * 5.0f)));
            break;

        case ARMA_RAIO_CLASSICO:    // cano com bobina e um raio em zigue-zague
            DrawRectangle((int)(c.x - 12 * e), (int)(c.y - 3 * e), (int)(16 * e), (int)(6 * e), metal);
            DrawCircleV((Vector2){ c.x - 4 * e, c.y }, 3 * e, cor);
            DrawLineEx((Vector2){ c.x + 4 * e, c.y }, (Vector2){ c.x + 8 * e, c.y - 4 * e }, 1.5f, cor);
            DrawLineEx((Vector2){ c.x + 8 * e, c.y - 4 * e }, (Vector2){ c.x + 9 * e, c.y + 4 * e }, 1.5f, cor);
            DrawLineEx((Vector2){ c.x + 9 * e, c.y + 4 * e }, (Vector2){ c.x + 14 * e, c.y }, 1.5f, WHITE);
            break;

        case ARMA_RAIO:             // cajado com cristal e um chip quadrado girando
            DrawLineEx((Vector2){ c.x - 12 * e, c.y + 12 * e }, (Vector2){ c.x + 4 * e, c.y - 4 * e }, 3, (Color){ 90, 60, 120, 255 });
            DrawCircleV((Vector2){ c.x + 7 * e, c.y - 7 * e }, 4.5f * e, cor);
            DrawCircleV((Vector2){ c.x + 7 * e, c.y - 7 * e }, 2.0f * e, WHITE);
            DrawPolyLinesEx((Vector2){ c.x + 7 * e, c.y - 7 * e }, 4, 9.0f * e, 45.0f + tempo * 50.0f, 1.5f, Fade(cor, 0.8f));
            break;

        case ARMA_PULSO_FW:         // hexágonos concêntricos
            DrawPolyLinesEx(c, 6, 10 * e, tempo * 30.0f, 2, cor);
            DrawPolyLinesEx(c, 6, 5 * e, -tempo * 30.0f, 2, cor);
            break;

        default:                    // Disparador de Dados
            DrawRectangle((int)(c.x - 9 * e), (int)(c.y - 2 * e), (int)(16 * e), (int)(4 * e), (Color){ 30, 60, 120, 255 });
            DrawRectangle((int)(c.x - 9 * e), (int)(c.y + 1 * e), (int)(5 * e), (int)(6 * e), (Color){ 30, 60, 120, 255 });
            DrawCircleV((Vector2){ c.x + 9 * e, c.y }, 2 * e, cor);
            break;
    }
}

// Projétil do jogador conforme a arma (cada um com a sua forma)
// Projétil inimigo: laranja com contorno escuro (fácil de ver em qualquer chão), um brilho e um rastro curto
void DesenharProjetilInimigo(const Projetil *p, float tempo)
{
    float v = Vector2Length(p->vel);
    Vector2 d = (v > 1.0f) ? Vector2Scale(p->vel, 1.0f / v) : (Vector2){ 0, 0 };
    for (int r = 3; r >= 1; r--)
        DrawCircleV(Vector2Subtract(p->pos, Vector2Scale(d, p->raio * 1.1f * (float)r)), p->raio * (1.0f - 0.2f * (float)r), Fade(ORANGE, 0.28f / (float)r));
    DrawCircleV(p->pos, p->raio + 5.0f + sinf(tempo * 14.0f + p->pos.x * 0.05f), Fade(ORANGE, 0.18f));
    DrawCircleV(p->pos, p->raio + 2, (Color){ 30, 0, 20, 255 });
    DrawCircleV(p->pos, p->raio, ORANGE);
    DrawCircleV(p->pos, p->raio - 2, YELLOW);
    DrawCircleV(p->pos, fmaxf(1.0f, p->raio - 4.0f), Fade(WHITE, 0.85f));
}

void DesenharProjetilJogador(const Projetil *p, float tempo)
{
    Vector2 d = (Vector2Length(p->vel) > 0.01f) ? Vector2Normalize(p->vel) : (Vector2){ 1, 0 };

    if (p->secundario) {
        // Fragmento ou pacote do Botnet Reverso: losango verde-limão
        DrawPoly(p->pos, 4, p->raio + 2, tempo * 400.0f, Fade(LIME, 0.5f));
        DrawPoly(p->pos, 4, p->raio, tempo * 400.0f, LIME);
        return;
    }

    Color cor = DEF_ARMAS[p->arma].cor;

    switch (p->arma) {
        case ARMA_VETOR:            // feixe comprido
            DrawLineEx(Vector2Subtract(p->pos, Vector2Scale(d, 26)), p->pos, 6, Fade(SKYBLUE, 0.4f));
            DrawLineEx(Vector2Subtract(p->pos, Vector2Scale(d, 22)), p->pos, 2.5f, WHITE);
            break;
        case ARMA_ROTEADOR:         // pacote (losango) com rastro
            DrawLineEx(Vector2Subtract(p->pos, Vector2Scale(d, 12)), p->pos, 2, Fade(cor, 0.4f));
            DrawPoly(p->pos, 4, p->raio + 1, atan2f(d.y, d.x) * RAD2DEG, cor);
            break;
        case ARMA_FLUXO:            // gotinhas amarelas
            DrawCircleV(p->pos, p->raio, cor);
            break;
        case ARMA_ORBE:             // esfera pulsando
            DrawCircleV(p->pos, p->raio + 4 + sinf(tempo * 20.0f) * 1.5f, Fade(cor, 0.3f));
            DrawCircleV(p->pos, p->raio, cor);
            DrawCircleLines((int)p->pos.x, (int)p->pos.y, p->raio + 2, WHITE);
            break;
        case ARMA_KATANA:           // onda de corte: meia-lua
            DrawLineEx(Vector2Add(p->pos, (Vector2){ -d.y * 12, d.x * 12 }), Vector2Add(p->pos, (Vector2){ d.y * 12, -d.x * 12 }), 3, Fade(WHITE, 0.9f));
            DrawLineEx(Vector2Subtract(p->pos, Vector2Scale(d, 10)), p->pos, 2, Fade(SKYBLUE, 0.5f));
            break;
        case ARMA_RAIO_CLASSICO:    // feixe de plasma: rastro longo com núcleo branco
            DrawLineEx(Vector2Subtract(p->pos, Vector2Scale(d, 34)), p->pos, p->raio * 2.2f, Fade(cor, 0.35f));
            DrawLineEx(Vector2Subtract(p->pos, Vector2Scale(d, 28)), p->pos, p->raio, cor);
            DrawLineEx(Vector2Subtract(p->pos, Vector2Scale(d, 22)), p->pos, p->raio * 0.4f, WHITE);
            break;
        case ARMA_RAIO:             // feixe de plasma: rastro longo com núcleo branco
            DrawLineEx(Vector2Subtract(p->pos, Vector2Scale(d, 34)), p->pos, p->raio * 2.2f, Fade(cor, 0.35f));
            DrawLineEx(Vector2Subtract(p->pos, Vector2Scale(d, 28)), p->pos, p->raio, cor);
            DrawLineEx(Vector2Subtract(p->pos, Vector2Scale(d, 22)), p->pos, p->raio * 0.4f, WHITE);
            break;
        case ARMA_FRAGMENTADORA:    // estilhaço curto
            DrawLineEx(Vector2Subtract(p->pos, Vector2Scale(d, 6)), Vector2Add(p->pos, Vector2Scale(d, 3)), 3, cor);
            break;
        default:
            DrawCircleV(p->pos, p->raio + 3, Fade(SKYBLUE, 0.3f));
            DrawCircleV(p->pos, p->raio, (Color){ 180, 240, 255, 255 });
            break;
    }
}

// Efeitos em volta do jogador: escudo, campo de latência, Overclock, carga do Vetor e calor do Fluxo
// A arma equipada NA MÃO do Thomas (cada uma com a sua forma, apontada para a mira) e o aviso de troca:
// anel em volta, a arma "pulando" na mão e o nome dela sobre a cabeça. naMao = false no átrio (guardada).
void DesenharArmaNaMao(const Biblioteca *b, float tempo, bool naMao)
{
    const JogadorRogue *j = &b->j;
    TipoArma arma = RogueArmaAtual(j);
    const DefArma *def = &DEF_ARMAS[arma];
    Vector2 d = (Vector2Length(j->mira) > 0.01f) ? Vector2Normalize(j->mira) : (Vector2){ 1, 0 };
    Vector2 n = (d.x >= 0.0f) ? (Vector2){ -d.y, d.x } : (Vector2){ d.y, -d.x };   // a arma nunca fica de cabeça para baixo
    float coice = Clamp(j->cronoTiro / 0.14f, 0.0f, 1.0f);                         // recuo logo depois do tiro
    Vector2 base = Vector2Add(Vector2Add(j->pos, (Vector2){ 0, 4 }), Vector2Scale(d, -3.5f * coice * ((arma == ARMA_KATANA) ? 0.0f : 1.0f)));
    float pop = (j->trocaArma > 1.0f) ? Clamp((j->trocaArma - 1.0f) / 0.3f, 0.0f, 1.0f) : 0.0f;
    float e = 1.0f + 0.5f * pop;                                 // a arma "pula" na troca
    Color cor = def->cor;
    Color metal = { 40, 55, 95, 255 };

    #define PTO(fr, la) Vector2Add(base, Vector2Add(Vector2Scale(d, (fr) * e), Vector2Scale(n, (la) * e)))
    // corpo da arma com contorno escuro (lê melhor sobre o chão e sobre o personagem)
    #define LIN(a, b, w, c) do { DrawLineEx((a), (b), (w) + 2.0f, (Color){ 10, 14, 30, 255 }); DrawLineEx((a), (b), (w), (c)); } while (0)

    if (naMao) {
        switch (arma) {

            case ARMA_VETOR: {          // fuzil comprido com luneta; a ponta brilha enquanto carrega
                float k = Clamp(j->carga / VETOR_CARGA_MAX, 0.0f, 1.0f);
                LIN(PTO(-4, 0), PTO(32, 0), 4, metal);
                DrawLineEx(PTO(-4, 0), PTO(32, 0), 1.5f, cor);
                LIN(PTO(6, -3), PTO(15, -3), 3, ((Color){ 70, 100, 150, 255 }));
                if (k > 0.05f) DrawCircleV(PTO(33, 0), 2.0f + 5.0f * k, Fade(WHITE, 0.4f + 0.5f * k));
            } break;

            case ARMA_ROTEADOR:         // caixa com duas antenas e o anel na ponta
                LIN(PTO(2, 0), PTO(16, 0), 8, metal);
                DrawLineEx(PTO(3, 0), PTO(15, 0), 2, Fade(cor, 0.8f));
                DrawLineEx(PTO(6, -3), PTO(3, -10), 2, LIGHTGRAY);
                DrawLineEx(PTO(12, 3), PTO(9, 10), 2, LIGHTGRAY);
                DrawRing(PTO(19, 0), 3.0f, 4.5f, 0.0f, 360.0f, 10, cor);
                break;

            case ARMA_FLUXO:            // três canos; ficam vermelhos quando superaquece
                for (int k = -1; k <= 1; k++) {
                    LIN(PTO(0, k * 3), PTO(18, k * 3), 2, ColorLerp(metal, RED, j->calor * 0.8f));     // cano esquenta
                    DrawCircleV(PTO(19, k * 3), 1.8f + 1.2f * j->calor, j->superaquecida ? RED : ColorLerp(cor, ORANGE, j->calor));
                }
                break;

            case ARMA_ORBE: {           // esfera flutuando na frente, pulsando
                Vector2 c = PTO(22, 0);
                LIN(PTO(0, 0), PTO(12, 0), 3, metal);
                DrawCircleV(c, 7.0f + sinf(tempo * 8.0f), Fade(cor, 0.35f));
                DrawCircleV(c, 4.5f, cor);
                DrawRing(c, 8.0f, 9.5f, tempo * 90.0f, tempo * 90.0f + 260.0f, 12, cor);
            } break;

            case ARMA_FRAGMENTADORA:    // cano curto e largo com a boca em leque
                LIN(PTO(0, 0), PTO(14, 0), 8, metal);
                DrawLineEx(PTO(14, -4), PTO(23, -9), 2, cor);
                DrawLineEx(PTO(14, 0), PTO(24, 0), 2, cor);
                DrawLineEx(PTO(14, 4), PTO(23, 9), 2, cor);
                if (((j->cartucho + 1) % ((RogueUp(j, UP_ARMA_FRAG_POLVORA) > 0) ? 3 : 4)) == 0)      // próximo tiro é o dourado
                    DrawCircleV(PTO(10, 0), 2.5f + sinf(tempo * 12.0f), GOLD);
                break;

            case ARMA_RAIO_CLASSICO: {  // canhão com bobina; acende quando há EP para atirar
                bool pronto = j->ep >= RAIOC_CUSTO_EP * ((RogueUp(j, UP_ARMA_RAIOC_SUPER) > 0) ? RAIOC_SUPER_FATOR : 1.0f);
                LIN(PTO(-2, 0), PTO(26, 0), 5, metal);
                DrawCircleV(PTO(6, 0), 3.0f, pronto ? cor : (Color){ 60, 80, 90, 255 });
                DrawCircleV(PTO(14, 0), 2.5f, pronto ? cor : (Color){ 60, 80, 90, 255 });
                if (pronto) DrawCircleV(PTO(27, 0), 2.0f + 1.5f * sinf(tempo * 14.0f), Fade(WHITE, 0.8f));
            } break;

            case ARMA_RAIO: {           // cajado: cristal que brilha quando há EP e runas orbitando
                bool pronto = j->ep >= RAIO_CUSTO_EP * ((RogueUp(j, UP_ARMA_RAIO_SUPER) > 0) ? RAIO_SUPER_FATOR : 1.0f);
                Color cc = pronto ? cor : (Color){ 90, 80, 40, 255 };
                LIN(PTO(-4, 0), PTO(20, 0), 3, ((Color){ 90, 60, 120, 255 }));
                DrawCircleV(PTO(24, 0), 4.5f + (pronto ? sinf(tempo * 8.0f) : 0.0f), cc);
                if (pronto) {
                    DrawCircleV(PTO(24, 0), 2.0f, WHITE);
                    for (int k = 0; k < 3; k++) {
                        float ang = tempo * 3.0f + (float)k * 2.0944f;
                        DrawCircleV(Vector2Add(PTO(24, 0), (Vector2){ cosf(ang) * 10.0f, sinf(ang) * 10.0f }), 1.8f, Fade(cor, 0.9f));
                    }
                }
            } break;

            case ARMA_PULSO_FW:         // hexágonos girando na mão
                LIN(PTO(0, 0), PTO(8, 0), 3, metal);
                DrawPolyLinesEx(PTO(15, 0), 6, 9.0f * e, tempo * 40.0f, 2, cor);
                DrawPolyLinesEx(PTO(15, 0), 6, 4.5f * e, -tempo * 40.0f, 2, cor);
                if (j->ecoPulso > 0.0f) DrawPolyLinesEx(PTO(15, 0), 6, 14.0f, tempo * 120.0f, 2, Fade(WHITE, 0.8f));
                break;

            case ARMA_KATANA: {         // descansa baixa; no golpe a lâmina varre o mesmo arco do ataque
                float ang;
                if (j->katanaAnim > 0.0f) {
                    float k = 1.0f - j->katanaAnim / 0.18f;
                    ang = (j->katanaAng * RAD2DEG - j->katanaArco / 2.0f + j->katanaArco * Clamp(k * 1.7f, 0.0f, 1.0f)) * DEG2RAD;
                } else {
                    ang = atan2f(d.y, d.x) - 0.9f;
                }
                Vector2 dl = { cosf(ang), sinf(ang) };
                Vector2 punho = PTO(6, 0);
                Vector2 ponta = Vector2Add(punho, Vector2Scale(dl, 34.0f * e));
                DrawLineEx(Vector2Subtract(punho, Vector2Scale(dl, 8.0f)), punho, 4, (Color){ 60, 40, 90, 255 });
                DrawLineEx(Vector2Add(punho, (Vector2){ -dl.y * 5, dl.x * 5 }), Vector2Add(punho, (Vector2){ dl.y * 5, -dl.x * 5 }), 3, GOLD);
                DrawLineEx(punho, ponta, 3, (Color){ 225, 235, 255, 255 });
                DrawLineEx(punho, ponta, 1, WHITE);
                if (j->katanaAnim > 0.0f) DrawCircleV(ponta, 4.0f, Fade(WHITE, 0.6f));
            } break;

            default:                    // Disparador de Dados
                LIN(PTO(0, 0), PTO(20, 0), 5, metal);
                DrawLineEx(PTO(0, 0), PTO(20, 0), 2, SKYBLUE);
                DrawCircleV(PTO(20, 0), 2.5f, WHITE);
                break;
        }

        // A mão do Thomas segurando a arma e o clarão da boca logo depois do tiro
        if (arma != ARMA_KATANA && arma != ARMA_ORBE)
            DrawCircleV(PTO(2, 0), 3.2f, (Color){ 255, 214, 170, 255 });
        if (coice > 0.6f && arma != ARMA_KATANA && arma != ARMA_PULSO_FW && arma != ARMA_VETOR) {
            float boca = (arma == ARMA_FRAGMENTADORA) ? 24.0f : ((arma == ARMA_ORBE) ? 29.0f : ((arma == ARMA_RAIO || arma == ARMA_RAIO_CLASSICO) ? 28.0f : 21.0f));
            DrawCircleV(PTO(boca, 0), 3.0f + 4.0f * coice, Fade(WHITE, 0.55f * coice));
            DrawCircleV(PTO(boca, 0), 2.0f, Fade(cor, 0.9f));
        }
    }

    #undef LIN
    #undef PTO

    // Troca de arma: anel que se abre e o nome dela sobre a cabeça
    if (j->trocaArma > 0.0f) {
        float t = j->trocaArma / 1.3f;                           // 1 -> 0
        float a = Clamp(t * 2.5f, 0.0f, 1.0f);
        const char *nome = def->nome;
        int w = MeasureText(nome, 10);
        int cx = (int)j->pos.x, cy = (int)j->pos.y - 66;

        DrawCircleLines((int)j->pos.x, (int)j->pos.y, 14.0f + 34.0f * (1.0f - t), Fade(cor, t));
        DrawRectangle(cx - w / 2 - 24, cy - 3, w + 34, 18, Fade(BLACK, 0.7f * a));
        DrawRectangleLines(cx - w / 2 - 24, cy - 3, w + 34, 18, Fade(cor, 0.8f * a));
        DesenharIconeArma(arma, (Vector2){ (float)(cx - w / 2 - 11), (float)(cy + 6) }, 0.6f, tempo);
        DrawText(nome, cx - w / 2 + 2, cy + 1, 10, Fade(cor, a));
    }
}

void DesenharEfeitosJogador(const Biblioteca *b, float tempo)
{
    const JogadorRogue *j = &b->j;

    // Katana de Bytes: o arco do golpe varre a frente do Thomas
    if (j->katanaAnim > 0.0f) {
        float k = 1.0f - j->katanaAnim / 0.18f;
        float ini = j->katanaAng * RAD2DEG - j->katanaArco / 2.0f;
        float fim = ini + j->katanaArco * Clamp(k * 1.7f, 0.0f, 1.0f);
        DrawRing(j->pos, j->katanaAlcance * 0.45f, j->katanaAlcance * 0.95f, ini, fim, 24, Fade(WHITE, 0.55f * (1.0f - k)));
        DrawRing(j->pos, j->katanaAlcance * 0.82f, j->katanaAlcance * 1.0f, ini, fim, 24, Fade((Color){ 170, 230, 255, 255 }, 0.95f * (1.0f - k)));
    }

    if (j->efeito[EFEITO_LATENCIA] > 0.0f) {
        DrawCircleV(j->pos, LATENCIA_RAIO, Fade(PURPLE, 0.06f));
        for (int k = 0; k < 12; k++) {                 // marcas de relógio girando devagar
            float a = tempo * 0.5f + k * (PI / 6.0f);
            DrawLineEx((Vector2){ j->pos.x + cosf(a) * (LATENCIA_RAIO - 8), j->pos.y + sinf(a) * (LATENCIA_RAIO - 8) },
                       (Vector2){ j->pos.x + cosf(a) * LATENCIA_RAIO, j->pos.y + sinf(a) * LATENCIA_RAIO }, 2, Fade(PURPLE, 0.6f));
        }
    }

    if (j->efeito[EFEITO_ESCUDO] > 0.0f) {
        float pisca = (j->efeito[EFEITO_ESCUDO] < 2.0f && ((int)(tempo * 8.0f)) % 2 == 0) ? 0.3f : 0.9f;   // acabando: pisca
        DrawPolyLinesEx(j->pos, 6, 24, tempo * 60.0f, 3, Fade((Color){ 0, 228, 48, 255 }, pisca));
        DrawCircleV(j->pos, 24, Fade((Color){ 0, 228, 48, 255 }, 0.08f));
    }

    if (j->efeito[EFEITO_OVERCLOCK] > 0.0f)
        DrawRing(j->pos, 17, 19, tempo * 400.0f, tempo * 400.0f + 120.0f, 10, ORANGE);

    if (j->carga > 0.0f) {
        // Vetor: o anel enche e a mira laser cresce com a carga
        float k = j->carga / VETOR_CARGA_MAX;
        DrawRing(j->pos, 20, 23, -90.0f, -90.0f + 360.0f * k, 24, (k >= 1.0f) ? WHITE : SKYBLUE);
        DrawLineEx(j->pos, Vector2Add(j->pos, Vector2Scale(j->mira, 120.0f + 380.0f * k)), 1.0f, Fade(SKYBLUE, 0.3f + 0.4f * k));
    }

    if (j->calor > 0.0f) {
        // Fluxo: barrinha de calor embaixo do jogador
        int x = (int)j->pos.x - 16, y = (int)j->pos.y + 24;
        DrawRectangle(x, y, 32, 4, (Color){ 40, 30, 20, 255 });
        DrawRectangle(x, y, (int)(32 * j->calor), 4, j->superaquecida ? RED : ColorLerp(YELLOW, ORANGE, j->calor));
        if (j->superaquecida && ((int)(tempo * 6.0f)) % 2 == 0)
            DrawText("QUENTE", (int)j->pos.x - MeasureText("QUENTE", 8) / 2, y + 6, 8, RED);
    }
}

// Proxy-Isca: um holograma em arame (bem diferente do jogador)
void DesenharProxy(const JogadorRogue *j, float tempo)
{
    if (j->efeito[EFEITO_PROXY] <= 0.0f || ((int)(tempo * 15.0f)) % 7 == 0)
        return;

    Vector2 p = j->proxyPos;
    Color ciano = Fade(SKYBLUE, 0.75f);

    DrawCircleLines((int)p.x, (int)p.y - 14, 7, ciano);
    DrawRectangleLines((int)p.x - 8, (int)p.y - 6, 16, 18, ciano);
    DrawLine((int)p.x - 5, (int)p.y + 12, (int)p.x - 5, (int)p.y + 20, ciano);
    DrawLine((int)p.x + 5, (int)p.y + 12, (int)p.x + 5, (int)p.y + 20, ciano);
    for (int k = 0; k < 4; k++)                          // linhas de varredura
        DrawLine((int)p.x - 8, (int)p.y - 4 + k * 4 + ((int)(tempo * 20.0f) % 4), (int)p.x + 8, (int)p.y - 4 + k * 4 + ((int)(tempo * 20.0f) % 4), Fade(SKYBLUE, 0.3f));
    DrawText("PROXY", (int)p.x - MeasureText("PROXY", 8) / 2, (int)p.y + 22, 8, ciano);
    DrawRectangle((int)p.x - 12, (int)p.y - 26, (int)(24 * j->proxyHp / PROXY_HP), 2, ciano);
}

// Analisador Heurístico: losangos sobre o alvo (um por marca)
void DesenharMarcasAnalise(const Biblioteca *b)
{
    const JogadorRogue *j = &b->j;
    Vector2 pos;
    float raio;

    if (j->alvoAnalise == 0 || j->marcasAnalise == 0)
        return;

    if (j->alvoAnalise > 0) { pos = b->ini[j->alvoAnalise - 1].pos; raio = b->ini[j->alvoAnalise - 1].raio; }
    else if (b->chefe.ativo) { pos = b->chefe.pos; raio = CHEFE_RAIO; }
    else if (b->silogista.ativo) { pos = b->silogista.pos; raio = SIL_RAIO; }
    else return;

    for (int k = 0; k < ANALISADOR_MARCAS; k++) {
        Vector2 q = { pos.x - 10 + k * 10, pos.y + raio + 10 };
        if (k < j->marcasAnalise) DrawPoly(q, 4, 4, 0, (j->marcasAnalise >= ANALISADOR_MARCAS) ? RED : ORANGE);
        else                      DrawPolyLines(q, 4, 4, 0, Fade(ORANGE, 0.5f));
    }
}

// HUD da expansão: arma equipada, efeitos com tempo e upgrades passivos
void DesenharHudArmas(const Biblioteca *b, float tempo)
{
    const JogadorRogue *j = &b->j;
    TipoArma arma = RogueArmaAtual(j);

    // Arma (canto de baixo, à esquerda). Com duas, mostra a outra e a tecla de troca.
    Rectangle r = { 44, 506, 214, 22 };
    DrawRectangleRec(r, Fade((Color){ 12, 10, 24, 255 }, 0.8f));
    DrawRectangleLinesEx(r, 1, Fade(DEF_ARMAS[arma].cor, 0.7f));
    DesenharIconeArma(arma, (Vector2){ r.x + 16, r.y + 11 }, 0.8f, tempo);
    {
        int espaco = (int)r.width - 34 - ((j->armaExtra != ARMA_DISPARADOR) ? 52 : 6);     // sobra do nome até o ícone da outra arma
        int tamNome = (MeasureText(DEF_ARMAS[arma].nome, 10) > espaco) ? 8 : 10;
        DrawText(DEF_ARMAS[arma].nome, (int)r.x + 34, (int)r.y + 6 + (10 - tamNome) / 2, tamNome, DEF_ARMAS[arma].cor);
    }

    if (j->armaExtra != ARMA_DISPARADOR) {
        TipoArma outra = j->usandoExtra ? j->armaBase : j->armaExtra;
        DesenharIconeArma(outra, (Vector2){ r.x + r.width - 34, r.y + 11 }, 0.6f, tempo);
        DrawText("TAB", (int)(r.x + r.width) - 22, (int)r.y + 7, 8, GRAY);
    }

    // Efeitos com tempo (canto de baixo, à direita): ícone e uma barra com o que falta
    static const TipoItem itemDoEfeito[NUM_EFEITOS] = { ITEM_OVERCLOCK, ITEM_FIREWALL_PORTATIL, ITEM_PROXY, ITEM_CAMPO_LATENCIA };
    static const float duracao[NUM_EFEITOS] = { EFEITO_OVERCLOCK_DURACAO, ESCUDO_PORTATIL_DURACAO, PROXY_DURACAO, LATENCIA_DURACAO };
    int x = 734;

    for (int e = 0; e < NUM_EFEITOS; e++) {
        if (j->efeito[e] <= 0.0f) continue;
        DrawRectangle(x - 12, 506, 24, 22, Fade((Color){ 12, 10, 24, 255 }, 0.8f));
        DesenharIconeItem(itemDoEfeito[e], (Vector2){ (float)x, 515 }, 0.6f, tempo);
        DrawRectangle(x - 11, 525, (int)(22 * fminf(1.0f, j->efeito[e] / duracao[e])), 2, GOLD);
        x -= 28;
    }

    // Upgrades passivos da expansão (coluna na margem direita), com a quantidade
    int uy = 76;
    for (int i = PRIMEIRO_UPGRADE; i < NUM_ITENS; i++) {
        if (j->itens[i] <= 0) continue;
        DrawRectangle(770, uy - 11, 22, 22, Fade(BLACK, 0.6f));
        DrawRectangleLines(770, uy - 11, 22, 22, Fade(CORES_RARIDADE[ITENS[i].raridade], 0.8f));
        DesenharIconeItem((TipoItem)i, (Vector2){ 781, (float)uy }, 0.55f, tempo);
        if (ITENS[i].maxPilha > 1)
            DrawText(TextFormat("%d", RogueQtd(j, (TipoItem)i)), 785, uy + 2, 8, WHITE);
        uy += 26;
    }
}

// Anéis de explosão, de onda e de limpeza
void DesenharAneis(const Biblioteca *b)
{
    for (int i = 0; i < MAX_ANEIS; i++) {
        const AnelEfeito *a = &b->aneis[i];
        if (a->tempo <= 0.0f) continue;
        float k = 1.0f - a->tempo / 0.35f, sobra = 1.0f - k;
        float r = a->raio * (1.0f - sobra * sobra);                              // abre rápido e desacelera
        DrawCircleV(a->pos, r, Fade(a->cor, 0.14f * sobra));
        DrawRing(a->pos, fmaxf(0.0f, r - 1.5f - 3.0f * sobra), r, 0.0f, 360.0f, 36, Fade(a->cor, 0.9f * sobra));
        if (k < 0.3f)                                                            // clarão no começo
            DrawCircleV(a->pos, a->raio * 0.25f * (1.0f - k / 0.3f), Fade(WHITE, 0.5f * (1.0f - k / 0.3f)));
    }
}

// Marcas do Scanner de Ecos (as de uma varredura alucinada tremem) e a pista "ECO?" na Luci
void DesenharScannerLuci(const Biblioteca *b, float tempo)
{
    if (b->scanner > 0.0f) {
        for (int i = 0; i < MAX_INIMIGOS; i++) {

            const Inimigo *e = &b->ini[i];
            const char *marca = NULL;

            if (!e->ativo) continue;
            if (e->tipo == INIMIGO_PHISHER)   marca = b->scannerErrou ? "FALSO" : "PHISHER";
            else if (e->tipo == INIMIGO_ISCA) marca = b->scannerErrou ? "REAL" : "FALSO";
            else if (e->tipo == INIMIGO_ECO)  marca = "ECO";
            if (marca == NULL) continue;

            float dx = b->scannerErrou ? (float)GetRandomValue(-2, 2) : 0.0f;
            int w = MeasureText(marca, 10);
            int x = (int)(e->pos.x + dx) - w / 2 - 4, y = (int)(e->pos.y - e->raio - 30);
            DrawRectangle(x, y, w + 8, 14, Fade(BLACK, 0.7f));
            DrawRectangleLines(x, y, w + 8, 14, (Color){ 0, 228, 255, 255 });
            DrawText(marca, x + 4, y + 2, 10, (Color){ 0, 228, 255, 255 });

            // Cantoneiras em volta do alvo
            float r = e->raio + 6;
            for (int c = 0; c < 4; c++) {
                float sx = (c % 2) ? 1.0f : -1.0f, sy = (c / 2) ? 1.0f : -1.0f;
                Vector2 q = { e->pos.x + dx + sx * r, e->pos.y + sy * r };
                DrawLineEx(q, (Vector2){ q.x - sx * 6, q.y }, 2, (Color){ 0, 228, 255, 255 });
                DrawLineEx(q, (Vector2){ q.x, q.y - sy * 6 }, 2, (Color){ 0, 228, 255, 255 });
            }
        }
    }

    if (b->luciEco > 0.0f && ((int)(tempo * 20.0f)) % 3 != 0) {
        Vector2 p = { b->j.pos.x + 22, b->j.pos.y - 46 };
        DrawRectangle((int)p.x - 16, (int)p.y, 32, 14, Fade(BLACK, 0.7f));
        DrawText("ECO?", (int)p.x - MeasureText("ECO?", 10) / 2, (int)p.y + 2, 10, MAGENTA);
    }
}

// Estado da Luci (estabilidade, nome do estado e scanner) acima da arma

void DesenharHudLuci(const Biblioteca *b, float tempo)
{
    int e = RogueEstab(b);
    const char *estado = (e >= 4) ? "ESTÁVEL" : ((e == 3) ? "INSTÁVEL" : ((e >= 1) ? "GLITCHANDO" : "ECOANDO"));
    Color cor = (e >= 4) ? (Color){ 0, 228, 255, 255 } : ((e == 3) ? GOLD : ((e >= 1) ? ORANGE : MAGENTA));
    Rectangle r = { 44, 484, 214, 18 };
    int dx = (e <= 2 && ((int)(tempo * 9.0f)) % 7 == 0) ? GetRandomValue(-2, 2) : 0;     // glitchando: treme

    // Bloco 3: "data bleed" (a cor oscila entre o azul e o vermelho/magenta)
    if (RogueNoBloco3(b)) {
        float q = 0.5f + 0.5f * sinf(tempo * 5.0f + sinf(tempo * 1.7f) * 3.0f);
        Color a = (Color){ 102, 191, 255, 255 }, c = (((int)(tempo * 1.5f)) % 2) ? (Color){ 230, 41, 55, 255 } : MAGENTA;
        cor = (Color){ (unsigned char)(a.r + (c.r - a.r) * q), (unsigned char)(a.g + (c.g - a.g) * q), (unsigned char)(a.b + (c.b - a.b) * q), 255 };
        if (((int)(tempo * 11.0f)) % 9 == 0) dx = GetRandomValue(-3, 3);
    }

    DrawRectangleRec(r, Fade((Color){ 12, 10, 24, 255 }, 0.8f));
    DrawRectangleLinesEx(r, 1, Fade(cor, 0.7f));
    DrawText("LUCI", (int)r.x + 6 + dx, (int)r.y + 4, 10, cor);

    for (int k = 0; k < ESTABILIDADE_MAX_LUCI; k++) {
        Rectangle pip = { r.x + 38 + k * 10, r.y + 5, 7, 8 };
        if (k < e) DrawRectangleRec(pip, cor);
        DrawRectangleLinesEx(pip, 1, cor);
    }
    DrawText(estado, (int)r.x + 94 + dx, (int)r.y + 4, 10, cor);

    // Carga de ajuda dos laboratórios: quando enche, vira -1 de estabilidade
    if (b->cargaLuci > 0)
        DrawRectangle((int)r.x + 38, (int)r.y + 14, (int)(47.0f * (float)b->cargaLuci / LAB_CARGA_POR_ESTAB), 2, ORANGE);

    // Scanner (H): pronto ou recarregando
    Vector2 c = { r.x + r.width - 12, r.y + 9 };
    if (b->recargaScanner > 0.0f)
        DrawCircleSector(c, 7, -90.0f, -90.0f + 360.0f * (1.0f - b->recargaScanner / SCANNER_RECARGA), 16, Fade(SKYBLUE, 0.5f));
    else
        DrawCircleV(c, 7, Fade(SKYBLUE, 0.6f));
    DrawCircleLines((int)c.x, (int)c.y, 7, SKYBLUE);
    DrawText("H", (int)c.x - 3, (int)c.y - 4, 8, WHITE);

    // (o recado curto da Luci agora aparece no balão do topo: DesenharFalaLuciHUD)
}
