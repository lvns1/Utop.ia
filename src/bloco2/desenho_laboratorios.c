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

// =============================================================
// EXPANSÃO EDUCATIVA DO BLOCO 2: DESENHO DOS LABORATÓRIOS
// =============================================================

// Ícone de cada categoria (console da sala e janela). e = escala (1 = cerca de 24 px)
static void DesenharIconeLab(TipoLab t, Vector2 c, float e, float tempo)
{
    Color cor = CORES_LAB[t];

    switch (t) {
        case LAB_PROMPTS:       // balão de conversa com o cursor do terminal
            DrawRectangleRounded((Rectangle){ c.x - 11 * e, c.y - 8 * e, 22 * e, 14 * e }, 0.3f, 6, Fade(cor, 0.9f));
            DrawTriangle((Vector2){ c.x - 6 * e, c.y + 6 * e }, (Vector2){ c.x - 8 * e, c.y + 11 * e }, (Vector2){ c.x - 1 * e, c.y + 6 * e }, Fade(cor, 0.9f));
            DrawText(">_", (int)(c.x - 7 * e), (int)(c.y - 6 * e), (int)fmaxf(8.0f, 10 * e), (Color){ 10, 20, 40, 255 });
            if (((int)(tempo * 2.0f)) % 2 == 0) DrawRectangle((int)(c.x + 5 * e), (int)(c.y - 4 * e), (int)fmaxf(1.0f, 3 * e), (int)fmaxf(2.0f, 7 * e), (Color){ 10, 20, 40, 255 });
            break;
        case LAB_ARQUIVO:       // documento com uma linha corrompida
            DrawRectangleV((Vector2){ c.x - 8 * e, c.y - 10 * e }, (Vector2){ 16 * e, 20 * e }, Fade(WHITE, 0.85f));
            for (int k = 0; k < 4; k++)
                DrawRectangleV((Vector2){ c.x - 5 * e, c.y - 6 * e + k * 4 * e }, (Vector2){ (k == 2 ? 6 : 10) * e, 1.5f * e }, (Color){ 60, 60, 80, 255 });
            DrawRectangleV((Vector2){ c.x - 6 * e + sinf(tempo * 9.0f) * 2 * e, c.y + 1 * e }, (Vector2){ 12 * e, 2 * e }, cor);
            break;
        case LAB_NUCLEO:        // núcleo com dados em órbita
            DrawCircleV(c, 6 * e, cor);
            DrawCircleLines((int)c.x, (int)c.y, 10 * e, Fade(cor, 0.7f));
            for (int k = 0; k < 3; k++) {
                float a = tempo * 2.0f + k * 2.1f;
                DrawCircleV((Vector2){ c.x + cosf(a) * 10 * e, c.y + sinf(a) * 10 * e }, 2 * e, WHITE);
            }
            break;
        case LAB_FIREWALL:      // escudo
            DrawTriangle((Vector2){ c.x - 10 * e, c.y - 8 * e }, (Vector2){ c.x, c.y + 12 * e }, (Vector2){ c.x + 10 * e, c.y - 8 * e }, cor);
            DrawRectangleV((Vector2){ c.x - 10 * e, c.y - 11 * e }, (Vector2){ 20 * e, 4 * e }, cor);
            DrawLineEx((Vector2){ c.x, c.y - 9 * e }, (Vector2){ c.x, c.y + 8 * e }, 2 * e, (Color){ 10, 40, 20, 255 });
            break;
        default:                // lupa
            DrawCircleLines((int)(c.x - 2 * e), (int)(c.y - 2 * e), 7 * e, cor);
            DrawCircleLines((int)(c.x - 2 * e), (int)(c.y - 2 * e), 6 * e, cor);
            DrawLineEx((Vector2){ c.x + 3 * e, c.y + 3 * e }, (Vector2){ c.x + 10 * e, c.y + 10 * e }, 3 * e, cor);
            break;
    }
}

// Console do laboratório na sala (holograma da categoria)
void DesenharConsoleLab(const Biblioteca *b, float tempo)
{
    const LabEdu *L = &b->lab;
    if (L->emAcao && L->tipo == LAB_NUCLEO)
        return;                                     // em ação, o núcleo toma o lugar do console
    Vector2 c = b->posObjetoSala;
    Color cor = CORES_LAB[L->tipo];
    float k = L->concluido ? 0.4f : 1.0f;
    float flutua = sinf(tempo * 2.0f) * 3.0f;

    DrawEllipse((int)c.x, (int)c.y + 22, 40, 10, Fade(cor, 0.15f * k));
    DrawRectangle((int)c.x - 24, (int)c.y - 2, 48, 22, Fade((Color){ 30, 26, 50, 255 }, k));
    DrawRectangleLines((int)c.x - 24, (int)c.y - 2, 48, 22, Fade(cor, 0.8f * k));
    for (int i = 0; i < 3; i++)
        DrawRectangle((int)c.x - 18, (int)c.y + 3 + i * 5, 10 + (int)(Ruido((unsigned int)(tempo * 3.0f) + (unsigned int)i) % 26), 2, Fade(cor, 0.7f * k));

    if (!L->concluido) {
        // Feixe e holograma
        DrawTriangle((Vector2){ c.x - 14, c.y - 2 }, (Vector2){ c.x + 14, c.y - 2 }, (Vector2){ c.x, c.y - 30 + flutua }, Fade(cor, 0.12f));
        DrawCircleV((Vector2){ c.x, c.y - 34 + flutua }, 18, Fade(cor, 0.18f + 0.06f * sinf(tempo * 4.0f)));
        DesenharIconeLab(L->tipo, (Vector2){ c.x, c.y - 34 + flutua }, 1.0f, tempo);
    } else {
        DrawText("OK", (int)c.x - MeasureText("OK", 10) / 2, (int)c.y + 5, 10, Fade(LIME, 0.8f));
    }

    const char *nome = NOMES_LAB[L->tipo];
    DrawText(nome, (int)c.x - MeasureText(nome, 10) / 2, (int)c.y + 30, 10, Fade(cor, k));
    if (!L->concluido) {
        const char *t = "OPCIONAL";
        DrawText(t, (int)c.x - MeasureText(t, 8) / 2, (int)c.y + 42, 8, GOLD);
    }
}

static void DesenharPipsLab(float x, float y, int n, Color cor)
{
    for (int i = 0; i < n; i++)
        DrawCircleV((Vector2){ x + i * 8.0f, y }, 2.5f, cor);
}

// Ícone de cada peça do pedido: PAPEL (uma pessoa), CONTEXTO (o público), OBJETIVO (um alvo), FORMATO (uma lista)
static void DesenharIconeParte(int k, Vector2 c, float e, Color cor)
{
    switch (k) {
        case PP_PAPEL:
            DrawCircleV((Vector2){ c.x, c.y - 4 * e }, 4 * e, cor);
            DrawRectangleRounded((Rectangle){ c.x - 6 * e, c.y + 1 * e, 12 * e, 7 * e }, 0.6f, 4, cor);
            break;
        case PP_CONTEXTO:
            DrawCircleV((Vector2){ c.x - 4.5f * e, c.y - 3 * e }, 3 * e, cor);
            DrawCircleV((Vector2){ c.x + 4.5f * e, c.y - 3 * e }, 3 * e, cor);
            DrawRectangleRounded((Rectangle){ c.x - 8.5f * e, c.y + 1 * e, 8 * e, 6 * e }, 0.6f, 4, cor);
            DrawRectangleRounded((Rectangle){ c.x + 0.5f * e, c.y + 1 * e, 8 * e, 6 * e }, 0.6f, 4, cor);
            break;
        case PP_OBJETIVO:
            DrawCircleLines((int)c.x, (int)c.y, 7.5f * e, cor);
            DrawCircleLines((int)c.x, (int)c.y, 4.5f * e, cor);
            DrawCircleV(c, 1.8f * e, cor);
            break;
        default:
            for (int i = 0; i < 3; i++) {
                float y = c.y + (float)(i - 1) * 4.5f * e;
                DrawRectangle((int)(c.x - 7 * e), (int)(y - e), (int)fmaxf(2.0f, 2 * e), (int)fmaxf(2.0f, 2 * e), cor);
                DrawRectangle((int)(c.x - 3 * e), (int)(y - e), (int)(10 * e), (int)fmaxf(2.0f, 2 * e), cor);
            }
            break;
    }
}

// Borda tracejada (encaixe vazio)
static void DesenharBordaTracejada(Rectangle r, Color cor)
{
    for (float x = r.x; x < r.x + r.width; x += 10.0f) {
        float w = fminf(5.0f, r.x + r.width - x);
        DrawRectangle((int)x, (int)r.y, (int)w, 1, cor);
        DrawRectangle((int)x, (int)(r.y + r.height) - 1, (int)w, 1, cor);
    }
    for (float y = r.y; y < r.y + r.height; y += 10.0f) {
        float h = fminf(5.0f, r.y + r.height - y);
        DrawRectangle((int)r.x, (int)y, 1, (int)h, cor);
        DrawRectangle((int)(r.x + r.width) - 1, (int)y, 1, (int)h, cor);
    }
}

// ECO-1, a IA simulada do laboratório. humor: 0 neutro, 1 satisfeito, 2 confuso, 3 em erro
static void DesenharEco1(Vector2 c, int humor, float tempo)
{
    Color ciano = { 0, 228, 255, 255 };
    Color olho = (humor == 3) ? MAGENTA : ciano;

    DrawLineEx((Vector2){ c.x, c.y - 16 }, (Vector2){ c.x, c.y - 25 }, 2.0f, ciano);
    DrawCircleV((Vector2){ c.x, c.y - 27 }, 3, (((int)(tempo * 3.0f)) % 2 == 0) ? GOLD : Fade(GOLD, 0.4f));
    DrawRectangleRounded((Rectangle){ c.x - 26, c.y - 17, 52, 34 }, 0.35f, 6, Fade(olho, 0.8f));
    DrawRectangleRounded((Rectangle){ c.x - 24, c.y - 15, 48, 30 }, 0.35f, 6, (Color){ 26, 34, 70, 255 });
    DrawRectangle((int)c.x - 14, (int)c.y + 17, 28, 5, (Color){ 46, 40, 90, 255 });

    switch (humor) {
        case 1:         // ^ ^
            for (int s = -1; s <= 1; s += 2) {
                float ex = c.x + s * 10.0f;
                DrawLineEx((Vector2){ ex - 5, c.y - 1 }, (Vector2){ ex, c.y - 6 }, 2.0f, olho);
                DrawLineEx((Vector2){ ex, c.y - 6 }, (Vector2){ ex + 5, c.y - 1 }, 2.0f, olho);
            }
            break;
        case 2:         // um olho e uma dúvida
            DrawCircleV((Vector2){ c.x - 10, c.y - 3 }, 3, olho);
            DrawText("?", (int)c.x + 6, (int)c.y - 11, 14, GOLD);
            break;
        case 3:         // X X (com tremida)
            for (int s = -1; s <= 1; s += 2) {
                float ex = c.x + s * 10.0f + (float)GetRandomValue(-1, 1);
                DrawLineEx((Vector2){ ex - 4, c.y - 7 }, (Vector2){ ex + 4, c.y + 1 }, 2.0f, olho);
                DrawLineEx((Vector2){ ex + 4, c.y - 7 }, (Vector2){ ex - 4, c.y + 1 }, 2.0f, olho);
            }
            break;
        default:
            for (int s = -1; s <= 1; s += 2) {
                if (fmodf(tempo, 3.2f) < 0.12f) DrawRectangle((int)c.x + s * 10 - 3, (int)c.y - 3, 6, 2, olho);   // piscando
                else                            DrawCircleV((Vector2){ c.x + s * 10.0f, c.y - 3 }, 3, olho);
            }
            break;
    }
    DrawRectangle((int)c.x - 8, (int)c.y + 7, 16, 2, Fade(olho, 0.7f));
}

