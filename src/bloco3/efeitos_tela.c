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

// ---------- Reformulação do Bloco 3: efeitos de tela ----------

// Textos corrompidos que vazam no topo da tela ("data bleed")
static const char *TEXTOS_CORROMPIDOS[] = {
    "RESPOSTA SEM FONTE", "VERDADE NAO ENCONTRADA", "LUCI.exe: CONFIANCA 100%", "CONTEXTO PERDIDO",
    "REESCREVENDO MEMORIA", "ERRO 0xUT0P1A", "NAO CONFIE. CONFIRA.", "RESPONDER SEMPRE. MESMO SEM SABER."
};

// Copia o texto trocando algumas letras por lixo de dados (só ASCII, para não quebrar acentos)
void CorromperTexto(const char *origem, char *saida, int tam, unsigned int semente, int chance)
{
    static const char lixo[] = "#%&01/\\<>_";
    int i = 0;
    for (; origem[i] != '\0' && i < tam - 1; i++) {
        unsigned char c = (unsigned char)origem[i];
        bool trocar = (c < 128 && c != ' ' && Ruido(semente * 131u + (unsigned int)i) % 100 < (unsigned int)chance);
        saida[i] = trocar ? lixo[Ruido(semente + (unsigned int)i * 7u) % (sizeof(lixo) - 1)] : (char)c;
    }
    saida[i] = '\0';
}

// Texto com os canais vermelho e azul separados (aberração cromática na interface)
static void DesenharTextoAberracao(const char *texto, int x, int y, int tam, float deslocamento, Color cor)
{
    int d = (int)deslocamento;
    BeginBlendMode(BLEND_ADDITIVE);
    DrawText(texto, x - d, y, tam, (Color){ 255, 0, 60, 200 });
    DrawText(texto, x + d, y + (d > 2 ? 1 : 0), tam, (Color){ 0, 120, 255, 200 });
    EndBlendMode();
    DrawText(texto, x, y, tam, cor);
}

// Quanto a tela deve "sangrar" em aberração cromática agora (0 = nada)
float RogueIntensidadeAberracao(const Biblioteca *b)
{
    float k = 0.0f;

    if (b->resultadoAberto)
        return 0.0f;

    // Corredor do núcleo: cresce conforme o jogador caminha até a porta
    if (b->tipoSala == SALA_TRANSICAO)
        k = 0.2f + 0.8f * Clamp((SPAWN_ROGUE.y - b->j.pos.y) / (SPAWN_ROGUE.y - 80.0f), 0.0f, 1.0f);
    else if (b->tipoSala == SALA_LUCI && b->luci.ativo) {
        if (b->luci.emRegeneracao)       k = 0.9f;
        else if (b->luci.faseOculta == 3) k = 0.3f;
        else if (b->luci.faseOculta == 2) k = 0.12f;
    }

    // Focus Mode do Silogista: uma aberração leve enquanto o foco está ativo
    if (b->tipoSala == SALA_SILOGISTA && b->silogista.modoBH && b->silogista.bh.modoFocoAtivo) k = fmaxf(k, 0.22f);

    if (b->transicaoNucleo > 0.0f) k = fmaxf(k, 1.2f);
    if (b->transicaoMestre > 0.0f) k = fmaxf(k, 0.9f);
    if (b->estaticaTela > 0.0f)    k = fmaxf(k, fminf(1.0f, b->estaticaTela));
    return k;
}

// Pós-efeito: desloca os canais R e B do quadro inteiro em ondas senoidais que aceleram
static RenderTexture2D alvoAberracao = { 0 };

void AplicarAberracaoCromatica(RenderTexture2D alvo, float k, float tempo)
{
    if (alvoAberracao.id == 0)
        alvoAberracao = LoadRenderTexture(alvo.texture.width, alvo.texture.height);

    float dx = k * 5.0f * sinf(tempo * (3.0f + 9.0f * k));
    float dy = k * 1.5f * cosf(tempo * 7.0f);
    Rectangle origem = { 0, 0, (float)alvo.texture.width, -(float)alvo.texture.height };

    BeginTextureMode(alvoAberracao);
    ClearBackground(BLACK);
    BeginBlendMode(BLEND_ADDITIVE);
    DrawTextureRec(alvo.texture, origem, (Vector2){ dx, dy }, (Color){ 255, 0, 0, 255 });
    DrawTextureRec(alvo.texture, origem, (Vector2){ 0, 0 }, (Color){ 0, 255, 0, 255 });
    DrawTextureRec(alvo.texture, origem, (Vector2){ -dx, -dy }, (Color){ 0, 0, 255, 255 });
    EndBlendMode();
    EndTextureMode();

    BeginTextureMode(alvo);
    DrawTextureRec(alvoAberracao.texture, origem, (Vector2){ 0, 0 }, WHITE);
    EndTextureMode();
}

