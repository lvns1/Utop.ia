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

static const LicaoMestre LICOES_MESTRE[MT_FIM + 1] = {
    { "PEGUE A ARMA",      "E",      false, "vá até o pedestal que brilha" },
    { "ATIRAR",            NULL,     true,  "mire com o mouse e clique nos alvos" },
    { "ESQUIVAR",          "ESPAÇO", false, "quando a bala chegar: você atravessa" },
    { "ACHAR A BRECHA",    NULL,     true,  "DOURADO = atire.  ROXO = espere" },
    { "REBATER (PARRY)",   "V",      false, "aperte quando o anel ficar DOURADO" },
    { "PULSO",             "Q",      false, "balas em volta? apague todas de uma vez" },
    { "ESCOLHER MELHORIA", NULL,     true,  "clique numa carta" },
    { "LUTA DE VERDADE",   NULL,     false, "esquive, espere a brecha e rebata" },
    { "",                  NULL,     false, "" }
};

static bool MestreEtapaComCartao(EtapaMestre e)
{
    return e >= MT_TIRO && e <= MT_LUTA && e != MT_MELHORIA;
}

static int MestreProgressoEtapa(const MestreTreino *m, int *total)
{
    int n = 0;
    *total = 0;
    switch (m->etapa) {
        case MT_TIRO:     *total = MESTRE_ALVOS; for (int a = 0; a < MESTRE_ALVOS; a++) if (!m->alvoVivo[a]) n++; break;
        case MT_ESQUIVA:  *total = MESTRE_ESQUIVAS; n = m->esquivas; break;
        case MT_ABERTURA: *total = MESTRE_ACERTOS_ABERTURA; n = m->acertosAbertura; break;
        case MT_PARRY:    *total = 1; n = m->parries; break;
        case MT_PULSO:    *total = 1; n = m->pulsos; break;
        default: break;
    }
    return (n > *total) ? *total : n;
}

// Uma bala inimiga vindo na direção do Thomas (dentro do raio)
static bool MestreBalaChegando(const Biblioteca *b, float raio)
{
    for (int i = 0; i < MAX_PROJETEIS; i++) {
        const Projetil *p = &b->proj[i];
        if (!p->ativo || p->doJogador) continue;
        Vector2 d = Vector2Subtract(b->j.pos, p->pos);
        if (Vector2Length(d) < raio && Vector2DotProduct(d, p->vel) > 0.0f) return true;
    }
    return false;
}

static int MestreBalasPerto(const Biblioteca *b, float raio)
{
    int n = 0;
    for (int i = 0; i < MAX_PROJETEIS; i++)
        if (b->proj[i].ativo && !b->proj[i].doJogador && Vector2Distance(b->proj[i].pos, b->j.pos) < raio) n++;
    return n;
}

