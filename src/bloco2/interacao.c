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


static bool RoguePertoDaPorta(const Biblioteca *b, Rectangle r)
{
    return CheckCollisionCircleRec(b->j.pos, RAIO_CORPO_JOGADOR + DISTANCIA_PORTA_E, r);
}

// Porta aberta ao alcance do E (*indice: qual porta da missão)
PortaInteracao RoguePortaProxima(const Biblioteca *b, int *indice)
{
    *indice = -1;

    if (b->morreu)
        return PORTA_NENHUMA;

    if (b->tipoSala == SALA_ATRIO) {
        if (RoguePertoDaPorta(b, PORTA_SAIDA_ROGUE))                             return PORTA_ATRIO_CORREDOR;
        if (b->perm->bloco3Liberado && RoguePertoDaPorta(b, PORTA_BLOCO3_ROGUE)) return PORTA_ATRIO_NUCLEO;
        if (RoguePertoDaPorta(b, PORTA_PROXIMA_ROGUE))                           return PORTA_ATRIO_MISSAO;
    }
    else if (b->tipoSala == SALA_TRANSICAO) {
        if (b->limpa && RoguePertoDaPorta(b, PORTA_PROXIMA_ROGUE)) return PORTA_TRANSICAO_LUCI;
    }
    else if (b->noAtual >= 0 && b->limpa) {
        for (int k = 0; k < b->numPortas; k++)
            if (RoguePertoDaPorta(b, b->portas[k].r)) { *indice = k; return PORTA_DA_MISSAO; }
        if (b->temParedeSecreta && b->passagemAberta && RoguePertoDaPorta(b, PAREDE_SECRETA_ROGUE[b->paredeSecreta]))
            return PORTA_SECRETA;
    }

    return PORTA_NENHUMA;
}

AlvoRogue RogueAlvoProximo(const Biblioteca *b, int *pedestal)
{
    Vector2 p = b->j.pos;

    // Reformulação: recompensa no chão, altar/poltrona/console e arquivo para ler
    if (b->recompensaAtiva && Vector2Distance(p, b->recompensaPos) < 44.0f)
        return ALVO_RECOMPENSA;
    if (b->tipoSala == SALA_LABORATORIO && !b->lab.concluido && !b->lab.emAcao && Vector2Distance(p, b->posObjetoSala) < 58.0f)
        return ALVO_LAB;
    if ((b->tipoSala == SALA_EVENTO || b->tipoSala == SALA_DESCANSO || b->tipoSala == SALA_DESAFIO) &&
        !b->eventoUsado && !b->eventoCombate && Vector2Distance(p, b->posObjetoSala) < 58.0f)
        return ALVO_OBJETO_SALA;
    if (b->registroSala && !b->registroSalaLido && Vector2Distance(p, b->posRegistroSala) < 50.0f)
        return ALVO_REGISTRO_SALA;

    if (b->tipoSala == SALA_ATRIO) {
        if (Vector2Distance(p, POS_TERMINAL_SELETOR) < RAIO_TERMINAL_ATALHO)    return ALVO_TERMINAL_SELETOR;
        if (Vector2Distance(p, POS_TERMINAL_BLOCO3) < RAIO_TERMINAL_ATALHO)     return ALVO_TERMINAL_BLOCO3;
        if (Vector2Distance(p, POS_BYTE) < 70)                                  return ALVO_BYTE;
        if (Vector2Distance(p, (Vector2){ POS_BYTECO.x, POS_BYTECO.y + 50 }) < 60) return ALVO_BYTECO;
        if (Vector2Distance(p, POS_ENCICLOPEDIA) < 60)                          return ALVO_ENCICLOPEDIA;
    }
    else if (b->tipoSala == SALA_EDUCATIVA && !b->terminalLido) {
        if (Vector2Distance(p, POS_TERMINAL) < 70) return ALVO_TERMINAL;
    }
    else if (b->tipoSala == SALA_RECOMPENSA && !b->recompensaPega) {
        for (int i = 0; i < NUM_PEDESTAIS; i++)
            if (Vector2Distance(p, POS_PEDESTAIS[i]) < 42) { *pedestal = i; return ALVO_PEDESTAL; }
    }
    else if (b->tipoSala == SALA_TRANSICAO) {
        for (int i = 0; i < 3; i++)
            if (!(b->registrosLidos & (1 << i)) && Vector2Distance(p, POS_REGISTROS[i]) < 60.0f) { *pedestal = i; return ALVO_REGISTRO; }
    }
    else if (b->tipoSala == SALA_LUCI) {
        if (RogueAlvoContexto(b, pedestal)) return ALVO_CONTEXTO;     // *pedestal: pedaço (k) ou o núcleo (-1)
    }
    else if (b->tipoSala == SALA_MESTRE) {
        if (b->mestre.etapa == MT_ARMA && Vector2Distance(p, POS_ARMA_MESTRE) < 56.0f) return ALVO_ARMA_MESTRE;
    }
    else if (b->tipoSala == SALA_SILOGISTA) {
        // Montagem do silogismo (o índice do fragmento ou do mecanismo vai em *pedestal).
        // Carregando, o mecanismo vem primeiro; de mãos vazias, o fragmento no chão.
        const Silogista *s = &b->silogista;

        if (!s->ativo || !s->apresentado || s->etapa != SIL_MONTAGEM || s->modoBH)
            return ALVO_NADA;

        for (int vez = 0; vez < 2; vez++) {
            bool encaixes = (vez == 0) == (s->carregando >= 0);
            for (int i = 0; i < NUM_ENCAIXES; i++) {
                if (encaixes && (s->carregando >= 0 || s->encaixe[i] >= 0) && Vector2Distance(p, POS_ENCAIXES[i]) < SIL_RAIO_ENCAIXE)
                    { *pedestal = i; return ALVO_ENCAIXE; }
                if (!encaixes && SilPecaNoChao(s, i) && Vector2Distance(p, s->posPeca[i]) < SIL_RAIO_PEGAR)
                    { *pedestal = i; return ALVO_PECA; }
            }
        }
    }

    return ALVO_NADA;
}
int PrecoItemLoja(const Biblioteca *b, int item)
{
    int preco = ITENS[item].preco;
    if (item == b->ofertaItem) {
        preco = (preco * (100 - OFERTA_DESCONTO_PERC) + 99) / 100;      // arredonda para cima
        if (preco < 1) preco = 1;
    }
    return preco;
}

