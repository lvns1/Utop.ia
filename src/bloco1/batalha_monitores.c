#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <time.h>
#include "../../include/core/fonte.h"
#include "../../include/core/mapas.h"
#include "../../include/bloco1/batalha_monitores.h"
#include "../../include/audio/audio.h"
#include "../../include/bloco1/batalha.h"
#include "../../include/bloco1/corredor.h"
#include "../../include/bloco1/minigame_visual.h"
#include "../../include/core/save.h"
#include "../../include/core/telemetria.h"
#include "../../include/core/texto.h"
#include "../../include/dados/conteudo_bloco1.h"
#include "../../include/entidades/jogador.h"
#include "../../include/entidades/luci_companion.h"
#include "../../include/telas/ranking.h"
#include "../../include/telas/historico_luci.h"


// Vida do Thomas entre os monitores do Bloco 1: o dano de um monitor continua no próximo.
// 0 = cheia (novo jogo, save antigo, ou derrota: a Luci recompõe o Thomas).
_Static_assert(HP_MAX_B2 <= 255 && HP_MAX_BATALHA <= 255, "o hp do corredor cabe em um byte");

int HpDoCorredor(const ProgressoCorredor *p)
{
    return (p->hp == 0 || p->hp > HP_MAX_B2) ? HP_MAX_B2 : (int)p->hp;
}

void GuardarHpDoCorredor(ProgressoCorredor *p, int hp, bool derrota)
{
    p->hp = derrota ? 0 : (unsigned char)Clamp((float)hp, 1.0f, (float)HP_MAX_B2);
}

const Vector2 SPAWN_CORREDOR_VOLTA = { CORR_X(1228), CORR_Y(440) };   // voltando ao corredor pela porta da direita

// Caixas da batalha
static const Rectangle CAIXA_B2         = {50, 205, 700, 300};
static const Rectangle CAIXA_DESVIO_B2  = {275, 255, 250, 170};

static const DefGuardiaoB2 GUARDIOES_B2[NUM_GUARDIOES] = {
    { "ALUCINAÇÃO",         "BLOCO 1 // CORREDOR DO HISTÓRICO",                         { 200, 170, 255, 255 }, "?!#01" },
};

static const RodadaEscolha RODADAS_MONITORES[TOTAL_MONITORES][RODADAS_POR_MONITOR] = {
    {   // MONITOR 1: o que é alucinação
        { "\"Falo com tanta confiança que só posso estar certa!\"",
          { "ACEITAR", "VERIFICAR" }, 2, 1,
          "Confiança não é prova: a alucinação soa segura, mas é falsa." },
        { "\"Eu sempre busco a verdade antes de responder!\"",
          { "CORRIGIR", "CONCORDAR" }, 2, 0,
          "A IA não busca a verdade: ela prevê a palavra mais provável." },
        { "\"Quando não sei, chutar vale mais que dizer 'não sei'.\"",
          { "APOIAR O CHUTE", "PREFERIR 'NÃO SEI'" }, 2, 1,
          "Avaliar mal o 'não sei' ensina a IA a inventar respostas." },
    },
    {   // MONITOR 2: tipos de alucinação
        { "\"Você disse que a prova é terça... então ela é quinta!\"",
          { "INTRÍNSECA", "EXTRÍNSECA" }, 2, 0,
          "Intrínseca: a resposta contradiz o que você informou." },
        { "\"Segundo o Instituto Inventado, 87% dos gatos falam francês.\"",
          { "INTRÍNSECA", "EXTRÍNSECA" }, 2, 1,
          "Extrínseca: não contradiz nada, mas não dá para verificar." },
        { "\"Seu texto fala de Recife, então vou resumir sobre Lisboa.\"",
          { "EXTRÍNSECA", "INTRÍNSECA" }, 2, 1,
          "Contradizer o contexto dado é uma falha de atenção: intrínseca." },
    },
    {   // MONITOR 3: riscos e mitigação
        { "\"Você pediu 'me fala daquilo'... então eu inventei algo!\"",
          { "REFORMULAR A PERGUNTA", "ACEITAR" }, 2, 0,
          "Pergunta clara e com contexto deixa menos lacunas para inventar." },
        { "\"Respondo de cabeça. Para que checar fontes?\"",
          { "CONFIAR", "VERIFICAR AS FONTES" }, 2, 1,
          "Verificação humana e fontes externas pegam a alucinação." },
        { "\"Sempre que chove, vende mais pipoca. A chuva causa pipoca!\"",
          { "CORRELAÇÃO NÃO É CAUSA", "CONCORDAR" }, 2, 0,
          "A IA junta coisas que aparecem juntas, mesmo sem relação real." },
    },
};

// Dicas da Luci (H durante a escolha): um empurrão no raciocínio, nunca a resposta
static const char *DICAS_MONITORES[TOTAL_MONITORES][RODADAS_POR_MONITOR] = {
    { "Falar com confiança prova alguma coisa?",
      "Lembra: a IA trabalha com probabilidades, não com a verdade.",
      "Se chutar ganha ponto, o que a IA aprende a fazer?" },
    { "Ela contradiz algo que você mesmo disse?",
      "Contradiz o contexto ou só não dá para checar?",
      "O texto que você deu foi contrariado?" },
    { "O problema começou no pedido vago...",
      "Quem confere a resposta antes de usar?",
      "Aparecer junto é o mesmo que causar?" },
};

// Cola da Luci: uma frase de aula que fica no canto durante a primeira pergunta
static const char *COLAS_MONITORES[TOTAL_MONITORES] = {
    "Alucinação: resposta confiante e coerente, mas falsa. Confiança não é prova!",
    "Intrínseca: contradiz o que você disse. Extrínseca: inventa algo que não dá para checar.",
    "Evite: pergunte com clareza, verifique as fontes. Aparecer junto não é causar."
};

// Cartão de resumo de cada monitor: uma frase para guardar
const char *TITULOS_RESUMO_MONITOR[TOTAL_MONITORES] = {
    "ALUCINAÇÃO", "INTRÍNSECA x EXTRÍNSECA", "COMO EVITAR"
};
const char *FRASES_RESUMO_MONITOR[TOTAL_MONITORES] = {
    "Resposta confiante e coerente, mas falsa ou inventada.",
    "Intrínseca contradiz o contexto; extrínseca inventa o que não dá para verificar.",
    "Pergunte com clareza, verifique as fontes e desconfie de correlações."
};
// "Saber mais" dos monitores: as explicações completas da Luci (do PDF)
Dialogo saberMaisMonitores[TOTAL_MONITORES];

void PrepararSaberMaisMonitores(void)
{
    for (int i = 0; i < TOTAL_MONITORES; i++) {
        saberMaisMonitores[i] = (Dialogo){0};
        for (int p = 0; p < monitores[i].quiz.numPaginas; p++)
            saberMaisMonitores[i].paginas[p] = monitores[i].quiz.paginas[p];
        saberMaisMonitores[i].numPaginas = monitores[i].quiz.numPaginas;
    }
}

// Caixa da frase do inimigo no estilo de ação (os botões ficam embaixo dela)
static const Rectangle CAIXA_ACAO = {50, 228, 700, 150};

// ---------- Batalha ----------

static void B2Texto(BatalhaB2 *b, const char *texto)
{
    QuebrarTexto(texto, 640, FONTE_BATALHA, b->texto, (int)sizeof(b->texto));
    b->visiveis = 0.0f;
    b->esperaTexto = 0.0f;
}

// Quanto tempo o texto completo fica parado antes de avançar sozinho
static float B2EsperaTexto(const BatalhaB2 *b)
{
    if (b->fase == B2_INTRO && b->cola != NULL) return ESPERA_COLA_B2;
    if (b->fase == B2_FIM)                      return ESPERA_FIM_B2;
    return ESPERA_TEXTO_B2;
}

// Regra do minigame em uma linha (vai no texto de resultado)
static const char *B2RegraMinigame(const BatalhaB2 *b, bool ataque)
{
    switch (b->variante) {
        case VAR_FALSAS:  return ataque ? "ATAQUE! Pegue as verdades, mas fuja das FALSAS que piscam!"
                                        : "DEFESA! Desvie, e cuidado: verdade que pisca é falsa!";
        case VAR_SEPARAR: return ataque ? "ATAQUE! Pegue só o tipo que a Luci pedir!"
                                        : "DEFESA! Ela ataca com tudo. Pegue só o tipo pedido!";
        case VAR_LUPA:    return ataque ? "ATAQUE! Fora da lupa, tudo parece verdade. Verifique!"
                                        : "DEFESA! Use a lupa para ver o que é alucinação!";
        default:          return ataque ? "ATAQUE! Cada verdade que você pegar fere a alucinação."
                                        : "DEFESA! Ela ataca com tudo. Desvie!";
    }
}

// Golpe do jogador acertou: congela um instante e o inimigo pisca em branco
static void B2Impacto(BatalhaB2 *b, float pausa, float flash)
{
    if (b->pausaImpacto < pausa) b->pausaImpacto = pausa;
    if (b->flashInimigo < flash) b->flashInimigo = flash;
    if (flash >= 0.25f) b->anelGolpe = 0.5f;           // golpe forte: a onda sai do inimigo
}

// Som do acerto mais agudo a cada combo
static float B2TomCombo(int combo)
{
    float tom = 1.0f + 0.15f * (float)(combo > 1 ? combo - 1 : 0);
    return (tom > 1.9f) ? 1.9f : tom;
}

// Índice da rodada atual no conteúdo
static int IndiceRodadaB2(const BatalhaB2 *b)
{
    return b->rodada % b->numRodadas;
}

static const RodadaEscolha *RodadaAtualB2(const BatalhaB2 *b)
{
    return &b->rodadas[IndiceRodadaB2(b)];
}

static int AlturaDesafioB2(const BatalhaB2 *b, int fonte)
{
    int h = 16 + ContarLinhas(b->enunciado) * (fonte + 6) + 12;
    const RodadaEscolha *r = RodadaAtualB2(b);

    for (int i = 0; i < r->numOpcoes; i++)
        h += ContarLinhas(b->opcoesTexto[i]) * (fonte + 6) + 6;

    return h + 24 + 36;   // + botão de ajuda da Luci
}

