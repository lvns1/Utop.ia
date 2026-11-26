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
// EXPANSÃO DA PROGRESSÃO: LÓGICA DOS ENCONTROS ESPECIAIS
// =============================================================

// Salas dos encontros especiais: a da Sentinela tem estantes para se esconder; a do
// Retropropagador, pilares baixos; a da Janela é aberta (o espaço é para desviar)
static const LayoutSala LAYOUTS_ESPECIAIS[NUM_ESPECIAIS] = {
    { 5, { {190, 190, 34, 100}, {576, 190, 34, 100}, {290, 390, 90, 28}, {420, 390, 90, 28}, {372, 250, 56, 30} }, 4,
      { {150, 430}, {650, 430}, {140, 160}, {660, 160} } },
    { 4, { {190, 160, 30, 30}, {580, 160, 30, 30}, {190, 420, 30, 30}, {580, 420, 30, 30} }, 6,
      { {150, 300}, {650, 300}, {260, 140}, {540, 140}, {260, 460}, {540, 460} } },
    { 0, { {0} }, 0, { {0} } },
};

static const Dialogo dialogoJanelaIntro = {
    .paginas = {
        "Luci: A barra no topo é a memória dele. Atire no escudo para enchê-la de RUÍDO!",
        "Luci: Lotou, ele esquece o mais antigo: o PROTEGER. Aí o escudo cai. Pegue os dados dourados!"
    },
    .numPaginas = 2
};

static const Dialogo dialogoJanelaVitoria = {
    .paginas = {
        "* A JANELA DE CONTEXTO se apaga. Ela só lembrava do que cabia na barra.",
        "Luci: Encheu de ruído, esqueceu o começo. Em conversa longa, repita o essencial!"
    },
    .numPaginas = 2
};

// ---------- Utilidades de linha de visão ----------

// Nenhuma estante entre a e c (amostras a cada 8 px)
static bool RogueLinhaLivre(const Biblioteca *b, Vector2 a, Vector2 c)
{
    float dist = Vector2Distance(a, c);
    int passos = (int)(dist / 8.0f);

    for (int k = 1; k < passos; k++) {
        Vector2 p = Vector2Lerp(a, c, (float)k / (float)passos);
        for (int i = 0; i < b->numObst; i++)
            if (CheckCollisionPointRec(p, b->obst[i]))
                return false;
    }
    return true;
}

// Onde um feixe que sai de "origem" na direção "dir" para: na primeira estante ou na parede
static Vector2 RogueFimFeixe(const Biblioteca *b, Vector2 origem, Vector2 dir)
{
    Vector2 p = origem;

    for (int k = 0; k < 200; k++) {
        Vector2 q = Vector2Add(p, Vector2Scale(dir, 6.0f));
        if (!CheckCollisionPointRec(q, ArenaRogue()))
            return q;
        for (int i = 0; i < b->numObst; i++)
            if (CheckCollisionPointRec(q, b->obst[i]))
                return q;
        p = q;
    }
    return p;
}

// O ponto está dentro do feixe (segmento a-c com a largura do feixe)?
static bool RogueNoFeixe(Vector2 a, Vector2 c, Vector2 p, float largura)
{
    Vector2 ac = Vector2Subtract(c, a);
    float l2 = Vector2DotProduct(ac, ac);
    if (l2 < 1.0f)
        return false;
    float t = Clamp(Vector2DotProduct(Vector2Subtract(p, a), ac) / l2, 0.0f, 1.0f);
    return Vector2Distance(p, Vector2Add(a, Vector2Scale(ac, t))) < largura / 2.0f + RAIO_CORPO_JOGADOR;
}

// ---------- ELITE A: SENTINELA DE ATENÇÃO ----------
// estado 0 = focando/procurando, 1 = travada (aviso), 2 = feixe, 3 = recalculando (vulnerável)
static void RogueAtualizarSentinela(Biblioteca *b, Inimigo *e, Vector2 dir, float dist, float dt)
{
    JogadorRogue *j = &b->j;
    const float vel = DEF_INIMIGOS[INIMIGO_SENTINELA].velocidade;
    bool visao = RogueLinhaLivre(b, e->pos, j->pos);
    bool tinhaVisao = e->visao;

    e->visao = visao;
    e->miraPrevista = j->pos;                      // para o desenho da linha de atenção
    if (e->distraida > 0.0f) e->distraida -= dt;

    switch (e->estado) {

        case 0:
            if (visao) {
                // Com o jogador à vista, para e concentra a atenção
                e->foco = fminf(1.0f, e->foco + dt / SENTINELA_FOCO_TEMPO);
                e->vel = (dist > 300.0f) ? Vector2Scale(dir, vel * 0.5f) : (Vector2){0};
            } else {
                // Perdeu o jogador de vista: fica distraída e procura uma linha livre andando de lado
                if (tinhaVisao && e->foco > 0.2f) {
                    e->distraida = SENTINELA_DISTRAIDA;
                    RogueTextoFlutuante(b, (Vector2){ e->pos.x, e->pos.y - 30 }, "PERDEU O FOCO", GOLD);
                    TocarGrupo(&gChefeEscudo, 1.4f, 0.45f);      // a lente abriu: hora de atacar
                }
                e->foco = fmaxf(0.0f, e->foco - dt * SENTINELA_PERDE_FOCO);
                Vector2 lado = { -dir.y * e->lado, dir.x * e->lado };
                e->vel = Vector2Scale(Vector2Normalize(Vector2Add(Vector2Scale(dir, 0.5f), lado)), vel);
                if (fmodf(e->anim, 3.0f) < dt) e->lado = -e->lado;
            }

            if (e->foco >= 1.0f && e->recarga <= 0.0f && visao) {
                e->estado = 1;
                e->timer = SENTINELA_TRAVA;
                e->alvo = dir;
                e->fimFeixe = RogueFimFeixe(b, e->pos, dir);
                e->vel = (Vector2){0};
                TocarGrupo(&gChefeMira, 0.8f / SENTINELA_TRAVA, 0.75f);     // o "clack" da trava cai na hora do feixe
            }
            break;

        case 1:     // travada: a direção não muda mais (dá para sair da linha ou se esconder)
            e->vel = (Vector2){0};
            e->timer -= dt;
            if (e->timer <= 0.0f) {
                e->estado = 2;
                e->timer = SENTINELA_FEIXE;
                e->feixeAcertou = false;
                TocarGrupo(&gSentinelaFeixe, 1.0f, 0.95f);
            }
            break;

        case 2:     // feixe de atenção: atravessa a sala até a primeira estante
            e->vel = (Vector2){0};
            e->timer -= dt;
            if (!e->feixeAcertou && RogueNoFeixe(e->pos, e->fimFeixe, j->pos, SENTINELA_FEIXE_LARGURA)) {
                RogueFerirJogador(b, SENTINELA_FEIXE_DANO);
                e->feixeAcertou = true;
            }
            if (GetRandomValue(0, 1) == 0) RogueParticulas(b, e->fimFeixe, (Color){ 140, 230, 255, 255 }, 2, 120);
            if (e->timer <= 0.0f) {
                e->estado = 3;
                e->timer = SENTINELA_RECALCULA;
                e->foco = 0.0f;
                RogueTextoFlutuante(b, (Vector2){ e->pos.x, e->pos.y - 30 }, "RECALCULANDO PESOS", (Color){ 140, 230, 255, 255 });
            }
            break;

        default:    // recalculando: parada e vulnerável
            e->vel = (Vector2){0};
            e->timer -= dt;
            if (GetRandomValue(0, 4) == 0) RogueParticulas(b, e->pos, (Color){ 140, 230, 255, 255 }, 1, 60);
            if (e->timer <= 0.0f) { e->estado = 0; e->recarga = SENTINELA_INTERVALO; }
            break;
    }
}

bool RogueSentinelaVulneravel(const Inimigo *e)
{
    return e->estado == 3 || e->distraida > 0.0f;
}