void DescarregarAberracao(void)
{
    if (alvoAberracao.id > 0) UnloadRenderTexture(alvoAberracao);
    alvoAberracao = (RenderTexture2D){ 0 };
}

// A porta do núcleo no átrio: aura magenta com glitch (e a animação de abertura depois do Alucinador)
void DesenharPortaNucleoAtrio(const Biblioteca *b, float t)
{
    Rectangle r = PORTA_BLOCO3_ROGUE;
    float abrindo = (b->portaNucleoAbrindo > 0.0f) ? 1.0f - b->portaNucleoAbrindo / 3.0f : 1.0f;
    float h = r.height * fmaxf(0.08f, abrindo);
    Rectangle porta = { r.x, r.y + (r.height - h) / 2.0f, r.width, h };

    DrawRectangle((int)r.x - 22, (int)r.y - 12, 36, (int)r.height + 24, Fade(MAGENTA, 0.10f + 0.08f * sinf(t * 5.0f)));
    for (int i = 0; i < 5; i++) {
        unsigned int q = Ruido((unsigned int)(t * 9.0f) * 17u + (unsigned int)i);
        int y = (int)r.y + (int)(q % (unsigned int)r.height);
        DrawRectangle((int)r.x - 8 - (int)(q % 26), y, 8 + (int)((q >> 5) % 18), 2, Fade(MAGENTA, 0.55f));
    }
    DesenharPortaRogue(porta, true, "", t);
    DrawText("BLOCO 3", (int)r.x - MeasureText("BLOCO 3", 10) - 4, (int)r.y + 30, 10, MAGENTA);
    DrawText("NÚCLEO", (int)r.x - MeasureText("NÚCLEO", 10) - 4, (int)r.y + 42, 10, Fade(MAGENTA, 0.6f + 0.4f * sinf(t * 7.0f)));
}

// 3ª barra: o chão fragmentado e as ondas de choque (com a brecha segura)
void DesenharNucleoInstavel(const Biblioteca *b, float t)
{
    const LuciChefe *l = &b->luci;

    if (!l->ativo || l->faseOculta != 3 || l->morrendo > 0.0f || l->emRegeneracao)
        return;

    bool pisca = ((int)(t * 10.0f)) % 2 == 0;

    for (int k = 0; k < NUM_PISO_LUCI; k++) {
        if (l->fasePiso[k] < 0) continue;
        Rectangle r = l->piso[k];

        if (l->fasePiso[k] == 0) {
            DrawRectangleLinesEx(r, 1, Fade(MAGENTA, 0.22f));
        } else if (l->fasePiso[k] == 1) {
            // Rachando (aviso): vermelho piscando e as rachaduras
            DrawRectangleRec(r, Fade(RED, pisca ? 0.25f : 0.10f));
            DrawRectangleLinesEx(r, 2, RED);
            Vector2 c = CentroRetangulo(r);
            for (int s = 0; s < 6; s++) {
                unsigned int q = Ruido((unsigned int)(k * 41 + s * 7));
                Vector2 p = { r.x + (float)(q % (unsigned int)r.width), r.y + (float)((q >> 8) % (unsigned int)r.height) };
                DrawLineEx(c, p, 1.5f, Fade(RED, 0.8f));
            }
        } else {
            // Caiu: um buraco de dados
            DrawRectangleRec(r, (Color){ 4, 0, 10, 255 });
            for (int s = 0; s < 4; s++) {
                float y = r.y + fmodf(t * 40.0f + (float)s * r.height / 4.0f, r.height);
                DrawLine((int)r.x + 4, (int)y, (int)(r.x + r.width) - 4, (int)y, Fade(MAGENTA, 0.25f));
            }
            DrawRectangleLinesEx(r, 2, Fade(MAGENTA, 0.6f));
        }
    }

    float brecha = l->ondaBrecha * RAD2DEG;

    // Aviso da onda: ela brilha e a brecha aparece em dourado antes
    if (l->padrao3 == 1 && !l->ondaAtiva) {
        float k = 1.0f - l->avisoOnda / LUCI_ONDA_AVISO;
        DrawCircleV(l->pos, LUCI_RAIO + 10 + 30 * k, Fade(WHITE, 0.15f + 0.2f * k));
        DrawCircleSector(l->pos, 140.0f, brecha - LUCI_ONDA_BRECHA, brecha + LUCI_ONDA_BRECHA, 16, Fade(GOLD, 0.22f));
        DrawRing(l->pos, 60.0f + 80.0f * k, 64.0f + 80.0f * k, brecha + LUCI_ONDA_BRECHA, brecha + 360.0f - LUCI_ONDA_BRECHA, 48, Fade(RED, 0.5f));
        Vector2 placa = { l->pos.x + cosf(l->ondaBrecha) * 150.0f, l->pos.y + sinf(l->ondaBrecha) * 150.0f };
        DrawText("BRECHA", (int)placa.x - MeasureText("BRECHA", 10) / 2, (int)placa.y - 5, 10, GOLD);
    }

    if (l->ondaAtiva) {
        DrawRing(l->ondaCentro, l->ondaRaio - 9, l->ondaRaio + 9, brecha + LUCI_ONDA_BRECHA, brecha + 360.0f - LUCI_ONDA_BRECHA, 96, Fade(MAGENTA, 0.85f));
        DrawRing(l->ondaCentro, l->ondaRaio - 3, l->ondaRaio + 3, brecha + LUCI_ONDA_BRECHA, brecha + 360.0f - LUCI_ONDA_BRECHA, 96, Fade(WHITE, 0.8f));
        for (int s = -1; s <= 1; s += 2) {
            float a = l->ondaBrecha + (float)s * LUCI_ONDA_BRECHA * DEG2RAD;
            DrawCircleV((Vector2){ l->ondaCentro.x + cosf(a) * l->ondaRaio, l->ondaCentro.y + sinf(a) * l->ondaRaio }, 4, GOLD);
        }
    }
}


