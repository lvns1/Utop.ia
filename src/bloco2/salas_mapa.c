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

// ---------- Sorteios por raridade ----------

int RogueSortearPeso(const int *candidatos, int n, int (*peso)(int))
{
    int total = 0;
    for (int i = 0; i < n; i++)
        total += peso(candidatos[i]);

    if (n == 0 || total <= 0)
        return -1;

    int r = GetRandomValue(1, total);
    for (int i = 0; i < n; i++) {
        r -= peso(candidatos[i]);
        if (r <= 0) return candidatos[i];
    }
    return candidatos[n - 1];
}

static int PesoItem(int i) { return PESO_RARIDADE[ITENS[i].raridade]; }
static int PesoArma(int a) { return PESO_RARIDADE[DEF_ARMAS[a].raridade]; }

// Item para pedestal ou para o fim do combate: nunca temporário nem passivo que já está no máximo.
// soPassivos = só upgrades passivos. Devolve -1 se não houver nenhum.
int RogueSortearItem(const Biblioteca *b, unsigned int excluir, bool soPassivos)
{
    int candidatos[NUM_ITENS], n = 0;

    for (int i = 0; i < NUM_ITENS; i++) {
        if (ITENS[i].temporario || (excluir & (1u << i)))                 continue;
        if (ITENS[i].passivo && b->j.itens[i] >= ITENS[i].maxPilha)      continue;
        if (soPassivos && !ITENS[i].passivo)                              continue;
        candidatos[n++] = i;
    }

    return RogueSortearPeso(candidatos, n, PesoItem);
}

TipoItem RogueSortearTemporario(void)
{
    int candidatos[NUM_ITENS], n = 0;

    for (int i = 0; i < NUM_ITENS; i++)
        if (ITENS[i].temporario)
            candidatos[n++] = i;

    return (TipoItem)RogueSortearPeso(candidatos, n, PesoItem);
}

// Arma para oferecer: nunca a que o jogador já tem; de preferência uma que ainda
// não apareceu nesta tentativa. ARMA_DISPARADOR = nenhuma.
TipoArma RogueSortearArma(Biblioteca *b, unsigned int excluir)
{
    int candidatos[NUM_ARMAS], n = 0;

    for (int passada = 0; passada < 2 && n == 0; passada++)
        for (int a = ARMA_DISPARADOR + 1; a < NUM_ARMAS; a++) {
            if (a == (int)b->j.armaExtra || a == (int)b->j.armaBase || (excluir & (1u << a))) continue;
            if (passada == 0 && (b->armasOferecidas & (1u << a)))  continue;
            candidatos[n++] = a;
        }

    int a = RogueSortearPeso(candidatos, n, PesoArma);
    if (a < 0)
        return ARMA_DISPARADOR;

    b->armasOferecidas |= 1u << a;
    return (TipoArma)a;
}

// CONTINUIDADE EDUCATIVA: na 1ª vez que cada alucinação do Bloco 1 reaparece na Biblioteca, a Luci liga o inimigo ao
// conceito que o jogador acabou de aprender nos monitores (só nas primeiras missões, uma fala por inimigo)

static void RogueLigarConceitoInimigo(Biblioteca *b, TipoInimigo tipo)
{
    const char *fala = NULL;
    if (tipo == INIMIGO_FRAGMENTO)      fala = "Luci: Lembra dos monitores? Esses FRAGMENTOS são alucinações: respostas confiantes, mas falsas.";
    else if (tipo == INIMIGO_ECO)       fala = "Luci: Um ECO FALSO inventa o que não dá para verificar. É a alucinação EXTRÍNSECA!";
    else if (tipo == INIMIGO_GUARDIAO)  fala = "Luci: Esse GUARDIÃO guarda uma afirmação. Aprendeu a evitar alucinar? Verifique antes de acreditar!";
    if (fala == NULL || b->perm->tentativas > 3 || b->tipoSala == SALA_ATRIO || b->luciMsgTempo > 0.0f)
        return;
    if (b->conceitosLigados & (1u << tipo))
        return;
    b->conceitosLigados |= 1u << tipo;
    RogueRecadoLuci(b, fala);
    Telemetria("tutorial_ligacao_conceito", DEF_INIMIGOS[tipo].nome, (float)b->perm->tentativas, (int)tipo);
}

Inimigo *RogueCriarInimigo(Biblioteca *b, TipoInimigo tipo, Vector2 pos)
{
    for (int i = 0; i < MAX_INIMIGOS; i++) {

        Inimigo *e = &b->ini[i];

        if (e->ativo)
            continue;

        *e = (Inimigo){0};
        e->ativo = true;
        e->tipo = tipo;
        e->pos = pos;
        e->surgindo = TEMPO_SURGINDO;
        e->lado = (GetRandomValue(0, 1) == 0) ? -1.0f : 1.0f;
        e->recarga = (float)GetRandomValue(30, 120) / 100.0f;   // não atacam todos juntos
        e->hp = DEF_INIMIGOS[tipo].hp;
        e->raio = DEF_INIMIGOS[tipo].raio;
        e->frente = (Vector2){ 0, 1 };
        e->dono = -1;
        for (int k = 0; k < BOTNET_CONEXOES; k++)
            e->conexao[k] = -1;
        RogueLigarConceitoInimigo(b, tipo);

        // O que nasce de outro inimigo aparece na hora
        if (tipo == INIMIGO_ECO)            { e->timer = ECO_TEMPO; e->surgindo = 0.0f; }
        else if (tipo == INIMIGO_SPAM)      { e->vida = SPAM_VIDA; e->surgindo = 0.0f; }
        else if (tipo == INIMIGO_ISCA)      { e->vida = ISCA_DURACAO; e->surgindo = 0.0f; }
        else if (tipo == INIMIGO_FIREWALL)  e->habilidade = FIREWALL_INTERVALO * 0.6f;
        else if (tipo == INIMIGO_NUCLEO)    e->recarga = NUCLEO_INTERVALO * 0.5f;

        if (e->surgindo > 0.0f) {                       // uma onda inteira surgindo toca uma vez só
            static double ultimoSurgir = -10.0;
            if (SomLiberado(&ultimoSurgir, 0.35)) TocarGrupo(&gIniSurge, 1.0f, 0.45f);
        }

        // Reformulação: salas mais fundas da missão têm inimigos mais resistentes
        // (o que nasce de outro inimigo, como spam, ecos e cópias, não muda)
        if (tipo != INIMIGO_ECO && tipo != INIMIGO_SPAM && tipo != INIMIGO_ISCA && b->fatorVidaInimigos > 0.0f)
            e->hp *= b->fatorVidaInimigos;

        e->hpMax = e->hp;
        return e;
    }

    return NULL;
}

// Elite: aura dourada, mais vida, um pouco maior e com ataques reforçados (ver RogueAtualizarInimigos)
static void RogueTornarElite(Inimigo *e)
{
    if (e == NULL || e->elite)
        return;

    e->elite = true;
    e->hp *= ELITE_HP;
    e->hpMax = e->hp;
    e->raio *= ELITE_RAIO;

    // O afixo: DIVISÃO só nos que fazem sentido partidos ao meio (perseguem ou andam sozinhos)
    bool divide = (e->tipo == INIMIGO_FRAGMENTO || e->tipo == INIMIGO_GUARDIAO || e->tipo == INIMIGO_OVERCLOCK ||
                   e->tipo == INIMIGO_CACHE || e->tipo == INIMIGO_FIREWALL || e->tipo == INIMIGO_KEYLOGGER);
    e->afixo = GetRandomValue(AFIXO_PULSO, AFIXO_ORBITAL);
    if (e->afixo == AFIXO_DIVISAO && !divide)
        e->afixo = (GetRandomValue(0, 1) == 0) ? AFIXO_PULSO : AFIXO_ORBITAL;
    e->cronoAfixo = AFIXO_PULSO_INTERVALO * 0.8f;
    for (int k = 0; k < AFIXO_ORBES; k++) e->hpOrbe[k] = (e->afixo == AFIXO_ORBITAL) ? AFIXO_ORBE_HP : 0.0f;
}

Vector2 RogueOrbePos(const Inimigo *e, int k)
{
    float a = e->anim * 2.4f + (2.0f * PI / AFIXO_ORBES) * k;
    float d = e->raio + AFIXO_ORBE_DIST;
    return (Vector2){ e->pos.x + cosf(a) * d, e->pos.y + sinf(a) * d };
}

// ---------- Salas e missão ----------


// Uma sala da missão (reformulação) usa portas nos lados: nada pode ficar na frente delas
bool RogueSalaDaMissao(const Biblioteca *b)
{
    return (b->noAtual >= 0 || b->modoSeletor) && b->tipoSala != SALA_ATRIO && b->tipoSala != SALA_TRANSICAO && b->tipoSala != SALA_LUCI;
}

