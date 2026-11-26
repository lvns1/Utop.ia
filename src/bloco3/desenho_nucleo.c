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

// ---------- Desenho do Bloco 3 ----------

// Para onde a Luci companion mostra o caminho na Biblioteca (só com a sala sem inimigos)
bool RogueAlvoGuiaLuci(const Biblioteca *b, Vector2 *alvo)
{
    g_luciGuiaSoParado = false;
    if (RogueInimigosVivos(b) > 0 || b->resultadoAberto || b->morreu)
        return false;

    if (b->tipoSala == SALA_MESTRE) {
        const MestreTreino *m = &b->mestre;
        if (m->etapa == MT_ARMA) { *alvo = POS_ARMA_MESTRE; return true; }
        if (m->etapa == MT_TIRO)
            for (int a = 0; a < MESTRE_ALVOS; a++)
                if (m->alvoVivo[a]) { *alvo = m->alvo[a]; return true; }
        return false;
    }
    if (b->tipoSala == SALA_ATRIO) {
        *alvo = b->perm->byteVisto ? CentroRetangulo(PORTA_PROXIMA_ROGUE) : POS_BYTE;
        return true;
    }
    if (b->noAtual >= 0 && b->limpa && b->numPortas > 0) {     // sala resolvida: a porta mais perto (só se ele ficar parado)
        g_luciGuiaSoParado = true;
        float melhor = 1e9f;
        for (int k = 0; k < b->numPortas; k++) {
            Vector2 c = CentroRetangulo(b->portas[k].r);
            float d = Vector2Distance(c, b->j.pos);
            if (d < melhor) { melhor = d; *alvo = c; }
        }
        return true;
    }
    return false;
}

// Observa o que aconteceu neste quadro e faz a Luci reagir (chamado pelo main, antes do LuciCompAtualizar)
void RogueLuciCompObservar(const Biblioteca *b)
{
    static float hpAnt = -1.0f, esquivaAnt = 0.0f, pulsoAnt = 0.0f, scannerAnt = 0.0f;
    static int parriesAnt = -1;
    const JogadorRogue *j = &b->j;
    int parries = b->perm->parriesRun + b->mestre.parries;

    if (hpAnt >= 0.0f && parriesAnt >= 0 && !b->morreu) {
        if (j->hp < hpAnt - 0.5f)                LuciCompSusto(j->pos);
        if (j->tempoEsquiva > 0.0f && esquivaAnt <= 0.0f) LuciCompGirar();
        if (parries > parriesAnt)                LuciCompComemorar();
        if (j->efeitoPulso > pulsoAnt + 0.05f)   LuciCompAcao(LC_PULSO, LUCI_PULSO_TEMPO);
        if (b->scanner > scannerAnt + 0.05f)     LuciCompAcao(LC_SCANNER, LUCI_SCANNER_TEMPO);
    }
    hpAnt = j->hp;
    esquivaAnt = j->tempoEsquiva;
    pulsoAnt = j->efeitoPulso;
    scannerAnt = b->scanner;
    parriesAnt = parries;
}

bool RogueNoBloco3(const Biblioteca *b)
{
    return b->tipoSala == SALA_TRANSICAO || b->tipoSala == SALA_LUCI;
}

// Chão do núcleo: linhas de texto correndo (o mundo é feito de respostas geradas)
void DesenharChaoNucleo(const Biblioteca *b, float t)
{
    for (int i = 0; i < 12; i++) {
        float y = ArenaRogue().y + fmodf(i * 40.0f + t * 18.0f, ArenaRogue().height);
        int largura = 120 + (int)(Ruido((unsigned int)i * 7u) % 400);
        int x = (int)ArenaRogue().x + 10 + (int)(Ruido((unsigned int)i * 13u) % 200);
        DrawRectangle(x, (int)y, largura, 2, Fade(MAGENTA, 0.07f));
    }
    if (b->tipoSala == SALA_LUCI) {
        Vector2 c = PontoNaArena((Vector2){ 400, 300 });
        DrawCircleLines((int)c.x, (int)c.y, 200, Fade(MAGENTA, 0.08f));
        DrawCircleLines((int)c.x, (int)c.y, 120, Fade((Color){ 0, 228, 255, 255 }, 0.08f));
    }
}

