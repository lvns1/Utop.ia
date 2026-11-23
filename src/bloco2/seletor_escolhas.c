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
// REFORMULAÇÃO DO BLOCO 2: ATUALIZAÇÃO DOS SISTEMAS NOVOS
//   RogueAtualizarObjetivo   - ondas, reforços, núcleos, alvo prioritário, mecanismo
//   RogueAtualizarArmadilhas - feixes, zonas, torres, explosões, barreiras, bloqueios
//   RogueAtualizarAreas      - poças e explosões da build do jogador
//   RogueAtualizarEscolha    - a tela "escolha 1 de 3"
//   RogueAtualizarEvento     - eventos, sala de leitura e console de desafio
//   RogueAtualizarDesafioSala- as provas das salas de desafio
// =============================================================

// Inimigos que contam para ondas e reforços (sem ecos, spam, cópias, núcleos e o retransmissor)
static int RogueInimigosRelevantes(const Biblioteca *b)
{
    int n = 0;
    for (int i = 0; i < MAX_INIMIGOS; i++) {
        const Inimigo *e = &b->ini[i];
        if (e->ativo && !RogueInimigoPassageiro(e->tipo) && e->tipo != INIMIGO_NUCLEO && !e->prioritario)
            n++;
    }
    return n;
}

int RogueContarTipo(const Biblioteca *b, TipoInimigo t)
{
    int n = 0;
    for (int i = 0; i < MAX_INIMIGOS; i++)
        if (b->ini[i].ativo && b->ini[i].tipo == t) n++;
    return n;
}

// O objetivo foi cumprido de outro jeito: os ecos que sobraram se desfazem (sem pontos)
static void RogueDissolverInimigos(Biblioteca *b)
{
    for (int i = 0; i < MAX_INIMIGOS; i++) {
        Inimigo *e = &b->ini[i];
        if (!e->ativo) continue;
        RogueParticulas(b, e->pos, DEF_INIMIGOS[e->tipo].cor, 10, 160);
        e->ativo = false;
    }

    for (int i = 0; i < MAX_PROJETEIS; i++)
        if (b->proj[i].ativo && !b->proj[i].doJogador) b->proj[i].ativo = false;
}

// Reforços do encontro atual (perto de um ponto, ou num ponto da sala)
static void RogueReforcar(Biblioteca *b, int quantidade, Vector2 perto, bool usarPerto)
{
    if (b->encontro < 0)
        return;

    const EncontroDef *enc = &ENCONTROS[b->encontro];
    const LayoutSala *l = layoutCombateAtual;

    for (int q = 0; q < quantidade; q++) {
        if (RogueInimigosRelevantes(b) >= REFORCO_LIMITE_VIVOS)
            return;

        TipoInimigo t = enc->grupo[GetRandomValue(0, enc->numGrupos - 1)].tipo;
        Vector2 base = usarPerto ? (Vector2){ perto.x + (float)GetRandomValue(-50, 50), perto.y + (float)GetRandomValue(-50, 50) }
                                 : ((l->numPontos > 0) ? l->pontos[GetRandomValue(0, l->numPontos - 1)] : (Vector2){ 400, 200 });
        RogueCriarInimigo(b, t, RogueLugarSurgir(b, base, DEF_INIMIGOS[t].raio));
    }
}

// Sala de combate vencida: abre as portas, deixa a recompensa anunciada na porta e conta pontos
// =============================================================
// SELETOR DE CHEFES: CONFRONTO DIRETO E RETORNO AO LOBBY
// =============================================================

// Chefes já vencidos pelo seletor nesta sessão do jogo (a recompensa sai só na 1ª vitória).
// Fica fora da Biblioteca porque RogueIniciar a zera a cada volta do corredor, e fora do save
// para não mudar o formato dele.
static unsigned int chefesVencidosSeletor = 0;

const char *RogueDescricaoChefeSeletor(int chefe)
{
    if (chefe == CHEFE_SEL_SILOGISTA)
        return SILOGISTA_BH ? "Guardião da Câmara da Inferência. Acerte só as conclusões válidas (SHIFT = foco)."
                            : "Guardião da Câmara da Inferência. Monte o silogismo, ache o sofisma e ative o circuito.";
    return DESCRICOES_CHEFES_SELETOR[chefe];
}

bool RogueChefeSeletorVencido(const Biblioteca *b, int chefe)
{
    if (chefesVencidosSeletor & (1u << chefe))                     return true;
    if (chefe == CHEFE_SEL_SILOGISTA && b->perm->silogistaVencido)  return true;
    if (chefe == CHEFE_SEL_ALUCINADOR && b->perm->alucinadorVencido) return true;
    return false;
}

// A recompensa do chefe só sai se ele nunca foi vencido (flags permanentes do Silogista e do
// Alucinador; para os outros três, a marca desta sessão)
static bool RogueSeletorTemRecompensa(const Biblioteca *b, int chefe)
{
    return !RogueChefeSeletorVencido(b, chefe);
}

void RogueAbrirSeletor(Biblioteca *b)
{
    b->seletorAberto = true;
    b->seletorAnim = 0.0f;
    if (b->seletorFoco < 0 || b->seletorFoco >= NUM_CHEFES_SELETOR) b->seletorFoco = 0;
    TocarSom(sfxSelecao);
    Telemetria("seletor_aberto", "", 0.0f, b->perm->tentativas);
}

static void RogueFecharSeletor(Biblioteca *b)
{
    b->seletorAberto = false;
    TocarSomTom(sfxSelecao, 0.8f);
    Telemetria("seletor_fechado", "", b->seletorAnim, b->seletorFoco);
}

// Volta ao átrio depois de um confronto do seletor: some com o que veio do desafio e devolve
// o que o jogador tinha na mochila antes (o que comprou no Byteco não se perde)
void RogueVoltarAoLobby(Biblioteca *b)
{
    RogueEsvaziarMochila(&b->j);
    if (b->temMochilaGuardada) {
        memcpy(b->j.itens, b->itensGuardados, sizeof(b->j.itens));
        b->j.armaExtra = b->armaGuardada;
        b->j.armaBase = b->armaBaseGuardada;
        b->j.usandoExtra = b->usandoExtraGuardado;
        b->j.seloInferencia = b->seloGuardado;
        b->j.seloAmplificado = b->seloAmplificadoGuardado;
        b->temMochilaGuardada = false;
    }
    b->modoSeletor = false;
    b->seletorVencido = false;
    b->seletorRetorno = 0.0f;
    b->chefeSeletor = -1;
    RogueEntrarAtrio(b, false);
}

// Dá para abandonar a run? Só dentro de uma sala da missão do Bloco 2 (nem no átrio, nem no Bloco 3,
// nem na tela de derrota ou de resultado)
bool RoguePodeAbandonar(const Biblioteca *b)
{
    return RogueSalaDaMissao(b) && !b->morreu && !b->resultadoAberto && b->transicaoNucleo <= 0.0f;
}

// O jogador desistiu da run pelo menu de pause: volta ao átrio como depois de uma derrota
// (a mochila da tentativa se perde), mas sem contar morte nem tirar pontos
void RogueAbandonarRun(Biblioteca *b)
{
    Telemetria("rogue_abandono", TextFormat("sala %d", b->indiceRota + 1), (float)(GetTime() - b->inicio), b->abatidos);

    b->morreu = false;
    b->tempoMorte = 0.0f;
    b->desafioAberto = false;
    b->pausaImpacto = 0.0f;
    b->falaPendente = NULL;
    b->conversaPendente = NULL;
    RogueLimparBalasInimigas(b);

    if (b->modoSeletor) {
        RogueVoltarAoLobby(b);                 // confronto do seletor: devolve a mochila guardada
    } else {
        RogueEsvaziarMochila(&b->j);
        RogueEntrarAtrio(b, false);
    }

    RogueAviso(b, "Run abandonada. De volta ao lobby.");
}

// O confronto do seletor terminou com vitória (o retorno espera os diálogos de vitória)
void RogueSeletorVenceu(Biblioteca *b, ProgressoCorredor *prog, int pontos)
{
    if (!b->modoSeletor || b->seletorVencido)
        return;

    b->seletorVencido = true;
    b->seletorRetorno = 0.0f;
    RogueLimparBalasInimigas(b);
    if (b->recompensaSeletor && pontos > 0)
        AlterarPontos(prog, pontos);
    if (b->chefeSeletor >= 0 && b->chefeSeletor < NUM_CHEFES_SELETOR)
        chefesVencidosSeletor |= 1u << b->chefeSeletor;
    RogueAviso(b, b->recompensaSeletor ? "CONFRONTO VENCIDO! Voltando ao lobby..." : "VENCIDO! (treino: a recompensa já foi recebida)");
    TocarSom(sfxAcerto);
    Telemetria("seletor_vitoria", (b->chefeSeletor >= 0) ? NOMES_CHEFES_SELETOR[b->chefeSeletor] : "",
               (float)(GetTime() - b->inicio), b->recompensaSeletor ? 1 : 0);
}