// Demonstração animada de cada etapa (dentro do cartão): mostra o QUANDO sem depender de texto
static void DesenharDemoMestre(Rectangle r, EtapaMestre e, float t)
{
    Color ciano = { 0, 228, 255, 255 }, roxo = { 200, 122, 255, 255 };
    Vector2 jog = { r.x + 40.0f, r.y + r.height / 2.0f };
    DrawRectangleRec(r, (Color){ 14, 12, 28, 255 });
    DrawRectangleLinesEx(r, 1, Fade(SKYBLUE, 0.4f));
    BeginScissorMode((int)r.x, (int)r.y, (int)r.width, (int)r.height);

    switch (e) {
        case MT_TIRO: {                 // a mira chega no alvo, o clique, o tiro e o alvo estoura
            float k = fmodf(t, 1.6f) / 1.6f;
            Vector2 alvo = { r.x + r.width - 42.0f, r.y + r.height / 2.0f };
            if (k < 0.62f) {
                DrawCircleV(alvo, 13, GOLD); DrawCircleV(alvo, 8, (Color){ 14, 12, 28, 255 }); DrawCircleV(alvo, 4, GOLD);
            } else {
                float q = (k - 0.62f) / 0.38f;
                DrawRing(alvo, 10 + 22 * q, 13 + 22 * q, 0, 360, 24, Fade(GOLD, 1.0f - q));
            }
            Vector2 mira = Vector2Lerp((Vector2){ r.x + r.width * 0.45f, r.y + 14.0f }, alvo, fminf(k / 0.35f, 1.0f));
            DrawCircleLines((int)mira.x, (int)mira.y, 9, WHITE);
            DrawLineEx((Vector2){ mira.x - 13, mira.y }, (Vector2){ mira.x - 5, mira.y }, 2, WHITE);
            DrawLineEx((Vector2){ mira.x + 5, mira.y }, (Vector2){ mira.x + 13, mira.y }, 2, WHITE);
            if (k > 0.40f && k < 0.62f) DrawCircleV(Vector2Lerp(jog, alvo, (k - 0.40f) / 0.22f), 3.5f, ciano);
            DrawCircleV(jog, 8, ciano);
            DesenharMouseTutorial((Vector2){ r.x + 20.0f, r.y + 20.0f }, 0, 0.55f, k > 0.36f && k < 0.46f, 1.0f);
        } break;

        case MT_ESQUIVA: {              // a bala chega e o Thomas ATRAVESSA (rastro azul): a esquiva deixa invulnerável
            float k = fmodf(t, 1.8f) / 1.8f;
            float xBala = r.x + r.width + 10.0f - (r.width + 20.0f) * k;
            float xJog = jog.x;
            bool dash = (k > 0.40f && k < 0.52f);
            if (k >= 0.40f) xJog = jog.x + 70.0f * fminf((k - 0.40f) / 0.12f, 1.0f) - 70.0f * Clamp((k - 0.7f) / 0.3f, 0.0f, 1.0f);
            if (dash) for (int g = 1; g <= 3; g++) DrawCircleV((Vector2){ xJog - 10.0f * g, jog.y }, 8.0f - g, Fade(ciano, 0.35f / g));
            DrawCircleV((Vector2){ xBala, jog.y }, 6, (k > 0.40f && k < 0.62f) ? Fade(ORANGE, 0.5f) : ORANGE);
            DrawCircleV((Vector2){ xJog, jog.y }, 8, ciano);
            DesenharTeclaTutorial((Vector2){ r.x + r.width - 36.0f, r.y + 16.0f }, "ESPAÇO", 0.55f, dash, 1.0f);
        } break;

        case MT_ABERTURA: {             // escudo roxo: o tiro bate e some. Escudo dourado: o tiro acerta
            float k = fmodf(t, 2.6f) / 2.6f;
            bool aberto = k >= 0.5f;
            Vector2 chefe = { r.x + r.width - 46.0f, r.y + r.height / 2.0f };
            DrawCircleV(chefe, 16, (Color){ 60, 10, 80, 255 });
            if (aberto) DrawRing(chefe, 20, 24, 0, 360, 32, GOLD);
            else for (int s = 0; s < 6; s++) DrawRing(chefe, 20, 23, s * 60.0f + t * 60.0f, s * 60.0f + t * 60.0f + 35.0f, 6, roxo);
            float fase = fmodf(k * 6.0f, 1.0f);
            Vector2 bala = Vector2Lerp(jog, chefe, fase);
            if (fase < 0.85f) DrawCircleV(bala, 3.5f, ciano);
            else if (aberto) DrawCircleV(chefe, 18, Fade(WHITE, 0.5f));
            else { DrawLineEx((Vector2){ chefe.x - 30, chefe.y - 6 }, (Vector2){ chefe.x - 22, chefe.y + 2 }, 2, roxo);
                   DrawLineEx((Vector2){ chefe.x - 30, chefe.y + 2 }, (Vector2){ chefe.x - 22, chefe.y - 6 }, 2, roxo); }
            DrawCircleV(jog, 8, ciano);
            DrawText(aberto ? "ATIRE!" : "ESPERE", (int)chefe.x - MeasureText(aberto ? "ATIRE!" : "ESPERE", 12) / 2, (int)r.y + 4, 12, aberto ? GOLD : roxo);
        } break;

        case MT_PARRY: {                // ela vem; o anel fecha em volta do Thomas; DOURADO = V; o golpe volta nela
            float k = fmodf(t, 2.2f) / 2.2f;
            float chega = fminf(k / 0.8f, 1.0f);
            bool rebateu = k >= 0.8f;
            Vector2 chefe = { Lerp(r.x + r.width - 30.0f, jog.x + 34.0f, chega * chega), jog.y };
            if (rebateu) chefe.x = jog.x + 34.0f + 60.0f * (k - 0.8f) / 0.2f;
            float anel = 12.0f + 36.0f * (1.0f - chega);
            bool dourado = chega > 0.72f && !rebateu;
            if (!rebateu) DrawRing(jog, anel - 2.0f, anel + 1.0f, 0, 360, 32, dourado ? GOLD : Fade(RED, 0.8f));
            DrawCircleV(chefe, 12, (Color){ 120, 20, 40, 255 });
            DrawCircleLines((int)chefe.x, (int)chefe.y, 15, RED);
            if (rebateu) { float q = (k - 0.8f) / 0.2f; DrawRing(jog, 14 + 24 * q, 17 + 24 * q, 0, 360, 24, Fade(GOLD, 1.0f - q)); }
            DrawCircleV(jog, 8, ciano);
            DesenharTeclaTutorial((Vector2){ r.x + r.width / 2.0f, r.y + 16.0f }, "V", 0.6f, dourado || (k >= 0.8f && k < 0.86f), dourado ? 1.0f : 0.5f);
        } break;

        case MT_PULSO: {                // as balas cercam; Q: o anel azul apaga todas
            float k = fmodf(t, 2.0f) / 2.0f;
            Vector2 c = { r.x + r.width / 2.0f, r.y + r.height / 2.0f };
            if (k < 0.6f)
                for (int s = 0; s < 8; s++) {
                    float a = s * PI / 4.0f, d = 62.0f - 40.0f * (k / 0.6f);
                    DrawCircleV((Vector2){ c.x + cosf(a) * d, c.y + sinf(a) * d * 0.6f }, 4, ORANGE);
                }
            else {
                float q = (k - 0.6f) / 0.4f;
                DrawRing(c, 50.0f * q, 50.0f * q + 4.0f, 0, 360, 32, Fade(SKYBLUE, 1.0f - q));
            }
            DrawCircleV(c, 8, ciano);
            DesenharTeclaTutorial((Vector2){ r.x + 20.0f, r.y + 16.0f }, "Q", 0.6f, k >= 0.58f && k < 0.66f, 1.0f);
        } break;

        default: {                      // a luta: tudo junto
            static const char *nomes[4] = { "atira", "esquiva", "rebate", "pulso" };
            static const char *teclas[4] = { NULL, "ESPAÇO", "V", "Q" };
            for (int i = 0; i < 4; i++) {
                Vector2 p = { r.x + r.width * (0.14f + 0.24f * i), r.y + r.height / 2.0f - 6.0f };
                bool aceso = ((int)(t * 1.5f) % 4) == i;
                if (teclas[i] == NULL) DesenharMouseTutorial(p, 0, 0.7f, aceso, 1.0f);
                else                   DesenharTeclaTutorial(p, teclas[i], 0.6f, aceso, 1.0f);
                DrawText(nomes[i], (int)p.x - MeasureText(nomes[i], 10) / 2, (int)p.y + 20, 10, aceso ? GOLD : LIGHTGRAY);
            }
        } break;
    }
    EndScissorMode();
}

// O ícone da etapa (a tecla ou o mouse), apertando de tempos em tempos
static void DesenharIconeLicao(const LicaoMestre *l, Vector2 c, float escala, float tempo, float alfa)
{
    bool aperta = fmodf(tempo * 1.4f, 1.0f) < 0.25f;
    if (l->mouse)            DesenharMouseTutorial(c, 0, escala, aperta, alfa);
    else if (l->tecla)       DesenharTeclaTutorial(c, l->tecla, escala, aperta, alfa);
}

