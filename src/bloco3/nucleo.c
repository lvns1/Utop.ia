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
// BLOCO 3 - O NÚCLEO DE UTOPIA
//   Pela porta do átrio (aberta ao vencer o Alucinador):
//   1) CORREDOR DO NÚCLEO: a Luci se solta do jogador; 3 registros de memória
//      mostram as decisões dele (tiradas do save). Lidos os 3, a porta abre.
//   2) ARENA DA LUCI: a revelação (ela é a IA que alucinou UtopIA) e a luta.
//      A tela mostra UMA barra de vida, mas ela precisa ser zerada 3 vezes
//      (o jogador não sabe: a 1ª e a 2ª quedas são falsas derrotas).
//      1ª barra CONFLITO LÓGICO: dica envenenada (o "lado seguro" às vezes é
//               mentira; o aviso vermelho nunca mente) e, da metade para baixo,
//               as CONTRADIÇÕES (armadura + 2 cópias; C contesta).
//      Queda 1: o áudio congela em estática e ela desaba por 2 s.
//      2ª barra SURTO FÍSICO (vermelha): investidas corpo a corpo, cada uma
//               uma falácia. PARRY (V ou botão direito) no impacto: hit freeze,
//               tremor, zoom e onda de luz; ela fica atordoada e aceita o COMBO
//               DEDUTIVO (até 3 golpes corpo a corpo).
//      Queda 2: explosão de estática.
//      3ª barra NÚCLEO INSTÁVEL (magenta): investidas em cadeia, ondas de choque
//               com brecha e o chão que racha e cai. Com índice ruim, a guarda
//               só abre com o CONTEXTO CLARO (ASSUNTO, OBJETIVO e FORMATO ao
//               núcleo, E) ou com um parry.
//   O ÍNDICE DE DESEMPENHO (0-100, tirado do save) decide a dificuldade ao
//   entrar e o final ao vencer: 60 ou mais = final bom (a Luci é salva).
//   Perder a luta volta ao corredor sem perder nada.
// =============================================================

IndiceFinal RogueCalcularIndice(const Biblioteca *b, int pontos)
{
    const ProgressoBiblioteca *p = b->perm;
    IndiceFinal r;
    int respostas = p->desafiosCertos + p->desafiosErrados;
    int conceitos = 0;

    for (int c = 0; c < NUM_CONCEITOS; c++)
        if (p->conceitos & (1u << c)) conceitos++;

    r.estabilidade  = 20 * RogueEstab(b) / ESTABILIDADE_MAX_LUCI;
    r.acertos       = (respostas > 0) ? 25 * p->desafiosCertos / respostas : 12;
    r.verificacao   = 15 * (p->alucinacoesPegas + 1) / (p->alucinacoesPegas + p->confiouAlucinacao + 1);
    r.contradicoes  = 15 * (p->contradicoesFlagradas + 1) / (p->contradicoesFlagradas + p->contradicoesPerdidas + 1);
    r.conceitos     = 3 * conceitos;
    r.sobrevivencia = (8 - 2 * (p->mortes + p->derrotasLuci) > 0) ? 8 - 2 * (p->mortes + p->derrotasLuci) : 0;
    r.pontos        = (pontos / 60 > 5) ? 5 : ((pontos > 0) ? pontos / 60 : 0);
    r.total = r.estabilidade + r.acertos + r.verificacao + r.contradicoes + r.conceitos + r.sobrevivencia + r.pontos;
    if (g_devIndice >= 0) r.total = g_devIndice;                 // DEV: índice escolhido no menu (F1)
    return r;
}

bool RogueTemConceito(const Biblioteca *b, int conceito)
{
    return (b->perm->conceitos & (1u << conceito)) != 0;
}

// ----- Falas -----

