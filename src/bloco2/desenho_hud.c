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
// REFORMULAÇÃO DO BLOCO 2: DESENHO DOS SISTEMAS NOVOS
//   Tudo é desenhado com formas do Raylib. Os ícones das salas podem ganhar
//   sprites opcionais em resources/sprites/biblioteca/salas/<nome>.png
//   (ex.: combate.png, elite.png, evento.png). Se o arquivo não existir,
//   o ícone desenhado à mão é usado: o jogo nunca depende deles.
// =============================================================

static const char *ARQUIVOS_ICONES_SALAS[NUM_TIPOS_SALA] = {
    "atrio", "combate", "educativa", "tesouro", "alucinador", "silogista", "nucleo", "luci",
    "elite", "evento", "descanso", "desafio", "secreta", "laboratorio", "especial", "mestre"
};
static Texture2D iconesSalas[NUM_TIPOS_SALA];
static bool iconesSalasCarregados = false;

void CarregarIconesBiblioteca(void)
{
    if (iconesSalasCarregados)
        return;
    iconesSalasCarregados = true;

    for (int t = 0; t < NUM_TIPOS_SALA; t++) {
        const char *arq = TextFormat("resources/sprites/biblioteca/salas/%s.png", ARQUIVOS_ICONES_SALAS[t]);
        iconesSalas[t] = FileExists(arq) ? LoadTexture(arq) : (Texture2D){0};
        if (iconesSalas[t].id > 0) SetTextureFilter(iconesSalas[t], TEXTURE_FILTER_BILINEAR);
    }
}

void DescarregarIconesBiblioteca(void)
{
    for (int t = 0; t < NUM_TIPOS_SALA; t++)
        if (iconesSalas[t].id > 0) UnloadTexture(iconesSalas[t]);
    memset(iconesSalas, 0, sizeof(iconesSalas));
    iconesSalasCarregados = false;
}

// Ícone do tipo de sala (portas, mini-mapa e mapa grande). e = escala (1 = cerca de 24 px)
static void DesenharIconeSala(TipoSalaRogue t, Vector2 c, float e, float tempo)
{
    Color cor = CORES_SALAS_ROGUE[t];

    if (t < NUM_TIPOS_SALA && iconesSalas[t].id > 0) {
        Texture2D tx = iconesSalas[t];
        float lado = 24.0f * e;
        DrawTexturePro(tx, (Rectangle){ 0, 0, (float)tx.width, (float)tx.height },
                       (Rectangle){ c.x - lado / 2, c.y - lado / 2, lado, lado }, (Vector2){ 0, 0 }, 0.0f, WHITE);
        return;
    }

    switch (t) {
        case SALA_COMBATE:      // duas lâminas cruzadas
            DrawLineEx((Vector2){ c.x - 8 * e, c.y - 8 * e }, (Vector2){ c.x + 8 * e, c.y + 8 * e }, 3 * e, cor);
            DrawLineEx((Vector2){ c.x + 8 * e, c.y - 8 * e }, (Vector2){ c.x - 8 * e, c.y + 8 * e }, 3 * e, cor);
            DrawLineEx((Vector2){ c.x - 9 * e, c.y + 3 * e }, (Vector2){ c.x - 3 * e, c.y + 9 * e }, 2 * e, WHITE);
            DrawLineEx((Vector2){ c.x + 9 * e, c.y + 3 * e }, (Vector2){ c.x + 3 * e, c.y + 9 * e }, 2 * e, WHITE);
            break;

        case SALA_ELITE: {      // coroa
            Vector2 p0 = { c.x - 10 * e, c.y + 6 * e }, p1 = { c.x + 10 * e, c.y + 6 * e };
            DrawRectangleV((Vector2){ p0.x, p0.y - 3 * e }, (Vector2){ 20 * e, 4 * e }, GOLD);
            DrawTriangle((Vector2){ c.x - 10 * e, c.y + 3 * e }, (Vector2){ c.x - 4 * e, c.y + 3 * e }, (Vector2){ c.x - 9 * e, c.y - 8 * e }, GOLD);
            DrawTriangle((Vector2){ c.x - 4 * e, c.y + 3 * e }, (Vector2){ c.x + 4 * e, c.y + 3 * e }, (Vector2){ c.x, c.y - 10 * e }, GOLD);
            DrawTriangle((Vector2){ c.x + 4 * e, c.y + 3 * e }, (Vector2){ c.x + 10 * e, c.y + 3 * e }, (Vector2){ c.x + 9 * e, c.y - 8 * e }, GOLD);
            DrawCircleV((Vector2){ c.x, c.y - 1 * e }, 2 * e, RED);
            (void)p1;
        } break;

        case SALA_EDUCATIVA:    // livro aberto
            DrawRectangleV((Vector2){ c.x - 10 * e, c.y - 7 * e }, (Vector2){ 9 * e, 14 * e }, cor);
            DrawRectangleV((Vector2){ c.x + 1 * e, c.y - 7 * e }, (Vector2){ 9 * e, 14 * e }, cor);
            for (int k = 0; k < 3; k++) {
                DrawLineEx((Vector2){ c.x - 8 * e, c.y - 3 * e + k * 4 * e }, (Vector2){ c.x - 3 * e, c.y - 3 * e + k * 4 * e }, 1, (Color){ 10, 30, 50, 255 });
                DrawLineEx((Vector2){ c.x + 3 * e, c.y - 3 * e + k * 4 * e }, (Vector2){ c.x + 8 * e, c.y - 3 * e + k * 4 * e }, 1, (Color){ 10, 30, 50, 255 });
            }
            break;

        case SALA_RECOMPENSA:   // baú
            DrawRectangleV((Vector2){ c.x - 10 * e, c.y - 3 * e }, (Vector2){ 20 * e, 11 * e }, (Color){ 140, 90, 30, 255 });
            DrawRectangleV((Vector2){ c.x - 10 * e, c.y - 8 * e }, (Vector2){ 20 * e, 5 * e }, cor);
            DrawRectangleV((Vector2){ c.x - 2 * e, c.y - 4 * e }, (Vector2){ 4 * e, 5 * e }, WHITE);
            break;

        case SALA_EVENTO:       // ponto de interrogação
            DrawCircleLines((int)c.x, (int)c.y, 10 * e, cor);
            DrawText("?", (int)(c.x - MeasureText("?", (int)(18 * e)) / 2), (int)(c.y - 9 * e), (int)(18 * e), cor);
            break;

        case SALA_DESCANSO:     // poltrona e uma luz
            DrawRectangleV((Vector2){ c.x - 9 * e, c.y - 8 * e }, (Vector2){ 18 * e, 8 * e }, cor);
            DrawRectangleV((Vector2){ c.x - 11 * e, c.y }, (Vector2){ 22 * e, 6 * e }, Fade(cor, 0.8f));
            DrawRectangleV((Vector2){ c.x - 11 * e, c.y + 6 * e }, (Vector2){ 3 * e, 4 * e }, cor);
            DrawRectangleV((Vector2){ c.x + 8 * e, c.y + 6 * e }, (Vector2){ 3 * e, 4 * e }, cor);
            break;

        case SALA_DESAFIO:      // alvo
            DrawCircleLines((int)c.x, (int)c.y, 10 * e, cor);
            DrawCircleLines((int)c.x, (int)c.y, 6 * e, cor);
            DrawCircleV(c, 2.5f * e, RED);
            break;

        case SALA_ESPECIAL:     // losango de alerta (encontro especial obrigatório)
            DrawPoly(c, 4, 11 * e, 0.0f, Fade(cor, 0.35f));
            DrawPolyLinesEx(c, 4, 11 * e, 0.0f, 2 * e, cor);
            DrawRectangleV((Vector2){ c.x - 1.5f * e, c.y - 6 * e }, (Vector2){ 3 * e, 7 * e }, WHITE);
            DrawRectangleV((Vector2){ c.x - 1.5f * e, c.y + 3 * e }, (Vector2){ 3 * e, 3 * e }, WHITE);
            break;

        case SALA_LABORATORIO:  // frasco de laboratório
            DrawRectangleV((Vector2){ c.x - 3 * e, c.y - 11 * e }, (Vector2){ 6 * e, 8 * e }, cor);
            DrawTriangle((Vector2){ c.x - 10 * e, c.y + 9 * e }, (Vector2){ c.x + 10 * e, c.y + 9 * e }, (Vector2){ c.x, c.y - 6 * e }, cor);
            DrawCircleV((Vector2){ c.x - 2 * e, c.y + 5 * e }, 1.5f * e, WHITE);
            DrawCircleV((Vector2){ c.x + 3 * e, c.y + 2 * e }, 1.0f * e, WHITE);
            break;

        case SALA_SECRETA:      // olho
            DrawEllipse((int)c.x, (int)c.y, 11 * e, 6 * e, cor);
            DrawCircleV(c, 4 * e, (Color){ 20, 0, 30, 255 });
            DrawCircleV((Vector2){ c.x + 1 * e, c.y - 1 * e }, 1.5f * e, WHITE);
            break;

        case SALA_SILOGISTA:    // "portanto" (três pontos) dentro de uma engrenagem
            for (int k = 0; k < 8; k++) {
                float a = (float)k / 8.0f * 2.0f * PI + tempo * 0.6f;
                DrawCircleV((Vector2){ c.x + cosf(a) * 10 * e, c.y + sinf(a) * 10 * e }, 2 * e, cor);
            }
            DrawCircleV((Vector2){ c.x, c.y - 4 * e }, 2 * e, WHITE);
            DrawCircleV((Vector2){ c.x - 4 * e, c.y + 3 * e }, 2 * e, WHITE);
            DrawCircleV((Vector2){ c.x + 4 * e, c.y + 3 * e }, 2 * e, WHITE);
            break;

        case SALA_CHEFE:        // o olho do Alucinador
            for (int k = 0; k < 6; k++) {
                float a = (float)k / 6.0f * 2.0f * PI + tempo;
                DrawLineEx(c, (Vector2){ c.x + cosf(a) * 12 * e, c.y + sinf(a) * 12 * e }, 2 * e, Fade(cor, 0.7f));
            }
            DrawCircleV(c, 8 * e, cor);
            DrawCircleV(c, 4 * e, WHITE);
            DrawCircleV(c, 2 * e, BLACK);
            break;

        default:
            DrawCircleV(c, 7 * e, cor);
            break;
    }
}

static void DesenharIconeRecompensa(TipoRecompensa r, Vector2 c, float e, float tempo)
{
    switch (r) {
        case RECOMPENSA_MELHORIA:
            DrawCircleV(c, 7 * e, (Color){ 0, 228, 255, 255 });
            DrawTriangle((Vector2){ c.x, c.y - 5 * e }, (Vector2){ c.x - 4 * e, c.y + 1 * e }, (Vector2){ c.x + 4 * e, c.y + 1 * e }, WHITE);
            DrawRectangleV((Vector2){ c.x - 1.5f * e, c.y }, (Vector2){ 3 * e, 4 * e }, WHITE);
            break;
        case RECOMPENSA_RARA:
            DrawCircleV(c, 7 * e, GOLD);
            DrawPoly(c, 5, 4.5f * e, tempo * 90.0f, WHITE);
            break;
        case RECOMPENSA_FRAGMENTOS:
            DesenharFragmentoConhecimento(c, tempo);
            break;
        case RECOMPENSA_ARMA:
            DrawCircleV(c, 7 * e, ORANGE);
            DrawCircleLines((int)c.x, (int)c.y, 4 * e, WHITE);
            DrawLineEx((Vector2){ c.x - 6 * e, c.y }, (Vector2){ c.x + 6 * e, c.y }, 1.5f, WHITE);
            DrawLineEx((Vector2){ c.x, c.y - 6 * e }, (Vector2){ c.x, c.y + 6 * e }, 1.5f, WHITE);
            break;
        case RECOMPENSA_VIDA:       // patch de correção (a mesma cura do Bloco 1)
            DesenharPatchCuraEscala(c, 1.1f * e, 1.0f, tempo);
            break;
        case RECOMPENSA_SELO:       // SIMBOLO_SILOGISTA: o selo dourado da inferência (desafio opcional)
            DrawCircleV(c, 7 * e, GOLD);
            DrawCircleLines((int)c.x, (int)c.y, 7 * e, ORANGE);
            DrawText("Q", (int)(c.x - 3 * e), (int)(c.y - 5 * e), (int)fmaxf(8.0f, 10 * e), (Color){ 60, 40, 0, 255 });
            break;
        default:
            break;
    }
}