// O cartão no meio da tela (começo da etapa) e, depois, o painel no topo com o progresso
static void DesenharCartaoEtapaMestre(const Biblioteca *b, float tempo)
{
    const MestreTreino *m = &b->mestre;
    if (m->etapa == MT_MELHORIA || m->etapa >= MT_FIM || m->morrendo > 0.0f) return;
    const LicaoMestre *l = &LICOES_MESTRE[m->etapa];
    float te = m->tempoEtapa;
    bool comCartao = MestreEtapaComCartao(m->etapa);

    if (comCartao && te < CARTAO_ETAPA_TEMPO) {
        float entra = Clamp(te / 0.22f, 0.0f, 1.0f);
        float sai = Clamp((te - (CARTAO_ETAPA_TEMPO - 0.35f)) / 0.35f, 0.0f, 1.0f);
        entra = entra * entra * (3.0f - 2.0f * entra);
        sai = sai * sai * (3.0f - 2.0f * sai);
        Rectangle grande = { 190, 150, 420, 210 };
        Rectangle topo = { 220, 44, 360, 30 };
        float esc = 0.9f + 0.1f * entra;
        Rectangle r = { Lerp(grande.x + grande.width * (1 - esc) / 2, topo.x, sai), Lerp(grande.y + grande.height * (1 - esc) / 2, topo.y, sai),
                        Lerp(grande.width * esc, topo.width, sai), Lerp(grande.height * esc, topo.height, sai) };
        float a = entra * (1.0f - sai);
        DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, 0.35f * a));
        DrawRectangleRec(r, Fade((Color){ 12, 10, 26, 255 }, 0.95f * entra));
        DrawRectangleLinesEx(r, 3, Fade((Color){ 0, 228, 255, 255 }, entra));
        if (sai < 0.5f) {
            float c = 1.0f - sai * 2.0f;
            const char *cab = TextFormat("TREINO  -  ETAPA %d DE 8", (int)m->etapa + 1);
            DrawText(cab, (int)(r.x + r.width / 2) - MeasureText(cab, 10) / 2, (int)r.y + 10, 10, Fade((Color){ 0, 228, 255, 255 }, a * c));
            DrawText(l->titulo, (int)(r.x + r.width / 2) - MeasureText(l->titulo, 28) / 2, (int)r.y + 26, 28, Fade(WHITE, a * c));
            if (m->etapa != MT_LUTA) DesenharIconeLicao(l, (Vector2){ r.x + 70.0f, r.y + 112.0f }, 1.9f, tempo, a * c);
            Rectangle demo = (m->etapa == MT_LUTA) ? (Rectangle){ r.x + 20, r.y + 64, r.width - 40, 92 } : (Rectangle){ r.x + 140, r.y + 64, r.width - 160, 92 };
            if (a * c > 0.6f) DesenharDemoMestre(demo, m->etapa, te);
            DrawText(l->regra, (int)(r.x + r.width / 2) - MeasureText(l->regra, 16) / 2, (int)(r.y + r.height) - 34, 16, Fade(GOLD, a * c));
        }
        return;
    }

    // Painel no topo: o ícone, o nome da etapa, a regra e as bolinhas do progresso
    int total = 0, feito = MestreProgressoEtapa(m, &total);
    bool empacado = m->dicas > 0 && ((int)(tempo * 3.0f)) % 2 == 0;
    int wTit = MeasureText(l->titulo, 14), wReg = MeasureText(l->regra, 12);
    float wIc = l->mouse ? 18.0f : (l->tecla ? LarguraTeclaTutorial(l->tecla, 0.7f) : 0.0f);
    int w = (int)wIc + (wIc > 0 ? 10 : 0) + wTit + 12 + wReg + (total > 0 ? 12 + total * 14 : 0) + 24;
    int x = LARGURA_TELA / 2 - w / 2, y = 44;
    DrawRectangle(x, y, w, 30, Fade((Color){ 12, 10, 26, 255 }, 0.9f));
    DrawRectangleLinesEx((Rectangle){ (float)x, (float)y, (float)w, 30 }, 2, empacado ? GOLD : (Color){ 0, 228, 255, 255 });
    int cx = x + 12;
    if (wIc > 0) { DesenharIconeLicao(l, (Vector2){ cx + wIc / 2.0f, y + 14.0f }, 0.7f, tempo, 1.0f); cx += (int)wIc + 10; }
    DrawText(l->titulo, cx, y + 8, 14, WHITE);
    cx += wTit + 12;
    DrawText(l->regra, cx, y + 10, 12, LIGHTGRAY);
    cx += wReg + 12;
    for (int k = 0; k < total; k++) {
        Vector2 p = { cx + 6.0f + 14.0f * k, y + 15.0f };
        if (k < feito) DrawCircleV(p, 5, GOLD);
        DrawCircleLines((int)p.x, (int)p.y, 5, GOLD);
    }
}

// Dicas no mundo, na hora de agir (em cima do Thomas, dos alvos e da Mestre)
void DesenharDicasMestreMundo(const Biblioteca *b, float tempo)
{
    const MestreTreino *m = &b->mestre;
    const JogadorRogue *j = &b->j;
    if (!m->ativo || b->morreu || m->morrendo > 0.0f || b->escolhaAberta) return;
    if (MestreEtapaComCartao(m->etapa) && m->tempoEtapa < CARTAO_ETAPA_TEMPO) return;     // o cartão já está explicando
    Vector2 acima = { j->pos.x, j->pos.y - 70.0f };
    bool treino = m->etapa != MT_LUTA;

    switch (m->etapa) {
        case MT_ARMA: {                 // a seta no pedestal e uma trilha de pontos até ele
            DesenharSetaTutorial((Vector2){ POS_ARMA_MESTRE.x, POS_ARMA_MESTRE.y - 64.0f }, 11.0f, GOLD, tempo);
            if (Vector2Distance(j->pos, POS_ARMA_MESTRE) > 90.0f)
                for (int k = 1; k < 9; k++) {
                    Vector2 p = Vector2Lerp(j->pos, POS_ARMA_MESTRE, (float)k / 9.0f);
                    float onda = fmodf(tempo * 1.5f - (float)k / 9.0f, 1.0f);
                    DrawCircleV(p, 2.5f + 1.5f * (onda < 0.2f ? 1.0f : 0.0f), Fade(GOLD, 0.25f + 0.5f * (onda < 0.2f ? 1.0f : 0.0f)));
                }
        } break;
        case MT_TIRO:                   // setas nos alvos que faltam
            for (int a = 0; a < MESTRE_ALVOS; a++)
                if (m->alvoVivo[a]) DesenharSetaTutorial((Vector2){ m->alvo[a].x, m->alvo[a].y - 28.0f }, 8.0f, GOLD, tempo + a * 0.7f);
            break;
        case MT_ESQUIVA:
        case MT_ABERTURA:               // a bala vem: ESPERE... e, perto, AGORA!
            if (j->tempoEsquiva > 0.0f) DesenharDicaAgora(acima, "ESPAÇO", false, true, "ISSO!", tempo);
            else if (MestreBalaChegando(b, 95.0f) && j->recargaEsquiva <= 0.0f) DesenharDicaAgora(acima, "ESPAÇO", false, true, "AGORA!", tempo);
            else if (m->etapa == MT_ESQUIVA && MestreBalaChegando(b, 170.0f)) DesenharDicaAgora(acima, "ESPAÇO", false, false, "ESPERE...", tempo);
            break;
        case MT_PULSO: {                // várias balas em volta: Q
            int perto = MestreBalasPerto(b, RogueRaioPulso(j) * 1.15f);
            bool pronto = j->recargaPulso <= 0.0f && j->ep >= PULSO_CUSTO_EP;
            if (perto >= 3 && pronto)  DesenharDicaAgora(acima, "Q", false, true, "AGORA!", tempo);
            else if (perto >= 1)       DesenharDicaAgora(acima, "Q", false, false, pronto ? "ESPERE CHEGAREM..." : "SEM EP", tempo);
        } break;
        default: break;
    }

    // O golpe: o anel fecha em volta do Thomas. No treino: ESPERE... (vermelho) e AGORA! (dourado); na luta, só o AGORA!
    if (m->etapa == MT_PARRY || m->etapa == MT_LUTA) {
        float falta = MestreTempoAteGolpe(m, j->pos, MestreTelegrafia(m));
        if (falta >= 0.0f && falta < 1.4f) {
            bool agora = falta < MestreJanelaParry(b);
            if (agora)       DesenharDicaAgora(acima, "V", false, true, "AGORA!", tempo);
            else if (treino) DesenharDicaAgora(acima, "V", false, false, "ESPERE O DOURADO...", tempo);
        }
    }

    // A brecha: em cima da Mestre, o mouse e ATIRE! (com o tempo que falta) ou, fechada, ESPERE (só no treino)
    if (m->visivel && m->surgindo <= 0.0f && m->etapa >= MT_ABERTURA) {
        bool noAlto = m->pos.y < 240.0f;                  // perto do topo: a dica vai embaixo dela (não cobre a barra)
        Vector2 sobre = { m->pos.x, noAlto ? m->pos.y + MESTRE_RAIO + 50.0f : m->pos.y - MESTRE_RAIO - 46.0f };
        if (MestreVulneravel(m)) {
            float max = (m->acao == MA_ATORDOADA) ? MESTRE_STUN : ((m->acao == MA_ABERTA) ? MESTRE_ABERTURA : MESTRE_RECUPERA);
            DrawRing(m->pos, MESTRE_RAIO + 20.0f, MESTRE_RAIO + 24.0f, -90.0f, -90.0f + 360.0f * Clamp(m->tempoAcao / max, 0.0f, 1.0f), 40, GOLD);
            DesenharDicaAgora(sobre, NULL, true, true, (m->acao == MA_ATORDOADA) ? "ATORDOADA! ATIRE!" : "ATIRE!", tempo);
        } else if (m->etapa == MT_ABERTURA && m->acao != MA_TELEGRAFA && m->acao != MA_INVESTE) {
            DesenharDicaAgora(sobre, NULL, true, false, "ESPERE O DOURADO", tempo);
        }
    }
}

