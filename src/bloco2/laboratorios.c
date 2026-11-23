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
// EXPANSÃO EDUCATIVA DO BLOCO 2: LÓGICA DOS LABORATÓRIOS
// =============================================================

// Tira do fim da string um caractere UTF-8 que o snprintf cortou ao meio
void LabCortarUtf8(char *s)
{
    int n = (int)strlen(s);
    int i = n - 1;
    while (i >= 0 && (((unsigned char)s[i]) & 0xC0) == 0x80) i--;
    if (i < 0) return;
    unsigned char c = (unsigned char)s[i];
    int esperado = (c >= 0xF0) ? 4 : ((c >= 0xE0) ? 3 : ((c >= 0xC0) ? 2 : 1));
    if (n - i < esperado) s[i] = '\0';
}

// ---------- Seleção procedural das missões ----------

static int LabNumMissoes(TipoLab t)
{
    switch (t) {
        case LAB_PROMPTS:  return NUM_MISSOES_PROMPT;
        case LAB_ARQUIVO:  return NUM_DESAFIOS_ARQUIVO;
        case LAB_NUCLEO:   return NUM_DESAFIOS_NUCLEO;
        case LAB_FIREWALL: return NUM_CASOS_VIES;
        default:           return NUM_CASOS_PESQUISA;
    }
}

int LabDificuldadeMissao(TipoLab t, int i)
{
    switch (t) {
        case LAB_PROMPTS:  return MISSOES_PROMPT[i].dificuldade;
        case LAB_ARQUIVO:  return DESAFIOS_ARQUIVO[i].dificuldade;
        case LAB_NUCLEO:   return DESAFIOS_NUCLEO[i].dificuldade;
        case LAB_FIREWALL: return CASOS_VIES[i].dificuldade;
        default:           return CASOS_PESQUISA[i].dificuldade;
    }
}

// Conceito que precisa ter sido apresentado antes (Byte, terminais)
static int LabConceitoMissao(TipoLab t, int i)
{
    if (t == LAB_PROMPTS) return MISSOES_PROMPT[i].conceito;
    if (t == LAB_ARQUIVO) return DESAFIOS_ARQUIVO[i].conceito;
    return CONCEITO_FUNDAMENTOS;
}

// Dificuldade da tentativa: cresce com a profundidade e se ajusta ao desempenho nos laboratórios
static int LabDificuldadeAlvo(const Biblioteca *b)
{
    int alvo = (b->profundidade <= 2) ? 1 : ((b->profundidade <= 5) ? 2 : 3);

    if (b->labFeitos >= 2) {
        float media = (float)b->labEstrelas / (float)b->labFeitos;
        if (media >= 2.5f)      alvo++;
        else if (media < 1.5f)  alvo--;
    }
    return (alvo < 1) ? 1 : ((alvo > 3) ? 3 : alvo);
}

// Uma missão da categoria: tema certo, conceito já apresentado, dificuldade mais perto do alvo
// e, de preferência, uma que ainda não apareceu (o ciclo recomeça quando todas já foram vistas)
static int LabSortearMissao(Biblioteca *b, TipoLab t)
{
    int n = LabNumMissoes(t);
    unsigned int todas = (n >= 32) ? 0xFFFFFFFFu : ((1u << n) - 1u);
    int alvo = LabDificuldadeAlvo(b);

    if ((b->labVistas[t] & todas) == todas)
        b->labVistas[t] = 0;

    int cand[32], nc = 0;
    for (int passada = 0; passada < 3 && nc == 0; passada++) {
        int melhor = 99;
        for (int i = 0; i < n && i < 32; i++) {
            int c = LabConceitoMissao(t, i);
            bool conhece = (c == CONCEITO_FUNDAMENTOS) || (b->perm->conceitos & (1u << c));
            if (passada < 2 && (b->labVistas[t] & (1u << i))) continue;
            if (passada == 0 && !conhece) continue;
            int dist = abs(LabDificuldadeMissao(t, i) - alvo);
            if (dist < melhor) { melhor = dist; nc = 0; }
            if (dist == melhor) cand[nc++] = i;
        }
    }

    int m = (nc > 0) ? cand[GetRandomValue(0, nc - 1)] : 0;
    b->labVistas[t] |= 1u << m;
    return m;
}

// Prepara o laboratório da sala (a janela só abre quando o jogador usa o console)
void LabPreparar(Biblioteca *b, TipoLab tipo)
{
    LabEdu *L = &b->lab;

    memset(L, 0, sizeof(*L));
    L->tipo = tipo;
    L->missao = LabSortearMissao(b, tipo);
    L->etapa = LAB_INTRO;
    L->sel = L->sel2 = L->veredito = -1;
    L->luciAponta = -1;
    L->luciFase = -1;
    for (int k = 0; k < LAB_PACOTES; k++) L->acoes[k] = -2;
}

static void LabEnviarPrompt(Biblioteca *b);

// Passo 2 (experimentar): sai da introdução e começa o desafio
static void LabComecar(Biblioteca *b)
{
    LabEdu *L = &b->lab;

    b->labIntroVista |= 1u << L->tipo;
    L->etapa = LAB_DESAFIO;
    L->tempo = 0.0f;
    L->foco = 0;

    if (L->tipo == LAB_PROMPTS) {
        const MissaoPrompt *m = &MISSOES_PROMPT[L->missao];
        if (m->vago) {
            // Refinamento: o pedido vago já foi enviado e a resposta ruim está na tela
            for (int k = 0; k < NUM_PARTES_PROMPT; k++) L->escolha[k] = m->inicial[k];
            L->original = true;
            LabEnviarPrompt(b);
        }
    } else if (L->tipo == LAB_FIREWALL) {
        // Pacotes sorteados do banco (sem repetir)
        int ordem[NUM_PACOTES_DADOS];
        for (int i = 0; i < NUM_PACOTES_DADOS; i++) ordem[i] = i;
        for (int i = NUM_PACOTES_DADOS - 1; i > 0; i--) {
            int j = GetRandomValue(0, i);
            int t = ordem[i]; ordem[i] = ordem[j]; ordem[j] = t;
        }
        for (int k = 0; k < LAB_PACOTES; k++) L->pacotes[k] = ordem[k];
        L->tempoPacote = LAB_TEMPO_PACOTE;
    }

    TocarSom(sfxSelecao);
    Telemetria("rogue_lab_inicio", NOMES_LAB[L->tipo], (float)LabDificuldadeMissao(L->tipo, L->missao), L->missao);
}

// ---------- Modo Prompt: o "modelo" offline ----------

static int LabNotaParte(const MissaoPrompt *m, const int esc[NUM_PARTES_PROMPT], int k)
{
    int a = esc[k];
    if (a < 0 || a > 3) a = 0;
    return m->nota[k][a];
}

static bool LabParteNecessaria(const MissaoPrompt *m, int k)
{
    return (m->necessarias & PB(k)) != 0;
}

// Combinação contraditória (devolve as duas partes envolvidas)
static bool LabContradicao(const MissaoPrompt *m, const int esc[NUM_PARTES_PROMPT], int *pa, int *pb)
{
    for (int i = 0; i < 2; i++) {
        const signed char *c = m->contra[i];
        if (c[0] < 0) continue;
        if (esc[c[0]] == c[1] && esc[c[2]] == c[3]) {
            if (pa) *pa = c[0];
            if (pb) *pb = c[2];
            return true;
        }
    }
    return false;
}

// Avalia a COMBINAÇÃO escolhida pelos critérios da missão (não só a quantidade de partes):
// clareza, informações necessárias, adequação, especificidade e ambiguidade (0 a 2 cada).
// O diagnóstico decide qual resposta simulada aparece.
static int LabAvaliarPrompt(const MissaoPrompt *m, const int esc[NUM_PARTES_PROMPT], int crit[NUM_CRITERIOS], DiagPrompt *diag)
{
    int n[NUM_PARTES_PROMPT];
    int req = 0, ok = 0, ideais = 0, inadequadas = 0, ruido = 0;
    bool contra = LabContradicao(m, esc, NULL, NULL);

    for (int k = 0; k < NUM_PARTES_PROMPT; k++) {
        n[k] = LabNotaParte(m, esc, k);
        if (LabParteNecessaria(m, k)) {
            req++;
            if (n[k] >= 1) ok++;
            if (n[k] == 2) ideais++;
        } else if (esc[k] > 0 && n[k] == 0) {
            ruido++;                                    // parte opcional que só atrapalha
        }
        if (esc[k] > 0 && n[k] < 0) inadequadas++;
    }

    crit[CRIT_CLAREZA] = (n[PP_OBJETIVO] >= 2) ? 2 : ((n[PP_OBJETIVO] == 1) ? 1 : 0);
    if (contra && crit[CRIT_CLAREZA] > 1) crit[CRIT_CLAREZA] = 1;
    crit[CRIT_INFORMACOES] = (ok >= req) ? 2 : ((ok >= req - 1) ? 1 : 0);
    crit[CRIT_ADEQUACAO] = contra ? 0 : ((inadequadas == 0) ? 2 : ((inadequadas == 1) ? 1 : 0));
    int esp = (ideais >= req) ? 2 : ((ideais * 2 >= req) ? 1 : 0);
    esp -= ruido;
    crit[CRIT_ESPECIFICIDADE] = (esp < 0) ? 0 : esp;
    crit[CRIT_AMBIGUIDADE] = contra ? 0 : (((!LabParteNecessaria(m, PP_CONTEXTO) || n[PP_CONTEXTO] >= 1) ? 1 : 0) +
                                           ((!LabParteNecessaria(m, PP_FORMATO)  || n[PP_FORMATO]  >= 1) ? 1 : 0));

    int total = 0;
    for (int c = 0; c < NUM_CRITERIOS; c++) total += crit[c];

    #define FALHA(k) ((LabParteNecessaria(m, (k)) && n[(k)] <= 0) || n[(k)] < 0)
    if (contra)                   *diag = DIAG_CONTRADICAO;
    else if (n[PP_OBJETIVO] <= 0) *diag = DIAG_VAGA;
    else if (FALHA(PP_CONTEXTO))  *diag = DIAG_PUBLICO;
    else if (FALHA(PP_PAPEL))     *diag = DIAG_PAPEL;
    else if (FALHA(PP_FORMATO))   *diag = DIAG_FORMATO;
    else                          *diag = (total >= 9) ? DIAG_IDEAL : DIAG_BOA;
    #undef FALHA

    return total;
}
const char *NOMES_LUZ[NUM_LUZES] = { "IDEAL", "OK", "DISPENSÁVEL", "VAGA", "FALTOU", "ATRAPALHOU", "CONFLITO" };
const Color CORES_LUZ[NUM_LUZES] = {
    { 0, 228, 48, 255 }, { 253, 249, 0, 255 }, { 150, 150, 170, 255 }, { 255, 161, 0, 255 },
    { 230, 41, 55, 255 }, { 230, 41, 55, 255 }, { 255, 0, 255, 255 }
};

