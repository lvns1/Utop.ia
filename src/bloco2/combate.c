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

// ---------- Inimigos da expansão: utilidades e sinergias ----------

// Inimigo que nasce de outro e some sozinho (não recebe bônus nem protege ninguém)
bool RogueInimigoPassageiro(TipoInimigo t)
{
    return t == INIMIGO_ECO || t == INIMIGO_SPAM || t == INIMIGO_ISCA || t == INIMIGO_TOKEN;
}

// Está dentro do escudo de rede de algum Firewall (sem contar ele mesmo)?
bool RogueNaZonaFirewall(const Biblioteca *b, const Inimigo *e)
{
    for (int i = 0; i < MAX_INIMIGOS; i++) {
        const Inimigo *f = &b->ini[i];
        if (f != e && f->ativo && f->tipo == INIMIGO_FIREWALL && f->surgindo <= 0.0f &&
            Vector2Distance(f->pos, e->pos) < FIREWALL_ZONA)
            return true;
    }
    return false;
}

int RogueSpammersPerto(const Biblioteca *b, Vector2 pos, float raio)
{
    int n = 0;
    for (int i = 0; i < MAX_INIMIGOS; i++)
        if (b->ini[i].ativo && b->ini[i].tipo == INIMIGO_SPAMMER && Vector2Distance(b->ini[i].pos, pos) < raio)
            n++;
    return n;
}

// Keylogger: onde o jogador vai estar quando o tiro chegar. Só prevê se ele vem
// andando na mesma direção há algum tempo; a esquiva ou uma curva zeram a leitura.
Vector2 RoguePrevisaoJogador(const Biblioteca *b, Vector2 origem, float velProjetil)
{
    const JogadorRogue *j = &b->j;
    float confianca = Clamp((j->tempoMesmaDirecao - KEYLOGGER_LEITURA) / KEYLOGGER_LEITURA, 0.0f, 1.0f);
    float t = fminf(Vector2Distance(origem, j->pos) / velProjetil, KEYLOGGER_PREVISAO_MAX);
    Vector2 p = Vector2Add(j->pos, Vector2Scale(j->velMedia, t * confianca));

    p.x = Clamp(p.x, ArenaRogue().x + 10.0f, ArenaRogue().x + ArenaRogue().width - 10.0f);
    p.y = Clamp(p.y, ArenaRogue().y + 10.0f, ArenaRogue().y + ArenaRogue().height - 10.0f);
    return p;
}

bool RogueTemKeylogger(const Biblioteca *b)
{
    for (int i = 0; i < MAX_INIMIGOS; i++)
        if (b->ini[i].ativo && b->ini[i].tipo == INIMIGO_KEYLOGGER && b->ini[i].surgindo <= 0.0f)
            return true;
    return false;
}

// Overclock: carga de 0 (mínima) a 1 (máxima)
float RogueCargaOverclock(const Inimigo *e)
{
    return Clamp((e->carga - OVERCLOCK_CARGA_MIN) / (OVERCLOCK_CARGA_MAX - OVERCLOCK_CARGA_MIN), 0.0f, 1.0f);
}

void RogueSuperaquecer(Inimigo *e)
{
    if (e->estado == 1) GrupoParar(&gIniCarga);         // a carga escapou antes da hora
    TocarGrupo(&gIniSuperaquece, 1.0f, 0.7f);
    e->estado = 3;
    e->timer = OVERCLOCK_ESFRIAR_MIN + (OVERCLOCK_ESFRIAR_MAX - OVERCLOCK_ESFRIAR_MIN) * RogueCargaOverclock(e);
    e->vel = (Vector2){0};
}

// Phisher revelado: mira e, se a habilidade estiver pronta, solta uma cópia falsa ao lado.
// Com o jogador cercado de Spammers (distraído), ele ataca mais rápido.
void RogueRevelarPhisher(Biblioteca *b, Inimigo *e, bool distraido)
{
    if (e->estado != 0)
        return;

    Vector2 dir = Vector2Normalize(Vector2Subtract(b->j.pos, e->pos));

    e->estado = 1;
    e->timer = distraido ? PHISHER_PREPARO * 0.65f : PHISHER_PREPARO;
    e->alvo = dir;
    RogueParticulas(b, e->pos, GOLD, 10, 140);
    TocarGrupo(&gIniPhisher, 1.0f, 0.75f);

    if (e->habilidade > 0.0f)
        return;

    e->habilidade = ISCA_RECARGA;

    Vector2 lado = { -dir.y * e->lado, dir.x * e->lado };
    Vector2 onde = Vector2Add(e->pos, Vector2Scale(lado, 70.0f));
    RogueEmpurrarParaFora(&onde, e->raio, b->obst, b->numObst);

    Inimigo *isca = RogueCriarInimigo(b, INIMIGO_ISCA, onde);
    if (isca != NULL) {
        isca->estado = 1;
        isca->timer = e->timer;
        isca->alvo = Vector2Normalize(Vector2Subtract(b->j.pos, onde));
        isca->dono = (int)(e - b->ini);
        RogueParticulas(b, onde, GOLD, 10, 140);
    }
}

// Botnet: liga cada Botnet aos (até 3) inimigos mais próximos. Quem está ligado
// fica mais rápido e ataca mais vezes. Botnet derrotado = conexões desfeitas na hora.
void RogueAtualizarConexoes(Biblioteca *b)
{
    for (int i = 0; i < MAX_INIMIGOS; i++)
        b->ini[i].conectado = false;

    for (int i = 0; i < MAX_INIMIGOS; i++) {

        Inimigo *bt = &b->ini[i];

        if (!bt->ativo || bt->tipo != INIMIGO_BOTNET)
            continue;

        for (int k = 0; k < BOTNET_CONEXOES; k++) {

            bt->conexao[k] = -1;

            if (bt->surgindo > 0.0f)
                continue;

            float menor = BOTNET_ALCANCE;

            for (int o = 0; o < MAX_INIMIGOS; o++) {

                Inimigo *e = &b->ini[o];
                bool jaLigado = false;

                for (int q = 0; q < k; q++)
                    if (bt->conexao[q] == o) jaLigado = true;

                // (o Phisher camuflado fica de fora: a conexão entregaria o disfarce)
                if (!e->ativo || o == i || jaLigado || e->tipo == INIMIGO_BOTNET || RogueInimigoPassageiro(e->tipo) || e->surgindo > 0.0f ||
                    (e->tipo == INIMIGO_PHISHER && e->estado == 0))
                    continue;

                float d = Vector2Distance(bt->pos, e->pos);
                if (d < menor) { menor = d; bt->conexao[k] = o; }
            }

            if (bt->conexao[k] >= 0)
                b->ini[bt->conexao[k]].conectado = true;
        }
    }
}

// Firewall: quem ele protege. Um Cache tem prioridade (sinergia); senão, o centro dos aliados.
bool RogueQuemProteger(const Biblioteca *b, const Inimigo *fw, Vector2 *centro)
{
    Vector2 soma = {0};
    int n = 0;
    float menorCache = 99999.0f;

    for (int i = 0; i < MAX_INIMIGOS; i++) {

        const Inimigo *e = &b->ini[i];

        if (e == fw || !e->ativo || e->tipo == INIMIGO_FIREWALL || RogueInimigoPassageiro(e->tipo))
            continue;

        if (e->tipo == INIMIGO_CACHE && Vector2Distance(fw->pos, e->pos) < menorCache) {
            menorCache = Vector2Distance(fw->pos, e->pos);
            *centro = e->pos;
        }

        soma = Vector2Add(soma, e->pos);
        n++;
    }

    if (menorCache < 99999.0f)
        return true;

    if (n == 0)
        return false;

    *centro = Vector2Scale(soma, 1.0f / (float)n);
    return true;
}

// Ponto da barreira de energia do Firewall (segmento na frente dele)
void RogueBarreiraFirewall(const Inimigo *e, Vector2 *a, Vector2 *c)
{
    Vector2 meio = Vector2Add(e->pos, Vector2Scale(e->frente, e->raio + 16.0f));
    Vector2 lado = { -e->frente.y, e->frente.x };
    *a = Vector2Add(meio, Vector2Scale(lado, FIREWALL_BARREIRA_LARGURA / 2.0f));
    *c = Vector2Subtract(meio, Vector2Scale(lado, FIREWALL_BARREIRA_LARGURA / 2.0f));
}

// Um tiro do jogador bateu numa barreira de energia ativa?
bool RogueBarreiraBloqueia(const Biblioteca *b, Vector2 pos, float raio)
{
    for (int i = 0; i < MAX_INIMIGOS; i++) {

        const Inimigo *e = &b->ini[i];

        if (!e->ativo || e->tipo != INIMIGO_FIREWALL || e->estado != 2)
            continue;

        Vector2 a, c;
        RogueBarreiraFirewall(e, &a, &c);

        Vector2 ac = Vector2Subtract(c, a);
        float t = Clamp(Vector2DotProduct(Vector2Subtract(pos, a), ac) / Vector2DotProduct(ac, ac), 0.0f, 1.0f);
        if (Vector2Distance(pos, Vector2Add(a, Vector2Scale(ac, t))) < raio + 4.0f)
            return true;
    }
    return false;
}

// Quanto do dano o inimigo leva de verdade. "direcao" é para onde o golpe vai.
// protegido = bateu num escudo (frente do Firewall ou zona de rede): faíscas verdes.
static float RogueFatorDanoInimigo(const Biblioteca *b, const Inimigo *e, Vector2 direcao, bool *protegido)
{
    float f = 1.0f;
    *protegido = false;

    if (e->tipo == INIMIGO_FIREWALL && Vector2Length(direcao) > 0.01f) {
        float deFrente = Vector2DotProduct(Vector2Negate(direcao), e->frente);     // 1 = veio bem de frente
        if (deFrente > cosf(FIREWALL_ANGULO_FRENTE * DEG2RAD)) { f *= FIREWALL_FRENTE; *protegido = true; }
        else if (deFrente < -0.5f)                              f *= FIREWALL_COSTAS;
    }
    else if (e->tipo == INIMIGO_SENTINELA) {
        // A lente protege enquanto ela está focada; distraída ou recalculando, leva o dobro
        if (RogueSentinelaVulneravel(e)) f *= SENTINELA_DANO_VULNERAVEL;
        else                             { f *= SENTINELA_DANO_BLINDADA; *protegido = true; }
    }
    else if (e->tipo == INIMIGO_RETRO) {
        if (e->estado == 3) f *= RETRO_DANO_RECALIBRANDO;
    }
    else if (e->tipo == INIMIGO_JANELA) {
        // Com a instrução PROTEGER na janela, o golpe não fere: vira ruído (ver RogueJanelaRuido)
        if (b->janela.vulneravel > 0.0f) f *= JANELA_DANO_VULNERAVEL;
        else                             { f = 0.0f; *protegido = true; }
    }
    else if (e->tipo == INIMIGO_OVERCLOCK) {
        if (e->estado == 1) f *= OVERCLOCK_DANO_CARREGANDO;
        if (e->estado == 3) f *= 1.0f + (OVERCLOCK_DANO_QUENTE - 1.0f) * (0.5f + 0.5f * RogueCargaOverclock(e));
    }

    if (e->tipo != INIMIGO_FIREWALL && !RogueInimigoPassageiro(e->tipo) && RogueNaZonaFirewall(b, e)) {
        f *= 1.0f - FIREWALL_PROTECAO;
        *protegido = true;
    }

    return f;
}

