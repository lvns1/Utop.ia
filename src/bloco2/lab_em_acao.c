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

// ---------- Laboratórios EM AÇÃO: Núcleo Neural e Firewall Ético na própria sala ----------
//   NÚCLEO NEURAL: os blocos de contexto da missão voam até o núcleo (o console). Atirar destrói
//     o bloco; o que chega entra na janela de contexto e ocupa tokens. Passou da capacidade, o
//     bloco MAIS ANTIGO é cortado e sai voando. No fim, o resultado é o mesmo do laboratório:
//     faltou o essencial, cortou o começo, entrou ruído ou ficou na medida.
//   FIREWALL ÉTICO: pacotes de dados atravessam a sala até a IA. TIRO bloqueia, ENCOSTAR
//     anonimiza (os dados pessoais somem) e deixar passar permite. Errar custa um pouco de vida.

bool LabEmAcaoTipo(TipoLab t)
{
    return LAB_EM_ACAO && (t == LAB_NUCLEO || t == LAB_FIREWALL);
}

static void LabEmbaralhar(int *v, int n)
{
    for (int i = n - 1; i > 0; i--) {
        int j = GetRandomValue(0, i);
        int t = v[i]; v[i] = v[j]; v[j] = t;
    }
}

void LabComecarAcao(Biblioteca *b)
{
    LabEdu *L = &b->lab;

    L->emAcao = true;
    L->aberto = false;
    L->etapa = LAB_DESAFIO;
    L->tempoAcao = 0.0f;
    L->cronoSurgir = 1.2f;
    L->proximo = 0;
    L->carga = 0;
    L->marcados = L->cortados = 0;
    L->numOrdem = 0;
    L->acertos = L->decisoes = 0;
    L->ultimaFaixa = -1;
    memset(L->obj, 0, sizeof(L->obj));
    b->labIntroVista |= 1u << L->tipo;

    if (L->tipo == LAB_NUCLEO) {
        const DesafioNucleo *d = &DESAFIOS_NUCLEO[L->missao];
        L->numAcao = d->numBlocos;
        for (int i = 0; i < L->numAcao; i++) L->ordemAcao[i] = i;
        LabEmbaralhar(L->ordemAcao, L->numAcao);
        RogueAviso(b, "NÚCLEO NEURAL: ALIMENTE A JANELA DE CONTEXTO");
        RogueRecadoLuci(b, "Luci: Atire no que NÃO ajuda a tarefa. O resto, deixe chegar ao núcleo!");
    } else {
        // 6 pacotes, com pelo menos um de cada decisão (bloquear, anonimizar e permitir)
        int ordem[NUM_PACOTES_DADOS];
        for (int tentativa = 0; tentativa < 30; tentativa++) {
            for (int i = 0; i < NUM_PACOTES_DADOS; i++) ordem[i] = i;
            LabEmbaralhar(ordem, NUM_PACOTES_DADOS);
            int cont[NUM_ACOES_PACOTE] = { 0 };
            for (int k = 0; k < LAB_PACOTES; k++) cont[PACOTES_DADOS[ordem[k]].acao]++;
            if (cont[ACAO_PERMITIR] >= 1 && cont[ACAO_ANONIMIZAR] >= 1 && cont[ACAO_BLOQUEAR] >= 2) break;
        }
        L->numAcao = LAB_PACOTES;
        for (int k = 0; k < LAB_PACOTES; k++) { L->pacotes[k] = ordem[k]; L->ordemAcao[k] = k; L->acoes[k] = -2; }
        RogueAviso(b, "FIREWALL ÉTICO: PROTEJA OS DADOS");
        RogueRecadoLuci(b, "Luci: TIRO bloqueia, ENCOSTAR anonimiza, deixar passar libera!");
    }

    b->luciMsgTempo = 5.0f;
    TocarSom(sfxEstatica);
    Telemetria("rogue_lab_acao", NOMES_LAB[L->tipo], 0.0f, L->missao);
}

static ObjAcaoLab *LabNovoObj(LabEdu *L)
{
    for (int i = 0; i < LAB_MAX_OBJ_ACAO; i++)
        if (!L->obj[i].ativo) { memset(&L->obj[i], 0, sizeof(L->obj[i])); L->obj[i].ativo = true; return &L->obj[i]; }
    return NULL;
}

float LabRaioBloco(int tokens) { return 14.0f + 1.6f * (float)tokens; }