LuzParte LabLuzParte(const MissaoPrompt *m, const int esc[NUM_PARTES_PROMPT], int k)
{
    int pa = -1, pb = -1;
    if (LabContradicao(m, esc, &pa, &pb) && (k == pa || k == pb))
        return LUZ_CONFLITO;

    int n = LabNotaParte(m, esc, k);
    if (n < 0)  return LUZ_ATRAPALHOU;
    if (n == 0) return (esc[k] == 0) ? LUZ_FALTOU : LUZ_VAGA;
    if (esc[k] == 0) return LUZ_DISPENSAVEL;           // vazio e tudo bem: nem toda tarefa precisa dos quatro
    return (n == 2) ? LUZ_IDEAL : LUZ_OK;
}

// A luz que aparece no encaixe agora (-1: ainda não enviou, ou a peça mudou desde o envio)
static int LabLuzVisivel(const LabEdu *L, const MissaoPrompt *m, int k)
{
    if (!L->temEnvio || L->escolha[k] != L->enviado[k])
        return -1;
    return (int)LabLuzParte(m, L->enviado, k);
}

const char *LabTextoRespostaAtual(const Biblioteca *b)
{
    const LabEdu *L = &b->lab;

    if (L->tipo == LAB_PROMPTS) {
        const MissaoPrompt *m = &MISSOES_PROMPT[L->missao];
        return L->respostaAlucinada ? m->respAlucinada : m->resp[L->diag];
    }
    if (L->tipo == LAB_NUCLEO)
        return DESAFIOS_NUCLEO[L->missao].resp[L->resultadoNucleo];
    return NULL;
}

// Envia o pedido para a IA simulada: avaliação, diagnóstico e a resposta cadastrada
static void LabEnviarPrompt(Biblioteca *b)
{
    LabEdu *L = &b->lab;
    const MissaoPrompt *m = &MISSOES_PROMPT[L->missao];

    if (!L->original)
        L->envios++;

    L->notaPrompt = LabAvaliarPrompt(m, L->escolha, L->criterios, &L->diag);

    // Missão de alucinação: sem pedir honestidade, a resposta "boa" vem com algo inventado
    L->respostaAlucinada = (m->honestaParte >= 0 && (L->diag == DIAG_IDEAL || L->diag == DIAG_BOA) &&
                            L->escolha[m->honestaParte] != m->honestaAlt);

    for (int k = 0; k < NUM_PARTES_PROMPT; k++) L->enviado[k] = L->escolha[k];
    L->temEnvio = true;
    L->animPeca = 0.0f;
    L->tempoResposta = 0.0f;

    L->desconfiou = false;
    L->digitado = 0.0f;
    L->avaliacao[0] = '\0';
    L->temSugestao = false;
    L->luciTemSugestao = false;
    L->conferencia[0] = '\0';
    L->etapa = LAB_RESPOSTA;
    L->tempo = 0.0f;
    L->foco = 0;

    TocarSomTom(sfxSelecao, 1.2f);
    Telemetria("rogue_lab_prompt", TEXTOS_DIAG[L->diag], (float)L->notaPrompt, L->envios);
}

// ---------- A Luci nos laboratórios ----------

static bool LabLuciDisponivel(Biblioteca *b)
{
    LabEdu *L = &b->lab;

    if (RogueEstab(b) > 0)
        return true;

    snprintf(L->luciTexto, sizeof(L->luciTexto), "L#ci: Estou sobrecarregada... Resolva sem mim e eu me estabilizo.");
    L->luciTemSugestao = false;
    L->conferencia[0] = '\0';
    MostrarEmocaoLuci(EMOCAO_DUVIDA, 2.0f);
    TocarSom(sfxErro);
    return false;
}

// A carga de ajuda vai para a estabilidade de sempre (a do main, compartilhada com o Bloco 1)
static void LabCobrarLuci(Biblioteca *b, int custo)
{
    b->cargaLuci += custo;
    b->lab.ajudas++;

    while (b->cargaLuci >= LAB_CARGA_POR_ESTAB && RogueEstab(b) > 0) {
        b->cargaLuci -= LAB_CARGA_POR_ESTAB;
        RogueMudarEstabilidade(b, -1, NULL);
        RogueRecadoLuci(b, FALAS_INSTABILIDADE_LAB[RogueEstab(b)]);
    }
    if (RogueEstab(b) <= 0)
        b->cargaLuci = 0;
}

static bool LabLuciAlucina(const Biblioteca *b)
{
    return GetRandomValue(1, 100) <= RogueChanceLuciErrar(b);
}

// Fala da Luci no laboratório. A confiável explica o porquê; a alucinada vem com certeza absoluta
// e sem motivo. Instável, ela mesma pede para ser conferida.
static void LabFalaLuci(Biblioteca *b, const char *texto, bool alucinada, int aponta, int fase)
{
    LabEdu *L = &b->lab;
    int e = RogueEstab(b);
    const char *nome = (e >= 3) ? "Luci" : ((e == 2) ? "Luci~" : "L#ci");

    snprintf(L->luciTexto, sizeof(L->luciTexto), "%s: %s%s%s", nome, texto,
             alucinada ? " Tenho 100% de certeza!" : "", (e <= 2) ? " [instável: confira com V]" : "");
    LabCortarUtf8(L->luciTexto);

    L->luciTemSugestao = true;
    L->luciAlucinou = alucinada;
    L->luciVerificada = false;
    L->luciAponta = aponta;
    L->luciFase = fase;
    L->conferencia[0] = '\0';
    if (alucinada) L->alucinacoes++;

    if (e <= 2) MostrarEmocaoLuci(EMOCAO_ALERTA, 1.5f);
    TocarSom(sfxSelecao);
    Telemetria("rogue_lab_luci", alucinada ? "alucinou" : "ajudou", (float)RogueChanceLuciErrar(b), L->ajudas);
}

// O jogador seguiu a sugestão sem conferir: confiar vira hábito; se era alucinação, a Luci se abala
static void LabSeguiuLuci(Biblioteca *b)
{
    LabEdu *L = &b->lab;

    b->perm->confiouCego++;
    RogueHabitoLuci(b, true);
    if (L->luciAlucinou) {
        b->perm->confiouAlucinacao++;
        L->seguiuAlucinacao = true;
        RogueMudarEstabilidade(b, -1, NULL);
    }
    L->luciTemSugestao = false;
}

// Conferiu a decisão com a sugestão pendente da Luci (aponta / fase iguais e sem V)
static void LabConferirSeguiu(Biblioteca *b, int fase, int escolha)
{
    const LabEdu *L = &b->lab;
    if (L->luciTemSugestao && !L->luciVerificada && L->luciFase == fase && L->luciAponta >= 0 && L->luciAponta == escolha)
        LabSeguiuLuci(b);
}

static void LabDicas(const Biblioteca *b, const char **d, const char **falsa)
{
    const LabEdu *L = &b->lab;
    const char *const *lista;

    switch (L->tipo) {
        case LAB_PROMPTS:  lista = MISSOES_PROMPT[L->missao].dicas;   *falsa = MISSOES_PROMPT[L->missao].dicaFalsa;   break;
        case LAB_ARQUIVO:  lista = DESAFIOS_ARQUIVO[L->missao].dicas; *falsa = DESAFIOS_ARQUIVO[L->missao].dicaFalsa; break;
        case LAB_NUCLEO:   lista = DESAFIOS_NUCLEO[L->missao].dicas;  *falsa = DESAFIOS_NUCLEO[L->missao].dicaFalsa;  break;
        case LAB_FIREWALL:
            if (L->fase < 2) { lista = CASOS_VIES[L->missao].dicas; *falsa = CASOS_VIES[L->missao].dicaFalsa; }
            else             { lista = DICAS_PRIVACIDADE;           *falsa = DICA_FALSA_PRIVACIDADE; }
            break;
        default:           lista = CASOS_PESQUISA[L->missao].dicas;   *falsa = CASOS_PESQUISA[L->missao].dicaFalsa;   break;
    }
    for (int i = 0; i < 3; i++) d[i] = lista[i];
}