// Etapa 3: anel dourado que fecha em volta do Thomas no ritmo do golpe da investida (a hora do PARRY)
void DesenharGuiaParry(const Biblioteca *b, float t)
{
    const LuciChefe *l = &b->luci;
    const JogadorRogue *j = &b->j;
    if (!l->ativo || !l->apresentado || l->atordoada || l->emRegeneracao || l->morrendo > 0.0f) return;
    if (l->estadoMelee != 1 && !(l->estadoMelee == 2 && !l->acertouInvestida)) return;

    Vector2 rel = Vector2Subtract(j->pos, l->pos);
    float ao = Vector2DotProduct(rel, l->dirMelee);
    float lado = fabsf(rel.x * l->dirMelee.y - rel.y * l->dirMelee.x);
    float alcance = LUCI_INVESTIDA_VEL * LUCI_INVESTIDA_DURACAO + 24.0f + LUCI_LAMINA_RAIO + RAIO_CORPO_JOGADOR;
    if (ao <= 0.0f || ao > alcance || lado > LUCI_LAMINA_RAIO + RAIO_CORPO_JOGADOR) return;   // fora do caminho dela

    float percurso = fmaxf(0.0f, ao - 24.0f - LUCI_LAMINA_RAIO - RAIO_CORPO_JOGADOR);
    float falta = percurso / LUCI_INVESTIDA_VEL + ((l->estadoMelee == 1) ? l->tempoMelee : 0.0f);
    if (falta > 0.6f) return;

    float k = falta / 0.6f;                                   // 1 = longe, 0 = impacto
    bool agora = falta <= PARRY_JANELA;
    float r = RAIO_CORPO_JOGADOR + 6.0f + 60.0f * k;
    DrawRing(j->pos, r - 2.0f, r + 1.0f, 0.0f, 360.0f, 40, Fade(GOLD, 0.35f + 0.6f * (1.0f - k)));
    DrawCircleLines((int)j->pos.x, (int)j->pos.y, RAIO_CORPO_JOGADOR + 6.0f, Fade(GOLD, 0.5f));
    if (agora) {
        DrawText("V!", (int)j->pos.x - MeasureText("V!", 16) / 2, (int)j->pos.y - 52, 16, GOLD);
        DrawCircleV(j->pos, RAIO_CORPO_JOGADOR + 8.0f, Fade(GOLD, 0.18f));
    }
    (void)t;
}

// Etapa 3: o "clima" da luta final, por cima da arena (sem flashes rápidos de tela cheia)
//   2ª barra: vinheta vermelha que pulsa devagar; 3ª barra: vinheta magenta que aperta conforme a vida
//   dela cai, linhas de varredura e faixas de falha. Guarda aberta: borda dourada (a janela de dano).
//   Nas quedas: a faixa "INTEGRIDADE NÃO ENCONTRADA" e o nome da forma nova enquanto a barra renasce.
void DesenharVinheta(Color cor, float a, float tam)
{
    if (a <= 0.0f) return;
    DrawRectangleGradientV(0, 0, LARGURA_TELA, (int)tam, Fade(cor, a), Fade(cor, 0.0f));
    DrawRectangleGradientV(0, ALTURA_TELA - (int)tam, LARGURA_TELA, (int)tam, Fade(cor, 0.0f), Fade(cor, a));
    DrawRectangleGradientH(0, 0, (int)tam, ALTURA_TELA, Fade(cor, a), Fade(cor, 0.0f));
    DrawRectangleGradientH(LARGURA_TELA - (int)tam, 0, (int)tam, ALTURA_TELA, Fade(cor, 0.0f), Fade(cor, a));
}

