#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <time.h>
#include "../../include/core/fonte.h"
#include "../../include/core/mapas.h"
#include "../../include/telas/ranking.h"
#include "../../include/audio/audio.h"
#include "../../include/bloco1/minigame_visual.h"
#include "../../include/core/save.h"
#include "../../include/core/telemetria.h"
#include "../../include/core/texto.h"
#include "../../include/entidades/jogador.h"
#include "../../include/entidades/luci_companion.h"


int NovoIdPartida(void)
{
    return 1 + (int)(time(NULL) % 1000000) * 1000 + GetRandomValue(0, 999);
}

// Arquivo inexistente ou estragado = ranking vazio
void LerRanking(Ranking *r)
{
    FILE *f = fopen(ARQUIVO_RANKING, "rb");
    bool ok = false;

    if (f != NULL) {
        ok = (fread(r, sizeof(*r), 1, f) == 1);
        fclose(f);
    }

    ok = ok && r->assinatura == ASSINATURA_RANKING && r->versao == VERSAO_RANKING &&
         r->total >= 0 && r->total <= MAX_RANKING;

    for (int i = 0; ok && i < r->total; i++) {
        r->entradas[i].nickname[3] = '\0';
        for (int c = 0; c < 3; c++)
            if (r->entradas[i].nickname[c] < 'A' || r->entradas[i].nickname[c] > 'Z')
                ok = false;
    }

    if (!ok) {
        memset(r, 0, sizeof(*r));
        r->assinatura = ASSINATURA_RANKING;
        r->versao = VERSAO_RANKING;
    }
}

// Mesmo cuidado do save: grava no temporário e depois troca
static bool GravarRanking(const Ranking *r)
{
    FILE *f = fopen(ARQUIVO_RANKING_TEMP, "wb");

    if (f == NULL)
        return false;

    bool ok = (fwrite(r, sizeof(*r), 1, f) == 1);

    if (fclose(f) != 0)
        ok = false;

    if (!ok) {
        remove(ARQUIVO_RANKING_TEMP);
        return false;
    }

    remove(ARQUIVO_RANKING);

    return (rename(ARQUIVO_RANKING_TEMP, ARQUIVO_RANKING) == 0);
}

// Coloca (ou melhora) a pontuação desta partida.
// Devolve a posição (0 = 1º lugar) ou -1 se ficou fora do TOP 10.
static int RegistrarNoRanking(Ranking *r, const char *nickname, int pontos, int idPartida)
{
    LerRanking(r);      // relê: outra partida pode ter gravado desde a última vez

    int i = -1;

    for (int k = 0; k < r->total; k++)
        if (r->entradas[k].idPartida == idPartida) { i = k; break; }

    if (i >= 0) {
        if (pontos <= r->entradas[i].pontos)
            return i;
    } else if (r->total < MAX_RANKING) {
        i = r->total++;
    } else if (pontos > r->entradas[MAX_RANKING - 1].pontos) {
        i = MAX_RANKING - 1;                // tira o último colocado
    } else {
        return -1;
    }

    memcpy(r->entradas[i].nickname, nickname, 4);
    r->entradas[i].nickname[3] = '\0';
    r->entradas[i].pontos = pontos;
    r->entradas[i].idPartida = idPartida;

    // Sobe até o lugar certo (empate: quem chegou antes fica na frente)
    while (i > 0 && r->entradas[i].pontos > r->entradas[i - 1].pontos) {
        EntradaRanking troca = r->entradas[i];
        r->entradas[i] = r->entradas[i - 1];
        r->entradas[i - 1] = troca;
        i--;
    }

    if (!GravarRanking(r))
        TraceLog(LOG_WARNING, "RANKING: falha ao gravar " ARQUIVO_RANKING);

    Telemetria("ranking", nickname, (float)pontos, i + 1);
    return i;
}

// Nickname novo não pode repetir um que já está na tabela (relê o arquivo antes de conferir)
bool NicknameDisponivel(Ranking *r, const char *nickname)
{
    LerRanking(r);

    for (int i = 0; i < r->total; i++)
        if (strncmp(r->entradas[i].nickname, nickname, 3) == 0)
            return false;

    return true;
}