// Começa um confronto escolhido no seletor: sem mapa procedural, com o personagem padrão
static void RogueIniciarConfronto(Biblioteca *b, int chefe)
{
    static const int camada[NUM_CHEFES_SELETOR] = { PROF_ESPECIAL_A, PROF_ESPECIAL_B, PROF_MINICHEFE, PROF_SILOGISTA, PROF_CHEFE };
    static const int especial[NUM_CHEFES_SELETOR] = { ESP_SENTINELA, ESP_RETRO, ESP_JANELA, -1, -1 };

    if (chefe < 0 || chefe >= NUM_CHEFES_SELETOR)
        return;

    // 1. Fecha o seletor
    b->seletorAberto = false;

    // 2. Marca o modo seletor (e se esta vitória ainda vale recompensa)
    b->modoSeletor = true;
    b->chefeSeletor = chefe;
    b->seletorVencido = false;
    b->seletorRetorno = 0.0f;
    b->recompensaSeletor = RogueSeletorTemRecompensa(b, chefe);
    b->especialDireto = especial[chefe];

    // 4. Personagem padrão: guarda a mochila do átrio e começa só com o Disparador
    memcpy(b->itensGuardados, b->j.itens, sizeof(b->itensGuardados));
    b->armaGuardada = b->j.armaExtra;
    b->armaBaseGuardada = b->j.armaBase;
    b->usandoExtraGuardado = b->j.usandoExtra;
    b->seloGuardado = b->j.seloInferencia;
    b->seloAmplificadoGuardado = b->j.seloAmplificado;
    b->temMochilaGuardada = true;
    RogueEsvaziarMochila(&b->j);                // itens, armas extras, efeitos, build e selos
    b->j.ultimoUp = -1;

    // Fora do mapa: nenhum nó, nenhuma porta. A dificuldade é a da camada em que o chefe fica.
    b->noAtual = -1;
    b->indiceRota = -1;
    b->numPortas = 0;
    b->ladoEntrada = LADO_BAIXO;
    b->mapaAberto = false;
    b->combates = 0;
    b->falaPendente = NULL;
    b->conversaPendente = NULL;
    b->inicio = GetTime();
    float eq = PROF_DIFICULDADE[camada[chefe]];
    bool equilibrioProprio = (chefe >= CHEFE_SEL_JANELA);       // mini-chefe e chefes não escalam (igual à missão)
    b->profundidade = (int)(eq + 0.5f);
    b->fatorVidaInimigos  = equilibrioProprio ? 1.0f : 1.0f + DIFICULDADE_VIDA_PROF * eq;
    b->fatorRitmoInimigos = equilibrioProprio ? 1.0f : 1.0f + DIFICULDADE_RITMO_PROF * eq;
    b->fatorDanoSala      = equilibrioProprio ? 1.0f : 1.0f + DIFICULDADE_DANO_PROF * eq;

    // 3, 5, 6 e 7. RogueEntrarSala limpa a sala anterior, põe o jogador na entrada de baixo e
    // monta o confronto com o inicializador de sempre (intro, padrões, barras e vitória originais)
    TipoSalaRogue tipo = (chefe == CHEFE_SEL_SILOGISTA) ? SALA_SILOGISTA : ((chefe == CHEFE_SEL_ALUCINADOR) ? SALA_CHEFE : SALA_ESPECIAL);
    RogueEntrarSala(b, tipo);
    b->numPortas = 0;
    b->j.hp = RogueHpMax(b);
    b->j.ep = RogueEpMax(b);
    b->j.direcao = DIR_COSTAS;

    Telemetria("seletor_confronto", NOMES_CHEFES_SELETOR[chefe], 0.0f, b->recompensaSeletor ? 1 : 0);
}

// Cards do seletor: 5 lado a lado (o do foco sobe um pouco)
Rectangle RogueRetCardSeletor(int i, int foco)
{
    float w = 140.0f, gap = 10.0f;
    float total = (float)NUM_CHEFES_SELETOR * w + (float)(NUM_CHEFES_SELETOR - 1) * gap;
    float x = 400.0f - total / 2.0f + (float)i * (w + gap);
    return (i == foco) ? (Rectangle){ x, 124, w, 312 } : (Rectangle){ x, 134, w, 300 };
}

Rectangle RogueRetFecharSeletor(void)
{
    return (Rectangle){ 290, 450, 220, 30 };
}

// O seletor aberto: o resto do átrio espera (A/D ou setas, ENTER ou clique, ESC ou o botão)
void RogueAtualizarSeletor(Biblioteca *b)
{
    b->seletorAnim += DtJogo();

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
        RogueFecharSeletor(b);
        return;
    }

    if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))  { b->seletorFoco = (b->seletorFoco + NUM_CHEFES_SELETOR - 1) % NUM_CHEFES_SELETOR; TocarSom(sfxSelecao); }
    if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) { b->seletorFoco = (b->seletorFoco + 1) % NUM_CHEFES_SELETOR;                      TocarSom(sfxSelecao); }

    Vector2 m = GetMousePosition();
    bool moveu = Vector2Length(GetMouseDelta()) > 0.5f;
    bool clique = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    if (b->seletorAnim < 0.25f)                 // o E que abriu o terminal não confirma nada
        return;

    if (clique && CheckCollisionPointRec(m, RogueRetFecharSeletor())) {
        RogueFecharSeletor(b);
        return;
    }

    for (int i = 0; i < NUM_CHEFES_SELETOR; i++) {
        if (!CheckCollisionPointRec(m, RogueRetCardSeletor(i, b->seletorFoco))) continue;
        if (moveu && b->seletorFoco != i) { b->seletorFoco = i; TocarSom(sfxSelecao); }
        if (clique) { RogueIniciarConfronto(b, i); return; }
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_E))
        RogueIniciarConfronto(b, b->seletorFoco);
}

static void RogueConcluirCombate(Biblioteca *b, ProgressoCorredor *prog)
{
    // Seletor de chefes: sem recompensa no chão (a build é descartada) e de volta ao átrio
    if (b->modoSeletor) {
        b->limpa = true;
        RogueDissolverInimigos(b);
        RogueSeletorVenceu(b, prog, PONTOS_SALA_ROGUE + 12);
        return;
    }

    b->limpa = true;
    RogueSalaConcluida(b);
    RogueDissolverInimigos(b);

    // Às vezes o combate deixa também um upgrade passivo (sistema antigo, preservado)
    int upgrade = RogueSortearItem(b, 0, true);
    if (upgrade >= 0 && GetRandomValue(1, 100) <= CHANCE_UPGRADE_SALA)
        RogueSoltar(b, (Vector2){ 400, 330 }, false, (TipoItem)upgrade);

    TipoRecompensa rec = RECOMPENSA_MELHORIA;
    if (b->eventoCombate) {
        b->perm->fragmentos += 10;
        RogueAviso(b, "BANCO ESTABILIZADO! +10 FRAGMENTOS");
    } else if (b->noAtual >= 0 && b->mapa.no[b->noAtual].recompensa != RECOMPENSA_NENHUMA) {
        rec = b->mapa.no[b->noAtual].recompensa;
    }
    RogueColocarRecompensa(b, rec);
    if (b->tipoSala == SALA_ESPECIAL && !b->eventoCombate) {
        b->j.rerolls++;                         // sala especial: escolha comum, mas com um reroll grátis a mais
        RogueAviso(b, "+1 REROLL GRÁTIS");
    }

    if (b->objetivoSala != OBJC_ELIMINAR && !b->eventoCombate)
        RogueAviso(b, "OBJETIVO CUMPRIDO!");

    AlterarPontos(prog, PONTOS_SALA_ROGUE + ((b->tipoSala == SALA_ELITE) ? 8 : 0) + ((b->tipoSala == SALA_ESPECIAL) ? 12 : 0));
    TocarSom(sfxAcerto);
    MostrarEmocaoLuci(EMOCAO_ALEGRE, 2.0f);
    Telemetria("rogue_sala_limpa", TextFormat("sala %d %s", b->indiceRota + 1, NOMES_OBJC[b->objetivoSala]), (float)(GetTime() - b->inicio), b->abatidos);
}