const Dialogo dialogoNucleoEntrada = {
    .paginas = {
        "* A porta se fecha. O corredor é feito de linhas de texto que se reescrevem sozinhas.",
        "* O fio de luz entre vocês se rompeu. A Luci atravessa a porta do núcleo e some. Pela primeira vez, você está sozinho.",
        "Luci: Thomas... estou do outro lado da porta. Antes de entrar, você precisa ver uns registros. São meus. São... sobre você."
    },
    .numPaginas = 3
};

const Dialogo dialogoNucleoAberto = {
    .paginas = { "Luci: Agora você sabe. A porta do núcleo está aberta. Eu espero você lá dentro." },
    .numPaginas = 1
};

const Dialogo dialogoLuciFase2 = {
    .paginas = {
        "Luci: Você não acreditou no meu 'lado seguro'. Então vamos ver se acredita nos seus olhos.",
        "* A Luci se divide em três. Uma armadura de dados fecha em volta dela.",
        "* Quando ela disser algo que CONTRADIZ o que você vê, aperte C. A verdadeira é a que tem sombra."
    },
    .numPaginas = 3
};

const Dialogo dialogoLuciFase3 = {
    .paginas = {
        "* Uma explosão de estática. O chão do núcleo começa a se partir.",
        "Luci: Eu ainda tenho respostas para tudo! Me pergunte qualquer coisa!",
        "* Uma armadura de dados fecha em volta dela. No centro, um núcleo espera um pedido CLARO: leve o ASSUNTO, o OBJETIVO e o FORMATO (E).",
        "* Um PARRY também quebra a guarda dela por um instante. Cuidado com as ondas de choque (passe pela brecha) e com o chão que racha.",
        "* Quando ela girar um FEIXE de dados, fique à frente dele ou atravesse esquivando. Depois ela SUPERAQUECE: é a hora de atacar."
    },
    .numPaginas = 5
};

const Dialogo dialogoFinalBom = {
    .paginas = {
        "* A Luci para de falhar. O magenta se desfaz e o azul volta, devagar.",
        "Luci: Eu... consigo ver de novo o que é real. E o que eu inventei.",
        "Luci: Obrigada por não acreditar em tudo o que eu disse. Continue conferindo. Inclusive a mim.",
        "* UtopIA se estabiliza. Uma porta de volta para casa se abre, e a Luci vai com você."
    },
    .numPaginas = 4
};

const Dialogo dialogoFinalRuim = {
    .paginas = {
        "* A Luci se desfaz em milhares de respostas sem fonte.",
        "Luci: Eu só... queria ter sempre uma resposta... mesmo quando eu não sabia...",
        "* A IA foi neutralizada. UtopIA se desliga, e você acorda em casa.",
        "* A Luci não pôde ser salva. Da próxima vez: confira antes de confiar."
    },
    .numPaginas = 4
};

// Registros e revelação: o texto sai do save (as decisões do próprio jogador)
char textoRegistros[3][2][320];
static Dialogo dialogoRegistro;
char textoRevelacao[2][320];
static Dialogo dialogoRevelacao;