void DesenharMestreTreino(const Biblioteca *b, float tempo)
{
    const MestreTreino *m = &b->mestre;

    if (!m->ativo)
        return;

    if (m->etapa == MT_ARMA)
        DesenharPedestalArmaMestre(POS_ARMA_MESTRE, tempo);

    if (m->etapa == MT_TIRO)
        for (int a = 0; a < MESTRE_ALVOS; a++)
            if (m->alvoVivo[a]) DesenharAlvoTreino(m->alvo[a], tempo, a);

    if (!m->visivel && m->morrendo <= 0.0f)
        return;

    Vector2 c = m->pos;
    if (m->morrendo > 0.0f) { c.x += (float)GetRandomValue(-4, 4); c.y += (float)GetRandomValue(-4, 4); }
    if (m->acao == MA_TELEGRAFA) c.x += (float)GetRandomValue(-2, 2);

    // Golpe vindo: aura vermelha, a linha do caminho e o "!"
    bool golpe = (m->acao == MA_TELEGRAFA || m->acao == MA_INVESTE) && m->morrendo <= 0.0f;
    if (golpe) {
        float p = 0.5f + 0.5f * sinf(tempo * 18.0f);
        DrawCircleV(c, MESTRE_RAIO + 26 + 6 * p, Fade(RED, 0.18f + 0.12f * p));
        if (m->acao == MA_TELEGRAFA) {          // o corredor enche; no último terço a mira trava (bordas brancas e setas)
            float tel = MestreTelegrafia(m);
            DesenharCorredorAvisoIni(c, m->dirGolpe, MESTRE_INVESTIDA_VEL * MESTRE_INVESTIDA_TEMPO + MESTRE_RAIO, MESTRE_RAIO * 0.7f,
                                     1.0f - m->tempoAcao / tel, m->tempoAcao <= tel * 0.3f, RED, tempo);
            DrawText("!", (int)c.x - 4, (int)c.y - 110, 30, RED);
        }
    }

    // A Mestre (o mesmo desenho do Bloco 1)
    DesenharAlucinacaoMestre(c, tempo);
    if (golpe) {        // os "olhos" em vermelho neon
        DrawCircleV((Vector2){ c.x - 12, c.y - 8 }, 5, RED);
        DrawCircleV((Vector2){ c.x + 12, c.y - 8 }, 5, RED);
    }
    if (m->flash > 0.0f)
        DrawCircleV(c, MESTRE_RAIO + 6, Fade(WHITE, Clamp(m->flash * 5.0f, 0.0f, 0.7f)));
    if (m->surgindo > 0.0f)
        DrawCircleV(c, 110, Fade((Color){ 16, 12, 30, 255 }, Clamp(m->surgindo, 0.0f, 1.0f)));
    if (m->morrendo > 0.0f)
        DrawCircleV(c, 120, Fade((Color){ 16, 12, 30, 255 }, Clamp(1.0f - m->morrendo / 1.8f, 0.0f, 1.0f)));

    // Escudo: fechado (roxo) ou aberto (dourado, a hora de atirar)
    if (m->morrendo <= 0.0f && m->surgindo <= 0.0f) {
        if (MestreVulneravel(m)) {
            float p = 0.5f + 0.5f * sinf(tempo * 8.0f);
            DrawCircleLines((int)c.x, (int)c.y, MESTRE_RAIO + 10 + 4 * p, GOLD);
            DrawCircleLines((int)c.x, (int)c.y, MESTRE_RAIO + 14 + 4 * p, Fade(GOLD, 0.5f));
            if (m->acao == MA_ATORDOADA)
                for (int k = 0; k < 3; k++) {
                    float a = tempo * 4.0f + k * 2.1f;
                    DrawPoly((Vector2){ c.x + cosf(a) * 30, c.y - 66 + sinf(a) * 8 }, 4, 5, tempo * 200.0f, GOLD);
                }
        } else {
            Color roxo = { 200, 122, 255, 255 };
            for (int k = 0; k < 6; k++) {
                float a0 = tempo * 0.8f + k * (PI / 3.0f);
                DrawRing(c, MESTRE_RAIO + 9, MESTRE_RAIO + 12, a0 * RAD2DEG, a0 * RAD2DEG + 40.0f, 6, Fade(roxo, 0.75f));
            }
        }
    }

    // O anel do parry: fecha em volta do Thomas no instante do impacto
    float falta = MestreTempoAteGolpe(m, b->j.pos, MestreTelegrafia(m));
    if (falta >= 0.0f && falta < 1.4f && (m->etapa == MT_PARRY || m->etapa == MT_LUTA)) {
        float raio = RAIO_CORPO_JOGADOR + 4.0f + 70.0f * (falta / 1.4f);
        bool agora = falta < MestreJanelaParry(b);
        float k = falta / 1.4f;                                   // o mesmo anel da luta da Luci
        DrawRing(b->j.pos, raio - 2.0f, raio + 1.0f, 0.0f, 360.0f, 40, agora ? GOLD : Fade(RED, 0.4f + 0.5f * (1.0f - k)));
        DrawCircleLines((int)b->j.pos.x, (int)b->j.pos.y, RAIO_CORPO_JOGADOR + 6.0f, Fade(agora ? GOLD : RED, 0.5f));
        if (agora)
            DrawCircleV(b->j.pos, RAIO_CORPO_JOGADOR + 8.0f, Fade(GOLD, 0.18f));
    }
}

