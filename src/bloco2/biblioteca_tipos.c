#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <time.h>
#include "../../include/core/fonte.h"
#include "../../include/core/mapas.h"
#include "../../include/bloco2/biblioteca_tipos.h"
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

const int PRECO_MELHORIA[NIVEL_MAX_MELHORIA] = { 10, 16, 24 };   // era { 8, 12, 16 }: tudo máximo em 2 missões
const int PESO_RARIDADE[NUM_RARIDADES] = { 60, 30, 10 };     // chance relativa de aparecer
const char *NOMES_RARIDADE[NUM_RARIDADES] = { "COMUM", "INCOMUM", "RARO" };
const Color CORES_RARIDADE[NUM_RARIDADES] = { { 200, 200, 200, 255 }, { 0, 228, 255, 255 }, { 255, 203, 0, 255 } };

// ---------- A Luci na Biblioteca (AJUSTE AQUI) ----------
// A estabilidade é a mesma do Bloco 1 (vai para o save). Pedir dica gasta 1;
// resolver sozinho, ler terminais, pegar a Luci alucinando e destruir ecos repara.
const int CHANCE_LUCI_ALUCINAR_B2[ESTABILIDADE_MAX_LUCI + 1] = { 80, 60, 40, 25, 10, 0 };   // % por estabilidade
const Rectangle ARENA_ROGUE_PADRAO = RET_ARENA_ROGUE_PADRAO;
Rectangle arenaRogueAtual = RET_ARENA_ROGUE_PADRAO;

Rectangle ArenaRogue(void)
{
    return arenaRogueAtual;
}

// O chão: da base da parede de cima (y 280) até a borda de baixo (992), entre as laterais (22 a 1004)
const Rectangle ARENA_LUCI = { LUCI_MX(22), LUCI_MY(280), LUCI_MT(982), LUCI_MT(712) };

Texture2D texMapaLuci = {0};          // sem o arquivo, a luta usa o chão simples (na mesma arena)

// Um ponto pensado para a arena de sempre, levado para a arena atual (mesma posição relativa).
// Nas salas de sempre devolve o próprio ponto.
Vector2 PontoNaArena(Vector2 p)
{
    Rectangle a = arenaRogueAtual, d = ARENA_ROGUE_PADRAO;

    if (a.x == d.x && a.y == d.y && a.width == d.width && a.height == d.height)
        return p;

    return (Vector2){ a.x + (p.x - d.x) * a.width / d.width, a.y + (p.y - d.y) * a.height / d.height };
}

Rectangle RetanguloNaArena(Rectangle r)
{
    Rectangle a = arenaRogueAtual, d = ARENA_ROGUE_PADRAO;
    Vector2 p = PontoNaArena((Vector2){ r.x, r.y });

    return (Rectangle){ p.x, p.y, r.width * a.width / d.width, r.height * a.height / d.height };
}

// Carrega a arte de um mapa já no tamanho que ela ocupa na tela (a imagem original é bem maior).
// Sem o arquivo, devolve uma textura vazia e o jogo usa o cenário simples.
static Texture2D CarregarMapaReduzido(const char *arquivo, int largura, int altura)
{
    if (!FileExists(arquivo)) {
        TraceLog(LOG_WARNING, "MAPA: '%s' nao encontrado (usando o cenario simples)", arquivo);
        return (Texture2D){0};
    }

    Image img = LoadImage(arquivo);

    if (img.data == NULL)
        return (Texture2D){0};

    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

    if (img.width != largura || img.height != altura)
        ImageResize(&img, largura, altura);

    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    return t;
}

// Mapas novos (corredor do Bloco 1 e arena final): carregados uma vez, depois do InitWindow
void CarregarMapasNovos(void)
{
    texMapaCorredor = CarregarMapaReduzido(ARQUIVO_MAPA_CORREDOR, (int)roundf(MAPA_CORREDOR_LARGURA), (int)roundf(MAPA_CORREDOR_ALTURA));
    texMapaLuci     = CarregarMapaReduzido(ARQUIVO_MAPA_LUCI, (int)MAPA_LUCI_LADO, (int)MAPA_LUCI_LADO);
}

void DescarregarMapasNovos(void)
{
    if (texMapaCorredor.id > 0) UnloadTexture(texMapaCorredor);
    if (texMapaLuci.id > 0)     UnloadTexture(texMapaLuci);

    texMapaCorredor = (Texture2D){0};
    texMapaLuci = (Texture2D){0};
}

const Rectangle PORTA_PROXIMA_ROGUE = { 370, 60, 60, 14 };
const Rectangle PORTA_SAIDA_ROGUE   = { 40, 260, 14, 70 };
const Rectangle PORTA_BLOCO3_ROGUE  = { 746, 160, 14, 70 };   // abre ao vencer o Alucinador
const Vector2   SPAWN_ROGUE         = { 400, 480 };   // entrando numa sala da missão
const Vector2   SPAWN_ATRIO         = { 140, 295 };   // chegando do corredor

// Lugares fixos das salas especiais
const Vector2 POS_BYTE      = { 300, 150 };
const Vector2 POS_BYTECO    = { 610, 298 };
const Vector2 POS_TERMINAL  = { 400, 270 };
const Vector2 POS_ENCICLOPEDIA = { 180, 432 };

