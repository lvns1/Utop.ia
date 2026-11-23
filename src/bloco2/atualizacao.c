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

// ---------- Atualização ----------

void RogueAtualizarJogador(Biblioteca *b, ProgressoCorredor *prog, float dt)
{
    JogadorRogue *j = &b->j;
    Vector2 antes = j->pos;

    // Movimento (WASD)
    Vector2 mov = {0};
    if (IsKeyDown(KEY_D)) mov.x += 1.0f;
    if (IsKeyDown(KEY_A)) mov.x -= 1.0f;
    if (IsKeyDown(KEY_S)) mov.y += 1.0f;
    if (IsKeyDown(KEY_W)) mov.y -= 1.0f;

    bool andando = Vector2Length(mov) > 0.0f;
    if (andando) mov = Vector2Normalize(mov);

    // Mira: setas (mira e atira) ou mouse
    Vector2 setas = {0};
    if (IsKeyDown(KEY_RIGHT)) setas.x += 1.0f;
    if (IsKeyDown(KEY_LEFT))  setas.x -= 1.0f;
    if (IsKeyDown(KEY_DOWN))  setas.y += 1.0f;
    if (IsKeyDown(KEY_UP))    setas.y -= 1.0f;

    bool usandoSetas = Vector2Length(setas) > 0.0f;

    if (usandoSetas) {
        j->mira = Vector2Normalize(setas);
        j->miraSetas = true;
    } else if (Vector2Length(GetMouseDelta()) > 0.5f || IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        j->miraSetas = false;
    }

    if (!j->miraSetas) {
        Vector2 d = Vector2Subtract(GetMousePosition(), j->pos);
        if (Vector2Length(d) > 4.0f)
            j->mira = Vector2Normalize(d);
    }

    j->direcao = DirecaoDoVetor(j->mira);

    // Tempos
    if (j->cronoTiro > 0.0f)      j->cronoTiro -= dt;
    if (j->recargaEsquiva > 0.0f) j->recargaEsquiva -= dt;
    if (j->recargaPulso > 0.0f)   j->recargaPulso -= dt;
    if (j->invulneravel > 0.0f)   j->invulneravel -= dt;
    if (j->efeitoPulso > 0.0f)    j->efeitoPulso -= dt;

    j->ep += (ROGUE_EP_POR_SEGUNDO + j->permRegen) * dt;
    if (j->ep > RogueEpMax(b)) j->ep = RogueEpMax(b);

    // Bloco 3: PARRY (V ou botão direito, só na arena da Luci). A janela é curta e
    // uma janela perdida tem recarga (não dá para ficar apertando sem parar).
    JogadorParryState *pr = &j->parry;
    bool arenaLuci = (b->tipoSala == SALA_LUCI);
    bool arenaParry = arenaLuci || (b->tipoSala == SALA_SILOGISTA && b->silogista.modoBH)      // na Câmara bullet-hell: rebater símbolos
                   || (b->tipoSala == SALA_MESTRE && b->mestre.etapa >= MT_PARRY);              // fim do Bloco 1: o parry é ensinado aqui
    if (pr->cooldownParry > 0.0f) pr->cooldownParry -= dt;
    if (pr->golpeAnim > 0.0f)     pr->golpeAnim -= dt;
    if (pr->tempoHitFreeze > 0.0f) { pr->tempoHitFreeze -= dt; if (pr->tempoHitFreeze <= 0.0f) pr->hitFreezeAtivo = false; }
    if (pr->emParry) {
        pr->tempoJanelaParry -= dt;
        if (pr->tempoJanelaParry <= 0.0f) { pr->emParry = false; pr->cooldownParry = PARRY_RECARGA; }
    }
    if (arenaParry && !pr->emParry && pr->cooldownParry <= 0.0f && (IsKeyPressed(KEY_V) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))) {
        pr->emParry = true;
        pr->tempoJanelaParry = MestreJanelaParry(b);
        TocarGrupo(&gJogParry, 1.0f, 0.7f);
    }

    // Esquiva (para onde está andando; parado, para onde está mirando)
    // Buffer de Esquiva: uma 2ª carga, com a recarga própria
    bool segundaCarga = RogueUp(j, UP_ESQUIVA_DUPLA) > 0 && j->recargaEsquiva2 <= 0.0f;

    if (IsKeyPressed(KEY_SPACE) && (j->recargaEsquiva <= 0.0f || segundaCarga) && j->tempoEsquiva <= 0.0f) {
        if (j->recargaEsquiva <= 0.0f) j->recargaEsquiva = RogueRecargaEsquiva(j);
        else                           j->recargaEsquiva2 = RogueRecargaEsquiva(j) * 1.5f;
        j->dirEsquiva = andando ? mov : j->mira;
        j->tempoEsquiva = ESQUIVA_DURACAO;
        b->perm->esquivasRun++;
        if (j->invulneravel < ESQUIVA_INVULNERAVEL) j->invulneravel = ESQUIVA_INVULNERAVEL;
        TocarGrupo(&gJogEsquiva, 1.0f, 0.75f);

        // Contra-Ataque Heurístico (Passo Fantasma dobra o tempo)
        if (RogueUp(j, UP_POS_ESQUIVA) > 0)
            j->bonusPosEsquiva = ESQUIVA_DURACAO + UP_POS_ESQUIVA_TEMPO * ((RogueUp(j, UP_SIN_FANTASMA) > 0) ? 2.0f : 1.0f);

        // Esquiva Fragmentada: explode onde o jogador estava
        if (RogueUp(j, UP_ESQUIVA_EXPLOSIVA) > 0 && b->tipoSala != SALA_ATRIO)
            RogueCriarArea(b, j->pos, UP_ESQUIVA_EXP_RAIO, 0.25f, 0.35f, 0.0f,
                           (UP_ESQUIVA_EXP_DANO + 12.0f * (float)(RogueUp(j, UP_ESQUIVA_EXPLOSIVA) - 1)) * RogueFatorDano(b), (Color){ 102, 191, 255, 255 });

        for (int k = 0; k < MAX_INIMIGOS; k++)
            b->ini[k].atingidoEsquiva = false;
    }

    // Silogista (bullet-hell): FOCUS MODE. Segurar SHIFT, ou continuar segurando ESPAÇO depois da
    // esquiva, desacelera o mundo (dt * 0,3). Gasta EP; o Thomas ainda anda, mais devagar.
    Silogista *sg = &b->silogista;
    if (b->tipoSala == SALA_SILOGISTA && sg->ativo && sg->modoBH && sg->apresentado && sg->morrendo <= 0.0f) {
        bool segurando = IsKeyDown(KEY_LEFT_SHIFT) || (IsKeyDown(KEY_SPACE) && j->tempoEsquiva <= 0.0f);
        sg->bh.modoFocoAtivo = segurando && j->ep > 1.0f;
        sg->bh.fatorTempo = sg->bh.modoFocoAtivo ? SIL_BH_FATOR_FOCO : 1.0f;
        if (sg->bh.modoFocoAtivo)
            j->ep = fmaxf(0.0f, j->ep - SIL_BH_FOCO_EP * dt);
    } else if (sg->bh.modoFocoAtivo) {
        sg->bh.modoFocoAtivo = false;
        sg->bh.fatorTempo = 1.0f;
    }

    if (j->tempoEsquiva > 0.0f) {
        j->tempoEsquiva -= dt;
        j->pos = Vector2Add(j->pos, Vector2Scale(j->dirEsquiva, ESQUIVA_DISTANCIA / ESQUIVA_DURACAO * dt));
        RogueParticulas(b, j->pos, Fade(SKYBLUE, 0.7f), 1, 30);

        // Passo Fantasma: atravessar inimigos durante a esquiva fere (uma vez cada)
        if (RogueUp(j, UP_SIN_FANTASMA) > 0)
            for (int k = 0; k < MAX_INIMIGOS; k++) {
                Inimigo *e = &b->ini[k];
                if (!e->ativo || e->atingidoEsquiva || e->selado || e->surgindo > 0.0f) continue;
                if (Vector2Distance(e->pos, j->pos) > e->raio + RAIO_CORPO_JOGADOR + 6.0f) continue;
                e->atingidoEsquiva = true;
                RogueFerirInimigo(b, prog, e, UP_FANTASMA_DANO * RogueFatorDano(b), j->dirEsquiva);
            }
    } else {
        float vel = RogueVelocidadeJogador(j);
        if (b->silogista.ativo && b->silogista.carregando >= 0)
            vel *= SIL_VELOCIDADE_CARREGANDO;           // carregando um fragmento
        if (sg->bh.modoFocoAtivo)
            vel *= SIL_BH_FOCO_VELOCIDADE;              // Focus Mode: movimentação parcial
        j->pos = Vector2Add(j->pos, Vector2Scale(mov, vel * dt));
    }

    RogueEmpurrarParaFora(&j->pos, RAIO_CORPO_JOGADOR, b->obst, b->numObst);
    AtualizarAnimacao(&j->anim, andando || j->tempoEsquiva > 0.0f, dt);

    // Keylogger: velocidade recente e há quanto tempo ele anda na mesma direção (esquivar quebra a leitura)
    if (dt > 0.0f) {
        Vector2 v = Vector2Scale(Vector2Subtract(j->pos, antes), 1.0f / dt);
        bool mesmaDirecao = Vector2Length(j->velMedia) > 30.0f && Vector2Length(v) > 30.0f &&
                            Vector2DotProduct(Vector2Normalize(v), Vector2Normalize(j->velMedia)) > 0.92f;

        j->tempoMesmaDirecao = (mesmaDirecao && j->tempoEsquiva <= 0.0f) ? j->tempoMesmaDirecao + dt : 0.0f;
        j->velMedia = Vector2Lerp(j->velMedia, v, fminf(1.0f, 10.0f * dt));
    }

    if (j->trocaArma > 0.0f)
        j->trocaArma -= dt;

    // Troca de arma: TAB ou a rodinha do mouse (só com uma segunda arma)
    if ((IsKeyPressed(KEY_TAB) || GetMouseWheelMove() != 0.0f) && j->armaExtra != ARMA_DISPARADOR) {
        j->usandoExtra = !j->usandoExtra;
        j->carga = 0.0f;
        j->trocaArma = 1.3f;
        RogueAviso(b, DEF_ARMAS[RogueArmaAtual(j)].nome);
        TocarSomTom(sfxSelecao, 1.2f);
    }

    // Arma equipada (no átrio não se atira)
    bool atirando = usandoSetas || IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    if (b->tipoSala == SALA_MESTRE && b->mestre.etapa == MT_ARMA)
        atirando = false;                       // fim do Bloco 1: ainda sem a arma

    // Bloco 3: a Luci atordoada e perto -> o ataque vira GOLPE DEDUTIVO (corpo a corpo) em vez de tiro
    if (arenaLuci && b->luci.ativo && b->luci.atordoada && Vector2Distance(j->pos, b->luci.pos) < MELEE_ALCANCE + LUCI_RAIO) {
        bool golpe = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_LEFT) ||
                     IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN);
        atirando = false;
        if (golpe) {
            Vector2 d = Vector2Subtract(b->luci.pos, j->pos);
            pr->dirGolpe = (Vector2Length(d) > 0.1f) ? Vector2Normalize(d) : j->mira;
            pr->golpeAnim = 0.18f;
            RogueGolpeDedutivo(b);
        }
    }

    if (b->tipoSala != SALA_ATRIO)
        RogueUsarArma(b, prog, atirando, dt);

    // Pulso de Integridade (na arena da Luci o botão direito é o parry: lá o Pulso fica só no Q)
    if (b->tipoSala != SALA_ATRIO && ((IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && !arenaParry) || IsKeyPressed(KEY_Q)))
        RoguePulso(b, prog);

    // Itens de usar: 1 = Antivírus, 2 = Pacote de Dados
    if (IsKeyPressed(KEY_ONE) && j->itens[ITEM_ANTIVIRUS] > 0 && j->hp < RogueHpMax(b)) {
        j->itens[ITEM_ANTIVIRUS]--;
        j->hp = fminf(RogueHpMax(b), j->hp + CURA_ANTIVIRUS);
        RogueParticulas(b, j->pos, LIME, 14, 120);
        TocarSomTom(sfxPegar, 0.9f);
    }

    if (IsKeyPressed(KEY_TWO) && j->itens[ITEM_PACOTE_DADOS] > 0 && j->ep < RogueEpMax(b)) {
        j->itens[ITEM_PACOTE_DADOS]--;
        j->ep = fminf(RogueEpMax(b), j->ep + RECARGA_PACOTE);
        RogueParticulas(b, j->pos, SKYBLUE, 14, 120);
        TocarSomTom(sfxPegar, 0.9f);
    }
}