// Dano de encostar no jogador agora (alguns só machucam atacando)
float RogueDanoContato(const Inimigo *e)
{
    switch (e->tipo) {
        case INIMIGO_PHISHER:   return (e->estado == 2) ? PHISHER_DANO : 0.0f;
        case INIMIGO_OVERCLOCK: return (e->estado == 2) ? OVERCLOCK_DANO_INVESTIDA : ((e->estado == 3) ? 0.0f : DEF_INIMIGOS[e->tipo].danoContato);
        default:                return DEF_INIMIGOS[e->tipo].danoContato;
    }
}

// O Pulso de Integridade interrompe o ataque que está sendo preparado
static void RogueInterromperInimigo(Inimigo *e)
{
    switch (e->tipo) {
        case INIMIGO_FRAGMENTO: e->estado = 0; e->recarga = FRAGMENTO_RECARGA;  break;
        case INIMIGO_GUARDIAO:  e->estado = 0; e->recarga = GUARDIAO_INTERVALO; break;
        case INIMIGO_PHISHER:
        case INIMIGO_ISCA:      if (e->estado == 1) { e->estado = 3; e->timer = PHISHER_RECUO; } break;
        case INIMIGO_OVERCLOCK: if (e->estado == 1) RogueSuperaquecer(e); break;      // a carga escapa: superaquece
        case INIMIGO_FIREWALL:  if (e->estado == 1) { e->estado = 0; e->habilidade = FIREWALL_INTERVALO; } break;
        case INIMIGO_BOTNET:    if (e->estado == 1) { e->estado = 0; e->recarga = BOTNET_INTERVALO; }    break;
        case INIMIGO_KEYLOGGER: if (e->estado == 1) { e->estado = 0; e->recarga = KEYLOGGER_INTERVALO; } break;
        case INIMIGO_CACHE:     if (e->estado == 1) { e->estado = 0; e->recarga = CACHE_INTERVALO; }     break;
        default: break;
    }
}

// ---------- Dano ----------

// DEV MENU (F1): modo invencível
bool g_devInvencivel = false;
// DEV MENU: índice final forçado (-1 = o calculado; 0 a 100 = esse valor). 60 ou mais = luta e final BONS.
int g_devIndice = -1;


void RogueFerirJogador(Biblioteca *b, float dano)
{
    JogadorRogue *j = &b->j;

    if (g_devInvencivel)
        return;

    if (j->invulneravel > 0.0f || j->tempoEsquiva > 0.0f || b->morreu)
        return;

    // Firewall Portátil: o escudo absorve este golpe e some
    if (j->efeito[EFEITO_ESCUDO] > 0.0f) {
        j->efeito[EFEITO_ESCUDO] = 0.0f;
        j->invulneravel = 0.4f;
        RogueParticulas(b, j->pos, (Color){ 0, 228, 48, 255 }, 18, 200);
        RogueAnel(b, j->pos, 50.0f, (Color){ 0, 228, 48, 255 });
        TocarGrupo(&gIniBloqueio, 0.9f, 0.75f);           // o escudo segurou o golpe
        return;
    }

    // Reformulação: nas salas mais fundas da missão os golpes doem mais
    if (RogueSalaDaMissao(b) && b->fatorDanoSala > 0.0f)
        dano *= b->fatorDanoSala;

    j->hp -= dano * RogueFatorDanoRecebido(j);
    j->invulneravel = INVULNERAVEL_APOS_DANO;
    j->golpesSofridos++;                        // desafio de integridade
    b->perm->golpesRun++;
    b->tremor = 0.25f;                          // a tela só treme quando o jogador toma dano
    b->flashDanoJog = 0.35f;                    // e as bordas ficam vermelhas por um instante

    // Mecanismo corrompido: levar dano interrompe o console que estava sendo desligado
    for (int k = 0; k < b->numMecanismos; k++)
        if (!b->mecanismoDesligado[k]) b->progressoMecanismo[k] = 0.0f;

    RogueParticulas(b, j->pos, RED, 10, 160);
    TocarSom(sfxDano);

    // Bloco 3: levou dano carregando um pedaço de contexto, ele cai
    if (b->luci.ativo && b->luci.carregando >= 0) {
        b->luci.posContexto[b->luci.carregando] = j->pos;
        b->luci.carregando = -1;
    }

    // Câmara da Inferência: levou dano carregando um fragmento, ele cai no chão (não some)
    Silogista *s = &b->silogista;
    if (s->ativo && s->carregando >= 0) {
        s->posPeca[s->carregando] = j->pos;
        s->carregando = -1;
        RogueAviso(b, "O fragmento caiu! Pegue de novo (E).");
    }
}



