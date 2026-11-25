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


// Uma seta ">" apontando para "dir" (s = tamanho)
// Formato do coração (centro x,y; vai de y - r até y + r)
static void DesenharFormaCoracao(float x, float y, float r, Color cor)
{
    DrawCircleV((Vector2){ x - r * 0.5f, y - r * 0.42f }, r * 0.58f, cor);
    DrawCircleV((Vector2){ x + r * 0.5f, y - r * 0.42f }, r * 0.58f, cor);
    DrawTriangle((Vector2){ x - r * 1.07f, y - r * 0.25f }, (Vector2){ x, y + r }, (Vector2){ x + r * 1.07f, y - r * 0.25f }, cor);
}

// Uma vida (coração com contorno escuro): cheia em vermelho com brilho, gasta em roxo apagado
static void DesenharIconeVida(float x, float y, float r, bool cheia, float alfa)
{
    DesenharFormaCoracao(x, y + r * 0.04f, r * 1.22f, Fade((Color){ 8, 6, 16, 255 }, alfa));
    DesenharFormaCoracao(x, y, r, Fade(cheia ? (Color){ 235, 45, 70, 255 } : (Color){ 72, 62, 92, 255 }, alfa));
    if (cheia)
        DrawCircleV((Vector2){ x - r * 0.48f, y - r * 0.5f }, r * 0.2f, Fade((Color){ 255, 190, 200, 255 }, 0.9f * alfa));
}

// As vidas da partida em fila, centradas em cx (a que acabou de ser gasta pode piscar)
static void DesenharVidas(const Biblioteca *b, float cx, float y, float r, float alfa, bool piscarUltima)
{
    int restam = RogueVidasRestantes(b);
    float passo = r * 2.75f;
    float x0 = cx - passo * (LIMITE_MORTES - 1) / 2.0f;

    for (int i = 0; i < LIMITE_MORTES; i++) {
        bool cheia = i < restam;
        if (piscarUltima && i == restam && ((int)(GetTime() * 4.0) & 1) == 0)
            cheia = true;
        DesenharIconeVida(x0 + passo * i, y, r, cheia, alfa);
    }
}

static void DesenharSetaAtalho(Vector2 p, Vector2 dir, float s, float espessura, Color c)
{
    Vector2 perp = { -dir.y, dir.x };
    Vector2 ponta = Vector2Add(p, Vector2Scale(dir, s));
    Vector2 a = Vector2Add(Vector2Subtract(p, Vector2Scale(dir, s)), Vector2Scale(perp, s));
    Vector2 b = Vector2Subtract(Vector2Subtract(p, Vector2Scale(dir, s)), Vector2Scale(perp, s));
    DrawLineEx(a, ponta, espessura, c);
    DrawLineEx(b, ponta, espessura, c);
}

// c = centro do console; destino = o que a placa anuncia; alvoSeta = a porta (x < 0: sem trilha)
void DesenharTerminalAtalho(Vector2 c, const char *destino, Vector2 alvoSeta, bool liberado, float t)
{
    Color cor = liberado ? COR_ATALHO : (Color){ 130, 130, 140, 255 };
    float pulso = 0.5f + 0.5f * sinf(t * 4.0f);

    // Trilha de setas até a porta (o caminho que o atalho "pula")
    if (liberado && alvoSeta.x >= 0.0f) {
        Vector2 d = Vector2Subtract(alvoSeta, c);
        float l = Vector2Length(d);
        if (l > 40.0f) {
            Vector2 dir = Vector2Scale(d, 1.0f / l);
            for (int k = 0; k < 6; k++) {
                float s = fmodf(t * 55.0f + (float)k * (l / 6.0f), l);
                if (s < 26.0f || s > l - 14.0f) continue;
                float a = 0.25f + 0.55f * sinf(s / l * PI);
                DesenharSetaAtalho(Vector2Add(c, Vector2Scale(dir, s)), dir, 5.0f, 2.0f, Fade(cor, a));
            }
        }
    }

    // Brilho no chão
    DrawEllipse((int)c.x, (int)c.y + 20, 36, 10, Fade(cor, liberado ? 0.10f + 0.10f * pulso : 0.06f));

    // Base com faixas de aviso (amarelo e preto)
    for (int i = 0; i < 8; i++)
        DrawRectangle((int)c.x - 20 + i * 5, (int)c.y + 8, 5, 8, (i % 2 == 0) ? Fade(cor, 0.9f) : (Color){ 20, 16, 10, 255 });

    // Corpo e tela
    DrawRectangle((int)c.x - 19, (int)c.y - 24, 38, 32, (Color){ 40, 30, 14, 255 });
    DrawRectangleLinesEx((Rectangle){ c.x - 19, c.y - 24, 38, 32 }, 2, cor);
    Rectangle tela = { c.x - 14, c.y - 19, 28, 20 };
    DrawRectangleRec(tela, liberado ? Fade(cor, 0.18f + 0.10f * pulso) : (Color){ 30, 30, 36, 255 });
    if (liberado) {
        float desloc = fmodf(t * 18.0f, 10.0f);
        for (int k = 0; k < 2; k++)
            DesenharSetaAtalho((Vector2){ tela.x + 7 + desloc + k * 8, tela.y + 10 }, (Vector2){ 1, 0 }, 3.5f, 2.0f, cor);
    } else {
        // Cadeado
        DrawRectangle((int)c.x - 5, (int)c.y - 10, 10, 8, cor);
        DrawRing((Vector2){ c.x, c.y - 10 }, 3, 5, 180, 360, 10, cor);
    }

    // Placa "ATALHO >>" com o destino (o jogador reconhece de longe)
    float bob = sinf(t * 2.5f) * 2.0f;
    const char *tag = liberado ? "ATALHO" : "BLOQUEADO";
    int wt = MeasureText(tag, 10) + 26;
    int wd = MeasureText(destino, 10) + 10;
    int yt = (int)(c.y - 50 + bob);
    int metade = ((wt > wd) ? wt : wd) / 2;
    float cx = Clamp(c.x, 44.0f + (float)metade, 756.0f - (float)metade);     // a placa não sai da sala
    // Texto escuro na placa clara; se a placa estiver apagada (escura), o texto fica claro (contraste)
    bool placaClara = (0.299f * cor.r + 0.587f * cor.g + 0.114f * cor.b) * cor.a / 255.0f > 140.0f;
    DrawRectangle((int)cx - wt / 2, yt, wt, LinhaTexto(14), cor);
    DrawText(tag, (int)cx - wt / 2 + 5, yt + 1, 10, placaClara ? (Color){ 30, 20, 0, 255 } : WHITE);
    if (liberado) {
        DesenharSetaAtalho((Vector2){ cx + wt / 2.0f - 13, (float)yt + 7 }, (Vector2){ 1, 0 }, 3.0f, 2.0f, (Color){ 30, 20, 0, 255 });
        DesenharSetaAtalho((Vector2){ cx + wt / 2.0f - 7, (float)yt + 7 }, (Vector2){ 1, 0 }, 3.0f, 2.0f, (Color){ 30, 20, 0, 255 });
    }
    DrawRectangle((int)cx - wd / 2, yt + 14, wd, 13, Fade(BLACK, 0.75f));
    DrawText(destino, (int)cx - wd / 2 + 5, yt + 16, 10, cor);
}

// Miniatura do chefe no card: as rotinas de desenho do próprio jogo, numa câmera reduzida
static void DesenharMiniaturaChefe(int chefe, Vector2 centro, float tempo)
{
    static const float zoom[NUM_CHEFES_SELETOR] = { 1.5f, 1.5f, 1.05f, 0.85f, 0.72f };
    Camera2D cam = { 0 };
    cam.offset = centro;
    cam.target = (Vector2){ 0, 0 };
    cam.zoom = zoom[chefe];

    BeginMode2D(cam);
    if (chefe <= CHEFE_SEL_JANELA) {
        Inimigo e;
        memset(&e, 0, sizeof(e));
        e.ativo = true;
        e.tipo = (chefe == CHEFE_SEL_SENTINELA) ? INIMIGO_SENTINELA : ((chefe == CHEFE_SEL_RETRO) ? INIMIGO_RETRO : INIMIGO_JANELA);
        e.raio = DEF_INIMIGOS[e.tipo].raio;
        e.hp = e.hpMax = 1.0f;
        e.alvo = (Vector2){ 0, 1 };
        e.foco = 0.6f;
        e.anim = tempo;
        if (chefe == CHEFE_SEL_SENTINELA)   DesenharSentinela(&e, 1.0f, tempo);
        else if (chefe == CHEFE_SEL_RETRO)  DesenharRetro(&e, 1.0f, tempo);
        else                                DesenharJanelaChefe(&e, 1.0f, tempo);
    } else if (chefe == CHEFE_SEL_SILOGISTA) {
        static Silogista s;
        memset(&s, 0, sizeof(s));
        s.ativo = true;
        s.modoBH = SILOGISTA_BH;
        s.etapa = SIL_MONTAGEM;
        s.hp = SIL_HP;
        s.carregando = -1;
        DesenharSilogista(&s, (Vector2){ 0, 200 }, tempo);
    } else {
        static ChefeRogue c;
        memset(&c, 0, sizeof(c));
        c.ativo = true;
        c.hp = CHEFE_HP;
        c.fase = 1;
        DesenharAlucinador(&c, (Vector2){ 0, 200 }, tempo);
    }
    EndMode2D();
}