// O que a resposta virou (carimbo do holograma), por diagnóstico
static const char *CARIMBOS_DIAG[NUM_DIAG_PROMPT] = {
    "NA MEDIDA!", "QUASE LÁ", "GENÉRICA\nDEMAIS", "PÚBLICO\nERRADO", "PAPEL\nERRADO", "FORMATO\nRUIM", "PEDIDO\nCONTRADITÓRIO"
};
static const Color CORES_DIAG[NUM_DIAG_PROMPT] = {
    { 255, 203, 0, 255 }, { 0, 228, 48, 255 }, { 150, 150, 170, 255 }, { 255, 161, 0, 255 },
    { 255, 161, 0, 255 }, { 255, 161, 0, 255 }, { 255, 0, 255, 255 }
};

static void DesenharPictogramaDiag(DiagPrompt d, Vector2 c, float tempo)
{
    Color cd = CORES_DIAG[d];

    if (d == DIAG_IDEAL || d == DIAG_BOA) {
        DrawCircleLines((int)c.x, (int)c.y, 20, cd);
        if (d == DIAG_IDEAL) DrawCircleLines((int)c.x, (int)c.y, 23, Fade(cd, 0.5f + 0.5f * sinf(tempo * 5.0f)));
        DrawLineEx((Vector2){ c.x - 10, c.y + 1 }, (Vector2){ c.x - 3, c.y + 9 }, 4.0f, cd);
        DrawLineEx((Vector2){ c.x - 3, c.y + 9 }, (Vector2){ c.x + 11, c.y - 8 }, 4.0f, cd);
        return;
    }
    if (d == DIAG_CONTRADICAO) {         // duas setas puxando para lados opostos
        float t = (float)GetRandomValue(-1, 1);
        DrawLineEx((Vector2){ c.x - 4, c.y - 6 + t }, (Vector2){ c.x - 20, c.y - 6 + t }, 3.0f, cd);
        DrawTriangle((Vector2){ c.x - 26, c.y - 6 + t }, (Vector2){ c.x - 18, c.y - 1 + t }, (Vector2){ c.x - 18, c.y - 11 + t }, cd);
        DrawLineEx((Vector2){ c.x + 4, c.y + 6 - t }, (Vector2){ c.x + 20, c.y + 6 - t }, 3.0f, cd);
        DrawTriangle((Vector2){ c.x + 26, c.y + 6 - t }, (Vector2){ c.x + 18, c.y + 1 - t }, (Vector2){ c.x + 18, c.y + 11 - t }, cd);
        return;
    }

    // A peça que falhou, riscada (VAGA = o objetivo, embaçado)
    int parte = (d == DIAG_VAGA) ? PP_OBJETIVO : (d == DIAG_PUBLICO) ? PP_CONTEXTO : (d == DIAG_PAPEL) ? PP_PAPEL : PP_FORMATO;
    DesenharIconeParte(parte, c, 2.3f, Fade(CORES_PARTES_PROMPT[parte], (d == DIAG_VAGA) ? 0.45f : 0.9f));
    if (d == DIAG_VAGA) {
        DrawText("?", (int)c.x + 10, (int)c.y - 22, 20, cd);
    } else {
        DrawLineEx((Vector2){ c.x + 8, c.y + 6 }, (Vector2){ c.x + 20, c.y + 18 }, 3.0f, RED);
        DrawLineEx((Vector2){ c.x + 20, c.y + 6 }, (Vector2){ c.x + 8, c.y + 18 }, 3.0f, RED);
    }
}

static void DesenharBotaoLab(const BotaoLab *o, bool foco, float tempo)
{
    Rectangle r = o->r;
    Color c = o->ativo ? o->cor : Fade(GRAY, 0.6f);

    switch (o->estilo) {

        case EST_ENCAIXE: {         // laboratório de prompts: um encaixe da "frase" do pedido
            int k = o->param;
            Color ck = o->cor;
            bool sel = o->marcado;

            DrawRectangleRec(r, Fade(BLACK, 0.5f));
            if (sel) DrawRectangleRec(r, Fade(ck, 0.10f + 0.06f * sinf(tempo * 5.0f)));
            if (o->texto == NULL && !sel && o->luz < 0) DesenharBordaTracejada(r, Fade(ck, 0.6f));
            else DrawRectangleLinesEx(r, (sel || foco) ? 2.0f : 1.0f, (sel || foco) ? ck : Fade(ck, 0.6f));

            DrawRectangle((int)r.x, (int)r.y, (int)r.width, 18, Fade(ck, 0.22f));
            DesenharIconeParte(k, (Vector2){ r.x + 11, r.y + 9 }, 0.75f, ck);
            // O resultado (IDEAL, ATRAPALHOU...) fica à direita; se não couber do lado do nome (texto grande),
            // ele toma o lugar do nome (o ícone e a cor continuam dizendo qual é o encaixe)
            bool resultadoNoLugar = false;
            if (o->luz >= 0)
                resultadoNoLugar = (int)r.x + 22 + MeasureText(o->rotulo, 10) + 6 > (int)(r.x + r.width) - MeasureText(NOMES_LUZ[o->luz], 10) - 13;
            if (!resultadoNoLugar)
                DrawText(o->rotulo, (int)r.x + 22, (int)r.y + 4, 10, sel ? WHITE : ck);

            if (o->luz >= 0) {
                const char *nl = NOMES_LUZ[o->luz];
                Color cl = CORES_LUZ[o->luz];
                int w = MeasureText(nl, 10) + 10;
                int xl = resultadoNoLugar ? (int)r.x + 20 : (int)(r.x + r.width) - w - 3;
                DrawRectangle(xl, (int)r.y + 3, w, LinhaTexto(13), cl);
                DrawText(nl, xl + 5, (int)r.y + 4, 10, BLACK);
                DrawRectangleLinesEx(r, 2.0f, Fade(cl, 0.45f + 0.3f * sinf(tempo * 4.0f)));
            } else if (o->chip) {
                Color ciano = { 0, 228, 255, 255 };
                DrawRectangle((int)(r.x + r.width) - 42, (int)r.y + 3, 39, 13, ciano);
                DrawText("LUCI", (int)(r.x + r.width) - 35, (int)r.y + 4, 10, BLACK);
            } else {
                // A dica ("QUEM responde"...) só aparece se couber do lado do nome (com o texto grande, às vezes não cabe)
                const char *d = DICAS_PARTES_PROMPT[k];
                int xd = (int)(r.x + r.width) - MeasureText(d, 10) - 6;
                if (xd > (int)r.x + 22 + MeasureText(o->rotulo, 10) + 6)
                    DrawText(d, xd, (int)r.y + 4, 10, Fade(LIGHTGRAY, 0.6f));
            }

            const char *t = (o->texto != NULL) ? o->texto : (sel ? "escolha a peça abaixo" : "(vazio)");
            char linhas[256];
            QuebrarTexto(t, (int)r.width - 12, 10, linhas, (int)sizeof(linhas));
            int quebras = 0;                                    // no máximo 3 linhas
            for (char *q = linhas; *q; q++) if (*q == '\n' && ++quebras == 3) { *q = '\0'; break; }
            Color ct = (o->texto == NULL) ? Fade(GRAY, 0.9f) : (o->chip ? (Color){ 160, 240, 255, 255 } : WHITE);
            DesenharTextoLinhas(linhas, (int)r.x + 6, (int)r.y + 23, 10, 11, ct);

            if (sel) {      // seta apontando para as peças
                float cx = r.x + r.width / 2, y = r.y + r.height + 2;
                DrawTriangle((Vector2){ cx - 7, y }, (Vector2){ cx, y + 8 }, (Vector2){ cx + 7, y }, ck);
            }
        } break;

        case EST_PECA: {            // laboratório de prompts: uma peça para o encaixe ativo
            bool m = o->marcado;
            Color ck = o->cor;
            DrawRectangleRec(r, foco ? Fade(ck, 0.20f) : Fade(BLACK, 0.42f));
            DrawRectangle((int)r.x, (int)r.y, 6, (int)r.height, (o->param == 0) ? Fade(ck, 0.4f) : ck);     // a lingueta que encaixa
            DrawRectangle((int)r.x + 6, (int)(r.y + r.height / 2) - 6, 5, 12, (o->param == 0) ? Fade(ck, 0.4f) : ck);
            DrawRectangleLinesEx(r, (foco || m) ? 2.0f : 1.0f, (foco || m) ? ck : Fade(ck, 0.35f));

            int yb = (int)(r.y + r.height / 2) - 8;
            DrawRectangle((int)r.x + 18, yb, 18, 16, m ? ck : Fade(ck, 0.3f));
            DrawText(o->rotulo, (int)r.x + 27 - MeasureText(o->rotulo, 10) / 2, yb + 3, 10, m ? BLACK : WHITE);

            char linhas[256];
            QuebrarTexto(o->texto != NULL ? o->texto : "", (int)r.width - 110, 10, linhas, (int)sizeof(linhas));
            int nl = ContarLinhas(linhas);
            DesenharTextoLinhas(linhas, (int)r.x + 44, (int)(r.y + r.height / 2) - (nl * EspacoLinhaTexto(12, 10)) / 2 + 1, 10, 12,
                                (o->param == 0) ? LIGHTGRAY : WHITE);
            if (m) {
                const char *u = "NO ENCAIXE";
                DrawText(u, (int)(r.x + r.width) - MeasureText(u, 10) - 8, (int)(r.y + r.height / 2) - 5, 10, ck);
            }
            if (foco) DrawRectangle((int)r.x + 3, (int)r.y + 4, 2, (int)r.height - 8, Fade(WHITE, 0.5f + 0.5f * sinf(tempo * 8.0f)));
        } break;

        case EST_SELETOR: {
            DrawRectangleRec(r, foco ? Fade(c, 0.18f) : Fade(BLACK, 0.45f));
            DrawRectangleLinesEx(r, foco ? 2.0f : 1.0f, foco ? c : Fade(c, 0.5f));
            DrawText(o->rotulo, (int)r.x + 22, (int)r.y + 5, 10, c);
            char linhas[256];
            QuebrarTexto((o->texto != NULL) ? o->texto : "(não incluir)", (int)r.width - 50, 10, linhas, (int)sizeof(linhas));
            int nl = ContarLinhas(linhas);
            DesenharTextoLinhas(linhas, (int)r.x + 22, (int)r.y + ((nl > 1) ? 16 : 20), 10, 11,
                                (o->texto != NULL) ? (o->ativo ? WHITE : LIGHTGRAY) : Fade(GRAY, 0.9f));
            if (o->ativo) {
                float my = r.y + r.height / 2;
                DrawTriangle((Vector2){ r.x + 6, my }, (Vector2){ r.x + 14, my + 6 }, (Vector2){ r.x + 14, my - 6 }, Fade(c, foco ? 1.0f : 0.6f));
                DrawTriangle((Vector2){ r.x + r.width - 6, my }, (Vector2){ r.x + r.width - 14, my - 6 }, (Vector2){ r.x + r.width - 14, my + 6 }, Fade(c, foco ? 1.0f : 0.6f));
            }
        } break;

        case EST_ITEM: {
            bool m = o->marcado;
            DrawRectangleRec(r, m ? Fade(o->cor, 0.22f) : Fade(BLACK, 0.40f));
            DrawRectangleLinesEx(r, (foco || m) ? 2.0f : 1.0f, foco ? (o->ativo ? o->cor : GRAY) : Fade(o->cor, m ? 0.9f : 0.35f));

            int xt = (int)r.x + 10;
            if (o->rotulo[0] != '\0' && !o->chip) {
                DrawRectangle((int)r.x + 6, (int)r.y + 6, 18, 16, m ? o->cor : Fade(o->cor, 0.35f));
                DrawText(o->rotulo, (int)r.x + 15 - MeasureText(o->rotulo, 10) / 2, (int)r.y + 9, 10, m ? BLACK : WHITE);
                xt = (int)r.x + 32;
            } else if (o->rotulo[0] == '\0') {
                // Caixa de marcar (pistas da Câmara de Pesquisa)
                DrawRectangleLines((int)r.x + 8, (int)(r.y + r.height / 2) - 6, 12, 12, o->cor);
                if (m) DrawText("!", (int)r.x + 12, (int)(r.y + r.height / 2) - 6, 12, o->cor);
                xt = (int)r.x + 28;
            }
            if (o->chip) {
                int w = MeasureText(o->rotulo, 10) + 10;
                DrawRectangle((int)(r.x + r.width) - w - 4, (int)r.y + 4, w, 14, m ? o->cor : Fade(o->cor, 0.3f));
                DrawText(o->rotulo, (int)(r.x + r.width) - w + 1, (int)r.y + 6, 10, m ? BLACK : WHITE);
            }

            int espaco = (o->fonte >= 12) ? 16 : 13;
            int larg = (int)r.width - ((xt - (int)r.x) + (o->chip ? 54 : 14));
            char linhas[512];
            QuebrarTexto(o->texto != NULL ? o->texto : "", larg, o->fonte, linhas, (int)sizeof(linhas));
            int nl = ContarLinhas(linhas);
            int yt = o->chip ? (int)r.y + 6 : (int)(r.y + r.height / 2) - (nl * EspacoLinhaTexto(espaco, o->fonte)) / 2 + 1;
            DesenharTextoLinhas(linhas, xt, yt, o->fonte, espaco, (o->ativo || m) ? WHITE : LIGHTGRAY);
        } break;

        default: {      // botão comum e botões da Luci
            DrawRectangleRec(r, foco ? Fade(c, 0.35f) : Fade(c, o->marcado ? 0.25f : 0.10f));
            DrawRectangleLinesEx(r, foco ? 2.0f : 1.0f, c);
            DrawText(o->rotulo, (int)r.x + LAB_PAD_BOTAO, (int)(r.y + r.height / 2) - LinhaTexto(6), 10, o->ativo ? WHITE : GRAY);
            if (o->estilo == EST_LUCI && o->custo > 0)
                DesenharPipsLab(r.x + LAB_PAD_BOTAO + 4 + (float)MeasureText(o->rotulo, 10), r.y + r.height / 2, o->custo, c);
            if (foco && o->ativo)
                DrawRectangle((int)r.x + 3, (int)r.y + 4, 2, (int)r.height - 8, Fade(WHITE, 0.5f + 0.5f * sinf(tempo * 8.0f)));
        } break;
    }
}

