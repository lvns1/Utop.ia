#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../../include/core/fonte.h"
#include "../../include/core/config.h"
#include "../../include/core/mapas.h"
#include "../../include/core/texto.h"

// =============================================================
// FUNÇÕES AUXILIARES - TEXTO
// =============================================================

// Copia "texto" para "saida" inserindo '\n' nos espaços para que
// nenhuma linha passe de "larguraMax" pixels (DrawText não quebra linha sozinho).
static void QuebrarTextoCalcular(const char *texto, int larguraMax, int tamFonte, char *saida, int tamSaida)
{
    int pos = 0;
    int inicioLinha = 0;
    int ultimoEspaco = -1;

    for (int i = 0; texto[i] != '\0' && pos < tamSaida - 1; i++) {

        char c = texto[i];
        saida[pos++] = c;
        saida[pos] = '\0';

        if (c == '\n') {
            inicioLinha = pos;
            ultimoEspaco = -1;
            continue;
        }

        if (c == ' ')
            ultimoEspaco = pos - 1;

        if (ultimoEspaco >= inicioLinha &&
            MeasureText(saida + inicioLinha, tamFonte) > larguraMax) {

            saida[ultimoEspaco] = '\n';
            inicioLinha = ultimoEspaco + 1;
            ultimoEspaco = -1;
        }
    }

    saida[pos] = '\0';
}

// Versão com cache: as telas de escolha, loja e troca chamam isto todo frame com o mesmo texto,
// e medir caractere por caractere é caro. Guarda os últimos resultados (texto + largura + tamanho).
#define CACHE_QUEBRA_N      48
#define CACHE_QUEBRA_TEXTO  400
#define CACHE_QUEBRA_SAIDA  800
typedef struct {
    bool usado;
    int larg, tam;
    char origem[CACHE_QUEBRA_TEXTO];
    char resultado[CACHE_QUEBRA_SAIDA];
} EntradaQuebra;

void QuebrarTexto(const char *texto, int larguraMax, int tamFonte, char *saida, int tamSaida)
{
    static EntradaQuebra cache[CACHE_QUEBRA_N];
    static int proximo = 0;
    size_t tamTexto = strlen(texto);

    if (tamTexto >= CACHE_QUEBRA_TEXTO) {
        QuebrarTextoCalcular(texto, larguraMax, tamFonte, saida, tamSaida);
        return;
    }

    for (int i = 0; i < CACHE_QUEBRA_N; i++) {
        const EntradaQuebra *e = &cache[i];
        if (e->usado && e->larg == larguraMax && e->tam == tamFonte && strcmp(e->origem, texto) == 0 &&
            strlen(e->resultado) < (size_t)tamSaida) {
            strcpy(saida, e->resultado);
            return;
        }
    }

    QuebrarTextoCalcular(texto, larguraMax, tamFonte, saida, tamSaida);

    if (strlen(saida) < CACHE_QUEBRA_SAIDA) {
        EntradaQuebra *e = &cache[proximo];
        proximo = (proximo + 1) % CACHE_QUEBRA_N;
        e->usado = true;
        e->larg = larguraMax;
        e->tam = tamFonte;
        strcpy(e->origem, texto);
        strcpy(e->resultado, saida);
    }
}

// Desenha um texto com várias linhas (separadas por '\n') e devolve quantas linhas desenhou.
int DesenharTextoLinhas(const char *texto, int x, int y, int tamFonte, int espacoLinha, Color cor)
{
    char linha[512];
    int n = 0;
    int linhas = 0;

    for (const char *p = texto; ; p++) {

        if (*p == '\n' || *p == '\0') {

            linha[n] = '\0';
            DrawText(linha, x, y + linhas * espacoLinha, tamFonte, cor);
            linhas++;
            n = 0;

            if (*p == '\0')
                break;

        } else if (n < 511) {

            linha[n++] = *p;
        }
    }

    return linhas;
}

int ContarLinhas(const char *texto)
{
    int n = 1;

    for (; *texto != '\0'; texto++)
        if (*texto == '\n')
            n++;

    return n;
}

// ---------- Mouse nos menus ----------
// Todo menu do jogo aceita teclado E mouse: passar o mouse por cima marca a opção,
// o clique escolhe, e nas telas de texto o clique vale como ENTER.
bool MouseMexeu(void)     { return Vector2Length(GetMouseDelta()) > 0.5f; }
bool CliqueMouse(void)    { return IsMouseButtonPressed(MOUSE_BUTTON_LEFT); }
bool MouseEm(Rectangle r) { return CheckCollisionPointRec(GetMousePosition(), r); }