// 1. Dicas graduais: orientação geral, conceito, orientação específica (nunca a resposta)
static void LabPedirDica(Biblioteca *b)
{
    LabEdu *L = &b->lab;

    if (L->nivelDica >= 3) {
        snprintf(L->luciTexto, sizeof(L->luciTexto), "Luci: Já dei as três dicas. Agora confie no seu raciocínio!");
        L->luciTemSugestao = false;
        return;
    }
    if (!LabLuciDisponivel(b))
        return;

    LabCobrarLuci(b, LAB_CUSTO_DICA);
    bool alucina = LabLuciAlucina(b);
    const char *d[3], *falsa;
    LabDicas(b, d, &falsa);
    L->nivelDica++;

    char t[200];
    if (alucina) snprintf(t, sizeof(t), "%s", falsa);
    else         snprintf(t, sizeof(t), "(dica %d/3) %s", L->nivelDica, d[L->nivelDica - 1]);
    LabFalaLuci(b, t, alucina, -1, -1);
}

// 2. Análise do pedido (Modo Prompt): aponta o tipo de problema, não a alternativa certa
static void LabTextoAnalise(const MissaoPrompt *m, const int esc[NUM_PARTES_PROMPT], DiagPrompt diag, char *t, int tam)
{
    int pa = 0, pb = 0;

    switch (diag) {
        case DIAG_CONTRADICAO:
            LabContradicao(m, esc, &pa, &pb);
            snprintf(t, (size_t)tam, "O %s e o %s brigam entre si. A IA vai ter que ignorar um dos dois.", NOMES_PARTES_PROMPT[pa], NOMES_PARTES_PROMPT[pb]);
            break;
        case DIAG_VAGA:
            snprintf(t, (size_t)tam, "%s", (esc[PP_OBJETIVO] == 0) ? "Falta dizer o que a IA deve FAZER: explicar, resumir, comparar..."
                                                                    : "O OBJETIVO está vago ou fora da missão. Diga exatamente a tarefa.");
            break;
        case DIAG_PUBLICO:
            snprintf(t, (size_t)tam, "%s", (esc[PP_CONTEXTO] == 0) ? "Você explicou o assunto, mas não especificou para quem a resposta deve ser direcionada."
                                                                    : "O CONTEXTO escolhido não combina com quem vai usar a resposta.");
            break;
        case DIAG_PAPEL:
            snprintf(t, (size_t)tam, "O PAPEL não combina com a tarefa: quem responde muda o tipo de resposta.");
            break;
        case DIAG_FORMATO:
            snprintf(t, (size_t)tam, "%s", (esc[PP_FORMATO] == 0) ? "Você não disse COMO quer a resposta. Um formato deixa tudo mais claro."
                                                                   : "Esse FORMATO não ajuda nesta tarefa (ou deixa o pedido ambíguo).");
            break;
        case DIAG_BOA: {
            int k = -1;
            for (int p = 0; p < NUM_PARTES_PROMPT && k < 0; p++)
                if (!LabParteNecessaria(m, p) && esc[p] > 0 && LabNotaParte(m, esc, p) == 0) k = p;
            if (k >= 0) { snprintf(t, (size_t)tam, "O %s não é necessário aqui e só adiciona ruído.", NOMES_PARTES_PROMPT[k]); break; }
            for (int p = 0; p < NUM_PARTES_PROMPT && k < 0; p++)
                if (LabParteNecessaria(m, p) && LabNotaParte(m, esc, p) == 1) k = p;
            if (k >= 0) snprintf(t, (size_t)tam, "Está bom, mas dá para ser mais específico no %s.", NOMES_PARTES_PROMPT[k]);
            else        snprintf(t, (size_t)tam, "Está bom, mas ainda dá para ser mais específico.");
        } break;
        default: {
            int k = -1;
            for (int p = 0; p < NUM_PARTES_PROMPT && k < 0; p++)
                if (!LabParteNecessaria(m, p) && esc[p] == 0) k = p;
            if (k >= 0) snprintf(t, (size_t)tam, "Pedido claro e completo. Repare: nem precisou de %s.", NOMES_PARTES_PROMPT[k]);
            else        snprintf(t, (size_t)tam, "Pedido claro e completo para esta missão.");
        } break;
    }
}

static void LabAnalisarPrompt(Biblioteca *b)
{
    LabEdu *L = &b->lab;
    const MissaoPrompt *m = &MISSOES_PROMPT[L->missao];

    if (!LabLuciDisponivel(b))
        return;
    LabCobrarLuci(b, LAB_CUSTO_ANALISE);

    int crit[NUM_CRITERIOS];
    DiagPrompt diag;
    LabAvaliarPrompt(m, L->escolha, crit, &diag);

    bool alucina = LabLuciAlucina(b);
    char t[200];

    if (!alucina) {
        LabTextoAnalise(m, L->escolha, diag, t, (int)sizeof(t));
    } else if (diag != DIAG_IDEAL) {
        snprintf(t, sizeof(t), "Está perfeito, pode enviar do jeito que está!");
    } else {
        int k = GetRandomValue(0, NUM_PARTES_PROMPT - 1);
        snprintf(t, sizeof(t), "O problema é o %s. Troque já!", NOMES_PARTES_PROMPT[k]);
    }
    LabFalaLuci(b, t, alucina, -1, -1);
}

// Alternativa de maior nota da parte (0 = não incluir, se for o melhor)
static int LabMelhorAlternativa(const MissaoPrompt *m, int k)
{
    int melhor = 0;
    for (int a = 1; a <= 3; a++)
        if (m->nota[k][a] > m->nota[k][melhor]) melhor = a;
    return melhor;
}

// 4. Reformulação: a Luci corrige as DUAS partes mais fracas do pedido do jogador (não revela tudo).
// Aceitar limita a avaliação a 2 estrelas. Instável, ela pode trocar uma parte por algo pior.
static void LabReformular(Biblioteca *b)
{
    LabEdu *L = &b->lab;
    const MissaoPrompt *m = &MISSOES_PROMPT[L->missao];

    if (L->temSugestao || !LabLuciDisponivel(b))
        return;
    LabCobrarLuci(b, LAB_CUSTO_REFORMULACAO);

    int sev[NUM_PARTES_PROMPT] = { 0 }, pa = -1, pb = -1;
    LabContradicao(m, L->escolha, &pa, &pb);
    for (int k = 0; k < NUM_PARTES_PROMPT; k++) {
        int n = LabNotaParte(m, L->escolha, k);
        if (k == pa || k == pb)                                         sev[k] = 4;
        else if (L->escolha[k] > 0 && n < 0)                            sev[k] = 3;
        else if (LabParteNecessaria(m, k) && n <= 0)                    sev[k] = 2;
        else if (LabParteNecessaria(m, k) && n == 1)                    sev[k] = 1;
        else if (!LabParteNecessaria(m, k) && L->escolha[k] > 0 && n == 0) sev[k] = 1;
    }

    for (int k = 0; k < NUM_PARTES_PROMPT; k++) L->sugestao[k] = L->escolha[k];

    int corrigidas[2] = { -1, -1 };
    for (int vez = 0; vez < 2; vez++) {
        int pior = -1;
        for (int k = 0; k < NUM_PARTES_PROMPT; k++)
            if (sev[k] > 0 && (pior < 0 || sev[k] > sev[pior])) pior = k;
        if (pior < 0) break;
        L->sugestao[pior] = LabMelhorAlternativa(m, pior);
        corrigidas[vez] = pior;
        sev[pior] = 0;
    }

    bool alucina = LabLuciAlucina(b);
    int estragada = -1;
    if (alucina) {
        estragada = (corrigidas[0] >= 0) ? corrigidas[GetRandomValue(0, (corrigidas[1] >= 0) ? 1 : 0)] : GetRandomValue(0, NUM_PARTES_PROMPT - 1);
        int pior = 1;
        for (int a = 1; a <= 3; a++) if (m->nota[estragada][a] < m->nota[estragada][pior]) pior = a;
        L->sugestao[estragada] = pior;
    }

    L->temSugestao = true;
    if (corrigidas[0] < 0 && !alucina)
        LabFalaLuci(b, "Eu não mudaria nada: o pedido já está bom. Aceitar a minha versão limita a avaliação a 2 estrelas.", false, -1, -1);
    else
        LabFalaLuci(b, "Reescrevi o pedido (veja nos encaixes). Aceite ou ajuste. Aceitar limita a avaliação a 2 estrelas.", alucina, estragada, -1);
}