// Marca "LUCI" no que a sugestão dela aponta (para o jogador conferir com calma)
static void DesenharMarcaLuciLab(const Biblioteca *b, const BotaoLab *bt, int n, float tempo)
{
    const LabEdu *L = &b->lab;
    if (!L->luciTemSugestao || L->luciAponta < 0 || L->etapa != LAB_DESAFIO)
        return;

    AcaoLab alvo;
    if (L->tipo == LAB_ARQUIVO)       { alvo = (L->luciFase == 0) ? LB_ITEM : LB_OPCAO; if (L->luciFase != L->fase) return; }
    else if (L->tipo == LAB_FIREWALL) { alvo = (L->fase < 2) ? LB_OPCAO : LB_PACOTE; if (L->fase < 2 ? (L->luciFase != L->fase) : (L->luciFase != 20 + L->pacote)) return; }
    else if (L->tipo == LAB_NUCLEO || L->tipo == LAB_PESQUISA) alvo = LB_ITEM;
    else return;

    for (int i = 0; i < n; i++) {
        if (bt[i].acao != alvo || bt[i].param != L->luciAponta) continue;
        Color cor = L->luciVerificada ? (L->luciAlucinou ? RED : LIME) : (Color){ 0, 228, 255, 255 };
        int x = (int)(bt[i].r.x + bt[i].r.width) - 40, y = (int)bt[i].r.y - 7;
        DrawRectangle(x, y, 36, 12, Fade(BLACK, 0.8f));
        DrawRectangleLines(x, y, 36, 12, cor);
        DrawText("LUCI", x + 6 + ((L->luciAlucinou && RogueEstab(b) <= 2 && ((int)(tempo * 12.0f)) % 5 == 0) ? 1 : 0), y + 1, 10, cor);
    }
}

// Mídia simulada da Câmara de Pesquisa (só formas simples do Raylib)
static void DesenharMidiaPesquisa(const Biblioteca *b, Rectangle r, float tempo)
{
    const CasoPesquisa *c = &CASOS_PESQUISA[b->lab.missao];
    int caso = b->lab.missao;

    DrawRectangleRec(r, (Color){ 12, 14, 26, 255 });

    if (c->midia == MIDIA_AUDIO) {
        // Forma de onda animada
        float cy = r.y + r.height / 2;
        for (int i = 0; i < 48; i++) {
            float x = r.x + 12 + i * ((r.width - 24) / 48.0f);
            float h = (8.0f + 30.0f * fabsf(sinf(i * 0.7f + tempo * 5.0f) * sinf(i * 0.23f + tempo))) ;
            DrawRectangle((int)x, (int)(cy - h / 2), 3, (int)h, Fade(GOLD, 0.8f));
        }
        DrawText("ÁUDIO  0:14", (int)r.x + 10, (int)(r.y + r.height) - 18, 10, LIGHTGRAY);
        DrawCircleV((Vector2){ r.x + r.width - 20, r.y + 18 }, 6, RED);
    } else if (caso == 0) {
        // Avenida alagada com uma barbatana
        DrawRectangle((int)r.x, (int)r.y, (int)r.width, (int)(r.height * 0.45f), (Color){ 70, 80, 100, 255 });
        DrawRectangle((int)r.x, (int)(r.y + r.height * 0.45f), (int)r.width, (int)(r.height * 0.55f), (Color){ 60, 90, 110, 255 });
        for (int i = 0; i < 4; i++)
            DrawRectangle((int)r.x + 20 + i * 70, (int)(r.y + r.height * 0.15f), 40, (int)(r.height * 0.3f), (Color){ 50, 55, 70, 255 });
        for (int i = 0; i < 6; i++)
            DrawLine((int)r.x + 10, (int)(r.y + r.height * 0.55f + i * 10), (int)(r.x + r.width) - 10, (int)(r.y + r.height * 0.55f + i * 10 + sinf(tempo + i) * 2), Fade(SKYBLUE, 0.3f));
        Vector2 f = { r.x + r.width * 0.55f + sinf(tempo) * 6, r.y + r.height * 0.68f };
        DrawTriangle((Vector2){ f.x - 14, f.y }, (Vector2){ f.x + 14, f.y }, (Vector2){ f.x + 4, f.y - 26 }, (Color){ 90, 100, 110, 255 });
    } else if (caso == 3) {
        // Arco-íris duplo sobre a escola
        DrawRectangle((int)r.x, (int)r.y, (int)r.width, (int)r.height, (Color){ 90, 130, 180, 255 });
        static const Color arco[5] = { { 230, 41, 55, 255 }, { 255, 161, 0, 255 }, { 253, 249, 0, 255 }, { 0, 228, 48, 255 }, { 0, 121, 241, 255 } };
        Vector2 cc = { r.x + r.width / 2, r.y + r.height + 10 };
        for (int i = 0; i < 5; i++) DrawRing(cc, 70.0f + i * 5, 74.0f + i * 5, 180, 360, 40, Fade(arco[i], 0.75f));
        for (int i = 0; i < 5; i++) DrawRing(cc, 110.0f + i * 5, 114.0f + i * 5, 180, 360, 40, Fade(arco[4 - i], 0.4f));
        DrawRectangle((int)(r.x + r.width / 2) - 40, (int)(r.y + r.height) - 40, 80, 40, (Color){ 180, 150, 120, 255 });
        DrawTriangle((Vector2){ r.x + r.width / 2 - 46, r.y + r.height - 40 }, (Vector2){ r.x + r.width / 2 + 46, r.y + r.height - 40 },
                     (Vector2){ r.x + r.width / 2, r.y + r.height - 62 }, (Color){ 150, 70, 60, 255 });
    } else if (caso == 4) {
        // Céu noturno com um clarão
        DrawRectangle((int)r.x, (int)r.y, (int)r.width, (int)r.height, (Color){ 10, 12, 30, 255 });
        for (int i = 0; i < 30; i++)
            DrawPixel((int)r.x + (int)(Ruido((unsigned int)i * 7u) % (unsigned int)r.width), (int)r.y + (int)(Ruido((unsigned int)i * 13u) % (unsigned int)(r.height * 0.7f)), WHITE);
        float k = fmodf(tempo * 0.5f, 1.0f);
        Vector2 p = { r.x + 30 + k * (r.width - 60), r.y + 20 + k * 50 };
        DrawLineEx((Vector2){ p.x - 40, p.y - 18 }, p, 3, Fade(ORANGE, 0.7f));
        DrawCircleV(p, 4, YELLOW);
        DrawRectangle((int)r.x, (int)(r.y + r.height) - 30, (int)r.width, 30, (Color){ 20, 20, 30, 255 });
        for (int i = 0; i < 8; i++) DrawRectangle((int)r.x + i * 37, (int)(r.y + r.height) - 30 - (i % 3) * 12, 30, 30 + (i % 3) * 12, (Color){ 25, 25, 40, 255 });
    } else {
        // Pessoa falando num púlpito (vídeo)
        DrawRectangle((int)r.x, (int)r.y, (int)r.width, (int)r.height, (Color){ 40, 60, 90, 255 });
        Vector2 cab = { r.x + r.width / 2, r.y + r.height * 0.38f };
        DrawCircleV(cab, 20, (Color){ 200, 160, 130, 255 });
        DrawRectangle((int)cab.x - 30, (int)cab.y + 20, 60, 50, (Color){ 40, 40, 60, 255 });
        float boca = 2.0f + 3.0f * fabsf(sinf(tempo * 7.0f + ((((int)(tempo * 3.0f)) % 4 == 0) ? 1.5f : 0.0f)));
        DrawEllipse((int)cab.x, (int)cab.y + 9, 6, boca, (Color){ 90, 30, 40, 255 });
        DrawRectangle((int)cab.x - 44, (int)(r.y + r.height) - 34, 88, 34, (Color){ 90, 60, 40, 255 });
    }

    if (c->midia == MIDIA_VIDEO) {
        DrawRectangle((int)r.x, (int)(r.y + r.height) - 8, (int)r.width, 8, Fade(BLACK, 0.6f));
        DrawRectangle((int)r.x, (int)(r.y + r.height) - 8, (int)(r.width * fmodf(tempo * 0.1f, 1.0f)), 8, RED);
        DrawTriangle((Vector2){ r.x + 8, r.y + 8 }, (Vector2){ r.x + 8, r.y + 22 }, (Vector2){ r.x + 20, r.y + 15 }, Fade(WHITE, 0.8f));
    }

    DrawRectangleLinesEx(r, 2, Fade(CORES_LAB[LAB_PESQUISA], 0.8f));
    const char *rot = NOMES_MIDIA[c->midia];
    DrawRectangle((int)(r.x + r.width) - MeasureText(rot, 10) - 12, (int)r.y + 4, MeasureText(rot, 10) + 8, 14, Fade(BLACK, 0.7f));
    DrawText(rot, (int)(r.x + r.width) - MeasureText(rot, 10) - 8, (int)r.y + 6, 10, GOLD);
}

