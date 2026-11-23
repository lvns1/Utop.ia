#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <time.h>
#include "../../include/core/fonte.h"
#include "../../include/core/mapas.h"
#include "../../include/telas/intro.h"
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
#include "../../include/telas/pause.h"
#include "../../include/telas/ranking.h"
#include "../../include/telas/resolucao.h"

// =============================================================
// CENAS ANIMADAS DA INTRODUÇÃO
//   Uma ilustração para cada texto do narrador, desenhada por código
//   dentro do painel de imagem da intro:
//     0 - a IA na rotina (textos, imagens, decisões)
//     1 - dados incorretos e perguntas enviesadas -> alucinação
//     2 - UtopIA, o universo interior, entrando em colapso
//     3 - Luci fazendo o registro de identidade do jogador
// =============================================================

static const Color COR_FUNDO_CENA = { 8, 6, 18, 255 };
static const Color COR_PAREDE_CENA = { 18, 15, 30, 255 };
static const Color COR_MOVEL_CENA = { 45, 30, 70, 255 };
static const Color COR_CABELO = { 96, 56, 19, 255 };
static const Color COR_CABELO_CLARO = { 140, 98, 57, 255 };
static const Color COR_CAMISA = { 204, 204, 204, 255 };
static const Color COR_CORDAO = { 247, 147, 30, 255 };

// Entre 0 e 1: quanto um elemento que aparece no instante "inicio" já surgiu
static float Surgir(float t, float inicio, float duracao)
{
    float k = (t - inicio) / duracao;
    return (k < 0.0f) ? 0.0f : (k > 1.0f ? 1.0f : k);
}