void RogueAtualizarObjetivo(Biblioteca *b, ProgressoCorredor *prog, float dt)
{
    bool combate = (b->tipoSala == SALA_COMBATE || b->tipoSala == SALA_ELITE || b->tipoSala == SALA_ESPECIAL ||
                    (b->tipoSala == SALA_EVENTO && b->eventoCombate));
    if (!combate || b->limpa || b->desafioAberto)
        return;

    switch (b->objetivoSala) {

        case OBJC_ELIMINAR:
            // Ondas: a próxima chega (com aviso) quando sobra no máximo um inimigo
            if (b->ondaAtual + 1 < b->numOndas) {
                if (b->avisoOnda > 0.0f) {
                    b->avisoOnda -= dt;
                    if (b->avisoOnda <= 0.0f) { b->ondaAtual++; RogueSurgirOnda(b, b->ondaAtual); }
                } else if (RogueInimigosRelevantes(b) <= 1) {
                    b->avisoOnda = ONDA_AVISO;
                    RogueAviso(b, TextFormat("ONDA %d/%d CHEGANDO!", b->ondaAtual + 2, b->numOndas));
                    TocarSomTom(sfxErro, 0.7f);
                }
                return;
            }
            if (RogueInimigosVivos(b) == 0)
                RogueConcluirCombate(b, prog);
            break;

        case OBJC_SOBREVIVER:
            b->tempoObjetivo -= dt;
            b->reforco -= dt;
            if (b->reforco <= 0.0f) {
                b->reforco = REFORCO_INTERVALO;
                RogueReforcar(b, 2 + b->profundidade / 3, (Vector2){0}, false);
            }
            if (b->tempoObjetivo <= 0.0f)
                RogueConcluirCombate(b, prog);
            break;

        case OBJC_NUCLEOS: {
            if (RogueContarTipo(b, INIMIGO_NUCLEO) == 0) {
                RogueConcluirCombate(b, prog);
                break;
            }
            // Enquanto houver núcleo, ecos novos nascem perto de um deles
            b->reforco -= dt;
            if (b->reforco <= 0.0f) {
                b->reforco = REFORCO_INTERVALO + 1.0f;
                int vivos[MAX_INIMIGOS], n = 0;
                for (int i = 0; i < MAX_INIMIGOS; i++)
                    if (b->ini[i].ativo && b->ini[i].tipo == INIMIGO_NUCLEO) vivos[n++] = i;
                if (n > 0) RogueReforcar(b, 2, b->ini[vivos[GetRandomValue(0, n - 1)]].pos, true);
            }
        } break;

        case OBJC_PRIORITARIO:
            if (b->alvoPrioritario >= 0 && !b->ini[b->alvoPrioritario].ativo)
                b->alvoPrioritario = -1;

            if (b->alvoPrioritario >= 0) {
                b->reforco -= dt;
                if (b->reforco <= 0.0f) {
                    b->reforco = REFORCO_INTERVALO;
                    RogueReforcar(b, 2, b->ini[b->alvoPrioritario].pos, true);
                }
            } else if (RogueInimigosVivos(b) == 0) {
                RogueConcluirCombate(b, prog);
            }
            break;

        case OBJC_MECANISMO: {
            int restantes = 0;
            for (int k = 0; k < b->numMecanismos; k++) {
                if (b->mecanismoDesligado[k]) continue;
                restantes++;

                bool perto = Vector2Distance(b->j.pos, b->posMecanismo[k]) < MECANISMO_RAIO;
                if (perto && IsKeyDown(KEY_E) && b->j.tempoEsquiva <= 0.0f) {
                    b->progressoMecanismo[k] += dt;
                    if (GetRandomValue(0, 3) == 0) RogueParticulas(b, b->posMecanismo[k], (Color){ 0, 228, 255, 255 }, 1, 60);
                    if (b->progressoMecanismo[k] >= MECANISMO_TEMPO) {
                        b->mecanismoDesligado[k] = true;
                        restantes--;
                        RogueAviso(b, TextFormat("CONSOLE DESLIGADO (%d/%d)", b->numMecanismos - restantes, b->numMecanismos));
                        RogueParticulas(b, b->posMecanismo[k], (Color){ 0, 228, 255, 255 }, 20, 200);
                        TocarSomTom(sfxAcerto, 1.1f);
                    }
                } else {
                    b->progressoMecanismo[k] = fmaxf(0.0f, b->progressoMecanismo[k] - dt * 0.5f);
                }
            }

            if (restantes == 0) {
                RogueConcluirCombate(b, prog);
                break;
            }

            b->reforco -= dt;
            if (b->reforco <= 0.0f) {
                b->reforco = REFORCO_INTERVALO;
                RogueReforcar(b, 2, (Vector2){0}, false);
            }
        } break;

        default:
            break;
    }
}

// Barreiras e bloqueios sólidos entram no fim do vetor de estantes (colisão de todo mundo)
static void RogueAtualizarObstaculosDinamicos(Biblioteca *b)
{
    b->numObst = b->numObstFixos;

    for (int i = 0; i < MAX_ARMADILHAS && b->numObst < MAX_OBSTACULOS; i++) {
        const Armadilha *a = &b->arm[i];
        if (!a->ativo) continue;
        if (a->tipo == ARM_BARREIRA || (a->tipo == ARM_BLOQUEIO && a->fase == 2))
            b->obst[b->numObst++] = a->r;
    }
}

void RogueAtualizarArmadilhas(Biblioteca *b, ProgressoCorredor *prog, float dt)
{
    JogadorRogue *j = &b->j;

    // Sala vencida: as armadilhas desligam (a não ser durante o desafio de integridade)
    bool desligadas = b->limpa && !(b->tipoSala == SALA_DESAFIO && b->estadoDesafio == 1);

    for (int i = 0; i < MAX_ARMADILHAS; i++) {

        Armadilha *a = &b->arm[i];
        if (!a->ativo) continue;

        if (desligadas) {
            if (a->tipo != ARM_BARREIRA) a->fase = 0;
            continue;
        }

        switch (a->tipo) {

            case ARM_FEIXE:
                a->timer -= dt;
                if (a->timer <= 0.0f) {
                    a->fase = (a->fase + 1) % 3;
                    a->timer = (a->fase == 1) ? FEIXE_AVISO : ((a->fase == 2) ? FEIXE_ATIVO : FEIXE_DESCANSO);
                    if (a->fase == 2) TocarGrupo(&gSentinelaFeixe, 1.15f, 0.55f);
                    if (a->fase == 0) {
                        // O feixe muda de lugar a cada ciclo
                        float passo = (float)GetRandomValue(60, 130) * ((GetRandomValue(0, 1) == 0) ? -1.0f : 1.0f);
                        if (a->vertical) a->r.x = Clamp(a->r.x + passo, 110.0f, 680.0f);
                        else             a->r.y = Clamp(a->r.y + passo, 120.0f, 450.0f);
                    }
                }
                if (a->fase == 2 && CheckCollisionCircleRec(j->pos, RAIO_CORPO_JOGADOR, a->r))
                    RogueFerirJogador(b, FEIXE_DANO);
                break;

            case ARM_ZONA:
                a->timer -= dt;
                if (a->timer <= 0.0f) {
                    a->fase = (a->fase + 1) % 3;
                    a->timer = (a->fase == 1) ? 0.8f : ((a->fase == 2) ? ZONA_LIGADA : ZONA_DESLIGADA);
                }
                if (a->fase == 2) {
                    a->tick -= dt;
                    if (a->tick <= 0.0f && Vector2Distance(j->pos, a->pos) < a->raio) {
                        a->tick = ZONA_TICK;
                        RogueFerirJogador(b, ZONA_DANO);
                    }
                }
                break;

            case ARM_TORRE:
                a->timer -= dt;
                if (a->fase == 0 && a->timer <= 0.0f) { a->fase = 1; a->timer = TORRE_PREPARO; }
                else if (a->fase == 1 && a->timer <= 0.0f) {
                    Vector2 d = Vector2Subtract(j->pos, a->pos);
                    d = (Vector2Length(d) > 1.0f) ? Vector2Normalize(d) : (Vector2){ 0, 1 };
                    RogueDisparar(b, a->pos, d, TORRE_TIRO_VEL, TORRE_TIRO_DANO, 6.0f, false);
                    TocarGrupo(&gIniPacote, 0.95f, 0.6f);
                    a->fase = 0;
                    a->timer = TORRE_INTERVALO;
                }
                break;

            case ARM_EXPLOSAO:
                a->timer -= dt;
                if (a->fase == 0 && a->timer <= 0.0f) {
                    // Marca o chão perto do jogador (dá tempo de sair)
                    a->fase = 1;
                    a->timer = EXPLOSAO_AVISO;
                    a->pos = (Vector2){ Clamp(j->pos.x + (float)GetRandomValue(-40, 40), ArenaRogue().x + 30, ArenaRogue().x + ArenaRogue().width - 30),
                                        Clamp(j->pos.y + (float)GetRandomValue(-40, 40), ArenaRogue().y + 30, ArenaRogue().y + ArenaRogue().height - 30) };
                }
                else if (a->fase == 1 && a->timer <= 0.0f) {
                    // Explode e fere todo mundo por perto (dá para atrair os inimigos)
                    if (Vector2Distance(j->pos, a->pos) < a->raio + RAIO_CORPO_JOGADOR * 0.5f)
                        RogueFerirJogador(b, EXPLOSAO_DANO);
                    for (int k = 0; k < MAX_INIMIGOS; k++) {
                        Inimigo *e = &b->ini[k];
                        if (!e->ativo || e->selado || e->surgindo > 0.0f || Vector2Distance(e->pos, a->pos) > a->raio + e->raio) continue;
                        Vector2 d = Vector2Subtract(e->pos, a->pos);
                        RogueFerirInimigo(b, prog, e, EXPLOSAO_DANO, (Vector2Length(d) > 0.1f) ? Vector2Normalize(d) : (Vector2){ 0, -1 });
                    }
                    RogueAnel(b, a->pos, a->raio, MAGENTA);
                    RogueParticulas(b, a->pos, MAGENTA, 18, 220);
                    TocarGrupo(&gIniEco, 0.9f, 0.8f);
                    a->fase = 0;
                    a->timer = EXPLOSAO_INTERVALO;
                }
                break;

            case ARM_BARREIRA:
                if (a->vertical) {
                    a->r.x += a->vel * dt;
                    if (a->r.x < a->limiteA || a->r.x > a->limiteB) { a->vel = -a->vel; a->r.x = Clamp(a->r.x, a->limiteA, a->limiteB); }
                } else {
                    a->r.y += a->vel * dt;
                    if (a->r.y < a->limiteA || a->r.y > a->limiteB) { a->vel = -a->vel; a->r.y = Clamp(a->r.y, a->limiteA, a->limiteB); }
                }
                break;

            case ARM_BLOQUEIO:
                a->timer -= dt;
                if (a->timer <= 0.0f) {
                    a->fase = (a->fase + 1) % 3;
                    a->timer = (a->fase == 1) ? BLOQUEIO_AVISO : ((a->fase == 2) ? BLOQUEIO_SOLIDO : BLOQUEIO_LIVRE);
                }
                break;

            default:
                break;
        }
    }

    RogueAtualizarObstaculosDinamicos(b);
}