// Texto quebrado numa área (devolve a altura usada)
static int DesenharTextoLab(const char *texto, int x, int y, int largura, int fonte, int espaco, Color cor)
{
    char linhas[1024];
    QuebrarTexto(texto, largura, fonte, linhas, (int)sizeof(linhas));
    return DesenharTextoLinhas(linhas, x, y, fonte, espaco, cor) * EspacoLinhaTexto(espaco, fonte);
}

static void DesenharBarrasCriterios(const int crit[NUM_CRITERIOS], int nota, int x, int y)
{
    for (int c = 0; c < NUM_CRITERIOS; c++) {
        int yy = y + c * 15;
        Color cor = (crit[c] >= 2) ? LIME : ((crit[c] == 1) ? GOLD : RED);
        DrawText(NOMES_CRITERIOS[c], x, yy, 10, LIGHTGRAY);
        for (int k = 0; k < 2; k++)
            DrawRectangle(x + 118 + k * 22, yy + 1, 18, 8, (k < crit[c]) ? cor : Fade(GRAY, 0.3f));
    }
    DrawText(TextFormat("%d/10", nota), x + 172, y + 22, 20, (nota >= 9) ? LIME : ((nota >= 6) ? GOLD : RED));
}

// Holograma do ECO-1: o que a IA "entendeu" vira uma imagem. Quanto pior o pedido, mais chiado.
static void DesenharHologramaEco(const Biblioteca *b, Rectangle r, float tempo)
{
    const LabEdu *L = &b->lab;
    Color cor = CORES_LAB[LAB_PROMPTS];

    DrawRectangleRec(r, (Color){ 10, 12, 24, 255 });
    DrawRectangleLinesEx(r, 2, Fade(cor, 0.6f));
    DrawRectangle((int)r.x, (int)r.y, (int)r.width, 20, Fade(cor, 0.18f));
    DrawText("ECO-1  //  IA SIMULADA (OFFLINE)", (int)r.x + 8, (int)r.y + 5, 10, cor);
    const char *env = TextFormat("envios %d/%d", L->envios, LAB_MAX_ENVIOS);
    DrawText(env, (int)(r.x + r.width) - MeasureText(env, 10) - 8, (int)r.y + 5, 10, GRAY);

    Rectangle tela = { r.x + 18, r.y + 30, r.width - 36, 116 };
    Vector2 robo = { r.x + r.width / 2, r.y + r.height - 30 };
    int humor = 0;

    // Feixe do projetor
    DrawTriangle((Vector2){ robo.x, robo.y - 26 }, (Vector2){ tela.x + tela.width, tela.y + tela.height },
                 (Vector2){ tela.x, tela.y + tela.height }, Fade(cor, 0.07f));

    if (L->etapa == LAB_DESAFIO) {
        DrawRectangleRec(tela, Fade(cor, 0.05f));
        float varre = fmodf(tempo * 40.0f, tela.height);
        DrawRectangle((int)tela.x, (int)(tela.y + varre), (int)tela.width, 2, Fade(cor, 0.25f));
        const char *t = "AGUARDANDO O PEDIDO";
        DrawText(t, (int)(tela.x + tela.width / 2) - MeasureText(t, 10) / 2, (int)tela.y + 36, 10, Fade(cor, 0.6f + 0.3f * sinf(tempo * 3.0f)));
        for (int k = 0; k < NUM_PARTES_PROMPT; k++) {           // as peças que já estão nos encaixes
            Vector2 c = { tela.x + tela.width / 2 - 45 + k * 30.0f, tela.y + 72 };
            if (L->escolha[k] > 0) DesenharIconeParte(k, c, 1.1f, CORES_PARTES_PROMPT[k]);
            else                   DrawCircleLines((int)c.x, (int)c.y, 7, Fade(CORES_PARTES_PROMPT[k], 0.4f));
        }
    } else {
        DiagPrompt d = L->diag;
        Color cd = CORES_DIAG[d];
        bool pego = L->pegouAlucinacao && L->respostaAlucinada;

        DrawRectangleRec(tela, Fade(cd, 0.08f));
        DrawRectangleLinesEx(tela, 1, Fade(cd, 0.5f));
        DesenharPictogramaDiag(d, (Vector2){ tela.x + 42, tela.y + 48 }, tempo);
        if (!pego) DrawText(CARIMBOS_DIAG[d], (int)tela.x + 82, (int)tela.y + 26, 16, cd);

        // Nitidez: a nota do pedido em 10 barrinhas
        DrawText("NITIDEZ", (int)tela.x + 8, (int)(tela.y + tela.height) - 18, 10, LIGHTGRAY);
        for (int i = 0; i < 10; i++)
            DrawRectangle((int)tela.x + 62 + i * 15, (int)(tela.y + tela.height) - 17, 12, 8, (i < L->notaPrompt) ? cd : Fade(GRAY, 0.25f));

        // Chiado: o que faltou no pedido vira ruído na imagem
        int ruido = (10 - L->notaPrompt) * 9;
        for (int i = 0; i < ruido; i++) {
            int w = GetRandomValue(2, 5);
            DrawRectangle(GetRandomValue((int)tela.x, (int)(tela.x + tela.width) - w), GetRandomValue((int)tela.y, (int)(tela.y + tela.height) - 3),
                          w, 2, Fade((i % 3 == 0) ? WHITE : cd, 0.35f));
        }

        // O holograma "liga" de cima para baixo logo depois do envio
        float monta = Clamp(L->tempoResposta / 0.5f, 0.0f, 1.0f);
        if (monta < 1.0f)
            DrawRectangle((int)tela.x, (int)(tela.y + tela.height * monta), (int)tela.width, (int)(tela.height * (1.0f - monta)) + 1, (Color){ 10, 12, 24, 255 });

        if (pego) {         // pegou a invenção: carimbo por cima
            const char *inv = "INVENTADO!";
            int w = MeasureText(inv, 20);
            int x = (int)(tela.x + tela.width / 2) - w / 2, y = (int)(tela.y + tela.height / 2) - 14;
            DrawRectangle(x - 8, y - 4, w + 16, 28, Fade(BLACK, 0.75f));
            DrawRectangleLinesEx((Rectangle){ (float)x - 8, (float)y - 4, (float)w + 16, 28 }, 2, MAGENTA);
            DrawText(inv, x, y, 20, MAGENTA);
        }

        humor = pego ? 3 : ((d == DIAG_IDEAL || d == DIAG_BOA) ? 1 : ((d == DIAG_CONTRADICAO) ? 3 : 2));
    }

    DesenharEco1(robo, humor, tempo);
}