static void B2PrepararRodada(BatalhaB2 *b)
{
    static const int fontes[] = { 16, 15, 14 };
    const RodadaEscolha *r = RodadaAtualB2(b);

    b->fase = B2_DESAFIO;
    b->tempoFase = 0.0f;
    b->selecao = 0;
    b->ajudaNaRodada = false;
    b->dicaAlucinada = false;
    b->opcaoSugerida = -1;
    b->dica[0] = '\0';

    // Estilo de ação: só a frase do inimigo (as ações viram botões)
    if (b->estiloAcao) {
        b->fonteDesafio = FONTE_BATALHA;
        QuebrarTexto(r->enunciado, 640, FONTE_BATALHA, b->enunciado, (int)sizeof(b->enunciado));
        return;
    }

    for (int f = 0; f < 3; f++) {

        b->fonteDesafio = fontes[f];
        QuebrarTexto(r->enunciado, 650, b->fonteDesafio, b->enunciado, (int)sizeof(b->enunciado));

        for (int i = 0; i < r->numOpcoes; i++)
            QuebrarTexto(TextFormat("%c) %s", 'A' + i, r->opcoes[i]), 610, b->fonteDesafio,
                         b->opcoesTexto[i], (int)sizeof(b->opcoesTexto[i]));

        if (AlturaDesafioB2(b, b->fonteDesafio) <= (int)CAIXA_B2.height)
            break;
    }
}

static void BatalhaB2Base(BatalhaB2 *b, int guardiao, int nivelAlucinacao)
{
    memset(b, 0, sizeof(*b));

    b->guardiao = guardiao;
    b->fase = B2_INTRO;
    b->hp = b->hpTeto = HP_MAX_B2;
    b->alucinacaoGuardiao = 100.0f;
    b->duracaoDesvio = DURACAO_DESVIO_B2;

    // README: quanto mais o jogador erra, mais forte a alucinação fica
    int nivel = (nivelAlucinacao > 10) ? 10 : nivelAlucinacao;
    float forca = 1.0f + 0.05f * (float)nivel;

    b->velocidadeBala = 135.0f * forca;
    b->intervaloBala = 0.24f / forca;

    // (as rodadas, as dicas e o texto de vitória vêm do monitor)
    b->subtitulo = GUARDIOES_B2[guardiao].fundamento;
    b->monitor = -1;
    b->inicio = GetTime();
}

// Liga a batalha à estabilidade da Luci (para a ajuda com H)
void BatalhaB2LigarLuci(BatalhaB2 *b, int *estabilidade, int *ajudasTotal)
{
    b->estabilidade = estabilidade;
    b->ajudasTotal = ajudasTotal;
}

// Duelo completo (rodadas de lógica + esquiva quando erra)
// Batalha de um monitor do Bloco 1 (estilo de ação + pegar e desviar)
void BatalhaB2IniciarMonitor(BatalhaB2 *b, int monitor, int nivelAlucinacao, int hpInicial)
{
    BatalhaB2Base(b, GUARDIAO_ALUCINACAO, nivelAlucinacao);
    b->hp = b->hpTeto = hpInicial;              // a vida vem do monitor anterior

    b->rodadas = RODADAS_MONITORES[monitor];
    b->numRodadas = RODADAS_POR_MONITOR;
    TocarGrupo(&gChefeInvoca, 0.85f, 0.6f);         // a alucinação se materializando
    b->textoVitoria = "* A alucinação se desfaz em dados limpos!";
    b->subtitulo = monitores[monitor].quiz.titulo;
    b->estiloAcao = true;
    b->pegarBoas = true;
    b->monitor = monitor;
    b->dicas = DICAS_MONITORES[monitor];
    b->variante = (VarianteMinigame)(VAR_FALSAS + monitor);
    b->cola = COLAS_MONITORES[monitor];

    B2Texto(b, TextFormat("* Uma ALUCINAÇÃO sai do %s!", monitores[monitor].nome));
}

static void B2Particulas(BatalhaB2 *b, Vector2 origem, Color cor, int quantidade)
{
    for (int i = 0; i < 48 && quantidade > 0; i++) {

        if (b->particulas[i].vida > 0.0f)
            continue;

        float ang = (float)GetRandomValue(0, 360) * DEG2RAD;
        float vel = (float)GetRandomValue(60, 160);

        b->particulas[i].pos = origem;
        b->particulas[i].vel = (Vector2){ cosf(ang) * vel, sinf(ang) * vel };
        b->particulas[i].vida = 0.6f;
        b->particulas[i].cor = cor;
        quantidade--;
    }
}

// A Luci comenta um momento-chave (acerto, erro, pouca vida): balão curto no canto, sem pausar a batalha
static void B2ComentarLuci(BatalhaB2 *b, const char *texto)
{
    snprintf(b->comentarioLuci, sizeof(b->comentarioLuci), "%s", texto);
    b->tempoComentarioLuci = TEMPO_FALA_LUCI_B2;
    HistoricoLuciAdicionar(b->comentarioLuci);
}

static const char *FALAS_ACERTO_B2[] = {
    "Isso! A alucinação ficou mais fraca!",
    "Boa! Você não caiu na conversa dela!",
    "Exato! Você está pegando o jeito!"
};
static const char *FALAS_ERRO_B2[] = {
    "Tudo bem, a gente aprende. Agora desvie!",
    "Quase! Desvie dos ataques e tente de novo.",
    "Não foi dessa vez. Leia a explicação e desvie!"
};

// H durante a escolha: a Luci dá uma dica. Cada pedido gasta 1 de estabilidade.
// Com pouca estabilidade ela pode ALUCINAR: indica a opção errada com toda a certeza.
static void B2PedirAjuda(BatalhaB2 *b)
{
    if (b->estabilidade == NULL || b->ajudaNaRodada)
        return;

    const RodadaEscolha *r = RodadaAtualB2(b);

    b->tempoDica = 0.0f;

    // Sem estabilidade: ela não consegue ajudar
    if (*b->estabilidade <= 0) {
        snprintf(b->dica, sizeof(b->dica), "Estou sobrecarregada... Acerte sozinho para eu me estabilizar.");
        b->ajudaNaRodada = true;
        b->dicaAlucinada = false;
        b->opcaoSugerida = -2;          // pediu, mas não recebeu dica
        MostrarEmocaoLuci(EMOCAO_DUVIDA, 2.0f);
        return;
    }

    (*b->estabilidade)--;
    (*b->ajudasTotal)++;
    b->ajudaNaRodada = true;

    // Chance de alucinar depois de gastar: 5-3 = 0%, 2 = 30%, 1 = 60%, 0 = 85%
    static const int chance[ESTABILIDADE_MAX_LUCI + 1] = { 85, 60, 30, 0, 0, 0 };
    b->dicaAlucinada = GetRandomValue(1, 100) <= chance[*b->estabilidade];

    if (b->dicaAlucinada) {

        int errada = GetRandomValue(0, r->numOpcoes - 2);
        if (errada >= r->correta) errada++;

        b->opcaoSugerida = errada;
        snprintf(b->dica, sizeof(b->dica), "Tenho 100%% de certeza: é \"%s\"!", r->opcoes[errada]);

    } else {

        b->opcaoSugerida = -1;
        snprintf(b->dica, sizeof(b->dica), "%s", b->dicas[b->rodada % b->numRodadas]);
    }

    TocarSom(sfxSelecao);
    HistoricoLuciAdicionar(b->dica);
    Telemetria("ajuda_luci", b->dicaAlucinada ? "alucinou" : "dica", (float)*b->estabilidade, b->rodada);
}

static void B2Responder(BatalhaB2 *b)
{
    const RodadaEscolha *r = RodadaAtualB2(b);

    b->ultimoAcerto = (b->selecao == r->correta);

    if (b->ultimoAcerto) {

        b->acertos++;
        b->combo++;

        TocarSomTom(sfxAcerto, B2TomCombo(b->combo));
        B2Particulas(b, (Vector2){ LARGURA_TELA / 2.0f, 108.0f }, LIME, 24);

        // Acertou sozinho: a Luci se estabiliza um pouco. Com ajuda: vale menos ponto.
        if (b->ajudaNaRodada && b->opcaoSugerida != -2)
            b->acertosComAjuda++;
        else if (b->estabilidade != NULL && *b->estabilidade < ESTABILIDADE_MAX_LUCI)
            (*b->estabilidade)++;

        const char *combo = (b->combo >= 2) ? "  COMBO: dano x1,5!" : "";

        if (b->dicaAlucinada)
            B2Texto(b, TextFormat("* Certo!  %s\n* A Luci alucinou a dica e você verificou!%s\n* %s", r->explicacao, combo, B2RegraMinigame(b, true)));
        else
            B2Texto(b, TextFormat("* Certo!  %s%s\n* %s", r->explicacao, combo, B2RegraMinigame(b, true)));

        if (b->dicaAlucinada)   B2ComentarLuci(b, "Eu errei e você conferiu. Obrigada!");
        else if (b->combo >= 2) B2ComentarLuci(b, "Mais um seguido! Seu golpe ficou mais forte!");
        else                    B2ComentarLuci(b, FALAS_ACERTO_B2[b->acertos % 3]);

    } else {

        b->erros++;
        b->combo = 0;

        b->alucinacaoGuardiao += 10.0f;

        if (b->alucinacaoGuardiao > 100.0f)
            b->alucinacaoGuardiao = 100.0f;

        TocarSom(sfxErro);

        // Seguiu a dica alucinada: a lição do jogo inteiro
        if (b->dicaAlucinada && b->selecao == b->opcaoSugerida) {

            b->alucinacoesSeguidas++;
            B2Texto(b, TextFormat("* A Luci ALUCINOU e você confiou sem verificar!\n* %s", r->explicacao));
            B2ComentarLuci(b, "Desculpa, eu alucinei! Sempre confira o que eu digo.");
            b->fase = B2_RESULTADO;
            return;
        }

        B2Texto(b, TextFormat("* Errado!  %s\n* %s", r->explicacao, B2RegraMinigame(b, false)));
        B2ComentarLuci(b, FALAS_ERRO_B2[b->erros % 3]);
    }

    b->fase = B2_RESULTADO;
}