// Tornado de Dados: anda devagar atrás do inimigo mais perto, puxa quem está em volta,
// fere a cada 0,2 s e apaga as balas inimigas que toca
void RogueAtualizarTornado(Biblioteca *b, ProgressoCorredor *prog, float dt)
{
    for (int r = 0; r < 3; r++)                     // os relâmpagos do Surto só somem com o tempo
        if (b->raioK[r].vida > 0.0f) b->raioK[r].vida -= dt;

    if (!b->torAtivo) return;

    b->torVida -= dt;
    if (b->torVida <= 0.0f) { b->torAtivo = false; return; }

    int alvo = RogueInimigoMaisPerto(b, b->torPos, 320.0f, -1);
    if (alvo >= 0) {
        Vector2 para = Vector2Subtract(b->ini[alvo].pos, b->torPos);
        if (Vector2Length(para) > 1.0f)
            b->torDir = Vector2Normalize(Vector2Lerp(b->torDir, Vector2Normalize(para), Clamp(3.0f * dt, 0.0f, 1.0f)));
    }
    b->torPos = Vector2Add(b->torPos, Vector2Scale(b->torDir, 110.0f * dt));
    RogueEmpurrarParaFora(&b->torPos, 18.0f, b->obst, b->numObst);

    // puxa os inimigos para dentro do redemoinho
    for (int k = 0; k < MAX_INIMIGOS; k++) {
        Inimigo *e = &b->ini[k];
        if (!e->ativo || e->selado || e->surgindo > 0.0f || Vector2Distance(e->pos, b->torPos) > 140.0f) continue;
        Vector2 para = Vector2Subtract(b->torPos, e->pos);
        if (Vector2Length(para) < 1.0f) continue;
        e->empurrao = Vector2Add(e->empurrao, Vector2Scale(Vector2Normalize(para), 520.0f * dt * DEF_INIMIGOS[e->tipo].empurrao));
    }

    // apaga as balas inimigas
    for (int i = 0; i < MAX_PROJETEIS; i++) {
        Projetil *q = &b->proj[i];
        if (q->ativo && !q->doJogador && Vector2Distance(q->pos, b->torPos) < 70.0f) {
            q->ativo = false;
            RogueParticulas(b, q->pos, (Color){ 150, 230, 255, 255 }, 2, 60);
        }
    }

    b->torTick -= dt;
    if (b->torTick <= 0.0f) {
        b->torTick = 0.2f;
        b->golpeSecundario = true;
        RogueDanoArea(b, prog, b->torPos, 60.0f, b->torDano, 0.0f);
        b->golpeSecundario = false;
    }
}

void RogueAtualizarAreas(Biblioteca *b, ProgressoCorredor *prog, float dt)
{
    for (int i = 0; i < MAX_AREAS; i++) {

        AreaDano *a = &b->areas[i];
        if (!a->ativo) continue;

        if (a->atraso > 0.0f) {
            a->atraso -= dt;
            if (a->atraso <= 0.0f && a->danoInstantaneo > 0.0f) {
                int acertados = 0;
                if (a->estilo == 1)
                    for (int k = 0; k < MAX_INIMIGOS; k++) {
                        const Inimigo *e = &b->ini[k];
                        if (e->ativo && !e->selado && e->surgindo <= 0.0f && Vector2Distance(e->pos, a->pos) <= a->raio + e->raio) acertados++;
                    }
                b->golpeSecundario = true;
                RogueDanoArea(b, prog, a->pos, a->raio, a->danoInstantaneo, (a->estilo == 1) ? 60.0f : 160.0f);
                b->golpeSecundario = false;
                if (a->estilo == 1 && acertados > 0) {      // a luz devolve energia: RAIO_EP_RETORNO por inimigo (máx. RAIO_EP_RETORNO_MAX)
                    float volta = RAIO_EP_RETORNO * (float)((acertados > RAIO_EP_RETORNO_MAX) ? RAIO_EP_RETORNO_MAX : acertados);
                    b->j.ep = fminf(RogueEpMax(b), b->j.ep + volta);
                    RogueTextoFlutuante(b, (Vector2){ b->j.pos.x, b->j.pos.y - 30 }, TextFormat("+%.0f EP", volta), (Color){ 120, 200, 255, 255 });
                }
                RogueAnel(b, a->pos, a->raio, a->cor);
                RogueParticulas(b, a->pos, a->cor, 14, 200);
                if (a->estilo == 1) TocarGrupo(&gRaioColuna, 1.0f, 1.0f);   // a coluna de luz do sigilo
                else                TocarSomTom(sfxAcerto, 0.8f);
                a->danoInstantaneo = 0.0f;
            }
            continue;
        }

        a->vida -= dt;
        if (a->vida <= 0.0f) { a->ativo = false; continue; }

        if (a->dps > 0.0f) {
            a->tick -= dt;
            if (a->tick <= 0.0f) {
                a->tick = 0.25f;
                for (int k = 0; k < MAX_INIMIGOS; k++) {
                    Inimigo *e = &b->ini[k];
                    if (!e->ativo || e->selado || e->surgindo > 0.0f || Vector2Distance(e->pos, a->pos) > a->raio + e->raio) continue;
                    b->golpeSecundario = true;
                    RogueFerirInimigo(b, prog, e, a->dps * 0.25f, (Vector2){ 0, 0 });
                    b->golpeSecundario = false;
                }
            }
        }
    }
}

// ---------- Escolha 1 de 3 ----------

static bool RogueUpgradeDisponivel(const Biblioteca *b, int u)
{
    const DefUpgrade *d = &UPGRADES[u];

    if (b->j.up[u] >= d->maxNivel)
        return false;
    if (u == UP_RAJADA && b->j.up[u] >= 1 && b->j.up[UP_PERFURAR] == 0)       // 2º nível da Amostragem Múltipla exige Atenção Multicabeça
        return false;
    for (int k = 0; k < NUM_UPGRADES; k++)
        if ((d->requer & BIT_UP(k)) && b->j.up[k] == 0)
            return false;
    if (d->arma >= 0 && d->arma != ARMA_DISPARADOR && (int)b->j.armaExtra != d->arma && (int)b->j.armaBase != d->arma)
        return false;
    return true;
}

// Peso no sorteio: raridade (raras ficam mais comuns no fundo do mapa), sinergias e o que já está na build
static int RoguePesoUpgrade(const Biblioteca *b, int u, ModoEscolha modo)
{
    const DefUpgrade *d = &UPGRADES[u];

    if (modo == ESCOLHA_ARMA && d->categoria != CAT_APRIMORAMENTO && d->categoria != CAT_DISPARO)
        return 0;

    int peso = PESO_RARIDADE[d->raridade];
    if (d->raridade == RARIDADE_RARA)    peso += 4 * b->profundidade;
    if (d->raridade == RARIDADE_INCOMUM) peso += 2 * b->profundidade;
    if (modo == ESCOLHA_RARA && d->raridade == RARIDADE_COMUM) peso /= 6;
    if (d->categoria == CAT_SINERGIA)     peso *= 3;
    if (d->categoria == CAT_APRIMORAMENTO) peso += (modo == ESCOLHA_ARMA) ? 40 : 10;
    if (b->j.up[u] > 0)                    peso += 12;      // aprofundar a build também é uma escolha
    return (peso < 1) ? 1 : peso;
}

static int RogueSortearUpgrade(const Biblioteca *b, ModoEscolha modo, unsigned long long excluir, int raridade)
{
    int cand[NUM_UPGRADES], peso[NUM_UPGRADES], n = 0, total = 0;

    for (int u = 0; u < NUM_UPGRADES; u++) {
        if ((excluir & BIT_UP(u)) || !RogueUpgradeDisponivel(b, u))         continue;
        if (raridade >= 0 && (int)UPGRADES[u].raridade != raridade)        continue;
        int p = RoguePesoUpgrade(b, u, modo);
        if (p <= 0) continue;
        cand[n] = u; peso[n] = p; total += p; n++;
    }

    if (n == 0)
        return -1;

    int r = GetRandomValue(1, total);
    for (int i = 0; i < n; i++) {
        r -= peso[i];
        if (r <= 0) return cand[i];
    }
    return cand[n - 1];
}