// Laboratório de prompts: os encaixes ficam no alto (desenhados como botões), as peças à
// esquerda e o holograma do ECO-1 à direita. Depois do envio, a resposta ocupa a esquerda.
static void DesenharLabPrompt(const Biblioteca *b, float tempo)
{
    const LabEdu *L = &b->lab;
    const MissaoPrompt *m = &MISSOES_PROMPT[L->missao];
    Color cor = CORES_LAB[LAB_PROMPTS];
    Color ciano = { 0, 228, 255, 255 };

    // O texto começa logo depois do rótulo (que cresce com o texto grande) e tem espaço para 2 linhas antes dos encaixes
    int xMissao = 44 + MeasureText("MISSÃO:", 10) + 6;
    DrawText("MISSÃO:", 44, 80, 10, GOLD);
    DesenharTextoLab(m->missao, xMissao, 80, 756 - xMissao, 10, 13, WHITE);

    if (L->etapa == LAB_DESAFIO) {
        if (L->temSugestao) {
            DrawText("A LUCI REMONTOU O PEDIDO", 60, 200, 12, ciano);
            DesenharTextoLab("As peças que ela trocou estão marcadas. Ela pode ter alucinado: confira (V) antes de usar.",
                             60, 220, 360, 10, 13, LIGHTGRAY);
        } else {
            int k = L->slotAtivo;
            DrawText(TextFormat("PEÇAS PARA %s", NOMES_PARTES_PROMPT[k]), 44, 184, 10, CORES_PARTES_PROMPT[k]);
            const char *atalho = "1-3 peça   0 vazio   A/D encaixe";
            DrawText(atalho, 436 - MeasureText(atalho, 10), 184, 10, Fade(GRAY, 0.9f));
        }
    } else {
        // A resposta do ECO-1 (máquina de escrever) e o resultado do "desconfiar"
        Rectangle caixa = { 44, 180, 392, 222 };
        DrawRectangleRec(caixa, Fade(cor, 0.06f));
        DrawRectangleLinesEx(caixa, 1, Fade(cor, 0.35f));
        DrawText(L->original ? "ECO-1 RESPONDEU AO PEDIDO ORIGINAL:" : "ECO-1 RESPONDEU:", 52, 187, 10, cor);

        const char *resp = LabTextoRespostaAtual(b);
        char parcial[600];
        TextoParcial(resp, L->digitado, parcial, (int)sizeof(parcial));
        int h = DesenharTextoLab(parcial, 52, 204, 376, 10, 13, (Color){ 210, 245, 255, 255 });
        if (L->digitado < (float)strlen(resp) && ((int)(tempo * 6.0f)) % 2 == 0)
            DrawRectangle(52, 204 + h, 6, 2, cor);

        if (L->avaliacao[0] != '\0') {
            bool pegou = L->pegouAlucinacao && L->respostaAlucinada;
            DesenharTextoLab(L->avaliacao, 52, 204 + h + 10, 376, 10, 13, pegou ? MAGENTA : LIME);
        }
    }

    DesenharHologramaEco(b, (Rectangle){ 448, 180, 308, 222 }, tempo);
}

// A peça voando do cartão até o encaixe (desenhada por cima dos botões)
static void DesenharAnimPecaLab(const LabEdu *L)
{
    if (L->tipo != LAB_PROMPTS || L->animPeca <= 0.0f)
        return;

    float t = Clamp(1.0f - L->animPeca / LAB_ANIM_PECA, 0.0f, 1.0f);
    t = t * t * (3.0f - 2.0f * t);
    Rectangle de = LabRetPeca(L->animDe), para = LabRetEncaixe(L->animSlot);
    Vector2 a = { de.x + 60, de.y + de.height / 2 }, z = { para.x + para.width / 2, para.y + para.height / 2 };
    Vector2 p = Vector2Lerp(a, z, t);
    p.y -= sinf(t * PI) * 26.0f;
    Color ck = CORES_PARTES_PROMPT[L->animSlot];

    for (int i = 1; i <= 4; i++) {          // rastro
        float tt = fmaxf(0.0f, t - 0.06f * i);
        Vector2 q = Vector2Lerp(a, z, tt);
        q.y -= sinf(tt * PI) * 26.0f;
        DrawRectangle((int)q.x - 14, (int)q.y - 5, 28, 10, Fade(ck, 0.25f / i));
    }
    DrawRectangle((int)p.x - 22, (int)p.y - 8, 44, 16, ck);
    DrawRectangle((int)p.x - 26, (int)p.y - 4, 4, 8, ck);
    if (t > 0.8f)                           // o clique do encaixe
        DrawRectangleLinesEx((Rectangle){ para.x - 3, para.y - 3, para.width + 6, para.height + 6 }, 2, Fade(ck, (1.0f - t) * 5.0f));
}

// Introdução do laboratório de prompts: o conceito como 4 peças (em vez de um parágrafo)
static void DesenharIntroPrompt(const Biblioteca *b, float tempo)
{
    const LabEdu *L = &b->lab;
    const MissaoPrompt *m = &MISSOES_PROMPT[L->missao];
    Color cor = CORES_LAB[LAB_PROMPTS];

    const char *t = "UM BOM PEDIDO TEM ATÉ 4 PEÇAS";
    DrawText(t, 400 - MeasureText(t, 16) / 2, 88, 16, GOLD);

    for (int k = 0; k < NUM_PARTES_PROMPT; k++) {
        Rectangle r = { 44.0f + k * 180.0f, 114, 172, 92 };
        Color ck = CORES_PARTES_PROMPT[k];
        DrawRectangleRec(r, Fade(ck, 0.08f));
        DrawRectangleLinesEx(r, 2, Fade(ck, 0.7f));
        DesenharIconeParte(k, (Vector2){ r.x + r.width / 2, r.y + 30 + sinf(tempo * 2.0f + k) * 2.0f }, 2.0f, ck);
        DrawText(NOMES_PARTES_PROMPT[k], (int)(r.x + r.width / 2) - MeasureText(NOMES_PARTES_PROMPT[k], 12) / 2, (int)r.y + 54, 12, ck);
        DrawText(DICAS_PARTES_PROMPT[k], (int)(r.x + r.width / 2) - MeasureText(DICAS_PARTES_PROMPT[k], 10) / 2, (int)r.y + 72, 10, LIGHTGRAY);
        if (k < NUM_PARTES_PROMPT - 1)
            DrawText("+", (int)(r.x + r.width) + 1, (int)r.y + 40, 10, GOLD);
    }

    t = "O ECO-1 faz EXATAMENTE o que você pedir.";
    DrawText(t, 400 - MeasureText(t, 12) / 2, 218, 12, WHITE);

    Rectangle mc = { 44, 242, 712, 72 };
    DrawRectangleRec(mc, Fade(BLACK, 0.4f));
    DrawRectangleLinesEx(mc, 1, Fade(cor, 0.6f));
    DrawText(TextFormat("MISSÃO: %s", m->titulo), 56, 250, 12, cor);
    DesenharTextoLab(m->missao, 56, 270, 688, 12, 16, WHITE);

    // As regras em três selos curtos
    static const char *selos[3] = { "PEÇA ERRADA ATRAPALHA", "NEM SEMPRE PRECISA DAS 4", "3 ENVIOS" };
    int x = 44;
    for (int i = 0; i < 3; i++) {
        int w = MeasureText(selos[i], 10) + 20;
        DrawRectangle(x, 328, w, 20, Fade(GOLD, 0.12f));
        DrawRectangleLines(x, 328, w, 20, Fade(GOLD, 0.6f));
        DrawText(selos[i], x + 10, 333, 10, GOLD);
        x += w + 10;
    }

    DesenharLuciTamanho((Vector2){ 58, 378 }, 24.0f);
    DrawText("Luci: H pede dica (gasta estabilidade). V confere o que eu disser.", 78, 372, 10, (Color){ 0, 228, 255, 255 });
    DrawText("Sala comum: opcional. A porta já está aberta; concluir dá a recompensa.", 44, 404, 10, LIGHTGRAY);
}

static void DesenharLabArquivo(const Biblioteca *b, float tempo)
{
    const LabEdu *L = &b->lab;
    const DesafioArquivo *d = &DESAFIOS_ARQUIVO[L->missao];
    Color cor = CORES_LAB[LAB_ARQUIVO];

    Rectangle doc = { 44, 84, 476, 320 };
    DrawRectangleRec(doc, (Color){ 16, 12, 26, 255 });
    DrawRectangleLinesEx(doc, 2, Fade(cor, 0.6f));
    DrawText(TextFormat("RESPOSTA RECUPERADA // %s", d->titulo), (int)doc.x + 10, (int)doc.y + 8, 10, cor);
    DesenharTextoLab(d->pergunta, (int)doc.x + 10, (int)doc.y + 24, 456, 10, 12, LIGHTGRAY);

    // Corrupção: blocos e caracteres que piscam por cima do documento
    for (int i = 0; i < 7; i++) {
        unsigned int h = Ruido((unsigned int)i * 31u + (unsigned int)(tempo * 4.0f));
        if (h % 3 != 0) continue;
        int x = (int)doc.x + 8 + (int)(h % 440), y = (int)doc.y + 50 + (int)((h >> 8) % 250);
        DrawRectangle(x, y, 6 + (int)((h >> 4) % 30), 2, Fade(MAGENTA, 0.35f));
    }

    // Passos
    static const char *passos[3] = { "1. ANALISE AS AFIRMAÇÕES", "2. MARQUE A SUSPEITA", "3. ESCOLHA COMO VERIFICAR" };
    for (int i = 0; i < 3; i++) {
        bool atual = (i == 0 && L->fase == 0 && L->sel < 0) || (i == 1 && L->fase == 0 && L->sel >= 0) || (i == 2 && L->fase == 1);
        DrawText(passos[i], 532, 90 + i * 16, 10, atual ? GOLD : Fade(LIGHTGRAY, 0.6f));
    }
    if (L->fase == 0)
        DesenharTextoLab("Uma resposta convincente não é necessariamente verdadeira. Qual afirmação você não aceitaria sem conferir?", 532, 150, 220, 10, 13, GRAY);
    else
        DrawText(TextFormat("Suspeita marcada: afirmação %d", L->sel + 1), 532, 156, 10, cor);
}

