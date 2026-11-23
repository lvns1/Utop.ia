#define UTOPIA_FONTE_IMPL
#include "../../include/core/fonte.h"

// =============================================================
// FONTE DO JOGO
// =============================================================
#define ARQUIVO_FONTE_JOGO   "resources/fonts/DejaVuSansCondensed-Bold.ttf"
#define NUM_FONTES_JOGO      3

static const int TAMANHOS_FONTE_JOGO[NUM_FONTES_JOGO] = { 16, 28, 56 };   // cada tamanho usa o atlas mais próximo (fica nítido)
static Font g_fontesJogo[NUM_FONTES_JOGO];
static bool g_fonteJogoOk = false;

void CarregarFonteJogo(void)
{
    int pontos[256];
    int n = 0;

    for (int c = 32; c < 127; c++)  pontos[n++] = c;        // ASCII
    for (int c = 160; c < 256; c++) pontos[n++] = c;        // Latin-1 (acentos)
    pontos[n++] = 0x2192;                                    // →
    pontos[n++] = 0x2194;                                    // ↔
    pontos[n++] = 0x2227;                                    // ∧
    pontos[n++] = 0x2228;                                    // ∨

    if (!FileExists(ARQUIVO_FONTE_JOGO))
        return;

    for (int i = 0; i < NUM_FONTES_JOGO; i++) {
        g_fontesJogo[i] = LoadFontEx(ARQUIVO_FONTE_JOGO, TAMANHOS_FONTE_JOGO[i], pontos, n);
        if (g_fontesJogo[i].texture.id == 0)
            return;
        SetTextureFilter(g_fontesJogo[i].texture, TEXTURE_FILTER_BILINEAR);
    }

    g_fonteJogoOk = true;
}

void DescarregarFonteJogo(void)
{
    if (!g_fonteJogoOk)
        return;

    for (int i = 0; i < NUM_FONTES_JOGO; i++)
        UnloadFont(g_fontesJogo[i]);

    g_fonteJogoOk = false;
}

// Textos pequenos (<= 12) ganham um pouco de tamanho: eram o maior motivo de reclamação
static float TamanhoFonteJogo(int tam)
{
    return (tam <= 12) ? tam * 1.1f : (float)tam;
}

static Font FonteJogoPara(float tam)
{
    for (int i = 0; i < NUM_FONTES_JOGO - 1; i++)
        if (tam <= (float)TAMANHOS_FONTE_JOGO[i])
            return g_fontesJogo[i];

    return g_fontesJogo[NUM_FONTES_JOGO - 1];
}

void DesenharTextoJogo(const char *texto, int x, int y, int tam, Color cor)
{
    if (!g_fonteJogoOk) {
        DrawText(texto, x, y, tam, cor);
        return;
    }

    // Textos pequenos em cinza (dicas de tecla) quase sumiam no fundo escuro: clareia
    if (tam <= 12 && cor.r == GRAY.r && cor.g == GRAY.g && cor.b == GRAY.b)
        cor = (Color){ 190, 190, 200, cor.a };

    float t = TamanhoFonteJogo(tam);
    DrawTextEx(FonteJogoPara(t), texto, (Vector2){ (float)x, (float)y }, t, t * 0.03f, cor);
}

int MedirTextoJogo(const char *texto, int tam)
{
    if (!g_fonteJogoOk)
        return MeasureText(texto, tam);

    float t = TamanhoFonteJogo(tam);
    return (int)MeasureTextEx(FonteJogoPara(t), texto, t, t * 0.03f).x;
}