static bool RogueComprar(Biblioteca *b, TipoItem item)
{
    const DefItem *it = &ITENS[item];
    int *tenho = &b->j.itens[item];

    if (it->passivo && *tenho >= it->maxPilha) {
        RogueAviso(b, "Você já tem o máximo desse item.");
        TocarSom(sfxErro);
        return false;
    }

    int preco = PrecoItemLoja(b, (int)item);
    if (b->perm->fragmentos < preco) {
        RogueAviso(b, TextFormat("Faltam %d FC.", preco - b->perm->fragmentos));
        TocarSom(sfxErro);
        return false;
    }

    b->perm->fragmentos -= preco;
    (*tenho)++;
    RogueAviso(b, TextFormat("Comprou: %s (agora tem %d)", it->nome, *tenho));
    TocarSomTom(sfxAcerto, 1.2f);
    Telemetria("rogue_loja", it->nome, (float)preco, b->perm->fragmentos);
    return true;
}

// Melhoria permanente: fica para sempre (vai para o save)
static bool RogueComprarMelhoria(Biblioteca *b, int m)
{
    int nivel = PermNivel(b->perm, m);

    if (nivel >= NIVEL_MAX_MELHORIA) {
        RogueAviso(b, "Essa melhoria já está no nível máximo.");
        TocarSom(sfxErro);
        return false;
    }

    if (b->perm->fragmentos < PRECO_MELHORIA[nivel]) {
        RogueAviso(b, "Fragmentos de Conhecimento insuficientes.");
        TocarSom(sfxErro);
        return false;
    }

    b->perm->fragmentos -= PRECO_MELHORIA[nivel];
    PermSubir(b->perm, m);

    RogueSincronizarPerm(b);
    if (m == MELHORIA_REROLL) b->j.rerolls++;
    // A vida e a energia novas já valem agora
    if (m == MELHORIA_VIDA)    b->j.hp += MELHORIA_HP;
    if (m == MELHORIA_ENERGIA) b->j.ep += MELHORIA_EP;

    RogueAviso(b, TextFormat("%s: nível %d!", NOMES_MELHORIAS[m], PermNivel(b->perm, m)));
    TocarSomTom(sfxAcerto, 1.3f);
    Telemetria("rogue_melhoria", NOMES_MELHORIAS[m], (float)PRECO_MELHORIA[nivel], PermNivel(b->perm, m));
    return true;
}

// Arma do Byteco: vira a arma extra desta tentativa (substitui a anterior)
static bool RogueComprarArma(Biblioteca *b, TipoArma arma)
{
    const DefArma *a = &DEF_ARMAS[arma];

    if (arma == ARMA_DISPARADOR || RogueTemArma(&b->j, arma)) {
        RogueAviso(b, "Você já tem essa arma.");
        TocarSom(sfxErro);
        return false;
    }

    if (b->perm->fragmentos < a->preco) {
        RogueAviso(b, "Fragmentos de Conhecimento insuficientes.");
        TocarSom(sfxErro);
        return false;
    }

    // Vagas cheias: o jogador escolhe qual arma trocar e só então paga (cancelar não custa nada)
    if (b->j.armaExtra != ARMA_DISPARADOR) {
        RogueReceberArma(b, arma, 1, -1);
        return false;
    }

    b->perm->fragmentos -= a->preco;
    RogueInstalarArma(b, arma, 1);
    RogueAviso(b, TextFormat("Comprou: %s (TAB troca)", a->nome));
    TocarSomTom(sfxAcerto, 1.2f);
    Telemetria("rogue_loja", a->nome, (float)a->preco, b->perm->fragmentos);
    return true;
}

// Troca de arma: as duas cartas (as armas que você carrega) e o botão de cancelar
Rectangle RogueCartaTroca(int i) { return (Rectangle){ 120.0f + 290.0f * (float)i, 262.0f, 270.0f, 190.0f }; }

static void RogueFecharTroca(Biblioteca *b) { b->trocaArmaAberta = false; }

static void RogueAtualizarTroca(Biblioteca *b)
{
    b->animTroca += DtJogo();

    if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) { b->trocaSel = 1 - b->trocaSel; TocarSom(sfxSelecao); }
    if (IsKeyPressed(KEY_ONE))  { b->trocaSel = 0; TocarSom(sfxSelecao); }
    if (IsKeyPressed(KEY_TWO))  { b->trocaSel = 1; TocarSom(sfxSelecao); }

    bool clique = false;
    Vector2 m = GetMousePosition();
    for (int i = 0; i < 2; i++)
        if (CheckCollisionPointRec(m, RogueCartaTroca(i))) {
            if (Vector2Length(GetMouseDelta()) > 0.5f) b->trocaSel = i;
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { b->trocaSel = i; clique = true; }
        }

    if (b->animTroca < 0.3f) return;                // evita confirmar sem querer ao abrir

    if (IsKeyPressed(KEY_BACKSPACE) || (CliqueMouse() && MouseEm(RET_CANCELAR_TROCA))) {
        RogueFecharTroca(b);
        RogueAviso(b, (b->trocaOrigem == 0) ? "Arma descartada." : "Você manteve as suas armas.");
        TocarSom(sfxSelecao);
        return;
    }

    if (clique || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        JogadorRogue *j = &b->j;
        int vaga = b->trocaSel;
        TipoArma velha = (vaga == 0) ? j->armaBase : j->armaExtra;
        const DefArma *nova = &DEF_ARMAS[b->armaNova];

        if (b->trocaOrigem == 1) {                              // loja: paga agora
            if (b->perm->fragmentos < nova->preco) {
                RogueAviso(b, "Fragmentos de Conhecimento insuficientes.");
                TocarSom(sfxErro);
                RogueFecharTroca(b);
                return;
            }
            b->perm->fragmentos -= nova->preco;
            Telemetria("rogue_loja", nova->nome, (float)nova->preco, b->perm->fragmentos);
        }
        else if (b->trocaOrigem == 2) {                         // pedestal: leva a arma e a recompensa some
            b->recompensaPega = true;
            RogueParticulas(b, POS_PEDESTAIS[b->trocaPedestal], nova->cor, 20, 180);
            Telemetria("rogue_recompensa", nova->nome, 0.0f, 1);
        }

        RogueInstalarArma(b, b->armaNova, vaga);
        RogueAviso(b, TextFormat("Trocou %s por %s", DEF_ARMAS[velha].nome, nova->nome));
        RogueParticulas(b, j->pos, nova->cor, 20, 180);
        TocarSomTom(sfxAcerto, 1.2f);
        RogueFecharTroca(b);
    }
}