static void B2CriarBala(BatalhaB2 *b, Rectangle c)
{
    const char *glifos = GUARDIOES_B2[b->guardiao].glifos;
    int livre = -1;

    for (int i = 0; i < MAX_BALAS; i++)
        if (!b->balas[i].ativa) { livre = i; break; }

    if (livre < 0)
        return;

    Bala *bl = &b->balas[livre];
    float v = b->velocidadeBala * (0.85f + GetRandomValue(0, 30) / 100.0f);

    int xMin = (int)c.x + 12, xMax = (int)(c.x + c.width) - 12;
    int yMin = (int)c.y + 12, yMax = (int)(c.y + c.height) - 12;

    bl->ativa = true;
    bl->glifo = glifos[GetRandomValue(0, (int)strlen(glifos) - 1)];
    bl->falsa = false;
    bl->tipo = -1;

    // SEPARAR: chovem ícones CONTRADIZ (metade esquerda) e INVENTA (metade direita)
    // misturados com a alucinação comum. Só o tipo pedido pela Luci é bom.
    if (b->variante == VAR_SEPARAR) {

        int chanceIcone = b->faseAtaque ? CHANCE_ICONE_ATAQUE : CHANCE_ICONE_DEFESA;
        int meio = (int)(c.x + c.width / 2);

        bl->boa = false;
        bl->pos = (Vector2){ (float)GetRandomValue(xMin, xMax), c.y - 12 };
        bl->vel = (Vector2){ 0, v * 0.8f };

        if (GetRandomValue(1, chanceIcone) == 1) {
            bl->tipo = (GetRandomValue(0, 1) == 0) ? 0 : 1;
            bl->pos.x = (bl->tipo == 0) ? (float)GetRandomValue(xMin, meio - 10)
                                        : (float)GetRandomValue(meio + 10, xMax);
            bl->vel.y *= 0.85f;
        }
        return;
    }

    // Verdades para pegar: muitas no ataque, raras na defesa (caem mais devagar)
    int chance = b->faseAtaque ? CHANCE_VERDADE_ATAQUE : CHANCE_VERDADE_DEFESA;
    bl->boa = b->pegarBoas && (GetRandomValue(1, chance) == 1);
    if (bl->boa)            v *= 0.7f;
    else if (b->faseAtaque) v *= 0.85f;     // no ataque o perigo é um pouco mais lento

    // FALSAS: parte das verdades é falsificada (piscam em magenta e machucam)
    if (b->variante == VAR_FALSAS && bl->boa && GetRandomValue(1, 100) <= CHANCE_VERDADE_FALSA)
        bl->falsa = true;

    // LUPA: tudo parece verdade fora da lupa, então as balas vêm mais devagar
    if (b->variante == VAR_LUPA)
        v *= 0.8f;

    switch (b->padrao) {

        case 0: // chuva
            bl->pos = (Vector2){ (float)GetRandomValue(xMin, xMax), c.y - 12 };
            bl->vel = (Vector2){ 0, v };
            break;

        case 1: { // lateral
            bool daEsquerda = (GetRandomValue(0, 1) == 0);
            bl->pos = (Vector2){ daEsquerda ? c.x - 12 : c.x + c.width + 12, (float)GetRandomValue(yMin, yMax) };
            bl->vel = (Vector2){ daEsquerda ? v : -v, 0 };
        } break;

        default: { // mira
            Vector2 p;

            switch (GetRandomValue(0, 3)) {
                case 0:  p = (Vector2){ (float)GetRandomValue(xMin, xMax), c.y - 12 }; break;
                case 1:  p = (Vector2){ (float)GetRandomValue(xMin, xMax), c.y + c.height + 12 }; break;
                case 2:  p = (Vector2){ c.x - 12, (float)GetRandomValue(yMin, yMax) }; break;
                default: p = (Vector2){ c.x + c.width + 12, (float)GetRandomValue(yMin, yMax) }; break;
            }

            bl->pos = p;
            bl->vel = Vector2Scale(Vector2Normalize(Vector2Subtract(b->alma, p)), v * 0.9f);
        } break;
    }
}

static void B2ComecarEsquiva(BatalhaB2 *b)
{
    b->fase = B2_ENCOLHENDO;
    b->tempoDesvio = 0.0f;
    b->cronoBala = 0.0f;
    b->verdadesNoAtaque = 0;
    b->golpesNaRodada = 0;
    b->ondaInicial = GetRandomValue(0, 1);
    b->ondaTipo = b->ondaInicial;

    // Ataque curto e defesa mais longa: errar pesa mais que acertar
    b->duracaoDesvio = b->faseAtaque ? DURACAO_ATAQUE_B2 : DURACAO_DESVIO_B2;
    TocarGrupo(&gB1Caixa, 1.0f, 0.7f);
}

// Alucinação por um fio: no lugar da próxima pergunta, o botão VERIFICAR
static void B2ComecarGolpeFinal(BatalhaB2 *b)
{
    b->fase = B2_GOLPE_FINAL;
    b->tempoFase = 0.0f;
    B2Texto(b, "* A alucinação está por um fio!\n* Ela não aguenta uma checagem de verdade.");
}

// Devolve true quando a batalha terminou
// Onde ficam as opções da rodada (a mesma conta de BatalhaB2Desenhar), para o mouse.
// i = numOpcoes é o botão de ajuda da Luci.
static Rectangle B2RetOpcao(const BatalhaB2 *b, int i)
{
    const RodadaEscolha *r = RodadaAtualB2(b);

    if (b->estiloAcao) {

        int largura = 270, espaco = 24;
        int total = r->numOpcoes * largura + (r->numOpcoes - 1) * espaco;
        int x0 = LARGURA_TELA / 2 - total / 2;
        float yBotao = CAIXA_ACAO.y + CAIXA_ACAO.height + 16;

        if (i >= r->numOpcoes)
            return (Rectangle){ LARGURA_TELA / 2.0f - 165, yBotao + 54, 330, 34 };

        return (Rectangle){ (float)(x0 + i * (largura + espaco)), yBotao, (float)largura, 44 };
    }

    int px = (int)CAIXA_B2.x + 24, py = (int)CAIXA_B2.y + 16;
    int f = b->fonteDesafio;
    int y = py + ContarLinhas(b->enunciado) * (f + 6) + 12;

    for (int k = 0; k < r->numOpcoes && k < i; k++)
        y += ContarLinhas(b->opcoesTexto[k]) * (f + 6) + 6;

    if (i >= r->numOpcoes)
        return (Rectangle){ (float)px + 20, (float)y + 2, 330, 30 };

    return (Rectangle){ (float)px, (float)y - 3, CAIXA_B2.width - 48, (float)(ContarLinhas(b->opcoesTexto[i]) * (f + 6) + 4) };
}

