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

// ---------- Fim do Bloco 1: a sala da ALUCINAÇÃO MESTRE ----------

bool MestreVulneravel(const MestreTreino *m)
{
    if (m->morrendo > 0.0f || !m->visivel || m->surgindo > 0.0f)
        return false;
    if (m->acao == MA_ATORDOADA || m->acao == MA_ABERTA)
        return true;
    return m->acao == MA_RECUPERA && m->etapa == MT_LUTA;      // brecha menor: só na luta
}

// Tempo que falta até o golpe acertar (o anel em volta do Thomas fecha nesse instante; < 0 = sem golpe)
float MestreTempoAteGolpe(const MestreTreino *m, Vector2 jogador, float telegrafia)
{
    float dist = fmaxf(0.0f, Vector2Distance(m->pos, jogador) - MESTRE_RAIO - RAIO_CORPO_JOGADOR);
    if (m->acao == MA_TELEGRAFA) return m->tempoAcao + dist / MESTRE_INVESTIDA_VEL;
    if (m->acao == MA_INVESTE && !m->acertouGolpe) return dist / MESTRE_INVESTIDA_VEL;
    (void)telegrafia;
    return -1.0f;
}

float MestreTelegrafia(const MestreTreino *m)
{
    return (m->etapa == MT_LUTA) ? MESTRE_TELEGRAFIA * ((m->fase == 2) ? 0.85f : 1.0f) : MESTRE_TELEGRAFIA * 1.3f;
}

// Entra na sala (o main chama ao sair do corredor pela porta da direita)
void RogueEntrarSalaMestre(Biblioteca *b)
{
    MestreTreino *m = &b->mestre;

    b->noAtual = -1;
    b->ladoEntrada = LADO_BAIXO;
    b->fatorVidaInimigos = b->fatorRitmoInimigos = b->fatorDanoSala = 1.0f;
    RogueLimparSala(b, SALA_MESTRE, SPAWN_ROGUE);
    RogueUsarLayout(b, &LAYOUT_CHEFE);
    RogueLimparEfeitos(&b->j);
    b->j.hp = RogueHpMax(b);
    b->j.ep = RogueEpMax(b);
    b->j.direcao = DIR_COSTAS;

    memset(m, 0, sizeof(*m));
    m->ativo = true;
    m->etapa = MT_ARMA;
    m->pos = POS_MESTRE_INICIO;
    m->hp = MESTRE_HP;
    m->fase = 1;
    for (int i = 0; i < MESTRE_ALVOS; i++) { m->alvo[i] = POS_ALVOS_MESTRE[i]; m->alvoVivo[i] = false; }

    b->conversaPendente = &dialogoMestreChegada;
    b->objConversa = OBJ_NENHUM;
    Telemetria("mestre_entrada", "", 0.0f, 0);
}

