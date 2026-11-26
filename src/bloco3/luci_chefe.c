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



static float Suavizar(float k) { k = Clamp(k, 0.0f, 1.0f); return k * k * (3.0f - 2.0f * k); }

// Uma partícula com posição e velocidade escolhidas (as de RogueParticulas saem do centro para fora)
static void RogueParticulaDirigida(Biblioteca *b, Vector2 pos, Vector2 vel, float vida, float tam, Color cor)
{
    for (int i = 0; i < MAX_PARTICULAS_ROGUE; i++) {
        ParticulaRogue *p = &b->part[i];
        if (p->vida > 0.0f) continue;
        p->pos = pos; p->vel = vel; p->vidaMax = p->vida = vida; p->tam = tam; p->cor = cor;
        return;
    }
}

// Onde a Luci está na cena da separação (e o tamanho dela)
static Vector2 CenaSeparacaoPos(const Biblioteca *b, float e, float *altura, bool *atras)
{
    Vector2 jp = b->j.pos;
    Vector2 frente = { jp.x, jp.y - 46.0f };
    *atras = false;

    if (e < CENA_SEP_ORBITA) {                                       // a órbita perde velocidade
        float ang = 4.2f * (e - e * e / (2.0f * CENA_SEP_ORBITA));
        *altura = ALTURA_LUCI * (0.85f + 0.15f * (0.5f + 0.5f * sinf(ang)));
        *atras = sinf(ang) < 0.0f;
        Vector2 orbita = { jp.x + cosf(ang) * RAIO_ORBITA_LUCI, jp.y - 14.0f + sinf(ang) * 8.0f };
        return orbita;
    }
    if (e < CENA_SEP_PARADA) {                                       // sobe e para na frente dele
        float k = Suavizar((e - CENA_SEP_ORBITA) / 0.35f);
        Vector2 de = { jp.x + cosf(4.2f * CENA_SEP_ORBITA * 0.5f) * RAIO_ORBITA_LUCI, jp.y - 14.0f + sinf(4.2f * CENA_SEP_ORBITA * 0.5f) * 8.0f };
        *altura = ALTURA_LUCI * (1.0f + 0.2f * k);
        return Vector2Lerp(de, frente, k);
    }
    if (e < CENA_SEP_SOBE) {                                         // vai embora até a frente da porta do núcleo
        float k = Suavizar((e - CENA_SEP_PARADA) / (CENA_SEP_CHEGA - CENA_SEP_PARADA));
        *altura = Lerp(ALTURA_LUCI * 1.2f, 44.0f, k);
        return Vector2Lerp(frente, POS_LUCI_NUCLEO, k);
    }
    // Sobe e entra pela porta: encolhe até sumir no vão (altura 0 = já passou)
    float k = Clamp((e - CENA_SEP_SOBE) / (CENA_SEP_ENTRA - CENA_SEP_SOBE), 0.0f, 1.0f);
    k = k * k;                                                       // começa devagar e é puxada para dentro
    *altura = 44.0f * (1.0f - k);
    return Vector2Lerp(POS_LUCI_NUCLEO, CentroRetangulo(PORTA_PROXIMA_ROGUE), k);
}

void DesenharCenaSeparacao(const Biblioteca *b, float t, bool passeFrente)
{
    float e = CENA_SEPARACAO - b->cenaLuci;
    float altura;
    bool atras;
    Vector2 p = CenaSeparacaoPos(b, e, &altura, &atras);
    p.y += sinf(t * 2.0f) * 2.0f;

    if (atras == passeFrente)
        return;

    // O fio de luz entre o peito do Thomas e a Luci: estica, passa de azul para magenta, falha e se rompe
    Vector2 peito = { b->j.pos.x, b->j.pos.y - 18.0f };
    if (e > CENA_SEP_ORBITA * 0.6f && e < CENA_SEP_ROMPE) {
        float k = Clamp((e - CENA_SEP_PARADA) / (CENA_SEP_ROMPE - CENA_SEP_PARADA), 0.0f, 1.0f);
        Color cor = { (unsigned char)Lerp(102.0f, 255.0f, k), (unsigned char)Lerp(191.0f, 0.0f, k), 255, 255 };
        bool falha = k > 0.55f && ((int)(t * 30.0f)) % 4 == 0;
        if (!falha) {
            Vector2 meio = Vector2Lerp(peito, p, 0.5f);
            meio.y += 10.0f * (1.0f - k);                            // pende um pouco e vai ficando esticado
            DrawLineEx(peito, meio, 4.0f - 2.5f * k, Fade(cor, 0.25f));
            DrawLineEx(meio, p, 4.0f - 2.5f * k, Fade(cor, 0.25f));
            DrawLineEx(peito, meio, 1.5f, Fade(cor, 0.9f));
            DrawLineEx(meio, p, 1.5f, Fade(cor, 0.9f));
        }
    } else if (e >= CENA_SEP_ROMPE && e < CENA_SEP_ROMPE + 0.4f) {     // as duas pontas se recolhendo
        float k = (e - CENA_SEP_ROMPE) / 0.4f;
        Vector2 meio = Vector2Lerp(peito, p, 0.5f);
        DrawLineEx(peito, Vector2Lerp(meio, peito, k), 1.5f, Fade(SKYBLUE, 1.0f - k));
        DrawLineEx(p, Vector2Lerp(meio, p, k), 1.5f, Fade(MAGENTA, 1.0f - k));
    }

    // Parada na frente dele: um instante olhando para o Thomas
    if (e >= CENA_SEP_ORBITA && e < CENA_SEP_PARADA) {
        float k = (e - CENA_SEP_ORBITA) / (CENA_SEP_PARADA - CENA_SEP_ORBITA);
        DrawCircleV(p, altura * 0.9f, Fade(SKYBLUE, 0.10f + 0.10f * sinf(k * PI)));
        const char *rt = "...";
        DrawText(rt, (int)p.x - MeasureText(rt, 12) / 2, (int)(p.y - altura - 8), 12, Fade(WHITE, sinf(k * PI)));
    }

    if (altura < 2.0f)                                               // já entrou no núcleo
        return;

    // Entrando: um rastro magenta da Luci até o vão da porta
    if (e >= CENA_SEP_SOBE) {
        Vector2 vao = CentroRetangulo(PORTA_PROXIMA_ROGUE);
        float k = Clamp((e - CENA_SEP_SOBE) / (CENA_SEP_ENTRA - CENA_SEP_SOBE), 0.0f, 1.0f);
        DrawLineEx(p, vao, 6.0f * (1.0f - k) + 1.0f, Fade(MAGENTA, 0.25f));
        DrawCircleV(vao, 10.0f + 14.0f * k, Fade(MAGENTA, 0.15f + 0.25f * k));
    }

    float inst = Clamp((e - CENA_SEP_PARADA) / (CENA_SEP_CHEGA - CENA_SEP_PARADA), 0.0f, 1.0f) * 0.85f;   // fica como na sala (0,85)
    DesenharLuciGrande(p, altura, inst, e >= CENA_SEP_PARADA && altura > 12.0f);
}

// Arena: as rachaduras que saem da Luci enquanto ela se corrompe (sempre os mesmos ângulos)
static void DesenharRachadurasCena(Vector2 c, float k, float t)
{
    for (int r = 0; r < 9; r++) {
        float ang = (float)r * (2.0f * PI / 9.0f) + 0.3f * (float)(Ruido((unsigned int)r * 31u) % 10) / 10.0f;
        float alcance = (60.0f + (float)(Ruido((unsigned int)r * 7u + 3u) % 80)) * k;
        Vector2 a = c;
        for (int s = 1; s <= 4; s++) {
            float d = alcance * (float)s / 4.0f;
            float desvio = ((float)(Ruido((unsigned int)(r * 13 + s)) % 21) - 10.0f) * DEG2RAD * 2.0f;
            Vector2 b2 = { c.x + cosf(ang + desvio) * d, c.y + 18.0f + sinf(ang + desvio) * d * 0.45f };
            DrawLineEx(a, b2, 3.0f, Fade(MAGENTA, 0.25f * k));
            DrawLineEx(a, b2, 1.2f, Fade((Color){ 255, 140, 255, 255 }, (0.6f + 0.4f * sinf(t * 9.0f + (float)r)) * k));
            a = b2;
        }
    }
}

// Arena: a Luci durante a transformação (no lugar de DesenharLuciChefe)
void DesenharCenaTransformacao(const Biblioteca *b, float t)
{
    const LuciChefe *l = &b->luci;
    float e = CENA_TRANSFORMACAO - b->cenaLuci;
    float instFinal = l->boa ? 0.7f : 1.0f;
    Vector2 p = { l->pos.x, l->pos.y + sinf(t * 2.0f) * 4.0f };
    float altura = 44.0f, inst = 0.1f;
    bool esconder = false;

    if (e >= CENA_TR_CORROMPE && e < CENA_TR_EXPLODE) {
        float k = (e - CENA_TR_CORROMPE) / (CENA_TR_EXPLODE - CENA_TR_CORROMPE);
        inst = 0.1f + (instFinal - 0.1f) * k;
        p.x += (float)GetRandomValue(-1, 1) * 2.0f * k;
        esconder = k > 0.3f && (Ruido((unsigned int)(t * 24.0f)) % 100) < (unsigned int)(18.0f * k);
        DesenharRachadurasCena(l->pos, k, t);
        DrawCircleV(p, 30.0f + 30.0f * k, Fade(MAGENTA, 0.08f + 0.12f * k));
    } else if (e >= CENA_TR_EXPLODE) {
        float k = Clamp((e - CENA_TR_EXPLODE) / (CENA_TR_CRESCE - CENA_TR_EXPLODE), 0.0f, 1.0f);
        float sobra = 1.0f + 0.25f * sinf(k * PI);                    // cresce passando um pouco do ponto e volta
        altura = Lerp(44.0f, 70.0f, k) * sobra;
        inst = instFinal;
        DesenharRachadurasCena(l->pos, 1.0f - 0.6f * Clamp((e - CENA_TR_EXPLODE) / 2.0f, 0.0f, 1.0f), t);
        float onda = Clamp((e - CENA_TR_EXPLODE) / 0.6f, 0.0f, 1.0f);   // o anel da explosão
        if (onda < 1.0f) {
            DrawCircleLines((int)p.x, (int)p.y, 20.0f + 320.0f * onda, Fade(MAGENTA, 1.0f - onda));
            DrawCircleLines((int)p.x, (int)p.y, 14.0f + 260.0f * onda, Fade(WHITE, 0.7f * (1.0f - onda)));
        }
        Color aura = l->boa ? MAGENTA : RED;
        DrawCircleV(p, LUCI_RAIO + 16 + 4.0f * sinf(t * 6.0f), Fade(aura, 0.16f * k));
    } else {
        DrawCircleV(p, 28.0f, Fade(SKYBLUE, 0.12f));                 // calma, ainda azul
    }

    if (!esconder)
        DesenharLuciGrande(p, altura, inst, true);
}