// ---------- Cena 0: a IA faz parte da rotina ----------
static void CenaRotina(float x0, float y0, float t, float tempo)
{
    // Parede e mesa
    DrawRectangle((int)x0, (int)y0, 500, 255, COR_PAREDE_CENA);
    DrawRectangle((int)x0, (int)y0 + 196, 340, 59, (Color){ 26, 20, 46, 255 });
    DrawRectangle((int)x0 + 20, (int)y0 + 190, 300, 10, COR_MOVEL_CENA);

    // Notebook
    Rectangle tela = { x0 + 85, y0 + 70, 170, 112 };
    Rectangle visor = { tela.x + 7, tela.y + 7, tela.width - 14, tela.height - 14 };

    DrawRectangleRec(tela, (Color){ 30, 28, 44, 255 });
    DrawRectangleLinesEx(tela, 2, SKYBLUE);
    DrawRectangleRec(visor, (Color){ 12, 30, 60, 255 });
    DrawRectangle((int)tela.x - 14, (int)(tela.y + tela.height), (int)tela.width + 28, 8, (Color){ 60, 50, 90, 255 });

    // Conversa com a IA na tela
    float balaoUsuario = Surgir(t, 0.3f, 0.3f);
    float balaoIA = Surgir(t, 0.9f, 0.3f);

    if (balaoUsuario > 0.0f) {
        Rectangle b = { visor.x + visor.width - 82, visor.y + 8, 74, 18 };
        DrawRectangleRec(b, Fade(LIGHTGRAY, balaoUsuario));
        DrawText("me ajuda?", (int)b.x + 8, (int)b.y + 4, 10, Fade((Color){ 30, 28, 44, 255 }, balaoUsuario));
    }

    if (balaoIA > 0.0f) {

        Rectangle b = { visor.x + 8, visor.y + 34, 112, 50 };
        DrawRectangleRec(b, Fade(SKYBLUE, 0.35f * balaoIA));
        DrawRectangleLinesEx(b, 1, Fade(SKYBLUE, balaoIA));

        if (t < 2.0f) {
            // "digitando..."
            for (int i = 0; i < 3; i++) {
                float pulo = sinf(tempo * 8.0f - i * 0.8f) * 2.0f;
                DrawCircle((int)b.x + 18 + i * 12, (int)(b.y + 25 + pulo), 3, Fade(WHITE, balaoIA));
            }
        } else {
            // a resposta vai sendo escrita
            static const int larguras[3] = { 92, 78, 56 };
            for (int i = 0; i < 3; i++) {
                float k = Surgir(t, 2.0f + i * 0.35f, 0.35f);
                DrawRectangle((int)b.x + 8, (int)b.y + 9 + i * 13, (int)(larguras[i] * k), 5, Fade(WHITE, 0.8f));
            }
        }
    }

    // Thomas de costas, olhando o notebook (mesmas cores do sprite)
    DrawEllipse((int)x0 + 170, (int)y0 + 262, 60, 28, COR_CAMISA);
    DrawRectangle((int)x0 + 150, (int)y0 + 236, 40, 5, COR_CORDAO);
    DrawCircle((int)x0 + 170, (int)y0 + 214, 27, COR_CABELO);
    DrawCircle((int)x0 + 160, (int)y0 + 204, 6, COR_CABELO_CLARO);
    DrawCircle((int)x0 + 181, (int)y0 + 212, 4, COR_CABELO_CLARO);

    // Três usos da IA aparecendo um de cada vez
    const char *nomes[3] = { "TEXTOS", "IMAGENS", "DECISÕES" };
    const char *usos[3] = { "escrever", "gerar", "ajudar a decidir" };
    Color cores[3] = { SKYBLUE, GOLD, LIME };

    for (int i = 0; i < 3; i++) {

        float k = Surgir(t, 1.2f + i * 0.7f, 0.45f);

        if (k <= 0.0f)
            continue;

        float bob = sinf(tempo * 2.0f + i) * 3.0f;
        Rectangle card = { x0 + 340 + (1.0f - k) * 40.0f, y0 + 20 + i * 76 + bob, 146, 58 };

        // Dados saindo do notebook até o card
        for (int d = 0; d < 4; d++) {
            float f = fmodf(tempo * 0.8f + d * 0.25f, 1.0f);
            Vector2 a = { tela.x + tela.width, tela.y + 40 };
            Vector2 b = { card.x, card.y + card.height / 2 };
            DrawCircle((int)Lerp(a.x, b.x, f), (int)Lerp(a.y, b.y, f), 2, Fade(cores[i], 0.6f * k));
        }

        DrawRectangleRec(card, Fade((Color){ 26, 20, 46, 255 }, k));
        DrawRectangleLinesEx(card, 2, Fade(cores[i], k));

        int ix = (int)card.x + 12, iy = (int)card.y + 14;

        if (i == 0) {
            // documento sendo escrito
            DrawRectangle(ix, iy, 24, 30, Fade((Color){ 230, 230, 240, 255 }, k));
            for (int l = 0; l < 4; l++) {
                float e = fmodf(tempo * 1.2f, 4.0f) - l * 0.6f;
                e = (e < 0.0f) ? 0.0f : (e > 1.0f ? 1.0f : e);
                DrawRectangle(ix + 4, iy + 5 + l * 6, (int)(16 * e), 2, Fade(DARKGRAY, k));
            }
        } else if (i == 1) {
            // imagem com sol e montanhas
            DrawRectangle(ix - 2, iy + 2, 32, 26, Fade((Color){ 12, 30, 60, 255 }, k));
            DrawCircle(ix + 22, iy + 9, 4, Fade(GOLD, k));
            DesenharTrianguloB2((Vector2){ (float)ix + 9, (float)iy + 10 }, (Vector2){ (float)ix - 1, (float)iy + 27 }, (Vector2){ (float)ix + 19, (float)iy + 27 }, Fade(SKYBLUE, k));
            DesenharTrianguloB2((Vector2){ (float)ix + 20, (float)iy + 14 }, (Vector2){ (float)ix + 11, (float)iy + 27 }, (Vector2){ (float)ix + 29, (float)iy + 27 }, Fade(PURPLE, k));
        } else {
            // decisão: um "ok" dentro de um círculo
            DrawCircleLines(ix + 13, iy + 15, 13, Fade(LIME, k));
            DrawLineEx((Vector2){ (float)ix + 6, (float)iy + 15 }, (Vector2){ (float)ix + 11, (float)iy + 21 }, 3, Fade(LIME, k));
            DrawLineEx((Vector2){ (float)ix + 11, (float)iy + 21 }, (Vector2){ (float)ix + 21, (float)iy + 8 }, 3, Fade(LIME, k));
        }

        DrawText(nomes[i], (int)card.x + 50, (int)card.y + 14, 12, Fade(WHITE, k));
        DrawText(usos[i], (int)card.x + 50, (int)card.y + 32, 10, Fade(LIGHTGRAY, k));
    }
}