// Linha do conceito c na enciclopédia (a mesma conta de DesenharEnciclopediaRogue)
static Rectangle RogueRetConceito(int c)
{
    return (Rectangle){ 122, 120.0f + c * 96.0f, 556, 88 };
}

static void RogueAtualizarEnciclopedia(Biblioteca *b, SessaoDialogo *dlg)
{
    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))   { b->selecaoEnciclopedia = (b->selecaoEnciclopedia + NUM_CONCEITOS - 1) % NUM_CONCEITOS; TocarSom(sfxSelecao); }
    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) { b->selecaoEnciclopedia = (b->selecaoEnciclopedia + 1) % NUM_CONCEITOS;                  TocarSom(sfxSelecao); }

    bool clique = CliqueMouse();

    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_ESCAPE) || (clique && MouseEm(RET_FECHAR_ENCICLOPEDIA))) {
        b->enciclopediaAberta = false;
        return;
    }

    bool confirmar = IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);

    // Mouse: passar por cima marca o conceito, clicar lê
    for (int k = 0; k < NUM_CONCEITOS; k++) {
        if (!MouseEm(RogueRetConceito(k))) continue;
        if (MouseMexeu() && b->selecaoEnciclopedia != k) { b->selecaoEnciclopedia = k; TocarSom(sfxSelecao); }
        if (clique) { b->selecaoEnciclopedia = k; confirmar = true; }
    }

    int c = b->selecaoEnciclopedia;

    if (!confirmar)
        return;

    if (b->perm->conceitos & (1u << c)) {
        b->enciclopediaAberta = false;      // volta quando a leitura terminar
        DialogoIniciar(dlg, TEXTOS_CONCEITOS[c], OBJ_ENCICLOPEDIA);
    } else {
        RogueAviso(b, "Conceito ainda não descoberto.");
        TocarSom(sfxErro);
    }
}

// Loja do Byteco: A/D troca a aba, W/S escolhe, E (ou ENTER) compra, BACKSPACE/ESC sai
// Abas e linhas da loja (a mesma conta de DesenharLojaRogue), para o mouse
static Rectangle RogueRetAbaLoja(int a)
{
    return (Rectangle){ 174.0f + a * 152.0f, 92, 146, 24 };
}

static Rectangle RogueRetLinhaLoja(int i)
{
    return (Rectangle){ 162, 126.0f + i * 46.0f, 476, 40 };
}

static void RogueAtualizarLoja(Biblioteca *b)
{
    bool clique = CliqueMouse();
    int abaClicada = -1;

    for (int a = 0; a < 3; a++)
        if (clique && MouseEm(RogueRetAbaLoja(a)))
            abaClicada = a;

    bool esquerda = IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT), direita = IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT);
    if (abaClicada >= 0) {
        if (abaClicada != b->abaLoja) {
            b->abaLoja = abaClicada;
            b->selecaoLoja = 0;
            TocarSom(sfxSelecao);
        }
    } else if (esquerda || direita) {
        int nova = (b->abaLoja + (direita ? 1 : 2)) % 3;
        if (nova != b->abaLoja) {
            b->abaLoja = nova;
            b->selecaoLoja = 0;
            TocarSom(sfxSelecao);
        }
    }

    int linhas = (b->abaLoja == 0) ? NUM_ITENS_LOJA : ((b->abaLoja == 1) ? NUM_MELHORIAS : ARMAS_NA_LOJA);
    int total = linhas + 1;                         // a última linha é SAIR

    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))   { b->selecaoLoja = (b->selecaoLoja + total - 1) % total; TocarSom(sfxSelecao); }
    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) { b->selecaoLoja = (b->selecaoLoja + 1) % total;         TocarSom(sfxSelecao); }

    bool sair = IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_ESCAPE);
    bool confirmar = IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);

    // Mouse: passar por cima marca a linha, clicar compra (ou sai, na linha SAIR)
    for (int i = 0; i < total; i++) {
        if (!MouseEm(RogueRetLinhaLoja(i))) continue;
        if (MouseMexeu() && b->selecaoLoja != i) { b->selecaoLoja = i; TocarSom(sfxSelecao); }
        if (clique && abaClicada < 0) { b->selecaoLoja = i; confirmar = true; }
    }

    if (sair || (confirmar && b->selecaoLoja == linhas)) {
        b->lojaAberta = false;
        return;
    }

    if (confirmar && b->abaLoja == 0)      RogueComprar(b, (TipoItem)b->selecaoLoja);
    else if (confirmar && b->abaLoja == 1) RogueComprarMelhoria(b, b->selecaoLoja);
    else if (confirmar)                    RogueComprarArma(b, b->estoqueArmas[b->selecaoLoja]);
}

