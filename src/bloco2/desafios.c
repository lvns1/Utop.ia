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

// ---------- Desafios educativos ----------

int RogueEstab(const Biblioteca *b)
{
    return *b->estabilidade;
}

// Chance (%) de a Luci alucinar (dica, verificação, scanner): depende da estabilidade
int RogueChanceLuciErrar(const Biblioteca *b)
{
    return CHANCE_LUCI_ALUCINAR_B2[(int)Clamp((float)RogueEstab(b), 0.0f, (float)ESTABILIDADE_MAX_LUCI)];
}

void RogueRecadoLuci(Biblioteca *b, const char *texto)
{
    snprintf(b->luciMsg, sizeof(b->luciMsg), "%s", texto);
    b->luciMsgTempo = 3.0f;
}

// Sobe ou desce a estabilidade. Subir por uma ação do jogador é um REPARO (vai para o save).
void RogueMudarEstabilidade(Biblioteca *b, int delta, const char *motivo)
{
    int antes = RogueEstab(b);
    *b->estabilidade = (int)Clamp((float)(antes + delta), 0.0f, (float)ESTABILIDADE_MAX_LUCI);

    if (*b->estabilidade > antes) {
        b->perm->reparos++;
        if (motivo != NULL) RogueRecadoLuci(b, TextFormat("LUCI +1: %s", motivo));
        TocarSomTom(sfxPegar, 1.6f);
    } else if (*b->estabilidade < antes && motivo != NULL) {
        RogueRecadoLuci(b, TextFormat("LUCI -1: %s", motivo));
    }
}

// Confiar ou verificar vira hábito; com 3 iguais seguidos, a Luci comenta (pista para o Bloco 3)
void RogueHabitoLuci(Biblioteca *b, bool confiou)
{
    if (confiou) { b->habitoConfiar++;   b->habitoVerificar = 0; }
    else         { b->habitoVerificar++; b->habitoConfiar = 0; }

    if (b->habitoConfiar >= HABITO_LUCI && !b->comentouConfiar) {
        b->comentouConfiar = true;
        b->falaPendente = "Luci: Você nunca me confere... Eu gosto disso. Fica tudo mais fácil para mim.";
    }
    if (b->habitoVerificar >= HABITO_LUCI && !b->comentouVerificar) {
        b->comentouVerificar = true;
        b->falaPendente = "Luci: Você sempre me confere. Faz bem... mesmo que isso me incomode um pouco.";
    }
}

// Desempenho que vai para o save (o Bloco 3 usa): cada resposta, puzzle e a corrupção da Luci
void RogueRegistrarResposta(Biblioteca *b, bool certo)
{
    if (certo) b->perm->desafiosCertos++;
    else       b->perm->desafiosErrados++;

    if (RogueChanceLuciErrar(b) > b->perm->corrupcaoLuciMax)
        b->perm->corrupcaoLuciMax = RogueChanceLuciErrar(b);
}

// Quanto a Luci está "corrompida" (0 a 1): deixa ela magenta e com glitch
float RogueCorrupcaoLuci(const Biblioteca *b)
{
    return 1.0f - (float)RogueEstab(b) / ESTABILIDADE_MAX_LUCI;
}

// Um desafio de conceito já descoberto (de preferência um que ainda não caiu nesta tentativa)
static int RogueSortearDesafio(const Biblioteca *b)
{
    int candidatos[NUM_DESAFIOS], n = 0;

    for (int passada = 0; passada < 2 && n == 0; passada++)
        for (int i = 0; i < NUM_DESAFIOS; i++) {
            if (!(b->perm->conceitos & (1u << DESAFIOS[i].conceito)))
                continue;
            if (passada == 0 && (b->desafiosUsados & (1u << i)))
                continue;
            candidatos[n++] = i;
        }

    return (n > 0) ? candidatos[GetRandomValue(0, n - 1)] : 0;    // (o Byte sempre ensina o 1º conceito)
}

void RogueAbrirDesafio(Biblioteca *b, int indiceInimigo)
{
    b->desafioAberto = true;
    b->alvoDesafio = indiceInimigo;
    b->desafio = RogueSortearDesafio(b);
    b->desafiosUsados |= 1u << b->desafio;
    b->selecaoDesafio = 0;
    b->tempoDesafio = 0.0f;
    b->tempoRespostaSelo = TEMPO_RESPOSTA_SELO;
    b->tempoEsgotadoSelo = false;
    b->respondido = false;
    b->acertouDesafio = false;
    b->usouManual = false;
    b->dicaDesafio[0] = '\0';
    b->dicaDaLuci = false;
    b->luciMentiu = false;
    b->opcaoDica = -1;
    b->verificouDica = false;
    memset(b->opcaoRiscada, 0, sizeof(b->opcaoRiscada));

    TocarSomTom(sfxSelecao, 0.8f);
}