void RogueFerirInimigo(Biblioteca *b, ProgressoCorredor *prog, Inimigo *e, float dano, Vector2 direcao)
{
    const DefInimigo *def = &DEF_INIMIGOS[e->tipo];

    // Selado: o golpe bate no selo e abre o desafio
    if (e->selado) {
        RogueParticulas(b, e->pos, GOLD, 4, 100);
        if (!b->desafioAberto)
            RogueAbrirDesafio(b, (int)(e - b->ini));
        return;
    }

    bool protegido;
    float fator = RogueFatorDanoInimigo(b, e, direcao, &protegido);

    // Rotulagem de Dados: o alvo rotulado leva mais dano de tudo
    if (e->rotulo > 0.0f && RogueUp(&b->j, UP_ROTULAR) > 0)
        fator *= 1.0f + UP_ROTULO_BONUS;

    // Choque Térmico (sinergia da katana): queimando E congelado
    if (e->queima > 0.0f && e->gelo > 0.0f && RogueUp(&b->j, UP_SIN_CHOQUE_TERMICO) > 0)
        fator *= 1.4f;

    if (e->tipo == INIMIGO_SENTINELA || e->tipo == INIMIGO_RETRO)
        fator /= RogueAdaptacaoChefe(b);
    e->hp -= dano * fator;
    e->flash = 0.1f;
    e->empurrao = Vector2Add(e->empurrao, Vector2Scale(direcao, 140.0f * def->empurrao));

    RogueParticulas(b, e->pos, protegido ? (Color){ 0, 228, 48, 255 } : def->cor, 4, 120);
    if (protegido && fator < 0.6f && (e->tipo == INIMIGO_FIREWALL || e->tipo == INIMIGO_SENTINELA)) {
        static double ultimoEscudo = -10.0;
        if (SomLiberado(&ultimoEscudo, 0.07)) TocarGrupo(&gIniBloqueio, 1.0f, 0.55f);
    }

    // Reações de cada tipo ao levar dano
    if (e->tipo == INIMIGO_PHISHER && e->estado == 0)
        RogueRevelarPhisher(b, e, false);                   // descoberto antes da hora

    // Expansão da progressão: o Retropropagador guarda de onde veio o golpe; a Janela vira ruído
    if (e->tipo == INIMIGO_RETRO)
        RogueRetroRegistrar(b, e);
    if (e->tipo == INIMIGO_JANELA && b->janela.vulneravel <= 0.0f)
        RogueJanelaRuido(b, dano);

    if (e->tipo == INIMIGO_CACHE) {
        e->energia = fminf(CACHE_ENERGIA_MAX, e->energia + dano * fator * CACHE_ARMAZENA);
        e->semDano = 0.0f;
        if (e->recuperando > 0.0f) {
            e->recuperando = 0.0f;                          // a pressão interrompe a recuperação
            RogueParticulas(b, e->pos, WHITE, 8, 140);
            TocarGrupo(&gIniBloqueio, 0.75f, 0.6f);
        }
    }

    if (e->hp > 0.0f)
        return;

    // Derrotado
    Vector2 onde = e->pos;
    TipoInimigo tipo = e->tipo;
    int indice = (int)(e - b->ini);

    // Afixo DIVISÃO: o elite se parte em dois menores (sem aura; eles não se dividem de novo).
    // Antes de desativar: assim as metades não ocupam a vaga dele no vetor
    if (e->elite && e->afixo == AFIXO_DIVISAO) {
        float hpMetade = fmaxf(8.0f, e->hpMax * AFIXO_DIVISAO_HP);
        for (int k = 0; k < 2; k++) {
            Vector2 lado = { (k == 0) ? -18.0f : 18.0f, (float)GetRandomValue(-8, 8) };
            Inimigo *f = RogueCriarInimigo(b, tipo, Vector2Add(onde, lado));
            if (f == NULL) continue;
            f->hp = f->hpMax = hpMetade;
            f->raio = DEF_INIMIGOS[tipo].raio * 0.85f;
            f->surgindo = 0.15f;
            f->empurrao = Vector2Scale(lado, 10.0f);
        }
        RogueTextoFlutuante(b, (Vector2){ onde.x, onde.y - 30 }, "DIVISÃO!", GOLD);
        RogueAnel(b, onde, 60.0f, GOLD);
    }

    e->ativo = false;
    b->abatidos++;
    if (tipo != INIMIGO_SPAM && tipo != INIMIGO_ISCA && tipo != INIMIGO_TOKEN && tipo != INIMIGO_ECO)
        b->perm->abatidosRun++;

    if (tipo != INIMIGO_SPAM && tipo != INIMIGO_ISCA && tipo != INIMIGO_TOKEN)
        b->pausaImpacto = 0.04f;

    RogueParticulas(b, onde, def->cor, (tipo == INIMIGO_SPAM) ? 6 : 18, 220);
    RogueParticulas(b, onde, WHITE, 6, 160);
    if (tipo != INIMIGO_SPAM && tipo != INIMIGO_TOKEN)
        RogueAnel(b, onde, e->raio * 2.6f, def->cor);
    if (e->elite) {
        RogueAnel(b, onde, 90.0f, GOLD);
        TocarGrupo(&gIniAnel, 0.7f, 0.8f);
    }
    if (tipo == INIMIGO_OVERCLOCK && e->estado == 1)
        GrupoParar(&gIniCarga);                             // morreu carregando: o rangido para junto
    if (tipo == INIMIGO_SENTINELA || tipo == INIMIGO_RETRO || tipo == INIMIGO_JANELA) {
        TocarGrupo(&gChefeMorte, (tipo == INIMIGO_JANELA) ? 1.0f : 1.15f, 0.95f);
        RogueAnel(b, onde, 150.0f, def->cor);
        RogueAnel(b, onde, 80.0f, WHITE);
        b->pausaImpacto = 0.12f;
    }
    TocarGrupo(&gMorteInimigo, 0.85f + 0.3f * (def->tomSom - 1.0f), (tipo == INIMIGO_SPAM) ? 0.6f : 1.0f);   // cada tipo um pouco mais grave ou agudo

    // Eco da sala de desafio: conta para a meta (e não repara a Luci)
    if (tipo == INIMIGO_ECO && e->desafio) {
        b->contadorDesafio++;
        return;
    }

    // Ecos falsos verificados (destruídos a tempo) estabilizam a Luci
    if (tipo == INIMIGO_ECO && ++b->ecosDestruidos % ECOS_PARA_REPARO == 0)
        RogueMudarEstabilidade(b, 1, "ecos falsos verificados");

    if (tipo == INIMIGO_ECO || tipo == INIMIGO_SPAM || tipo == INIMIGO_TOKEN)
        return;

    if (tipo == INIMIGO_ISCA) {
        RogueAviso(b, "CÓPIA FALSA! O verdadeiro é outro.");
        return;
    }

    // As cópias somem junto com o Phisher verdadeiro
    if (tipo == INIMIGO_PHISHER)
        for (int i = 0; i < MAX_INIMIGOS; i++)
            if (b->ini[i].ativo && b->ini[i].tipo == INIMIGO_ISCA && b->ini[i].dono == indice) {
                b->ini[i].ativo = false;
                RogueParticulas(b, b->ini[i].pos, GOLD, 10, 140);
            }

    AlterarPontos(prog, def->pontos);

    // Expansão da progressão: o mini-chefe derrotado (recompensa uma vez e a explicação)
    if (tipo == INIMIGO_JANELA)
        RogueJanelaDerrotada(b, prog);

    // Reformulação: efeitos da build ao derrotar um inimigo
    JogadorRogue *jg = &b->j;
    bool elite = e->elite;

    if (RogueUp(jg, UP_ENERGIA_ABATE) > 0)
        jg->ep = fminf(RogueEpMax(b), jg->ep + UP_ENERGIA_ABATE_EP * (float)RogueUp(jg, UP_ENERGIA_ABATE));
    if (RogueUp(jg, UP_VERIFICACAO) > 0)
        jg->hp = fminf(RogueHpMax(b), jg->hp + UP_VERIFICACAO_CURA * (float)RogueUp(jg, UP_VERIFICACAO));
    if (RogueUp(jg, UP_ESCUDO_ELITE) > 0 && (elite || ++jg->abatesEscudo >= UP_ESCUDO_ABATES)) {
        jg->abatesEscudo = 0;
        if (jg->efeito[EFEITO_ESCUDO] < UP_ESCUDO_TEMPO) jg->efeito[EFEITO_ESCUDO] = UP_ESCUDO_TEMPO;
        RogueParticulas(b, jg->pos, (Color){ 0, 228, 48, 255 }, 14, 160);
    }

    if (elite || tipo == INIMIGO_JANELA) b->perm->especiaisRun++;

    if (elite) {
        b->elitesDerrotados++;
        RogueSoltar(b, onde, true, ITEM_ANTIVIRUS);         // elite sempre deixa conhecimento
        RogueSoltar(b, onde, true, ITEM_ANTIVIRUS);
        RogueAviso(b, "ELITE DERROTADO!");
    }

    if (e->prioritario) {
        b->alvoPrioritario = -1;
        RogueAviso(b, "RETRANSMISSOR DERRUBADO! Os reforços pararam.");
        TocarSomTom(sfxAcerto, 0.9f);
    }

    // Rótulo Volátil (sinergia): o rotulado explode (sem reação em cadeia)
    if (e->rotulo > 0.0f && RogueUp(jg, UP_SIN_ROTULO) > 0 && !b->golpeSecundario) {
        b->golpeSecundario = true;
        RogueDanoArea(b, prog, onde, UP_ROTULO_EXP_RAIO, UP_ROTULO_EXP_DANO * RogueFatorDano(b), 140.0f);
        b->golpeSecundario = false;
        RogueAnel(b, onde, UP_ROTULO_EXP_RAIO, (Color){ 255, 109, 194, 255 });
    }

    // Recursos: dado corrompido derrotado libera conhecimento
    if (GetRandomValue(1, 100) <= def->chanceFC)        RogueSoltar(b, onde, true, ITEM_ANTIVIRUS);
    if (GetRandomValue(1, 100) <= CHANCE_ANTIVIRUS)     RogueSoltar(b, onde, false, ITEM_ANTIVIRUS);
    if (GetRandomValue(1, 100) <= CHANCE_TEMPORARIO)    RogueSoltar(b, onde, false, RogueSortearTemporario());

    // Botnet Reverso: o inimigo derrotado solta um pacote teleguiado (só uma vez por morte,
    // nunca a partir de um golpe secundário e com um intervalo entre um e outro)
    if (RogueQtd(&b->j, ITEM_BOTNET_REVERSO) > 0 && !b->golpeSecundario && b->j.recargaBotnet <= 0.0f &&
        GetRandomValue(1, 100) <= BOTNET_REVERSO_CHANCE) {
        TipoArma arma = RogueArmaAtual(&b->j);
        Projetil *q = RogueDisparar(b, onde, (Vector2){ 0, -1 }, 300.0f, DEF_ARMAS[arma].dano * RogueFatorDano(b) * BOTNET_REVERSO_DANO, 4.0f, true);
        if (q != NULL) {
            q->guiado = true;
            q->secundario = true;
            q->vida = 1.5f;
            q->ultimoAlvo = indice + 1;
        }
        b->j.recargaBotnet = BOTNET_REVERSO_RECARGA;
    }

    if (tipo == INIMIGO_FRAGMENTO && (elite || GetRandomValue(1, 100) <= CHANCE_ECO_FALSO))
        RogueCriarInimigo(b, INIMIGO_ECO, onde);          // a informação falsa ecoa

    // Spammer: a notificação se multiplica em dois fragmentos de spam
    if (tipo == INIMIGO_SPAMMER && GetRandomValue(1, 100) <= SPAMMER_CHANCE_DIVIDIR)
        for (int k = 0; k < 2; k++) {
            Inimigo *f = RogueCriarInimigo(b, INIMIGO_SPAM, (Vector2){ onde.x + ((k == 0) ? -8.0f : 8.0f), onde.y });
            if (f != NULL) f->empurrao = (Vector2){ (k == 0) ? -160.0f : 160.0f, 0.0f };
        }
}
float RogueAdaptacaoChefe(const Biblioteca *b)
{
    int melhorias = 0, passivos = 0, perm = 0;
    for (int k = 0; k < NUM_UPGRADES; k++) melhorias += b->j.up[k];
    for (int k = 0; k < NUM_ITENS; k++) if (ITENS[k].passivo) passivos += b->j.itens[k];
    for (int m = 0; m < NUM_MELHORIAS; m++) perm += PermNivel(b->perm, m);
    float f = 1.0f + ADAPT_POR_MELHORIA * (float)melhorias + ADAPT_POR_PASSIVO * (float)passivos + ADAPT_POR_PERMANENTE * (float)perm;
    return fminf(f, ADAPT_MAX);
}

// Quanto mais adaptado, mais rápido ele ataca (metade do bônus de resistência, no máximo +30%)
float RogueRitmoChefe(const Biblioteca *b)
{
    return fminf(1.0f + 0.5f * (RogueAdaptacaoChefe(b) - 1.0f), 1.3f);
}

void RogueFerirChefe(Biblioteca *b, float dano, Vector2 onde)
{
    ChefeRogue *c = &b->chefe;

    if (!c->ativo || !c->apresentado || c->morrendo > 0.0f)
        return;

    if (c->fase == 2 && c->janela <= 0.0f) {
        RogueParticulas(b, onde, MAGENTA, 3, 90);          // bateu no escudo de distorção
        return;
    }

    c->hp -= dano / RogueAdaptacaoChefe(b);
    c->flash = 0.08f;
    RogueParticulas(b, onde, MAGENTA, 3, 120);
}

// O Alucinador leva dano agora? (o Disparo Dedutivo só marca quem pode ser ferido)
bool RogueChefeVulneravel(const ChefeRogue *c)
{
    return c->ativo && c->apresentado && c->morrendo <= 0.0f && (c->fase == 1 || c->janela > 0.0f);
}

// O Silogista só leva dano sem a armadura lógica. Devolve true se feriu.
bool RogueFerirSilogista(Biblioteca *b, float dano, Vector2 onde)
{
    Silogista *s = &b->silogista;

    if (!s->ativo || !s->apresentado || s->morrendo > 0.0f)
        return false;

    if (s->etapa != SIL_VULNERAVEL) {
        RogueParticulas(b, onde, (Color){ 0, 228, 255, 255 }, 3, 90);     // bateu na armadura lógica
        return false;
    }

    s->hp -= dano / RogueAdaptacaoChefe(b);
    s->flash = 0.08f;
    RogueParticulas(b, onde, PURPLE, 3, 120);
    return true;
}

// ---------- Selo da Inferência: DISPARO DEDUTIVO (Modus Ponens) ----------
//   1º acerto num inimigo: marca P → Q.  2º acerto: confirma P.
//   P → Q e P, logo Q: um projétil de energia Q cai no alvo com dano extra.

// Onde está o alvo de um projétil Q (false = o alvo sumiu)
bool RoguePosAlvoDedutivo(const Biblioteca *b, int alvo, Vector2 *pos)
{
    if (alvo > 0) {
        if (!b->ini[alvo - 1].ativo) return false;
        *pos = b->ini[alvo - 1].pos;
        return true;
    }

    if (b->chefe.ativo)     { *pos = b->chefe.pos;     return true; }
    if (b->silogista.ativo) { *pos = b->silogista.pos; return true; }
    if (b->luci.ativo)      { *pos = b->luci.pos;      return true; }
    return false;
}

