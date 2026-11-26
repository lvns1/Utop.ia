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

// ---------- ALUCINADOR ----------

void RogueIniciarChefe(Biblioteca *b)
{
    ChefeRogue *c = &b->chefe;

    *c = (ChefeRogue){0};
    c->ativo = true;
    c->fase = 1;
    c->hp = CHEFE_HP;
    c->pos = (Vector2){ 400, 170 };
    c->destino = c->pos;
    c->padrao = PADRAO_REPOSICIONAR;
    c->tempoPadrao = 1.2f;
    c->proximoPadrao = PADRAO_LEQUE;
    c->afirmacaoAtual = -1;
    c->tempoAfirm = 1.5f;

    // Ordem das afirmações: sorteada a cada luta
    for (int i = 0; i < NUM_AFIRMACOES_ALUCINADOR; i++)
        c->ordemAfirm[i] = i;

    for (int i = NUM_AFIRMACOES_ALUCINADOR - 1; i > 0; i--) {
        int k = GetRandomValue(0, i);
        int t = c->ordemAfirm[i]; c->ordemAfirm[i] = c->ordemAfirm[k]; c->ordemAfirm[k] = t;
    }
}

// Depois de cada ataque ele troca de lugar (na fase 2, mais embaixo, longe da faixa das afirmações)
static void RogueChefeReposicionar(ChefeRogue *c)
{
    c->padrao = PADRAO_REPOSICIONAR;
    c->tempoPadrao = 1.0f;
    c->destino = (Vector2){ (float)GetRandomValue(140, 660),
                            (float)((c->fase == 1) ? GetRandomValue(120, 260) : GetRandomValue(240, 320)) };
}

static void RogueChefeComecarPadrao(Biblioteca *b)
{
    ChefeRogue *c = &b->chefe;
    float ritmo = ((c->fase == 2) ? 0.7f + 0.25f * (float)c->surto : 1.0f) * RogueRitmoChefe(b);     // fase 2: mais devagar; os surtos e a adaptação aceleram

    c->padrao = (PadraoChefe)c->proximoPadrao;
    c->cronoTiro = 0.3f;

    // Revezamento: fase 1 = leque, espiral, investida, ecos; fase 2 = leque, espiral, ecos
    switch (c->padrao) {
        case PADRAO_LEQUE:     c->proximoPadrao = PADRAO_ESPIRAL; break;
        case PADRAO_ESPIRAL:   c->proximoPadrao = (c->fase == 1 || c->surto >= 1) ? PADRAO_INVESTIDA : PADRAO_ECOS; break;
        case PADRAO_INVESTIDA: c->proximoPadrao = PADRAO_ECOS; break;
        default:               c->proximoPadrao = PADRAO_LEQUE; break;
    }

    if (c->padrao == PADRAO_LEQUE) {
        c->rajadas = 3;
        c->tempoPadrao = 2.4f / ritmo;
    }
    else if (c->padrao == PADRAO_ESPIRAL) {
        c->tempoPadrao = 3.0f;
        c->cronoTiro = 0.0f;
        TocarGrupo(&gChefeEspiral, 1.0f, 0.6f);      // o rotor dura os 3 s da espiral
    }
    else if (c->padrao == PADRAO_INVESTIDA) {
        c->etapaInvestida = 0;
        c->tempoPadrao = 0.8f;                       // o corredor avisa para onde ele vai
        TocarGrupo(&gChefeMira, 1.0f, 0.7f);
    }
    else {
        c->tempoPadrao = 1.2f;
        for (int k = 0; k < 2 + c->surto; k++) {
            Inimigo *e = RogueCriarInimigo(b, INIMIGO_FRAGMENTO, (Vector2){ (k % 2 == 0) ? 120.0f : 680.0f, 260.0f + 40.0f * (float)(k / 2) });
            if (e != NULL) { e->recarga = 1.0f; RogueAnel(b, e->pos, 34.0f, MAGENTA); }
        }
        if (c->surto >= 2) RogueCriarInimigo(b, INIMIGO_SPAMMER, (Vector2){ 400.0f, 470.0f });
        TocarGrupo(&gChefeInvoca, 1.0f, 0.75f);
    }
}

