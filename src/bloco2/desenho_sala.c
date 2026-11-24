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

// ---------- Desenho ----------

void DesenharEstante(Rectangle r, float tempo)
{
    static const Color lombadas[5] = { { 120, 60, 200, 255 }, { 60, 140, 220, 255 }, { 200, 80, 160, 255 },
                                       { 80, 200, 220, 255 }, { 230, 190, 80, 255 } };

    DrawRectangleRec(r, (Color){ 40, 28, 64, 255 });

    // Lombadas dos livros (o comprimento da estante decide a direção)
    bool deitada = r.width >= r.height;
    int passo = 7;
    int n = (int)((deitada ? r.width : r.height) - 6) / passo;

    for (int i = 0; i < n; i++) {
        Color c = lombadas[Ruido((unsigned int)(r.x * 7 + r.y * 13 + i)) % 5];
        if (deitada) DrawRectangle((int)r.x + 3 + i * passo, (int)r.y + 4, passo - 2, (int)r.height - 8, Fade(c, 0.8f));
        else         DrawRectangle((int)r.x + 4, (int)r.y + 3 + i * passo, (int)r.width - 8, passo - 2, Fade(c, 0.8f));
    }

    DrawRectangleLinesEx(r, 2, Fade(SKYBLUE, 0.35f + 0.1f * sinf(tempo * 2.0f + r.x)));
}

// Afixos de elite: o nome em cima, o aviso do PULSO e os escudos do ORBITAL
void DesenharAfixosElite(const Biblioteca *b, float tempo)
{
    for (int i = 0; i < MAX_INIMIGOS; i++) {
        const Inimigo *e = &b->ini[i];
        if (!e->ativo || !e->elite || e->afixo == AFIXO_NENHUM || e->surgindo > 0.0f)
            continue;

        const char *nome = NOMES_AFIXOS[e->afixo];
        DrawText(nome, (int)e->pos.x - MeasureText(nome, 10) / 2, (int)(e->pos.y - e->raio) - 30, 10, GOLD);

        if (e->afixo == AFIXO_PULSO && e->cronoAfixo < AFIXO_PULSO_AVISO && !e->selado) {
            float k = 1.0f - e->cronoAfixo / AFIXO_PULSO_AVISO;
            DrawCircleLines((int)e->pos.x, (int)e->pos.y, e->raio + 4 + 50.0f * (1.0f - k), Fade(GOLD, 0.4f + 0.6f * k));
            DrawCircleV(e->pos, e->raio + 4, Fade(GOLD, 0.25f * k));
        }
        if (e->afixo == AFIXO_DIVISAO) {
            DrawLine((int)e->pos.x, (int)(e->pos.y - e->raio - 4), (int)e->pos.x, (int)(e->pos.y + e->raio + 4), Fade(GOLD, 0.5f + 0.3f * sinf(tempo * 6.0f)));
        }
        if (e->afixo == AFIXO_ORBITAL) {
            for (int k = 0; k < AFIXO_ORBES; k++) {
                if (e->hpOrbe[k] <= 0.0f) continue;
                Vector2 o = RogueOrbePos(e, k);
                float v = e->hpOrbe[k] / AFIXO_ORBE_HP;
                DrawCircleV(o, AFIXO_ORBE_RAIO + 3, Fade(GOLD, 0.2f));
                DrawPoly(o, 6, AFIXO_ORBE_RAIO, tempo * 120.0f, Fade(GOLD, 0.45f + 0.55f * v));
                DrawPolyLines(o, 6, AFIXO_ORBE_RAIO, tempo * 120.0f, WHITE);
            }
        }
    }
}