// Um tiro comum acertou um alvo que pode ser ferido (alvo: k+1 = inimigo k, -1 = chefe da sala)
void RogueMarcarDedutivo(Biblioteca *b, int *marca, float *tempo, int alvo, Vector2 pos)
{
    JogadorRogue *j = &b->j;

    if (!j->seloInferencia || j->recargaDedutivo > 0.0f || *marca == 2)
        return;

    *tempo = DEDUTIVO_MARCA_DURACAO;

    if (*marca == 0) {
        *marca = 1;                             // premissa 1: P → Q
        TocarSomTom(sfxDedutivo, 1.0f);
        RogueTextoFlutuante(b, (Vector2){ pos.x, pos.y - 36 }, "P -> Q", (Color){ 0, 228, 255, 255 });
        RogueAnel(b, pos, 34.0f, (Color){ 0, 228, 255, 255 });
        return;
    }

    // Premissa 2: P. Conclusão: Q
    *marca = 2;
    *tempo = 0.6f;                              // os símbolos ficam um instante na tela
    j->recargaDedutivo = DEDUTIVO_RECARGA * (j->seloAmplificado ? SIL_BH_DEDUTIVO_RECARGA : 1.0f);

    Vector2 origem = { pos.x, fmaxf(ArenaRogue().y + 8.0f, pos.y - 70.0f) };
    Projetil *q = RogueDisparar(b, origem, (Vector2){ 0, 1 }, DEDUTIVO_VEL, RogueDanoTiro(b) * DEDUTIVO_DANO * (j->seloAmplificado ? SIL_BH_DEDUTIVO_BONUS : 1.0f), 7.0f, true);
    if (q != NULL) q->alvoDedutivo = alvo;

    RogueParticulas(b, origem, GOLD, 12, 140);
    TocarSomTom(sfxDedutivo, 1.6f);                         // o encadeamento tem som próprio
    RogueTextoFlutuante(b, (Vector2){ pos.x, pos.y - 36 }, "P!  LOGO, Q", GOLD);
    RogueAnel(b, pos, 46.0f, GOLD);
}

static void PassarMarcaDedutiva(int *marca, float *tempo, float dt)
{
    if (*marca == 0)
        return;

    *tempo -= dt;
    if (*tempo <= 0.0f)
        *marca = 0;
}

void RogueAtualizarDedutivo(Biblioteca *b, float dt)
{
    if (b->j.recargaDedutivo > 0.0f)
        b->j.recargaDedutivo -= dt;

    for (int i = 0; i < MAX_INIMIGOS; i++)
        PassarMarcaDedutiva(&b->ini[i].marca, &b->ini[i].tempoMarca, dt);

    PassarMarcaDedutiva(&b->chefe.marca, &b->chefe.tempoMarca, dt);
    PassarMarcaDedutiva(&b->silogista.marca, &b->silogista.tempoMarca, dt);
    PassarMarcaDedutiva(&b->luci.marca, &b->luci.tempoMarca, dt);
}

void RoguePulso(Biblioteca *b, ProgressoCorredor *prog)
{
    JogadorRogue *j = &b->j;

    if (j->recargaPulso > 0.0f || j->ep < PULSO_CUSTO_EP)
        return;

    j->ep -= PULSO_CUSTO_EP;
    j->recargaPulso = PULSO_RECARGA;
    j->efeitoPulso = 0.4f;

    float raio = RogueRaioPulso(j), danoPulso = RogueDanoPulso(j);
    Vector2 refletir[16];
    int refletidas = 0;

    // Apaga as balas inimigas por perto (Pulso Reflexivo: guarda onde estavam)
    for (int i = 0; i < MAX_PROJETEIS; i++) {
        Projetil *p = &b->proj[i];
        if (p->ativo && !p->doJogador && Vector2Distance(p->pos, j->pos) < raio) {
            p->ativo = false;
            RogueParticulas(b, p->pos, SKYBLUE, 3, 80);
            if (RogueUp(j, UP_PULSO_REFLEXIVO) > 0 && refletidas < 16)
                refletir[refletidas++] = p->pos;
        }
    }

    for (int r = 0; r < refletidas; r++) {
        Vector2 d = Vector2Subtract(refletir[r], j->pos);
        d = (Vector2Length(d) > 0.01f) ? Vector2Normalize(d) : j->mira;
        Projetil *q = RogueDisparar(b, refletir[r], d, 420.0f, 7.0f * RogueFatorDano(b), 4.0f, true);
        if (q != NULL) { q->secundario = true; q->alcance = 320.0f; q->arma = ARMA_DISPARADOR; }
    }

    int derrubados = 0;

    // Fere, empurra e interrompe os ataques de quem está perto
    for (int i = 0; i < MAX_INIMIGOS; i++) {

        Inimigo *e = &b->ini[i];

        if (!e->ativo || Vector2Distance(e->pos, j->pos) > raio + e->raio)
            continue;

        Vector2 dir = Vector2Normalize(Vector2Subtract(e->pos, j->pos));

        if (e->tipo != INIMIGO_ECO) {
            RogueInterromperInimigo(e);
            e->empurrao = Vector2Add(e->empurrao, Vector2Scale(dir, 260.0f * DEF_INIMIGOS[e->tipo].empurrao));
        }

        RogueFerirInimigo(b, prog, e, danoPulso, dir);
        if (!e->ativo) derrubados++;
    }

    // Circuito Fechado (sinergia): cada derrubado pelo Pulso devolve energia e recarga
    if (derrubados > 0 && RogueUp(j, UP_SIN_CIRCUITO) > 0) {
        j->ep = fminf(RogueEpMax(b), j->ep + 12.0f * (float)derrubados);
        j->recargaPulso = fmaxf(0.0f, j->recargaPulso - 1.0f * (float)derrubados);
    }

    if (b->chefe.ativo && Vector2Distance(b->chefe.pos, j->pos) < raio + CHEFE_RAIO)
        RogueFerirChefe(b, danoPulso, b->chefe.pos);

    if (b->silogista.ativo && Vector2Distance(b->silogista.pos, j->pos) < raio + SIL_RAIO)
        RogueFerirSilogista(b, danoPulso, b->silogista.pos);

    if (b->luci.ativo && Vector2Distance(b->luci.pos, j->pos) < raio + LUCI_RAIO)
        RogueFerirLuciChefe(b, danoPulso, b->luci.pos);

    TocarGrupo(&gJogPulso, 1.0f, 0.85f);
}

// ---------- Armas, itens temporários e upgrades (expansão) ----------

void RogueAnel(Biblioteca *b, Vector2 pos, float raio, Color cor)
{
    for (int i = 0; i < MAX_ANEIS; i++)
        if (b->aneis[i].tempo <= 0.0f) {
            b->aneis[i] = (AnelEfeito){ pos, raio, 0.35f, cor };
            return;
        }
}

// Poça ou explosão do jogador (Resíduo de Dados, Esquiva Fragmentada). Fere em RogueAtualizarAreas.
void RogueCriarArea(Biblioteca *b, Vector2 pos, float raio, float atraso, float vida, float dps, float danoInstantaneo, Color cor)
{
    for (int i = 0; i < MAX_AREAS; i++)
        if (!b->areas[i].ativo) {
            b->areas[i] = (AreaDano){ true, pos, raio, atraso, vida, vida, dps, 0.0f, danoInstantaneo, cor };
            return;
        }
}
// Sigilo de Luz: runa que se desenha no chão, e depois vira uma coluna de luz
static bool RogueCriarSigilo(Biblioteca *b, Vector2 pos, float raio, float dano, Color cor)
{
    for (int i = 0; i < MAX_AREAS; i++)
        if (!b->areas[i].ativo) {
            b->areas[i] = (AreaDano){ true, pos, raio, SIGILO_ATRASO, RAIO_RESIDUO_TEMPO, RAIO_RESIDUO_TEMPO, RAIO_RESIDUO_DPS, 0.0f, dano, cor, 1 };
            return true;
        }
    return false;
}

// Inimigo mais perto de um ponto (dentro do raio), sem contar "ignorar" (-1 = nenhum)
int RogueInimigoMaisPerto(const Biblioteca *b, Vector2 pos, float raio, int ignorar)
{
    int melhor = -1;
    float menor = raio;

    for (int k = 0; k < MAX_INIMIGOS; k++) {
        const Inimigo *e = &b->ini[k];
        if (!e->ativo || k == ignorar || e->surgindo > 0.0f || e->selado || e->tipo == INIMIGO_ISCA)
            continue;
        float d = Vector2Distance(pos, e->pos);
        if (d < menor) { menor = d; melhor = k; }
    }

    return melhor;
}

// Dano em área (Orbe de Buffer e Pulso de Firewall). Empurra para fora do centro.
void RogueDanoArea(Biblioteca *b, ProgressoCorredor *prog, Vector2 centro, float raio, float dano, float empurrao)
{
    for (int k = 0; k < MAX_INIMIGOS; k++) {

        Inimigo *e = &b->ini[k];

        if (!e->ativo || e->surgindo > 0.0f || Vector2Distance(centro, e->pos) > raio + e->raio)
            continue;

        Vector2 dir = Vector2Subtract(e->pos, centro);
        dir = (Vector2Length(dir) > 0.01f) ? Vector2Normalize(dir) : (Vector2){ 0, -1 };

        bool selado = e->selado;
        RogueFerirInimigo(b, prog, e, dano, dir);
        if (e->ativo && !selado)
            e->empurrao = Vector2Add(e->empurrao, Vector2Scale(dir, empurrao * DEF_INIMIGOS[e->tipo].empurrao));
    }

    if (b->chefe.ativo && Vector2Distance(centro, b->chefe.pos) < raio + CHEFE_RAIO)
        RogueFerirChefe(b, dano, b->chefe.pos);

    if (b->silogista.ativo && Vector2Distance(centro, b->silogista.pos) < raio + SIL_RAIO)
        RogueFerirSilogista(b, dano, b->silogista.pos);

    if (b->luci.ativo && Vector2Distance(centro, b->luci.pos) < raio + LUCI_RAIO)
        RogueFerirLuciChefe(b, dano, b->luci.pos);

    // Os escudos orbitais dos mini-chefes também quebram com dano em área (Orbe, Pulso, Katana, Tornado...)
    for (int k = 0; k < MAX_INIMIGOS; k++) {
        Inimigo *e = &b->ini[k];
        if (!e->ativo || e->afixo != AFIXO_ORBITAL) continue;
        for (int z = 0; z < AFIXO_ORBES; z++) {
            if (e->hpOrbe[z] <= 0.0f) continue;
            Vector2 o = RogueOrbePos(e, z);
            if (Vector2Distance(o, centro) > raio + AFIXO_ORBE_RAIO) continue;
            e->hpOrbe[z] -= dano;
            RogueParticulas(b, o, GOLD, 4, 120);
            if (e->hpOrbe[z] <= 0.0f) {
                RogueParticulas(b, o, WHITE, 14, 200);
                TocarSomTom(sfxAcerto, 1.5f);
            }
        }
    }

    // ... e a parede corrompida da câmara secreta racha com qualquer golpe que a alcance
    if (b->temParedeSecreta && !b->passagemAberta) {
        Rectangle r = PAREDE_SECRETA_ROGUE[b->paredeSecreta];
        Vector2 ponto = { Clamp(centro.x, r.x, r.x + r.width), Clamp(centro.y, r.y, r.y + r.height) };
        if (Vector2Distance(ponto, centro) <= raio) {
            Projetil golpe = {0};
            golpe.pos = ponto;
            golpe.dano = dano;
            RogueAcertarParede(b, prog, &golpe);
        }
    }
}