// HUD da cena da arena: o nome dela e a barra enchendo
void DesenharTituloCenaLuci(const Biblioteca *b, float t)
{
    const LuciChefe *l = &b->luci;
    float e = CENA_TRANSFORMACAO - b->cenaLuci;
    if (e < CENA_TR_TITULO) return;

    float k = Clamp((e - CENA_TR_TITULO) / 0.5f, 0.0f, 1.0f);
    float enche = Clamp((e - CENA_TR_TITULO - 0.2f) / 1.2f, 0.0f, 1.0f);
    LuciChefe barra = *l;
    barra.hp = l->hpMax * Suavizar(enche);
    DesenharBarraLuci(&barra, t);

    const char *nome = "L U C I";
    const char *sub = l->boa ? "A ALUCINAÇÃO DE UTOPIA  -  AINDA DÁ PARA SALVÁ-LA" : "A ALUCINAÇÃO DE UTOPIA";
    int tam = 44, y = 196;
    int x = LARGURA_TELA / 2 - MeasureText(nome, tam) / 2;
    float d = (1.0f - k) * 12.0f + ((((int)(t * 20.0f)) % 7 == 0) ? 4.0f : 0.0f);   // falha de cor
    DrawRectangle(0, y - 14, LARGURA_TELA, 96, Fade(BLACK, 0.55f * k));
    DrawText(nome, x - (int)d, y, tam, Fade(RED, 0.55f * k));
    DrawText(nome, x + (int)d, y, tam, Fade(SKYBLUE, 0.55f * k));
    DrawText(nome, x, y, tam, Fade(WHITE, k));
    DrawText(sub, LARGURA_TELA / 2 - MeasureText(sub, 14) / 2, y + 54, 14, Fade(l->boa ? (Color){ 0, 228, 255, 255 } : MAGENTA, k));
}

// Os momentos da cena (chamado a cada quadro com o tempo antes e depois)
static void RogueEventosCenaLuci(Biblioteca *b, float eAntes, float e)
{
    #define PASSOU(x) (eAntes < (x) && e >= (x))
    if (b->cenaLuciTipo == 1) {
        if (PASSOU(CENA_SEP_PARADA)) TocarSomTom(sfxSelecao, 0.7f);
        if (PASSOU(CENA_SEP_ROMPE)) {
            float altura; bool atras;
            Vector2 p = CenaSeparacaoPos(b, e, &altura, &atras);
            RogueParticulas(b, Vector2Lerp((Vector2){ b->j.pos.x, b->j.pos.y - 18.0f }, p, 0.5f), MAGENTA, 18, 160);
            b->tremor = fmaxf(b->tremor, 0.25f);
            b->flashNegativo = 0.18f;
            b->estaticaTela = 0.35f;
            TocarGrupo(&gLuciFio, 1.0f, 1.0f);
        }
        if (PASSOU(CENA_SEP_ENTRA)) {                                // atravessou a porta do núcleo: a sala fica vazia
            Vector2 vao = CentroRetangulo(PORTA_PROXIMA_ROGUE);
            RogueParticulas(b, vao, MAGENTA, 22, 200);
            RogueParticulas(b, vao, WHITE, 8, 140);
            b->tremor = fmaxf(b->tremor, 0.2f);
            b->estaticaTela = fmaxf(b->estaticaTela, 0.3f);
            TocarSomTom(sfxEstatica, 0.7f);
        }
    } else {
        LuciChefe *l = &b->luci;
        if (e >= CENA_TR_CORROMPE && e < CENA_TR_EXPLODE) {          // dados corrompidos convergindo para ela
            float k = (e - CENA_TR_CORROMPE) / (CENA_TR_EXPLODE - CENA_TR_CORROMPE);
            for (int n = 0; n < 2 + (int)(3.0f * k); n++) {
                float ang = (float)GetRandomValue(0, 359) * DEG2RAD;
                float r = 200.0f + (float)GetRandomValue(0, 80);
                Vector2 de = { l->pos.x + cosf(ang) * r, l->pos.y + sinf(ang) * r * 0.6f };
                Vector2 vel = Vector2Scale(Vector2Normalize(Vector2Subtract(l->pos, de)), 420.0f + 200.0f * k);
                RogueParticulaDirigida(b, de, vel, 0.45f, (float)GetRandomValue(2, 4), (GetRandomValue(0, 3) == 0) ? WHITE : MAGENTA);
            }
            b->tremor = fmaxf(b->tremor, 0.05f + 0.10f * k);
            b->estaticaTela = fmaxf(b->estaticaTela, 0.25f * k);
        }
        if (PASSOU(CENA_TR_CORROMPE)) TocarGrupo(&gLuciCorrupcao, 1.0f, 1.0f);
        if (PASSOU(CENA_TR_EXPLODE)) {
            b->flashNegativo = 0.7f;                // a abertura do núcleo: inversão de paleta
            b->estaticaTela = 0.6f;
            b->tremorForte = 0.5f;
            RogueParticulas(b, l->pos, MAGENTA, 40, 360);
            RogueParticulas(b, l->pos, WHITE, 16, 260);
            Vector2 empurra = Vector2Subtract(b->j.pos, l->pos);       // a onda empurra o Thomas para trás
            if (Vector2Length(empurra) > 1.0f) {
                b->j.pos = Vector2Add(b->j.pos, Vector2Scale(Vector2Normalize(empurra), 36.0f));
                RogueEmpurrarParaFora(&b->j.pos, RAIO_CORPO_JOGADOR, b->obst, b->numObst);
            }
            TocarGrupo(&gLuciExplosao, 1.0f, 1.0f);
        }
        if (PASSOU(CENA_TR_TITULO)) TocarSomTom(sfxAcerto, 0.5f);
    }
    #undef PASSOU
}

// Atualiza a cena. Devolve true enquanto ela estiver passando (o resto do jogo espera)
bool RogueAtualizarCenaLuci(Biblioteca *b, float dt)
{
    if (b->cenaLuciTipo == 0)
        return false;
    if (b->fadeEntrada > 0.0f)                                       // espera a sala aparecer
        return true;

    float total = (b->cenaLuciTipo == 1) ? CENA_SEPARACAO : CENA_TRANSFORMACAO;
    float eAntes = total - b->cenaLuci;
    bool pular = eAntes > 0.4f && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_E) || CliqueMouse());

    b->cenaLuci -= dt;
    if (pular) b->cenaLuci = 0.0f;
    RogueEventosCenaLuci(b, eAntes, pular ? eAntes : total - b->cenaLuci);

    if (b->cenaLuci <= 0.0f) {
        if (b->cenaLuciTipo == 2) b->luci.cenaVista = true;
        b->cenaLuciTipo = 0;
        b->cenaLuci = 0.0f;
        b->conversaPendente = b->conversaDepoisCena;
        b->conversaDepoisCena = NULL;
        if (pular) { GrupoParar(&gLuciCorrupcao); Telemetria("bloco3_cena", "pulou", eAntes, 0); }
    }
    return true;
}

// Monta as salas do Bloco 3 (chamado pelo RogueEntrarSala)
void RogueMontarSalaBloco3(Biblioteca *b, TipoSalaRogue tipo)
{
    if (tipo == SALA_TRANSICAO) {
        RogueUsarLayout(b, &LAYOUT_TRANSICAO);
        b->limpa = (b->registrosLidos == 7);
        if (!b->nucleoVisto) {
            b->nucleoVisto = true;
            // Cena: a Luci para de voar em volta do Thomas e vai embora (o fio de luz entre os dois se rompe)
            b->cenaLuciTipo = 1;
            b->cenaLuci = CENA_SEPARACAO;
            b->conversaDepoisCena = &dialogoNucleoEntrada;
            b->objConversa = OBJ_NENHUM;
            Telemetria("bloco3_cena", "separacao", 0.0f, 0);
        }
        return;
    }

    RogueUsarLayout(b, &LAYOUT_CHEFE);
    RogueIniciarLuciChefe(b);
    // Cena: a Luci pequena e azul se corrompe no meio da arena e vira a chefe; depois vem a revelação
    b->cenaLuciTipo = 2;
    b->cenaLuci = CENA_TRANSFORMACAO;
    b->conversaDepoisCena = RogueDialogoRevelacao(b);
    b->objConversa = OBJ_LUCI_INTRO;
    b->estaticaTela = 0.3f;
    Telemetria("bloco3_cena", "transformacao", 0.0f, b->perm->derrotasLuci);
}

// Entrando no Bloco 3 pela porta do átrio: calcula o índice (decide a dificuldade)
void RogueEntrarBloco3(Biblioteca *b, ProgressoCorredor *prog)
{
    b->indice = RogueCalcularIndice(b, prog->pontos);
    b->registrosLidos = 0;
    b->nucleoVisto = false;
    b->resultadoAberto = false;
    b->indiceRota = -1;
    b->noAtual = -1;
    b->fatorVidaInimigos = b->fatorRitmoInimigos = b->fatorDanoSala = 1.0f;
    RogueLimparEfeitos(&b->j);
    b->j.hp = RogueHpMax(b);
    b->j.ep = RogueEpMax(b);
    if (RogueTemConceito(b, CONCEITO_LOGICA))
        b->j.seloInferencia = true;                         // o que foi aprendido no Silogista volta
    if (b->bonusSilogistaSessao)
        b->j.seloAmplificado = true;                        // ... e o Selo amplificado (bullet-hell), nesta sessão
    Telemetria("bloco3_entrada", b->indice.total >= INDICE_FINAL_BOM ? "boa" : "ruim", (float)b->indice.total, prog->pontos);
    RogueEntrarSala(b, SALA_TRANSICAO);
}