// =============================================================
// TERMINAIS DE ATALHO E SELETOR DE CHEFES
// =============================================================
// Três terminais dourados ("ATALHO", com setas >> correndo até a porta a que se referem):
//   Corredor do Histórico (parede de baixo, perto da saída) -> lobby do Bloco 2, sem os quizzes,
//       a Alucinação Mestre e o tutorial de shooter (nada é marcado como concluído)
//   Átrio, ao lado da porta de cima -> SELETOR DE CHEFES (liberado depois de falar com o Professor Byte)
//   Átrio, ao lado da porta da direita -> Bloco 3 (rota alternativa: não marca o Alucinador como vencido)
// Os atalhos entre blocos pedem confirmação SIM / NÃO. O seletor leva direto à arena escolhida,
// com o personagem padrão (só o Disparador de Dados); vencendo ou perdendo, volta ao átrio.
// Nada disso vai para o save: o seletor e o confronto direto ficam só na Biblioteca (transitória).
const Vector2 POS_TERMINAL_SELETOR = { 532, 104 };      // à direita da porta de cima
const Vector2 POS_TERMINAL_BLOCO3  = { 722, 282 };      // abaixo da porta da direita (Bloco 3)

const char *TEXTO_ATALHO_BLOCO2 =
    "TERMINAL DE ATALHO // BLOCO 2\nEste atalho vai pular o conteúdo educativo do Bloco 1, incluindo os três quizzes, "
    "a Alucinação Mestre e o tutorial de shooter. Você será levado diretamente ao lobby do Bloco 2. Deseja continuar?";

const char *TEXTO_ATALHO_BLOCO3 =
    "TERMINAL DE ATALHO // BLOCO 3\nEste terminal permite pular o restante da Biblioteca dos Ecos e acessar o Bloco 3. "
    "O Alucinador NÃO conta como derrotado. Você deseja continuar?";

const char *NOMES_CHEFES_SELETOR[NUM_CHEFES_SELETOR] = {
    "SENTINELA DE ATENÇÃO", "RETROPROPAGADOR", "JANELA DE CONTEXTO", "O SILOGISTA", "ALUCINADOR"
};
const char *TIPOS_CHEFES_SELETOR[NUM_CHEFES_SELETOR] = { "ELITE", "ELITE", "MINI-CHEFE", "GUARDIÃO", "CHEFE FINAL" };
const char *DESCRICOES_CHEFES_SELETOR[NUM_CHEFES_SELETOR] = {
    "Só ataca o que vê. Fuja da visão atrás das estantes e ataque quando ela piscar.",
    "Guarda de onde veio cada golpe e devolve tudo. Atire e mude de lugar.",
    "Encha a janela de contexto com ruído até a instrução PROTEGER ser esquecida.",
    NULL,   // depende do modo do Silogista (ver RogueDescricaoChefeSeletor)
    "O chefe do Bloco 2. Desvie das alucinações e julgue as afirmações para abrir o escudo."
};
const Color CORES_CHEFES_SELETOR[NUM_CHEFES_SELETOR] = {
    { 140, 230, 255, 255 }, { 255, 110, 50, 255 }, { 150, 140, 255, 255 }, { 120, 110, 255, 255 }, { 230, 41, 255, 255 }
};
const Vector2 POS_PEDESTAIS[NUM_PEDESTAIS] = { { 270, 290 }, { 400, 260 }, { 530, 290 } };

const int   JANELA_CAPACIDADE[4]  = { 5, 6, 7, 8 };
const float JANELA_EMISSAO[4]     = { 2.4f, 2.0f, 1.8f, 1.5f };   // um dado dourado (dois na fase 3)
const float JANELA_RESPOSTA[4]    = { 3.0f, 2.6f, 2.2f, 1.7f };
const float JANELA_RESUMO[4]      = { 99.0f, 6.0f, 5.0f, 3.8f };  // a partir da fase 2 ela resume o ruído
const float TOKEN_VELOCIDADE[4]   = { 70.0f, 85.0f, 95.0f, 105.0f };
const char *NOMES_ESPECIAIS[NUM_ESPECIAIS]  = { "SENTINELA DE ATENÇÃO", "RETROPROPAGADOR", "JANELA DE CONTEXTO" };
static const char *PLACAS_ESPECIAIS[NUM_ESPECIAIS] = { "ELITE: SENTINELA", "ELITE: RETROPROPAGADOR", "MINI-CHEFE: JANELA" };
const char *FALAS_LUCI_ESPECIAL[NUM_ESPECIAIS] = {
    "Luci: Ela só ataca o que consegue VER. Use as estantes!",
    "Luci: Ele guarda de onde veio cada golpe. Atire e mude de lugar!",
    "Luci: Esse aí só lembra do que cabe na janela dele...",
};