// Ajuda progressiva: se o jogador empaca numa etapa, a Luci explica de novo (cada vez mais direta)
// e, por fim, deixa a etapa passar. Ninguém fica preso no tutorial.
static void MestreDicaProgressiva(Biblioteca *b)
{
    MestreTreino *m = &b->mestre;
    static const float ESPERA[3] = { 10.0f, 22.0f, 38.0f };
    static const char *DICAS[MT_MELHORIA + 1][3] = {
        { NULL, NULL, NULL },
        { "Luci: Mova o MOUSE sobre um alvo dourado e CLIQUE com o botão esquerdo.",
          "Luci: Os alvos estão lá em cima. O tiro sai na direção do cursor do mouse.",
          "Luci: Deixe o cursor em cima de um alvo e clique. É só isso!" },
        { "Luci: Aperte ESPAÇO para dar um impulso rápido. Faça isso duas vezes.",
          "Luci: Ande com WASD e aperte ESPAÇO: o Thomas dá um pulo e fica invulnerável.",
          "Luci: Pode apertar ESPAÇO a qualquer momento, até sem bala por perto!" },
        { "Luci: Desvie das balas até ela cansar. Com o escudo DOURADO, clique para atirar!",
          "Luci: Só o escudo DOURADO recebe dano. Mire nela e clique rápido nessa hora.",
          "Luci: Fique longe, esquive (ESPAÇO) e atire só quando o escudo brilhar." },
        { "Luci: Quando ela brilhar em VERMELHO e vier em você, aperte V (ou o botão direito).",
          "Luci: Não precisa ser perfeito: aperte V quando o anel vermelho fechar em volta do Thomas.",
          "Luci: Aumentei sua margem de tempo. Aperte V assim que o anel ficar dourado!" },
        { "Luci: A barra azul é o EP. Aperte Q para soltar o PULSO: ele apaga as balas por perto.",
          "Luci: Espere as balas dela chegarem perto do Thomas e aperte Q. O Pulso gasta 35 de EP.",
          "Luci: É só apertar Q! O EP volta sozinho com o tempo." },
        { NULL, NULL, NULL }
    };

    if (m->etapa < MT_TIRO || m->etapa > MT_PULSO)
        return;

    // Depois das três dicas, a Luci deixa a etapa passar
    if (m->dicas >= 3 && m->tempoEtapa > 75.0f) {
        if (m->etapa == MT_TIRO)         for (int a = 0; a < MESTRE_ALVOS; a++) m->alvoVivo[a] = false;
        else if (m->etapa == MT_ESQUIVA) m->esquivas = MESTRE_ESQUIVAS;
        else if (m->etapa == MT_ABERTURA) m->acertosAbertura = MESTRE_ACERTOS_ABERTURA;
        else if (m->etapa == MT_PARRY)   m->golpesLevados = 5;
        else if (m->etapa == MT_PULSO)   m->pulsos = 1;
        RogueRecadoLuci(b, "Luci: Tudo bem, a gente segue. Você aprende na prática!");
        m->tempoEtapa = 0.0f;
        m->dicas = 0;
        Telemetria("mestre_pulo_etapa", "", 75.0f, (int)m->etapa);
        return;
    }

    if (m->dicas >= 3 || m->tempoEtapa < ESPERA[m->dicas] || b->luciMsgTempo > 0.0f)
        return;

    RogueRecadoLuci(b, DICAS[m->etapa][m->dicas]);
    b->luciMsgTempo = 6.0f;
    m->dicas++;
    Telemetria("mestre_dica", "", m->tempoEtapa, (int)m->etapa);
}

// Escolha de TREINO: a mesma tela do jogo, com 2 opções, para o jogador conhecê-la antes de precisar dela.
// Nada é aplicado, gasto ou guardado (a Mestre só avança a etapa quando o jogador confirma).
static void MestreAbrirEscolhaTreino(Biblioteca *b)
{
    b->numOpcoes = 2;
    b->opcoes[0] = (OpcaoEscolha){ OPC_UPGRADE, UP_CADENCIA };      // um atributo...
    b->opcoes[1] = (OpcaoEscolha){ OPC_UPGRADE, UP_PULSO };         // ...e uma habilidade ligada ao Pulso que acabou de usar
    b->escolhaAberta = true;
    b->escolhaTreino = true;
    b->escolhaTreinoFeita = false;
    b->escolhaGratis = true;
    b->modoEscolha = ESCOLHA_NORMAL;
    b->selecaoEscolha = 0;
    b->animEscolha = 0.0f;
    TocarSomTom(sfxAcerto, 1.3f);
}

void MestreMudarEtapa(Biblioteca *b, EtapaMestre etapa)
{
    MestreTreino *m = &b->mestre;

    m->etapa = etapa;
    m->tempoEtapa = 0.0f;
    m->dicas = 0;
    if (etapa == MT_PARRY) m->golpesLevados = 0;           // a ajuda do parry conta só os golpes desta etapa
    m->acao = MA_PAUSA;
    m->tempoAcao = CARTAO_ETAPA_TEMPO + 0.4f;               // ela espera o cartão da etapa sair do meio da tela
    m->proximoGolpe = false;
    if (etapa >= MT_ESQUIVA && etapa != MT_MELHORIA) TocarSomTom(sfxAcerto, 1.1f);     // etapa anterior concluída
    for (int i = 0; i < MAX_PROJETEIS; i++)                 // a etapa nova começa sem balas no ar
        if (!b->proj[i].doJogador) b->proj[i].ativo = false;

    if (etapa == MT_TIRO) {
        for (int i = 0; i < MESTRE_ALVOS; i++) m->alvoVivo[i] = true;
    } else if (etapa == MT_ESQUIVA) {
        m->visivel = true;
        m->surgindo = 1.0f;
        m->tempoAcao = CARTAO_ETAPA_TEMPO + 0.4f;
        RogueParticulas(b, m->pos, MAGENTA, 40, 260);
        RogueAnel(b, m->pos, 120.0f, MAGENTA);
        TocarGrupo(&gChefeInvoca, 0.8f, 0.8f);
    } else if (etapa == MT_PULSO) {
        b->j.ep = RogueEpMax(b);                            // o Pulso precisa de EP: começa cheio
        b->j.recargaPulso = 0.0f;
        m->pulsos = 0;
        m->recargaPulsoAnt = 0.0f;
    } else if (etapa == MT_MELHORIA) {
        MestreAbrirEscolhaTreino(b);
    } else if (etapa == MT_LUTA) {
        b->j.hp = RogueHpMax(b);
        b->j.ep = RogueEpMax(b);
        RogueFaixaVirada(b, "ALUCINAÇÃO MESTRE", "agora é pra valer!", MAGENTA);
    }

    m->falaDepois = FALAS_MESTRE_ETAPA[etapa];         // recado (não para o jogo): entra depois do cartão
    Telemetria("mestre_etapa", "", (float)m->tempoEtapa, (int)etapa);
}