// Tipo do encontro especial da sala: do nó do mapa ou, no seletor (sem mapa), o escolhido
int RogueEspecialAtual(const Biblioteca *b)
{
    int esp = (b->noAtual >= 0 && b->noAtual < b->mapa.numNos) ? b->mapa.no[b->noAtual].especial : b->especialDireto;
    return (esp >= 0 && esp < NUM_ESPECIAIS) ? esp : ESP_SENTINELA;
}

// O ponto está numa faixa livre (portas e entrada)?
static bool RogueNaZonaLivre(Vector2 p, float raio)
{
    for (int z = 0; z < NUM_LADOS; z++)
        if (CheckCollisionCircleRec(p, raio, ZONAS_LIVRES_ROGUE[z]))
            return true;
    return false;
}

void RogueUsarLayout(Biblioteca *b, const LayoutSala *l)
{
    b->numObst = 0;

    for (int i = 0; i < l->numObst && b->numObst < MAX_OBSTACULOS; i++) {

        // Validação dos layouts de combate: nenhuma estante bloqueia uma porta ou a entrada
        bool bloqueia = false;
        if (RogueSalaDaMissao(b))
            for (int z = 0; z < NUM_LADOS && !bloqueia; z++)
                bloqueia = CheckCollisionRecs(l->obst[i], ZONAS_LIVRES_ROGUE[z]);

        if (!bloqueia)
            b->obst[b->numObst++] = l->obst[i];
    }

    b->numObstFixos = b->numObst;
}

// Sorteia um encontro do nível da sala, de preferência diferente do anterior.
// As salas de ELITE só usam os encontros de elite (e as outras nunca).
int RogueSortearEncontro(const Biblioteca *b, int nivel)
{
    int candidatos[NUM_ENCONTROS], n = 0;
    bool elite = (b->tipoSala == SALA_ELITE || b->eventoCombate);

    for (int passada = 0; passada < 3 && n == 0; passada++)
        for (int i = 0; i < NUM_ENCONTROS; i++) {
            if (ENCONTROS[i].salaElite != elite)                                              continue;
            if (passada < 2 && (nivel < ENCONTROS[i].nivelMin || nivel > ENCONTROS[i].nivelMax)) continue;
            if (passada == 0 && i == b->ultimoEncontro)                                       continue;
            candidatos[n++] = i;
        }

    return (n > 0) ? candidatos[GetRandomValue(0, n - 1)] : 0;
}

// Lugar para um inimigo perto de um ponto da sala: fora das estantes, longe da
// entrada do jogador e sem cair em cima de outro inimigo
Vector2 RogueLugarSurgir(const Biblioteca *b, Vector2 base, float raio)
{
    Vector2 escolhido = base;

    for (int tentativa = 0; tentativa < 16; tentativa++) {

        float ang = (float)GetRandomValue(0, 360) * DEG2RAD;
        float dist = (tentativa == 0) ? 0.0f : (float)GetRandomValue(14, 60);
        Vector2 p = { base.x + cosf(ang) * dist, base.y + sinf(ang) * dist };

        RogueEmpurrarParaFora(&p, raio, b->obst, b->numObst);

        if (Vector2Distance(p, b->j.pos) < DIST_SURGIR_JOGADOR)
            continue;

        // Reformulação: ninguém nasce na frente das portas (a não ser que não haja outro lugar)
        if (RogueSalaDaMissao(b) && tentativa < 12 && RogueNaZonaLivre(p, raio))
            continue;

        escolhido = p;
        bool livre = true;
        for (int i = 0; i < MAX_INIMIGOS && livre; i++)
            if (b->ini[i].ativo && Vector2Distance(p, b->ini[i].pos) < raio + b->ini[i].raio + 4.0f)
                livre = false;

        if (livre)
            return p;
    }

    return escolhido;
}

// Nível da sala de combate atual: 1 = inicial, 2 = intermediária, 3 = avançada
// Reformulação: o nível vem da profundidade no mapa (1 = começo, 2 = meio, 3 = fim);
// a sala de ELITE conta um nível acima. Fora do mapa, vale a contagem antiga.
int RogueNivelCombate(const Biblioteca *b)
{
    int nivel;

    if (b->noAtual >= 0) nivel = (b->profundidade <= 0) ? 1 : ((b->profundidade <= 3) ? 2 : 3);
    else                 nivel = (b->combates > 3) ? 3 : ((b->combates < 1) ? 1 : b->combates);

    if (b->tipoSala == SALA_ELITE && nivel < 3)
        nivel++;
    return nivel;
}

// Layout usado na sala de combate atual (os reforços nascem nos pontos dele)
const LayoutSala *layoutCombateAtual = &LAYOUTS_COMBATE[0];

// Cria os grupos de uma onda do encontro atual nos pontos da sala
void RogueSurgirOnda(Biblioteca *b, int onda)
{
    const LayoutSala *l = layoutCombateAtual;
    const EncontroDef *enc = &ENCONTROS[(b->encontro >= 0) ? b->encontro : 0];
    int nivel = RogueNivelCombate(b);

    // Pontos da sala em ordem sorteada
    int ordem[8];
    for (int i = 0; i < l->numPontos; i++)
        ordem[i] = i;

    for (int i = l->numPontos - 1; i > 0; i--) {
        int k = GetRandomValue(0, i);
        int t = ordem[i]; ordem[i] = ordem[k]; ordem[k] = t;
    }

    int ponto = 0;

    for (int g = 0; g < enc->numGrupos; g++) {

        const GrupoEncontro *gr = &enc->grupo[g];
        if (gr->onda != onda)
            continue;

        int qtd = GetRandomValue(gr->min, gr->max) + ((nivel >= 2 && gr->max > 1) ? 1 : 0) + ((nivel >= 3 && gr->max > 2) ? 1 : 0) + ((nivel >= 5 && gr->max > 2) ? 1 : 0);
        bool bando = (gr->tipo == INIMIGO_SPAMMER);

        for (int k = 0; k < qtd; k++) {
            Vector2 base = l->pontos[ordem[ponto % l->numPontos]];
            Inimigo *e = RogueCriarInimigo(b, gr->tipo, RogueLugarSurgir(b, base, DEF_INIMIGOS[gr->tipo].raio));
            if (gr->elite) RogueTornarElite(e);
            if (!bando || k % BANDO_SPAMMERS == BANDO_SPAMMERS - 1 || k == qtd - 1)
                ponto++;
        }
    }

    // Versão mais desafiadora: nas salas de combate mais fundas, um inimigo comum vira elite (com afixo)
    if (onda == 0 && b->tipoSala == SALA_COMBATE && b->profundidade >= PROF_ELITE_SURPRESA &&
        GetRandomValue(1, 100) <= CHANCE_ELITE_SURPRESA) {
        int cand[MAX_INIMIGOS], nc = 0;
        for (int i = 0; i < MAX_INIMIGOS; i++) {
            const Inimigo *e = &b->ini[i];
            if (e->ativo && !e->elite && !e->selado && e->tipo != INIMIGO_SPAMMER && e->tipo != INIMIGO_SPAM &&
                e->tipo != INIMIGO_NUCLEO && e->tipo != INIMIGO_ISCA && e->tipo != INIMIGO_ECO)
                cand[nc++] = i;
        }
        if (nc > 0) RogueTornarElite(&b->ini[cand[GetRandomValue(0, nc - 1)]]);
    }

    // A partir do 2º combate, um Guardião vem selado com um desafio educativo (só na 1ª onda
    // e só quando o objetivo é eliminar: assim o desafio nunca fica para trás)
    // (Versão do estande: nunca nas salas de elite e no máximo MAX_SELADOS_TENTATIVA por tentativa)
    if (onda == 0 && b->combates >= 2 && b->objetivoSala == OBJC_ELIMINAR &&
        b->tipoSala != SALA_ELITE && b->seladosTentativa < MAX_SELADOS_TENTATIVA) {
        Vector2 base = l->pontos[ordem[ponto % l->numPontos]];
        Inimigo *e = RogueCriarInimigo(b, INIMIGO_GUARDIAO, RogueLugarSurgir(b, base, GUARDIAO_RAIO));
        if (e != NULL) {
            e->selado = true;
            b->seladosTentativa++;
        }
    }
}

// Monta a sala de combate com o encontro "id" (só a 1ª onda; as outras vêm em RogueAtualizarObjetivo)
void RogueMontarEncontro(Biblioteca *b, const LayoutSala *l, int id)
{
    int nivel = RogueNivelCombate(b);
    const EncontroDef *enc = &ENCONTROS[id];

    layoutCombateAtual = l;
    b->encontro = id;
    b->ultimoEncontro = id;
    b->ondaAtual = 0;
    b->numOndas = 1;

    // Ondas extras só quando o objetivo é eliminar (os outros objetivos têm reforços próprios)
    if (b->objetivoSala == OBJC_ELIMINAR)
        for (int g = 0; g < enc->numGrupos; g++)
            if (enc->grupo[g].onda + 1 > b->numOndas)
                b->numOndas = enc->grupo[g].onda + 1;

    // Expansão da progressão: com o dobro de salas, no máximo 2 ondas por sala (a 3ª onda não vem)
    if (b->numOndas > MAX_ONDAS_SALA)
        b->numOndas = MAX_ONDAS_SALA;

    RogueSurgirOnda(b, 0);

    RogueAviso(b, TextFormat("ENCONTRO: %s", enc->nome));
    Telemetria("rogue_encontro", enc->nome, (float)nivel, b->combates);
}