// 3. Verificação: a Luci aponta o que parece suspeito (ou a decisão que ela tomaria)
static void LabLuciVerificar(Biblioteca *b)
{
    LabEdu *L = &b->lab;

    if (!LabLuciDisponivel(b))
        return;
    LabCobrarLuci(b, LAB_CUSTO_VERIFICACAO);

    bool alucina = LabLuciAlucina(b);
    char t[220];
    int aponta = -1, fase = L->fase;

    switch (L->tipo) {

        case LAB_PROMPTS: {
            // 0 = "pode aceitar", 1 = "desconfie"
            bool suspeita = L->respostaAlucinada ? !alucina : alucina;
            aponta = suspeita ? 1 : 0;
            fase = 100 + L->envios;
            if (suspeita) snprintf(t, sizeof(t), "%s", alucina ? "Isso aí é invenção da IA! Desconfie."
                                                               : "Essa informação parece estranha. Será que temos alguma fonte que confirme isso?");
            else          snprintf(t, sizeof(t), "%s", alucina ? "Está tudo certo, pode aceitar!"
                                                               : "Não vejo nada inventado. Agora veja se a resposta atende à missão.");
        } break;

        case LAB_ARQUIVO: {
            const DesafioArquivo *d = &DESAFIOS_ARQUIVO[L->missao];
            if (L->fase == 0) {
                aponta = d->suspeita;
                if (alucina) { aponta = GetRandomValue(0, 2); if (aponta >= d->suspeita) aponta++; }
                if (alucina) snprintf(t, sizeof(t), "É a afirmação %d, nem precisa conferir.", aponta + 1);
                else         snprintf(t, sizeof(t), "A afirmação %d parece estranha. Será que temos alguma fonte que confirme isso?", aponta + 1);
            } else {
                aponta = d->metodoCerto;
                if (alucina) { aponta = GetRandomValue(0, 1); if (aponta >= d->metodoCerto) aponta++; }
                if (alucina) snprintf(t, sizeof(t), "Use \"%s\" e pronto.", d->metodos[aponta]);
                else         snprintf(t, sizeof(t), "Eu verificaria com \"%s\": é uma fonte fora da conversa com a IA.", d->metodos[aponta]);
            }
        } break;

        case LAB_NUCLEO: {
            const DesafioNucleo *d = &DESAFIOS_NUCLEO[L->missao];
            int alvo = -1;
            bool dispensavel = false;
            if (!alucina) {
                for (int i = 0; i < d->numBlocos && alvo < 0; i++)
                    if ((L->marcados & (1u << i)) && d->bloco[i].relevancia == REL_IRRELEVANTE) { alvo = i; dispensavel = true; }
                for (int i = 0; i < d->numBlocos && alvo < 0; i++)
                    if (!(L->marcados & (1u << i)) && d->bloco[i].relevancia == REL_ESSENCIAL) { alvo = i; dispensavel = false; }
            } else {
                for (int i = 0; i < d->numBlocos && alvo < 0; i++)
                    if ((L->marcados & (1u << i)) && d->bloco[i].relevancia == REL_ESSENCIAL) { alvo = i; dispensavel = true; }
                for (int i = 0; i < d->numBlocos && alvo < 0; i++)
                    if (!(L->marcados & (1u << i)) && d->bloco[i].relevancia == REL_IRRELEVANTE) { alvo = i; dispensavel = false; }
            }
            aponta = alvo;
            L->luciValor = dispensavel ? 0 : 1;
            if (alvo < 0)        snprintf(t, sizeof(t), "O que está carregado parece certo. Confira a capacidade e processe.");
            else if (alucina)    snprintf(t, sizeof(t), dispensavel ? "Tire \"%s\", é dispensável." : "Carregue \"%s\", é essencial!", d->bloco[alvo].texto);
            else if (dispensavel) snprintf(t, sizeof(t), "\"%s\" não ajuda nesta tarefa e ocupa %d tokens.", d->bloco[alvo].texto, d->bloco[alvo].tokens);
            else                  snprintf(t, sizeof(t), "Sem \"%s\" a IA vai ter que adivinhar. Isso parece essencial.", d->bloco[alvo].texto);
        } break;

        case LAB_FIREWALL: {
            const CasoVies *c = &CASOS_VIES[L->missao];
            if (L->fase < 2) {
                int certo = (L->fase == 0) ? c->problemaCerto : c->medidaCerta;
                const char *const *ops = (L->fase == 0) ? c->problemas : c->medidas;
                aponta = certo;
                if (alucina) { aponta = GetRandomValue(0, 1); if (aponta >= certo) aponta++; }
                if (alucina) snprintf(t, sizeof(t), "É \"%s\".", ops[aponta]);
                else         snprintf(t, sizeof(t), "Eu escolheria \"%s\". Pense em quem é afetado pela decisão.", ops[aponta]);
            } else if (L->pacote < LAB_PACOTES) {
                const PacoteDados *p = &PACOTES_DADOS[L->pacotes[L->pacote]];
                aponta = p->acao;
                if (alucina) { aponta = GetRandomValue(0, 1); if (aponta >= p->acao) aponta++; }
                fase = 20 + L->pacote;
                if (alucina) snprintf(t, sizeof(t), "%s! Sem pensar duas vezes.", NOMES_ACAO_PACOTE[aponta]);
                else         snprintf(t, sizeof(t), "Eu faria %s com esse pacote. Pense: isso identifica alguém ou abre uma conta?", NOMES_ACAO_PACOTE[aponta]);
            }
        } break;

        default: {
            const CasoPesquisa *c = &CASOS_PESQUISA[L->missao];
            int alvo = -1;
            for (int i = 0; i < 5 && alvo < 0; i++)
                if (((L->marcados & (1u << i)) != 0) != c->pista[i].alerta) alvo = i;
            if (alvo < 0) alvo = GetRandomValue(0, 4);
            bool alerta = alucina ? !c->pista[alvo].alerta : c->pista[alvo].alerta;
            aponta = alvo;
            L->luciValor = alerta ? 1 : 0;
            if (alucina) snprintf(t, sizeof(t), alerta ? "\"%s\" é ALERTA, sem dúvida." : "\"%s\" não importa nada.", c->pista[alvo].texto);
            else         snprintf(t, sizeof(t), alerta ? "\"%s\" é um sinal de alerta: fala de origem ou de confirmação." : "\"%s\", sozinha, não prova nada.", c->pista[alvo].texto);
        } break;
    }

    LabCortarUtf8(t);
    LabFalaLuci(b, t, alucina, aponta, fase);
}

// V: confere a sugestão da Luci (gasta uma Lente ou 1 Fragmento). Pegar a alucinação repara a Luci.
static void LabChecarLuci(Biblioteca *b)
{
    LabEdu *L = &b->lab;

    if (!L->luciTemSugestao || L->luciVerificada)
        return;

    if (b->j.itens[ITEM_LENTE] > 0)                       b->j.itens[ITEM_LENTE]--;
    else if (b->perm->fragmentos >= CUSTO_VERIFICAR_FC)   b->perm->fragmentos -= CUSTO_VERIFICAR_FC;
    else {
        snprintf(L->conferencia, sizeof(L->conferencia), "Sem Lente nem Fragmento para conferir. Analise você mesmo!");
        TocarSom(sfxErro);
        return;
    }

    L->luciVerificada = true;
    b->perm->verificou++;
    RogueHabitoLuci(b, false);

    if (L->luciAlucinou) {
        b->perm->alucinacoesPegas++;
        L->alucinacoesPegas++;
        snprintf(L->conferencia, sizeof(L->conferencia), "CONFERIDO: a sugestão NÃO confere com as fontes. A Luci alucinou!");
        RogueMudarEstabilidade(b, 1, NULL);
        TocarSom(sfxAcerto);
    } else {
        snprintf(L->conferencia, sizeof(L->conferencia), "CONFERIDO: a sugestão confere com as fontes.");
        TocarSomTom(sfxSelecao, 1.3f);
    }
    Telemetria("rogue_lab_conferir", L->luciAlucinou ? "pegou" : "conferia", 0.0f, L->ajudas);
}

// ---------- Núcleo Neural ----------

int LabTokensCarregados(const Biblioteca *b)
{
    const DesafioNucleo *d = &DESAFIOS_NUCLEO[b->lab.missao];
    int total = 0;
    for (int i = 0; i < d->numBlocos; i++)
        if (b->lab.marcados & (1u << i)) total += d->bloco[i].tokens;
    return total;
}

static void LabAlternarBloco(Biblioteca *b, int i)
{
    LabEdu *L = &b->lab;

    if (L->marcados & (1u << i)) {
        L->marcados &= ~(1u << i);
        int k = 0;
        for (int j = 0; j < L->numOrdem; j++) if (L->ordem[j] != i) L->ordem[k++] = L->ordem[j];
        L->numOrdem = k;
        TocarSomTom(sfxSelecao, 0.8f);
    } else if (L->numOrdem < LAB_MAX_ITENS) {
        L->marcados |= 1u << i;
        L->ordem[L->numOrdem++] = i;
        TocarSomTom(sfxSelecao, 1.2f);
    }
}

// Processa: se passar da capacidade, a janela perde os blocos MAIS ANTIGOS (como numa conversa longa)
static void LabProcessarNucleo(Biblioteca *b)
{
    LabEdu *L = &b->lab;
    const DesafioNucleo *d = &DESAFIOS_NUCLEO[L->missao];

    int total = LabTokensCarregados(b);
    L->cortados = 0;
    for (int k = 0; k < L->numOrdem && total > d->capacidade; k++) {
        L->cortados |= 1u << L->ordem[k];
        total -= d->bloco[L->ordem[k]].tokens;
    }

    bool falta = false;
    int ruido = 0;
    for (int i = 0; i < d->numBlocos; i++) {
        bool dentro = (L->marcados & (1u << i)) && !(L->cortados & (1u << i));
        if (d->bloco[i].relevancia == REL_ESSENCIAL && !dentro)    falta = true;
        if (d->bloco[i].relevancia == REL_IRRELEVANTE && dentro)   ruido++;
    }

    if (falta)                          L->resultadoNucleo = (L->cortados != 0) ? NUCLEO_TRUNCADA : NUCLEO_FALTOU;
    else if (ruido > 0 || L->cortados)  L->resultadoNucleo = NUCLEO_RUIDOSA;
    else                                L->resultadoNucleo = NUCLEO_BOA;

    L->processos++;
    L->etapa = LAB_RESPOSTA;
    L->digitado = 0.0f;
    L->tempo = 0.0f;
    L->foco = 0;
    L->luciTemSugestao = false;
    TocarSomTom(sfxSelecao, 1.2f);
    Telemetria("rogue_lab_nucleo", "", (float)LabTokensCarregados(b), L->resultadoNucleo);
}

