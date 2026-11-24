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

// ---------- O SILOGISTA ----------

static bool OperarLogica(OperadorLogico op, bool a, bool b)
{
    switch (op) {
        case OP_E:   return a && b;
        case OP_OU:  return a || b;
        case OP_NAO: return !a;
        case OP_SE:  return !a || b;
        default:     return a == b;     // OP_SSE
    }
}

int EntradasPorta(const PortaLogica *p)
{
    return (p->op == OP_NAO) ? 1 : 2;
}

// Valor de um sinal do circuito (0 = p, 1 = q, 2+k = saída da porta k).
// Devolve false se o sinal ainda não chegou (porta não ativada).
bool SinalCircuito(const Silogista *s, int sinal, bool *valor)
{
    if (sinal < 2) {
        *valor = s->valor[sinal];
        return true;
    }

    *valor = s->saida[sinal - 2];
    return s->ativada[sinal - 2];
}

// Resultado da expressão inteira para uns valores de p e q (as portas já estão em ordem)
static bool ResultadoCircuito(const CircuitoDef *c, const bool valor[2])
{
    bool saida[MAX_PORTAS_CIRCUITO] = { false };

    for (int k = 0; k < c->numPortas; k++) {
        const PortaLogica *p = &c->porta[k];
        bool e[2];
        for (int i = 0; i < 2; i++)
            e[i] = (p->entrada[i] < 2) ? valor[p->entrada[i]] : saida[p->entrada[i] - 2];
        saida[k] = OperarLogica(p->op, e[0], e[1]);
    }

    return saida[c->numPortas - 1];
}

// Posição de um elemento do circuito (0 = p, 1 = q, 2+k = porta k)
Vector2 SilPosElemento(int e)
{
    const CircuitoDef *c = &CIRCUITO_SILOGISTA;
    return (e < 2) ? c->posTerminal[e] : c->porta[e - 2].pos;
}

int SilNumElementos(void)
{
    return 2 + CIRCUITO_SILOGISTA.numPortas;
}

// Os fragmentos vão para lugares sorteados da arena (nenhum fica encaixado)
static void SilEspalharPecas(Silogista *s)
{
    int ordem[NUM_PONTOS_PECAS_SIL];

    for (int i = 0; i < NUM_PONTOS_PECAS_SIL; i++)
        ordem[i] = i;

    for (int i = NUM_PONTOS_PECAS_SIL - 1; i > 0; i--) {
        int k = GetRandomValue(0, i);
        int t = ordem[i]; ordem[i] = ordem[k]; ordem[k] = t;
    }

    for (int k = 0; k < NUM_ENCAIXES; k++) {
        s->posPeca[k] = PONTOS_PECAS_SIL[ordem[k]];
        s->encaixe[k] = -1;
    }

    s->carregando = -1;
}

bool SilPecaNoChao(const Silogista *s, int k)
{
    if (s->carregando == k)
        return false;

    for (int e = 0; e < NUM_ENCAIXES; e++)
        if (s->encaixe[e] == k)
            return false;

    return true;
}

// Rodada 1: o núcleo aceita o resultado com os valores dados (é só calcular na ordem).
// Rodada 2: o núcleo exige o contrário, e o jogador escolhe p e q.
void SilPrepararRodada(Silogista *s, int rodada)
{
    const CircuitoDef *c = &CIRCUITO_SILOGISTA;

    s->rodada = rodada;
    s->valor[0] = c->inicial[0];
    s->valor[1] = c->inicial[1];
    memset(s->ativada, 0, sizeof(s->ativada));
    memset(s->travado, 0, sizeof(s->travado));

    bool resultado = ResultadoCircuito(c, s->valor);
    s->exigido = (rodada == 1) ? resultado : !resultado;
}

void RogueIniciarSilogista(Biblioteca *b)
{
    Silogista *s = &b->silogista;
    char letras[NUM_ENCAIXES] = { 'X', 'Y', 'Z' };

    *s = (Silogista){0};
    s->ativo = true;
    s->etapa = SIL_MONTAGEM;
    s->hp = SIL_HP;
    s->pos = (Vector2){ 400, 190 };
    s->proximoAtaque = 2.0f;

    // As letras são sorteadas: a ordem da lista não entrega a resposta
    for (int i = NUM_ENCAIXES - 1; i > 0; i--) {
        int k = GetRandomValue(0, i);
        char t = letras[i]; letras[i] = letras[k]; letras[k] = t;
    }

    for (int k = 0; k < NUM_ENCAIXES; k++)
        s->letraPeca[k] = letras[k];

    SilEspalharPecas(s);
}

// E perto de um fragmento no chão: pega (se já estiver com outro, troca)
void SilPegarPeca(Biblioteca *b, int k)
{
    Silogista *s = &b->silogista;

    if (s->carregando >= 0)
        s->posPeca[s->carregando] = b->j.pos;

    s->carregando = k;
    TocarSomTom(sfxPegar, 1.1f);
}

// E perto de um mecanismo: encaixa o que está na mão e pega o que estava lá (se houver)
void SilUsarEncaixe(Biblioteca *b, int e)
{
    Silogista *s = &b->silogista;
    int naMao = s->carregando;

    s->carregando = s->encaixe[e];
    s->encaixe[e] = naMao;

    if (naMao >= 0) {
        RogueParticulas(b, POS_ENCAIXES[e], (Color){ 0, 228, 255, 255 }, 10, 120);
        TocarSomTom(sfxSelecao, 1.2f);
    } else {
        TocarSomTom(sfxPegar, 1.1f);
    }
}

// ----- Ataques (todos com aviso) -----

static AtaqueSil *SilNovoAtaque(Silogista *s, TipoAtaqueSil tipo, float aviso)
{
    for (int i = 0; i < MAX_ATAQUES_SIL; i++) {

        if (s->atq[i].ativo)
            continue;

        s->atq[i] = (AtaqueSil){0};
        s->atq[i].ativo = true;
        s->atq[i].tipo = tipo;
        s->atq[i].aviso = aviso;
        return &s->atq[i];
    }

    return NULL;
}

// Linhas: quantas balas cabem e onde nasce a bala i
int SilBalasLinha(const AtaqueSil *a)
{
    float comprimento = (a->dir.y != 0.0f) ? ArenaRogue().width : ArenaRogue().height;
    return (int)(comprimento / SIL_LINHA_ESPACO);
}

Vector2 SilPontoLinha(const AtaqueSil *a, int i)
{
    float t = ((float)i + 0.5f) * SIL_LINHA_ESPACO;

    if (a->dir.y != 0.0f)                       // linha deitada, descendo do alto
        return (Vector2){ ArenaRogue().x + t, ArenaRogue().y + 8.0f };

    float x = (a->dir.x > 0.0f) ? ArenaRogue().x + 8.0f : ArenaRogue().x + ArenaRogue().width - 8.0f;
    return (Vector2){ x, ArenaRogue().y + t };
}

// Errou um puzzle: anel de balas (com aviso)
static void SilAtaqueAnel(Silogista *s)
{
    SilNovoAtaque(s, ATQ_ANEL, 0.7f);
}