// Parry perfeito na investida: congela, aproxima a câmera e ela fica atordoada (brecha grande)
static void MestreParry(Biblioteca *b, Vector2 ponto)
{
    MestreTreino *m = &b->mestre;
    JogadorRogue *j = &b->j;

    m->acao = MA_ATORDOADA;
    m->tempoAcao = MESTRE_STUN;
    m->parries++;
    RogueRecadoLuci(b, "Luci: PARRY PERFEITO! Ela ficou atordoada. Atire!");
    b->perm->parriesRun++;
    m->pos = Vector2Subtract(m->pos, Vector2Scale(m->dirGolpe, 26.0f));
    RogueEmpurrarParaFora(&m->pos, MESTRE_RAIO, NULL, 0);

    j->parry.emParry = false;
    j->parry.tempoJanelaParry = 0.0f;
    j->parry.cooldownParry = 0.15f;
    j->parry.hitFreezeAtivo = true;
    j->parry.tempoHitFreeze = PARRY_HIT_FREEZE;
    if (j->invulneravel < 0.45f) j->invulneravel = 0.45f;

    b->pausaImpacto = PARRY_HIT_FREEZE;
    b->tremor = 0.3f;
    b->tremorForte = 0.3f;
    b->zoomImpacto = PARRY_ZOOM;
    b->focoZoom = ponto;
    RogueAnel(b, ponto, 150.0f, WHITE);
    RogueAnel(b, ponto, 90.0f, GOLD);
    RogueParticulas(b, ponto, GOLD, 36, 400);
    for (int i = 0; i < MAX_PROJETEIS; i++)
        if (b->proj[i].ativo && !b->proj[i].doJogador && Vector2Distance(b->proj[i].pos, ponto) < 170.0f)
            b->proj[i].ativo = false;

    TocarSom(sfxParryPerfeito);
    RogueTextoFlutuante(b, (Vector2){ ponto.x, ponto.y - 44 }, "PARRY PERFEITO!", GOLD);
    Telemetria("mestre_parry", "", (float)m->etapa, m->parries);
}

// Rajadas: 0 = anéis, 1 = leques mirados, 2 = espiral. Devolve true quando terminou.
static bool MestreRajada(Biblioteca *b, float dt)
{
    MestreTreino *m = &b->mestre;
    bool treino = m->etapa != MT_LUTA;
    float dano = treino ? MESTRE_DANO_BALA_TREINO : MESTRE_DANO_BALA;
    float ritmo = (m->fase == 2) ? 1.2f : 1.0f;

    m->cronoTiro -= dt * ritmo;
    if (m->cronoTiro > 0.0f)
        return false;

    Vector2 mira = Vector2Subtract(b->j.pos, m->pos);
    mira = (Vector2Length(mira) > 0.1f) ? Vector2Normalize(mira) : (Vector2){ 0, 1 };

    switch (m->padrao) {
        case 0: {           // anéis lentos (o treino da esquiva usa só este)
            int n = treino ? 14 : 18;
            float giro = (float)m->disparos * 0.21f;
            for (int k = 0; k < n; k++) {
                float a = giro + (2.0f * PI / n) * k;
                RogueDisparar(b, m->pos, (Vector2){ cosf(a), sinf(a) }, treino ? 140.0f : 165.0f, dano, 7.0f, false);
            }
            m->cronoTiro = treino ? 0.9f : 0.6f;
            m->disparos++;
            RogueAnel(b, m->pos, 70.0f, MAGENTA);
            TocarGrupo(&gIniAnel, 0.85f, 0.75f);
            return m->disparos >= 3;
        }
        case 1:             // leques mirados
            for (int k = -2; k <= 2; k++)
                RogueDisparar(b, m->pos, Vector2Rotate(mira, (float)k * 13.0f * DEG2RAD), 235.0f, dano, 6.0f, false);
            m->cronoTiro = 0.42f;
            m->disparos++;
            TocarGrupo(&gChefeRajada, 1.05f, 0.75f);
            return m->disparos >= 3;
        default: {          // espiral (dois braços)
            float a = (float)m->disparos * 0.33f;
            for (int k = 0; k < 2; k++)
                RogueDisparar(b, m->pos, (Vector2){ cosf(a + PI * k), sinf(a + PI * k) }, 185.0f, dano, 6.0f, false);
            m->cronoTiro = 0.07f;
            m->disparos++;
            if (m->disparos % 4 == 0) TocarGrupo(&gIniPacote, 0.8f, 0.55f);
            return m->disparos >= 22;
        }
    }
}