// E perto de alguém ou de algo: fala, lê ou pega
static void RogueInteragir(Biblioteca *b, SessaoDialogo *dlg, AlvoRogue alvo, int pedestal)
{
    switch (alvo) {

        case ALVO_ARMA_MESTRE:
            b->mestre.etapa = MT_TIRO;              // o pedestal some já (a etapa começa no fim da conversa)
            RogueParticulas(b, POS_ARMA_MESTRE, GOLD, 30, 220);
            RogueAnel(b, POS_ARMA_MESTRE, 70.0f, GOLD);
            TocarSomTom(sfxAcerto, 1.2f);
            DialogoIniciar(dlg, &dialogoMestreArma, OBJ_MESTRE_ARMA);
            break;

        case ALVO_BYTE:
            DialogoIniciar(dlg, b->perm->byteVisto ? &dialogoByteDepois : &dialogoByte, OBJ_BYTE);
            break;

        case ALVO_BYTECO:
            if (b->perm->bytecoVisto) { b->lojaAberta = true; b->selecaoLoja = 0; }
            else                      DialogoIniciar(dlg, &dialogoByteco, OBJ_BYTECO);
            break;

        case ALVO_TERMINAL: {
            // Os dois primeiros ensinam os conceitos; a partir do 3º, terminais que reforçam o que já foi visto
            const Dialogo *d = (b->educativas == 0) ? &dialogoTerminalAlucinacao
                             : (b->educativas == 1) ? &dialogoTerminalPraticas
                             : ((b->educativas % 2 == 0) ? &dialogoTerminalVies : &dialogoTerminalFontes);
            DialogoIniciar(dlg, d, OBJ_TERMINAL);
            b->educativas++;
        } break;

        case ALVO_PEDESTAL: {
            TipoArma arma = b->armaPedestal[pedestal];
            if (arma != ARMA_DISPARADOR) {
                if (RogueTemArma(&b->j, arma)) { RogueAviso(b, "Você já tem essa arma."); break; }
                if (!RogueReceberArma(b, arma, 2, pedestal)) break;     // vagas cheias: abre a escolha de qual trocar
                b->recompensaPega = true;
                RogueAviso(b, TextFormat("Pegou: %s (TAB troca)", DEF_ARMAS[arma].nome));
                RogueParticulas(b, POS_PEDESTAIS[pedestal], DEF_ARMAS[arma].cor, 20, 180);
                TocarSomTom(sfxAcerto, 1.2f);
                Telemetria("rogue_recompensa", DEF_ARMAS[arma].nome, 0.0f, 1);
                break;
            }
            TipoItem it = b->pedestal[pedestal];
            b->j.itens[it]++;
            b->recompensaPega = true;
            RogueAviso(b, TextFormat("Pegou: %s", ITENS[it].nome));
            RogueParticulas(b, POS_PEDESTAIS[pedestal], GOLD, 20, 180);
            TocarSomTom(sfxAcerto, 1.2f);
            Telemetria("rogue_recompensa", ITENS[it].nome, 0.0f, 0);
        } break;

        case ALVO_ENCICLOPEDIA:
            b->enciclopediaAberta = true;
            b->selecaoEnciclopedia = 0;
            break;

        case ALVO_PECA:
            SilPegarPeca(b, pedestal);
            break;

        case ALVO_ENCAIXE:
            SilUsarEncaixe(b, pedestal);
            break;

        case ALVO_REGISTRO:
            b->registroAtual = pedestal;
            DialogoIniciar(dlg, RogueDialogoRegistro(b, pedestal), OBJ_REGISTRO);
            break;

        case ALVO_CONTEXTO:
            RogueLuciContexto(b, pedestal);
            break;

        case ALVO_RECOMPENSA:
            RogueUsarRecompensa(b);
            break;

        case ALVO_TERMINAL_SELETOR:
            if (!b->perm->byteVisto) {
                DialogoFalaAvulsa(dlg, "SELETOR DE CHEFES: BLOQUEADO. Converse primeiro com o Professor Byte (perto da porta de cima) para liberar este terminal.");
                Telemetria("seletor_bloqueado", "", 0.0f, 0);
            } else {
                RogueAbrirSeletor(b);
            }
            break;

        case ALVO_TERMINAL_BLOCO3:
            DialogoConfirmar(dlg, TEXTO_ATALHO_BLOCO3, OBJ_ATALHO_BLOCO3);
            Telemetria("atalho_bloco3_terminal", "", 0.0f, b->perm->bloco3Liberado ? 1 : 0);
            break;

        case ALVO_LAB:
            if (LabEmAcaoTipo(b->lab.tipo) && b->lab.etapa != LAB_RESULTADO) {
                LabComecarAcao(b);              // Núcleo Neural e Firewall Ético: na própria sala
                break;
            }
            b->lab.aberto = true;
            b->lab.tempo = 0.0f;
            TocarSom(sfxSelecao);
            break;

        case ALVO_OBJETO_SALA:
            b->eventoAberto = true;
            b->eventoResolvido = false;
            b->selecaoEvento = 0;
            TocarSom(sfxSelecao);
            break;

        case ALVO_REGISTRO_SALA:
            if (b->registroSalaId >= 100) {
                int m = b->registroSalaId - 100;
                b->memoriasVistas |= 1u << m;
                DialogoIniciar(dlg, &dialogosMemoria[m], OBJ_REGISTRO_SALA);
            } else {
                DialogoIniciar(dlg, &dialogosAmeaca[b->registroSalaId], OBJ_REGISTRO_SALA);
            }
            break;

        default:
            break;
    }
}

