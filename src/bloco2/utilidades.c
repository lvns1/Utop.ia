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

// ---------- Utilidades ----------

// Direção do sprite a partir da mira (8 direções; y cresce para baixo)
Direcao DirecaoDoVetor(Vector2 v)
{
    static const Direcao setores[8] = {
        DIR_DIREITA, DIR_FRENTE_DIREITA, DIR_FRENTE, DIR_FRENTE_ESQUERDA,
        DIR_ESQUERDA, DIR_COSTAS_ESQUERDA, DIR_COSTAS, DIR_COSTAS_DIREITA
    };

    int s = (int)roundf(atan2f(v.y, v.x) / (PI / 4.0f));
    return setores[(s + 8) % 8];
}

// Mantém um círculo dentro da arena e fora das estantes
void RogueEmpurrarParaFora(Vector2 *p, float raio, const Rectangle *obst, int n)
{
    p->x = Clamp(p->x, ArenaRogue().x + raio, ArenaRogue().x + ArenaRogue().width - raio);
    p->y = Clamp(p->y, ArenaRogue().y + raio, ArenaRogue().y + ArenaRogue().height - raio);

    for (int i = 0; i < n; i++) {

        Rectangle r = obst[i];
        float cx = Clamp(p->x, r.x, r.x + r.width);
        float cy = Clamp(p->y, r.y, r.y + r.height);
        float dx = p->x - cx, dy = p->y - cy;
        float d2 = dx * dx + dy * dy;

        if (d2 >= raio * raio)
            continue;

        if (d2 > 0.0001f) {
            float d = sqrtf(d2);
            p->x = cx + dx / d * raio;
            p->y = cy + dy / d * raio;
        } else {
            // Centro dentro da estante: sai pelo lado mais perto
            float esq = p->x - r.x, dir = r.x + r.width - p->x;
            float cima = p->y - r.y, baixo = r.y + r.height - p->y;
            float m = fminf(fminf(esq, dir), fminf(cima, baixo));

            if (m == esq)       p->x = r.x - raio;
            else if (m == dir)  p->x = r.x + r.width + raio;
            else if (m == cima) p->y = r.y - raio;
            else                p->y = r.y + r.height + raio;
        }
    }
}

void RogueParticulas(Biblioteca *b, Vector2 origem, Color cor, int quantidade, float velMax)
{
    for (int i = 0; i < MAX_PARTICULAS_ROGUE && quantidade > 0; i++) {

        ParticulaRogue *p = &b->part[i];

        if (p->vida > 0.0f)
            continue;

        float ang = (float)GetRandomValue(0, 360) * DEG2RAD;
        float vel = (float)GetRandomValue(20, (int)velMax);

        p->pos = origem;
        p->vel = (Vector2){ cosf(ang) * vel, sinf(ang) * vel };
        p->vidaMax = p->vida = 0.25f + (float)GetRandomValue(0, 30) / 100.0f;
        p->tam = (float)GetRandomValue(2, 4);
        p->cor = cor;
        quantidade--;
    }
}

void RogueAviso(Biblioteca *b, const char *texto)
{
    snprintf(b->aviso, sizeof(b->aviso), "%s", texto);
    b->tempoAviso = 1.8f;
}
void RogueFaixaVirada(Biblioteca *b, const char *titulo, const char *sub, Color cor)
{
    snprintf(b->faixaTitulo, sizeof(b->faixaTitulo), "%s", titulo);
    snprintf(b->faixaSub, sizeof(b->faixaSub), "%s", sub);
    b->faixaCor = cor;
    b->faixaVirada = FAIXA_VIRADA_TEMPO;
    TocarGrupo(&gChefeVirada, 1.0f, 0.9f);
}

// O mouse em coordenadas do mundo. A câmera na mira desloca o mundo na tela (offset = -camAdiante),
// então o ponto do mundo embaixo do cursor é o mouse + camAdiante.
Vector2 RogueMouseMundo(const Biblioteca *b)
{
    return Vector2Add(GetMousePosition(), b->camAdiante);
}