// ---------- ELITE B: RETROPROPAGADOR ----------
// estado 0 = para frente (registra os golpes), 1 = aviso, 2 = volta (devolve), 3 = recalibrando (vulnerável)
static void RogueAtualizarRetro(Biblioteca *b, Inimigo *e, Vector2 dir, float dist, float ritmo, float dt)
{
    const float vel = DEF_INIMIGOS[INIMIGO_RETRO].velocidade;

    if (e->habilidade > 0.0f) e->habilidade -= dt;

    switch (e->estado) {

        case 0:
            e->vel = RogueManterDistancia(e, dir, dist, 120.0f, 220.0f, vel, dt);
            e->timer += dt * ritmo;
            if (e->timer >= RETRO_FORWARD) {
                e->timer = 0.0f;
                if (e->numRegistros > 0) {
                    e->estado = 1;
                    e->timer = RETRO_AVISO;
                    TocarGrupo(&gRetroAviso, 0.9f / RETRO_AVISO, 0.8f);    // a fita corta seco na hora da volta
                    RogueTextoFlutuante(b, (Vector2){ e->pos.x, e->pos.y - 32 }, "RETROPROPAGANDO...", ORANGE);
                } else {
                    // Gradiente zero: ninguém o feriu, então só um anel lento e uma pausa curta
                    for (int k = 0; k < 8; k++) {
                        float a = (2.0f * PI / 8.0f) * k + e->anim;
                        RogueDisparar(b, e->pos, (Vector2){ cosf(a), sinf(a) }, 140.0f, RETRO_TIRO_DANO * 0.75f, 6.0f, false);
                    }
                    RogueAnel(b, e->pos, 50.0f, DEF_INIMIGOS[INIMIGO_RETRO].cor);
                    TocarGrupo(&gIniAnel, 0.9f, 0.6f);
                    e->estado = 3;
                    e->timer = RETRO_RECALIBRA * 0.5f;
                }
            }
            break;

        case 1:
            e->vel = (Vector2){0};
            e->timer -= dt;
            if (e->timer <= 0.0f) { e->estado = 2; e->tiros = 0; e->timer = 0.0f; }
            break;

        case 2: {
            // Um projétil para cada marca, em sequência (golpes demais deixam a volta mais rápida)
            e->vel = (Vector2){0};
            e->timer -= dt;
            float rapidez = 1.0f + 0.08f * (float)((e->excesso < 6) ? e->excesso : 6);
            while (e->timer <= 0.0f && e->tiros < e->numRegistros) {
                Vector2 d = Vector2Subtract(e->registro[e->tiros], e->pos);
                d = (Vector2Length(d) > 1.0f) ? Vector2Normalize(d) : dir;
                RogueDisparar(b, e->pos, d, RETRO_TIRO_VEL * rapidez, RETRO_TIRO_DANO, 7.0f, false);
                TocarGrupo(&gRetroTiro, rapidez, 0.75f);
                e->tiros++;
                e->timer += 0.06f;
            }
            if (e->tiros >= e->numRegistros) {
                e->numRegistros = 0;
                e->excesso = 0;
                e->estado = 3;
                e->timer = RETRO_RECALIBRA;
                RogueTextoFlutuante(b, (Vector2){ e->pos.x, e->pos.y - 32 }, "RECALIBRANDO", LIGHTGRAY);
            }
        } break;

        default:
            e->vel = Vector2Scale(dir, vel * 0.2f);
            e->timer -= dt;
            if (GetRandomValue(0, 5) == 0) RogueParticulas(b, e->pos, GRAY, 1, 50);
            if (e->timer <= 0.0f) { e->estado = 0; e->timer = 0.0f; }
            break;
    }
}

// Golpe recebido na passagem "para frente": guarda de onde o jogador atacou
void RogueRetroRegistrar(Biblioteca *b, Inimigo *e)
{
    if (e->estado != 0 || e->habilidade > 0.0f)
        return;

    if (e->numRegistros < RETRO_MAX_REGISTROS) e->registro[e->numRegistros++] = b->j.pos;
    else                                       e->excesso++;
    e->habilidade = RETRO_REGISTRO_INTERVALO;
}

// ---------- MINI-CHEFE: JANELA DE CONTEXTO ----------

static Inimigo *RogueJanelaChefe(Biblioteca *b)
{
    JanelaContexto *jc = &b->janela;
    if (!jc->ativo || jc->indice < 0 || jc->indice >= MAX_INIMIGOS)
        return NULL;
    Inimigo *e = &b->ini[jc->indice];
    return (e->ativo && e->tipo == INIMIGO_JANELA) ? e : NULL;
}

static int RogueJanelaContar(const JanelaContexto *jc, int tipo)
{
    int n = 0;
    for (int i = 0; i < jc->num; i++) if (jc->slot[i] == tipo) n++;
    return n;
}

static void RogueJanelaDica(Biblioteca *b, const char *texto)
{
    snprintf(b->janela.dica, sizeof(b->janela.dica), "%s", texto);
}

// A instrução PROTEGER saiu da janela: o escudo cai
static void RogueJanelaAbrir(Biblioteca *b)
{
    JanelaContexto *jc = &b->janela;
    Inimigo *e = RogueJanelaChefe(b);

    jc->vulneravel = JANELA_VULNERAVEL;
    jc->ciclos++;
    if (e != NULL) {
        e->estado = 3;
        e->timer = 0.0f;
        RogueParticulas(b, e->pos, (Color){ 150, 140, 255, 255 }, 30, 260);
        RogueAnel(b, e->pos, 90.0f, (Color){ 150, 140, 255, 255 });
    }
    RogueAviso(b, "INSTRUÇÃO ESQUECIDA! O escudo caiu: ataque!");
    TocarGrupo(&gChefeEscudo, 0.95f, 0.9f);
    if (jc->ciclos == 1)
        RogueJanelaDica(b, "A janela passou do limite e o bloco mais antigo (PROTEGER) foi esquecido.");
}

// Tira o bloco mais antigo da janela
static void RogueJanelaEsquecer(Biblioteca *b)
{
    JanelaContexto *jc = &b->janela;
    if (jc->num <= 0)
        return;

    int tipo = jc->slot[0];
    for (int i = 1; i < jc->num; i++) { jc->slot[i - 1] = jc->slot[i]; jc->memoria[i - 1] = jc->memoria[i]; }
    jc->num--;
    jc->esquecidoTipo = tipo;
    jc->esquecidoAnim = 0.9f;
    jc->estouros++;

    Inimigo *e = RogueJanelaChefe(b);
    if (tipo == CTX_SISTEMA) {
        RogueJanelaAbrir(b);
    } else if (tipo == CTX_RELEVANTE && jc->fase >= 3 && e != NULL && jc->vulneravel <= 0.0f) {
        // Fase 3: perder um dado o deixa nervoso (um anel curto)
        for (int k = 0; k < 8; k++) {
            float a = (2.0f * PI / 8.0f) * k + e->anim;
            RogueDisparar(b, e->pos, (Vector2){ cosf(a), sinf(a) }, 150.0f, 6.0f, 6.0f, false);
        }
        RogueTextoFlutuante(b, (Vector2){ e->pos.x, e->pos.y - 40 }, "PERDEU O FIO", GOLD);
        TocarGrupo(&gIniAnel, 1.1f, 0.6f);
    }
}

// Um bloco entra no fim da janela; se passar do limite, o mais antigo sai
static void RogueJanelaInserir(Biblioteca *b, int tipo, Vector2 memoria)
{
    JanelaContexto *jc = &b->janela;

    if (jc->num >= JANELA_MAX_SLOTS + 1)
        RogueJanelaEsquecer(b);
    jc->slot[jc->num] = (unsigned char)tipo;
    jc->memoria[jc->num] = memoria;
    jc->num++;
    jc->entrouAnim = 0.35f;

    while (jc->num > jc->capacidade)
        RogueJanelaEsquecer(b);
}

// Golpe no escudo: o dano vira ruído na janela (prompt inundado)
void RogueJanelaRuido(Biblioteca *b, float dano)
{
    JanelaContexto *jc = &b->janela;
    Inimigo *e = RogueJanelaChefe(b);

    if (e == NULL || jc->vulneravel > 0.0f)
        return;

    jc->ruidoAcumulado += dano;
    while (jc->ruidoAcumulado >= JANELA_DANO_POR_RUIDO && jc->vulneravel <= 0.0f) {
        jc->ruidoAcumulado -= JANELA_DANO_POR_RUIDO;
        RogueTextoFlutuante(b, (Vector2){ e->pos.x + (float)GetRandomValue(-20, 20), e->pos.y - 36 }, "+RUÍDO", LIGHTGRAY);
        RogueJanelaInserir(b, CTX_RUIDO, (Vector2){0});
    }
}

// Fim do escudo aberto: ela relê as instruções (na fase 3, repete PROTEGER no FIM da janela)
static void RogueJanelaReler(Biblioteca *b)
{
    JanelaContexto *jc = &b->janela;
    Inimigo *e = RogueJanelaChefe(b);

    jc->num = 0;
    jc->vulneravel = 0.0f;
    jc->ruidoAcumulado = 0.0f;
    if (jc->fase < 3) {
        RogueJanelaInserir(b, CTX_SISTEMA, (Vector2){0});
        RogueJanelaInserir(b, CTX_RELEVANTE, b->j.pos);
    } else {
        RogueJanelaInserir(b, CTX_RELEVANTE, b->j.pos);
        RogueJanelaInserir(b, CTX_SISTEMA, (Vector2){0});
    }
    if (e != NULL) { e->estado = 0; e->recarga = 1.2f; RogueAnel(b, e->pos, 70.0f, (Color){ 0, 228, 255, 255 }); }
    TocarGrupo(&gIniBarreira, 0.85f, 0.7f);              // o escudo volta
    RogueAviso(b, (jc->fase < 3) ? "Ela releu as instruções. Encha a janela de novo!" : "Agora ela repete PROTEGER no FIM da janela!");
}