// A Luci grande (NPC no corredor ou chefe na arena), com a falha que cresce com a instabilidade
void DesenharLuciGrande(Vector2 p, float altura, float instabilidade, bool sombra)
{
    float guardada = instabilidadeLuci;

    if (sombra)
        DrawEllipse((int)p.x, (int)(p.y + altura * 0.6f), altura * 0.35f, 6, Fade(BLACK, 0.5f));

    instabilidadeLuci = instabilidade;
    DesenharLuciTamanho(p, altura);
    instabilidadeLuci = guardada;
}


void DesenharLuciChefe(const Biblioteca *b, float t)
{
    const LuciChefe *l = &b->luci;

    if (b->cenaLuciTipo == 2) {                     // cena: ela ainda está virando a chefe
        DesenharCenaTransformacao(b, t);
        return;
    }
    float inst = (l->boa && l->faseOculta == 1) ? 0.7f : 1.0f;
    float k = (l->morrendo > 0.0f) ? l->morrendo / 2.0f : 1.0f;

    if (l->morrendo > 0.0f && b->finalBom)
        inst = 0.7f * k;                                // salva: o magenta vai embora

    // Contradições: as cópias (sem sombra e falhando de vez em quando); a verdadeira tem sombra
    if (l->fase == LUCI_SUB_CONTRADICAO && l->morrendo <= 0.0f && !l->emRegeneracao)
        for (int c = 0; c < 3; c++)
            if (c != l->real && ((int)(t * 13.0f + c * 5)) % 9 != 0)
                DesenharLuciGrande((Vector2){ l->copia[c].x, l->copia[c].y + sinf(t * 2.0f + c) * 4.0f }, 70.0f, inst, false);

    Vector2 p = { l->pos.x, l->pos.y + sinf(t * 2.0f) * 4.0f };
    float altura = 70.0f * ((l->morrendo > 0.0f && !b->finalBom) ? (0.4f + 0.6f * k) : 1.0f);
    bool esconder = false;

    // Queda (falsa derrota): ela afunda, encolhe e falha; renascendo: sobe com a aura da cor nova
    if (l->emRegeneracao) {
        Color corNova = (l->faseOculta == 1) ? RED : MAGENTA;
        if (l->etapaRegeneracao == 0) {
            float total = (l->faseOculta == 1) ? LUCI_COLAPSO : LUCI_EXPLOSAO_ESTATICA;
            float q = Clamp(1.0f - l->tempoRegeneracao / total, 0.0f, 1.0f);
            p.y += 26.0f * fminf(1.0f, q * 3.0f);
            altura *= 1.0f - 0.35f * fminf(1.0f, q * 3.0f);
            esconder = ((int)(t * 18.0f)) % 5 == 0;
        } else {
            float q = Clamp(1.0f - l->tempoRegeneracao / LUCI_RENASCER, 0.0f, 1.0f);
            p.y += 26.0f * (1.0f - q);
            altura *= 0.65f + 0.35f * q;
            DrawRectangle((int)p.x - (int)(10 + 30 * q), (int)ArenaRogue().y, (int)(20 + 60 * q), (int)(p.y - ArenaRogue().y), Fade(corNova, 0.10f + 0.15f * q));
            DrawCircleV(p, LUCI_RAIO + 10 + 30 * q, Fade(corNova, 0.25f * q));
        }
    }

    // Aura da barra atual: vermelha corrompida (2ª) ou magenta psíquica (3ª)
    if (l->faseOculta >= 2 && !l->emRegeneracao && l->morrendo <= 0.0f) {
        Color cor = (l->faseOculta == 2) ? RED : MAGENTA;
        DrawCircleV(p, LUCI_RAIO + 16 + 4.0f * sinf(t * 6.0f), Fade(cor, 0.16f));
        DrawCircleLines((int)p.x, (int)p.y, LUCI_RAIO + 20 + 3.0f * sinf(t * 9.0f), Fade(cor, 0.5f));
    }

    // Telegrafia: flash vermelho neon no corpo e nos olhos, o CORREDOR da investida enchendo e a falácia.
    // Quando a mira trava (últimos 30%), as bordas acendem e as setas correm: é o golpe chegando.
    if (l->estadoMelee == 1 && !l->atordoada && !l->emRegeneracao && l->morrendo <= 0.0f) {
        float pisca = (sinf(t * 40.0f) > 0.0f) ? 1.0f : 0.35f;
        float total = (l->telegrafiaTotal > 0.01f) ? l->telegrafiaTotal : LUCI_TELEGRAFIA;
        float k = Clamp(1.0f - l->tempoMelee / total, 0.0f, 1.0f);
        bool travou = l->tempoMelee <= total * 0.3f;
        Vector2 dir = l->dirMelee, n = { -dir.y, dir.x };
        float alcance = LUCI_INVESTIDA_VEL * LUCI_INVESTIDA_DURACAO + 24.0f + LUCI_LAMINA_RAIO;
        float largura = LUCI_LAMINA_RAIO;
        Vector2 ini = Vector2Add(p, Vector2Scale(dir, LUCI_RAIO * 0.5f));
        Vector2 fim = Vector2Add(p, Vector2Scale(dir, alcance));
        Vector2 cheio = Vector2Lerp(ini, fim, k);
        Vector2 a1 = Vector2Add(ini, Vector2Scale(n, largura)), a2 = Vector2Subtract(ini, Vector2Scale(n, largura));
        Vector2 b1 = Vector2Add(fim, Vector2Scale(n, largura)), b2 = Vector2Subtract(fim, Vector2Scale(n, largura));
        Vector2 c1 = Vector2Add(cheio, Vector2Scale(n, largura)), c2 = Vector2Subtract(cheio, Vector2Scale(n, largura));
        DesenharTrianguloB2(a1, a2, b2, Fade(RED, 0.08f));
        DesenharTrianguloB2(a1, b2, b1, Fade(RED, 0.08f));
        DesenharTrianguloB2(a1, a2, c2, Fade(RED, travou ? 0.30f : 0.18f));
        DesenharTrianguloB2(a1, c2, c1, Fade(RED, travou ? 0.30f : 0.18f));
        Color borda = travou ? Fade(WHITE, 0.6f + 0.4f * pisca) : Fade(RED, 0.6f);
        DrawLineEx(a1, b1, travou ? 3.0f : 2.0f, borda);
        DrawLineEx(a2, b2, travou ? 3.0f : 2.0f, borda);
        DrawLineEx(b1, b2, 2.0f, borda);
        if (travou)
            for (int s = 0; s < 3; s++) {                         // setas correndo para frente
                float u = fmodf(t * 3.0f + (float)s / 3.0f, 1.0f);
                Vector2 c = Vector2Lerp(ini, fim, u);
                Vector2 ponta = Vector2Add(c, Vector2Scale(dir, 10.0f));
                DrawLineEx(Vector2Add(c, Vector2Scale(n, 10.0f)), ponta, 3.0f, Fade(WHITE, 0.8f * (1.0f - u)));
                DrawLineEx(Vector2Subtract(c, Vector2Scale(n, 10.0f)), ponta, 3.0f, Fade(WHITE, 0.8f * (1.0f - u)));
            }
        DrawCircleV(p, LUCI_RAIO + 28, Fade(RED, 0.30f * pisca));
        DrawCircleV((Vector2){ p.x - 7, p.y - altura * 0.22f }, 4, Fade((Color){ 255, 40, 60, 255 }, pisca));
        DrawCircleV((Vector2){ p.x + 7, p.y - altura * 0.22f }, 4, Fade((Color){ 255, 40, 60, 255 }, pisca));
        const char *f = TextFormat("FALÁCIA: %s", FALACIAS_LUCI[l->falacia][0]);
        int wf = MeasureText(f, 10);
        DrawRectangle((int)p.x - wf / 2 - 4, (int)(p.y - altura * 0.6f) - 18, wf + 8, 14, Fade(BLACK, 0.7f));
        DrawText(f, (int)p.x - wf / 2, (int)(p.y - altura * 0.6f) - 16, 10, RED);
    }

    // Rastro da investida: cópias vermelhas que somem
    if ((l->estadoMelee == 2 || (l->estadoMelee == 3 && !l->atordoada && l->tempoMelee > LUCI_RECUPERA - 0.15f)) && !l->emRegeneracao && l->morrendo <= 0.0f)
        for (int r = 5; r >= 1; r--) {
            if (l->rastro[r].x == 0.0f && l->rastro[r].y == 0.0f) continue;
            g_alfaLuci = 0.32f * (1.0f - (float)r / 6.0f);
            DrawCircleV(l->rastro[r], LUCI_RAIO * 0.8f, Fade(RED, 0.10f * (1.0f - (float)r / 6.0f)));
            DesenharLuciGrande(l->rastro[r], altura * (1.0f - 0.04f * (float)r), 1.0f, false);
        }
    g_alfaLuci = 1.0f;

    // 3ª barra: a imagem dela falha de vez em quando (uma cópia deslocada em magenta)
    if (l->faseOculta == 3 && !l->emRegeneracao && l->morrendo <= 0.0f && (Ruido((unsigned int)(t * 14.0f)) % 100) < 12) {
        float d = 6.0f + (float)(Ruido((unsigned int)(t * 31.0f)) % 10);
        g_alfaLuci = 0.45f;
        DesenharLuciGrande((Vector2){ p.x + d, p.y }, altura, 1.0f, false);
        g_alfaLuci = 1.0f;
    }

    if (!esconder) {
        for (int vez = 0; vez < ((l->flash > 0.0f) ? 2 : 1); vez++) {
            if (vez == 1) BeginBlendMode(BLEND_ADDITIVE);
            DesenharLuciGrande(p, altura, inst, true);
        }
        if (l->flash > 0.0f) EndBlendMode();
    }

    if (l->morrendo > 0.0f || l->emRegeneracao)
        return;

    // A investida: a lâmina da falácia na frente dela
    if (l->estadoMelee == 2) {
        float ang = atan2f(l->dirMelee.y, l->dirMelee.x) * RAD2DEG;
        Vector2 c = Vector2Add(p, Vector2Scale(l->dirMelee, 24.0f));
        DrawRing(c, LUCI_LAMINA_RAIO - 6, LUCI_LAMINA_RAIO, ang - 70.0f, ang + 70.0f, 16, Fade(RED, 0.9f));
        DrawRing(c, LUCI_LAMINA_RAIO - 12, LUCI_LAMINA_RAIO - 8, ang - 50.0f, ang + 50.0f, 12, Fade(WHITE, 0.7f));
    }

    // Atordoada: estrelas girando, o tempo do atordoamento e o combo
    if (l->atordoada) {
        for (int s = 0; s < 3; s++) {
            float a = t * 5.0f + (float)s * 2.0f * PI / 3.0f;
            DrawPoly((Vector2){ p.x + cosf(a) * 22.0f, p.y - altura * 0.55f + sinf(a) * 6.0f }, 4, 4.0f, t * 200.0f, GOLD);
        }
        DrawRing(p, LUCI_RAIO + 14, LUCI_RAIO + 18, -90.0f, -90.0f + 360.0f * l->tempoStun / PARRY_STUN, 32, GOLD);
        const char *txt = TextFormat("ATORDOADA  -  COMBO %d/3", l->combo);
        DrawText(txt, (int)p.x - MeasureText(txt, 10) / 2, (int)(p.y + LUCI_RAIO + 24), 10, GOLD);
        return;
    }

    // Armadura de dados (hexágonos) ou a janela aberta (anel dourado com o tempo)
    if (!RogueLuciVulneravel(l)) {
        DrawPolyLinesEx(p, 6, LUCI_RAIO + 12, t * 40.0f, 3, Fade(MAGENTA, 0.8f));
        DrawPolyLinesEx(p, 6, LUCI_RAIO + 20, -t * 30.0f, 2, Fade(MAGENTA, 0.5f));
    } else if (l->janela > 0.0f) {
        DrawRing(p, LUCI_RAIO + 12, LUCI_RAIO + 16, -90.0f, -90.0f + 360.0f * l->janela / LUCI_JANELA, 32, GOLD);
    }
}