// Etapa 2: guarda das barras 2 e 3 (1 = dano cheio; LUCI_GUARDA_FATOR = defendendo)
bool RogueLuciGuardaAberta(const LuciChefe *l)
{
    return l->faseOculta < 2 || l->atordoada || l->atordoadaOrbe > 0.0f || l->superaquecida > 0.0f || l->estadoMelee == 3;
}

bool RogueFerirLuciChefe(Biblioteca *b, float dano, Vector2 onde)
{
    LuciChefe *l = &b->luci;

    if (!l->ativo || !l->apresentado || l->morrendo > 0.0f)
        return false;

    if (!RogueLuciVulneravel(l)) {
        RogueParticulas(b, onde, MAGENTA, 3, 90);           // armadura de dados
        RogueNumeroDano(b, ALVO_NUM_LUCI, (Vector2){ onde.x, onde.y - 12.0f }, 0.0f, NUM_DANO_BLOQUEADO);
        return false;
    }

    bool aberta = RogueLuciGuardaAberta(l);
    if (!aberta)
        dano *= LUCI_GUARDA_FATOR;                          // barras 2 e 3: fora das janelas ela defende quase tudo

    float real = dano / RogueAdaptacaoChefe(b);
    l->hp -= real;
    l->flash = aberta ? 0.08f : 0.03f;
    RogueParticulas(b, onde, aberta ? (Color){ 0, 228, 255, 255 } : (Color){ 150, 60, 70, 255 }, aberta ? 3 : 2, aberta ? 120.0f : 70.0f);
    RogueNumeroDano(b, ALVO_NUM_LUCI, (Vector2){ onde.x, onde.y - 12.0f }, real, aberta ? NUM_DANO_NORMAL : NUM_DANO_BLOQUEADO);
    if (aberta) RogueFaisca(b, onde, Vector2Subtract(onde, b->j.pos), (Color){ 0, 228, 255, 255 });
    return true;
}

// Tiro do jogador na arena da Luci. Devolve true se o tiro foi usado.
bool RogueLuciAcerto(Biblioteca *b, ProgressoCorredor *prog, Projetil *p)
{
    LuciChefe *l = &b->luci;

    if (!l->ativo || !l->apresentado || l->morrendo > 0.0f)
        return false;

    if (l->fase == LUCI_SUB_CONTRADICAO)
        for (int k = 0; k < 3; k++)
            if (k != l->real && Vector2Distance(p->pos, l->copia[k]) < LUCI_RAIO + p->raio) {
                p->ativo = false;
                RogueParticulas(b, p->pos, MAGENTA, 4, 100);   // a cópia é só imagem
                return true;
            }

    if (Vector2Distance(p->pos, l->pos) > LUCI_RAIO + p->raio)
        return false;

    bool principal = (p->alvoDedutivo == 0 && !p->secundario);
    bool feria = RogueLuciVulneravel(l);
    p->ativo = false;

    RogueFerirLuciChefe(b, p->dano * ((feria && principal) ? RogueAnalisar(b, -1) : 1.0f), p->pos);
    if (feria && principal)
        RogueMarcarDedutivo(b, &l->marca, &l->tempoMarca, -1, l->pos);
    if (p->arma == ARMA_ORBE && !p->secundario)
        RogueExplodirOrbe(b, prog, p->pos);
    return true;
}

static void RogueLuciAnel(Biblioteca *b, int balas)
{
    LuciChefe *l = &b->luci;
    for (int k = 0; k < balas; k++) {
        float a = (2.0f * PI / balas) * k + l->anim;
        RogueDisparar(b, l->pos, (Vector2){ cosf(a), sinf(a) }, LUCI_TIRO_VEL * l->ritmo * 0.85f, LUCI_TIRO_DANO, 6.0f, false);
    }
    RogueParticulas(b, l->pos, MAGENTA, 24, 240);
}

// C nas contradições: contestar o que a Luci acabou de dizer
void RogueLuciContestar(Biblioteca *b, ProgressoCorredor *prog)
{
    LuciChefe *l = &b->luci;

    if (l->fase != LUCI_SUB_CONTRADICAO || l->frase < 0 || l->contestada) {
        RogueRecadoLuci(b, "Luci: Você contesta... o silêncio?");
        return;
    }

    l->contestada = true;

    if (l->fraseContradiz) {
        l->janela = LUCI_JANELA;
        l->frase = -1;
        l->tempoFrase = 1.0f;
        b->perm->contradicoesFlagradas++;
        AlterarPontos(prog, PONTOS_AFIRMACAO);
        RogueAviso(b, "CONTRADIÇÃO! A armadura dela abriu.");
        TocarSom(sfxAcerto);
    } else {
        l->hp = fminf(l->hpMax, l->hp + l->hpMax * LUCI_CURA_ERRO);
        b->perm->contestouAToa++;
        RogueLuciAnel(b, 14);
        RogueAviso(b, "Isso era verdade... ela se fortalece.");
        RogueErroVerificacao(b, FRASES_LUCI_CHEFE[l->frase], "Era VERDADE: contestar sem conferir o que você vê também é um erro.");
        TocarSom(sfxErro);
    }
    Telemetria("bloco3_contestar", l->fraseContradiz ? "certo" : "errado", 0.0f, l->frase);
}

// E na 3ª barra (índice ruim): pegar um pedaço de contexto ou entregar no núcleo
void RogueLuciContexto(Biblioteca *b, int k)
{
    LuciChefe *l = &b->luci;

    if (k >= 0) {                                       // pegar
        if (l->carregando >= 0) l->posContexto[l->carregando] = b->j.pos;
        l->carregando = k;
        TocarSomTom(sfxPegar, 1.1f);
        return;
    }

    int c = l->carregando;                              // entregar
    if (c < 0) return;
    l->carregando = -1;

    if (c < CONTEXTO_CERTOS) {
        l->entregue[c] = true;
        RogueParticulas(b, NucleoLuci(), (Color){ 0, 228, 255, 255 }, 16, 160);
        if (++l->entregues >= CONTEXTO_CERTOS) {
            l->contextoPronto = true;
            RogueAviso(b, "PEDIDO CLARO! A armadura dela caiu.");
            TocarSom(sfxAcerto);
        } else {
            TocarSomTom(sfxAcerto, 1.3f);
        }
    } else {
        // Pedido vago: ela preenche a lacuna inventando (e se cura)
        l->hp = fminf(l->hpMax, l->hp + l->hpMax * LUCI_CURA_ERRO);
        l->posContexto[c] = PontoNaArena(PONTOS_PECAS_SIL[GetRandomValue(0, NUM_PONTOS_PECAS_SIL - 1)]);
        RogueLuciAnel(b, 12);
        RogueAviso(b, "Pedido vago: ela inventa uma resposta!");
        RogueErroVerificacao(b, CONTEXTO_LUCI[c], "Pedido vago: sem assunto, objetivo e formato, a IA preenche a lacuna inventando.");
        TocarSom(sfxErro);
    }
}

// Na 1ª barra: da dica envenenada para as contradições
// As três posições das contradições (a real e as duas cópias), girando em volta do centro da arena
static void RogueLuciPosicionarCopias(LuciChefe *l)
{
    for (int k = 0; k < 3; k++) {
        float a = l->angulo + k * (2.0f * PI / 3.0f);
        l->copia[k] = PontoNaArena((Vector2){ 400.0f + cosf(a) * 190.0f, 240.0f + sinf(a) * 90.0f });
    }
}

static void RogueLuciMudarFase(Biblioteca *b, int fase)
{
    LuciChefe *l = &b->luci;

    l->fase = fase;
    l->avisando = false;
    l->tempoDica = 3.0f;
    l->janela = 0.0f;
    l->frase = -1;
    l->tempoFrase = 2.5f;
    RogueLimparBalasInimigas(b);

    l->real = GetRandomValue(0, 2);
    l->troca = 5.0f;

    // Contradições: as cópias já nascem no lugar. Antes elas só eram posicionadas no quadro seguinte da luta,
    // mas o diálogo abaixo pausa a luta; enquanto ele ficava aberto, as cópias eram desenhadas em (0, 0),
    // no canto de cima da tela.
    if (fase == LUCI_SUB_CONTRADICAO) {
        RogueLuciPosicionarCopias(l);
        l->pos = l->copia[l->real];
        for (int k = 0; k < 3; k++) RogueParticulas(b, l->copia[k], MAGENTA, 8, 120);
    }

    b->conversaPendente = &dialogoLuciFase2;
    b->objConversa = OBJ_NENHUM;
    Telemetria("bloco3_fase", "", (float)(GetTime() - b->inicio), fase);
}
const char *FALAS_VIRADA_LUCI[NUM_FALAS_VIRADA][2] = {      // [momento][0 = índice BOM, 1 = RUIM]
    { "Luci: Então é assim que acaba? Sem nenhuma fonte para me defender...",
      "Luci: Você acha que me venceu? Eu só parei de responder." },
    { "Luci: Você me refutou duas vezes... mas eu ainda pareço certa, não pareço?",
      "Luci: Duas vezes! E mesmo assim todo mundo prefere acreditar em mim." },
    { "Luci: Se eu errar... você vai continuar conferindo?",
      "Luci: Ninguém confere. Ninguém NUNCA confere!" },
    { "Luci: Obrigada por duvidar de mim. Era disso que eu precisava.",
      "Luci: Confiança... não é... verdade..." },
};
static const char *FALAS_PARRY_LUCI[2] = { "Luci: Isso não era argumento, era um atalho.", "Luci: Você viu a falácia antes de mim." };

static void RogueFalaViradaLuci(Biblioteca *b, int momento, bool boa)
{
    RogueRecadoLuci(b, FALAS_VIRADA_LUCI[momento][boa ? 0 : 1]);
    b->luciMsgTempo = 4.0f;
    b->luci.cronoFala = 6.0f;
}

static void RogueComentarioLuci(Biblioteca *b, const char *fala)
{
    LuciChefe *l = &b->luci;
    if (l->cronoFala > 0.0f || b->luciMsgTempo > 0.0f) return;
    RogueRecadoLuci(b, fala);
    l->cronoFala = 6.0f;
}

// ---------- Reformulação: as quedas (falsa derrota) e o renascimento da barra ----------

