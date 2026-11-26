#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <time.h>
#include "../../include/core/fonte.h"
#include "../../include/core/mapas.h"
#include "../../include/bloco1/objetivo.h"
#include "../../include/audio/audio.h"
#include "../../include/bloco1/batalha.h"
#include "../../include/bloco1/batalha_monitores.h"
#include "../../include/bloco1/corredor.h"
#include "../../include/bloco1/minigame_visual.h"
#include "../../include/core/save.h"
#include "../../include/core/telemetria.h"
#include "../../include/core/texto.h"
#include "../../include/dados/conteudo_bloco1.h"
#include "../../include/entidades/jogador.h"
#include "../../include/entidades/luci_companion.h"
#include "../../include/telas/ranking.h"


Vector2 CentroRetangulo(Rectangle r)
{
    return (Vector2){ r.x + r.width / 2, r.y + r.height / 2 };
}

Objetivo ObjetivoAtual(TelaAtual tela, const ProgressoCorredor *pc,
                              bool luciAcompanhando, Rectangle luciHitbox, bool chefeVencido)
{
    Objetivo o = { "", false, { 0, 0 } };

    #define OBJETIVO(txt, x, y) do { snprintf(o.texto, sizeof(o.texto), "%s", (txt)); \
                                       o.temAlvo = true; o.alvo = (Vector2){ (x), (y) }; } while (0)

    if (tela == JOGANDO) {

        if (!luciAcompanhando) OBJETIVO("Fale com a Luci", luciHitbox.x + luciHitbox.width / 2, luciHitbox.y + luciHitbox.height / 2);
        else                   OBJETIVO("Vá até a porta do CORREDOR", CentroRetangulo(PORTA_SALA).x, CentroRetangulo(PORTA_SALA).y);
    }
    else if (tela == CORREDOR) {

        int proximo = ProximoMonitor(pc);

        if (proximo >= 0) {
            Rectangle a = monitores[proximo].area;
            OBJETIVO(TextFormat("Enfrente a alucinação do MONITOR %d", proximo + 1), a.x + a.width / 2, a.y + a.height / 2);
        } else if (!chefeVencido) {
            OBJETIVO("Enfrente a ALUCINAÇÃO MESTRE na porta da direita", CentroRetangulo(PORTA_FIM_CORREDOR).x, CentroRetangulo(PORTA_FIM_CORREDOR).y);
        } else {
            OBJETIVO("Siga pela porta da direita", CentroRetangulo(PORTA_FIM_CORREDOR).x, CentroRetangulo(PORTA_FIM_CORREDOR).y);
        }
    }
    #undef OBJETIVO
    return o;
}

// ---------- A voz da Luci no HUD ----------
// O objetivo e os recados aparecem num balão de fala com o retrato da Luci (ela mexe a boca
// enquanto o texto "digita" e toca um bip): o jogador percebe na hora que é ELA quem está orientando.

// Tira o "Luci:" do começo (a etiqueta do balão já diz quem fala)
static const char *FalaSemPrefixoLuci(const char *t)
{
    if (TextoDeFalaDaLuci(t)) {
        while (*t != ':') t++;
        t++;
        while (*t == ' ') t++;
    }
    return t;
}

// Começa com o nome dela, mas não é "Luci:" ("LUCI +1: reparo"): não repete a etiqueta
static bool FalaComecaComLuci(const char *t)
{
    return (t[0] == 'L' || t[0] == 'l') && (t[1] == 'U' || t[1] == 'u') && (t[2] == 'C' || t[2] == 'c') && (t[3] == 'I' || t[3] == 'i');
}

// Identifica o texto ignorando os números (um contador "1/3" que sobe não reinicia a digitação)
static unsigned int AssinaturaFala(const char *t)
{
    unsigned int h = 2166136261u;
    for (; *t != '\0'; t++) {
        if (*t >= '0' && *t <= '9') continue;
        h = (h ^ (unsigned char)*t) * 16777619u;
    }
    return h;
}

// Posição (em bytes) depois de N letras (o texto é UTF-8: não corta no meio de um acento)
static int BytesAteLetras(const char *t, int letras)
{
    int i = 0, n = 0;
    while (t[i] != '\0') {
        if (((unsigned char)t[i] & 0xC0) != 0x80) {
            if (n == letras) break;
            n++;
        }
        i++;
    }
    return i;
}

