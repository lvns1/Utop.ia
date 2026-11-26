#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <time.h>
#include "../../include/core/fonte.h"
#include "../../include/core/mapas.h"
#include "../../include/entidades/luci_companion.h"
#include "../../include/audio/audio.h"
#include "../../include/bloco1/minigame_visual.h"
#include "../../include/core/save.h"
#include "../../include/core/telemetria.h"
#include "../../include/core/texto.h"
#include "../../include/entidades/jogador.h"


LuciComp g_luciComp = { 0 };
bool g_luciPedirDica = false;        // E sem nada por perto (o main consome no LuciCompAtualizar)
double g_repetirFalaLuciAte = 0.0;   // o balão do HUD digita a dica de novo (até este instante)
bool g_luciGuiaSoParado = false;     // o alvo só vale quando ele fica parado (ex.: a porta de uma sala resolvida)

void LuciCompSusto(Vector2 de)
{
    if (!LUCI_REACOES || !g_luciComp.iniciada) return;
    LuciComp *c = &g_luciComp;
    Vector2 d = Vector2Subtract(c->pos, de);
    d = (Vector2Length(d) > 0.1f) ? Vector2Normalize(d) : (Vector2){ 0, -1 };
    c->vel = Vector2Add(c->vel, Vector2Scale(d, 320.0f));     // pula para longe do golpe
    c->susto = 0.5f;
    MostrarEmocaoLuci(EMOCAO_ALERTA, 1.0f);
}

void LuciCompGirar(void)
{
    if (!LUCI_REACOES) return;
    g_luciComp.giro = 360.0f;
}

void LuciCompComemorar(void)
{
    if (!LUCI_REACOES || !g_luciComp.iniciada) return;
    g_luciComp.comemora = 0.8f;
    g_luciComp.giro = 360.0f;
    g_luciComp.vel.y -= 180.0f;                                 // um pulinho
    MostrarEmocaoLuci(EMOCAO_ALEGRE, 1.2f);
}

void LuciCompAcao(AcaoLuciComp a, float tempo)
{
    if (!LUCI_REACOES && (a == LC_PULSO || a == LC_SCANNER)) return;
    g_luciComp.acao = a;
    g_luciComp.tempoAcao = tempo;
}

static void LuciCompGuiar(Vector2 alvo)
{
    if (!LUCI_GUIA) return;
    LuciComp *c = &g_luciComp;
    c->acao = LC_GUIA;
    c->tempoAcao = LUCI_GUIA_TEMPO;
    c->alvo = alvo;
    MostrarEmocaoLuci(EMOCAO_ALERTA, 1.2f);
}