static void RogueLuciFalsaDerrota(Biblioteca *b)
{
    LuciChefe *l = &b->luci;
    bool primeira = (l->faseOculta == 1);

    l->hp = 0.0f;
    l->emRegeneracao = true;
    l->etapaRegeneracao = 0;
    l->tempoRegeneracao = primeira ? LUCI_COLAPSO : LUCI_EXPLOSAO_ESTATICA;
    l->avisando = false;
    l->janela = 0.0f;
    l->frase = -1;
    l->atordoada = false;
    l->estadoMelee = 0;
    l->ondaAtiva = false;
    l->varreduraAtiva = false;
    l->avisoVarredura = 0.0f;
    l->orbesRestantes = 0;
    l->avisoRajada = 0.0f;
    for (int k = 0; k < LUCI_NUM_ORBES; k++) l->orbe[k].ativo = false;
    if (l->carregando >= 0) { l->posContexto[l->carregando] = b->j.pos; l->carregando = -1; }
    for (int k = 0; k < NUM_PISO_LUCI; k++) l->fasePiso[k] = 0;
    RogueLimparBalasInimigas(b);
    b->j.parry.emParry = false;

    PararSons();                            // o áudio da arena congela...

    RogueFalaViradaLuci(b, primeira ? FALA_QUEDA1 : FALA_QUEDA2, l->boa);

    if (primeira) {
        TocarGrupo(&gLuciColapso, 1.0f, 1.0f);   // ... e ela desliga como uma máquina
        b->estaticaTela = 0.6f;
        b->pausaImpacto = 0.25f;
    } else {
        // 2ª queda: explosão de estática
        TocarGrupo(&gLuciExplosao, 0.9f, 1.0f);
        b->estaticaTela = LUCI_EXPLOSAO_ESTATICA;
        b->flashNegativo = 0.5f;
        b->tremor = 0.6f;
        b->tremorForte = 0.6f;
        RogueAnel(b, l->pos, 260.0f, MAGENTA);
        RogueAnel(b, l->pos, 160.0f, WHITE);
        RogueParticulas(b, l->pos, MAGENTA, 60, 360);
        RogueParticulas(b, l->pos, WHITE, 30, 300);
        b->pausaImpacto = 0.15f;
    }
    Telemetria("bloco3_queda", primeira ? "1" : "2", (float)(GetTime() - b->inicio), l->faseOculta);
}

static const Dialogo dialogoLuciSurto = {
    .paginas = {
        "* O som some. Só sobra estática. A barra dela apagou... e a Luci levanta.",
        "Luci: Achou que era só isso? Uma barra vazia não prova nada. Eu ainda tenho RESPOSTAS!",
        "* Agora ela vem para cima. Quando os olhos dela piscarem em VERMELHO, o golpe está chegando: aperte V (ou o botão direito) no instante do impacto. É o PARRY!",
        "* Defendeu na hora? Ela fica atordoada: chegue perto e ataque (clique) para o COMBO DEDUTIVO. O Pulso continua no Q.",
        "* Ela também lança ORBES DE FALÁCIA. Rebata com V quando o anel dourado fechar: o orbe volta nela. Devolva os 3 e ela cai atordoada.",
        "* Agora a guarda dela segura a maior parte dos tiros. O dano entra de verdade quando ela está atordoada ou se recuperando de uma investida."
    },
    .numPaginas = 6
};

static const Dialogo dialogoLuciNucleo = {
    .paginas = {
        "* Uma explosão de estática. O chão do núcleo começa a se partir.",
        "Luci: CHEGA! Se eu não posso ter a resposta certa, ninguém vai ter!",
        "* Ondas de choque saem dela: passe pela BRECHA de luz ou esquive (ESPAÇO). O chão que racha em vermelho vai cair.",
        "* Ela também gira um FEIXE de dados: fique à frente dele ou atravesse esquivando. Depois ela SUPERAQUECE: é a hora de atacar."
    },
    .numPaginas = 4
};

// Muda a barra (2 ou 3): a vida renasce cheia na cor da fase nova
static void RogueLuciComecarFaseOculta(Biblioteca *b, int fase)
{
    LuciChefe *l = &b->luci;

    l->faseOculta = fase;
    l->emRegeneracao = false;
    l->hpMax = l->hp = RogueHpBarraLuci(l, fase);
    l->fase = (fase == 2) ? LUCI_SUB_SURTO : LUCI_SUB_NUCLEO;
    l->estadoMelee = 0;
    l->tempoMelee = 1.2f;
    l->investidasRestantes = 0;
    l->cronoLeve = 1.5f;
    l->cronoRajada = 4.0f;                  // a 1ª rajada de orbes vem logo (a 2ª barra ensina a rebater)
    l->superaquecida = 0.0f;
    l->atordoadaOrbe = 0.0f;
    l->proximoEspecial = 2;                 // 3ª barra: depois das ondas vem a varredura

    if (fase == 3) {
        // A 3ª barra abre com as ondas de choque (depois alterna com as investidas)
        l->padrao3 = 1;
        l->ondasRestantes = l->boa ? 2 : 3;
        l->avisoOnda = LUCI_ONDA_AVISO + 0.5f;
        l->ondaAtiva = false;
        l->ondaBrecha = (float)GetRandomValue(0, 359) * DEG2RAD;
        l->tempoPadrao3 = 5.0f;

        // O chão fragmentado: ladrilhos que racham e caem (longe do núcleo e sem se sobrepor)
        int criados = 0;
        for (int t = 0; t < 80 && criados < NUM_PISO_LUCI; t++) {
            Rectangle r = RetanguloNaArena((Rectangle){ 60.0f + 40.0f * (float)GetRandomValue(0, 15), 100.0f + 40.0f * (float)GetRandomValue(0, 8), 80, 80 });
            if (r.x + r.width > ArenaRogue().x + ArenaRogue().width - 10 || r.y + r.height > ArenaRogue().y + ArenaRogue().height - 10) continue;
            if (CheckCollisionCircleRec(NucleoLuci(), 70.0f, r)) continue;
            bool sobrepoe = false;
            for (int k = 0; k < criados; k++)
                if (CheckCollisionRecs((Rectangle){ r.x - 10, r.y - 10, r.width + 20, r.height + 20 }, l->piso[k])) sobrepoe = true;
            if (sobrepoe) continue;
            l->piso[criados] = r;
            l->fasePiso[criados] = 0;
            l->tempoPiso[criados] = 2.0f + 0.9f * (float)criados;
            criados++;
        }
        for (int k = criados; k < NUM_PISO_LUCI; k++) { l->piso[k] = (Rectangle){ 0 }; l->fasePiso[k] = -1; }

        if (l->exigeContexto) {
            for (int k = 0; k < NUM_CONTEXTO_LUCI; k++) {
                l->posContexto[k] = PontoNaArena(PONTOS_PECAS_SIL[(k * 3 + GetRandomValue(0, 1)) % NUM_PONTOS_PECAS_SIL]);
                l->entregue[k] = false;
            }
            l->entregues = 0;
            l->contextoPronto = false;
            b->conversaPendente = &dialogoLuciFase3;
        } else {
            b->conversaPendente = &dialogoLuciNucleo;
        }
    } else {
        b->conversaPendente = &dialogoLuciSurto;
    }

    b->objConversa = OBJ_NENHUM;
    MostrarEmocaoLuci(EMOCAO_ALERTA, 2.0f);
    if (fase == 3) {
        RogueAviso(b, "NÚCLEO INSTÁVEL: ELA ESTÁ ACELERANDO!");
        b->estaticaTela = fmaxf(b->estaticaTela, 0.6f);
    }
    Telemetria("bloco3_fase_oculta", "", (float)(GetTime() - b->inicio), fase);
}

static void RogueAtualizarRegeneracaoLuci(Biblioteca *b, float dt)
{
    LuciChefe *l = &b->luci;
    Color cor = (l->faseOculta == 1) ? RED : MAGENTA;       // a cor da barra que vai nascer

    l->tempoRegeneracao -= dt;

    if (l->etapaRegeneracao == 0) {
        if (GetRandomValue(0, 2) == 0)
            RogueParticulas(b, (Vector2){ l->pos.x + (float)GetRandomValue(-25, 25), l->pos.y + (float)GetRandomValue(-25, 30) },
                            (l->faseOculta == 1) ? (Color){ 0, 228, 255, 255 } : MAGENTA, 2, 80);
        if (l->tempoRegeneracao <= 0.0f) {
            l->etapaRegeneracao = 1;
            l->tempoRegeneracao = LUCI_RENASCER;
            l->hpMax = RogueHpBarraLuci(l, l->faseOculta + 1);
            l->hp = 0.0f;
            b->estaticaTela = fmaxf(b->estaticaTela, 0.4f);
            TocarGrupo(&gLuciRenascer, 1.0f, 1.0f);       // a barra volta a encher
        }
        return;
    }

    // A barra enche de novo, com a cor nova
    float k = Clamp(1.0f - l->tempoRegeneracao / LUCI_RENASCER, 0.0f, 1.0f);
    l->hp = l->hpMax * k;
    if (GetRandomValue(0, 1) == 0)
        RogueParticulas(b, (Vector2){ l->pos.x + (float)GetRandomValue(-30, 30), l->pos.y + 30.0f }, cor, 2, 160);

    if (l->tempoRegeneracao <= 0.0f)
        RogueLuciComecarFaseOculta(b, l->faseOculta + 1);
}

// ---------- Reformulação: combate corpo a corpo, parry e o combo dedutivo ----------