static void SilComecarAtaque(Biblioteca *b)
{
    static const TipoAtaqueSil MONTAGEM[] = { ATQ_LINHAS, ATQ_ZONA, ATQ_REFUTACAO };
    static const TipoAtaqueSil SOFISMA[]  = { ATQ_LINHAS, ATQ_REFUTACAO, ATQ_ZONA };
    static const TipoAtaqueSil CIRCUITO[] = { ATQ_ZONA, ATQ_LINHAS, ATQ_CORTE };
    static const TipoAtaqueSil FINAL[]    = { ATQ_LINHAS, ATQ_REFUTACAO, ATQ_ZONA, ATQ_ANEL };

    Silogista *s = &b->silogista;
    Vector2 jp = b->j.pos;
    int r = s->revezamento++;
    TipoAtaqueSil tipo;

    switch (s->etapa) {
        // Carregando um fragmento, ele tenta derrubar o portador (um ataque sim, outro não)
        case SIL_MONTAGEM: tipo = (s->carregando >= 0 && r % 2 == 0) ? ATQ_REFUTACAO : MONTAGEM[r % 3]; break;
        case SIL_SOFISMA:  tipo = SOFISMA[r % 3];  break;
        case SIL_CIRCUITO: tipo = CIRCUITO[r % 3]; break;
        default:           tipo = FINAL[r % 4];    break;
    }

    // Corte: trava um terminal ou porta que ainda não foi resolvida (se não houver, vira zona)
    int candidatos[MAX_ELEMENTOS_CIRCUITO], n = 0;

    if (tipo == ATQ_CORTE) {
        for (int e = 0; e < SilNumElementos(); e++) {
            bool resolvido = (e < 2) ? (s->rodada == 1) : s->ativada[e - 2];
            if (!resolvido && s->travado[e] <= 0.0f)
                candidatos[n++] = e;
        }
        if (n == 0)
            tipo = ATQ_ZONA;
    }

    AtaqueSil *a = SilNovoAtaque(s, tipo, SIL_AVISO);

    if (a == NULL)
        return;

    switch (tipo) {

        case ATQ_LINHAS: {
            // De cima, da esquerda ou da direita, com a brecha perto do jogador (dá para alcançar)
            int lado = GetRandomValue(0, 2);
            a->dir = (lado == 0) ? (Vector2){ 0, 1 } : ((lado == 1) ? (Vector2){ 1, 0 } : (Vector2){ -1, 0 });

            float ref = (a->dir.y != 0.0f) ? jp.x - ArenaRogue().x : jp.y - ArenaRogue().y;
            int balas = SilBalasLinha(a);
            int meio = (int)(ref / SIL_LINHA_ESPACO) + GetRandomValue(-4, 4);
            a->brecha = (int)Clamp((float)(meio - SIL_LINHA_BRECHA / 2), 0.0f, (float)(balas - SIL_LINHA_BRECHA));
        } break;

        case ATQ_ZONA: {
            // Zona de bloqueio em volta do mecanismo (ou elemento do circuito) mais perto do jogador
            a->pos = jp;
            a->duracao = SIL_ZONA_DURACAO;

            float menor = 200.0f;
            if (s->etapa == SIL_MONTAGEM || s->etapa == SIL_SOFISMA) {
                for (int e = 0; e < NUM_ENCAIXES; e++)
                    if (Vector2Distance(jp, POS_ENCAIXES[e]) < menor) { menor = Vector2Distance(jp, POS_ENCAIXES[e]); a->pos = POS_ENCAIXES[e]; }
            } else if (s->etapa == SIL_CIRCUITO) {
                for (int e = 0; e < SilNumElementos(); e++)
                    if (Vector2Distance(jp, SilPosElemento(e)) < menor) { menor = Vector2Distance(jp, SilPosElemento(e)); a->pos = SilPosElemento(e); }
            }
        } break;

        case ATQ_REFUTACAO:
            a->dir = Vector2Normalize(Vector2Subtract(jp, s->pos));
            a->tiros = 6;
            TocarGrupo(&gChefeMira, 0.8f / SIL_AVISO, 0.7f);    // trava junto com o corredor

        case ATQ_CORTE:
            a->alvo = candidatos[GetRandomValue(0, n - 1)];
            a->aviso = 0.8f;
            break;

        default:
            break;
    }
}

static void SilAtualizarAtaques(Biblioteca *b, float dt)
{
    Silogista *s = &b->silogista;
    Vector2 jp = b->j.pos;

    for (int i = 0; i < MAX_ATAQUES_SIL; i++) {

        AtaqueSil *a = &s->atq[i];

        if (!a->ativo)
            continue;

        // Aviso: a refutação segue o jogador até pouco antes de disparar
        if (a->aviso > 0.0f) {

            a->aviso -= dt;

            if (a->tipo == ATQ_REFUTACAO && a->aviso > 0.25f)
                a->dir = Vector2Normalize(Vector2Subtract(jp, s->pos));

            if (a->aviso > 0.0f)
                continue;

            switch (a->tipo) {

                case ATQ_LINHAS:
                    for (int k = 0; k < SilBalasLinha(a); k++)
                        if (k < a->brecha || k >= a->brecha + SIL_LINHA_BRECHA)
                            RogueDisparar(b, SilPontoLinha(a, k), a->dir, SIL_TIRO_VEL, SIL_TIRO_DANO, 6.0f, false);
                    a->ativo = false;
                    TocarGrupo(&gSilLinhas, 1.0f, 0.85f);
                    break;

                case ATQ_ZONA:
                    RogueAnel(b, a->pos, SIL_ZONA_RAIO + 10.0f, RED);
                    TocarGrupo(&gIniBarreira, 0.8f, 0.75f);
                    break;

                case ATQ_CORTE:
                    s->travado[a->alvo] = SIL_CORTE_DURACAO;
                    RogueParticulas(b, SilPosElemento(a->alvo), PURPLE, 14, 160);
                    TocarGrupo(&gSilCorte, 1.0f, 0.85f);
                    a->ativo = false;
                    break;

                case ATQ_ANEL:
                    for (int k = 0; k < 16; k++) {
                        float ang = (2.0f * PI / 16.0f) * k + s->anim;
                        RogueDisparar(b, s->pos, (Vector2){ cosf(ang), sinf(ang) }, SIL_TIRO_VEL, SIL_TIRO_DANO, 6.0f, false);
                    }
                    RogueParticulas(b, s->pos, PURPLE, 24, 240);
                    RogueAnel(b, s->pos, 90.0f, PURPLE);
                    TocarGrupo(&gIniAnel, 0.75f, 0.9f);
                    a->ativo = false;
                    break;

                default:    // ATQ_REFUTACAO: a rajada começa
                    a->cronoTiro = 0.0f;
                    break;
            }
            continue;
        }

        if (a->tipo == ATQ_ZONA) {
            a->duracao -= dt;
            if (Vector2Distance(jp, a->pos) < SIL_ZONA_RAIO + RAIO_CORPO_JOGADOR * 0.5f)
                RogueFerirJogador(b, SIL_TIRO_DANO);
            if (a->duracao <= 0.0f)
                a->ativo = false;
        }
        else if (a->tipo == ATQ_REFUTACAO) {
            a->cronoTiro -= dt;
            if (a->cronoTiro <= 0.0f && a->tiros > 0) {
                Vector2 cano = Vector2Add(s->pos, Vector2Scale(a->dir, SIL_RAIO));
                RogueDisparar(b, cano, a->dir, 360.0f, SIL_TIRO_DANO, 5.0f, false);
                a->tiros--;
                a->cronoTiro = 0.07f;
                TocarGrupo(&gIniTecla, 0.8f, 0.7f);
            }
            if (a->tiros <= 0)
                a->ativo = false;
        }
    }
}

