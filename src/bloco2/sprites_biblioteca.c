#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include "../../include/core/texto.h"
#include "../../include/bloco2/biblioteca.h"

// =============================================================
// SPRITES DA BIBLIOTECA (Blocos 2 e 3): inimigos e ladrilhos do chão
//   Folhas em resources/sprites/biblioteca/inimigos/ e .../chao/ (ver o LEIA-ME de cada pasta).
//   Uma folha = quadros na horizontal e estados na vertical, todos do mesmo tamanho (tabela abaixo).
//   Arquivo ausente ou com tamanho errado = o jogo desenha aquele inimigo/chão com formas, como antes.
// =============================================================

typedef struct {
    const char *nome;           // arquivo (sem .png)
    int largura, altura;        // tamanho de UM quadro
} FolhaSprite;

static const FolhaSprite FOLHAS_INIMIGOS[NUM_SPRITES_INIMIGO] = {
    [SPR_SPAMMER]          = { "spammer",          28, 26 },
    [SPR_SPAM]             = { "spam",             16, 12 },
    [SPR_PHISHER]          = { "phisher",          28, 46 },
    [SPR_FIREWALL]         = { "firewall",         46, 46 },
    [SPR_OVERCLOCK]        = { "overclock",        42, 42 },
    [SPR_BOTNET]           = { "botnet",           22, 22 },
    [SPR_KEYLOGGER]        = { "keylogger",        22, 44 },
    [SPR_CACHE]            = { "cache",            36, 12 },
    [SPR_NUCLEO]           = { "nucleo",           40, 48 },
    [SPR_SENTINELA]        = { "sentinela",        44, 44 },
    [SPR_SENTINELA_CABECA] = { "sentinela_cabeca", 12, 12 },
    [SPR_RETRO]            = { "retro",            46, 40 },
    [SPR_JANELA]           = { "janela",           74, 58 },
    [SPR_TOKEN]            = { "token",            16, 16 },
    [SPR_ALUCINACAO]       = { "alucinacao",       36, 38 },
    [SPR_VERBOSO]          = { "verboso",          60, 52 },
    [SPR_FRAGMENTO]        = { "fragmento",        30, 36 },
    [SPR_GUARDIAO]         = { "guardiao",         38, 42 },
};

#define LADO_LADRILHO        40      // os ladrilhos têm o tamanho da grade antiga
#define NUM_VARIACOES_CHAO   8       // ladrilhos em cada folha do chão (na horizontal)

#if SPRITES_CHAO
// Quanto aparece cada ladrilho (soma 100): os três primeiros são lisos, os outros têm detalhe
static const int PESOS_BIBLIOTECA[NUM_VARIACOES_CHAO] = { 22, 22, 16, 10, 10, 5, 7, 8 };
static const int PESOS_NUCLEO[NUM_VARIACOES_CHAO]     = { 25, 25, 15, 8, 7, 6, 7, 7 };
#endif

static Texture2D texInimigos[NUM_SPRITES_INIMIGO];
static int quadrosInimigo[NUM_SPRITES_INIMIGO], estadosInimigo[NUM_SPRITES_INIMIGO];
static Texture2D texChaoBiblioteca, texChaoNucleo;
static bool spritesCarregados = false;

#if SPRITES_INIMIGOS || SPRITES_CHAO
// Carrega uma folha e confere se o tamanho fecha com o quadro (senão, fica sem: o jogo usa as formas)
static Texture2D CarregarFolha(const char *arquivo, int largura, int altura, int *quadros, int *estados)
{
    *quadros = *estados = 0;
    if (!FileExists(arquivo))
        return (Texture2D){ 0 };

    Texture2D t = LoadTexture(arquivo);
    if (t.id == 0)
        return t;
    if (t.width < largura || t.height < altura || t.width % largura != 0 || t.height % altura != 0) {
        TraceLog(LOG_WARNING, "UtopIA: %s tem %dx%d, que não fecha com quadros de %dx%d (ignorado)", arquivo, t.width, t.height, largura, altura);
        UnloadTexture(t);
        return (Texture2D){ 0 };
    }
    SetTextureFilter(t, TEXTURE_FILTER_POINT);          // pixel art: sem borrar
    *quadros = t.width / largura;
    *estados = t.height / altura;
    return t;
}
#endif

void CarregarSpritesBiblioteca(void)
{
    if (spritesCarregados)
        return;
    spritesCarregados = true;

#if SPRITES_INIMIGOS
    for (int s = 0; s < NUM_SPRITES_INIMIGO; s++) {
        const FolhaSprite *f = &FOLHAS_INIMIGOS[s];
        texInimigos[s] = CarregarFolha(TextFormat("resources/sprites/biblioteca/inimigos/%s.png", f->nome),
                                       f->largura, f->altura, &quadrosInimigo[s], &estadosInimigo[s]);
    }
#endif
#if SPRITES_CHAO
    int q, e;
    texChaoBiblioteca = CarregarFolha("resources/sprites/biblioteca/chao/biblioteca.png", LADO_LADRILHO, LADO_LADRILHO, &q, &e);
    if (texChaoBiblioteca.id > 0 && q < NUM_VARIACOES_CHAO) { UnloadTexture(texChaoBiblioteca); texChaoBiblioteca = (Texture2D){ 0 }; }
    texChaoNucleo = CarregarFolha("resources/sprites/biblioteca/chao/nucleo.png", LADO_LADRILHO, LADO_LADRILHO, &q, &e);
    if (texChaoNucleo.id > 0 && q < NUM_VARIACOES_CHAO) { UnloadTexture(texChaoNucleo); texChaoNucleo = (Texture2D){ 0 }; }
#endif
}