void RogueExplodirOrbe(Biblioteca *b, ProgressoCorredor *prog, Vector2 centro)
{
    float k = (RogueUp(&b->j, UP_ARMA_ORBE) > 0) ? 1.5f : 1.0f;      // Buffer Expandido
    // Implosão: puxa quem está perto para o centro e, uma fração de segundo depois, explode
    bool buraco = RogueUp(&b->j, UP_ARMA_ORBE_BURACO) > 0;              // Buraco Negro
    float raioPuxa = ORBE_RAIO_EXPLOSAO * k * (buraco ? 2.8f : 1.9f);
    for (int i = 0; i < MAX_INIMIGOS; i++) {
        Inimigo *e = &b->ini[i];
        if (!e->ativo || e->selado || e->surgindo > 0.0f || Vector2Distance(centro, e->pos) > raioPuxa + e->raio) continue;
        Vector2 para = Vector2Subtract(centro, e->pos);
        if (Vector2Length(para) < 0.01f) continue;
        e->empurrao = Vector2Add(e->empurrao, Vector2Scale(Vector2Normalize(para), (buraco ? 460.0f : 300.0f) * DEF_INIMIGOS[e->tipo].empurrao));
    }
    RogueAnel(b, centro, raioPuxa, Fade(DEF_ARMAS[ARMA_ORBE].cor, 0.6f));
    bool nova = RogueUp(&b->j, UP_ARMA_ORBE_SUPERNOVA) > 0;               // Supernova: campo que queima
    RogueCriarArea(b, centro, ORBE_RAIO_EXPLOSAO * k, 0.28f, nova ? 2.5f : 0.05f, nova ? 14.0f : 0.0f, ORBE_DANO_EXPLOSAO * k * 1.6f * RogueFatorDano(b), DEF_ARMAS[ARMA_ORBE].cor);
    RogueParticulas(b, centro, DEF_ARMAS[ARMA_ORBE].cor, 12, 120);
    TocarGrupo(&gOrbeImplosao, 1.0f, 1.0f);
}

// Pulso de Firewall (arma): onda em volta que fere, empurra e apaga as balas inimigas
static void RoguePulsoFirewall(Biblioteca *b, ProgressoCorredor *prog, float dano)
{
    JogadorRogue *j = &b->j;
    float raio = DEF_ARMAS[ARMA_PULSO_FW].alcance * ((RogueUp(j, UP_ARMA_PULSO_FW) > 0) ? 1.35f : 1.0f);

    for (int i = 0; i < MAX_PROJETEIS; i++) {
        Projetil *p = &b->proj[i];
        if (p->ativo && !p->doJogador && Vector2Distance(p->pos, j->pos) < raio) {
            p->ativo = false;
            RogueParticulas(b, p->pos, DEF_ARMAS[ARMA_PULSO_FW].cor, 3, 80);
        }
    }

    RogueDanoArea(b, prog, j->pos, raio, dano, PULSO_FW_EMPURRAO);
    RogueAnel(b, j->pos, raio, DEF_ARMAS[ARMA_PULSO_FW].cor);
    TocarGrupo(&gArma[ARMA_PULSO_FW], g_tomPulsoFW, (g_tomPulsoFW > 1.0f) ? 0.6f : 1.0f);
}

// Fragmentação de Pacotes: o acerto se divide em pedaços que vão para os inimigos mais perto
static void RogueFragmentar(Biblioteca *b, Vector2 pos, float dano, int atingido)
{
    unsigned int usados = 0;

    for (int f = 0; f < FRAGMENTACAO_PEDACOS; f++) {

        int alvo = -1;
        float menor = FRAGMENTACAO_ALCANCE;
        for (int k = 0; k < MAX_INIMIGOS; k++) {
            const Inimigo *e = &b->ini[k];
            if (!e->ativo || k == atingido || (usados & (1u << (k % 32))) || e->surgindo > 0.0f) continue;
            if (Vector2Distance(pos, e->pos) < menor) { menor = Vector2Distance(pos, e->pos); alvo = k; }
        }

        Vector2 dir;
        if (alvo >= 0) {
            usados |= 1u << (alvo % 32);
            dir = Vector2Normalize(Vector2Subtract(b->ini[alvo].pos, pos));
        } else {
            float ang = (float)GetRandomValue(0, 360) * DEG2RAD;
            dir = (Vector2){ cosf(ang), sinf(ang) };
        }

        Projetil *q = RogueDisparar(b, pos, dir, 480.0f, dano, 3.0f, true);
        if (q != NULL) {
            q->secundario = true;                       // fragmento não se fragmenta de novo
            q->alcance = FRAGMENTACAO_ALCANCE;
            q->ultimoAlvo = atingido + 1;
        }
    }

    RogueParticulas(b, pos, LIME, 8, 140);
}

// Analisador Heurístico: devolve o multiplicador deste acerto (alvo: k+1 = inimigo, -1 = chefe)
float RogueAnalisar(Biblioteca *b, int alvo)
{
    JogadorRogue *j = &b->j;

    if (RogueQtd(j, ITEM_ANALISADOR) == 0)
        return 1.0f;

    j->tempoAnalise = ANALISADOR_TEMPO;

    if (j->alvoAnalise != alvo) {                       // mudou de alvo: recomeça
        j->alvoAnalise = alvo;
        j->marcasAnalise = 1;
        return 1.0f;
    }

    if (j->marcasAnalise >= ANALISADOR_MARCAS) {         // padrão identificado: golpe reforçado
        j->marcasAnalise = 0;
        TocarSomTom(sfxAcerto, 1.6f);
        return 1.0f + ANALISADOR_BONUS;
    }

    j->marcasAnalise++;
    return 1.0f;
}

// Disparo contado para a Fragmentação de Pacotes
static void RogueContarDisparo(JogadorRogue *j)
{
    if (RogueQtd(j, ITEM_FRAGMENTACAO) == 0 || j->fragmentacaoArmada)
        return;

    if (++j->disparos >= FRAGMENTACAO_A_CADA) {
        j->disparos = 0;
        j->fragmentacaoArmada = true;
    }
}

static Projetil *RogueTiroArma(Biblioteca *b, TipoArma arma, Vector2 dir, float dano)
{
    const JogadorRogue *j = &b->j;
    const DefArma *a = &DEF_ARMAS[arma];
    Vector2 cano = Vector2Add(j->pos, Vector2Add((Vector2){ 0, 4 }, Vector2Scale(j->mira, 20.0f)));
    float alc = (float)RogueUp(j, UP_ALCANCE);                  // Janela de Contexto
    Projetil *p = RogueDisparar(b, cano, dir, a->velocidade * (1.0f + UP_VEL_PROJETIL_NIVEL * alc), dano, a->raio, true);

    if (p != NULL) {
        p->arma = arma;
        p->alcance = a->alcance * (1.0f + UP_ALCANCE_NIVEL * alc);

        // Disparos da build: ricochete e perfuração (o Orbe explode, então não entra)
        if (arma != ARMA_ORBE) {
            p->ricochetes += RogueUp(j, UP_RICOCHETE);
            if (RogueUp(j, UP_RICOCHETE) > 0 && RogueUp(j, UP_SIN_TEMPESTADE) > 0) p->ricochetes++;
            p->perfura += RogueUp(j, UP_PERFURAR);
        }
        if (arma == ARMA_DISPARADOR)
            p->raio += 1.5f * (float)RogueUp(j, UP_ARMA_DISPARADOR);  // Disparador Calibrado
    }
    return p;
}

// Amostragem Múltipla (projéteis extras em leque) e Eco Benigno (chance de duplicar o disparo)
static void RogueDisparosExtras(Biblioteca *b, TipoArma arma, Vector2 mira, float dano, int perfura)
{
    JogadorRogue *j = &b->j;
    int extras = RogueUp(j, UP_RAJADA);

    // Com o Fluxo Contínuo (muito rápido), só 1 a cada 3 tiros ganha os extras
    if (arma == ARMA_FLUXO && extras > 0 && (++j->contaFluxo % 3) != 0)
        extras = 0;

    // Nível 1: 1 extra; nível 2: 2 extras (um de cada lado)
    for (int k = 0; k < extras; k++) {
        float lado = (k % 2 == 0) ? 1.0f : -1.0f;
        float ang = lado * 9.0f * (float)(k / 2 + 1) * DEG2RAD;
        Projetil *q = RogueTiroArma(b, arma, Vector2Rotate(mira, ang), dano * UP_RAJADA_DANO);
        if (q == NULL) continue;
        q->secundario = (arma != ARMA_ORBE);
        q->perfura += perfura;
        if (arma == ARMA_ROTEADOR) { q->guiado = true; q->ricochetes += ROTEADOR_RICOCHETES; q->vida = ROTEADOR_VIDA; }
    }

    if (RogueUp(j, UP_DUPLICAR) > 0 && GetRandomValue(1, 100) <= UP_DUPLICAR_CHANCE * RogueUp(j, UP_DUPLICAR)) {
        Projetil *q = RogueTiroArma(b, arma, Vector2Rotate(mira, 5.0f * DEG2RAD), dano);
        if (q != NULL) {
            q->perfura += perfura;
            if (arma == ARMA_ROTEADOR) { q->guiado = true; q->ricochetes += ROTEADOR_RICOCHETES; q->vida = ROTEADOR_VIDA; }
            RogueParticulas(b, q->pos, (Color){ 0, 228, 48, 255 }, 3, 60);
        }
    }
}