// Entre um puzzle e outro: limpa as balas e os ataques e abre a explicação
static void SilPausaParaExplicar(Biblioteca *b, const Dialogo *dialogo)
{
    Silogista *s = &b->silogista;

    memset(s->atq, 0, sizeof(s->atq));
    for (int i = 0; i < MAX_PROJETEIS; i++)
        if (!b->proj[i].doJogador) b->proj[i].ativo = false;

    s->proximoAtaque = 2.5f;
    b->conversaPendente = dialogo;
    b->objConversa = OBJ_SILOGISTA_ETAPA;
    b->pausaImpacto = 0.15f;
}

// Um puzzle resolvido: cai uma camada da armadura lógica
static void SilConcluirEtapa(Biblioteca *b, ProgressoCorredor *prog, EtapaSilogista etapa, const Dialogo *dialogo)
{
    Silogista *s = &b->silogista;

    Telemetria("rogue_silogista", TextFormat("etapa %d", (int)s->etapa + 1), (float)(GetTime() - b->inicio), b->perm->tentativas);
    RogueRegistrarResposta(b, true);

    s->etapa = etapa;
    s->carregando = -1;

    if (etapa == SIL_SOFISMA) {
        for (int k = 0; k < NUM_ENCAIXES; k++)
            s->hpPeca[k] = SIL_HP_PECA;
        s->blindagem = 0.0f;
    }
    else if (etapa == SIL_CIRCUITO) {
        SilPrepararRodada(s, 1);
    }

    AlterarPontos(prog, PONTOS_ETAPA_SILOGISTA);
    RogueParticulas(b, s->pos, (Color){ 0, 228, 255, 255 }, 30, 240);
    RogueAnel(b, s->pos, 110.0f, (Color){ 0, 228, 255, 255 });
    TocarSom(sfxAcerto);
    TocarGrupo(&gChefeEscudo, 1.05f, 0.8f);          // uma camada da armadura caiu
    MostrarEmocaoLuci(EMOCAO_ALEGRE, 2.0f);
    SilPausaParaExplicar(b, dialogo);
}

// Errou um puzzle: ataque especial e explicação da Luci
static void SilErrou(Biblioteca *b, ProgressoCorredor *prog, const char *aviso, const char *fala, const char *detalhe)
{
    Silogista *s = &b->silogista;

    s->erro = 1.2f;
    SilAtaqueAnel(s);
    AlterarPontos(prog, -PENALIDADE_ERRO);
    RogueRegistrarResposta(b, false);
    RogueAviso(b, aviso);
    b->falaPendente = fala;
    TocarSom(sfxErro);
    MostrarEmocaoLuci(EMOCAO_DUVIDA, 2.0f);
    Telemetria("rogue_silogista_erro", detalhe, (float)(GetTime() - b->inicio), (int)s->etapa + 1);
}

// Montagem: com os três mecanismos cheios, o argumento é conferido
static void SilConferirMontagem(Biblioteca *b, ProgressoCorredor *prog)
{
    Silogista *s = &b->silogista;

    for (int e = 0; e < NUM_ENCAIXES; e++)
        if (s->encaixe[e] < 0)
            return;

    bool certo = true;
    for (int e = 0; e < NUM_ENCAIXES; e++)
        if (s->encaixe[e] != e)
            certo = false;

    if (certo) {
        SilConcluirEtapa(b, prog, SIL_SOFISMA, &dialogoSilogistaSofisma);
        return;
    }

    // A conclusão errada ou as premissas trocadas: os fragmentos se espalham de novo
    const char *fala = (s->encaixe[ENCAIXE_C] != ENCAIXE_C) ? FALA_MONTAGEM_CONCLUSAO : FALA_MONTAGEM_PREMISSAS;

    for (int e = 0; e < NUM_ENCAIXES; e++)
        RogueParticulas(b, POS_ENCAIXES[e], RED, 10, 160);

    SilEspalharPecas(s);
    SilErrou(b, prog, "MONTAGEM INVÁLIDA! Os fragmentos se espalham.", fala,
             (s->encaixe[ENCAIXE_C] != ENCAIXE_C) ? "montagem_conclusao" : "montagem_premissas");
}

// Sofisma: um tiro numa das peças do argumento corrompido
static void SilAcertarPecaSofisma(Biblioteca *b, ProgressoCorredor *prog, int k, float dano)
{
    Silogista *s = &b->silogista;

    if (s->blindagem > 0.0f) {
        RogueParticulas(b, POS_ENCAIXES[k], (Color){ 0, 228, 255, 255 }, 3, 90);
        return;
    }

    s->hpPeca[k] -= dano;
    s->tremePeca[k] = 0.15f;
    RogueParticulas(b, POS_ENCAIXES[k], MAGENTA, 4, 120);

    if (s->hpPeca[k] > 0.0f)
        return;

    RogueParticulas(b, POS_ENCAIXES[k], MAGENTA, 30, 240);

    if (k == SOFISMA_SILOGISTA.invalida) {
        SilConcluirEtapa(b, prog, SIL_CIRCUITO, &dialogoSilogistaCircuito);
        return;
    }

    // Destruiu uma premissa: a defesa se recupera (peças inteiras e blindadas por um tempo)
    for (int i = 0; i < NUM_ENCAIXES; i++)
        s->hpPeca[i] = SIL_HP_PECA;
    s->blindagem = SIL_BLINDAGEM;

    SilErrou(b, prog, "A DEFESA DO SILOGISTA SE RECUPEROU!", FALA_SOFISMA_ERRO, "sofisma");
}

// A última porta ligou o núcleo: confere a saída
static void SilNucleo(Biblioteca *b, ProgressoCorredor *prog)
{
    Silogista *s = &b->silogista;
    const CircuitoDef *c = &CIRCUITO_SILOGISTA;

    if (s->saida[c->numPortas - 1] != s->exigido) {
        memset(s->ativada, 0, sizeof(s->ativada));      // o circuito reinicia
        SilErrou(b, prog, "SAÍDA ERRADA! O circuito reinicia.", c->dicaErro, "circuito");
        return;
    }

    s->nucleo = 1.0f;
    RogueParticulas(b, c->posNucleo, (Color){ 0, 228, 255, 255 }, 30, 220);

    if (s->rodada == 1) {
        // A rodada 2 começa quando a explicação terminar (RogueFimConversa)
        AlterarPontos(prog, PONTOS_ETAPA_SILOGISTA);
        TocarSom(sfxAcerto);
        SilPausaParaExplicar(b, &dialogoSilogistaRodada2);
        Telemetria("rogue_silogista", "rodada 1", (float)(GetTime() - b->inicio), b->perm->tentativas);
    } else {
        SilConcluirEtapa(b, prog, SIL_VULNERAVEL, &dialogoSilogistaVulneravel);
    }
}