static void MestreComecarRajada(MestreTreino *m)
{
    m->acao = MA_RAJADA;
    m->disparos = 0;
    m->cronoTiro = 0.25f;
    m->padrao = (m->etapa == MT_LUTA) ? GetRandomValue(0, 2) : 0;
}

// Os ataques da Mestre (ESQUIVA, ABERTURA, PARRY e LUTA usam a mesma máquina)
static void MestreAtacar(Biblioteca *b, float dt)
{
    MestreTreino *m = &b->mestre;
    JogadorRogue *j = &b->j;
    Vector2 d = Vector2Subtract(j->pos, m->pos);
    float dist = Vector2Length(d);
    Vector2 dir = (dist > 0.01f) ? Vector2Scale(d, 1.0f / dist) : (Vector2){ 0, 1 };

    m->tempoAcao -= dt;

    switch (m->acao) {

        case MA_PAUSA: {        // flutua de um lado para o outro, perto do alto da sala
            float alvoX = 400.0f + sinf(m->anim * 0.8f) * 160.0f;
            m->pos.x += (alvoX - m->pos.x) * Clamp(dt * 1.5f, 0.0f, 1.0f);
            m->pos.y += (POS_MESTRE_INICIO.y - m->pos.y) * Clamp(dt * 1.5f, 0.0f, 1.0f);
            if (m->tempoAcao > 0.0f) break;

            bool golpe = (m->etapa == MT_PARRY) || (m->etapa == MT_LUTA && m->proximoGolpe);
            if (golpe) { m->acao = MA_APROXIMA; m->tempoAcao = 1.6f; }
            else       MestreComecarRajada(m);
            if (m->etapa == MT_LUTA) m->proximoGolpe = !m->proximoGolpe;
        } break;

        case MA_RAJADA:
            if (MestreRajada(b, dt)) {
                if (m->etapa == MT_ESQUIVA) { m->acao = MA_PAUSA; m->tempoAcao = 1.0f; }
                else { m->acao = MA_ABERTA; m->tempoAcao = MESTRE_ABERTURA; TocarGrupo(&gChefeEscudo, 1.1f, 0.7f); RogueAnel(b, m->pos, 90.0f, GOLD); }
            }
            break;

        case MA_ABERTA:         // cansada: parada, escudo dourado
            if (m->tempoAcao <= 0.0f) { m->acao = MA_PAUSA; m->tempoAcao = (m->etapa == MT_LUTA) ? 0.9f : 1.3f; }
            break;

        case MA_APROXIMA:       // chega até a distância do golpe (o ritmo do parry fica sempre igual)
            if (dist > MESTRE_DIST_GOLPE + 8.0f) {
                m->pos = Vector2Add(m->pos, Vector2Scale(dir, 230.0f * dt));
                RogueEmpurrarParaFora(&m->pos, MESTRE_RAIO, NULL, 0);
            } else if (dist < MESTRE_DIST_GOLPE - 30.0f) {
                m->pos = Vector2Subtract(m->pos, Vector2Scale(dir, 160.0f * dt));
                RogueEmpurrarParaFora(&m->pos, MESTRE_RAIO, NULL, 0);
            }
            if (m->tempoAcao <= 0.0f || fabsf(dist - MESTRE_DIST_GOLPE) < 14.0f) {
                m->acao = MA_TELEGRAFA;
                m->tempoAcao = MestreTelegrafia(m);
                m->dirGolpe = dir;
                TocarSom(sfxAvisoParry);
            }
            break;

        case MA_TELEGRAFA:      // brilho vermelho; a mira trava no último terço
            if (m->tempoAcao > MestreTelegrafia(m) * 0.3f) m->dirGolpe = dir;
            if (m->tempoAcao <= 0.0f) {
                m->acao = MA_INVESTE;
                m->tempoAcao = MESTRE_INVESTIDA_TEMPO;
                m->acertouGolpe = false;
                TocarGrupo(&gChefeInvestida, 0.95f, 0.85f);
            }
            break;

        case MA_INVESTE: {
            m->pos = Vector2Add(m->pos, Vector2Scale(m->dirGolpe, MESTRE_INVESTIDA_VEL * dt));
            RogueEmpurrarParaFora(&m->pos, MESTRE_RAIO, NULL, 0);
            if (GetRandomValue(0, 1) == 0) RogueParticulas(b, m->pos, RED, 1, 60);

            if (!m->acertouGolpe && CheckCollisionCircles(m->pos, MESTRE_RAIO, j->pos, RAIO_CORPO_JOGADOR)) {
                m->acertouGolpe = true;
                Vector2 ponto = Vector2Lerp(m->pos, j->pos, 0.6f);
                bool luciSalva = (m->etapa == MT_PARRY && m->golpesLevados >= 5);      // empacou: a Luci rebate por ele
                if (j->parry.emParry || luciSalva) {
                    if (luciSalva) RogueRecadoLuci(b, "Luci: Eu segurei o golpe! Viu o anel fechar? É nesse instante que se aperta V.");
                    MestreParry(b, ponto);
                    break;
                }
                if (j->invulneravel <= 0.0f && j->tempoEsquiva <= 0.0f) {
                    RogueFerirJogador(b, (m->etapa == MT_LUTA) ? MESTRE_DANO_GOLPE : MESTRE_DANO_BALA_TREINO);
                    j->pos = Vector2Add(j->pos, Vector2Scale(m->dirGolpe, 30.0f));
                    RogueEmpurrarParaFora(&j->pos, RAIO_CORPO_JOGADOR, b->obst, b->numObst);
                    m->golpesLevados++;
                    if (m->etapa == MT_PARRY)
                        RogueRecadoLuci(b, (m->golpesLevados >= 3) ? "Aperte V um instante ANTES de ela encostar!"
                                                                    : "Quase! Aperte V quando o anel fechar em você.");
                }
            }
            if (m->tempoAcao <= 0.0f) { m->acao = MA_RECUPERA; m->tempoAcao = MESTRE_RECUPERA; }
        } break;

        case MA_RECUPERA:
        case MA_ATORDOADA:
            if (m->tempoAcao <= 0.0f) { m->acao = MA_PAUSA; m->tempoAcao = (m->etapa == MT_LUTA) ? 0.8f : 1.2f; }
            break;
    }
}

