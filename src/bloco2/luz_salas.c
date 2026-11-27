// Iluminação das salas da missão (Bloco 2): uma penumbra por cima do chão, dos personagens e dos
// objetos, com luz em volta do Thomas, dos inimigos, dos tiros, das portas e do que dá para usar.
// Tiros, partículas, faíscas, números e avisos de tecla são desenhados depois: ficam acesos.
// No fim do arquivo, a poeira no ar (aqui e nas salas do Bloco 1).
//
// Como funciona: uma grade cobre a sala e cada ponto soma a luz de cada fonte por perto (que some
// suave até o raio dela). A grade é desenhada duas vezes, com a cor interpolada entre os pontos:
//   RogueDesenharLuzChao - logo depois do chão, SOMANDO um pouco da luz: as poças de luz no chão
//                          (o chão é quase preto, então só escurecer não mostraria a luz)
//   RogueDesenharLuz     - por cima dos personagens e objetos, MULTIPLICANDO (BLEND_MULTIPLIED) pela
//                          luz ambiente da sala mais as fontes: luz cheia = a cor de sempre, menos = mais escuro
#include <math.h>
#include "raylib.h"
#include "rlgl.h"
#include "../../include/core/fonte.h"
#include "../../include/core/texto.h"
#include "../../include/bloco2/biblioteca.h"

#if LUZ_SALAS

#define LUZ_COLUNAS  61                 // pontos da grade (60 x 41 quadradinhos de uns 12 pixels)
#define LUZ_LINHAS   42

typedef struct { float r, g, b; } LuzPonto;

static LuzPonto grade[LUZ_LINHAS][LUZ_COLUNAS];     // só as fontes (o ambiente entra na hora de desenhar)
static bool gradePronta = false;                    // o chão já juntou as luzes deste quadro

// A sala com as paredes (a arena é { 40, 60, 720, 470 } e a estante decorativa fica em y 42)
static const Rectangle AREA_LUZ = { 36, 38, 728, 496 };

// Salas com penumbra: as da missão e a do Alucinador. O átrio, a sala da Mestre (tutorial),
// o Silogista e o Bloco 3 já têm o próprio clima e ficam como estão.
static bool SalaComLuz(const Biblioteca *b)
{
    if (!Prefs()->luzSalas || RogueNoBloco3(b))       // (desligada na tela de Opções)
        return false;

    switch (b->tipoSala) {
        case SALA_ATRIO: case SALA_SILOGISTA: case SALA_TRANSICAO: case SALA_LUCI: case SALA_MESTRE:
            return false;
        default:
            return true;
    }
}

// Luz ambiente de cada tipo de sala (antes da LUZ_FORCA_PENUMBRA)
static Color AmbienteDaSala(const Biblioteca *b)
{
    switch (b->tipoSala) {
        case SALA_DESCANSO: return (Color){ 175, 150, 135, 255 };     // sala de leitura: penumbra quente
        case SALA_SECRETA:  return (Color){ 100,  95, 140, 255 };     // a mais escura
        case SALA_CHEFE:    return (Color){ 130, 105, 155, 255 };     // o Alucinador puxa para o magenta
        default:            return (Color){ 125, 120, 170, 255 };     // azul-arroxeado
    }
}