static void RogueGerarOpcoes(Biblioteca *b, ModoEscolha modo)
{
    unsigned long long usados = b->rerollUsados;        // o que o reroll já mostrou não volta
    unsigned int itensUsados = b->rerollItens;
    b->numOpcoes = 0;

    // Arsenal: uma arma nova (duas, se ainda não houver arma extra) e aprimoramentos de disparo
    if (modo == ESCOLHA_ARMA) {
        TipoArma a = RogueSortearArma(b, b->rerollArmas);
        if (a != ARMA_DISPARADOR) b->opcoes[b->numOpcoes++] = (OpcaoEscolha){ OPC_ARMA, a };
        if (b->j.armaExtra == ARMA_DISPARADOR && a != ARMA_DISPARADOR) {
            TipoArma a2 = RogueSortearArma(b, (1u << a) | b->rerollArmas);
            if (a2 != ARMA_DISPARADOR && a2 != a) b->opcoes[b->numOpcoes++] = (OpcaoEscolha){ OPC_ARMA, a2 };
        }
    }

    // Melhoria rara: pelo menos uma das três é rara (quando ainda existe alguma)
    if (modo == ESCOLHA_RARA) {
        int u = RogueSortearUpgrade(b, modo, usados, RARIDADE_RARA);
        if (u >= 0) { b->opcoes[b->numOpcoes++] = (OpcaoEscolha){ OPC_UPGRADE, u }; usados |= BIT_UP(u); }
    }

    while (b->numOpcoes < 3) {
        int u = RogueSortearUpgrade(b, modo, usados, -1);
        if (u < 0) break;
        b->opcoes[b->numOpcoes++] = (OpcaoEscolha){ OPC_UPGRADE, u };
        usados |= BIT_UP(u);
    }

    // Sem melhorias suficientes: completa com os upgrades passivos antigos
    while (b->numOpcoes < 3) {
        int it = RogueSortearItem(b, itensUsados, true);
        if (it < 0) break;
        b->opcoes[b->numOpcoes++] = (OpcaoEscolha){ OPC_ITEM, it };
        itensUsados |= 1u << it;
    }

    if (b->numOpcoes == 0)
        b->opcoes[b->numOpcoes++] = (OpcaoEscolha){ OPC_FRAGMENTOS, FC_OPCAO_RESERVA };

    // Embaralha (a rara não fica sempre no mesmo lugar)
    for (int i = b->numOpcoes - 1; i > 0; i--) {
        int k = GetRandomValue(0, i);
        OpcaoEscolha t = b->opcoes[i]; b->opcoes[i] = b->opcoes[k]; b->opcoes[k] = t;
    }
}

void RogueAbrirEscolha(Biblioteca *b, ModoEscolha modo, bool gratis)
{
    b->rerollUsados = 0;
    b->rerollItens = 0;
    b->rerollArmas = 0;
    RogueGerarOpcoes(b, modo);
    b->escolhaPrimeiraMissao = false;
    if (b->perm->tentativas <= 1 && b->indiceRota == 0 && modo == ESCOLHA_NORMAL && !gratis) {
        // A primeira melhoria da primeira missão é sempre a mesma: 3 atributos simples, para entender a tela
        b->numOpcoes = 3;
        b->opcoes[0] = (OpcaoEscolha){ OPC_UPGRADE, UP_DANO };
        b->opcoes[1] = (OpcaoEscolha){ OPC_UPGRADE, UP_CADENCIA };
        b->opcoes[2] = (OpcaoEscolha){ OPC_UPGRADE, UP_VIDA };
        b->escolhaPrimeiraMissao = true;
        Telemetria("primeira_missao_escolha", "", 0.0f, 0);
    }
    b->escolhaAberta = true;
    b->escolhaGratis = gratis;
    b->modoEscolha = modo;
    b->selecaoEscolha = 0;
    b->animEscolha = 0.0f;
    TocarSomTom(sfxAcerto, 1.3f);
}

// Armas: no máximo DUAS (a do tutorial, o Disparador, já conta como uma). Com as duas vagas cheias,
// a arma nova abre a ESCOLHA de qual das duas substituir.
bool RogueTemArma(const JogadorRogue *j, TipoArma a)
{
    return j->armaBase == a || j->armaExtra == a;
}

void RogueInstalarArma(Biblioteca *b, TipoArma arma, int vaga)
{
    JogadorRogue *j = &b->j;
    if (vaga == 0) { j->armaBase = arma;  j->usandoExtra = false; }
    else           { j->armaExtra = arma; j->usandoExtra = true;  }
    j->trocaArma = 1.3f;
    j->carga = 0.0f;
}

// true = a arma já foi instalada (havia vaga); false = abriu a escolha de qual arma substituir
bool RogueReceberArma(Biblioteca *b, TipoArma arma, int origem, int pedestal)
{
    if (b->j.armaExtra == ARMA_DISPARADOR) { RogueInstalarArma(b, arma, 1); return true; }

    b->trocaArmaAberta = true;
    b->armaNova = arma;
    b->trocaOrigem = origem;
    b->trocaPedestal = pedestal;
    b->trocaSel = 0;
    b->animTroca = 0.0f;
    return false;
}

static void RogueAplicarOpcao(Biblioteca *b, OpcaoEscolha o)
{
    JogadorRogue *j = &b->j;
    char texto[96];

    switch (o.tipo) {

        case OPC_UPGRADE: {
            // Sinergias que já podiam aparecer antes desta escolha
            unsigned long long antes = 0;
            for (int s = 0; s < NUM_UPGRADES; s++)
                if (UPGRADES[s].categoria == CAT_SINERGIA && RogueUpgradeDisponivel(b, s)) antes |= BIT_UP(s);

            j->up[o.id]++;
            j->ultimoUp = o.id;
            if (o.id == UP_VIDA) j->hp = fminf(RogueHpMax(b), j->hp + UP_VIDA_NIVEL);

            if (j->up[o.id] > 1) snprintf(texto, sizeof(texto), "%s NÍVEL %d", UPGRADES[o.id].nome, j->up[o.id]);
            else                 snprintf(texto, sizeof(texto), "NOVA MELHORIA: %s", UPGRADES[o.id].nome);
            RogueAviso(b, texto);

            for (int s = 0; s < NUM_UPGRADES; s++)
                if (UPGRADES[s].categoria == CAT_SINERGIA && !(antes & BIT_UP(s)) && RogueUpgradeDisponivel(b, s)) {
                    snprintf(texto, sizeof(texto), "Luci: Essa combinação abre uma SINERGIA: %s!", UPGRADES[s].nome);
                    RogueRecadoLuci(b, texto);
                    b->luciMsgTempo = 4.0f;
                    break;
                }

            RogueParticulas(b, j->pos, CORES_RARIDADE[UPGRADES[o.id].raridade], 26, 200);
            Telemetria("rogue_build", UPGRADES[o.id].nome, (float)UPGRADES[o.id].raridade, j->up[o.id]);
        } break;

        case OPC_ITEM:
            if (j->itens[o.id] < ITENS[o.id].maxPilha) j->itens[o.id]++;
            snprintf(texto, sizeof(texto), "Pegou: %s", ITENS[o.id].nome);
            RogueAviso(b, texto);
            RogueParticulas(b, j->pos, CORES_RARIDADE[ITENS[o.id].raridade], 20, 180);
            Telemetria("rogue_build", ITENS[o.id].nome, (float)ITENS[o.id].raridade, j->itens[o.id]);
            break;

        case OPC_ARMA:
            if (!RogueReceberArma(b, (TipoArma)o.id, 0, -1)) break;       // vagas cheias: o jogador escolhe qual trocar
            snprintf(texto, sizeof(texto), "Pegou: %s (TAB troca)", DEF_ARMAS[o.id].nome);
            RogueAviso(b, texto);
            RogueParticulas(b, j->pos, DEF_ARMAS[o.id].cor, 20, 180);
            Telemetria("rogue_build", DEF_ARMAS[o.id].nome, (float)DEF_ARMAS[o.id].raridade, 1);
            break;

        default:
            b->perm->fragmentos += FC_OPCAO_RESERVA;
            snprintf(texto, sizeof(texto), "+%d FRAGMENTOS DE CONHECIMENTO", FC_OPCAO_RESERVA);
            RogueAviso(b, texto);
            break;
    }

    TocarSomTom(sfxAcerto, 1.2f);
}

// Cartas da escolha na tela (também usadas no desenho e no mouse)
Rectangle RogueCartaEscolha(int i, int n)
{
    float w = 222.0f, h = 300.0f, gap = 16.0f;
    float total = (float)n * w + (float)(n - 1) * gap;
    return (Rectangle){ 400.0f - total / 2.0f + (float)i * (w + gap), 132.0f, w, h };
}

// LIBERAÇÃO GRADUAL (um sistema novo por missão; perm->tentativas conta as missões começadas):
//   missão 1: o básico (escolha de 1 entre 3, itens do Byteco)
//   missão 2: REROLAR as opções e a aba MELHORIAS (sempre) da loja
//   missão 3: armas novas como recompensa e a aba ARMAS da loja
// Quem já jogou bastante (ou tem save antigo) tem tudo liberado.
bool RogueRerollLiberado(const Biblioteca *b)
{
    return b->perm->tentativas >= MISSOES_PARA_REROLL;
}

bool RogueAbaLojaLiberada(const Biblioteca *b, int aba)          // a loja é visitada ENTRE as missões
{
    return aba == 0 || b->perm->tentativas >= aba;                      // aba 1 depois da 1ª missão; aba 2 depois da 2ª
}

// REROLAR: sorteia outras opções (o que já apareceu nesta escolha não volta). Grátis nas primeiras vezes; depois custa FC.
static void RogueRerolarEscolha(Biblioteca *b)
{
    JogadorRogue *j = &b->j;
    if (!RogueRerollLiberado(b)) {
        RogueAviso(b, TextFormat("O REROLAR abre na %dª missão.", MISSOES_PARA_REROLL));
        TocarSom(sfxErro);
        return;
    }
    bool gratis = j->rerolls > 0;
    if (!gratis && b->perm->fragmentos < REROLL_CUSTO_FC) {
        RogueAviso(b, TextFormat("Reroll custa %d FC.", REROLL_CUSTO_FC));
        TocarSom(sfxErro);
        return;
    }
    if (gratis) j->rerolls--;
    else        b->perm->fragmentos -= REROLL_CUSTO_FC;

    for (int i = 0; i < b->numOpcoes; i++) {
        OpcaoEscolha o = b->opcoes[i];
        if (o.tipo == OPC_UPGRADE) b->rerollUsados |= BIT_UP(o.id);
        else if (o.tipo == OPC_ITEM) b->rerollItens |= 1u << o.id;
        else if (o.tipo == OPC_ARMA) b->rerollArmas |= 1u << o.id;
    }
    int antes = b->numOpcoes;
    RogueGerarOpcoes(b, b->modoEscolha);
    if (b->numOpcoes < antes || (b->numOpcoes == 1 && b->opcoes[0].tipo == OPC_FRAGMENTOS)) {   // acabaram as novidades: libera tudo de novo
        b->rerollUsados = 0; b->rerollItens = 0; b->rerollArmas = 0;
        RogueGerarOpcoes(b, b->modoEscolha);
    }
    b->selecaoEscolha = 0;
    b->animEscolha = 0.2f;
    TocarSomTom(sfxSelecao, 1.4f);
    RogueAviso(b, gratis ? TextFormat("Novas opções! (%d grátis restantes)", j->rerolls) : TextFormat("Novas opções! (-%d FC)", REROLL_CUSTO_FC));
}