// Laboratórios em ação: o núcleo com a janela de contexto e os blocos; o firewall e os pacotes
void DesenharLabAcao(const Biblioteca *b, float tempo)
{
    const LabEdu *L = &b->lab;

    if (b->tipoSala != SALA_LABORATORIO || !L->emAcao)
        return;

    if (L->tipo == LAB_NUCLEO) {
        const DesafioNucleo *d = &DESAFIOS_NUCLEO[L->missao];
        Color roxo = { 150, 140, 255, 255 };
        Vector2 c = b->posObjetoSala;
        if (L->tremeNucleo > 0.0f) { c.x += (float)GetRandomValue(-3, 3); c.y += (float)GetRandomValue(-3, 3); }
        float frac = (float)L->carga / (float)d->capacidade;
        Color corCarga = (frac < 0.75f) ? roxo : ((frac <= 1.0f) ? ORANGE : RED);

        DrawCircleV(c, 58, Fade(roxo, 0.08f + 0.04f * sinf(tempo * 3.0f)));
        DrawRing(c, 46, 54, 0, 360, 48, Fade(GRAY, 0.25f));
        DrawRing(c, 46, 54, -90.0f, -90.0f + 360.0f * Clamp(frac, 0.0f, 1.0f), 48, corCarga);
        DrawCircleV(c, 30, (Color){ 30, 24, 60, 255 });
        DrawCircleLines((int)c.x, (int)c.y, 30, roxo);
        for (int k = 0; k < 3; k++) {
            float a = tempo * (1.5f + k) + k * 2.1f;
            DrawCircleV((Vector2){ c.x + cosf(a) * 16, c.y + sinf(a) * 16 }, 3, roxo);
        }
        const char *t = TextFormat("%d/%d tokens", L->carga, d->capacidade);
        DrawText(t, (int)c.x - MeasureText(t, 10) / 2, (int)c.y + 60, 10, corCarga);

        for (int i = 0; i < LAB_MAX_OBJ_ACAO; i++) {
            const ObjAcaoLab *o = &L->obj[i];
            if (!o->ativo) continue;
            const BlocoContexto *bl = &d->bloco[o->id];
            float r = LabRaioBloco(bl->tokens);
            Color cor = o->ejetado ? Fade(RED, Clamp(o->vida, 0.0f, 1.0f)) : (Color){ 200, 195, 255, 255 };

            // A cápsula: o tamanho é o "peso" em tokens (o que é relevante, o jogador decide lendo)
            Rectangle cap = { o->pos.x - r - 34, o->pos.y - 15, 2 * r + 68, 30 };
            DrawRectangleRounded(cap, 0.5f, 8, Fade((Color){ 30, 26, 64, 255 }, o->ejetado ? 0.5f : 0.92f));
            DrawRectangleRoundedLines(cap, 0.5f, 8, cor);
            char linhas[160];
            QuebrarTexto(bl->texto, (int)cap.width - 14, 10, linhas, (int)sizeof(linhas));
            int nl = ContarLinhas(linhas);
            if (nl > 2) { int q = 0; for (char *x = linhas; *x; x++) if (*x == '\n' && ++q == 2) { *x = '\0'; break; } nl = 2; }
            DesenharTextoLinhas(linhas, (int)cap.x + 7, (int)o->pos.y - nl * EspacoLinhaTexto(6, 10) + 1, 10, 12, o->ejetado ? cor : WHITE);
            for (int k = 0; k < bl->tokens; k++)       // os tokens embaixo
                DrawRectangle((int)(o->pos.x - bl->tokens * 3.5f + k * 7.0f), (int)o->pos.y + 18, 5, 4, roxo);
        }
    } else {
        // A linha do firewall no meio, você à esquerda e a IA na nuvem à direita
        Color verde = { 0, 228, 48, 255 };
        for (int y = 90; y < 520; y += 14)
            DrawRectangle(398, y, 4, 8, Fade(verde, 0.35f + 0.25f * sinf(tempo * 4.0f + y * 0.05f)));
        DrawText("FIREWALL", 400 - MeasureText("FIREWALL", 10) / 2, 76, 10, verde);

        Vector2 nuvem = { 742, 300 };
        DrawCircleV((Vector2){ nuvem.x - 12, nuvem.y + 4 }, 16, (Color){ 60, 70, 120, 255 });
        DrawCircleV((Vector2){ nuvem.x + 6, nuvem.y - 6 }, 20, (Color){ 60, 70, 120, 255 });
        DrawCircleV((Vector2){ nuvem.x + 14, nuvem.y + 8 }, 14, (Color){ 60, 70, 120, 255 });
        DrawText("IA", (int)nuvem.x - 2, (int)nuvem.y - 6, 14, WHITE);
        DrawLine((int)LAB_ACAO_X_SAIDA, 110, (int)LAB_ACAO_X_SAIDA, 500, Fade(SKYBLUE, 0.25f));

        for (int i = 0; i < LAB_MAX_OBJ_ACAO; i++) {
            const ObjAcaoLab *o = &L->obj[i];
            if (!o->ativo) continue;
            const char *rot = o->anonimizado ? "* * * ANÔNIMO * * *" : ROTULOS_PACOTES[o->id];
            int w = MeasureText(rot, 10) + 30;
            Rectangle cap = { o->pos.x - w / 2.0f, o->pos.y - 12, (float)w, 24 };
            Color cor = o->anonimizado ? (Color){ 0, 228, 255, 255 } : GOLD;
            for (int k = 1; k <= 3; k++)            // rastro
                DrawRectangle((int)(cap.x - 10 * k), (int)o->pos.y - 2, 6, 4, Fade(cor, 0.3f / k));
            DrawRectangleRounded(cap, 0.4f, 6, Fade((Color){ 24, 20, 44, 255 }, 0.95f));
            DrawRectangleRoundedLines(cap, 0.4f, 6, cor);
            DrawRectangle((int)cap.x + 6, (int)o->pos.y - 5, 10, 10, cor);           // o "envelope"
            DrawLine((int)cap.x + 6, (int)o->pos.y - 5, (int)cap.x + 11, (int)o->pos.y, BLACK);
            DrawLine((int)cap.x + 16, (int)o->pos.y - 5, (int)cap.x + 11, (int)o->pos.y, BLACK);
            DrawText(rot, (int)cap.x + 22, (int)o->pos.y - 5, 10, WHITE);
        }
    }
}