// Um dado dourado nasce na borda da sala e voa até a Janela (carrega a posição do jogador)
static void RogueJanelaEmitir(Biblioteca *b)
{
    Vector2 p = { 400, 80 };
    for (int t = 0; t < 8; t++) {
        int lado = GetRandomValue(0, 3);
        p = (lado == 0) ? (Vector2){ (float)GetRandomValue(80, 720), 76 } :
            (lado == 1) ? (Vector2){ (float)GetRandomValue(80, 720), 514 } :
            (lado == 2) ? (Vector2){ 56, (float)GetRandomValue(100, 490) } : (Vector2){ 744, (float)GetRandomValue(100, 490) };
        if (Vector2Distance(p, b->j.pos) > 130.0f) break;
    }
    Inimigo *t = RogueCriarInimigo(b, INIMIGO_TOKEN, p);
    if (t != NULL) { t->surgindo = 0.25f; t->memoria = b->j.pos; }
}

static bool RogueAtualizarToken(Biblioteca *b, Inimigo *e, float dt)
{
    Inimigo *chefe = RogueJanelaChefe(b);
    (void)dt;

    if (chefe == NULL) {
        e->ativo = false;
        return false;
    }

    Vector2 d = Vector2Subtract(chefe->pos, e->pos);
    float l = Vector2Length(d);
    if (l < chefe->raio + e->raio) {
        RogueJanelaInserir(b, CTX_RELEVANTE, e->memoria);
        RogueParticulas(b, e->pos, GOLD, 6, 100);
        TocarGrupo(&gIniPacote, 1.35f, 0.4f);            // o dado entrou na janela
        e->ativo = false;
        return false;
    }

    e->vel = Vector2Scale(d, TOKEN_VELOCIDADE[b->janela.fase - 1] / l);
    return true;
}

// Ajudantes da Janela: ALUCINAÇÕES (fragmentos) e RUÍDO (spammers) nascem em volta dela
static int RogueJanelaAjudantes(const Biblioteca *b)
{
    int n = 0;
    for (int k = 0; k < MAX_INIMIGOS; k++)
        if (b->ini[k].ativo && (b->ini[k].tipo == INIMIGO_ALUCINACAO || b->ini[k].tipo == INIMIGO_VERBOSO)) n++;
    return n;
}

static void RogueJanelaReforcar(Biblioteca *b, int fragmentos, int spammers)
{
    static const Vector2 pontos[6] = { {120, 150}, {680, 150}, {120, 450}, {680, 450}, {400, 120}, {400, 480} };
    int ponto = GetRandomValue(0, 5);
    for (int k = 0; k < fragmentos; k++)
        RogueCriarInimigo(b, INIMIGO_FRAGMENTO, RogueLugarSurgir(b, pontos[(ponto + k) % 6], FRAGMENTO_RAIO));
    for (int k = 0; k < spammers; k++)
        RogueCriarInimigo(b, INIMIGO_SPAMMER, RogueLugarSurgir(b, pontos[(ponto + fragmentos + k) % 6], 10.0f));
}

// Ajudantes exclusivos da Janela: ALUCINAÇÕES (some e reaparece atirando) e VERBOSOS (anéis de balas)
static void RogueJanelaAjudar(Biblioteca *b, int alucinacoes, int verbosos)
{
    static const Vector2 pontos[6] = { {120, 150}, {680, 150}, {120, 450}, {680, 450}, {400, 120}, {400, 480} };
    int ponto = GetRandomValue(0, 5);
    for (int k = 0; k < alucinacoes; k++)
        RogueCriarInimigo(b, INIMIGO_ALUCINACAO, RogueLugarSurgir(b, pontos[(ponto + k) % 6], 15.0f));
    for (int k = 0; k < (verbosos + 1) / 2; k++)
        RogueCriarInimigo(b, INIMIGO_VERBOSO, RogueLugarSurgir(b, pontos[(ponto + alucinacoes + k) % 6], 22.0f));
}

// Fases dos elites (Sentinela e Retropropagador): a 66% e a 33% da vida ele chama ajudantes e solta um anel
static void RogueFasesElite(Biblioteca *b, Inimigo *e)
{
    if (e->faseElite >= 2) return;
    float limite = (e->faseElite == 0) ? 0.66f : 0.33f;
    if (e->hp > e->hpMax * limite) return;

    e->faseElite++;
    bool sentinela = (e->tipo == INIMIGO_SENTINELA);
    RogueFaixaVirada(b, TextFormat("%s: FASE %d", sentinela ? "SENTINELA" : "RETROPROPAGADOR", e->faseElite + 1),
                     sentinela ? "ela chama alucinações para ajudar" : "ele chama spam para ajudar", DEF_INIMIGOS[e->tipo].cor);
    RogueAnel(b, e->pos, 120.0f, DEF_INIMIGOS[e->tipo].cor);
    b->tremor = fmaxf(b->tremor, 0.15f);
    int balas = 10 + 4 * e->faseElite;
    for (int k = 0; k < balas; k++) {
        float a = (2.0f * PI / (float)balas) * (float)k;
        RogueDisparar(b, e->pos, (Vector2){ cosf(a), sinf(a) }, 150.0f, 6.0f, 6.0f, false);
    }
    if (sentinela) RogueJanelaReforcar(b, 1 + e->faseElite, 0);     // alucinações
    else           RogueJanelaReforcar(b, 0, 1 + e->faseElite);     // spam
}