// O resultado do Núcleo (a mesma regra do LabProcessarNucleo) e a janela de resultado
static void LabEncerrarAcao(Biblioteca *b, ProgressoCorredor *prog)
{
    LabEdu *L = &b->lab;

    if (L->tipo == LAB_NUCLEO) {
        const DesafioNucleo *d = &DESAFIOS_NUCLEO[L->missao];
        bool falta = false;
        int ruido = 0;
        for (int i = 0; i < d->numBlocos; i++) {
            bool dentro = (L->marcados & (1u << i)) && !(L->cortados & (1u << i));
            if (d->bloco[i].relevancia == REL_ESSENCIAL && !dentro)    falta = true;
            if (d->bloco[i].relevancia == REL_IRRELEVANTE && dentro)   ruido++;
        }
        if (falta)                          L->resultadoNucleo = (L->cortados != 0) ? NUCLEO_TRUNCADA : NUCLEO_FALTOU;
        else if (ruido > 0 || L->cortados)  L->resultadoNucleo = NUCLEO_RUIDOSA;
        else                                L->resultadoNucleo = NUCLEO_BOA;
        L->marcados &= ~L->cortados;        // a tela de resultado mostra o que ficou dentro
        L->processos = 1;
    }

    L->emAcao = false;
    L->aberto = true;
    LabFinalizar(b, prog);
}

// Firewall: o pacote chegou ao fim (acao: ACAO_BLOQUEAR, ACAO_ANONIMIZAR ou -1 = passou e foi permitido)
static void LabDecidirPacoteAcao(Biblioteca *b, ObjAcaoLab *o, int acao)
{
    LabEdu *L = &b->lab;
    const PacoteDados *p = &PACOTES_DADOS[o->id];
    int efetiva = (acao < 0) ? ACAO_PERMITIR : acao;
    bool certo = (efetiva == p->acao);

    L->acoes[o->slot] = acao;
    L->decisoes++;
    o->ativo = false;

    if (certo) {
        L->acertos++;
        static const char *ok[NUM_ACOES_PACOTE] = { "LIBERADO!", "BLOQUEADO!", "ANONIMIZADO!" };
        RogueTextoFlutuante(b, (Vector2){ o->pos.x, o->pos.y - 26 }, ok[efetiva], LIME);
        RogueParticulas(b, o->pos, LIME, 14, 180);
        TocarSom(sfxAcerto);
    } else {
        const char *t = (p->acao == ACAO_BLOQUEAR)     ? "VAZOU!"
                      : (p->acao == ACAO_ANONIMIZAR)   ? ((efetiva == ACAO_BLOQUEAR) ? "ERA SÓ ANONIMIZAR" : "FOI COM O NOME!")
                                                       : "BLOQUEOU À TOA";
        RogueTextoFlutuante(b, (Vector2){ o->pos.x, o->pos.y - 26 }, t, MAGENTA);
        RogueParticulas(b, o->pos, MAGENTA, 16, 200);
        b->j.hp = fmaxf(1.0f, b->j.hp - LAB_ACAO_DANO_VAZAMENTO);
        b->tremor = 0.25f;
        TocarSom(sfxErro);
    }
}