// Terminou uma conversa da Biblioteca: o que ela muda
static void RogueFimConversa(Biblioteca *b, ProgressoCorredor *prog, int obj)
{
    if (obj == OBJ_MESTRE_ARMA) {
        MestreMudarEtapa(b, MT_TIRO);
        RogueAviso(b, "DISPARADOR DE DADOS!");
        return;
    }
    if (obj == OBJ_MESTRE_VITORIA) {
        AlterarPontos(prog, MESTRE_PONTOS);
        b->mestreVencidoPendente = true;
        return;
    }

    if (obj == OBJ_BYTE) {
        b->perm->byteVisto = true;
        if (!(b->perm->conceitos & (1u << CONCEITO_FUNDAMENTOS)))
            RogueAviso(b, "NOVO CONCEITO NA ENCICLOPÉDIA!");
        b->perm->conceitos |= 1u << CONCEITO_FUNDAMENTOS;
    }
    else if (obj == OBJ_ENCICLOPEDIA) {
        b->enciclopediaAberta = true;
    }
    else if (obj == OBJ_BYTECO) {
        b->perm->bytecoVisto = true;
        // Primeira visita: brinde (1 Antivírus) e a Luci mostra a porta da missão; a loja abre em seguida
        if (b->j.itens[ITEM_ANTIVIRUS] < ITENS[ITEM_ANTIVIRUS].maxPilha)
            b->j.itens[ITEM_ANTIVIRUS]++;
        RogueAviso(b, "BRINDE DO BYTECO: 1 ANTIVÍRUS (tecla 1)");
        RogueRecadoLuci(b, "Luci: Quando terminar aqui, a porta da missão fica no alto do átrio. Eu aviso quando algo for novo.");
        Telemetria("atrio_byteco_brinde", "", (float)(GetTime() - b->inicio), b->perm->tentativas);
        b->lojaAberta = true;
        b->selecaoLoja = 0;
    }
    else if (obj == OBJ_TERMINAL) {
        b->terminalLido = true;
        b->limpa = true;
        RogueMudarEstabilidade(b, 1, "base de conhecimento confiável");
        b->perm->conceitos |= 1u << ((b->educativas <= 1) ? CONCEITO_ALUCINACOES : CONCEITO_PRATICAS);
        b->perm->fragmentos += FC_POR_TERMINAL;
        AlterarPontos(prog, PONTOS_TERMINAL);
        RogueAviso(b, TextFormat("+%d FRAGMENTOS DE CONHECIMENTO", FC_POR_TERMINAL));
        RogueParticulas(b, POS_TERMINAL, (Color){ 0, 228, 255, 255 }, 24, 200);
        TocarSom(sfxAcerto);
        MostrarEmocaoLuci(EMOCAO_ALEGRE, 2.0f);
        Telemetria("rogue_terminal", TextFormat("conceito %d", b->educativas), 0.0f, b->perm->fragmentos);
    }
    else if (obj == OBJ_REGISTRO_SALA) {
        // Narrativa opcional: arquivo de ameaça (+1 fragmento) ou memória da Luci (estabiliza)
        b->registroSalaLido = true;
        if (b->registroSalaId >= 100) {
            RogueMudarEstabilidade(b, 1, "memória recuperada");
        } else {
            b->perm->fragmentos += 1;
            RogueAviso(b, "+1 FRAGMENTO DE CONHECIMENTO");
        }
        Telemetria("rogue_registro_sala", TextFormat("%d", b->registroSalaId), 0.0f, b->profundidade);
    }
    else if (obj == OBJ_CHEFE_INTRO) {
        b->chefe.apresentado = true;
    }
    else if (obj == OBJ_SILOGISTA_INTRO) {
        b->silogista.apresentado = true;
    }
    else if (obj == OBJ_SILOGISTA_ETAPA) {
        // Rodada 1 do circuito resolvida: a rodada 2 começa depois da explicação
        Silogista *s = &b->silogista;
        if (s->etapa == SIL_CIRCUITO && s->nucleo > 0.0f) {
            SilPrepararRodada(s, 2);
            s->nucleo = 0.0f;
        }
    }
    else if (obj == OBJ_REGISTRO) {
        b->registrosLidos |= 1 << b->registroAtual;
        if (b->registrosLidos == 7 && !b->limpa) {
            b->limpa = true;
            b->conversaPendente = &dialogoNucleoAberto;
            b->objConversa = OBJ_NENHUM;
        }
    }
    else if (obj == OBJ_LUCI_INTRO) {
        b->luci.apresentado = true;
    }
    else if (obj == OBJ_LUCI_FINAL) {
        // Fim de UtopIA: o final, os pontos e a tela de resultado (depois, o ranking)
        if (!b->perm->jogoConcluido || b->perm->finalJogo == 0) {
            b->perm->jogoConcluido = true;
            b->perm->finalJogo = b->finalBom ? 1 : 2;
            b->perm->indiceFinal = b->indice.total;
            AlterarPontos(prog, b->finalBom ? PONTOS_FINAL_BOM : PONTOS_FINAL_RUIM);
            Telemetria("fim_do_jogo", b->finalBom ? "bom" : "ruim", (float)b->indice.total, prog->pontos);
        }
        b->pontosFinais = prog->pontos;
        b->resultadoAberto = true;
        b->tempoResultado = 0.0f;
    }
    else if (obj == OBJ_SILOGISTA_VITORIA) {
        // Recompensa (uma vez por vitória): Selo da Inferência, conceito novo e a porta aberta
        Silogista *s = &b->silogista;
        if (!s->recompensaDada) {
            s->recompensaDada = true;
            b->j.seloInferencia = true;
            if (s->modoBH) {
                // Desafio opcional vencido: Selo amplificado e um alívio de estabilidade
                s->bh.desafioConcluido = s->bh.recompensaObtida = true;
                b->j.seloAmplificado = true;
                b->bonusSilogistaSessao = true;
                b->j.ep = RogueEpMax(b);
                RogueMudarEstabilidade(b, 1, "raciocínio verificado");
            }
            if (!(b->perm->conceitos & (1u << CONCEITO_LOGICA)))
                RogueAviso(b, "NOVO CONCEITO NA ENCICLOPÉDIA!");
            else
                RogueAviso(b, "SELO DA INFERÊNCIA!");
            b->perm->conceitos |= 1u << CONCEITO_LOGICA;
            b->perm->silogistaVencido = true;
            if (!b->modoSeletor || b->recompensaSeletor) {          // seletor: só na 1ª vitória
                b->perm->fragmentos += FC_SILOGISTA;
                AlterarPontos(prog, PONTOS_SILOGISTA);
            }
            RogueSalaConcluida(b);
            MostrarEmocaoLuci(EMOCAO_ALEGRE, 3.0f);
            Telemetria("rogue_silogista_vitoria", "", (float)(GetTime() - b->inicio), b->perm->tentativas);
        }
        b->limpa = true;
        if (b->modoSeletor)
            RogueSeletorVenceu(b, prog, 0);      // o átrio volta depois desta explicação
    }
    else if (obj == OBJ_ALUCINADOR_VITORIA && b->modoSeletor) {
        // Seletor de chefes: a vitória libera o Bloco 3 como sempre, mas volta ao átrio
        // (sem ranking de fim da Biblioteca) e a recompensa só sai na 1ª vitória
        bool recompensa = b->recompensaSeletor;
        b->perm->alucinadorVencido = true;
        b->perm->bloco3Liberado = true;
        if (recompensa) {
            b->perm->fragmentos += FC_ALUCINADOR;
            AlterarPontos(prog, PONTOS_ALUCINADOR);
        }
        Telemetria("rogue_chefe_vitoria", "seletor", (float)(GetTime() - b->inicio), b->perm->tentativas);
        RogueSeletorVenceu(b, prog, 0);
        RogueVoltarAoLobby(b);
        b->portaNucleoAbrindo = 3.0f;
        b->estaticaTela = 0.5f;
        b->falaPendente = "Luci: Aquela porta à direita... leva ao núcleo de UtopIA. Quando quiser, eu vou com você.";
    }
    else if (obj == OBJ_ALUCINADOR_VITORIA) {
        // Fim da Biblioteca: os fragmentos voltam, a mochila esvazia e o main mostra o ranking
        b->perm->alucinadorVencido = true;
        b->perm->bloco3Liberado = true;
        b->perm->fragmentos += FC_ALUCINADOR;
        AlterarPontos(prog, PONTOS_ALUCINADOR);
        Telemetria("rogue_chefe_vitoria", "", (float)(GetTime() - b->inicio), b->perm->tentativas);
        RogueEsvaziarMochila(&b->j);
        RogueEntrarAtrio(b, false);
        b->vitoriaPendente = true;

        // Continuidade linear para o Bloco 3: a porta da direita (x = 746) se abre com um glitch
        b->portaNucleoAbrindo = 3.0f;
        b->estaticaTela = 0.5f;
        b->falaPendente = "Luci: Aquela porta à direita... leva ao núcleo de UtopIA. Eu preciso que você venha. Agora.";
    }
}