// Soma uma fonte de luz nos pontos da grade dentro do raio dela
static void SomarLuz(Vector2 pos, float raio, Color cor, float forca)
{
    if (raio < 2.0f || forca <= 0.0f)
        return;

    float cw = AREA_LUZ.width / (LUZ_COLUNAS - 1);
    float ch = AREA_LUZ.height / (LUZ_LINHAS - 1);
    int c0 = (int)floorf((pos.x - raio - AREA_LUZ.x) / cw), c1 = (int)ceilf((pos.x + raio - AREA_LUZ.x) / cw);
    int l0 = (int)floorf((pos.y - raio - AREA_LUZ.y) / ch), l1 = (int)ceilf((pos.y + raio - AREA_LUZ.y) / ch);
    if (c0 < 0) c0 = 0;
    if (l0 < 0) l0 = 0;
    if (c1 > LUZ_COLUNAS - 1) c1 = LUZ_COLUNAS - 1;
    if (l1 > LUZ_LINHAS - 1)  l1 = LUZ_LINHAS - 1;

    float kr = cor.r / 255.0f * forca, kg = cor.g / 255.0f * forca, kb = cor.b / 255.0f * forca;
    float inv = 1.0f / (raio * raio);

    for (int l = l0; l <= l1; l++) {
        float dy = AREA_LUZ.y + l * ch - pos.y;
        for (int c = c0; c <= c1; c++) {
            float dx = AREA_LUZ.x + c * cw - pos.x;
            float t = 1.0f - (dx * dx + dy * dy) * inv;
            if (t <= 0.0f) continue;
            t *= t;                                 // forte no centro, some suave na borda
            grade[l][c].r += kr * t;
            grade[l][c].g += kg * t;
            grade[l][c].b += kb * t;
        }
    }
}

// Soma uma luz ao longo de um segmento (laser, barreira, feixe): cai com a distância até a linha,
// em vez de um círculo enorme no meio dela
static void SomarLuzLinha(Vector2 a, Vector2 b, float raio, Color cor, float forca)
{
    if (raio < 2.0f || forca <= 0.0f)
        return;

    float cw = AREA_LUZ.width / (LUZ_COLUNAS - 1);
    float ch = AREA_LUZ.height / (LUZ_LINHAS - 1);
    int c0 = (int)floorf((fminf(a.x, b.x) - raio - AREA_LUZ.x) / cw), c1 = (int)ceilf((fmaxf(a.x, b.x) + raio - AREA_LUZ.x) / cw);
    int l0 = (int)floorf((fminf(a.y, b.y) - raio - AREA_LUZ.y) / ch), l1 = (int)ceilf((fmaxf(a.y, b.y) + raio - AREA_LUZ.y) / ch);
    if (c0 < 0) c0 = 0;
    if (l0 < 0) l0 = 0;
    if (c1 > LUZ_COLUNAS - 1) c1 = LUZ_COLUNAS - 1;
    if (l1 > LUZ_LINHAS - 1)  l1 = LUZ_LINHAS - 1;

    Vector2 ab = Vector2Subtract(b, a);
    float comp2 = fmaxf(Vector2LengthSqr(ab), 0.0001f);
    float kr = cor.r / 255.0f * forca, kg = cor.g / 255.0f * forca, kb = cor.b / 255.0f * forca;
    float inv = 1.0f / (raio * raio);

    for (int l = l0; l <= l1; l++) {
        for (int c = c0; c <= c1; c++) {
            Vector2 p = { AREA_LUZ.x + c * cw, AREA_LUZ.y + l * ch };
            float u = Clamp(Vector2DotProduct(Vector2Subtract(p, a), ab) / comp2, 0.0f, 1.0f);
            Vector2 perto = Vector2Add(a, Vector2Scale(ab, u));
            float t = 1.0f - Vector2LengthSqr(Vector2Subtract(p, perto)) * inv;
            if (t <= 0.0f) continue;
            t *= t;
            grade[l][c].r += kr * t;
            grade[l][c].g += kg * t;
            grade[l][c].b += kb * t;
        }
    }
}

// Luz de uma armadilha retangular: uma linha no eixo comprido (o laser corta a sala inteira)
static void SomarLuzRetangulo(Rectangle r, float margem, Color cor, float forca)
{
    Vector2 c = { r.x + r.width / 2, r.y + r.height / 2 };
    bool deitado = r.width >= r.height;
    float curto = deitado ? r.height : r.width;
    float meio = ((deitado ? r.width : r.height) - curto) / 2;
    Vector2 eixo = deitado ? (Vector2){ meio, 0 } : (Vector2){ 0, meio };
    SomarLuzLinha(Vector2Subtract(c, eixo), Vector2Add(c, eixo), curto / 2 + margem, cor, forca);
}