// Porta da missão: moldura na cor do tipo de sala, ícone e recompensa anunciada
void DesenharPortaMissao(const Biblioteca *b, const PortaMissao *p, float tempo)
{
    const NoMapa *n = &b->mapa.no[p->destino];
    Color cor = CORES_SALAS_ROGUE[n->tipo];
    bool aberta = b->limpa;
    float pulso = 0.5f + 0.5f * sinf(tempo * 4.0f);
    Rectangle r = p->r;
    const Color FOLHA = { 30, 10, 20, 255 };            // a porta fechada

    // Abrindo agora (a sala acabou de ficar limpa): k vai de 0 (fechada) a 1 (aberta)
    bool abrindo = false;
    float k = 1.0f, e = 1.0f;
#if PORTAS_ANIMADAS
    if (aberta && b->portasAbrindo > 0.0f) {
        abrindo = true;
        k = 1.0f - b->portasAbrindo / PORTAS_ABRIR_TEMPO;
        e = 1.0f - (1.0f - k) * (1.0f - k) * (1.0f - k);    // sai rápido e freia no fim
    }
#endif

    DrawRectangleRec(r, aberta ? Fade(cor, 0.25f + 0.15f * pulso) : FOLHA);

    if (abrindo) {
        // As duas folhas correm para os lados (porta de cima) ou para cima e para baixo (portas dos lados)
        bool deitada = r.width >= r.height;
        float metade = (deitada ? r.width : r.height) * 0.5f * (1.0f - e);
        Rectangle f1 = deitada ? (Rectangle){ r.x, r.y, metade, r.height } : (Rectangle){ r.x, r.y, r.width, metade };
        Rectangle f2 = deitada ? (Rectangle){ r.x + r.width - metade, r.y, metade, r.height }
                               : (Rectangle){ r.x, r.y + r.height - metade, r.width, metade };
        DrawRectangleRec(f1, FOLHA);
        DrawRectangleRec(f2, FOLHA);

        // A fresta acesa entre as folhas e um clarão na cor da sala, que apaga até o fim
        float apaga = (1.0f - k) * (1.0f - k);
        BeginBlendMode(BLEND_ADDITIVE);
        if (deitada) {
            DrawRectangle((int)(f1.x + f1.width) - 2, (int)r.y, 2, (int)r.height, Fade(WHITE, 0.8f * (1.0f - e)));
            DrawRectangle((int)f2.x, (int)r.y, 2, (int)r.height, Fade(WHITE, 0.8f * (1.0f - e)));
        } else {
            DrawRectangle((int)r.x, (int)(f1.y + f1.height) - 2, (int)r.width, 2, Fade(WHITE, 0.8f * (1.0f - e)));
            DrawRectangle((int)r.x, (int)f2.y, (int)r.width, 2, Fade(WHITE, 0.8f * (1.0f - e)));
        }
        for (int camada = 0; camada < 3; camada++) {       // três camadas: o clarão sai suave da porta
            float cresce = 2.0f + (4.0f + 4.0f * (float)camada) * e;
            DrawRectangleRec((Rectangle){ r.x - cresce, r.y - cresce, r.width + cresce * 2, r.height + cresce * 2 }, Fade(cor, 0.2f * apaga));
        }
        EndBlendMode();
    }

    DrawRectangleLinesEx(r, 2, aberta ? Fade(cor, 0.6f + 0.4f * pulso) : MAROON);

    // Placa logo na frente da porta (dentro da arena)
    Vector2 c = CentroRetangulo(r);
    if (p->lado == LADO_CIMA)          c.y += 34;
    else if (p->lado == LADO_ESQUERDA) c.x += 36;
    else                               c.x -= 36;

    if (abrindo) {      // uma onda sai da placa
        BeginBlendMode(BLEND_ADDITIVE);
        DrawRing(c, 17.0f + 16.0f * e, 19.0f + 16.0f * e, 0, 360, 36, Fade(cor, 0.7f * (1.0f - e)));
        EndBlendMode();
    }

    DrawCircleV(c, 17, Fade(BLACK, 0.65f));
    DrawCircleLines((int)c.x, (int)c.y, 17, aberta ? cor : Fade(cor, 0.5f));
    DesenharIconeSala(n->tipo, c, 0.85f, tempo);

    if (n->recompensa != RECOMPENSA_NENHUMA) {
        Vector2 rc = { c.x + 14, c.y + 12 };
        DrawCircleV(rc, 9, Fade(BLACK, 0.85f));
        DesenharIconeRecompensa(n->recompensa, rc, 0.8f, tempo);
    }

    if (!aberta) {      // cadeado
        DrawRectangle((int)c.x - 6, (int)c.y - 2, 12, 9, MAROON);
        DrawRing(c, 3, 5, 180, 360, 10, MAROON);
    } else if (abrindo) {   // o cadeado destrava (a argola sobe primeiro), sobe e some
        Vector2 lc = { c.x, c.y - 16.0f * e };
        float solta = 3.0f * fminf(1.0f, k * 4.0f);
        Color corCadeado = Fade(MAROON, 1.0f - e);
        DrawRectangle((int)lc.x - 6, (int)lc.y - 2, 12, 9, corCadeado);
        DrawRing((Vector2){ lc.x, lc.y - solta }, 3, 5, 180, 360, 10, corCadeado);
    }

    const char *nome = RoguePlacaNo(n);
    const char *rec = NOMES_RECOMPENSA[n->recompensa];
    int w1 = MeasureText(nome, 10), w2 = MeasureText(rec, 10);
    int linha = LinhaTexto(12);                 // a 2ª linha (recompensa) desce junto com o texto grande
    Color corTexto = aberta ? cor : Fade(cor, 0.6f);
    Color corRec = Fade(GOLD, aberta ? 1.0f : 0.6f);
    int x1, x2, y1;

    if (p->lado == LADO_CIMA)          { x1 = (int)c.x - w1 / 2;  x2 = (int)c.x - w2 / 2;  y1 = (int)c.y + 21; }
    else if (p->lado == LADO_ESQUERDA) { x1 = (int)c.x + 22;      x2 = (int)c.x + 22;      y1 = (int)c.y - 10; }
    else                               { x1 = (int)c.x - 22 - w1; x2 = (int)c.x - 22 - w2; y1 = (int)c.y - 10; }

    // Plaquinha escura atrás do nome (e da recompensa): legível sobre o chão de qualquer sala
    DrawRectangle(x1 - 3, y1 - 1, w1 + 6, linha + 1, Fade(BLACK, 0.8f));
    DrawText(nome, x1, y1, 10, corTexto);
    if (rec[0]) {
        DrawRectangle(x2 - 3, y1 + linha - 1, w2 + 6, linha + 1, Fade(BLACK, 0.8f));
        DrawText(rec, x2, y1 + linha, 10, corRec);
    }
}

// Parede corrompida (passagem secreta), a marca do scanner e a entrada por onde o jogador veio
void DesenharParedesMissao(const Biblioteca *b, float tempo)
{
    // Entrada (selada atrás do jogador)
    static const Rectangle entradas[NUM_LADOS] = { { 370, 60, 60, 14 }, { 40, 260, 14, 70 }, { 746, 260, 14, 70 }, { 370, 516, 60, 14 } };
    Rectangle en = entradas[b->ladoEntrada];
    DrawRectangleRec(en, (Color){ 14, 12, 26, 255 });
    DrawRectangleLinesEx(en, 1, Fade(GRAY, 0.35f));

    if (b->temParedeSecreta) {
        Rectangle r = PAREDE_SECRETA_ROGUE[b->paredeSecreta];

        if (b->passagemAberta) {
            Color cor = CORES_SALAS_ROGUE[SALA_SECRETA];
            float pulso = 0.5f + 0.5f * sinf(tempo * 5.0f);
            DrawRectangleRec(r, b->limpa ? Fade(cor, 0.3f + 0.3f * pulso) : (Color){ 30, 10, 30, 255 });
            DrawRectangleLinesEx(r, 2, b->limpa ? cor : Fade(cor, 0.5f));
            const char *t = b->limpa ? "ARQUIVO OCULTO" : "OCULTO (TRANCADO)";
            int w = MeasureText(t, 10);
            int x = (r.width > r.height) ? (int)(r.x + r.width / 2) - w / 2 : ((r.x < 400) ? (int)(r.x + r.width) + 6 : (int)r.x - w - 6);
            int y = (r.width > r.height) ? (int)(r.y + r.height) + 4 : (int)(r.y + r.height / 2) - 5;
            DrawText(t, x, y, 10, cor);
        } else {
            // Rachaduras que aumentam com os tiros (sutis no começo: é preciso reparar)
            float dano = 1.0f - b->hpParede / PAREDE_SECRETA_HP;
            float brilho = 0.18f + 0.5f * dano + ((((int)(tempo * 7.0f)) % 9 == 0) ? 0.25f : 0.0f);
            Vector2 c = CentroRetangulo(r);
            for (int k = 0; k < 5 + (int)(dano * 6); k++) {
                unsigned int h = Ruido((unsigned int)(b->paredeSecreta * 31 + k * 7));
                float ox = (float)((int)(h % 40) - 20) * (r.width > r.height ? 1.0f : 0.25f);
                float oy = (float)((int)((h >> 8) % 40) - 20) * (r.width > r.height ? 0.25f : 1.0f);
                DrawLineEx((Vector2){ c.x + ox * 0.3f, c.y + oy * 0.3f }, (Vector2){ c.x + ox, c.y + oy }, 1.5f, Fade(MAGENTA, brilho));
            }
        }
    }

    // Scanner: a marca "PASSAGEM?" (alucinando, pode estar na parede errada)
    if (b->marcaParede > 0.0f && b->paredeMarcada >= 0 && !b->passagemAberta) {
        Rectangle r = PAREDE_SECRETA_ROGUE[b->paredeMarcada];
        Vector2 c = CentroRetangulo(r);
        float dx = b->scannerErrou ? (float)GetRandomValue(-2, 2) : 0.0f;
        Vector2 t = { c.x + dx + ((r.width > r.height) ? 0.0f : ((c.x < 400) ? 46.0f : -46.0f)), c.y + ((r.width > r.height) ? 26.0f : 0.0f) };
        const char *txt = "PASSAGEM?";
        int w = MeasureText(txt, 10);
        DrawRectangle((int)t.x - w / 2 - 4, (int)t.y - 7, w + 8, 14, Fade(BLACK, 0.7f));
        DrawRectangleLines((int)t.x - w / 2 - 4, (int)t.y - 7, w + 8, 14, (Color){ 0, 228, 255, 255 });
        DrawText(txt, (int)t.x - w / 2, (int)t.y - 5, 10, (Color){ 0, 228, 255, 255 });
        DrawRectangleLinesEx((Rectangle){ r.x - 3, r.y - 3, r.width + 6, r.height + 6 }, 1, Fade((Color){ 0, 228, 255, 255 }, 0.6f));
    }
}