static void DesenharClimaLuci(const Biblioteca *b, float t)
{
    const LuciChefe *l = &b->luci;
    if (b->tipoSala != SALA_LUCI || !l->ativo || l->morrendo > 0.0f || b->cenaLuciTipo != 0) return;

    // Quedas: a faixa do erro e, renascendo, o nome da forma nova
    if (l->emRegeneracao) {
        char linha[96];
        const char *titulo;
        const char *sub;
        Color cor = (l->faseOculta == 1) ? RED : MAGENTA;
        float alfa;
        if (l->etapaRegeneracao == 0) {
            float total = (l->faseOculta == 1) ? LUCI_COLAPSO : LUCI_EXPLOSAO_ESTATICA;
            float q = 1.0f - l->tempoRegeneracao / total;
            alfa = Clamp(q * 3.0f, 0.0f, 1.0f);
            CorromperTexto("ERRO: INTEGRIDADE NAO ENCONTRADA", linha, sizeof(linha), (unsigned int)(t * 12.0f), 15 + (int)(30.0f * q));
            titulo = linha;
            sub = "reescrevendo...";
            cor = WHITE;
        } else {
            float q = 1.0f - l->tempoRegeneracao / LUCI_RENASCER;
            alfa = Clamp(q * 2.5f, 0.0f, 1.0f);
            titulo = (l->faseOculta == 1) ? "SURTO FÍSICO" : "NÚCLEO INSTÁVEL";
            sub = (l->faseOculta == 1) ? "uma barra vazia não prova nada" : "tudo o que ela sabe está se desfazendo";
        }
        DrawRectangle(0, 214, LARGURA_TELA, 82, Fade(BLACK, 0.6f * alfa));
        DrawRectangle(0, 214, LARGURA_TELA, 2, Fade(cor, 0.7f * alfa));
        DrawRectangle(0, 294, LARGURA_TELA, 2, Fade(cor, 0.7f * alfa));
        int tam = 30;
        DesenharTextoAberracao(titulo, LARGURA_TELA / 2 - MeasureText(titulo, tam) / 2, 230, tam, 3.0f, Fade(cor, alfa));
        DrawText(sub, LARGURA_TELA / 2 - MeasureText(sub, 12) / 2, 268, 12, Fade(LIGHTGRAY, alfa));
        DesenharVinheta(cor, 0.25f * alfa, 120.0f);
        return;
    }

    if (!l->apresentado) return;

    // Janela de dano: borda dourada (atordoada, orbe rebatido, superaquecida ou recuperando da investida)
    bool janela = l->faseOculta >= 2 && (l->atordoada || l->atordoadaOrbe > 0.0f || l->superaquecida > 0.0f);
    if (janela) DesenharVinheta(GOLD, 0.14f, 70.0f);

    if (l->faseOculta == 2) {
        DesenharVinheta((Color){ 200, 20, 40, 255 }, 0.16f + 0.06f * sinf(t * 2.0f), 110.0f);
    } else if (l->faseOculta == 3) {
        float aperto = 1.0f - Clamp(l->hp / fmaxf(1.0f, l->hpMax), 0.0f, 1.0f);   // quanto menos vida, mais tensa
        DesenharVinheta(MAGENTA, 0.16f + 0.16f * aperto + 0.05f * sinf(t * (2.0f + 2.0f * aperto)), 110.0f + 50.0f * aperto);
        for (int y = 0; y < ALTURA_TELA; y += 4)                 // linhas de varredura
            DrawRectangle(0, y, LARGURA_TELA, 1, Fade(BLACK, 0.06f + 0.04f * aperto));
        unsigned int quadro = (unsigned int)(t * 8.0f);          // faixas de falha (poucas e curtas)
        if (Ruido(quadro * 97u) % 100 < (unsigned int)(10 + 25 * aperto)) {
            unsigned int q = Ruido(quadro * 31u + 7u);
            int y = 60 + (int)(q % (unsigned int)(ALTURA_TELA - 120));
            int h = 2 + (int)((q >> 8) % 6);
            DrawRectangle(0, y, LARGURA_TELA, h, Fade(MAGENTA, 0.16f));
            DrawRectangle((int)((q >> 12) % 200), y + h, LARGURA_TELA / 2, 1, Fade(SKYBLUE, 0.25f));
        }
    }

    // Vida baixa do Thomas: pulso vermelho como batimento
    float vida = b->j.hp / fmaxf(1.0f, RogueHpMax(b));
    if (vida < 0.3f) {
        float bate = powf(fmaxf(0.0f, sinf(t * 2.0f * PI * 1.1f)), 6.0f);
        DesenharVinheta(RED, (0.12f + 0.20f * bate) * (1.0f - vida / 0.3f), 90.0f);
    }
}