// Todas as fontes de luz da sala neste quadro
static void JuntarLuzes(const Biblioteca *b, float tempo)
{
    const JogadorRogue *j = &b->j;

    // O Thomas leva a luz com ele (e a Luci, um pouco de ciano)
    SomarLuz(j->pos, LUZ_RAIO_THOMAS, (Color){ 255, 236, 210, 255 }, 0.95f);
    SomarLuz((Vector2){ j->pos.x, j->pos.y - 24.0f }, 90.0f, (Color){ 110, 200, 255, 255 }, 0.22f + 0.04f * sinf(tempo * 3.0f));

    // Inimigos brilham na própria cor (mais quando levam dano)
    for (int i = 0; i < MAX_INIMIGOS; i++) {
        const Inimigo *e = &b->ini[i];
        if (!e->ativo) continue;
        float surgindo = (e->surgindo > 0.0f) ? 1.0f - e->surgindo / TEMPO_SURGINDO : 1.0f;
        float forca = (e->elite ? 0.6f : 0.45f) + ((e->flash > 0.0f) ? 0.4f : 0.0f);
        SomarLuz(e->pos, (e->raio * 3.0f + 24.0f) * surgindo, DEF_INIMIGOS[e->tipo].cor, forca);

        if (e->tipo == INIMIGO_SENTINELA && e->estado == 2)           // o feixe acende o chão por onde passa
            SomarLuzLinha(e->pos, e->fimFeixe, 46.0f, (Color){ 170, 235, 255, 255 }, 0.8f);
        if (e->tipo == INIMIGO_FIREWALL && e->estado == 2) {          // a barreira de energia
            Vector2 a, c;
            RogueBarreiraFirewall(e, &a, &c);
            SomarLuzLinha(a, c, 34.0f, (Color){ 0, 228, 48, 255 }, 0.45f);
        }
    }
    if (b->chefe.ativo)
        SomarLuz(b->chefe.pos, 170.0f, (Color){ 255, 70, 220, 255 }, 0.6f + ((b->chefe.flash > 0.0f) ? 0.3f : 0.0f));

    // Tiros: cada um ilumina o chão por onde passa. Armas de muitas balas por disparo (a Fragmentadora
    // solta 7 ou mais de uma vez) acendem menos cada uma: juntas, iluminavam como um holofote
    for (int i = 0; i < MAX_PROJETEIS; i++) {
        const Projetil *p = &b->proj[i];
        if (!p->ativo) continue;
        if (p->alvoDedutivo != 0) {
            SomarLuz(p->pos, 70.0f, GOLD, 0.6f);
        } else if (p->doJogador) {
            float balas = (float)DEF_ARMAS[p->arma].projeteis;
            float forca = (balas > 1.0f && !p->secundario) ? 0.5f / sqrtf(balas) : 0.5f;
            float raio = (balas > 1.0f && !p->secundario) ? 26.0f + p->raio * 2.0f : 34.0f + p->raio * 4.0f;
            SomarLuz(p->pos, raio, p->secundario ? LIME : DEF_ARMAS[p->arma].cor, forca);
        } else {
            SomarLuz(p->pos, 30.0f + p->raio * 3.0f, (Color){ 255, 150, 50, 255 }, 0.55f);
        }
    }

    // Explosões e mortes: um clarão rápido
    for (int i = 0; i < MAX_ANEIS; i++) {
        const AnelEfeito *a = &b->aneis[i];
        if (a->tempo > 0.0f)
            SomarLuz(a->pos, a->raio * 1.6f + 30.0f, a->cor, 0.9f * a->tempo / 0.35f);
    }

    // Poças e zonas de dano ficam bem visíveis
    for (int i = 0; i < MAX_AREAS; i++) {
        const AreaDano *a = &b->areas[i];
        if (a->ativo) SomarLuz(a->pos, a->raio * 1.4f + 10.0f, a->cor, 0.55f);
    }

    // Armadilhas: laranja avisando, vermelho ativas. O laser acende só a faixa dele: vermelho piscando
    // no aviso e na cor do raio (ciano e branco) quando dispara. Com a sala limpa elas ficam apagadas.
    bool desligadas = b->limpa && !(b->tipoSala == SALA_DESAFIO && b->estadoDesafio == 1);
    for (int i = 0; i < MAX_ARMADILHAS && !desligadas; i++) {
        const Armadilha *a = &b->arm[i];
        if (!a->ativo || a->fase == 0) continue;
        Color cor = (a->fase == 1) ? (Color){ 255, 170, 50, 255 } : (Color){ 255, 60, 60, 255 };
        if (a->tipo == ARM_FEIXE) {
            if (a->fase == 1) SomarLuzRetangulo(a->r, 22.0f, (Color){ 255, 50, 60, 255 }, (sinf(tempo * 31.4f) > 0.0f) ? 0.35f : 0.18f);
            else              SomarLuzRetangulo(a->r, 40.0f, (Color){ 150, 235, 255, 255 }, 0.9f);
        } else if (a->r.width > 0.0f && a->r.height > 0.0f) {
            SomarLuzRetangulo(a->r, 30.0f, cor, (a->fase == 1) ? 0.4f : 0.6f);
        } else {
            SomarLuz(a->pos, a->raio * 1.4f + 10.0f, cor, (a->fase == 1) ? 0.4f : 0.6f);
        }
    }

    // Portas: ciano quando a sala está limpa, um vermelho fraco enquanto está trancada
    // (abrindo agora: um clarão que vai apagando até a luz de sempre)
    float clarao = 0.0f;
#if PORTAS_ANIMADAS
    if (b->limpa && b->portasAbrindo > 0.0f)
        clarao = b->portasAbrindo / PORTAS_ABRIR_TEMPO;
#endif
    for (int k = 0; k < b->numPortas; k++) {
        Rectangle r = b->portas[k].r;
        Vector2 c = { r.x + r.width / 2, r.y + r.height / 2 };
        if (b->limpa) SomarLuz(c, 110.0f + 50.0f * clarao, (Color){ 0, 228, 255, 255 }, 0.55f + 0.6f * clarao);
        else          SomarLuz(c, 70.0f, (Color){ 230, 41, 55, 255 }, 0.3f);
    }

    // O que dá para usar na sala
    if (b->tipoSala == SALA_EDUCATIVA)
        SomarLuz(POS_TERMINAL, 110.0f, (Color){ 0, 228, 255, 255 }, b->terminalLido ? 0.35f : 0.6f);
    if (b->tipoSala == SALA_RECOMPENSA && !b->recompensaPega)
        for (int i = 0; i < NUM_PEDESTAIS; i++)
            SomarLuz(POS_PEDESTAIS[i], 95.0f, GOLD, 0.5f);
    if (b->recompensaAtiva)
        SomarLuz(b->recompensaPos, 95.0f, GOLD, 0.55f);
    if (b->tipoSala == SALA_EVENTO || b->tipoSala == SALA_DESCANSO || b->tipoSala == SALA_DESAFIO || b->tipoSala == SALA_LABORATORIO) {
        Color cor = (b->tipoSala == SALA_DESCANSO) ? (Color){ 255, 190, 120, 255 }
                  : (b->tipoSala == SALA_EVENTO)   ? (Color){ 230, 90, 255, 255 } : (Color){ 0, 228, 255, 255 };
        SomarLuz(b->posObjetoSala, 120.0f, cor, 0.6f);
    }

    // Fragmentos e itens no chão
    for (int i = 0; i < MAX_COLETAVEIS; i++) {
        const Coletavel *c = &b->col[i];
        if (c->ativo) SomarLuz(c->pos, 50.0f, c->fragmento ? (Color){ 0, 228, 255, 255 } : GOLD, 0.5f);
    }
}