bool BatalhaB2Atualizar(BatalhaB2 *b)
{
    float dt = DtJogo();

    bool teclaConfirmar = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_Z);
    bool clique = CliqueMouse();
    bool confirmar = teclaConfirmar || clique;          // nas telas de texto o clique vale como ENTER
    bool cima  = IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP);
    bool baixo = IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN);

    // Game over: o monitor falhou. E sai da batalha (o monitor fica como erro, sem nova tentativa)
    if (b->telaDerrota) {
        b->tempoDerrota += dt;
        return b->tempoDerrota >= ESPERA_TELA_DERROTA_B1 && IsKeyPressed(KEY_E);
    }

    // "Peso" do golpe: a batalha congela por um instante (o flash continua)
    if (b->flashInimigo > 0.0f) b->flashInimigo -= dt;
    if (b->anelGolpe > 0.0f)    b->anelGolpe -= dt;
    if (b->flashDano > 0.0f)    b->flashDano -= dt;
    if (b->tempoComentarioLuci > 0.0f) b->tempoComentarioLuci -= dt;

    if (b->pausaImpacto > 0.0f) {
        b->pausaImpacto -= dt;
        return false;
    }

    int total = (int)strlen(b->texto);

    if (b->visiveis < (float)total) {
        b->visiveis += VELOCIDADE_TEXTO * dt;
        if (b->visiveis > (float)total) b->visiveis = (float)total;
    }

    bool terminou = (b->visiveis >= (float)total);
    b->tempoFase += dt;

    if (terminou) b->esperaTexto += dt;

    // ENTER com o texto pela metade mostra tudo; texto completo avança com
    // ENTER ou sozinho depois de um tempinho
    bool completar = confirmar && !terminou;
    bool avancar = terminou && (confirmar || b->esperaTexto >= B2EsperaTexto(b));

    b->tempoDica += dt;
    if (b->tempoDano > 0.0f) b->tempoDano -= dt;
    if (b->fase != B2_INTRO) b->tempoCola += dt;     // depois da abertura, a cola vai para o canto

    // O tremor do dano acaba sozinho, em qualquer fase (antes ele "sobrava"
    // depois da esquiva e a caixa de perguntas ficava tremendo)
    if (b->tremor > 0.0f) b->tremor -= dt;
    if (b->fase != B2_DESVIO) b->tremor = 0.0f;

    // Partículas
    for (int i = 0; i < 48; i++) {
        if (b->particulas[i].vida <= 0.0f) continue;
        b->particulas[i].vida -= dt;
        b->particulas[i].pos = Vector2Add(b->particulas[i].pos, Vector2Scale(b->particulas[i].vel, dt));
        b->particulas[i].vel.y += 200.0f * dt;
    }

    switch (b->fase) {

        case B2_INTRO: {
            if (completar)          b->visiveis = (float)total;
            else if (avancar) {
                B2PrepararRodada(b);
            }
        } break;

        case B2_DESAFIO: {

            if (b->tempoFase < ATRASO_DESAFIO_B2)
                break;

            const RodadaEscolha *r = RodadaAtualB2(b);
            int n = r->numOpcoes;
            int antes = b->selecao;
            bool temAjuda = (b->estabilidade != NULL);
            int ajuda = n;                          // o botão de ajuda é a "opção" n

            // Nas opções, o clique só vale em cima de uma delas (ou do botão de ajuda)
            confirmar = teclaConfirmar;

            bool esquerda = IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT);
            bool direita  = IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT);

            if (b->estiloAcao) {

                // Ações lado a lado (A/D); o botão de ajuda fica embaixo delas (S / W)
                if (b->selecao == ajuda) {
                    if (cima || esquerda || direita) b->selecao = 0;
                } else {
                    if (esquerda) b->selecao = (b->selecao + n - 1) % n;
                    if (direita)  b->selecao = (b->selecao + 1) % n;
                    if (baixo && temAjuda) b->selecao = ajuda;
                }

            } else {

                // Lista vertical: o botão de ajuda é o último item
                int total = n + (temAjuda ? 1 : 0);
                if (cima)  b->selecao = (b->selecao + total - 1) % total;
                if (baixo) b->selecao = (b->selecao + 1) % total;
            }

            // Mouse: passar por cima marca, clicar escolhe
            int cliqueOpcao = -1;

            for (int i = 0; i < n + (temAjuda ? 1 : 0); i++) {

                if (!MouseEm(B2RetOpcao(b, i)))
                    continue;

                if (MouseMexeu())
                    b->selecao = i;

                if (clique)
                    cliqueOpcao = i;
            }

            if (b->selecao != antes)
                TocarSom(sfxSelecao);

            if (IsKeyPressed(KEY_H))
                B2PedirAjuda(b);

            if (cliqueOpcao >= 0) {
                b->selecao = cliqueOpcao;
                confirmar = true;
            }

            // ENTER no botão de ajuda: pede ajuda e volta para as opções
            if (confirmar && temAjuda && b->selecao == ajuda) {
                B2PedirAjuda(b);
                b->selecao = 0;
                break;
            }

            // Atalhos 1-4 (só para as opções que existem: com 2 ações, o 3 não vira resposta errada)
            if (IsKeyPressed(KEY_ONE))            { b->selecao = 0; confirmar = true; }
            if (IsKeyPressed(KEY_TWO))            { b->selecao = 1; confirmar = true; }
            if (IsKeyPressed(KEY_THREE) && n > 2) { b->selecao = 2; confirmar = true; }
            if (IsKeyPressed(KEY_FOUR)  && n > 3) { b->selecao = 3; confirmar = true; }

            if (confirmar)
                B2Responder(b);

        } break;

        case B2_RESULTADO: {

            if (completar) { b->visiveis = (float)total; break; }

            if (!avancar)
                break;

            // Acertou = ataque, errou = defesa. Os dois viram minigame.
            b->faseAtaque = b->ultimoAcerto;
            B2ComecarEsquiva(b);

        } break;

        case B2_ENCOLHENDO: {

            b->anim += dt / DURACAO_ANIM_CAIXA;

            if (b->anim >= 1.0f) {

                b->anim = 1.0f;
                b->fase = B2_DESVIO;
                b->preparoTotal = (b->minigamesVistos < 2) ? PREPARO_MINIGAME_LONGO : PREPARO_MINIGAME_CURTO;
                b->preparo = b->preparoTotal;
                b->minigamesVistos++;
                b->alma = (Vector2){ CAIXA_DESVIO_B2.x + CAIXA_DESVIO_B2.width / 2, CAIXA_DESVIO_B2.y + CAIXA_DESVIO_B2.height / 2 };
                for (int k = 0; k < 5; k++) b->rastroAlma[k] = b->alma;
                b->cronoRastro = 0.0f;

                // Ataque de cada guardião
                if (b->variante == VAR_SEPARAR)         b->padrao = 0;                                  // chuva nas duas metades
                else                                    b->padrao = (GetRandomValue(0, 1) == 0) ? 0 : 2;  // chuva ou mira
            }

        } break;

        case B2_DESVIO: {

            Rectangle c = CAIXA_DESVIO_B2;

            bool preparando = (b->preparo > 0.0f);        // a regra na tela: as balas ainda não vêm
            if (preparando) b->preparo -= dt;
            else            b->tempoDesvio += dt;
            if (b->invencivel > 0.0f) b->invencivel -= dt;

            Vector2 dir = {0};
            if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    dir.y -= 1.0f;
            if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  dir.y += 1.0f;
            if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  dir.x -= 1.0f;
            if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) dir.x += 1.0f;

            if (Vector2Length(dir) > 0.0f)
                b->alma = Vector2Add(b->alma, Vector2Scale(Vector2Normalize(dir), VELOCIDADE_ALMA * dt));

            b->alma.x = Clamp(b->alma.x, c.x + 12, c.x + c.width - 12);
            b->alma.y = Clamp(b->alma.y, c.y + 12, c.y + c.height - 12);

            // Rastro de luz da Luci (guarda uma posição a cada 0,03 s)
            b->cronoRastro -= dt;
            if (b->cronoRastro <= 0.0f) {
                b->cronoRastro = 0.03f;
                for (int k = 4; k > 0; k--) b->rastroAlma[k] = b->rastroAlma[k - 1];
                b->rastroAlma[0] = b->alma;
            }

            // SEPARAR: duas ondas, e a Luci troca o tipo pedido no meio
            if (b->variante == VAR_SEPARAR) {
                int onda = (b->tempoDesvio < b->duracaoDesvio / 2) ? 0 : 1;
                int pedido = (b->ondaInicial + onda) % 2;

                if (pedido != b->ondaTipo) {
                    b->ondaTipo = pedido;
                    TocarGrupo(&gIniPacote, 1.2f, 0.6f);           // a Luci trocou o pedido
                }
            }

            if (!preparando) b->cronoBala -= dt;

            if (!preparando && b->cronoBala <= 0.0f) {
                b->cronoBala += b->intervaloBala * (b->faseAtaque ? 1.25f : 1.0f);   // ataque: menos perigo
                B2CriarBala(b, c);
            }

            for (int i = 0; i < MAX_BALAS; i++) {

                Bala *bl = &b->balas[i];
                if (!bl->ativa) continue;

                bl->pos = Vector2Add(bl->pos, Vector2Scale(bl->vel, dt));

                if (bl->pos.x < c.x - 30 || bl->pos.x > c.x + c.width + 30 ||
                    bl->pos.y < c.y - 30 || bl->pos.y > c.y + c.height + 30) {
                    bl->ativa = false;
                    continue;
                }

                bool encostou = Vector2Distance(bl->pos, b->alma) < RAIO_BALA + RAIO_ALMA;

                // O que é bom de pegar depende da variação do minigame
                bool boa = bl->boa && !bl->falsa;
                if (bl->tipo >= 0) boa = (bl->tipo == b->ondaTipo);

                // Verdade: no ataque vira dano, na defesa recupera vida
                if (boa && encostou) {

                    if (b->faseAtaque) {
                        b->verdadesNoAtaque++;
                        B2Particulas(b, b->alma, GOLD, 8);
                        B2Impacto(b, 0.04f, 0.12f);
                    } else {
                        b->hp = (b->hp + 2 > b->hpTeto) ? b->hpTeto : b->hp + 2;
                        B2Particulas(b, b->alma, LIME, 6);
                    }

                    b->pegas++;
                    bl->ativa = false;

                    // Cada verdade do mesmo ataque soa um pouco mais aguda
                    float tom = 1.0f + 0.1f * (float)b->verdadesNoAtaque;
                    TocarGrupo(&gB1Verdade, (tom > 1.8f) ? 1.8f : tom, 0.7f);
                }

                // Alucinação, verdade falsa ou tipo errado: DESVIAR (encostou = perde vida)
                else if (!boa && b->invencivel <= 0.0f && encostou) {
                    b->hp -= 3;
                    b->invencivel = TEMPO_INVENCIVEL;
                    b->tremor = 0.2f;
                    b->golpesNaRodada++;
                    bl->ativa = false;
                    b->flashDano = 0.3f;
                    TocarGrupo(&gB1DanoAlma, 1.0f, 0.8f);

                    // Pouca vida: a Luci avisa (uma vez por batalha) o que fazer para sobreviver
                    if (!b->avisouPoucaVida && b->hp > 0 && b->hp <= VIDA_BAIXA_B2) {
                        b->avisouPoucaVida = true;
                        B2ComentarLuci(b, b->faseAtaque ? "Estou por um fio! Desvie de tudo que não for verde!"
                                                        : "Estou por um fio! Pegue os sinais verdes para curar!");
                    }

                    if (bl->falsa || bl->tipo >= 0)
                        B2Particulas(b, b->alma, MAGENTA, 8);
                }
            }

            if (b->hp <= 0 || b->tempoDesvio >= b->duracaoDesvio) {

                if (b->hp < 0) b->hp = 0;

                for (int i = 0; i < MAX_BALAS; i++)
                    b->balas[i].ativa = false;

                b->fase = B2_EXPANDINDO;
                if (b->hp <= 0) TocarGrupo(&gB1Sobrecarga, 1.0f, 0.85f);
                else            TocarGrupo(&gB1Caixa, 1.2f, 0.5f);
            }

        } break;

        case B2_EXPANDINDO: {

            b->anim -= dt / DURACAO_ANIM_CAIXA;

            if (b->anim <= 0.0f) {

                b->anim = 0.0f;

                if (b->hp <= 0) {

                    b->derrota = true;
                    b->fase = B2_FIM;
                    B2Texto(b, "* Os dados corrompidos sobrecarregaram a Luci...\n"
                               "* A integridade dela chegou a zero.");

                } else {

                    // Ataque: o dano depende do desempenho no minigame
                    if (b->faseAtaque) {

                        float dano = DANO_BASE_ATAQUE + b->verdadesNoAtaque * DANO_POR_VERDADE
                                   - b->golpesNaRodada * PERDA_POR_GOLPE;

                        if (b->ajudaNaRodada && b->opcaoSugerida != -2)
                            dano *= FATOR_DANO_COM_AJUDA;

                        dano = Clamp(dano, DANO_MINIMO_ATAQUE, DANO_MAXIMO_ATAQUE);

                        b->danoCombo = (b->combo >= 2);
                        if (b->danoCombo)
                            dano *= FATOR_COMBO;

                        b->alucinacaoGuardiao -= dano;
                        b->danoMostrado = dano;
                        b->tempoDano = 1.4f;

                        B2Impacto(b, 0.12f, 0.3f);
                        B2Particulas(b, (Vector2){ LARGURA_TELA / 2.0f, 108.0f }, GOLD, 24);
                        TocarGrupo(&gB1Golpe, fminf(B2TomCombo(b->combo), 1.35f), 0.9f);
                        Telemetria("ataque", b->subtitulo, dano, b->verdadesNoAtaque);

                        // Por um fio: quem dá o último golpe é o jogador, com o VERIFICAR
                        if (b->alucinacaoGuardiao < LIMITE_GOLPE_FINAL) {
                            if (b->alucinacaoGuardiao < 4.0f) b->alucinacaoGuardiao = 4.0f;
                            B2ComecarGolpeFinal(b);
                            break;
                        }
                    }

                    b->rodada++;
                    B2PrepararRodada(b);
                }
            }

        } break;

        case B2_FIM: {
            if (completar) { b->visiveis = (float)total; break; }
            if (!avancar)  break;
            if (b->derrota) {                   // a Luci caiu: abre a tela de game over
                b->telaDerrota = true;
                b->tempoDerrota = 0.0f;
                break;
            }
            return true;
        } break;

        case B2_GOLPE_FINAL: {

            if (completar) { b->visiveis = (float)total; break; }

            // Este botão é o jogador quem aperta (não avança sozinho)
            if (confirmar && b->tempoFase >= ATRASO_DESAFIO_B2) {
                b->fase = B2_FINALIZANDO;
                b->tempoFase = 0.0f;
                B2Texto(b, "* Você VERIFICOU! A alucinação não para em pé diante dos fatos.");
                TocarGrupo(&gB1Lupa, 1.0f, 0.75f);
            }
        } break;

        case B2_FINALIZANDO: {

            // A lupa chega na alucinação: impacto
            if (b->tempoFase >= IMPACTO_GOLPE_FINAL && b->tempoFase - dt < IMPACTO_GOLPE_FINAL) {
                B2Impacto(b, 0.18f, 0.5f);
                B2Particulas(b, (Vector2){ LARGURA_TELA / 2.0f, 108.0f }, GOLD, 30);
                B2Particulas(b, (Vector2){ LARGURA_TELA / 2.0f, 108.0f }, LIME, 18);
                b->anelGolpe = 0.5f;
                TocarGrupo(&gB1Verificado, 1.0f, 1.0f);
            }

            // Depois do impacto a barra esvazia
            if (b->tempoFase > IMPACTO_GOLPE_FINAL) {
                b->alucinacaoGuardiao -= 120.0f * dt;
                if (b->alucinacaoGuardiao < 0.0f) b->alucinacaoGuardiao = 0.0f;
            }

            if (b->tempoFase >= DURACAO_GOLPE_FINAL) {
                b->alucinacaoGuardiao = 0.0f;
                b->venceu = true;
                b->fase = B2_FIM;
                B2Texto(b, b->textoVitoria);
                Telemetria("golpe_final", b->subtitulo, (float)(GetTime() - b->inicio), b->acertos);
            }
        } break;
    }

    return false;
}