// A tarefa do laboratório em ação, numa linha embaixo do objetivo
void DesenharTarefaLabAcao(const Biblioteca *b)
{
    const LabEdu *L = &b->lab;
    if (b->tipoSala != SALA_LABORATORIO || !L->emAcao)
        return;

    const char *t = (L->tipo == LAB_NUCLEO) ? TextFormat("TAREFA DA IA: %s", DESAFIOS_NUCLEO[L->missao].tarefa)
                                            : "Senha, documento ou código: BLOQUEIE. Nome ou telefone: ANONIMIZE.";
    int w = MeasureText(t, 10);
    if (w > 700) { t = TextFormat("TAREFA: %s", DESAFIOS_NUCLEO[L->missao].titulo); w = MeasureText(t, 10); }
    DrawRectangle(400 - w / 2 - 8, 44, w + 16, 16, Fade(BLACK, 0.7f));
    DrawText(t, 400 - w / 2, 47, 10, (L->tipo == LAB_NUCLEO) ? (Color){ 200, 195, 255, 255 } : GOLD);
}

// Console de saída da Câmara: uma porta de luz azul; perto, "SEGURE E PARA SAIR" com o anel enchendo
void DesenharSaidaSilogista(const Biblioteca *b, float tempo)
{
    if (b->tipoSala != SALA_SILOGISTA || !b->silogista.ativo || b->limpa)
        return;

    Vector2 c = POS_SAIDA_SILOGISTA;
    Color azul = { 120, 110, 255, 255 };
    float pulso = 0.5f + 0.5f * sinf(tempo * 3.0f);

    DrawRectangle((int)c.x - 16, (int)c.y - 30, 32, 44, (Color){ 30, 24, 60, 255 });
    DrawRectangle((int)c.x - 12, (int)c.y - 26, 24, 36, Fade(azul, 0.25f + 0.2f * pulso));
    DrawRectangleLines((int)c.x - 16, (int)c.y - 30, 32, 44, azul);
    DrawTriangle((Vector2){ c.x - 6, c.y - 14 }, (Vector2){ c.x - 6, c.y + 2 }, (Vector2){ c.x + 6, c.y - 6 }, WHITE);    // seta "sair"
    DrawText("SAÍDA", (int)c.x - MeasureText("SAÍDA", 10) / 2, (int)c.y + 18, 10, azul);

    if (RoguePertoSaidaSilogista(b)) {
        float k = Clamp(b->segurandoSaida / SAIDA_SILOGISTA_SEGURAR, 0.0f, 1.0f);
        DrawRing(c, 30, 34, -90.0f, -90.0f + 360.0f * k, 32, GOLD);
        DrawCircleLines((int)c.x, (int)c.y, 34, Fade(GOLD, 0.4f));
        const char *t = "SEGURE E PARA SAIR DA LUTA";
        DrawRectangle((int)c.x - 10, (int)c.y - 62, MeasureText(t, 10) + 16, 18, Fade((Color){ 16, 12, 30, 255 }, 0.85f));
        DrawRectangleLines((int)c.x - 10, (int)c.y - 62, MeasureText(t, 10) + 16, 18, Fade(GOLD, 0.7f));
        DrawText(t, (int)c.x - 2, (int)c.y - 58, 10, GOLD);
    }
}