// Etapa 2: os ataques novos (orbes, varredura) e o sinal da guarda
void DesenharAtaquesLuci(const Biblioteca *b, float t)
{
    const LuciChefe *l = &b->luci;
    if (!l->ativo || l->morrendo > 0.0f || l->emRegeneracao || b->cenaLuciTipo != 0) return;

    // Rajada se formando: três orbes girando em volta dela
    if (l->avisoRajada > 0.0f) {
        float k = 1.0f - l->avisoRajada / LUCI_ORBE_AVISO;
        DrawCircleV(l->pos, LUCI_RAIO + 26.0f, Fade(RED, 0.10f + 0.15f * k));
        for (int i = 0; i < 3; i++) {
            float a = t * 5.0f + (float)i * 2.0f * PI / 3.0f;
            Vector2 p = { l->pos.x + cosf(a) * (LUCI_RAIO + 18.0f), l->pos.y + sinf(a) * (LUCI_RAIO + 18.0f) * 0.6f };
            DrawCircleV(p, 4.0f + 6.0f * k, Fade(RED, 0.8f));
        }
    }

    for (int k = 0; k < LUCI_NUM_ORBES; k++) {
        const OrbeFalacia *o = &l->orbe[k];
        if (!o->ativo) continue;
        Color cor = o->rebatido ? GOLD : (Color){ 235, 40, 60, 255 };
        float pulso = 0.5f + 0.5f * sinf(t * 12.0f + (float)k);
        DrawCircleV(o->pos, LUCI_ORBE_RAIO + 6.0f + 2.0f * pulso, Fade(cor, 0.22f));
        DrawCircleV(o->pos, LUCI_ORBE_RAIO, cor);
        DrawCircleV(o->pos, LUCI_ORBE_RAIO - 5.0f, (Color){ 40, 8, 16, 255 });
        DrawCircleLines((int)o->pos.x, (int)o->pos.y, LUCI_ORBE_RAIO + 1.0f, WHITE);
        if (!o->rebatido) {
            // Perto do Thomas: o anel dourado mostra a hora do parry
            float d = Vector2Distance(o->pos, b->j.pos);
            if (d < 110.0f)
                DrawCircleLines((int)o->pos.x, (int)o->pos.y, LUCI_ORBE_RAIO + 4.0f + (d - 30.0f) * 0.3f, Fade(GOLD, 0.9f * (1.0f - d / 110.0f)));
            const char *nome = FALACIAS_LUCI[o->falacia][0];
            int w = MeasureText(nome, 8);
            DrawRectangle((int)o->pos.x - w / 2 - 2, (int)o->pos.y - 30, w + 4, 11, Fade(BLACK, 0.7f));
            DrawText(nome, (int)o->pos.x - w / 2, (int)o->pos.y - 29, 8, Fade(WHITE, 0.9f));
        }
    }

    // Varredura: aviso (linha tracejada + seta do giro) e o feixe
    if (l->padrao3 == 2 && l->faseOculta == 3) {
        if (!l->varreduraAtiva && l->avisoVarredura > 0.0f) {
            float k = 1.0f - l->avisoVarredura / LUCI_VARREDURA_AVISO;
            Vector2 dir = { cosf(l->varreduraAng * DEG2RAD), sinf(l->varreduraAng * DEG2RAD) };
            for (float d = LUCI_RAIO + 6.0f; d < LUCI_VARREDURA_ALCANCE; d += 26.0f)
                DrawLineEx(Vector2Add(l->pos, Vector2Scale(dir, d)), Vector2Add(l->pos, Vector2Scale(dir, d + 14.0f)), 2.0f + 2.0f * k, Fade(MAGENTA, 0.4f + 0.5f * k));
            float a0 = l->varreduraAng, a1 = l->varreduraAng + l->varreduraSentido * 50.0f;
            DrawRing(l->pos, 64.0f, 70.0f, fminf(a0, a1), fmaxf(a0, a1), 16, Fade(MAGENTA, 0.8f));
            Vector2 ponta = { l->pos.x + cosf(a1 * DEG2RAD) * 67.0f, l->pos.y + sinf(a1 * DEG2RAD) * 67.0f };
            DrawCircleV(ponta, 5.0f, MAGENTA);
        } else if (l->varreduraAtiva) {
            Vector2 dir = { cosf(l->varreduraAng * DEG2RAD), sinf(l->varreduraAng * DEG2RAD) };
            Vector2 a = Vector2Add(l->pos, Vector2Scale(dir, LUCI_RAIO));
            Vector2 z = Vector2Add(l->pos, Vector2Scale(dir, LUCI_VARREDURA_ALCANCE));
            Rectangle ar = ArenaRogue();
            for (float d = LUCI_RAIO; d < LUCI_VARREDURA_ALCANCE; d += 6.0f) {      // o feixe termina na borda da arena
                Vector2 q = Vector2Add(l->pos, Vector2Scale(dir, d));
                if (!CheckCollisionPointRec(q, ar)) { z = q; break; }
            }
            float tremeL = 1.0f + 0.25f * sinf(t * 45.0f);
            DrawLineEx(a, z, 26.0f * tremeL, Fade(MAGENTA, 0.14f));
            DrawLineEx(a, z, 12.0f * tremeL, Fade(MAGENTA, 0.5f));
            DrawLineEx(a, z, 4.0f, WHITE);
            DrawCircleV(a, 14.0f * tremeL, Fade(WHITE, 0.6f));
            DrawCircleV(z, 6.0f + 3.0f * sinf(t * 30.0f), Fade(WHITE, 0.7f));      // faíscas onde o feixe encosta na borda
            DrawCircleV(z, 12.0f, Fade(MAGENTA, 0.35f));
        }
    }

    // A guarda das barras 2 e 3: fechada = hexágono vermelho fino; aberta = anel dourado
    if (l->faseOculta >= 2 && !l->atordoada) {
        if (RogueLuciGuardaAberta(l)) {
            float p = 0.5f + 0.5f * sinf(t * 10.0f);
            DrawCircleLines((int)l->pos.x, (int)l->pos.y, LUCI_RAIO + 10.0f + 2.0f * p, Fade(GOLD, 0.6f + 0.4f * p));
        } else {
            DrawPolyLinesEx(l->pos, 6, LUCI_RAIO + 10.0f, t * 50.0f, 1.5f, Fade((Color){ 200, 60, 70, 255 }, 0.55f));
        }
    }
}