static void DesenharLabNucleo(const Biblioteca *b, float tempo)
{
    const LabEdu *L = &b->lab;
    const DesafioNucleo *d = &DESAFIOS_NUCLEO[L->missao];
    Color cor = CORES_LAB[LAB_NUCLEO];

    int xTarefa = 44 + MeasureText("TAREFA:", 10) + 6;
    DrawText("TAREFA:", 44, 84, 10, GOLD);
    DesenharTextoLab(d->tarefa, xTarefa, 84, 756 - xTarefa, 10, 13, WHITE);
    DrawText("Clique nos blocos para carregá-los no contexto da IA:", 44, 112, 10, GRAY);

    // Núcleo: anel de capacidade (vermelho no excesso)
    int tokens = LabTokensCarregados(b);
    float frac = (float)tokens / (float)d->capacidade;
    Vector2 c = { 643, 190 };
    bool excesso = tokens > d->capacidade;
    Color corAnel = excesso ? RED : ((frac >= 0.8f) ? GOLD : cor);
    DrawCircleV(c, 58, Fade(cor, 0.06f));
    DrawRing(c, 44, 54, -90, 270, 48, Fade(GRAY, 0.25f));
    DrawRing(c, 44, 54, -90, -90 + 360.0f * fminf(frac, 1.0f), 48, corAnel);
    if (excesso && ((int)(tempo * 6.0f)) % 2 == 0) DrawRing(c, 56, 60, -90, 270, 48, Fade(RED, 0.6f));
    DrawCircleV(c, 30 + 2 * sinf(tempo * 3.0f), Fade(cor, 0.35f));
    DrawText(TextFormat("%d/%d", tokens, d->capacidade), (int)c.x - MeasureText(TextFormat("%d/%d", tokens, d->capacidade), 16) / 2, (int)c.y - 12, 16, WHITE);
    DrawText("tokens", (int)c.x - MeasureText("tokens", 10) / 2, (int)c.y + 6, 10, LIGHTGRAY);

    // Barra de capacidade e ordem de carga (o mais antigo é o primeiro a sair no excesso)
    DrawRectangle(540, 256, 206, 8, Fade(GRAY, 0.3f));
    DrawRectangle(540, 256, (int)(206 * fminf(frac, 1.0f)), 8, corAnel);
    {   // o rótulo encurta se não couber na largura da barra (texto grande)
        const char *rot = excesso ? "EXCESSO: o início vai ser cortado!" : "CAPACIDADE DA JANELA DE CONTEXTO";
        if (MeasureText(rot, 10) > 212) rot = excesso ? "EXCESSO: início cortado!" : "JANELA DE CONTEXTO";
        DrawText(rot, 540, 268, 10, excesso ? RED : LIGHTGRAY);
    }
    char ordem[64] = "Ordem de carga:";
    for (int k = 0; k < L->numOrdem && k < LAB_MAX_ITENS; k++)
        snprintf(ordem + strlen(ordem), sizeof(ordem) - strlen(ordem), " #%d", L->ordem[k] + 1);
    DrawText(ordem, 540, 284, 10, GRAY);
    DesenharTextoLab(AVISO_TOKENS, 540, 302, 212, 8, 10, Fade(SKYBLUE, 0.7f));

    // Números dos blocos e o que foi cortado
    for (int i = 0; i < d->numBlocos; i++) {
        int x = 44 + (i % 2) * 242, y = 128 + (i / 2) * 62;
        DrawText(TextFormat("#%d", i + 1), x + 8, y + 40, 8, Fade(cor, 0.9f));
        if (L->etapa == LAB_RESPOSTA && (L->cortados & (1u << i))) {
            DrawRectangle(x, y, 232, 54, Fade(RED, 0.25f));
            DrawText("CORTADO", x + 232 - MeasureText("CORTADO", 10) - 6, y + 40, 10, RED);
        }
    }

    // Resposta da IA (ou instruções)
    Rectangle caixa = { 44, 314, 476, 90 };
    DrawRectangleRec(caixa, (Color){ 10, 12, 24, 255 });
    DrawRectangleLinesEx(caixa, 1, Fade(cor, 0.5f));
    if (L->etapa == LAB_RESPOSTA) {
        const char *resp = LabTextoRespostaAtual(b);
        char parcial[400];
        TextoParcial(resp, L->digitado, parcial, (int)sizeof(parcial));
        DrawText("RESPOSTA DA IA SIMULADA:", 52, 320, 10, cor);
        DesenharTextoLab(parcial, 52, 336, 460, 10, 12, (Color){ 210, 245, 255, 255 });
    } else {
        DesenharTextoLab("Carregue o que a IA precisa para cumprir a tarefa sem estourar a capacidade. Depois, PROCESSAR.",
                         52, 322, 460, 10, 13, GRAY);
        DrawText(TextFormat("Processamentos: %d/%d", L->processos, LAB_MAX_PROCESSOS), 52, 384, 10, LIGHTGRAY);
    }
}

static void DesenharLabFirewall(const Biblioteca *b, float tempo)
{
    const LabEdu *L = &b->lab;
    const CasoVies *c = &CASOS_VIES[L->missao];
    Color cor = CORES_LAB[LAB_FIREWALL];

    if (L->fase < 2) {
        DrawText(TextFormat("VIÉS ALGORÍTMICO // %s", c->titulo), 44, 84, 12, cor);
        Rectangle caso = { 44, 102, 712, 104 };
        DrawRectangleRec(caso, Fade(BLACK, 0.45f));
        DrawRectangleLinesEx(caso, 1, Fade(cor, 0.4f));
        DesenharTextoLab(c->caso, 56, 112, 688, 12, 17, WHITE);
        DrawText((L->fase == 0) ? "1/3  QUAL É O PROBLEMA?" : "2/3  QUAL É A MELHOR CORREÇÃO?", 44, 226, 14, GOLD);
        if (L->fase == 1 && L->avaliacao[0] != '\0')
            DesenharTextoLab(L->avaliacao, 56, 390, 688, 10, 12, (strncmp(L->avaliacao, "CERTO", 5) == 0) ? LIME : ORANGE);
        return;
    }

    // Privacidade: o pacote anda até o firewall
    DrawText("3/3  PRIVACIDADE: o que pode passar pelo firewall até a IA?", 44, 84, 14, GOLD);
    DrawText(TextFormat("PACOTE %d/%d", (L->pacote < LAB_PACOTES) ? L->pacote + 1 : LAB_PACOTES, LAB_PACOTES), 44, 106, 10, LIGHTGRAY);
    if (L->avaliacao[0] != '\0')
        DesenharTextoLab(L->avaliacao, 44, 122, 712, 10, 12, (strncmp(L->avaliacao, "CERTO", 5) == 0) ? LIME : ORANGE);

    // Esteira e a parede do firewall
    DrawRectangle(44, 250, 600, 6, Fade(GRAY, 0.4f));
    for (int i = 0; i < 12; i++)
        DrawRectangle(44 + (int)fmodf((float)i * 50.0f + tempo * 40.0f, 600.0f), 251, 20, 4, Fade(cor, 0.25f));
    for (int k = 0; k < 6; k++)
        DrawRectangle(652, 168 + k * 24, 22, 20, Fade(((k + (int)(tempo * 4.0f)) % 2) ? cor : (Color){ 0, 120, 30, 255 }, 0.8f));
    DrawText("FIREWALL", 640, 318, 10, cor);
    DrawText("IA", 708, 236, 20, (Color){ 0, 228, 255, 255 });

    if (L->pacote < LAB_PACOTES) {
        const PacoteDados *p = &PACOTES_DADOS[L->pacotes[L->pacote]];
        float prog = 1.0f - L->tempoPacote / LAB_TEMPO_PACOTE;
        Rectangle card = { 50 + prog * 300.0f, 178, 290, 66 };
        DrawRectangleRec(card, (Color){ 30, 26, 50, 255 });
        DrawRectangleLinesEx(card, 2, (prog > 0.75f && ((int)(tempo * 8.0f)) % 2 == 0) ? RED : cor);
        DrawText("PACOTE DE DADOS", (int)card.x + 8, (int)card.y + 5, 8, cor);
        DesenharTextoLab(p->texto, (int)card.x + 8, (int)card.y + 18, 274, 10, 12, WHITE);
        // Tempo até atravessar sozinho
        DrawRectangle(44, 270, 600, 5, Fade(BLACK, 0.6f));
        DrawRectangle(44, 270, (int)(600 * (L->tempoPacote / LAB_TEMPO_PACOTE)), 5, (prog > 0.75f) ? RED : GOLD);
        DrawText("Sem decisão, o pacote atravessa sozinho.", 44, 280, 10, GRAY);
    }
    DesenharTextoLab("PERMITIR: pode ir.  BLOQUEAR: nunca deve ir.  ANONIMIZAR: pode ir sem os dados pessoais.", 44, 300, 590, 10, 12, LIGHTGRAY);
}

static void DesenharLabPesquisa(const Biblioteca *b, float tempo)
{
    const LabEdu *L = &b->lab;
    const CasoPesquisa *c = &CASOS_PESQUISA[L->missao];
    Color cor = CORES_LAB[LAB_PESQUISA];

    DrawText(c->titulo, 44, 84, 12, cor);
    DesenharMidiaPesquisa(b, (Rectangle){ 44, 100, 290, 150 }, tempo);
    DesenharTextoLab(c->descricao, 44, 256, 290, 10, 12, WHITE);

    int consultas = 0;
    for (int i = 0; i < 3; i++) if (L->ferramentas & (1u << i)) consultas++;
    DrawText(TextFormat("INVESTIGAR (consultas %d/%d):", consultas, LAB_MAX_FERRAMENTAS), 44, 300, 10, SKYBLUE);

    {   // o cabeçalho encurta se não couber até a borda do painel (texto grande)
        const char *dossie = "DOSSIÊ: marque os SINAIS DE ALERTA (aparência sozinha não prova nada)";
        if (352 + MeasureText(dossie, 10) > 756) dossie = "DOSSIÊ: marque os SINAIS DE ALERTA";
        DrawText(dossie, 352, 98, 10, GOLD);
    }

    // Resultados das consultas
    int y = 310;
    for (int i = 0; i < 3; i++) {
        if (!(L->ferramentas & (1u << i))) continue;
        DrawText(TextFormat("%s:", c->ferramenta[i]), 352, y, 10, SKYBLUE);
        y += 12;
        y += DesenharTextoLab(c->resultado[i], 360, y, 392, 10, 12, WHITE) + 4;
    }
    if (consultas == 0)
        DesenharTextoLab("Use as ferramentas ao lado para investigar a origem do material antes de dar o veredito.", 352, 316, 400, 10, 12, GRAY);
}