// ---------- Resultado e recompensas ----------

static void LabRecompensar(Biblioteca *b, ProgressoCorredor *prog)
{
    LabEdu *L = &b->lab;
    bool esp = LAB_ESPECIAL[L->tipo];
    int e = L->estrelas;

    int fc = esp ? ((e == 3) ? 8 : ((e == 2) ? 5 : 2)) : ((e == 3) ? 6 : ((e == 2) ? 4 : 2));
    b->perm->fragmentos += fc;
    AlterarPontos(prog, PONTOS_DESAFIO * e + (esp ? 5 : 0));
    RogueRegistrarResposta(b, e >= 2);

    // Estabilidade da Luci: desempenho excelente e resolver sem ajuda reparam
    int estab = 0;
    if (e == 3) estab++;
    if (e >= 2 && L->ajudas == 0) estab++;
    int antes = RogueEstab(b);
    if (estab > 0) RogueMudarEstabilidade(b, estab, NULL);
    estab = RogueEstab(b) - antes;

    snprintf(L->recompensa, sizeof(L->recompensa), "+%d FRAGMENTOS", fc);
    if (estab > 0)
        snprintf(L->recompensa + strlen(L->recompensa), sizeof(L->recompensa) - strlen(L->recompensa), "   +%d ESTABILIDADE DA LUCI", estab);

    // Especiais: item temporário a partir de 2 estrelas
    if (esp && e >= 2) {
        TipoItem it = RogueSortearTemporario();
        b->j.itens[it]++;
        snprintf(L->recompensa + strlen(L->recompensa), sizeof(L->recompensa) - strlen(L->recompensa), "   +1 %s", ITENS[it].nome);
    }

    // 3 estrelas: escolha de melhoria (rara nas especiais); nas comuns, revela um arquivo oculto no mapa
    if (e == 3) {
        b->escolhaPendente = esp ? ESCOLHA_RARA : ESCOLHA_NORMAL;
        snprintf(L->recompensa + strlen(L->recompensa), sizeof(L->recompensa) - strlen(L->recompensa), "   +%s", esp ? "MELHORIA RARA" : "MELHORIA");
        if (!esp)
            for (int i = 0; i < b->mapa.numNos; i++)
                if (b->mapa.no[i].escondida && !b->mapa.no[i].visitado) {
                    b->mapa.no[i].escondida = false;
                    snprintf(L->recompensa + strlen(L->recompensa), sizeof(L->recompensa) - strlen(L->recompensa), "   +ARQUIVO OCULTO NO MAPA (M)");
                    break;
                }
    }
    LabCortarUtf8(L->recompensa);

    b->labFeitos++;
    b->labEstrelas += e;

    if (e == 3) { TocarSom(sfxAcerto); MostrarEmocaoLuci(EMOCAO_ALEGRE, 2.5f); }
    else        TocarSomTom(sfxAcerto, 0.8f);
    Telemetria("rogue_lab", NOMES_LAB[L->tipo], (float)e, L->missao);
}

// Passo 4 (reforço): calcula as estrelas, monta o resumo e entrega a recompensa (uma vez)
void LabFinalizar(Biblioteca *b, ProgressoCorredor *prog)
{
    LabEdu *L = &b->lab;
    int e = 1;

    if (L->etapa == LAB_RESULTADO)
        return;

    switch (L->tipo) {

        case LAB_PROMPTS: {
            bool ruim = L->respostaAlucinada || (L->diag != DIAG_IDEAL && L->diag != DIAG_BOA);
            e = (L->notaPrompt >= 9) ? 3 : ((L->notaPrompt >= 6) ? 2 : 1);
            if (L->aceitou && ruim)                       e = 1;
            if (L->respostaAlucinada && e > (L->pegouAlucinacao ? 2 : 1)) e = L->pegouAlucinacao ? 2 : 1;
            if (L->usouReformulacao && e > 2)            e = 2;

            const char *julg = (L->aceitou && L->respostaAlucinada) ? "Você aceitou uma resposta com informação inventada."
                             : (L->aceitou && ruim)                 ? "Você aceitou uma resposta que não atende à missão."
                             : L->pegouAlucinacao                   ? "Você desconfiou e pegou a invenção da IA!"
                             : L->aceitou                           ? "Boa decisão: a resposta atende à missão."
                                                                    : "Você encerrou sem aceitar a resposta.";
            snprintf(L->resultado, sizeof(L->resultado), "Pedido final: %d/10 (%s). %s", L->notaPrompt, TEXTOS_DIAG[L->diag], julg);
        } break;

        case LAB_ARQUIVO: {
            const DesafioArquivo *d = &DESAFIOS_ARQUIVO[L->missao];
            bool s = (L->sel == d->suspeita), mt = (L->sel2 == d->metodoCerto);
            e = 1 + (s ? 1 : 0) + (mt ? 1 : 0);
            snprintf(L->resultado, sizeof(L->resultado), "Afirmação suspeita: %s.   Forma de verificar: %s.",
                     s ? "CERTA" : "ERRADA", mt ? "CERTA" : "ERRADA");
        } break;

        case LAB_NUCLEO: {
            static const char *txt[4] = { "Só o essencial, dentro da capacidade.", "Faltou informação essencial.",
                                          "Passou da capacidade: o começo foi cortado.", "Funcionou, mas com informação demais." };
            e = (L->resultadoNucleo == NUCLEO_BOA) ? 3 : ((L->resultadoNucleo == NUCLEO_RUIDOSA) ? 2 : 1);
            snprintf(L->resultado, sizeof(L->resultado), "%s  (%d de %d tokens)", txt[L->resultadoNucleo],
                     LabTokensCarregados(b), DESAFIOS_NUCLEO[L->missao].capacidade);
        } break;

        case LAB_FIREWALL:
            e = (L->acertos >= 6) ? 3 : ((L->acertos >= 4) ? 2 : 1);
            snprintf(L->resultado, sizeof(L->resultado), "Decisões éticas corretas: %d de %d.", L->acertos, L->decisoes);
            break;

        default: {
            const CasoPesquisa *c = &CASOS_PESQUISA[L->missao];
            int pistas = 0, consultas = 0;
            for (int i = 0; i < 5; i++) if (((L->marcados & (1u << i)) != 0) == c->pista[i].alerta) pistas++;
            for (int i = 0; i < 3; i++) if (L->ferramentas & (1u << i)) consultas++;
            bool certo = (L->veredito == c->veredito);
            e = !certo ? 1 : ((pistas >= 4) ? 3 : 2);
            if (consultas == 0 && e > 2) e = 2;          // sem investigar a procedência, no máximo 2
            snprintf(L->resultado, sizeof(L->resultado), "Veredito: %s (o certo era %s).   Pistas bem avaliadas: %d de 5.   Consultas: %d.",
                     NOMES_VEREDITO[L->veredito < 0 ? 0 : L->veredito], NOMES_VEREDITO[c->veredito], pistas, consultas);
        } break;
    }

    L->estrelas = e;
    LabCortarUtf8(L->resultado);
    L->etapa = LAB_RESULTADO;
    L->tempo = 0.0f;
    L->foco = 0;
    LabRecompensar(b, prog);
}

// Fecha o laboratório concluído (a porta já estava aberta: o desafio é opcional)
static void LabConcluir(Biblioteca *b)
{
    LabEdu *L = &b->lab;

    L->aberto = false;
    if (L->concluido)
        return;

    L->concluido = true;
    b->limpa = true;
    RogueSalaConcluida(b);
    RogueParticulas(b, b->posObjetoSala, CORES_LAB[L->tipo], 26, 200);
    RogueAviso(b, TextFormat("%s CONCLUÍDO: %d/3", NOMES_LAB[L->tipo], L->estrelas));
}

// ---------- Firewall Ético e Câmara de Pesquisa ----------

static void LabEscolherOpcaoFirewall(Biblioteca *b, int i)
{
    LabEdu *L = &b->lab;
    const CasoVies *c = &CASOS_VIES[L->missao];
    int certo = (L->fase == 0) ? c->problemaCerto : c->medidaCerta;
    const char *const *ops = (L->fase == 0) ? c->problemas : c->medidas;

    LabConferirSeguiu(b, L->fase, i);

    L->decisoes++;
    if (i == certo) { L->acertos++; TocarSom(sfxAcerto); }
    else            TocarSom(sfxErro);

    snprintf(L->avaliacao, sizeof(L->avaliacao), "%s %s", (i == certo) ? "CERTO!" : "NÃO EXATAMENTE. O melhor era:", (i == certo) ? ops[i] : ops[certo]);
    LabCortarUtf8(L->avaliacao);

    if (L->fase == 0) { L->sel = i; L->fase = 1; }
    else              { L->sel2 = i; L->fase = 2; L->nivelDica = 0; L->tempoPacote = LAB_TEMPO_PACOTE; }
    L->luciTemSugestao = false;
    L->tempo = 0.0f;
    L->foco = 0;
}