// Errou (ou o tempo acabou): anel de balas e mais Fragmentos
static void RogueAtaqueEspecialChefe(Biblioteca *b)
{
    ChefeRogue *c = &b->chefe;

    for (int k = 0; k < 18; k++) {
        float ang = (2.0f * PI / 18.0f) * k + c->anim;
        RogueDisparar(b, c->pos, (Vector2){ cosf(ang), sinf(ang) }, 190.0f, CHEFE_TIRO_DANO, 6.0f, false);
    }

    for (int k = 0; k < 2; k++)
        RogueCriarInimigo(b, INIMIGO_FRAGMENTO, (Vector2){ c->pos.x + ((k == 0) ? -90.0f : 90.0f), c->pos.y });

    RogueParticulas(b, c->pos, MAGENTA, 30, 260);
    RogueAnel(b, c->pos, 110.0f, MAGENTA);
    TocarGrupo(&gIniAnel, 0.7f, 0.9f);
}

static void RogueNovaAfirmacao(ChefeRogue *c)
{
    c->afirmacaoAtual = c->ordemAfirm[c->proximaAfirm % NUM_AFIRMACOES_ALUCINADOR];
    c->proximaAfirm++;
    c->tempoAfirm = TEMPO_AFIRMACAO;
    c->mostrandoResultado = false;
    c->usouManualAfirm = false;
    c->dicaAfirm[0] = '\0';
    c->dicaAfirmLuci = false;
    c->luciMentiuAfirm = false;
    c->opcaoDicaAfirm = -1;
    c->verificouAfirm = false;
}

// C = CONFIÁVEL, F = PROBLEMÁTICA. Acertou: escudo aberto. Errou: ataque especial.
static void RogueResponderAfirmacao(Biblioteca *b, ProgressoCorredor *prog, bool disseConfiavel, bool tempoEsgotado)
{
    ChefeRogue *c = &b->chefe;
    const AfirmacaoChefe *a = &AFIRMACOES_ALUCINADOR[c->afirmacaoAtual];
    bool certo = !tempoEsgotado && (disseConfiavel == a->confiavel);

    // Manual do Conhecimento: o primeiro erro não conta
    if (!certo && !tempoEsgotado && b->j.itens[ITEM_MANUAL] > 0 && !c->usouManualAfirm) {
        b->j.itens[ITEM_MANUAL]--;
        c->usouManualAfirm = true;
        c->tempoAfirm += 3.0f;
        snprintf(c->dicaAfirm, sizeof(c->dicaAfirm), "MANUAL DO CONHECIMENTO: não foi essa. Pense de novo!");
        c->dicaAfirmLuci = false;
        TocarSom(sfxErro);
        return;
    }

    c->mostrandoResultado = true;
    c->acertouAfirm = certo;
    c->tempoAfirm = TEMPO_RESULTADO_AFIRM;
    b->desafiosRespondidos++;
    RogueRegistrarResposta(b, certo);

    // Confiar ou verificar (como nos desafios)
    if (!tempoEsgotado && c->dicaAfirmLuci && c->opcaoDicaAfirm >= 0 && !c->verificouAfirm && (disseConfiavel ? 1 : 0) == c->opcaoDicaAfirm) {
        b->perm->confiouCego++;
        RogueHabitoLuci(b, true);
        if (c->luciMentiuAfirm) { b->perm->confiouAlucinacao++; RogueMudarEstabilidade(b, -1, "você acreditou na alucinação"); }
        else if (certo)         AlterarPontos(prog, BONUS_CONFIAR_PONTOS);
    }
    if (certo && !c->dicaAfirmLuci)
        RogueMudarEstabilidade(b, 1, "resolvido sem ajuda");

    if (certo) {
        c->janela = CHEFE_JANELA;
        AlterarPontos(prog, PONTOS_AFIRMACAO);
        RogueParticulas(b, c->pos, GOLD, 30, 240);
        RogueAviso(b, "ESCUDO ABERTO! Ataque agora!");
        RogueAnel(b, c->pos, 120.0f, GOLD);
        TocarSom(sfxAcerto);
        TocarGrupo(&gChefeEscudo, 1.0f, 0.85f);
    } else {
        AlterarPontos(prog, -PENALIDADE_ERRO);
        RogueAtaqueEspecialChefe(b);
        RogueAviso(b, tempoEsgotado ? "TEMPO ESGOTADO! Ataque especial!" : "ERRADO! Ataque especial!");
        RogueErroVerificacao(b, a->texto, a->explicacao);
    }

    Telemetria("rogue_chefe_afirmacao", certo ? "certo" : (tempoEsgotado ? "tempo" : "errado"),
               TEMPO_AFIRMACAO - c->tempoAfirm, c->afirmacaoAtual);
}