static void DesenharSeletorChefes(const Biblioteca *b, float tempo)
{
    Vector2 m = GetMousePosition();

    DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, 0.78f));
    Rectangle painel = { 16, 56, 768, 448 };
    DrawRectangleRec(painel, (Color){ 18, 14, 10, 255 });
    DrawRectangleLinesEx(painel, 3, COR_ATALHO);
    for (int i = 0; i < 16; i++)       // faixa de aviso no topo do painel
        DrawRectangle((int)painel.x + 3 + i * 48, (int)painel.y + 3, 24, 4, Fade(COR_ATALHO, 0.6f));

    DrawText("SELETOR DE CHEFES", 36, 70, 22, COR_ATALHO);
    int xt = 36 + MeasureText("SELETOR DE CHEFES", 22) + 14;
    DesenharSetaAtalho((Vector2){ (float)xt, 81 }, (Vector2){ 1, 0 }, 5.0f, 3.0f, COR_ATALHO);
    DesenharSetaAtalho((Vector2){ (float)xt + 12, 81 }, (Vector2){ 1, 0 }, 5.0f, 3.0f, COR_ATALHO);
    DrawText("CONFRONTO DIRETO, SEM O MAPA DA MISSÃO", xt + 26, 76, 12, Fade(COR_ATALHO, 0.8f));
    DrawText("Só com o Disparador de Dados, sem itens nem melhorias. Vencendo ou perdendo, volta ao átrio.",
             36, 98, 10, LIGHTGRAY);      // cabe na tela também com o TEXTO GRANDE

    for (int i = 0; i < NUM_CHEFES_SELETOR; i++) {
        Rectangle r = RogueRetCardSeletor(i, b->seletorFoco);
        bool foco = (i == b->seletorFoco);
        bool hover = CheckCollisionPointRec(m, r);
        Color cor = CORES_CHEFES_SELETOR[i];
        bool vencido = RogueChefeSeletorVencido(b, i);

        if (foco) DrawRectangle((int)r.x - 4, (int)r.y - 4, (int)r.width + 8, (int)r.height + 8, Fade(COR_ATALHO, 0.18f + 0.10f * sinf(tempo * 5.0f)));
        DrawRectangleRec(r, foco ? (Color){ 34, 26, 18, 255 } : (Color){ 24, 20, 16, 255 });
        DrawRectangleLinesEx(r, foco ? 3.0f : (hover ? 2.0f : 1.0f), foco ? COR_ATALHO : (hover ? Fade(COR_ATALHO, 0.8f) : Fade(cor, 0.5f)));

        // Miniatura
        Rectangle mini = { r.x + 8, r.y + 8, r.width - 16, 108 };
        DrawRectangleRec(mini, (Color){ 12, 10, 22, 255 });
        DrawRectangleLinesEx(mini, 1, Fade(cor, 0.5f));
        DesenharMiniaturaChefe(i, (Vector2){ mini.x + mini.width / 2, mini.y + mini.height / 2 + 4 }, tempo);
        if (!foco) DrawRectangleRec(mini, Fade(BLACK, 0.35f));

        // Tipo, nome e descrição
        int y = (int)(mini.y + mini.height) + 8;
        const char *tipo = TIPOS_CHEFES_SELETOR[i];
        DrawRectangle((int)r.x + 8, y, MeasureText(tipo, 10) + 8, 14, Fade(cor, 0.85f));
        DrawText(tipo, (int)r.x + 12, y + 2, 10, BLACK);
        y += 20;
        char linhas[256];
        int fn = 14;                                // nomes longos numa palavra só (RETROPROPAGADOR) diminuem
        while (fn > 10 && MeasureText(NOMES_CHEFES_SELETOR[i], fn) > (int)r.width - 16 && strchr(NOMES_CHEFES_SELETOR[i], ' ') == NULL) fn--;
        QuebrarTexto(NOMES_CHEFES_SELETOR[i], (int)r.width - 16, fn, linhas, (int)sizeof(linhas));
        y += DesenharTextoLinhas(linhas, (int)r.x + 8, y, fn, fn + 2, foco ? WHITE : LIGHTGRAY) * (fn + 2) + 4;
        QuebrarTexto(RogueDescricaoChefeSeletor(i), (int)r.width - 16, 10, linhas, (int)sizeof(linhas));
        DesenharTextoLinhas(linhas, (int)r.x + 8, y, 10, 13, foco ? (Color){ 220, 215, 200, 255 } : GRAY);

        // Rodapé: vencido e recompensa
        int yb = (int)(r.y + r.height) - 34;
        if (vencido) {
            DrawRectangle((int)r.x + 8, yb, 62, 14, Fade(LIME, 0.85f));
            DrawText("VENCIDO", (int)r.x + 12, yb + 2, 10, BLACK);
        }
        {   // versão curta se não couber no cartão (texto grande)
            const char *premio = vencido ? "treino: sem recompensa" : "1ª vitória: recompensa";
            if (MeasureText(premio, 10) > (int)r.width - 12) premio = vencido ? "treino: sem prêmio" : "1ª vitória: prêmio";
            DrawText(premio, (int)r.x + 8, yb + 18, 10, vencido ? GRAY : GOLD);
        }
    }

    // Botão de voltar
    Rectangle fechar = RogueRetFecharSeletor();
    bool sobre = CheckCollisionPointRec(m, fechar);
    DrawRectangleRec(fechar, sobre ? Fade(COR_ATALHO, 0.35f) : Fade(BLACK, 0.6f));
    DrawRectangleLinesEx(fechar, sobre ? 2.0f : 1.0f, COR_ATALHO);
    const char *txt = "VOLTAR AO LOBBY (ESC)";
    DrawText(txt, (int)(fechar.x + fechar.width / 2) - MeasureText(txt, 12) / 2, (int)fechar.y + 9, 12, sobre ? WHITE : COR_ATALHO);

    const char *ajuda = "A/D ou SETAS: escolher     ENTER ou CLIQUE: lutar     ESC: voltar ao lobby";
    DrawText(ajuda, LARGURA_TELA / 2 - MeasureText(ajuda, 10) / 2, 486, 10, GRAY);
}

// Dicas da PRIMEIRA missão (salas 1 e 2): a tecla em destaque no alto, uma de cada vez
static void DesenharDicasPrimeiraMissao(const Biblioteca *b, float tempo)
{
    if (b->perm->tentativas > 1 || b->indiceRota < 0 || b->indiceRota > 1 || b->tipoSala != SALA_COMBATE)
        return;
    if (b->escolhaAberta || b->morreu || b->luciMsgTempo > 0.0f)
        return;

    const char *tecla = NULL, *acao = NULL;
    if (b->limpa && b->recompensaAtiva)  { tecla = "E";      acao = "pegar a recompensa"; }
    else if (!b->limpa && b->tempoSalaAtual < 7.0f)  { tecla = "CLIQUE"; acao = "atirar: mire com o mouse"; }
    else if (!b->limpa && b->tempoSalaAtual < 14.0f) { tecla = "ESPAÇO"; acao = "esquivar: você atravessa os ataques"; }
    if (tecla == NULL)
        return;

    int wt = MeasureText(tecla, 16) + 18;
    int wa = MeasureText(acao, 14);
    int total = wt + 10 + wa;
    int kx = LARGURA_TELA / 2 - total / 2, ky = 78;                     // abaixo da faixa do objetivo
    DrawRectangle(kx - 8, ky - 4, total + 16, 30, Fade(BLACK, 0.75f));
    DrawRectangle(kx, ky, wt, 22, (((int)(tempo * 3.0f)) % 2 == 0) ? GOLD : (Color){ 235, 235, 245, 255 });
    DrawRectangleLines(kx, ky, wt, 22, (Color){ 0, 228, 255, 255 });
    DrawText(tecla, kx + 9, ky + 3, 16, BLACK);
    DrawText(acao, kx + wt + 10, ky + 4, 14, WHITE);
}

// MIRA DO MOUSE (combate da Biblioteca): fácil de ver em qualquer chão.
//   • contorno escuro + cor da arma (cor muito escura vira azul claro), ponto central;
//   • os traços "abrem" no tiro e voltam a fechar (recuo);
//   • sobre um inimigo ela TRAVA: fica vermelha, menor, com cantos girando;
//   • Vetor: arco da carga em volta; Fluxo: arco do calor (pisca vermelho superaquecida);
//   • uma setinha em volta do Thomas aponta para a mira (para não perder a direção do tiro).
static void DesenharLinhaMira(Vector2 a, Vector2 b, Color cor)
{
    DrawLineEx(a, b, 4.0f, Fade(BLACK, 0.75f));
    DrawLineEx(a, b, 2.0f, cor);
}