// Katana de Bytes: golpe em arco na frente do Thomas (com um pequeno avanço). A zona de corte fere
// quem estiver nela, empurra e apaga as balas inimigas que pega.
static void RogueGolpeKatana(Biblioteca *b, ProgressoCorredor *prog, float dano)
{
    JogadorRogue *j = &b->j;
    bool largo = RogueUp(j, UP_ARMA_KATANA_ARCO) > 0;                     // Arco Ampliado
    float alcance = KATANA_ALCANCE * (largo ? 1.35f : 1.0f);
    float arco = DEF_ARMAS[ARMA_KATANA].abertura * (largo ? 1.45f : 1.0f);

    dano *= 1.0f + 0.25f * (float)RogueUp(j, UP_ARMA_KATANA_AFIADA);      // Lâmina Afiada
    bool vento = RogueUp(j, UP_ARMA_KATANA_VENTO) > 0;                    // Vento: Ventilação

    // Avanço curto para dentro do golpe
    j->pos = Vector2Add(j->pos, Vector2Scale(j->mira, 10.0f));
    RogueEmpurrarParaFora(&j->pos, RAIO_CORPO_JOGADOR, b->obst, b->numObst);

    Vector2 centro = Vector2Add(j->pos, Vector2Scale(j->mira, alcance * 0.55f));
    float raio = alcance * 0.62f;

    // O desenho do arco
    j->katanaAnim = 0.18f;
    j->katanaAng = atan2f(j->mira.y, j->mira.x);
    j->katanaArco = arco;
    j->katanaAlcance = alcance;

    // Corta as balas inimigas (Reflexo Espelhado: voltam como tiros do Thomas)
    Vector2 devolver[16];
    int devolvidas = 0;
    for (int i = 0; i < MAX_PROJETEIS; i++) {
        Projetil *p = &b->proj[i];
        if (!p->ativo || p->doJogador || Vector2Distance(p->pos, centro) > raio * (vento ? 1.5f : 1.0f) + p->raio)
            continue;
        p->ativo = false;
        RogueParticulas(b, p->pos, WHITE, 3, 90);
        if (RogueUp(j, UP_ARMA_KATANA_REFLEXO) > 0 && devolvidas < 16)
            devolver[devolvidas++] = p->pos;
    }
    for (int r = 0; r < devolvidas; r++) {
        Vector2 d = Vector2Subtract(devolver[r], j->pos);
        d = (Vector2Length(d) > 0.01f) ? Vector2Normalize(d) : j->mira;
        Projetil *q = RogueDisparar(b, devolver[r], d, 460.0f, dano * 0.5f, 4.0f, true);
        if (q != NULL) { q->secundario = true; q->alcance = 340.0f; q->arma = ARMA_DISPARADOR; }
    }

    // Quem está na zona de corte (para os elementos: o fogo e o gelo pegam em quem sobreviver)
    int naZona[MAX_INIMIGOS], numZona = 0;
    for (int k = 0; k < MAX_INIMIGOS; k++) {
        const Inimigo *e = &b->ini[k];
        if (e->ativo && !e->selado && e->surgindo <= 0.0f && Vector2Distance(centro, e->pos) <= raio + e->raio)
            naZona[numZona++] = k;
    }

    RogueDanoArea(b, prog, centro, raio, dano, vento ? 420.0f : 170.0f);

    // Elementos: FOGO (queima), GELO (congela)
    int nivelFogo = RogueUp(j, UP_ARMA_KATANA_FOGO);
    bool gelo = RogueUp(j, UP_ARMA_KATANA_GELO) > 0;
    for (int n = 0; n < numZona; n++) {
        Inimigo *e = &b->ini[naZona[n]];
        if (!e->ativo) continue;
        if (nivelFogo > 0) {
            e->queima = 3.0f;
            e->queimaDano = (nivelFogo >= 2) ? 7.0f : 4.0f;
            e->cronoQueima = 0.5f;
        }
        if (gelo) e->gelo = 2.5f;
    }

    // ÁGUA: cada inimigo atingido recupera vida e energia (Resfriamento)
    if (RogueUp(j, UP_ARMA_KATANA_AGUA) > 0 && numZona > 0) {
        int acertos = (numZona > 5) ? 5 : numZona;
        j->hp = fminf(RogueHpMax(b), j->hp + 1.5f * (float)acertos);
        j->ep = fminf(RogueEpMax(b), j->ep + 4.0f * (float)acertos);
        RogueParticulas(b, j->pos, (Color){ 80, 170, 255, 255 }, 4, 80);
    }

    // RAIO: 3 golpes SEGUIDOS que acertam (um golpe no vazio zera a contagem): um raio salta entre até 3 inimigos
    bool surto = false;
    if (RogueUp(j, UP_ARMA_KATANA_RAIO) > 0) {
        if (numZona > 0) {
            b->katanaGolpes++;
            if (b->katanaGolpes < 3)                                   // carregando: faíscas que crescem a cada acerto
                RogueParticulas(b, j->pos, (Color){ 255, 240, 120, 255 }, 3 * b->katanaGolpes, 60.0f + 40.0f * (float)b->katanaGolpes);
            else
                surto = true;
        } else if (b->katanaGolpes > 0) {
            b->katanaGolpes = 0;                                       // errou: perde a carga
            RogueParticulas(b, j->pos, GRAY, 4, 50);
        }
    }
    if (surto) {
        b->katanaGolpes = 0;
        Vector2 de = j->pos;
        bool usados[MAX_INIMIGOS] = { false };
        for (int salto = 0; salto < 3; salto++) {
            int alvo = -1;
            float menor = 240.0f;
            for (int k = 0; k < MAX_INIMIGOS; k++) {
                const Inimigo *e = &b->ini[k];
                if (!e->ativo || usados[k] || e->selado || e->surgindo > 0.0f || e->tipo == INIMIGO_ISCA) continue;
                float d = Vector2Distance(de, e->pos);
                if (d < menor) { menor = d; alvo = k; }
            }
            if (alvo < 0) break;
            usados[alvo] = true;
            Inimigo *e = &b->ini[alvo];
            b->raioK[salto].de = de;                     // o relâmpago desenhado (DesenharRaiosKatana)
            b->raioK[salto].para = e->pos;
            b->raioK[salto].vida = 0.30f + 0.05f * (float)salto;
            b->raioK[salto].semente = (unsigned int)GetRandomValue(1, 1000000);
            RogueParticulas(b, e->pos, (Color){ 255, 240, 120, 255 }, 6, 140);    // faíscas no impacto
            Vector2 dir = Vector2Subtract(e->pos, de);
            dir = (Vector2Length(dir) > 0.01f) ? Vector2Normalize(dir) : j->mira;
            bool antes = b->golpeSecundario;
            b->golpeSecundario = true;
            RogueFerirInimigo(b, prog, e, dano * 0.6f, dir);
            b->golpeSecundario = antes;
            RogueTextoFlutuante(b, (Vector2){ e->pos.x, e->pos.y - 24 }, "SURTO!", (Color){ 255, 240, 120, 255 });
            de = e->pos;
        }
        TocarSomTom(sfxAcerto, 1.8f);
        b->tremor = fmaxf(b->tremor, 0.10f);
    }

    // Tornado de Dados: o golpe também invoca um redemoinho (um por vez)
    if (RogueUp(j, UP_ARMA_KATANA_ONDA) > 0 && !b->torAtivo) {
        b->torAtivo = true;
        b->torPos = Vector2Add(j->pos, Vector2Scale(j->mira, 40.0f));
        b->torDir = j->mira;
        b->torVida = 3.2f;
        b->torTick = 0.0f;
        b->torDano = dano * 0.25f;
        RogueAnel(b, b->torPos, 50.0f, (Color){ 150, 230, 255, 255 });
    }

    RogueParticulas(b, centro, (Color){ 220, 235, 255, 255 }, 6, 180);
    b->tremor = fmaxf(b->tremor, 0.05f);
    TocarSomArma(ARMA_KATANA, 1.0f);
}

