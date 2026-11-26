#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <time.h>
#include "../../include/core/fonte.h"
#include "../../include/core/mapas.h"
#include "../../include/bloco1/batalha.h"
#include "../../include/audio/audio.h"
#include "../../include/bloco1/corredor.h"
#include "../../include/bloco1/minigame_visual.h"
#include "../../include/core/save.h"
#include "../../include/core/telemetria.h"
#include "../../include/core/texto.h"
#include "../../include/core/preferencias.h"
#include "../../include/entidades/jogador.h"
#include "../../include/entidades/luci_companion.h"
#include "../../include/telas/ranking.h"

// =============================================================
// BATALHA ESTILO UNDERTALE
// =============================================================

static void BatalhaTexto(Batalha *b, const char *texto)
{
    QuebrarTexto(texto, 640, FONTE_BATALHA, b->texto, (int)sizeof(b->texto));
    b->visiveis = 0.0f;
}

void BatalhaIniciar(Batalha *b, TipoBatalha tipo, int quiz, int nivelAlucinacao, int hpInicial)
{
    TocarGrupo(&gChefeInvoca, 0.8f, 0.6f);          // a alucinação se materializando
    memset(b, 0, sizeof(*b));

    b->tipo = tipo;
    b->quiz = quiz;
    b->fase = FASE_INTRO;
    b->hp = hpInicial;

    // README: quanto mais o jogador erra, mais forte a alucinação fica
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
        b->padrao = quiz % 3;   // cada monitor tem um padrão de ataque

        BatalhaTexto(b, TextFormat(
            "* Resposta errada! Um fragmento de alucinação escapou do monitor %d.\n"
            "* Desvie dos dados corrompidos!", quiz + 1));
    }
}