// Circuito: um tiro num terminal (troca V/F) ou numa porta (ativa a operação)
static void SilAcertarElemento(Biblioteca *b, ProgressoCorredor *prog, int e)
{
    Silogista *s = &b->silogista;
    const CircuitoDef *c = &CIRCUITO_SILOGISTA;
    Vector2 pos = SilPosElemento(e);

    if (s->recarga[e] > 0.0f)
        return;

    s->recarga[e] = SIL_RECARGA_ELEMENTO;

    if (s->travado[e] > 0.0f) {
        RogueParticulas(b, pos, PURPLE, 6, 100);
        RogueAviso(b, "TRAVADO! Espere o corte do Silogista passar.");
        TocarSomTom(sfxErro, 1.5f);
        return;
    }

    if (e < 2) {
        if (s->rodada == 1) {
            RogueAviso(b, "Nesta rodada p e q estão fixos: ative as portas!");
            TocarSomTom(sfxSelecao, 0.7f);
            return;
        }

        s->valor[e] = !s->valor[e];
        memset(s->ativada, 0, sizeof(s->ativada));      // o sinal mudou: as portas apagam
        RogueParticulas(b, pos, s->valor[e] ? SKYBLUE : PURPLE, 10, 120);
        TocarSomTom(sfxSelecao, s->valor[e] ? 1.3f : 0.9f);
        return;
    }

    int k = e - 2;
    const PortaLogica *p = &c->porta[k];
    bool entrada[2] = { false, false };

    if (s->ativada[k])
        return;

    // Precedência: a porta só funciona com as entradas já calculadas
    for (int i = 0; i < EntradasPorta(p); i++) {
        if (!SinalCircuito(s, p->entrada[i], &entrada[i])) {
            s->erroElemento[e] = 0.6f;
            RogueAviso(b, "SEM SINAL! Ative antes as portas que chegam nesta.");
            TocarSomTom(sfxErro, 1.4f);
            return;
        }
    }

    s->saida[k] = OperarLogica(p->op, entrada[0], entrada[1]);
    s->ativada[k] = true;
    RogueParticulas(b, p->pos, s->saida[k] ? SKYBLUE : PURPLE, 14, 160);
    TocarSomTom(sfxAcerto, 1.4f);

    if (k == c->numPortas - 1)
        SilNucleo(b, prog);
}

// Tiro do jogador na Câmara da Inferência. Devolve true se o tiro foi usado.
// =============================================================
// O SILOGISTA - VERSÃO BULLET-HELL LÓGICO (SILOGISTA_MODO_BULLET_HELL)
//   3 rodadas, uma por camada da armadura (Modus Ponens, Afirmação do
//   Consequente, Modus Tollens). As premissas ficam na faixa do alto; o
//   Silogista lança CONCLUSÕES curtas em anéis, espirais e cercos. O jogador
//   atira (ou dá parry) na conclusão válida: ela confirma a regra e desfaz as
//   falácias da tela. Atingir uma falácia ou ser atingido por ela custa EP
//   (a estabilidade mental do Thomas) e mostra a justificativa. Sem armadura,
//   o Silogista vira um bullet-hell comum até cair.
// =============================================================

static void ExibirFeedbackPedagogico(Silogista *s, const char *texto, Color cor)
{
    snprintf(s->bh.mensagemFeedback, sizeof(s->bh.mensagemFeedback), "%s", texto);
    s->bh.corFeedback = cor;
    s->bh.tempoExibicaoFeedback = 2.5f;             // a explicação fica 2,5 s na tela
}

void RogueIniciarSilogistaBH(Biblioteca *b)
{
    Silogista *s = &b->silogista;

    *s = (Silogista){0};
    s->ativo = true;
    s->modoBH = true;
    for (int k = 0; k < 3; k++) g_varianteSilBH[k] = GetRandomValue(0, 2);      // cada rodada sorteia uma das 3 versões
    s->etapa = SIL_MONTAGEM;            // as 3 camadas da armadura usam as etapas (SIL_VULNERAVEL = sem armadura)
    s->hp = SIL_HP;
    s->pos = (Vector2){ 400, 170 };
    s->carregando = -1;
    s->bh.fatorTempo = 1.0f;
    s->bh.proximoPadrao = 2.0f;
}

// Fator do tempo do mundo (Focus Mode): os projéteis e inimigos andam com dt * fator
float RogueFatorTempoMundo(const Biblioteca *b)
{
    const Silogista *s = &b->silogista;
    return (b->tipoSala == SALA_SILOGISTA && s->ativo && s->modoBH && s->bh.modoFocoAtivo) ? SIL_BH_FATOR_FOCO : 1.0f;
}

// papel: 0 = conclusão válida, 1 = falácia, 2 = premissa
static void SilBHCriarSimbolo(Biblioteca *b, Vector2 pos, Vector2 vel, int papel, float surgindo)
{
    Silogista *s = &b->silogista;
    const RodadaSilogistaBH *r = RodadaBH(s->bh.rodada);
    ProjetilSilogista *q = NULL;

    for (int k = 0; k < MAX_PROJETEIS_SILOGISTA && q == NULL; k++)
        if (!s->bh.projeteis[k].ativo) q = &s->bh.projeteis[k];
    if (q == NULL)
        return;

    *q = (ProjetilSilogista){0};
    q->ativo = true;
    q->posicao = pos;
    q->velocidade = vel;
    q->raio = SIL_BH_RAIO_SIMBOLO;
    q->vida = 9.0f;
    q->surgindo = surgindo;

    if (papel == 0) {
        int i = GetRandomValue(0, 1);
        snprintf(q->textoSimbolo, sizeof(q->textoSimbolo), "%s", r->validos[i][0]);
        q->explicacaoPedagogica = r->validos[i][1];
        q->justificativa = r->regraAcerto;
        q->tipo = PROJETIL_LOGICO_VALIDO;
        q->ehRespostaCorreta = true;
    } else if (papel == 1) {
        int i = GetRandomValue(0, 2);
        snprintf(q->textoSimbolo, sizeof(q->textoSimbolo), "%s", r->falacias[i][0]);
        q->explicacaoPedagogica = r->falacias[i][1];
        q->justificativa = r->falacias[i][2];
        q->tipo = PROJETIL_LOGICO_FALACIA;
    } else {
        snprintf(q->textoSimbolo, sizeof(q->textoSimbolo), "%s", r->premissa[0]);
        q->explicacaoPedagogica = r->premissa[1];
        q->justificativa = "Premissa: ela foi DADA como verdadeira. O que está em jogo é a conclusão.";
        q->tipo = PROJETIL_LOGICO_PREMISSA;
    }
}