// Clima dos outros chefes (no padrão da luta da Luci): borda dourada na janela de dano e a tensão das fases finais
static void DesenharClimaChefes(const Biblioteca *b, float t)
{
    const ChefeRogue *c = &b->chefe;
    if (c->ativo && c->apresentado && c->morrendo <= 0.0f) {
        if (c->fase == 2 && c->janela > 0.0f) DesenharVinheta(GOLD, 0.14f, 70.0f);
        if (c->surto >= 1) {
            bool colapso = (c->surto >= 2);
            DesenharVinheta(MAGENTA, (colapso ? 0.20f : 0.12f) + 0.05f * sinf(t * (colapso ? 4.0f : 2.0f)), colapso ? 140.0f : 100.0f);
            if (colapso) {
                for (int y = 0; y < ALTURA_TELA; y += 4)
                    DrawRectangle(0, y, LARGURA_TELA, 1, Fade(BLACK, 0.07f));
                unsigned int quadro = (unsigned int)(t * 8.0f);
                if (Ruido(quadro * 97u) % 100 < 22u) {
                    unsigned int q = Ruido(quadro * 31u + 7u);
                    int y = 60 + (int)(q % (unsigned int)(ALTURA_TELA - 120));
                    DrawRectangle(0, y, LARGURA_TELA, 2 + (int)((q >> 8) % 5), Fade(MAGENTA, 0.14f));
                }
            }
        }
    }
    if (b->janela.ativo) {
        if (b->janela.vulneravel > 0.0f) DesenharVinheta(GOLD, 0.14f, 70.0f);
        else if (b->janela.fase >= 4)    DesenharVinheta(RED, 0.12f + 0.05f * sinf(t * 3.0f), 100.0f);
    }
    const MestreTreino *mt = &b->mestre;
    if (mt->ativo && mt->visivel && mt->morrendo <= 0.0f) {
        if (mt->etapa >= MT_ABERTURA && MestreVulneravel(mt)) DesenharVinheta(GOLD, 0.14f, 70.0f);
        else if (mt->etapa == MT_LUTA && mt->fase == 2)      DesenharVinheta(MAGENTA, 0.12f + 0.05f * sinf(t * 2.5f), 100.0f);
    }
    const Silogista *s = &b->silogista;
    if (s->ativo && s->apresentado && s->morrendo <= 0.0f && s->etapa == SIL_VULNERAVEL)
        DesenharVinheta(PURPLE, 0.12f + 0.04f * sinf(t * 2.5f), 100.0f);
    for (int i = 0; i < MAX_INIMIGOS; i++) {
        const Inimigo *e = &b->ini[i];
        if (e->ativo && (e->tipo == INIMIGO_SENTINELA || e->tipo == INIMIGO_RETRO) && e->faseElite >= 2) {
            DesenharVinheta(DEF_INIMIGOS[e->tipo].cor, 0.10f + 0.04f * sinf(t * 2.5f), 90.0f);
            break;
        }
    }
}

// O Thomas no padrão da luta da Luci: bordas vermelhas no golpe e, com pouca vida, o batimento
// (na arena da Luci o batimento já vem do clima dela)
static void DesenharClimaJogador(const Biblioteca *b, float t)
{
    if (b->morreu) return;
    if (b->flashDanoJog > 0.0f)
        DesenharVinheta(RED, 0.30f * Clamp(b->flashDanoJog / 0.35f, 0.0f, 1.0f), 90.0f);
    if (b->tipoSala == SALA_LUCI || b->tipoSala == SALA_ATRIO) return;
    float vida = b->j.hp / fmaxf(1.0f, RogueHpMax(b));
    if (vida < 0.3f) {
        float bate = powf(fmaxf(0.0f, sinf(t * 2.0f * PI * 1.1f)), 6.0f);
        DesenharVinheta(RED, (0.10f + 0.18f * bate) * (1.0f - vida / 0.3f), 90.0f);
    }
}