// Passo 4: estrelas, explicação, detalhes revelados, a Luci e a recompensa
static void DesenharLabResultado(const Biblioteca *b, float tempo)
{
    const LabEdu *L = &b->lab;
    Color cor = CORES_LAB[L->tipo];

    for (int i = 0; i < 3; i++) {
        Vector2 c = { 400.0f + (i - 1) * 44.0f, 98.0f };
        float giro = tempo * 40.0f + i * 20.0f;
        if (i < L->estrelas) { DrawPoly(c, 5, 16, giro, GOLD); DrawPolyLines(c, 5, 16, giro, ORANGE); }
        else                 DrawPolyLines(c, 5, 16, giro, Fade(GRAY, 0.6f));
    }
    static const char *notas[4] = { "", "DÁ PARA MELHORAR", "BOM TRABALHO", "EXCELENTE" };
    const char *nt = notas[L->estrelas];
    DrawText(nt, 400 - MeasureText(nt, 16) / 2, 120, 16, (L->estrelas == 3) ? GOLD : WHITE);
    int y = 142;
    y += DesenharTextoLab(L->resultado, 44, y, 712, 10, 13, LIGHTGRAY) + 6;

    // O que aprendemos
    const char *exp;
    switch (L->tipo) {
        case LAB_PROMPTS:  exp = MISSOES_PROMPT[L->missao].explicacao;   break;
        case LAB_ARQUIVO:  exp = DESAFIOS_ARQUIVO[L->missao].explicacao; break;
        case LAB_NUCLEO:   exp = DESAFIOS_NUCLEO[L->missao].explicacao;  break;
        case LAB_FIREWALL: exp = CASOS_VIES[L->missao].explicacao;       break;
        default:           exp = CASOS_PESQUISA[L->missao].explicacao;   break;
    }
    DrawText("O QUE APRENDEMOS", 44, y, 10, cor);
    y += 14;
    y += DesenharTextoLab(exp, 44, y, 712, 12, 16, WHITE) + 8;

    // Detalhes revelados
    switch (L->tipo) {
        case LAB_PROMPTS: {
            const MissaoPrompt *m = &MISSOES_PROMPT[L->missao];
            for (int k = 0; k < NUM_PARTES_PROMPT; k++) {
                BotaoLab o;
                memset(&o, 0, sizeof(o));
                o.r = LabRetEncaixe(k);
                o.r.y = (float)y;
                o.estilo = EST_ENCAIXE;
                o.param = k;
                o.cor = CORES_PARTES_PROMPT[k];
                snprintf(o.rotulo, sizeof(o.rotulo), "%s", NOMES_PARTES_PROMPT[k]);
                o.texto = (L->escolha[k] > 0) ? m->alt[k][L->escolha[k] - 1] : NULL;
                o.luz = (int)LabLuzParte(m, L->escolha, k);
                DesenharBotaoLab(&o, false, tempo);
            }
            y += 76;
            DesenharBarrasCriterios(L->criterios, L->notaPrompt, 44, y);
            if (L->respostaAlucinada || L->pegouAlucinacao)
                DesenharTextoLab(TextFormat("A IA inventou: %s", m->trechoFalso), 290, y, 466, 10, 12, MAGENTA);
        } break;

        case LAB_ARQUIVO: {
            const DesafioArquivo *d = &DESAFIOS_ARQUIVO[L->missao];
            for (int i = 0; i < 4; i++) {
                int t = d->tipo[i];
                DrawText(NOMES_AFIRMACAO[t], 44, y, 10, CORES_AFIRMACAO[t]);
                y += DesenharTextoLab(d->af[i], 160, y, 596, 10, 12, (i == L->sel) ? GOLD : LIGHTGRAY) + 3;
            }
            DesenharTextoLab(TextFormat("Melhor verificação: %s", d->metodos[d->metodoCerto]), 44, y + 2, 712, 10, 12, LIME);
        } break;

        case LAB_NUCLEO: {
            const DesafioNucleo *d = &DESAFIOS_NUCLEO[L->missao];
            static const char *rel[3] = { "DISPENSÁVEL", "ÚTIL", "ESSENCIAL" };
            static const Color corRel[3] = { { 230, 41, 55, 255 }, { 255, 203, 0, 255 }, { 0, 228, 48, 255 } };
            // Duas colunas; se alguma linha não couber na coluna (texto grande), vira uma coluna só
            int wRotulo = MeasureText(rel[0], 10) + 8;
            int linha = EspacoLinhaTexto(15, 10);
            int colunas = 2;
            for (int i = 0; i < d->numBlocos; i++)
                if (wRotulo + MeasureText(TextFormat("[ ] %s (%d tk)", d->bloco[i].texto, d->bloco[i].tokens), 10) > 352) colunas = 1;
            for (int i = 0; i < d->numBlocos; i++) {
                int x = 44 + ((colunas == 2) ? (i % 2) * 360 : 0), yy = y + ((colunas == 2) ? i / 2 : i) * linha;
                bool usou = (L->marcados & (1u << i)) != 0;
                DrawText(rel[d->bloco[i].relevancia], x, yy, 10, corRel[d->bloco[i].relevancia]);
                DrawText(TextFormat("%s%s (%d tk)", usou ? "[x] " : "[ ] ", d->bloco[i].texto, d->bloco[i].tokens), x + wRotulo, yy, 10, usou ? WHITE : GRAY);
            }
            int linhasLista = (colunas == 2) ? (d->numBlocos + 1) / 2 : d->numBlocos;
            DesenharTextoLab(AVISO_TOKENS, 44, y + linhasLista * linha + 5, 712, 10, 12, Fade(SKYBLUE, 0.8f));
        } break;

        case LAB_FIREWALL:
            for (int k = 0; k < LAB_PACOTES; k++) {
                const PacoteDados *p = &PACOTES_DADOS[L->pacotes[k]];
                int a = L->acoes[k];
                int efetiva = (a < 0) ? ACAO_PERMITIR : a;
                bool certo = (a >= -1) && efetiva == p->acao;
                DrawText(certo ? "OK" : "X", 44, y, 10, certo ? LIME : RED);
                DrawText(TextFormat("%s  ->  certo: %s", p->texto, NOMES_ACAO_PACOTE[p->acao]), 64, y, 10, LIGHTGRAY);
                y += 14;
            }
            break;

        default: {
            const CasoPesquisa *c = &CASOS_PESQUISA[L->missao];
            for (int i = 0; i < 5; i++) {
                bool marcou = (L->marcados & (1u << i)) != 0;
                DrawText(c->pista[i].alerta ? "ALERTA" : "NÃO PROVA", 44, y, 10, c->pista[i].alerta ? ORANGE : SKYBLUE);
                DrawText(TextFormat("%s%s", marcou ? "[!] " : "[ ] ", c->pista[i].texto), 124, y, 10, (marcou == c->pista[i].alerta) ? WHITE : RED);
                y += 14;
            }
        } break;
    }

    // A Luci nesta sala
    char luci[200];
    if (L->ajudas == 0)
        snprintf(luci, sizeof(luci), "Você resolveu sem a ajuda da Luci.");
    else if (L->alucinacoes == 0)
        snprintf(luci, sizeof(luci), "A Luci ajudou %d vez(es) e não alucinou nesta sala.", L->ajudas);
    else
        snprintf(luci, sizeof(luci), "A Luci alucinou em %d de %d ajuda(s). Você conferiu %d.%s", L->alucinacoes, L->ajudas, L->alucinacoesPegas,
                 L->seguiuAlucinacao ? " Você seguiu uma sugestão falsa sem conferir!" : "");
    LabCortarUtf8(luci);
    DesenharLuciTamanho((Vector2){ 58, 428 }, 26.0f);
    DesenharTextoLab(luci, 76, 418, 680, 10, 12, (L->seguiuAlucinacao) ? MAGENTA : (Color){ 0, 228, 255, 255 });
    DesenharTextoLab(TextFormat("RECOMPENSA: %s", L->recompensa), 76, 446, 680, 10, 12, GOLD);
}

void DesenharLab(const Biblioteca *b, float tempo)
{
    const LabEdu *L = &b->lab;
    Color cor = CORES_LAB[L->tipo];
    Rectangle painel = { LAB_PAINEL_X, LAB_PAINEL_Y, LAB_PAINEL_W, LAB_PAINEL_H };

    DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, 0.7f));
    DrawRectangleRec(painel, (Color){ 16, 13, 28, 255 });
    DrawRectangleLinesEx(painel, 3, cor);
    // Linhas de varredura sutis (terminal)
    for (int y = (int)painel.y + 4; y < (int)(painel.y + painel.height); y += 4)
        DrawLine((int)painel.x + 3, y, (int)(painel.x + painel.width) - 3, y, Fade(cor, 0.025f));

    // Cabeçalho: categoria, missão, nível, opcional e a estabilidade da Luci (com a carga de ajuda)
    DrawCircleV((Vector2){ 58, 52 }, 15, Fade(cor, 0.2f));
    DesenharIconeLab(L->tipo, (Vector2){ 58, 52 }, 0.9f, tempo);
    DrawText(NOMES_LAB[L->tipo], 82, 40, 18, cor);
    int dif = LabDificuldadeMissao(L->tipo, L->missao);
    const char *sub = TextFormat("%s   NÍVEL %d/3%s", LAB_ESPECIAL[L->tipo] ? "SALA ESPECIAL (opcional)" : "SALA COMUM (opcional)", dif, "");
    DrawText(sub, 82, 60, 10, LAB_ESPECIAL[L->tipo] ? GOLD : LIGHTGRAY);

    int e = RogueEstab(b);
    Color corL = (e >= 4) ? (Color){ 0, 228, 255, 255 } : ((e == 3) ? GOLD : ((e >= 1) ? ORANGE : MAGENTA));
    int dx = (e <= 2 && ((int)(tempo * 9.0f)) % 7 == 0) ? GetRandomValue(-2, 2) : 0;
    DrawText("LUCI", 560 + dx, 40, 10, corL);
    for (int k = 0; k < ESTABILIDADE_MAX_LUCI; k++) {
        Rectangle pip = { 594.0f + k * 12.0f, 40, 9, 9 };
        if (k < e) DrawRectangleRec(pip, corL);
        DrawRectangleLinesEx(pip, 1, corL);
    }
    DrawText("carga", 660, 40, 10, GRAY);
    for (int k = 0; k < LAB_CARGA_POR_ESTAB; k++)
        DrawRectangle(694 + k * 12, 42, 9, 6, (k < b->cargaLuci) ? ORANGE : Fade(GRAY, 0.3f));
    DrawText(FALAS_INSTABILIDADE_LAB[e], 560 + dx, 56, 10, Fade(corL, 0.85f));

    DrawLine(40, 76, 760, 76, Fade(cor, 0.4f));
    DesenharBotaoFechar(RET_FECHAR_LAB, cor);

    // Conteúdo
    if (L->etapa == LAB_INTRO && L->tipo == LAB_PROMPTS) {
        DesenharIntroPrompt(b, tempo);
    } else if (L->etapa == LAB_INTRO) {
        static const char *passos[4] = { "1 CONCEITO", "2 EXPERIMENTE", "3 APLIQUE", "4 ENTENDA" };
        for (int i = 0; i < 4; i++) {
            int x = 44 + i * 180;
            DrawRectangle(x, 86, 168, 20, (i == 0) ? Fade(cor, 0.3f) : Fade(GRAY, 0.12f));
            DrawText(passos[i], x + 8, 91, 10, (i == 0) ? WHITE : GRAY);
        }
        bool revisao = (b->labIntroVista & (1u << L->tipo)) != 0;
        DrawText(revisao ? "CONCEITO (revisão)" : "CONCEITO NOVO", 44, 118, 10, GOLD);
        int y = 134 + DesenharTextoLab(CONCEITOS_LAB[L->tipo], 44, 134, 712, 12, 17, WHITE) + 12;

        const char *missao;
        const char *titulo;
        switch (L->tipo) {
            case LAB_PROMPTS:  titulo = MISSOES_PROMPT[L->missao].titulo;   missao = MISSOES_PROMPT[L->missao].missao;   break;
            case LAB_ARQUIVO:  titulo = DESAFIOS_ARQUIVO[L->missao].titulo; missao = "Uma IA respondeu com várias afirmações. Encontre a suspeita e diga como verificá-la."; break;
            case LAB_NUCLEO:   titulo = DESAFIOS_NUCLEO[L->missao].titulo;  missao = DESAFIOS_NUCLEO[L->missao].tarefa; break;
            case LAB_FIREWALL: titulo = CASOS_VIES[L->missao].titulo;       missao = "Analise um caso de viés algorítmico e depois decida quais dados podem chegar a uma IA."; break;
            default:           titulo = CASOS_PESQUISA[L->missao].titulo;   missao = "Investigue uma mídia suspeita: marque os sinais de alerta, consulte as fontes e dê o veredito."; break;
        }
        DrawText(TextFormat("MISSÃO: %s", titulo), 44, y, 12, cor);
        y += 18;
        y += DesenharTextoLab(missao, 44, y, 712, 12, 16, LIGHTGRAY) + 12;

        if (L->tipo == LAB_NUCLEO)
            y += DesenharTextoLab(AVISO_TOKENS, 44, y, 712, 10, 13, Fade(SKYBLUE, 0.85f)) + 8;

        DesenharTextoLab(LAB_ESPECIAL[L->tipo] ? "Sala especial: opcional, com recompensa maior. A porta já está aberta."
                                                : "Sala comum: opcional. A porta já está aberta; concluir dá a recompensa (o desempenho decide quanto).",
                         44, y, 712, 10, 13, LAB_ESPECIAL[L->tipo] ? GOLD : LIGHTGRAY);
        y += 30;
        DesenharTextoLab("A Luci pode ajudar: DICA (1 de carga), ANALISAR e VERIFICAR (2), REFORMULAR (3). A cada 3 de carga ela perde 1 de "
                         "estabilidade, e instável ela pode errar com muita confiança. V confere uma sugestão dela (Lente ou 1 Fragmento).",
                         44, y, 712, 10, 13, (Color){ 0, 228, 255, 255 });
    } else if (L->etapa == LAB_RESULTADO) {
        DesenharLabResultado(b, tempo);
    } else {
        switch (L->tipo) {
            case LAB_PROMPTS:  DesenharLabPrompt(b, tempo);   break;
            case LAB_ARQUIVO:  DesenharLabArquivo(b, tempo);  break;
            case LAB_NUCLEO:   DesenharLabNucleo(b, tempo);   break;
            case LAB_FIREWALL: DesenharLabFirewall(b, tempo); break;
            default:           DesenharLabPesquisa(b, tempo); break;
        }

        // Faixa da Luci: a fala dela e o resultado da conferência
        Rectangle faixa = { 44, LAB_Y_LUCI, 712, 52 };
        DrawRectangleRec(faixa, Fade((Color){ 0, 40, 60, 255 }, 0.55f));
        DrawRectangleLinesEx(faixa, 1, Fade((Color){ 0, 228, 255, 255 }, 0.35f));
        DesenharLuciTamanho((Vector2){ 64, LAB_Y_LUCI + 26 }, 30.0f);
        Color corFala = L->luciVerificada ? (L->luciAlucinou ? RED : LIME) : (Color){ 120, 230, 255, 255 };
        int gx = (e <= 2 && L->luciTemSugestao && ((int)(tempo * 11.0f)) % 9 == 0) ? GetRandomValue(-2, 2) : 0;
        const char *fala = (L->luciTexto[0] != '\0') ? L->luciTexto : "Luci: Estou aqui se precisar. Cada ajuda gasta um pouco da minha estabilidade.";
        int alt = DesenharTextoLab(fala, 90 + gx, LAB_Y_LUCI + 6, 656, 10, 12, corFala);
        if (L->conferencia[0] != '\0')
            DesenharTextoLab(L->conferencia, 90, LAB_Y_LUCI + 8 + alt, 656, 10, 12, L->luciAlucinou ? RED : GOLD);
    }

    // Botões
    BotaoLab bt[MAX_BOTOES_LAB];
    int n = LabMontarBotoes(b, bt, MAX_BOTOES_LAB);
    for (int i = 0; i < n; i++)
        DesenharBotaoLab(&bt[i], i == L->foco, tempo);
    DesenharMarcaLuciLab(b, bt, n, tempo);
    DesenharAnimPecaLab(L);

    const char *ajuda = (L->tipo == LAB_PROMPTS && L->etapa == LAB_DESAFIO)
                      ? "MOUSE ou 1-3 peça   0 vazio   A/D encaixe   ENTER enviar   H dica   V conferir a Luci   BACKSPACE sair"
                      : "MOUSE ou W/S escolher   E/ENTER confirmar   H dica da Luci   V conferir a Luci   BACKSPACE sair (dá para voltar)";
    DrawText(ajuda, LARGURA_TELA / 2 - MeasureText(ajuda, 10) / 2, 516, 10, GRAY);
}