// Esvazia a sala e coloca o jogador na entrada
void RogueLimparSala(Biblioteca *b, TipoSalaRogue tipo, Vector2 entrada)
{
    memset(b->proj, 0, sizeof(b->proj));
    memset(b->ini, 0, sizeof(b->ini));
    memset(b->col, 0, sizeof(b->col));
    memset(b->arm, 0, sizeof(b->arm));
    memset(b->areas, 0, sizeof(b->areas));
    b->torAtivo = false;
    memset(b->raioK, 0, sizeof(b->raioK));
    b->cenaLuciTipo = 0;
    b->cenaLuci = 0.0f;
    b->conversaDepoisCena = NULL;

    b->tipoSala = tipo;
    arenaRogueAtual = (tipo == SALA_LUCI) ? ARENA_LUCI : ARENA_ROGUE_PADRAO;
    b->encontro = -1;
    b->limpa = false;
    b->terminalLido = false;
    b->recompensaPega = false;
    b->lojaAberta = false;
    b->chefe.ativo = false;
    b->silogista.ativo = false;
    b->luci.ativo = false;
    b->fadeEntrada = 0.35f;
    b->j.pos = PontoNaArena(entrada);
    b->j.tempoEsquiva = 0.0f;
    b->j.golpesSofridos = 0;

    // Reformulação: estado de sala (portas, objetivo, armadilhas, recompensa, eventos)
    b->numPortas = 0;
    b->numObstFixos = 0;
    b->objetivoSala = OBJC_ELIMINAR;
    b->ondaAtual = 0;
    b->numOndas = 1;
    b->avisoOnda = 0.0f;
    b->tempoObjetivo = b->tempoObjetivoMax = 0.0f;
    b->reforco = 0.0f;
    b->alvoPrioritario = -1;
    b->numMecanismos = 0;
    memset(b->progressoMecanismo, 0, sizeof(b->progressoMecanismo));
    memset(b->mecanismoDesligado, 0, sizeof(b->mecanismoDesligado));
    b->temParedeSecreta = false;
    b->passagemAberta = false;
    b->paredeFalsa = -1;
    b->paredeMarcada = -1;
    b->marcaParede = 0.0f;
    b->recompensaAtiva = false;
    b->escolhaAberta = false;
    b->escolhaPendente = -1;
    b->eventoAberto = false;
    b->eventoResolvido = false;
    b->eventoUsado = false;
    b->eventoCombate = false;
    b->estadoDesafio = 0;
    b->contadorDesafio = 0;
    b->registroSala = false;
    b->registroSalaLido = false;
    b->mensagemSistema = 0.0f;
    b->mapaAberto = false;
    memset(&b->lab, 0, sizeof(b->lab));        // expansão educativa: o laboratório não passa de uma sala para outra
    memset(&b->janela, 0, sizeof(b->janela));  // expansão da progressão: o mini-chefe também não
    b->janela.indice = -1;

    // Bloco 3: impacto e parry não passam de uma sala para outra
    b->tremorForte = 0.0f;
    b->zoomImpacto = 0.0f;
    b->tempoErro = 0.0f;
    memset(b->textos, 0, sizeof(b->textos));
    memset(&b->j.parry, 0, sizeof(b->j.parry));
}

// ---------- Reformulação: montagem das salas da missão ----------

// Lado oposto (sair pela porta da esquerda = entrar na próxima pela direita)
static LadoPorta RogueLadoOposto(LadoPorta l)
{
    switch (l) {
        case LADO_CIMA:     return LADO_BAIXO;
        case LADO_ESQUERDA: return LADO_DIREITA;
        case LADO_DIREITA:  return LADO_ESQUERDA;
        default:            return LADO_CIMA;
    }
}

// Distribui as saídas do nó atual nas paredes: nunca na parede por onde o jogador entrou.
// Duas saídas: a que fica mais à esquerda no mapa vai na porta mais à esquerda.
static void RoguePrepararPortas(Biblioteca *b)
{
    b->numPortas = 0;
    if (b->noAtual < 0)
        return;

    const NoMapa *n = &b->mapa.no[b->noAtual];
    if (n->numSaidas <= 0)
        return;

    // Destinos da esquerda para a direita (como aparecem no mapa)
    int dest[MAX_SAIDAS];
    for (int k = 0; k < n->numSaidas; k++) dest[k] = n->saida[k];
    for (int a = 0; a < n->numSaidas; a++)
        for (int c = a + 1; c < n->numSaidas; c++)
            if (b->mapa.no[dest[c]].x < b->mapa.no[dest[a]].x) { int t = dest[a]; dest[a] = dest[c]; dest[c] = t; }

    if (n->numSaidas >= 3) {
        // Três caminhos: esquerda, cima e direita (só acontece em salas em que se entra por baixo)
        static const LadoPorta tres[3] = { LADO_ESQUERDA, LADO_CIMA, LADO_DIREITA };
        for (int k = 0; k < 3; k++)
            b->portas[k] = (PortaMissao){ tres[k], dest[k], PORTA_LADO_ROGUE[tres[k]] };
        b->numPortas = 3;
        return;
    }

    if (n->numSaidas == 1) {
        // Câmara e chefe: sempre pela porta de cima (as duas arenas foram feitas para entrar por baixo)
        LadoPorta lado = LADO_CIMA;
        TipoSalaRogue t = b->mapa.no[dest[0]].tipo;
        if (t != SALA_SILOGISTA && t != SALA_CHEFE && t != SALA_ESPECIAL && GetRandomValue(0, 2) == 0) {
            LadoPorta lat = (GetRandomValue(0, 1) == 0) ? LADO_ESQUERDA : LADO_DIREITA;
            if (lat == b->ladoEntrada) lat = RogueLadoOposto(lat);
            lado = lat;
        }
        b->portas[0] = (PortaMissao){ lado, dest[0], PORTA_LADO_ROGUE[lado] };
        b->numPortas = 1;
        return;
    }

    // Pares possíveis, sempre da esquerda para a direita: esquerda+cima, cima+direita, esquerda+direita
    static const LadoPorta pares[3][2] = { { LADO_ESQUERDA, LADO_CIMA }, { LADO_CIMA, LADO_DIREITA }, { LADO_ESQUERDA, LADO_DIREITA } };
    int validos[3], nv = 0;
    for (int p = 0; p < 3; p++)
        if (pares[p][0] != b->ladoEntrada && pares[p][1] != b->ladoEntrada)
            validos[nv++] = p;

    int p = validos[GetRandomValue(0, nv - 1)];
    for (int k = 0; k < 2; k++)
        b->portas[k] = (PortaMissao){ pares[p][k], dest[k], PORTA_LADO_ROGUE[pares[p][k]] };
    b->numPortas = 2;
}

// Objetivo da sala de combate (a 1ª sala é sempre "eliminar"; elite também)
static ObjetivoCombate RogueSortearObjetivo(const Biblioteca *b)
{
    if (b->tipoSala == SALA_ELITE || b->profundidade == 0 || b->noAtual < 0)
        return OBJC_ELIMINAR;

    static const int peso[NUM_OBJC] = { 40, 15, 15, 15, 15 };
    int total = 0;
    for (int i = 0; i < NUM_OBJC; i++) total += peso[i];

    int r = GetRandomValue(1, total);
    for (int i = 0; i < NUM_OBJC; i++) {
        r -= peso[i];
        if (r <= 0) return (ObjetivoCombate)i;
    }
    return OBJC_ELIMINAR;
}

// Lugar livre para uma armadilha ou objeto: dentro da arena, fora das estantes, das portas,
// longe da entrada do jogador e de outras armadilhas
static bool RogueLugarLivre(const Biblioteca *b, Vector2 p, float raio, float longeJogador)
{
    if (p.x < ArenaRogue().x + raio + 10 || p.x > ArenaRogue().x + ArenaRogue().width - raio - 10) return false;
    if (p.y < ArenaRogue().y + raio + 10 || p.y > ArenaRogue().y + ArenaRogue().height - raio - 10) return false;
    if (RogueNaZonaLivre(p, raio))                                                   return false;
    if (Vector2Distance(p, b->j.pos) < longeJogador)                                 return false;

    for (int i = 0; i < b->numObst; i++)
        if (CheckCollisionCircleRec(p, raio + 6, b->obst[i]))
            return false;

    for (int i = 0; i < MAX_ARMADILHAS; i++) {
        const Armadilha *a = &b->arm[i];
        if (!a->ativo) continue;
        if ((a->tipo == ARM_ZONA || a->tipo == ARM_TORRE) && Vector2Distance(a->pos, p) < a->raio + raio + 30) return false;
        if ((a->tipo == ARM_BARREIRA || a->tipo == ARM_BLOQUEIO) && CheckCollisionCircleRec(p, raio + 20, a->r))   return false;
    }
    return true;
}