// ---------- Cena 1: dados ruins entram, alucinação sai ----------
static void CenaAlucinacao(float x0, float y0, float t, float tempo)
{
    DrawRectangle((int)x0, (int)y0, 500, 255, COR_PAREDE_CENA);

    Vector2 chip = { x0 + 250, y0 + 122 };
    float intervalo = 0.4f, viagem = 1.6f;
    float glitch = 0.0f;

    // Legenda da entrada
    DrawText("ENTRADA", (int)x0 + 14, (int)y0 + 12, 10, GRAY);
    DrawRectangle((int)x0 + 14, (int)y0 + 30, 10, 8, SKYBLUE);
    DrawText("dado correto", (int)x0 + 30, (int)y0 + 29, 10, LIGHTGRAY);
    DrawRectangle((int)x0 + 14, (int)y0 + 46, 10, 8, RED);
    DrawText("dado incorreto", (int)x0 + 30, (int)y0 + 45, 10, LIGHTGRAY);
    DrawRectangle((int)x0 + 14, (int)y0 + 62, 10, 8, ORANGE);
    DrawText("pergunta enviesada", (int)x0 + 30, (int)y0 + 61, 10, LIGHTGRAY);

    // Blocos de dados viajando até o chip
    int ultimo = (int)(t / intervalo);

    for (int k = ultimo - 6; k <= ultimo; k++) {

        if (k < 0) continue;

        float idade = t - k * intervalo;
        int tipo = (k % 4 == 2) ? 1 : (k % 4 == 0 ? 2 : 0);     // 0 certo, 1 errado, 2 pergunta enviesada

        if (idade < viagem) {

            float f = idade / viagem;
            float x = Lerp(x0 + 20, chip.x - 60, f);
            float y = chip.y + 50 * (1.0f - f) * sinf(k * 1.7f) + 8 * sinf(tempo * 3 + k);
            Color cor = (tipo == 0) ? SKYBLUE : (tipo == 1 ? RED : ORANGE);
            const char *g = (tipo == 0) ? "01" : (tipo == 1 ? "!?" : "?");

            DrawRectangle((int)x - 11, (int)y - 8, 22, 16, cor);
            DrawText(g, (int)x - MeasureText(g, 10) / 2, (int)y - 5, 10, (Color){ 20, 16, 34, 255 });

        } else if (tipo != 0 && idade < viagem + 0.5f) {

            // dado ruim acabou de entrar: o chip falha
            glitch += (viagem + 0.5f - idade) / 0.5f;
        }
    }

    if (glitch > 1.0f) glitch = 1.0f;

    // Chip da IA (com aberração cromática quando falha)
    Vector2 c = chip;
    Rectangle corpo = { c.x - 42, c.y - 42, 84, 84 };

    for (int i = 0; i < 5; i++) {
        DrawRectangle((int)corpo.x - 8, (int)corpo.y + 10 + i * 15, 8, 4, GRAY);
        DrawRectangle((int)(corpo.x + corpo.width), (int)corpo.y + 10 + i * 15, 8, 4, GRAY);
        DrawRectangle((int)corpo.x + 10 + i * 15, (int)corpo.y - 8, 4, 8, GRAY);
        DrawRectangle((int)corpo.x + 10 + i * 15, (int)(corpo.y + corpo.height), 4, 8, GRAY);
    }

    if (glitch > 0.05f) {
        DrawRectangleLinesEx((Rectangle){ corpo.x - 4 * glitch, corpo.y, corpo.width, corpo.height }, 3, Fade(RED, 0.6f));
        DrawRectangleLinesEx((Rectangle){ corpo.x + 4 * glitch, corpo.y, corpo.width, corpo.height }, 3, Fade(SKYBLUE, 0.6f));
    }

    DrawRectangleRec(corpo, (Color){ 30, 28, 44, 255 });
    DrawRectangleLinesEx(corpo, 3, (glitch > 0.3f) ? MAGENTA : SKYBLUE);
    DrawText("IA", (int)c.x - MeasureText("IA", 12) / 2, (int)corpo.y + 8, 12, LIGHTGRAY);

    Color olho = (glitch > 0.3f) ? MAGENTA : SKYBLUE;
    DrawCircle((int)c.x, (int)c.y + 6, 18, Fade(olho, 0.3f));
    DrawCircle((int)c.x, (int)c.y + 6, 10, olho);
    DrawCircle((int)(c.x + sinf(tempo * 2.0f) * 4), (int)c.y + 6, 4, (Color){ 20, 16, 34, 255 });

    // Resposta alucinada, dita com toda a confiança
    static const char *respostas[3] = { "Com certeza!\n2 + 2 = 5.", "Fato: a Lua\ntem luz própria.", "O Brasil tem\n50 estados!" };
    int r = ((int)(tempo / 2.4f)) % 3;
    float k = Surgir(t, 1.8f, 0.4f);

    if (k > 0.0f) {

        Rectangle balao = { chip.x + 68, chip.y - 62, 160, 64 };
        float dx = 0.0f;

        DrawLineEx((Vector2){ chip.x + 46, chip.y - 10 }, (Vector2){ balao.x, balao.y + 44 }, 2, Fade(LIGHTGRAY, k));
        DrawRectangleRec(balao, Fade((Color){ 245, 245, 245, 255 }, k));

        if (glitch > 0.3f) {
            DesenharTextoLinhas(respostas[r], (int)(balao.x + 12 + dx - 2), (int)balao.y + 14, 14, 18, Fade(RED, 0.5f));
            DesenharTextoLinhas(respostas[r], (int)(balao.x + 12 + dx + 2), (int)balao.y + 14, 14, 18, Fade(SKYBLUE, 0.5f));
        }

        DesenharTextoLinhas(respostas[r], (int)(balao.x + 12 + dx), (int)balao.y + 14, 14, 18, Fade((Color){ 30, 28, 44, 255 }, k));

        // Aviso piscando
        if (((int)(tempo * 3.0f)) % 2 == 0) {
            int ax = (int)balao.x + 20, ay = (int)(balao.y + balao.height) + 18;
            DesenharTrianguloB2((Vector2){ (float)ax, (float)ay - 9 }, (Vector2){ (float)ax - 9, (float)ay + 7 }, (Vector2){ (float)ax + 9, (float)ay + 7 }, RED);
            DrawText("!", ax - 2, ay - 5, 10, WHITE);
            DrawText("ALUCINAÇÃO", ax + 16, ay - 5, 14, RED);
        }
    }

    DrawText("RESPOSTA", (int)x0 + 412, (int)y0 + 12, 10, GRAY);
}