void DesenharArmadilhas(const Biblioteca *b, float tempo)
{
    bool desligadas = b->limpa && !(b->tipoSala == SALA_DESAFIO && b->estadoDesafio == 1);
    float apaga = desligadas ? 0.25f : 1.0f;
    bool pisca = ((int)(tempo * 10.0f)) % 2 == 0;

    for (int i = 0; i < MAX_ARMADILHAS; i++) {

        const Armadilha *a = &b->arm[i];
        if (!a->ativo) continue;

        switch (a->tipo) {

            case ARM_FEIXE: {
                Color cor = (Color){ 0, 228, 255, 255 };
                Vector2 p0, p1;
                if (a->vertical) { p0 = (Vector2){ a->r.x + a->r.width / 2, a->r.y }; p1 = (Vector2){ p0.x, a->r.y + a->r.height }; }
                else             { p0 = (Vector2){ a->r.x, a->r.y + a->r.height / 2 }; p1 = (Vector2){ a->r.x + a->r.width, p0.y }; }

                // Emissores nas paredes
                DrawRectangle((int)p0.x - 6, (int)p0.y - 6, 12, 12, Fade((Color){ 40, 60, 90, 255 }, apaga));
                DrawRectangle((int)p1.x - 6, (int)p1.y - 6, 12, 12, Fade((Color){ 40, 60, 90, 255 }, apaga));

                if (desligadas || a->fase == 0) {
                    DrawLineEx(p0, p1, 1, Fade(cor, 0.12f * apaga));
                } else if (a->fase == 1) {
                    DrawLineEx(p0, p1, pisca ? 3.0f : 1.0f, Fade(RED, pisca ? 0.8f : 0.4f));       // aviso: vai disparar
                    DrawRectangleRec(a->r, Fade(RED, 0.08f));
                } else {
                    DrawRectangleRec(a->r, Fade(cor, 0.35f));
                    DrawLineEx(p0, p1, a->vertical ? a->r.width * 0.5f : a->r.height * 0.5f, Fade(WHITE, 0.85f));
                }
            } break;

            case ARM_ZONA: {
                Color cor = (Color){ 190, 33, 255, 255 };
                if (desligadas || a->fase == 0) {
                    DrawCircleLines((int)a->pos.x, (int)a->pos.y, a->raio, Fade(cor, 0.25f * apaga));
                } else if (a->fase == 1) {
                    DrawCircleV(a->pos, a->raio, Fade(cor, pisca ? 0.18f : 0.06f));
                    DrawCircleLines((int)a->pos.x, (int)a->pos.y, a->raio, Fade(RED, 0.8f));
                } else {
                    DrawCircleV(a->pos, a->raio, Fade(cor, 0.30f));
                    DrawCircleLines((int)a->pos.x, (int)a->pos.y, a->raio, cor);
                    for (int k = 0; k < 6; k++) {       // bolhas de dados corrompidos
                        unsigned int h = Ruido((unsigned int)(i * 97 + k * 13 + (int)(tempo * 3.0f)));
                        float ang = (float)(h % 360) * DEG2RAD, d = (float)((h >> 9) % (unsigned int)a->raio);
                        DrawCircleV((Vector2){ a->pos.x + cosf(ang) * d, a->pos.y + sinf(ang) * d }, 3, Fade(MAGENTA, 0.7f));
                    }
                }
            } break;

            case ARM_TORRE: {
                Vector2 d = Vector2Subtract(b->j.pos, a->pos);
                d = (Vector2Length(d) > 1.0f) ? Vector2Normalize(d) : (Vector2){ 0, 1 };
                DrawRectangle((int)a->pos.x - 14, (int)a->pos.y - 14, 28, 28, Fade((Color){ 50, 40, 70, 255 }, apaga));
                DrawRectangleLines((int)a->pos.x - 14, (int)a->pos.y - 14, 28, 28, Fade(ORANGE, 0.7f * apaga));
                DrawLineEx(a->pos, Vector2Add(a->pos, Vector2Scale(d, 18)), 5, Fade(GRAY, apaga));
                if (!desligadas && a->fase == 1) {
                    float k = 1.0f - a->timer / TORRE_PREPARO;
                    DrawCircleV(a->pos, 6 + 6 * k, Fade(ORANGE, 0.6f));                          // aviso: carregando
                    DrawLineEx(a->pos, Vector2Add(a->pos, Vector2Scale(d, 120)), 1, Fade(RED, 0.5f * k));
                } else {
                    DrawCircleV(a->pos, 5, Fade(ORANGE, 0.5f * apaga));
                }
            } break;

            case ARM_EXPLOSAO:
                if (!desligadas && a->fase == 1) {
                    float k = 1.0f - a->timer / EXPLOSAO_AVISO;
                    DrawCircleV(a->pos, a->raio, Fade(RED, 0.10f));
                    DrawCircleSector(a->pos, a->raio * k, 0, 360, 24, Fade(RED, 0.18f));
                    DrawCircleLines((int)a->pos.x, (int)a->pos.y, a->raio, pisca ? RED : ORANGE);
                    DrawText("!", (int)a->pos.x - 3, (int)a->pos.y - 10, 20, YELLOW);
                }
                break;

            case ARM_BARREIRA: {
                Color cor = (Color){ 0, 228, 48, 255 };
                DrawRectangleRec(a->r, Fade((Color){ 10, 50, 20, 255 }, 0.9f));
                for (int k = 0; k < 6; k++) {
                    float f = fmodf(tempo * 1.5f + (float)k / 6.0f, 1.0f);
                    if (a->vertical) DrawLine((int)a->r.x, (int)(a->r.y + f * a->r.height), (int)(a->r.x + a->r.width), (int)(a->r.y + f * a->r.height), Fade(cor, 0.5f));
                    else             DrawLine((int)(a->r.x + f * a->r.width), (int)a->r.y, (int)(a->r.x + f * a->r.width), (int)(a->r.y + a->r.height), Fade(cor, 0.5f));
                }
                DrawRectangleLinesEx(a->r, 2, cor);
                // Seta para onde ela está indo
                if (!desligadas) {
                    Vector2 c = CentroRetangulo(a->r);
                    Vector2 dir = a->vertical ? (Vector2){ (a->vel > 0) ? 1.0f : -1.0f, 0 } : (Vector2){ 0, (a->vel > 0) ? 1.0f : -1.0f };
                    Vector2 ponta = Vector2Add(c, Vector2Scale(dir, 26));
                    DrawLineEx(c, ponta, 2, Fade(WHITE, 0.6f));
                    DrawCircleV(ponta, 3, Fade(WHITE, 0.6f));
                }
            } break;

            case ARM_BLOQUEIO:
                if (desligadas || a->fase == 0) {
                    DrawRectangleLinesEx(a->r, 1, Fade(RED, 0.2f * apaga));
                } else if (a->fase == 1) {
                    DrawRectangleRec(a->r, Fade(RED, pisca ? 0.22f : 0.08f));                    // aviso: vai fechar
                    DrawRectangleLinesEx(a->r, 2, RED);
                    DrawText("!", (int)(a->r.x + a->r.width / 2) - 3, (int)(a->r.y + a->r.height / 2) - 10, 20, YELLOW);
                } else {
                    DrawRectangleRec(a->r, (Color){ 70, 15, 30, 255 });
                    DrawRectangleLinesEx(a->r, 2, (Color){ 255, 60, 90, 255 });
                    DrawLineEx((Vector2){ a->r.x, a->r.y }, (Vector2){ a->r.x + a->r.width, a->r.y + a->r.height }, 2, Fade(RED, 0.6f));
                    DrawLineEx((Vector2){ a->r.x + a->r.width, a->r.y }, (Vector2){ a->r.x, a->r.y + a->r.height }, 2, Fade(RED, 0.6f));
                }
                break;

            default:
                break;
        }
    }
}

// Sigilo de Luz (CIRCUITO): trilha que se fecha em volta do alvo, chip quadrado girando, nós piscando nos 4 eixos,
// coluna de luz no impacto e, depois, uma placa de circuito que apaga aos poucos
static void DesenharNosCircuito(Vector2 p, float rInterno, float rExterno, float t, Color c, float alfa)
{
    for (int i = 0; i < 4; i++) {
        float ang = (float)i * 1.5708f;
        Vector2 d = { cosf(ang), sinf(ang) };
        Vector2 ini = { p.x + d.x * rInterno, p.y + d.y * rInterno };
        Vector2 fim = { p.x + d.x * rExterno, p.y + d.y * rExterno };
        DrawLineEx(ini, fim, 2, Fade(c, alfa * 0.8f));
        bool aceso = (((int)(t * 8.0f) + i) % 3) != 0;                      // os nós piscam em sequência
        DrawRectangle((int)fim.x - 3, (int)fim.y - 3, 6, 6, aceso ? Fade(WHITE, alfa) : Fade(c, alfa * 0.6f));
        // pequena derivação lateral da trilha (parece uma trilha de placa)
        Vector2 meio = { p.x + d.x * (rInterno + rExterno) * 0.5f, p.y + d.y * (rInterno + rExterno) * 0.5f };
        Vector2 lado = { -d.y * 5.0f, d.x * 5.0f };
        DrawLineEx(meio, Vector2Add(meio, lado), 1, Fade(c, alfa * 0.6f));
    }
}

static void DesenharSigilo(const AreaDano *a)
{
    float t = (float)GetTime();
    Vector2 p = a->pos;
    Color c = a->cor;

    if (a->atraso > 0.0f) {
        float k = 1.0f - a->atraso / SIGILO_ATRASO;           // 0 -> 1
        float r = a->raio * (0.55f + 0.45f * k);
        float alfa = 0.5f + 0.4f * k;
        DrawRing(p, r - 2.0f, r, -90.0f, -90.0f + 360.0f * k, 48, Fade(c, alfa + 0.1f));   // a trilha externa se fecha
        DrawCircleLines((int)p.x, (int)p.y, r * 0.62f, Fade(c, 0.4f));
        DesenharNosCircuito(p, r * 0.62f, r, t, c, alfa);
        DrawPolyLinesEx(p, 4, r * 0.34f, 45.0f + t * 120.0f, 2, Fade(c, alfa));            // chip girando...
        DrawPolyLinesEx(p, 4, r * 0.20f, 45.0f - t * 180.0f, 2, Fade(WHITE, alfa * 0.8f)); // ...e o núcleo contra-girando
        DrawCircleLines((int)p.x, (int)p.y, a->raio * (1.6f - 0.6f * k), Fade(WHITE, 0.6f * (1.0f - k)));
        DrawPoly(p, 4, a->raio * 0.12f * k, 45.0f, Fade(c, 0.7f));
        return;
    }

    float depois = a->vidaMax - a->vida;                       // tempo desde o impacto
    if (depois < 0.35f) {
        float k = 1.0f - depois / 0.35f;
        BeginBlendMode(BLEND_ADDITIVE);
        float larg = a->raio * (0.5f + 0.5f * k);
        DrawRectangleGradientV((int)(p.x - larg), (int)(p.y - 420), (int)(larg * 2.0f), 420, Fade(c, 0.0f), Fade(c, 0.55f * k));
        DrawRectangle((int)(p.x - larg * 0.35f), (int)(p.y - 420), (int)(larg * 0.7f), 420, Fade(WHITE, 0.5f * k));
        DrawCircleV(p, a->raio * (1.1f - 0.3f * k), Fade(c, 0.4f * k));
        EndBlendMode();
    }
    float f = a->vida / a->vidaMax;                            // brilho residual que apaga
    DrawCircleV(p, a->raio, Fade(c, 0.14f * f));
    DrawCircleLines((int)p.x, (int)p.y, a->raio, Fade(c, 0.6f * f));
    DrawCircleLines((int)p.x, (int)p.y, a->raio * 0.62f, Fade(c, 0.35f * f));
    DesenharNosCircuito(p, a->raio * 0.62f, a->raio * 0.9f, t, c, 0.7f * f);
    DrawPolyLinesEx(p, 4, a->raio * 0.34f, 45.0f + t * 40.0f, 2, Fade(c, 0.6f * f));
}