// Texto que sobe de um ponto da arena e some (o mais antigo é reaproveitado)
void RogueTextoFlutuante(Biblioteca *b, Vector2 pos, const char *texto, Color cor)
{
    int escolhido = 0;
    for (int i = 0; i < MAX_TEXTOS_FLUTUANTES; i++) {
        if (b->textos[i].tempo <= 0.0f) { escolhido = i; break; }
        if (b->textos[i].tempo < b->textos[escolhido].tempo) escolhido = i;
    }
    TextoFlutuante *t = &b->textos[escolhido];
    t->pos = pos;
    t->tempo = 1.2f;
    t->cor = cor;
    snprintf(t->texto, sizeof(t->texto), "%s", texto);
}

// Número de dano: soma no número do mesmo alvo se ele acabou de nascer; senão usa uma vaga
// livre (ou o número mais antigo). O número que soma não volta para o começo: ele continua
// subindo, e o próximo golpe já nasce como um número novo.
void RogueNumeroDano(Biblioteca *b, int alvo, Vector2 pos, float dano, int tipo)
{
#if COMBATE_NUMEROS_DANO
    if (!(dano >= 0.0f) || !Prefs()->numerosDano)      // (desligado na tela de Opções)
        return;

    for (int i = 0; i < MAX_NUMEROS_DANO; i++) {
        NumeroDano *n = &b->numeros[i];
        if (n->tempo > NUMERO_DANO_TEMPO - NUMERO_DANO_JUNTAR && n->alvo == alvo && n->tipo == tipo) {
            n->valor += dano;
            n->pop = 1.0f;
            return;
        }
    }

    int escolhido = 0;
    for (int i = 0; i < MAX_NUMEROS_DANO; i++) {
        if (b->numeros[i].tempo <= 0.0f) { escolhido = i; break; }
        if (b->numeros[i].tempo < b->numeros[escolhido].tempo) escolhido = i;
    }
    NumeroDano *n = &b->numeros[escolhido];
    n->pos = (Vector2){ pos.x + (float)GetRandomValue(-8, 8), pos.y };
    n->valor = dano;
    n->tempo = NUMERO_DANO_TEMPO;
    n->pop = 1.0f;
    n->alvo = alvo;
    n->tipo = (unsigned char)tipo;
#else
    (void)b; (void)alvo; (void)pos; (void)dano; (void)tipo;
#endif
}

// Faísca no ponto do acerto (a mais antiga é reaproveitada)
void RogueFaisca(Biblioteca *b, Vector2 pos, Vector2 dir, Color cor)
{
#if COMBATE_FAISCAS
    int escolhido = 0;
    for (int i = 0; i < MAX_FAISCAS; i++) {
        if (b->faiscas[i].tempo <= 0.0f) { escolhido = i; break; }
        if (b->faiscas[i].tempo < b->faiscas[escolhido].tempo) escolhido = i;
    }
    FaiscaAcerto *f = &b->faiscas[escolhido];
    f->pos = pos;
    f->dir = (Vector2Length(dir) > 0.01f) ? Vector2Normalize(dir) : (Vector2){ 0.0f, -1.0f };
    f->tempo = FAISCA_TEMPO;
    f->cor = cor;
#else
    (void)b; (void)pos; (void)dir; (void)cor;
#endif
}

// Feedback de erro na verificação: o termo falso aparece com aberração cromática e a justificativa
void RogueErroVerificacao(Biblioteca *b, const char *termo, const char *explicacao)
{
    snprintf(b->erroTermo, sizeof(b->erroTermo), "%s", termo);
    snprintf(b->erroExplicacao, sizeof(b->erroExplicacao), "%s", explicacao);
    b->tempoErro = 2.8f;
}

Projetil *RogueDisparar(Biblioteca *b, Vector2 origem, Vector2 dir, float vel, float dano, float raio, bool doJogador)
{
    for (int i = 0; i < MAX_PROJETEIS; i++) {

        if (b->proj[i].ativo)
            continue;

        b->proj[i] = (Projetil){ .ativo = true, .pos = origem, .vel = Vector2Scale(dir, vel), .raio = raio, .dano = dano, .doJogador = doJogador };
        return &b->proj[i];
    }

    return NULL;
}