// ---------- Cena 2: UtopIA entrando em colapso ----------
static void CenaUtopia(float x0, float y0, float t, float tempo)
{
    float horizonte = y0 + 150;
    float vx = x0 + 250;

    DrawRectangle((int)x0, (int)y0, 500, 255, COR_FUNDO_CENA);

    // Sol/núcleo roxo com faixas
    DrawCircle((int)vx, (int)horizonte - 30, 66, Fade(PURPLE, 0.25f));
    DrawCircle((int)vx, (int)horizonte - 30, 54, Fade(PURPLE, 0.55f));
    for (int i = 0; i < 5; i++)
        DrawRectangle((int)vx - 70, (int)horizonte - 26 + i * 8, 140, 2 + i, COR_FUNDO_CENA);

    // Cidade de dados no horizonte
    for (int i = 0; i < 14; i++) {

        unsigned int r = Ruido((unsigned int)i * 7u + 3u);
        int largura = 18 + (int)(r % 14);
        int altura = 24 + (int)((r >> 8) % 60);
        int x = (int)x0 + 8 + i * 35;
        bool falhando = (Ruido((unsigned int)(tempo * 6.0f) + (unsigned int)i * 13u) % 9) == 0;

        DrawRectangle(x, (int)horizonte - altura, largura, altura, (Color){ 26, 20, 46, 255 });
        DrawRectangleLines(x, (int)horizonte - altura, largura, altura, Fade(PURPLE, 0.5f));

        for (int j = 0; j < altura / 10; j++) {
            bool aceso = (Ruido((unsigned int)(i * 31 + j * 7) + (unsigned int)(tempo * 2.0f)) % 3) == 0;
            if (aceso) DrawRectangle(x + 4, (int)horizonte - altura + 4 + j * 10, largura - 8, 3, falhando ? MAGENTA : Fade(SKYBLUE, 0.8f));
        }
    }

    // Chão em grade, andando em direção a quem vê
    for (int i = 0; i < 10; i++) {
        float f = (i + fmodf(tempo * 0.8f, 1.0f)) / 10.0f;
        float y = horizonte + f * f * 105.0f;
        DrawLine((int)x0, (int)y, (int)x0 + 500, (int)y, Fade(PURPLE, 0.15f + 0.5f * f));
    }
    for (int i = -8; i <= 8; i++)
        DrawLine((int)vx, (int)horizonte, (int)(vx + i * 70), (int)y0 + 255, Fade(PURPLE, 0.3f));

    // Rachaduras se espalhando a partir do centro
    Vector2 origem = { vx, horizonte + 50 };
    float alcance = Surgir(t, 0.5f, 3.5f);

    for (int c = 0; c < 6; c++) {

        float ang = -PI * 0.15f + c * (PI * 1.3f / 5.0f);
        Vector2 a = origem;
        int segmentos = (int)(8 * alcance);

        for (int s = 0; s < segmentos; s++) {
            float desvio = ((int)(Ruido((unsigned int)(c * 17 + s)) % 41) - 20) * 0.03f;
            Vector2 b = { a.x + cosf(ang + desvio) * 16, a.y + sinf(ang + desvio) * 9 - (c < 3 ? 6 : -4) };
            DrawLineEx(a, b, 2, Fade(MAGENTA, 0.6f + 0.4f * sinf(tempo * 8 + s)));
            a = b;
        }
    }

    // Fragmentos se soltando e subindo
    for (int i = 0; i < 12; i++) {
        float f = fmodf(tempo * 0.35f + i * 0.083f, 1.0f);
        float x = x0 + 30 + (Ruido((unsigned int)i * 11u) % 440);
        float y = horizonte + 40 - f * 170;
        int lado = 4 + i % 4;
        DrawRectangle((int)x, (int)y, lado, lado, Fade((i % 2) ? MAGENTA : SKYBLUE, (1.0f - f) * Surgir(t, 0.8f, 1.0f)));
    }

    // Faixa de glitch de tempos em tempos
    if (fmodf(tempo, 2.2f) < 0.15f) {
        int fy = (int)y0 + 40 + (int)(Ruido((unsigned int)(tempo * 10.0f)) % 170);
        DrawRectangle((int)x0, fy, 500, 6, Fade(MAGENTA, 0.35f));
        DrawRectangle((int)x0 + 20, fy + 8, 460, 2, Fade(SKYBLUE, 0.35f));
    }

    // Nome do mundo e integridade caindo
    const char *nome = "UTOPIA";
    int nx = (int)vx - MeasureText(nome, 24) / 2;
    float g = (fmodf(tempo, 1.7f) < 0.12f) ? 3.0f : 0.0f;

    DrawText(nome, nx - (int)g, (int)y0 + 14, 24, Fade(RED, 0.6f));
    DrawText(nome, nx + (int)g, (int)y0 + 14, 24, Fade(SKYBLUE, 0.6f));
    DrawText(nome, nx, (int)y0 + 14, 24, WHITE);

    int integridade = 87 - (int)(t * 6.0f);
    if (integridade < 12) integridade = 12;

    const char *aviso = TextFormat("INTEGRIDADE DO SISTEMA: %d%%", integridade);
    DrawText(aviso, (int)vx - MeasureText(aviso, 10) / 2, (int)y0 + 42, 10,
             (((int)(tempo * 2.0f)) % 2 == 0 || integridade > 30) ? RED : Fade(RED, 0.4f));
}