void DesenharAreas(const Biblioteca *b)
{
    for (int i = 0; i < MAX_AREAS; i++) {
        const AreaDano *a = &b->areas[i];
        if (!a->ativo) continue;

        if (a->estilo == 1) { DesenharSigilo(a); continue; }
        if (a->atraso > 0.0f) {
            DrawCircleLines((int)a->pos.x, (int)a->pos.y, a->raio, Fade(a->cor, 0.7f));
            DrawCircleV(a->pos, a->raio * 0.3f, Fade(a->cor, 0.4f));
        } else if (a->dps > 0.0f) {
            float k = a->vida / a->vidaMax;
            DrawCircleV(a->pos, a->raio, Fade(a->cor, 0.22f * k));
            DrawCircleLines((int)a->pos.x, (int)a->pos.y, a->raio, Fade(a->cor, 0.6f * k));
        }
    }
}

// Tornado de Dados: um FUNIL de 0s e 1s — fino embaixo (tocando o chão) e largo em cima,
// girando, balançando no alto e levantando poeira de dados na base.
// Os bits de trás são desenhados antes e mais apagados; os da frente, depois e mais fortes (dá volume).
static void DesenharTornadoBits(const Biblioteca *b, float t, float fade, float cresce, bool frente)
{
    const int CAMADAS = 9;
    const float ALTURA = 96.0f;
    for (int c = 0; c < CAMADAS; c++) {
        float k = (float)c / (float)(CAMADAS - 1);                    // 0 = chão, 1 = topo
        float raio = (5.0f + 42.0f * powf(k, 1.35f)) * cresce;
        float balanco = sinf(t * 2.3f + k * 2.6f) * 9.0f * k;          // o alto balança, a base fica presa
        float cy = b->torPos.y - ALTURA * k * cresce;
        float cx = b->torPos.x + balanco;
        int n = 3 + (int)(9.0f * k);
        float giro = t * (10.0f - 5.0f * k);                          // embaixo gira mais rápido
        for (int i = 0; i < n; i++) {
            float ang = giro + (float)i * (2.0f * PI / (float)n) + k * 1.7f;
            float sn = sinf(ang);
            if ((sn >= 0.0f) != frente) continue;
            Vector2 p = { cx + cosf(ang) * raio, cy + sn * raio * 0.32f };
            float prof = 0.55f + 0.45f * sn;                          // 0,1 atrás .. 1 na frente
            int tam = (frente ? 11 : 9) + (int)(2.0f * k);
            const char *bit = (((i * 7 + c * 3 + (int)(t * 10.0f)) & 1) == 0) ? "0" : "1";
            Color cor = frente ? (Color){ 210, 250, 255, 255 } : (Color){ 90, 170, 230, 255 };
            DrawText(bit, (int)p.x - tam / 4, (int)p.y - tam / 2, tam, Fade(cor, (0.35f + 0.6f * prof) * fade));
        }
    }
}

void DesenharTornado(const Biblioteca *b)
{
    if (!b->torAtivo) return;
    float t = (float)GetTime();
    float fade = Clamp(b->torVida / 0.6f, 0.0f, 1.0f);
    float cresce = Clamp((3.2f - b->torVida) / 0.25f, 0.2f, 1.0f);    // nasce crescendo do chão
    Color c = (Color){ 150, 230, 255, 255 };
    const float ALTURA = 96.0f;
    Vector2 base = b->torPos;

    // sombra e área de dano no chão
    DrawEllipse((int)base.x, (int)base.y + 4, 46, 13, Fade(BLACK, 0.30f * fade));
    DrawEllipseLines((int)base.x, (int)base.y + 4, 60, 17, Fade(c, 0.30f * fade));

    DesenharTornadoBits(b, t, fade, cresce, false);                   // parte de trás

    // corpo do funil: faixas translúcidas de baixo para cima
    for (int f = 0; f < 12; f++) {
        float k0 = (float)f / 12.0f, k1 = (float)(f + 1) / 12.0f;
        float r0 = (5.0f + 42.0f * powf(k0, 1.35f)) * cresce, r1 = (5.0f + 42.0f * powf(k1, 1.35f)) * cresce;
        float x0 = base.x + sinf(t * 2.3f + k0 * 2.6f) * 9.0f * k0, x1 = base.x + sinf(t * 2.3f + k1 * 2.6f) * 9.0f * k1;
        float y0 = base.y - ALTURA * k0 * cresce, y1 = base.y - ALTURA * k1 * cresce;
        Color corpo = Fade(c, (0.10f + 0.06f * sinf(t * 7.0f - k0 * 9.0f)) * fade);
        DrawTriangle((Vector2){ x1 - r1, y1 }, (Vector2){ x0 - r0, y0 }, (Vector2){ x0 + r0, y0 }, corpo);
        DrawTriangle((Vector2){ x1 - r1, y1 }, (Vector2){ x0 + r0, y0 }, (Vector2){ x1 + r1, y1 }, corpo);
        // anéis de vento (riscos girando)
        if (f % 3 == 1) {
            float a0 = fmodf(t * 360.0f * (1.6f - k0), 360.0f);
            DrawEllipseLines((int)x0, (int)y0, r0, r0 * 0.32f, Fade((Color){ 210, 250, 255, 255 }, 0.18f * fade));
            DrawCircleV((Vector2){ x0 + cosf(a0 * DEG2RAD) * r0, y0 + sinf(a0 * DEG2RAD) * r0 * 0.32f }, 2.0f, Fade(WHITE, 0.7f * fade));
        }
    }

    // poeira de dados subindo em espiral perto da base
    for (int i = 0; i < 10; i++) {
        float fase = fmodf(t * 0.9f + (float)i * 0.1f, 1.0f);           // 0 = chão, 1 = some no alto
        float ang = t * 6.0f + (float)i * 2.4f;
        float r = (14.0f + 30.0f * fase) * cresce;
        Vector2 p = { base.x + cosf(ang) * r, base.y - 40.0f * fase * cresce + sinf(ang) * r * 0.3f };
        DrawRectangleV((Vector2){ p.x - 1.5f, p.y - 1.5f }, (Vector2){ 3, 3 }, Fade(c, (1.0f - fase) * 0.8f * fade));
    }

    DesenharTornadoBits(b, t, fade, cresce, true);                    // parte da frente
}

// Surto da Katana: relâmpago em zigue-zague com brilho (camadas grossas e fracas + núcleo branco),
// galhos curtos e clarão nas pontas. O traçado muda ~25x por segundo (pisca como raio de verdade).
static unsigned int RaioHash(unsigned int x)
{
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return x;
}
static float RaioRnd(unsigned int *s) { *s = RaioHash(*s + 0x9E3779B9u); return (float)(*s & 0xFFFF) / 65535.0f * 2.0f - 1.0f; }

static void DesenharRelampago(Vector2 de, Vector2 para, unsigned int semente, float alfa, float largura, bool galhos)
{
    Vector2 d = Vector2Subtract(para, de);
    float len = Vector2Length(d);
    if (len < 2.0f) return;
    Vector2 n = { -d.y / len, d.x / len };
    int segs = 3 + (int)(len / 16.0f);
    if (segs > 24) segs = 24;
    Vector2 pts[25];
    unsigned int s = semente;
    for (int i = 0; i <= segs; i++) {
        float k = (float)i / (float)segs;
        float desvio = (i == 0 || i == segs) ? 0.0f : RaioRnd(&s) * fminf(len * 0.12f, 16.0f) * sinf(PI * k);
        pts[i] = Vector2Add(Vector2Add(de, Vector2Scale(d, k)), Vector2Scale(n, desvio));
    }
    Color brilho = (Color){ 255, 220, 90, 255 }, meio = (Color){ 255, 245, 170, 255 };
    for (int i = 0; i < segs; i++) DrawLineEx(pts[i], pts[i + 1], largura * 4.5f, Fade(brilho, 0.16f * alfa));
    for (int i = 0; i < segs; i++) DrawLineEx(pts[i], pts[i + 1], largura * 2.2f, Fade(meio, 0.45f * alfa));
    for (int i = 0; i < segs; i++) DrawLineEx(pts[i], pts[i + 1], largura, Fade(WHITE, alfa));
    if (galhos) {
        for (int g = 0; g < 2; g++) {
            int i = 1 + (int)((RaioRnd(&s) * 0.5f + 0.5f) * (float)(segs - 2));
            Vector2 dir = Vector2Rotate(Vector2Normalize(d), RaioRnd(&s) * 0.9f);
            Vector2 fim = Vector2Add(pts[i], Vector2Scale(dir, len * (0.18f + 0.12f * (RaioRnd(&s) * 0.5f + 0.5f))));
            DesenharRelampago(pts[i], fim, s + 77u * (unsigned int)g, alfa * 0.7f, largura * 0.55f, false);
        }
    }
}

void DesenharRaiosKatana(const Biblioteca *b)
{
    float t = (float)GetTime();
    for (int r = 0; r < 3; r++) {
        if (b->raioK[r].vida <= 0.0f) continue;
        float alfa = Clamp(b->raioK[r].vida / 0.30f, 0.0f, 1.0f);
        unsigned int sem = b->raioK[r].semente + (unsigned int)(t * 25.0f) * 131u;     // pisca/muda de forma
        DesenharRelampago(b->raioK[r].de, b->raioK[r].para, sem, alfa, 2.5f, true);
        // clarão no alvo e na origem
        DrawCircleV(b->raioK[r].para, 16.0f * alfa + 4.0f, Fade((Color){ 255, 230, 120, 255 }, 0.35f * alfa));
        DrawCircleV(b->raioK[r].para, 6.0f, Fade(WHITE, 0.9f * alfa));
        DrawCircleV(b->raioK[r].de, 5.0f, Fade(WHITE, 0.6f * alfa));
    }
}