// Distância de um ponto ao trajeto do tiro neste quadro (alvo pequeno não é atravessado com FPS baixo)
float MestreDistTrajeto(const Projetil *p, Vector2 c, float dt)
{
    Vector2 a = Vector2Subtract(p->pos, Vector2Scale(p->vel, dt)), ab = Vector2Subtract(p->pos, a);
    float len2 = Vector2DotProduct(ab, ab);
    float t = (len2 > 0.0001f) ? Clamp(Vector2DotProduct(Vector2Subtract(c, a), ab) / len2, 0.0f, 1.0f) : 1.0f;
    return Vector2Distance(c, Vector2Add(a, Vector2Scale(ab, t)));
}

// Tiros do Thomas: alvos de treino e a Mestre (escudo fechado = o tiro bate e some)
static void MestreReceberTiros(Biblioteca *b, ProgressoCorredor *prog, float dt)
{
    MestreTreino *m = &b->mestre;
    (void)prog;

    for (int i = 0; i < MAX_PROJETEIS; i++) {
        Projetil *p = &b->proj[i];
        if (!p->ativo || !p->doJogador)
            continue;

        if (m->etapa == MT_TIRO) {
            for (int a = 0; a < MESTRE_ALVOS; a++) {
                if (!m->alvoVivo[a] || MestreDistTrajeto(p, m->alvo[a], dt) > 18.0f + p->raio) continue;
                m->alvoVivo[a] = false;
                p->ativo = false;
                RogueParticulas(b, m->alvo[a], GOLD, 22, 220);
                RogueAnel(b, m->alvo[a], 40.0f, GOLD);
                RogueTextoFlutuante(b, (Vector2){ m->alvo[a].x, m->alvo[a].y - 26 }, "FATO!", GOLD);
                {
                    int restam = 0;
                    for (int q = 0; q < MESTRE_ALVOS; q++) if (m->alvoVivo[q]) restam++;
                    RogueRecadoLuci(b, (restam > 0) ? TextFormat("Luci: Na mosca! Faltam %d.", restam) : "Luci: Perfeito! Todos os fatos acertados!");
                }
                TocarSomTom(sfxAcerto, 1.1f + 0.1f * (float)a);
                break;
            }
            continue;
        }

        if (!m->visivel || m->morrendo > 0.0f || MestreDistTrajeto(p, m->pos, dt) > MESTRE_RAIO + p->raio)
            continue;

        p->ativo = false;
        if (MestreVulneravel(m)) {
            float piso = (m->etapa == MT_LUTA) ? 0.0f : MESTRE_HP * MESTRE_TREINO_HP_CHEFE;
            m->hp = fmaxf(piso, m->hp - p->dano);
            m->flash = 0.12f;
            if (m->etapa == MT_ABERTURA || (m->etapa == MT_PARRY && m->acao == MA_ATORDOADA)) m->acertosAbertura++;
            if (m->etapa == MT_ABERTURA && m->acertosAbertura <= MESTRE_ACERTOS_ABERTURA)
                RogueRecadoLuci(b, TextFormat("Luci: Acertou o escudo! (%d/%d)", m->acertosAbertura, MESTRE_ACERTOS_ABERTURA));
            RogueParticulas(b, p->pos, GOLD, 5, 160);
            TocarSomTom(sfxAcerto, 1.4f);
        } else {
            RogueParticulas(b, p->pos, (Color){ 200, 122, 255, 255 }, 4, 120);
            {
                static double ultimoEscudoMestre = -10.0;
                if (SomLiberado(&ultimoEscudoMestre, 0.07)) TocarGrupo(&gIniBloqueio, 0.9f, 0.5f);
            }
            if (m->avisoEscudo <= 0.0f) {
                RogueTextoFlutuante(b, (Vector2){ m->pos.x, m->pos.y - 70 }, "ESCUDO!", (Color){ 200, 122, 255, 255 });
                m->avisoEscudo = 1.2f;
            }
        }
    }
}