bool RogueSortearLugar(const Biblioteca *b, float raio, float longeJogador, Vector2 *saida)
{
    for (int t = 0; t < 40; t++) {
        Vector2 p = { (float)GetRandomValue(100, 700), (float)GetRandomValue(120, 470) };
        if (RogueLugarLivre(b, p, raio, longeJogador)) { *saida = p; return true; }
    }
    return false;
}

// Cria uma armadilha. Todas avisam antes de machucar (ver RogueAtualizarArmadilhas).
void RogueCriarArmadilha(Biblioteca *b, TipoArmadilha tipo)
{
    Armadilha *a = NULL;
    for (int i = 0; i < MAX_ARMADILHAS && a == NULL; i++)
        if (!b->arm[i].ativo) a = &b->arm[i];
    if (a == NULL)
        return;

    *a = (Armadilha){ .ativo = true, .tipo = tipo };
    Vector2 p;

    switch (tipo) {
        case ARM_FEIXE:
            // Linha que corta a arena inteira; nunca em cima da entrada
            a->vertical = GetRandomValue(0, 1) == 0;
            for (int t = 0; t < 20; t++) {
                float c = a->vertical ? (float)GetRandomValue(120, 680) : (float)GetRandomValue(130, 450);
                float jog = a->vertical ? b->j.pos.x : b->j.pos.y;
                if (fabsf(c - jog) < 90.0f) continue;
                a->r = a->vertical ? (Rectangle){ c - FEIXE_LARGURA / 2, ArenaRogue().y, FEIXE_LARGURA, ArenaRogue().height }
                                   : (Rectangle){ ArenaRogue().x, c - FEIXE_LARGURA / 2, ArenaRogue().width, FEIXE_LARGURA };
                break;
            }
            if (a->r.width <= 0.0f) { a->ativo = false; return; }
            a->timer = FEIXE_DESCANSO + (float)GetRandomValue(0, 150) / 100.0f;
            break;

        case ARM_ZONA:
            if (!RogueSortearLugar(b, ZONA_RAIO, 150.0f, &p)) { a->ativo = false; return; }
            a->pos = p;
            a->raio = ZONA_RAIO;
            a->timer = 1.5f + (float)GetRandomValue(0, 100) / 100.0f;
            break;

        case ARM_TORRE:
            if (!RogueSortearLugar(b, 16.0f, 220.0f, &p)) { a->ativo = false; return; }
            a->pos = p;
            a->raio = 16.0f;
            a->timer = TORRE_INTERVALO;
            break;

        case ARM_EXPLOSAO:
            a->raio = EXPLOSAO_RAIO;
            a->timer = EXPLOSAO_INTERVALO;
            break;

        case ARM_BARREIRA: {
            // Barreira que vai e volta no meio da arena (longe das portas)
            a->vertical = GetRandomValue(0, 1) == 0;
            if (a->vertical) {                  // barra em pé, anda para os lados
                a->r = (Rectangle){ 230, (float)GetRandomValue(160, 300), 22, 100 };
                a->limiteA = 230; a->limiteB = 548;
            } else {                            // barra deitada, anda para cima e para baixo
                a->r = (Rectangle){ (float)GetRandomValue(220, 480), 170, 100, 22 };
                a->limiteA = 170; a->limiteB = 392;
            }
            a->vel = BARREIRA_VEL * ((GetRandomValue(0, 1) == 0) ? 1.0f : -1.0f);
        } break;

        case ARM_BLOQUEIO:
            if (!RogueSortearLugar(b, 56.0f, 160.0f, &p)) { a->ativo = false; return; }
            a->r = (Rectangle){ p.x - 40, p.y - 40, 80, 80 };
            a->timer = BLOQUEIO_LIVRE;
            break;

        default:
            break;
    }
}

// Quantas armadilhas e de que tipo (cresce com a profundidade; a 1ª sala não tem)
static int RogueQtdArmadilhas(const Biblioteca *b)
{
    int n = 0;
    if (b->profundidade == 0)       n = 0;
    else if (b->profundidade <= 2)  n = (GetRandomValue(1, 100) <= 55) ? 1 : 0;
    else if (b->profundidade <= 4)  n = 1;
    else                            n = GetRandomValue(1, 2);
    if (b->tipoSala == SALA_ELITE)  n++;
    return n;
}

static TipoArmadilha RogueSortearTipoArmadilha(const Biblioteca *b)
{
    int maximo = (b->profundidade <= 2) ? ARM_EXPLOSAO : ARM_BLOQUEIO;
    return (TipoArmadilha)GetRandomValue(0, maximo);
}

// Objetos do objetivo: núcleos, alvo prioritário ou consoles do mecanismo
static void RogueMontarObjetivo(Biblioteca *b, const LayoutSala *l)
{
    switch (b->objetivoSala) {

        case OBJC_SOBREVIVER:
            b->tempoObjetivoMax = b->tempoObjetivo = fminf(38.0f, SOBREVIVER_TEMPO_BASE + 2.5f * (float)b->profundidade);
            b->reforco = REFORCO_INTERVALO * 0.6f;
            break;

        case OBJC_NUCLEOS: {
            // Os núcleos ficam longe da entrada e longe uns dos outros
            int criados = 0;
            for (int t = 0; t < 60 && criados < NUM_NUCLEOS; t++) {
                Vector2 p = l->pontos[GetRandomValue(0, l->numPontos - 1)];
                p.x += (float)GetRandomValue(-30, 30);
                p.y += (float)GetRandomValue(-30, 30);
                if (!RogueLugarLivre(b, p, DEF_INIMIGOS[INIMIGO_NUCLEO].raio, 200.0f)) continue;

                bool perto = false;
                for (int i = 0; i < MAX_INIMIGOS; i++)
                    if (b->ini[i].ativo && b->ini[i].tipo == INIMIGO_NUCLEO && Vector2Distance(b->ini[i].pos, p) < 170.0f) perto = true;
                if (perto) continue;

                RogueCriarInimigo(b, INIMIGO_NUCLEO, p);
                criados++;
            }
            if (criados == 0) b->objetivoSala = OBJC_ELIMINAR;     // sem lugar: vira um combate comum
            b->reforco = REFORCO_INTERVALO;
        } break;

        case OBJC_PRIORITARIO: {
            // O RETRANSMISSOR: um Botnet de elite no ponto mais longe da entrada
            Vector2 melhor = l->pontos[0];
            for (int i = 1; i < l->numPontos; i++)
                if (Vector2Distance(l->pontos[i], b->j.pos) > Vector2Distance(melhor, b->j.pos)) melhor = l->pontos[i];

            Inimigo *e = RogueCriarInimigo(b, INIMIGO_BOTNET, RogueLugarSurgir(b, melhor, DEF_INIMIGOS[INIMIGO_BOTNET].raio));
            if (e != NULL) {
                RogueTornarElite(e);
                e->prioritario = true;
                e->hp *= PRIORITARIO_HP;
                e->hpMax = e->hp;
                b->alvoPrioritario = (int)(e - b->ini);
            } else {
                b->objetivoSala = OBJC_ELIMINAR;
            }
            b->reforco = REFORCO_INTERVALO;
        } break;

        case OBJC_MECANISMO: {
            b->numMecanismos = 0;
            for (int t = 0; t < 80 && b->numMecanismos < NUM_MECANISMOS; t++) {
                Vector2 p;
                if (!RogueSortearLugar(b, 26.0f, 140.0f, &p)) continue;
                bool perto = false;
                for (int k = 0; k < b->numMecanismos; k++)
                    if (Vector2Distance(b->posMecanismo[k], p) < 180.0f) perto = true;
                if (perto) continue;
                b->posMecanismo[b->numMecanismos++] = p;
            }
            if (b->numMecanismos == 0) b->objetivoSala = OBJC_ELIMINAR;
            b->reforco = REFORCO_INTERVALO;
        } break;

        default:
            break;
    }
}

// Algumas salas escondem uma passagem para a sala secreta (parede corrompida).
// A Luci às vezes avisa, mas, instável, pode apontar a parede errada.
static void RogueMontarParedeSecreta(Biblioteca *b)
{
    if (b->noAtual < 0 || b->mapa.no[b->noAtual].secreta < 0)
        return;

    const NoMapa *s = &b->mapa.no[b->mapa.no[b->noAtual].secreta];
    if (s->visitado)
        return;

    b->temParedeSecreta = true;
    b->paredeSecreta = GetRandomValue(0, 3);
    b->hpParede = PAREDE_SECRETA_HP;

    if (GetRandomValue(1, 100) <= 70) {
        bool errou = GetRandomValue(1, 100) <= RogueChanceLuciErrar(b);
        int onde = b->paredeSecreta;
        if (errou) {
            b->paredeFalsa = (b->paredeSecreta + GetRandomValue(1, 3)) % 4;
            onde = b->paredeFalsa;
        }
        RogueRecadoLuci(b, TextFormat("Luci: Sinto um eco atrás da parede %s...", NOMES_PAREDES_SECRETAS[onde]));
        b->luciMsgTempo = 5.0f;
    }
}