// ECONOMIA (nov/2026): as chances de FC dos inimigos comuns caíram ~30% (elites e mini-chefe continuam 100%).
// Uma missão completa rendia ~80 FC e as melhorias permanentes custavam 144 no total: tudo comprado em 2 missões.
// Agora ~65 FC por missão e 200 nas melhorias permanentes: ~3 missões, disputando com a loja.
const DefInimigo DEF_INIMIGOS[NUM_TIPOS_INIMIGO] = {
    //  nome                   hp     raio   vel    contato  empurrão pts  FC   cor                        som
    { "FRAGMENTO ALUCINADO",   FRAGMENTO_HP, FRAGMENTO_RAIO, FRAGMENTO_VELOCIDADE, FRAGMENTO_DANO, 1.0f, PONTOS_FRAGMENTO, CHANCE_FC_FRAGMENTO, { 255, 0, 255, 255 },  1.2f },
    { "GUARDIÃO DE DADOS",     GUARDIAO_HP, GUARDIAO_RAIO, GUARDIAO_VELOCIDADE, 0.0f, 1.0f, PONTOS_GUARDIAO, CHANCE_FC_GUARDIAO, { 102, 191, 255, 255 }, 0.8f },
    { "ECO FALSO",              ECO_HP, 12.0f, 0.0f, 0.0f, 1.0f, 0, 0, { 255, 0, 255, 255 },   1.2f },
    { "SPAMMER",               14.0f, 10.0f, 165.0f,  6.0f,  1.2f,    1,  10, { 255, 120, 60, 255 },  1.7f },
    { "SPAM",                   3.0f,  6.0f, 190.0f,  3.0f,  1.5f,    0,   0, { 255, 160, 90, 255 },  2.0f },
    { "PHISHER",               40.0f, 15.0f,  60.0f,  0.0f,  1.0f,    3,  45, { 255, 203, 0, 255 },   0.9f },
    { "CÓPIA FALSA",            1.0f, 15.0f,  60.0f,  0.0f,  1.0f,    0,   0, { 255, 203, 0, 255 },   1.8f },
    { "FIREWALL",              90.0f, 20.0f,  45.0f,  8.0f,  0.25f,   4,  55, { 0, 158, 47, 255 },    0.6f },
    { "OVERCLOCK",             45.0f, 16.0f,  70.0f,  8.0f,  0.8f,    3,  45, { 255, 161, 0, 255 },   1.0f },
    { "BOTNET",                35.0f, 13.0f,  90.0f,  0.0f,  1.0f,    4,  50, { 0, 228, 255, 255 },   1.4f },
    { "KEYLOGGER",             30.0f, 14.0f,  95.0f,  0.0f,  1.0f,    3,  45, { 200, 200, 200, 255 }, 1.1f },
    { "CACHE",                 70.0f, 18.0f,  50.0f,  8.0f,  0.5f,    3,  50, { 0, 228, 48, 255 },    0.7f },
    { "NÚCLEO DE CORRUPÇÃO",  130.0f, 22.0f,   0.0f,  0.0f,  0.0f,    4,  70, { 230, 41, 255, 255 },  0.5f },
    { "SENTINELA DE ATENÇÃO", SENTINELA_HP, SENTINELA_RAIO, SENTINELA_VELOCIDADE, 8.0f, 0.4f, 12, 100, { 140, 230, 255, 255 }, 0.6f },
    { "RETROPROPAGADOR",      RETRO_HP, RETRO_RAIO, RETRO_VELOCIDADE, 10.0f, 0.3f, 14, 100, { 255, 110, 50, 255 }, 0.55f },
    { "JANELA DE CONTEXTO",   JANELA_HP, JANELA_RAIO, 45.0f, 10.0f, 0.0f, 30, 100, { 150, 140, 255, 255 }, 0.4f },
    { "DADO DOURADO",           1.0f, TOKEN_RAIO, 80.0f, 0.0f, 0.0f, 0, 0, { 255, 203, 0, 255 }, 2.0f },
    { "ALUCINAÇÃO",            40.0f, 15.0f,  60.0f,  6.0f,  1.0f,    5,  50, { 255, 60, 200, 255 },  1.5f },
    { "VERBOSO",              110.0f, 22.0f,  32.0f, 10.0f,  0.4f,    8,  55, { 120, 220, 255, 255 }, 0.6f },
};

const DefItem ITENS[NUM_ITENS] = {
    { "ANTIVÍRUS",              "Recupera 40 de HP (tecla 1).",                  3, false, false, 99, RARIDADE_COMUM },
    { "PACOTE DE DADOS",        "Recupera 50 de EP (tecla 2).",                  2, false, false, 99, RARIDADE_COMUM },
    { "LENTE DA VERIFICAÇÃO",   "Dá uma dica num desafio educativo.",            4, false, false, 99, RARIDADE_COMUM },
    { "FIREWALL",               "Reduz em 25% o dano recebido.",                 8, true,  false, MAX_PILHA_PASSIVO, RARIDADE_INCOMUM },
    { "PROCESSADOR NEURAL",     "Aumenta em 25% o dano dos disparos.",           9, true,  false, MAX_PILHA_PASSIVO, RARIDADE_INCOMUM },
    { "MANUAL DO CONHECIMENTO", "Segunda tentativa num desafio educativo.",      5, false, false, 99, RARIDADE_INCOMUM },

    { "OVERCLOCK",              "8 s: +15% de velocidade e +20% de cadência.",   0, false, true, 1, RARIDADE_COMUM },
    { "FIREWALL PORTÁTIL",      "Escudo que absorve o próximo golpe (12 s).",    0, false, true, 1, RARIDADE_INCOMUM },
    { "LIMPEZA DE CACHE",       "Apaga as balas inimigas e atordoa quem está perto.", 0, false, true, 1, RARIDADE_INCOMUM },
    { "PROXY-ISCA",             "Holograma que atrai os perseguidores por 5 s.", 0, false, true, 1, RARIDADE_RARA },
    { "PACOTE DE INTEGRIDADE",  "Recupera 25% da vida máxima.",                  0, false, true, 1, RARIDADE_COMUM },
    { "CAMPO DE LATÊNCIA",      "5 s: inimigos perto de você 30% mais lentos.",  0, false, true, 1, RARIDADE_INCOMUM },

    { "NÚCLEO DE INTEGRIDADE",  "+10% de vida máxima.",                          0, true, false, 3, RARIDADE_COMUM },
    { "BUFFER OTIMIZADO",       "+10% de cadência em todas as armas.",           0, true, false, 2, RARIDADE_COMUM },
    { "ROTEAMENTO DINÂMICO",    "+8% de velocidade e esquiva 8% mais rápida.",   0, true, false, 2, RARIDADE_COMUM },
    { "FRAGMENTAÇÃO DE PACOTES","A cada 6 disparos, o acerto se divide em 3.",   0, true, false, 1, RARIDADE_RARA },
    { "FIREWALL ADAPTATIVO",    "-8% de dano recebido (até -24%).",              0, true, false, 3, RARIDADE_COMUM },
    { "ANALISADOR HEURÍSTICO",  "3 acertos no mesmo alvo: o próximo dá +25%.",   0, true, false, 1, RARIDADE_INCOMUM },
    { "BOTNET REVERSO",         "Inimigo derrotado pode soltar um pacote teleguiado.", 0, true, false, 1, RARIDADE_RARA },
    { "CACHE PERSISTENTE",      "A cada 3 salas vencidas, recupera 5% da vida.", 0, true, false, 1, RARIDADE_INCOMUM },
};