void RogueAtualizarLabAcao(Biblioteca *b, ProgressoCorredor *prog, float dt)
{
    LabEdu *L = &b->lab;

    if (b->tipoSala != SALA_LABORATORIO || !L->emAcao)
        return;

    L->tempoAcao += dt;
    if (L->tremeNucleo > 0.0f) L->tremeNucleo -= dt;
    Vector2 nucleo = b->posObjetoSala;

    // Surge o próximo
    L->cronoSurgir -= dt;
    if (L->proximo < L->numAcao && L->cronoSurgir <= 0.0f) {
        ObjAcaoLab *o = LabNovoObj(L);
        if (o != NULL) {
            if (L->tipo == LAB_NUCLEO) {
                o->id = L->ordemAcao[L->proximo];
                float a = (float)GetRandomValue(0, 359) * DEG2RAD;
                o->pos = (Vector2){ Clamp(nucleo.x + cosf(a) * 330.0f, 70.0f, 730.0f), Clamp(nucleo.y + sinf(a) * 330.0f, 90.0f, 500.0f) };
                o->vel = Vector2Scale(Vector2Normalize(Vector2Subtract(nucleo, o->pos)), LAB_ACAO_VEL_BLOCO);
                L->cronoSurgir = LAB_ACAO_SURGIR_NUCLEO;
            } else {
                static const float faixas[4] = { 170, 250, 330, 410 };
                int f = GetRandomValue(0, 3);
                if (f == L->ultimaFaixa) f = (f + 1 + GetRandomValue(0, 2)) % 4;
                L->ultimaFaixa = f;
                o->slot = L->ordemAcao[L->proximo];
                o->id = L->pacotes[o->slot];
                o->pos = (Vector2){ 50.0f + (float)(MeasureText(ROTULOS_PACOTES[o->id], 10) + 30) / 2.0f, faixas[f] };
                o->vel = (Vector2){ LAB_ACAO_VEL_PACOTE, 0 };
                L->cronoSurgir = LAB_ACAO_SURGIR_PACOTE;
            }
            o->anim = (float)GetRandomValue(0, 100) / 10.0f;
            L->proximo++;
            TocarSomTom(sfxSelecao, 0.7f);
        }
    }

    for (int i = 0; i < LAB_MAX_OBJ_ACAO; i++) {
        ObjAcaoLab *o = &L->obj[i];
        if (!o->ativo) continue;
        o->anim += dt;

        if (L->tipo == LAB_NUCLEO) {
            const DesafioNucleo *d = &DESAFIOS_NUCLEO[L->missao];
            const BlocoContexto *bl = &d->bloco[o->id];

            if (o->ejetado) {               // cortado da janela: sai voando e some
                o->pos = Vector2Add(o->pos, Vector2Scale(o->vel, dt));
                o->vida -= dt;
                if (o->vida <= 0.0f) o->ativo = false;
                continue;
            }

            // Anda até o núcleo balançando um pouco (fica mais difícil acertar)
            Vector2 lado = { -o->vel.y, o->vel.x };
            o->pos = Vector2Add(o->pos, Vector2Scale(o->vel, dt));
            o->pos = Vector2Add(o->pos, Vector2Scale(Vector2Normalize(lado), cosf(o->anim * 2.2f) * 22.0f * dt));

            // Tiro: o bloco é destruído (não entra na janela)
            float raio = LabRaioBloco(bl->tokens);
            bool destruido = false;
            for (int k = 0; k < MAX_PROJETEIS && !destruido; k++) {
                Projetil *p = &b->proj[k];
                if (!p->ativo || !p->doJogador || MestreDistTrajeto(p, o->pos, dt) > raio + p->raio) continue;
                p->ativo = false;
                destruido = true;
            }
            if (destruido) {
                o->ativo = false;
                bool essencial = bl->relevancia == REL_ESSENCIAL;
                RogueTextoFlutuante(b, (Vector2){ o->pos.x, o->pos.y - 24 }, essencial ? "ERA ESSENCIAL!" : ((bl->relevancia == REL_UTIL) ? "ERA ÚTIL..." : "RUÍDO FORA!"),
                                    essencial ? RED : ((bl->relevancia == REL_UTIL) ? GOLD : LIME));
                RogueParticulas(b, o->pos, essencial ? RED : (Color){ 150, 140, 255, 255 }, 18, 220);
                TocarSom(essencial ? sfxErro : sfxAcerto);
                continue;
            }

            // Chegou ao núcleo: entra na janela de contexto
            if (Vector2Distance(o->pos, nucleo) < LAB_ACAO_RAIO_NUCLEO) {
                o->ativo = false;
                L->marcados |= 1u << o->id;
                if (L->numOrdem < LAB_MAX_ITENS) L->ordem[L->numOrdem++] = o->id;
                L->carga += bl->tokens;
                RogueTextoFlutuante(b, (Vector2){ nucleo.x, nucleo.y - 60 }, TextFormat("+%d tokens", bl->tokens), (Color){ 150, 140, 255, 255 });
                RogueAnel(b, nucleo, 60.0f, (Color){ 150, 140, 255, 255 });
                TocarSomTom(sfxPegar, 0.9f);

                // Passou da capacidade: o MAIS ANTIGO é cortado e sai voando
                for (int k = 0; k < L->numOrdem && L->carga > d->capacidade; k++) {
                    int velho = L->ordem[k];
                    if (L->cortados & (1u << velho)) continue;
                    L->cortados |= 1u << velho;
                    L->carga -= d->bloco[velho].tokens;
                    ObjAcaoLab *e = LabNovoObj(L);
                    if (e != NULL) {
                        float a = (float)GetRandomValue(0, 359) * DEG2RAD;
                        e->id = velho;
                        e->ejetado = true;
                        e->pos = nucleo;
                        e->vel = (Vector2){ cosf(a) * 260.0f, sinf(a) * 260.0f };
                        e->vida = 1.2f;
                    }
                    L->tremeNucleo = 0.4f;
                    RogueTextoFlutuante(b, (Vector2){ nucleo.x, nucleo.y - 80 }, "ESTOUROU! O MAIS ANTIGO FOI CORTADO", ORANGE);
                    TocarSom(sfxErro);
                }
            }
        } else {
            // Firewall: o pacote atravessa a sala; tiro bloqueia, encostar anonimiza
            o->pos.x += o->vel.x * dt;
            o->pos.y += sinf(o->anim * 2.0f) * 10.0f * dt;

            bool bloqueado = false;
            for (int k = 0; k < MAX_PROJETEIS && !bloqueado; k++) {
                Projetil *p = &b->proj[k];
                if (!p->ativo || !p->doJogador || MestreDistTrajeto(p, o->pos, dt) > 22.0f + p->raio) continue;
                p->ativo = false;
                bloqueado = true;
            }
            if (bloqueado) { LabDecidirPacoteAcao(b, o, ACAO_BLOQUEAR); continue; }

            if (!o->anonimizado && Vector2Distance(o->pos, b->j.pos) < 30.0f + RAIO_CORPO_JOGADOR) {
                o->anonimizado = true;
                RogueParticulas(b, o->pos, (Color){ 0, 228, 255, 255 }, 14, 160);
                RogueTextoFlutuante(b, (Vector2){ o->pos.x, o->pos.y - 22 }, "DADOS ESCONDIDOS", (Color){ 0, 228, 255, 255 });
                TocarSomTom(sfxSelecao, 1.4f);
            }

            if (o->pos.x > LAB_ACAO_X_SAIDA)
                LabDecidirPacoteAcao(b, o, o->anonimizado ? ACAO_ANONIMIZAR : -1);
        }
    }

    // Acabou: tudo surgiu e nada está mais no ar
    if (L->proximo >= L->numAcao) {
        bool resta = false;
        for (int i = 0; i < LAB_MAX_OBJ_ACAO; i++) if (L->obj[i].ativo) resta = true;
        if (!resta) LabEncerrarAcao(b, prog);
    }
}