// estado 0 = livre, 1 = preparando a resposta, 2 = resumindo, 3 = escudo esquecido (vulnerável)
static void RogueAtualizarJanela(Biblioteca *b, Inimigo *e, Vector2 dir, float dist, float dt)
{
    JanelaContexto *jc = &b->janela;
    (void)dir; (void)dist;

    e->empurrao = (Vector2){0};
    if (jc->esquecidoAnim > 0.0f) jc->esquecidoAnim -= dt;
    if (jc->entrouAnim > 0.0f)    jc->entrouAnim -= dt;

    // Fases: a janela cresce e ele aprende a se organizar
    float frac = e->hp / e->hpMax;
    int fase = (frac <= JANELA_FASE4) ? 4 : ((frac <= JANELA_FASE3) ? 3 : ((frac <= JANELA_FASE2) ? 2 : 1));
    if (fase > jc->fase) {
        jc->fase = fase;
        jc->capacidade = JANELA_CAPACIDADE[fase - 1];
        jc->resumo = JANELA_RESUMO[fase - 1];
        if (fase == 2) {
            RogueFaixaVirada(b, "FASE 2: JANELA MAIOR", "e agora ele RESUME o ruído", (Color){ 150, 140, 255, 255 });
            RogueJanelaAjudar(b, 1, 2);       // o ruído chega para ajudar
            RogueJanelaDica(b, "Janela maior: precisa de mais ruído. Encha mais rápido do que ele resume!");
        } else if (fase == 4) {
            RogueFaixaVirada(b, "FASE 4: SOBRECARGA", "a janela enche de alucinações!", RED);
            RogueJanelaDica(b, "Sobrecarga: tudo mais rápido e mais ajudantes. Esvazie a janela antes que ele resuma!");
            RogueJanelaAjudar(b, 2, 2);
            b->tremor = fmaxf(b->tremor, 0.2f);
        } else {
            RogueFaixaVirada(b, "FASE 3: INSTRUÇÃO REPETIDA", "ele repete a instrução no fim da janela", (Color){ 150, 140, 255, 255 });
            RogueJanelaAjudar(b, 3, 1);       // alucinações em peso
            RogueJanelaDica(b, "Dados dourados também ocupam espaço: deixá-los entrar enche a janela, mas melhora a mira dele.");
        }
    }

    // Deriva devagar pela sala
    Vector2 alvo = { 400.0f + sinf(e->anim * 0.45f) * 150.0f, 250.0f + sinf(e->anim * 0.8f) * 60.0f };
    Vector2 ate = Vector2Subtract(alvo, e->pos);
    float veloc = DEF_INIMIGOS[INIMIGO_JANELA].velocidade;
    e->vel = (e->estado == 0 && Vector2Length(ate) > 8.0f) ? Vector2Scale(Vector2Normalize(ate), veloc) : (Vector2){0};

    // Reforços: enquanto houver poucos ajudantes, chegam mais alucinações e ruído (mais rápido em fases altas)
    jc->reforco -= dt;
    if (jc->reforco <= 0.0f) {
        jc->reforco = 14.0f - 2.5f * (float)((jc->fase > 3) ? 3 : jc->fase) - ((jc->fase >= 4) ? 1.5f : 0.0f);
        if (RogueJanelaAjudantes(b) < 2 + jc->fase) {
            RogueJanelaAjudar(b, 1, (jc->fase >= 2) ? 1 : 0);
            RogueTextoFlutuante(b, (Vector2){ e->pos.x, e->pos.y - 40 }, "ALUCINAÇÃO!", (Color){ 255, 0, 255, 255 });
            TocarGrupo(&gChefeInvoca, 1.1f, 0.6f);
        }
    }

    // Dados dourados (o que ele sabe sobre o jogador)
    jc->emissor -= dt;
    if (jc->emissor <= 0.0f) {
        jc->emissor = JANELA_EMISSAO[jc->fase - 1];
        for (int k = 0; k < ((jc->fase >= 3) ? 2 : 1); k++)
            RogueJanelaEmitir(b);
    }

    switch (e->estado) {

        case 3:     // escudo esquecido
            jc->vulneravel -= dt;
            if (GetRandomValue(0, 2) == 0) RogueParticulas(b, e->pos, MAGENTA, 1, 80);
            if (jc->vulneravel <= 0.0f) RogueJanelaReler(b);
            break;

        case 2: {   // resumindo: descarta os 2 blocos de ruído mais antigos
            e->timer -= dt;
            if (e->timer <= 0.0f) {
                int tirados = 0;
                for (int i = 0; i < jc->num && tirados < 2; ) {
                    if (jc->slot[i] == CTX_RUIDO) {
                        for (int k = i + 1; k < jc->num; k++) { jc->slot[k - 1] = jc->slot[k]; jc->memoria[k - 1] = jc->memoria[k]; }
                        jc->num--;
                        tirados++;
                    } else i++;
                }
                RogueTextoFlutuante(b, (Vector2){ e->pos.x, e->pos.y - 40 }, TextFormat("RESUMO: -%d RUÍDO", tirados), (Color){ 150, 140, 255, 255 });
                e->estado = 0;
                if (e->recarga < 0.8f) e->recarga = 0.8f;
            }
        } break;

        case 1:     // preparando a resposta (linhas até o alvo)
            e->timer -= dt;
            if (e->timer <= 0.0f) {
                int nRel = e->tiros;
                if (e->carga > 0.5f) {
                    // Sem contexto relevante: resposta genérica (um anel espalhado e fraco)
                    float giro = (float)GetRandomValue(0, 360) * DEG2RAD;
                    for (int k = 0; k < 8; k++) {
                        float a = giro + (2.0f * PI / 8.0f) * k;
                        RogueDisparar(b, e->pos, (Vector2){ cosf(a), sinf(a) }, 150.0f, 6.0f, 6.0f, false);
                    }
                    RogueTextoFlutuante(b, (Vector2){ e->pos.x, e->pos.y - 40 }, "SEM CONTEXTO: ???", LIGHTGRAY);
                } else {
                    // Com contexto: um leque mirado no que ele LEMBRA (mais dados = mais balas e mais precisão)
                    Vector2 d = Vector2Subtract(e->alvo, e->pos);
                    d = (Vector2Length(d) > 1.0f) ? Vector2Normalize(d) : (Vector2){ 0, 1 };
                    int balas = 3 + 2 * nRel;
                    if (balas > 11) balas = 11;
                    float arco = (50.0f / (1.0f + 0.4f * (float)nRel)) * DEG2RAD;
                    for (int k = 0; k < balas; k++) {
                        float a = (balas > 1) ? ((float)k / (float)(balas - 1) - 0.5f) * arco : 0.0f;
                        RogueDisparar(b, e->pos, Vector2Rotate(d, a), 200.0f + 12.0f * (float)nRel, 8.0f, 6.0f, false);
                    }
                }
                TocarGrupo(&gChefeRajada, (e->carga > 0.5f) ? 1.2f : 0.95f, 0.75f);     // sem contexto: resposta fraca, mais aguda
                e->estado = 0;
                e->recarga = JANELA_RESPOSTA[jc->fase - 1];
            }
            break;

        default:
            if (jc->fase >= 2) {
                jc->resumo -= dt;
                if (jc->resumo <= 0.0f) {
                    jc->resumo = JANELA_RESUMO[jc->fase - 1];
                    if (RogueJanelaContar(jc, CTX_RUIDO) >= 2) {
                        e->estado = 2;
                        e->timer = 1.3f;
                        RogueTextoFlutuante(b, (Vector2){ e->pos.x, e->pos.y - 40 }, "RESUMINDO...", (Color){ 150, 140, 255, 255 });
                        TocarGrupo(&gJanelaResumo, 1.0f, 0.7f);
                        break;
                    }
                }
            }
            if (e->recarga <= 0.0f) {
                // Mira no dado mais recente que ainda está na janela
                int nRel = RogueJanelaContar(jc, CTX_RELEVANTE);
                e->tiros = nRel;
                e->carga = (nRel == 0) ? 1.0f : 0.0f;
                e->alvo = e->pos;
                for (int i = jc->num - 1; i >= 0; i--)
                    if (jc->slot[i] == CTX_RELEVANTE) { e->alvo = jc->memoria[i]; break; }
                e->estado = 1;
                e->timer = 0.8f;
            }
            break;
    }
}

// Derrotada: os dados somem, a recompensa sai uma vez e a explicação aparece
void RogueJanelaDerrotada(Biblioteca *b, ProgressoCorredor *prog)
{
    JanelaContexto *jc = &b->janela;

    jc->ativo = false;
    jc->vulneravel = 0.0f;
    for (int i = 0; i < MAX_INIMIGOS; i++)
        if (b->ini[i].ativo && b->ini[i].tipo == INIMIGO_TOKEN) {
            b->ini[i].ativo = false;
            RogueParticulas(b, b->ini[i].pos, GOLD, 4, 80);
        }

    if (jc->recompensaDada)
        return;
    jc->recompensaDada = true;

    if (!b->modoSeletor || b->recompensaSeletor) {
        b->perm->fragmentos += 10;
        AlterarPontos(prog, 20);
        RogueMudarEstabilidade(b, 1, "contexto organizado");
        RogueAviso(b, "JANELA DE CONTEXTO DERROTADA! +10 FRAGMENTOS");
    } else {
        RogueAviso(b, "JANELA DE CONTEXTO DERROTADA!");
    }
    b->conversaPendente = &dialogoJanelaVitoria;
    b->objConversa = OBJ_NENHUM;
    MostrarEmocaoLuci(EMOCAO_ALEGRE, 3.0f);
    Telemetria("rogue_minichefe", "janela", (float)(GetTime() - b->inicio), jc->ciclos);
}

// Monta a sala de um encontro especial (objetivo: derrotar o especial e quem vier com ele)
void RogueMontarSalaEspecial(Biblioteca *b, int esp)
{
    if (esp < 0 || esp >= NUM_ESPECIAIS) esp = ESP_SENTINELA;

    const LayoutSala *l = &LAYOUTS_ESPECIAIS[esp];
    RogueUsarLayout(b, l);
    layoutCombateAtual = (l->numPontos > 0) ? l : &LAYOUTS_COMBATE[0];
    b->combates++;
    b->objetivoSala = OBJC_ELIMINAR;
    b->ondaAtual = 0;
    b->numOndas = 1;
    b->encontro = -1;

    Vector2 topo = { 400, 170 };
    if (esp == ESP_SENTINELA) {
        Inimigo *e = RogueCriarInimigo(b, INIMIGO_SENTINELA, RogueLugarSurgir(b, topo, SENTINELA_RAIO));
        if (e != NULL) e->elite = true;
        for (int k = 0; k < 3; k++)
            RogueCriarInimigo(b, INIMIGO_FRAGMENTO, RogueLugarSurgir(b, l->pontos[k % l->numPontos], FRAGMENTO_RAIO));
    } else if (esp == ESP_RETRO) {
        Inimigo *e = RogueCriarInimigo(b, INIMIGO_RETRO, RogueLugarSurgir(b, topo, RETRO_RAIO));
        if (e != NULL) e->elite = true;
        for (int k = 0; k < 4; k++)
            RogueCriarInimigo(b, INIMIGO_SPAMMER, RogueLugarSurgir(b, l->pontos[k % l->numPontos], 10.0f));
        RogueCriarInimigo(b, INIMIGO_GUARDIAO, RogueLugarSurgir(b, l->pontos[4], GUARDIAO_RAIO));
    } else {
        Inimigo *e = RogueCriarInimigo(b, INIMIGO_JANELA, RogueLugarSurgir(b, (Vector2){ 400, 210 }, JANELA_RAIO));
        JanelaContexto *jc = &b->janela;
        memset(jc, 0, sizeof(*jc));
        if (e != NULL) {
            jc->ativo = true;
            jc->indice = (int)(e - b->ini);
            jc->fase = 1;
            jc->capacidade = JANELA_CAPACIDADE[0];
            jc->emissor = 2.5f;
            jc->resumo = JANELA_RESUMO[0];
            e->recarga = 2.5f;
            jc->reforco = 12.0f;
            RogueJanelaAjudar(b, 2, 0);       // duas alucinações desde o começo
            RogueJanelaInserir(b, CTX_SISTEMA, (Vector2){0});
            RogueJanelaInserir(b, CTX_RELEVANTE, b->j.pos);
            RogueJanelaDica(b, "Golpes no escudo viram RUÍDO. Encha a janela: o bloco mais antigo é esquecido.");
        }
        b->conversaPendente = &dialogoJanelaIntro;
        b->objConversa = OBJ_NENHUM;
    }

    RogueAviso(b, TextFormat("ENCONTRO ESPECIAL: %s", NOMES_ESPECIAIS[esp]));
    RogueRecadoLuci(b, FALAS_LUCI_ESPECIAL[esp]);
    b->luciMsgTempo = 4.5f;
    Telemetria("rogue_especial", NOMES_ESPECIAIS[esp], 0.0f, b->indiceRota);
}