// Ataque numa metade da arena: o aviso vermelho nunca mente
void DesenharAvisoMetade(const Biblioteca *b, float t)
{
    const LuciChefe *l = &b->luci;

    if (!l->ativo || !l->avisando || l->morrendo > 0.0f)
        return;

    float k = 1.0f - l->tempoDica / LUCI_DICA_AVISO;
    Rectangle metade = { (l->ladoAtaque == 0) ? ArenaRogue().x : 400.0f, ArenaRogue().y, ArenaRogue().width / 2.0f, ArenaRogue().height };
    bool nitido = RogueTemConceito(b, CONCEITO_FUNDAMENTOS);

    DrawRectangleRec(metade, Fade(RED, (nitido ? 0.12f : 0.06f) + 0.18f * k));
    DrawRectangleLinesEx(metade, 2, Fade(RED, 0.5f + 0.4f * sinf(t * 15.0f)));

    // Conceito FUNDAMENTOS: o aviso fica mais claro (listras e a palavra PERIGO)
    if (nitido) {
        for (int i = 0; i < 10; i++) {
            float x = metade.x + fmodf(i * 40.0f + t * 60.0f, metade.width);
            DrawLineEx((Vector2){ x, metade.y }, (Vector2){ x - 40, metade.y + metade.height }, 2, Fade(RED, 0.15f));
        }
        const char *perigo = "PERIGO";
        DrawText(perigo, (int)(metade.x + metade.width / 2) - MeasureText(perigo, 30) / 2, 290, 30, Fade(RED, 0.5f));
    }
}