void RogueAtualizarMestre(Biblioteca *b, ProgressoCorredor *prog, float dt)
{
    MestreTreino *m = &b->mestre;
    JogadorRogue *j = &b->j;

    if (!m->ativo)
        return;

    m->tempoEtapa += dt;
    m->anim += dt;
    if (m->flash > 0.0f)       m->flash -= dt;
    if (m->surgindo > 0.0f)    m->surgindo -= dt;
    if (m->avisoEscudo > 0.0f) m->avisoEscudo -= dt;

    // Derrota da Mestre: ela se desfaz e vem a conversa final
    if (m->morrendo > 0.0f) {
        m->morrendo -= dt;
        if (GetRandomValue(0, 2) == 0) RogueParticulas(b, m->pos, (GetRandomValue(0, 1) == 0) ? MAGENTA : GOLD, 3, 260);
        if (m->morrendo <= 0.0f) {
            m->visivel = false;
            m->etapa = MT_FIM;
            b->conversaPendente = &dialogoMestreVitoria;
            b->objConversa = OBJ_MESTRE_VITORIA;
        }
        return;
    }
    if (m->etapa == MT_ARMA || m->etapa == MT_FIM)
        return;

    // Pulso (Q): conta o instante em que ele dispara (a recarga sai de zero)
    if (m->recargaPulsoAnt <= 0.0f && j->recargaPulso > 0.0f && m->etapa == MT_PULSO) {
        m->pulsos++;
        RogueRecadoLuci(b, "Luci: Isso! O Pulso apagou as balas. Cuidado: ele gasta EP e tem recarga.");
    }
    m->recargaPulsoAnt = j->recargaPulso;

    // Esquivas feitas (o treino conta as que começam)
    bool esquivando = j->tempoEsquiva > 0.0f;
    if (esquivando && !m->estavaEsquivando) {
        m->esquivas++;
        if (m->etapa == MT_ESQUIVA)
            RogueRecadoLuci(b, (m->esquivas >= MESTRE_ESQUIVAS) ? "Luci: Isso! É assim que se esquiva!"
                                                                : TextFormat("Luci: Boa esquiva! (%d/%d)", m->esquivas, MESTRE_ESQUIVAS));
    }
    m->estavaEsquivando = esquivando;

    MestreReceberTiros(b, prog, dt);

    if (m->visivel && m->surgindo <= 0.0f)
        MestreAtacar(b, dt);

    // Fase 2 da luta: as três alucinações dos monitores atiram de onde estiverem na órbita
    if (m->etapa == MT_LUTA && m->fase == 2 && m->acao != MA_ATORDOADA) {
        m->cronoOrbita -= dt;
        if (m->cronoOrbita <= 0.0f) {
            for (int i = 0; i < TOTAL_MONITORES; i++) {
                float ang = m->anim * 1.4f + i * (2.0f * PI / TOTAL_MONITORES);
                Vector2 o = { m->pos.x + cosf(ang) * 96, m->pos.y + 6 + sinf(ang) * 22 };
                Vector2 d = Vector2Subtract(j->pos, o);
                if (Vector2Length(d) > 0.1f)
                    RogueDisparar(b, o, Vector2Normalize(d), 200.0f, MESTRE_DANO_BALA * 0.7f, 5.0f, false);
            }
            m->cronoOrbita = 1.7f;
            TocarGrupo(&gIniPacote, 0.9f, 0.5f);
        }
    }

    // No tutorial a Luci segura a vida do Thomas
    if (m->etapa != MT_LUTA && j->hp < RogueHpMax(b) * MESTRE_TREINO_HP_MIN)
        j->hp = RogueHpMax(b) * MESTRE_TREINO_HP_MIN;

    MestreDicaProgressiva(b);

    // A fala da Luci da etapa entra quando o cartão sai do meio da tela
    if (m->falaDepois != NULL && m->tempoEtapa >= CARTAO_ETAPA_TEMPO && b->luciMsgTempo <= 0.0f) {
        RogueRecadoLuci(b, m->falaDepois);
        b->luciMsgTempo = 5.0f;
        m->falaDepois = NULL;
    }

    // Treino do parry, até acertar o 1º: câmera lenta quando o anel está para fechar (dá para ver a hora certa)
    if (m->etapa == MT_PARRY && m->parries == 0 && m->morrendo <= 0.0f) {
        float falta = MestreTempoAteGolpe(m, j->pos, MestreTelegrafia(m));
        if (falta >= 0.0f && falta < MestreJanelaParry(b) + 0.25f) b->lentidao = 0.6f;
    }

    // Progresso do tutorial: uma mecânica de cada vez
    switch (m->etapa) {
        case MT_TIRO: {
            bool restam = false;
            for (int a = 0; a < MESTRE_ALVOS; a++) if (m->alvoVivo[a]) restam = true;
            if (!restam) MestreMudarEtapa(b, MT_ESQUIVA);
        } break;
        case MT_ESQUIVA:
            if (m->esquivas >= MESTRE_ESQUIVAS && m->tempoEtapa > 5.0f && m->acao == MA_PAUSA) MestreMudarEtapa(b, MT_ABERTURA);
            break;
        case MT_ABERTURA:
            if (m->acertosAbertura >= MESTRE_ACERTOS_ABERTURA && m->acao != MA_ABERTA) MestreMudarEtapa(b, MT_PARRY);
            break;
        case MT_PARRY:
            if (m->parries >= 1 && m->acao == MA_PAUSA) MestreMudarEtapa(b, MT_PULSO);
            break;
        case MT_PULSO:
            if (m->pulsos >= 1 && m->tempoEtapa > 1.5f) MestreMudarEtapa(b, MT_MELHORIA);
            break;
        case MT_MELHORIA:                                   // a escolha de treino fecha: Luci explica e começa a luta
            if (!b->escolhaAberta && b->escolhaTreinoFeita) {
                b->escolhaTreinoFeita = false;
                MestreMudarEtapa(b, MT_LUTA);
                b->mestre.falaDepois = "Luci: Na missão é 1 entre 3 melhorias e vale até o fim. Agora junte tudo e vença a Mestre!";
            }
            break;
        case MT_LUTA:
            if (m->fase == 1 && m->hp <= MESTRE_HP * MESTRE_FASE2) {
                m->fase = 2;
                m->cronoOrbita = 1.0f;
                RogueFaixaVirada(b, "FASE 2: AS TRÊS ALUCINAÇÕES", "os monitores atiram da órbita", MAGENTA);
                RogueAnel(b, m->pos, 160.0f, MAGENTA);
                RogueRecadoLuci(b, "Luci: Ela chamou as alucinações dos monitores! Não pare de se mexer!");
                b->luciMsgTempo = 4.5f;
            }
            if (m->hp <= 0.0f) {
                m->hp = 0.0f;
                m->morrendo = 1.8f;
                for (int i = 0; i < MAX_PROJETEIS; i++)
                    if (!b->proj[i].doJogador) b->proj[i].ativo = false;
                RogueAnel(b, m->pos, 180.0f, MAGENTA);
                RogueAnel(b, m->pos, 110.0f, WHITE);
                RogueParticulas(b, m->pos, MAGENTA, 50, 320);
                b->pausaImpacto = 0.2f;
                TocarGrupo(&gChefeMorte, 1.0f, 1.0f);
                Telemetria("mestre_vitoria", "", m->tempoEtapa, m->quedas);
            }
            break;
        default:
            break;
    }
}