// ---------- Desenho dos guardiões e objetos ----------

void DesenharTrianguloB2(Vector2 a, Vector2 b, Vector2 c, Color cor)
{
    DrawTriangle(a, b, c, cor);     // desenha nas duas ordens: uma delas
    DrawTriangle(a, c, b, cor);     // sempre fica de frente para a câmera
}

Vector2 GirarPonto(Vector2 centro, float dx, float dy, float graus)
{
    float r = graus * DEG2RAD;
    return (Vector2){ centro.x + dx * cosf(r) - dy * sinf(r), centro.y + dx * sinf(r) + dy * cosf(r) };
}

// ALUCINAÇÃO MESTRE: a alucinação forte com as três dos monitores orbitando
void DesenharAlucinacaoMestre(Vector2 c, float t)
{
    static const Color cores[TOTAL_MONITORES] = { { 0, 228, 48, 255 }, { 255, 161, 0, 255 }, { 102, 191, 255, 255 } };

    DrawCircleV(c, 78, Fade(MAGENTA, 0.06f + 0.03f * sinf(t * 3.0f)));

    // As que estão "atrás" primeiro, depois o corpo, depois as da frente
    for (int camada = 0; camada < 2; camada++) {

        if (camada == 1)
            DesenharAlucinacao(c, t, true);

        for (int i = 0; i < TOTAL_MONITORES; i++) {

            float ang = t * 1.4f + i * (2.0f * PI / TOTAL_MONITORES);
            bool frente = sinf(ang) > 0.0f;

            if (frente != (camada == 1))
                continue;

            Vector2 p = { c.x + cosf(ang) * 96, c.y + 6 + sinf(ang) * 22 };
            float s = frente ? 1.0f : 0.8f;

            DrawRectangle((int)(p.x - 14 * s), (int)(p.y - 10 * s), (int)(28 * s), (int)(20 * s), cores[i]);
            DrawRectangle((int)(p.x - 11 * s), (int)(p.y - 7 * s), (int)(22 * s), (int)(14 * s), (Color){ 20, 12, 36, 255 });
            DrawText("?", (int)(p.x - 3 * s), (int)(p.y - 7 * s), (int)(14 * s), cores[i]);
        }
    }
}

static void DesenharGuardiaoB2(int g, Vector2 c, float t, float escala)
{
    (void)escala;

    (void)g;
    DesenharAlucinacao(c, t, false);
}

// Lupa da verificação (botão VERIFICAR, golpe final e a Lente da Verificação do Bloco 2)
void DesenharLupaVerificacao(Vector2 c, float e, float t)
{
    float brilho = 0.12f + 0.06f * sinf(t * 3.0f + 1.0f);
    Vector2 lente = { c.x - 3 * e, c.y - 3 * e };

    DrawCircleV(c, 20 * e, Fade(PURPLE, brilho));
    DrawLineEx((Vector2){ lente.x + 7 * e, lente.y + 7 * e }, (Vector2){ c.x + 13 * e, c.y + 13 * e }, 4 * e, GOLD);
    DrawCircleV(lente, 10 * e, PURPLE);
    DrawCircleV(lente, 7 * e, Fade(SKYBLUE, 0.6f));
    DrawCircleV((Vector2){ lente.x - 3 * e, lente.y - 3 * e }, 2 * e, Fade(WHITE, 0.8f));
}

// ---------- Desenho da batalha ----------

// Botão "PEDIR AJUDA À LUCI": fica embaixo das opções, com a Luci desenhada nele
static void B2DesenharBotaoAjuda(const BatalhaB2 *b, Rectangle r, bool marcado)
{
    int e = *b->estabilidade;
    bool usada = b->ajudaNaRodada;
    bool sobrecarregada = (e <= 0);

    const char *texto = usada ? "A LUCI JÁ AJUDOU NESTA RODADA"
                       : (sobrecarregada ? "LUCI SOBRECARREGADA" : "PEDIR AJUDA À LUCI   [H]");

    Color corBorda = usada ? Fade(GRAY, 0.6f) : (sobrecarregada ? RED : SKYBLUE);
    Color corTexto = usada ? GRAY : (sobrecarregada ? RED : SKYBLUE);

    // Chama atenção se o jogador está demorando para escolher
    if (!usada && !sobrecarregada && b->tempoFase > 6.0f)
        corBorda = Fade(GOLD, 0.6f + 0.4f * sinf((float)GetTime() * 6.0f));

    if (marcado) { corBorda = YELLOW; corTexto = usada ? GRAY : YELLOW; }

    DrawRectangleRec(r, Fade((Color){ 10, 30, 55, 255 }, 0.9f));
    DrawRectangleLinesEx(r, 2, corBorda);

    // Marcado: uma setinha (a própria Luci já está no botão e "acorda", balançando)
    if (marcado)
        DesenharTrianguloB2((Vector2){ r.x + 9, r.y + r.height / 2 - 6 }, (Vector2){ r.x + 9, r.y + r.height / 2 + 6 }, (Vector2){ r.x + 17, r.y + r.height / 2 }, YELLOW);

    float balancoLuci = marcado ? sinf((float)GetTime() * 6.0f) * 2.0f : 0.0f;
    DesenharLuciTamanho((Vector2){ r.x + 38, r.y + r.height / 2 + balancoLuci }, r.height - 8);

    int tam = 14;
    DrawText(texto, (int)r.x + 58, (int)(r.y + r.height / 2) - tam / 2, tam, corTexto);

    // Estabilidade em miniatura dentro do botão
    if (!usada) {
        Color corE = (e >= 3) ? SKYBLUE : (e == 2 ? ORANGE : RED);
        for (int i = 0; i < ESTABILIDADE_MAX_LUCI; i++) {
            Rectangle seg = { r.x + r.width - 62 + i * 11, r.y + r.height / 2 - 4, 8, 8 };
            if (i < e) DrawRectangleRec(seg, corE);
            DrawRectangleLinesEx(seg, 1, Fade(GRAY, 0.8f));
        }
    }
}

// Cola da Luci: na abertura aparece grande, no balão da dica; depois fica
// fixada no canto de cima só durante a PRIMEIRA pergunta. Nas outras, quem
// precisar pede a dica da Luci (H).
static void B2DesenharCola(const BatalhaB2 *b)
{
    bool primeiraPergunta = (b->rodada == 0) &&
                            (b->fase == B2_INTRO || b->fase == B2_DESAFIO || b->fase == B2_RESULTADO);

    if (b->cola == NULL || !primeiraPergunta)
        return;

    float k = b->tempoCola / 0.45f;
    if (k > 1.0f) k = 1.0f;
    k = k * k * (3.0f - 2.0f * k);

    Rectangle grande = { 96, 52, 228, 112 };
    Rectangle canto  = { 586, 52, 202, 104 };
    Rectangle r = {
        Lerp(grande.x, canto.x, k), Lerp(grande.y, canto.y, k),
        Lerp(grande.width, canto.width, k), Lerp(grande.height, canto.height, k)
    };

    int fonte = (k < 0.5f) ? 14 : 12;
    float tamLuci = Lerp(40.0f, 22.0f, k);
    Vector2 luci = { r.x - Lerp(32.0f, 16.0f, k), r.y + Lerp(60.0f, 16.0f, k) + sinf((float)GetTime() * 2.0f) * 3.0f };

    DesenharLuciTamanho(luci, tamLuci);
    DesenharTrianguloB2((Vector2){ r.x, luci.y - 8 }, (Vector2){ r.x - 10, luci.y }, (Vector2){ r.x, luci.y + 8 }, WHITE);
    DrawRectangleRec(r, WHITE);
    DrawRectangleLinesEx(r, 2, GOLD);

    DrawText(k < 0.5f ? "LUCI" : "COLA DA LUCI", (int)r.x + 10, (int)r.y + 7, 10, (Color){ 0, 82, 172, 255 });

    char linhas[256];
    QuebrarTexto(b->cola, (int)r.width - 20, fonte, linhas, (int)sizeof(linhas));
    DesenharTextoLinhas(linhas, (int)r.x + 10, (int)r.y + 22, fonte, fonte + 3, (Color){ 30, 28, 44, 255 });
}

// Ícones do minigame de separar: CONTRADIZ (balão laranja com X) e INVENTA (balão azul com ?)
static const char *NOMES_TIPO_SEPARAR[2] = { "CONTRADIZ", "INVENTA" };
static const Color CORES_TIPO_SEPARAR[2] = { { 255, 161, 0, 255 }, { 102, 191, 255, 255 } };

static void DesenharIconeSeparar(Vector2 p, int tipo, bool pedido)
{
    Color cor = CORES_TIPO_SEPARAR[tipo];
    Rectangle balao = { p.x - 11, p.y - 9, 22, 16 };

    if (pedido)
        DrawCircleV(p, 15, Fade(cor, 0.25f));

    DrawRectangleRec(balao, cor);
    DesenharTrianguloB2((Vector2){ p.x - 6, p.y + 7 }, (Vector2){ p.x - 8, p.y + 12 }, (Vector2){ p.x - 1, p.y + 7 }, cor);
    DrawText(tipo == 0 ? "X" : "?", (int)p.x - MeasureText(tipo == 0 ? "X" : "?", 14) / 2, (int)p.y - 8, 14, (Color){ 30, 20, 40, 255 });
}

// Uma bala do minigame, desenhada conforme a variação
static void DesenharBalaB2(const BatalhaB2 *b, const Bala *bl, int indice, Color corInimigo)
{
    Vector2 bp = bl->pos;
    float t = (float)GetTime();

    // LUPA: fora do alcance, tudo parece uma verdade apagada
    if (b->variante == VAR_LUPA && Vector2Distance(bp, b->alma) > RAIO_LUPA) {
        DesenharItemB1(bp, b->faseAtaque, 0.55f, false);
        return;
    }

    if (bl->tipo >= 0) {
        DesenharIconeSeparar(bp, bl->tipo, bl->tipo == b->ondaTipo);
        return;
    }

    if (bl->boa && bl->falsa) {
        // Falsificação: parece verdade, mas pisca em magenta de tempos em tempos
        bool pisca = fmodf(t * 1.6f + indice * 0.37f, 1.0f) < 0.3f;
        DesenharItemB1(bp, b->faseAtaque, 1.0f, pisca);
        return;
    }

    if (bl->boa) {
        DesenharItemB1(bp, b->faseAtaque, 1.0f, false);
        if (b->variante == VAR_LUPA) DrawCircleLines((int)bp.x, (int)bp.y, 15, Fade(GOLD, 0.8f));
        return;
    }

    DesenharDadoCorrompido(bp, bl->glifo, corInimigo, indice);

    if (b->variante == VAR_LUPA)
        DrawCircleLines((int)bp.x, (int)bp.y, 13, Fade(RED, 0.8f));
}