void RogueAtualizarInimigos(Biblioteca *b, float dt)
{
    JogadorRogue *j = &b->j;

    RogueAtualizarConexoes(b);

    for (int i = 0; i < MAX_INIMIGOS; i++) {

        Inimigo *e = &b->ini[i];
        const DefInimigo *def = &DEF_INIMIGOS[e->tipo];

        if (!e->ativo)
            continue;

        e->anim += dt;
        if (e->flash > 0.0f) e->flash -= dt;
        if (e->rotulo > 0.0f) e->rotulo -= dt;
        if (e->gelo > 0.0f) e->gelo -= dt;
        if (e->queima > 0.0f) {                         // Katana (fogo): a cada 0,5 s a brasa fere
            e->queima -= dt;
            e->cronoQueima -= dt;
            if (e->cronoQueima <= 0.0f && b->progRef != NULL) {
                bool antes = b->golpeSecundario;
                e->cronoQueima = 0.5f;
                RogueParticulas(b, e->pos, ORANGE, 2, 70);
                b->golpeSecundario = true;               // não dispara reações em cadeia
                RogueFerirInimigo(b, b->progRef, e, e->queimaDano, (Vector2){ 0, 0 });
                b->golpeSecundario = antes;
                if (!e->ativo) continue;
            }
        }

        if (e->surgindo > 0.0f) {
            e->surgindo -= dt;
            continue;
        }

        // Proxy-Isca: quem persegue vai atrás do holograma
        bool enganado = j->efeito[EFEITO_PROXY] > 0.0f && RoguePerseguidor(e->tipo);
        Vector2 paraJogador = Vector2Subtract(enganado ? j->proxyPos : j->pos, e->pos);
        float dist = Vector2Length(paraJogador);
        Vector2 dir = (dist > 0.01f) ? Vector2Scale(paraJogador, 1.0f / dist) : (Vector2){ 0, 1 };

        // Ligado a um Botnet: as recargas passam mais rápido (ataca mais vezes)
        float ritmo = (e->conectado ? BOTNET_BONUS_RITMO : 1.0f) * (e->elite ? ELITE_RITMO : 1.0f) *
                      ((b->fatorRitmoInimigos > 0.0f) ? b->fatorRitmoInimigos : 1.0f);       // elite e profundidade

        if (e->recarga > 0.0f) e->recarga -= dt * ritmo;

        if (e->atordoado > 0.0f) {
            e->atordoado -= dt;
            e->vel = (Vector2){0};
        }
        else switch (e->tipo) {

            case INIMIGO_FRAGMENTO:

                if (e->estado == 0) {
                    // Persegue com um zigue-zague de dado corrompido
                    Vector2 lado = { -dir.y, dir.x };
                    e->vel = Vector2Add(Vector2Scale(dir, FRAGMENTO_VELOCIDADE),
                                        Vector2Scale(lado, sinf(e->anim * 5.0f) * 40.0f));

                    if (dist < FRAGMENTO_ALCANCE_BOTE && e->recarga <= 0.0f) {
                        e->estado = 1;
                        e->timer = FRAGMENTO_PREPARO;
                        e->alvo = dir;
                        e->vel = (Vector2){0};
                    }
                }
                else if (e->estado == 1) {
                    e->vel = (Vector2){0};
                    e->timer -= dt;
                    if (e->timer <= 0.0f) {
                        e->estado = 2;
                        e->timer = FRAGMENTO_BOTE_DURACAO;
                        e->vel = Vector2Scale(e->alvo, FRAGMENTO_BOTE_VEL);
                        TocarGrupo(&gIniBote, 1.0f, 0.75f);
                    }
                }
                else {
                    e->timer -= dt;
                    if (e->timer <= 0.0f) {
                        e->estado = 0;
                        e->recarga = FRAGMENTO_RECARGA;
                    }
                }
                break;

            case INIMIGO_GUARDIAO:

                if (e->estado == 0) {
                    // Mantém distância: foge de perto, chega mais se longe, senão anda de lado
                    if (dist < GUARDIAO_DIST_MIN)      e->vel = Vector2Scale(dir, -GUARDIAO_VELOCIDADE);
                    else if (dist > GUARDIAO_DIST_MAX) e->vel = Vector2Scale(dir, GUARDIAO_VELOCIDADE);
                    else                               e->vel = Vector2Scale((Vector2){ -dir.y * e->lado, dir.x * e->lado }, GUARDIAO_VELOCIDADE * 0.6f);

                    if (fmodf(e->anim, 2.5f) < dt) e->lado = -e->lado;

                    if (e->recarga <= 0.0f) {
                        e->estado = 1;
                        e->timer = GUARDIAO_PREPARO;
                        e->vel = (Vector2){0};
                        e->alvo = dir;
                    }
                }
                else {
                    e->vel = (Vector2){0};
                    e->timer -= dt;
                    if (e->timer > 0.12f) e->alvo = dir;          // a mira trava um instante antes (como a da Luci)

                    if (e->timer <= 0.0f) {
                        // Leque mirado no jogador (padrão previsível)
                        int leque = e->elite ? GUARDIAO_LEQUE + 2 : GUARDIAO_LEQUE;    // elite: leque de 5
                        for (int k = 0; k < leque; k++) {
                            float ang = ((float)k - (leque - 1) / 2.0f) * GUARDIAO_ABERTURA * DEG2RAD;
                            RogueDisparar(b, e->pos, Vector2Rotate(e->alvo, ang), GUARDIAO_TIRO_VEL, GUARDIAO_TIRO_DANO, 6.0f, false);
                        }
                        TocarGrupo(&gIniLeque, e->elite ? 0.9f : 1.0f, 0.75f);
                        e->estado = 0;
                        e->recarga = GUARDIAO_INTERVALO;
                    }
                }
                break;

            case INIMIGO_ECO:

                // Eco falso: se ninguém checar (atirar) a tempo, explode e espalha balas
                e->vel = (Vector2){0};
                e->timer -= dt;

                if (e->timer <= 0.0f) {
                    for (int k = 0; k < ECO_BALAS; k++) {
                        float ang = (2.0f * PI / ECO_BALAS) * k + e->anim;
                        RogueDisparar(b, e->pos, (Vector2){ cosf(ang), sinf(ang) }, 170.0f, 8.0f, 5.0f, false);
                    }
                    RogueParticulas(b, e->pos, MAGENTA, 14, 200);
                    RogueAnel(b, e->pos, 40.0f, MAGENTA);
                    TocarGrupo(&gIniEco, 1.0f, 0.8f);
                    e->ativo = false;
                    continue;
                }
                break;

            case INIMIGO_SPAMMER: {
                // Enxame: persegue em zigue-zague nervoso, cada um com seu desvio (o bando se espalha)
                Vector2 lado = { -dir.y, dir.x };
                float desvio = sinf(e->anim * 7.0f + e->lado * 2.0f + (float)i) * 70.0f;
                e->vel = Vector2Add(Vector2Scale(dir, def->velocidade), Vector2Scale(lado, desvio));
            } break;

            case INIMIGO_SPAM:
                e->vel = Vector2Scale(dir, def->velocidade);
                e->vida -= dt;
                if (e->vida <= 0.0f) {
                    RogueParticulas(b, e->pos, def->cor, 4, 80);
                    e->ativo = false;
                    continue;
                }
                break;

            case INIMIGO_PHISHER: {
                // Spammer + Phisher: com o jogador cercado de spam, ele ataca de mais longe e mais rápido
                bool distraido = RogueSpammersPerto(b, j->pos, 140.0f) >= 3;

                if (e->habilidade > 0.0f) e->habilidade -= dt;

                if (e->estado == 0 && e->recarga <= 0.0f && dist < PHISHER_ALCANCE * (distraido ? 1.4f : 1.0f))
                    RogueRevelarPhisher(b, e, distraido);

                RogueMoverPhisher(e, dir, def->velocidade, dt);
            } break;

            case INIMIGO_ISCA: {
                // Cópia falsa: imita o bote, mas não machuca. Some com o tempo ou com o dono.
                bool donoVivo = e->dono >= 0 && b->ini[e->dono].ativo && b->ini[e->dono].tipo == INIMIGO_PHISHER;

                e->vida -= dt;
                if (e->vida <= 0.0f || !donoVivo) {
                    RogueParticulas(b, e->pos, def->cor, 8, 120);
                    e->ativo = false;
                    continue;
                }

                RogueMoverPhisher(e, dir, def->velocidade, dt);
                if (e->estado == 0 && e->recarga <= 0.0f && dist < PHISHER_ALCANCE) {
                    e->estado = 1;
                    e->timer = PHISHER_PREPARO;
                }
            } break;

            case INIMIGO_FIREWALL: {
                // Vira devagar para o jogador: quem dá a volta acha as costas desprotegidas
                float ang = atan2f(e->frente.y, e->frente.x);
                float diferenca = atan2f(dir.y, dir.x) - ang;
                while (diferenca > PI)  diferenca -= 2.0f * PI;
                while (diferenca < -PI) diferenca += 2.0f * PI;
                float passo = FIREWALL_GIRO * dt;
                ang += Clamp(diferenca, -passo, passo);
                e->frente = (Vector2){ cosf(ang), sinf(ang) };

                // Fica na frente de quem protege (entre os aliados e o jogador)
                Vector2 centro = {0};
                Vector2 destino = j->pos;
                if (RogueQuemProteger(b, e, &centro))
                    destino = Vector2Add(centro, Vector2Scale(Vector2Normalize(Vector2Subtract(j->pos, centro)), 55.0f));
                else if (dist < 160.0f)
                    destino = e->pos;

                Vector2 ate = Vector2Subtract(destino, e->pos);
                e->vel = (Vector2Length(ate) > 16.0f) ? Vector2Scale(Vector2Normalize(ate), def->velocidade) : (Vector2){0};

                // Barreira de energia na frente (com aviso)
                if (e->estado == 0) {
                    e->habilidade -= dt * ritmo;
                    if (e->habilidade <= 0.0f) { e->estado = 1; e->timer = FIREWALL_AVISO_BARREIRA; }
                }
                else if (e->estado == 1) {
                    e->timer -= dt;
                    if (e->timer <= 0.0f) {
                        e->estado = 2;
                        e->timer = FIREWALL_BARREIRA;
                        TocarGrupo(&gIniBarreira, 1.0f, 0.7f);
                    }
                }
                else {
                    e->timer -= dt;
                    if (e->timer <= 0.0f) { e->estado = 0; e->habilidade = FIREWALL_INTERVALO; }
                }
            } break;

            case INIMIGO_OVERCLOCK:

                if (e->estado == 0) {
                    // Aproxima-se devagar até uma boa distância
                    e->vel = (dist > OVERCLOCK_ALCANCE * 0.8f) ? Vector2Scale(dir, def->velocidade)
                                                               : Vector2Scale((Vector2){ -dir.y * e->lado, dir.x * e->lado }, def->velocidade * 0.5f);
                    if (dist < OVERCLOCK_ALCANCE && e->recarga <= 0.0f) {
                        e->estado = 1;
                        e->carga = 0.0f;
                        e->cargaAlvo = OVERCLOCK_CARGA_MIN + (OVERCLOCK_CARGA_MAX - OVERCLOCK_CARGA_MIN) * (float)GetRandomValue(0, 100) / 100.0f;
                        e->alvo = dir;
                        TocarGrupo(&gIniCarga, Clamp(ritmo / e->cargaAlvo, 0.6f, 1.6f), 0.6f);     // o rangido dura o tempo da carga
                    }
                }
                else if (e->estado == 1) {
                    // Carregando: quanto mais carga, mais rápida a investida (e mais longa a fraqueza depois).
                    // Overclock + Keylogger: se houver um Keylogger vivo, mira onde ele previu.
                    e->vel = (Vector2){0};
                    e->carga += dt * ritmo;

                    if (e->cargaAlvo - e->carga > 0.15f) {
                        Vector2 alvo = RogueTemKeylogger(b) ? RoguePrevisaoJogador(b, e->pos, OVERCLOCK_INVESTIDA_MAX) : j->pos;
                        Vector2 d = Vector2Subtract(alvo, e->pos);
                        e->alvo = (Vector2Length(d) > 1.0f) ? Vector2Normalize(d) : dir;
                    }

                    if (e->carga >= e->cargaAlvo) {
                        float vel = OVERCLOCK_INVESTIDA_MIN + (OVERCLOCK_INVESTIDA_MAX - OVERCLOCK_INVESTIDA_MIN) * RogueCargaOverclock(e);
                        e->estado = 2;
                        e->timer = OVERCLOCK_INVESTIDA_DURACAO;
                        e->vel = Vector2Scale(e->alvo, vel);
                        TocarGrupo(&gIniInvestida, 1.05f - 0.15f * RogueCargaOverclock(e), 0.85f);   // carga maior = arranque mais grave
                    }
                }
                else if (e->estado == 2) {
                    e->timer -= dt;
                    if (GetRandomValue(0, 1) == 0) RogueParticulas(b, e->pos, ORANGE, 1, 40);      // rastro de energia
                    if (e->timer <= 0.0f) RogueSuperaquecer(e);
                }
                else {
                    // Superaquecido: quase parado e vulnerável
                    e->vel = Vector2Scale(dir, 20.0f);
                    e->timer -= dt;
                    if (GetRandomValue(0, 6) == 0) RogueParticulas(b, (Vector2){ e->pos.x, e->pos.y - 10 }, GRAY, 1, 30);
                    if (e->timer <= 0.0f) { e->estado = 0; e->recarga = OVERCLOCK_RECARGA; }
                }
                break;

            case INIMIGO_BOTNET: {
                // Fica longe e, se der, escondido atrás de um aliado (o aliado entre ele e o jogador)
                const Inimigo *escudo = NULL;
                float menor = 9999.0f;
                for (int o = 0; o < MAX_INIMIGOS; o++) {
                    const Inimigo *a = &b->ini[o];
                    if (a == e || !a->ativo || a->tipo == INIMIGO_BOTNET || RogueInimigoPassageiro(a->tipo)) continue;
                    if (Vector2Distance(a->pos, e->pos) < menor) { menor = Vector2Distance(a->pos, e->pos); escudo = a; }
                }

                if (dist < BOTNET_DIST_MIN || escudo == NULL) {
                    e->vel = RogueManterDistancia(e, dir, dist, BOTNET_DIST_MIN, BOTNET_DIST_MAX, def->velocidade, dt);
                } else {
                    Vector2 atras = Vector2Add(escudo->pos, Vector2Scale(Vector2Normalize(Vector2Subtract(escudo->pos, j->pos)), 70.0f));
                    Vector2 ate = Vector2Subtract(atras, e->pos);
                    e->vel = (Vector2Length(ate) > 12.0f) ? Vector2Scale(Vector2Normalize(ate), def->velocidade) : (Vector2){0};
                }

                // De vez em quando, um pacote fraco (brilha antes)
                if (e->estado == 0 && e->recarga <= 0.0f) { e->estado = 1; e->timer = BOTNET_PREPARO; e->alvo = dir; }
                else if (e->estado == 1) {
                    e->vel = Vector2Scale(e->vel, 0.3f);
                    e->timer -= dt;
                    if (e->timer > 0.1f) e->alvo = dir;
                    if (e->timer <= 0.0f) {
                        RogueDisparar(b, e->pos, e->alvo, 200.0f, BOTNET_TIRO_DANO, 5.0f, false);
                        TocarGrupo(&gIniPacote, 1.0f, 0.65f);
                        e->estado = 0;
                        e->recarga = BOTNET_INTERVALO;
                    }
                }
            } break;

            case INIMIGO_KEYLOGGER:

                if (e->estado == 0) {
                    e->vel = RogueManterDistancia(e, dir, dist, KEYLOGGER_DIST_MIN, KEYLOGGER_DIST_MAX, def->velocidade, dt);
                    if (e->recarga <= 0.0f) { e->estado = 1; e->timer = KEYLOGGER_PREPARO; }
                }
                else if (e->estado == 1) {
                    // A mira aparece no chão, seguindo a previsão; trava um instante antes da rajada
                    e->vel = (Vector2){0};
                    e->timer -= dt;
                    if (e->timer > 0.15f) e->miraPrevista = RoguePrevisaoJogador(b, e->pos, KEYLOGGER_TIRO_VEL);
                    if (e->timer <= 0.0f) { e->estado = 2; e->tiros = e->elite ? KEYLOGGER_RAJADA + 2 : KEYLOGGER_RAJADA; e->timer = 0.0f; }
                }
                else {
                    e->vel = (Vector2){0};
                    e->timer -= dt;
                    if (e->timer <= 0.0f && e->tiros > 0) {
                        Vector2 d = Vector2Normalize(Vector2Subtract(e->miraPrevista, e->pos));
                        float espalhar = (float)GetRandomValue(-3, 3) * DEG2RAD;
                        RogueDisparar(b, e->pos, Vector2Rotate(d, espalhar), KEYLOGGER_TIRO_VEL, KEYLOGGER_TIRO_DANO, 5.0f, false);
                        TocarGrupo(&gIniTecla, 1.0f, 0.7f);
                        e->tiros--;
                        e->timer = 0.1f;
                    }
                    if (e->tiros <= 0) { e->estado = 0; e->recarga = KEYLOGGER_INTERVALO; }
                }
                break;

            case INIMIGO_CACHE: {
                // Anda devagar atrás do jogador. Sem levar dano por um tempo, usa a energia guardada
                // para se curar (dentro do escudo de um Firewall, começa mais cedo).
                float espera = RogueNaZonaFirewall(b, e) ? CACHE_ESPERA_PROTEGIDO : CACHE_ESPERA;

                e->vel = Vector2Scale(dir, def->velocidade);
                e->semDano += dt;

                if (e->recuperando > 0.0f) {
                    e->vel = (Vector2){0};
                    e->recuperando -= dt;
                    if (GetRandomValue(0, 3) == 0) RogueParticulas(b, e->pos, LIME, 1, 50);
                    if (e->recuperando <= 0.0f) {
                        float cura = fminf(e->energia, e->hpMax - e->hp);
                        e->hp += cura;
                        e->energia = 0.0f;
                        e->semDano = 0.0f;
                        RogueParticulas(b, e->pos, LIME, 16, 140);
                        RogueAnel(b, e->pos, 44.0f, LIME);
                        TocarGrupo(&gIniCura, 1.0f, 0.65f);
                    }
                }
                else if (e->energia > 0.0f && e->hp < e->hpMax && e->semDano >= espera) {
                    e->recuperando = CACHE_RECUPERAR;
                }

                // Despeja dados: anel lento de balas (os blocos brilham antes)
                if (e->recuperando <= 0.0f) {
                    if (e->estado == 0 && e->recarga <= 0.0f) { e->estado = 1; e->timer = CACHE_PREPARO; }
                    else if (e->estado == 1) {
                        e->vel = (Vector2){0};
                        e->timer -= dt;
                        if (e->timer <= 0.0f) {
                            int balas = e->elite ? CACHE_BALAS + 4 : CACHE_BALAS;     // elite: anel mais denso
                            for (int k = 0; k < balas; k++) {
                                float a = (2.0f * PI / balas) * k + e->anim;
                                RogueDisparar(b, e->pos, (Vector2){ cosf(a), sinf(a) }, 140.0f, CACHE_TIRO_DANO, 6.0f, false);
                            }
                            RogueAnel(b, e->pos, 46.0f, def->cor);
                            TocarGrupo(&gIniAnel, 1.0f, 0.7f);
                            e->estado = 0;
                            e->recarga = CACHE_INTERVALO;
                        }
                    }
                }
            } break;

            case INIMIGO_SENTINELA: RogueFasesElite(b, e); RogueAtualizarSentinela(b, e, dir, dist, dt);     break;
            case INIMIGO_RETRO:     RogueFasesElite(b, e); RogueAtualizarRetro(b, e, dir, dist, ritmo, dt); break;
            case INIMIGO_JANELA:    RogueAtualizarJanela(b, e, dir, dist, dt);      break;

            case INIMIGO_ALUCINACAO:
                // Fantasma: mantém distância, treme (aviso), some e reaparece perto do Thomas atirando em leque
                if (e->estado == 0) {
                    Vector2 lado = { -dir.y * e->lado, dir.x * e->lado };
                    float quer = (dist > 190.0f) ? 1.0f : ((dist < 130.0f) ? -1.0f : 0.0f);
                    e->vel = Vector2Add(Vector2Scale(dir, quer * DEF_INIMIGOS[e->tipo].velocidade), Vector2Scale(lado, 40.0f));
                    if (e->recarga <= 0.0f) { e->estado = 1; e->timer = 0.7f; }
                } else {
                    e->vel = (Vector2){0};
                    e->timer -= dt;
                    if (e->timer <= 0.0f) {
                        Rectangle ar = ArenaRogue();
                        float ang = (float)GetRandomValue(0, 359) * DEG2RAD;
                        float rd = (float)GetRandomValue(120, 200);
                        Vector2 novo = { Clamp(j->pos.x + cosf(ang) * rd, ar.x + 30.0f, ar.x + ar.width - 30.0f),
                                         Clamp(j->pos.y + sinf(ang) * rd, ar.y + 30.0f, ar.y + ar.height - 30.0f) };
                        RogueAnel(b, e->pos, 26.0f, DEF_INIMIGOS[e->tipo].cor);
                        e->pos = novo;
                        RogueEmpurrarParaFora(&e->pos, e->raio, b->obst, b->numObst);
                        RogueAnel(b, e->pos, 26.0f, DEF_INIMIGOS[e->tipo].cor);
                        Vector2 d2 = Vector2Normalize(Vector2Subtract(j->pos, e->pos));
                        for (int k = -1; k <= 1; k++)
                            RogueDisparar(b, e->pos, Vector2Rotate(d2, (float)k * 16.0f * DEG2RAD), 190.0f, 8.0f, 6.0f, false);
                        TocarGrupo(&gIniFantasma, 1.0f, 0.7f);
                        e->estado = 0;
                        e->recarga = 2.4f;
                    }
                }
                break;

            case INIMIGO_VERBOSO:
                // Fala demais: anda devagar, incha (aviso) e solta dois anéis de balas, um logo depois do outro
                if (e->estado == 0) {
                    e->vel = Vector2Scale(dir, DEF_INIMIGOS[e->tipo].velocidade);
                    if (e->recarga <= 0.0f) { e->estado = 1; e->timer = 0.85f; }
                } else {
                    e->vel = (Vector2){0};
                    e->timer -= dt;
                    if (e->timer <= 0.0f) {
                        int balas = (e->hp < e->hpMax * 0.5f) ? 14 : 10;
                        float giro = (e->estado == 2) ? PI / (float)balas : 0.0f;       // o 2º anel preenche as brechas do 1º
                        for (int k = 0; k < balas; k++) {
                            float a = giro + (2.0f * PI / (float)balas) * (float)k;
                            RogueDisparar(b, e->pos, (Vector2){ cosf(a), sinf(a) }, 125.0f, 7.0f, 6.0f, false);
                        }
                        RogueAnel(b, e->pos, 50.0f, DEF_INIMIGOS[e->tipo].cor);
                        TocarGrupo(&gIniAnel, (e->estado == 1) ? 1.15f : 1.25f, 0.65f);
                        if (e->estado == 1) { e->estado = 2; e->timer = 0.4f; }
                        else                { e->estado = 0; e->recarga = 3.0f; }
                    }
                }
                break;
            case INIMIGO_TOKEN:
                if (!RogueAtualizarToken(b, e, dt)) continue;
                break;

            case INIMIGO_NUCLEO:
                // Núcleo de corrupção: parado; de tempos em tempos pulsa um anel de balas (brilha antes)
                e->vel = (Vector2){0};
                e->empurrao = (Vector2){0};
                if (e->estado == 0 && e->recarga <= 0.0f) { e->estado = 1; e->timer = NUCLEO_PREPARO; }
                else if (e->estado == 1) {
                    e->timer -= dt;
                    if (e->timer <= 0.0f) {
                        for (int k = 0; k < NUCLEO_BALAS; k++) {
                            float a = (2.0f * PI / NUCLEO_BALAS) * k + e->anim * 0.7f;
                            RogueDisparar(b, e->pos, (Vector2){ cosf(a), sinf(a) }, 125.0f, 7.0f, 6.0f, false);
                        }
                        RogueAnel(b, e->pos, 56.0f, def->cor);
                        TocarGrupo(&gIniAnel, 0.8f, 0.85f);
                        e->estado = 0;
                        e->recarga = NUCLEO_INTERVALO;
                    }
                }
                break;

            default:
                break;
        }

        // Movimento (com o bônus do Botnet) + empurrão dos golpes
        float velocidade = e->conectado ? BOTNET_BONUS_VELOCIDADE : 1.0f;
        if (j->efeito[EFEITO_LATENCIA] > 0.0f && Vector2Distance(e->pos, j->pos) < LATENCIA_RAIO)
            velocidade *= 1.0f - LATENCIA_LENTIDAO;             // Campo de Latência
        if (e->gelo > 0.0f)
            velocidade *= 0.5f;                                 // Katana (gelo): cache congelado
        e->pos = Vector2Add(e->pos, Vector2Scale(Vector2Add(Vector2Scale(e->vel, velocidade), e->empurrao), dt));
        e->empurrao = Vector2Scale(e->empurrao, fmaxf(0.0f, 1.0f - 8.0f * dt));
        RogueEmpurrarParaFora(&e->pos, e->raio, b->obst, b->numObst);

        // Encostou no jogador
        float contato = RogueDanoContato(e) * (e->elite ? ELITE_CONTATO : 1.0f);
        if (contato > 0.0f && enganado && dist < e->raio + RAIO_CORPO_JOGADOR)
            j->proxyHp -= contato * 3.0f * dt;                  // o holograma vai se desfazendo
        if (contato > 0.0f && Vector2Distance(e->pos, j->pos) < e->raio + RAIO_CORPO_JOGADOR)
            RogueFerirJogador(b, contato);
    }

    // Um inimigo não entra no outro
    for (int a = 0; a < MAX_INIMIGOS; a++) {
        for (int c = a + 1; c < MAX_INIMIGOS; c++) {

            Inimigo *e1 = &b->ini[a], *e2 = &b->ini[c];

            if (!e1->ativo || !e2->ativo)
                continue;

            Vector2 d = Vector2Subtract(e2->pos, e1->pos);
            float dist = Vector2Length(d), min = e1->raio + e2->raio;

            // O núcleo de corrupção não sai do lugar: só o outro é empurrado
            float w1 = (e1->tipo == INIMIGO_NUCLEO || e1->tipo == INIMIGO_JANELA) ? 0.0f : 1.0f;
            float w2 = (e2->tipo == INIMIGO_NUCLEO || e2->tipo == INIMIGO_JANELA) ? 0.0f : 1.0f;
            if (dist < min && dist > 0.01f && w1 + w2 > 0.0f) {
                Vector2 empurra = Vector2Scale(d, (min - dist) / dist / (w1 + w2));
                e1->pos = Vector2Subtract(e1->pos, Vector2Scale(empurra, w1));
                e2->pos = Vector2Add(e2->pos, Vector2Scale(empurra, w2));
            }
        }
    }
}