static unsigned char Canal(float v)
{
    if (v >= 1.0f) return 255;
    if (v <= 0.0f) return 0;
    return (unsigned char)(v * 255.0f);
}

// Desenha a grade em quadradinhos com uma cor em cada canto (o meio sai em degradê): cor = base + luz * k.
// Usa a textura das formas, como as funções Draw* da raylib, para funcionar em qualquer versão.
static void DesenharGrade(LuzPonto base, float kLuz, int modo)
{
    Texture2D tex = GetShapesTexture();
    Rectangle ret = GetShapesTextureRectangle();
    float u = (ret.x + ret.width * 0.5f) / (float)tex.width;
    float v = (ret.y + ret.height * 0.5f) / (float)tex.height;
    float cw = AREA_LUZ.width / (LUZ_COLUNAS - 1);
    float ch = AREA_LUZ.height / (LUZ_LINHAS - 1);

    BeginBlendMode(modo);
    for (int l = 0; l < LUZ_LINHAS - 1; l++) {
        rlCheckRenderBatchLimit((LUZ_COLUNAS - 1) * 4);
        rlSetTexture(tex.id);
        rlBegin(RL_QUADS);
        for (int c = 0; c < LUZ_COLUNAS - 1; c++) {
            float x0 = AREA_LUZ.x + c * cw, x1 = x0 + cw;
            float y0 = AREA_LUZ.y + l * ch, y1 = y0 + ch;
            const LuzPonto *p[4] = { &grade[l][c], &grade[l + 1][c], &grade[l + 1][c + 1], &grade[l][c + 1] };
            const float px[4] = { x0, x0, x1, x1 }, py[4] = { y0, y1, y1, y0 };
            for (int k = 0; k < 4; k++) {           // em cima à esquerda, embaixo à esquerda, embaixo à direita, em cima à direita
                // a luz somada tem teto: muitas fontes juntas (rajada, explosões) não estouram o chão em branco
                rlColor4ub(Canal(base.r + fminf(p[k]->r, 1.0f) * kLuz), Canal(base.g + fminf(p[k]->g, 1.0f) * kLuz),
                           Canal(base.b + fminf(p[k]->b, 1.0f) * kLuz), 255);
                rlTexCoord2f(u, v);
                rlVertex2f(px[k], py[k]);
            }
        }
        rlEnd();
        rlSetTexture(0);
    }
    EndBlendMode();
}

