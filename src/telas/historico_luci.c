#include <string.h>
#include <stdio.h>
#include <math.h>
#include "raylib.h"
#include "raymath.h"
#include "../../include/core/fonte.h"
#include "../../include/core/texto.h"
#include "../../include/telas/historico_luci.h"
#include "../../include/bloco1/minigame_visual.h"
#include "../../include/entidades/jogador.h"

// =============================================================
// HISTÓRICO DAS FALAS DA LUCI (issue #30)
// =============================================================

static char g_historico[MAX_HISTORICO_LUCI][TAM_FALA_HISTORICO];
static int g_inicioHistorico = 0;       // posição da fala mais antiga no anel
static int g_totalHistorico = 0;

// Painel e área da lista (a lista rola dentro dela)
static const Rectangle PAINEL_HISTORICO = { 50, 36, 700, 528 };
static const Rectangle LISTA_HISTORICO  = { 66, 104, 668, 404 };
static const Rectangle FECHAR_HISTORICO = { 50 + 700 - 40, 36 + 12, 26, 22 };

#define FONTE_HISTORICO      16
#define LINHA_HISTORICO      21
#define ESPACO_HISTORICO     12
#define LARGURA_HISTORICO    590

// i = 0 é a fala mais antiga guardada
static const char *FalaHistorico(int i)
{
    return g_historico[(g_inicioHistorico + i) % MAX_HISTORICO_LUCI];
}

// Guarda uma fala da Luci. Tira o "Luci:" (a tela já diz quem fala), junta as linhas e
// não repete a mesma fala duas vezes seguidas (voltar uma página ou um recado repetido).
void HistoricoLuciAdicionar(const char *texto)
{
    if (texto == NULL)
        return;

    if (TextoDeFalaDaLuci(texto)) {
        texto = strchr(texto, ':') + 1;
        while (*texto == ' ') texto++;
    }

    char fala[TAM_FALA_HISTORICO];
    snprintf(fala, sizeof(fala), "%s", texto);

    for (char *p = fala; *p != '\0'; p++)
        if (*p == '\n') *p = ' ';

    if (fala[0] == '\0')
        return;

    // Já está entre as últimas (o jogador voltou e avançou de novo, ou ela repetiu o recado): não duplica
    for (int i = g_totalHistorico - 1; i >= 0 && i >= g_totalHistorico - 6; i--)
        if (strcmp(FalaHistorico(i), fala) == 0)
            return;

    if (g_totalHistorico < MAX_HISTORICO_LUCI) {
        strcpy(g_historico[(g_inicioHistorico + g_totalHistorico) % MAX_HISTORICO_LUCI], fala);
        g_totalHistorico++;
    } else {
        strcpy(g_historico[g_inicioHistorico], fala);       // cheio: a mais antiga dá lugar à nova
        g_inicioHistorico = (g_inicioHistorico + 1) % MAX_HISTORICO_LUCI;
    }
}

void HistoricoLuciLimpar(void)
{
    g_inicioHistorico = 0;
    g_totalHistorico = 0;
}

int HistoricoLuciTotal(void)
{
    return g_totalHistorico;
}

// Altura de uma fala na lista (texto quebrado + espaço até a próxima)
static int AlturaFalaHistorico(int i, char *quebrado, int tam)
{
    QuebrarTexto(FalaHistorico(i), LARGURA_HISTORICO, FONTE_HISTORICO, quebrado, tam);
    return ContarLinhas(quebrado) * LINHA_HISTORICO + ESPACO_HISTORICO;
}

static float RolagemMaximaHistorico(void)
{
    char quebrado[TAM_FALA_HISTORICO + 64];
    int total = 0;

    for (int i = 0; i < g_totalHistorico; i++)
        total += AlturaFalaHistorico(i, quebrado, (int)sizeof(quebrado));

    return fmaxf(0.0f, (float)total - LISTA_HISTORICO.height);
}

// W / S, setas, rodinha do mouse: rola (rolagem = quanto subiu a partir das falas mais novas)
bool AtualizarHistoricoLuci(float *rolagem)
{
    float passo = 0.0f;

    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))   passo += 360.0f * GetFrameTime();
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) passo -= 360.0f * GetFrameTime();
    passo += GetMouseWheelMove() * 42.0f;

    *rolagem = Clamp(*rolagem + passo, 0.0f, RolagemMaximaHistorico());

    return IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) ||
           IsKeyPressed(TECLA_HISTORICO_LUCI) || (CliqueMouse() && MouseEm(FECHAR_HISTORICO));
}