// Dispara a arma equipada. "atirando" = botão ou seta segurados neste quadro.
void RogueUsarArma(Biblioteca *b, ProgressoCorredor *prog, bool atirando, float dt)
{
    JogadorRogue *j = &b->j;
    TipoArma arma = RogueArmaAtual(j);
    const DefArma *a = &DEF_ARMAS[arma];
    float fator = RogueFatorDano(b);
    Vector2 cano = Vector2Add(j->pos, Vector2Add((Vector2){ 0, 4 }, Vector2Scale(j->mira, 20.0f)));

    if (arma == ARMA_DISPARADOR)
        fator *= 1.0f + 0.25f * (float)RogueUp(j, UP_ARMA_DISPARADOR);      // Disparador Calibrado

    if (j->katanaAnim > 0.0f)
        j->katanaAnim -= dt;

    // Fluxo Contínuo: esfria quando não está atirando com ela; superaquecida, só volta fria
    if (!(atirando && arma == ARMA_FLUXO) || j->superaquecida) {
        j->calor -= (j->superaquecida ? FLUXO_ESFRIA_TRAVADO : FLUXO_ESFRIA) * dt;
        if (j->calor <= 0.0f) { j->calor = 0.0f; j->superaquecida = false; }
    }

    // Pulso de Firewall: o eco volta logo depois do pulso principal
    if (j->ecoPulso > 0.0f) {
        j->ecoPulso -= dt;
        if (j->ecoPulso <= 0.0f) {
            g_tomPulsoFW = 1.22f;                                   // o eco soa mais agudo
            RoguePulsoFirewall(b, prog, j->ecoDano);
            g_tomPulsoFW = 1.0f;
            if (j->ecoN > 0) { j->ecoN--; j->ecoPulso = 0.38f; }        // Eco Duplo
        }
    }

    if (j->tempoEsquiva > 0.0f)
        return;

    // Vetor de Compilação: segurar carrega, soltar dispara (mais carga, mais dano)
    if (arma == ARMA_VETOR) {
        if (atirando) {
            float velCarga = (RogueUp(j, UP_ARMA_VETOR) > 0) ? 1.4f : 1.0f;    // Compilação Otimizada
            float cargaAntes = j->carga;
            if (j->cronoTiro <= 0.0f) j->carga = fminf(VETOR_CARGA_MAX, j->carga + dt * velCarga);
            if (cargaAntes < VETOR_CARGA_MAX * 0.98f && j->carga >= VETOR_CARGA_MAX * 0.98f)
                TocarGrupo(&gVetorCarga, 1.0f, 0.9f);              // carga cheia: o tiro perfeito está pronto
            return;
        }
        if (j->carga >= VETOR_CARGA_MIN) {
            float k = j->carga / VETOR_CARGA_MAX;
            float danoVetor = (a->dano + (VETOR_DANO_MAX - a->dano) * k) * fator;
            if (RogueUp(j, UP_ARMA_VETOR_LENTE) > 0) danoVetor *= 1.3f;       // Lente de Cristal
            if (RogueUp(j, UP_ARMA_VETOR_ESTAMPIDO) > 0 && k >= 0.5f) {      // Estampido Sônico
                RogueDanoArea(b, prog, j->pos, 80.0f, 4.0f * fator, 280.0f);
                RogueAnel(b, j->pos, 80.0f, a->cor);
            }
            int atravessa = VETOR_ATRAVESSA + ((RogueUp(j, UP_ARMA_VETOR) > 0) ? 2 : 0);
            if (j->carga >= VETOR_CARGA_MAX * 0.98f) {          // carga total: tiro perfeito
                danoVetor *= 1.5f;
                atravessa = 99;
                RogueAviso(b, "TIRO PERFEITO!");
                RogueAnel(b, cano, 40.0f, a->cor);
            }
            Projetil *p = RogueTiroArma(b, arma, j->mira, danoVetor);
            if (p != NULL) p->perfura += atravessa;
            RogueDisparosExtras(b, arma, j->mira, danoVetor, atravessa);
            RogueParticulas(b, cano, a->cor, 6 + (int)(10 * k), 140);
            TocarSomArma(ARMA_VETOR, 1.15f - 0.25f * k);           // mais carga, mais grave
            j->cronoTiro = RogueIntervaloArma(j, arma);
            RogueContarDisparo(j);
        }
        j->carga = 0.0f;
        return;
    }

    if (!atirando || j->cronoTiro > 0.0f || (arma == ARMA_FLUXO && j->superaquecida))
        return;

    if (arma == ARMA_PULSO_FW) {
        RoguePulsoFirewall(b, prog, a->dano * fator);
        j->ecoPulso = 0.38f;
        j->ecoDano = a->dano * fator * 0.7f;
        j->ecoN = (RogueUp(j, UP_ARMA_PULSO_ECO) > 0) ? 1 : 0;
        if (RogueUp(j, UP_ARMA_PULSO_REPARO) > 0) j->hp = fminf(RogueHpMax(b), j->hp + 0.6f);     // Campo de Reparo
    }
    else if (arma == ARMA_KATANA) {
        RogueGolpeKatana(b, prog, a->dano * fator);
    }
    else if (arma == ARMA_RAIO_CLASSICO) {
        // Gasta EP: sem energia suficiente, não atira
        float custo = RAIOC_CUSTO_EP * ((RogueUp(j, UP_ARMA_RAIOC_SUPER) > 0) ? RAIOC_SUPER_FATOR : 1.0f);
        if (j->ep < custo) {
            j->cronoTiro = 0.45f;
            RogueTextoFlutuante(b, (Vector2){ j->pos.x, j->pos.y - 30 }, "SEM EP!", ORANGE);
            j->epPiscaAte = GetTime() + 0.6;
            TocarSom(sfxErro);
            return;
        }
        j->ep -= custo;
        bool lente = RogueUp(j, UP_ARMA_RAIOC_LENTE) > 0;
        Projetil *r = RogueTiroArma(b, arma, j->mira, a->dano * fator * (lente ? 1.4f : 1.0f));
        if (r != NULL) {
            r->perfura += 99;
            if (lente) r->raio += 3.0f;
        }
        RogueAnel(b, cano, 26.0f, a->cor);
        RogueParticulas(b, cano, a->cor, 8, 160);
        TocarSomArma(ARMA_RAIO_CLASSICO, 1.0f);
    }
    else if (arma == ARMA_RAIO) {
        // SIGILO DE LUZ: gasta EP, escolhe o inimigo mais perto na direção da mira e desenha ali uma runa que vira coluna de luz
        float custo = RAIO_CUSTO_EP * ((RogueUp(j, UP_ARMA_RAIO_SUPER) > 0) ? RAIO_SUPER_FATOR : 1.0f);
        if (j->ep < custo) {
            j->cronoTiro = 0.45f;
            RogueTextoFlutuante(b, (Vector2){ j->pos.x, j->pos.y - 30 }, "SEM EP!", ORANGE);
            j->epPiscaAte = GetTime() + 0.6;
            TocarSom(sfxErro);
            return;
        }
        j->ep -= custo;
        bool lente = RogueUp(j, UP_ARMA_RAIO_LENTE) > 0;
        Vector2 dirM = Vector2Normalize(j->mira);
        if (Vector2Length(dirM) < 0.1f) dirM = (Vector2){ 1, 0 };
        int alvoI = -1;
        float melhorD = 420.0f;
        for (int k = 0; k < MAX_INIMIGOS; k++) {
            const Inimigo *e = &b->ini[k];
            if (!e->ativo || e->surgindo > 0.0f || e->selado || e->tipo == INIMIGO_ISCA) continue;
            Vector2 para = Vector2Subtract(e->pos, j->pos);
            float d = Vector2Length(para);
            if (d > melhorD || d < 1.0f) continue;
            if (Vector2DotProduct(dirM, Vector2Scale(para, 1.0f / d)) < 0.766f) continue;      // cone de 40 graus
            melhorD = d;
            alvoI = k;
        }
        Vector2 alvo = (alvoI >= 0) ? b->ini[alvoI].pos : Vector2Add(j->pos, Vector2Scale(dirM, 210.0f));
        Rectangle ar = ArenaRogue();
        alvo.x = Clamp(alvo.x, ar.x + 24.0f, ar.x + ar.width - 24.0f);
        alvo.y = Clamp(alvo.y, ar.y + 24.0f, ar.y + ar.height - 24.0f);
        if (!RogueCriarSigilo(b, alvo, lente ? 84.0f : 62.0f, a->dano * fator * (lente ? 1.4f : 1.0f), a->cor)) {
            j->ep += custo;                 // sem vaga para o sigilo: devolve o EP em vez de perder
            j->cronoTiro = 0.15f;
            return;
        }
        RogueAnel(b, cano, 26.0f, a->cor);
        RogueParticulas(b, cano, a->cor, 8, 160);
        TocarSomArma(ARMA_RAIO, 1.0f);                             // o circuito se desenhando
    }
    else if (arma == ARMA_FRAGMENTADORA) {
        // Cone Denso: +3 fragmentos e cone mais fechado; Amostragem Múltipla: +2 por nível
        int n = a->projeteis + ((RogueUp(j, UP_ARMA_FRAGMENTADORA) > 0) ? 3 : 0) + 2 * RogueUp(j, UP_RAJADA);
        float abertura = a->abertura * ((RogueUp(j, UP_ARMA_FRAGMENTADORA) > 0) ? 0.75f : 1.0f);
        float danoFrag = a->dano * fator;
        bool dourado = (++j->cartucho % ((RogueUp(j, UP_ARMA_FRAG_POLVORA) > 0) ? 3 : 4)) == 0;               // a cada 4 tiros: cartucho dourado
        if (dourado) {
            n += (RogueUp(j, UP_ARMA_FRAG_POLVORA) > 0) ? 6 : 4;
            danoFrag *= 1.5f;
            RogueAnel(b, cano, 34.0f, GOLD);
        }
        for (int k = 0; k < n; k++) {
            float ang = ((float)k / (float)(n - 1) - 0.5f) * abertura + (float)GetRandomValue(-3, 3);
            Projetil *q = RogueTiroArma(b, arma, Vector2Rotate(j->mira, ang * DEG2RAD), danoFrag);
            if (q != NULL && dourado) q->perfura += 1;
            if (q != NULL && RogueUp(j, UP_ARMA_FRAG_QUICANTE) > 0) q->ricochetes += 1;     // Fragmentos Quicantes
        }
        // Coice: o tiro empurra o Thomas para trás
        j->pos = Vector2Subtract(j->pos, Vector2Scale(j->mira, dourado ? 14.0f : 8.0f));
        RogueEmpurrarParaFora(&j->pos, RAIO_CORPO_JOGADOR, b->obst, b->numObst);
        if (dourado) TocarGrupo(&gFragDourado, 1.0f, 1.0f);
        else         TocarSomArma(ARMA_FRAGMENTADORA, 1.0f);
    }
    else {
        float espalhar = (float)GetRandomValue(-10, 10) / 10.0f * (a->abertura / 2.0f) * DEG2RAD;
        Vector2 mira = Vector2Rotate(j->mira, espalhar);
        float danoTiro = a->dano * fator;
        if (arma == ARMA_FLUXO) danoTiro *= 0.8f + ((RogueUp(j, UP_ARMA_FLUXO_OVERCLOCK) > 0) ? 1.2f : 0.7f) * j->calor;       // esquenta: bate mais forte
        if (arma == ARMA_ROTEADOR) {
            // Salva de 3 pacotes em leque, cada um procurando um alvo diferente
            // Começa com 1 pacote; cada nível de PACOTE EXTRA soma mais um (até 3). Menos dano por pacote quando são vários.
            int pacotes = 1 + RogueUp(j, UP_ARMA_ROT_BROADCAST);
            float porPacote = (pacotes == 1) ? 1.2f : ((pacotes == 2) ? 0.9f : 0.7f);
            for (int s = 0; s < pacotes; s++) {
                float graus = ((float)s - (float)(pacotes - 1) / 2.0f) * 13.0f;
                Projetil *p = RogueTiroArma(b, arma, Vector2Rotate(mira, graus * DEG2RAD), danoTiro * porPacote);
                if (p == NULL) continue;
                if (RogueUp(j, UP_ARMA_ROT_EXPRESSA) > 0) p->vel = Vector2Scale(p->vel, 1.35f);    // Rota Expressa
                p->guiado = true;
                p->ricochetes += ROTEADOR_RICOCHETES + ((RogueUp(j, UP_ARMA_ROTEADOR) > 0) ? 2 : 0);    // Tabela de Rotas
                p->vida = ROTEADOR_VIDA;
                if (s != 0) p->secundario = true;
            }
            RogueDisparosExtras(b, arma, mira, danoTiro * porPacote, 0);
        }
        else {
            Projetil *p = RogueTiroArma(b, arma, mira, danoTiro);
            (void)p;
            RogueDisparosExtras(b, arma, mira, danoTiro, 0);
        }
        TocarSomArma(arma, (arma == ARMA_FLUXO) ? 0.92f + 0.45f * j->calor : 1.0f);    // o Fluxo fica mais agudo quando esquenta
    }

    if (arma == ARMA_FLUXO) {
        j->calor += FLUXO_CALOR_TIRO * ((RogueUp(j, UP_ARMA_FLUXO) > 0) ? 0.6f : 1.0f);    // Resfriamento Líquido
        if (j->calor >= 1.0f) {
            j->calor = 1.0f;
            j->superaquecida = true;
            RogueAviso(b, "FLUXO CONTÍNUO SUPERAQUECIDO!");
            bool valvula = RogueUp(j, UP_ARMA_FLUXO_VALVULA) > 0;               // Válvula de Escape
            float raioVent = valvula ? 150.0f : 110.0f;
            RogueDanoArea(b, prog, j->pos, raioVent, 18.0f * fator * (valvula ? 2.0f : 1.0f), 240.0f);   // explosão em volta
            RogueAnel(b, j->pos, raioVent, a->cor);
            RogueParticulas(b, j->pos, a->cor, 16, 200);
            TocarSomTom(sfxErro, 1.2f);
        }
    }

    RogueParticulas(b, cano, a->cor, 2, 60);
    j->cronoTiro = RogueIntervaloArma(j, arma);
    RogueContarDisparo(j);
}

// Fim do alcance ou do tempo de vida: o Orbe explode, os outros somem
void RogueFimProjetil(Biblioteca *b, ProgressoCorredor *prog, Projetil *p)
{
    p->ativo = false;

    if (p->doJogador && p->arma == ARMA_ORBE && !p->secundario)
        RogueExplodirOrbe(b, prog, p->pos);
    else
        RogueParticulas(b, p->pos, p->doJogador ? DEF_ARMAS[p->arma].cor : ORANGE, 2, 60);
}