// "ENTER / CLIQUE >" piscando, terminando onde terminava o antigo "ENTER >"
void DesenharDicaAvancar(int x, int y, int tam)
{
    const char *dica = "ENTER / CLIQUE >";
    int fim = x + MeasureText("ENTER >", tam);
    DrawText(dica, fim - MeasureText(dica, tam), y, tam, GOLD);
}

// Botão "X" de fechar janela (faz o mesmo que ESC / BACKSPACE)
void DesenharBotaoFechar(Rectangle r, Color cor)
{
    bool sobre = MouseEm(r);
    Color corX = sobre ? WHITE : cor;
    float m = 6.0f;

    DrawRectangleRec(r, sobre ? Fade(cor, 0.35f) : Fade(BLACK, 0.6f));
    DrawRectangleLinesEx(r, 2, corX);
    DrawLineEx((Vector2){ r.x + m, r.y + m }, (Vector2){ r.x + r.width - m, r.y + r.height - m }, 2.0f, corX);
    DrawLineEx((Vector2){ r.x + r.width - m, r.y + m }, (Vector2){ r.x + m, r.y + r.height - m }, 2.0f, corX);
}

// Botão de texto: acende com o mouse em cima ou quando o teclado o marca
void DesenharBotaoMenu(Rectangle r, const char *texto, int tam, Color cor, bool marcado)
{
    bool aceso = marcado || MouseEm(r);

    DrawRectangleRec(r, aceso ? Fade(cor, 0.25f) : Fade(BLACK, 0.55f));
    DrawRectangleLinesEx(r, 2, aceso ? cor : Fade(GRAY, 0.7f));
    DrawText(texto, (int)(r.x + r.width / 2) - MeasureText(texto, tam) / 2, (int)(r.y + r.height / 2) - tam / 2, tam, aceso ? cor : LIGHTGRAY);
}

// Texto clicável (atalhos escritos nas janelas, como "TAB - saber mais")
Rectangle RetTextoClicavel(const char *texto, int x, int y, int tam)
{
    return (Rectangle){ (float)x - 3, (float)y - 3, (float)MeasureText(texto, tam) + 6, (float)tam + 6 };
}

void DesenharTextoClicavel(const char *texto, int x, int y, int tam, Color cor)
{
    bool sobre = MouseEm(RetTextoClicavel(texto, x, y, tam));

    DrawText(texto, x, y, tam, sobre ? WHITE : cor);

    if (sobre)
        DrawLine(x, y + tam + 1, x + MeasureText(texto, tam), y + tam + 1, WHITE);
}

// Letras com acento ocupam 2 bytes em UTF-8. Na máquina de escrever,
// isso garante que nunca aparece "meia letra" na tela.
static int FimDoCaractere(const char *s, int n)
{
    while (s[n] != '\0' && (((unsigned char)s[n]) & 0xC0) == 0x80)
        n++;

    return n;
}

// Copia a parte do texto que já "apareceu" na máquina de escrever
bool TextoParcial(const char *texto, float visiveis, char *saida, int tamSaida)
{
    int total = (int)strlen(texto);
    int n = FimDoCaractere(texto, (int)visiveis);

    if (n > total) n = total;
    if (n > tamSaida - 1) n = tamSaida - 1;

    memcpy(saida, texto, (size_t)n);
    saida[n] = '\0';

    return (n >= total);
}

// Gerador de "ruído" para chiados e glitches (sempre igual para o mesmo número)
unsigned int Ruido(unsigned int n)
{
    n = (n << 13) ^ n;
    return n * (n * n * 15731u + 789221u) + 1376312589u;
}

// Coração em pixel art (a "alma" do Undertale)
void DesenharCoracao(float cx, float cy, int pixel, Color cor)
{
    static const char *forma[6] = {
        ".XX.XX.",
        "XXXXXXX",
        "XXXXXXX",
        ".XXXXX.",
        "..XXX..",
        "...X..."
    };

    float x0 = cx - 3.5f * pixel;
    float y0 = cy - 3.0f * pixel;

    for (int l = 0; l < 6; l++)
        for (int c = 0; c < 7; c++)
            if (forma[l][c] == 'X')
                DrawRectangle((int)(x0 + c * pixel), (int)(y0 + l * pixel), pixel, pixel, cor);
}