// Barra de vida e as etapas do treino (no alto da tela)
void DesenharHudMestre(const Biblioteca *b, float tempo)
{
    const MestreTreino *m = &b->mestre;

    if (!m->ativo)
        return;

    // As etapas: o que já foi aprendido fica dourado (embaixo, à esquerda, acima do painel da Luci)
    static const char *nomes[8] = { "ARMA", "TIRO", "ESQUIVA", "BRECHA", "PARRY", "PULSO", "ESCOLHA", "LUTA" };
    int x = 46, y = 464;
    for (int i = 0; i < 8; i++) {
        bool feito = (int)m->etapa > i, atual = (int)m->etapa == i;
        int w = MeasureText(nomes[i], 10) + 12;
        DrawRectangle(x, y, w, 15, feito ? Fade(GOLD, 0.25f) : (atual ? Fade((Color){ 0, 228, 255, 255 }, 0.2f + 0.1f * sinf(tempo * 4.0f)) : Fade(BLACK, 0.5f)));
        DrawRectangleLines(x, y, w, 15, feito ? GOLD : (atual ? (Color){ 0, 228, 255, 255 } : Fade(GRAY, 0.6f)));
        DrawText(nomes[i], x + 6, y + 3, 10, feito ? GOLD : (atual ? WHITE : GRAY));
        x += w + 4;
    }

    // O cartão da etapa (no meio da tela) e o painel com o progresso (no topo)
    DesenharCartaoEtapaMestre(b, tempo);
    if (m->etapa == MT_TIRO && m->tempoEtapa >= CARTAO_ETAPA_TEMPO && !b->escolhaAberta) {
        Vector2 mm = GetMousePosition();
        DesenharMouseTutorial((Vector2){ mm.x + 26.0f, mm.y + 24.0f }, 0, 0.7f, fmodf(tempo * 2.0f, 1.0f) < 0.3f, 0.9f);
    }

    if (!m->visivel || m->etapa < MT_ABERTURA)
        return;

    int bx = 200, by = 76, w = 400;
    DrawRectangle(bx - 4, by - 2, w + 8, 16, Fade(BLACK, 0.8f));
    DrawRectangle(bx, by + 2, w, 8, (Color){ 40, 20, 50, 255 });
    DrawRectangle(bx, by + 2, (int)(w * m->hp / MESTRE_HP), 8, MestreVulneravel(m) ? GOLD : MAGENTA);
    if (m->etapa == MT_LUTA)
        DrawRectangle(bx + (int)(w * MESTRE_FASE2) - 1, by, 2, 12, WHITE);
    DrawRectangleLines(bx, by + 2, w, 8, GRAY);
    const char *nome = "ALUCINAÇÃO MESTRE";
    int wn = MeasureText(nome, 10);
    DrawText(nome, bx + w / 2 - wn / 2, by + 14, 10, MAGENTA);
}

void DesenharPortaRogue(Rectangle r, bool aberta, const char *nome, float tempo)
{
    float pulso = 0.5f + 0.5f * sinf(tempo * 4.0f);
    Color cor = aberta ? Fade(GREEN, 0.55f + 0.45f * pulso) : MAROON;

    DrawRectangleRec(r, aberta ? Fade(GREEN, 0.25f) : (Color){ 30, 10, 20, 255 });
    DrawRectangleLinesEx(r, 2, cor);

    const char *rotulo = aberta ? nome : "TRANCADA";
    int w = MeasureText(rotulo, 10);
    int x = (r.width > r.height) ? (int)(r.x + r.width / 2) - w / 2 : (int)(r.x + r.width) + 6;
    int y = (r.width > r.height) ? (int)(r.y + r.height) + 4 : (int)(r.y + r.height / 2) - 5;

    DrawText(rotulo, x, y, 10, aberta ? GREEN : RED);
}