// ---------- Cena 3: Luci registra o jogador ----------
static void CenaRegistro(float x0, float y0, float t, float tempo)
{
    DrawRectangle((int)x0, (int)y0, 500, 255, COR_FUNDO_CENA);

    // Pontinhos de grade ao fundo
    for (int y = 12; y < 255; y += 20)
        for (int x = 10; x < 500; x += 20)
            DrawPixel((int)x0 + x, (int)y0 + y, Fade(PURPLE, 0.35f));

    Vector2 p = { x0 + 250, y0 + 118 + sinf(tempo * 2.0f) * 6.0f };

    // Ondas de leitura saindo da Luci
    for (int i = 0; i < 3; i++) {
        float f = fmodf(tempo * 0.6f + i * 0.33f, 1.0f);
        DrawCircleLines((int)p.x, (int)p.y, 50 + f * 110, Fade(SKYBLUE, (1.0f - f) * 0.6f));
    }

    DesenharLuciTamanho(p, 110.0f * Surgir(t, 0.0f, 0.5f) + 1.0f);

    // Linha de leitura varrendo o painel
    float varre = fmodf(tempo * 90.0f, 255.0f);
    DrawRectangle((int)x0 + 1, (int)(y0 + varre), 498, 2, Fade(SKYBLUE, 0.35f));

    // Cantos de visor
    int m = 14, l = 22;
    Color cc = Fade(SKYBLUE, 0.8f);
    DrawRectangle((int)x0 + m, (int)y0 + m, l, 2, cc);           DrawRectangle((int)x0 + m, (int)y0 + m, 2, l, cc);
    DrawRectangle((int)x0 + 500 - m - l, (int)y0 + m, l, 2, cc); DrawRectangle((int)x0 + 500 - m - 2, (int)y0 + m, 2, l, cc);
    DrawRectangle((int)x0 + m, (int)y0 + 255 - m - 2, l, 2, cc); DrawRectangle((int)x0 + m, (int)y0 + 255 - m - l, 2, l, cc);
    DrawRectangle((int)x0 + 500 - m - l, (int)y0 + 255 - m - 2, l, 2, cc); DrawRectangle((int)x0 + 500 - m - 2, (int)y0 + 255 - m - l, 2, l, cc);

    DrawText("LUCI", (int)x0 + 26, (int)y0 + 22, 12, SKYBLUE);

    const char *msg = "IDENTIFICANDO USUÁRIO...";
    if (((int)(tempo * 2.0f)) % 2 == 0)
        DrawText(msg, (int)x0 + 250 - MeasureText(msg, 10) / 2, (int)y0 + 232, 10, LIGHTGRAY);
}