void RogueSoltar(Biblioteca *b, Vector2 pos, bool fragmento, TipoItem item)
{
    for (int i = 0; i < MAX_COLETAVEIS; i++) {

        if (b->col[i].ativo)
            continue;

        float ang = (float)GetRandomValue(0, 360) * DEG2RAD;
        b->col[i] = (Coletavel){ true, fragmento, item, pos, { cosf(ang) * 90.0f, sinf(ang) * 90.0f }, 0.0f };
        return;
    }
}

int RogueInimigosVivos(const Biblioteca *b)
{
    int n = 0;

    for (int i = 0; i < MAX_INIMIGOS; i++)
        if (b->ini[i].ativo)
            n++;

    return n;
}

// Atributos: base + melhorias permanentes (Byteco) + itens passivos da mochila + efeitos.
// Todo bônus passa por aqui (nada é somado direto nos atributos), então pegar, perder
// ou trocar de arma nunca aplica um bônus duas vezes.

// Quantos de um item passivo valem (nunca passa do limite da tabela)
int RogueQtd(const JogadorRogue *j, TipoItem it)
{
    return (j->itens[it] > ITENS[it].maxPilha) ? ITENS[it].maxPilha : j->itens[it];
}

// Nível de uma melhoria da build (reformulação)
int RogueUp(const JogadorRogue *j, TipoUpgrade u)
{
    return j->up[u];
}

float RogueHpMax(const Biblioteca *b)
{
    float hp = (ROGUE_HP_MAX + MELHORIA_HP * (float)PermNivel(b->perm, MELHORIA_VIDA) + UP_VIDA_NIVEL * (float)RogueUp(&b->j, UP_VIDA))
               * (1.0f + NUCLEO_HP * (float)RogueQtd(&b->j, ITEM_NUCLEO));
    if (b->j.penalidade & PEN_VIDA) hp *= PEN_VIDA_FATOR;
    return hp;
}

float RogueEpMax(const Biblioteca *b)
{
    return ROGUE_EP_MAX + MELHORIA_EP * (float)PermNivel(b->perm, MELHORIA_ENERGIA) + (b->j.seloAmplificado ? SIL_BH_EP_BONUS : 0.0f);
}

// Multiplicador de dano de todas as armas (Processador Neural, a melhoria do Byteco e a build)
float RogueFatorDano(const Biblioteca *b)
{
    const JogadorRogue *j = &b->j;
    float f = 1.0f + BONUS_PROCESSADOR * (float)RogueQtd(j, ITEM_PROCESSADOR) + MELHORIA_DANO * (float)PermNivel(b->perm, MELHORIA_ATAQUE)
            + UP_DANO_NIVEL * (float)RogueUp(j, UP_DANO);

    // Contra-Ataque Heurístico: logo depois de esquivar
    if (j->bonusPosEsquiva > 0.0f && RogueUp(j, UP_POS_ESQUIVA) > 0)
        f *= 1.4f + 0.3f * (float)(RogueUp(j, UP_POS_ESQUIVA) - 1);

    return f;
}

// Dano do Disparador de Dados (também é a base do Disparo Dedutivo)
float RogueDanoTiro(const Biblioteca *b)
{
    return TIRO_DANO * RogueFatorDano(b);
}

float RogueFatorDanoRecebido(const JogadorRogue *j)
{
    float f = 1.0f - REDUCAO_FIREWALL * (float)RogueQtd(j, ITEM_FIREWALL) - FIREWALL_ADAPTATIVO * (float)RogueQtd(j, ITEM_FIREWALL_ADAPTATIVO);
    f = fmaxf(FATOR_DANO_MINIMO, f);
    if (j->penalidade & PEN_FRAGIL) f *= PEN_FRAGIL_FATOR;      // Modelo Experimental
    return f;
}

float RogueVelocidadeJogador(const JogadorRogue *j)
{
    float f = 1.0f + ROTEAMENTO_VELOCIDADE * (float)RogueQtd(j, ITEM_ROTEAMENTO) + UP_VELOCIDADE_NIVEL * (float)RogueUp(j, UP_VELOCIDADE) + j->permVel;
    if (j->efeito[EFEITO_OVERCLOCK] > 0.0f) f += EFEITO_OVERCLOCK_VELOCIDADE;
    if (j->penalidade & PEN_LENTO) f *= PEN_LENTO_FATOR;
    return ROGUE_VELOCIDADE * f;
}