// Atualiza a Luci que acompanha o Thomas.
//   mira: direção da arma ({0,0} = sem arma: ela pode passear em volta dele)
//   temAlvo/alvo: o objetivo atual (para ela mostrar o caminho); vidaFrac: 0 a 1
void LuciCompAtualizar(Vector2 jog, Vector2 mira, bool temAlvo, Vector2 alvo, float vidaFrac, float dt)
{
    LuciComp *c = &g_luciComp;

    if (!c->iniciada || Vector2Distance(jog, c->jogAnt) > 80.0f) {     // entrou numa sala: aparece junto dele
        float ang = c->angAntigo;
        *c = (LuciComp){ 0 };
        c->iniciada = true;
        c->angAntigo = ang;
        c->lado = 1.0f;
        c->pos = (Vector2){ jog.x + 20.0f, jog.y - 36.0f };
        c->jogAnt = jog;
        c->dirAndar = (Vector2){ 0, 1 };
        c->cronoVolta = 3.0f;
        c->ampAsas = 0.06f;
    }
    if (dt <= 0.0f) return;
    dt = fminf(dt, 1.0f / 30.0f);                               // um quadro lento não faz a mola "explodir"

    // O que o Thomas está fazendo
    Vector2 passo = Vector2Subtract(jog, c->jogAnt);
    c->jogAnt = jog;
    if (Vector2Length(passo) / dt > 20.0f) { c->dirAndar = Vector2Normalize(passo); c->parado = 0.0f; c->guiouParado = false; }
    else c->parado += dt;

    bool comMira = Vector2Length(mira) > 0.01f;
    if (comMira) {                                              // o ombro do lado contrário da mira
        if (mira.x > 0.25f) c->lado = -1.0f;
        else if (mira.x < -0.25f) c->lado = 1.0f;
    } else if (fabsf(c->dirAndar.x) > 0.3f) {
        c->lado = (c->dirAndar.x > 0.0f) ? -1.0f : 1.0f;        // fica do lado de trás de quem anda
    }

    float alvoPerto = (LUCI_REACOES && vidaFrac < 0.3f) ? 1.0f : 0.0f;      // pouca vida: fica mais perto dele
    c->pertoVida += (alvoPerto - c->pertoVida) * fminf(1.0f, dt * 3.0f);
    float perto = c->pertoVida;

    // E sem nada por perto: repete a dica e vai mostrar onde é
    if (g_luciPedirDica) {
        g_luciPedirDica = false;
        if (LUCI_GUIA) {
            g_repetirFalaLuciAte = GetTime() + 0.3;
            c->falando = 1.4f;
            if (temAlvo && Vector2Distance(jog, alvo) > 70.0f) LuciCompGuiar(alvo);
            else { c->vel.y -= 120.0f; MostrarEmocaoLuci(EMOCAO_ALERTA, 1.0f); }
        }
    }

    // Objetivo novo (ou ele parado faz tempo, longe dele): ela vai até lá e volta
    if (LUCI_GUIA && temAlvo) {
        if (!c->temAlvoAnt || Vector2Distance(alvo, c->alvoAnt) > 40.0f) c->esperaGuia = 0.9f;
        c->temAlvoAnt = true;
        c->alvoAnt = alvo;
        bool longe = Vector2Distance(jog, alvo) > 130.0f;
        if (c->esperaGuia > 0.0f && !g_luciGuiaSoParado) {
            c->esperaGuia -= dt;
            if (c->esperaGuia <= 0.0f && longe && c->acao == LC_SEGUIR) LuciCompGuiar(alvo);
        }
        if (c->parado > 6.0f && !c->guiouParado && longe && c->acao == LC_SEGUIR) { c->guiouParado = true; LuciCompGuiar(alvo); }
        if (c->acao == LC_GUIA) c->alvo = alvo;
    } else {
        c->temAlvoAnt = false;
        if (c->acao == LC_GUIA) c->acao = LC_SEGUIR;
    }

    // Para onde ela quer ir (a "âncora") e a força da mola
    Vector2 ancora;
    float k = 70.0f, amort = 11.0f;
    c->atras = false;

    if (c->parado > 0.35f) {                                    // parado: no ombro
        ancora = (Vector2){ jog.x + c->lado * (20.0f - 6.0f * perto), jog.y - 36.0f + 4.0f * perto };
        if (!comMira && c->acao == LC_SEGUIR) {                 // de vez em quando, uma volta lenta em volta dele
            if (c->voltando <= 0.0f) {
                c->cronoVolta -= dt;
                if (c->cronoVolta <= 0.0f) { c->voltando = 2.6f; c->orbita = (c->lado > 0.0f) ? 0.0f : PI; }
            }
            if (c->voltando > 0.0f) {
                c->voltando -= dt;
                c->orbita += dt * (2.0f * PI / 2.6f);
                ancora = (Vector2){ jog.x + cosf(c->orbita) * 26.0f, jog.y - 20.0f + sinf(c->orbita) * 9.0f };
                c->atras = sinf(c->orbita) < 0.0f;
                if (c->voltando <= 0.0f) c->cronoVolta = 5.0f + (float)GetRandomValue(0, 30) / 10.0f;
            }
        }
    } else {                                                    // andando: logo atrás dele, um pouco acima
        c->voltando = 0.0f;
        c->cronoVolta = 3.0f;
        ancora = (Vector2){ jog.x - c->dirAndar.x * (24.0f - 8.0f * perto) + c->lado * 6.0f, jog.y - 30.0f - c->dirAndar.y * 14.0f };
    }

    // Longe da linha da mira: se a âncora cair na frente da arma, vai para o lado
    if (comMira) {
        Vector2 rel = Vector2Subtract(ancora, jog);
        if (Vector2Length(rel) > 0.1f && Vector2DotProduct(Vector2Normalize(rel), mira) > 0.55f) {
            Vector2 n = { -mira.y, mira.x };
            if (n.x * c->lado < 0.0f) n = Vector2Scale(n, -1.0f);
            ancora = Vector2Add(jog, Vector2Add(Vector2Scale(n, 26.0f), (Vector2){ 0, -12.0f }));
        }
    }

    // Ações: guiar, Pulso e Scanner
    if (c->acao != LC_SEGUIR) {
        c->tempoAcao -= dt;
        if (c->acao == LC_GUIA) {
            ancora = (Vector2){ c->alvo.x, c->alvo.y - 34.0f };
            k = 34.0f; amort = 9.0f;
            if (Vector2Distance(jog, c->alvo) < 60.0f) c->tempoAcao = fminf(c->tempoAcao, 0.2f);   // ele chegou: ela volta
        } else if (c->acao == LC_PULSO) {
            ancora = (Vector2){ jog.x, jog.y - 14.0f };
            k = 420.0f; amort = 32.0f;
        } else if (c->acao == LC_SCANNER) {
            ancora = (Vector2){ jog.x, jog.y - 54.0f };
            k = 170.0f; amort = 22.0f;
        }
        if (c->tempoAcao <= 0.0f) c->acao = LC_SEGUIR;
    }

    // A mola (com atraso: ela "voa" até a âncora)
    Vector2 antes = c->vel;
    if (LUCI_MOV_NOVO || c->acao != LC_SEGUIR) {
        Vector2 acc = Vector2Subtract(Vector2Scale(Vector2Subtract(ancora, c->pos), k), Vector2Scale(c->vel, amort));
        c->vel = Vector2Add(c->vel, Vector2Scale(acc, dt));
        c->pos = Vector2Add(c->pos, Vector2Scale(c->vel, dt));
        if (c->acao != LC_GUIA && Vector2Distance(c->pos, jog) > 160.0f)            // nunca fica muito para trás
            c->pos = Vector2Add(jog, Vector2Scale(Vector2Normalize(Vector2Subtract(c->pos, jog)), 160.0f));
    } else {                                                    // a órbita fixa antiga
        c->angAntigo += dt * 2.5f;
        Vector2 nova = { jog.x + cosf(c->angAntigo) * RAIO_ORBITA_LUCI, jog.y - 14.0f + sinf(c->angAntigo) * 8.0f + sinf(c->angAntigo * 2.7f) * 2.0f };
        c->vel = Vector2Scale(Vector2Subtract(nova, c->pos), 1.0f / dt);
        c->pos = nova;
        c->atras = sinf(c->angAntigo) < 0.0f;
    }

    // Por cima da cabeça e bem junto dele: passa por trás
    if (!c->atras && c->pos.y < jog.y - 22.0f && fabsf(c->pos.x - jog.x) < 12.0f && c->acao == LC_SEGUIR)
        c->atras = true;

    // Animação
    float vel = Vector2Length(c->vel);
    float kVel = Clamp(vel / 260.0f, 0.0f, 1.0f);
    float acel = Vector2Length(Vector2Subtract(c->vel, antes)) / dt;
    c->inclina += (Clamp(c->vel.x * 0.07f, -24.0f, 24.0f) - c->inclina) * fminf(1.0f, dt * 10.0f);
    c->estica += (Clamp(acel / 2400.0f, 0.0f, 1.0f) - c->estica) * fminf(1.0f, dt * 8.0f);
    c->ampAsas += ((0.05f + 0.09f * kVel) - c->ampAsas) * fminf(1.0f, dt * 6.0f);
    c->faseAsas = fmodf(c->faseAsas + dt * (9.0f + 18.0f * kVel), 2.0f * PI);
    if (c->giro > 0.0f) c->giro = fmaxf(0.0f, c->giro - dt * 900.0f);
    if (c->susto > 0.0f) c->susto -= dt;
    if (c->comemora > 0.0f) c->comemora -= dt;
    if (c->falando > 0.0f) c->falando -= dt;

    for (int i = 0; i < LUCI_RASTRO_N; i++)
        if (c->vidaRastro[i] > 0.0f) c->vidaRastro[i] -= dt * 2.2f;
    c->cronoRastro += dt;
    if (vel > 110.0f && c->cronoRastro > 0.03f) {
        c->cronoRastro = 0.0f;
        c->rastro[c->iRastro] = c->pos;
        c->vidaRastro[c->iRastro] = 1.0f;
        c->iRastro = (c->iRastro + 1) % LUCI_RASTRO_N;
    }
}