void RogueAtualizarEscolha(Biblioteca *b)
{
    int n = b->numOpcoes;
    b->animEscolha += DtJogo();

    // Escolha de TREINO (sala da Mestre): só escolher e confirmar. Sem reroll, sem recusar e nada é aplicado.
    if (b->escolhaTreino) {
        if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))  { b->selecaoEscolha = (b->selecaoEscolha + n - 1) % n; TocarSom(sfxSelecao); }
        if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) { b->selecaoEscolha = (b->selecaoEscolha + 1) % n;     TocarSom(sfxSelecao); }
        for (int i = 0; i < n; i++)
            if (IsKeyPressed(KEY_ONE + i)) { b->selecaoEscolha = i; TocarSom(sfxSelecao); }

        bool cliqueTreino = false;
        Vector2 mt = GetMousePosition();
        for (int i = 0; i < n; i++)
            if (CheckCollisionPointRec(mt, RogueCartaEscolha(i, n))) {
                if (Vector2Length(GetMouseDelta()) > 0.5f) b->selecaoEscolha = i;
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { b->selecaoEscolha = i; cliqueTreino = true; }
            }

        if (b->animEscolha >= 0.35f && (cliqueTreino || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))) {
            b->escolhaAberta = false;
            b->escolhaTreino = false;
            b->escolhaTreinoFeita = true;
            RogueParticulas(b, b->j.pos, GOLD, 20, 180);
            TocarSomTom(sfxAcerto, 1.2f);
            Telemetria("tutorial_escolha_treino", "", (float)b->selecaoEscolha, 0);
        }
        return;
    }

    if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))  { b->selecaoEscolha = (b->selecaoEscolha + n - 1) % n; TocarSom(sfxSelecao); }
    if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) { b->selecaoEscolha = (b->selecaoEscolha + 1) % n;     TocarSom(sfxSelecao); }
    for (int i = 0; i < n; i++)
        if (IsKeyPressed(KEY_ONE + i)) { b->selecaoEscolha = i; TocarSom(sfxSelecao); }

    bool clique = false;
    Vector2 m = GetMousePosition();
    for (int i = 0; i < n; i++)
        if (CheckCollisionPointRec(m, RogueCartaEscolha(i, n))) {
            if (Vector2Length(GetMouseDelta()) > 0.5f) b->selecaoEscolha = i;
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { b->selecaoEscolha = i; clique = true; }
        }

    if (b->animEscolha < 0.35f)             // evita confirmar sem querer ao abrir
        return;

    if (IsKeyPressed(KEY_R) || (CliqueMouse() && MouseEm(RET_REROLAR_ESCOLHA))) {
        RogueRerolarEscolha(b);
        return;
    }

    if (IsKeyPressed(KEY_BACKSPACE) || (CliqueMouse() && MouseEm(RET_RECUSAR_ESCOLHA))) {
        b->perm->fragmentos += FC_RECUSAR_ESCOLHA;
        b->escolhaAberta = false;
        RogueAviso(b, TextFormat("Recusou: +%d FRAGMENTOS", FC_RECUSAR_ESCOLHA));
        TocarSom(sfxSelecao);
        return;
    }

    if (clique || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        RogueAplicarOpcao(b, b->opcoes[b->selecaoEscolha]);
        b->escolhaAberta = false;
    }
}

// Tocou na recompensa do chão
void RogueUsarRecompensa(Biblioteca *b)
{
    JogadorRogue *j = &b->j;
    b->recompensaAtiva = false;

    switch (b->recompensaTipo) {
        case RECOMPENSA_MELHORIA: RogueAbrirEscolha(b, ESCOLHA_NORMAL, false); break;
        case RECOMPENSA_RARA:     RogueAbrirEscolha(b, ESCOLHA_RARA, false);   break;
        case RECOMPENSA_ARMA:     RogueAbrirEscolha(b, ESCOLHA_ARMA, false);   break;

        case RECOMPENSA_FRAGMENTOS: {
            int n = 5 + b->profundidade;
            for (int k = 0; k < n; k++)
                RogueSoltar(b, b->recompensaPos, true, ITEM_ANTIVIRUS);
            RogueAviso(b, TextFormat("BIBLIOTECA RESTAURADA: %d FRAGMENTOS!", n));
            TocarSomTom(sfxAcerto, 1.5f);
        } break;

        case RECOMPENSA_VIDA:
            j->hp = fminf(RogueHpMax(b), j->hp + RogueHpMax(b) * 0.35f);
            j->itens[ITEM_ANTIVIRUS]++;
            RogueAviso(b, "RESTAURAÇÃO: +35% de vida e 1 Antivírus");
            RogueParticulas(b, j->pos, LIME, 24, 180);
            TocarSomTom(sfxPegar, 0.9f);
            break;

        default:
            break;
    }

    Telemetria("rogue_recompensa_sala", NOMES_RECOMPENSA[b->recompensaTipo], (float)b->profundidade, b->indiceRota);
}

const char *NOMES_PENALIDADE[3] = { "+20% de dano recebido", "-10% de velocidade", "-15% de vida máxima" };