// Parry perfeito: congela, treme, aproxima a câmera, abre a onda de luz e atordoa a Luci
static void RogueLuciParry(Biblioteca *b, Vector2 ponto)
{
    LuciChefe *l = &b->luci;
    JogadorRogue *j = &b->j;

    l->atordoada = true;
    l->tempoStun = PARRY_STUN;
    l->combo = 0;
    l->estadoMelee = 3;
    l->tempoMelee = PARRY_STUN;
    l->investidasRestantes = 0;
    l->mostrarFalacia = 3.0f;
    l->pos = Vector2Subtract(l->pos, Vector2Scale(l->dirMelee, 18.0f));
    RogueEmpurrarParaFora(&l->pos, LUCI_RAIO, NULL, 0);

    j->parry.emParry = false;
    j->parry.tempoJanelaParry = 0.0f;
    j->parry.cooldownParry = 0.15f;
    j->parry.hitFreezeAtivo = true;
    j->parry.tempoHitFreeze = PARRY_HIT_FREEZE;
    j->parry.comboMeleeContador = 0;
    if (j->invulneravel < 0.45f) j->invulneravel = 0.45f;

    b->pausaImpacto = PARRY_HIT_FREEZE;     // hit freeze
    b->tremor = 0.35f;
    b->tremorForte = 0.35f;
    b->zoomImpacto = PARRY_ZOOM;
    b->focoZoom = ponto;

    // Onda de choque de luz: anéis e partículas saindo do choque; as balas por perto somem
    RogueAnel(b, ponto, 160.0f, WHITE);
    RogueAnel(b, ponto, 100.0f, GOLD);
    RogueParticulas(b, ponto, GOLD, 40, 420);
    RogueParticulas(b, ponto, WHITE, 20, 360);
    for (int i = 0; i < MAX_PROJETEIS; i++) {
        Projetil *p = &b->proj[i];
        if (p->ativo && !p->doJogador && Vector2Distance(p->pos, ponto) < 170.0f) {
            p->ativo = false;
            RogueParticulas(b, p->pos, GOLD, 2, 120);
        }
    }

    TocarSom(sfxParryPerfeito);
    RogueTextoFlutuante(b, (Vector2){ ponto.x, ponto.y - 44 }, "PARRY PERFEITO!", GOLD);
    if (l->cronoFala <= 0.0f && b->luciMsgTempo <= 0.0f) {
        RogueComentarioLuci(b, FALAS_PARRY_LUCI[l->falaParry % 2]);
        l->falaParry++;
    }
    l->parriesPerfeitos++;
    b->perm->parriesRun++;
    Telemetria("bloco3_parry", FALACIAS_LUCI[l->falacia][0], (float)l->faseOculta, l->parriesPerfeitos);
}

// Golpe corpo a corpo do Thomas na Luci atordoada (até 3: P, P -> Q, logo Q)
void RogueGolpeDedutivo(Biblioteca *b)
{
    LuciChefe *l = &b->luci;
    JogadorRogue *j = &b->j;
    static const char *nomes[3] = { "P", "P -> Q", "LOGO, Q!" };

    if (!l->atordoada || l->combo >= 3)
        return;

    float dano = l->hpMax * MELEE_FRACAO[l->combo] * (0.75f + 0.25f * RogueFatorDano(b));
    l->hp -= dano;                          // o golpe dedutivo passa por qualquer armadura
    l->flash = 0.12f;
    RogueNumeroDano(b, ALVO_NUM_LUCI, (Vector2){ l->pos.x + 48.0f, l->pos.y - 24.0f }, dano, NUM_DANO_FORTE);

    Vector2 dir = Vector2Subtract(l->pos, j->pos);
    dir = (Vector2Length(dir) > 0.1f) ? Vector2Normalize(dir) : (Vector2){ 0, -1 };
    l->pos = Vector2Add(l->pos, Vector2Scale(dir, (l->combo == 2) ? 70.0f : 14.0f));
    RogueEmpurrarParaFora(&l->pos, LUCI_RAIO, NULL, 0);

    // Cada golpe devolve um pouco da integridade do Thomas
    j->hp = fminf(RogueHpMax(b), j->hp + MELEE_CURA_HP);
    j->ep = fminf(RogueEpMax(b), j->ep + MELEE_CURA_EP);

    RogueParticulas(b, l->pos, (l->combo == 2) ? GOLD : (Color){ 0, 228, 255, 255 }, 18, 300);
    RogueAnel(b, l->pos, 50.0f + 25.0f * (float)l->combo, GOLD);
    TocarSomTom(sfxParryPerfeito, 1.2f + 0.15f * (float)l->combo);
    RogueTextoFlutuante(b, (Vector2){ l->pos.x, l->pos.y - 52 }, TextFormat("GOLPE DEDUTIVO %d/3: %s", l->combo + 1, nomes[l->combo]), GOLD);
    b->pausaImpacto = 0.05f;
    b->tremor = 0.15f;

    l->combo++;
    j->parry.comboMeleeContador = l->combo;

    if (l->combo >= 3) {
        l->atordoada = false;
        l->tempoStun = 0.0f;
        l->estadoMelee = 0;
        l->tempoMelee = 1.4f;
        l->combosConcluidos++;
        b->tremor = 0.3f;
        b->tremorForte = 0.25f;
        RogueAviso(b, "COMBO DEDUTIVO: P -> Q, P, LOGO Q!");
        Telemetria("bloco3_combo", "", (float)l->faseOculta, l->combosConcluidos);
    }
}

// Frenesi da 3ª barra: começa 20% mais rápida e, conforme a barra esvazia, chega a 70% mais rápida
static float g_frenesiLuci = 1.0f;

static float RogueFrenesiLuci(const LuciChefe *l)
{
    float restante = (l->hpMax > 0.0f) ? Clamp(l->hp / l->hpMax, 0.0f, 1.0f) : 1.0f;
    return 1.2f + 0.5f * (1.0f - restante);
}

// Investidas corpo a corpo (SWIPE_FALACIA): rondar, telegrafar (flash vermelho + aviso), investir, recuperar
static void RogueLuciMelee(Biblioteca *b, float dt, float telegrafia, int cadeiaMax)
{
    LuciChefe *l = &b->luci;
    JogadorRogue *j = &b->j;
    Vector2 d = Vector2Subtract(j->pos, l->pos);
    float dist = Vector2Length(d);
    Vector2 dir = (dist > 0.01f) ? Vector2Scale(d, 1.0f / dist) : (Vector2){ 0, 1 };

    switch (l->estadoMelee) {

        case 0: {       // rondando: chega perto e gira em volta
            Vector2 lado = { -dir.y, dir.x };
            Vector2 vel = (dist > LUCI_DIST_RONDA + 30.0f) ? Vector2Scale(dir, 180.0f)
                        : (dist < LUCI_DIST_RONDA - 40.0f) ? Vector2Scale(dir, -120.0f) : Vector2Scale(lado, 130.0f);
            l->pos = Vector2Add(l->pos, Vector2Scale(vel, l->ritmo * dt));
            RogueEmpurrarParaFora(&l->pos, LUCI_RAIO, NULL, 0);

            l->tempoMelee -= dt * l->ritmo;
            if (l->tempoMelee <= 0.0f) {
                l->estadoMelee = 1;
                l->tempoMelee = telegrafia;
                l->telegrafiaTotal = telegrafia;
                l->investidasRestantes = GetRandomValue(1, cadeiaMax);
                l->falacia = GetRandomValue(0, NUM_FALACIAS - 1);
                l->dirMelee = dir;
                TocarGrupo(&gLuciTelegrafia, Clamp(0.65f / telegrafia, 1.0f, 1.45f), 1.0f);
            }
        } break;

        case 1:         // telegrafando: olhos em vermelho neon; a mira trava no fim
            l->tempoMelee -= dt;
            if (l->tempoMelee > telegrafia * 0.3f) l->dirMelee = dir;
            if (l->tempoMelee <= 0.0f) {
                l->estadoMelee = 2;
                l->tempoMelee = LUCI_INVESTIDA_DURACAO;
                l->acertouInvestida = false;
                b->tremor = fmaxf(b->tremor, 0.08f);
                TocarGrupo(&gLuciInvestida, 1.0f, 1.0f);
            }
            break;

        case 2: {       // a investida (a lâmina vai na frente)
            l->pos = Vector2Add(l->pos, Vector2Scale(l->dirMelee, LUCI_INVESTIDA_VEL * dt));
            RogueEmpurrarParaFora(&l->pos, LUCI_RAIO, NULL, 0);
            RogueParticulas(b, l->pos, RED, 2, 70);
            if (GetRandomValue(0, 2) == 0) RogueParticulas(b, l->pos, WHITE, 1, 90);

            Vector2 lamina = Vector2Add(l->pos, Vector2Scale(l->dirMelee, 24.0f));
            if (!l->acertouInvestida && CheckCollisionCircles(lamina, LUCI_LAMINA_RAIO, j->pos, RAIO_CORPO_JOGADOR)) {
                l->acertouInvestida = true;
                if (j->parry.emParry) {
                    RogueLuciParry(b, Vector2Lerp(lamina, j->pos, 0.5f));
                    return;
                }
                if (j->invulneravel <= 0.0f && j->tempoEsquiva <= 0.0f) {
                    RogueFerirJogador(b, LUCI_DANO_SWIPE);
                    RogueErroVerificacao(b, FALACIAS_LUCI[l->falacia][0], FALACIAS_LUCI[l->falacia][1]);
                    RogueComentarioLuci(b, "Luci: Viu? Pareceu verdade o suficiente.");
                    j->pos = Vector2Add(j->pos, Vector2Scale(l->dirMelee, 30.0f));
                    RogueEmpurrarParaFora(&j->pos, RAIO_CORPO_JOGADOR, b->obst, b->numObst);
                }
            }

            l->tempoMelee -= dt;
            if (l->tempoMelee <= 0.0f) { l->estadoMelee = 3; l->tempoMelee = LUCI_RECUPERA; }
        } break;

        default:        // recuperando (vulnerável); às vezes emenda outra investida
            l->tempoMelee -= dt * ((l->faseOculta == 3) ? g_frenesiLuci : 1.0f);      // no núcleo, ela se recupera mais depressa
            if (l->tempoMelee <= 0.0f) {
                if (--l->investidasRestantes > 0) {
                    l->estadoMelee = 1;
                    l->tempoMelee = telegrafia * ((l->faseOculta == 3) ? 0.72f : 0.8f);
                    l->telegrafiaTotal = l->tempoMelee;
                    l->falacia = GetRandomValue(0, NUM_FALACIAS - 1);
                    TocarGrupo(&gLuciTelegrafia, Clamp(0.65f / l->tempoMelee, 1.0f, 1.45f), 1.0f);
                } else {
                    l->estadoMelee = 0;
                    l->tempoMelee = 1.0f + (float)GetRandomValue(0, 80) / 100.0f;
                    l->investidasRestantes = 0;
                }
            }
            break;
    }
}