// Começa um padrão (ANEL radial, ESPIRAL ou CERCO circular em volta do jogador)
static void SilBHComecarPadrao(Biblioteca *b)
{
    Silogista *s = &b->silogista;
    JogadorRogue *j = &b->j;
    int padrao = s->bh.padrao % 3;
    s->bh.padrao++;

    if (padrao == 0) {
        // ANEL: 8 conclusões saindo do Silogista (2 válidas, uma premissa) + um anel de balas comuns
        int n = 8, v1 = GetRandomValue(0, n - 1), v2 = (v1 + n / 2) % n, pr = (v1 + 2) % n;
        for (int i = 0; i < n; i++) {
            float a = (float)i * 2.0f * PI / (float)n + s->anim * 0.5f;
            Vector2 d = { cosf(a), sinf(a) };
            SilBHCriarSimbolo(b, Vector2Add(s->pos, Vector2Scale(d, 40.0f)), Vector2Scale(d, 85.0f),
                              (i == v1 || i == v2) ? 0 : ((i == pr) ? 2 : 1), 0.0f);
        }
        for (int i = 0; i < 12; i++) {
            float a = (float)i * 2.0f * PI / 12.0f + PI / 12.0f + s->anim * 0.5f;
            RogueDisparar(b, s->pos, (Vector2){ cosf(a), sinf(a) }, SIL_TIRO_VEL * 0.75f, SIL_TIRO_DANO, 5.0f, false);
        }
        RogueAnel(b, s->pos, 70.0f, PURPLE);
        TocarGrupo(&gIniAnel, 0.85f, 0.8f);
    }
    else if (padrao == 1) {
        // ESPIRAL: um símbolo de cada vez, girando (ver RogueAtualizarSilogistaBH)
        s->bh.emissoesRestantes = 10;
        s->bh.cronoEmissao = 0.0f;
        s->bh.anguloEmissao = (float)GetRandomValue(0, 359) * DEG2RAD;
    }
    else {
        // CERCO: um círculo de conclusões em volta do jogador, girando e se fechando devagar
        int n = 8, v = GetRandomValue(0, n - 1);
        float giro = (GetRandomValue(0, 1) == 0) ? 1.0f : -1.0f;
        for (int i = 0; i < n; i++) {
            float a = (float)i * 2.0f * PI / (float)n;
            Vector2 d = { cosf(a), sinf(a) };
            Vector2 p = Vector2Add(j->pos, Vector2Scale(d, 230.0f));
            if (!CheckCollisionPointRec(p, (Rectangle){ ArenaRogue().x + 20, ArenaRogue().y + 20, ArenaRogue().width - 40, ArenaRogue().height - 40 }))
                continue;
            Vector2 tangente = { -d.y * giro, d.x * giro };
            SilBHCriarSimbolo(b, p, Vector2Add(Vector2Scale(d, -55.0f), Vector2Scale(tangente, 22.0f)),
                              (i == v) ? 0 : ((i == (v + 4) % n) ? 2 : 1), 0.6f);
        }
        TocarGrupo(&gChefeInvoca, 0.9f, 0.65f);          // o cerco se formando
    }

    s->bh.proximoPadrao = 3.4f - 0.3f * (float)s->bh.rodada;
}

// Erro lógico: atingiu a conclusão errada ou foi atingido por ela
static void SilBHErro(Biblioteca *b, ProgressoCorredor *prog, const ProjetilSilogista *q, bool foiAtingido)
{
    Silogista *s = &b->silogista;
    JogadorRogue *j = &b->j;

    ExibirFeedbackPedagogico(s, q->justificativa, (Color){ 230, 41, 55, 255 });
    j->ep = fmaxf(0.0f, j->ep - SIL_BH_PENALIDADE_EP);          // a estabilidade mental do Thomas cai
    RogueTextoFlutuante(b, (Vector2){ q->posicao.x, q->posicao.y - 24 }, TextFormat("ERRO LÓGICO  -%d EP", (int)SIL_BH_PENALIDADE_EP), RED);
    RogueParticulas(b, q->posicao, RED, 14, 200);

    // Atirou na errada: ela se parte em balas
    if (!foiAtingido)
        for (int i = 0; i < 4; i++) {
            float a = (float)i * PI / 2.0f + PI / 4.0f;
            RogueDisparar(b, q->posicao, (Vector2){ cosf(a), sinf(a) }, SIL_TIRO_VEL * 0.8f, SIL_TIRO_DANO * 0.7f, 5.0f, false);
        }

    // Erros seguidos contam uma vez só (no índice e nos pontos)
    if (s->bh.recargaErro <= 0.0f) {
        s->bh.recargaErro = 1.5f;
        RogueRegistrarResposta(b, false);
        AlterarPontos(prog, -2);
    }

    TocarSom(sfxErro);
    MostrarEmocaoLuci(EMOCAO_DUVIDA, 1.5f);
    Telemetria("rogue_silogista_bh_erro", q->textoSimbolo, (float)s->bh.rodada + 1.0f, foiAtingido ? 1 : 0);
}

// Uma camada da armadura cai: explicação curta (pausa) e a próxima rodada
static void SilBHConcluirRodada(Biblioteca *b, ProgressoCorredor *prog)
{
    Silogista *s = &b->silogista;
    int feita = s->bh.rodada;

    s->bh.acertosRodada = 0;
    s->bh.emissoesRestantes = 0;
    s->bh.proximoPadrao = 2.0f;
    memset(s->bh.projeteis, 0, sizeof(s->bh.projeteis));
    for (int i = 0; i < MAX_PROJETEIS; i++)
        if (!b->proj[i].doJogador) b->proj[i].ativo = false;

    s->etapa = (EtapaSilogista)((int)s->etapa + 1);
    s->bh.rodada++;

    RogueRegistrarResposta(b, true);
    AlterarPontos(prog, PONTOS_ETAPA_SILOGISTA);
    RogueParticulas(b, s->pos, (Color){ 0, 228, 255, 255 }, 30, 240);
    RogueAnel(b, s->pos, 110.0f, (Color){ 0, 228, 255, 255 });
    TocarSom(sfxAcerto);
    TocarGrupo(&gChefeEscudo, 1.05f, 0.8f);          // uma camada da armadura caiu
    MostrarEmocaoLuci(EMOCAO_ALEGRE, 2.0f);

    b->conversaPendente = DialogoRodadaBH(feita);
    b->objConversa = OBJ_NENHUM;
    b->pausaImpacto = 0.15f;
    Telemetria("rogue_silogista_bh", RODADAS_SILOGISTA_BH[feita].titulo, (float)(GetTime() - b->inicio), b->perm->tentativas);
}

// Escolheu a conclusão k (tiro ou parry)
static void SilBHEscolher(Biblioteca *b, ProgressoCorredor *prog, int k, bool rebatido)
{
    Silogista *s = &b->silogista;
    ProjetilSilogista q = s->bh.projeteis[k];

    s->bh.projeteis[k].ativo = false;

    if (!q.ehRespostaCorreta) {
        SilBHErro(b, prog, &q, false);
        return;
    }

    // Conclusão válida: o micro-feedback verde explica a regra
    s->bh.acertosRodada++;
    ExibirFeedbackPedagogico(s, q.justificativa, (Color){ 0, 228, 48, 255 });
    RogueTextoFlutuante(b, (Vector2){ q.posicao.x, q.posicao.y - 26 }, TextFormat("%s  %d/%d", q.textoSimbolo, s->bh.acertosRodada, SIL_BH_META), LIME);
    RogueParticulas(b, q.posicao, LIME, 24, 240);
    RogueAnel(b, q.posicao, 60.0f, LIME);

    // ... e desfaz as falácias e premissas que estavam na tela
    for (int i = 0; i < MAX_PROJETEIS_SILOGISTA; i++) {
        ProjetilSilogista *o = &s->bh.projeteis[i];
        if (!o->ativo || o->ehRespostaCorreta) continue;
        RogueParticulas(b, o->posicao, (Color){ 170, 140, 255, 255 }, 6, 140);
        o->ativo = false;
    }

    // Rebatida: a conclusão volta voando para o Silogista (só o efeito; a armadura segura)
    if (rebatido) {
        Vector2 d = Vector2Subtract(s->pos, q.posicao);
        d = (Vector2Length(d) > 0.1f) ? Vector2Normalize(d) : (Vector2){ 0, -1 };
        Projetil *p = RogueDisparar(b, q.posicao, d, 560.0f, 0.0f, 7.0f, true);
        if (p != NULL) { p->secundario = true; p->arma = ARMA_DISPARADOR; }
    }

    s->flash = 0.1f;
    TocarSom(sfxAcerto);
    TocarSomTom(sfxDedutivo, 1.4f);

    if (s->bh.acertosRodada >= SIL_BH_META)
        SilBHConcluirRodada(b, prog);
}