// Fase 3: os pedaços de contexto e o núcleo
void DesenharContextoLuci(const Biblioteca *b, float t)
{
    const LuciChefe *l = &b->luci;

    if (!l->ativo || l->faseOculta != 3 || !l->exigeContexto || l->morrendo > 0.0f || l->emRegeneracao)
        return;

    float pulso = 0.5f + 0.5f * sinf(t * 4.0f);
    DrawCircleV(NucleoLuci(), 34 + pulso * 4, Fade((Color){ 0, 228, 255, 255 }, l->contextoPronto ? 0.3f : 0.12f));
    DrawPolyLinesEx(NucleoLuci(), 8, 30, t * 20.0f, 3, (Color){ 0, 228, 255, 255 });
    DrawText(TextFormat("%d/%d", l->entregues, CONTEXTO_CERTOS), (int)NucleoLuci().x - 12, (int)NucleoLuci().y - 8, 16, WHITE);
    DrawText("NÚCLEO", (int)NucleoLuci().x - MeasureText("NÚCLEO", 10) / 2, (int)NucleoLuci().y + 36, 10, (Color){ 0, 228, 255, 255 });

    if (l->contextoPronto)
        return;

    for (int c = 0; c < NUM_CONTEXTO_LUCI; c++) {
        if (l->entregue[c]) continue;
        Vector2 p = (c == l->carregando) ? (Vector2){ b->j.pos.x, b->j.pos.y - 40 } : l->posContexto[c];
        DesenharPecaSilogismo(p, '?', (c == l->carregando) ? YELLOW : (Color){ 0, 228, 255, 255 }, t);
        int w = MeasureText(CONTEXTO_LUCI[c], 10);
        DrawRectangle((int)p.x - w / 2 - 3, (int)p.y + 18, w + 6, 13, Fade(BLACK, 0.7f));
        DrawText(CONTEXTO_LUCI[c], (int)p.x - w / 2, (int)p.y + 20, 10, WHITE);
    }
}