// Fase 2: as afirmações aparecem no alto da arena e o combate continua
static void RogueAtualizarAfirmacoes(Biblioteca *b, ProgressoCorredor *prog, float dt)
{
    ChefeRogue *c = &b->chefe;

    // Sem afirmação na tela: espera o escudo fechar e a pausa acabar
    if (c->afirmacaoAtual < 0) {
        if (c->janela > 0.0f) return;
        c->tempoAfirm -= dt;
        if (c->tempoAfirm <= 0.0f) RogueNovaAfirmacao(c);
        return;
    }

    c->tempoAfirm -= dt;

    if (c->mostrandoResultado) {
        if (c->tempoAfirm <= 0.0f) {
            c->afirmacaoAtual = -1;
            c->tempoAfirm = INTERVALO_AFIRMACAO;
        }
        return;
    }

    const AfirmacaoChefe *a = &AFIRMACOES_ALUCINADOR[c->afirmacaoAtual];

    if (IsKeyPressed(KEY_L) && b->j.itens[ITEM_LENTE] > 0) {
        b->j.itens[ITEM_LENTE]--;
        snprintf(c->dicaAfirm, sizeof(c->dicaAfirm), "LENTE: %s", a->dica);
        c->dicaAfirmLuci = false;
        TocarSomTom(sfxPegar, 1.3f);
    }

    // Dica da Luci: custa estabilidade e pode vir alucinada (sem motivo e com certeza absoluta)
    if (IsKeyPressed(KEY_H) && !c->dicaAfirmLuci) {
        c->dicaAfirmLuci = true;
        if (RogueEstab(b) <= 0) {
            c->opcaoDicaAfirm = -1;
            snprintf(c->dicaAfirm, sizeof(c->dicaAfirm), "Luci: Estou sobrecarregada... decida sem mim.");
        } else {
            RogueMudarEstabilidade(b, -1, NULL);
            c->luciMentiuAfirm = GetRandomValue(1, 100) <= RogueChanceLuciErrar(b);
            bool diz = c->luciMentiuAfirm ? !a->confiavel : a->confiavel;
            c->opcaoDicaAfirm = diz ? 1 : 0;
            if (c->luciMentiuAfirm)
                snprintf(c->dicaAfirm, sizeof(c->dicaAfirm), "Luci: É %s. Tenho 100%% de certeza!", diz ? "CONFIÁVEL" : "PROBLEMÁTICA");
            else
                snprintf(c->dicaAfirm, sizeof(c->dicaAfirm), "Luci: Eu diria %s. %s", diz ? "CONFIÁVEL" : "PROBLEMÁTICA", a->dica);
        }
        TocarSom(sfxSelecao);
    }

    // V: verificar a dica da Luci (Lente ou 1 Fragmento)
    if (IsKeyPressed(KEY_V) && c->dicaAfirmLuci && c->opcaoDicaAfirm >= 0 && !c->verificouAfirm) {
        bool pagou = true;
        if (b->j.itens[ITEM_LENTE] > 0)                       b->j.itens[ITEM_LENTE]--;
        else if (b->perm->fragmentos >= CUSTO_VERIFICAR_FC)   b->perm->fragmentos -= CUSTO_VERIFICAR_FC;
        else { pagou = false; RogueRecadoLuci(b, "Sem Lente nem Fragmento para verificar."); }

        if (pagou) {
            c->verificouAfirm = true;
            b->perm->verificou++;
            RogueHabitoLuci(b, false);
            if (c->luciMentiuAfirm) {
                b->perm->alucinacoesPegas++;
                snprintf(c->dicaAfirm, sizeof(c->dicaAfirm), "VERIFICADO: a dica da Luci NÃO confere!");
                RogueMudarEstabilidade(b, 1, "você pegou a alucinação");
            } else {
                snprintf(c->dicaAfirm, sizeof(c->dicaAfirm), "VERIFICADO: confere. %s", a->dica);
            }
        }
    }

    if (IsKeyPressed(KEY_C))           RogueResponderAfirmacao(b, prog, true, false);
    else if (IsKeyPressed(KEY_F))      RogueResponderAfirmacao(b, prog, false, false);
    else if (c->tempoAfirm <= 0.0f)    RogueResponderAfirmacao(b, prog, false, true);
}