// Ícone de cada item (loja, pedestais, chão e HUD)
void DesenharIconeItem(TipoItem item, Vector2 c, float e, float tempo)
{
    switch (item) {

        case ITEM_ANTIVIRUS:        // escudo verde com uma cruz
            DesenharTrianguloB2((Vector2){ c.x - 9 * e, c.y - 8 * e }, (Vector2){ c.x + 9 * e, c.y - 8 * e }, (Vector2){ c.x, c.y + 11 * e }, LIME);
            DrawRectangle((int)(c.x - 9 * e), (int)(c.y - 11 * e), (int)(18 * e), (int)(5 * e), LIME);
            DrawRectangle((int)(c.x - 1.5f * e), (int)(c.y - 8 * e), (int)(3 * e), (int)(10 * e), WHITE);
            DrawRectangle((int)(c.x - 5 * e), (int)(c.y - 4.5f * e), (int)(10 * e), (int)(3 * e), WHITE);
            break;

        case ITEM_PACOTE_DADOS:     // pacote azul com bits
            DrawRectangle((int)(c.x - 9 * e), (int)(c.y - 9 * e), (int)(18 * e), (int)(18 * e), (Color){ 30, 90, 180, 255 });
            DrawRectangleLinesEx((Rectangle){ c.x - 9 * e, c.y - 9 * e, 18 * e, 18 * e }, 2, SKYBLUE);
            DrawText("01", (int)(c.x - 6 * e), (int)(c.y - 5 * e), (int)(10 * e), WHITE);
            break;

        case ITEM_LENTE:
            DesenharLupaVerificacao(c, 0.6f * e, tempo);
            break;

        case ITEM_FIREWALL:         // muro de tijolos em chamas
            for (int l = 0; l < 3; l++)
                for (int k = 0; k < 3; k++)
                    DrawRectangle((int)(c.x - 10 * e + k * 7 * e + (l % 2) * 3 * e), (int)(c.y - 4 * e + l * 5 * e),
                                  (int)(6 * e), (int)(4 * e), ORANGE);
            DesenharTrianguloB2((Vector2){ c.x - 6 * e, c.y - 5 * e }, (Vector2){ c.x, c.y - 13 * e - sinf(tempo * 8) * 2 * e },
                                (Vector2){ c.x + 6 * e, c.y - 5 * e }, RED);
            break;

        case ITEM_PROCESSADOR:      // chip roxo com pernas
            for (int k = 0; k < 3; k++) {
                DrawRectangle((int)(c.x - 6 * e + k * 5 * e), (int)(c.y - 12 * e), (int)(2 * e), (int)(24 * e), LIGHTGRAY);
                DrawRectangle((int)(c.x - 12 * e), (int)(c.y - 6 * e + k * 5 * e), (int)(24 * e), (int)(2 * e), LIGHTGRAY);
            }
            DrawRectangle((int)(c.x - 8 * e), (int)(c.y - 8 * e), (int)(16 * e), (int)(16 * e), PURPLE);
            DrawRectangleLinesEx((Rectangle){ c.x - 8 * e, c.y - 8 * e, 16 * e, 16 * e }, 1, MAGENTA);
            break;

        case ITEM_OVERCLOCK:        // raio laranja
            DesenharTrianguloB2((Vector2){ c.x + 2 * e, c.y - 12 * e }, (Vector2){ c.x - 7 * e, c.y + 2 * e }, (Vector2){ c.x + 1 * e, c.y + 1 * e }, ORANGE);
            DesenharTrianguloB2((Vector2){ c.x - 2 * e, c.y + 12 * e }, (Vector2){ c.x + 7 * e, c.y - 2 * e }, (Vector2){ c.x - 1 * e, c.y - 1 * e }, ORANGE);
            break;

        case ITEM_FIREWALL_PORTATIL:    // hexágono com um escudo dentro
            DrawPolyLinesEx(c, 6, 11 * e, 0, 2, (Color){ 0, 228, 48, 255 });
            DesenharTrianguloB2((Vector2){ c.x - 5 * e, c.y - 4 * e }, (Vector2){ c.x + 5 * e, c.y - 4 * e }, (Vector2){ c.x, c.y + 6 * e }, (Color){ 0, 228, 48, 255 });
            break;

        case ITEM_LIMPEZA_CACHE:    // lixeira
            DrawRectangle((int)(c.x - 7 * e), (int)(c.y - 5 * e), (int)(14 * e), (int)(15 * e), SKYBLUE);
            DrawRectangle((int)(c.x - 9 * e), (int)(c.y - 9 * e), (int)(18 * e), (int)(3 * e), SKYBLUE);
            for (int k = -1; k <= 1; k++)
                DrawRectangle((int)(c.x + k * 4 * e - 1), (int)(c.y - 2 * e), 2, (int)(9 * e), (Color){ 20, 40, 70, 255 });
            break;

        case ITEM_PROXY:            // bonequinho tracejado (holograma)
            DrawCircleLines((int)c.x, (int)(c.y - 7 * e), 4 * e, SKYBLUE);
            DrawRectangleLines((int)(c.x - 5 * e), (int)(c.y - 2 * e), (int)(10 * e), (int)(12 * e), SKYBLUE);
            DrawLine((int)(c.x - 8 * e), (int)c.y, (int)(c.x + 8 * e), (int)c.y, Fade(SKYBLUE, 0.5f));
            break;

        case ITEM_PACOTE_INTEGRIDADE:   // patch de correção (a cura do jogo)
            DesenharPatchCuraEscala(c, e, 1.0f, tempo);
            break;

        case ITEM_CAMPO_LATENCIA:   // relógio
            DrawCircleV(c, 10 * e, (Color){ 50, 30, 80, 255 });
            DrawCircleLines((int)c.x, (int)c.y, 10 * e, PURPLE);
            DrawLineEx(c, (Vector2){ c.x, c.y - 7 * e }, 2, WHITE);
            DrawLineEx(c, (Vector2){ c.x + 5 * e, c.y + 2 * e }, 2, WHITE);
            break;

        case ITEM_NUCLEO:           // núcleo de integridade: hexágono verde com um núcleo de luz
            DesenharNucleoIntegridade(c, e, tempo);
            break;

        case ITEM_BUFFER:           // barras crescendo
            for (int k = 0; k < 3; k++)
                DrawRectangle((int)(c.x - 9 * e + k * 7 * e), (int)(c.y + 8 * e - (k + 1) * 5 * e), (int)(5 * e), (int)((k + 1) * 5 * e), YELLOW);
            break;

        case ITEM_ROTEAMENTO:       // ">>"
            for (int k = 0; k < 2; k++) {
                float x0 = c.x - 8 * e + k * 7 * e;
                DrawLineEx((Vector2){ x0, c.y - 7 * e }, (Vector2){ x0 + 6 * e, c.y }, 2.5f, SKYBLUE);
                DrawLineEx((Vector2){ x0 + 6 * e, c.y }, (Vector2){ x0, c.y + 7 * e }, 2.5f, SKYBLUE);
            }
            break;

        case ITEM_FRAGMENTACAO:     // um círculo que vira três
            DrawCircleV((Vector2){ c.x - 5 * e, c.y }, 4 * e, LIME);
            for (int k = -1; k <= 1; k++)
                DrawCircleV((Vector2){ c.x + 6 * e, c.y + k * 6 * e }, 2 * e, LIME);
            break;

        case ITEM_FIREWALL_ADAPTATIVO:  // escudo em camadas
            DesenharTrianguloB2((Vector2){ c.x - 9 * e, c.y - 8 * e }, (Vector2){ c.x + 9 * e, c.y - 8 * e }, (Vector2){ c.x, c.y + 10 * e }, (Color){ 0, 158, 47, 255 });
            DesenharTrianguloB2((Vector2){ c.x - 5 * e, c.y - 5 * e }, (Vector2){ c.x + 5 * e, c.y - 5 * e }, (Vector2){ c.x, c.y + 5 * e }, (Color){ 0, 228, 48, 255 });
            break;

        case ITEM_ANALISADOR:       // mira com três marcas
            DrawCircleLines((int)c.x, (int)c.y, 8 * e, ORANGE);
            DrawLine((int)(c.x - 11 * e), (int)c.y, (int)(c.x + 11 * e), (int)c.y, ORANGE);
            DrawLine((int)c.x, (int)(c.y - 11 * e), (int)c.x, (int)(c.y + 11 * e), ORANGE);
            for (int k = 0; k < 3; k++)
                DrawPoly((Vector2){ c.x - 6 * e + k * 6 * e, c.y + 13 * e }, 4, 2 * e, 0, ORANGE);
            break;

        case ITEM_BOTNET_REVERSO:   // três nós ligados e uma seta para fora
            for (int k = 0; k < 3; k++) {
                float a = k * (2.0f * PI / 3.0f) - PI / 2.0f;
                Vector2 q = { c.x + cosf(a) * 7 * e, c.y + sinf(a) * 7 * e };
                DrawLineEx(c, q, 1, SKYBLUE);
                DrawCircleV(q, 2.5f * e, SKYBLUE);
            }
            DrawLineEx(c, (Vector2){ c.x + 11 * e, c.y + 9 * e }, 2, LIME);
            break;

        case ITEM_CACHE_PERSISTENTE:    // pente de memória com "+"
            DrawRectangle((int)(c.x - 11 * e), (int)(c.y - 4 * e), (int)(22 * e), (int)(9 * e), (Color){ 20, 90, 50, 255 });
            for (int k = 0; k < 5; k++)
                DrawRectangle((int)(c.x - 10 * e + k * 4 * e), (int)(c.y + 5 * e), (int)(2 * e), (int)(2 * e), GOLD);
            DrawText("+", (int)(c.x - 3 * e), (int)(c.y - 10 * e), (int)(14 * e), LIME);
            break;

        case ITEM_MANUAL:           // livro dourado
        default:
            DrawRectangle((int)(c.x - 9 * e), (int)(c.y - 11 * e), (int)(18 * e), (int)(22 * e), GOLD);
            DrawRectangle((int)(c.x - 9 * e), (int)(c.y - 11 * e), (int)(4 * e), (int)(22 * e), ORANGE);
            DrawRectangle((int)(c.x - 2 * e), (int)(c.y - 5 * e), (int)(8 * e), (int)(2 * e), (Color){ 80, 50, 20, 255 });
            DrawRectangle((int)(c.x - 2 * e), (int)(c.y - 1 * e), (int)(8 * e), (int)(2 * e), (Color){ 80, 50, 20, 255 });
            break;
    }
}