// Move a seleção pulando as opções riscadas
static void RogueMoverSelecaoDesafio(Biblioteca *b, int passo)
{
    int n = DESAFIOS[b->desafio].numOpcoes;

    for (int k = 0; k < n; k++) {
        b->selecaoDesafio = (b->selecaoDesafio + passo + n) % n;
        if (!b->opcaoRiscada[b->selecaoDesafio])
            return;
    }
}

// Lente da Verificação: dica confiável (e, com 3 ou mais opções, risca uma errada)
static void RogueUsarLente(Biblioteca *b)
{
    const DesafioEdu *d = &DESAFIOS[b->desafio];

    if (b->j.itens[ITEM_LENTE] <= 0 || b->respondido)
        return;

    b->j.itens[ITEM_LENTE]--;

    if (d->numOpcoes > 2) {
        int erradas[4], n = 0;
        for (int i = 0; i < d->numOpcoes; i++)
            if (i != d->correta && !b->opcaoRiscada[i])
                erradas[n++] = i;
        if (n > 0)
            b->opcaoRiscada[erradas[GetRandomValue(0, n - 1)]] = true;
        if (b->opcaoRiscada[b->selecaoDesafio])
            RogueMoverSelecaoDesafio(b, 1);
    }

    snprintf(b->dicaDesafio, sizeof(b->dicaDesafio), "LENTE: %s", d->dica);
    b->dicaDaLuci = false;
    TocarSomTom(sfxPegar, 1.3f);
    Telemetria("rogue_lente", "", 0.0f, b->desafio);
}

// H: a Luci aponta uma opção (e diz por quê). Custa 1 de estabilidade; quanto menor
// a estabilidade, maior a chance de ela ALUCINAR. A alucinação vem sem motivo e com
// certeza absoluta: um jogador atento percebe. V verifica (Lente ou 1 Fragmento).
static void RoguePedirLuci(Biblioteca *b)
{
    const DesafioEdu *d = &DESAFIOS[b->desafio];

    if (b->dicaDaLuci || b->respondido)
        return;

    b->dicaDaLuci = true;
    b->verificouDica = false;

    if (RogueEstab(b) <= 0) {
        b->luciMentiu = false;
        b->opcaoDica = -1;
        snprintf(b->dicaDesafio, sizeof(b->dicaDesafio), "Luci: Estou sobrecarregada... Resolva sem mim e eu me estabilizo.");
        MostrarEmocaoLuci(EMOCAO_DUVIDA, 2.0f);
        return;
    }

    RogueMudarEstabilidade(b, -1, NULL);
    b->luciMentiu = GetRandomValue(1, 100) <= RogueChanceLuciErrar(b);

    if (b->luciMentiu) {
        int errada = GetRandomValue(0, d->numOpcoes - 2);
        if (errada >= d->correta) errada++;
        b->opcaoDica = errada;
        snprintf(b->dicaDesafio, sizeof(b->dicaDesafio), "Luci: É \"%s\". Tenho 100%% de certeza!", d->opcoes[errada]);
    } else {
        b->opcaoDica = d->correta;
        snprintf(b->dicaDesafio, sizeof(b->dicaDesafio), "Luci: Acho que é \"%s\". %s", d->opcoes[d->correta], d->dica);
    }

    TocarSom(sfxSelecao);
    Telemetria("rogue_dica_luci", b->luciMentiu ? "alucinou" : "dica", (float)RogueChanceLuciErrar(b), b->desafio);
}

// V: confere a dica da Luci (gasta uma Lente ou 1 Fragmento). Pegar a alucinação repara a Luci.
static void RogueVerificarDica(Biblioteca *b)
{
    if (!b->dicaDaLuci || b->opcaoDica < 0 || b->verificouDica || b->respondido)
        return;

    if (b->j.itens[ITEM_LENTE] > 0)                       b->j.itens[ITEM_LENTE]--;
    else if (b->perm->fragmentos >= CUSTO_VERIFICAR_FC)   b->perm->fragmentos -= CUSTO_VERIFICAR_FC;
    else {
        RogueRecadoLuci(b, "Sem Lente nem Fragmento para verificar.");
        TocarSom(sfxErro);
        return;
    }

    b->verificouDica = true;
    b->perm->verificou++;
    RogueHabitoLuci(b, false);

    if (b->luciMentiu) {
        b->perm->alucinacoesPegas++;
        b->opcaoRiscada[b->opcaoDica] = true;
        if (b->selecaoDesafio == b->opcaoDica) RogueMoverSelecaoDesafio(b, 1);
        snprintf(b->dicaDesafio, sizeof(b->dicaDesafio), "VERIFICADO: a fonte NÃO confere. A Luci alucinou essa resposta!");
        RogueMudarEstabilidade(b, 1, "você pegou a alucinação");
        TocarSom(sfxAcerto);
    } else {
        snprintf(b->dicaDesafio, sizeof(b->dicaDesafio), "VERIFICADO: a fonte confere. %s", DESAFIOS[b->desafio].dica);
        TocarSomTom(sfxSelecao, 1.3f);
    }
    Telemetria("rogue_verificar", b->luciMentiu ? "pegou" : "conferia", 0.0f, b->desafio);
}