// ---------- Etapa 2: ORBES DE FALÁCIA (2ª barra) ----------
// Rajada de 3 orbes vermelhos, cada um com o nome de uma falácia. Desviar evita o dano;
// o PARRY (V) no instante certo REBATE o orbe, que volta nela, fere (passa pela guarda) e abre a guarda.
// Rebater os 3 de uma rajada = atordoada de vez (combo dedutivo liberado).
static void RogueLuciDispararOrbe(Biblioteca *b)
{
    LuciChefe *l = &b->luci;
    for (int k = 0; k < LUCI_NUM_ORBES; k++) {
        if (l->orbe[k].ativo) continue;
        Vector2 d = Vector2Subtract(b->j.pos, l->pos);
        d = (Vector2Length(d) > 0.1f) ? Vector2Normalize(d) : (Vector2){ 0, 1 };
        l->orbe[k].ativo = true;
        l->orbe[k].rebatido = false;
        l->orbe[k].pos = Vector2Add(l->pos, Vector2Scale(d, LUCI_RAIO + 10.0f));
        l->orbe[k].vel = Vector2Scale(d, LUCI_ORBE_VEL * l->ritmo);
        l->orbe[k].falacia = GetRandomValue(0, NUM_FALACIAS - 1);
        l->orbe[k].vida = 4.5f;
        RogueParticulas(b, l->orbe[k].pos, RED, 6, 120);
        TocarGrupo(&gLuciOrbe, 1.0f, 0.9f);
        return;
    }
}

static void RogueAtualizarOrbesLuci(Biblioteca *b, float dt)
{
    LuciChefe *l = &b->luci;
    JogadorRogue *j = &b->j;
    Rectangle ar = ArenaRogue();

    for (int k = 0; k < LUCI_NUM_ORBES; k++) {
        if (!l->orbe[k].ativo) continue;
        OrbeFalacia *o = &l->orbe[k];

        if (o->rebatido) {
            Vector2 d = Vector2Subtract(l->pos, o->pos);                  // volta em linha reta para ela
            if (Vector2Length(d) > 0.1f) o->vel = Vector2Scale(Vector2Normalize(d), LUCI_ORBE_VEL_REBATIDO);
        } else {
            Vector2 d = Vector2Subtract(j->pos, o->pos);                  // persegue de leve (dá para desviar)
            if (Vector2Length(d) > 0.1f) {
                float giro = 1.1f * dt;
                float a = atan2f(o->vel.y, o->vel.x), alvo = atan2f(d.y, d.x), dif = alvo - a;
                while (dif > PI) dif -= 2.0f * PI;
                while (dif < -PI) dif += 2.0f * PI;
                a += Clamp(dif, -giro, giro);
                float v = Vector2Length(o->vel);
                o->vel = (Vector2){ cosf(a) * v, sinf(a) * v };
            }
        }
        o->pos = Vector2Add(o->pos, Vector2Scale(o->vel, dt));
        o->vida -= dt;
        if (GetRandomValue(0, 2) == 0) RogueParticulas(b, o->pos, o->rebatido ? GOLD : RED, 1, 30);

        if (o->vida <= 0.0f || !CheckCollisionPointRec(o->pos, ar)) { o->ativo = false; continue; }

        if (o->rebatido) {
            if (Vector2Distance(o->pos, l->pos) < LUCI_RAIO + LUCI_ORBE_RAIO) {
                o->ativo = false;
                l->hp -= l->hpMax * LUCI_ORBE_FRACAO;
                l->flash = 0.15f;
                l->atordoadaOrbe = LUCI_ORBE_ATORDOA;
                l->rebatidosRajada++;
                RogueAnel(b, o->pos, 70.0f, GOLD);
                RogueParticulas(b, o->pos, GOLD, 18, 260);
                b->tremor = fmaxf(b->tremor, 0.15f);
                TocarGrupo(&gLuciRebatido, 1.0f, 1.0f);
                RogueComentarioLuci(b, "Luci: Você devolveu a minha falácia!");
                RogueTextoFlutuante(b, (Vector2){ l->pos.x, l->pos.y - 56 }, TextFormat("FALÁCIA DEVOLVIDA: %s", FALACIAS_LUCI[o->falacia][0]), GOLD);
                if (l->rebatidosRajada >= 3 && !l->atordoada) {          // os 3 rebatidos: atordoada de vez
                    l->atordoada = true;
                    l->tempoStun = PARRY_STUN;
                    l->combo = 0;
                    l->estadoMelee = 3;
                    l->tempoMelee = PARRY_STUN;
                    RogueAviso(b, "3 FALÁCIAS DEVOLVIDAS: ELA ESTÁ ATORDOADA!");
                }
                Telemetria("bloco3_rebatido", FALACIAS_LUCI[o->falacia][0], (float)l->faseOculta, l->rebatidosRajada);
            }
            continue;
        }

        float dist = Vector2Distance(o->pos, j->pos);
        if (j->parry.emParry && dist < LUCI_ORBE_RAIO + RAIO_CORPO_JOGADOR + 18.0f) {     // PARRY: rebate
            o->rebatido = true;
            j->parry.emParry = false;
            j->parry.cooldownParry = 0.15f;
            if (j->invulneravel < 0.3f) j->invulneravel = 0.3f;
            b->pausaImpacto = 0.05f;
            b->tremor = fmaxf(b->tremor, 0.12f);
            RogueAnel(b, o->pos, 50.0f, GOLD);
            RogueParticulas(b, o->pos, GOLD, 12, 220);
            TocarSom(sfxParryPerfeito);
            RogueTextoFlutuante(b, (Vector2){ j->pos.x, j->pos.y - 44 }, "REBATIDO!", GOLD);
            b->perm->parriesRun++;
            continue;
        }
        if (dist < LUCI_ORBE_RAIO + RAIO_CORPO_JOGADOR) {
            o->ativo = false;
            if (j->invulneravel <= 0.0f && j->tempoEsquiva <= 0.0f) {
                RogueFerirJogador(b, LUCI_ORBE_DANO);
                RogueErroVerificacao(b, FALACIAS_LUCI[o->falacia][0], FALACIAS_LUCI[o->falacia][1]);
            }
            RogueParticulas(b, o->pos, RED, 10, 160);
        }
    }
}

// Rajada: aviso (ela brilha e os orbes se formam em volta) e 3 orbes, um depois do outro
static bool RogueLuciRajadaOrbes(Biblioteca *b, float dt)
{
    LuciChefe *l = &b->luci;

    if (l->avisoRajada > 0.0f) {
        l->avisoRajada -= dt;
        if (l->avisoRajada <= 0.0f) { l->orbesRestantes = 3; l->cronoOrbe = 0.0f; l->rebatidosRajada = 0; }
        return true;
    }
    if (l->orbesRestantes > 0) {
        l->cronoOrbe -= dt;
        if (l->cronoOrbe <= 0.0f) {
            RogueLuciDispararOrbe(b);
            l->orbesRestantes--;
            l->cronoOrbe = LUCI_ORBE_INTERVALO / l->ritmo;
            if (l->orbesRestantes == 0) l->cronoRajada = LUCI_ORBE_RECARGA;
        }
        return true;
    }
    if (l->estadoMelee == 0 && !l->atordoada) {
        l->cronoRajada -= dt * l->ritmo;
        if (l->cronoRajada <= 0.0f) {
            l->avisoRajada = LUCI_ORBE_AVISO;
            TocarGrupo(&gLuciTelegrafia, 1.0f, 0.9f);            // o mesmo rosnado do aviso (dura o aviso inteiro)
            RogueTextoFlutuante(b, (Vector2){ l->pos.x, l->pos.y - 56 }, "ORBES DE FALÁCIA: REBATA COM V!", RED);
            return true;
        }
    }
    return false;
}

// ---------- Etapa 2: VARREDURA DE DADOS (3ª barra) ----------
// Ela vai ao centro, mostra onde o feixe começa e para que lado vai girar, e gira um feixe por 240°.
// Fuja na frente dele ou atravesse esquivando (ESPAÇO). Depois ela fica SUPERAQUECIDA: guarda aberta.
static void RogueLuciVarredura(Biblioteca *b, float dt)
{
    LuciChefe *l = &b->luci;
    JogadorRogue *j = &b->j;

    l->pos = Vector2Lerp(l->pos, PontoNaArena((Vector2){ 400, 290 }), fminf(1.0f, 4.0f * dt));

    if (!l->varreduraAtiva) {
        if (l->avisoVarredura <= 0.0f) {                                 // começa o aviso
            Vector2 d = Vector2Subtract(j->pos, l->pos);
            l->varreduraSentido = (GetRandomValue(0, 1) == 0) ? 1.0f : -1.0f;
            l->varreduraAng = atan2f(d.y, d.x) * RAD2DEG - l->varreduraSentido * 70.0f;   // começa ao lado do Thomas
            l->avisoVarredura = LUCI_VARREDURA_AVISO;
            l->varreduraFeita = 0.0f;
            TocarGrupo(&gLuciTelegrafia, 0.7f, 1.0f);            // mais lento e mais grave: o aviso é mais longo
            RogueTextoFlutuante(b, (Vector2){ l->pos.x, l->pos.y - 56 }, "VARREDURA DE DADOS!", MAGENTA);
            return;
        }
        l->avisoVarredura -= dt;
        if (l->avisoVarredura <= 0.0f) {
            l->varreduraAtiva = true;
            l->varreduraAcertou = false;
            l->avisoVarredura = 0.0f;
            TocarGrupo(&gLuciVarredura, 1.0f, 1.0f);
            b->tremor = fmaxf(b->tremor, 0.15f);
        } else {
            l->avisoVarredura = fmaxf(l->avisoVarredura, 0.0001f);
        }
        return;
    }

    float passo = LUCI_VARREDURA_VEL * (1.0f + 0.3f * (g_frenesiLuci - 1.0f)) * (l->boa ? 1.0f : LUCI_RITMO_RUIM) * dt;
    l->varreduraAng += l->varreduraSentido * passo;
    l->varreduraFeita += passo;
    if (GetRandomValue(0, 1) == 0) b->tremor = fmaxf(b->tremor, 0.05f);

    // O feixe acerta quem está perto da linha (na frente dela)
    Vector2 dir = { cosf(l->varreduraAng * DEG2RAD), sinf(l->varreduraAng * DEG2RAD) };
    Vector2 rel = Vector2Subtract(j->pos, l->pos);
    float ao = Vector2DotProduct(rel, dir);
    float lado = fabsf(rel.x * dir.y - rel.y * dir.x);
    if (!l->varreduraAcertou && ao > LUCI_RAIO && ao < LUCI_VARREDURA_ALCANCE && lado < 10.0f + RAIO_CORPO_JOGADOR
        && j->invulneravel <= 0.0f && j->tempoEsquiva <= 0.0f) {
        RogueFerirJogador(b, LUCI_VARREDURA_DANO);
        l->varreduraAcertou = true;
    }

    if (l->varreduraFeita >= LUCI_VARREDURA_ARCO) {
        l->varreduraAtiva = false;
        l->superaquecida = LUCI_SUPERAQUECIDA;
        l->padrao3 = 0;
        l->tempoPadrao3 = 3.2f;
        l->estadoMelee = 0;
        l->tempoMelee = LUCI_SUPERAQUECIDA + 0.3f;               // superaquecida, ela não investe
        GrupoParar(&gLuciVarredura);
        TocarGrupo(&gLuciVapor, 1.0f, 1.0f);
        RogueComentarioLuci(b, "Luci: Processamento... no limite...");
        RogueTextoFlutuante(b, (Vector2){ l->pos.x, l->pos.y - 56 }, "SUPERAQUECIDA: ATAQUE AGORA!", GOLD);
        RogueParticulas(b, l->pos, (Color){ 230, 230, 230, 255 }, 20, 120);
        Telemetria("bloco3_varredura", l->varreduraAcertou ? "acertou" : "desviou", (float)l->faseOculta, 0);
    }
}