void AbrirTelaRanking(TelaRanking *t, Ranking *r, const char *titulo,
                             const char *nickname, int pontos, int idPartida)
{
    t->aberto = true;
    t->titulo = titulo;
    t->pontos = pontos;
    t->idPartida = idPartida;
    t->posicao = RegistrarNoRanking(r, nickname, pontos, idPartida);
    t->inicio = GetTime();
    t->revelado = false;
}

// Animação da tela do ranking (segundos desde que abriu): as linhas entram uma a uma, a pontuação
// conta de 0 até o total e, no fim, a posição aparece com um "pulo"
#define RANK_LINHAS_INICIO   0.25f
#define RANK_LINHA_PASSO     0.05f
#define RANK_CONTA_INICIO    0.80f
#define RANK_CONTA_DURACAO   1.20f
#define RANK_LUGAR_TEMPO     (RANK_CONTA_INICIO + RANK_CONTA_DURACAO + 0.15f)
#define RANK_ANIM_FIM        (RANK_LUGAR_TEMPO + 0.25f)

static float FracaoContagem(double passou)
{
    float p = Clamp((float)(passou - RANK_CONTA_INICIO) / RANK_CONTA_DURACAO, 0.0f, 1.0f);
    return 1.0f - (1.0f - p) * (1.0f - p) * (1.0f - p);         // começa rápido e freia no fim
}

// Devolve true quando o jogador fecha o ranking. O primeiro ENTER (ou clique) durante a
// animação só pula para o fim dela; o seguinte fecha.
bool AtualizarTelaRanking(TelaRanking *t)
{
    static double ultimoTique = 0.0;

    if (!t->aberto)
        return false;

    double passou = GetTime() - t->inicio;

    // Tique enquanto a pontuação conta (mais agudo perto do fim) e um acerto quando a posição aparece
    if (t->pontos > 0 && passou > RANK_CONTA_INICIO && passou < RANK_CONTA_INICIO + RANK_CONTA_DURACAO && GetTime() - ultimoTique > 0.07) {
        ultimoTique = GetTime();
        TocarSomTom(sfxSelecao, 1.0f + 0.6f * (float)((passou - RANK_CONTA_INICIO) / RANK_CONTA_DURACAO));
    }
    if (!t->revelado && passou >= RANK_LUGAR_TEMPO) {
        t->revelado = true;
        TocarSom(sfxAcerto);
    }

    if (passou < 0.6)
        return false;

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE) || CliqueMouse()) {

        if (passou < RANK_ANIM_FIM) {
            t->inicio = GetTime() - RANK_ANIM_FIM;
            t->revelado = true;
            return false;
        }

        t->aberto = false;
        return true;
    }

    return false;
}

// As 10 linhas do ranking. idDestaque = partida que pisca em dourado (0 = nenhuma).
// aberto = segundos desde que a tela abriu: as linhas entram uma a uma (< 0 = todas já na tela)
static void DesenharListaRanking(const Ranking *r, int x, int y, int largura, int fonte, int idDestaque, float tempo, float aberto)
{
    static const Color podio[3] = { { 255, 203, 0, 255 }, { 200, 200, 220, 255 }, { 205, 127, 50, 255 } };
    int alturaLinha = fonte + 8;

    for (int i = 0; i < MAX_RANKING; i++) {

        float k = (aberto < 0.0f) ? 1.0f : Clamp((aberto - RANK_LINHAS_INICIO - i * RANK_LINHA_PASSO) / 0.2f, 0.0f, 1.0f);
        if (k <= 0.0f)
            continue;

        int lx = x + (int)((1.0f - k) * 24.0f);                  // entra deslizando da direita
        int ly = y + i * alturaLinha;
        bool existe = (i < r->total);
        bool destaque = existe && idDestaque != 0 && r->entradas[i].idPartida == idDestaque;
        Color cor = !existe ? Fade(GRAY, 0.5f) : (i < 3 ? podio[i] : WHITE);

        if (destaque) {
            DrawRectangle(lx - 6, ly - 4, largura + 12, alturaLinha, Fade(GOLD, (0.18f + 0.1f * sinf(tempo * 6.0f)) * k));
            if (k >= 1.0f)                                      // a Luci só aparece com a linha já no lugar
                DesenharCursorLuci((float)lx - 16, (float)ly + fonte / 2.0f - 1);
            cor = GOLD;
        }

        cor = Fade(cor, (cor.a / 255.0f) * k);
        DrawText(TextFormat("%2d.", i + 1), lx, ly, fonte, cor);
        DrawText(existe ? r->entradas[i].nickname : "---", lx + fonte * 3, ly, fonte, cor);

        const char *pts = existe ? TextFormat("%d", r->entradas[i].pontos) : "-";
        DrawText(pts, lx + largura - MeasureText(pts, fonte), ly, fonte, cor);
    }
}