// esgotou = o cronômetro chegou a zero: é um erro comum (o Manual não salva e não conta como seguir a Luci)
static void RogueResponderDesafio(Biblioteca *b, ProgressoCorredor *prog, bool esgotou)
{
    const DesafioEdu *d = &DESAFIOS[b->desafio];
    int s = b->selecaoDesafio;

    if (b->respondido)
        return;

    if (!esgotou && b->opcaoRiscada[s])
        return;

    if (!esgotou && s != d->correta && b->j.itens[ITEM_MANUAL] > 0 && !b->usouManual) {

        // Manual do Conhecimento: o erro não conta, a opção errada fica riscada
        b->j.itens[ITEM_MANUAL]--;
        b->usouManual = true;
        b->opcaoRiscada[s] = true;
        RogueMoverSelecaoDesafio(b, 1);
        snprintf(b->dicaDesafio, sizeof(b->dicaDesafio), "MANUAL DO CONHECIMENTO: não foi essa. Você tem outra chance!");
        b->dicaDaLuci = false;
        TocarSom(sfxErro);
        Telemetria("rogue_manual", "", 0.0f, b->desafio);
        return;
    }

    b->respondido = true;
    b->tempoEsgotadoSelo = esgotou;
    b->acertouDesafio = !esgotou && (s == d->correta);
    b->desafiosRespondidos++;
    RogueRegistrarResposta(b, b->acertouDesafio);

    // Confiar: seguiu a dica sem verificar. Certa = bônus; alucinada = a Luci se abala.
    if (!esgotou && b->dicaDaLuci && b->opcaoDica >= 0 && !b->verificouDica && s == b->opcaoDica) {
        b->perm->confiouCego++;
        RogueHabitoLuci(b, true);
        if (b->luciMentiu) {
            b->perm->confiouAlucinacao++;
            RogueMudarEstabilidade(b, -1, "você acreditou na alucinação");
        } else {
            AlterarPontos(prog, BONUS_CONFIAR_PONTOS);
            b->perm->fragmentos += BONUS_CONFIAR_FC;
        }
    }

    // Resolveu sem a Luci: ela se estabiliza
    if (b->acertouDesafio && !b->dicaDaLuci)
        RogueMudarEstabilidade(b, 1, "resolvido sem ajuda");

    if (b->acertouDesafio) {
        AlterarPontos(prog, PONTOS_DESAFIO);
        b->perm->fragmentos += FC_POR_DESAFIO;
        TocarSom(sfxAcerto);
    } else {
        AlterarPontos(prog, -PENALIDADE_ERRO);
        TocarSom(sfxErro);
    }

    Telemetria("rogue_desafio", b->acertouDesafio ? "certo" : (esgotou ? "tempo" : "errado"),
               (float)((b->dicaDaLuci ? 1 : 0) + (b->luciMentiu ? 2 : 0)), b->desafio);
}

// Depois da explicação: o selo quebra. Acertou = o guardião fica fraco e atordoado;
// errou = ele usa a informação falsa num ataque especial (anel de balas).
static void RogueFecharDesafio(Biblioteca *b)
{
    Inimigo *e = &b->ini[b->alvoDesafio];

    b->desafioAberto = false;

    if (!e->ativo)
        return;

    e->selado = false;

    if (b->acertouDesafio) {
        e->hp -= e->hpMax * DANO_SELO_CERTO;
        e->flash = 0.3f;
        e->estado = 0;
        e->atordoado = ATORDOADO_SELO;
        e->recarga = ATORDOADO_SELO + 0.5f;
        RogueParticulas(b, e->pos, GOLD, 24, 220);
        RogueAviso(b, "SELO QUEBRADO! O guardião está atordoado.");
    } else {
        for (int k = 0; k < BALAS_ATAQUE_ESPECIAL; k++) {
            float ang = (2.0f * PI / BALAS_ATAQUE_ESPECIAL) * k;
            RogueDisparar(b, e->pos, (Vector2){ cosf(ang), sinf(ang) }, 190.0f, 9.0f, 6.0f, false);
        }
        e->estado = 0;
        e->recarga = GUARDIAO_INTERVALO;
        RogueParticulas(b, e->pos, MAGENTA, 24, 220);
        RogueAviso(b, "O guardião usa a informação falsa contra você!");
    }
}