static bool LuciCompAtras(float angulo)
{
    return g_luciComp.iniciada ? g_luciComp.atras : LuciAtrasDoJogador(angulo);
}

static void DesenharLuciComp(Vector2 jog)
{
    const LuciComp *c = &g_luciComp;
    float t = (float)GetTime();
    Vector2 p = { c->pos.x, c->pos.y + sinf(t * 2.6f) * 2.0f };
    if (c->susto > 0.0f) p.x += sinf(t * 90.0f) * 3.0f * (c->susto / 0.5f);
    float altura = ALTURA_LUCI * (c->atras ? 0.88f : 1.0f);
    float kPulso = (c->acao == LC_PULSO) ? 1.0f - c->tempoAcao / LUCI_PULSO_TEMPO : 0.0f;
    if (c->acao == LC_PULSO) altura *= 1.0f + 0.4f * sinf(kPulso * PI);

#if LUCI_ANIM_SOMBRA
    {
        float chao = jog.y + 20.0f;
        float s = Clamp(1.0f - (chao - p.y - 30.0f) / 90.0f, 0.4f, 1.0f);
        DrawEllipse((int)p.x, (int)chao, 7.0f * s, 2.5f * s, Fade(BLACK, 0.3f * s));
    }
#endif
#if LUCI_ANIM_RASTRO
    for (int i = 0; i < LUCI_RASTRO_N; i++) {
        float v = c->vidaRastro[i];
        if (v <= 0.0f) continue;
        DrawCircleV(c->rastro[i], 1.0f + 3.0f * v, Fade(SKYBLUE, 0.3f * v));
        DrawCircleV(c->rastro[i], 0.8f + 1.0f * v, Fade(WHITE, 0.5f * v));
    }
#endif

    if (c->pertoVida > 0.02f)                                   // pouca vida: um pulso rosado em volta dela, colada nele
        DrawCircleV(p, altura * (0.75f + 0.12f * sinf(t * 7.0f)), Fade((Color){ 255, 90, 140, 255 }, 0.2f * c->pertoVida));

    if (c->acao == LC_PULSO) {                                  // o Pulso sai dela
        float r = altura * (0.6f + 2.4f * kPulso);
        DrawRing(p, r - 2.5f, r, 0.0f, 360.0f, 40, Fade(SKYBLUE, 0.8f * (1.0f - kPulso)));
        DrawCircleV(p, altura * 0.9f, Fade(WHITE, 0.25f * (1.0f - kPulso)));
    }

    if (c->acao == LC_SCANNER) {                                // o Scanner: um radar girando a partir dela
        float k = 1.0f - c->tempoAcao / LUCI_SCANNER_TEMPO;
        float a = k * 540.0f - 90.0f;
        Color ciano = { 0, 228, 255, 255 };
        float R = 72.0f;
        DrawCircleSector(p, R, a - 45.0f, a, 18, Fade(ciano, 0.16f * (1.0f - k)));
        DrawLineEx(p, (Vector2){ p.x + cosf(a * DEG2RAD) * R, p.y + sinf(a * DEG2RAD) * R }, 2.0f, Fade(ciano, 0.8f * (1.0f - k)));
        DrawRing(p, R - 1.5f, R, 0.0f, 360.0f, 48, Fade(ciano, 0.3f * (1.0f - k)));
    }

    if (c->acao == LC_GUIA && Vector2Distance(c->pos, (Vector2){ c->alvo.x, c->alvo.y - 34.0f }) < 30.0f) {
        float r = 14.0f + 4.0f * sinf(t * 8.0f);                // chegou: brilha em cima do objetivo
        DrawRing(p, r - 2.0f, r, 0.0f, 360.0f, 32, Fade(GOLD, 0.7f));
        DrawCircleV(p, altura * 0.8f, Fade(GOLD, 0.15f));
    }

    g_luciGiro = (LUCI_ANIM_INCLINAR ? c->inclina : 0.0f) + c->giro;
    g_luciEsticaX = LUCI_ANIM_INCLINAR ? 1.0f - 0.08f * c->estica : 1.0f;
    g_luciEsticaY = LUCI_ANIM_INCLINAR ? 1.0f + 0.14f * c->estica : 1.0f;
    if (LUCI_ANIM_ASAS) { g_luciAsasFase = c->faseAsas; g_luciAsasAmp = c->ampAsas; }

    bool falandoAntes = luciFalando;
    float tempoAntes = tempoFalaLuci;
    if (c->falando > 0.0f) { luciFalando = true; tempoFalaLuci = 1.4f - c->falando; }

    DesenharLuciTamanho(p, altura);

    luciFalando = falandoAntes;
    tempoFalaLuci = tempoAntes;
    g_luciGiro = 0.0f;
    g_luciEsticaX = g_luciEsticaY = 1.0f;
    g_luciAsasFase = -1.0f;
    g_luciAsasAmp = 0.07f;

    DesenharEmocaoLuci(p, altura);
}