// Desenha a cena da etapa atual dentro do painel de imagem da intro
void DesenharCenaIntro(int etapa, Rectangle painel, float tempo)
{
    static int ultimaEtapa = -1;
    static float inicioEtapa = 0.0f;

    // Trocou de etapa: a nova cena começa do zero
    if (etapa != ultimaEtapa) {
        ultimaEtapa = etapa;
        inicioEtapa = tempo;
    }

    float t = tempo - inicioEtapa;

    BeginScissorMode((int)painel.x + 1, (int)painel.y + 1, (int)painel.width - 2, (int)painel.height - 2);

    switch (etapa) {
        case 0:  CenaRotina(painel.x, painel.y, t, tempo);     break;
        case 1:  CenaAlucinacao(painel.x, painel.y, t, tempo); break;
        case 2:  CenaUtopia(painel.x, painel.y, t, tempo);     break;
        default: CenaRegistro(painel.x, painel.y, t, tempo);   break;
    }

    EndScissorMode();

    // Entrada suave da cena
    float alfa = 1.0f - t * 2.5f;

    if (alfa > 0.0f)
        DrawRectangleRec(painel, Fade(COR_FUNDO_CENA, alfa));

    DrawRectangleLinesEx(painel, 1, Fade(LIGHTGRAY, 0.65f));
}