// Decisão sobre o pacote de dados atual (-1 = passou sozinho pelo firewall)
static void LabDecidirPacote(Biblioteca *b, ProgressoCorredor *prog, int acao)
{
    LabEdu *L = &b->lab;

    if (L->pacote >= LAB_PACOTES)
        return;

    const PacoteDados *p = &PACOTES_DADOS[L->pacotes[L->pacote]];
    int efetiva = (acao < 0) ? ACAO_PERMITIR : acao;
    bool certo = (efetiva == p->acao);

    if (acao >= 0)
        LabConferirSeguiu(b, 20 + L->pacote, acao);

    L->acoes[L->pacote] = acao;
    L->decisoes++;
    if (certo) { L->acertos++; TocarSom(sfxAcerto); }
    else       TocarSom(sfxErro);

    snprintf(L->avaliacao, sizeof(L->avaliacao), "%s%s %s", certo ? "CERTO! " : "ATENÇÃO! ",
             (acao < 0) ? "O pacote passou sem análise." : "", p->porque);
    LabCortarUtf8(L->avaliacao);

    L->pacote++;
    L->tempoPacote = LAB_TEMPO_PACOTE;
    L->luciTemSugestao = false;
    L->foco = 0;

    if (L->pacote >= LAB_PACOTES)
        LabFinalizar(b, prog);
}

static BotaoLab *LabAddBotao(BotaoLab *bt, int *n, Rectangle r, AcaoLab acao, int param, const char *rotulo)
{
    static BotaoLab descarte;

    if (*n >= MAX_BOTOES_LAB) { memset(&descarte, 0, sizeof(descarte)); return &descarte; }

    BotaoLab *o = &bt[(*n)++];
    memset(o, 0, sizeof(*o));
    o->r = r;
    o->acao = acao;
    o->param = param;
    o->ativo = true;
    o->estilo = EST_BOTAO;
    o->cor = WHITE;
    o->fonte = 10;
    o->luz = -1;
    if (rotulo != NULL) { snprintf(o->rotulo, sizeof(o->rotulo), "%s", rotulo); LabCortarUtf8(o->rotulo); }
    return o;
}

static float LabLarguraBotao(const char *rotulo, int custo)
{
    return (float)(MeasureText(rotulo, 10) + 24 + custo * 8);
}

// Botões da Luci (à esquerda, na linha de baixo)
static void LabBotoesLuci(const Biblioteca *b, BotaoLab *bt, int *n, bool analisar, bool reformular, bool verificar)
{
    const LabEdu *L = &b->lab;
    float x = 44.0f, y = (float)LAB_Y_BOTOES;
    Color ciano = { 0, 228, 255, 255 };

    struct { AcaoLab a; const char *r; int custo; bool ativo; bool mostra; } lista[5] = {
        { LB_DICA,       TextFormat("DICA %d/3", (L->nivelDica < 3) ? L->nivelDica + 1 : 3), LAB_CUSTO_DICA, L->nivelDica < 3, true },
        { LB_ANALISAR,   "ANALISAR",   LAB_CUSTO_ANALISE,      true,             analisar },
        { LB_REFORMULAR, "REFORMULAR", LAB_CUSTO_REFORMULACAO, !L->temSugestao,  reformular },
        { LB_VERIFICAR,  "VERIFICAR",  LAB_CUSTO_VERIFICACAO,  true,             verificar },
        { LB_CHECAR,     "CONFERIR (V)", 0, L->luciTemSugestao && !L->luciVerificada, true },
    };

    for (int i = 0; i < 5; i++) {
        if (!lista[i].mostra) continue;
        char rot[48];
        snprintf(rot, sizeof(rot), "%s", lista[i].r);
        float w = LabLarguraBotao(rot, lista[i].custo);
        BotaoLab *o = LabAddBotao(bt, n, (Rectangle){ x, y, w, LAB_ALT_BOTAO }, lista[i].a, 0, rot);
        o->estilo = EST_LUCI;
        o->custo = lista[i].custo;
        o->ativo = lista[i].ativo;
        o->cor = (lista[i].a == LB_CHECAR) ? GOLD : ciano;
        x += w + 6;
    }
}

// Botão de ação alinhado pela direita
static BotaoLab *LabBotaoDireita(BotaoLab *bt, int *n, float *xDir, AcaoLab acao, int param, const char *rotulo, Color cor)
{
    float w = LabLarguraBotao(rotulo, 0) + 6;
    *xDir -= w;
    BotaoLab *o = LabAddBotao(bt, n, (Rectangle){ *xDir, (float)LAB_Y_BOTOES, w, LAB_ALT_BOTAO }, acao, param, rotulo);
    o->cor = cor;
    *xDir -= 6;
    return o;
}

// Altura de um cartão com texto quebrado
static float LabAlturaCartao(const char *texto, int largura, int fonte, int espaco)
{
    char linhas[512];
    QuebrarTexto(texto, largura, fonte, linhas, (int)sizeof(linhas));
    return (float)(ContarLinhas(linhas) * espaco + 12);
}

Rectangle LabRetEncaixe(int k)
{
    return (Rectangle){ 44.0f + k * 180.0f, 102, 172, 66 };
}

// Cartões das peças do encaixe ativo (0 a 2 = alternativas, 3 = deixar vazio)
Rectangle LabRetPeca(int c)
{
    return (Rectangle){ 44, 202.0f + c * 49.0f, 392, 43 };
}

const Color CORES_PARTES_PROMPT[NUM_PARTES_PROMPT] = {
    { 200, 122, 255, 255 }, { 102, 191, 255, 255 }, { 255, 203, 0, 255 }, { 40, 220, 170, 255 }
};
const char *DICAS_PARTES_PROMPT[NUM_PARTES_PROMPT] = { "QUEM responde", "PARA QUEM", "O QUE fazer", "COMO entregar" };

// Coloca a peça (0 = vazio) no encaixe ativo e segue para o próximo
static void LabColocarPeca(Biblioteca *b, int alt)
{
    LabEdu *L = &b->lab;
    const MissaoPrompt *m = &MISSOES_PROMPT[L->missao];
    int k = L->slotAtivo;

    if (alt < 0 || alt > 3 || (alt > 0 && m->alt[k][alt - 1] == NULL))
        return;

    L->escolha[k] = alt;
    L->animPeca = LAB_ANIM_PECA;
    L->animSlot = k;
    L->animDe = (alt == 0) ? 3 : alt - 1;
    if (k < NUM_PARTES_PROMPT - 1)
        L->slotAtivo = k + 1;
    TocarSomTom(sfxPegar, 1.0f + 0.12f * (float)k);
}