// Objetos da sala: recompensa, altar/poltrona/console, consoles do mecanismo e arquivos
void DesenharObjetosSala(const Biblioteca *b, float tempo)
{
    // Expansão educativa: o console do laboratório
    if (b->tipoSala == SALA_LABORATORIO)
        DesenharConsoleLab(b, tempo);

    // Consoles do mecanismo corrompido
    for (int k = 0; k < b->numMecanismos; k++) {
        Vector2 c = b->posMecanismo[k];
        bool feito = b->mecanismoDesligado[k];
        Color cor = feito ? LIME : MAGENTA;
        DrawRectangle((int)c.x - 16, (int)c.y - 14, 32, 26, (Color){ 30, 22, 46, 255 });
        DrawRectangleLines((int)c.x - 16, (int)c.y - 14, 32, 26, cor);
        DrawRectangle((int)c.x - 11, (int)c.y - 9, 22, 12, Fade(cor, feito ? 0.6f : 0.3f + 0.3f * sinf(tempo * 6.0f + k)));
        if (!feito) {
            DrawCircleLines((int)c.x, (int)c.y, MECANISMO_RAIO, Fade(cor, 0.25f));
            if (b->progressoMecanismo[k] > 0.0f)
                DrawRing(c, MECANISMO_RAIO - 4, MECANISMO_RAIO, -90, -90 + 360.0f * b->progressoMecanismo[k] / MECANISMO_TEMPO, 32, (Color){ 0, 228, 255, 255 });
        }
    }

    // Altar do evento / poltrona / console do desafio
    if (b->tipoSala == SALA_EVENTO || b->tipoSala == SALA_DESCANSO || b->tipoSala == SALA_DESAFIO) {
        Vector2 c = b->posObjetoSala;
        float usado = b->eventoUsado ? 0.4f : 1.0f;
        float flutua = sinf(tempo * 2.0f) * 3.0f;
        Color cor = CORES_SALAS_ROGUE[b->tipoSala];

        DrawEllipse((int)c.x, (int)c.y + 22, 34, 9, Fade(cor, 0.15f * usado));
        if (b->tipoSala == SALA_EVENTO) {
            DrawRectangle((int)c.x - 18, (int)c.y + 4, 36, 16, Fade((Color){ 60, 40, 90, 255 }, usado));
            DrawRectangle((int)c.x - 12, (int)c.y - 4, 24, 8, Fade((Color){ 80, 60, 120, 255 }, usado));
            if (!b->eventoUsado) {
                DrawCircleV((Vector2){ c.x, c.y - 22 + flutua }, 14, Fade(cor, 0.25f));
                DesenharIconeSala(SALA_EVENTO, (Vector2){ c.x, c.y - 22 + flutua }, 1.0f, tempo);
            }
        } else if (b->tipoSala == SALA_DESCANSO) {
            DrawRectangle((int)c.x - 22, (int)c.y - 16, 44, 18, Fade((Color){ 40, 110, 60, 255 }, usado));
            DrawRectangle((int)c.x - 28, (int)c.y + 2, 56, 14, Fade((Color){ 30, 90, 50, 255 }, usado));
            DrawRectangle((int)c.x - 28, (int)c.y - 6, 8, 20, Fade((Color){ 30, 80, 45, 255 }, usado));
            DrawRectangle((int)c.x + 20, (int)c.y - 6, 8, 20, Fade((Color){ 30, 80, 45, 255 }, usado));
            DrawCircleV((Vector2){ c.x + 44, c.y - 30 }, 18, Fade(YELLOW, 0.08f * usado));           // abajur
            DrawLine((int)c.x + 44, (int)c.y - 30, (int)c.x + 44, (int)c.y + 14, Fade(GRAY, usado));
            DrawTriangle((Vector2){ c.x + 36, c.y - 28 }, (Vector2){ c.x + 52, c.y - 28 }, (Vector2){ c.x + 44, c.y - 38 }, Fade(GOLD, usado));
        } else {
            DrawRectangle((int)c.x - 20, (int)c.y - 18, 40, 32, Fade((Color){ 40, 40, 20, 255 }, usado));
            DrawRectangleLines((int)c.x - 20, (int)c.y - 18, 40, 32, Fade(cor, usado));
            DesenharIconeSala(SALA_DESAFIO, (Vector2){ c.x, c.y - 2 }, 0.9f, tempo);
            if (b->estadoDesafio == 1) {
                float total = (b->desafioSala == DESAFIO_SALA_ECOS) ? DESAFIO_ECOS_TEMPO : DESAFIO_INTEG_TEMPO;
                DrawRing(c, 26, 30, -90, -90 + 360.0f * fmaxf(0.0f, b->tempoDesafioSala) / total, 32, cor);
            }
        }
    }

    // Arquivo de ameaça ou memória
    if (b->registroSala) {
        DesenharTerminal(b->posRegistroSala, b->registroSalaLido, tempo);
        const char *nome = (b->registroSalaId >= 100) ? "MEMÓRIA" : "ARQUIVO DE AMEAÇA";
        DrawText(nome, (int)b->posRegistroSala.x - MeasureText(nome, 10) / 2, (int)b->posRegistroSala.y + 26, 10,
                 b->registroSalaLido ? GRAY : ((b->registroSalaId >= 100) ? MAGENTA : (Color){ 0, 228, 255, 255 }));
    }

    // Recompensa no chão
    if (b->recompensaAtiva) {
        Vector2 c = { b->recompensaPos.x, b->recompensaPos.y + sinf(tempo * 3.0f) * 3.0f };
        Color cor = (b->recompensaTipo == RECOMPENSA_RARA) ? GOLD : ((b->recompensaTipo == RECOMPENSA_ARMA) ? ORANGE :
                    ((b->recompensaTipo == RECOMPENSA_VIDA) ? LIME : (Color){ 0, 228, 255, 255 }));
        DrawCircleV(c, 24, Fade(cor, 0.12f + 0.06f * sinf(tempo * 4.0f)));
        DrawRing(c, 17, 20, tempo * 120.0f, tempo * 120.0f + 270.0f, 24, cor);
        DrawCircleV(c, 13, Fade(BLACK, 0.7f));
        DesenharIconeRecompensa(b->recompensaTipo, c, 1.5f, tempo);
        const char *nome = (b->recompensaTipo == RECOMPENSA_MELHORIA) ? "NÚCLEO DE MELHORIA" :
                           (b->recompensaTipo == RECOMPENSA_RARA)     ? "NÚCLEO RARO" : NOMES_RECOMPENSA[b->recompensaTipo];
        DrawText(nome, (int)c.x - MeasureText(nome, 10) / 2, (int)c.y + 26, 10, cor);
    }
}

// Mini-mapa no canto de cima: as camadas da esquerda para a direita
static Vector2 RoguePosMiniMapa(const Biblioteca *b, const NoMapa *n)
{
    float y = 7.0f + n->x * 28.0f;
    if (n->tipo == SALA_SECRETA && n->pai >= 0) y = 7.0f + b->mapa.no[n->pai].x * 28.0f + 6.0f;
    return (Vector2){ 12.0f + (float)n->prof * 8.2f + ((n->tipo == SALA_SECRETA) ? 4.0f : 0.0f), y };
}

void DesenharMiniMapa(const Biblioteca *b, float tempo)
{
    const MapaRogue *m = &b->mapa;
    const NoMapa *atual = &m->no[b->noAtual];

    DrawRectangle(4, 2, 138, 38, Fade(BLACK, 0.6f));
    DrawRectangleLines(4, 2, 138, 38, Fade(PURPLE, 0.6f));

    for (int i = 0; i < m->numNos; i++) {
        const NoMapa *n = &m->no[i];
        if (n->escondida) continue;
        Vector2 a = RoguePosMiniMapa(b, n);
        for (int k = 0; k < n->numSaidas; k++) {
            const NoMapa *d = &m->no[n->saida[k]];
            bool caminho = n->visitado && d->visitado;
            DrawLineEx(a, RoguePosMiniMapa(b, d), 1, Fade(caminho ? WHITE : GRAY, caminho ? 0.7f : 0.25f));
        }
    }

    for (int i = 0; i < m->numNos; i++) {
        const NoMapa *n = &m->no[i];
        if (n->escondida) continue;

        Vector2 p = RoguePosMiniMapa(b, n);
        Color cor = CORES_SALAS_ROGUE[n->tipo];
        float lado = (n->tipo == SALA_CHEFE) ? 8.0f : ((n->tipo == SALA_SECRETA) ? 4.0f : ((n->tipo == SALA_ESPECIAL) ? 7.0f : 5.0f));
        Rectangle r = { p.x - lado / 2, p.y - lado / 2, lado, lado };

        bool proxima = false;
        for (int k = 0; k < atual->numSaidas; k++) if (atual->saida[k] == i) proxima = true;

        if (n->visitado) DrawRectangleRec(r, Fade(cor, (i == b->noAtual) ? 1.0f : 0.45f));
        DrawRectangleLinesEx(r, 1, proxima ? Fade(cor, 0.6f + 0.4f * sinf(tempo * 6.0f)) : Fade(cor, n->visitado ? 0.9f : 0.45f));

        if (i == b->noAtual)
            DrawRectangleLinesEx((Rectangle){ r.x - 2, r.y - 2, r.width + 4, r.height + 4 }, 1, Fade(WHITE, 0.6f + 0.4f * sinf(tempo * 5.0f)));
    }
}