#endif // LUZ_SALAS

// Poças de luz no chão (chamar dentro do BeginMode2D, logo depois do chão e antes das estantes)
void RogueDesenharLuzChao(const Biblioteca *b, float tempo)
{
#if LUZ_SALAS
    gradePronta = false;
    if (!SalaComLuz(b))
        return;

    for (int l = 0; l < LUZ_LINHAS; l++)
        for (int c = 0; c < LUZ_COLUNAS; c++)
            grade[l][c] = (LuzPonto){ 0.0f, 0.0f, 0.0f };
    JuntarLuzes(b, tempo);
    gradePronta = true;

    DesenharGrade((LuzPonto){ 0.0f, 0.0f, 0.0f }, LUZ_BRILHO_CHAO, BLEND_ADDITIVE);
#else
    (void)b; (void)tempo;
#endif
}

#if LUZ_SALAS

// Luz cheia num retângulo (a penumbra não escurece o que está dentro)
static void AcenderRetangulo(Rectangle r, LuzPonto base)
{
    float cw = AREA_LUZ.width / (LUZ_COLUNAS - 1);
    float ch = AREA_LUZ.height / (LUZ_LINHAS - 1);

    for (int l = 0; l < LUZ_LINHAS; l++) {
        float y = AREA_LUZ.y + l * ch;
        if (y < r.y - ch * 0.5f || y > r.y + r.height + ch * 0.5f) continue;
        for (int c = 0; c < LUZ_COLUNAS; c++) {
            float x = AREA_LUZ.x + c * cw;
            if (x < r.x - cw * 0.5f || x > r.x + r.width + cw * 0.5f) continue;
            grade[l][c].r = fmaxf(grade[l][c].r, 1.0f - base.r);
            grade[l][c].g = fmaxf(grade[l][c].g, 1.0f - base.g);
            grade[l][c].b = fmaxf(grade[l][c].b, 1.0f - base.b);
        }
    }
}