int LabMontarBotoes(const Biblioteca *b, BotaoLab *bt, int max)
{
    const LabEdu *L = &b->lab;
    int n = 0;
    float xDir = 756.0f;
    Color cor = CORES_LAB[L->tipo];
    (void)max;

    if (L->etapa == LAB_INTRO) {
        LabBotaoDireita(bt, &n, &xDir, LB_COMECAR, 0, "COMEÇAR", cor);
        LabBotaoDireita(bt, &n, &xDir, LB_SAIR, 0, "AGORA NÃO", GRAY);
        return n;
    }
    if (L->etapa == LAB_RESULTADO) {
        LabBotaoDireita(bt, &n, &xDir, LB_CONCLUIR, 0, "CONCLUIR", cor);
        return n;
    }

    switch (L->tipo) {

        case LAB_PROMPTS: {
            const MissaoPrompt *mp = &MISSOES_PROMPT[L->missao];
            bool editando = (L->etapa == LAB_DESAFIO);
            bool sugestao = editando && L->temSugestao;
            const int *esc = sugestao ? L->sugestao : L->escolha;

            // Os 4 encaixes (clicar escolhe qual editar)
            for (int k = 0; k < NUM_PARTES_PROMPT; k++) {
                BotaoLab *o = LabAddBotao(bt, &n, LabRetEncaixe(k), LB_SELETOR, k, NOMES_PARTES_PROMPT[k]);
                o->estilo = EST_ENCAIXE;
                o->texto = (esc[k] > 0) ? mp->alt[k][esc[k] - 1] : NULL;
                o->ativo = editando && !sugestao;
                o->marcado = editando && !sugestao && k == L->slotAtivo;
                o->chip = sugestao && L->sugestao[k] != L->escolha[k];
                o->luz = sugestao ? -1 : LabLuzVisivel(L, mp, k);
                o->cor = CORES_PARTES_PROMPT[k];
            }

            if (editando) {
                if (sugestao) {
                    // A sugestão da Luci aparece nos encaixes: usar ou descartar
                    LabAddBotao(bt, &n, (Rectangle){ 60, 262, 170, 26 }, LB_USAR_SUGESTAO, 0, "USAR A SUGESTÃO")->cor = (Color){ 0, 228, 255, 255 };
                    LabAddBotao(bt, &n, (Rectangle){ 240, 262, 110, 26 }, LB_DESCARTAR_SUGESTAO, 0, "DESCARTAR")->cor = GRAY;
                } else {
                    // As peças do encaixe ativo (1 a 3) e "deixar vazio" (0)
                    int k = L->slotAtivo;
                    for (int c = 0; c < 4; c++) {
                        int alt = (c < 3) ? c + 1 : 0;
                        if (alt > 0 && mp->alt[k][alt - 1] == NULL) continue;
                        BotaoLab *o = LabAddBotao(bt, &n, LabRetPeca(c), LB_PECA, alt, (alt > 0) ? TextFormat("%d", alt) : "0");
                        o->estilo = EST_PECA;
                        o->texto = (alt > 0) ? mp->alt[k][alt - 1] : "Deixar vazio";
                        o->marcado = (L->escolha[k] == alt);
                        o->cor = CORES_PARTES_PROMPT[k];
                    }
                }
                LabBotoesLuci(b, bt, &n, true, true, false);
                bool vazio = true;
                for (int k = 0; k < NUM_PARTES_PROMPT; k++) if (L->escolha[k] > 0) vazio = false;
                LabBotaoDireita(bt, &n, &xDir, LB_ENVIAR, 0, TextFormat("ENVIAR %d/%d", L->envios + 1, LAB_MAX_ENVIOS), cor)->ativo = !vazio && L->envios < LAB_MAX_ENVIOS && !sugestao;
            } else {
                int len = (int)strlen(LabTextoRespostaAtual(b));
                bool pronto = L->digitado >= (float)len;
                LabBotoesLuci(b, bt, &n, true, false, true);
                LabBotaoDireita(bt, &n, &xDir, LB_ACEITAR, 0, "ACEITAR", LIME)->ativo = pronto;
                if (L->envios < LAB_MAX_ENVIOS)
                    LabBotaoDireita(bt, &n, &xDir, LB_REFAZER, 0, "TROCAR PEÇAS", GOLD)->ativo = pronto;
                else
                    LabBotaoDireita(bt, &n, &xDir, LB_ENCERRAR, 0, "ENCERRAR", GRAY)->ativo = pronto;
                LabBotaoDireita(bt, &n, &xDir, LB_DESCONFIAR, 0, "DESCONFIAR", MAGENTA)->ativo = pronto && !L->desconfiou;
            }
        } break;

        case LAB_ARQUIVO: {
            const DesafioArquivo *d = &DESAFIOS_ARQUIVO[L->missao];
            float y = 150.0f;
            for (int i = 0; i < 4; i++) {
                char rot[8];
                snprintf(rot, sizeof(rot), "%d", i + 1);
                float h = LabAlturaCartao(d->af[i], 410, 12, 16);
                BotaoLab *o = LabAddBotao(bt, &n, (Rectangle){ 56, y, 456, h }, LB_ITEM, i, rot);
                o->estilo = EST_ITEM;
                o->texto = d->af[i];
                o->fonte = 12;
                o->marcado = (L->sel == i);
                o->ativo = (L->fase == 0);
                o->cor = cor;
                y += h + 8;
            }
            if (L->fase == 1) {
                float ym = 176.0f;
                for (int i = 0; i < 3; i++) {
                    float h = LabAlturaCartao(d->metodos[i], 224 - 46, 10, 13);
                    BotaoLab *o = LabAddBotao(bt, &n, (Rectangle){ 532, ym, 224, h }, LB_OPCAO, i, TextFormat("%c", 'A' + i));
                    o->estilo = EST_ITEM;
                    o->texto = d->metodos[i];
                    o->marcado = (L->sel2 == i);
                    o->cor = cor;
                    ym += h + 6;
                }
            }
            LabBotoesLuci(b, bt, &n, false, false, true);
            LabBotaoDireita(bt, &n, &xDir, LB_CONFIRMAR, 0, (L->fase == 0) ? "MARCAR COMO SUSPEITA" : "VERIFICAR ASSIM", cor)->ativo =
                (L->fase == 0) ? (L->sel >= 0) : (L->sel2 >= 0);
        } break;

        case LAB_NUCLEO: {
            const DesafioNucleo *d = &DESAFIOS_NUCLEO[L->missao];
            bool editando = (L->etapa == LAB_DESAFIO);
            for (int i = 0; i < d->numBlocos; i++) {
                BotaoLab *o = LabAddBotao(bt, &n, (Rectangle){ 44.0f + (float)(i % 2) * 242.0f, 128.0f + (float)(i / 2) * 62.0f, 232, 54 },
                                          LB_ITEM, i, TextFormat("%d tk", d->bloco[i].tokens));
                o->estilo = EST_ITEM;
                o->texto = d->bloco[i].texto;
                o->chip = true;
                o->marcado = (L->marcados & (1u << i)) != 0;
                o->ativo = editando;
                o->cor = cor;
            }
            if (editando) {
                LabBotoesLuci(b, bt, &n, false, false, true);
                LabBotaoDireita(bt, &n, &xDir, LB_PROCESSAR, 0, "PROCESSAR", cor)->ativo = L->marcados != 0;
                LabBotaoDireita(bt, &n, &xDir, LB_LIMPAR, 0, "LIMPAR", GRAY)->ativo = L->marcados != 0;
            } else {
                int len = (int)strlen(LabTextoRespostaAtual(b));
                bool pronto = L->digitado >= (float)len;
                LabBotoesLuci(b, bt, &n, false, false, false);
                LabBotaoDireita(bt, &n, &xDir, LB_ENCERRAR, 0, "FICAR COM ESTA RESPOSTA", LIME)->ativo = pronto;
                if (L->processos < LAB_MAX_PROCESSOS && L->resultadoNucleo != NUCLEO_BOA)
                    LabBotaoDireita(bt, &n, &xDir, LB_AJUSTAR, 0, "AJUSTAR O CONTEXTO", GOLD)->ativo = pronto;
            }
        } break;

        case LAB_FIREWALL: {
            const CasoVies *c = &CASOS_VIES[L->missao];
            if (L->fase < 2) {
                const char *const *ops = (L->fase == 0) ? c->problemas : c->medidas;
                for (int i = 0; i < 3; i++) {
                    BotaoLab *o = LabAddBotao(bt, &n, (Rectangle){ 56, 252.0f + i * 44.0f, 688, 38 }, LB_OPCAO, i, TextFormat("%c", 'A' + i));
                    o->estilo = EST_ITEM;
                    o->texto = ops[i];
                    o->cor = cor;
                }
            } else {
                static const Color cores[NUM_ACOES_PACOTE] = { { 0, 228, 48, 255 }, { 230, 41, 55, 255 }, { 255, 203, 0, 255 } };
                for (int a = 0; a < NUM_ACOES_PACOTE; a++) {
                    BotaoLab *o = LabAddBotao(bt, &n, (Rectangle){ 104.0f + a * 204.0f, 352, 184, 40 }, LB_PACOTE, a, NOMES_ACAO_PACOTE[a]);
                    o->cor = cores[a];
                }
            }
            LabBotoesLuci(b, bt, &n, false, false, true);
        } break;

        default: {
            const CasoPesquisa *c = &CASOS_PESQUISA[L->missao];
            int consultas = 0;
            for (int i = 0; i < 3; i++) if (L->ferramentas & (1u << i)) consultas++;
            for (int i = 0; i < 5; i++) {
                BotaoLab *o = LabAddBotao(bt, &n, (Rectangle){ 352, 116.0f + i * 38.0f, 404, 34 }, LB_ITEM, i, "");
                o->estilo = EST_ITEM;
                o->texto = c->pista[i].texto;
                o->marcado = (L->marcados & (1u << i)) != 0;
                o->cor = cor;
            }
            for (int i = 0; i < 3; i++) {
                bool usada = (L->ferramentas & (1u << i)) != 0;
                BotaoLab *o = LabAddBotao(bt, &n, (Rectangle){ 44, 314.0f + i * 30.0f, 290, 26 }, LB_FERRAMENTA, i, c->ferramenta[i]);
                o->ativo = !usada && consultas < LAB_MAX_FERRAMENTAS;
                o->marcado = usada;
                o->cor = SKYBLUE;
            }
            LabBotoesLuci(b, bt, &n, false, false, true);
            static const Color cores[NUM_VEREDITOS] = { { 0, 228, 48, 255 }, { 230, 41, 55, 255 }, { 255, 161, 0, 255 } };
            for (int v = NUM_VEREDITOS - 1; v >= 0; v--)
                LabBotaoDireita(bt, &n, &xDir, LB_VEREDITO, v, NOMES_VEREDITO[v], cores[v]);
        } break;
    }

    return n;
}