// Um projétil do jogador acertou o inimigo k: dano, efeitos e o que acontece com o projétil
void RogueAcertarInimigo(Biblioteca *b, ProgressoCorredor *prog, Projetil *p, int k)
{
    Inimigo *e = &b->ini[k];
    bool selado = e->selado;
    bool principal = (p->alvoDedutivo == 0 && !p->secundario);
    Vector2 dir = Vector2Normalize(p->vel);
    float dano = p->dano;

    if (principal && !selado)
        dano *= RogueAnalisar(b, k + 1);

    // Alta Confiança: acerto crítico
    bool critico = false;
    if (principal && !selado && RogueUp(&b->j, UP_CRITICO) > 0 && GetRandomValue(1, 100) <= UP_CRITICO_CHANCE * RogueUp(&b->j, UP_CRITICO)) {
        critico = true;
        dano *= 2.0f;
        RogueParticulas(b, p->pos, GOLD, 8, 180);
    }

    b->golpeSecundario = !principal;
    RogueFerirInimigo(b, prog, e, dano, dir);
    b->golpeSecundario = false;

    // Reformulação: efeitos da build no acerto (só o disparo principal: sem reação em cadeia)
    if (principal && !selado) {
        JogadorRogue *j = &b->j;

        if (e->ativo && RogueUp(j, UP_ROTULAR) > 0)
            e->rotulo = UP_ROTULO_TEMPO;                        // Rotulagem de Dados

        if (RogueUp(j, UP_EXPLOSAO) > 0 && j->recargaExplosao <= 0.0f) {       // Saída Generativa
            float raio = UP_EXPLOSAO_RAIO + 14.0f * (float)(RogueUp(j, UP_EXPLOSAO) - 1);
            j->recargaExplosao = UP_EXPLOSAO_RECARGA;
            b->golpeSecundario = true;
            RogueDanoArea(b, prog, p->pos, raio, p->dano * UP_EXPLOSAO_DANO, 60.0f);
            b->golpeSecundario = false;
            RogueAnel(b, p->pos, raio, (Color){ 255, 161, 0, 255 });
        }

        if (RogueUp(j, UP_RASTRO) > 0 && j->recargaRastro <= 0.0f) {           // Resíduo de Dados
            j->recargaRastro = UP_RASTRO_RECARGA;
            RogueCriarArea(b, p->pos, UP_RASTRO_RAIO, 0.0f, 1.5f, UP_RASTRO_DPS * RogueFatorDano(b), 0.0f, (Color){ 0, 228, 48, 255 });
        }

        if (critico && RogueUp(j, UP_SIN_CONFIANCA) > 0) {                     // Confiança Calibrada
            int outro = RogueInimigoMaisPerto(b, p->pos, 260.0f, k);
            if (outro >= 0) {
                Vector2 d = Vector2Normalize(Vector2Subtract(b->ini[outro].pos, p->pos));
                Projetil *q = RogueDisparar(b, p->pos, d, 520.0f, p->dano * 0.5f, 4.0f, true);
                if (q != NULL) { q->secundario = true; q->ultimoAlvo = k + 1; q->arma = ARMA_DISPARADOR; q->alcance = 300.0f; }
            }
        }
    }

    if (e->ativo && !selado && p->arma == ARMA_FRAGMENTADORA)
        e->empurrao = Vector2Add(e->empurrao, Vector2Scale(dir, 140.0f * (FRAGMENTADORA_EMPURRAO - 1.0f) * DEF_INIMIGOS[e->tipo].empurrao));

    if (e->ativo && !selado && principal)
        RogueMarcarDedutivo(b, &e->marca, &e->tempoMarca, k + 1, e->pos);

    // Fragmentação de Pacotes: o próximo acerto depois de 6 disparos se divide
    if (principal && !selado && b->j.fragmentacaoArmada) {
        b->j.fragmentacaoArmada = false;
        RogueFragmentar(b, p->pos, p->dano * FRAGMENTACAO_DANO, k);
    }

    p->ultimoAlvo = k + 1;

    if (p->arma == ARMA_ORBE && !p->secundario) {
        p->ativo = false;
        RogueExplodirOrbe(b, prog, p->pos);
    }
    else if (p->perfura > 0 && !selado) {
        p->perfura--;                                   // Vetor: segue em frente
        RogueParticulas(b, p->pos, WHITE, 6, 160);
    }
    else if (p->ricochetes > 0) {
        int proximo = RogueInimigoMaisPerto(b, p->pos, ROTEADOR_RICOCHETE_RAIO, k);
        if (proximo >= 0) {
            // Tempestade de Pacotes: sem perda; Tabela de Rotas (Roteador): perde menos
            float perda = ROTEADOR_PERDA;
            if (RogueUp(&b->j, UP_SIN_TEMPESTADE) > 0)                                   perda = 1.0f;
            else if (p->arma == ARMA_ROTEADOR && RogueUp(&b->j, UP_ARMA_ROTEADOR) > 0)   perda = 0.85f;

            float vel = (p->arma == ARMA_ROTEADOR) ? DEF_ARMAS[ARMA_ROTEADOR].velocidade : fmaxf(300.0f, Vector2Length(p->vel));
            p->ricochetes--;
            p->dano *= perda;
            p->vel = Vector2Scale(Vector2Normalize(Vector2Subtract(b->ini[proximo].pos, p->pos)), vel);
            if (p->arma == ARMA_ROTEADOR) {
                p->vida = ROTEADOR_VIDA;
                p->alcance = DEF_ARMAS[ARMA_ROTEADOR].alcance;
            } else if (p->alcance > 0.0f) {
                p->alcance = fmaxf(p->alcance, ROTEADOR_RICOCHETE_RAIO + 40.0f);
            }
            RogueParticulas(b, p->pos, DEF_ARMAS[p->arma].cor, 6, 120);
        } else {
            p->ativo = false;
        }
    }
    else {
        p->ativo = false;
    }
}

// ----- Itens temporários -----

static void RogueLimpezaCache(Biblioteca *b)
{
    JogadorRogue *j = &b->j;

    // Some com as balas inimigas (até as dos chefes) ...
    for (int i = 0; i < MAX_PROJETEIS; i++) {
        Projetil *p = &b->proj[i];
        if (p->ativo && !p->doJogador) {
            p->ativo = false;
            RogueParticulas(b, p->pos, SKYBLUE, 2, 60);
        }
    }

    // ... e atordoa os inimigos comuns por perto (os chefes seguem o padrão deles)
    for (int k = 0; k < MAX_INIMIGOS; k++) {
        Inimigo *e = &b->ini[k];
        if (!e->ativo || Vector2Distance(e->pos, j->pos) > LIMPEZA_RAIO)
            continue;
        RogueInterromperInimigo(e);
        if (e->atordoado < LIMPEZA_ATORDOA) e->atordoado = LIMPEZA_ATORDOA;
    }

    RogueAnel(b, j->pos, LIMPEZA_RAIO, SKYBLUE);
}

void RogueAtivarTemporario(Biblioteca *b, TipoItem it)
{
    JogadorRogue *j = &b->j;

    switch (it) {
        // Pegar de novo só renova o tempo (o bônus não se soma)
        case ITEM_OVERCLOCK:         j->efeito[EFEITO_OVERCLOCK] = EFEITO_OVERCLOCK_DURACAO; break;
        case ITEM_FIREWALL_PORTATIL: j->efeito[EFEITO_ESCUDO] = ESCUDO_PORTATIL_DURACAO;   break;
        case ITEM_CAMPO_LATENCIA:    j->efeito[EFEITO_LATENCIA] = LATENCIA_DURACAO;         break;
        case ITEM_LIMPEZA_CACHE:     RogueLimpezaCache(b);                                  break;

        case ITEM_PROXY: {
            Vector2 lado = { -j->mira.y, j->mira.x };
            j->proxyPos = Vector2Add(j->pos, Vector2Scale(lado, 50.0f));
            RogueEmpurrarParaFora(&j->proxyPos, RAIO_CORPO_JOGADOR, b->obst, b->numObst);
            j->proxyHp = PROXY_HP;
            j->efeito[EFEITO_PROXY] = PROXY_DURACAO;
        } break;

        case ITEM_PACOTE_INTEGRIDADE:
            j->hp = fminf(RogueHpMax(b), j->hp + RogueHpMax(b) * PACOTE_INTEGRIDADE_CURA);
            RogueParticulas(b, j->pos, LIME, 20, 160);
            break;

        default:
            break;
    }

    RogueAviso(b, ITENS[it].nome);
    Telemetria("rogue_temporario", ITENS[it].nome, 0.0f, 0);
}

// Uma sala vencida (combate ou Silogista): conta para o Cache Persistente
void RogueSalaConcluida(Biblioteca *b)
{
    JogadorRogue *j = &b->j;

    j->salasConcluidas++;

    if (RogueQtd(j, ITEM_CACHE_PERSISTENTE) > 0 && j->salasConcluidas % CACHE_PERSISTENTE_SALAS == 0) {
        j->hp = fminf(RogueHpMax(b), j->hp + RogueHpMax(b) * CACHE_PERSISTENTE_CURA);
        RogueParticulas(b, j->pos, LIME, 16, 140);
        RogueAviso(b, "CACHE PERSISTENTE: integridade recuperada");
    }
}

// Tempos dos efeitos, marcas do Analisador e os anéis
void RogueAtualizarEfeitos(Biblioteca *b, float dt)
{
    JogadorRogue *j = &b->j;

    for (int i = 0; i < NUM_EFEITOS; i++)
        if (j->efeito[i] > 0.0f) j->efeito[i] -= dt;

    if (j->efeito[EFEITO_PROXY] > 0.0f && j->proxyHp <= 0.0f) {
        j->efeito[EFEITO_PROXY] = 0.0f;
        RogueParticulas(b, j->proxyPos, SKYBLUE, 16, 160);
    }

    if (j->recargaBotnet > 0.0f) j->recargaBotnet -= dt;

    // Reformulação: tempos da build
    if (j->bonusPosEsquiva > 0.0f)  j->bonusPosEsquiva -= dt;
    if (j->recargaEsquiva2 > 0.0f)  j->recargaEsquiva2 -= dt;
    if (j->recargaExplosao > 0.0f)  j->recargaExplosao -= dt;
    if (j->recargaRastro > 0.0f)    j->recargaRastro -= dt;

    // Analisador: as marcas somem com o tempo ou se o alvo sumiu
    if (j->alvoAnalise != 0) {
        j->tempoAnalise -= dt;
        bool sumiu = (j->alvoAnalise > 0) ? !b->ini[j->alvoAnalise - 1].ativo : !(b->chefe.ativo || b->silogista.ativo);
        if (j->tempoAnalise <= 0.0f || sumiu) { j->alvoAnalise = 0; j->marcasAnalise = 0; }
    }

    for (int i = 0; i < MAX_ANEIS; i++)
        if (b->aneis[i].tempo > 0.0f) b->aneis[i].tempo -= dt;
}

// Inimigos que perseguem o jogador (e por isso caem na Proxy-Isca)
bool RoguePerseguidor(TipoInimigo t)
{
    return t == INIMIGO_FRAGMENTO || t == INIMIGO_SPAMMER || t == INIMIGO_SPAM || t == INIMIGO_OVERCLOCK ||
           t == INIMIGO_CACHE || t == INIMIGO_PHISHER || t == INIMIGO_ISCA;
}