// Um disparo bateu perto da parede corrompida: ela racha e, sem vida, abre a passagem secreta
void RogueAcertarParede(Biblioteca *b, ProgressoCorredor *prog, const Projetil *p)
{
    Rectangle r = PAREDE_SECRETA_ROGUE[b->paredeSecreta];
    Rectangle perto = { r.x - 16, r.y - 16, r.width + 32, r.height + 32 };

    if (!CheckCollisionPointRec(p->pos, perto))
        return;

    b->hpParede -= p->dano;
    RogueParticulas(b, p->pos, MAGENTA, 5, 120);

    if (b->hpParede > 0.0f) {
        TocarSomTom(sfxSelecao, 0.4f);
        return;
    }

    b->passagemAberta = true;
    b->mapa.no[b->mapa.no[b->noAtual].secreta].escondida = false;
    AlterarPontos(prog, 5);
    RogueAviso(b, "PASSAGEM SECRETA! (abre com a sala limpa)");
    RogueParticulas(b, CentroRetangulo(r), MAGENTA, 30, 240);
    TocarSomTom(sfxAcerto, 0.8f);
    MostrarEmocaoLuci(EMOCAO_ALEGRE, 2.0f);
    Telemetria("rogue_secreta", "descoberta", 0.0f, b->profundidade);
}