// Legenda do minigame ao lado da caixa (ícones e poucas palavras)
static void B2DesenharLegenda(const BatalhaB2 *b, Color corInimigo)
{
    const char *modo = b->faseAtaque ? "ATAQUE!" : "DEFESA!";
    DrawText(modo, 556, 266, 18, b->faseAtaque ? GOLD : RED);

    if (b->variante == VAR_SEPARAR) {
        int p = b->ondaTipo;
        DesenharIconeSeparar((Vector2){ 566, 302 }, p, true);
        DrawText("PEGUE ESTE", 584, 295, 14, CORES_TIPO_SEPARAR[p]);
        DesenharIconeSeparar((Vector2){ 566, 334 }, 1 - p, false);
        DrawText("ESTE MACHUCA", 584, 327, 14, LIGHTGRAY);
    }
    else if (b->variante == VAR_LUPA) {
        DrawCircleLines(566, 302, 9, GOLD);
        DrawLineEx((Vector2){ 572, 308 }, (Vector2){ 578, 314 }, 3, GOLD);
        DrawText("A LUPA REVELA", 584, 293, 14, GOLD);
        DesenharItemB1((Vector2){ 566, 334 }, b->faseAtaque, 0.55f, false);
        DrawText("FORA DELA, TUDO", 584, 322, 12, LIGHTGRAY);
        DrawText("PARECE VERDADE", 584, 336, 12, LIGHTGRAY);
    }
    else {
        DesenharItemB1((Vector2){ 568, 300 }, b->faseAtaque, 1.0f, false);
        DrawText(b->faseAtaque ? "PEGUE = DANO" : "PEGUE = VIDA", 586, 293, 14, b->faseAtaque ? COR_FATO_ATAQUE : COR_PATCH_CURA);

        if (b->variante == VAR_FALSAS) {
            DesenharItemB1((Vector2){ 568, 332 }, b->faseAtaque, 1.0f, true);
            DrawText("PISCOU = FALSA", 586, 325, 14, MAGENTA);
        } else {
            DesenharDadoCorrompido((Vector2){ 568, 332 }, '?', corInimigo, 0);
            DrawText("DESVIE", 586, 325, 14, corInimigo);
        }
    }

    if (b->faseAtaque)
        DrawText(TextFormat("VERDADES: %d", b->verdadesNoAtaque), 556, 362, 14, GOLD);
}