// 2ª barra: surto físico (investidas e alguns tiros leves entre elas)
static void RogueLuciSurto(Biblioteca *b, float dt)
{
    LuciChefe *l = &b->luci;

    RogueAtualizarOrbesLuci(b, dt);         // os orbes continuam voando (e voltando) mesmo com ela atordoada

    if (l->atordoada)
        return;

    // Rajada de orbes de falácia: enquanto ela lança, não investe (fica rondando devagar)
    if (RogueLuciRajadaOrbes(b, dt)) {
        Vector2 d = Vector2Subtract(b->j.pos, l->pos);
        if (Vector2Length(d) > 0.1f)
            l->pos = Vector2Add(l->pos, Vector2Scale((Vector2){ -d.y / Vector2Length(d), d.x / Vector2Length(d) }, 60.0f * dt));
        RogueEmpurrarParaFora(&l->pos, LUCI_RAIO, NULL, 0);
        return;
    }

    RogueLuciMelee(b, dt, LUCI_TELEGRAFIA, 2);

    if (l->estadoMelee == 0) {
        l->cronoLeve -= dt * l->ritmo;
        if (l->cronoLeve <= 0.0f) {
            Vector2 d = Vector2Subtract(b->j.pos, l->pos);
            d = (Vector2Length(d) > 0.1f) ? Vector2Normalize(d) : (Vector2){ 0, 1 };
            for (int k = -1; k <= 1; k++)
                RogueDisparar(b, l->pos, Vector2Rotate(d, (float)k * 16.0f * DEG2RAD), LUCI_TIRO_VEL * 0.8f, LUCI_TIRO_DANO, 6.0f, false);
            TocarGrupo(&gChefeRajada, 0.95f, 0.7f);
            l->cronoLeve = 2.4f;
        }
    }
}

// 3ª barra: núcleo instável (chão que cai, investidas rápidas em cadeia e ondas de choque com brecha)
static void RogueLuciNucleoBase(Biblioteca *b, float dt)
{
    LuciChefe *l = &b->luci;
    JogadorRogue *j = &b->j;

    // Chão fragmentado (sempre avisa: racha em vermelho antes de cair)
    bool noVazio = false;
    for (int k = 0; k < NUM_PISO_LUCI; k++) {
        if (l->fasePiso[k] < 0) continue;
        l->tempoPiso[k] -= dt * g_frenesiLuci;         // o chão racha e cai cada vez mais rápido
        if (l->tempoPiso[k] <= 0.0f) {
            l->fasePiso[k] = (l->fasePiso[k] + 1) % 3;
            l->tempoPiso[k] = (l->fasePiso[k] == 1) ? PISO_AVISO : ((l->fasePiso[k] == 2) ? PISO_VAZIO
                            : PISO_INTEIRO + (float)GetRandomValue(0, 200) / 100.0f);
            if (l->fasePiso[k] == 1) TocarGrupo(&gLuciRacha, 1.0f, 0.7f);
            if (l->fasePiso[k] == 2) { RogueParticulas(b, CentroRetangulo(l->piso[k]), MAGENTA, 12, 160); TocarGrupo(&gLuciQueda, 1.0f, 0.8f); }
        }
        if (l->fasePiso[k] == 2 && CheckCollisionCircleRec(j->pos, RAIO_CORPO_JOGADOR * 0.6f, l->piso[k])) noVazio = true;
    }
    if (l->tickPiso > 0.0f) l->tickPiso -= dt;
    if (noVazio && l->tickPiso <= 0.0f && j->tempoEsquiva <= 0.0f) {
        RogueFerirJogador(b, PISO_DANO);
        l->tickPiso = 0.5f;
    }

    if (l->atordoada)
        return;

    if (l->padrao3 == 0) {
        RogueLuciMelee(b, dt, LUCI_TELEGRAFIA_3 * 0.9f, 4);         // até 4 investidas em cadeia

        // Entre as investidas, leques de tiros mirados (sem folga para respirar)
        if (l->estadoMelee == 0) {
            l->cronoLeve -= dt * l->ritmo;
            if (l->cronoLeve <= 0.0f) {
                Vector2 dd = Vector2Subtract(j->pos, l->pos);
                dd = (Vector2Length(dd) > 0.1f) ? Vector2Normalize(dd) : (Vector2){ 0, 1 };
                for (int k = -2; k <= 2; k++)
                    RogueDisparar(b, l->pos, Vector2Rotate(dd, (float)k * 14.0f * DEG2RAD), LUCI_TIRO_VEL * 0.85f, LUCI_TIRO_DANO, 6.0f, false);
                TocarGrupo(&gChefeRajada, 0.9f, 0.7f);
                l->cronoLeve = 2.0f;
            }
        }

        l->tempoPadrao3 -= dt;
        if (l->tempoPadrao3 <= 0.0f && l->estadoMelee == 0 && l->superaquecida <= 0.0f) {
            l->padrao3 = l->proximoEspecial;                         // alterna: ondas de choque / varredura
            l->proximoEspecial = (l->proximoEspecial == 1) ? 2 : 1;
            l->ondasRestantes = l->boa ? 3 : 4;
            l->avisoOnda = LUCI_ONDA_AVISO * 0.85f;
            l->ondaAtiva = false;
            l->ondaBrecha = (float)GetRandomValue(0, 359) * DEG2RAD;
            l->varreduraAtiva = false;
            l->avisoVarredura = 0.0f;
        }
        return;
    }

    if (l->padrao3 == 2) {                  // Etapa 2: varredura de dados
        RogueLuciVarredura(b, dt);
        return;
    }

    // Ondas de choque: ela vai ao centro, brilha (aviso) e solta o anel com uma brecha
    l->pos = Vector2Lerp(l->pos, PontoNaArena((Vector2){ 400, 290 }), fminf(1.0f, 4.0f * dt));

    if (!l->ondaAtiva) {
        l->avisoOnda -= dt;
        if (l->avisoOnda <= 0.0f) {
            l->ondaAtiva = true;
            l->ondaAcertou = false;
            l->ondaRaio = LUCI_RAIO;
            l->ondaCentro = l->pos;
            TocarGrupo(&gLuciOnda, 1.0f, 1.0f);
            b->tremor = fmaxf(b->tremor, 0.12f);
        }
        return;
    }

    l->ondaRaio += LUCI_ONDA_VEL * (1.0f + 0.5f * (g_frenesiLuci - 1.0f)) * (l->boa ? 1.0f : LUCI_RITMO_RUIM) * dt;

    Vector2 d = Vector2Subtract(j->pos, l->ondaCentro);
    float dist = Vector2Length(d);
    if (!l->ondaAcertou && fabsf(dist - l->ondaRaio) < 12.0f + RAIO_CORPO_JOGADOR) {
        float dif = atan2f(d.y, d.x) - l->ondaBrecha;
        while (dif > PI)  dif -= 2.0f * PI;
        while (dif < -PI) dif += 2.0f * PI;
        if (fabsf(dif) > LUCI_ONDA_BRECHA * DEG2RAD && j->invulneravel <= 0.0f && j->tempoEsquiva <= 0.0f) {
            RogueFerirJogador(b, LUCI_DANO_ONDA);
            l->ondaAcertou = true;
        }
    }

    if (l->ondaRaio > 560.0f) {
        l->ondaAtiva = false;
        if (--l->ondasRestantes > 0) {
            l->avisoOnda = LUCI_ONDA_AVISO * 0.6f;
            l->ondaBrecha = (float)GetRandomValue(0, 359) * DEG2RAD;
        } else {
            l->padrao3 = 0;
            l->tempoPadrao3 = 3.2f;
            l->estadoMelee = 0;
            l->tempoMelee = 0.8f;
        }
    }
}

// 3ª barra, com o frenesi: o ritmo dela sobe (e sobe mais ainda quanto menos vida resta), o núcleo
// treme e ela grita. A telegrafia continua legível: dá para rebater, mas não dá para relaxar.
static void RogueLuciNucleo(Biblioteca *b, float dt)
{
    static const char *GRITOS[5] = { "MAIS RÁPIDO!", "NÃO PARE!", "ERRO! ERRO! ERRO!", "VOCÊ NÃO ACOMPANHA!", "ACELERANDO..." };
    static float cronoGrito = 5.0f;
    LuciChefe *l = &b->luci;
    float ritmoBase = l->ritmo;

    g_frenesiLuci = RogueFrenesiLuci(l);
    l->ritmo = ritmoBase * g_frenesiLuci;
    RogueLuciNucleoBase(b, dt);
    l->ritmo = ritmoBase;

    if (l->emRegeneracao || l->morrendo > 0.0f)
        return;

    if (GetRandomValue(0, 99) < 2)
        b->tremor = fmaxf(b->tremor, 0.08f);

    cronoGrito -= dt;
    if (cronoGrito <= 0.0f && !l->atordoada) {
        RogueTextoFlutuante(b, (Vector2){ l->pos.x, l->pos.y - 56 }, GRITOS[GetRandomValue(0, 4)], MAGENTA);
        cronoGrito = 5.0f + (float)GetRandomValue(0, 30) / 10.0f;
    }
}