const char *NOMES_ALCANCE[] = { "LONGO ALCANCE", "MÉDIO ALCANCE", "CURTO ALCANCE" };

// BALANCEAMENTO (nov/2026): modelo de DPS sem melhorias — alvo único (com precisão/uptime) e 4 inimigos juntos.
//   Generalistas ficam perto do Disparador em alvo único (~44); especialistas trocam alvo único por área/controle.
//   Roteador 11->13, Fluxo 4,5->5, Orbe 10->12 e 0,75->0,70 s, Pulso FW 18->20 e 0,95->0,85 s (estavam fracos);
//   Katana 24->22 (a mais forte em área e ainda corta balas); Raio 42->38 e EP (ver RAIO_CUSTO_EP/RAIO_EP_RETORNO):
//   com 3 inimigos o EP devolvido passava do gasto e o sigilo virava "infinito". Preços acompanham a força.
const DefArma DEF_ARMAS[NUM_ARMAS] = {
    { "DISPARADOR DE DADOS", "Tiro rápido e reto.",                           1, TIRO_DANO, TIRO_INTERVALO, 0.10f, TIRO_VELOCIDADE, TIRO_RAIO, 0.0f, 1, 4.0f,
      RARIDADE_COMUM, 0, { 102, 191, 255, 255 } },
    { "VETOR DE COMPILAÇÃO", "Segure para carregar. Carga total: tiro perfeito que atravessa tudo.",   0, 12.0f, 0.55f, 0.40f, 900.0f, 6.0f, 0.0f, 1, 0.0f,
      RARIDADE_RARA, 14, { 220, 240, 255, 255 } },
    { "ROTEADOR DE PACOTES", "Pacote teleguiado que ricocheteia. Melhore para mais pacotes.",         0, 13.0f, 0.45f, 0.30f, 340.0f, 6.0f, 650.0f, 1, 0.0f,
      RARIDADE_INCOMUM, 11, { 255, 161, 0, 255 } },
    { "FLUXO CONTÍNUO",      "Quanto mais quente, mais forte. Superaquecer solta uma explosão.",       1, 5.0f, 0.07f, 0.055f, 620.0f, 4.0f, 380.0f, 1, 10.0f,
      RARIDADE_COMUM, 9, { 253, 249, 0, 255 } },
    { "ORBE DE BUFFER",      "Esfera lenta: ao fim, puxa os inimigos e explode.",             1, 12.0f, 0.70f, 0.52f, 260.0f, 9.0f, 330.0f, 1, 0.0f,
      RARIDADE_INCOMUM, 12, { 200, 122, 255, 255 } },
    { "FRAGMENTADORA",       "Cone com coice. O 4º tiro é um cartucho dourado.",         2, 5.5f, 0.65f, 0.45f, 520.0f, 4.0f, 210.0f, 7, 40.0f,
      RARIDADE_COMUM, 8, { 255, 109, 194, 255 } },
    { "PULSO DE FIREWALL",   "Onda em volta que fere, empurra, apaga balas e ecoa.",   2, 20.0f, 0.85f, 0.68f, 0.0f, 0.0f, 95.0f, 0, 0.0f,
      RARIDADE_RARA, 14, { 0, 228, 48, 255 } },
    { "KATANA DE BYTES",     "Golpe em arco, corpo a corpo. Corta as balas.",  2, 22.0f, 0.42f, 0.28f, 480.0f, 8.0f, 300.0f, 0, 120.0f,
      RARIDADE_RARA, 15, { 235, 240, 255, 255 } },
    { "RAIO DE PROCESSAMENTO", "Gasta EP: um circuito se desenha no inimigo e vira uma coluna de luz. Acertar devolve EP.",   0, 38.0f, 0.55f, 0.40f, 1100.0f, 7.0f, 0.0f, 1, 0.0f,
      RARIDADE_RARA, 18, { 255, 230, 80, 255 } },
    { "RAIO CLÁSSICO", "Gasta EP a cada tiro: raio que atravessa tudo.",   0, 55.0f, 0.60f, 0.45f, 1100.0f, 7.0f, 0.0f, 1, 0.0f,
      RARIDADE_RARA, 15, { 120, 225, 255, 255 } },
};