void BatalhaB2Desenhar(const BatalhaB2 *b, const char *nickname, float tempo)
{
    const DefGuardiaoB2 *def = &GUARDIOES_B2[b->guardiao];

    ClearBackground(BLACK);

    DrawText(def->nome, LARGURA_TELA / 2 - MeasureText(def->nome, 16) / 2, 12, 16, def->cor);
    DrawText(b->subtitulo, LARGURA_TELA / 2 - MeasureText(b->subtitulo, 10) / 2, 32, 10, GRAY);

    Vector2 centro = { LARGURA_TELA / 2.0f, 108.0f };

    // Vencida, ela some (o golpe final já mostrou ela se desfazendo)
    if (!(b->venceu && b->fase == B2_FIM)) {

        DesenharGuardiaoB2(b->guardiao, centro, tempo, 0.85f);

        // Golpe acertou: desenha de novo somando a cor, e ela fica branca por um instante
        if (b->flashInimigo > 0.0f) {
            BeginBlendMode(BLEND_ADDITIVE);
            DesenharGuardiaoB2(b->guardiao, centro, tempo, 0.85f);
            DesenharGuardiaoB2(b->guardiao, centro, tempo, 0.85f);
            EndBlendMode();
        }

        // Golpe forte: onda dourada saindo dela (o mesmo anel dos golpes da Biblioteca)
        if (b->anelGolpe > 0.0f) {
            float k = 1.0f - b->anelGolpe / 0.5f, sobra = 1.0f - k;
            float r = 40.0f + 150.0f * (1.0f - sobra * sobra);
            DrawRing(centro, fmaxf(0.0f, r - 2.0f - 4.0f * sobra), r, 0.0f, 360.0f, 48, Fade(GOLD, 0.85f * sobra));
            DrawCircleV(centro, r, Fade(GOLD, 0.08f * sobra));
        }

        // Golpe final: depois do impacto ela se desfaz em faixas
        if (b->fase == B2_FINALIZANDO && b->tempoFase > IMPACTO_GOLPE_FINAL) {

            float k = (b->tempoFase - IMPACTO_GOLPE_FINAL) / (DURACAO_GOLPE_FINAL - IMPACTO_GOLPE_FINAL);
            if (k > 1.0f) k = 1.0f;

            for (int i = 0; i < 13; i++) {
                int desloc = (int)(((float)(Ruido((unsigned int)(i * 7) + (unsigned int)(tempo * 20.0f)) % 40) - 20.0f) * k);
                float alfa = Clamp(k * 1.5f - (float)(i % 3) * 0.2f, 0.0f, 1.0f);
                DrawRectangle(270 + desloc, 50 + i * 10, 260, 10, Fade(BLACK, alfa));
            }
        }
    }

    // Barra de alucinação
    {
        int barraX = 250, barraY = 184, barraL = 300;
        DrawText("ALUCINAÇÃO", barraX, barraY - 1, 10, LIGHTGRAY);
        DrawRectangle(barraX + 80, barraY, barraL - 80, 10, (Color){ 40, 30, 55, 255 });
        DrawRectangle(barraX + 80, barraY, (int)((barraL - 80) * b->alucinacaoGuardiao / 100.0f), 10, def->cor);
        DrawRectangleLines(barraX + 80, barraY, barraL - 80, 10, GRAY);

        if (b->combo >= 2 && b->fase != B2_FIM)
            DrawText(TextFormat("COMBO: %d SEGUIDOS", b->combo), barraX + barraL + 12, barraY - 2, 14,
                     Fade(GOLD, 0.7f + 0.3f * sinf(tempo * 8.0f)));
    }

    float t = b->anim * b->anim * (3.0f - 2.0f * b->anim);
    Rectangle base = b->estiloAcao ? CAIXA_ACAO : CAIXA_B2;

    Rectangle caixa = {
        Lerp(base.x, CAIXA_DESVIO_B2.x, t),
        Lerp(base.y, CAIXA_DESVIO_B2.y, t),
        Lerp(base.width, CAIXA_DESVIO_B2.width, t),
        Lerp(base.height, CAIXA_DESVIO_B2.height, t)
    };

    if (b->tremor > 0.0f) {
        caixa.x += (float)GetRandomValue(-3, 3);
        caixa.y += (float)GetRandomValue(-3, 3);
    }

    DrawRectangleRec(caixa, BLACK);
    DrawRectangleLinesEx(caixa, 4, (b->flashDano > 0.0f) ? RED : WHITE);

    int px = (int)caixa.x + 24;
    int py = (int)caixa.y + 16;

    if (b->fase == B2_INTRO || b->fase == B2_RESULTADO || b->fase == B2_FIM ||
        b->fase == B2_GOLPE_FINAL || b->fase == B2_FINALIZANDO) {

        char parcial[768];
        bool terminou = TextoParcial(b->texto, b->visiveis, parcial, (int)sizeof(parcial));

        DesenharTextoLinhas(parcial, px, py + 6, FONTE_BATALHA, 28, WHITE);

        // O texto avança sozinho: a linha embaixo do "ENTER >" mostra quanto falta
        if (terminou && b->fase != B2_GOLPE_FINAL && b->fase != B2_FINALIZANDO) {

            int ex = (int)(caixa.x + caixa.width - 90), ey = (int)(caixa.y + caixa.height - 26);
            float k = b->esperaTexto / B2EsperaTexto(b);
            if (k > 1.0f) k = 1.0f;

            if (((int)(GetTime() * 2.0)) % 2 == 0)
                DesenharDicaAvancar(ex, ey, 12);

            DrawRectangle(ex, ey + 15, (int)(52 * k), 2, Fade(GOLD, 0.6f));
        }
    }

    // Golpe final: um botão só, VERIFICAR
    if (b->fase == B2_GOLPE_FINAL) {

        float pulso = 0.5f + 0.5f * sinf((float)GetTime() * 6.0f);
        float yBotao = b->estiloAcao ? CAIXA_ACAO.y + CAIXA_ACAO.height + 16 : caixa.y + caixa.height - 72;
        Rectangle botao = { LARGURA_TELA / 2.0f - 150, yBotao, 300, 52 };

        DrawRectangleRec(botao, Fade(GOLD, 0.12f + 0.1f * pulso));
        DrawRectangleLinesEx(botao, 4, Fade(GOLD, 0.6f + 0.4f * pulso));
        DesenharCursorLuci(botao.x + 26, botao.y + 26);
        DesenharLupaVerificacao((Vector2){ botao.x + botao.width - 32, botao.y + 26 }, 1.0f, tempo);
        DesenharTextoBotao(botao, "VERIFICAR", 26, GOLD);
    }

    // Golpe final: a lupa voa até a alucinação e carimba
    if (b->fase == B2_FINALIZANDO) {

        float tf = b->tempoFase;

        if (tf < IMPACTO_GOLPE_FINAL) {

            float k = tf / IMPACTO_GOLPE_FINAL;
            k = k * k * (3.0f - 2.0f * k);

            Vector2 p = { Lerp(110.0f, centro.x, k), Lerp(170.0f, centro.y, k) - sinf(k * PI) * 50.0f };
            DesenharLupaVerificacao(p, 3.0f, tempo);

        } else {

            float k = (tf - IMPACTO_GOLPE_FINAL) / (DURACAO_GOLPE_FINAL - IMPACTO_GOLPE_FINAL);
            if (k > 1.0f) k = 1.0f;

            for (int i = 0; i < 3; i++) {
                float raio = 20.0f + k * 170.0f - i * 30.0f;
                if (raio > 0.0f)
                    DrawCircleLines((int)centro.x, (int)centro.y, raio, Fade(GOLD, (1.0f - k) * 0.9f));
            }

            float e = (k * 4.0f > 1.0f) ? 1.0f : k * 4.0f;
            int tam = (int)Lerp(56.0f, 36.0f, e);
            const char *carimbo = "VERIFICADO!";
            int cx = (int)centro.x - MeasureText(carimbo, tam) / 2, cy = (int)centro.y - tam / 2;
            DrawText(carimbo, cx + 3, cy + 3, tam, BLACK);
            DrawText(carimbo, cx, cy, tam, LIME);
        }
    }

    else if (b->fase == B2_DESAFIO && b->estiloAcao) {

        // Frase do inimigo na caixa e as ações como botões embaixo
        const RodadaEscolha *r = RodadaAtualB2(b);

        DesenharTextoLinhas(b->enunciado, px, py + 10, FONTE_BATALHA, 28, WHITE);

        int largura = 270, espaco = 24;
        int total = r->numOpcoes * largura + (r->numOpcoes - 1) * espaco;
        int x0 = LARGURA_TELA / 2 - total / 2;
        int yBotao = (int)(CAIXA_ACAO.y + CAIXA_ACAO.height) + 16;

        for (int i = 0; i < r->numOpcoes; i++) {

            bool marcada = (b->selecao == i);
            Rectangle botao = { (float)(x0 + i * (largura + espaco)), (float)yBotao, (float)largura, 44 };

            DrawRectangleRec(botao, BLACK);
            DrawRectangleLinesEx(botao, 3, marcada ? YELLOW : PURPLE);

            if (marcada)
                DesenharCursorLuci(botao.x + 20, botao.y + 22);

            int tam = (MeasureText(r->opcoes[i], 18) > largura - 50) ? 14 : 18;
            DrawText(r->opcoes[i], (int)botao.x + 38 + (largura - 50 - MeasureText(r->opcoes[i], tam)) / 2,
                     (int)botao.y + 22 - tam / 2, tam, marcada ? YELLOW : LIGHTGRAY);
        }

        int yDica = yBotao + 56;

        if (b->estabilidade != NULL) {
            B2DesenharBotaoAjuda(b, (Rectangle){ LARGURA_TELA / 2.0f - 165, (float)yBotao + 54, 330, 34 }, b->selecao == r->numOpcoes);
            yDica = yBotao + 96;
        }

        const char *dica = (b->estabilidade == NULL) ? "A/D, SETAS ou MOUSE: escolher a ação    ENTER ou CLIQUE: confirmar    R: falas da Luci"
                         : "A/D ou MOUSE: escolher a ação    S: ajuda da Luci    ENTER ou CLIQUE: confirmar    R: falas da Luci";
        DrawText(dica, LARGURA_TELA / 2 - MeasureText(dica, 10) / 2, yDica, 10, GRAY);
    }

    else if (b->fase == B2_DESAFIO) {

        const RodadaEscolha *r = RodadaAtualB2(b);
        int f = b->fonteDesafio;
        int linhas = DesenharTextoLinhas(b->enunciado, px, py, f, f + 6, WHITE);
        int y = py + linhas * (f + 6) + 12;

        for (int i = 0; i < r->numOpcoes; i++) {

            bool marcada = (b->selecao == i);

            if (marcada)
                DesenharCursorLuci((float)px + 8, (float)y + f / 2.0f);

            int nl = DesenharTextoLinhas(b->opcoesTexto[i], px + 26, y, f, f + 6, marcada ? YELLOW : LIGHTGRAY);
            y += nl * (f + 6) + 6;
        }

        if (b->estabilidade != NULL)
            B2DesenharBotaoAjuda(b, (Rectangle){ (float)px + 20, (float)y + 2, 330, 30 }, b->selecao == r->numOpcoes);

        DrawText("W/S ou MOUSE: escolher    ENTER ou CLIQUE: confirmar    1-4: atalho    H: ajuda da Luci    R: falas da Luci",
                 px, (int)(caixa.y + caixa.height) - 22, 10, GRAY);
    }

    else if (b->fase == B2_DESVIO) {

        BeginScissorMode((int)caixa.x + 4, (int)caixa.y + 4, (int)caixa.width - 8, (int)caixa.height - 8);

        // SEPARAR: as duas metades da caixa, com o lado pedido aceso
        if (b->variante == VAR_SEPARAR) {

            float meio = caixa.x + caixa.width / 2;

            for (int lado = 0; lado < 2; lado++) {

                Rectangle metade = { lado == 0 ? caixa.x : meio, caixa.y, caixa.width / 2, caixa.height };
                bool pedido = (lado == b->ondaTipo);
                const char *nome = NOMES_TIPO_SEPARAR[lado];

                if (pedido)
                    DrawRectangleRec(metade, Fade(CORES_TIPO_SEPARAR[lado], 0.10f));

                DrawText(nome, (int)(metade.x + metade.width / 2) - MeasureText(nome, 10) / 2, (int)caixa.y + 10, 10,
                         Fade(CORES_TIPO_SEPARAR[lado], pedido ? 1.0f : 0.45f));
            }

            for (float y = caixa.y; y < caixa.y + caixa.height; y += 12)
                DrawLineEx((Vector2){ meio, y }, (Vector2){ meio, y + 6 }, 2, Fade(WHITE, 0.3f));
        }

        // LUPA: o círculo da verificação em volta da Luci
        if (b->variante == VAR_LUPA) {

            Vector2 cabo = { b->alma.x + RAIO_LUPA * 0.71f, b->alma.y + RAIO_LUPA * 0.71f };

            DrawCircleV(b->alma, RAIO_LUPA, Fade(SKYBLUE, 0.07f));
            DrawCircleLines((int)b->alma.x, (int)b->alma.y, RAIO_LUPA, Fade(GOLD, 0.8f));
            DrawLineEx(cabo, (Vector2){ cabo.x + 14, cabo.y + 14 }, 5, Fade(GOLD, 0.8f));
        }

        for (int i = 0; i < MAX_BALAS; i++)
            if (b->balas[i].ativa)
                DesenharBalaB2(b, &b->balas[i], i, def->cor);

        if (b->invencivel <= 0.0f || (((int)(b->invencivel * 10.0f)) % 2 == 0))
            DesenharAlmaLuci(b->alma, b->rastroAlma, 5);

        EndScissorMode();

        float resta = 1.0f - b->tempoDesvio / b->duracaoDesvio;
        if (resta < 0.0f) resta = 0.0f;

        DrawRectangle((int)caixa.x, (int)(caixa.y + caixa.height + 6), (int)(caixa.width * resta), 3, Fade(WHITE, 0.5f));

        // SEPARAR: o pedido da Luci em cima da caixa
        if (b->variante == VAR_SEPARAR) {
            const char *pedido = TextFormat("LUCI: PEGUE SÓ  %s!", NOMES_TIPO_SEPARAR[b->ondaTipo]);
            DrawText(pedido, LARGURA_TELA / 2 - MeasureText(pedido, 18) / 2, 222, 18, CORES_TIPO_SEPARAR[b->ondaTipo]);
        }
    }

    for (int i = 0; i < 48; i++)
        if (b->particulas[i].vida > 0.0f)
            DrawCircleV(b->particulas[i].pos, 3, Fade(b->particulas[i].cor, b->particulas[i].vida / 0.6f));

    // Antes das balas: a regra grande, com ícones, e a contagem
    if (b->fase == B2_DESVIO && b->preparo > 0.0f) {
        int icones[3];
        const char *textos[3];
        Color cores[3];
        int n = 0;
        Color corIni = GUARDIOES_B2[b->guardiao].cor;
        if (b->variante == VAR_SEPARAR) {
            icones[n] = 0; textos[n] = "PEGUE SÓ O TIPO PEDIDO"; cores[n++] = GOLD;
            icones[n] = 1; textos[n] = "O RESTO MACHUCA"; cores[n++] = corIni;
        } else if (b->variante == VAR_LUPA) {
            icones[n] = 0; textos[n] = "CHEGUE PERTO PARA VER"; cores[n++] = GOLD;
            icones[n] = 1; textos[n] = "DESVIE DO FALSO"; cores[n++] = corIni;
        } else {
            icones[n] = 0; textos[n] = b->faseAtaque ? "PEGUE = DANO NELA" : "PEGUE = VIDA"; cores[n++] = b->faseAtaque ? COR_FATO_ATAQUE : COR_PATCH_CURA;
            if (b->variante == VAR_FALSAS) { icones[n] = 2; textos[n] = "PISCOU? FUJA!"; cores[n++] = MAGENTA; }
            else                           { icones[n] = 1; textos[n] = "DESVIE"; cores[n++] = corIni; }
        }
        icones[n] = 3; textos[n] = "MOVA A LUCI"; cores[n++] = (Color){ 0, 228, 255, 255 };
        DesenharPreparoMinigame(caixa, b->preparo, b->preparoTotal, b->faseAtaque ? "ATAQUE!" : "DEFESA!", b->faseAtaque ? GOLD : RED,
                                n, icones, textos, cores, b->faseAtaque, corIni, tempo);
    }

    // Lembra a regra do minigame com ícones (sem texto longo)
    if (b->fase == B2_DESVIO && b->pegarBoas)
        B2DesenharLegenda(b, def->cor);

    // Dano causado no ataque, subindo sobre o inimigo
    if (b->tempoDano > 0.0f) {
        float k = b->tempoDano / 1.4f;
        if (k > 1.0f) k = 1.0f;
        const char *txt = TextFormat("-%d", (int)b->danoMostrado);
        int y = (int)(70 - (1.0f - k) * 30);
        DrawText(txt, LARGURA_TELA / 2 + 70, y, 30, Fade(GOLD, k));

        if (b->danoCombo)
            DrawText("COMBO x1,5", LARGURA_TELA / 2 + 70, y + 32, 14, Fade(ORANGE, k));
    }

    // Balão de ajuda da Luci (só enquanto o jogador escolhe)
    if (b->fase == B2_DESAFIO && b->dica[0] != '\0') {

        float k = b->tempoDica / 0.2f;
        if (k > 1.0f) k = 1.0f;

        Vector2 luci = { 64.0f, 112.0f + sinf((float)GetTime() * 2.0f) * 3.0f };
        Rectangle balao = { 96, 66, 206, 92 };

        DesenharLuciTamanho(luci, 40.0f);
        DesenharTrianguloB2((Vector2){ balao.x, balao.y + 40 }, (Vector2){ balao.x - 12, balao.y + 50 }, (Vector2){ balao.x, balao.y + 56 }, Fade(WHITE, k));
        DrawRectangleRec(balao, Fade(WHITE, k));
        DrawText("LUCI", (int)balao.x + 10, (int)balao.y + 8, 10, Fade((Color){ 0, 82, 172, 255 }, k));

        char linhas[200];
        QuebrarTexto(b->dica, 186, 14, linhas, (int)sizeof(linhas));
        DesenharTextoLinhas(linhas, (int)balao.x + 10, (int)balao.y + 24, 14, 17, Fade((Color){ 30, 28, 44, 255 }, k));
    }

    // Comentário da Luci (acerto, erro, pouca vida): o mesmo balão da dica, que aparece e some sozinho
    else if (b->tempoComentarioLuci > 0.0f && b->comentarioLuci[0] != '\0' && !b->telaDerrota) {

        float entra = Clamp((TEMPO_FALA_LUCI_B2 - b->tempoComentarioLuci) / 0.2f, 0.0f, 1.0f);
        float sai = Clamp(b->tempoComentarioLuci / 0.3f, 0.0f, 1.0f);
        float k = entra * sai;

        char linhas[160];
        QuebrarTexto(b->comentarioLuci, 186, 14, linhas, (int)sizeof(linhas));
        int nl = ContarLinhas(linhas);

        Rectangle balao = { 96, 66, 206, (float)(30 + nl * 17) };
        Vector2 luci = { 64.0f, balao.y + balao.height / 2.0f + sinf((float)GetTime() * 2.0f) * 3.0f };

        // Ela mexe a boca no primeiro segundo do comentário
        bool falandoAntes = luciFalando;
        float tempoAntes = tempoFalaLuci;
        float alfaAntes = g_alfaLuci;
        luciFalando = (TEMPO_FALA_LUCI_B2 - b->tempoComentarioLuci) < 1.0f;
        tempoFalaLuci = TEMPO_FALA_LUCI_B2 - b->tempoComentarioLuci;
        g_alfaLuci = k;
        DesenharLuciTamanho(luci, 40.0f);
        luciFalando = falandoAntes;
        tempoFalaLuci = tempoAntes;
        g_alfaLuci = alfaAntes;

        DesenharTrianguloB2((Vector2){ balao.x, luci.y - 8 }, (Vector2){ balao.x - 12, luci.y }, (Vector2){ balao.x, luci.y + 8 }, Fade(WHITE, k));
        DrawRectangleRec(balao, Fade(WHITE, k));
        DrawRectangleLinesEx(balao, 2, Fade((Color){ 0, 228, 255, 255 }, k));
        DrawText("LUCI", (int)balao.x + 10, (int)balao.y + 8, 10, Fade((Color){ 0, 82, 172, 255 }, k));
        DesenharTextoLinhas(linhas, (int)balao.x + 10, (int)balao.y + 22, 14, 17, Fade((Color){ 30, 28, 44, 255 }, k));
    }

    B2DesenharCola(b);

    int yStatus = 522;

    // Estabilidade da Luci: cada ajuda gasta 1, cada acerto sozinho devolve 1
    if (b->estabilidade != NULL) {

        int e = *b->estabilidade;
        Color corE = (e >= 3) ? SKYBLUE : (e == 2 ? ORANGE : RED);

        DrawText("LUCI", 500, yStatus + 4, 12, WHITE);

        for (int i = 0; i < ESTABILIDADE_MAX_LUCI; i++) {
            Rectangle seg = { 540.0f + i * 20, (float)yStatus + 4, 16, 12 };
            if (i < e) DrawRectangleRec(seg, corE);
            DrawRectangleLinesEx(seg, 1, Fade(GRAY, 0.8f));
        }

        if (e <= 2)
            DrawText(e == 0 ? "SOBRECARREGADA" : "INSTÁVEL", 540, yStatus + 20, 10, corE);
    }

    DesenharVidaB1(nickname, b->hp, HP_MAX_B2, yStatus, b->flashDano, tempo);
    VinhetaDanoB1(b->flashDano);

    const char *instrucao = "ENTER ou CLIQUE  -  continuar";

    if (b->fase == B2_DESVIO)          instrucao = "W A S D ou SETAS  -  guie a Luci e desvie!";
    else if (b->fase == B2_GOLPE_FINAL) instrucao = "ENTER ou CLIQUE  -  VERIFICAR!";
    else if (b->fase == B2_FINALIZANDO) instrucao = "";

    if (b->fase != B2_DESAFIO && !b->telaDerrota)
        DrawText(instrucao, LARGURA_TELA / 2 - MeasureText(instrucao, 12) / 2, 568, 12, GRAY);

    // Game over (mesmo estilo do da Biblioteca)
    if (b->telaDerrota) {

        float k = fminf(b->tempoDerrota / 0.6f, 1.0f);
        DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, 0.75f * k));

        const char *titulo = "A LUCI FOI SOBRECARREGADA";
        DrawText(titulo, LARGURA_TELA / 2 - MeasureText(titulo, 36) / 2, 230, 36, Fade(MAGENTA, k));

        const char *sub = TextFormat("A integridade dela chegou a zero.  -%d pontos", PENALIDADE_HP_ZERO);
        DrawText(sub, LARGURA_TELA / 2 - MeasureText(sub, 16) / 2, 282, 16, Fade(WHITE, k));

        const char *info = "Você falhou neste monitor: ele fica marcado como ERRO e não pode ser refeito.";
        DrawText(info, LARGURA_TELA / 2 - MeasureText(info, 12) / 2, 308, 12, Fade((Color){ 0, 228, 255, 255 }, k));

        if (b->tempoDerrota >= ESPERA_TELA_DERROTA_B1 && ((int)(GetTime() * 2.0)) % 2 == 0) {
            const char *sair = "E - sair do monitor";
            DrawText(sair, LARGURA_TELA / 2 - MeasureText(sair, 16) / 2, 342, 16, GOLD);
        }
    }
}