// As placas das portas (ícone, nome da sala e recompensa) ficam sempre legíveis, mesmo com a
// porta trancada. As mesmas posições e medidas da DesenharPortaMissao (desenho_hud.c).
static void AcenderPlacasDasPortas(const Biblioteca *b, LuzPonto base)
{
    for (int k = 0; k < b->numPortas; k++) {
        const PortaMissao *p = &b->portas[k];
        const NoMapa *n = &b->mapa.no[p->destino];
        Vector2 c = { p->r.x + p->r.width / 2, p->r.y + p->r.height / 2 };

        if (p->lado == LADO_CIMA)          c.y += 34;
        else if (p->lado == LADO_ESQUERDA) c.x += 36;
        else                               c.x -= 36;

        AcenderRetangulo((Rectangle){ c.x - 18, c.y - 18, 36, 36 }, base);      // o ícone

        const char *nome = RoguePlacaNo(n);
        const char *rec = NOMES_RECOMPENSA[n->recompensa];
        int w1 = MeasureText(nome, 10), w2 = MeasureText(rec, 10);
        int linha = LinhaTexto(12);
        float x1, x2, y1;

        if (p->lado == LADO_CIMA)          { x1 = c.x - w1 / 2;  x2 = c.x - w2 / 2;  y1 = c.y + 21; }
        else if (p->lado == LADO_ESQUERDA) { x1 = c.x + 22;      x2 = c.x + 22;      y1 = c.y - 10; }
        else                               { x1 = c.x - 22 - w1; x2 = c.x - 22 - w2; y1 = c.y - 10; }

        AcenderRetangulo((Rectangle){ x1 - 3, y1 - 1, (float)w1 + 6, (float)linha + 1 }, base);
        if (rec[0])
            AcenderRetangulo((Rectangle){ x2 - 3, y1 + linha - 1, (float)w2 + 6, (float)linha + 1 }, base);
    }
}

#endif // LUZ_SALAS

// Penumbra da sala (chamar dentro do BeginMode2D, depois dos personagens e antes dos tiros)
void RogueDesenharLuz(const Biblioteca *b, float tempo)
{
#if LUZ_SALAS
    if (!SalaComLuz(b) || !gradePronta)
        return;

    Color amb = AmbienteDaSala(b);
    LuzPonto base = { 1.0f - (1.0f - amb.r / 255.0f) * LUZ_FORCA_PENUMBRA,
                      1.0f - (1.0f - amb.g / 255.0f) * LUZ_FORCA_PENUMBRA,
                      1.0f - (1.0f - amb.b / 255.0f) * LUZ_FORCA_PENUMBRA };
    AcenderPlacasDasPortas(b, base);      // (o chão já foi desenhado: mexer na grade agora só afeta a penumbra)
    DesenharGrade(base, 1.0f, BLEND_MULTIPLIED);
    (void)tempo;
#else
    (void)b; (void)tempo;
#endif
}

// ---------- Poeira no ar ----------
// Grãozinhos que flutuam devagar pela sala (um ou dois pixels). Cada grão tem um caminho fixo,
// tirado do próprio número: não guarda estado nenhum e é igual em todo quadro.

#if AMBIENTE_POEIRA

#define POEIRA_BORDA  24.0f             // perto da borda o grão some (ele dá a volta para o outro lado)

typedef struct { Vector2 pos; float brilho; int tam; } GraoPoeira;