static void DesenharMiraMouse(const Biblioteca *b, Camera2D cam, float tempo)
{
    const JogadorRogue *j = &b->j;
    Vector2 m = GetMousePosition();
    Vector2 mundo = GetScreenToWorld2D(m, cam);
    TipoArma arma = RogueArmaAtual(j);
    Color cor = DEF_ARMAS[arma].cor;
    if ((int)cor.r + (int)cor.g + (int)cor.b < 260) cor = SKYBLUE;

    bool sobre = false;
    for (int k = 0; k < MAX_INIMIGOS && !sobre; k++) {
        const Inimigo *e = &b->ini[k];
        if (e->ativo && e->surgindo <= 0.0f && Vector2Distance(mundo, e->pos) <= e->raio + 8.0f)
            sobre = true;
    }

    // Chefes (ficam fora da lista de inimigos): os mesmos alvos e raios que os tiros usam
    if (!sobre && b->chefe.ativo && Vector2Distance(mundo, b->chefe.pos) <= CHEFE_RAIO + 8.0f)
        sobre = true;

    const LuciChefe *l = &b->luci;
    if (!sobre && l->ativo && l->apresentado && l->morrendo <= 0.0f) {
        if (Vector2Distance(mundo, l->pos) <= LUCI_RAIO + 8.0f)
            sobre = true;
        if (l->fase == LUCI_SUB_CONTRADICAO)                 // as cópias também travam (a mira não entrega qual é a real)
            for (int k = 0; k < 3 && !sobre; k++)
                if (Vector2Distance(mundo, l->copia[k]) <= LUCI_RAIO + 8.0f)
                    sobre = true;
    }

    const Silogista *s = &b->silogista;
    if (!sobre && s->ativo && s->apresentado && s->morrendo <= 0.0f) {
        if (Vector2Distance(mundo, s->pos) <= SIL_RAIO + 8.0f)
            sobre = true;
        if (!s->modoBH && s->etapa == SIL_SOFISMA)          // peças do sofisma
            for (int k = 0; k < NUM_ENCAIXES && !sobre; k++)
                if (Vector2Distance(mundo, POS_ENCAIXES[k]) <= 28.0f + 6.0f)
                    sobre = true;
        if (!s->modoBH && s->etapa == SIL_CIRCUITO)         // elementos do circuito
            for (int k = 0; k < SilNumElementos() && !sobre; k++)
                if (Vector2Distance(mundo, SilPosElemento(k)) <= 24.0f + 6.0f)
                    sobre = true;
        if (s->modoBH && s->etapa != SIL_VULNERAVEL)        // conclusões que dá para rebater
            for (int k = 0; k < MAX_PROJETEIS_SILOGISTA && !sobre; k++) {
                const ProjetilSilogista *q = &s->bh.projeteis[k];
                if (q->ativo && q->surgindo <= 0.0f && Vector2Distance(mundo, q->posicao) <= q->raio + 6.0f)
                    sobre = true;
            }
    }
    Color corMira = sobre ? (Color){ 255, 70, 85, 255 } : cor;

    // Setinha em volta do Thomas
    Vector2 pj = GetWorldToScreen2D(j->pos, cam);
    Vector2 d = Vector2Subtract(m, pj);
    if (Vector2Length(d) > 48.0f) {
        d = Vector2Normalize(d);
        Vector2 n = { -d.y, d.x };
        Vector2 ponta = Vector2Add(pj, Vector2Scale(d, 36.0f));
        Vector2 b1 = Vector2Add(Vector2Add(pj, Vector2Scale(d, 27.0f)), Vector2Scale(n, 6.0f));
        Vector2 b2 = Vector2Add(Vector2Add(pj, Vector2Scale(d, 27.0f)), Vector2Scale(n, -6.0f));
        DesenharTrianguloB2(Vector2Add(ponta, Vector2Scale(d, 2.0f)), Vector2Add(b1, Vector2Scale(n, 2.0f)), Vector2Add(b2, Vector2Scale(n, -2.0f)), Fade(BLACK, 0.55f));
        DesenharTrianguloB2(ponta, b1, b2, Fade(corMira, 0.85f));
    }

    // Arcos de carga (Vetor) e de calor (Fluxo)
    if (arma == ARMA_VETOR && j->carga > 0.0f) {
        float k = Clamp(j->carga / VETOR_CARGA_MAX, 0.0f, 1.0f);
        DrawRing(m, 17.0f, 21.0f, 0.0f, 360.0f, 36, Fade(BLACK, 0.55f));
        DrawRing(m, 18.0f, 20.0f, -90.0f, -90.0f + 360.0f * k, 36, (k >= 0.98f) ? WHITE : GOLD);
    }
    if (arma == ARMA_FLUXO && (j->calor > 0.0f || j->superaquecida)) {
        float k = Clamp(j->calor, 0.0f, 1.0f);
        Color corCalor = j->superaquecida ? Fade(RED, 0.5f + 0.5f * sinf(tempo * 18.0f))
                                          : (Color){ 255, (unsigned char)(220 - 170 * k), 40, 255 };
        DrawRing(m, 17.0f, 21.0f, 0.0f, 360.0f, 36, Fade(BLACK, 0.55f));
        DrawRing(m, 18.0f, 20.0f, -90.0f, -90.0f + 360.0f * (j->superaquecida ? 1.0f : k), 36, corCalor);
    }

    // Anel + traços (abrem com o recuo do tiro)
    float recuo = Clamp(j->cronoTiro / 0.25f, 0.0f, 1.0f);
    float raio = sobre ? 8.0f : 10.0f;
    DrawRing(m, raio - 2.0f, raio + 2.0f, 0.0f, 360.0f, 32, Fade(BLACK, 0.7f));
    DrawRing(m, raio - 1.0f, raio + 1.0f, 0.0f, 360.0f, 32, corMira);
    float de = raio + 3.0f + 4.0f * recuo, ate = de + 7.0f;
    for (int k = 0; k < 4; k++) {
        float a = (float)k * PI * 0.5f;
        Vector2 u = { cosf(a), sinf(a) };
        DesenharLinhaMira(Vector2Add(m, Vector2Scale(u, de)), Vector2Add(m, Vector2Scale(u, ate)), corMira);
    }

    // Travada num inimigo: quatro cantos girando em volta
    if (sobre) {
        float giro = tempo * 3.0f;
        for (int k = 0; k < 4; k++) {
            float a = giro + (float)k * PI * 0.5f + PI * 0.25f;
            Vector2 c = Vector2Add(m, (Vector2){ cosf(a) * 21.0f, sinf(a) * 21.0f });
            Vector2 t1 = { cosf(a + PI * 0.75f), sinf(a + PI * 0.75f) };
            Vector2 t2 = { cosf(a - PI * 0.75f), sinf(a - PI * 0.75f) };
            DesenharLinhaMira(c, Vector2Add(c, Vector2Scale(t1, 6.0f)), corMira);
            DesenharLinhaMira(c, Vector2Add(c, Vector2Scale(t2, 6.0f)), corMira);
        }
    }

    // Ponto central
    DrawCircleV(m, 3.0f, Fade(BLACK, 0.8f));
    DrawCircleV(m, 1.6f, WHITE);
}