// Recompensa no chão no meio da sala (núcleo de melhoria, arsenal, fragmentos ou restauração)
void RogueColocarRecompensa(Biblioteca *b, TipoRecompensa tipo)
{
    if (tipo == RECOMPENSA_NENHUMA)
        return;

    Vector2 p = { 400, 290 };
    for (int t = 0; t < 30; t++) {
        Vector2 q = (t == 0) ? p : (Vector2){ 400.0f + (float)GetRandomValue(-140, 140), 290.0f + (float)GetRandomValue(-100, 100) };
        bool livre = true;
        for (int i = 0; i < b->numObst && livre; i++)
            if (CheckCollisionCircleRec(q, 26, b->obst[i])) livre = false;
        if (livre) { p = q; break; }
    }

    b->recompensaAtiva = true;
    b->recompensaTipo = tipo;
    b->recompensaPos = p;
    RogueParticulas(b, p, GOLD, 24, 200);
}

static int RogueSortearEvento(Biblioteca *b)
{
    int candidatos[NUM_EVENTOS_SORTEAVEIS], n = 0;

    for (int passada = 0; passada < 2 && n == 0; passada++)
        for (int e = 0; e < NUM_EVENTOS_SORTEAVEIS; e++) {
            if (passada == 0 && (b->eventosVistos & (1u << e))) continue;
            if (e == EVT_MEMORIA && b->memoriasVistas == (1u << NUM_MEMORIAS) - 1) continue;
            candidatos[n++] = e;
        }

    int e = (n > 0) ? candidatos[GetRandomValue(0, n - 1)] : EVT_PROTOCOLO;
    b->eventosVistos |= 1u << e;
    return e;
}

void RogueEntrarSala(Biblioteca *b, TipoSalaRogue tipo)
{
    // Na missão, o jogador aparece do lado por onde entrou; fora dela, embaixo
    Vector2 entrada = (b->noAtual >= 0) ? ENTRADA_LADO_ROGUE[b->ladoEntrada] : SPAWN_ROGUE;
    RogueLimparSala(b, tipo, entrada);
    RoguePrepararPortas(b);

    if (tipo == SALA_COMBATE || tipo == SALA_ELITE) {

        int idLayout = GetRandomValue(0, NUM_LAYOUTS_COMBATE - 1);
        const LayoutSala *l = &LAYOUTS_COMBATE[idLayout];
        RogueUsarLayout(b, l);
        b->combates++;
        b->objetivoSala = RogueSortearObjetivo(b);

        int armadilhas = RogueQtdArmadilhas(b) + ((idLayout == LAYOUT_AREA_ABERTA) ? 1 : 0);
        for (int k = 0; k < armadilhas; k++)
            RogueCriarArmadilha(b, RogueSortearTipoArmadilha(b));

        RogueMontarEncontro(b, l, RogueSortearEncontro(b, RogueNivelCombate(b)));
        RogueMontarObjetivo(b, l);
        RogueMontarParedeSecreta(b);

        if (b->objetivoSala != OBJC_ELIMINAR)
            RogueAviso(b, TextFormat("OBJETIVO: %s", NOMES_OBJC[b->objetivoSala]));
        if (tipo == SALA_ELITE && b->luciMsgTempo <= 0.0f)
            RogueRecadoLuci(b, FALAS_LUCI_SALA[SALA_ELITE]);
        if (tipo == SALA_ELITE) {
            RogueAviso(b, "ELITE À FRENTE!");
            MostrarEmocaoLuci(EMOCAO_ALERTA, 2.5f);
        }
    }
    else if (tipo == SALA_EDUCATIVA) {
        RogueUsarLayout(b, &LAYOUT_EDUCATIVA);

        // Instável, a Luci começa a se contradizer
        if (RogueEstab(b) <= 3) {
            int nivel = (int)Clamp((float)(4 - RogueEstab(b)), 1.0f, 3.0f);
            b->falaPendente = FALAS_LUCI_CORROMPIDA[nivel - 1];
        }

        // Reformulação: um arquivo de ameaça opcional (narrativa sobre um inimigo)
        b->registroSala = true;
        b->registroSalaId = GetRandomValue(0, NUM_AMEACAS - 1);
        b->posRegistroSala = POS_REGISTRO_SALA;
    }
    else if (tipo == SALA_RECOMPENSA) {

        RogueUsarLayout(b, &LAYOUT_RECOMPENSA);
        b->limpa = true;                        // pode pegar uma recompensa ou seguir em frente

        // Três recompensas diferentes, sorteadas pela raridade: no máximo uma arma;
        // nenhum temporário e nenhum passivo que já está no máximo
        unsigned int usados = 0;
        int comArma = (GetRandomValue(1, 100) <= CHANCE_ARMA_PEDESTAL) ? GetRandomValue(0, NUM_PEDESTAIS - 1) : -1;

        for (int i = 0; i < NUM_PEDESTAIS; i++) {
            b->armaPedestal[i] = (i == comArma) ? RogueSortearArma(b, 0) : ARMA_DISPARADOR;
            int it = RogueSortearItem(b, usados, false);
            b->pedestal[i] = (TipoItem)((it < 0) ? ITEM_ANTIVIRUS : it);
            usados |= 1u << b->pedestal[i];
        }
    }
    else if (tipo == SALA_EVENTO || tipo == SALA_DESCANSO || tipo == SALA_DESAFIO) {
        // Salas sem combate obrigatório: a porta já está aberta
        RogueUsarLayout(b, &LAYOUT_EVENTO);
        layoutCombateAtual = &LAYOUT_EVENTO;
        b->limpa = true;
        b->posObjetoSala = POS_OBJETO_SALA;

        if (tipo == SALA_EVENTO) {
            b->evento = (TipoEvento)RogueSortearEvento(b);
            if (b->evento == EVT_FONTE_FALSA)          b->varianteEvento = GetRandomValue(0, NUM_QUIZ_FONTES - 1);
            else if (b->evento == EVT_RESPOSTA_IA)     b->varianteEvento = GetRandomValue(0, NUM_QUIZ_RESPOSTAS - 1);
            else if (b->evento == EVT_MODELO_EXPERIMENTAL) {
                int livres[3], n = 0;
                for (int p = 0; p < 3; p++) if (!(b->j.penalidade & (1 << p))) livres[n++] = p;
                b->varianteEvento = (n > 0) ? livres[GetRandomValue(0, n - 1)] : 0;
            }
            else if (b->evento == EVT_MEMORIA) {
                int livres[NUM_MEMORIAS], n = 0;
                for (int m = 0; m < NUM_MEMORIAS; m++) if (!(b->memoriasVistas & (1u << m))) livres[n++] = m;
                b->varianteEvento = (n > 0) ? livres[GetRandomValue(0, n - 1)] : GetRandomValue(0, NUM_MEMORIAS - 1);
            }
        } else if (tipo == SALA_DESCANSO) {
            b->evento = EVT_DESCANSO;
        } else {
            b->evento = EVT_DESAFIO;
            b->desafioSala = (TipoDesafioSala)GetRandomValue(0, NUM_DESAFIOS_SALA - 1);
        }

        if (FALAS_LUCI_SALA[tipo] != NULL) {
            RogueRecadoLuci(b, FALAS_LUCI_SALA[tipo]);
            b->luciMsgTempo = 4.5f;
        }
    }
    else if (tipo == SALA_ESPECIAL) {
        // Expansão da progressão: elite ou mini-chefe obrigatório
        RogueMontarSalaEspecial(b, RogueEspecialAtual(b));
    }
    else if (tipo == SALA_LABORATORIO) {
        // Expansão educativa: o console do laboratório no meio da sala
        RogueUsarLayout(b, &LAYOUT_EVENTO);
        layoutCombateAtual = &LAYOUT_EVENTO;
        b->posObjetoSala = POS_OBJETO_SALA;
        TipoLab lab = (b->noAtual >= 0) ? (TipoLab)b->mapa.no[b->noAtual].lab : LAB_PROMPTS;
        LabPreparar(b, lab);
        b->limpa = true;                       // desafio opcional: a porta já abre (a recompensa sai só ao concluir)
        RogueRecadoLuci(b, FALAS_LUCI_LAB[lab]);
        b->luciMsgTempo = 4.5f;
    }
    else if (tipo == SALA_SECRETA) {
        // Arquivo oculto: uma recompensa rara, fragmentos e uma memória da Luci
        RogueUsarLayout(b, &LAYOUT_SECRETA);
        b->limpa = true;
        RogueColocarRecompensa(b, RECOMPENSA_RARA);
        for (int k = 0; k < 4; k++)
            RogueSoltar(b, (Vector2){ 400, 330 }, true, ITEM_ANTIVIRUS);

        int livres[NUM_MEMORIAS], n = 0;
        for (int m = 0; m < NUM_MEMORIAS; m++) if (!(b->memoriasVistas & (1u << m))) livres[n++] = m;
        b->registroSala = true;
        b->registroSalaId = 100 + ((n > 0) ? livres[GetRandomValue(0, n - 1)] : GetRandomValue(0, NUM_MEMORIAS - 1));   // 100+ = memória
        b->posRegistroSala = (Vector2){ 400, 150 };

        RogueRecadoLuci(b, FALAS_LUCI_SALA[SALA_SECRETA]);
        b->luciMsgTempo = 4.0f;
    }
    else if (tipo == SALA_TRANSICAO || tipo == SALA_LUCI) {
        RogueMontarSalaBloco3(b, tipo);
    }
    else if (tipo == SALA_SILOGISTA) {
        RogueUsarLayout(b, &LAYOUT_CHEFE);
        b->saiuSilogista = false;
        b->segurandoSaida = 0.0f;
        if (SILOGISTA_BH) RogueIniciarSilogistaBH(b);       // versão nova (desafio opcional)
        else              RogueIniciarSilogista(b);         // versão legada (Câmara obrigatória)
        b->conversaPendente = SILOGISTA_BH ? &dialogoSilogistaIntroBH : &dialogoSilogistaIntro;
        b->objConversa = OBJ_SILOGISTA_INTRO;
    }
    else {
        RogueUsarLayout(b, &LAYOUT_CHEFE);
        RogueIniciarChefe(b);
        RogueMudarEstabilidade(b, -1, "o eco do Alucinador");
        b->conversaPendente = &dialogoAlucinadorIntro;
        b->objConversa = OBJ_CHEFE_INTRO;
    }

    // Mensagem de sistema corrompido (narrativa ambiental) em algumas salas da missão
    if (RogueSalaDaMissao(b) && tipo != SALA_CHEFE && tipo != SALA_SILOGISTA && GetRandomValue(1, 100) <= 35) {
        snprintf(b->textoSistema, sizeof(b->textoSistema), "%s", MENSAGENS_SISTEMA[GetRandomValue(0, NUM_MENSAGENS_SISTEMA - 1)]);
        b->mensagemSistema = 4.0f;
    }
}