static GraoPoeira PosicaoGrao(int i, Rectangle area, float tempo)
{
    unsigned int h1 = Ruido((unsigned int)i * 7919u + 101u), h2 = Ruido(h1 + 3u), h3 = Ruido(h2 + 5u);
    float fx = (float)(h1 % 10000u) / 10000.0f, fy = (float)(h2 % 10000u) / 10000.0f;
    float vx = 2.0f + (float)(h3 % 100u) * 0.05f;               // 2 a 7 pixels por segundo
    float vy = -1.0f - (float)((h3 >> 8) % 100u) * 0.02f;       // e sobe bem pouco
    float fase = (float)((h3 >> 16) % 628u) / 100.0f;

    float x = fmodf(fx * area.width + tempo * vx + sinf(tempo * 0.55f + fase) * 9.0f, area.width);
    float y = fmodf(fy * area.height + tempo * vy + sinf(tempo * 0.8f + fase * 1.7f) * 6.0f, area.height);
    if (x < 0.0f) x += area.width;
    if (y < 0.0f) y += area.height;

    float borda = fminf(fminf(x, area.width - x), fminf(y, area.height - y)) / POEIRA_BORDA;
    GraoPoeira g;
    g.pos = (Vector2){ area.x + x, area.y + y };
    g.brilho = fminf(1.0f, borda) * (0.6f + 0.4f * sinf(tempo * (0.7f + (float)(h1 >> 24) * 0.004f) + fase));
    g.tam = ((h2 >> 20) % 3u == 0u) ? 2 : 1;
    return g;
}

#endif // AMBIENTE_POEIRA

// Poeira sem luz própria (salas do Bloco 1, átrio...): forca = o quanto aparece (0 a 1)
void DesenharPoeiraAr(Rectangle area, float tempo, float forca)
{
#if AMBIENTE_POEIRA
    if (!Prefs()->luzSalas || forca <= 0.0f)
        return;

    int n = (int)(area.width * area.height / 4300.0f);          // a mesma densidade em qualquer sala
    BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 0; i < n; i++) {
        GraoPoeira g = PosicaoGrao(i, area, tempo);
        DrawRectangle((int)g.pos.x, (int)g.pos.y, g.tam, g.tam, Fade((Color){ 200, 215, 255, 255 }, 0.42f * forca * g.brilho));
    }
    EndBlendMode();
#else
    (void)area; (void)tempo; (void)forca;
#endif
}

// Poeira das salas da Biblioteca: com a luz da sala, ela brilha (na cor da luz) perto do Thomas,
// dos tiros e das portas e quase some na penumbra. Chamar logo depois da RogueDesenharLuz.
void RogueDesenharPoeira(const Biblioteca *b, float tempo)
{
#if AMBIENTE_POEIRA
    if (RogueNoBloco3(b) || b->tipoSala == SALA_LUCI || b->tipoSala == SALA_TRANSICAO)
        return;                                                 // essas já têm o próprio clima
#if LUZ_SALAS
    if (!SalaComLuz(b) || !gradePronta) {
        DesenharPoeiraAr((Rectangle){ 40, 60, 720, 470 }, tempo, 0.8f);
        return;
    }

    const Rectangle arena = { 40, 60, 720, 470 };
    float cw = AREA_LUZ.width / (LUZ_COLUNAS - 1);
    float ch = AREA_LUZ.height / (LUZ_LINHAS - 1);
    int n = (int)(arena.width * arena.height / 3000.0f);

    BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 0; i < n; i++) {
        GraoPoeira g = PosicaoGrao(i, arena, tempo);
        int c = (int)((g.pos.x - AREA_LUZ.x) / cw + 0.5f), l = (int)((g.pos.y - AREA_LUZ.y) / ch + 0.5f);
        if (c < 0 || l < 0 || c >= LUZ_COLUNAS || l >= LUZ_LINHAS)
            continue;

        LuzPonto luz = grade[l][c];
        float forte = fminf(1.0f, (luz.r + luz.g + luz.b) / 3.0f);
        float alfa = (0.06f + 0.75f * forte) * g.brilho;
        Color cor = { Canal(0.45f + luz.r * 0.7f), Canal(0.45f + luz.g * 0.7f), Canal(0.5f + luz.b * 0.7f), 255 };
        DrawRectangle((int)g.pos.x, (int)g.pos.y, g.tam, g.tam, Fade(cor, alfa));
    }
    EndBlendMode();
#else
    DesenharPoeiraAr((Rectangle){ 40, 60, 720, 470 }, tempo, 0.8f);
#endif
#else
    (void)b; (void)tempo;
#endif
}