// Mapa grande (M): o caminho todo, as recompensas das próximas portas e a build
void DesenharMapaGrande(const Biblioteca *b, float tempo)
{
    const MapaRogue *m = &b->mapa;
    const NoMapa *atual = &m->no[b->noAtual];

    DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, 0.75f));
    Rectangle painel = { 30, 30, 740, 520 };
    DrawRectangleRec(painel, (Color){ 16, 13, 28, 255 });
    DrawRectangleLinesEx(painel, 3, PURPLE);
    DrawText("MAPA DA BIBLIOTECA", (int)painel.x + 20, (int)painel.y + 14, 22, (Color){ 200, 122, 255, 255 });
    DrawText(TextFormat("Tentativa %d  -  sala %d", b->perm->tentativas, b->indiceRota + 1), (int)painel.x + 20, (int)painel.y + 40, 12, GRAY);

    // Grafo: de baixo (entrada) para cima (Alucinador)
    Rectangle area = { painel.x + 20, painel.y + 62, 430, 420 };
    DrawRectangleRec(area, Fade((Color){ 30, 24, 50, 255 }, 0.6f));

    #define POS_NO(n) ((Vector2){ area.x + 40.0f + (n)->x * (area.width - 80.0f) + (((n)->tipo == SALA_SECRETA) ? 34.0f : 0.0f), \
                                  area.y + area.height - 20.0f - (float)(n)->prof * ((area.height - 40.0f) / (float)PROF_CHEFE) - (((n)->tipo == SALA_SECRETA) ? 11.0f : 0.0f) })

    for (int i = 0; i < m->numNos; i++) {
        const NoMapa *n = &m->no[i];
        if (n->escondida) continue;
        Vector2 a = POS_NO(n);
        for (int k = 0; k < n->numSaidas; k++) {
            const NoMapa *d = &m->no[n->saida[k]];
            if (d->escondida) continue;
            bool caminho = n->visitado && d->visitado;
            bool opcao = (i == b->noAtual);
            DrawLineEx(a, POS_NO(d), caminho ? 3.0f : 2.0f, opcao ? Fade(GOLD, 0.8f) : Fade(caminho ? WHITE : GRAY, caminho ? 0.8f : 0.3f));
        }
        if (n->tipo == SALA_SECRETA && n->pai >= 0) {
            Vector2 pai = POS_NO(&m->no[n->pai]);
            for (int s = 0; s < 6; s++) {
                Vector2 q0 = Vector2Lerp(pai, a, (float)s / 6.0f), q1 = Vector2Lerp(pai, a, ((float)s + 0.5f) / 6.0f);
                DrawLineEx(q0, q1, 2, Fade(CORES_SALAS_ROGUE[SALA_SECRETA], 0.8f));
            }
        }
    }

    for (int i = 0; i < m->numNos; i++) {
        const NoMapa *n = &m->no[i];
        if (n->escondida) continue;

        Vector2 c = POS_NO(n);
        Color cor = CORES_SALAS_ROGUE[n->tipo];
        float raio = (n->tipo == SALA_CHEFE) ? 14.0f : ((n->tipo == SALA_SILOGISTA || n->tipo == SALA_ESPECIAL) ? 12.0f : 10.0f);
        bool proxima = false;
        for (int k = 0; k < atual->numSaidas; k++) if (atual->saida[k] == i) proxima = true;

        DrawCircleV(c, raio, n->visitado ? Fade(cor, 0.35f) : (Color){ 20, 16, 34, 255 });
        DrawCircleLines((int)c.x, (int)c.y, raio, proxima ? GOLD : Fade(cor, n->visitado ? 1.0f : 0.6f));
        DesenharIconeSala(n->tipo, c, (n->tipo == SALA_CHEFE) ? 0.8f : 0.55f, tempo);

        if (i == b->noAtual)
            DrawCircleLines((int)c.x, (int)c.y, raio + 5 + 2 * sinf(tempo * 5.0f), WHITE);

        // Recompensa: só das próximas salas (dá para planejar, mas não tudo)
        if (proxima && n->recompensa != RECOMPENSA_NENHUMA) {
            Vector2 rc = { c.x + raio, c.y - raio + 2 };
            DrawCircleV(rc, 8, Fade(BLACK, 0.85f));
            DesenharIconeRecompensa(n->recompensa, rc, 0.8f, tempo);
        }
        if (proxima || n->tipo == SALA_CHEFE || n->tipo == SALA_SILOGISTA || n->tipo == SALA_ESPECIAL) {
            // Ao lado (as camadas ficaram próximas), sem passar da próxima sala da mesma linha nem da borda do mapa:
            // com o texto grande, um nome que não cabe termina em "..."
            int xNome = (int)(c.x + raio + 4);
            int limite = (int)(area.x + area.width) - 4;
            for (int k = 0; k < m->numNos; k++) {
                Vector2 o = POS_NO(&m->no[k]);
                if (k != i && !m->no[k].escondida && fabsf(o.y - c.y) < 12.0f && o.x > c.x && (int)o.x - 16 < limite)
                    limite = (int)o.x - 16;
            }
            DrawText(TextoQueCabe(RoguePlacaNo(n), 10, limite - xNome), xNome, (int)c.y - LinhaTexto(6), 10, proxima ? GOLD : cor);
        }
    }
    #undef POS_NO

    // A build desta tentativa
    int x = (int)(area.x + area.width) + 18, y = (int)area.y;
    DrawText("SUA BUILD", x, y, 16, GOLD);
    y += 22;
    DesenharIconeArma(RogueArmaAtual(&b->j), (Vector2){ (float)x + 8, (float)y + 6 }, 0.7f, tempo);
    DrawText((b->j.armaExtra != ARMA_DISPARADOR) ? TextFormat("%s + %s", DEF_ARMAS[b->j.armaBase].nome, DEF_ARMAS[b->j.armaExtra].nome) : DEF_ARMAS[b->j.armaBase].nome, x + 20, y + 1, 10, WHITE);
    y += 18;

    // Legenda (embaixo, à direita): duas colunas, ou uma só se os nomes não couberem (texto grande)
    static const TipoSalaRogue legenda[] = { SALA_COMBATE, SALA_ELITE, SALA_EDUCATIVA, SALA_EVENTO, SALA_DESCANSO, SALA_DESAFIO, SALA_RECOMPENSA, SALA_SECRETA,
                                             SALA_LABORATORIO, SALA_ESPECIAL };
    int maiorLegenda = 0;
    for (int i = 0; i < 10; i++) {
        int w = MeasureText(PLACAS_SALAS_ROGUE[legenda[i]], 10);
        if (w > maiorLegenda) maiorLegenda = w;
    }
    int colunasLegenda = (x + 130 + 18 + maiorLegenda <= (int)(painel.x + painel.width) - 10) ? 2 : 1;
    int linhaLegenda = EspacoLinhaTexto(18, 10);
    int ly = (int)(painel.y + painel.height) - LinhaTexto(24) - ((10 + colunasLegenda - 1) / colunasLegenda) * linhaLegenda;

    int linhas = 0;
    for (int u = 0; u < NUM_UPGRADES && y < ly - 8; u++) {
        if (b->j.up[u] <= 0) continue;
        Color c = CORES_RARIDADE[UPGRADES[u].raridade];
        DrawRectangle(x, y + 2, 6, 6, CORES_CATEGORIA_UP[UPGRADES[u].categoria]);
        DrawText(TextFormat("%s %s", UPGRADES[u].nome, (UPGRADES[u].maxNivel > 1) ? TextFormat("%d/%d", b->j.up[u], UPGRADES[u].maxNivel) : ""), x + 10, y, 10, c);
        y += 13;
        linhas++;
    }
    for (int i = PRIMEIRO_UPGRADE; i < NUM_ITENS && y < ly - 8; i++) {
        if (b->j.itens[i] <= 0) continue;
        DrawText(TextFormat("%s x%d", ITENS[i].nome, RogueQtd(&b->j, (TipoItem)i)), x + 10, y, 10, LIGHTGRAY);
        y += 13;
        linhas++;
    }
    if (linhas == 0) { DrawText("Nenhuma melhoria ainda.", x, y, 10, GRAY); y += 13; }

    for (int p = 0; p < 3; p++)
        if (b->j.penalidade & (1 << p)) { DrawText(TextFormat("! %s", NOMES_PENALIDADE[p]), x, y, 10, RED); y += 13; }

    for (int i = 0; i < 10; i++) {
        int lx = x + ((colunasLegenda == 2) ? (i % 2) * 130 : 0), yy = ly + ((colunasLegenda == 2) ? i / 2 : i) * linhaLegenda;
        DesenharIconeSala(legenda[i], (Vector2){ (float)lx + 7, (float)yy + 6 }, 0.5f, tempo);
        DrawText(PLACAS_SALAS_ROGUE[legenda[i]], lx + 18, yy + 1, 10, CORES_SALAS_ROGUE[legenda[i]]);
    }

    const char *ajuda = "Portas douradas: as próximas salas.   M, ENTER ou CLIQUE - fechar";
    DrawText(ajuda, LARGURA_TELA / 2 - MeasureText(ajuda, 10) / 2, (int)(painel.y + painel.height) - LinhaTexto(16), 10, GRAY);

    DesenharBotaoFechar((Rectangle){ painel.x + painel.width - 36, painel.y + 10, 26, 22 }, PURPLE);
}

// Ícone da categoria da melhoria (cartas e HUD)
void DesenharIconeUpgrade(const Biblioteca *b, OpcaoEscolha o, Vector2 c, float e, float tempo)
{
    if (o.tipo == OPC_ITEM)       { DesenharIconeItem((TipoItem)o.id, c, e, tempo); return; }
    if (o.tipo == OPC_ARMA)       { DesenharIconeArma((TipoArma)o.id, c, e * 1.2f, tempo); return; }
    if (o.tipo == OPC_FRAGMENTOS) { DesenharFragmentoConhecimento(c, tempo); return; }

    const DefUpgrade *d = &UPGRADES[o.id];
    Color cor = CORES_CATEGORIA_UP[d->categoria];
    (void)b;

    switch (d->categoria) {
        case CAT_ATRIBUTO:
            DrawTriangle((Vector2){ c.x, c.y - 12 * e }, (Vector2){ c.x - 9 * e, c.y }, (Vector2){ c.x + 9 * e, c.y }, cor);
            DrawRectangleV((Vector2){ c.x - 4 * e, c.y }, (Vector2){ 8 * e, 10 * e }, cor);
            break;
        case CAT_DISPARO:
            for (int k = 0; k < 3; k++)
                DrawCircleV((Vector2){ c.x - 8 * e + k * 8 * e, c.y + ((k == 1) ? -4.0f : 4.0f) * e }, 4 * e, cor);
            DrawLineEx((Vector2){ c.x - 12 * e, c.y + 10 * e }, (Vector2){ c.x + 12 * e, c.y - 10 * e }, 2, Fade(WHITE, 0.6f));
            break;
        case CAT_HABILIDADE:      // raio
            DrawTriangle((Vector2){ c.x + 2 * e, c.y - 13 * e }, (Vector2){ c.x - 8 * e, c.y + 2 * e }, (Vector2){ c.x + 1 * e, c.y + 2 * e }, cor);
            DrawTriangle((Vector2){ c.x - 1 * e, c.y - 2 * e }, (Vector2){ c.x - 2 * e, c.y + 13 * e }, (Vector2){ c.x + 8 * e, c.y - 2 * e }, cor);
            break;
        case CAT_ESPECIAL:
            DrawPoly(c, 5, 12 * e, tempo * 40.0f, cor);
            DrawPoly(c, 5, 6 * e, -tempo * 40.0f, WHITE);
            break;
        case CAT_APRIMORAMENTO:
            DesenharIconeArma((d->arma >= 0) ? (TipoArma)d->arma : ARMA_DISPARADOR, c, e * 1.2f, tempo);
            DrawCircleLines((int)c.x, (int)c.y, 14 * e, cor);
            break;
        default:                  // sinergia: dois anéis entrelaçados
            DrawRing((Vector2){ c.x - 5 * e, c.y }, 6 * e, 9 * e, 0, 360, 24, cor);
            DrawRing((Vector2){ c.x + 5 * e, c.y }, 6 * e, 9 * e, 0, 360, 24, WHITE);
            break;
    }
}

// A tela "escolha 1 de 3": cartas com nome, raridade, categoria, nível, efeito e pré-requisitos
// Números para comparar armas na tela: dano por segundo (aproximado), cadência e alcance (0..1)
float DpsArma(TipoArma arma)
{
    const DefArma *a = &DEF_ARMAS[arma];
    float n = (a->projeteis > 1) ? (float)a->projeteis : 1.0f;
    switch (arma) {
        case ARMA_VETOR:     return (a->dano + (VETOR_DANO_MAX - a->dano) * 0.8f) / (a->intervalo + VETOR_CARGA_MAX * 0.8f);   // carrega 80%
        case ARMA_ROTEADOR:  return a->dano * 1.2f / a->intervalo;
        case ARMA_ORBE:      return (a->dano + ORBE_DANO_EXPLOSAO * 1.6f) / a->intervalo;     // tiro + implosão
        case ARMA_PULSO_FW:  return a->dano * 1.7f / a->intervalo;                           // pulso + eco
        case ARMA_RAIO: {                    // média de 20 s: o EP (reserva + regeneração + o que volta dos acertos) limita os sigilos
            float porSigilo = a->dano + RAIO_RESIDUO_DPS * RAIO_RESIDUO_TEMPO * 0.6f;     // impacto + parte da luz residual
            float liquido = RAIO_CUSTO_EP - RAIO_EP_RETORNO * 0.85f;                       // gasto médio por sigilo (85% de acertos)
            float sigilos = fminf(20.0f / a->intervalo, (ROGUE_EP_MAX + ROGUE_EP_POR_SEGUNDO * 20.0f) / liquido);
            return sigilos * porSigilo * 0.85f / 20.0f;
        }
        default:             return a->dano * n / a->intervalo;
    }
}

float AlcanceArma(TipoArma arma)
{
    int t = DEF_ARMAS[arma].alcanceTipo;
    return (t == 0) ? 1.0f : ((t == 1) ? 0.66f : 0.33f);
}

// Uma barra com nome e valor; "ref" (>= 0) mostra a diferença para a arma de comparação
void DesenharBarraArma(const char *nome, float x, float y, float w, float valor, float maximo, const char *texto, Color cor, float ref)
{
    // A barra começa depois do rótulo mais largo (CADÊNCIA), que cresce com o texto grande
    float rotulo = fmaxf(62.0f, (float)MeasureText("CADÊNCIA", 9) + 8.0f);
    float valor_ = fmaxf(52.0f, (float)MeasureText("1.8/s", 9) + 26.0f);
    DrawText(nome, (int)x, (int)y, 9, GRAY);
    Rectangle fundo = { x + rotulo, y + 1, w - rotulo - valor_, 8 };
    DrawRectangleRec(fundo, (Color){ 40, 36, 60, 255 });
    DrawRectangleRec((Rectangle){ fundo.x, fundo.y, fundo.width * Clamp(valor / maximo, 0.0f, 1.0f), fundo.height }, cor);
    DrawText(texto, (int)(fundo.x + fundo.width) + 6, (int)y, 9, WHITE);
    if (ref >= 0.0f) {
        float dif = valor - ref;
        const char *t = (fabsf(dif) < 0.5f) ? "=" : TextFormat("%+.0f", dif);
        DrawText(t, (int)(x + w) - MeasureText(t, 9), (int)y, 9, (dif > 0.5f) ? GREEN : ((dif < -0.5f) ? RED : GRAY));
    }
}