// Fragmento de Conhecimento: uma página luminosa
void DesenharFragmentoConhecimento(Vector2 c, float t)
{
    Vector2 p = { c.x, c.y + sinf(t * 5.0f) * 2.0f };
    DrawCircleV(p, 9, Fade((Color){ 0, 228, 255, 255 }, 0.25f));
    DrawRectangle((int)p.x - 4, (int)p.y - 6, 8, 11, (Color){ 200, 250, 255, 255 });
    DrawRectangle((int)p.x - 2, (int)p.y - 3, 5, 1, (Color){ 0, 120, 160, 255 });
    DrawRectangle((int)p.x - 2, (int)p.y, 5, 1, (Color){ 0, 120, 160, 255 });
}

// Professor Byte: um holograma de professor (cabeça de monitor, óculos e capelo)
void DesenharProfessorByte(Vector2 c, float t)
{
    Vector2 p = { c.x, c.y + sinf(t * 2.0f) * 3.0f };
    Color holo = (Color){ 0, 228, 255, 255 };

    DrawEllipse((int)c.x, (int)c.y + 26, 20, 6, Fade(holo, 0.25f));
    DrawRectangle((int)p.x - 12, (int)p.y + 4, 24, 20, Fade(holo, 0.35f));                     // corpo
    DrawRectangle((int)p.x - 16, (int)p.y - 20, 32, 24, (Color){ 20, 50, 80, 255 });           // cabeça (monitor)
    DrawRectangleLinesEx((Rectangle){ p.x - 16, p.y - 20, 32, 24 }, 2, holo);
    DrawCircleLines((int)p.x - 7, (int)p.y - 8, 5, WHITE);                                     // óculos
    DrawCircleLines((int)p.x + 7, (int)p.y - 8, 5, WHITE);
    DrawLine((int)p.x - 2, (int)p.y - 8, (int)p.x + 2, (int)p.y - 8, WHITE);
    DrawCircle((int)p.x - 7, (int)p.y - 8, 2, holo);
    DrawCircle((int)p.x + 7, (int)p.y - 8, 2, holo);
    DrawRectangle((int)p.x - 18, (int)p.y - 26, 36, 6, (Color){ 30, 20, 50, 255 });             // capelo
    DrawRectangle((int)p.x - 8, (int)p.y - 30, 16, 5, (Color){ 30, 20, 50, 255 });
    DrawLine((int)p.x + 14, (int)p.y - 24, (int)p.x + 16, (int)p.y - 14, GOLD);                // pendão

    const char *nome = "PROF. BYTE";
    DrawText(nome, (int)c.x - MeasureText(nome, 10) / 2, (int)c.y + 34, 10, holo);
}

// Byteco: um robozinho comerciante atrás do balcão
void DesenharByteco(Vector2 c, float t)
{
    Vector2 p = { c.x, c.y + sinf(t * 3.0f) * 2.0f };

    DrawLine((int)p.x, (int)p.y - 22, (int)p.x, (int)p.y - 14, LIGHTGRAY);                     // antena
    DrawCircle((int)p.x, (int)p.y - 24, 3, (((int)(t * 2)) % 2) ? GOLD : ORANGE);
    DrawCircle((int)p.x, (int)p.y, 16, (Color){ 230, 160, 60, 255 });                          // corpo redondo
    DrawCircle((int)p.x, (int)p.y, 11, (Color){ 40, 30, 60, 255 });
    DrawCircle((int)p.x - 4, (int)p.y - 2, 3, (Color){ 0, 228, 255, 255 });
    DrawCircle((int)p.x + 4, (int)p.y - 2, 3, (Color){ 0, 228, 255, 255 });
    DrawLine((int)p.x - 4, (int)p.y + 5, (int)p.x + 4, (int)p.y + 5, (Color){ 0, 228, 255, 255 });

    // Balcão com os itens à mostra
    DrawRectangle(555, 318, 110, 24, (Color){ 70, 45, 30, 255 });
    DrawRectangleLinesEx((Rectangle){ 555, 318, 110, 24 }, 2, GOLD);
    for (int i = 0; i < 4; i++)
        DesenharIconeItem((TipoItem)i, (Vector2){ 572.0f + i * 26.0f, 330.0f }, 0.55f, t);

    const char *nome = "BYTECO - LOJA";
    DrawText(nome, 610 - MeasureText(nome, 10) / 2, 346, 10, GOLD);
}