float RogueRecargaEsquiva(const JogadorRogue *j)
{
    float f = 1.0f - ROTEAMENTO_ESQUIVA * (float)RogueQtd(j, ITEM_ROTEAMENTO) - UP_ESQUIVA_NIVEL * (float)RogueUp(j, UP_ESQUIVA);
    return ESQUIVA_RECARGA * fmaxf(0.4f, f);
}

// Intervalo entre disparos: Overclock e Buffer aceleram, mas nunca passam do limite da arma
float RogueIntervaloArma(const JogadorRogue *j, TipoArma arma)
{
    float cadencia = 1.0f + BUFFER_CADENCIA * (float)RogueQtd(j, ITEM_BUFFER) + UP_CADENCIA_NIVEL * (float)RogueUp(j, UP_CADENCIA);
    if (j->efeito[EFEITO_OVERCLOCK] > 0.0f) cadencia += EFEITO_OVERCLOCK_CADENCIA;
    float intervalo = DEF_ARMAS[arma].intervalo;
    float minimo = DEF_ARMAS[arma].intervaloMin;
    if (arma == ARMA_PULSO_FW && RogueUp(j, UP_ARMA_PULSO_FW) > 0) { intervalo *= 0.8f; minimo *= 0.8f; }   // Muralha Ativa
    if (arma == ARMA_KATANA && RogueUp(j, UP_ARMA_KATANA_RAPIDA) > 0)    { intervalo *= 0.8f; minimo *= 0.8f; }   // Têmpera Rápida
    return fmaxf(minimo, intervalo / cadencia);
}

// Raio e dano do Pulso de Integridade (Pulso Amplificado)
float RogueRaioPulso(const JogadorRogue *j)
{
    return PULSO_RAIO * (1.0f + UP_PULSO_RAIO * (float)RogueUp(j, UP_PULSO));
}

float RogueDanoPulso(const JogadorRogue *j)
{
    return PULSO_DANO * (1.0f + UP_PULSO_DANO * (float)RogueUp(j, UP_PULSO));
}

TipoArma RogueArmaAtual(const JogadorRogue *j)
{
    return j->usandoExtra ? j->armaExtra : j->armaBase;
}

// Efeitos com tempo e contadores da arma: somem ao voltar ao átrio, ao morrer e ao começar outra tentativa
void RogueLimparEfeitos(JogadorRogue *j)
{
    memset(j->efeito, 0, sizeof(j->efeito));
    j->carga = 0.0f;
    j->calor = 0.0f;
    j->superaquecida = false;
    j->disparos = 0;
    j->fragmentacaoArmada = false;
    j->alvoAnalise = 0;
    j->marcasAnalise = 0;
    j->recargaBotnet = 0.0f;
    j->bonusPosEsquiva = 0.0f;
    j->recargaEsquiva2 = 0.0f;
    j->recargaExplosao = 0.0f;
    j->recargaRastro = 0.0f;
    j->contaFluxo = 0;
    j->katanaAnim = 0.0f;
    j->trocaArma = 0.0f;
}

// A build da tentativa (melhorias e penalidades) some ao morrer e ao começar outra tentativa
void RogueLimparBuild(JogadorRogue *j)
{
    memset(j->up, 0, sizeof(j->up));
    j->rerolls = REROLL_GRATIS;
    j->ultimoUp = -1;
    j->penalidade = 0;
    j->abatesEscudo = 0;
}

// Vidas que ainda restam na partida (as mortes ficam no save: carregar o jogo não devolve vidas)
int RogueVidasRestantes(const Biblioteca *b)
{
    int restam = LIMITE_MORTES - b->perm->mortes;
    return (restam > 0) ? restam : 0;
}

// Perdeu a tentativa (ou terminou a Biblioteca): a mochila, a arma extra, a build e o Selo da Inferência ficam para trás
void RogueEsvaziarMochila(JogadorRogue *j)
{
    memset(j->itens, 0, sizeof(j->itens));
    j->seloInferencia = false;
    j->seloAmplificado = false;
    j->armaBase = ARMA_DISPARADOR;
    j->armaExtra = ARMA_DISPARADOR;
    j->usandoExtra = false;
    RogueLimparEfeitos(j);
    RogueLimparBuild(j);
}