// Anda para manter uma distância do jogador (foge de perto, chega mais se longe, senão anda de lado)
Vector2 RogueManterDistancia(Inimigo *e, Vector2 dir, float dist, float minimo, float maximo, float vel, float dt)
{
    if (fmodf(e->anim, 2.5f) < dt) e->lado = -e->lado;

    if (dist < minimo) return Vector2Scale(dir, -vel);
    if (dist > maximo) return Vector2Scale(dir, vel);
    return Vector2Scale((Vector2){ -dir.y * e->lado, dir.x * e->lado }, vel * 0.6f);
}

// Phisher e a cópia falsa: mira (1), bote (2) e recuo (3). Só o verdadeiro se camufla (0).
void RogueMoverPhisher(Inimigo *e, Vector2 dir, float vel, float dt)
{
    if (e->estado == 0) {
        e->vel = (Vector2){ sinf(e->anim * 0.7f) * 8.0f, 0.0f };     // um arquivo "inofensivo" balançando
    }
    else if (e->estado == 1) {
        e->vel = (Vector2){0};
        e->timer -= dt;
        if (e->timer > 0.12f) e->alvo = dir;
        if (e->timer <= 0.0f) {
            e->estado = 2;
            e->timer = PHISHER_BOTE_DURACAO;
            e->vel = Vector2Scale(e->alvo, PHISHER_BOTE_VEL);
            TocarGrupo(&gIniBote, 0.85f, 0.75f);           // a cópia falsa faz o mesmo barulho (é para enganar)
        }
    }
    else if (e->estado == 2) {
        e->timer -= dt;
        if (e->timer <= 0.0f) {
            e->estado = 3;
            e->timer = PHISHER_RECUO;
        }
    }
    else {
        e->vel = Vector2Scale(dir, -vel * 2.2f);
        e->timer -= dt;
        if (e->timer <= 0.0f) {
            e->estado = 0;
            e->recarga = PHISHER_RECARGA;
        }
    }
}