// Terminal de conhecimento das salas educativas
void DesenharTerminal(Vector2 c, bool lido, float t)
{
    Color cor = lido ? Fade(GRAY, 0.8f) : (Color){ 0, 228, 255, 255 };

    if (!lido)
        DrawCircleV(c, 40 + sinf(t * 3.0f) * 4, Fade(cor, 0.12f));

    DrawRectangle((int)c.x - 20, (int)c.y - 20, 40, 34, (Color){ 20, 30, 50, 255 });
    DrawRectangleLinesEx((Rectangle){ c.x - 20, c.y - 20, 40, 34 }, 2, cor);
    for (int i = 0; i < 3; i++)
        DrawRectangle((int)c.x - 14, (int)c.y - 13 + i * 8, 12 + (int)(Ruido((unsigned int)(t * 4) + i) % 16), 3, Fade(cor, 0.8f));
    DrawRectangle((int)c.x - 6, (int)c.y + 14, 12, 8, (Color){ 40, 40, 60, 255 });
}

// Pedestal com uma recompensa
void DesenharPedestal(Vector2 c, TipoItem item, bool disponivel, float t)
{
    DrawEllipse((int)c.x, (int)c.y + 16, 22, 8, (Color){ 60, 45, 90, 255 });
    DrawRectangle((int)c.x - 16, (int)c.y + 4, 32, 12, (Color){ 50, 36, 80, 255 });

    if (!disponivel)
        return;

    Vector2 icone = { c.x, c.y - 10 + sinf(t * 3.0f + c.x) * 3.0f };
    DrawCircleV(icone, 18, Fade(GOLD, 0.12f));
    DesenharIconeItem(item, icone, 1.0f, t);
}

// Fragmento Alucinado: um cristal de dado corrompido (camadas deslocadas = glitch)
void DesenharFragmentoAlucinado(const Inimigo *e, float escala)
{
    float s = e->raio * escala;
    Vector2 p = e->pos;

    if (e->estado == 1)           // preparando o bote: treme e fica vermelho
        p.x += (float)GetRandomValue(-2, 2);

    Color cores[3] = { Fade(SKYBLUE, 0.5f), Fade(RED, 0.5f), (e->estado == 1) ? RED : (Color){ 230, 41, 255, 255 } };
    float desloc[3] = { -3.0f, 3.0f, 0.0f };

    for (int k = 0; k < 3; k++) {
        Vector2 c = { p.x + desloc[k], p.y };
        float giro = e->anim * 2.0f;
        Vector2 a = GirarPonto(c, 0, -s * 1.2f, giro * 20.0f);
        Vector2 d = GirarPonto(c, s, 0, giro * 20.0f);
        Vector2 f = GirarPonto(c, 0, s * 1.2f, giro * 20.0f);
        Vector2 g = GirarPonto(c, -s, 0, giro * 20.0f);
        DesenharTrianguloB2(a, d, f, cores[k]);
        DesenharTrianguloB2(a, f, g, cores[k]);
    }

    DrawText(e->estado == 1 ? "!" : "?", (int)p.x - 3, (int)p.y - 7, 14, WHITE);
}

// Guardião de Dados: um tomo flutuante que vigia (brilha antes de atirar)
void DesenharGuardiao(const Inimigo *e, float escala)
{
    float w = 30 * escala, h = 34 * escala;
    Vector2 p = { e->pos.x, e->pos.y + sinf(e->anim * 2.0f) * 3.0f };

    if (e->estado == 1) {
        float k = 1.0f - e->timer / GUARDIAO_PREPARO;
        DrawCircleV(p, e->raio + 6 + 10 * k, Fade(ORANGE, 0.25f + 0.3f * k));
    }

    DrawCircleV(p, e->raio + 4, Fade(SKYBLUE, 0.12f));
    DrawRectangle((int)(p.x - w / 2), (int)(p.y - h / 2), (int)w, (int)h, (Color){ 30, 60, 120, 255 });
    DrawRectangle((int)(p.x - w / 2), (int)(p.y - h / 2), (int)(6 * escala), (int)h, SKYBLUE);           // lombada
    DrawRectangleLinesEx((Rectangle){ p.x - w / 2, p.y - h / 2, w, h }, 2, SKYBLUE);

    DrawCircle((int)(p.x + 3 * escala), (int)p.y, 7 * escala, WHITE);
    DrawCircle((int)(p.x + 3 * escala), (int)p.y, 3.5f * escala, (e->estado == 1) ? ORANGE : (Color){ 0, 60, 140, 255 });
}

// Eco falso: um fantasma do fragmento, piscando cada vez mais rápido até explodir
void DesenharEco(const Inimigo *e)
{
    float k = 1.0f - e->timer / ECO_TEMPO;
    float pisca = 0.35f + 0.35f * sinf(e->anim * (8.0f + 20.0f * k));

    DrawCircleV(e->pos, e->raio + 4 * k, Fade(MAGENTA, 0.25f + pisca * 0.4f));
    DrawCircleLines((int)e->pos.x, (int)e->pos.y, e->raio + 10 * (1.0f - k), Fade(MAGENTA, 0.8f));
    DrawText("?", (int)e->pos.x - 3, (int)e->pos.y - 7, 14, Fade(WHITE, 0.8f));
}

// Selo dourado em volta do Guardião com desafio
void DesenharSelo(const Inimigo *e, float tempo)
{
    float r = e->raio + 14.0f;

    for (int i = 0; i < 6; i++) {
        float a1 = tempo * 1.2f + i * (PI / 3.0f), a2 = a1 + PI / 3.0f;
        DrawLineEx((Vector2){ e->pos.x + cosf(a1) * r, e->pos.y + sinf(a1) * r },
                   (Vector2){ e->pos.x + cosf(a2) * r, e->pos.y + sinf(a2) * r }, 3, Fade(GOLD, 0.8f));
    }

    DrawCircleV(e->pos, r, Fade(GOLD, 0.08f + 0.05f * sinf(tempo * 4.0f)));
    DrawText("?", (int)e->pos.x - 6, (int)(e->pos.y - e->raio - 40 + sinf(tempo * 3.0f) * 3.0f), 24, GOLD);
}