// Por cima de tudo: textos flutuantes, erro de verificação, scanner, data bleed, estática e o flash negativo
void DesenharEfeitosTela(const Biblioteca *b, float t)
{
    DesenharClimaLuci(b, t);                // Etapa 3: o clima da luta final (por baixo dos textos)
    DesenharClimaChefes(b, t);              // o mesmo padrão nos outros chefes
    DesenharClimaJogador(b, t);             // golpe e vida baixa do Thomas

    // Textos que sobem (P -> Q, PARRY PERFEITO!, golpes do combo...)
    for (int i = 0; i < MAX_TEXTOS_FLUTUANTES; i++) {
        const TextoFlutuante *f = &b->textos[i];
        if (f->tempo <= 0.0f) continue;
        float k = fminf(1.0f, f->tempo / 0.4f);
        int w = MeasureText(f->texto, 14);
        DrawText(f->texto, (int)f->pos.x - w / 2 + 1, (int)f->pos.y + 1, 14, Fade(BLACK, 0.7f * k));
        DrawText(f->texto, (int)f->pos.x - w / 2, (int)f->pos.y, 14, Fade(f->cor, k));
    }

    // Erro na verificação: o termo falso com aberração cromática e a justificativa
    if (b->tempoErro > 0.0f) {
        float k = fminf(1.0f, b->tempoErro / 2.8f);
        float fade = fminf(1.0f, b->tempoErro / 0.4f);
        char termo[96];
        snprintf(termo, sizeof(termo), "ALUCINAÇÃO: %s", b->erroTermo);
        int w = MeasureText(termo, 18), w2 = MeasureText(b->erroExplicacao, 10);
        int largura = (w > w2 ? w : w2) + 30, y = 428;
        DrawRectangle(LARGURA_TELA / 2 - largura / 2, y - 6, largura, 44, Fade(BLACK, 0.75f * fade));
        DrawRectangleLines(LARGURA_TELA / 2 - largura / 2, y - 6, largura, 44, Fade(RED, 0.8f * fade));
        float d = 2.0f + 7.0f * k * k + (float)(((int)(t * 30.0f)) % 3);
        DesenharTextoAberracao(termo, LARGURA_TELA / 2 - w / 2, y, 18, d, Fade(WHITE, fade));
        DrawText(b->erroExplicacao, LARGURA_TELA / 2 - w2 / 2, y + 22, 10, Fade(LIGHTGRAY, fade));
    }

    // Scanner de Ecos: moldura ciano nas bordas e o tempo que falta (diferente do aviso de dano)
    if (b->scanner > 0.0f) {
        float k = Clamp(b->scanner / SCANNER_DURACAO, 0.0f, 1.0f);
        Color ciano = (Color){ 0, 228, 255, 255 };
        float a = 0.45f + 0.25f * sinf(t * 6.0f);
        DrawRectangleLinesEx((Rectangle){ 1, 1, LARGURA_TELA - 2, 536 }, 2, Fade(ciano, a));
        for (int c = 0; c < 4; c++) {
            float x = (c % 2) ? LARGURA_TELA - 4.0f : 4.0f, y = (c / 2) ? 533.0f : 4.0f;
            float sx = (c % 2) ? -1.0f : 1.0f, sy = (c / 2) ? -1.0f : 1.0f;
            DrawLineEx((Vector2){ x, y }, (Vector2){ x + sx * 26, y }, 4, ciano);
            DrawLineEx((Vector2){ x, y }, (Vector2){ x, y + sy * 26 }, 4, ciano);
        }
        DrawRectangle(0, 0, (int)(LARGURA_TELA * k), 3, ciano);
        DrawRectangle(0, 535, (int)(LARGURA_TELA * k), 3, ciano);
        const char *txt = TextFormat("SCANNER %.1f s", b->scanner);
        DrawText(txt, LARGURA_TELA - MeasureText(txt, 10) - 10, 6, 10, ciano);
    }

    // Data bleed: trechos corrompidos no topo (corredor do núcleo e quedas da Luci)
    bool bleed = b->tipoSala == SALA_TRANSICAO || (b->tipoSala == SALA_LUCI && b->luci.ativo && b->luci.emRegeneracao) || b->transicaoNucleo > 0.0f || b->transicaoMestre > 0.0f;
    if (bleed) {
        unsigned int quadro = (unsigned int)(t * 6.0f);
        for (int i = 0; i < 3; i++) {
            unsigned int q = Ruido(quadro * 7u + (unsigned int)i * 101u);
            if (q % 3 == 0) continue;
            char texto[64];
            CorromperTexto(TEXTOS_CORROMPIDOS[q % NUM_TEXTOS_CORROMPIDOS], texto, sizeof(texto), quadro + (unsigned int)i, 18);
            int x = 20 + (int)((q >> 4) % 560), y = 44 + (int)((q >> 12) % 14) + i * 2;
            DesenharTextoAberracao(texto, x, y, 10, 2.0f, Fade(MAGENTA, 0.85f));
        }
    }

    // Ponte Bloco 1 -> Biblioteca: duas frases sobre a estática (a Mestre foge / o que vem a seguir)
    if (b->transicaoMestre > 0.0f) {
        float k = b->transicaoMestre / TRANSICAO_MESTRE;            // 1 -> 0
        bool primeira = k > 0.5f;
        float local = primeira ? (1.0f - k) * 2.0f : (0.5f - k) * 2.0f;           // 0 -> 1 dentro de cada metade
        float alfa = fminf(1.0f, fminf(local * 4.0f, (1.0f - local) * 4.0f + 0.15f));
        const char *linha1 = primeira ? "A MESTRE FOGE PELA PORTA..." : "BLOCO 2";
        const char *linha2 = primeira ? "Fim do Bloco 1: você aprendeu a duvidar e a reagir." : "BIBLIOTECA DOS ECOS";
        DrawRectangle(0, ALTURA_TELA / 2 - 42, LARGURA_TELA, 84, Fade(BLACK, 0.55f * alfa));
        DrawText(linha1, LARGURA_TELA / 2 - MeasureText(linha1, 20) / 2, ALTURA_TELA / 2 - 26, 20, Fade(GOLD, alfa));
        DrawText(linha2, LARGURA_TELA / 2 - MeasureText(linha2, 10) / 2, ALTURA_TELA / 2 + 8, 10, Fade(WHITE, alfa));
    }

    // Estática: pontos e faixas
    if (b->estaticaTela > 0.0f) {
        float k = fminf(1.0f, b->estaticaTela);
        unsigned int quadro = (unsigned int)(t * 30.0f);
        int pontos = (int)(260.0f * k);
        for (int i = 0; i < pontos; i++) {
            unsigned int q = Ruido(quadro * 977u + (unsigned int)i);
            unsigned char c = (unsigned char)(120 + q % 136);
            DrawRectangle((int)(q % LARGURA_TELA), (int)((q >> 10) % ALTURA_TELA), 2 + (int)((q >> 20) % 3), 2, (Color){ c, c, c, (unsigned char)(200 * k) });
        }
        for (int i = 0; i < 6; i++) {
            unsigned int q = Ruido(quadro * 131u + (unsigned int)i * 17u);
            DrawRectangle(0, (int)(q % ALTURA_TELA), LARGURA_TELA, 1 + (int)((q >> 8) % 5), Fade((i % 2) ? WHITE : MAGENTA, 0.18f * k));
        }
    }

    // Inversão de paleta (negative flash) com faixas de ruído e o desbotamento magenta
    if (b->flashNegativo > 0.0f) {
        float k = fminf(1.0f, b->flashNegativo / 0.4f);
        // Uma inversão só, curta e sem piscar (tela cheia piscando rápido é risco para fotossensíveis)
        if (b->flashNegativo > 0.3f) {
            BeginBlendMode(BLEND_SUBTRACT_COLORS);      // branco - tela = negativo
            DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, WHITE);
            EndBlendMode();
            BeginBlendMode(BLEND_ADDITIVE);             // devolve o alfa (a subtração zera o alfa)
            DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, (Color){ 0, 0, 0, 255 });
            EndBlendMode();
        }
        unsigned int quadro = (unsigned int)(t * 20.0f);
        for (int i = 0; i < 8; i++) {
            unsigned int q = Ruido(quadro * 53u + (unsigned int)i * 29u);
            DrawRectangle(0, (int)(q % ALTURA_TELA), LARGURA_TELA, 2 + (int)((q >> 6) % 10), Fade(MAGENTA, 0.35f * k));
        }
        DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(MAGENTA, 0.22f * k));
    }

    // Cutscene da porta do núcleo
    if (b->transicaoNucleo > 0.0f) {
        float q = 1.0f - b->transicaoNucleo / TRANSICAO_NUCLEO;
        DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade((Color){ 20, 0, 30, 255 }, 0.85f * sinf(q * PI)));
        char texto[64];
        CorromperTexto("ACESSANDO O NUCLEO DE UTOPIA...", texto, sizeof(texto), (unsigned int)(t * 12.0f), (int)(10 + 40 * q));
        int w = MeasureText(texto, 22);
        DesenharTextoAberracao(texto, LARGURA_TELA / 2 - w / 2, 280, 22, 3.0f + 6.0f * q, Fade(MAGENTA, sinf(q * PI)));
    }
}