// sistema = a orientação NÃO vem dela (na luta contra a Luci ela é a chefe): balão do SISTEMA, sem o retrato
void DesenharFalaLuciHUD(const char *bruto, bool recado, bool sistema)
{
    static unsigned int assinatura = 0;
    static float tempoFala = 0.0f;

    if (bruto == NULL || bruto[0] == '\0')
        return;

    bool nomeNoTexto = !sistema && FalaComecaComLuci(bruto) && !TextoDeFalaDaLuci(bruto);
    const char *texto = FalaSemPrefixoLuci(bruto);
    unsigned int ass = AssinaturaFala(texto) ^ (recado ? 0x9E3779B9u : 0u) ^ (sistema ? 0x51ED270Bu : 0u);

    if (!sistema && !recado && GetTime() < g_repetirFalaLuciAte) {     // E sem nada por perto: ela repete a dica
        g_repetirFalaLuciAte = 0.0;
        assinatura = 0;
    }

    if (ass != assinatura) {            // fala nova: o balão aparece, ela começa a falar e o bip toca
        assinatura = ass;
        tempoFala = 0.0f;
        TocarSomTom(sfxSelecao, sistema ? 0.8f : (recado ? 1.8f : 1.4f));
    }
    tempoFala += DtJogo();

    int corte = BytesAteLetras(texto, (int)(tempoFala * 90.0f));
    bool digitando = texto[corte] != '\0';

    Color ciano = sistema ? (Color){ 255, 60, 170, 255 } : (Color){ 0, 228, 255, 255 };
    const char *etiqueta = sistema ? "SISTEMA:" : "LUCI:";
    int tam = 16;
    int wEtiqueta = nomeNoTexto ? 0 : MeasureText(etiqueta, 12) + 8;
    int wTexto = MeasureText(texto, tam);
    if (wEtiqueta + wTexto > 560) { tam = 13; wTexto = MeasureText(texto, tam); }

    int largura = wEtiqueta + wTexto + 24;
    int x0 = (LARGURA_TELA - (46 + largura)) / 2;
    float k = Clamp(tempoFala / 0.18f, 0.0f, 1.0f);
    int y = 9 - (int)(8.0f * (1.0f - k));
    float tempo = (float)GetTime();
    Rectangle balao = { (float)(x0 + 46), (float)y, (float)largura, 28 };
    Color fundo = Fade(sistema ? (Color){ 30, 8, 30, 255 } : (Color){ 10, 24, 48, 255 }, 0.9f * k);
    Color borda = recado ? Fade(ciano, k * (0.75f + 0.25f * sinf(tempo * 6.0f))) : Fade(SKYBLUE, 0.8f * k);

    DrawRectangleRec(balao, fundo);
    DrawRectangleLinesEx(balao, 2, borda);
    DrawTriangle((Vector2){ balao.x + 1, balao.y + 8 }, (Vector2){ balao.x - 9, balao.y + 14 }, (Vector2){ balao.x + 1, balao.y + 20 }, fundo);
    DrawLineEx((Vector2){ balao.x, balao.y + 8 }, (Vector2){ balao.x - 9, balao.y + 14 }, 2, borda);
    DrawLineEx((Vector2){ balao.x - 9, balao.y + 14 }, (Vector2){ balao.x, balao.y + 20 }, 2, borda);

    if (sistema) {
        // Sem retrato: um terminalzinho com o cursor piscando
        Rectangle term = { (float)x0 + 5.0f, 9.0f, 34.0f, 26.0f };
        DrawRectangleRec(term, fundo);
        DrawRectangleLinesEx(term, 2, borda);
        DrawText(((int)(tempo * 3.0f) % 2 == 0) ? ">_" : ">", (int)term.x + 7, (int)term.y + 6, 14, Fade(ciano, k));
    } else {
        // O retrato dela (mexe a boca enquanto o texto digita)
        bool falandoAntes = luciFalando;
        float tempoAntes = tempoFalaLuci;
        luciFalando = digitando;
        tempoFalaLuci = tempoFala;
        DesenharLuciTamanho((Vector2){ (float)x0 + 20.0f, 22.0f + sinf(tempo * 2.5f) * 2.0f }, 38.0f);
        luciFalando = falandoAntes;
        tempoFalaLuci = tempoAntes;
    }

    int tx = (int)balao.x + 12;
    if (!nomeNoTexto) {
        DrawText(etiqueta, tx, y + 8, 12, Fade(ciano, k));
        tx += wEtiqueta;
    }
    DrawText(TextSubtext(texto, 0, corte), tx, y + (28 - tam) / 2 - 1, tam, Fade(recado ? WHITE : GOLD, k));

    // Leu rápido demais? O "R" do lado do balão lembra que as falas dela ficam no histórico (issue #30)
    if (!sistema) {
        const char *rotulo = "rever";
        int wTecla = MeasureText("R", 10) + 10;
        int xr = (int)(balao.x + balao.width) + 8;
        if (xr + wTecla + 4 + MeasureText(rotulo, 10) < LARGURA_TELA - 4) {
            Rectangle tecla = { (float)xr, (float)y + 6, (float)wTecla, 16 };
            DrawRectangleRec(tecla, Fade((Color){ 40, 40, 60, 255 }, 0.85f * k));
            DrawRectangleLinesEx(tecla, 1, Fade(LIGHTGRAY, 0.8f * k));
            DrawText("R", xr + 5, y + 8, 10, Fade(WHITE, k));
            DrawText(rotulo, xr + wTecla + 4, y + 9, 10, Fade(SKYBLUE, 0.8f * k));
        }
    }
}

// Objetivo atual: dito pela Luci
void DesenharObjetivoHUD(const Objetivo *o)
{
    if (o->texto[0] == '\0')
        return;

    DesenharFalaLuciHUD(o->texto, false, false);
}

void DesenharGlitchAmbiente(int nivel, float tempo)
{
    if (nivel <= 0 || !MOSTRAR_GLITCH_AMBIENTE)
        return;

    int n = (nivel > 8) ? 8 : nivel;
    unsigned int quadro = (unsigned int)(tempo * 8.0f);

    for (int i = 0; i < n * 2; i++) {

        unsigned int r = Ruido(quadro * 13u + (unsigned int)i);

        if (r % 3 != 0)
            continue;

        int largura = 60 + (int)((r >> 8) % 300);
        int x = (int)((r >> 16) % (unsigned int)(LARGURA_TELA - largura));
        int y = 40 + (int)(r % 520);

        DrawRectangle(x, y, largura, 2 + (int)((r >> 4) % 3), Fade((i % 2) ? MAGENTA : SKYBLUE, 0.08f + 0.02f * n));
    }

    DrawRectangleLinesEx((Rectangle){ 0, 0, LARGURA_TELA, ALTURA_TELA }, 6.0f + n * 2, Fade((Color){ 70, 0, 50, 255 }, 0.05f * n));
}