static void DesenharCompanion(Vector2 jogadorPos, float angulo)
{
    if (g_luciComp.iniciada) {
        DesenharLuciComp(jogadorPos);
        return;
    }

    Vector2 p = {
        jogadorPos.x + cosf(angulo) * RAIO_ORBITA_LUCI,
        jogadorPos.y - 14.0f + sinf(angulo) * 8.0f + sinf(angulo * 2.7f) * 2.0f
    };

    float profundidade = 0.85f + 0.15f * (0.5f + 0.5f * sinf(angulo));   // atrás = menor

    DesenharLuciTamanho(p, ALTURA_LUCI * profundidade);
    DesenharEmocaoLuci(p, ALTURA_LUCI * profundidade);
}

// Desenha o personagem com a Luci passando atrás ou na frente dele
void DesenharJogadorComLuci(const SpriteDirecao sprites[NUM_DIRECOES], Vector2 pos, Direcao direcao,
                                   int quadro, bool comLuci, float anguloLuci)
{
    if (comLuci && LuciCompAtras(anguloLuci))
        DesenharCompanion(pos, anguloLuci);

    DesenharJogador(sprites, pos, direcao, quadro);

    if (comLuci && !LuciCompAtras(anguloLuci))
        DesenharCompanion(pos, anguloLuci);
}

// Centro da Luci parada na sala inicial (flutuando de leve)
Vector2 CentroLuciSala(Rectangle hitbox, float tempo)
{
    return (Vector2){
        hitbox.x + hitbox.width / 2,
        hitbox.y + hitbox.height / 2 + sinf(tempo * 2.0f) * 4.0f
    };
}

// Parte "sólida" da Luci parada: só o tamanho do desenho dela.
// (a hitbox inteira continua valendo para falar com ela)
Rectangle CorpoLuciSala(Rectangle hitbox)
{
    return (Rectangle){ hitbox.x + hitbox.width / 2 - 12, hitbox.y + hitbox.height / 2 - 12, 24, 24 };
}