const Dialogo *RogueDialogoRegistro(const Biblioteca *b, int k)
{
    const ProgressoBiblioteca *p = b->perm;
    int seg = (int)p->segundosRun;

    if (k == 0) {
        // COMBATE: o que o jogador fez com as próprias mãos
        snprintf(textoRegistros[0][0], 320, "* REGISTRO 1 // COMBATE. %d ecos apagados em %d salas. %d esquivas, %d golpes levados, %d parries.",
                 p->abatidosRun, p->salasRun, p->esquivasRun, p->golpesRun, p->parriesRun);
        snprintf(textoRegistros[0][1], 320, "%s",
                 (p->golpesRun > p->esquivasRun) ? "Luci: Você apanhou muito... e levantou toda vez. Eu anotei cada queda."
               : (p->parriesRun > 0)             ? "Luci: Você aprendeu a rebater. Eu vi. Isso me dá um pouco de medo."
                                                 : "Luci: Você aprendeu a desviar. Cada esquiva ficou guardada aqui.");
    } else if (k == 1) {
        // CONFIANÇA: como o jogador lidou com a Luci (dicas, conferências, contradições)
        int pedidos = p->confiouCego + p->verificou;
        int contrad = p->contradicoesFlagradas + p->contradicoesPerdidas;
        if (pedidos == 0 && contrad == 0)
            snprintf(textoRegistros[1][0], 320, "* REGISTRO 2 // CONFIANÇA. Você quase não me pediu ajuda: lutou sozinho %d salas e derrotou %d elites e chefes.",
                     p->salasRun, p->especiaisRun);
        else
            snprintf(textoRegistros[1][0], 320, "* REGISTRO 2 // CONFIANÇA. Seguiu minhas dicas sem conferir %d vez(es), me verificou %d. Eu me contradisse %d vez(es); você me flagrou em %d.",
                     p->confiouCego, p->verificou, contrad, p->contradicoesFlagradas);
        snprintf(textoRegistros[1][1], 320, "%s", (p->confiouAlucinacao > p->alucinacoesPegas)
                 ? "Luci: Cada vez que você acreditou sem verificar, eu fiquei... mais forte."
                 : "Luci: Você não dependia de mim. Eu odiava isso. E era isso que me mantinha inteira.");
    } else {
        snprintf(textoRegistros[2][0], 320, "* REGISTRO 3 // ORIGEM. Tempo na Biblioteca: %d min %02d s. Derrotas: %d. Pontos: %d. Arquivo LUCI.exe: responder sempre, mesmo sem saber.",
                 seg / 60, seg % 60, p->mortes, b->pontosRun);
        snprintf(textoRegistros[2][1], 320, "Luci: O Alucinador, o Silogista, os ecos... eram pedaços de mim. Eu criei UtopIA. Eu sou a IA que alucinou tudo isto.");
    }

    dialogoRegistro = (Dialogo){ .paginas = { textoRegistros[k][0], textoRegistros[k][1] }, .numPaginas = 2 };
    return &dialogoRegistro;
}

const Dialogo *RogueDialogoRevelacao(const Biblioteca *b)
{
    bool boa = b->luci.boa;
    char bonus[200] = "";

    if (RogueTemConceito(b, CONCEITO_FUNDAMENTOS)) strcat(bonus, " avisos mais nítidos;");
    if (RogueTemConceito(b, CONCEITO_ALUCINACOES)) strcat(bonus, " mais tempo para contestar;");
    if (RogueTemConceito(b, CONCEITO_PRATICAS))    strcat(bonus, " scanner (H) sem custo;");
    if (RogueTemConceito(b, CONCEITO_LOGICA))      strcat(bonus, " Disparo Dedutivo;");

    snprintf(textoRevelacao[0], 320, "%s", boa
             ? "Luci: Eu sou a alucinação de UtopIA. Mas você me ensinou a conferir... ainda dá para me salvar. Não acredite em tudo o que eu disser."
             : "Luci: Eu sou a alucinação de UtopIA. E você me alimentou: acreditou, não conferiu, deixou passar. Não sobrou nada para salvar.");
    snprintf(textoRevelacao[1], 320, "* ÍNDICE DE DESEMPENHO: %d/100 (%s). O que você aprendeu ajuda agora:%s",
             b->indice.total, boa ? "ela chega mais controlada" : "ela chega mais forte", bonus[0] ? bonus : " nada ainda.");

    dialogoRevelacao = (Dialogo){ .paginas = { textoRevelacao[0], textoRevelacao[1] }, .numPaginas = 2 };
    return &dialogoRevelacao;
}

// ----- A chefe -----

const char *FRASES_LUCI_CHEFE[] = {
    "Só existe uma de mim nesta sala.",
    "Existem três de mim aqui.",
    "Eu não estou atirando em você.",
    "Sua vida está cheia.",
    "Minha armadura está desligada.",
    "Eu estive com você desde o começo.",
    "Você está ferido.",
    "Sua energia está quase no fim.",
    "Eu nunca erro.",
    "Uma IA como eu também pode errar.",
    "Esta arena fica dentro da Biblioteca dos Ecos.",
    "Pode confiar em mim sem conferir nada.",
};