// ---------- Fim do Bloco 1: desenho da sala da ALUCINAÇÃO MESTRE ----------


// O pedestal com o Disparador de Dados (um facho dourado chama a atenção)
void DesenharPedestalArmaMestre(Vector2 c, float tempo)
{
    float pulso = 0.5f + 0.5f * sinf(tempo * 3.0f);
    DrawTriangle((Vector2){ c.x, c.y - 150 }, (Vector2){ c.x - 34, c.y + 6 }, (Vector2){ c.x + 34, c.y + 6 }, Fade(GOLD, 0.06f + 0.05f * pulso));
    DrawEllipse((int)c.x, (int)c.y + 18, 34, 10, Fade(BLACK, 0.4f));
    DrawRectangle((int)c.x - 22, (int)c.y + 2, 44, 16, (Color){ 70, 45, 110, 255 });
    DrawRectangle((int)c.x - 28, (int)c.y + 14, 56, 8, (Color){ 50, 32, 84, 255 });
    DrawRectangleLines((int)c.x - 22, (int)c.y + 2, 44, 16, GOLD);
    float y = c.y - 14 + sinf(tempo * 2.2f) * 4.0f;
    DrawCircleV((Vector2){ c.x, y }, 18 + 3 * pulso, Fade(GOLD, 0.12f));
    DesenharIconeArma(ARMA_DISPARADOR, (Vector2){ c.x, y }, 1.6f, tempo);
}

// Alvo de treino: um monitorzinho com "?" que balança
void DesenharAlvoTreino(Vector2 c, float tempo, int i)
{
    float y = c.y + sinf(tempo * 2.5f + i) * 4.0f;
    DrawRectangle((int)c.x - 16, (int)y - 12, 32, 24, (Color){ 255, 161, 0, 255 });
    DrawRectangle((int)c.x - 13, (int)y - 9, 26, 18, (Color){ 20, 12, 36, 255 });
    DrawText("?", (int)c.x - 3, (int)y - 8, 16, (Color){ 255, 161, 0, 255 });
    DrawCircleLines((int)c.x, (int)y, 24 + 2 * sinf(tempo * 4.0f), Fade(GOLD, 0.5f));
    DrawRectangle((int)c.x - 2, (int)y + 12, 4, 8, GRAY);
}