// Ranking por cima do jogo, com a posição desta partida
void DesenharTelaRanking(const TelaRanking *t, const Ranking *r, float tempo)
{
    if (!t->aberto)
        return;

    float k = (float)(GetTime() - t->inicio) / 0.3f;
    if (k > 1.0f) k = 1.0f;

    DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, 0.7f * k));

    Rectangle painel = { 220, 60 + (1.0f - k) * 30.0f, 360, 480 };

    DrawRectangleRec(painel, (Color){ 18, 15, 30, 255 });
    DrawRectangleLinesEx(painel, 4, GOLD);

    int cx = (int)(painel.x + painel.width / 2);

    DrawText(t->titulo, cx - MeasureText(t->titulo, 12) / 2, (int)painel.y + 18, 12, Fade(SKYBLUE, 0.9f));
    DrawText(TITULO_RANKING, cx - MeasureText(TITULO_RANKING, 26) / 2, (int)painel.y + 38, 26, GOLD);

    double passou = GetTime() - t->inicio;

    DesenharListaRanking(r, (int)painel.x + 70, (int)painel.y + 90, 220, 20, t->idPartida, tempo, (float)passou);

    // A pontuação conta de 0 até o total
    if (passou >= RANK_CONTA_INICIO) {
        float f = FracaoContagem(passou);
        const char *pontos = TextFormat("SUA PONTUAÇÃO: %d", (int)(t->pontos * f + 0.5f));
        DrawText(pontos, cx - MeasureText(pontos, 18) / 2, (int)painel.y + 380, 18, (f >= 1.0f) ? GOLD : WHITE);
    }

    // A posição aparece no fim, com um pulo
    if (passou >= RANK_LUGAR_TEMPO) {
        float q = Clamp((float)(passou - RANK_LUGAR_TEMPO) / 0.25f, 0.0f, 1.0f);
        int tam = 14 + (int)(8.0f * (1.0f - q) * q * 4.0f * 0.5f);      // cresce e volta ao tamanho normal
        const char *lugar = (t->posicao >= 0) ? TextFormat("VOCÊ ESTÁ EM %dº LUGAR!", t->posicao + 1)
                                               : "Fora do TOP 10... jogue de novo para entrar!";
        Color corLugar = (t->posicao == 0) ? GOLD : (t->posicao > 0 ? LIME : ORANGE);
        DrawText(lugar, cx - MeasureText(lugar, tam) / 2, (int)painel.y + 408 - (tam - 14) / 2, tam, Fade(corLugar, q));
    }

    if (passou > RANK_ANIM_FIM && ((int)(GetTime() * 2.0)) % 2 == 0)
        DesenharDicaAvancar((int)(painel.x + painel.width) - 80, (int)(painel.y + painel.height) - 26, 12);
}

// Painel do ranking no menu principal
void DesenharRankingMenu(const Ranking *r, float tempo)
{
    Rectangle painel = { 566, 262, 214, 278 };

    DrawRectangleRec(painel, Fade((Color){ 18, 15, 30, 255 }, 0.9f));
    DrawRectangleLinesEx(painel, 2, Fade(GOLD, 0.6f + 0.2f * sinf(tempo * 2.0f)));

    DrawText(TITULO_RANKING, (int)(painel.x + painel.width / 2) - MeasureText(TITULO_RANKING, 16) / 2, (int)painel.y + 12, 16, GOLD);

    if (r->total == 0) {
        const char *vazio = "Ninguém jogou ainda.";
        DrawText(vazio, (int)(painel.x + painel.width / 2) - MeasureText(vazio, 12) / 2, (int)painel.y + 130, 12, LIGHTGRAY);
        DrawText("Seja o primeiro!", (int)(painel.x + painel.width / 2) - MeasureText("Seja o primeiro!", 12) / 2, (int)painel.y + 148, 12, GOLD);
        return;
    }

    DesenharListaRanking(r, (int)painel.x + 24, (int)painel.y + 42, 166, 14, 0, tempo, -1.0f);
}