// Onde fica a opção i do desafio do guardião (a mesma conta de DesenharDesafioRogue), para o mouse
static Rectangle RogueRetOpcaoDesafio(const Biblioteca *b, int i)
{
    const DesafioEdu *d = &DESAFIOS[b->desafio];
    Rectangle painel = { 60, 70, 680, 440 };
    int x = (int)painel.x + 24, y = (int)painel.y + 16;
    char linhas[512];

    QuebrarTexto(d->enunciado, (int)painel.width - 48, 18, linhas, (int)sizeof(linhas));
    y += 42 + ContarLinhas(linhas) * 24 + 14;

    for (int k = 0; k <= i && k < d->numOpcoes; k++) {
        char op[256];
        QuebrarTexto(TextFormat("%d) %s", k + 1, d->opcoes[k]), (int)painel.width - 90, 16, op, (int)sizeof(op));
        int n = ContarLinhas(op);
        if (k == i)
            return (Rectangle){ (float)x, (float)y - 4, painel.width - 48, (float)(n * 20 + 6) };
        y += n * 20 + 8;
    }

    return (Rectangle){ 0, 0, 0, 0 };
}

const char *RogueTextoLenteDesafio(const Biblioteca *b)
{
    return (b->j.itens[ITEM_LENTE] > 0) ? TextFormat("L: Lente da Verificação (x%d)", b->j.itens[ITEM_LENTE])
                                        : "L: (sem Lente da Verificação)";
}

const char *RogueTextoLuciDesafio(const Biblioteca *b)
{
    return !b->dicaDaLuci ? "H: dica da Luci (-1 de estabilidade)"
         : ((b->verificouDica || b->opcaoDica < 0) ? "H: a Luci já deu a dica" : "V: verificar a dica (Lente ou 1 FC)");
}

void RogueAtualizarDesafio(Biblioteca *b, ProgressoCorredor *prog, float dt)
{
    const DesafioEdu *d = &DESAFIOS[b->desafio];
    bool confirmar = IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);

    b->tempoDesafio += dt;

    // Cronômetro: começa quando a pergunta aparece e só corre enquanto ela espera resposta
    // (o RogueAtualizar não roda na pausa nem nas transições). Chegou a zero: erro, uma vez
    // só (respondido = true trava a volta e o tempo para de descer).
    if (!b->respondido) {
        b->tempoRespostaSelo -= dt;
        if (b->tempoRespostaSelo <= 0.0f) {
            b->tempoRespostaSelo = 0.0f;
            RogueResponderDesafio(b, prog, true);
            b->tempoDesafio = 0.0f;         // a explicação não fecha com uma tecla apertada no susto
            return;
        }
    }

    if (b->tempoDesafio < 0.4f)             // evita responder sem querer
        return;

    bool clique = CliqueMouse();

    if (b->respondido) {
        if (confirmar || clique)
            RogueFecharDesafio(b);
        return;
    }

    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))   { RogueMoverSelecaoDesafio(b, -1); TocarSom(sfxSelecao); }
    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) { RogueMoverSelecaoDesafio(b, 1);  TocarSom(sfxSelecao); }

    // Mouse: passar por cima marca, clicar responde (opção riscada não vale)
    for (int i = 0; i < d->numOpcoes; i++) {
        if (b->opcaoRiscada[i] || !MouseEm(RogueRetOpcaoDesafio(b, i))) continue;
        if (MouseMexeu() && b->selecaoDesafio != i) { b->selecaoDesafio = i; TocarSom(sfxSelecao); }
        if (clique) { b->selecaoDesafio = i; confirmar = true; }
    }

    // Os atalhos do rodapé também são clicáveis
    if (clique && MouseEm(RetTextoClicavel(RogueTextoLenteDesafio(b), X_LENTE_DESAFIO, Y_AJUDA_DESAFIO, 12)))
        RogueUsarLente(b);
    if (clique && MouseEm(RetTextoClicavel(RogueTextoLuciDesafio(b), X_LUCI_DESAFIO, Y_AJUDA_DESAFIO, 12))) {
        if (b->dicaDaLuci) RogueVerificarDica(b);
        else               RoguePedirLuci(b);
    }

    for (int i = 0; i < d->numOpcoes; i++)
        if (IsKeyPressed(KEY_ONE + i) && !b->opcaoRiscada[i]) {
            b->selecaoDesafio = i;
            confirmar = true;
        }

    if (IsKeyPressed(KEY_L)) RogueUsarLente(b);
    if (IsKeyPressed(KEY_H)) RoguePedirLuci(b);
    if (IsKeyPressed(KEY_V)) RogueVerificarDica(b);

    if (confirmar)
        RogueResponderDesafio(b, prog, false);
}