// Volta para o átrio (chegando do corredor, depois da derrota ou no fim da missão)
void RogueEntrarAtrio(Biblioteca *b, bool vindoDoCorredor)
{
    b->entradaAtrio = GetTime();
    b->noAtual = -1;
    b->profundidade = 0;
    b->ladoEntrada = LADO_BAIXO;
    b->fatorVidaInimigos = b->fatorRitmoInimigos = b->fatorDanoSala = 1.0f;

    RogueLimparSala(b, SALA_ATRIO, vindoDoCorredor ? SPAWN_ATRIO : (Vector2){ 400, 420 });
    RogueUsarLayout(b, &LAYOUT_ATRIO);

    // Terminais de atalho: sólidos (o Thomas encosta na frente deles)
    const Vector2 terminais[2] = { POS_TERMINAL_SELETOR, POS_TERMINAL_BLOCO3 };
    for (int k = 0; k < 2 && b->numObst < MAX_OBSTACULOS; k++)
        b->obst[b->numObst++] = (Rectangle){ terminais[k].x - 19, terminais[k].y - 24, 38, 40 };
    b->numObstFixos = b->numObst;
    b->seletorAberto = false;

    b->indiceRota = -1;
    b->limpa = true;
    RogueLimparEfeitos(&b->j);
    b->j.hp = RogueHpMax(b);
    b->j.ep = RogueEpMax(b);
    b->j.direcao = vindoDoCorredor ? DIR_DIREITA : DIR_COSTAS;

    // Armas à venda no Byteco (diferentes entre si)
    unsigned int naLoja = 0;
    for (int i = 0; i < ARMAS_NA_LOJA; i++) {
        b->estoqueArmas[i] = RogueSortearArma(b, naLoja);
        naLoja |= 1u << b->estoqueArmas[i];
    }
    b->armasOferecidas = 0;
    b->ofertaItem = GetRandomValue(0, NUM_ITENS_LOJA - 1);      // a oferta muda a cada volta ao átrio
}

// ---------- Reformulação: geração do mapa da tentativa ----------

static NoMapa *RogueNo(Biblioteca *b, int prof, int coluna)
{
    return &b->mapa.no[b->mapa.primeiro[prof] + coluna];
}

static void RogueLigar(NoMapa *n, int destino)
{
    for (int k = 0; k < n->numSaidas; k++)
        if (n->saida[k] == destino) return;
    if (n->numSaidas < MAX_SAIDAS)
        n->saida[n->numSaidas++] = destino;
}

// Tipo sorteado de uma sala intermediária, pelos pesos da camada
static TipoSalaRogue RogueSortearTipoSala(int prof, unsigned int jaNaCamada)
{
    //                              COMBATE ELITE EDUC  EVENTO DESC  DESAFIO TESOURO LAB
    static const int pesos[MAPA_PROFUNDIDADES][8] = {
        {  1,  0,  0,  0,  0,  0,  0,  0 },     // 0
        { 40,  0, 30, 20,  0, 12, 14, 22 },     // 1
        { 40, 25, 20, 20,  0, 15, 10, 22 },     // 2
        {  0,  0,  0,  0,  0,  0,  0,  0 },     // 3 (Sentinela)
        {  0,  0,  0,  0,  0,  0,  0,  0 },     // 4 (Câmara)
        { 35, 25, 15, 20, 10, 15, 10, 22 },     // 5
        {  0,  0,  0,  0,  0,  0,  0,  0 },     // 6 (Retropropagador)
        { 40, 30, 10, 20, 10, 15, 10, 20 },     // 7
        {  0,  0,  0,  0,  0,  0,  0,  0 },     // 8 (Janela de Contexto)
        { 30, 15,  0, 20, 30,  0, 25, 10 },     // 9 (recuperação antes do chefe)
        {  0,  0,  0,  0,  0,  0,  0,  0 },     // 10 (chefe)
    };
    static const TipoSalaRogue tipos[8] = { SALA_COMBATE, SALA_ELITE, SALA_EDUCATIVA, SALA_EVENTO, SALA_DESCANSO, SALA_DESAFIO, SALA_RECOMPENSA,
                                            SALA_LABORATORIO };

    int total = 0, peso[8];
    for (int i = 0; i < 8; i++) {
        peso[i] = pesos[prof][i];
        if (jaNaCamada & (1u << tipos[i])) peso[i] = 0;         // camadas sem tipos repetidos (escolha de verdade)
        total += peso[i];
    }

    if (total <= 0)
        return SALA_COMBATE;

    int r = GetRandomValue(1, total);
    for (int i = 0; i < 8; i++) {
        r -= peso[i];
        if (r <= 0) return tipos[i];
    }
    return SALA_COMBATE;
}

// Encontro especial de uma camada (-1 = nenhum)
static int RogueEspecialDaCamada(int d)
{
    return (d == PROF_ESPECIAL_A) ? ESP_SENTINELA : ((d == PROF_ESPECIAL_B) ? ESP_RETRO : ((d == PROF_MINICHEFE) ? ESP_JANELA : -1));
}

// Camada em que o jogador escolhe o caminho (nem a 1ª, nem a Câmara, nem um encontro especial, nem o chefe)
static bool RogueCamadaComEscolha(int d)
{
    return d > 0 && d < PROF_CHEFE && d != PROF_SILOGISTA && RogueEspecialDaCamada(d) < 0;
}

// Uma camada com escolha entre d1 e d2, sorteada (-1 = nenhuma)
static int RogueSortearCamadaComEscolha(int d1, int d2)
{
    int cand[MAPA_PROFUNDIDADES], n = 0;
    for (int d = d1; d <= d2 && d < MAPA_PROFUNDIDADES; d++)
        if (RogueCamadaComEscolha(d)) cand[n++] = d;
    return (n > 0) ? cand[GetRandomValue(0, n - 1)] : -1;
}