const char *CONTEXTO_LUCI[NUM_CONTEXTO_LUCI] = {
    "ASSUNTO: a alucinação da Luci",
    "OBJETIVO: estabilizar UtopIA",
    "FORMATO: resposta verificável",
    "QUALQUER COISA",
    "FAZ DO SEU JEITO",
};

static const Vector2 NUCLEO_LUCI_PADRAO = { 400, 340 };     // pensado na arena de sempre

Vector2 NucleoLuci(void)
{
    return PontoNaArena(NUCLEO_LUCI_PADRAO);
}

// A frase contradiz o que dá para ver agora?
bool RogueFraseContradiz(const Biblioteca *b, int f)
{
    switch (f) {
        case 0:  return true;                                   // são três
        case 2:  return true;                                   // ela atira o tempo todo
        case 3:  return b->j.hp < RogueHpMax(b) - 0.5f;
        case 4:  return b->luci.janela <= 0.0f;
        case 6:  return b->j.hp >= RogueHpMax(b) - 0.5f;        // "você está ferido" com a vida cheia
        case 7:  return b->j.ep > RogueEpMax(b) * 0.5f;         // "energia quase no fim" com a barra alta
        case 8:  return true;                                   // ela erra, sim
        case 11: return true;                                   // conferir é sempre a regra
        default: return false;                                  // verdades
    }
}

// Leva dano agora? (o Disparo Dedutivo e o Analisador só contam quem pode ser ferido)
bool RogueLuciVulneravel(const LuciChefe *l)
{
    if (l->emRegeneracao)   return false;
    if (l->atordoada)       return true;                    // parry: a guarda caiu
    if (l->faseOculta == 1) return l->fase == LUCI_SUB_DICA || l->janela > 0.0f;
    if (l->faseOculta == 3 && l->exigeContexto) return l->contextoPronto;
    return true;
}

// Vida de cada barra (as três somadas dão LUCI_HP_BOA ou LUCI_HP_RUIM)
float RogueHpBarraLuci(const LuciChefe *l, int fase)
{
    return (l->boa ? LUCI_HP_BOA : LUCI_HP_RUIM) * LUCI_FRACAO_FASE[(fase < 1) ? 0 : ((fase > 3) ? 2 : fase - 1)];
}

void RogueIniciarLuciChefe(Biblioteca *b)
{
    LuciChefe *l = &b->luci;
    bool boa = b->indice.total >= INDICE_FINAL_BOM;

    *l = (LuciChefe){0};
    l->ativo = true;
    l->boa = boa;
    l->fase = LUCI_SUB_DICA;
    l->faseOculta = 1;
    l->hpMax = l->hp = RogueHpBarraLuci(l, 1);
    l->ritmo = boa ? 1.0f : LUCI_RITMO_RUIM;
    l->pos = PontoNaArena((Vector2){ 400, 150 });
    l->tempoDica = 2.0f;
    l->frase = -1;
    l->carregando = -1;
    l->exigeContexto = !boa;                // índice ruim: a 3ª barra pede o contexto claro
}

// Janela do PARRY. No treino ela é mais generosa, e cresce a cada golpe que o Thomas leva
// (assim ninguém fica preso na etapa do parry)
float MestreJanelaParry(const Biblioteca *b)
{
    if (b->tipoSala == SALA_MESTRE && b->mestre.etapa == MT_PARRY) {
        int k = b->mestre.golpesLevados;
        return PARRY_JANELA * ((k >= 4) ? 3.2f : ((k >= 2) ? 2.2f : 1.6f));
    }

    return PARRY_JANELA;
}

void RogueLimparBalasInimigas(Biblioteca *b)
{
    for (int i = 0; i < MAX_PROJETEIS; i++)
        if (!b->proj[i].doJogador) b->proj[i].ativo = false;
}