static void BatalhaCriarBala(Batalha *b, Rectangle c)
{
    static const char glifos[] = "01?!#%";

    int livre = -1;

    for (int i = 0; i < MAX_BALAS; i++) {
        if (!b->balas[i].ativa) {
            livre = i;
            break;
        }
    }

    if (livre < 0)
        return;

    Bala *bl = &b->balas[livre];
    float v = b->velocidadeBala * (0.85f + GetRandomValue(0, 30) / 100.0f);

    int xMin = (int)c.x + 12, xMax = (int)(c.x + c.width) - 12;
    int yMin = (int)c.y + 12, yMax = (int)(c.y + c.height) - 12;

    bl->ativa = true;
    bl->glifo = glifos[GetRandomValue(0, (int)sizeof(glifos) - 2)];

    switch (b->padrao) {

        // CHUVA: dados caindo de cima
        case 0: {
            bl->pos = (Vector2){ (float)GetRandomValue(xMin, xMax), c.y - 12 };
            bl->vel = (Vector2){ 0, v };
        } break;

        // LATERAL: dados atravessando de um lado para o outro
        case 1: {
            bool daEsquerda = (GetRandomValue(0, 1) == 0);
            bl->pos = (Vector2){ daEsquerda ? c.x - 12 : c.x + c.width + 12, (float)GetRandomValue(yMin, yMax) };
            bl->vel = (Vector2){ daEsquerda ? v : -v, 0 };
        } break;

        // MIRA: dados vindo das bordas em direção ao coração
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

// Atualiza a batalha. Devolve true quando o jogador fecha o texto final.
//   ENTER / E / ESPAÇO / Z : avança o texto
//   W A S D / SETAS        : move o coração
bool BatalhaAtualizar(Batalha *b)
{
    float dt = DtJogo();

    if (b->tremor > 0.0f) b->tremor -= dt;
    if (b->fase != FASE_DESVIO) b->tremor = 0.0f;
    if (b->flashDano > 0.0f) b->flashDano -= dt;

    bool avancar = IsKeyPressed(KEY_ENTER) ||
                   IsKeyPressed(KEY_E) ||
                   IsKeyPressed(KEY_SPACE) ||
                   IsKeyPressed(KEY_Z) ||
                   CliqueMouse();                   // o clique vale como ENTER

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
                else         { b->fase = FASE_ENCOLHENDO; TocarGrupo(&gB1Caixa, 1.0f, 0.7f); }
            }

        } break;

        case FASE_ENCOLHENDO: {

            b->anim += dt / DURACAO_ANIM_CAIXA;

            if (b->anim >= 1.0f) {

                b->anim = 1.0f;
                b->fase = FASE_DESVIO;
                b->preparo = PREPARO_MINIGAME_LONGO;
                b->alma = (Vector2){ CAIXA_DESVIO.x + CAIXA_DESVIO.width / 2, CAIXA_DESVIO.y + CAIXA_DESVIO.height / 2 };
            }

        } break;

        case FASE_DESVIO: {

            Rectangle c = CAIXA_DESVIO;

            bool preparando = (b->preparo > 0.0f);        // a regra na tela: as balas ainda não vêm
            if (preparando) b->preparo -= dt;
            else            b->tempoDesvio += dt;

            if (b->invencivel > 0.0f) b->invencivel -= dt;

            // ---------- Movimento do coração ----------
            Vector2 dir = {0};

            if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    dir.y -= 1.0f;
            if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  dir.y += 1.0f;
            if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  dir.x -= 1.0f;
            if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) dir.x += 1.0f;

            if (Vector2Length(dir) > 0.0f)
                b->alma = Vector2Add(b->alma, Vector2Scale(Vector2Normalize(dir), VELOCIDADE_ALMA * dt));

            float margem = 4.0f + 8.0f;   // borda da caixa + metade do coração

            b->alma.x = Clamp(b->alma.x, c.x + margem, c.x + c.width - margem);
            b->alma.y = Clamp(b->alma.y, c.y + margem, c.y + c.height - margem);

            // ---------- Ataques ----------
            // No COMBATE os padrões se alternam: chuva -> lateral -> mira
            if (b->tipo == BAT_COMBATE)
                b->padrao = ((int)(b->tempoDesvio / (b->duracao / 3.0f))) % 3;

            if (!preparando) b->cronoBala -= dt;

            if (!preparando && b->cronoBala <= 0.0f) {

                b->cronoBala += b->intervaloBala;
                BatalhaCriarBala(b, c);
            }

            for (int i = 0; i < MAX_BALAS; i++) {

                Bala *bl = &b->balas[i];

                if (!bl->ativa)
                    continue;

                bl->pos = Vector2Add(bl->pos, Vector2Scale(bl->vel, dt));

                // Saiu da caixa: some
                if (bl->pos.x < c.x - 30 || bl->pos.x > c.x + c.width + 30 ||
                    bl->pos.y < c.y - 30 || bl->pos.y > c.y + c.height + 30) {

                    bl->ativa = false;
                    continue;
                }

                // Acertou o coração
                if (b->invencivel <= 0.0f &&
                    Vector2Distance(bl->pos, b->alma) < RAIO_BALA + RAIO_ALMA) {

                    b->hp -= b->dano;
                    b->invencivel = TEMPO_INVENCIVEL;
                    b->tremor = 0.2f;
                    b->flashDano = 0.3f;
                    b->danosRecebidos++;
                    bl->ativa = false;
                    TocarGrupo(&gB1DanoAlma, 1.0f, 0.8f);
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
                if (b->derrota) TocarGrupo(&gB1Sobrecarga, 1.0f, 0.85f);
                else            TocarGrupo(&gB1Caixa, 1.2f, 0.5f);
            }

        } break;

        case FASE_EXPANDINDO: {

            b->anim -= dt / DURACAO_ANIM_CAIXA;

            if (b->anim <= 0.0f) {

                b->anim = 0.0f;
                b->fase = FASE_FIM;
                if (!b->derrota) TocarGrupo(&gChefeEscudo, 1.25f, 0.55f);       // a alucinação se desfaz em ruído

                if (b->derrota)
                    BatalhaTexto(b,
                        "* A alucinação te dominou por um instante...\n"
                        "* Você não perde vidas, mas perde pontos e ela ficou mais forte.");
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

        default:
            break;
    }

    return false;
}

// ---------- Tutorial: teclas e mouse desenhados (o jogador reconhece na hora, sem precisar ler) ----------
float LarguraTeclaTutorial(const char *rotulo, float escala)
{
    return fmaxf(26.0f * escala, (float)MeasureText(rotulo, (int)(14.0f * escala)) + 16.0f * escala);
}

void DesenharTeclaTutorial(Vector2 c, const char *rotulo, float escala, bool apertada, float alfa)
{
    int tam = (int)(14.0f * escala);
    float w = LarguraTeclaTutorial(rotulo, escala), h = 24.0f * escala;
    float desce = apertada ? 3.0f * escala : 0.0f;
    Color escuro = Fade((Color){ 20, 18, 30, 255 }, alfa);
    Rectangle lado = { c.x - w / 2.0f, c.y - h / 2.0f + 4.0f * escala, w, h };
    Rectangle topo = { c.x - w / 2.0f, c.y - h / 2.0f + desce, w, h };
    DrawRectangleRounded(lado, 0.3f, 6, Fade((Color){ 70, 64, 96, 255 }, alfa));                      // a lateral da tecla
    DrawRectangleRounded(topo, 0.3f, 6, Fade(apertada ? GOLD : (Color){ 235, 235, 245, 255 }, alfa));
    DrawRectangleRoundedLines(topo, 0.3f, 6, escuro);
    DrawText(rotulo, (int)(c.x - (float)MeasureText(rotulo, tam) / 2.0f), (int)(topo.y + h / 2.0f - (float)tam / 2.0f), tam, escuro);
}

// Mouse com o botão da vez aceso (0 = esquerdo, 1 = direito)
void DesenharMouseTutorial(Vector2 c, int botao, float escala, bool apertado, float alfa)
{
    float w = 22.0f * escala, h = 32.0f * escala, corte = h * 0.42f;
    Color claro = Fade((Color){ 235, 235, 245, 255 }, alfa), escuro = Fade((Color){ 20, 18, 30, 255 }, alfa);
    Rectangle corpo = { c.x - w / 2.0f, c.y - h / 2.0f, w, h };
    DrawRectangleRounded(corpo, 0.7f, 8, claro);
    Rectangle bt = { (botao == 0) ? corpo.x + 2.0f : c.x, corpo.y + 2.0f, w / 2.0f - 2.0f, corte - 2.0f };
    DrawRectangleRounded(bt, 0.6f, 6, Fade(apertado ? GOLD : (Color){ 255, 190, 60, 255 }, alfa * (apertado ? 1.0f : 0.7f)));
    DrawRectangleRoundedLines(corpo, 0.7f, 8, escuro);
    DrawLineEx((Vector2){ c.x, corpo.y + 1.0f }, (Vector2){ c.x, corpo.y + corte }, 1.5f, escuro);
    DrawLineEx((Vector2){ corpo.x + 1.0f, corpo.y + corte }, (Vector2){ corpo.x + w - 1.0f, corpo.y + corte }, 1.5f, escuro);
}

// Seta pulando apontando para baixo (o "é aqui")
void DesenharSetaTutorial(Vector2 ponta, float tam, Color cor, float tempo)
{
    Vector2 p = { ponta.x, ponta.y + sinf(tempo * 5.0f) * 4.0f };
    Vector2 a = { p.x - tam, p.y - tam * 1.3f }, b = { p.x + tam, p.y - tam * 1.3f };
    DrawTriangle(a, p, b, Fade(BLACK, 0.6f));
    DrawTriangle(a, b, p, Fade(BLACK, 0.6f));
    Vector2 a2 = { a.x + 2.0f, a.y + 1.5f }, b2 = { b.x - 2.0f, b.y + 1.5f }, p2 = { p.x, p.y - 2.5f };
    DrawTriangle(a2, p2, b2, cor);
    DrawTriangle(a2, b2, p2, cor);
}

// Dica "na hora certa": a tecla (ou o mouse) e uma palavra. agora = dourado pulsando; senão, apagada ("ESPERE...")
void DesenharDicaAgora(Vector2 c, const char *tecla, bool mouse, bool agora, const char *texto, float tempo)
{
    float esc = agora ? 0.95f + 0.08f * sinf(tempo * 16.0f) : 0.85f;
    float alfa = agora ? 1.0f : 0.6f;
    bool aperta = agora && fmodf(tempo * 3.0f, 1.0f) < 0.45f;
    int tam = 14;
    float wIcone = mouse ? 22.0f * esc : LarguraTeclaTutorial(tecla, esc);
    float w = wIcone + 8.0f + (float)MeasureText(texto, tam);
    float x0 = c.x - w / 2.0f;
    Rectangle fundo = { x0 - 8.0f, c.y - 20.0f, w + 16.0f, 40.0f };
    DrawRectangleRounded(fundo, 0.35f, 6, Fade((Color){ 12, 10, 24, 255 }, 0.82f * alfa));
    DrawRectangleRoundedLines(fundo, 0.35f, 6, Fade(agora ? GOLD : GRAY, alfa));
    if (mouse) DesenharMouseTutorial((Vector2){ x0 + wIcone / 2.0f, c.y }, 0, esc * 0.85f, aperta, alfa);
    else       DesenharTeclaTutorial((Vector2){ x0 + wIcone / 2.0f, c.y - 2.0f }, tecla, esc, aperta, alfa);
    DrawText(texto, (int)(x0 + wIcone + 8.0f), (int)c.y - tam / 2, tam, Fade(agora ? GOLD : LIGHTGRAY, alfa));
}

// Contagem antes do minigame do Bloco 1 (a caixa mostra a regra com ícones e "3, 2, 1, JÁ!"; as balas esperam)
void DesenharPreparoMinigame(Rectangle caixa, float resta, float total, const char *titulo, Color corTitulo,
                                    int linhas, const int *icones, const char *const *textos, const Color *cores, bool ataque, Color corInimigo, float tempo)
{
    float k = Clamp(resta / total, 0.0f, 1.0f);
    float entra = Clamp((total - resta) / 0.15f, 0.0f, 1.0f);
    DrawRectangleRec((Rectangle){ caixa.x + 4, caixa.y + 4, caixa.width - 8, caixa.height - 8 }, Fade(BLACK, 0.78f * entra));
    int tt = 24, wt = MeasureText(titulo, tt);
    DrawText(titulo, (int)(caixa.x + caixa.width / 2) - wt / 2, (int)caixa.y + 12, tt, Fade(corTitulo, entra));
    float y = caixa.y + 52.0f;
    for (int i = 0; i < linhas; i++, y += 30.0f) {
        Vector2 ic = { caixa.x + 50.0f, y + 6.0f };
        if (icones[i] == 0)      DesenharItemB1(ic, ataque, entra, false);
        else if (icones[i] == 1) DesenharDadoCorrompido(ic, '?', corInimigo, 0);
        else if (icones[i] == 2) DesenharItemB1(ic, ataque, entra, true);
        else {                    // 3 = WASD
            float e = 0.55f;
            DesenharTeclaTutorial((Vector2){ ic.x, ic.y - 9.0f }, "W", e, false, entra);
            DesenharTeclaTutorial((Vector2){ ic.x - 16.0f, ic.y + 7.0f }, "A", e, false, entra);
            DesenharTeclaTutorial((Vector2){ ic.x, ic.y + 7.0f }, "S", e, false, entra);
            DesenharTeclaTutorial((Vector2){ ic.x + 16.0f, ic.y + 7.0f }, "D", e, false, entra);
        }
        int tam = 16;
        while (tam > 11 && MeasureText(textos[i], tam) > (int)caixa.width - 92) tam--;     // cabe na caixa
        DrawText(textos[i], (int)caixa.x + 80, (int)y + 6 - tam / 2, tam, Fade(cores[i], entra));
    }
    // a contagem: anel esvaziando no canto e o número grande; no fim, "JÁ!"
    Vector2 cc = { caixa.x + caixa.width - 34.0f, caixa.y + caixa.height - 34.0f };
    const char *num = (resta < total * 0.2f) ? "JÁ!" : TextFormat("%d", (int)ceilf((resta - total * 0.2f) / (total * 0.8f) * 3.0f));
    DrawRing(cc, 18.0f, 22.0f, -90.0f, -90.0f + 360.0f * k, 32, Fade(GOLD, entra));
    DrawText(num, (int)cc.x - MeasureText(num, 18) / 2, (int)cc.y - 9, 18, Fade(GOLD, entra));
    (void)tempo;
}

// Vida da Luci nas batalhas do Bloco 1: barra segmentada no estilo do HUD do jogo (sem o "LV/HP" de antes)
void DesenharVidaB1(const char *nickname, int hp, int hpMax, int y, float flashDano, float tempo)
{
    float k = (hpMax > 0) ? Clamp((float)hp / (float)hpMax, 0.0f, 1.0f) : 0.0f;
    Color cor = (k > 0.5f) ? (Color){ 70, 220, 110, 255 } : ((k > 0.25f) ? GOLD : RED);      // verde: não confunde com a Luci (ciano) nem com a estabilidade (azul)
    if (k <= 0.25f && sinf(tempo * 8.0f) > 0.0f) cor = Fade(cor, 0.6f);         // pouca vida: pisca
    int w = 200, h = 14;
    int bx = 60 + MeasureText(nickname, 20) + 18;
    if (bx < 170) bx = 170;
    DrawText(nickname, 60, y, 20, WHITE);
    DrawText("INTEGRIDADE DA LUCI", bx, y - 3, 10, Fade(cor, 0.9f));
    Rectangle fundo = { (float)bx, (float)y + 9, (float)w, (float)h };
    DrawRectangleRec(fundo, (Color){ 20, 16, 36, 255 });
    DrawRectangle(bx, y + 9, (int)((float)w * k), h, cor);
    DrawRectangle(bx, y + 9, (int)((float)w * k), 4, Fade(WHITE, 0.25f));                      // brilho em cima
    for (int s = 1; s < 10; s++) DrawRectangle(bx + w * s / 10, y + 9, 1, h, Fade(BLACK, 0.5f));
    DrawRectangleLinesEx(fundo, 1, (flashDano > 0.0f) ? RED : Fade(cor, 0.8f));
    DrawText(TextFormat("%d/%d", hp, hpMax), bx + w + 10, y + 8, 14, WHITE);
}

// Golpe na Luci: as bordas da tela ficam vermelhas por um instante (o padrão do jogo; a tela não pisca inteira)
void VinhetaDanoB1(float flashDano)
{
    if (flashDano <= 0.0f) return;
    Color c = Fade(RED, 0.35f * Clamp(flashDano / 0.3f, 0.0f, 1.0f));
    DrawRectangleGradientV(0, 0, LARGURA_TELA, 80, c, Fade(RED, 0.0f));
    DrawRectangleGradientV(0, ALTURA_TELA - 80, LARGURA_TELA, 80, Fade(RED, 0.0f), c);
    DrawRectangleGradientH(0, 0, 80, ALTURA_TELA, c, Fade(RED, 0.0f));
    DrawRectangleGradientH(LARGURA_TELA - 80, 0, 80, ALTURA_TELA, Fade(RED, 0.0f), c);
}

// Inimigo: um fantasma glitchado (placeholder até a arte ficar pronta)
void DesenharAlucinacao(Vector2 centro, float tempo, bool forte)
{
    float s = forte ? 1.25f : 0.9f;
    Vector2 p = { centro.x, centro.y + sinf(tempo * 2.0f) * 6.0f };

    // Três camadas deslocadas = efeito de "aberração cromática"
    Color cores[3] = { Fade(RED, 0.45f), Fade(SKYBLUE, 0.45f), (Color){ 200, 170, 255, 255 } };
    float desloc[3] = { -4.0f, 4.0f, 0.0f };

    for (int k = 0; k < 3; k++) {

        float cx = p.x + desloc[k] * (forte ? 1.5f : 1.0f);

        DrawCircle((int)cx, (int)(p.y - 10 * s), 42 * s, cores[k]);
        DrawRectangle((int)(cx - 42 * s), (int)(p.y - 10 * s), (int)(84 * s), (int)(50 * s), cores[k]);

        // Barra ondulada embaixo (parada: a alucinação só flutua, sem tremer)
        for (int j = 0; j < 5; j++)
            DrawCircle((int)(cx - 34 * s + j * 17 * s), (int)(p.y + 40 * s + ((j % 2) ? 2.0f : -2.0f)), 9 * s, cores[k]);
    }

    // Olhos
    DrawCircle((int)(p.x - 15 * s), (int)(p.y - 14 * s), 7 * s, BLACK);
    DrawCircle((int)(p.x + 15 * s), (int)(p.y - 14 * s), 7 * s, BLACK);

    if (forte) {
        DrawCircle((int)(p.x - 15 * s), (int)(p.y - 14 * s), 2.5f * s, RED);
        DrawCircle((int)(p.x + 15 * s), (int)(p.y - 14 * s), 2.5f * s, RED);
    }

    // Boca em zigue-zague
    for (int j = 0; j < 5; j++) {

        Vector2 a = { p.x - 18 * s + j * 7.2f * s,       p.y + 8 * s + (j % 2) * 6 * s };
        Vector2 b = { p.x - 18 * s + (j + 1) * 7.2f * s, p.y + 8 * s + ((j + 1) % 2) * 6 * s };

        DrawLineEx(a, b, 2, BLACK);
    }

    // Símbolos orbitando
    const char *simbolos[4] = { "?", "404", "01", "!" };

    for (int j = 0; j < 4; j++) {

        float ang = tempo * 1.2f + j * (PI / 2.0f);
        int sx = (int)(p.x + cosf(ang) * 85 * s);
        int sy = (int)(p.y + sinf(ang) * 30 * s);

        DrawText(simbolos[j], sx - MeasureText(simbolos[j], 14) / 2, sy - 7, 14, Fade(WHITE, 0.6f));
    }
}

void BatalhaDesenhar(const Batalha *b, const char *nickname, float tempo)
{
    ClearBackground(BLACK);

    bool forte = (b->tipo == BAT_COMBATE);

    // ---------- Inimigo ----------
    const char *nome = forte ? "ALUCINAÇÃO" : "FRAGMENTO DE ALUCINAÇÃO";

    DrawText(forte ? "COMBATE" : "MINIGAME", 20, 16, 10, forte ? RED : ORANGE);
    DrawText(nome, LARGURA_TELA / 2 - MeasureText(nome, 14) / 2, 34, 14, GRAY);

    DesenharAlucinacao((Vector2){ LARGURA_TELA / 2.0f, 150.0f }, tempo, forte);

    // ---------- Caixa (encolhe/expande com suavização) ----------
    float t = b->anim * b->anim * (3.0f - 2.0f * b->anim);

    Rectangle caixa = {
        Lerp(CAIXA_TEXTO_BATALHA.x, CAIXA_DESVIO.x, t),
        Lerp(CAIXA_TEXTO_BATALHA.y, CAIXA_DESVIO.y, t),
        Lerp(CAIXA_TEXTO_BATALHA.width, CAIXA_DESVIO.width, t),
        Lerp(CAIXA_TEXTO_BATALHA.height, CAIXA_DESVIO.height, t)
    };

    if (b->tremor > 0.0f && Prefs()->tremorTela) {      // (o tremor pode ser desligado na tela de Opções)
        caixa.x += (float)GetRandomValue(-3, 3);
        caixa.y += (float)GetRandomValue(-3, 3);
    }

    DrawRectangleRec(caixa, BLACK);
    DrawRectangleLinesEx(caixa, 4, (b->flashDano > 0.0f) ? RED : WHITE);

    // ---------- Texto (intro e final) ----------
    if (b->fase == FASE_INTRO || b->fase == FASE_FIM) {

        char parcial[512];
        bool terminou = TextoParcial(b->texto, b->visiveis, parcial, (int)sizeof(parcial));

        DesenharTextoLinhas(parcial, (int)caixa.x + 24, (int)caixa.y + 22, FONTE_BATALHA, 30, WHITE);

        if (terminou && (((int)(GetTime() * 2.0)) % 2 == 0))
            DesenharDicaAvancar((int)(caixa.x + caixa.width - 90), (int)(caixa.y + caixa.height - 26), 12);
    }

    // ---------- Esquiva ----------
    if (b->fase == FASE_DESVIO) {

        BeginScissorMode((int)caixa.x + 4, (int)caixa.y + 4, (int)caixa.width - 8, (int)caixa.height - 8);

        for (int i = 0; i < MAX_BALAS; i++) {

            if (!b->balas[i].ativa)
                continue;

            DesenharDadoCorrompido(b->balas[i].pos, b->balas[i].glifo, (Color){ 200, 170, 255, 255 }, i);
        }

        // A Luci pisca enquanto está invencível
        if (b->invencivel <= 0.0f || (((int)(b->invencivel * 10.0f)) % 2 == 0))
            DesenharAlmaLuci(b->alma, NULL, 0);

        EndScissorMode();

        if (b->preparo > 0.0f) {
            static const int icones[2] = { 1, 3 };
            static const char *const textos[2] = { "NÃO ENCOSTE", "MOVA A LUCI" };
            const Color cores[2] = { (Color){ 200, 170, 255, 255 }, (Color){ 0, 228, 255, 255 } };
            DesenharPreparoMinigame(caixa, b->preparo, PREPARO_MINIGAME_LONGO, "DESVIE!", RED, 2, icones, textos, cores, false,
                                    (Color){ 200, 170, 255, 255 }, tempo);
        }

        // Quanto falta para a alucinação perder a força
        float resta = 1.0f - b->tempoDesvio / b->duracao;

        if (resta < 0.0f)
            resta = 0.0f;

        DrawRectangle((int)caixa.x, (int)(caixa.y + caixa.height + 6), (int)(caixa.width * resta), 3, Fade(WHITE, 0.5f));
    }

    // ---------- Vida da Luci ----------
    DesenharVidaB1(nickname, b->hp, HP_MAX_BATALHA, 452, b->flashDano, tempo);
    VinhetaDanoB1(b->flashDano);

    // ---------- Instruções ----------
    const char *instrucao = (b->fase == FASE_DESVIO)
        ? "W A S D ou SETAS  -  guie a Luci e desvie!"
        : "ENTER ou CLIQUE  -  continuar";

    DrawText(instrucao, LARGURA_TELA / 2 - MeasureText(instrucao, 12) / 2, 540, 12, GRAY);
}