// Confirmação de um terminal da Biblioteca (SIM / NÃO)
static void RogueRespostaConfirmacao(Biblioteca *b, int obj, bool sim)
{
    if (obj != OBJ_ATALHO_BLOCO3)
        return;

    Telemetria("atalho_bloco2_bloco3", sim ? "sim" : "nao", (float)(GetTime() - b->inicio), b->perm->bloco3Liberado ? 1 : 0);
    if (!sim)
        return;

    // A mesma entrada da porta do núcleo: a cutscene e depois RogueEntrarBloco3 (sem exigir bloco3Liberado
    // e sem marcar o Alucinador como vencido)
    b->transicaoNucleo = TRANSICAO_NUCLEO;
    b->entrouNucleo = false;
    PararSons();
    TocarSom(sfxEstatica);
}

EventoRogue RogueAtualizar(Biblioteca *b, ProgressoCorredor *prog, SessaoDialogo *dlg, float dt)
{
    b->lentidao = 0.0f;                             // quem quiser câmera lenta pede de novo neste quadro (sala da Mestre)
    b->progRef = prog;
    b->pontosRun = prog->pontos;                    // os registros do Bloco 3 mostram
    b->perm->segundosRun += dt;

    for (int i = 0; i < MAX_PARTICULAS_ROGUE; i++) {
        ParticulaRogue *p = &b->part[i];
        if (p->vida <= 0.0f) continue;
        p->vida -= dt;
        p->pos = Vector2Add(p->pos, Vector2Scale(p->vel, dt));
        p->vel = Vector2Scale(p->vel, 1.0f - 3.0f * dt);
    }

    if (b->tremor > 0.0f)      b->tremor -= dt;
    if (b->fadeEntrada > 0.0f) b->fadeEntrada -= dt;
    if (b->tempoAviso > 0.0f)  b->tempoAviso -= dt;
    if (b->faixaVirada > 0.0f) b->faixaVirada -= dt;
    if (b->flashDanoJog > 0.0f) b->flashDanoJog -= dt;

    // Bloco 3 (reformulação): efeitos de tela e de impacto
    if (b->tremorForte > 0.0f)        b->tremorForte -= dt;
    if (b->zoomImpacto > 0.0f)        b->zoomImpacto -= dt;
    if (b->flashNegativo > 0.0f)      b->flashNegativo -= dt;
    if (b->estaticaTela > 0.0f)       b->estaticaTela -= dt;
    if (b->tempoErro > 0.0f)          b->tempoErro -= dt;
    if (b->portaNucleoAbrindo > 0.0f) b->portaNucleoAbrindo -= dt;
    if (b->tituloBloco > 0.0f)        b->tituloBloco -= dt;
    for (int i = 0; i < MAX_TEXTOS_FLUTUANTES; i++)
        if (b->textos[i].tempo > 0.0f) { b->textos[i].tempo -= dt; b->textos[i].pos.y -= 28.0f * dt; }

    // Cutscene da porta do núcleo: glitch, estática e a inversão de paleta (o jogo espera)
    if (b->transicaoNucleo > 0.0f) {
        b->transicaoNucleo -= dt;
        if (!b->entrouNucleo && b->transicaoNucleo <= TRANSICAO_NUCLEO * 0.45f) {
            b->entrouNucleo = true;
            RogueEntrarBloco3(b, prog);
            b->flashNegativo = 0.6f;
            b->estaticaTela = 0.8f;
        }
        return ROGUE_NADA;
    }

    if (b->vitoriaPendente) {
        b->vitoriaPendente = false;
        return ROGUE_VITORIA;
    }

    // Venceu a ALUCINAÇÃO MESTRE: fim do Bloco 1 (o main abre o ranking) e o átrio do Bloco 2
    if (b->mestreVencidoPendente) {
        // Em vez de cortar seco para o átrio: a Mestre foge, a porta se abre e o cenário muda atrás da estática
        b->mestreVencidoPendente = false;
        b->transicaoMestre = TRANSICAO_MESTRE;
        b->trocouCenaMestre = false;
        b->estaticaTela = 0.5f;
        TocarSom(sfxEstatica);
        RogueRecadoLuci(b, "Luci: Ela fugiu pela porta! Vamos atrás: lá as alucinações imitam o que você já aprendeu a duvidar.");
        Telemetria("ponte_bloco1_2", "inicio", (float)(GetTime() - b->inicio), 0);
    }

    if (b->transicaoMestre > 0.0f) {
        b->transicaoMestre -= dt;
        b->estaticaTela = fmaxf(b->estaticaTela, 0.35f * (1.0f - b->transicaoMestre / TRANSICAO_MESTRE));   // o ruído cresce até a troca

        if (!b->trocouCenaMestre && b->transicaoMestre <= TRANSICAO_MESTRE * 0.5f) {
            b->trocouCenaMestre = true;
            RogueSairSalaMestre(b);                         // átrio, cartão "BLOCO 2" e a conversa de chegada (pendente)
            b->tituloBloco += fmaxf(0.0f, b->transicaoMestre);   // o cartão só começa a contar de verdade depois da cutscene
            b->flashNegativo = 0.5f;
            b->estaticaTela = 0.8f;
            TocarSom(sfxEstatica);
            return ROGUE_MESTRE_VENCIDO;                    // o main marca o fim do Bloco 1 e pede o auto save
        }
        return ROGUE_NADA;                                  // o jogo espera a cutscene (nada se move, nada machuca)
    }

    // Cena da Luci (Bloco 3): deixando o Thomas / virando a chefe. O jogo espera.
    if (RogueAtualizarCenaLuci(b, dt))
        return ROGUE_NADA;

    // Tela de resultado do fim do jogo: ENTER leva ao ranking e ao menu
    if (b->resultadoAberto) {
        b->tempoResultado += dt;
        if (b->tempoResultado > 1.0f && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE) || CliqueMouse())) {
            b->resultadoAberto = false;
            RogueEsvaziarMochila(&b->j);
            RogueEntrarAtrio(b, false);
            return ROGUE_FIM_JOGO;
        }
        return ROGUE_NADA;
    }

    // Conversa aberta (Byte, Byteco, terminal, chefe): o resto espera
    if (dlg->estado != DLG_NENHUM) {

        int obj = dlg->objeto;

        ResultadoDialogo r = DialogoAtualizar(dlg);
        if (r == RES_FIM)
            RogueFimConversa(b, prog, obj);
        else if (r == RES_SIM || r == RES_NAO)
            RogueRespostaConfirmacao(b, obj, r == RES_SIM);

        return ROGUE_NADA;
    }

    if (b->trocaArmaAberta) {
        RogueAtualizarTroca(b);
        return ROGUE_NADA;
    }

    if (b->lojaAberta) {
        RogueAtualizarLoja(b);
        return ROGUE_NADA;
    }

    if (b->enciclopediaAberta) {
        RogueAtualizarEnciclopedia(b, dlg);
        return ROGUE_NADA;
    }

    // Desafio educativo aberto: o combate espera
    if (b->desafioAberto) {
        RogueAtualizarDesafio(b, prog, dt);
        return ROGUE_NADA;
    }

    // Reformulação: escolha 1 de 3, eventos e o mapa grande (o combate espera)
    if (b->escolhaAberta) {
        RogueAtualizarEscolha(b);
        return ROGUE_NADA;
    }

    if (b->eventoAberto) {
        RogueAtualizarEvento(b, prog, dlg);
        return ROGUE_NADA;
    }

    // Expansão educativa: a janela do laboratório (o combate espera)
    if (b->lab.aberto) {
        RogueAtualizarLab(b, prog, dt);
        return ROGUE_NADA;
    }

    // Seletor de chefes aberto: o átrio espera
    if (b->seletorAberto) {
        RogueAtualizarSeletor(b);
        return ROGUE_NADA;
    }

    if (b->escolhaPendente >= 0) {
        RogueAbrirEscolha(b, (ModoEscolha)b->escolhaPendente, true);
        b->escolhaPendente = -1;
        return ROGUE_NADA;
    }

    if (b->mapaAberto) {
        if (IsKeyPressed(KEY_M) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_ENTER) || CliqueMouse())
            b->mapaAberto = false;
        return ROGUE_NADA;
    }

    if (IsKeyPressed(KEY_M) && b->noAtual >= 0 && !b->morreu) {
        b->mapaAberto = true;
        TocarSom(sfxSelecao);
        return ROGUE_NADA;
    }

    // Conversa do chefe guardada para quando a sala terminar de aparecer
    if (b->conversaPendente != NULL && b->fadeEntrada <= 0.0f) {
        DialogoIniciar(dlg, b->conversaPendente, b->objConversa);
        b->conversaPendente = NULL;
        return ROGUE_NADA;
    }

    // Fala da Luci guardada para quando a sala terminar de aparecer
    if (b->falaPendente != NULL && b->fadeEntrada <= 0.0f) {
        DialogoFalaAvulsa(dlg, b->falaPendente);
        b->falaPendente = NULL;
        return ROGUE_NADA;
    }

    // Seletor de chefes: venceu e os diálogos de vitória já passaram -> de volta ao átrio
    if (b->modoSeletor && b->seletorVencido && b->tipoSala != SALA_ATRIO) {
        b->seletorRetorno += dt;
        b->j.invulneravel = fmaxf(b->j.invulneravel, 0.2f);       // nada mais machuca depois da vitória
        if (b->seletorRetorno >= SELETOR_TEMPO_RETORNO) {
            RogueVoltarAoLobby(b);
            return ROGUE_NADA;
        }
    }

    if (b->pausaImpacto > 0.0f) {
        b->pausaImpacto -= dt;
        return ROGUE_NADA;
    }

    // Derrota: volta para o átrio sem os itens da tentativa
    if (b->morreu) {
        b->tempoMorte += dt;
        bool confirmar = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE) || CliqueMouse();
        if (b->tempoMorte > 1.0f && (confirmar || b->tempoMorte > 5.0f)) {
            b->morreu = false;
            if (b->tipoSala == SALA_TRANSICAO || b->tipoSala == SALA_LUCI) {
                // Bloco 3: volta ao corredor do núcleo sem perder nada
                if (b->tipoSala == SALA_LUCI) b->perm->derrotasLuci++;
                RogueLimparEfeitos(&b->j);
                RogueEntrarSala(b, SALA_TRANSICAO);
                b->j.hp = RogueHpMax(b);
                b->j.ep = RogueEpMax(b);
            } else if (b->modoSeletor) {
                // Seletor de chefes: a derrota volta ao átrio e limpa o desafio
                Telemetria("seletor_derrota", (b->chefeSeletor >= 0) ? NOMES_CHEFES_SELETOR[b->chefeSeletor] : "", (float)(GetTime() - b->inicio), 0);
                RogueVoltarAoLobby(b);
            } else {
                RogueEsvaziarMochila(&b->j);
                RogueEntrarAtrio(b, false);
            }
        }
        return ROGUE_NADA;
    }

    // Focus Mode (Silogista bullet-hell): projéteis, inimigos e o Silogista andam com dt * 0,3
    float dtMundo = dt * RogueFatorTempoMundo(b);

    RogueAtualizarJogador(b, prog, dt);
    RogueAtualizarInimigos(b, dtMundo);
    RogueAtualizarProjeteis(b, prog, dtMundo);
    RogueAtualizarColetaveis(b, dt);

    RogueAtualizarDedutivo(b, dt);
    RogueAtualizarEfeitos(b, dt);
    RogueAtualizarLuci(b, prog, dt);

    if (b->tipoSala == SALA_CHEFE)
        RogueAtualizarChefe(b, prog, dt);

    if (b->tipoSala == SALA_SILOGISTA) {
        RogueAtualizarSilogista(b, prog, dtMundo);
        RogueAtualizarSaidaSilogista(b, dt);
    }

    if (b->tipoSala == SALA_LUCI)
        RogueAtualizarLuciChefe(b, prog, dt);

    if (b->tipoSala == SALA_MESTRE)
        RogueAtualizarMestre(b, prog, dt);

    b->tempoSalaAtual += dt;
    RogueAtualizarLabAcao(b, prog, dt);
    RogueAtualizarAfixos(b, dtMundo);

    // Reformulação: armadilhas, áreas da build e provas
    RogueAtualizarArmadilhas(b, prog, dt);
    RogueAtualizarAreas(b, prog, dt);
    RogueAtualizarTornado(b, prog, dt);
    RogueAtualizarDesafioSala(b, prog, dt);
    if (b->mensagemSistema > 0.0f) b->mensagemSistema -= dt;
    if (b->marcaParede > 0.0f)     b->marcaParede -= dt;

    // Chegou perto do Guardião selado: o desafio abre
    for (int i = 0; i < MAX_INIMIGOS && !b->desafioAberto; i++) {
        Inimigo *e = &b->ini[i];
        if (e->ativo && e->selado && e->surgindo <= 0.0f && Vector2Distance(e->pos, b->j.pos) < RAIO_DESAFIO)
            RogueAbrirDesafio(b, i);
    }

    // Sala da Mestre (fim do Bloco 1): cair não é derrota, a Luci reergue o Thomas
    if (b->tipoSala == SALA_MESTRE && b->j.hp <= 0.0f) {
        RogueMestreReerguer(b);
        return ROGUE_NADA;
    }

    if (b->j.hp <= 0.0f) {
        b->j.hp = 0.0f;
        b->morreu = true;
        b->tempoMorte = 0.0f;
        b->perm->mortes++;
        AlterarPontos(prog, -PENALIDADE_HP_ZERO);
        RogueParticulas(b, b->j.pos, SKYBLUE, 30, 260);
        RogueAnel(b, b->j.pos, 120.0f, RED);
        TocarGrupo(&gJogMorte, 1.0f, 0.95f);
        Telemetria("rogue_morte", TextFormat("sala %d", b->indiceRota + 1), (float)(GetTime() - b->inicio), b->abatidos);
        if (b->perm->tentativas <= 1)
            Telemetria("primeira_missao_morte", TextFormat("sala %d", b->indiceRota + 1), (float)(GetTime() - b->inicio), b->abatidos);
        return ROGUE_NADA;
    }

    // Objetivo da sala de combate (eliminar, ondas, sobreviver, núcleos, alvo prioritário, mecanismo).
    // Concluído, abre as portas e deixa a recompensa (ver RogueConcluirCombate).
    RogueAtualizarObjetivo(b, prog, dt);

    // Portas: só com E, perto da porta (a porta tem prioridade sobre o que estiver ao lado)
    int portaK = -1;
    PortaInteracao porta = RoguePortaProxima(b, &portaK);

    if (IsKeyPressed(KEY_E) && porta != PORTA_NENHUMA) {
        switch (porta) {
            case PORTA_ATRIO_CORREDOR:
                return ROGUE_SAIR;

            case PORTA_ATRIO_NUCLEO:
                // Continuidade linear: a porta do núcleo abre a cutscene de transição
                b->transicaoNucleo = TRANSICAO_NUCLEO;
                b->entrouNucleo = false;
                PararSons();
                TocarSom(sfxEstatica);
                Telemetria("bloco3_porta", "", (float)(GetTime() - b->inicio), b->perm->tentativas);
                return ROGUE_NADA;

            case PORTA_ATRIO_MISSAO:
                if (b->perm->byteVisto) RogueComecarMissao(b);
                else DialogoFalaAvulsa(dlg, "Luci: Antes de entrar, vamos falar com o Professor Byte. Ele está ali, perto da porta!");
                return ROGUE_NADA;

            case PORTA_TRANSICAO_LUCI:
                RogueEntrarSala(b, SALA_LUCI);
                return ROGUE_NADA;

            case PORTA_DA_MISSAO:
                RogueAtravessarPorta(b, portaK);
                return ROGUE_NADA;

            case PORTA_SECRETA:
                RogueEntrarSecreta(b);      // passagem secreta aberta: um desvio opcional
                return ROGUE_NADA;

            default:
                break;
        }
    }

    // E: falar, ler, pegar
    int pedestal = -1;
    AlvoRogue alvo = RogueAlvoProximo(b, &pedestal);

    if (IsKeyPressed(KEY_E) && alvo != ALVO_NADA) {
        RogueInteragir(b, dlg, alvo, pedestal);
        return ROGUE_NADA;
    }

    // E sem nada por perto (nem um mecanismo para segurar): a Luci repete a dica
    if (IsKeyPressed(KEY_E) && b->tipoSala != SALA_TRANSICAO && b->tipoSala != SALA_LUCI) {
        bool mecanismo = false;
        for (int k = 0; k < b->numMecanismos; k++)
            if (!b->mecanismoDesligado[k] && Vector2Distance(b->j.pos, b->posMecanismo[k]) < MECANISMO_RAIO) mecanismo = true;
        if (!mecanismo) g_luciPedirDica = true;
    }

    return ROGUE_NADA;
}