// ---------- Silogista bullet-hell: desenho ----------

// Os projéteis simbólicos (todos com o mesmo visual: é preciso LER a conclusão) e o glossário do foco
void DesenharSimbolosSilogistaBH(const Biblioteca *b, float t)
{
    const EstadoSilogista *e = &b->silogista.bh;
    Color lilas = { 170, 140, 255, 255 };
    int maisPerto = -1;
    float menor = 260.0f;

    for (int k = 0; k < MAX_PROJETEIS_SILOGISTA; k++) {
        const ProjetilSilogista *q = &e->projeteis[k];
        if (!q->ativo) continue;
        float d = Vector2Distance(q->posicao, b->j.pos);
        if (q->surgindo <= 0.0f && d < menor) { menor = d; maisPerto = k; }
    }

    for (int k = 0; k < MAX_PROJETEIS_SILOGISTA; k++) {
        const ProjetilSilogista *q = &e->projeteis[k];
        if (!q->ativo) continue;

        float alfa = (q->surgindo > 0.0f) ? 0.35f + 0.65f * (1.0f - q->surgindo / 0.6f) : 1.0f;
        int wt = MeasureText(q->textoSimbolo, 10);
        Rectangle r = { q->posicao.x - (float)(wt + 14) / 2.0f, q->posicao.y - 10, (float)(wt + 14), 20 };

        DrawCircleV(q->posicao, q->raio + 4 + 2.0f * sinf(t * 6.0f + (float)k), Fade(lilas, 0.16f * alfa));
        DrawRectangleRounded(r, 0.6f, 8, Fade((Color){ 30, 20, 60, 255 }, 0.92f * alfa));
        DrawRectangleRoundedLinesEx(r, 0.6f, 8, (k == maisPerto && e->modoFocoAtivo) ? 2.0f : 1.0f, Fade((k == maisPerto && e->modoFocoAtivo) ? GOLD : lilas, alfa));
        DrawText(q->textoSimbolo, (int)q->posicao.x - wt / 2, (int)q->posicao.y - 5, 10, Fade(WHITE, alfa));
        if (q->surgindo > 0.0f)
            DrawCircleLines((int)q->posicao.x, (int)q->posicao.y, q->raio + 10.0f * q->surgindo, Fade(lilas, 0.6f));
    }

    // Glossário do foco: o símbolo mais perto mostra o argumento completo
    if (e->modoFocoAtivo && maisPerto >= 0) {
        const ProjetilSilogista *q = &e->projeteis[maisPerto];
        const char *titulo = q->textoSimbolo;
        const char *texto = q->explicacaoPedagogica ? q->explicacaoPedagogica : "";
        int w = MeasureText(texto, 10);
        if (MeasureText(titulo, 12) > w) w = MeasureText(titulo, 12);
        w += 16;

        float bx = q->posicao.x + 26.0f, by = q->posicao.y - 40.0f;
        if (bx + (float)w > ArenaRogue().x + ArenaRogue().width - 4) bx = q->posicao.x - 26.0f - (float)w;
        by = Clamp(by, ArenaRogue().y + 4, ArenaRogue().y + ArenaRogue().height - 40);
        Rectangle caixa = { bx, by, (float)w, 34 };

        DrawLineEx(q->posicao, (Vector2){ (bx < q->posicao.x) ? bx + (float)w : bx, by + 17 }, 1, Fade(GOLD, 0.8f));
        DrawRectangleRec(caixa, Fade((Color){ 10, 20, 45, 255 }, 0.92f));
        DrawRectangleLinesEx(caixa, 1, (Color){ 102, 191, 255, 255 });
        DrawText(titulo, (int)bx + 8, (int)by + 4, 12, GOLD);
        DrawText(texto, (int)bx + 8, (int)by + 19, 10, WHITE);
    }
}