void RogueDesenhar(const Biblioteca *b, const SpriteDirecao sprites[NUM_DIRECOES], float anguloLuci,
                          float tempo, const char *nickname, int pontos)
{
    const JogadorRogue *j = &b->j;
    bool atrio = (b->tipoSala == SALA_ATRIO);
    bool semArmaAinda = (b->tipoSala == SALA_MESTRE && b->mestre.etapa == MT_ARMA);   // fim do Bloco 1: a arma ainda está no pedestal

    CarregarIconesBiblioteca();         // sprites opcionais dos ícones das salas (só na 1ª vez)
    ClearBackground((Color){ 8, 6, 18, 255 });

    // A tela só treme quando o jogador toma dano
    Camera2D cam = { 0 };
    cam.zoom = 1.0f;
    if (b->tremor > 0.0f) {
        int f = (b->tremorForte > 0.0f) ? (int)PARRY_TREMOR : 4;      // o parry treme mais
        cam.offset = (Vector2){ (float)GetRandomValue(-f, f), (float)GetRandomValue(-f, f) };
    }

    // Parry: zoom rápido no ponto do choque
    if (b->zoomImpacto > 0.0f) {
        float k = b->zoomImpacto / PARRY_ZOOM;
        cam.target = b->focoZoom;
        cam.offset = Vector2Add(cam.offset, b->focoZoom);
        cam.zoom = 1.0f + 0.14f * k * k;
    }

    BeginMode2D(cam);

    // Luta final da Luci: a arte do mapa roxo no lugar do chão, da grade, das estantes e da borda
    bool mapaLuci = (b->tipoSala == SALA_LUCI && texMapaLuci.id > 0);

    if (mapaLuci) {

        DrawTexture(texMapaLuci, (int)MAPA_LUCI_X, (int)MAPA_LUCI_Y, WHITE);

    } else {

        // Chão da biblioteca: ladrilhos (no átrio, um tapete no caminho da porta)
        DrawRectangleRec(ArenaRogue(), atrio ? (Color){ 22, 18, 38, 255 } : (Color){ 18, 15, 30, 255 });

        for (int x = (int)ArenaRogue().x; x < ArenaRogue().x + ArenaRogue().width; x += 40)
            DrawLine(x, (int)ArenaRogue().y, x, (int)(ArenaRogue().y + ArenaRogue().height), Fade(PURPLE, 0.10f));
        for (int y = (int)ArenaRogue().y; y < ArenaRogue().y + ArenaRogue().height; y += 40)
            DrawLine((int)ArenaRogue().x, y, (int)(ArenaRogue().x + ArenaRogue().width), y, Fade(PURPLE, 0.10f));
    }

    if (atrio)
        DrawRectangle(370, 74, 60, 456, Fade(PURPLE, 0.18f));

    if (b->tipoSala == SALA_SILOGISTA)
        DesenharChaoCamara(tempo);

    if (RogueNoBloco3(b))
        DesenharChaoNucleo(b, tempo);

    // Prateleira decorativa na parede de cima e a borda (a arte da luta final já tem as suas paredes)
    if (!mapaLuci) {
        DesenharEstante((Rectangle){ ArenaRogue().x, 42, 320, 16 }, tempo);
        DesenharEstante((Rectangle){ 440, 42, 320, 16 }, tempo);

        DrawRectangleLinesEx(ArenaRogue(), 4, Fade(PURPLE, 0.6f));
    }

    // Estantes (no átrio, o Byte e o Byteco também ocupam lugar, mas são desenhados à parte)
    for (int i = 0; i < b->numObstFixos; i++)        // as barreiras das armadilhas são desenhadas à parte
        if (!atrio || i < 2)
            DesenharEstante(b->obst[i], tempo);

    // Portas
    if (atrio) {
        DesenharPortaRogue(PORTA_PROXIMA_ROGUE, true, "BIBLIOTECA (MISSÃO)", tempo);
        DesenharPortaRogue(PORTA_SAIDA_ROGUE, true, "CORREDOR", tempo);
        if (b->perm->bloco3Liberado)
            DesenharPortaNucleoAtrio(b, tempo);
    } else if (b->tipoSala == SALA_TRANSICAO) {
        DesenharPortaRogue(PORTA_PROXIMA_ROGUE, b->limpa, "NÚCLEO", tempo);
        for (int i = 0; i < 3; i++) {
            DesenharTerminal(POS_REGISTROS[i], (b->registrosLidos & (1 << i)) != 0, tempo);
            const char *nome = TextFormat("REGISTRO %d", i + 1);
            DrawText(nome, (int)POS_REGISTROS[i].x - MeasureText(nome, 10) / 2, (int)POS_REGISTROS[i].y + 26, 10, MAGENTA);
        }
        if (b->cenaLuciTipo == 1) DesenharCenaSeparacao(b, tempo, false);       // cena: ela sai de perto do Thomas e entra no núcleo
        // Depois da cena a sala fica vazia: a Luci já está lá dentro, esperando na arena
    } else if (b->tipoSala == SALA_LUCI) {
        DesenharNucleoInstavel(b, tempo);
        DesenharAvisoMetade(b, tempo);
        DesenharContextoLuci(b, tempo);
    } else if (b->noAtual >= 0) {
        // Reformulação: as portas da missão (cada uma mostra o tipo da sala e a recompensa)
        DesenharParedesMissao(b, tempo);
        for (int k = 0; k < b->numPortas; k++)
            DesenharPortaMissao(b, &b->portas[k], tempo);
    }

    // Personagens e objetos de cada tipo de sala
    if (atrio) {
        DesenharProfessorByte(POS_BYTE, tempo);
        DesenharByteco(POS_BYTECO, tempo);
        DesenharPulpitoEnciclopedia(POS_ENCICLOPEDIA, tempo);
        // Terminais de atalho: o seletor de chefes (trilha até a porta de cima) e o Bloco 3 (até a porta da direita)
        DesenharTerminalAtalho(POS_TERMINAL_SELETOR, b->perm->byteVisto ? "SELETOR DE CHEFES" : "FALE COM O PROF. BYTE",
                               CentroRetangulo(PORTA_PROXIMA_ROGUE), b->perm->byteVisto, tempo);
        if (b->perm->byteVisto && b->perm->bytecoVisto && b->perm->tentativas == 0) {     // primeira visita: a Luci mostra a porta
            Vector2 cp = CentroRetangulo(PORTA_PROXIMA_ROGUE);
            float balanco = sinf(tempo * 5.0f) * 4.0f;
            DrawPoly((Vector2){ cp.x, cp.y + 26.0f + balanco }, 3, 9.0f, 90.0f, Fade(GOLD, 0.9f));
            DrawText("MISSÃO", (int)cp.x - MeasureText("MISSÃO", 10) / 2, (int)cp.y + 38 + (int)balanco, 10, GOLD);
        }
        DesenharTerminalAtalho(POS_TERMINAL_BLOCO3, "IR AO BLOCO 3",
                               b->perm->bloco3Liberado ? CentroRetangulo(PORTA_BLOCO3_ROGUE) : (Vector2){ -1, -1 }, true, tempo);
    } else if (b->tipoSala == SALA_EDUCATIVA) {
        DesenharTerminal(POS_TERMINAL, b->terminalLido, tempo);
    } else if (b->tipoSala == SALA_RECOMPENSA) {
        for (int i = 0; i < NUM_PEDESTAIS; i++)
        {
            bool arma = b->armaPedestal[i] != ARMA_DISPARADOR;
            DesenharPedestal(POS_PEDESTAIS[i], b->pedestal[i], !b->recompensaPega && !arma, tempo);
            if (arma && !b->recompensaPega) {
                Vector2 icone = { POS_PEDESTAIS[i].x, POS_PEDESTAIS[i].y - 10 + sinf(tempo * 3.0f) * 3.0f };
                DrawCircleV(icone, 18, Fade(DEF_ARMAS[b->armaPedestal[i]].cor, 0.15f));
                DesenharIconeArma(b->armaPedestal[i], icone, 1.2f, tempo);
            }
        }
    } else if (b->tipoSala == SALA_SILOGISTA) {
        // Câmara da Inferência: o mecanismo (montagem e sofisma) ou o circuito, que fica aceso depois
        const Silogista *s = &b->silogista;

        if (s->modoBH) {
            if (s->ativo) DesenharSimbolosSilogistaBH(b, tempo);       // versão bullet-hell
        }
        else if (s->etapa >= SIL_CIRCUITO)
            DesenharCircuito(s, tempo);
        else if (s->ativo)
            DesenharMecanismoSilogismo(s, tempo);

        if (s->ativo && s->etapa == SIL_MONTAGEM && !s->modoBH)
            for (int k = 0; k < NUM_ENCAIXES; k++)
                if (SilPecaNoChao(s, k))
                    DesenharPecaSilogismo(s->posPeca[k], s->letraPeca[k], (Color){ 0, 228, 255, 255 }, tempo);

        if (s->ativo)
            DesenharAtaquesSilogista(s, tempo);
    }

    // Reformulação: armadilhas, poças da build e objetos das salas novas
    DesenharArmadilhas(b, tempo);
    DesenharAreas(b);
    DesenharTornado(b);
    DesenharObjetosSala(b, tempo);

    // Coisas no chão
    for (int i = 0; i < MAX_COLETAVEIS; i++) {
        const Coletavel *c = &b->col[i];
        if (!c->ativo) continue;
        if (c->fragmento) DesenharFragmentoConhecimento(c->pos, c->anim);
        else {
            if (c->item >= NUM_ITENS_LOJA)
                DrawRing(c->pos, 13, 15, tempo * 200.0f, tempo * 200.0f + 240.0f, 12, CORES_RARIDADE[ITENS[c->item].raridade]);
            DesenharIconeItem(c->item, c->pos, 0.8f, tempo);
        }
    }

    // Pulso de Integridade: onda de choque (abre rápido e desacelera, borda grossa e um clarão no começo)
    if (j->efeitoPulso > 0.0f) {
        float k = 1.0f - j->efeitoPulso / 0.4f, sobra = 1.0f - k;
        float r = RogueRaioPulso(j) * (1.0f - sobra * sobra);
        DrawCircleV(j->pos, r, Fade(SKYBLUE, 0.16f * sobra));
        DrawRing(j->pos, fmaxf(0.0f, r - 2.0f - 5.0f * sobra), r, 0.0f, 360.0f, 48, Fade(SKYBLUE, 0.95f * sobra));
        DrawRing(j->pos, fmaxf(0.0f, r * 0.8f - 1.5f), r * 0.8f, 0.0f, 360.0f, 48, Fade(WHITE, 0.35f * sobra));
        if (k < 0.25f) DrawCircleV(j->pos, 26.0f * (1.0f - k / 0.25f), Fade(WHITE, 0.5f * (1.0f - k / 0.25f)));
    }

    DesenharSinergiasChao(b, tempo);

    for (int i = 0; i < MAX_INIMIGOS; i++)
        if (b->ini[i].ativo)
            DesenharInimigo(&b->ini[i]);

    DesenharSinergiasMarcas(b, tempo);
    DesenharScannerLuci(b, tempo);
    DesenharMarcasAnalise(b);
    DesenharAneis(b);
    DesenharProxy(j, tempo);

    if (b->chefe.ativo)
        DesenharAlucinador(&b->chefe, j->pos, tempo);

    if (b->silogista.ativo)
        DesenharSilogista(&b->silogista, j->pos, tempo);

    if (b->luci.ativo) {
        DesenharLuciChefe(b, tempo);
        DesenharAtaquesLuci(b, tempo);         // Etapa 2: orbes, varredura e a guarda
    }

    if (b->tipoSala == SALA_MESTRE)
        DesenharMestreTreino(b, tempo);

    DesenharSaidaSilogista(b, tempo);
    DesenharLabAcao(b, tempo);
    DesenharAfixosElite(b, tempo);

    // Disparo Dedutivo: P → Q e P sobre quem foi marcado
    for (int i = 0; i < MAX_INIMIGOS; i++)
        if (b->ini[i].ativo)
            DesenharMarcaDedutiva(b->ini[i].marca, b->ini[i].pos, b->ini[i].raio);
    if (b->chefe.ativo)
        DesenharMarcaDedutiva(b->chefe.marca, b->chefe.pos, CHEFE_RAIO);
    if (b->silogista.ativo)
        DesenharMarcaDedutiva(b->silogista.marca, b->silogista.pos, SIL_RAIO + 20);

    // Esquiva: rastro de luz atrás do Thomas (some junto com a esquiva)
    if (!b->morreu && j->tempoEsquiva > 0.0f) {
        float q = j->tempoEsquiva / ESQUIVA_DURACAO;                         // 1 -> 0
        for (int r = 1; r <= 4; r++) {
            Vector2 p = Vector2Subtract(j->pos, Vector2Scale(j->dirEsquiva, 11.0f * (float)r));
            DrawCircleV((Vector2){ p.x, p.y - 8.0f }, 11.0f - 1.5f * (float)r, Fade(SKYBLUE, 0.28f * q / (float)r));
        }
        Vector2 n = { -j->dirEsquiva.y, j->dirEsquiva.x };
        for (int r = -1; r <= 1; r += 2) {                                   // duas linhas de vento
            Vector2 a = Vector2Add(j->pos, Vector2Scale(n, 9.0f * (float)r));
            DrawLineEx(a, Vector2Subtract(a, Vector2Scale(j->dirEsquiva, 40.0f)), 2.0f, Fade(WHITE, 0.45f * q));
        }
    }

    // Protagonista (pisca enquanto está invulnerável depois do dano)
    if (!b->morreu) {

        bool pisca = j->invulneravel > 0.0f && j->tempoEsquiva <= 0.0f && ((int)(j->invulneravel * 12.0f)) % 2 == 0;

        if (!pisca) {
            DesenharJogadorComLuci(sprites, j->pos, j->direcao, j->anim.quadro, !RogueNoBloco3(b), anguloLuci);   // no Bloco 3 ela se solta
            if (b->cenaLuciTipo == 1) DesenharCenaSeparacao(b, tempo, true);    // cena: a Luci na frente do Thomas

            // A arma equipada na mão, apontada para a mira (guardada no átrio) + o aviso de troca.
            // Na sala da Mestre, antes de pegar a arma no pedestal, o Thomas ainda está de mãos vazias.
            if (!semArmaAinda) DesenharArmaNaMao(b, tempo, !atrio);
        }

        DesenharEfeitosJogador(b, tempo);
        if (b->tipoSala == SALA_LUCI) DesenharGuiaParry(b, tempo);     // Etapa 3: a hora do parry
        if (b->tipoSala == SALA_MESTRE) DesenharDicasMestreMundo(b, tempo);   // tutorial: a tecla na hora de agir

        // Bloco 3: o Escudo Lógico (janela do parry) e o arco do golpe dedutivo
        if (j->parry.emParry) {
            float q = fminf(1.0f, j->parry.tempoJanelaParry / PARRY_JANELA);
            DrawCircleV(j->pos, 26, Fade(WHITE, 0.10f + 0.10f * q));
            DrawRing(j->pos, 22, 27, 0, 360, 32, Fade(GOLD, 0.5f + 0.5f * q));
        }
        if (j->parry.golpeAnim > 0.0f) {
            float q = 1.0f - j->parry.golpeAnim / 0.18f;
            float ang = atan2f(j->parry.dirGolpe.y, j->parry.dirGolpe.x) * RAD2DEG;
            Vector2 c = Vector2Add(j->pos, Vector2Scale(j->parry.dirGolpe, 18.0f));
            DrawRing(c, 26, 36, ang - 80.0f + 120.0f * q, ang - 20.0f + 120.0f * q, 12, Fade(GOLD, 1.0f - q));
        }

        // Fragmento do silogismo na mão: flutua sobre a cabeça
        if (b->silogista.ativo && b->silogista.carregando >= 0)
            DesenharPecaSilogismo((Vector2){ j->pos.x, j->pos.y - 40 }, b->silogista.letraPeca[b->silogista.carregando], YELLOW, tempo);
    }

    // Projéteis: os do jogador em ciano, os inimigos em laranja com contorno (fácil de ver)
    for (int i = 0; i < MAX_PROJETEIS; i++) {

        const Projetil *p = &b->proj[i];

        if (!p->ativo)
            continue;

        if (p->alvoDedutivo != 0) {
            // Conclusão Q do Disparo Dedutivo
            DrawCircleV(p->pos, p->raio + 5, Fade(GOLD, 0.35f));
            DrawCircleV(p->pos, p->raio + 1, GOLD);
            DrawText("Q", (int)p->pos.x - MeasureText("Q", 10) / 2, (int)p->pos.y - 5, 10, (Color){ 60, 40, 0, 255 });
        } else if (p->doJogador) {
            DesenharProjetilJogador(p, tempo);
        } else {
            DesenharProjetilInimigo(p, tempo);
        }
    }

    DesenharRaiosKatana(b);                       // Surto da Katana: os relâmpagos ficam por cima de todos

    for (int i = 0; i < MAX_PARTICULAS_ROGUE; i++) {
        const ParticulaRogue *p = &b->part[i];
        if (p->vida > 0.0f)
            DrawRectangleV((Vector2){ p->pos.x - p->tam / 2, p->pos.y - p->tam / 2 }, (Vector2){ p->tam, p->tam },
                           Fade(p->cor, p->vida / p->vidaMax));
    }

    // Tecla E sobre o que dá para usar
    int pedestal = -1;
    AlvoRogue alvo = RogueAlvoProximo(b, &pedestal);

    int portaK = -1;
    PortaInteracao porta = RoguePortaProxima(b, &portaK);

    if (!b->lojaAberta && !b->enciclopediaAberta && !b->desafioAberto && !b->lab.aberto && !b->seletorAberto && !b->morreu) {
        if (porta != PORTA_NENHUMA)     DesenharAvisoPorta(j->pos, (porta == PORTA_ATRIO_CORREDOR) ? "SAIR" : "ENTRAR", tempo);
        else if (alvo == ALVO_ENCICLOPEDIA)  DesenharTeclaE((Vector2){ POS_ENCICLOPEDIA.x, POS_ENCICLOPEDIA.y - 40 }, tempo);
        else if (alvo == ALVO_BYTE)          DesenharTeclaE((Vector2){ POS_BYTE.x, POS_BYTE.y - 48 }, tempo);
        else if (alvo == ALVO_BYTECO)   DesenharTeclaE((Vector2){ POS_BYTECO.x, POS_BYTECO.y - 42 }, tempo);
        else if (alvo == ALVO_TERMINAL) DesenharTeclaE((Vector2){ POS_TERMINAL.x, POS_TERMINAL.y - 36 }, tempo);
        else if (alvo == ALVO_PECA)     DesenharTeclaE((Vector2){ b->silogista.posPeca[pedestal].x, b->silogista.posPeca[pedestal].y - 32 }, tempo);
        else if (alvo == ALVO_ENCAIXE)  DesenharTeclaE((Vector2){ POS_ENCAIXES[pedestal].x + 44, POS_ENCAIXES[pedestal].y - 20 }, tempo);
        else if (alvo == ALVO_REGISTRO) DesenharTeclaE((Vector2){ POS_REGISTROS[pedestal].x, POS_REGISTROS[pedestal].y - 36 }, tempo);
        else if (alvo == ALVO_CONTEXTO) {
            Vector2 c = (pedestal < 0) ? NucleoLuci() : b->luci.posContexto[pedestal];
            DesenharTeclaE((Vector2){ c.x, c.y - 34 }, tempo);
        }
        else if (alvo == ALVO_RECOMPENSA)    DesenharTeclaE((Vector2){ b->recompensaPos.x, b->recompensaPos.y - 40 }, tempo);
        else if (alvo == ALVO_OBJETO_SALA)   DesenharTeclaE((Vector2){ b->posObjetoSala.x, b->posObjetoSala.y - 52 }, tempo);
        else if (alvo == ALVO_LAB)           DesenharTeclaE((Vector2){ b->posObjetoSala.x, b->posObjetoSala.y - 62 }, tempo);
        else if (alvo == ALVO_TERMINAL_SELETOR) DesenharTeclaE((Vector2){ POS_TERMINAL_SELETOR.x + 34, POS_TERMINAL_SELETOR.y - 10 }, tempo);
        else if (alvo == ALVO_TERMINAL_BLOCO3)  DesenharTeclaE((Vector2){ POS_TERMINAL_BLOCO3.x - 34, POS_TERMINAL_BLOCO3.y - 10 }, tempo);
        else if (alvo == ALVO_ARMA_MESTRE)   DesenharTeclaE((Vector2){ POS_ARMA_MESTRE.x, POS_ARMA_MESTRE.y - 52 }, tempo);
        else if (alvo == ALVO_REGISTRO_SALA) DesenharTeclaE((Vector2){ b->posRegistroSala.x, b->posRegistroSala.y - 36 }, tempo);
        else if (alvo == ALVO_PEDESTAL) {
            Vector2 c = POS_PEDESTAIS[pedestal];
            DesenharTeclaE((Vector2){ c.x, c.y - 42 }, tempo);
            TipoArma arma = b->armaPedestal[pedestal];
            const char *nome = (arma != ARMA_DISPARADOR) ? DEF_ARMAS[arma].nome : ITENS[b->pedestal[pedestal]].nome;
            const char *efeito = (arma != ARMA_DISPARADOR) ? TextFormat("%s: %s", NOMES_ALCANCE[DEF_ARMAS[arma].alcanceTipo], DEF_ARMAS[arma].efeito)
                                                           : ITENS[b->pedestal[pedestal]].efeito;
            Raridade rar = (arma != ARMA_DISPARADOR) ? DEF_ARMAS[arma].raridade : ITENS[b->pedestal[pedestal]].raridade;
            DrawText(nome, (int)c.x - MeasureText(nome, 12) / 2, (int)c.y + 26, 12, GOLD);
            DrawText(efeito, (int)c.x - MeasureText(efeito, 10) / 2, (int)c.y + 42, 10, LIGHTGRAY);
            DrawText(NOMES_RARIDADE[rar], (int)c.x - MeasureText(NOMES_RARIDADE[rar], 10) / 2, (int)c.y + 56, 10, CORES_RARIDADE[rar]);
        }
    }

    // Mecanismo corrompido: "SEGURE E" perto de um console
    for (int k = 0; k < b->numMecanismos && !b->morreu; k++)
        if (!b->mecanismoDesligado[k] && Vector2Distance(j->pos, b->posMecanismo[k]) < MECANISMO_RAIO) {
            const char *t = "SEGURE E";
            DrawText(t, (int)b->posMecanismo[k].x - MeasureText(t, 10) / 2, (int)b->posMecanismo[k].y - 32, 10, (Color){ 0, 228, 255, 255 });
        }

    EndMode2D();

    // Mira do mouse (só onde dá para atirar)
    if (!j->miraSetas && !b->morreu && !atrio && !semArmaAinda && b->cenaLuciTipo == 0 && !b->lojaAberta && !b->desafioAberto && !b->escolhaAberta && !b->eventoAberto && !b->mapaAberto && !b->lab.aberto) {
        DesenharMiraMouse(b, cam, tempo);
    }

    // Objetivo no topo
    Objetivo o = { "", false, { 0, 0 } };

    switch (b->tipoSala) {
        case SALA_ATRIO:
            snprintf(o.texto, sizeof(o.texto), "%s", !b->perm->byteVisto     ? "Fale com o Professor Byte"
                                                   : b->perm->bloco3Liberado ? "Uma porta nova se abriu à direita: BLOCO 3"
                                                   : !b->perm->bytecoVisto   ? "Passe no Byteco (loja) e depois entre pela porta de cima: missão"
                                                                             : "Porta de cima: missão. Terminal dourado: seletor de chefes");
            break;
        case SALA_COMBATE:
        case SALA_ELITE: {
            bool selado = false;
            for (int i = 0; i < MAX_INIMIGOS; i++)
                if (b->ini[i].ativo && b->ini[i].selado) selado = true;

            int vivos = RogueInimigosVivos(b);
            if (b->limpa)        snprintf(o.texto, sizeof(o.texto), "%s", b->recompensaAtiva ? "Sala limpa! Pegue a recompensa (E) e escolha uma porta"
                                                                                           : "Sala limpa! Escolha uma porta");
            else if (b->avisoOnda > 0.0f) snprintf(o.texto, sizeof(o.texto), "Prepare-se: onda %d/%d chegando!", b->ondaAtual + 2, b->numOndas);
            else if (b->objetivoSala == OBJC_SOBREVIVER)
                snprintf(o.texto, sizeof(o.texto), "SOBREVIVA aos reforços: %d s", (int)ceilf(fmaxf(0.0f, b->tempoObjetivo)));
            else if (b->objetivoSala == OBJC_NUCLEOS)
                snprintf(o.texto, sizeof(o.texto), "Destrua os NÚCLEOS DE CORRUPÇÃO (%d restantes)", RogueContarTipo(b, INIMIGO_NUCLEO));
            else if (b->objetivoSala == OBJC_PRIORITARIO)
                snprintf(o.texto, sizeof(o.texto), "%s", (b->alvoPrioritario >= 0) ? "Derrube o RETRANSMISSOR para parar os reforços"
                                                                                   : TextFormat("Reforços parados! Elimine o resto (%d)", vivos));
            else if (b->objetivoSala == OBJC_MECANISMO) {
                int feitos = 0;
                for (int k = 0; k < b->numMecanismos; k++) if (b->mecanismoDesligado[k]) feitos++;
                snprintf(o.texto, sizeof(o.texto), "Desligue os consoles corrompidos (segure E): %d/%d", feitos, b->numMecanismos);
            }
            else if (selado)     snprintf(o.texto, sizeof(o.texto), "Chegue perto do Guardião SELADO e quebre o selo");
            else if (b->numOndas > 1) snprintf(o.texto, sizeof(o.texto), "Elimine os dados corrompidos (%d)  -  onda %d/%d", vivos, b->ondaAtual + 1, b->numOndas);
            else                 snprintf(o.texto, sizeof(o.texto), "Elimine os dados corrompidos (%d)", vivos);
        } break;
        case SALA_EDUCATIVA:
            snprintf(o.texto, sizeof(o.texto), "%s", b->terminalLido ? "Escolha uma porta (o arquivo de ameaça é opcional)" : "Leia o terminal de conhecimento");
            break;
        case SALA_RECOMPENSA:
            snprintf(o.texto, sizeof(o.texto), "%s", b->recompensaPega ? "Escolha uma porta" : "Escolha 1 recompensa (E) ou siga em frente");
            break;
        case SALA_EVENTO:
            if (b->eventoCombate && !b->limpa) snprintf(o.texto, sizeof(o.texto), "Estabilize o banco: derrote os elites (%d)", RogueInimigosVivos(b));
            else if (b->recompensaAtiva)      snprintf(o.texto, sizeof(o.texto), "Pegue a recompensa (E) e escolha uma porta");
            else snprintf(o.texto, sizeof(o.texto), "%s", b->eventoUsado ? "Escolha uma porta" : "Examine o evento (E) ou escolha uma porta");
            break;
        case SALA_DESCANSO:
            snprintf(o.texto, sizeof(o.texto), "%s", b->eventoUsado ? "Escolha uma porta" : "Sala de leitura: use a poltrona (E) ou siga");
            break;
        case SALA_DESAFIO:
            if (b->estadoDesafio == 0)      snprintf(o.texto, sizeof(o.texto), "Desafio opcional: use o console (E) ou escolha uma porta");
            else if (b->estadoDesafio == 1 && b->desafioSala == DESAFIO_SALA_ECOS)
                snprintf(o.texto, sizeof(o.texto), "Destrua os ecos: %d/%d  (%d s)", b->contadorDesafio, DESAFIO_ECOS_META, (int)ceilf(fmaxf(0.0f, b->tempoDesafioSala)));
            else if (b->estadoDesafio == 1)
                snprintf(o.texto, sizeof(o.texto), "Sobreviva: %d s  (golpes %d/%d)", (int)ceilf(fmaxf(0.0f, b->tempoDesafioSala)), b->j.golpesSofridos, DESAFIO_INTEG_GOLPES);
            else if (b->estadoDesafio == 2) snprintf(o.texto, sizeof(o.texto), "%s", b->recompensaAtiva ? "Desafio vencido! Pegue a recompensa (E)" : "Desafio vencido! Escolha uma porta");
            else                            snprintf(o.texto, sizeof(o.texto), "Desafio falhou. Escolha uma porta");
            break;
        case SALA_SECRETA:
            snprintf(o.texto, sizeof(o.texto), "%s", b->recompensaAtiva ? "ARQUIVO OCULTO! Pegue a recompensa rara (E)" : "Escolha uma porta");
            break;
        case SALA_ESPECIAL: {
            int esp = RogueEspecialAtual(b);
            if (b->limpa)              snprintf(o.texto, sizeof(o.texto), "%s", b->recompensaAtiva ? "Encontro vencido! Pegue a recompensa (E)" : "Encontro vencido! Escolha uma porta");
            else if (esp == ESP_SENTINELA) snprintf(o.texto, sizeof(o.texto), "SENTINELA: fuja da visão nas estantes e ataque quando ela piscar");
            else if (esp == ESP_RETRO)     snprintf(o.texto, sizeof(o.texto), "RETROPROPAGADOR: atire e MUDE de lugar (o golpe volta às marcas)");
            else                           snprintf(o.texto, sizeof(o.texto), "%s", (b->janela.vulneravel > 0.0f) ? "ESCUDO ESQUECIDO! Ataque o núcleo"
                                                                                                                 : "Golpes no escudo viram RUÍDO: encha a janela");
        } break;
        case SALA_LABORATORIO:
            if (b->lab.emAcao && b->lab.tipo == LAB_NUCLEO)
                snprintf(o.texto, sizeof(o.texto), "TIRO destrói o que NÃO ajuda. O resto deixe chegar ao núcleo (%d/%d)", b->lab.proximo, b->lab.numAcao);
            else if (b->lab.emAcao)
                snprintf(o.texto, sizeof(o.texto), "TIRO bloqueia  -  ENCOSTAR anonimiza  -  deixar passar libera (%d/%d)", b->lab.decisoes, b->lab.numAcao);
            else if (b->lab.concluido)            snprintf(o.texto, sizeof(o.texto), "%s concluído (%d/3). Escolha uma porta", PLACAS_LAB[b->lab.tipo], b->lab.estrelas);
            else                                  snprintf(o.texto, sizeof(o.texto), "%s (opcional): use o console (E) ou siga", PLACAS_LAB[b->lab.tipo]);
            break;
        case SALA_TRANSICAO: {
            int lidos = ((b->registrosLidos & 1) != 0) + ((b->registrosLidos & 2) != 0) + ((b->registrosLidos & 4) != 0);
            if (lidos < 3) snprintf(o.texto, sizeof(o.texto), "Leia os registros de memória (E): %d/3", lidos);
            else           snprintf(o.texto, sizeof(o.texto), "Entre no núcleo (porta de cima)");
        } break;
        case SALA_LUCI: {
            const LuciChefe *l = &b->luci;
            if (!l->ativo || !l->apresentado)  snprintf(o.texto, sizeof(o.texto), "A LUCI");
            else if (l->emRegeneracao && l->etapaRegeneracao == 0)
                snprintf(o.texto, sizeof(o.texto), "%s", (l->faseOculta == 1) ? "A Luci caiu... acabou?" : "ESTÁTICA! O núcleo está explodindo");
            else if (l->emRegeneracao)         snprintf(o.texto, sizeof(o.texto), "ERRO: INTEGRIDADE NÃO ENCONTRADA... REINICIANDO");
            else if (l->atordoada)             snprintf(o.texto, sizeof(o.texto), "ATORDOADA! Chegue perto e ataque (clique): COMBO %d/3", l->combo);
            else if (l->fase == LUCI_SUB_DICA) snprintf(o.texto, sizeof(o.texto), "Confira o aviso vermelho antes de acreditar no lado seguro");
            else if (l->fase == LUCI_SUB_CONTRADICAO)
                snprintf(o.texto, sizeof(o.texto), "%s", (l->janela > 0.0f) ? "ARMADURA ABERTA! Acerte a Luci verdadeira (a que tem sombra)"
                                                                           : "Conteste (C) o que contradiz a sala para abrir a armadura");
            else if (l->faseOculta == 2)       snprintf(o.texto, sizeof(o.texto), "Olhos em VERMELHO = golpe: PARRY (V ou botão direito) no impacto");
            else if (l->exigeContexto && !l->contextoPronto)
                snprintf(o.texto, sizeof(o.texto), "Leve ASSUNTO, OBJETIVO e FORMATO ao núcleo (E) ou dê um PARRY");
            else if (l->padrao3 == 1)          snprintf(o.texto, sizeof(o.texto), "ONDA DE CHOQUE: passe pela BRECHA de luz ou esquive");
            else                               snprintf(o.texto, sizeof(o.texto), "Núcleo instável! PARRY (V) nas investidas, brecha nas ondas, fuja do chão rachado");
        } break;
        case SALA_SILOGISTA: {
            const Silogista *s = &b->silogista;
            if (b->limpa)                          snprintf(o.texto, sizeof(o.texto), "%s", b->saiuSilogista ? "Você saiu da Câmara. Escolha uma porta (E)" : "Câmara vencida! Escolha uma porta");
            else if (!s->ativo || !s->apresentado) snprintf(o.texto, sizeof(o.texto), "O SILOGISTA");
            else if (s->modoBH && s->etapa != SIL_VULNERAVEL)
                snprintf(o.texto, sizeof(o.texto), "Acerte a conclusão VÁLIDA: %d/%d  (SHIFT = foco)", s->bh.acertosRodada, SIL_BH_META);
            else if (s->etapa == SIL_MONTAGEM)     snprintf(o.texto, sizeof(o.texto), "%s", (s->carregando >= 0) ? "Leve o fragmento até P1, P2 ou C (E encaixa)"
                                                                                                               : "Pegue os fragmentos (E) e monte o silogismo");
            else if (s->etapa == SIL_SOFISMA)      snprintf(o.texto, sizeof(o.texto), "Destrua a tiros a peça do argumento que não se sustenta");
            else if (s->etapa == SIL_CIRCUITO)     snprintf(o.texto, sizeof(o.texto), "%s", (s->rodada == 1) ? "Ative as portas lógicas com tiros, na ordem certa"
                                                                                                          : TextFormat("Escolha p e q (tiros) para o núcleo receber %s", s->exigido ? "V" : "F"));
            else                                   snprintf(o.texto, sizeof(o.texto), "Armadura lógica desativada! Derrote o Silogista");
        } break;
        case SALA_MESTRE: {
            const MestreTreino *m = &b->mestre;
            bool vul = MestreVulneravel(m);
            bool golpe = (m->acao == MA_TELEGRAFA || m->acao == MA_INVESTE);
            switch (m->etapa) {
                case MT_ARMA:     snprintf(o.texto, sizeof(o.texto), "Pegue o que brilha no pedestal (E)"); break;
                case MT_TIRO: {
                    int n = 0;
                    for (int a = 0; a < MESTRE_ALVOS; a++) if (!m->alvoVivo[a]) n++;
                    snprintf(o.texto, sizeof(o.texto), "Mire com o MOUSE e CLIQUE (botão esquerdo) nos alvos: %d/%d", n, MESTRE_ALVOS);
                } break;
                case MT_ESQUIVA:  snprintf(o.texto, sizeof(o.texto), "Aperte ESPAÇO para esquivar (impulso rápido): %d/%d", (m->esquivas < MESTRE_ESQUIVAS) ? m->esquivas : MESTRE_ESQUIVAS, MESTRE_ESQUIVAS); break;
                case MT_ABERTURA: snprintf(o.texto, sizeof(o.texto), "%s", vul ? TextFormat("ESCUDO DOURADO! Atire agora (%d/%d)", m->acertosAbertura < MESTRE_ACERTOS_ABERTURA ? m->acertosAbertura : MESTRE_ACERTOS_ABERTURA, MESTRE_ACERTOS_ABERTURA)
                                                                   : "Escudo fechado: esquive e espere a brecha dourada"); break;
                case MT_PARRY:    snprintf(o.texto, sizeof(o.texto), "%s", golpe ? "Brilho VERMELHO: aperte V quando o anel fechar!"
                                                                   : (m->acao == MA_ATORDOADA ? "ATORDOADA! Atire à vontade" : "Espere o golpe e rebata com o PARRY (V)")); break;
                case MT_PULSO:    snprintf(o.texto, sizeof(o.texto), "Aperte Q para soltar o PULSO (gasta EP): %d/1", (m->pulsos < 1) ? m->pulsos : 1); break;
                case MT_MELHORIA: snprintf(o.texto, sizeof(o.texto), "Escolha uma melhoria (treino)"); break;
                case MT_LUTA:     snprintf(o.texto, sizeof(o.texto), "%s", vul ? "BRECHA! Atire na ALUCINAÇÃO MESTRE"
                                                                   : (golpe ? "Golpe vindo: PARRY (V) ou esquive (ESPAÇO)" : "Esquive (ESPAÇO) e espere a brecha")); break;
                default:          snprintf(o.texto, sizeof(o.texto), "A ALUCINAÇÃO MESTRE"); break;
            }
        } break;
        default:
            if (!b->chefe.ativo || !b->chefe.apresentado) snprintf(o.texto, sizeof(o.texto), "O ALUCINADOR");
            else if (b->chefe.fase == 1)              snprintf(o.texto, sizeof(o.texto), "Derrote o ALUCINADOR: desvie e atire!");
            else if (b->chefe.janela > 0.0f)          snprintf(o.texto, sizeof(o.texto), "ESCUDO ABERTO! Ataque o Alucinador!");
            else                                      snprintf(o.texto, sizeof(o.texto), "Responda: C = CONFIÁVEL, F = PROBLEMÁTICA");
            break;
    }

    // Contradição da Luci: o objetivo é reescrito com algo que não bate com a sala (com uma falha de vez em quando)
    if (b->contradicao > 0.0f)
        snprintf(o.texto, sizeof(o.texto), "%s", CONTRADICOES_LUCI[b->textoContradicao]);

    if (b->luciMsgTempo > 0.0f && b->luciMsg[0] != '\0')
        DesenharFalaLuciHUD(b->luciMsg, true, false);   // o recado dela toma o lugar do objetivo por alguns segundos
    else if (b->tipoSala == SALA_LUCI)
        DesenharFalaLuciHUD(o.texto, false, true);      // contra a Luci chefe, quem orienta é o SISTEMA (ela é a inimiga)
    else
        DesenharObjetivoHUD(&o);
    DesenharTarefaLabAcao(b);
    DesenharDicasPrimeiraMissao(b, tempo);

    // Reformulação: barra do tempo (sobreviver / desafio) e a mensagem de sistema corrompido
    {
        float frac = -1.0f;
        if ((b->tipoSala == SALA_COMBATE || b->tipoSala == SALA_ELITE) && !b->limpa && b->objetivoSala == OBJC_SOBREVIVER && b->tempoObjetivoMax > 0.0f)
            frac = b->tempoObjetivo / b->tempoObjetivoMax;
        if (b->tipoSala == SALA_DESAFIO && b->estadoDesafio == 1)
            frac = b->tempoDesafioSala / ((b->desafioSala == DESAFIO_SALA_ECOS) ? DESAFIO_ECOS_TEMPO : DESAFIO_INTEG_TEMPO);
        if (frac >= 0.0f) {
            DrawRectangle(300, 40, 200, 5, Fade(BLACK, 0.7f));
            DrawRectangle(300, 40, (int)(200 * Clamp(frac, 0.0f, 1.0f)), 5, GOLD);
        }

        if (b->mensagemSistema > 0.0f && !b->chefe.ativo && !b->silogista.ativo) {
            float k = fminf(1.0f, b->mensagemSistema / 0.5f);
            int dx = (((int)(tempo * 15.0f)) % 11 == 0) ? GetRandomValue(-3, 3) : 0;
            int w = MeasureText(b->textoSistema, 10);
            DrawRectangle(LARGURA_TELA / 2 - w / 2 - 6, 47, w + 12, 13, Fade(BLACK, 0.6f * k));
            DrawText(b->textoSistema, LARGURA_TELA / 2 - w / 2 + dx, 49, 10, Fade(MAGENTA, k));
        }
    }

    if (b->contradicao > 0.0f && ((int)(tempo * 12.0f)) % 9 == 0) {
        const char *t = TextFormat("OBJETIVO:  %s", o.texto);
        DrawText(t, LARGURA_TELA / 2 - MeasureText(t, 14) / 2 + 2, 18, 14, Fade(MAGENTA, 0.7f));
    }

    if (b->janela.ativo)
        DesenharBarraJanela(b, tempo);      // expansão da progressão: vida e janela de contexto do mini-chefe

    if (b->chefe.ativo && b->chefe.apresentado) {
        DesenharBarraChefe(&b->chefe);
        DesenharFaixaAfirmacao(b, tempo);
    }

    if (b->silogista.ativo && b->silogista.apresentado) {
        DesenharBarraSilogista(&b->silogista);
        DesenharFaixaSilogista(b, tempo);
        if (b->silogista.modoBH)
            DesenharFocoSilogistaBH(b, tempo);
    }

    if (b->luci.ativo && b->cenaLuciTipo == 2)
        DesenharTituloCenaLuci(b, tempo);           // cena: o nome dela e a barra enchendo
    else if (b->luci.ativo && (b->luci.apresentado || b->luci.cenaVista)) {
        DesenharBarraLuci(&b->luci, tempo);
        if (b->luci.apresentado) DesenharFaixaLuci(b, tempo);
    }

    if (b->tipoSala == SALA_MESTRE)
        DesenharHudMestre(b, tempo);

    // Mapa de progressão (canto de cima): o grafo da tentativa; M abre o mapa grande
    if (!atrio && b->noAtual >= 0)
        DesenharMiniMapa(b, tempo);
    else if (b->modoSeletor && !atrio) {
        // Confronto direto: no lugar do mapa, o aviso de que a vitória volta ao átrio
        DrawRectangle(4, 2, 150, 30, Fade(BLACK, 0.6f));
        DrawRectangleLines(4, 2, 150, 30, COR_ATALHO);
        DrawText("SELETOR DE CHEFES", 10, 6, 10, COR_ATALHO);
        DrawText(b->seletorVencido ? "VITÓRIA! Voltando ao átrio..." : "confronto direto", 10, 18, 10, b->seletorVencido ? LIME : LIGHTGRAY);
    }

    // HUD de baixo: vida, energia, recargas, pontos, fragmentos e mochila
    int yHud = 542;

    DrawRectangle(0, yHud - 4, LARGURA_TELA, ALTURA_TELA - yHud + 4, (Color){ 12, 10, 24, 255 });
    DrawLine(0, yHud - 4, LARGURA_TELA, yHud - 4, Fade(PURPLE, 0.6f));

    DrawText(nickname, 14, yHud + 4, 20, WHITE);

    DrawText("HP", 66, yHud + 2, 10, WHITE);
    DrawRectangle(84, yHud, 140, 12, (Color){ 60, 10, 20, 255 });
    DrawRectangle(84, yHud, (int)(140 * j->hp / RogueHpMax(b)), 12, (j->hp > RogueHpMax(b) * 0.3f) ? YELLOW : RED);
    DrawRectangleLines(84, yHud, 140, 12, GRAY);
    DrawText(TextFormat("%d", (int)ceilf(j->hp)), 230, yHud + 1, 10, WHITE);

    DrawText("EP", 66, yHud + 20, 10, WHITE);
    DrawRectangle(84, yHud + 18, 140, 12, (Color){ 10, 30, 50, 255 });
    DrawRectangle(84, yHud + 18, (int)(140 * j->ep / RogueEpMax(b)), 12, SKYBLUE);
    DrawRectangleLines(84, yHud + 18, 140, 12, GRAY);
    {   // armas que gastam EP: uma marca mostra o custo do próximo disparo
        TipoArma atualEp = RogueArmaAtual(j);
        float custoEp = 0.0f;
        if (atualEp == ARMA_RAIO)          custoEp = RAIO_CUSTO_EP * ((RogueUp(j, UP_ARMA_RAIO_SUPER) > 0) ? RAIO_SUPER_FATOR : 1.0f);
        else if (atualEp == ARMA_RAIO_CLASSICO) custoEp = RAIOC_CUSTO_EP * ((RogueUp(j, UP_ARMA_RAIOC_SUPER) > 0) ? RAIOC_SUPER_FATOR : 1.0f);
        if (custoEp > 0.0f) {
            int mx = 84 + (int)(140 * custoEp / RogueEpMax(b));
            DrawLine(mx, yHud + 15, mx, yHud + 33, (j->ep >= custoEp) ? WHITE : ORANGE);
            DrawTriangle((Vector2){ (float)mx - 3, (float)yHud + 14 }, (Vector2){ (float)mx + 3, (float)yHud + 14 }, (Vector2){ (float)mx, (float)yHud + 18 }, (j->ep >= custoEp) ? WHITE : ORANGE);
        }
        if (GetTime() < j->epPiscaAte && ((int)(GetTime() * 14.0) & 1) == 0)
            DrawRectangleLinesEx((Rectangle){ 83, (float)yHud + 17, 142, 14 }, 2, RED);
    }
    DrawText(TextFormat("%d", (int)j->ep), 230, yHud + 19, 10, WHITE);

    // Rótulos centralizados embaixo dos ícones; com o texto grande eles ficam mais estreitos (PULSO / ESQ.)
    // e sobem para não encostar na barra de controles
    DesenharRecargaRogue(278, yHud + 12, j->recargaPulso, PULSO_RECARGA, "Q", SKYBLUE, j->ep >= PULSO_CUSTO_EP);
    DesenharRecargaRogue(324, yHud + 12, j->recargaEsquiva, RogueRecargaEsquiva(j), "ESP", LIME, true);
    {
        const char *rotEsquiva = TextoGrande() ? "ESQ." : "ESQUIVA";
        int yRot = ALTURA_TELA - 21 - LinhaTexto(12);
        DrawText("PULSO", 278 - MeasureText("PULSO", 8) / 2, yRot, 8, LIGHTGRAY);
        DrawText(rotEsquiva, 324 - MeasureText(rotEsquiva, 8) / 2, yRot, 8, LIGHTGRAY);
    }
    if (RogueUp(j, UP_ESQUIVA_DUPLA) > 0)        // Buffer de Esquiva: a 2ª carga
        DrawCircle(340, yHud + 2, 4, (j->recargaEsquiva2 <= 0.0f) ? LIME : DARKGRAY);

    DrawText(TextFormat("PONTOS: %d", pontos), 360, yHud + 2, 14, GOLD);
    DesenharFragmentoConhecimento((Vector2){ 366, (float)yHud + 26 }, tempo);
    DrawText(TextFormat("%d FRAGMENTOS", b->perm->fragmentos), 378, yHud + 21, 10, (Color){ 0, 228, 255, 255 });

    // Selo da Inferência (Disparo Dedutivo), com a recarga
    if (j->seloInferencia) {
        Vector2 c = { 466, (float)yHud + 10 };
        DrawCircleV(c, 9, GOLD);
        if (j->recargaDedutivo > 0.0f)
            DrawCircleSector(c, 9, -90.0f, -90.0f + 360.0f * j->recargaDedutivo / DEDUTIVO_RECARGA, 20, Fade(BLACK, 0.5f));
        DrawCircleLines((int)c.x, (int)c.y, 9, ORANGE);
        DrawText("Q", (int)c.x - MeasureText("Q", 10) / 2, (int)c.y - 5, 10, (Color){ 60, 40, 0, 255 });
        DrawText("SELO", (int)c.x - MeasureText("SELO", 8) / 2, yHud + 22, 8, GOLD);
    }

    if (!(b->tipoSala == SALA_MESTRE && b->mestre.etapa == MT_ARMA))   // ainda sem arma: o painel aparece quando ele pega
        DesenharHudArmas(b, tempo);

    // Vidas da partida: um painel no mesmo estilo do da arma, logo à direita dele
    // (a sala da Mestre ainda é o Bloco 1: lá não há vidas)
    if (b->tipoSala != SALA_MESTRE) {
        Rectangle pv = { 262, 506, 62, 22 };
        bool ultima = RogueVidasRestantes(b) == 1;
        DrawRectangleRec(pv, Fade((Color){ 12, 10, 24, 255 }, 0.8f));
        DrawRectangleLinesEx(pv, 1, Fade((Color){ 235, 45, 70, 255 }, ultima ? 0.5f + 0.4f * sinf(tempo * 5.0f) : 0.7f));
        DesenharVidas(b, pv.x + pv.width / 2, pv.y + pv.height / 2, 5.5f, 1.0f, false);
    }
    if (!atrio)
        DesenharHudLuci(b, tempo);

    // A build (melhorias escolhidas) continua a coluna dos upgrades passivos
    {
        int uy = 76;
        for (int i = PRIMEIRO_UPGRADE; i < NUM_ITENS; i++)
            if (j->itens[i] > 0) uy += 26;
        DesenharHudBuild(b, uy, tempo);
    }

    // Mochila: as teclas dos itens de usar e os passivos
    for (int i = 0; i < NUM_ITENS_LOJA; i++) {

        int x = 500 + i * 48;
        bool tem = j->itens[i] > 0;

        DrawRectangle(x - 18, yHud, 36, 36, Fade((Color){ 30, 24, 50, 255 }, tem ? 1.0f : 0.4f));
        DrawRectangleLines(x - 18, yHud, 36, 36, tem ? Fade(GOLD, 0.7f) : Fade(GRAY, 0.3f));

        if (tem) {
            DesenharIconeItem((TipoItem)i, (Vector2){ (float)x, (float)yHud + 16 }, 0.8f, tempo);
            DrawText(TextFormat("x%d", j->itens[i]), x + 4, yHud + 26, 10, WHITE);
        }

        if (i == ITEM_ANTIVIRUS)    DrawText("1", x - 15, yHud + 2, 10, GOLD);
        if (i == ITEM_PACOTE_DADOS) DrawText("2", x - 15, yHud + 2, 10, GOLD);
    }

    const char *controles = atrio ? "WASD andar    E falar, comprar, ler e abrir portas    porta de cima: missão    porta da esquerda: corredor"
                          : (b->tipoSala == SALA_MESTRE) ? ((b->mestre.etapa >= MT_PULSO) ? "WASD andar  MOUSE atirar  ESPAÇO esquiva  V / botão direito PARRY  Q pulso"
                                                                                            : (b->mestre.etapa >= MT_PARRY) ? "WASD andar  MOUSE atirar  ESPAÇO esquiva  V / botão direito PARRY"
                                                                                            : (b->mestre.etapa == MT_ARMA) ? "WASD andar  E pegar"
                                                                                            : "WASD andar  MOUSE atirar  ESPAÇO esquiva  E usar")
                          : (b->tipoSala == SALA_SILOGISTA && b->silogista.modoBH) ? "WASD andar  MOUSE atirar  ESPAÇO esquiva  SHIFT foco  V / botão direito PARRY  Q pulso  H scanner  E usar"
                          : (b->tipoSala == SALA_LUCI) ? "WASD andar  MOUSE atirar  ESPAÇO esquiva  V / botão direito PARRY  Q pulso  C contestar  H scanner  E usar"
                                  : "WASD andar  MOUSE atirar  ESPAÇO esquiva  Q pulso  1/2 itens  TAB arma  H scanner  C contestar  M mapa  E usar";
    int tamControles = 12;
    if (MeasureText(controles, tamControles) > LARGURA_TELA - 16) tamControles = 10;
    DrawRectangle(0, ALTURA_TELA - 20, LARGURA_TELA, 20, Fade(BLACK, 0.55f));
    DrawText(controles, LARGURA_TELA / 2 - MeasureText(controles, tamControles) / 2, ALTURA_TELA - 17, tamControles, LIGHTGRAY);

    // Cartão de título do Bloco 2 (chegada à Biblioteca)
    if (b->tituloBloco > 0.0f) {
        float k = fminf(b->tituloBloco / 1.0f, 1.0f) * fminf((4.5f - b->tituloBloco) / 0.6f, 1.0f);
        DrawRectangle(0, 150, LARGURA_TELA, 110, Fade(BLACK, 0.65f * k));
        const char *t1 = "BLOCO 2";
        const char *t2 = "A BIBLIOTECA DOS ECOS";
        DrawText(t1, LARGURA_TELA / 2 - MeasureText(t1, 14) / 2, 168, 14, Fade(LIGHTGRAY, k));
        DrawText(t2, LARGURA_TELA / 2 - MeasureText(t2, 30) / 2, 196, 30, Fade(GOLD, k));
        DrawLine(LARGURA_TELA / 2 - 140, 240, LARGURA_TELA / 2 + 140, 240, Fade(GOLD, 0.6f * k));
    }

    // Virada de fase de um chefe: a faixa entra rápido, o título falha no começo e assenta
    int yFaixa = (b->tipoSala == SALA_CHEFE) ? 176 : ((b->tipoSala == SALA_SILOGISTA || b->janela.ativo) ? 134 : 92);
    if (b->faixaVirada > 0.0f) {
        float q = b->faixaVirada / FAIXA_VIRADA_TEMPO;                               // 1 -> 0
        float k = fminf((1.0f - q) / 0.10f, 1.0f) * fminf(q / 0.25f, 1.0f);
        Color cor = b->faixaCor;
        DesenharVinheta(cor, 0.22f * k, 110.0f);
        DrawRectangle(0, yFaixa, LARGURA_TELA, 56, Fade(BLACK, 0.62f * k));
        DrawRectangle(0, yFaixa, LARGURA_TELA, 2, Fade(cor, 0.8f * k));
        DrawRectangle(0, yFaixa + 54, LARGURA_TELA, 2, Fade(cor, 0.8f * k));
        char linha[64];
        CorromperTexto(b->faixaTitulo, linha, sizeof(linha), (unsigned int)(tempo * 12.0f), (int)(45.0f * fmaxf(0.0f, (q - 0.7f) / 0.3f)));
        int tam = 24, wt = MeasureText(linha, tam);
        int d = (int)(1.5f + 4.0f * fmaxf(0.0f, (q - 0.55f) / 0.45f));
        BeginBlendMode(BLEND_ADDITIVE);
        DrawText(linha, LARGURA_TELA / 2 - wt / 2 - d, yFaixa + 8, tam, Fade((Color){ 255, 0, 60, 255 }, 0.8f * k));
        DrawText(linha, LARGURA_TELA / 2 - wt / 2 + d, yFaixa + 8, tam, Fade((Color){ 0, 120, 255, 255 }, 0.8f * k));
        EndBlendMode();
        DrawText(linha, LARGURA_TELA / 2 - wt / 2, yFaixa + 8, tam, Fade(WHITE, k));
        DrawText(b->faixaSub, LARGURA_TELA / 2 - MeasureText(b->faixaSub, 12) / 2, yFaixa + 36, 12, Fade(cor, k));
    }

    // Mensagem curta no meio da tela
    if (b->tempoAviso > 0.0f) {
        float k = fminf(b->tempoAviso / 0.4f, 1.0f);
        int w = MeasureText(b->aviso, 18);
        int ya = (b->tipoSala == SALA_CHEFE) ? 176 : ((b->tipoSala == SALA_SILOGISTA || b->janela.ativo) ? 134 : ((b->tipoSala == SALA_LUCI) ? 122 : 92));    // abaixo das faixas dos chefes
        if (b->faixaVirada > 0.0f) ya = yFaixa + 62;                                    // não fica por baixo da faixa
        DrawRectangle(LARGURA_TELA / 2 - w / 2 - 12, ya, w + 24, 30, Fade(BLACK, 0.7f * k));
        DrawText(b->aviso, LARGURA_TELA / 2 - w / 2, ya + 6, 18, Fade(GOLD, k));
    }

    if (b->lojaAberta)
        DesenharLojaRogue(b, tempo);

    if (b->enciclopediaAberta)
        DesenharEnciclopediaRogue(b);

    if (b->desafioAberto)
        DesenharDesafioRogue(b, tempo);

    // Reformulação: janelas novas
    if (b->eventoAberto)
        DesenharEvento(b, tempo);

    // Expansão educativa: a janela do laboratório
    if (b->lab.aberto)
        DesenharLab(b, tempo);

    // Seletor de chefes (por cima de tudo, com o cursor visível)
    if (b->seletorAberto)
        DesenharSeletorChefes(b, tempo);

    if (b->escolhaAberta)
        DesenharEscolha(b, tempo);

    if (b->trocaArmaAberta)
        DesenharTrocaArma(b, tempo);

    if (b->mapaAberto)
        DesenharMapaGrande(b, tempo);

    if (b->resultadoAberto)
        DesenharResultadoFinal(b, tempo);

    // Bloco 3: textos flutuantes, erro de verificação, scanner, data bleed, estática e flash negativo
    DesenharEfeitosTela(b, tempo);

    // Entrando numa sala nova
    if (b->fadeEntrada > 0.0f)
        DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, b->fadeEntrada / 0.35f));

    // Derrota
    if (b->morreu) {

        float k = fminf(b->tempoMorte / 0.6f, 1.0f);
        DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, 0.7f * k));

        bool bloco3 = RogueNoBloco3(b);
        int restam = RogueVidasRestantes(b);
        bool semVidas = !b->modoSeletor && restam <= 0;

        // As vidas em cima do título: a que acabou de ser gasta pisca (o seletor de chefes não gasta vida)
        if (!b->modoSeletor)
            DesenharVidas(b, LARGURA_TELA / 2.0f, 196.0f, 9.0f, k, true);

        const char *titulo = semVidas ? "FIM DA PARTIDA" : (bloco3 ? "A LUCI VENCEU ESTA" : "VOCÊ FOI CORROMPIDO");
        DrawText(titulo, LARGURA_TELA / 2 - MeasureText(titulo, 36) / 2, 230, 36, Fade(MAGENTA, k));

        const char *sub = semVidas ? TextFormat("Você usou as %d vidas.  -%d pontos", LIMITE_MORTES, PENALIDADE_HP_ZERO)
                                   : TextFormat("Os ecos falsos venceram desta vez.  -%d pontos", PENALIDADE_HP_ZERO);
        DrawText(sub, LARGURA_TELA / 2 - MeasureText(sub, 16) / 2, 282, 16, Fade(WHITE, k));

        const char *perda;
        if (semVidas)          perda = "A partida acabou. Seus pontos vão para o ranking.";
        else if (b->modoSeletor) perda = "Seletor de chefes: é treino, não gasta vida. Você volta ao átrio.";
        else if (bloco3)       perda = "Você volta ao corredor do núcleo, sem perder nada além da vida.";
        else                   perda = "Você volta ao átrio sem os itens. Os Fragmentos de Conhecimento ficam com você.";
        DrawText(perda, LARGURA_TELA / 2 - MeasureText(perda, 12) / 2, 308, 12, Fade((Color){ 0, 228, 255, 255 }, k));

        if (!semVidas && !b->modoSeletor) {
            const char *aviso = (restam == 1) ? "Resta 1 vida. A próxima derrota encerra a partida."
                                              : TextFormat("Restam %d vidas.", restam);
            DrawText(aviso, LARGURA_TELA / 2 - MeasureText(aviso, 14) / 2, 376, 14, Fade((restam == 1) ? ORANGE : WHITE, k));
        }

        if (b->tempoMorte > (semVidas ? 1.5f : 1.0f) && ((int)(GetTime() * 2.0)) % 2 == 0)
        {
            const char *volta = semVidas ? "ENTER / CLIQUE - ver o ranking"
                              : (bloco3 ? "ENTER / CLIQUE - voltar ao corredor" : "ENTER / CLIQUE - voltar ao átrio");
            DrawText(volta, LARGURA_TELA / 2 - MeasureText(volta, 14) / 2, 340, 14, GOLD);
        }
    }
}