void RogueAtualizarProjeteis(Biblioteca *b, ProgressoCorredor *prog, float dt)
{
    JogadorRogue *j = &b->j;

    for (int i = 0; i < MAX_PROJETEIS; i++) {

        Projetil *p = &b->proj[i];

        if (!p->ativo)
            continue;

        // Projétil Q (Disparo Dedutivo): vai atrás do alvo marcado
        Vector2 alvoQ;
        if (p->alvoDedutivo != 0 && RoguePosAlvoDedutivo(b, p->alvoDedutivo, &alvoQ) && Vector2Distance(alvoQ, p->pos) > 1.0f)
            p->vel = Vector2Scale(Vector2Normalize(Vector2Subtract(alvoQ, p->pos)), DEDUTIVO_VEL);

        // Teleguiado (Roteador, Botnet Reverso): vira aos poucos para o inimigo mais perto
        if (p->guiado) {
            int alvo = RogueInimigoMaisPerto(b, p->pos, ROTEADOR_AQUISICAO, p->ultimoAlvo - 1);
            if (alvo >= 0) {
                float vel = Vector2Length(p->vel);
                float atual = atan2f(p->vel.y, p->vel.x);
                Vector2 d = Vector2Subtract(b->ini[alvo].pos, p->pos);
                float diferenca = atan2f(d.y, d.x) - atual;
                while (diferenca > PI)  diferenca -= 2.0f * PI;
                while (diferenca < -PI) diferenca += 2.0f * PI;
                atual += Clamp(diferenca, -ROTEADOR_GIRO * dt, ROTEADOR_GIRO * dt);
                p->vel = (Vector2){ cosf(atual) * vel, sinf(atual) * vel };
            }
        }

        p->pos = Vector2Add(p->pos, Vector2Scale(p->vel, dt));

        // Alcance e tempo de vida (as armas novas não voam para sempre)
        if (p->alcance > 0.0f) {
            p->alcance -= Vector2Length(p->vel) * dt;
            if (p->alcance <= 0.0f) { RogueFimProjetil(b, prog, p); continue; }
        }
        if (p->vida > 0.0f) {
            p->vida -= dt;
            if (p->vida <= 0.0f) { RogueFimProjetil(b, prog, p); continue; }
        }

        // Saiu da arena ou bateu numa estante (o projétil Q passa por cima delas)
        bool bateu = !CheckCollisionPointRec(p->pos, ArenaRogue());
        for (int k = 0; k < b->numObst && !bateu && p->alvoDedutivo == 0; k++)
            bateu = CheckCollisionPointRec(p->pos, b->obst[k]);

        if (bateu) {
            if (p->doJogador && b->temParedeSecreta && !b->passagemAberta)
                RogueAcertarParede(b, prog, p);
            RogueFimProjetil(b, prog, p);
            continue;
        }

        if (p->doJogador) {

            if (b->chefe.ativo && Vector2Distance(p->pos, b->chefe.pos) < CHEFE_RAIO + p->raio) {
                bool feria = RogueChefeVulneravel(&b->chefe);
                bool principal = (p->alvoDedutivo == 0 && !p->secundario);
                p->ativo = false;
                RogueFerirChefe(b, p->dano * ((feria && principal) ? RogueAnalisar(b, -1) : 1.0f), p->pos);
                if (feria && principal)
                    RogueMarcarDedutivo(b, &b->chefe.marca, &b->chefe.tempoMarca, -1, b->chefe.pos);
                if (p->arma == ARMA_ORBE && !p->secundario)
                    RogueExplodirOrbe(b, prog, p->pos);
                continue;
            }

            if (RogueLuciAcerto(b, prog, p))
                continue;

            if (RogueSilogistaAcerto(b, prog, p)) {
                if (p->arma == ARMA_ORBE && !p->secundario && b->silogista.etapa == SIL_VULNERAVEL)
                    RogueExplodirOrbe(b, prog, p->pos);
                continue;
            }

            // Barreira de energia do Firewall segura os tiros (o projétil Q passa)
            if (p->alvoDedutivo == 0 && RogueBarreiraBloqueia(b, p->pos, p->raio)) {
                static double ultimoBloqueio = -10.0;
                RogueFimProjetil(b, prog, p);
                RogueParticulas(b, p->pos, (Color){ 0, 228, 48, 255 }, 4, 90);
                if (SomLiberado(&ultimoBloqueio, 0.07)) TocarGrupo(&gIniBloqueio, 1.15f, 0.5f);
                continue;
            }

            for (int k = 0; k < MAX_INIMIGOS; k++) {

                Inimigo *e = &b->ini[k];

                if (!e->ativo || e->surgindo > 0.0f || k == p->ultimoAlvo - 1 || Vector2Distance(p->pos, e->pos) > e->raio + p->raio)
                    continue;

                RogueAcertarInimigo(b, prog, p, k);
                break;
            }

        } else if (j->efeito[EFEITO_PROXY] > 0.0f && Vector2Distance(p->pos, j->proxyPos) < RAIO_CORPO_JOGADOR + p->raio) {

            p->ativo = false;                               // a bala bate no holograma
            j->proxyHp -= p->dano;
            RogueParticulas(b, p->pos, SKYBLUE, 4, 80);

        } else if (Vector2Distance(p->pos, j->pos) < RAIO_CORPO_JOGADOR + p->raio &&
                   j->invulneravel <= 0.0f && j->tempoEsquiva <= 0.0f) {

            p->ativo = false;
            RogueFerirJogador(b, p->dano);
        }
    }
}