// Focus Mode (vinheta azul nas bordas) e o micro-feedback pedagógico no alto
void DesenharFocoSilogistaBH(const Biblioteca *b, float t)
{
    const EstadoSilogista *e = &b->silogista.bh;

    if (e->modoFocoAtivo) {
        Color azul = { 40, 110, 255, 255 };
        DrawRectangle(0, 0, LARGURA_TELA, 538, Fade(azul, 0.06f));
        DrawRectangleGradientV(0, 0, LARGURA_TELA, 80, Fade(azul, 0.38f), Fade(azul, 0.0f));
        DrawRectangleGradientV(0, 458, LARGURA_TELA, 80, Fade(azul, 0.0f), Fade(azul, 0.38f));
        DrawRectangleGradientH(0, 0, 80, 538, Fade(azul, 0.38f), Fade(azul, 0.0f));
        DrawRectangleGradientH(LARGURA_TELA - 80, 0, 80, 538, Fade(azul, 0.0f), Fade(azul, 0.38f));
        const char *rotulo = "FOCUS MODE - ANÁLISE LÓGICA ATIVA";
        DrawText(rotulo, LARGURA_TELA / 2 - MeasureText(rotulo, 14) / 2, 506, 14, Fade(SKYBLUE, 0.75f + 0.25f * sinf(t * 6.0f)));
    }

    if (e->tempoExibicaoFeedback > 0.0f) {
        // Título curto (o erro "corrompe" com aberração) e a justificativa sempre legível embaixo
        float k = fminf(1.0f, e->tempoExibicaoFeedback / 0.4f);
        bool erro = (e->corFeedback.r > 200 && e->corFeedback.g < 100);
        const char *titulo = erro ? "CORRUPÇÃO LÓGICA" : "REGRA VALIDADA";
        int wt = MeasureText(titulo, 12), wm = MeasureText(e->mensagemFeedback, 12);
        int w = ((wt > wm) ? wt : wm) + 30;
        Rectangle r = { (float)(LARGURA_TELA / 2 - w / 2), 166, (float)w, 40 };

        DrawRectangleRec(r, Fade(BLACK, 0.85f * k));
        DrawRectangleLinesEx(r, 2, Fade(e->corFeedback, k));
        if (erro) DesenharTextoAberracao(titulo, LARGURA_TELA / 2 - wt / 2, (int)r.y + 5, 12, 1.0f + 3.0f * (e->tempoExibicaoFeedback / 2.5f), Fade(e->corFeedback, k));
        else      DrawText(titulo, LARGURA_TELA / 2 - wt / 2, (int)r.y + 5, 12, Fade(e->corFeedback, k));
        DrawText(e->mensagemFeedback, LARGURA_TELA / 2 - wm / 2, (int)r.y + 22, 12, Fade(WHITE, k));
    }
}