// As melhorias que você já tem para uma arma (somem do efeito se você a trocar)
void RogueMelhoriasDaArma(const JogadorRogue *j, TipoArma arma, char *saida, int tam)
{
    saida[0] = '\0';
    int n = 0;
    for (int k = 0; k < NUM_UPGRADES; k++) {
        if (UPGRADES[k].arma != (int)arma || arma == ARMA_DISPARADOR || j->up[k] <= 0) continue;
        int len = (int)strlen(saida);
        snprintf(saida + len, (size_t)(tam - len), "%s%s", (n > 0) ? ", " : "", UPGRADES[k].nome);
        n++;
    }
}

// Troca de arma: a arma nova em cima e as duas armas que você carrega para escolher qual sai
void DesenharTrocaArma(const Biblioteca *b, float tempo)
{
    const DefArma *nova = &DEF_ARMAS[b->armaNova];
    float dpsNova = DpsArma(b->armaNova);
    char linhas[256], texto[48];
    DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, 0.82f));

    const char *titulo = "QUAL ARMA VOCÊ QUER TROCAR?";
    DrawText(titulo, LARGURA_TELA / 2 - MeasureText(titulo, 24) / 2, 30, 24, GOLD);

    // ---- A arma nova (em cima)
    Rectangle pn = { 120, 70, 560, 130 };
    DrawRectangleRec(pn, (Color){ 20, 16, 34, 255 });
    DrawRectangleLinesEx(pn, 3, nova->cor);
    DrawRectangle((int)pn.x, (int)pn.y, (int)pn.width, 18, Fade(nova->cor, 0.25f));
    DrawText("ARMA NOVA", (int)pn.x + 8, (int)pn.y + 4, 10, nova->cor);
    DrawText(NOMES_RARIDADE[nova->raridade], (int)(pn.x + pn.width) - 8 - MeasureText(NOMES_RARIDADE[nova->raridade], 10), (int)pn.y + 4, 10, CORES_RARIDADE[nova->raridade]);
    DrawCircleV((Vector2){ pn.x + 50, pn.y + 78 }, 32, Fade(nova->cor, 0.15f));
    DesenharIconeArma(b->armaNova, (Vector2){ pn.x + 50, pn.y + 78 }, 2.0f, tempo);
    {   // o nome não invade a coluna dos números (texto grande): diminui um pouco e, se precisar, termina em "..."
        int tamNome = 18;
        while (tamNome > 14 && MeasureText(nova->nome, tamNome) > 240) tamNome--;
        DrawText(TextoQueCabe(nova->nome, tamNome, 240), (int)pn.x + 100, (int)pn.y + 28, tamNome, nova->cor);
    }
    QuebrarTexto(nova->efeito, 230, 11, linhas, (int)sizeof(linhas));
    DesenharTextoLinhas(linhas, (int)pn.x + 100, (int)pn.y + 54, 11, 14, WHITE);
    snprintf(texto, sizeof(texto), "%.0f", dpsNova);
    DesenharBarraArma("DANO/s",  pn.x + 350, pn.y + 40, 200, dpsNova, 110.0f, texto, nova->cor, -1.0f);
    snprintf(texto, sizeof(texto), "%.1f/s", 1.0f / nova->intervalo);
    DesenharBarraArma("CADÊNCIA", pn.x + 350, pn.y + 58, 200, 1.0f / nova->intervalo, 6.0f, texto, nova->cor, -1.0f);
    DesenharBarraArma("ALCANCE", pn.x + 350, pn.y + 76, 200, AlcanceArma(b->armaNova), 1.0f, NOMES_ALCANCE[nova->alcanceTipo] + 0, nova->cor, -1.0f);
    if (b->armaNova == ARMA_RAIO || b->armaNova == ARMA_RAIO_CLASSICO)
    {
        QuebrarTexto((b->armaNova == ARMA_RAIO) ? "Gasta EP a cada sigilo (sem EP, não atira). Acertar devolve EP." : "Gasta EP a cada disparo (sem EP, não atira).", 200, 9, linhas, (int)sizeof(linhas));
        DesenharTextoLinhas(linhas, (int)pn.x + 350, (int)pn.y + 98, 9, 11, ORANGE);
    }

    DrawText("v  ESCOLHA QUAL DAS SUAS ARMAS SAI  v", LARGURA_TELA / 2 - MeasureText("v  ESCOLHA QUAL DAS SUAS ARMAS SAI  v", 12) / 2, 218, 12, GOLD);
    DrawText("Os números verdes/vermelhos mostram como a arma NOVA fica em relação a cada uma.", LARGURA_TELA / 2 - MeasureText("Os números verdes/vermelhos mostram como a arma NOVA fica em relação a cada uma.", 10) / 2, 238, 10, GRAY);

    // ---- As duas armas que você carrega
    for (int i = 0; i < 2; i++) {
        TipoArma velha = (i == 0) ? b->j.armaBase : b->j.armaExtra;
        const DefArma *v = &DEF_ARMAS[velha];
        Rectangle r = RogueCartaTroca(i);
        bool sel = (b->trocaSel == i);
        if (sel) DrawRectangleRec((Rectangle){ r.x - 5, r.y - 5, r.width + 10, r.height + 10 }, Fade(RED, 0.2f + 0.1f * sinf(tempo * 6.0f)));
        DrawRectangleRec(r, (Color){ 20, 16, 34, 255 });
        DrawRectangleLinesEx(r, sel ? 3.0f : 1.0f, sel ? RED : Fade(v->cor, 0.7f));
        DrawRectangle((int)r.x, (int)r.y, (int)r.width, 18, Fade(sel ? RED : v->cor, 0.25f));
        DrawText(TextFormat("[%d]  %s", i + 1, (i == 0) ? "ARMA 1 (INICIAL)" : "ARMA 2"), (int)r.x + 8, (int)r.y + 4, 10, sel ? RED : v->cor);
        if (sel) DrawText("SAI", (int)(r.x + r.width) - 8 - MeasureText("SAI", 10), (int)r.y + 4, 10, RED);

        DrawCircleV((Vector2){ r.x + 34, r.y + 52 }, 24, Fade(v->cor, 0.15f));
        DesenharIconeArma(velha, (Vector2){ r.x + 34, r.y + 52 }, 1.5f, tempo);
        DrawText(TextoQueCabe(v->nome, 14, (int)r.width - 76), (int)r.x + 66, (int)r.y + 26, 14, v->cor);
        QuebrarTexto(v->efeito, (int)r.width - 76, 10, linhas, (int)sizeof(linhas));
        DesenharTextoLinhas(linhas, (int)r.x + 66, (int)r.y + 46, 10, 12, LIGHTGRAY);

        float dps = DpsArma(velha);
        snprintf(texto, sizeof(texto), "%.0f", dpsNova);
        DesenharBarraArma("DANO/s",  r.x + 10, r.y + 92,  r.width - 20, dpsNova, 110.0f, texto, nova->cor, dps);
        snprintf(texto, sizeof(texto), "%.1f/s", 1.0f / nova->intervalo);
        DesenharBarraArma("CADÊNCIA", r.x + 10, r.y + 108, r.width - 20, 1.0f / nova->intervalo, 6.0f, texto, nova->cor, 1.0f / v->intervalo);
        DesenharBarraArma("ALCANCE", r.x + 10, r.y + 124, r.width - 20, AlcanceArma(b->armaNova), 1.0f, "", nova->cor, AlcanceArma(velha));

        char ups[200];
        RogueMelhoriasDaArma(&b->j, velha, ups, (int)sizeof(ups));
        if (ups[0]) {
            QuebrarTexto(TextFormat("PERDE o efeito de: %s", ups), (int)r.width - 20, 9, linhas, (int)sizeof(linhas));
            DesenharTextoLinhas(linhas, (int)r.x + 10, (int)r.y + 144, 9, 11, ORANGE);
        } else {
            const char *nada = "Sem melhorias nesta arma: nada se perde.";
            if (MeasureText(nada, 9) > (int)r.width - 20) nada = "Sem melhorias: nada se perde.";
            DrawText(nada, (int)r.x + 10, (int)r.y + 150, 9, GRAY);
        }
        if (sel) DrawText("ENTER / CLIQUE: TROCAR ESTA", (int)(r.x + r.width / 2) - MeasureText("ENTER / CLIQUE: TROCAR ESTA", 10) / 2, (int)r.y + (int)r.height - 16, 10, YELLOW);
    }

    DrawText("A/D ou 1/2 escolhe   ENTER troca", LARGURA_TELA / 2 - MeasureText("A/D ou 1/2 escolhe   ENTER troca", 12) / 2, 484, 12, GRAY);
    Rectangle c = RET_CANCELAR_TROCA;
    DrawRectangleRec(c, Fade(BLACK, 0.6f));
    DrawRectangleLinesEx(c, 1, MouseEm(c) ? WHITE : GRAY);
    const char *rotulo = (b->trocaOrigem == 0) ? "DESCARTAR A ARMA NOVA (BACKSPACE)" : "MANTER AS MINHAS (BACKSPACE)";
    DrawText(rotulo, (int)(c.x + c.width / 2) - MeasureText(rotulo, 10) / 2, (int)c.y + 10, 10, WHITE);
}