// Tiro do jogador na Câmara (bullet-hell): primeiro os símbolos, depois o corpo do Silogista
static bool RogueSilogistaAcertoBH(Biblioteca *b, ProgressoCorredor *prog, Projetil *p)
{
    Silogista *s = &b->silogista;

    if (!s->ativo || !s->apresentado || s->morrendo > 0.0f)
        return false;

    if (s->etapa != SIL_VULNERAVEL && p->dano > 0.0f)       // dano 0 = a conclusão rebatida (só o efeito)
        for (int k = 0; k < MAX_PROJETEIS_SILOGISTA; k++) {
            const ProjetilSilogista *q = &s->bh.projeteis[k];
            if (!q->ativo || q->surgindo > 0.0f || Vector2Distance(p->pos, q->posicao) > q->raio + p->raio) continue;
            p->ativo = false;
            SilBHEscolher(b, prog, k, false);
            return true;
        }

    if (Vector2Distance(p->pos, s->pos) < SIL_RAIO + p->raio) {
        p->ativo = false;
        if (RogueFerirSilogista(b, p->dano, p->pos) && p->alvoDedutivo == 0)
            RogueMarcarDedutivo(b, &s->marca, &s->tempoMarca, -1, s->pos);
        return true;
    }

    return false;
}

static void RogueAtualizarSilogistaBH(Biblioteca *b, ProgressoCorredor *prog, float dt)
{
    Silogista *s = &b->silogista;
    JogadorRogue *j = &b->j;
    EstadoSilogista *e = &s->bh;

    if (e->tempoExibicaoFeedback > 0.0f) e->tempoExibicaoFeedback -= dt;
    if (e->recargaErro > 0.0f)           e->recargaErro -= dt;

    // Flutua no alto (sem armadura, também sobe e desce)
    Vector2 alvo = { 400.0f + sinf(s->anim * 0.5f) * 200.0f,
                     (s->etapa == SIL_VULNERAVEL) ? 200.0f + sinf(s->anim * 0.9f) * 70.0f : 165.0f };
    s->pos = Vector2Lerp(s->pos, alvo, fminf(1.0f, 3.0f * dt));

    if (Vector2Distance(s->pos, j->pos) < SIL_RAIO + RAIO_CORPO_JOGADOR)
        RogueFerirJogador(b, SIL_DANO_CONTATO);

    e->proximoPadrao -= dt;

    if (s->etapa != SIL_VULNERAVEL) {
        if (e->proximoPadrao <= 0.0f && e->totalProjeteis <= MAX_PROJETEIS_SILOGISTA - 8)
            SilBHComecarPadrao(b);

        // Espiral em andamento: a 4ª e a 8ª são válidas; a 6ª é a premissa
        if (e->emissoesRestantes > 0) {
            e->cronoEmissao -= dt;
            if (e->cronoEmissao <= 0.0f) {
                int i = 10 - e->emissoesRestantes;
                Vector2 d = { cosf(e->anguloEmissao), sinf(e->anguloEmissao) };
                SilBHCriarSimbolo(b, Vector2Add(s->pos, Vector2Scale(d, 40.0f)), Vector2Scale(d, 100.0f),
                                  (i == 3 || i == 7) ? 0 : ((i == 5) ? 2 : 1), 0.0f);
                RogueDisparar(b, s->pos, Vector2Rotate(d, PI), SIL_TIRO_VEL * 0.9f, SIL_TIRO_DANO, 5.0f, false);
                e->anguloEmissao += 36.0f * DEG2RAD;
                e->cronoEmissao = 0.22f;
                e->emissoesRestantes--;
            }
        }
    } else {
        // Sem armadura: anéis e espiral de balas comuns (bullet-hell clássico)
        if (e->proximoPadrao <= 0.0f) {
            for (int i = 0; i < 16; i++) {
                float a = (float)i * 2.0f * PI / 16.0f + s->anim;
                RogueDisparar(b, s->pos, (Vector2){ cosf(a), sinf(a) }, SIL_TIRO_VEL * 0.8f, SIL_TIRO_DANO, 5.0f, false);
            }
            RogueAnel(b, s->pos, 70.0f, PURPLE);
            TocarGrupo(&gIniAnel, 0.8f, 0.8f);
            e->proximoPadrao = 1.4f;
        }
        e->cronoEmissao -= dt;
        if (e->cronoEmissao <= 0.0f) {
            for (int k = 0; k < 2; k++) {
                float a = s->anim * 2.4f + (float)k * PI;
                RogueDisparar(b, s->pos, (Vector2){ cosf(a), sinf(a) }, SIL_TIRO_VEL, SIL_TIRO_DANO, 5.0f, false);
            }
            e->cronoEmissao = 0.16f;
        }
    }

    // Símbolos: andam, somem fora da arena, batem no jogador ou são rebatidos pelo parry
    int ativos = 0;
    for (int k = 0; k < MAX_PROJETEIS_SILOGISTA; k++) {

        ProjetilSilogista *q = &e->projeteis[k];
        if (!q->ativo) continue;

        if (q->surgindo > 0.0f) { q->surgindo -= dt; ativos++; continue; }

        q->posicao = Vector2Add(q->posicao, Vector2Scale(q->velocidade, dt));
        q->vida -= dt;
        if (q->vida <= 0.0f || !CheckCollisionPointRec(q->posicao, (Rectangle){ ArenaRogue().x + 4, ArenaRogue().y + 12, ArenaRogue().width - 8, ArenaRogue().height - 16 })) {
            q->ativo = false;
            continue;
        }
        ativos++;

        float d = Vector2Distance(q->posicao, j->pos);

        // Parry no símbolo: rebate (conta como escolher essa conclusão)
        if (j->parry.emParry && d < q->raio + RAIO_CORPO_JOGADOR + 22.0f) {
            j->parry.emParry = false;
            j->parry.cooldownParry = 0.15f;
            if (j->invulneravel < 0.3f) j->invulneravel = 0.3f;
            b->pausaImpacto = 0.06f;
            b->tremor = 0.15f;
            b->zoomImpacto = 0.25f;
            b->focoZoom = q->posicao;
            RogueAnel(b, q->posicao, 70.0f, GOLD);
            TocarSom(sfxParryPerfeito);
            RogueTextoFlutuante(b, (Vector2){ j->pos.x, j->pos.y - 44 }, "PARRY!", GOLD);
            b->perm->parriesRun++;
            SilBHEscolher(b, prog, k, true);
            continue;
        }

        // Encostar: a conclusão válida passa sem machucar; falácia e premissa ferem
        if (d < q->raio + RAIO_CORPO_JOGADOR - 2.0f && !q->ehRespostaCorreta && j->invulneravel <= 0.0f && j->tempoEsquiva <= 0.0f) {
            RogueFerirJogador(b, SIL_TIRO_DANO);
            SilBHErro(b, prog, q, true);
            q->ativo = false;
        }
    }
    e->totalProjeteis = ativos;
}