// Fragmentos no chão voam até o jogador; itens caídos são pegos ao encostar
void RogueAtualizarColetaveis(Biblioteca *b, float dt)
{
    for (int i = 0; i < MAX_COLETAVEIS; i++) {

        Coletavel *c = &b->col[i];

        if (!c->ativo)
            continue;

        c->anim += dt;
        c->pos = Vector2Add(c->pos, Vector2Scale(c->vel, dt));
        c->vel = Vector2Scale(c->vel, fmaxf(0.0f, 1.0f - 5.0f * dt));
        RogueEmpurrarParaFora(&c->pos, 6.0f, b->obst, b->numObst);

        float dist = Vector2Distance(c->pos, b->j.pos);

        if (c->anim > 0.4f && dist < RAIO_IMA)
            c->pos = Vector2Add(c->pos, Vector2Scale(Vector2Normalize(Vector2Subtract(b->j.pos, c->pos)), 320.0f * dt));

        if (c->anim > 0.4f && dist < RAIO_CORPO_JOGADOR + 8.0f) {

            c->ativo = false;

            if (c->fragmento) {
                b->perm->fragmentos++;
                RogueParticulas(b, c->pos, (Color){ 0, 228, 255, 255 }, 6, 100);
                TocarSomTom(sfxPegar, 1.5f);
            } else if (ITENS[c->item].temporario) {
                RogueAtivarTemporario(b, c->item);
                RogueParticulas(b, c->pos, CORES_RARIDADE[ITENS[c->item].raridade], 14, 160);
                TocarSomTom(sfxAcerto, 1.4f);
            } else {
                if (b->j.itens[c->item] < ITENS[c->item].maxPilha)
                    b->j.itens[c->item]++;
                RogueAviso(b, TextFormat("+1 %s", ITENS[c->item].nome));
                RogueParticulas(b, c->pos, CORES_RARIDADE[ITENS[c->item].raridade], 14, 160);
                TocarSomTom(sfxAcerto, 1.2f);
            }
        }
    }
}