const char *NOMES_CATEGORIA_UP[NUM_CATEGORIAS_UP] = { "ATRIBUTO", "DISPARO", "HABILIDADE", "ESPECIAL", "APRIMORAMENTO DE ARMA", "SINERGIA" };
const Color CORES_CATEGORIA_UP[NUM_CATEGORIAS_UP] = {
    { 102, 191, 255, 255 }, { 255, 161, 0, 255 }, { 0, 228, 48, 255 }, { 255, 0, 255, 255 }, { 253, 249, 0, 255 }, { 255, 109, 194, 255 }
};

const DefUpgrade UPGRADES[NUM_UPGRADES] = {
    { "SOBRECARGA DE TENSORES",  "+12% de dano em todas as armas por nível.",                        CAT_ATRIBUTO,   RARIDADE_COMUM,   3, 0, -1 },
    { "PROPAGAÇÃO RÁPIDA",       "+8% de velocidade de movimento por nível.",                        CAT_ATRIBUTO,   RARIDADE_COMUM,   3, 0, -1 },
    { "REDE NEURAL ÁGIL",        "A esquiva recarrega 12% mais rápido por nível.",                   CAT_ATRIBUTO,   RARIDADE_COMUM,   3, 0, -1 },
    { "REDUNDÂNCIA DE DADOS",    "+20 de vida máxima por nível (e recupera 20 agora).",              CAT_ATRIBUTO,   RARIDADE_COMUM,   3, 0, -1 },
    { "PROCESSAMENTO PARALELO",  "+10% de cadência em todas as armas por nível.",                    CAT_ATRIBUTO,   RARIDADE_COMUM,   3, 0, -1 },
    { "JANELA DE CONTEXTO",      "+20% de alcance e +15% de velocidade dos projéteis por nível.",    CAT_ATRIBUTO,   RARIDADE_COMUM,   2, 0, -1 },

    { "TOKENS SALTITANTES",      "Seus disparos ricocheteiam para +1 inimigo por nível.",            CAT_DISPARO,    RARIDADE_INCOMUM, 2, 0, -1 },
    { "ATENÇÃO MULTICABEÇA",     "Seus disparos atravessam +1 inimigo por nível.",                   CAT_DISPARO,    RARIDADE_INCOMUM, 2, 0, -1 },
    { "SAÍDA GENERATIVA",        "Cada acerto explode em área (35% do dano). Nível 2: raio maior.",  CAT_DISPARO,    RARIDADE_INCOMUM, 2, 0, -1 },
    { "AMOSTRAGEM MÚLTIPLA",     "+1 projétil extra em leque por nível (45% do dano). O nível 2 exige Atenção Multicabeça.",              CAT_DISPARO,    RARIDADE_RARA,    2, 0, -1 },
    { "RESÍDUO DE DADOS",        "Acertos deixam uma poça de dados que fere quem pisa (1,5 s).",     CAT_DISPARO,    RARIDADE_INCOMUM, 1, 0, -1 },
    { "CONTRA-ATAQUE HEURÍSTICO","Depois de esquivar: +40% de dano por 1,5 s (+30% por nível).",     CAT_DISPARO,    RARIDADE_INCOMUM, 2, 0, -1 },

    { "PULSO AMPLIFICADO",       "Pulso de Integridade: +35% de raio e +40% de dano por nível.",     CAT_HABILIDADE, RARIDADE_COMUM,   2, 0, -1 },
    { "ESQUIVA FRAGMENTADA",     "A esquiva deixa uma explosão de dados onde você estava.",          CAT_HABILIDADE, RARIDADE_INCOMUM, 2, 0, -1 },
    { "RECICLAGEM DE ENERGIA",   "+6 de EP por inimigo derrotado (por nível).",                      CAT_HABILIDADE, RARIDADE_COMUM,   2, 0, -1 },
    { "PROTOCOLO DE BLINDAGEM",  "Derrotar um elite (ou 12 inimigos) gera um escudo por 8 s.",       CAT_HABILIDADE, RARIDADE_INCOMUM, 1, 0, -1 },
    { "ECO BENIGNO",             "15% de chance por nível de duplicar um disparo.",                  CAT_HABILIDADE, RARIDADE_INCOMUM, 2, 0, -1 },
    { "ROTULAGEM DE DADOS",      "Alvos atingidos ficam ROTULADOS por 4 s e levam +25% de dano.",    CAT_HABILIDADE, RARIDADE_INCOMUM, 1, 0, -1 },
    { "VERIFICAÇÃO CRUZADA",     "Recupera 2 de vida por inimigo derrotado (por nível).",            CAT_HABILIDADE, RARIDADE_INCOMUM, 2, 0, -1 },
    { "ALTA CONFIANÇA",          "10% de chance por nível de acerto crítico (dano x2).",             CAT_HABILIDADE, RARIDADE_COMUM,   3, 0, -1 },

    { "BUFFER DE ESQUIVA",       "A esquiva ganha uma segunda carga.",                               CAT_ESPECIAL,   RARIDADE_RARA,    1, 0, -1 },
    { "PULSO REFLEXIVO",         "O Pulso devolve as balas que apaga como disparos seus.",           CAT_ESPECIAL,   RARIDADE_RARA,    1, 0, -1 },

    { "DISPARADOR CALIBRADO",    "Disparador de Dados: +25% de dano e projétil maior por nível.",    CAT_APRIMORAMENTO, RARIDADE_COMUM,   2, 0, ARMA_DISPARADOR },
    { "COMPILAÇÃO OTIMIZADA",    "Vetor de Compilação: carrega 40% mais rápido e atravessa +2.",     CAT_APRIMORAMENTO, RARIDADE_INCOMUM, 1, 0, ARMA_VETOR },
    { "TABELA DE ROTAS",         "Roteador de Pacotes: +2 ricochetes e perde menos dano.",           CAT_APRIMORAMENTO, RARIDADE_INCOMUM, 1, 0, ARMA_ROTEADOR },
    { "RESFRIAMENTO LÍQUIDO",    "Fluxo Contínuo: esquenta 40% menos.",                              CAT_APRIMORAMENTO, RARIDADE_COMUM,   1, 0, ARMA_FLUXO },
    { "BUFFER EXPANDIDO",        "Orbe de Buffer: explosão com +50% de raio e de dano.",             CAT_APRIMORAMENTO, RARIDADE_INCOMUM, 1, 0, ARMA_ORBE },
    { "CONE DENSO",              "Fragmentadora: +3 fragmentos e um cone mais fechado.",             CAT_APRIMORAMENTO, RARIDADE_INCOMUM, 1, 0, ARMA_FRAGMENTADORA },
    { "MURALHA ATIVA",           "Pulso de Firewall: +35% de raio e dispara 20% mais rápido.",       CAT_APRIMORAMENTO, RARIDADE_INCOMUM, 1, 0, ARMA_PULSO_FW },
    { "LENTE DE CRISTAL",        "Vetor: +30% de dano em todo disparo (óptica de precisão).",        CAT_APRIMORAMENTO, RARIDADE_INCOMUM, 1, 0, ARMA_VETOR },
    { "ESTAMPIDO SÔNICO",        "Vetor: disparo com meia carga ou mais empurra quem estiver perto.", CAT_APRIMORAMENTO, RARIDADE_COMUM,   1, 0, ARMA_VETOR },
    { "PACOTE EXTRA",            "Roteador: +1 pacote por salva a cada nível (começa com 1, chega a 3).", CAT_APRIMORAMENTO, RARIDADE_INCOMUM, 2, 0, ARMA_ROTEADOR },
    { "ROTA EXPRESSA",           "Roteador: pacotes 35% mais rápidos (banda larga).",                CAT_APRIMORAMENTO, RARIDADE_COMUM,   1, 0, ARMA_ROTEADOR },
    { "OVERCLOCK",               "Fluxo: o dano cresce muito mais com o calor (até o dobro).",       CAT_APRIMORAMENTO, RARIDADE_INCOMUM, 1, 0, ARMA_FLUXO },
    { "VÁLVULA DE ESCAPE",       "Fluxo: a explosão do superaquecimento fica maior e 2x mais forte.", CAT_APRIMORAMENTO, RARIDADE_INCOMUM, 1, 0, ARMA_FLUXO },
    { "BURACO NEGRO",            "Orbe: puxa de bem mais longe e com mais força antes de explodir.", CAT_APRIMORAMENTO, RARIDADE_INCOMUM, 1, 0, ARMA_ORBE },
    { "SUPERNOVA",               "Orbe: a explosão deixa um campo de radiação que queima por 2,5 s.", CAT_APRIMORAMENTO, RARIDADE_RARA,   1, 0, ARMA_ORBE },
    { "PÓLVORA FARTA",           "Fragmentadora: cartucho dourado a cada 3 tiros e com +6 fragmentos.", CAT_APRIMORAMENTO, RARIDADE_INCOMUM, 1, 0, ARMA_FRAGMENTADORA },
    { "FRAGMENTOS QUICANTES",    "Fragmentadora: cada fragmento ricocheteia 1 vez nas paredes e inimigos.", CAT_APRIMORAMENTO, RARIDADE_COMUM, 1, 0, ARMA_FRAGMENTADORA },
    { "ECO DUPLO",               "Pulso de Firewall: um segundo eco volta depois do primeiro.",      CAT_APRIMORAMENTO, RARIDADE_RARA,    1, 0, ARMA_PULSO_FW },
    { "CAMPO DE REPARO",         "Pulso de Firewall: cada pulso recupera 0,6 de vida (backup em nuvem).", CAT_APRIMORAMENTO, RARIDADE_COMUM, 1, 0, ARMA_PULSO_FW },
    { "SUPERCONDUTOR",           "Raio de Processamento: cada sigilo gasta 35% menos EP.",           CAT_APRIMORAMENTO, RARIDADE_INCOMUM, 1, 0, ARMA_RAIO },
    { "LENTE DE PLASMA",         "Raio de Processamento: +40% de dano e um sigilo bem maior.",       CAT_APRIMORAMENTO, RARIDADE_COMUM,   1, 0, ARMA_RAIO },
    { "SUPERCONDUTOR CLÁSSICO",  "Raio Clássico: cada disparo gasta 45% menos EP.",                   CAT_APRIMORAMENTO, RARIDADE_INCOMUM, 1, 0, ARMA_RAIO_CLASSICO },
    { "LENTE DE PLASMA CLÁSSICA","Raio Clássico: +40% de dano e um feixe mais grosso.",               CAT_APRIMORAMENTO, RARIDADE_COMUM,   1, 0, ARMA_RAIO_CLASSICO },
    { "LÂMINA AFIADA",           "Katana de Bytes: +25% de dano por nível.",                         CAT_APRIMORAMENTO, RARIDADE_COMUM,   2, 0, ARMA_KATANA },
    { "ARCO AMPLIADO",           "Katana de Bytes: +35% de alcance e um arco 45% maior.",            CAT_APRIMORAMENTO, RARIDADE_INCOMUM, 1, 0, ARMA_KATANA },
    { "TÊMPERA RÁPIDA",          "Katana de Bytes: golpeia 20% mais rápido.",                        CAT_APRIMORAMENTO, RARIDADE_COMUM,   1, 0, ARMA_KATANA },
    { "TORNADO DE DADOS",         "Katana: o golpe invoca um redemoinho de 0s e 1s que persegue, puxa e fere.", CAT_APRIMORAMENTO, RARIDADE_RARA, 1, 0, ARMA_KATANA },
    { "REFLEXO ESPELHADO",       "Katana de Bytes: as balas que ela corta voltam como tiros seus.",  CAT_APRIMORAMENTO, RARIDADE_RARA,    1, 0, ARMA_KATANA },
    { "FOGO: SUPERAQUECIMENTO",  "Katana: os golpes incendeiam (queima por 3 s). Nível 2: queima mais forte.", CAT_APRIMORAMENTO, RARIDADE_INCOMUM, 2, 0, ARMA_KATANA },
    { "GELO: CACHE CONGELADO",   "Katana: os golpes congelam o alvo: 50% mais lento por 2,5 s.",     CAT_APRIMORAMENTO, RARIDADE_INCOMUM, 1, 0, ARMA_KATANA },
    { "RAIO: SURTO DE TENSÃO",   "Katana: 3 golpes seguidos que acertam: um raio salta entre até 3 inimigos.",     CAT_APRIMORAMENTO, RARIDADE_RARA,    1, 0, ARMA_KATANA },
    { "VENTO: VENTILAÇÃO",       "Katana: empurra muito mais longe e corta balas num raio maior.",   CAT_APRIMORAMENTO, RARIDADE_COMUM,   1, 0, ARMA_KATANA },
    { "ÁGUA: RESFRIAMENTO",      "Katana: cada inimigo atingido recupera 1,5 de vida e 4 de EP.",    CAT_APRIMORAMENTO, RARIDADE_INCOMUM, 1, 0, ARMA_KATANA },

    { "TEMPESTADE DE PACOTES",   "Ricochetes não perdem dano e ganham +1 salto.",                    CAT_SINERGIA, RARIDADE_RARA, 1, BIT_UP(UP_RICOCHETE) | BIT_UP(UP_RAJADA), -1 },
    { "CIRCUITO FECHADO",        "Cada inimigo derrotado pelo Pulso devolve 12 EP e 1 s de recarga.", CAT_SINERGIA, RARIDADE_RARA, 1, BIT_UP(UP_PULSO) | BIT_UP(UP_ENERGIA_ABATE), -1 },
    { "PASSO FANTASMA",          "Esquivar através de inimigos fere (25) e o bônus pós-esquiva dobra.", CAT_SINERGIA, RARIDADE_RARA, 1, BIT_UP(UP_POS_ESQUIVA) | BIT_UP(UP_ESQUIVA), -1 },
    { "RÓTULO VOLÁTIL",          "Inimigos rotulados explodem ao serem derrotados.",                 CAT_SINERGIA, RARIDADE_RARA, 1, BIT_UP(UP_ROTULAR) | BIT_UP(UP_EXPLOSAO), -1 },
    { "CONFIANÇA CALIBRADA",     "Acertos críticos disparam um pacote extra no inimigo mais perto.", CAT_SINERGIA, RARIDADE_RARA, 1, BIT_UP(UP_CRITICO) | BIT_UP(UP_DUPLICAR), -1 },
    { "CHOQUE TÉRMICO",          "Katana: alvos queimando E congelados levam +40% de dano.",         CAT_SINERGIA, RARIDADE_RARA, 1, BIT_UP(UP_ARMA_KATANA_FOGO) | BIT_UP(UP_ARMA_KATANA_GELO), ARMA_KATANA },
};