// Faixa no alto: o "lado seguro" (fase 1), a frase para contestar (fase 2) ou o pedido (fase 3)
// A Luci está falando na Biblioteca? (o recado curto dela, ou a fala do chefe na faixa de cima)
bool RogueLuciFalando(const Biblioteca *b)
{
    const LuciChefe *l = &b->luci;

    if (b->luciMsgTempo > 0.0f)
        return true;

    if (b->tipoSala != SALA_LUCI || !l->ativo || !l->apresentado || l->morrendo > 0.0f || l->emRegeneracao)
        return false;

    return (l->fase == LUCI_SUB_DICA && l->avisando) || (l->fase == LUCI_SUB_CONTRADICAO && l->frase >= 0);
}

void DesenharFaixaLuci(const Biblioteca *b, float t)
{
    const LuciChefe *l = &b->luci;
    char texto[160] = "";
    const char *ajuda = "";
    Color borda = MAGENTA;

    if (!l->ativo || !l->apresentado || l->morrendo > 0.0f || l->emRegeneracao)
        return;

    if (l->fase == LUCI_SUB_DICA && l->avisando) {
        int seguro = l->dicaFalsa ? l->ladoAtaque : 1 - l->ladoAtaque;
        snprintf(texto, sizeof(texto), "LUCI: \"O lado %s está seguro! Pode confiar.\"", seguro == 0 ? "ESQUERDO" : "DIREITO");
        ajuda = "O aviso vermelho mostra onde o ataque cai de verdade.";
    } else if (l->fase == LUCI_SUB_CONTRADICAO && l->frase >= 0) {
        snprintf(texto, sizeof(texto), "LUCI: \"%s\"", FRASES_LUCI_CHEFE[l->frase]);
        ajuda = "C = CONTESTAR (só se contradiz o que você vê)";
        borda = l->contestada ? GRAY : MAGENTA;
    } else if (l->mostrarFalacia > 0.0f) {
        // Depois do parry: a falácia que ela tentou usar, explicada
        snprintf(texto, sizeof(texto), "FALÁCIA REFUTADA: %s", FALACIAS_LUCI[l->falacia][0]);
        ajuda = FALACIAS_LUCI[l->falacia][1];
        borda = GOLD;
    } else if (l->faseOculta == 3 && l->exigeContexto && !l->contextoPronto) {
        snprintf(texto, sizeof(texto), "PEDIDO CLARO: leve ASSUNTO, OBJETIVO e FORMATO ao núcleo (E)");
        ajuda = "Pedido vago faz a Luci inventar uma resposta.";
        borda = (Color){ 0, 228, 255, 255 };
    } else {
        return;
    }

    Rectangle faixa = { 100, 66, 600, 48 };
    DrawRectangleRec(faixa, Fade((Color){ 18, 15, 30, 255 }, 0.9f));
    DrawRectangleLinesEx(faixa, 2, borda);
    DrawText(texto, (int)(faixa.x + faixa.width / 2) - MeasureText(texto, 14) / 2, (int)faixa.y + 8, 14, WHITE);
    DrawText(ajuda, (int)(faixa.x + faixa.width / 2) - MeasureText(ajuda, 10) / 2, (int)faixa.y + 30, 10, Fade(GOLD, 0.75f + 0.25f * sinf(t * 5.0f)));

    if (l->fase == LUCI_SUB_CONTRADICAO && l->frase >= 0 && l->tempoFraseMax > 0.0f)
        DrawRectangle((int)faixa.x, (int)(faixa.y + faixa.height) - 3, (int)(faixa.width * l->tempoFrase / l->tempoFraseMax), 3, GOLD);
}