void DesenharEscolha(const Biblioteca *b, float tempo)
{
    DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, 0.88f));

    const char *titulo = b->escolhaTreino ? "TREINO: ESCOLHA UMA MELHORIA"
                       : b->escolhaPrimeiraMissao ? "SUA PRIMEIRA MELHORIA"
                       : (b->modoEscolha == ESCOLHA_RARA) ? "MELHORIA RARA: ESCOLHA UMA"
                       : (b->modoEscolha == ESCOLHA_ARMA) ? "ARSENAL: ESCOLHA UMA" : "ESCOLHA UMA MELHORIA";
    DrawText(titulo, LARGURA_TELA / 2 - MeasureText(titulo, 24) / 2, 70, 24, GOLD);
    const char *sub = b->escolhaTreino ? "Na missão, cada sala limpa dá uma escolha assim. Aqui é só treino: nada é gasto nem guardado."
                    : b->escolhaPrimeiraMissao ? "Escolha UMA. Ela vale até o fim desta missão. Dano, cadência ou vida: qualquer uma ajuda."
                                       : "Ela vale até o fim desta tentativa. Combine melhorias para abrir SINERGIAS.";
    DrawText(sub, LARGURA_TELA / 2 - MeasureText(sub, 12) / 2, 100, 12, LIGHTGRAY);

    float entrada = fminf(1.0f, b->animEscolha / 0.3f);

    for (int i = 0; i < b->numOpcoes; i++) {

        OpcaoEscolha o = b->opcoes[i];
        Rectangle r = RogueCartaEscolha(i, b->numOpcoes);
        bool marcada = (i == b->selecaoEscolha);
        r.y += (1.0f - entrada) * 60.0f - (marcada ? 8.0f : 0.0f);

        const char *nome, *efeito, *categoria;
        Raridade rar;
        Color corCat;
        char nivel[48] = "", requisito[120] = "";

        if (o.tipo == OPC_UPGRADE) {
            const DefUpgrade *d = &UPGRADES[o.id];
            nome = d->nome; efeito = d->efeito; rar = d->raridade;
            categoria = NOMES_CATEGORIA_UP[d->categoria]; corCat = CORES_CATEGORIA_UP[d->categoria];
            if (b->j.up[o.id] == 0 && d->maxNivel > 1) snprintf(nivel, sizeof(nivel), "NOVA  (máx. %d)", d->maxNivel);
            else if (b->j.up[o.id] == 0)                snprintf(nivel, sizeof(nivel), "NOVA");
            else                    snprintf(nivel, sizeof(nivel), "NÍVEL %d -> %d  (máx. %d)", b->j.up[o.id], b->j.up[o.id] + 1, d->maxNivel);

            if (d->categoria == CAT_SINERGIA) {
                int a = -1, c2 = -1;
                for (int k = 0; k < NUM_UPGRADES; k++)
                    if (d->requer & BIT_UP(k)) { if (a < 0) a = k; else c2 = k; }
                snprintf(requisito, sizeof(requisito), "COMBINA: %s + %s", (a >= 0) ? UPGRADES[a].nome : "?", (c2 >= 0) ? UPGRADES[c2].nome : "?");
            } else if (d->arma >= 0) {
                snprintf(requisito, sizeof(requisito), "REQUER: %s", DEF_ARMAS[d->arma].nome);
            } else {
                snprintf(requisito, sizeof(requisito), "Sem pré-requisitos");
            }
        } else if (o.tipo == OPC_ITEM) {
            nome = ITENS[o.id].nome; efeito = ITENS[o.id].efeito; rar = ITENS[o.id].raridade;
            categoria = "UPGRADE PASSIVO"; corCat = (Color){ 0, 228, 255, 255 };
            snprintf(nivel, sizeof(nivel), "TEM %d DE %d", b->j.itens[o.id], ITENS[o.id].maxPilha);
            snprintf(requisito, sizeof(requisito), "Sem pré-requisitos");
        } else if (o.tipo == OPC_ARMA) {
            nome = DEF_ARMAS[o.id].nome; efeito = DEF_ARMAS[o.id].efeito; rar = DEF_ARMAS[o.id].raridade;
            categoria = "ARMA"; corCat = DEF_ARMAS[o.id].cor;
            snprintf(nivel, sizeof(nivel), "%s  ~%.0f dano/s", NOMES_ALCANCE[DEF_ARMAS[o.id].alcanceTipo], DpsArma((TipoArma)o.id));
            if (b->j.armaExtra != ARMA_DISPARADOR) snprintf(requisito, sizeof(requisito), "VOCÊ ESCOLHE QUAL ARMA TROCAR");
            else                                   snprintf(requisito, sizeof(requisito), "Vira a sua 2ª arma (TAB troca)");
        } else {
            nome = "RESERVA DE CONHECIMENTO"; efeito = "Não há melhorias novas para você agora: leve Fragmentos de Conhecimento.";
            rar = RARIDADE_COMUM; categoria = "FRAGMENTOS"; corCat = (Color){ 0, 228, 255, 255 };
            snprintf(nivel, sizeof(nivel), "+%d FRAGMENTOS", FC_OPCAO_RESERVA);
        }

        Color corRar = CORES_RARIDADE[rar];
        if (marcada)
            DrawRectangleRec((Rectangle){ r.x - 6, r.y - 6, r.width + 12, r.height + 12 }, Fade(corRar, 0.18f + 0.08f * sinf(tempo * 5.0f)));
        DrawRectangleRec(r, (Color){ 20, 16, 34, 255 });
        DrawRectangleLinesEx(r, marcada ? 3.0f : 2.0f, marcada ? corRar : Fade(corRar, 0.6f));

        // Faixa da categoria
        DrawRectangle((int)r.x, (int)r.y, (int)r.width, 20, Fade(corCat, 0.25f));
        DrawText(categoria, (int)r.x + 8, (int)r.y + 5, 10, corCat);
        DrawText(TextFormat("[%d]", i + 1), (int)(r.x + r.width) - 26, (int)r.y + 5, 10, GRAY);

        // Ícone
        Vector2 ic = { r.x + r.width / 2, r.y + 58 };
        DrawCircleV(ic, 26, Fade(corCat, 0.15f));
        DrawCircleLines((int)ic.x, (int)ic.y, 26, Fade(corRar, 0.8f));
        DesenharIconeUpgrade(b, o, ic, 1.1f, tempo);

        // Nome, raridade e nível
        char linhas[256];
        QuebrarTexto(nome, (int)r.width - 20, 16, linhas, (int)sizeof(linhas));
        int ny = (int)r.y + 94;
        int n = DesenharTextoLinhas(linhas, (int)r.x + 10, ny, 16, 18, marcada ? YELLOW : WHITE);
        ny += n * EspacoLinhaTexto(18, 16) + 2;
        DrawText(NOMES_RARIDADE[rar], (int)r.x + 10, ny, 10, corRar);
        {   // o nível (ou o alcance da arma) vai para a linha de baixo se não couber do lado da raridade
            int xNivel = (int)r.x + 10 + MeasureText(NOMES_RARIDADE[rar], 10) + 10;
            if (xNivel + MeasureText(nivel, 10) > (int)(r.x + r.width) - 8) {
                ny += EspacoLinhaTexto(14, 10);
                xNivel = (int)r.x + 10;
            }
            DrawText(nivel, xNivel, ny, 10, LIGHTGRAY);
        }
        ny += EspacoLinhaTexto(18, 10);

        // Efeito
        QuebrarTexto(efeito, (int)r.width - 20, 12, linhas, (int)sizeof(linhas));
        DesenharTextoLinhas(linhas, (int)r.x + 10, ny, 12, 15, (Color){ 220, 220, 230, 255 });

        // Pré-requisitos
        if (requisito[0]) {
            QuebrarTexto(requisito, (int)r.width - 20, 10, linhas, (int)sizeof(linhas));
            int nl = ContarLinhas(linhas);
            int hReq = nl * EspacoLinhaTexto(12, 10);
            DrawLine((int)r.x + 10, (int)(r.y + r.height) - 16 - hReq, (int)(r.x + r.width) - 10, (int)(r.y + r.height) - 16 - hReq, Fade(corRar, 0.3f));
            DesenharTextoLinhas(linhas, (int)r.x + 10, (int)(r.y + r.height) - 10 - hReq, 10, 12,
                                (o.tipo == OPC_UPGRADE && UPGRADES[o.id].categoria == CAT_SINERGIA) ? CORES_CATEGORIA_UP[CAT_SINERGIA] : GRAY);
        }
    }

    if (b->escolhaTreino) {                                  // no treino não há reroll nem recusar: só a dica
        const char *dicaTreino = "A/D, 1-2 ou mouse: escolher     E / clique: confirmar";
        DrawText(dicaTreino, LARGURA_TELA / 2 - MeasureText(dicaTreino, 12) / 2, 450, 12, LIGHTGRAY);
        const char *dicaMissao = "Na missão você também pode REROLAR (R) as opções ou RECUSAR (BACKSPACE) e ganhar Fragmentos.";
        DrawText(dicaMissao, LARGURA_TELA / 2 - MeasureText(dicaMissao, 10) / 2, 474, 10, GRAY);
        return;
    }

    const char *ajuda = TextFormat("A/D, 1-3 ou mouse: escolher     E / clique: confirmar     BACKSPACE: recusar (+%d FC)", FC_RECUSAR_ESCOLHA);
    DrawText(ajuda, LARGURA_TELA / 2 - MeasureText(ajuda, 10) / 2, 438, 10, GRAY);      // logo abaixo das cartas (terminam em y=432)

    // Recusar também com o mouse (o mesmo que BACKSPACE)
    DesenharBotaoMenu(RET_REROLAR_ESCOLHA, (b->j.rerolls > 0) ? TextFormat("REROLAR [R]  (%d GRÁTIS)", b->j.rerolls) : TextFormat("REROLAR [R]  (%d FC)", REROLL_CUSTO_FC), 12,
                      (b->j.rerolls > 0 || b->perm->fragmentos >= REROLL_CUSTO_FC) ? SKYBLUE : GRAY, false);
    DesenharBotaoMenu(RET_RECUSAR_ESCOLHA, TextFormat("RECUSAR (+%d FC)", FC_RECUSAR_ESCOLHA), 12, GOLD, false);
}

// Janela dos eventos (o mesmo visual do desafio educativo)
void DesenharEvento(const Biblioteca *b, float tempo)
{
    TelaEvento t;
    RogueMontarTelaEvento(b, &t);

    DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, 0.65f));
    Rectangle painel = { 70, 80, 660, 420 };
    Color cor = CORES_SALAS_ROGUE[b->tipoSala];
    DrawRectangleRec(painel, (Color){ 18, 15, 30, 255 });
    DrawRectangleLinesEx(painel, 4, cor);

    int x = (int)painel.x + 24, y = (int)painel.y + 18;
    DesenharIconeSala(b->tipoSala, (Vector2){ (float)x + 12, (float)y + 10 }, 1.0f, tempo);
    DrawText(t.titulo, x + 34, y, 22, cor);
    y += 40;

    char linhas[640];
    QuebrarTexto(b->eventoResolvido ? b->resultadoEvento : t.texto, (int)painel.width - 48, 16, linhas, (int)sizeof(linhas));
    int n = DesenharTextoLinhas(linhas, x, y, 16, 22, WHITE);
    y += n * EspacoLinhaTexto(22, 16) + 18;

    if (b->eventoResolvido) {
        if (((int)(tempo * 2.0f)) % 2 == 0)
            DesenharDicaAvancar((int)(painel.x + painel.width) - 90, (int)(painel.y + painel.height) - 28, 14);
        return;
    }

    DesenharBotaoFechar(RET_FECHAR_EVENTO, cor);

    for (int i = 0; i < t.numOpcoes; i++) {
        bool marcada = (i == b->selecaoEvento);
        bool pode = t.disponivel[i];
        Color corOp = !pode ? (Color){ 150, 140, 165, 255 } : (marcada ? YELLOW : WHITE);     // indisponível: apagado, mas ainda legível

        QuebrarTexto(TextFormat("%d) %s", i + 1, t.rotulo[i]), (int)painel.width - 90, 16, linhas, (int)sizeof(linhas));
        int nr = ContarLinhas(linhas);
        char ef[300] = "";
        if (t.efeito[i][0]) QuebrarTexto(t.efeito[i], (int)painel.width - 100, 12, ef, (int)sizeof(ef));
        int ne = ef[0] ? ContarLinhas(ef) : 0;
        int alt = nr * EspacoLinhaTexto(20, 16) + ne * EspacoLinhaTexto(15, 12) + 10;

        if (marcada) {
            DrawRectangle(x - 10, y - 5, (int)painel.width - 28, alt, Fade(cor, 0.15f));
            DesenharCursorLuci((float)x, (float)y + 8);
        }
        DesenharTextoLinhas(linhas, x + 18, y, 16, 20, corOp);
        if (ne > 0) DesenharTextoLinhas(ef, x + 30, y + nr * EspacoLinhaTexto(20, 16), 12, 15, pode ? LIGHTGRAY : (Color){ 150, 140, 165, 255 });
        if (!pode)  DrawText("(indisponível)", (int)(painel.x + painel.width) - 120, y + 2, 10, RED);
        y += alt + 6;
    }

    const char *ajuda = "W/S ou MOUSE escolher     E / CLIQUE confirmar     BACKSPACE ou X sair";
    DrawText(ajuda, LARGURA_TELA / 2 - MeasureText(ajuda, 10) / 2, (int)(painel.y + painel.height) - LinhaTexto(20), 10, GRAY);
}