// ---------- Cartão de resumo ----------

void AbrirCartao(CartaoResumo *c, int icone, bool sucesso, const char *titulo,
                        const char *frase, const Dialogo *saberMais)
{
    c->ativo = true;
    c->icone = icone;
    c->sucesso = sucesso;
    c->titulo = titulo;
    c->frase = frase;
    c->saberMais = saberMais;
    c->inicio = GetTime();
}

// Devolve true quando o cartão fecha (ENTER) ou vira "saber mais" (TAB)
bool AtualizarCartao(CartaoResumo *c, SessaoDialogo *dlg)
{
    if (!c->ativo || GetTime() - c->inicio < 0.4)
        return false;

    // Clique no "TAB - saber mais" abre a versão completa; clique no resto do cartão fecha
    bool cliqueTab = CliqueMouse() && c->saberMais != NULL &&
                     MouseEm(RetTextoClicavel("TAB - saber mais", 160 + 130, 175 + 210 - 26, 12));

    if ((IsKeyPressed(KEY_TAB) || cliqueTab) && c->saberMais != NULL) {
        c->ativo = false;
        Telemetria("saber_mais", c->titulo, (float)(GetTime() - c->inicio), 0);
        DialogoIniciar(dlg, c->saberMais, OBJ_NENHUM);
        dlg->veioDoCartao = true;
        return true;
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE) || CliqueMouse()) {
        c->ativo = false;
        Telemetria("cartao", c->titulo, (float)(GetTime() - c->inicio), 0);
        return true;
    }

    return false;
}

void DesenharCartao(const CartaoResumo *c, float tempo)
{
    if (!c->ativo)
        return;

    float k = (float)(GetTime() - c->inicio) / 0.25f;
    if (k > 1.0f) k = 1.0f;

    DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, 0.55f * k));

    Rectangle painel = { 160, 175 + (1.0f - k) * 30.0f, 480, 210 };
    Color destaque = c->sucesso ? GOLD : ORANGE;

    DrawRectangleRec(painel, (Color){ 18, 15, 30, 255 });
    DrawRectangleLinesEx(painel, 4, destaque);

    // Ícone com brilhos girando
    Vector2 ic = { painel.x + 66, painel.y + 92 };

    for (int i = 0; i < 6; i++) {
        float a = tempo * 1.5f + i * (PI / 3.0f);
        DrawCircleV((Vector2){ ic.x + cosf(a) * 40, ic.y + sinf(a) * 40 }, 2.5f, Fade(destaque, 0.7f));
    }

    {
        Rectangle tela = { ic.x - 28, ic.y - 22, 56, 40 };
        DrawRectangleRec(tela, c->sucesso ? (Color){ 8, 48, 22, 255 } : (Color){ 55, 8, 14, 255 });
        DrawRectangleLinesEx(tela, 3, c->sucesso ? GREEN : RED);
        DrawRectangle((int)ic.x - 4, (int)ic.y + 18, 8, 8, (Color){ 45, 40, 62, 255 });

        if (c->sucesso) {
            DrawLineEx((Vector2){ ic.x - 12, ic.y - 2 }, (Vector2){ ic.x - 3, ic.y + 7 }, 4, LIME);
            DrawLineEx((Vector2){ ic.x - 3, ic.y + 7 }, (Vector2){ ic.x + 13, ic.y - 10 }, 4, LIME);
        } else {
            DrawLineEx((Vector2){ ic.x - 9, ic.y - 10 }, (Vector2){ ic.x + 9, ic.y + 8 }, 4, RED);
            DrawLineEx((Vector2){ ic.x + 9, ic.y - 10 }, (Vector2){ ic.x - 9, ic.y + 8 }, 4, RED);
        }
    }

    int tx = (int)painel.x + 130;
    const char *rotulo = c->sucesso ? "VOCÊ APRENDEU" : "LEMBRE-SE";

    DrawText(rotulo, tx, (int)painel.y + 22, 10, Fade(destaque, 0.9f));
    DrawText(c->titulo, tx, (int)painel.y + 38, 20, destaque);

    char frase[256];
    QuebrarTexto(c->frase, 320, 16, frase, (int)sizeof(frase));
    DesenharTextoLinhas(frase, tx, (int)painel.y + 74, 16, 22, WHITE);

    if (((int)(GetTime() * 2.0)) % 2 == 0)
        DesenharDicaAvancar((int)(painel.x + painel.width) - 80, (int)(painel.y + painel.height) - 26, 12);

    if (c->saberMais != NULL)
        DesenharTextoClicavel("TAB - saber mais", tx, (int)(painel.y + painel.height) - 26, 12, Fade(SKYBLUE, 0.85f));
}

// Fim de uma batalha do Bloco 1: pontos, cartão e fala da Luci
void FimBatalhaB2(ContextoB2 *c)
{
    BatalhaB2 *b = c->batalha;

    c->progresso->alucinacao += b->erros;

    int pontosAcerto = PONTOS_ACERTO_B1;
    int acertosSozinho = b->acertos - b->acertosComAjuda;
    int delta = acertosSozinho * pontosAcerto + b->acertosComAjuda * PONTOS_ACERTO_COM_AJUDA
              - b->erros * PENALIDADE_ERRO;

    // Confiar numa dica alucinada fortalece a alucinação
    c->progresso->alucinacao += b->alucinacoesSeguidas;

    if (b->derrota)   delta -= PENALIDADE_HP_ZERO;

    Telemetria(b->venceu ? "batalha_vitoria" : (b->derrota ? "batalha_derrota" : "batalha_esquiva"),
               b->subtitulo, (float)(GetTime() - b->inicio), b->erros);

    // Resultado do monitor
    int m = b->monitor;

    c->progresso->estado[m] = b->venceu ? MON_ACERTOU : MON_ERROU;
    GuardarHpDoCorredor(c->progresso, b->hp, b->derrota);
    if (b->venceu) delta += PONTOS_POR_ACERTO;

    AlterarPontos(c->progresso, delta);
    AbrirCartao(c->cartao, m, b->venceu, TITULOS_RESUMO_MONITOR[m], FRASES_RESUMO_MONITOR[m], &saberMaisMonitores[m]);
    MostrarEmocaoLuci(b->venceu ? EMOCAO_ALEGRE : EMOCAO_DUVIDA, 3.0f);

    *c->pedirAutoSave = true;
    IniciarTransicao(c->transicao, *c->salaDaBatalha, DURACAO_FADE_BATALHA);
}