// Uma barra só: o jogador não sabe que ela renasce. 1ª: azul/magenta; 2ª: vermelha corrompida; 3ª: magenta psíquica
void DesenharBarraLuci(const LuciChefe *l, float t)
{
    int x = 200, y = 44, w = 400;
    Color cor;

    if (l->faseOculta == 1)      cor = RogueLuciVulneravel(l) ? (Color){ 0, 228, 255, 255 } : MAGENTA;
    else if (l->faseOculta == 2) cor = (Color){ 235, 30, 45, 255 };
    else                         cor = (Color){ 255, 0, 255, 255 };

    bool renascendo = l->emRegeneracao && l->etapaRegeneracao == 1;
    if (renascendo)     cor = (l->faseOculta == 1) ? (Color){ 235, 30, 45, 255 } : (Color){ 255, 0, 255, 255 };
    if (l->atordoada)   cor = GOLD;

    if (l->faseOculta >= 2 || renascendo)          // brilho corrompido em volta
        DrawRectangle(x - 7, y - 5, w + 14, 22, Fade(cor, 0.16f + 0.10f * sinf(t * 8.0f)));

    DrawRectangle(x - 4, y - 2, w + 8, 16, Fade(BLACK, 0.8f));
    DrawRectangle(x, y + 2, w, 8, (Color){ 40, 20, 50, 255 });
    DrawRectangle(x, y + 2, (int)(w * fmaxf(0.0f, l->hp) / l->hpMax), 8, cor);

    // Renascendo: falhas de dados correndo pela barra
    if (renascendo || (l->emRegeneracao && ((int)(t * 10.0f)) % 2 == 0))
        for (int i = 0; i < 6; i++) {
            unsigned int q = Ruido((unsigned int)(t * 20.0f) * 13u + (unsigned int)i);
            DrawRectangle(x + (int)(q % (unsigned int)w), y + 2, 4 + (int)((q >> 8) % 20), 8, Fade(WHITE, 0.5f));
        }
    DrawRectangleLines(x, y + 2, w, 8, GRAY);

    int wNome = MeasureText("LUCI", 10);
    DrawRectangle(x - wNome - 18, y - 2, wNome + 12, 16, Fade(BLACK, 0.8f));
    DrawText("LUCI", x - wNome - 12, y + 1, 10, (l->faseOculta == 1) ? MAGENTA : cor);
}