// Caiu na luta: a Luci reergue o Thomas (sem perder pontos nem o que a Mestre já perdeu)
void RogueMestreReerguer(Biblioteca *b)
{
    MestreTreino *m = &b->mestre;

    m->quedas++;
    b->j.hp = RogueHpMax(b);
    b->j.invulneravel = 2.0f;
    m->acao = MA_PAUSA;
    m->tempoAcao = 2.0f;
    for (int i = 0; i < MAX_PROJETEIS; i++)
        if (!b->proj[i].doJogador) b->proj[i].ativo = false;
    RogueParticulas(b, b->j.pos, (Color){ 0, 228, 255, 255 }, 30, 220);
    b->falaPendente = "Luci: Levanta, Thomas! Eu seguro ela um instante. Respira e tenta de novo.";
    Telemetria("mestre_queda", "", 0.0f, m->quedas);
}

// Venceu: a sala fecha e o Thomas chega ao átrio da Biblioteca (o Bloco 2) com a arma
void RogueSairSalaMestre(Biblioteca *b)
{
    memset(&b->mestre, 0, sizeof(b->mestre));
    RogueEntrarAtrio(b, true);
    b->tituloBloco = 4.5f;                                  // cartão "BLOCO 2" (desenhado por cima, sem parar o jogo)
    b->conversaPendente = &dialogoChegadaBiblioteca;        // começa quando o fade de entrada terminar
    b->objConversa = OBJ_NENHUM;
    b->falaPendente = NULL;
}