const char *NOMES_RECOMPENSA[NUM_TIPOS_RECOMPENSA] = { "", "MELHORIA", "MELHORIA RARA", "FRAGMENTOS", "ARSENAL", "RESTAURAÇÃO", "DESAFIO OPCIONAL" };

// Profundidade equivalente (a escala antiga, de 0 a 7) de cada camada: decide a dificuldade
const float PROF_DIFICULDADE[MAPA_PROFUNDIDADES] = { 0.0f, 1.0f, 1.8f, 2.6f, 3.2f, 3.9f, 4.6f, 5.3f, 6.0f, 6.5f, 7.0f };

// Portas físicas (a de cima é a mesma do átrio; a da esquerda fica onde o átrio tem a do corredor)
const Rectangle PORTA_LADO_ROGUE[3] = { { 370, 60, 60, 14 }, { 40, 260, 14, 70 }, { 746, 260, 14, 70 } };
const Vector2   ENTRADA_LADO_ROGUE[NUM_LADOS] = { { 400, 110 }, { 84, 295 }, { 716, 295 }, { 400, 480 } };
// Onde a parede corrompida (passagem secreta) pode aparecer: longe das portas normais
const Rectangle PAREDE_SECRETA_ROGUE[4] = { { 140, 60, 70, 12 }, { 590, 60, 70, 12 }, { 40, 420, 12, 70 }, { 748, 420, 12, 70 } };

