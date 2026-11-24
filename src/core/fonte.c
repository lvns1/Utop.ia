#define UTOPIA_FONTE_IMPL
#include "../../include/core/fonte.h"
#include "../../include/core/config.h"

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

// =============================================================
// LEGIBILIDADE (issue #31): tamanho mínimo, texto grande e contraste.
// Todo texto do jogo passa por aqui, então as regras valem em todas as telas.
// =============================================================

static float g_escalaTexto = 1.0f;      // 1 = NORMAL; ESCALA_TEXTO_GRANDE = GRANDE (menu de opções)

void DefinirTextoGrande(bool grande)
{
    g_escalaTexto = grande ? ESCALA_TEXTO_GRANDE : 1.0f;
}

bool TextoGrande(void)
{
    return g_escalaTexto > 1.0f;
}

// Espaço entre linhas (ou altura de uma linha) que acompanha o tamanho do texto
int LinhaTexto(int espaco)
{
    return (int)((float)espaco * g_escalaTexto + 0.5f);
}

// Tamanho real na tela: textos pequenos (<= 12) ganham 10%, nada fica abaixo do mínimo
// combinado e, no modo GRANDE, tudo cresce (menos os títulos enormes, que já se leem bem)
static float TamanhoFonteJogo(int tam)
{
    float t = (tam <= 12) ? tam * 1.1f : (float)tam;

    if (t < TAMANHO_MINIMO_TEXTO)
        t = TAMANHO_MINIMO_TEXTO;

    if (tam < TAMANHO_TITULO_FIXO)
        t *= g_escalaTexto;

    return t;
}

// Quanto o texto deste tamanho cresceu em relação ao desenho original (piso + texto grande)
static float FatorTamanhoTexto(int tam)
{
    float original = (tam <= 12) ? tam * 1.1f : (float)tam;
    return (original > 0.0f) ? TamanhoFonteJogo(tam) / original : 1.0f;
}

// Espaço entre linhas de um texto deste tamanho, crescendo junto com ele (as linhas não se encostam)
int EspacoLinhaTexto(int espaco, int tam)
{
    return (int)((float)espaco * FatorTamanhoTexto(tam) + 0.5f);
}

// Cor com contraste suficiente no fundo escuro das salas:
//   - cinza neutro médio apagado (GRAY e parecidos) clareia até um mínimo
//   - texto meio transparente fica mais opaco (a curva mantém o fade-in e o fade-out funcionando)
static Color CorLegivel(Color c)
{
    int maior = (c.r > c.g) ? ((c.r > c.b) ? c.r : c.b) : ((c.g > c.b) ? c.g : c.b);
    int menor = (c.r < c.g) ? ((c.r < c.b) ? c.r : c.b) : ((c.g < c.b) ? c.g : c.b);

    // (cinza escuro, abaixo de 100, fica como está: ele é usado sobre fundo claro, como o painel do narrador)
    if (maior - menor <= 24 && maior >= 100 && maior < CINZA_MINIMO_TEXTO) {
        int soma = CINZA_MINIMO_TEXTO - maior;
        c.r = (unsigned char)(c.r + soma);
        c.g = (unsigned char)(c.g + soma);
        c.b = (unsigned char)(c.b + soma);
    }

    if (c.a < 255) {
        float a = (float)c.a / 255.0f;
        a = 1.0f - (1.0f - a) * (1.0f - a);
        c.a = (unsigned char)(a * 255.0f);
    }

    return c;
}

// Texto claro ganha uma sombra escura embaixo: continua legível sobre qualquer chão ou efeito
static bool PrecisaSombra(Color c)
{
    return (0.299f * c.r + 0.587f * c.g + 0.114f * c.b) > 110.0f && c.a > 20;
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
    float t = TamanhoFonteJogo(tam);

    cor = CorLegivel(cor);

    if (!g_fonteJogoOk) {
        if (PrecisaSombra(cor))
            DrawText(texto, x + 1, y + 1, (int)t, Fade(BLACK, 0.75f * cor.a / 255.0f));
        DrawText(texto, x, y, (int)t, cor);
        return;
    }

    Font fonte = FonteJogoPara(t);

    if (PrecisaSombra(cor)) {
        float d = (t >= 24.0f) ? 2.0f : 1.0f;
        DrawTextEx(fonte, texto, (Vector2){ (float)x + d, (float)y + d }, t, t * 0.03f, Fade(BLACK, 0.75f * cor.a / 255.0f));
    }

    DrawTextEx(fonte, texto, (Vector2){ (float)x, (float)y }, t, t * 0.03f, cor);
}

int MedirTextoJogo(const char *texto, int tam)
{
    float t = TamanhoFonteJogo(tam);

    if (!g_fonteJogoOk)
        return MeasureText(texto, (int)t);

    return (int)MeasureTextEx(FonteJogoPara(t), texto, t, t * 0.03f).x;
}