// ---------- Afixos de elite: PULSO e ORBITAL (a DIVISÃO acontece na morte) ----------
void RogueAtualizarAfixos(Biblioteca *b, float dt)
{
    for (int i = 0; i < MAX_INIMIGOS; i++) {
        Inimigo *e = &b->ini[i];
        if (!e->ativo || !e->elite || e->afixo == AFIXO_NENHUM || e->surgindo > 0.0f || e->selado)
            continue;

        if (e->afixo == AFIXO_PULSO && e->atordoado <= 0.0f) {
            float ritmo = (b->fatorRitmoInimigos > 0.0f) ? b->fatorRitmoInimigos : 1.0f;
            e->cronoAfixo -= dt * ritmo;
            if (e->cronoAfixo <= 0.0f) {
                float giro = (float)GetRandomValue(0, 30) * DEG2RAD;
                for (int k = 0; k < AFIXO_PULSO_BALAS; k++) {
                    float a = giro + (2.0f * PI / AFIXO_PULSO_BALAS) * k;
                    RogueDisparar(b, e->pos, (Vector2){ cosf(a), sinf(a) }, AFIXO_PULSO_VEL, AFIXO_PULSO_DANO, 6.0f, false);
                }
                RogueAnel(b, e->pos, 70.0f, GOLD);
                TocarGrupo(&gIniAnel, 1.0f, 0.65f);
                e->cronoAfixo = AFIXO_PULSO_INTERVALO;
            }
        }

        if (e->afixo == AFIXO_ORBITAL) {
            for (int k = 0; k < AFIXO_ORBES; k++) {
                if (e->hpOrbe[k] <= 0.0f) continue;
                Vector2 o = RogueOrbePos(e, k);

                // Segura os tiros do Thomas (e quebra com dano)
                for (int q = 0; q < MAX_PROJETEIS; q++) {
                    Projetil *p = &b->proj[q];
                    if (!p->ativo || !p->doJogador || MestreDistTrajeto(p, o, dt) > AFIXO_ORBE_RAIO + p->raio) continue;
                    p->ativo = false;
                    e->hpOrbe[k] -= p->dano;
                    RogueParticulas(b, o, GOLD, 4, 120);
                    if (e->hpOrbe[k] <= 0.0f) {
                        RogueParticulas(b, o, WHITE, 14, 200);
                        TocarSomTom(sfxAcerto, 1.5f);
                        bool resta = false;
                        for (int z = 0; z < AFIXO_ORBES; z++) if (e->hpOrbe[z] > 0.0f) resta = true;
                        if (!resta) RogueTextoFlutuante(b, (Vector2){ e->pos.x, e->pos.y - 34 }, "ESCUDOS QUEBRADOS!", GOLD);
                        break;
                    }
                }

                // Encostar no escudo machuca
                if (e->hpOrbe[k] > 0.0f && CheckCollisionCircles(o, AFIXO_ORBE_RAIO, b->j.pos, RAIO_CORPO_JOGADOR))
                    RogueFerirJogador(b, AFIXO_ORBE_DANO);
            }
        }
    }
}