void RogueAtualizarChefe(Biblioteca *b, ProgressoCorredor *prog, float dt)
{
    ChefeRogue *c = &b->chefe;
    JogadorRogue *j = &b->j;

    if (!c->ativo || !c->apresentado)
        return;

    c->anim += dt;
    if (c->flash > 0.0f) c->flash -= dt;

    // Derrotado: se desfaz e depois vem a conversa final
    if (c->morrendo > 0.0f) {
        c->morrendo -= dt;
        RogueParticulas(b, (Vector2){ c->pos.x + GetRandomValue(-40, 40), c->pos.y + GetRandomValue(-40, 40) },
                        (GetRandomValue(0, 1) == 0) ? MAGENTA : SKYBLUE, 3, 260);
        if (c->morrendo <= 0.0f) {
            c->ativo = false;
            b->conversaPendente = &dialogoAlucinadorVitoria;
            b->objConversa = OBJ_ALUCINADOR_VITORIA;
        }
        return;
    }

    if (c->hp <= 0.0f) {
        c->hp = 0.0f;
        c->morrendo = 1.6f;
        memset(b->proj, 0, sizeof(b->proj));
        for (int i = 0; i < MAX_INIMIGOS; i++) b->ini[i].ativo = false;
        b->pausaImpacto = 0.2f;
        GrupoParar(&gChefeEspiral);
        TocarGrupo(&gChefeMorte, 1.0f, 1.0f);
        RogueAnel(b, c->pos, 220.0f, MAGENTA);
        RogueAnel(b, c->pos, 120.0f, WHITE);
        return;
    }

    // Metade da vida: escudo de distorção e afirmações
    if (c->fase == 1 && c->hp <= CHEFE_HP * CHEFE_FASE2) {
        c->fase = 2;
        for (int i = 0; i < MAX_PROJETEIS; i++)
            if (!b->proj[i].doJogador) b->proj[i].ativo = false;
        RogueChefeReposicionar(c);
        c->proximoPadrao = PADRAO_LEQUE;
        c->afirmacaoAtual = -1;
        c->tempoAfirm = 1.5f;
        GrupoParar(&gChefeEspiral);
        TocarGrupo(&gChefeVirada, 1.0f, 0.9f);
        RogueAnel(b, c->pos, 180.0f, MAGENTA);
        b->conversaPendente = &dialogoAlucinadorFase2;
        b->objConversa = OBJ_NENHUM;
        Telemetria("rogue_chefe_fase2", "", (float)(GetTime() - b->inicio), 0);
        return;
    }

    // Fase 3 (SURTO) e fase 4 (COLAPSO): ele perde o controle, ataca mais e em mais direções
    if (c->fase == 2 && c->surto < 2 && c->hp <= CHEFE_HP * ((c->surto == 0) ? 0.28f : 0.12f)) {
        c->surto++;
        for (int i = 0; i < MAX_PROJETEIS; i++)
            if (!b->proj[i].doJogador) b->proj[i].ativo = false;
        RogueAnel(b, c->pos, 160.0f, (Color){ 255, 0, 255, 255 });
        RogueParticulas(b, c->pos, (Color){ 255, 0, 255, 255 }, 24, 260);
        b->tremor = fmaxf(b->tremor, 0.25f);
        GrupoParar(&gChefeEspiral);
        if (c->surto == 1) RogueFaixaVirada(b, "FASE 3: SURTO", "ele alucina mais rápido e em mais direções", MAGENTA);
        else               RogueFaixaVirada(b, "FASE 4: COLAPSO", "última chance: aguente!", (Color){ 255, 60, 90, 255 });
        RogueChefeReposicionar(c);
        c->proximoPadrao = PADRAO_ESPIRAL;
    }

    if (c->janela > 0.0f) c->janela -= dt;

    Vector2 paraJogador = Vector2Subtract(j->pos, c->pos);
    float dist = Vector2Length(paraJogador);
    Vector2 dir = (dist > 0.01f) ? Vector2Scale(paraJogador, 1.0f / dist) : (Vector2){ 0, 1 };
    float ritmo = ((c->fase == 2) ? 0.7f + 0.25f * (float)c->surto : 1.0f) * RogueRitmoChefe(b);

    c->tempoPadrao -= dt;
    c->cronoTiro -= dt;

    switch (c->padrao) {

        case PADRAO_REPOSICIONAR: {
            Vector2 ate = Vector2Subtract(c->destino, c->pos);
            if (Vector2Length(ate) > 4.0f)
                c->pos = Vector2Add(c->pos, Vector2Scale(Vector2Normalize(ate), CHEFE_VELOCIDADE * dt));
            if (c->tempoPadrao <= 0.0f)
                RogueChefeComecarPadrao(b);
        } break;

        case PADRAO_LEQUE:
            // 3 rajadas de 5 balas mirando o jogador
            if (c->cronoTiro <= 0.0f && c->rajadas > 0) {
                for (int k = -2 - c->surto; k <= 2 + c->surto; k++)
                    RogueDisparar(b, c->pos, Vector2Rotate(dir, k * 12.0f * DEG2RAD), CHEFE_TIRO_VEL, CHEFE_TIRO_DANO, 6.0f, false);
                c->rajadas--;
                c->cronoTiro = 0.6f / ritmo;
                TocarGrupo(&gChefeRajada, 1.0f - 0.06f * (float)c->surto, 0.8f);
            }
            if (c->tempoPadrao <= 0.0f) RogueChefeReposicionar(c);
            break;

        case PADRAO_ESPIRAL:
            // Dois (fase 1) ou três (fase 2) braços girando
            if (c->cronoTiro <= 0.0f) {
                int bracos = (c->fase == 1) ? 2 : 3 + c->surto;
                for (int k = 0; k < bracos; k++) {
                    float ang = c->anguloEspiral + k * (2.0f * PI / bracos);
                    RogueDisparar(b, c->pos, (Vector2){ cosf(ang), sinf(ang) }, CHEFE_TIRO_VEL * 0.85f, CHEFE_TIRO_DANO, 5.0f, false);
                }
                c->anguloEspiral += 0.32f;
                c->cronoTiro = 0.09f / ritmo;
            }
            if (c->tempoPadrao <= 0.0f) RogueChefeReposicionar(c);
            break;

        case PADRAO_INVESTIDA:
            if (c->etapaInvestida == 0) {
                // Mira (a linha vermelha mostra para onde ele vai)
                if (c->tempoPadrao > 0.2f) c->dirInvestida = dir;
                if (c->tempoPadrao <= 0.0f) {
                    c->etapaInvestida = 1;
                    c->tempoPadrao = 0.45f;
                    TocarGrupo(&gChefeInvestida, 1.0f, 0.9f);
                }
            } else {
                c->pos = Vector2Add(c->pos, Vector2Scale(c->dirInvestida, CHEFE_INVESTIDA_VEL * dt));
                if (c->tempoPadrao <= 0.0f) {
                    int anel = 12 + 4 * c->surto;
                    for (int k = 0; k < anel; k++) {
                        float ang = (2.0f * PI / (float)anel) * k;
                        RogueDisparar(b, c->pos, (Vector2){ cosf(ang), sinf(ang) }, CHEFE_TIRO_VEL * 0.8f, CHEFE_TIRO_DANO, 6.0f, false);
                    }
                    RogueAnel(b, c->pos, 90.0f, MAGENTA);
                    TocarGrupo(&gIniAnel, 0.75f, 0.85f);
                    RogueChefeReposicionar(c);
                }
            }
            break;

        default:    // PADRAO_ECOS: os Fragmentos já foram chamados
            if (c->tempoPadrao <= 0.0f) RogueChefeReposicionar(c);
            break;
    }

    RogueEmpurrarParaFora(&c->pos, CHEFE_RAIO, b->obst, b->numObst);

    if (dist < CHEFE_RAIO + RAIO_CORPO_JOGADOR)
        RogueFerirJogador(b, CHEFE_DANO_CONTATO);

    if (c->fase == 2)
        RogueAtualizarAfirmacoes(b, prog, dt);
}