void RogueMontarTelaEvento(const Biblioteca *b, TelaEvento *t)
{
    memset(t, 0, sizeof(*t));
    for (int i = 0; i < 3; i++) t->disponivel[i] = true;

    #define OPCAO(i, r, e) do { snprintf(t->rotulo[i], sizeof(t->rotulo[i]), "%s", (r)); \
                                snprintf(t->efeito[i], sizeof(t->efeito[i]), "%s", (e)); } while (0)

    switch (b->evento) {

        case EVT_ARQUIVO_CORROMPIDO:
            t->titulo = "ARQUIVO CORROMPIDO";
            snprintf(t->texto, sizeof(t->texto), "Um arquivo pulsa com dados instáveis. Lá dentro há algo raro, mas abri-lo sem cuidado vai corromper parte da sua integridade.");
            t->numOpcoes = 3;
            OPCAO(0, "Abrir o arquivo", "Perde 30% da vida atual. Escolha 1 de 3 melhorias RARAS.");
            OPCAO(1, "Verificar com a Lente", "Gasta 1 Lente da Verificação: abre com segurança e escolhe 1 de 3 melhorias.");
            OPCAO(2, "Deixar para lá", "Nada acontece.");
            t->disponivel[0] = b->j.hp > 10.0f;
            t->disponivel[1] = b->j.itens[ITEM_LENTE] > 0;
            break;

        case EVT_BANCO_INSTAVEL:
            t->titulo = "BANCO DE DADOS INSTÁVEL";
            snprintf(t->texto, sizeof(t->texto), "Um banco de dados está vazando Fragmentos de Conhecimento. Estabilizá-lo vai atrair ecos de ELITE.");
            t->numOpcoes = 3;
            OPCAO(0, "Estabilizar o banco", "Combate contra elites. Vencendo: +10 Fragmentos e 1 de 3 melhorias.");
            OPCAO(1, "Coletar o que vazou", "+3 Fragmentos agora, sem luta.");
            OPCAO(2, "Ignorar", "Nada acontece.");
            break;

        case EVT_MODELO_EXPERIMENTAL:
            t->titulo = "MODELO EXPERIMENTAL";
            snprintf(t->texto, sizeof(t->texto), "Uma versão beta de você mesmo oferece um poder que ninguém testou. Todo modelo sem testes tem efeitos colaterais.");
            t->numOpcoes = 2;
            OPCAO(0, "Instalar a versão beta", TextFormat("1 de 3 melhorias RARAS. Efeito colateral até o fim da tentativa: %s.", NOMES_PENALIDADE[b->varianteEvento]));
            OPCAO(1, "Recusar", "Testar antes de confiar. A Luci aprova: +1 de estabilidade.");
            t->disponivel[0] = !(b->j.penalidade & (1 << b->varianteEvento));
            break;

        case EVT_MEMORIA:
            t->titulo = "MEMÓRIA ESQUECIDA";
            snprintf(t->texto, sizeof(t->texto), "Entre as estantes, um registro antigo pisca com a assinatura da Luci. Ela fica em silêncio quando você se aproxima.");
            t->numOpcoes = 2;
            OPCAO(0, "Ler o registro com a Luci", "Uma memória dela. A Luci se estabiliza (+1).");
            OPCAO(1, "Arquivar para o Byteco", "+4 Fragmentos de Conhecimento.");
            break;

        case EVT_PROTOCOLO:
            t->titulo = "PROTOCOLO DE EMERGÊNCIA";
            snprintf(t->texto, sizeof(t->texto), "Um terminal de manutenção oferece restauração. Todo protocolo cobra um preço.");
            t->numOpcoes = 3;
            OPCAO(0, "Restauração paga", "Recupera 60% da vida. Custa 6 Fragmentos.");
            OPCAO(1, "Restauração por troca", (b->j.ultimoUp >= 0)
                  ? TextFormat("Recupera 40%% da vida. Perde um nível de: %s.", UPGRADES[b->j.ultimoUp].nome)
                  : "Recupera 40% da vida. Perde um nível da última melhoria (você não tem nenhuma).");
            OPCAO(2, "Recusar", "Nada acontece.");
            t->disponivel[0] = b->perm->fragmentos >= 6;
            t->disponivel[1] = b->j.ultimoUp >= 0 && b->j.up[b->j.ultimoUp] > 0;
            break;

        case EVT_FONTE_FALSA:
        case EVT_RESPOSTA_IA: {
            const QuizEvento *q = (b->evento == EVT_FONTE_FALSA) ? &QUIZ_FONTES[b->varianteEvento] : &QUIZ_RESPOSTAS[b->varianteEvento];
            t->titulo = (b->evento == EVT_FONTE_FALSA) ? "FONTE DUVIDOSA" : "RESPOSTA SUSPEITA";
            snprintf(t->texto, sizeof(t->texto), "%s", q->texto);
            t->numOpcoes = 3;
            for (int i = 0; i < 3; i++) OPCAO(i, q->opcoes[i], "");
        } break;

        case EVT_CONFIANCA_LUCI:
            t->titulo = "UMA PERGUNTA DA LUCI";
            snprintf(t->texto, sizeof(t->texto), "Luci: Thomas... posso perguntar uma coisa? Com tudo o que está acontecendo aqui... você confia em mim?");
            t->numOpcoes = 3;
            OPCAO(0, "\"Confio, mas vou conferir o que você disser.\"", "Confiança com verificação. (+1 de estabilidade)");
            OPCAO(1, "\"Confio totalmente.\"", "Ela fica feliz e divide um atalho: +3 Fragmentos.");
            OPCAO(2, "\"Ainda não sei.\"", "Uma resposta honesta.");
            break;

        case EVT_DESCANSO:
            t->titulo = "SALA DE LEITURA";
            snprintf(t->texto, sizeof(t->texto), "Poltronas antigas e uma luz calma. Os ecos não alcançam este lugar. Há tempo para UMA coisa.");
            t->numOpcoes = 3;
            OPCAO(0, "Descansar", "Recupera 40% da vida máxima.");
            OPCAO(1, "Conversar com a Luci", "+1 de estabilidade e uma conversa.");
            OPCAO(2, "Estudar", "Uma melhoria que você já tem sobe um nível.");
            t->disponivel[2] = false;
            for (int u = 0; u < NUM_UPGRADES; u++)
                if (b->j.up[u] > 0 && b->j.up[u] < UPGRADES[u].maxNivel) t->disponivel[2] = true;
            break;

        default:    // EVT_DESAFIO
            if (b->desafioSala == DESAFIO_SALA_ECOS) {
                t->titulo = "DESAFIO: VERIFICAÇÃO RELÂMPAGO";
                snprintf(t->texto, sizeof(t->texto), "Ecos falsos vão surgir pela sala. Destrua %d antes que o tempo acabe (%d s). Eco que ninguém confere explode em balas!",
                         DESAFIO_ECOS_META, (int)DESAFIO_ECOS_TEMPO);
            } else {
                t->titulo = "DESAFIO: TESTE DE INTEGRIDADE";
                snprintf(t->texto, sizeof(t->texto), "Sobreviva %d s entre feixes e explosões de dados levando no máximo %d golpes. Esquive com atenção aos avisos!",
                         (int)DESAFIO_INTEG_TEMPO, DESAFIO_INTEG_GOLPES);
            }
            t->numOpcoes = 2;
            OPCAO(0, "Aceitar o desafio", "Vencendo: 1 de 3 melhorias RARAS e +5 Fragmentos.");
            OPCAO(1, "Agora não", "A porta continua aberta. Dá para voltar ao console.");
            break;
    }

    #undef OPCAO
}

static void RogueResultadoEvento(Biblioteca *b, const char *texto)
{
    snprintf(b->resultadoEvento, sizeof(b->resultadoEvento), "%s", texto);
    b->eventoResolvido = true;
    b->eventoUsado = true;
}

static void RogueResolverEvento(Biblioteca *b, ProgressoCorredor *prog, SessaoDialogo *dlg, int op)
{
    JogadorRogue *j = &b->j;
    Telemetria("rogue_evento", TextFormat("%d", (int)b->evento), (float)op, b->profundidade);

    switch (b->evento) {

        case EVT_ARQUIVO_CORROMPIDO:
            if (op == 0) {
                j->hp = fmaxf(1.0f, j->hp * 0.7f);
                b->escolhaPendente = ESCOLHA_RARA;
                b->tremor = 0.25f;
                RogueResultadoEvento(b, "Os dados corrompidos queimam a sua integridade... mas algo valioso se revela.");
            } else if (op == 1) {
                j->itens[ITEM_LENTE]--;
                b->escolhaPendente = ESCOLHA_NORMAL;
                RogueResultadoEvento(b, "A Lente separa o que é dado do que é ruído. Verificar antes de abrir: sem nenhum dano.");
            } else {
                RogueResultadoEvento(b, "Luci: Às vezes não abrir um arquivo suspeito é a decisão mais segura.");
            }
            break;

        case EVT_BANCO_INSTAVEL:
            if (op == 0) {
                // Começa uma onda de elite aqui mesmo: as portas fecham até vencer
                b->eventoAberto = false;
                b->eventoUsado = true;
                b->eventoCombate = true;
                b->limpa = false;
                b->objetivoSala = OBJC_ELIMINAR;
                RogueMontarEncontro(b, &LAYOUT_EVENTO, RogueSortearEncontro(b, RogueNivelCombate(b)));
                TocarSomTom(sfxErro, 0.6f);
                return;
            } else if (op == 1) {
                b->perm->fragmentos += 3;
                RogueResultadoEvento(b, "Você recolhe o que vazou: +3 Fragmentos de Conhecimento.");
            } else {
                RogueResultadoEvento(b, "O banco segue vazando, longe de você.");
            }
            break;

        case EVT_MODELO_EXPERIMENTAL:
            if (op == 0) {
                j->penalidade |= 1 << b->varianteEvento;
                if (j->hp > RogueHpMax(b)) j->hp = RogueHpMax(b);
                b->escolhaPendente = ESCOLHA_RARA;
                RogueResultadoEvento(b, TextFormat("Versão beta instalada. Efeito colateral: %s. Escolha o seu poder.", NOMES_PENALIDADE[b->varianteEvento]));
            } else {
                RogueMudarEstabilidade(b, 1, "testar antes de confiar");
                RogueResultadoEvento(b, "Luci: Boa. Um modelo que ninguém testou pode fazer qualquer coisa... inclusive errar com confiança.");
            }
            break;

        case EVT_MEMORIA:
            if (op == 0) {
                b->eventoAberto = false;
                b->eventoUsado = true;
                b->memoriasVistas |= 1u << b->varianteEvento;
                RogueMudarEstabilidade(b, 1, "memória recuperada");
                DialogoIniciar(dlg, &dialogosMemoria[b->varianteEvento], OBJ_NENHUM);
                return;
            }
            b->perm->fragmentos += 4;
            RogueResultadoEvento(b, "O registro vira Fragmentos de Conhecimento: +4. A Luci não diz nada.");
            break;

        case EVT_PROTOCOLO:
            if (op == 0) {
                b->perm->fragmentos -= 6;
                j->hp = fminf(RogueHpMax(b), j->hp + RogueHpMax(b) * 0.6f);
                RogueParticulas(b, j->pos, LIME, 24, 180);
                RogueResultadoEvento(b, "Restauração concluída. -6 Fragmentos.");
            } else if (op == 1) {
                int u = j->ultimoUp;
                j->up[u]--;
                if (j->hp > RogueHpMax(b)) j->hp = RogueHpMax(b);
                j->hp = fminf(RogueHpMax(b), j->hp + RogueHpMax(b) * 0.4f);
                RogueParticulas(b, j->pos, LIME, 18, 160);
                RogueResultadoEvento(b, TextFormat("Restauração concluída. %s perdeu um nível.", UPGRADES[u].nome));
                if (j->up[u] == 0) j->ultimoUp = -1;
            } else {
                RogueResultadoEvento(b, "O terminal volta a dormir.");
            }
            break;

        case EVT_FONTE_FALSA:
        case EVT_RESPOSTA_IA: {
            const QuizEvento *q = (b->evento == EVT_FONTE_FALSA) ? &QUIZ_FONTES[b->varianteEvento] : &QUIZ_RESPOSTAS[b->varianteEvento];
            bool certo = (op == q->correta);
            RogueRegistrarResposta(b, certo);
            if (certo) {
                b->perm->fragmentos += 5;
                AlterarPontos(prog, PONTOS_DESAFIO);
                RogueMudarEstabilidade(b, 1, "fonte conferida");
                TocarSom(sfxAcerto);
                RogueResultadoEvento(b, TextFormat("CERTO! +5 Fragmentos. %s", q->explicacao));
            } else {
                j->hp = fmaxf(1.0f, j->hp - 15.0f);
                b->tremor = 0.25f;
                TocarSom(sfxErro);
                RogueResultadoEvento(b, TextFormat("ERROU (-15 de vida). %s", q->explicacao));
            }
        } break;

        case EVT_CONFIANCA_LUCI:
            if (op == 0) {
                RogueMudarEstabilidade(b, 1, "confiança com verificação");
                RogueHabitoLuci(b, false);
                RogueResultadoEvento(b, "Luci: É a melhor resposta que alguém já me deu. Confiar e conferir... vou lembrar disso.");
            } else if (op == 1) {
                b->perm->fragmentos += 3;
                b->perm->confiouCego++;
                RogueHabitoLuci(b, true);
                RogueResultadoEvento(b, "Luci: Que bom... assim fica tudo mais fácil. Para mim. (+3 Fragmentos)");
            } else {
                RogueResultadoEvento(b, "Luci: Justo. Confiança se constrói conferindo, não prometendo.");
            }
            break;

        case EVT_DESCANSO:
            if (op == 0) {
                j->hp = fminf(RogueHpMax(b), j->hp + RogueHpMax(b) * 0.4f);
                RogueParticulas(b, j->pos, LIME, 24, 180);
                RogueResultadoEvento(b, "Você descansa entre os livros. A integridade volta, devagar.");
            } else if (op == 1) {
                RogueMudarEstabilidade(b, 1, "uma boa conversa");
                RogueResultadoEvento(b, (RogueEstab(b) >= 4)
                    ? "Luci: Sabe o que eu gosto em você? Você pergunta \"de onde veio isso?\". Quase ninguém pergunta."
                    : "Luci: Às vezes eu sinto que tem outra voz dentro de mim, respondendo antes de eu pensar. Obrigada por ficar.");
            } else {
                int cand[NUM_UPGRADES], n = 0;
                for (int u = 0; u < NUM_UPGRADES; u++)
                    if (j->up[u] > 0 && j->up[u] < UPGRADES[u].maxNivel) cand[n++] = u;
                if (n > 0) {
                    int u = cand[GetRandomValue(0, n - 1)];
                    j->up[u]++;
                    if (u == UP_VIDA) j->hp = fminf(RogueHpMax(b), j->hp + UP_VIDA_NIVEL);
                    RogueResultadoEvento(b, TextFormat("Você estuda os seus próprios dados. %s subiu para o nível %d.", UPGRADES[u].nome, j->up[u]));
                }
            }
            break;

        default:    // EVT_DESAFIO
            b->eventoAberto = false;
            if (op != 0)
                return;                             // "agora não": dá para voltar ao console
            b->eventoUsado = true;
            b->estadoDesafio = 1;
            b->contadorDesafio = 0;
            b->j.golpesSofridos = 0;
            if (b->desafioSala == DESAFIO_SALA_ECOS) {
                b->tempoDesafioSala = DESAFIO_ECOS_TEMPO;
                b->cronoDesafio = 0.6f;
            } else {
                b->tempoDesafioSala = DESAFIO_INTEG_TEMPO;
                RogueCriarArmadilha(b, ARM_FEIXE);
                RogueCriarArmadilha(b, ARM_FEIXE);
                RogueCriarArmadilha(b, ARM_EXPLOSAO);
                RogueCriarArmadilha(b, ARM_TORRE);
            }
            RogueAviso(b, "DESAFIO INICIADO!");
            TocarSomTom(sfxErro, 0.8f);
            return;
    }
}