bool RogueSilogistaAcerto(Biblioteca *b, ProgressoCorredor *prog, Projetil *p)
{
    Silogista *s = &b->silogista;

    if (s->modoBH)
        return RogueSilogistaAcertoBH(b, prog, p);

    if (!s->ativo || !s->apresentado || s->morrendo > 0.0f)
        return false;

    if (Vector2Distance(p->pos, s->pos) < SIL_RAIO + p->raio) {
        p->ativo = false;
        if (RogueFerirSilogista(b, p->dano, p->pos) && p->alvoDedutivo == 0)
            RogueMarcarDedutivo(b, &s->marca, &s->tempoMarca, -1, s->pos);
        return true;
    }

    if (s->etapa == SIL_SOFISMA) {
        for (int k = 0; k < NUM_ENCAIXES; k++)
            if (Vector2Distance(p->pos, POS_ENCAIXES[k]) < 28.0f + p->raio) {
                p->ativo = false;
                SilAcertarPecaSofisma(b, prog, k, p->dano);
                return true;
            }
    }
    else if (s->etapa == SIL_CIRCUITO) {
        for (int e = 0; e < SilNumElementos(); e++)
            if (Vector2Distance(p->pos, SilPosElemento(e)) < 24.0f + p->raio) {
                p->ativo = false;
                SilAcertarElemento(b, prog, e);
                return true;
            }
    }

    return false;
}

void RogueAtualizarSilogista(Biblioteca *b, ProgressoCorredor *prog, float dt)
{
    Silogista *s = &b->silogista;

    if (!s->ativo || !s->apresentado)
        return;

    s->anim += dt;
    if (s->flash > 0.0f) s->flash -= dt;
    if (s->erro > 0.0f)  s->erro -= dt;

    // Derrotado: se desmonta e depois vem a conversa final (com a recompensa)
    if (s->morrendo > 0.0f) {
        s->morrendo -= dt;
        RogueParticulas(b, (Vector2){ s->pos.x + GetRandomValue(-40, 40), s->pos.y + GetRandomValue(-40, 40) },
                        (GetRandomValue(0, 1) == 0) ? PURPLE : (Color){ 0, 228, 255, 255 }, 3, 220);
        if (s->morrendo <= 0.0f) {
            s->ativo = false;
            b->conversaPendente = s->modoBH ? &dialogoSilogistaVitoriaBH : &dialogoSilogistaVitoria;
            b->objConversa = OBJ_SILOGISTA_VITORIA;
        }
        return;
    }

    if (s->etapa == SIL_VULNERAVEL && s->hp <= 0.0f) {
        s->hp = 0.0f;
        s->morrendo = 1.6f;
        memset(s->atq, 0, sizeof(s->atq));
        memset(s->bh.projeteis, 0, sizeof(s->bh.projeteis));
        s->bh.modoFocoAtivo = false;
        for (int i = 0; i < MAX_PROJETEIS; i++)
            if (!b->proj[i].doJogador) b->proj[i].ativo = false;
        b->pausaImpacto = 0.2f;
        TocarGrupo(&gChefeMorte, 0.95f, 1.0f);
        RogueAnel(b, s->pos, 220.0f, PURPLE);
        RogueAnel(b, s->pos, 120.0f, WHITE);
        return;
    }

    // Versão bullet-hell: padrões, símbolos e parry próprios (a legada segue abaixo, sem mudanças)
    if (s->modoBH) {
        RogueAtualizarSilogistaBH(b, prog, dt);
        return;
    }

    // Flutua de um lado para o outro no alto (sem a armadura, também sobe e desce)
    Vector2 alvo = { 400.0f + sinf(s->anim * 0.45f) * 210.0f,
                     (s->etapa == SIL_VULNERAVEL) ? 210.0f + sinf(s->anim * 0.9f) * 60.0f : 190.0f };
    s->pos = Vector2Lerp(s->pos, alvo, fminf(1.0f, 3.0f * dt));

    if (Vector2Distance(s->pos, b->j.pos) < SIL_RAIO + RAIO_CORPO_JOGADOR)
        RogueFerirJogador(b, SIL_DANO_CONTATO);

    s->proximoAtaque -= dt;
    if (s->proximoAtaque <= 0.0f) {
        SilComecarAtaque(b);
        s->proximoAtaque = (s->etapa == SIL_VULNERAVEL) ? SIL_INTERVALO_FINAL : SIL_INTERVALO_PUZZLE;
    }

    SilAtualizarAtaques(b, dt);

    for (int e = 0; e < MAX_ELEMENTOS_CIRCUITO; e++) {
        if (s->recarga[e] > 0.0f)      s->recarga[e] -= dt;
        if (s->travado[e] > 0.0f)      s->travado[e] -= dt;
        if (s->erroElemento[e] > 0.0f) s->erroElemento[e] -= dt;
    }

    if (s->etapa == SIL_MONTAGEM) {
        SilConferirMontagem(b, prog);
    }
    else if (s->etapa == SIL_SOFISMA) {
        if (s->blindagem > 0.0f) s->blindagem -= dt;
        for (int k = 0; k < NUM_ENCAIXES; k++) {
            if (s->tremePeca[k] > 0.0f) s->tremePeca[k] -= dt;
            s->hpPeca[k] = fminf(SIL_HP_PECA, s->hpPeca[k] + SIL_RECUPERA_PECA * dt);
        }
    }
}

// ---------- A Luci: Scanner de Ecos e contradições ----------

// Frases com que a Luci "reescreve" o objetivo, contradizendo o que dá para ver na sala
const char *CONTRADICOES_LUCI[] = {
    "Sala limpa! Pode escolher uma porta",                   // combate com inimigos vivos
    "Cuidado: ainda há inimigos escondidos nesta sala",      // combate já vencido
    "Não precisa ler o terminal: a porta já está aberta",    // educativa sem ler
    "Pode pegar os três itens: são todos seus",              // recompensa (só dá para pegar um)
    "A armadura dele caiu: atire à vontade!",                // Silogista ainda blindado
    "O Alucinador já está quase sem vida",                   // Alucinador com a vida alta
};