const char *NOMES_OBJC[NUM_OBJC] = { "ELIMINAR", "SOBREVIVER", "NÚCLEOS DE CORRUPÇÃO", "ALVO PRIORITÁRIO", "MECANISMO CORROMPIDO" };
const char *NOMES_AFIXOS[NUM_AFIXOS] = { "", "PULSO", "DIVISÃO", "ORBITAL" };

const char *NOMES_LAB[NUM_LABS]  = { "LABORATÓRIO DE PROMPTS", "ARQUIVO CORROMPIDO", "NÚCLEO NEURAL", "FIREWALL ÉTICO", "CÂMARA DE PESQUISA" };
const char *PLACAS_LAB[NUM_LABS] = { "LAB. DE PROMPTS", "ARQUIVO CORROMPIDO", "NÚCLEO NEURAL", "FIREWALL ÉTICO", "CÂMARA DE PESQUISA" };
const Color CORES_LAB[NUM_LABS]  = {
    { 0, 228, 255, 255 }, { 255, 109, 194, 255 }, { 150, 140, 255, 255 }, { 0, 228, 48, 255 }, { 255, 203, 0, 255 }
};
const bool LAB_ESPECIAL[NUM_LABS] = { false, false, false, true, true };    // especiais: mais generosos (todos são opcionais)