// Opção i do evento (a mesma conta de DesenharEvento), para o mouse
static Rectangle RogueRetOpcaoEvento(const TelaEvento *t, int i)
{
    Rectangle painel = { 70, 80, 660, 420 };
    int x = (int)painel.x + 24, y = (int)painel.y + 18 + 40;
    char linhas[640];

    QuebrarTexto(t->texto, (int)painel.width - 48, 16, linhas, (int)sizeof(linhas));
    y += ContarLinhas(linhas) * 22 + 18;

    for (int k = 0; k <= i && k < t->numOpcoes; k++) {
        QuebrarTexto(TextFormat("%d) %s", k + 1, t->rotulo[k]), (int)painel.width - 90, 16, linhas, (int)sizeof(linhas));
        int nr = ContarLinhas(linhas);
        char ef[300] = "";
        if (t->efeito[k][0]) QuebrarTexto(t->efeito[k], (int)painel.width - 100, 12, ef, (int)sizeof(ef));
        int ne = ef[0] ? ContarLinhas(ef) : 0;
        int alt = nr * 20 + ne * 15 + 10;
        if (k == i)
            return (Rectangle){ (float)x - 10, (float)y - 5, painel.width - 28, (float)alt };
        y += alt + 6;
    }

    return (Rectangle){ 0, 0, 0, 0 };
}

void RogueAtualizarEvento(Biblioteca *b, ProgressoCorredor *prog, SessaoDialogo *dlg)
{
    bool confirmar = IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);

    // Mostrando o resultado: ENTER fecha (e abre a escolha prometida, se houver)
    if (b->eventoResolvido) {
        if (confirmar || CliqueMouse()) {
            b->eventoAberto = false;
            b->eventoResolvido = false;
            if (b->escolhaPendente >= 0) {
                RogueAbrirEscolha(b, (ModoEscolha)b->escolhaPendente, true);
                b->escolhaPendente = -1;
            }
        }
        return;
    }

    TelaEvento t;
    RogueMontarTelaEvento(b, &t);
    int n = t.numOpcoes;

    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))   { b->selecaoEvento = (b->selecaoEvento + n - 1) % n; TocarSom(sfxSelecao); }
    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) { b->selecaoEvento = (b->selecaoEvento + 1) % n;     TocarSom(sfxSelecao); }
    for (int i = 0; i < n; i++)
        if (IsKeyPressed(KEY_ONE + i)) { b->selecaoEvento = i; TocarSom(sfxSelecao); }

    // Mouse: passar por cima marca, clicar escolhe; o X fecha sem escolher
    bool clique = CliqueMouse();
    for (int i = 0; i < n; i++) {
        if (!MouseEm(RogueRetOpcaoEvento(&t, i))) continue;
        if (MouseMexeu() && b->selecaoEvento != i) { b->selecaoEvento = i; TocarSom(sfxSelecao); }
        if (clique) { b->selecaoEvento = i; confirmar = true; }
    }

    if (IsKeyPressed(KEY_BACKSPACE) || (clique && MouseEm(RET_FECHAR_EVENTO))) {   // fecha sem escolher (dá para voltar)
        b->eventoAberto = false;
        return;
    }

    if (!confirmar)
        return;

    if (!t.disponivel[b->selecaoEvento]) {
        RogueAviso(b, "Essa opção não está disponível agora.");
        TocarSom(sfxErro);
        return;
    }

    RogueResolverEvento(b, prog, dlg, b->selecaoEvento);
}

// As provas da sala de desafio (as portas ficam abertas o tempo todo: é opcional)
void RogueAtualizarDesafioSala(Biblioteca *b, ProgressoCorredor *prog, float dt)
{
    if (b->tipoSala != SALA_DESAFIO || b->estadoDesafio != 1)
        return;

    b->tempoDesafioSala -= dt;
    int resultado = 0;          // 1 = venceu, -1 = falhou

    if (b->desafioSala == DESAFIO_SALA_ECOS) {
        b->cronoDesafio -= dt;
        if (b->cronoDesafio <= 0.0f && RogueContarTipo(b, INIMIGO_ECO) < 5) {
            b->cronoDesafio = 1.2f;
            for (int k = 0; k < 2; k++) {
                Vector2 p;
                if (!RogueSortearLugar(b, 14.0f, 110.0f, &p)) continue;
                Inimigo *e = RogueCriarInimigo(b, INIMIGO_ECO, p);
                if (e != NULL) { e->desafio = true; e->timer = DESAFIO_ECO_TEMPO; }
            }
        }
        if (b->contadorDesafio >= DESAFIO_ECOS_META) resultado = 1;
        else if (b->tempoDesafioSala <= 0.0f)        resultado = -1;
    } else {
        if (b->j.golpesSofridos > DESAFIO_INTEG_GOLPES) resultado = -1;
        else if (b->tempoDesafioSala <= 0.0f)          resultado = 1;
    }

    if (resultado == 0)
        return;

    // Fim da prova: some com os ecos, as balas e as armadilhas
    RogueDissolverInimigos(b);
    memset(b->arm, 0, sizeof(b->arm));
    RogueAtualizarObstaculosDinamicos(b);

    if (resultado > 0) {
        b->estadoDesafio = 2;
        b->perm->fragmentos += 5;
        RogueColocarRecompensa(b, RECOMPENSA_MELHORIA);
        b->j.rerolls++;                         // desafio resolvido: escolha comum + 1 reroll grátis
        RogueAviso(b, "+1 REROLL GRÁTIS");
        AlterarPontos(prog, 10);
        RogueRegistrarResposta(b, true);
        RogueAviso(b, "DESAFIO CONCLUÍDO! +5 FRAGMENTOS");
        TocarSom(sfxAcerto);
        MostrarEmocaoLuci(EMOCAO_ALEGRE, 2.5f);
    } else {
        b->estadoDesafio = 3;
        RogueAviso(b, "DESAFIO FALHOU");
        RogueRecadoLuci(b, "Luci: Quase! A porta continua aberta. Da próxima vez, de olho nos avisos.");
        TocarSom(sfxErro);
    }
    Telemetria("rogue_desafio_sala", (b->desafioSala == DESAFIO_SALA_ECOS) ? "ecos" : "integridade", (float)resultado, b->contadorDesafio);
}