// Categoria de um laboratório pela camada (os básicos cedo, os especiais mais fundo), sem repetir na tentativa
static int RogueSortearLab(int prof, unsigned int usados)
{
    //                                                PROMPTS ARQUIVO NÚCLEO FIREWALL PESQUISA
    static const int pesos[MAPA_PROFUNDIDADES][NUM_LABS] = {
        {  0,  0,  0,  0,  0 },     // 0
        { 40, 30, 30,  0,  0 },     // 1
        { 30, 25, 30, 15,  0 },     // 2
        {  0,  0,  0,  0,  0 },     // 3 (Sentinela)
        {  0,  0,  0,  0,  0 },     // 4 (Câmara)
        { 15, 15, 30, 30, 20 },     // 5
        {  0,  0,  0,  0,  0 },     // 6 (Retropropagador)
        { 10, 10, 30, 35, 25 },     // 7
        {  0,  0,  0,  0,  0 },     // 8 (Janela)
        { 10, 10, 30, 35, 25 },     // 9
        {  0,  0,  0,  0,  0 },     // 10 (chefe)
    };

    int total = 0, peso[NUM_LABS];
    for (int l = 0; l < NUM_LABS; l++) {
        peso[l] = (usados & (1u << l)) ? 0 : pesos[prof][l];
        total += peso[l];
    }
    if (total <= 0) {
        for (int l = 0; l < NUM_LABS; l++) if (!(usados & (1u << l))) return l;
        return GetRandomValue(0, NUM_LABS - 1);
    }

    int r = GetRandomValue(1, total);
    for (int l = 0; l < NUM_LABS; l++) {
        r -= peso[l];
        if (r <= 0) return l;
    }
    return LAB_PROMPTS;
}

static TipoRecompensa RogueSortearRecompensaCombate(int prof, TipoRecompensa evitar, int tentativasFeitas)
{
    for (int t = 0; t < 8; t++) {
        int r = GetRandomValue(1, 100);
        TipoRecompensa rec = (r <= 50) ? RECOMPENSA_MELHORIA : ((r <= 70) ? RECOMPENSA_FRAGMENTOS : ((r <= 85) ? RECOMPENSA_ARMA : RECOMPENSA_VIDA));
        if (rec == RECOMPENSA_VIDA && prof < 2) rec = RECOMPENSA_MELHORIA;
        if (rec == RECOMPENSA_ARMA && tentativasFeitas < MISSOES_PARA_ARMAS - 1) rec = RECOMPENSA_MELHORIA;   // armas: liberadas aos poucos
        if (rec != evitar || t == 7) return rec;
    }
    return RECOMPENSA_MELHORIA;
}

// Gera o grafo da tentativa (ver o comentário de MAPA PROCEDURAL RAMIFICADO)
static void RogueGerarMapa(Biblioteca *b)
{
    MapaRogue *m = &b->mapa;
    memset(m, 0, sizeof(*m));

    // Na versão bullet-hell a camada da Câmara tem 2 salas: a Câmara (opcional) e um caminho que a evita
    // Expansão da progressão: camadas de largura 1 para os encontros especiais (ninguém passa sem eles)
    int larg[MAPA_PROFUNDIDADES] = { 1, GetRandomValue(2, 3), GetRandomValue(2, 3), 1, SILOGISTA_BH ? 2 : 1,
                                     GetRandomValue(2, 3), 1, GetRandomValue(2, 3), 1, 2, 1 };
    int colunaSilogista = GetRandomValue(0, larg[PROF_SILOGISTA] - 1);

    // Nós
    for (int d = 0; d < MAPA_PROFUNDIDADES; d++) {
        m->primeiro[d] = m->numNos;
        m->largura[d] = larg[d];
        for (int c = 0; c < larg[d]; c++) {
            NoMapa *n = &m->no[m->numNos++];
            n->prof = d;
            n->coluna = c;
            n->x = ((float)c + 0.5f) / (float)larg[d];
            n->secreta = -1;
            n->pai = -1;
        }
    }

    // Ligações: cada nó vai para o vizinho mais alinhado e para um ao lado.
    // Com isso toda sala da camada seguinte recebe ligação (conferido logo abaixo).
    for (int d = 0; d < PROF_CHEFE; d++) {
        int a = larg[d], c2 = larg[d + 1];

        for (int i = 0; i < a; i++) {
            NoMapa *n = RogueNo(b, d, i);

            if (c2 == 1) { RogueLigar(n, m->primeiro[d + 1]); continue; }

            if (a == 1) {
                // Origem única (1ª sala e Câmara): liga em todas as salas da camada seguinte
                for (int j = 0; j < c2; j++)
                    RogueLigar(n, m->primeiro[d + 1] + j);
                continue;
            }

            int j0 = (int)roundf((float)i * (float)(c2 - 1) / (float)(a - 1));
            int j1 = j0 + ((GetRandomValue(0, 1) == 0) ? -1 : 1);
            if (j1 < 0 || j1 >= c2) j1 = j0 + ((j1 < 0) ? 1 : -1);
            RogueLigar(n, m->primeiro[d + 1] + j0);
            if (j1 >= 0 && j1 < c2) RogueLigar(n, m->primeiro[d + 1] + j1);
        }

        // Garantia: nenhum nó da camada seguinte fica sem entrada (não há caminho perdido)
        for (int j = 0; j < c2; j++) {
            int alvo = m->primeiro[d + 1] + j;
            bool temEntrada = false;
            for (int i = 0; i < a && !temEntrada; i++) {
                const NoMapa *n = RogueNo(b, d, i);
                for (int k = 0; k < n->numSaidas; k++) if (n->saida[k] == alvo) temEntrada = true;
            }
            if (temEntrada) continue;

            // O nó mais próximo no mapa troca a 2ª saída por esta
            int melhor = 0;
            for (int i = 1; i < a; i++)
                if (fabsf(RogueNo(b, d, i)->x - m->no[alvo].x) < fabsf(RogueNo(b, d, melhor)->x - m->no[alvo].x)) melhor = i;
            NoMapa *n = RogueNo(b, d, melhor);
            if (n->numSaidas < MAX_SAIDAS) n->saida[n->numSaidas++] = alvo;
            else                           n->saida[MAX_SAIDAS - 1] = alvo;
        }
    }

    // Tipos das salas
    for (int d = 0; d < MAPA_PROFUNDIDADES; d++) {
        unsigned int naCamada = 0;
        TipoRecompensa ultimaRec = RECOMPENSA_NENHUMA;

        for (int c = 0; c < larg[d]; c++) {
            NoMapa *n = RogueNo(b, d, c);

            if (d == 0)                   n->tipo = SALA_COMBATE;
            else if (d == PROF_SILOGISTA) n->tipo = (!SILOGISTA_BH || c == colunaSilogista) ? SALA_SILOGISTA
                                                   : ((GetRandomValue(1, 100) <= 60) ? SALA_COMBATE : ((GetRandomValue(0, 1) == 0) ? SALA_EVENTO : SALA_DESCANSO));
            else if (d == PROF_CHEFE)     n->tipo = SALA_CHEFE;
            else if (RogueEspecialDaCamada(d) >= 0) { n->tipo = SALA_ESPECIAL; n->especial = RogueEspecialDaCamada(d); }
            else                          n->tipo = RogueSortearTipoSala(d, naCamada);
            naCamada |= 1u << n->tipo;
        }

        // Camadas com escolha: sempre há pelo menos um caminho com luta (mais recompensa)
        if (RogueCamadaComEscolha(d) &&
            !(naCamada & ((1u << SALA_COMBATE) | (1u << SALA_ELITE))))
            RogueNo(b, d, GetRandomValue(0, larg[d] - 1))->tipo = SALA_COMBATE;

        for (int c = 0; c < larg[d]; c++) {
            NoMapa *n = RogueNo(b, d, c);
            if (n->tipo == SALA_COMBATE)      n->recompensa = ultimaRec = RogueSortearRecompensaCombate((int)PROF_DIFICULDADE[d], ultimaRec, b->perm->tentativas);
            else if (n->tipo == SALA_ELITE)   n->recompensa = RECOMPENSA_RARA;       // só Elite (e a sala secreta) dão rara
            else if (n->tipo == SALA_DESAFIO || n->tipo == SALA_ESPECIAL) n->recompensa = RECOMPENSA_MELHORIA;      // escolha comum + 1 reroll grátis
            else if (n->tipo == SALA_SILOGISTA && SILOGISTA_BH)       n->recompensa = RECOMPENSA_SELO;
            if (d == 0)                       n->recompensa = RECOMPENSA_MELHORIA;    // começa sempre com uma melhoria
        }
    }

    // O conteúdo educativo continua presente: uma sala educativa antes e uma depois da Câmara
    for (int metade = 0; metade < 2; metade++) {
        int d1 = (metade == 0) ? 1 : PROF_SILOGISTA + 1, d2 = (metade == 0) ? PROF_SILOGISTA - 1 : PROF_MINICHEFE - 1;
        bool tem = false;
        for (int d = d1; d <= d2; d++)
            for (int c = 0; c < larg[d]; c++) if (RogueNo(b, d, c)->tipo == SALA_EDUCATIVA) tem = true;
        int d = RogueSortearCamadaComEscolha(d1, d2);
        if (!tem && d >= 0) {
            // Troca uma sala que não é de luta (ou, se não houver, a 2ª de combate da camada)
            for (int c = 0; c < larg[d]; c++) {
                NoMapa *n = RogueNo(b, d, c);
                bool luta = (n->tipo == SALA_COMBATE || n->tipo == SALA_ELITE);
                int lutas = 0;
                for (int k = 0; k < larg[d]; k++) { TipoSalaRogue t = RogueNo(b, d, k)->tipo; if (t == SALA_COMBATE || t == SALA_ELITE) lutas++; }
                if (!luta || lutas > 1) { n->tipo = SALA_EDUCATIVA; n->recompensa = RECOMPENSA_NENHUMA; break; }
            }
        }
    }

    // Expansão educativa: um laboratório antes da Câmara e um depois (sem tirar o único caminho
    // com luta nem a sala educativa da camada)
    for (int metade = 0; metade < 2; metade++) {
        int d1 = (metade == 0) ? 1 : PROF_SILOGISTA + 1, d2 = (metade == 0) ? PROF_SILOGISTA - 1 : PROF_CHEFE - 1;
        bool tem = false;
        for (int d = d1; d <= d2; d++)
            for (int c = 0; c < larg[d]; c++) if (RogueNo(b, d, c)->tipo == SALA_LABORATORIO) tem = true;

        for (int tentativa = 0; tentativa < 8 && !tem; tentativa++) {
            int d = RogueSortearCamadaComEscolha(d1, d2);
            if (d < 0) break;
            int lutas = 0;
            for (int k = 0; k < larg[d]; k++) { TipoSalaRogue t = RogueNo(b, d, k)->tipo; if (t == SALA_COMBATE || t == SALA_ELITE) lutas++; }
            int c0 = GetRandomValue(0, larg[d] - 1);
            for (int k = 0; k < larg[d] && !tem; k++) {
                NoMapa *n = RogueNo(b, d, (c0 + k) % larg[d]);
                bool luta = (n->tipo == SALA_COMBATE || n->tipo == SALA_ELITE);
                if (n->tipo == SALA_EDUCATIVA || (luta && lutas <= 1)) continue;
                n->tipo = SALA_LABORATORIO;
                tem = true;
            }
        }
    }

    // A categoria de cada laboratório. Na primeira tentativa da sessão, o Modo Prompt vem primeiro.
    {
        unsigned int usados = 0;
        bool promptPrimeiro = !(b->labIntroVista & (1u << LAB_PROMPTS));
        for (int i = 0; i < m->numNos; i++) {
            NoMapa *n = &m->no[i];
            if (n->tipo != SALA_LABORATORIO) continue;
            int lab = (promptPrimeiro && !(usados & (1u << LAB_PROMPTS))) ? LAB_PROMPTS : RogueSortearLab(n->prof, usados);
            n->lab = lab;
            usados |= 1u << lab;
            n->recompensa = LAB_ESPECIAL[lab] ? RECOMPENSA_RARA : RECOMPENSA_FRAGMENTOS;
        }
    }

    // Salas secretas: escondidas em salas de luta das camadas com escolha (menos as duas antes do chefe)
    int secretas = GetRandomValue(1, MAX_SECRETAS);
    for (int s = 0; s < secretas && m->numNos < MAPA_MAX_NOS; s++) {
        int candidatos[MAPA_MAX_NOS], n = 0;
        for (int i = 0; i < m->numNos; i++) {
            const NoMapa *no = &m->no[i];
            if (no->secreta >= 0 || no->pai >= 0) continue;
            if (!RogueCamadaComEscolha(no->prof) || no->prof >= PROF_CHEFE - 2) continue;
            if (no->tipo != SALA_COMBATE && no->tipo != SALA_ELITE) continue;
            candidatos[n++] = i;
        }
        if (n == 0) break;

        int host = candidatos[GetRandomValue(0, n - 1)];
        NoMapa *sec = &m->no[m->numNos];
        *sec = m->no[host];                     // mesmas saídas e a mesma camada
        sec->tipo = SALA_SECRETA;
        sec->recompensa = RECOMPENSA_RARA;
        sec->escondida = true;
        sec->visitado = false;
        sec->secreta = -1;
        sec->pai = host;
        m->no[host].secreta = m->numNos;
        m->numNos++;
    }
}