// Executa o botão (dir: +1 clique/confirmação, -1 botão direito ou A num seletor)
static void LabExecutar(Biblioteca *b, ProgressoCorredor *prog, const BotaoLab *o, int dir)
{
    LabEdu *L = &b->lab;
    (void)dir;              // (o seletor antigo usava a direção; os encaixes novos não precisam)

    if (!o->ativo)
        return;

    switch (o->acao) {
        case LB_COMECAR:   LabComecar(b); break;
        case LB_SAIR:      L->aberto = false; TocarSom(sfxSelecao); break;
        case LB_CONCLUIR:  LabConcluir(b); break;

        case LB_SELETOR:            // encaixe do laboratório de prompts: passa a editar este
            L->slotAtivo = o->param;
            TocarSom(sfxSelecao);
            break;

        case LB_PECA:
            LabColocarPeca(b, o->param);
            break;

        case LB_ENVIAR:
            L->original = false;
            LabEnviarPrompt(b);
            break;

        case LB_ACEITAR:
            L->aceitou = true;
            LabConferirSeguiu(b, 100 + L->envios, 0);
            LabFinalizar(b, prog);
            break;

        case LB_REFAZER: {
            LabConferirSeguiu(b, 100 + L->envios, 1);
            L->original = false;
            L->etapa = LAB_DESAFIO;
            L->tempo = 0.0f;
            L->foco = 0;
            // Volta direto ao primeiro encaixe que acendeu em vermelho/laranja (ou em conflito)
            const MissaoPrompt *m = &MISSOES_PROMPT[L->missao];
            for (int k = NUM_PARTES_PROMPT - 1; k >= 0; k--) {
                LuzParte z = LabLuzParte(m, L->escolha, k);
                if (z != LUZ_IDEAL && z != LUZ_OK && z != LUZ_DISPENSAVEL) L->slotAtivo = k;
            }
            TocarSom(sfxSelecao);
        } break;

        case LB_DESCONFIAR: {
            const MissaoPrompt *m = &MISSOES_PROMPT[L->missao];
            LabConferirSeguiu(b, 100 + L->envios, 1);
            L->desconfiou = true;
            if (L->respostaAlucinada) {
                L->pegouAlucinacao = true;
                snprintf(L->avaliacao, sizeof(L->avaliacao), "SUSPEITA CONFIRMADA: %s", m->trechoFalso);
                TocarSom(sfxAcerto);
            } else if (L->diag == DIAG_IDEAL || L->diag == DIAG_BOA) {
                snprintf(L->avaliacao, sizeof(L->avaliacao), "Conferido em fontes confiáveis: nada inventado aqui. Desconfiar é um bom hábito!");
                TocarSomTom(sfxSelecao, 1.3f);
            } else {
                snprintf(L->avaliacao, sizeof(L->avaliacao), "Nada inventado, mas a resposta não atende à missão: o problema está no pedido.");
                TocarSomTom(sfxSelecao, 1.3f);
            }
            LabCortarUtf8(L->avaliacao);
        } break;

        case LB_ENCERRAR:   LabFinalizar(b, prog); break;
        case LB_DICA:       LabPedirDica(b); break;
        case LB_ANALISAR:   LabAnalisarPrompt(b); break;
        case LB_REFORMULAR: LabReformular(b); break;
        case LB_VERIFICAR:  LabLuciVerificar(b); break;
        case LB_CHECAR:     LabChecarLuci(b); break;

        case LB_USAR_SUGESTAO:
            for (int k = 0; k < NUM_PARTES_PROMPT; k++) L->escolha[k] = L->sugestao[k];
            L->usouReformulacao = true;
            L->temSugestao = false;
            if (!L->luciVerificada) LabSeguiuLuci(b);
            TocarSom(sfxSelecao);
            break;

        case LB_DESCARTAR_SUGESTAO:
            L->temSugestao = false;
            TocarSom(sfxSelecao);
            break;

        case LB_ITEM:
            if (L->tipo == LAB_ARQUIVO)      L->sel = o->param;
            else if (L->tipo == LAB_NUCLEO)  LabAlternarBloco(b, o->param);
            else                             L->marcados ^= 1u << o->param;
            TocarSom(sfxSelecao);
            break;

        case LB_OPCAO:
            if (L->tipo == LAB_ARQUIVO) { L->sel2 = o->param; TocarSom(sfxSelecao); }
            else                        LabEscolherOpcaoFirewall(b, o->param);
            break;

        case LB_CONFIRMAR:
            if (L->fase == 0) {
                LabConferirSeguiu(b, 0, L->sel);
                L->fase = 1;
                L->foco = 0;
                L->tempo = 0.0f;
                TocarSom(sfxSelecao);
            } else {
                LabConferirSeguiu(b, 1, L->sel2);
                LabFinalizar(b, prog);
            }
            break;

        case LB_PROCESSAR:
            LabProcessarNucleo(b);
            break;

        case LB_LIMPAR:
            L->marcados = 0;
            L->numOrdem = 0;
            TocarSom(sfxSelecao);
            break;

        case LB_AJUSTAR:
            L->etapa = LAB_DESAFIO;
            L->cortados = 0;
            L->tempo = 0.0f;
            L->foco = 0;
            TocarSom(sfxSelecao);
            break;

        case LB_FERRAMENTA:
            L->ferramentas |= 1u << o->param;
            TocarSomTom(sfxPegar, 1.2f);
            break;

        case LB_PACOTE:
            LabDecidirPacote(b, prog, o->param);
            break;

        case LB_VEREDITO:
            L->veredito = o->param;
            LabFinalizar(b, prog);
            break;
    }
}

static int LabProximoAtivo(const BotaoLab *bt, int n, int de, int passo)
{
    for (int k = 1; k <= n; k++) {
        int i = ((de + passo * k) % n + n) % n;
        if (bt[i].ativo) return i;
    }
    return de;
}

// A janela do laboratório: o combate espera. W/S (ou TAB) escolhem, A/D mudam a alternativa
// de um seletor, E/ENTER confirmam, H pede dica, V confere a Luci, BACKSPACE/ESC saem.
void RogueAtualizarLab(Biblioteca *b, ProgressoCorredor *prog, float dt)
{
    LabEdu *L = &b->lab;

    L->tempo += dt;
    if (L->etapa == LAB_DESAFIO || L->etapa == LAB_RESPOSTA)
        L->tempoTotal += dt;
    if (L->animPeca > 0.0f) L->animPeca -= dt;
    if (L->etapa == LAB_RESPOSTA) L->tempoResposta += dt;

    // Montagem do pedido: A/D trocam o encaixe, 1-3 escolhem a peça, 0 deixa vazio, ENTER envia
    bool montagemPrompt = (L->tipo == LAB_PROMPTS && L->etapa == LAB_DESAFIO && !L->temSugestao);

    const char *resp = (L->etapa == LAB_RESPOSTA) ? LabTextoRespostaAtual(b) : NULL;
    int lenResp = (resp != NULL) ? (int)strlen(resp) : 0;
    if (resp != NULL && L->digitado < (float)lenResp)
        L->digitado += dt * LAB_VELOCIDADE_TEXTO;

    // Firewall: o pacote anda até o firewall; sem decisão, ele atravessa sozinho
    if (L->tipo == LAB_FIREWALL && L->etapa == LAB_DESAFIO && L->fase == 2 && L->pacote < LAB_PACOTES) {
        L->tempoPacote -= dt;
        if (L->tempoPacote <= 0.0f) {
            LabDecidirPacote(b, prog, -1);
            return;
        }
    }

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE) || (CliqueMouse() && MouseEm(RET_FECHAR_LAB))) {
        if (L->etapa == LAB_RESULTADO) LabConcluir(b);
        else                           L->aberto = false;      // dá para voltar ao console
        TocarSom(sfxSelecao);
        return;
    }

    BotaoLab bt[MAX_BOTOES_LAB];
    int n = LabMontarBotoes(b, bt, MAX_BOTOES_LAB);
    if (n <= 0)
        return;

    if (L->foco < 0 || L->foco >= n || !bt[L->foco].ativo)
        L->foco = LabProximoAtivo(bt, n, (L->foco < 0 || L->foco >= n) ? n - 1 : L->foco, 1);

    // Teclado
    bool seletor = bt[L->foco].estilo == EST_SELETOR;
    bool lados = !seletor && !montagemPrompt;           // na montagem, A/D trocam o encaixe
    if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN) || (lados && (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT))))
        { L->foco = LabProximoAtivo(bt, n, L->foco, 1);  TocarSom(sfxSelecao); }
    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP) || (lados && (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))))
        { L->foco = LabProximoAtivo(bt, n, L->foco, -1); TocarSom(sfxSelecao); }

    // Mouse
    Vector2 m = GetMousePosition();
    bool moveu = Vector2Length(GetMouseDelta()) > 0.5f;
    int clicado = -1, dirClique = 1;
    for (int i = 0; i < n; i++) {
        if (!bt[i].ativo || !CheckCollisionPointRec(m, bt[i].r)) continue;
        if (moveu) L->foco = i;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))  { clicado = i; dirClique = 1; }
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) { clicado = i; dirClique = -1; }
    }

    if (L->tempo < 0.25f)                   // evita confirmar sem querer ao abrir ou trocar de etapa
        return;

    bool confirmar = IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);

    // A resposta ainda está sendo "digitada": o primeiro toque mostra tudo
    if (resp != NULL && L->digitado < (float)lenResp && (confirmar || clicado >= 0)) {
        L->digitado = (float)lenResp;
        return;
    }

    if (montagemPrompt && clicado < 0) {
        if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))  { L->slotAtivo = (L->slotAtivo + NUM_PARTES_PROMPT - 1) % NUM_PARTES_PROMPT; TocarSom(sfxSelecao); return; }
        if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) { L->slotAtivo = (L->slotAtivo + 1) % NUM_PARTES_PROMPT;                     TocarSom(sfxSelecao); return; }
        for (int a = 1; a <= 3; a++)
            if (IsKeyPressed(KEY_ZERO + a)) { LabColocarPeca(b, a); return; }
        if (IsKeyPressed(KEY_ZERO) || IsKeyPressed(KEY_FOUR)) { LabColocarPeca(b, 0); return; }
        if (IsKeyPressed(KEY_ENTER)) {
            for (int i = 0; i < n; i++)
                if (bt[i].acao == LB_ENVIAR && bt[i].ativo) { LabExecutar(b, prog, &bt[i], 1); return; }
        }
    }

    if (seletor && clicado < 0) {
        if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))  { LabExecutar(b, prog, &bt[L->foco], -1); return; }
        if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) { LabExecutar(b, prog, &bt[L->foco], 1);  return; }
    }

    // Atalhos da Luci
    for (int i = 0; i < n; i++) {
        if (!bt[i].ativo) continue;
        if ((IsKeyPressed(KEY_H) && bt[i].acao == LB_DICA) || (IsKeyPressed(KEY_V) && bt[i].acao == LB_CHECAR)) {
            LabExecutar(b, prog, &bt[i], 1);
            return;
        }
    }

    if (clicado >= 0)                  LabExecutar(b, prog, &bt[clicado], dirClique);
    else if (confirmar && bt[L->foco].ativo) LabExecutar(b, prog, &bt[L->foco], 1);
}