const char *FALAS_LUCI_LAB[NUM_LABS] = {
    "Luci: Um laboratório de prompts! É opcional: a porta já está aberta.",
    "Luci: Um arquivo corrompido... Opcional, e a porta já está aberta.",
    "Luci: O Núcleo Neural. Desafio opcional: a porta já está aberta.",
    "Luci: Firewall Ético. Opcional, mas guarda algo raro. A porta já está aberta.",
    "Luci: Câmara de Pesquisa: uma investigação opcional. A porta já está aberta."
};

const char *NOMES_SALAS_ROGUE[NUM_TIPOS_SALA] = { "ÁTRIO", "COMBATE", "EDUCATIVA", "RECOMPENSA", "ALUCINADOR", "SILOGISTA", "NÚCLEO", "LUCI",
                                                         "ELITE", "EVENTO", "DESCANSO", "DESAFIO", "SECRETA", "LABORATÓRIO", "ESPECIAL", "MESTRE" };
const Color CORES_SALAS_ROGUE[NUM_TIPOS_SALA] = {
    { 102, 191, 255, 255 }, { 230, 41, 55, 255 }, { 0, 228, 255, 255 }, { 255, 203, 0, 255 }, { 230, 41, 255, 255 },
    { 120, 110, 255, 255 }, { 255, 0, 255, 255 }, { 255, 0, 255, 255 },
    { 255, 120, 20, 255 }, { 200, 122, 255, 255 }, { 0, 228, 48, 255 }, { 253, 249, 0, 255 }, { 190, 33, 255, 255 },
    { 40, 220, 170, 255 }, { 255, 70, 100, 255 }, { 255, 0, 255, 255 }
};
// Nome que aparece na porta (o que o jogador vai encontrar)
const char *PLACAS_SALAS_ROGUE[NUM_TIPOS_SALA] = { "ÁTRIO", "COMBATE", "TERMINAL", "TESOURO", "ALUCINADOR", "CÂMARA DA INFERÊNCIA", "NÚCLEO", "LUCI",
                                                          "ELITE", "EVENTO", "SALA DE LEITURA", "DESAFIO", "ARQUIVO OCULTO", "LABORATÓRIO", "ENCONTRO ESPECIAL",
                                                          "ALUCINAÇÃO MESTRE" };

// Placa de um nó do mapa (o laboratório mostra a categoria)
const char *RoguePlacaNo(const NoMapa *n)
{
    if (n->tipo == SALA_LABORATORIO && n->lab >= 0 && n->lab < NUM_LABS)
        return PLACAS_LAB[n->lab];
    if (n->tipo == SALA_ESPECIAL && n->especial >= 0 && n->especial < NUM_ESPECIAIS)
        return PLACAS_ESPECIAIS[n->especial];
    return PLACAS_SALAS_ROGUE[n->tipo];
}

// ---------- BLOCO 3 (reformulação): 3 fases ocultas, combate corpo a corpo e PARRY (AJUSTE AQUI) ----------
//   A tela mostra UMA barra só. Ela zera 3 vezes:
//     1ª barra  CONFLITO LÓGICO  (dica envenenada e, na metade, as contradições)
//     2ª barra  SURTO FÍSICO     (vermelha: investidas corpo a corpo com parry)
//     3ª barra  NÚCLEO INSTÁVEL  (magenta: investidas rápidas, ondas de choque e chão que cai)
//   Somadas, as 3 barras dão LUCI_HP_BOA ou LUCI_HP_RUIM.
const float LUCI_FRACAO_FASE[3] = { 0.36f, 0.32f, 0.32f };    // era { 0,42, 0,30, 0,28 }: o clímax ficava curto
const float MELEE_FRACAO[3] = { 0.05f, 0.06f, 0.09f };       // da barra atual, por golpe do combo (20% por combo; era 28%)

// SWIPE_FALACIA: cada investida corpo a corpo é uma falácia (o parry a refuta)
const char *FALACIAS_LUCI[][2] = {
    { "FALSO DILEMA",            "Mostra só duas opções quando existem outras." },
    { "APELO À AUTORIDADE",      "\"É verdade porque alguém importante disse\" não prova nada." },
    { "ESPANTALHO",              "Distorce o que o outro disse para atacar a versão falsa." },
    { "GENERALIZAÇÃO APRESSADA", "Tira uma regra geral de poucos casos." },
    { "AD HOMINEM",              "Ataca a pessoa em vez do argumento." },
    { "BOLA DE NEVE",            "Supõe uma cadeia de consequências sem nenhuma prova." },
};