// Núcleo de corrupção: cristal que gira e brilha antes do anel de balas
void DesenharNucleoCorrupcao(const Inimigo *e, float escala, float t)
{
    Color cor = DEF_INIMIGOS[INIMIGO_NUCLEO].cor;
    float r = e->raio * escala;
    float aviso = (e->estado == 1) ? 1.0f - e->timer / NUCLEO_PREPARO : 0.0f;

    DrawCircleV(e->pos, r + 10 + 8 * aviso, Fade(cor, 0.12f + 0.25f * aviso));
    DrawPoly(e->pos, 4, r, t * 50.0f, (Color){ 60, 10, 70, 255 });
    DrawPolyLines(e->pos, 4, r, t * 50.0f, cor);
    DrawPoly(e->pos, 4, r * 0.55f, -t * 80.0f, Fade(cor, 0.8f));
    DrawCircleV(e->pos, r * 0.25f, WHITE);
    for (int k = 0; k < 3; k++) {
        float a = t * 2.0f + (float)k * 2.0f * PI / 3.0f;
        DrawCircleV((Vector2){ e->pos.x + cosf(a) * (r + 8), e->pos.y + sinf(a) * (r + 8) }, 3, Fade(cor, 0.8f));
    }
}

// Aura de elite, marca do alvo prioritário e rótulo (desenhados junto com o inimigo)
void DesenharMarcasReformulacao(const Inimigo *e, float t)
{
    if (e->elite) {
        for (int k = 0; k < 8; k++) {
            float a = t * 1.5f + (float)k * PI / 4.0f;
            Vector2 p = { e->pos.x + cosf(a) * (e->raio + 9), e->pos.y + sinf(a) * (e->raio + 9) };
            DrawCircleV(p, 2, GOLD);
        }
        // Coroa pequena sobre a cabeça
        Vector2 c = { e->pos.x, e->pos.y - e->raio - 18 };
        DrawTriangle((Vector2){ c.x - 7, c.y + 4 }, (Vector2){ c.x - 2, c.y + 4 }, (Vector2){ c.x - 6, c.y - 4 }, GOLD);
        DrawTriangle((Vector2){ c.x - 3, c.y + 4 }, (Vector2){ c.x + 3, c.y + 4 }, (Vector2){ c.x, c.y - 6 }, GOLD);
        DrawTriangle((Vector2){ c.x + 2, c.y + 4 }, (Vector2){ c.x + 7, c.y + 4 }, (Vector2){ c.x + 6, c.y - 4 }, GOLD);
    }

    if (e->prioritario) {
        float r = e->raio + 16;
        for (int k = 0; k < 4; k++) {
            float a = t * 2.0f + (float)k * PI / 2.0f;
            DrawLineEx((Vector2){ e->pos.x + cosf(a) * r, e->pos.y + sinf(a) * r },
                       (Vector2){ e->pos.x + cosf(a) * (r - 8), e->pos.y + sinf(a) * (r - 8) }, 3, RED);
        }
        const char *txt = "ALVO PRIORITÁRIO";
        DrawText(txt, (int)e->pos.x - MeasureText(txt, 10) / 2, (int)(e->pos.y + e->raio + 18), 10, RED);
    }

    if (e->gelo > 0.0f) {                       // congelado: casca de gelo azul
        DrawCircleV(e->pos, e->raio + 3, Fade((Color){ 140, 220, 255, 255 }, 0.28f));
        DrawCircleLines((int)e->pos.x, (int)e->pos.y, e->raio + 3, (Color){ 190, 240, 255, 255 });
    }
    if (e->queima > 0.0f) {                     // queimando: chamas subindo
        for (int k = 0; k < 3; k++) {
            float f = fmodf(e->anim * 2.0f + k * 0.33f, 1.0f);
            DrawCircleV((Vector2){ e->pos.x + (k - 1) * e->raio * 0.55f, e->pos.y + e->raio * 0.3f - f * e->raio * 1.6f }, 3.5f * (1.0f - f) + 1.0f, Fade(f < 0.5f ? ORANGE : RED, 0.9f - f * 0.6f));
        }
    }

    if (e->rotulo > 0.0f) {
        Vector2 c = { e->pos.x + e->raio + 4, e->pos.y - e->raio - 4 };
        DrawRectangle((int)c.x - 5, (int)c.y - 5, 10, 10, (Color){ 255, 109, 194, 255 });
        DrawText("R", (int)c.x - 3, (int)c.y - 4, 8, BLACK);
    }
}

// Coluna da build no HUD (abaixo dos upgrades passivos)
void DesenharHudBuild(const Biblioteca *b, int yInicio, float tempo)
{
    int y = yInicio;
    int mostrados = 0, total = 0;

    for (int u = 0; u < NUM_UPGRADES; u++)
        if (b->j.up[u] > 0) total++;

    for (int u = 0; u < NUM_UPGRADES && y < 470; u++) {
        if (b->j.up[u] <= 0) continue;
        OpcaoEscolha o = { OPC_UPGRADE, u };
        Color cor = CORES_RARIDADE[UPGRADES[u].raridade];
        DrawRectangle(770, y - 11, 22, 22, Fade(BLACK, 0.6f));
        DrawRectangleLines(770, y - 11, 22, 22, Fade(cor, 0.8f));
        DesenharIconeUpgrade(b, o, (Vector2){ 781, (float)y }, 0.55f, tempo);
        if (UPGRADES[u].maxNivel > 1) {
            const char *nivel = TextFormat("%d", b->j.up[u]);
            DrawText(nivel, 791 - MeasureText(nivel, 8), y + 2, 8, WHITE);     // alinhado à direita: não sai da tela
        }
        y += 26;
        mostrados++;
    }

    if (total > mostrados) {
        const char *mais = TextFormat("+%d", total - mostrados);
        DrawText(mais, 792 - MeasureText(mais, 10), y - 6, 10, GOLD);
    }

    if (b->j.penalidade) {
        // A etiqueta cresce para a esquerda conforme o texto (com o TEXTO GRANDE ela não cabe em 22 px)
        int largBeta = MeasureText("BETA", 8) + 4;
        if (largBeta < 22) largBeta = 22;
        DrawRectangle(792 - largBeta, y + 4, largBeta, 16, Fade(RED, 0.4f));
        DrawText("BETA", 794 - largBeta, y + 6, 8, WHITE);
    }
}