// Entra num nó do mapa por um lado
static void RogueEntrarNo(Biblioteca *b, int no, LadoPorta ladoEntrada)
{
    NoMapa *n = &b->mapa.no[no];

    b->noAtual = no;
    b->profundidade = (int)(PROF_DIFICULDADE[n->prof] + 0.5f);      // escala antiga (0 a 7)
    b->ladoEntrada = ladoEntrada;
    b->indiceRota++;
    b->perm->salasRun++;
    n->visitado = true;
    n->escondida = false;

    // A dificuldade cresce com a profundidade (os chefes já têm o próprio equilíbrio)
    bool chefe = (n->tipo == SALA_CHEFE || n->tipo == SALA_SILOGISTA || (n->tipo == SALA_ESPECIAL && n->especial == ESP_JANELA));
    float eq = PROF_DIFICULDADE[n->prof];
    eq *= 1.0f + DIFICULDADE_CURVA * eq;       // curva de pressão: cresce mais rápido nas camadas fundas (não é linear)
    b->fatorVidaInimigos  = chefe ? 1.0f : 1.0f + DIFICULDADE_VIDA_PROF * eq;
    b->fatorRitmoInimigos = chefe ? 1.0f : 1.0f + DIFICULDADE_RITMO_PROF * eq;
    b->fatorDanoSala      = chefe ? 1.0f : 1.0f + DIFICULDADE_DANO_PROF * eq;

    // PRIMEIRA MISSÃO: as duas primeiras salas são mais leves (menos vida, ritmo e dano dos inimigos)
    if (b->perm->tentativas <= 1 && !chefe && b->indiceRota <= 1) {
        b->fatorVidaInimigos  *= PRIMEIRA_MISSAO_VIDA;
        b->fatorRitmoInimigos *= PRIMEIRA_MISSAO_RITMO;
        b->fatorDanoSala      *= PRIMEIRA_MISSAO_DANO;
    }
    b->tempoSalaAtual = 0.0f;

    Telemetria("rogue_sala", NOMES_SALAS_ROGUE[n->tipo], (float)n->prof, b->indiceRota);
    RogueEntrarSala(b, n->tipo);
}

// Sorteia o mapa da tentativa e entra na primeira sala (sempre de combate)
void RogueComecarMissao(Biblioteca *b)
{
    RogueGerarMapa(b);

    b->indiceRota = -1;
    b->combates = 0;
    b->seladosTentativa = 0;
    b->ultimoEncontro = -1;
    b->j.salasConcluidas = 0;
    b->armasOferecidas = 0;
    RogueLimparEfeitos(&b->j);
    RogueLimparBuild(&b->j);
    RogueSincronizarPerm(b);
    b->j.rerolls += PermNivel(b->perm, MELHORIA_REROLL);          // Bolso Extra
    b->educativas = 0;
    b->abatidos = 0;
    b->elitesDerrotados = 0;
    b->eventosVistos = 0;
    b->j.hp = RogueHpMax(b);
    b->j.ep = RogueEpMax(b);
    b->desafiosUsados = 0;
    b->desafiosRespondidos = 0;
    b->cargaLuci = 0;
    b->falaPendente = NULL;
    b->inicio = GetTime();
    if (b->perm->tentativas == 0)                                   // medida: quanto tempo no átrio antes da 1ª missão
        Telemetria("atrio_primeira_visita", "", (float)(GetTime() - b->entradaAtrio), b->perm->bytecoVisto ? 1 : 0);
    b->perm->tentativas++;

    Telemetria("rogue_missao", "inicio", 0.0f, b->perm->tentativas);
    RogueEntrarNo(b, 0, LADO_BAIXO);
}

// Atravessou a porta k da sala atual
void RogueAtravessarPorta(Biblioteca *b, int k)
{
    if (k < 0 || k >= b->numPortas)
        return;
    RogueEntrarNo(b, b->portas[k].destino, RogueLadoOposto(b->portas[k].lado));
}

// Atravessou a passagem secreta (parede de cima = entra por baixo; parede lateral = entra pelo lado oposto)
void RogueEntrarSecreta(Biblioteca *b)
{
    int s = (b->noAtual >= 0) ? b->mapa.no[b->noAtual].secreta : -1;
    if (s < 0)
        return;

    LadoPorta lado = (b->paredeSecreta <= 1) ? LADO_BAIXO : ((b->paredeSecreta == 2) ? LADO_DIREITA : LADO_ESQUERDA);
    RogueEntrarNo(b, s, lado);
}

// Primeira vez na Biblioteca nesta sessão (ou recomeço sem nada na mochila)
void RogueIniciar(Biblioteca *b, ProgressoBiblioteca *perm)
{
    memset(b, 0, sizeof(*b));

    b->perm = perm;
    RogueSincronizarPerm(b);
    b->estabilidadeLocal = ESTABILIDADE_MAX_LUCI;
    b->estabilidade = &b->estabilidadeLocal;
    b->proximaContradicao = CONTRADICAO_INTERVALO_MIN;
    b->j.mira = (Vector2){ 1, 0 };
    b->j.ultimoUp = -1;
    b->noAtual = -1;
    b->inicio = GetTime();

    RogueEntrarAtrio(b, true);
}