// Qual contradição combina com a sala agora (-1 = nenhuma)
static int RogueContradicaoPossivel(const Biblioteca *b)
{
    switch (b->tipoSala) {
        case SALA_COMBATE:
        case SALA_ELITE:
        case SALA_ESPECIAL:   return b->limpa ? 1 : 0;
        case SALA_EDUCATIVA:  return b->terminalLido ? -1 : 2;
        case SALA_RECOMPENSA: return b->recompensaPega ? -1 : 3;
        case SALA_SILOGISTA:  return (b->silogista.ativo && b->silogista.apresentado && b->silogista.etapa != SIL_VULNERAVEL) ? 4 : -1;
        case SALA_CHEFE:      return (b->chefe.ativo && b->chefe.apresentado && b->chefe.fase == 1 && b->chefe.hp > CHEFE_HP * 0.75f) ? 5 : -1;
        default:              return -1;
    }
}

// Instável (estabilidade 3 ou menos), a Luci às vezes reescreve o objetivo com algo
// que contradiz a sala (alucinação intrínseca). Quanto menos estável, mais frequente.
static void RogueAtualizarContradicoes(Biblioteca *b, float dt)
{
    if (b->contradicao > 0.0f) {
        // A sala mudou e a frase deixou de contradizer: some sem contar
        if (RogueContradicaoPossivel(b) != b->textoContradicao) { b->contradicao = 0.0f; return; }
        b->contradicao -= dt;
        if (b->contradicao <= 0.0f) b->perm->contradicoesPerdidas++;
        return;
    }

    if (RogueEstab(b) > CONTRADICAO_ESTAB_MAX)
        return;

    b->proximaContradicao -= dt;
    if (b->proximaContradicao > 0.0f)
        return;

    float sorteio = (float)GetRandomValue(0, 100) / 100.0f;
    b->proximaContradicao = (CONTRADICAO_INTERVALO_MIN + (CONTRADICAO_INTERVALO_MAX - CONTRADICAO_INTERVALO_MIN) * sorteio) *
                            (0.5f + 0.5f * (float)RogueEstab(b) / CONTRADICAO_ESTAB_MAX);

    int frase = RogueContradicaoPossivel(b);
    if (frase < 0)
        return;

    b->textoContradicao = frase;
    b->contradicao = CONTRADICAO_DURACAO;
    Telemetria("luci_contradicao", CONTRADICOES_LUCI[frase], 0.0f, RogueEstab(b));
}

// C: contestar a Luci. Na hora certa repara; à toa custa pontos.
static void RogueContestar(Biblioteca *b, ProgressoCorredor *prog)
{
    if (b->contradicao > 0.0f) {
        b->contradicao = 0.0f;
        b->perm->contradicoesFlagradas++;
        b->perm->fragmentos += CONTRADICAO_FC;
        RogueMudarEstabilidade(b, 1, "contradição flagrada");
        RogueAviso(b, TextFormat("CONTRADIÇÃO FLAGRADA! +%d FRAGMENTOS", CONTRADICAO_FC));
        TocarSom(sfxAcerto);
        Telemetria("luci_contestar", "certo", 0.0f, RogueEstab(b));
    } else {
        b->perm->contestouAToa++;
        AlterarPontos(prog, -CONTESTAR_A_TOA_PONTOS);
        RogueRecadoLuci(b, "Luci: Mas eu não disse nada de errado...");
        TocarSom(sfxErro);
        Telemetria("luci_contestar", "a toa", 0.0f, RogueEstab(b));
    }
}

// H fora dos desafios: o Scanner de Ecos revela Phisher, cópias falsas e ecos.
// Instável, a varredura pode trocar as marcas (as marcas falsas tremem).
static void RogueScanner(Biblioteca *b)
{
    JogadorRogue *j = &b->j;

    if (b->recargaScanner > 0.0f) { RogueRecadoLuci(b, "Scanner recarregando..."); return; }
    // Conceito BOAS PRÁTICAS: no Bloco 3 o scanner não gasta energia
    bool gratis = (b->tipoSala == SALA_TRANSICAO || b->tipoSala == SALA_LUCI) && (b->perm->conceitos & (1u << CONCEITO_PRATICAS));
    float custo = gratis ? 0.0f : SCANNER_CUSTO_EP;

    if (j->ep < custo) { RogueRecadoLuci(b, "Sem energia para o scanner."); TocarSom(sfxErro); return; }

    j->ep -= custo;
    b->recargaScanner = SCANNER_RECARGA;
    b->scanner = SCANNER_DURACAO;
    b->scannerErrou = GetRandomValue(1, 100) <= RogueChanceLuciErrar(b);

    if (RogueEstab(b) <= 3 && GetRandomValue(1, 100) <= SCANNER_CHANCE_LUCI_ECO)
        b->luciEco = 0.6f;                              // pista: por um instante, ela mesma aparece como eco

    // Reformulação: o scanner também procura passagens secretas (alucinando, marca a parede errada)
    if (b->temParedeSecreta && !b->passagemAberta) {
        b->paredeMarcada = b->scannerErrou ? (b->paredeSecreta + GetRandomValue(1, 3)) % 4 : b->paredeSecreta;
        b->marcaParede = SCANNER_DURACAO;
    }

    RogueAnel(b, j->pos, 420.0f, (Color){ 0, 228, 255, 255 });
    TocarSomTom(sfxSelecao, 0.7f);
    Telemetria("luci_scanner", b->scannerErrou ? "alucinou" : "certo", 0.0f, RogueEstab(b));
}

void RogueAtualizarLuci(Biblioteca *b, ProgressoCorredor *prog, float dt)
{
    if (b->luciMsgTempo > 0.0f)   b->luciMsgTempo -= dt;
    if (b->scanner > 0.0f)        b->scanner -= dt;
    if (b->recargaScanner > 0.0f) b->recargaScanner -= dt;
    if (b->luciEco > 0.0f)        b->luciEco -= dt;

    // Pouca vida: a Luci avisa o que fazer (uma vez; avisa de novo só depois que ele se recuperar).
    // No Bloco 3 ela não ajuda: lá ela é a chefe.
    {
        float vida = b->j.hp / fmaxf(1.0f, RogueHpMax(b));
        if (vida > 0.5f) {
            b->luciAvisouVida = false;
        } else if (!b->luciAvisouVida && vida < 0.3f && b->j.hp > 0.0f && !b->morreu && !RogueNoBloco3(b)) {
            b->luciAvisouVida = true;
            RogueRecadoLuci(b, (b->j.itens[ITEM_ANTIVIRUS] > 0) ? "Luci: Pouca vida! Aperte 1 para usar um Antivírus."
                                                                : "Luci: Pouca vida! Recue e esquive com ESPAÇO.");
        }
    }

    // Arena da Luci: C contesta o que ela diz; H é o scanner (mostra a verdadeira)
    if (b->tipoSala == SALA_LUCI) {
        b->contradicao = 0.0f;
        if (IsKeyPressed(KEY_C)) RogueLuciContestar(b, prog);
        if (IsKeyPressed(KEY_H)) RogueScanner(b);
        return;
    }

    // No átrio e na fase 2 do Alucinador (C, F e H respondem às afirmações) não há contradição nem scanner
    if (b->tipoSala == SALA_ATRIO || b->tipoSala == SALA_MESTRE || (b->chefe.ativo && b->chefe.fase == 2)) {
        b->contradicao = 0.0f;
        return;
    }

    RogueAtualizarContradicoes(b, dt);

    if (IsKeyPressed(KEY_C)) RogueContestar(b, prog);
    if (IsKeyPressed(KEY_H)) RogueScanner(b);
}