// ---------- Câmara da Inferência: o console de SAÍDA (desistir da luta sem perder a run) ----------
const Vector2 POS_SAIDA_SILOGISTA = { 86, 466 };     // perto da entrada (o jogador entra por baixo)

bool RoguePertoSaidaSilogista(const Biblioteca *b)
{
    const Silogista *s = &b->silogista;
    return b->tipoSala == SALA_SILOGISTA && s->ativo && !b->limpa && s->morrendo <= 0.0f && !b->morreu &&
           Vector2Distance(b->j.pos, POS_SAIDA_SILOGISTA) < SAIDA_SILOGISTA_RAIO;
}

// Saiu da Câmara: o Silogista some, as portas abrem e a run continua (sem a recompensa da Câmara)
static void RogueSairSilogista(Biblioteca *b)
{
    b->silogista.ativo = false;
    b->silogista.carregando = -1;
    b->limpa = true;
    b->saiuSilogista = true;
    b->segurandoSaida = 0.0f;
    for (int i = 0; i < MAX_PROJETEIS; i++)
        if (!b->proj[i].doJogador) b->proj[i].ativo = false;
    for (int i = 0; i < MAX_INIMIGOS; i++)
        b->ini[i].ativo = false;

    RogueParticulas(b, POS_SAIDA_SILOGISTA, (Color){ 120, 110, 255, 255 }, 30, 220);
    RogueAnel(b, POS_SAIDA_SILOGISTA, 80.0f, (Color){ 120, 110, 255, 255 });
    TocarSom(sfxEstatica);
    RogueAviso(b, "VOCÊ SAIU DA CÂMARA DA INFERÊNCIA");
    RogueRecadoLuci(b, "Luci: Recuar também é estratégia. As portas abriram.");
    Telemetria("silogista_saida", "", 0.0f, b->indiceRota);
}

void RogueAtualizarSaidaSilogista(Biblioteca *b, float dt)
{
    if (RoguePertoSaidaSilogista(b) && IsKeyDown(KEY_E)) {
        b->segurandoSaida += dt;
        if (b->segurandoSaida >= SAIDA_SILOGISTA_SEGURAR)
            RogueSairSilogista(b);
    } else {
        b->segurandoSaida = fmaxf(0.0f, b->segurandoSaida - dt * 2.0f);
    }
}