void DesenharHistoricoLuci(Texture2D fundo, float rolagem, bool direto)
{
    Color fundoEscuro = { 8, 6, 18, 255 };
    Color painelCor   = { 18, 15, 30, 255 };
    Color ciano       = { 0, 228, 255, 255 };

    ClearBackground(fundoEscuro);

    if (fundo.id > 0)
        DrawTexturePro(fundo, (Rectangle){ 0, 0, (float)fundo.width, (float)fundo.height },
                       (Rectangle){ 0, 0, LARGURA_TELA, ALTURA_TELA }, (Vector2){ 0, 0 }, 0.0f, WHITE);

    DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(fundoEscuro, 0.55f));

    Rectangle p = PAINEL_HISTORICO;
    DrawRectangleRec(p, Fade(painelCor, 0.96f));
    DrawRectangleLinesEx(p, 4, PURPLE);
    DrawRectangleLinesEx((Rectangle){ p.x + 8, p.y + 8, p.width - 16, p.height - 16 }, 1, Fade(SKYBLUE, 0.35f));

    DesenharLuciTamanho((Vector2){ p.x + 40, p.y + 38 + sinf((float)GetTime() * 2.0f) * 2.0f }, 36.0f);
    DrawText("FALAS DA LUCI", (int)p.x + 66, (int)p.y + 20, 28, ciano);
    DrawText("O que ela já disse, da mais antiga (em cima) para a mais recente (embaixo).",
             (int)p.x + 66, (int)p.y + 50, 12, LIGHTGRAY);
    DesenharBotaoFechar(FECHAR_HISTORICO, PURPLE);

    Rectangle lista = LISTA_HISTORICO;
    DrawRectangleRec(lista, Fade(BLACK, 0.45f));

    if (g_totalHistorico == 0) {
        const char *vazio = "A Luci ainda não disse nada nesta partida.";
        DrawText(vazio, LARGURA_TELA / 2 - MeasureText(vazio, 16) / 2, (int)(lista.y + lista.height / 2) - 8, 16, GRAY);
    } else {

        // Altura total: a lista "encosta" embaixo (as falas novas ficam à vista, como num chat)
        char quebrado[TAM_FALA_HISTORICO + 64];
        int total = 0;
        for (int i = 0; i < g_totalHistorico; i++)
            total += AlturaFalaHistorico(i, quebrado, (int)sizeof(quebrado));

        float y = lista.y + lista.height - (float)total + rolagem;
        if (total < (int)lista.height) y = lista.y + 6.0f;          // poucas falas: começam no topo

        for (int i = 0; i < g_totalHistorico; i++) {

            int altura = AlturaFalaHistorico(i, quebrado, (int)sizeof(quebrado));
            bool recente = (i == g_totalHistorico - 1);
            Color cor = recente ? WHITE : (Color){ 200, 200, 214, 255 };

            // A mais recente ganha um fundo (é a que o jogador provavelmente quer reler)
            if (recente) {
                float topo = fmaxf(y - 4.0f, lista.y);
                float base = fminf(y + (float)altura - ESPACO_HISTORICO + 4.0f, lista.y + lista.height);
                if (base > topo) DrawRectangleRec((Rectangle){ lista.x + 4, topo, lista.width - 16, base - topo }, Fade(ciano, 0.12f));
            }

            // Cada linha só aparece se couber inteira na área da lista
            int linha = 0;
            const char *ini = quebrado;
            for (const char *c = quebrado; ; c++) {
                if (*c != '\n' && *c != '\0') continue;
                float yl = y + (float)(linha * LINHA_HISTORICO);
                if (yl >= lista.y && yl + LINHA_HISTORICO <= lista.y + lista.height) {
                    char txt[TAM_FALA_HISTORICO + 64];
                    snprintf(txt, sizeof(txt), "%.*s", (int)(c - ini), ini);
                    DrawText(txt, (int)lista.x + 60, (int)yl, FONTE_HISTORICO, cor);
                    if (linha == 0)
                        DrawText("LUCI", (int)lista.x + 12, (int)yl + 3, 11, recente ? GOLD : Fade(ciano, 0.6f));
                }
                linha++;
                if (*c == '\0') break;
                ini = c + 1;
            }

            y += (float)altura;
        }

        // Barra de rolagem
        float maximo = fmaxf(0.0f, (float)total - lista.height);
        if (maximo > 0.0f) {
            float trilho = lista.height - 8.0f;
            float tamBarra = fmaxf(30.0f, trilho * lista.height / (float)total);
            float k = 1.0f - rolagem / maximo;                    // 1 = embaixo (falas novas)
            DrawRectangle((int)(lista.x + lista.width) - 8, (int)lista.y + 4, 4, (int)trilho, Fade(GRAY, 0.3f));
            DrawRectangle((int)(lista.x + lista.width) - 8, (int)(lista.y + 4 + (trilho - tamBarra) * k), 4, (int)tamBarra, Fade(SKYBLUE, 0.8f));
        }
    }

    const char *dica1 = "W/S, SETAS ou RODINHA: rolar";
    const char *dica2 = direto ? "R, BACKSPACE, ENTER ou ESC: voltar ao jogo"
                               : "R, BACKSPACE ou ENTER: voltar ao menu de pause    ESC: voltar ao jogo";
    DrawText(dica1, LARGURA_TELA / 2 - MeasureText(dica1, 11) / 2, (int)(p.y + p.height) - 46, 11, Fade(SKYBLUE, 0.7f));
    DrawText(dica2, LARGURA_TELA / 2 - MeasureText(dica2, 11) / 2, (int)(p.y + p.height) - 30, 11, GRAY);
}