// Resultado final: o índice, parte por parte, e o final alcançado
void DesenharResultadoFinal(const Biblioteca *b, float t)
{
    const IndiceFinal *r = &b->indice;
    Color cor = b->finalBom ? (Color){ 0, 228, 255, 255 } : MAGENTA;

    DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, 0.85f));
    Rectangle painel = { 130, 60, 540, 470 };
    DrawRectangleRec(painel, (Color){ 18, 15, 30, 255 });
    DrawRectangleLinesEx(painel, 4, cor);

    const char *titulo = b->finalBom ? "FINAL BOM: A LUCI FOI SALVA" : "FINAL RUIM: A IA NÃO PÔDE SER SALVA";
    DrawText(titulo, LARGURA_TELA / 2 - MeasureText(titulo, 22) / 2, (int)painel.y + 20, 22, cor);
    const char *sub = b->finalBom ? "UtopIA estabilizada. Você volta para casa." : "UtopIA foi desligada. Confira antes de confiar.";
    DrawText(sub, LARGURA_TELA / 2 - MeasureText(sub, 12) / 2, (int)painel.y + 50, 12, LIGHTGRAY);

    const char *nomes[7] = { "Estabilidade da Luci", "Acertos (desafios, afirmações, puzzles)", "Alucinações pegas x confiança cega",
                             "Contradições flagradas", "Conceitos aprendidos", "Poucas derrotas", "Pontuação" };
    const int valores[7] = { r->estabilidade, r->acertos, r->verificacao, r->contradicoes, r->conceitos, r->sobrevivencia, r->pontos };
    const int maximos[7] = { 20, 25, 15, 15, 12, 8, 5 };

    for (int i = 0; i < 7; i++) {
        int y = (int)painel.y + 92 + i * 34;
        DrawText(nomes[i], (int)painel.x + 30, y, 12, WHITE);
        DrawRectangle((int)painel.x + 30, y + 16, 380, 6, (Color){ 40, 30, 55, 255 });
        DrawRectangle((int)painel.x + 30, y + 16, 380 * valores[i] / maximos[i], 6, cor);
        DrawText(TextFormat("%d/%d", valores[i], maximos[i]), (int)painel.x + 425, y + 10, 14, LIGHTGRAY);
    }

    const char *total = TextFormat("ÍNDICE: %d/100   (final bom: %d ou mais)", r->total, INDICE_FINAL_BOM);
    DrawText(total, LARGURA_TELA / 2 - MeasureText(total, 18) / 2, (int)painel.y + 340, 18, GOLD);
    DrawText(TextFormat("Pontos: %d", b->pontosFinais), LARGURA_TELA / 2 - MeasureText(TextFormat("Pontos: %d", b->pontosFinais), 14) / 2,
             (int)painel.y + 370, 14, WHITE);

    const char *luta = TextFormat("Parries perfeitos: %d     Combos dedutivos: %d", b->luci.parriesPerfeitos, b->luci.combosConcluidos);
    DrawText(luta, LARGURA_TELA / 2 - MeasureText(luta, 12) / 2, (int)painel.y + 394, 12, LIGHTGRAY);

    if (b->tempoResultado > 1.0f && ((int)(t * 2.0f)) % 2 == 0) {
        const char *ok = "ENTER / CLIQUE - ranking e menu";
        DrawText(ok, LARGURA_TELA / 2 - MeasureText(ok, 14) / 2, (int)(painel.y + painel.height) - 34, 14, GOLD);
    }
}