void DescarregarSpritesBiblioteca(void)
{
    for (int s = 0; s < NUM_SPRITES_INIMIGO; s++)
        if (texInimigos[s].id > 0) UnloadTexture(texInimigos[s]);
    if (texChaoBiblioteca.id > 0) UnloadTexture(texChaoBiblioteca);
    if (texChaoNucleo.id > 0)     UnloadTexture(texChaoNucleo);

    memset(texInimigos, 0, sizeof(texInimigos));
    memset(quadrosInimigo, 0, sizeof(quadrosInimigo));
    memset(estadosInimigo, 0, sizeof(estadosInimigo));
    texChaoBiblioteca = texChaoNucleo = (Texture2D){ 0 };
    spritesCarregados = false;
}

bool SpriteInimigoPronto(SpriteInimigo s)
{
    return s >= 0 && s < NUM_SPRITES_INIMIGO && texInimigos[s].id > 0;
}

int QuadrosSpriteInimigo(SpriteInimigo s)
{
    return SpriteInimigoPronto(s) ? quadrosInimigo[s] : 1;
}

// Desenha um quadro de uma folha. "pivo" é o ponto do quadro (em pixels da folha) que vai em "pos";
// sem escala, o canto cai num pixel inteiro (a pixel art fica nítida).
void DesenharSpriteInimigoPivo(SpriteInimigo s, int estado, int quadro, Vector2 pos, Vector2 pivo, float escala, Color tinta)
{
    if (!SpriteInimigoPronto(s) || escala <= 0.0f)
        return;

    const FolhaSprite *f = &FOLHAS_INIMIGOS[s];
    int q = ((quadro % quadrosInimigo[s]) + quadrosInimigo[s]) % quadrosInimigo[s];
    int e = Clamp(estado, 0, estadosInimigo[s] - 1);
    Rectangle origem = { (float)(q * f->largura), (float)(e * f->altura), (float)f->largura, (float)f->altura };
    Rectangle destino = { roundf(pos.x - pivo.x * escala), roundf(pos.y - pivo.y * escala), f->largura * escala, f->altura * escala };

    DrawTexturePro(texInimigos[s], origem, destino, (Vector2){ 0, 0 }, 0.0f, tinta);
}

void DesenharSpriteInimigo(SpriteInimigo s, int estado, int quadro, Vector2 centro, float escala, Color tinta)
{
    const FolhaSprite *f = &FOLHAS_INIMIGOS[s];
    DesenharSpriteInimigoPivo(s, estado, quadro, centro, (Vector2){ f->largura / 2.0f, f->altura / 2.0f }, escala, tinta);
}

// Quadro da animação pelo tempo (fps quadros por segundo); "fase" desencontra inimigos iguais
int QuadroAnimInimigo(SpriteInimigo s, float tempo, float fps, float fase)
{
    return (int)floorf(tempo * fps + fase) % QuadrosSpriteInimigo(s);
}

// Sombra no chão, em pixels (combina com os sprites)
void DesenharSombraSprite(Vector2 pos, float largura, float altura)
{
    DrawEllipse((int)roundf(pos.x), (int)roundf(pos.y), largura, altura, Fade(BLACK, 0.38f));
}

// ---------- Chão ----------
// Ladrilhos sorteados por sala (a mesma sala sempre tem o mesmo chão). Os lisos às vezes vêm espelhados.
bool DesenharChaoSprites(const Biblioteca *b)
{
#if SPRITES_CHAO
    bool nucleo = RogueNoBloco3(b);
    Texture2D tex = nucleo ? texChaoNucleo : texChaoBiblioteca;
    const int *pesos = nucleo ? PESOS_NUCLEO : PESOS_BIBLIOTECA;
    if (tex.id == 0 || b->tipoSala == SALA_SILOGISTA)
        return false;

    Rectangle arena = ArenaRogue();
    unsigned int semente = Ruido((unsigned int)(b->noAtual + 11) * 2654435761u ^ (unsigned int)b->tipoSala * 977u);
    // Sem a penumbra das salas da missão (átrio, sala do Mestre, luz desligada nas Opções) o chão fica
    // mais apagado, para não brigar com o resto; o corredor do Núcleo tem o tom dele
    bool semPenumbra = !LUZ_SALAS || !Prefs()->luzSalas || b->tipoSala == SALA_ATRIO || b->tipoSala == SALA_MESTRE;
    Color tinta = nucleo      ? (Color){ 206, 198, 214, 255 }
                : semPenumbra ? (Color){ 196, 196, 212, 255 }
                              : (Color){ 226, 226, 236, 255 };

    for (float y = arena.y; y < arena.y + arena.height; y += LADO_LADRILHO) {
        for (float x = arena.x; x < arena.x + arena.width; x += LADO_LADRILHO) {
            unsigned int h = Ruido(semente ^ ((unsigned int)(int)x * 73856093u) ^ ((unsigned int)(int)y * 19349663u));
            int sorteio = (int)(h % 100u), v = 0;
            while (v < NUM_VARIACOES_CHAO - 1 && sorteio >= pesos[v]) { sorteio -= pesos[v]; v++; }

            float w = fminf(LADO_LADRILHO, arena.x + arena.width - x), hh = fminf(LADO_LADRILHO, arena.y + arena.height - y);
            bool espelha = (v < 3) && ((h >> 9) & 1u);
            Rectangle origem = { (float)(v * LADO_LADRILHO), 0, espelha ? -w : w, hh };
            if (espelha) origem.x += LADO_LADRILHO - w;
            DrawTextureRec(tex, origem, (Vector2){ x, y }, tinta);
        }
    }
    return true;
#else
    (void)b;
    return false;
#endif
}