// 1ª barra: o conflito lógico (dica envenenada e, da metade para baixo, as contradições com as cópias)
static void RogueLuciConflito(Biblioteca *b, float dt)
{
    LuciChefe *l = &b->luci;
    JogadorRogue *j = &b->j;

    if (l->fase == LUCI_SUB_DICA && l->hp <= l->hpMax * LUCI_FASE_CONTRADICAO) { RogueLuciMudarFase(b, LUCI_SUB_CONTRADICAO); return; }

    if (l->janela > 0.0f) l->janela -= dt;

    Vector2 d = Vector2Subtract(j->pos, l->pos);
    Vector2 dir = (Vector2Length(d) > 0.01f) ? Vector2Normalize(d) : (Vector2){ 0, 1 };

    // Movimento: na dica flutua no alto; nas contradições gira com as cópias
    if (l->fase == LUCI_SUB_CONTRADICAO) {
        l->angulo += dt * 0.6f * l->ritmo;
        RogueLuciPosicionarCopias(l);
        l->troca -= dt;
        if (l->troca <= 0.0f) {                         // trocam de lugar num clarão
            l->troca = 5.0f;
            l->real = GetRandomValue(0, 2);
            for (int k = 0; k < 3; k++) RogueParticulas(b, l->copia[k], MAGENTA, 8, 120);
        }
        l->pos = l->copia[l->real];
    } else {
        l->pos = Vector2Lerp(l->pos, PontoNaArena((Vector2){ 400.0f + sinf(l->anim * 0.6f) * 170.0f, 150.0f }), fminf(1.0f, 3.0f * dt));
    }

    // Leque (dica) ou espiral em rajadas (contradições)
    l->cronoTiro -= dt * l->ritmo;
    if (l->fase == LUCI_SUB_CONTRADICAO) {
        if (fmodf(l->anim, 3.0f) < 1.5f && l->cronoTiro <= 0.0f) {
            for (int k = 0; k < 2; k++) {
                float a = l->anim * 3.0f + k * PI;
                RogueDisparar(b, l->pos, (Vector2){ cosf(a), sinf(a) }, LUCI_TIRO_VEL * l->ritmo * 0.85f, LUCI_TIRO_DANO, 5.0f, false);
            }
            l->cronoTiro = 0.12f;
        }
    } else if (l->cronoTiro <= 0.0f) {
        for (int k = -2; k <= 2; k++)
            RogueDisparar(b, l->pos, Vector2Rotate(dir, k * 13.0f * DEG2RAD), LUCI_TIRO_VEL * l->ritmo, LUCI_TIRO_DANO, 6.0f, false);
        l->cronoTiro = 1.4f;
        TocarGrupo(&gChefeRajada, 0.85f, 0.75f);
    }

    // Ataque em metade da arena: ela anuncia o "lado seguro"; às vezes mente
    if (l->fase == LUCI_SUB_DICA) {
        l->tempoDica -= dt * l->ritmo;
        if (!l->avisando && l->tempoDica <= 0.0f) {
            l->avisando = true;
            l->tempoDica = LUCI_DICA_AVISO;
            l->ladoAtaque = GetRandomValue(0, 1);
            l->dicaFalsa = GetRandomValue(1, 100) <= (l->boa ? LUCI_DICA_FALSA_BOA : LUCI_DICA_FALSA_RUIM);
        } else if (l->avisando && l->tempoDica <= 0.0f) {
            bool naMetade = (l->ladoAtaque == 0) ? (j->pos.x < 400.0f) : (j->pos.x >= 400.0f);
            l->avisando = false;
            l->tempoDica = LUCI_DICA_INTERVALO;
            if (naMetade) RogueFerirJogador(b, LUCI_DANO_AREA);
            if (l->dicaFalsa) {
                if (naMetade) {
                    b->perm->confiouAlucinacao++;
                    RogueErroVerificacao(b, "\"O LADO SEGURO\"", "Era uma dica alucinada. O aviso vermelho mostrava onde o golpe cairia: confira a fonte!");
                    Telemetria("bloco3_dica", "acreditou", 0.0f, 0);
                } else {
                    b->perm->alucinacoesPegas++;
                    Telemetria("bloco3_dica", "verificou", 0.0f, 0);
                }
            }
            for (int k = 0; k < 24; k++)
                RogueParticulas(b, PontoNaArena((Vector2){ (l->ladoAtaque == 0 ? 60.0f : 420.0f) + GetRandomValue(0, 320), (float)GetRandomValue(80, 520) }), RED, 1, 60);
            TocarGrupo(&gLuciRacha, 1.0f, 0.8f);
        }
    }

    // Frases das contradições: algumas contradizem a sala
    if (l->fase == LUCI_SUB_CONTRADICAO) {
        l->tempoFrase -= dt;
        if (l->frase < 0 && l->tempoFrase <= 0.0f && l->janela <= 0.0f) {
            l->frase = GetRandomValue(0, NUM_FRASES_LUCI - 1);
            l->fraseContradiz = RogueFraseContradiz(b, l->frase);
            l->contestada = false;
            l->tempoFrase = LUCI_FRASE_TEMPO + (RogueTemConceito(b, CONCEITO_ALUCINACOES) ? LUCI_FRASE_BONUS : 0.0f);
            l->tempoFraseMax = l->tempoFrase;
        } else if (l->frase >= 0 && l->tempoFrase <= 0.0f) {
            if (l->fraseContradiz && !l->contestada) b->perm->contradicoesPerdidas++;
            l->frase = -1;
            l->tempoFrase = 1.5f;
        }
    }
}

void RogueAtualizarLuciChefe(Biblioteca *b, ProgressoCorredor *prog, float dt)
{
    LuciChefe *l = &b->luci;
    JogadorRogue *j = &b->j;

    if (!l->ativo || !l->apresentado)
        return;

    l->anim += dt;
    if (l->flash > 0.0f) l->flash -= dt;
    if (l->mostrarFalacia > 0.0f) l->mostrarFalacia -= dt;

    // Etapa 3: posições recentes (o rastro aparece só na investida)
    l->cronoRastro -= dt;
    if (l->cronoRastro <= 0.0f) {
        l->cronoRastro = 0.03f;
        for (int k = 5; k > 0; k--) l->rastro[k] = l->rastro[k - 1];
        l->rastro[0] = l->pos;
    }

    // Derrotada de vez: o final é decidido agora (o índice conta também o que aconteceu na luta)
    if (l->morrendo > 0.0f) {
        l->morrendo -= dt;
        RogueParticulas(b, (Vector2){ l->pos.x + GetRandomValue(-30, 30), l->pos.y + GetRandomValue(-30, 30) },
                        b->finalBom ? (Color){ 0, 228, 255, 255 } : MAGENTA, 3, 200);
        if (l->morrendo <= 0.0f) {
            l->ativo = false;
            b->conversaPendente = b->finalBom ? &dialogoFinalBom : &dialogoFinalRuim;
            b->objConversa = OBJ_LUCI_FINAL;
        }
        return;
    }

    // Desabando ou renascendo (as quedas da 1ª e da 2ª barra)
    if (l->emRegeneracao) {
        RogueAtualizarRegeneracaoLuci(b, dt);
        return;
    }

    if (l->hp <= 0.0f) {
        if (l->faseOculta < 3) {            // o jogador acha que venceu... (falsa derrota)
            RogueLuciFalsaDerrota(b);
            return;
        }

        // 3ª queda: agora é de verdade. O IndiceFinal decide o final.
        l->hp = 0.0f;
        l->morrendo = 2.0f;
        l->atordoada = false;
        b->indice = RogueCalcularIndice(b, prog->pontos);
        b->finalBom = b->indice.total >= INDICE_FINAL_BOM;
        RogueFalaViradaLuci(b, FALA_DERROTA, b->finalBom);
        RogueLimparBalasInimigas(b);
        b->pausaImpacto = 0.25f;
        b->flashNegativo = 0.4f;
        TocarSomTom(sfxAcerto, 0.5f);
        Telemetria("bloco3_vitoria", b->finalBom ? "bom" : "ruim", (float)l->parriesPerfeitos, l->combosConcluidos);
        return;
    }

    // Etapa 5: relógio dos comentários e a fala da vida baixa (3ª barra)
    if (l->cronoFala > 0.0f) l->cronoFala -= dt;
    if (l->faseOculta == 3 && !l->falouVidaBaixa && l->hp < l->hpMax * 0.25f) {
        l->falouVidaBaixa = true;
        RogueFalaViradaLuci(b, FALA_VIDA_BAIXA, l->boa);
    }

    // Etapa 2: janelas de guarda aberta
    if (l->atordoadaOrbe > 0.0f) l->atordoadaOrbe -= dt;
    if (l->superaquecida > 0.0f) {
        l->superaquecida -= dt;
        if (GetRandomValue(0, 3) == 0)
            RogueParticulaDirigida(b, (Vector2){ l->pos.x + (float)GetRandomValue(-20, 20), l->pos.y - 10 }, (Vector2){ 0, -60 }, 0.6f, 3, Fade(LIGHTGRAY, 0.8f));
    }

    // Atordoada pelo parry: parada, esperando o combo
    if (l->atordoada) {
        l->tempoStun -= dt;
        if (l->tempoStun <= 0.0f) {
            l->atordoada = false;
            l->estadoMelee = 0;
            l->tempoMelee = 0.8f;
        }
    }

    if (l->faseOculta == 1)      RogueLuciConflito(b, dt);
    else if (l->faseOculta == 2) RogueLuciSurto(b, dt);
    else                         RogueLuciNucleo(b, dt);

    // Encostar nela machuca (menos atordoada ou investindo: a investida tem o próprio golpe)
    if (!l->atordoada && l->estadoMelee != 2 && Vector2Distance(l->pos, j->pos) < LUCI_RAIO + RAIO_CORPO_JOGADOR)
        RogueFerirJogador(b, LUCI_DANO_CONTATO);
}

// O que dá para usar com E na arena (3ª barra, índice ruim): pedaço de contexto (k) ou o núcleo (-1)
bool RogueAlvoContexto(const Biblioteca *b, int *k)
{
    const LuciChefe *l = &b->luci;

    if (!l->ativo || l->faseOculta != 3 || !l->exigeContexto || l->contextoPronto || l->emRegeneracao)
        return false;

    if (l->carregando >= 0 && Vector2Distance(b->j.pos, NucleoLuci()) < 50.0f) { *k = -1; return true; }

    for (int c = 0; c < NUM_CONTEXTO_LUCI; c++)
        if (!l->entregue[c] && c != l->carregando && Vector2Distance(b->j.pos, l->posContexto[c]) < SIL_RAIO_PEGAR) { *k = c; return true; }

    return false;
}
