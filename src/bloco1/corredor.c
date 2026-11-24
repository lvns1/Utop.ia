#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <time.h>
#include "../../include/core/fonte.h"
#include "../../include/core/mapas.h"
#include "../../include/bloco1/corredor.h"
#include "../../include/audio/audio.h"
#include "../../include/bloco1/minigame_visual.h"
#include "../../include/core/save.h"
#include "../../include/core/telemetria.h"
#include "../../include/core/texto.h"
#include "../../include/entidades/jogador.h"
#include "../../include/entidades/luci_companion.h"
#include "../../include/telas/ranking.h"
#include "../../include/telas/historico_luci.h"

Texture2D texMapaCorredor = {0};      // sem o arquivo, o corredor usa o cenário simples

// =============================================================
// FUNÇÕES AUXILIARES - CORREDOR
// =============================================================

int ContarConcluidos(const ProgressoCorredor *p)
{
    int total = 0;

    for (int i = 0; i < TOTAL_MONITORES; i++)
        if (p->estado[i] == MON_ACERTOU || p->estado[i] == MON_ERROU)
            total++;

    return total;
}

// Primeiro monitor que ainda não foi resolvido (-1 = todos resolvidos)
int ProximoMonitor(const ProgressoCorredor *p)
{
    for (int i = 0; i < TOTAL_MONITORES; i++)
        if (p->estado[i] == MON_NEUTRO || p->estado[i] == MON_ATIVO)
            return i;

    return -1;
}

// Área em que o jogador pode apertar E (o monitor + uma folga para
// os lados e para baixo, já que ele fica "em frente" dele).
Rectangle ZonaInteracao(Rectangle a)
{
    return (Rectangle){ a.x - 16, a.y, a.width + 32, a.height + 90 };
}

void IniciarTransicao(Transicao *t, TelaAtual destino, float duracao)
{
    if (t->ativa)
        return;

    t->ativa = true;
    t->alfa = 0.0f;
    t->fase = 0;
    t->duracao = duracao;
    t->destino = destino;
}

// Soma (ou tira, se delta < 0) pontos sem deixar a pontuação ficar negativa.
// Devolve quanto realmente mudou.
int AlterarPontos(ProgressoCorredor *p, int delta)
{
    int antes = p->pontos;

    p->pontos += delta;

    if (p->pontos < 0)
        p->pontos = 0;

    return p->pontos - antes;
}

// ---------- Sistema de diálogo (estilo Undertale) ----------

// Tamanho do "Luci: " do começo (o nome, os dois-pontos e os espaços depois deles)
static int TamanhoPrefixoFala(const char *t)
{
    const char *p = strchr(t, ':');

    if (p == NULL)
        return 0;

    p++;
    while (*p == ' ')
        p++;

    return (int)(p - t);
}

// Fala da Luci que passa de MAX_LINHAS_FALA_LUCI linhas: onde cortar (em bytes) para o pedaço caber.
// Prefere o fim de uma frase; depois uma vírgula, ponto e vírgula ou dois-pontos; por último, qualquer espaço.
// Devolve -1 se a fala já cabe.
static int CorteFalaLuci(const char *texto, int larguraMax, int tamFonte)
{
    if (LinhasDoTexto(texto, larguraMax, tamFonte) <= MAX_LINHAS_FALA_LUCI)
        return -1;

    char pedaco[768];
    int melhor[3] = { -1, -1, -1 };         // 0 = fim de frase, 1 = vírgula / ; / :, 2 = qualquer espaço
    int inicio = TamanhoPrefixoFala(texto);
    int n = (int)strlen(texto);

    for (int i = inicio + 1; i < n && i < (int)sizeof(pedaco); i++) {

        if (texto[i] != ' ' || texto[i - 1] == ' ')
            continue;

        memcpy(pedaco, texto, (size_t)i);
        pedaco[i] = '\0';

        if (LinhasDoTexto(pedaco, larguraMax, tamFonte) > MAX_LINHAS_FALA_LUCI)
            break;                          // daqui para frente só fica maior

        char c = texto[i - 1];
        if ((c == '"' || c == ')') && i >= 2) c = texto[i - 2];      // ponto antes de aspas ou parêntese

        if (c == '.' || c == '!' || c == '?')      melhor[0] = i;
        else if (c == ',' || c == ';' || c == ':') melhor[1] = i;
        melhor[2] = i;
    }

    for (int k = 0; k < 3; k++)
        if (melhor[k] > 0)
            return melhor[k];

    return -1;                              // uma palavra enorme: deixa como está
}

// Mostra um texto na caixa. dividir = fala da Luci com mais de MAX_LINHAS_FALA_LUCI linhas vira
// várias "páginas" (issue #29): o que não coube fica em d->resto, com o "Luci:" na frente.
// nova = a fala aparece pela primeira vez (false = o jogador voltou para ela com BACKSPACE):
// só a nova entra na pilha do "voltar" e no histórico da Luci (issue #30).
static void DialogoMostrarTexto(SessaoDialogo *d, const char *texto, int larguraMax, int tamFonte, bool dividir, bool nova)
{
    char pagina[768];
    char resto[768] = "";

    // Cópia: o texto pode ser o próprio d->resto, uma página guardada ou o buffer do TextFormat
    snprintf(pagina, sizeof(pagina), "%s", texto);

    // Guarda a página (antes de dividir: mostrar de novo divide igual) para o BACKSPACE voltar até ela
    if (nova && dividir) {

        if (d->numAnteriores >= MAX_VOLTAR_DIALOGO) {           // cheio: esquece a mais antiga
            memmove(d->anteriores[0], d->anteriores[1], sizeof(d->anteriores[0]) * (MAX_VOLTAR_DIALOGO - 1));
            memmove(&d->paginaAnterior[0], &d->paginaAnterior[1], sizeof(d->paginaAnterior[0]) * (MAX_VOLTAR_DIALOGO - 1));
            d->numAnteriores = MAX_VOLTAR_DIALOGO - 1;
        }

        snprintf(d->anteriores[d->numAnteriores], sizeof(d->anteriores[0]), "%s", pagina);
        d->paginaAnterior[d->numAnteriores] = d->pagina;
        d->numAnteriores++;
    }

    if (dividir && TextoDeFalaDaLuci(pagina)) {

        int corte = CorteFalaLuci(pagina, larguraMax, tamFonte);

        if (corte > 0) {

            const char *depois = pagina + corte;
            while (*depois == ' ')
                depois++;

            snprintf(resto, sizeof(resto), "%.*s%s", TamanhoPrefixoFala(pagina), pagina, depois);
            pagina[corte] = '\0';
        }
    }

    // Fala nova da Luci: vai para o histórico (R ou menu de pause)
    if (nova && dividir && TextoDeFalaDaLuci(pagina))
        HistoricoLuciAdicionar(pagina);

    snprintf(d->resto, sizeof(d->resto), "%s", resto);
    QuebrarTexto(pagina, larguraMax, tamFonte, d->texto, (int)sizeof(d->texto));
    d->visiveis = 0.0f;
}

// BACKSPACE: volta para a fala anterior desta conversa (já aparece inteira, para reler).
// Devolve false se não há para onde voltar.
static bool DialogoVoltarFala(SessaoDialogo *d)
{
    if (d->numAnteriores < 2)
        return false;

    d->numAnteriores--;
    int i = d->numAnteriores - 1;

    d->pagina = d->paginaAnterior[i];
    DialogoMostrarTexto(d, d->anteriores[i], LARGURA_TEXTO_CAIXA, FONTE_CAIXA, true, false);
    d->visiveis = (float)strlen(d->texto);
    TocarSomTom(sfxSelecao, 0.8f);
    return true;
}

void DialogoIniciar(SessaoDialogo *d, const Dialogo *conteudo, int objeto)
{
    d->numAnteriores = 0;           // conversa nova: o BACKSPACE não volta para a anterior
    d->pedirHistorico = false;

    d->conteudo = *conteudo;
    d->objeto = objeto;
    d->pagina = 0;
    d->respondeu = false;
    d->acertou = false;
    d->estado = DLG_PAGINAS;
    d->corBorda = WHITE;
    d->pontosGanhos = 0;
    d->inicio = GetTime();
    d->pulos = 0;
    d->origem = conteudo;
    d->versaoCurta = NULL;
    d->veioDoCartao = false;

    DialogoMostrarTexto(d, d->conteudo.paginas[0], LARGURA_TEXTO_CAIXA, FONTE_CAIXA, true, true);
}

// Conversa de uma página só montada na hora (ex.: com TextFormat)
void DialogoFalaAvulsa(SessaoDialogo *d, const char *texto)
{
    Dialogo fala = {0};
    fala.paginas[0] = texto;
    fala.numPaginas = 1;

    DialogoIniciar(d, &fala, OBJ_NENHUM);
}

// Confirmação SIM / NÃO (terminais de atalho). Começa em NÃO: um ENTER distraído não pula conteúdo.
// O resultado volta como RES_SIM ou RES_NAO para quem abriu (o objeto diz qual atalho era).
void DialogoConfirmar(SessaoDialogo *d, const char *texto, int objeto)
{
    Dialogo fala = {0};
    fala.paginas[0] = texto;
    fala.numPaginas = 1;

    DialogoIniciar(d, &fala, objeto);
    DialogoMostrarTexto(d, texto, LARGURA_TEXTO_CAIXA, FONTE_CAIXA, false, true);     // a pergunta SIM / NÃO fica inteira
    d->estado = DLG_CONFIRMA;
    d->selecao = 1;
}

// Onde ficam as opções SIM / NÃO na tela (a mesma conta de DialogoDesenhar), para o mouse
static Rectangle DialogoRetOpcaoConfirma(const SessaoDialogo *d, int i)
{
    static const char *opcoes[2] = { "SIM", "NÃO" };
    int alturaCaixa = 16 + ContarLinhas(d->texto) * EspacoLinhaTexto(26, FONTE_CAIXA) + 34 + 30;
    if (alturaCaixa < 150) alturaCaixa = 150;
    float caixaY = (float)(ALTURA_TELA - 14 - alturaCaixa);
    float x = 50.0f + 160.0f + (float)i * 240.0f;
    float y = caixaY + (float)alturaCaixa - 40.0f;
    return (Rectangle){ x - 26.0f, y - 6.0f, (float)MeasureText(opcoes[i], 20) + 40.0f, 32.0f };
}

void DialogoMostrarFeedback(SessaoDialogo *d, const char *texto, Color borda, int pontos)
{
    d->estado = DLG_FEEDBACK;
    d->corBorda = borda;
    d->pontosGanhos = pontos;
    d->numAnteriores = 0;           // o feedback não volta para a pergunta (ela tem tempo)

    DialogoMostrarTexto(d, texto, LARGURA_TEXTO_CAIXA, FONTE_CAIXA, true, true);
}

static int AlturaPainelPergunta(const SessaoDialogo *d)
{
    int h = 62;                                             // cabeçalho + barra de tempo
    h += ContarLinhas(d->texto) * EspacoLinhaTexto(24, FONTE_PERGUNTA) + 14;                  // pergunta

    for (int i = 0; i < NUM_OPCOES; i++)
        h += ContarLinhas(d->opcoesTexto[i]) * EspacoLinhaTexto(d->fonteOpcao + 5, d->fonteOpcao) + 10;

    h += 30;                                                // rodapé
    return h;
}

// Caixa de diálogo de baixo (a mesma conta de DialogoDesenhar)
static Rectangle DialogoRetCaixa(const SessaoDialogo *d)
{
    int alturaCaixa = 16 + ContarLinhas(d->texto) * EspacoLinhaTexto(26, FONTE_CAIXA) + 34 + ((d->estado == DLG_CONFIRMA) ? 30 : 0);

    if (alturaCaixa < 150)
        alturaCaixa = 150;

    return (Rectangle){ 50, (float)(ALTURA_TELA - 14 - alturaCaixa), 700, (float)alturaCaixa };
}

// ---------- Atalhos desenhados em cima da caixa (issue #30) ----------
//   0: BACKSPACE volta para a fala anterior    1: R abre o histórico das falas da Luci
static const char *TECLAS_ATALHO_DLG[2]  = { "BACKSPACE", "R" };
static const char *ROTULOS_ATALHO_DLG[2] = { "fala anterior", "falas da Luci" };

static bool DialogoMostraAtalhos(const SessaoDialogo *d)
{
    return d->estado == DLG_PAGINAS || d->estado == DLG_FEEDBACK;
}

static float LarguraAtalhoDialogo(int i)
{
    return (float)(MeasureText(TECLAS_ATALHO_DLG[i], 10) + 12 + 6 + MeasureText(ROTULOS_ATALHO_DLG[i], 11) + 10);
}

static Rectangle DialogoRetCaixa(const SessaoDialogo *d);

static Rectangle DialogoRetAtalho(const SessaoDialogo *d, int i)
{
    Rectangle c = DialogoRetCaixa(d);
    float x1 = c.x + c.width - LarguraAtalhoDialogo(1);
    float x = (i == 1) ? x1 : x1 - 8.0f - LarguraAtalhoDialogo(0);
    return (Rectangle){ x, c.y - 24.0f, LarguraAtalhoDialogo(i), 20.0f };
}

static void DesenharAtalhoDialogo(const SessaoDialogo *d, int i, bool ativo)
{
    Rectangle r = DialogoRetAtalho(d, i);
    bool sobre = ativo && MouseEm(r);
    float alfa = ativo ? 1.0f : 0.4f;
    int wTecla = MeasureText(TECLAS_ATALHO_DLG[i], 10) + 12;
    Rectangle tecla = { r.x + 4, r.y + 3, (float)wTecla, r.height - 6 };

    DrawRectangleRec(r, Fade(BLACK, 0.8f * alfa));
    DrawRectangleLinesEx(r, 1, Fade(sobre ? WHITE : SKYBLUE, 0.6f * alfa));
    DrawRectangleRec(tecla, Fade((Color){ 40, 40, 60, 255 }, alfa));
    DrawRectangleLinesEx(tecla, 1, Fade(LIGHTGRAY, alfa));
    DrawText(TECLAS_ATALHO_DLG[i], (int)tecla.x + 6, (int)tecla.y + 2, 10, Fade(WHITE, alfa));
    DrawText(ROTULOS_ATALHO_DLG[i], (int)(tecla.x + tecla.width) + 6, (int)r.y + 4, 11, Fade(sobre ? WHITE : SKYBLUE, alfa));
}

// BACKSPACE (ou clique no atalho): volta uma fala. Clique no "R": pede o histórico ao main.
// Devolve true se a fala anterior apareceu (o resto do quadro não avança a conversa).
static bool DialogoTratarAtalhos(SessaoDialogo *d, bool *clique)
{
    bool cliqueVoltar = *clique && MouseEm(DialogoRetAtalho(d, 0));
    bool cliqueHistorico = *clique && MouseEm(DialogoRetAtalho(d, 1));

    if (cliqueVoltar || cliqueHistorico)
        *clique = false;

    if (cliqueHistorico)
        d->pedirHistorico = true;

    if (IsKeyPressed(KEY_BACKSPACE) || cliqueVoltar) {
        DialogoVoltarFala(d);       // na primeira fala não faz nada (e o quadro também não avança)
        return true;
    }

    return false;
}

// O atalho "TAB - ..." escrito na caixa (NULL = não aparece)
static const char *DialogoTextoTab(const SessaoDialogo *d)
{
    if (d->estado != DLG_PAGINAS)                   return NULL;
    if (d->conteudo.saberMais != NULL)              return "TAB - saber mais";
    if (d->versaoCurta != NULL || d->veioDoCartao)  return "TAB - voltar ao resumo";
    return NULL;
}

static Rectangle DialogoRetTab(const SessaoDialogo *d)
{
    Rectangle c = DialogoRetCaixa(d);
    const char *t = DialogoTextoTab(d);

    return RetTextoClicavel((t != NULL) ? t : "", (int)c.x + 20, (int)(c.y + c.height - 24), 12);
}

// Alternativa i da pergunta (a mesma conta de DialogoDesenhar), para o mouse
static Rectangle DialogoRetOpcaoPergunta(const SessaoDialogo *d, int i)
{
    int espaco = EspacoLinhaTexto(d->fonteOpcao + 5, d->fonteOpcao);
    int y = (ALTURA_TELA - AlturaPainelPergunta(d)) / 2 + 58 + ContarLinhas(d->texto) * EspacoLinhaTexto(24, FONTE_PERGUNTA) + 14;

    for (int k = 0; k < i; k++)
        y += ContarLinhas(d->opcoesTexto[k]) * espaco + 10;

    return (Rectangle){ 56, (float)y - 5, 640, (float)(ContarLinhas(d->opcoesTexto[i]) * espaco + 8) };
}

static void DialogoMostrarPergunta(SessaoDialogo *d)
{
    static const int fontes[] = {16, 15, 14, 13};

    d->estado = DLG_ESCOLHA;
    d->selecao = 0;
    d->tempoRestante = TEMPO_QUIZ;

    DialogoMostrarTexto(d, d->conteudo.pergunta, LARGURA_PERGUNTA, FONTE_PERGUNTA, false, true);

    // As alternativas do PDF são longas: se não couberem, a fonte diminui um pouco
    for (int f = 0; f < 4; f++) {

        d->fonteOpcao = fontes[f];

        for (int i = 0; i < NUM_OPCOES; i++) {
            QuebrarTexto(
                TextFormat("%c) %s", 'A' + i, d->conteudo.opcoes[i]),
                LARGURA_OPCAO,
                d->fonteOpcao,
                d->opcoesTexto[i],
                (int)sizeof(d->opcoesTexto[i])
            );
        }

        if (AlturaPainelPergunta(d) <= ALTURA_TELA - 30)
            break;
    }
}

// Atualiza a conversa e avisa o que aconteceu neste frame.
//   ENTER / E / ESPAÇO : completa o texto na hora ou avança a página
//   W / S / SETAS      : move o coração entre as alternativas
//   ENTER / E / ESPAÇO : confirma a alternativa (1, 2, 3, 4 = atalho)
ResultadoDialogo DialogoAtualizar(SessaoDialogo *d)
{
    bool avancar = IsKeyPressed(KEY_ENTER) ||
                   IsKeyPressed(KEY_E) ||
                   IsKeyPressed(KEY_SPACE);

    // Mouse: o clique vale como ENTER (o primeiro instante da conversa não conta,
    // para um tiro dado bem na hora não pular a fala)
    bool clique = CliqueMouse() && (GetTime() - d->inicio > 0.25);

    // Efeito máquina de escrever
    int total = (int)strlen(d->texto);

    if (d->visiveis < (float)total) {

        d->visiveis += VELOCIDADE_TEXTO * DtJogo();

        if (d->visiveis > (float)total)
            d->visiveis = (float)total;
    }

    bool terminou = (d->visiveis >= (float)total);

    switch (d->estado) {

        case DLG_PAGINAS: {

            if (DialogoTratarAtalhos(d, &clique))
                break;

            // TAB (ou clique no "TAB - ..."): abre a versão completa / volta para a versão curta
            bool cliqueTab = clique && DialogoTextoTab(d) != NULL && MouseEm(DialogoRetTab(d));

            if (cliqueTab)
                clique = false;

            if (IsKeyPressed(KEY_TAB) || cliqueTab) {

                if (d->conteudo.saberMais != NULL) {

                    const Dialogo *curta = d->origem;

                    Telemetria("saber_mais", d->conteudo.paginas[0], (float)(GetTime() - d->inicio), d->pulos);
                    DialogoIniciar(d, d->conteudo.saberMais, d->objeto);
                    d->versaoCurta = curta;
                    break;
                }

                if (d->versaoCurta != NULL) {
                    DialogoIniciar(d, d->versaoCurta, d->objeto);
                    break;
                }

                if (d->veioDoCartao) {
                    d->estado = DLG_NENHUM;
                    return RES_VOLTAR_CARTAO;
                }
            }

            if (clique)
                avancar = true;

            if (!avancar)
                break;

            if (!terminou) {
                d->visiveis = (float)total;
                d->pulos++;
                break;
            }

            // A fala da Luci foi dividida: mostra o pedaço que faltou antes de ir para a próxima página
            if (d->resto[0] != '\0') {
                DialogoMostrarTexto(d, d->resto, LARGURA_TEXTO_CAIXA, FONTE_CAIXA, true, true);
                break;
            }

            d->pagina++;

            if (d->pagina < d->conteudo.numPaginas) {

                DialogoMostrarTexto(d, d->conteudo.paginas[d->pagina], LARGURA_TEXTO_CAIXA, FONTE_CAIXA, true, true);

            } else if (d->conteudo.pergunta != NULL) {

                DialogoMostrarPergunta(d);

            } else {

                Telemetria("dialogo", d->conteudo.paginas[0], (float)(GetTime() - d->inicio), d->pulos);
                d->estado = DLG_NENHUM;
                return RES_FIM;
            }

        } break;

        case DLG_ESCOLHA: {

            // A barra de tempo só começa quando a pergunta terminou de aparecer
            if (!terminou) {

                if (avancar || clique)
                    d->visiveis = (float)total;

                break;
            }

            d->tempoRestante -= DtJogo();

            if (d->tempoRestante <= 0.0f) {

                d->tempoRestante = 0.0f;
                d->respondeu = true;
                d->acertou = false;
                d->estado = DLG_NENHUM;
                return RES_TEMPO;
            }

            if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))
                d->selecao = (d->selecao + NUM_OPCOES - 1) % NUM_OPCOES;

            if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN))
                d->selecao = (d->selecao + 1) % NUM_OPCOES;

            // Mouse: passar por cima marca a alternativa, clicar responde
            int cliqueOpcao = -1;

            for (int i = 0; i < NUM_OPCOES; i++) {

                if (!MouseEm(DialogoRetOpcaoPergunta(d, i)))
                    continue;

                if (MouseMexeu())
                    d->selecao = i;

                if (clique)
                    cliqueOpcao = i;
            }

            // Pequeno atraso para ninguém responder sem querer
            if (TEMPO_QUIZ - d->tempoRestante < ATRASO_RESPOSTA)
                break;

            int escolha = -1;

            if (avancar) escolha = d->selecao;
            if (IsKeyPressed(KEY_ONE)   || IsKeyPressed(KEY_KP_1)) escolha = 0;
            if (IsKeyPressed(KEY_TWO)   || IsKeyPressed(KEY_KP_2)) escolha = 1;
            if (IsKeyPressed(KEY_THREE) || IsKeyPressed(KEY_KP_3)) escolha = 2;
            if (IsKeyPressed(KEY_FOUR)  || IsKeyPressed(KEY_KP_4)) escolha = 3;
            if (cliqueOpcao >= 0)                                    escolha = cliqueOpcao;

            if (escolha >= 0) {

                d->selecao = escolha;
                d->respondeu = true;
                d->acertou = (escolha == d->conteudo.correta);

                if (d->acertou) {

                    DialogoMostrarFeedback(
                        d,
                        TextFormat("Luci: Isso mesmo! %s", d->conteudo.explicacao),
                        GREEN,
                        PONTOS_POR_ACERTO
                    );

                } else {

                    // Errou: o jogo abre o minigame de esquiva
                    d->estado = DLG_NENHUM;
                    return RES_ERROU;
                }
            }

        } break;

        case DLG_FEEDBACK: {

            if (DialogoTratarAtalhos(d, &clique))
                break;

            if (clique)
                avancar = true;

            if (!avancar)
                break;

            if (!terminou) {
                d->visiveis = (float)total;
                break;
            }

            if (d->resto[0] != '\0') {         // a explicação foi dividida: o próximo pedaço
                DialogoMostrarTexto(d, d->resto, LARGURA_TEXTO_CAIXA, FONTE_CAIXA, true, true);
                break;
            }

            d->estado = DLG_NENHUM;
            return RES_FIM;

        } break;

        case DLG_CONFIRMA: {

            // ESC (ou BACKSPACE) cancela: vale NÃO
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
                d->estado = DLG_NENHUM;
                return RES_NAO;
            }

            if (!terminou) {

                if (avancar || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                    d->visiveis = (float)total;

                break;
            }

            // Mouse: passar por cima marca a opção, clicar confirma
            {
                Vector2 mouse = GetMousePosition();
                for (int i = 0; i < 2; i++) {
                    if (!CheckCollisionPointRec(mouse, DialogoRetOpcaoConfirma(d, i))) continue;
                    if (Vector2Length(GetMouseDelta()) > 0.5f) d->selecao = i;
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { d->selecao = i; avancar = true; }
                }
            }

            if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D) ||
                IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT) ||
                IsKeyPressed(KEY_W) || IsKeyPressed(KEY_S) ||
                IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN))
                d->selecao = 1 - d->selecao;

            if (avancar) {

                d->estado = DLG_NENHUM;
                return (d->selecao == 0) ? RES_SIM : RES_NAO;
            }

        } break;

        case DLG_NENHUM:
        default:
            break;
    }

    return RES_NADA;
}

void DialogoDesenhar(const SessaoDialogo *d)
{
    if (d->estado == DLG_NENHUM)
        return;

    // Parte do texto que já "apareceu"
    char parcial[768];
    bool terminou = TextoParcial(d->texto, d->visiveis, parcial, (int)sizeof(parcial));
    bool piscar = (((int)(GetTime() * 2.0)) % 2 == 0);

    // -----------------------------------------------------
    // PERGUNTA: painel central com as 4 alternativas e a barra de tempo
    // -----------------------------------------------------

    if (d->estado == DLG_ESCOLHA) {

        DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, 0.7f));

        int alturaPainel = AlturaPainelPergunta(d);
        Rectangle painel = { 40, (float)((ALTURA_TELA - alturaPainel) / 2), 720, (float)alturaPainel };

        int px = (int)painel.x + 20;
        int py = (int)painel.y;

        DrawRectangleRec(painel, BLACK);
        DrawRectangleLinesEx(painel, 4, WHITE);

        DrawText(
            d->conteudo.titulo != NULL ? d->conteudo.titulo : "LUCI // PENSE ANTES DE ESCOLHER",
            px,
            py + 14,
            10,
            GOLD
        );

        // ---------- Barra de tempo ----------
        float fracao = d->tempoRestante / TEMPO_QUIZ;
        Color corTempo = (fracao > 0.5f) ? LIME : (fracao > 0.25f ? YELLOW : RED);

        if (fracao <= 0.25f && (((int)(GetTime() * 6.0)) % 2 == 0))
            corTempo = MAROON;

        DrawRectangle(px, py + 32, 620, 12, (Color){40, 40, 40, 255});
        DrawRectangle(px, py + 32, (int)(620 * fracao), 12, corTempo);
        DrawRectangleLines(px, py + 32, 620, 12, GRAY);

        DrawText(
            TextFormat("%2d s", (int)ceilf(d->tempoRestante)),
            px + 632,
            py + 31,
            14,
            corTempo
        );

        // ---------- Pergunta ----------
        DesenharTextoLinhas(parcial, px, py + 58, FONTE_PERGUNTA, 24, WHITE);

        // As alternativas só aparecem quando a pergunta terminou de ser "digitada"
        if (terminou) {

            int y = py + 58 + ContarLinhas(d->texto) * EspacoLinhaTexto(24, FONTE_PERGUNTA) + 14;
            int espaco = d->fonteOpcao + 5;

            for (int i = 0; i < NUM_OPCOES; i++) {

                bool marcada = (i == d->selecao);

                if (marcada)
                    DesenharCursorLuci((float)px + 8, (float)y + d->fonteOpcao / 2.0f);

                int nl = DesenharTextoLinhas(
                    d->opcoesTexto[i],
                    px + 24,
                    y,
                    d->fonteOpcao,
                    espaco,
                    marcada ? YELLOW : LIGHTGRAY
                );

                y += nl * EspacoLinhaTexto(espaco, d->fonteOpcao) + 10;
            }

            DrawText(
                "W/S, SETAS ou MOUSE: escolher   |   ENTER ou CLIQUE: confirmar   |   1-4: atalho",
                px,
                (int)(painel.y + painel.height - 22),
                10,
                GRAY
            );
        }

        return;
    }

    // -----------------------------------------------------
    // PÁGINAS E FEEDBACK: caixa de diálogo na parte de baixo
    // (a altura cresce se o texto for grande)
    // -----------------------------------------------------

    bool confirma = (d->estado == DLG_CONFIRMA);
    int alturaCaixa = 16 + ContarLinhas(d->texto) * EspacoLinhaTexto(26, FONTE_CAIXA) + 34 + (confirma ? 30 : 0);

    if (alturaCaixa < 150)
        alturaCaixa = 150;

    Rectangle caixa = { 50, (float)(ALTURA_TELA - 14 - alturaCaixa), 700, (float)alturaCaixa };

    Color corBorda = (d->estado == DLG_FEEDBACK) ? d->corBorda : WHITE;

    DrawRectangleRec(caixa, BLACK);
    DrawRectangleLinesEx(caixa, 4, corBorda);

    DesenharTextoLinhas(
        parcial,
        (int)caixa.x + 20,
        (int)caixa.y + 16,
        FONTE_CAIXA,
        26,
        WHITE
    );

    // Atalhos em cima da caixa: BACKSPACE (fala anterior, apagado na primeira fala) e R (histórico)
    if (DialogoMostraAtalhos(d)) {
        DesenharAtalhoDialogo(d, 0, d->numAnteriores >= 2);
        DesenharAtalhoDialogo(d, 1, true);
    }

    // TAB: abre a versão completa ou volta para a curta
    if (d->estado == DLG_PAGINAS && d->conteudo.saberMais != NULL)
        DesenharTextoClicavel("TAB - saber mais", (int)caixa.x + 20, (int)(caixa.y + caixa.height - 24), 12, Fade(SKYBLUE, 0.85f));
    else if (d->estado == DLG_PAGINAS && (d->versaoCurta != NULL || d->veioDoCartao))
        DesenharTextoClicavel("TAB - voltar ao resumo", (int)caixa.x + 20, (int)(caixa.y + caixa.height - 24), 12, Fade(SKYBLUE, 0.85f));

    // Opções SIM / NÃO com a Luci na escolhida
    if (confirma && terminou) {

        const char *opcoes[2] = { "SIM", "NÃO" };
        int yOpcoes = (int)(caixa.y + caixa.height) - 40;

        for (int i = 0; i < 2; i++) {

            int xOpcao = (int)caixa.x + 160 + i * 240;
            bool marcada = (d->selecao == i);

            if (marcada)
                DesenharCursorLuci((float)xOpcao - 18, (float)yOpcoes + 10);

            DrawText(opcoes[i], xOpcao, yOpcoes, 20, marcada ? YELLOW : LIGHTGRAY);
        }

        return;
    }

    if (terminou) {

        if (d->estado == DLG_FEEDBACK && d->pontosGanhos < 0) {

            DrawText(
                TextFormat("%d PONTOS", d->pontosGanhos),
                (int)caixa.x + 20,
                (int)(caixa.y + caixa.height - 26),
                14,
                RED
            );
        }

        if (d->estado == DLG_FEEDBACK && d->pontosGanhos > 0) {

            DrawText(
                TextFormat("+%d PONTOS", d->pontosGanhos),
                (int)caixa.x + 20,
                (int)(caixa.y + caixa.height - 26),
                14,
                GREEN
            );
        }

        if (piscar) {

            DesenharDicaAvancar((int)(caixa.x + caixa.width - 90), (int)(caixa.y + caixa.height - 26), 12);
        }
    }
}

// ---------- Desenho dos monitores (4 estados) ----------
// Quando a arte final ficar pronta, é só colocar os PNGs nos caminhos
// SPRITE_MONITOR_* lá em cima: o jogo passa a usá-los automaticamente.

void DesenharMonitor(int indice, Rectangle a, EstadoMonitor estado, float tempo, const Texture2D sprites[4])
{
    // Com sprite: desenha a imagem ocupando a tela + o pé do monitor
    if (sprites[estado].id > 0) {

        Texture2D t = sprites[estado];

        DrawTexturePro(
            t,
            (Rectangle){ 0, 0, (float)t.width, (float)t.height },
            (Rectangle){ a.x, a.y, a.width, a.height + 18 },
            (Vector2){ 0, 0 },
            0.0f,
            WHITE
        );

        return;
    }

    // Sem sprite: formas simples
    int cx = (int)(a.x + a.width / 2);
    float pulso = 0.5f + 0.5f * sinf(tempo * 5.0f);

    Color corBorda = GRAY;
    Color corLed = DARKGRAY;

    // Pé do monitor
    DrawRectangle(cx - 6, (int)(a.y + a.height), 12, 12, (Color){45, 40, 62, 255});
    DrawRectangle(cx - 24, (int)(a.y + a.height) + 12, 48, 6, (Color){45, 40, 62, 255});

    // Carcaça
    DrawRectangleRec(a, (Color){30, 28, 44, 255});

    Rectangle visor = { a.x + 7, a.y + 7, a.width - 14, a.height - 18 };
    int vx = (int)visor.x;
    int vy = (int)visor.y;
    int vw = (int)visor.width;
    int vh = (int)visor.height;
    int cy = vy + vh / 2;

    switch (estado) {

        // ---------- NEUTRO: desligado, esperando o jogador ----------
        case MON_NEUTRO: {

            DrawRectangleRec(visor, (Color){10, 12, 20, 255});

            const char *num = TextFormat("%d", indice + 1);
            DrawText(num, cx - MeasureText(num, 20) / 2, cy - LinhaTexto(10), 20, (Color){110, 118, 140, 255});     // desligado, mas legível

            if (((int)(tempo * 1.5f)) % 2 == 0)
                DrawText("_", vx + 6, vy + vh - 14, 10, (Color){80, 90, 110, 255});

            corBorda = GRAY;
            corLed = (Color){70, 70, 70, 255};

        } break;

        // ---------- ATIVO: durante a interação / quiz ----------
        case MON_ATIVO: {

            DrawRectangleRec(visor, (Color){12, 40, 75, 255});

            // Linhas de varredura
            for (int yy = 0; yy < vh; yy += 4)
                DrawLine(vx, vy + yy, vx + vw, vy + yy, Fade(BLACK, 0.25f));

            float varre = fmodf(tempo * 40.0f, (float)vh);
            DrawRectangle(vx, vy + (int)varre, vw, 3, Fade(SKYBLUE, 0.35f));

            DrawText("?", cx - MeasureText("?", 28) / 2, cy - 14, 28, Fade(WHITE, 0.6f + 0.4f * pulso));

            corBorda = Fade(GOLD, 0.6f + 0.4f * pulso);
            corLed = (pulso > 0.5f) ? YELLOW : ORANGE;

        } break;

        // ---------- ACERTOU ----------
        case MON_ACERTOU: {

            DrawRectangleRec(visor, (Color){8, 48, 22, 255});

            DrawLineEx((Vector2){ (float)cx - 16, (float)cy }, (Vector2){ (float)cx - 4, (float)cy + 12 }, 5, LIME);
            DrawLineEx((Vector2){ (float)cx - 4, (float)cy + 12 }, (Vector2){ (float)cx + 18, (float)cy - 14 }, 5, LIME);

            corBorda = GREEN;
            corLed = LIME;

        } break;

        // ---------- ERROU: chiado de alucinação ----------
        case MON_ERROU: {

            DrawRectangleRec(visor, (Color){55, 8, 14, 255});

            unsigned int quadro = (unsigned int)(tempo * 12.0f) + (unsigned int)indice * 97u;

            for (int k = 0; k < 6; k++) {

                unsigned int r = Ruido(quadro * 7u + (unsigned int)k);
                int lw = 10 + (int)((r >> 8) % (unsigned int)(vw - 10));
                int lx = vx + (int)((r >> 16) % (unsigned int)(vw - lw + 1));
                int ly = vy + (int)(r % (unsigned int)vh);

                DrawRectangle(lx, ly, lw, 2, Fade(RED, 0.5f));
            }

            DrawLineEx((Vector2){ (float)cx - 12, (float)cy - 12 }, (Vector2){ (float)cx + 12, (float)cy + 12 }, 4, RED);
            DrawLineEx((Vector2){ (float)cx + 12, (float)cy - 12 }, (Vector2){ (float)cx - 12, (float)cy + 12 }, 4, RED);

            corBorda = RED;
            corLed = RED;

        } break;

        default:
            break;
    }

    DrawRectangleLinesEx(a, 3, corBorda);

    // LED de energia
    DrawCircle((int)(a.x + a.width - 10), (int)(a.y + a.height - 6), 2, corLed);
}

static void DesenharPortaCorredor(Rectangle porta, Color corBorda)
{
    DrawRectangleRec(porta, (Color){35, 25, 55, 255});
    DrawRectangleLinesEx(porta, 3, corBorda);

    // Maçaneta perto da borda maior
    if (porta.width > porta.height)
        DrawCircle((int)(porta.x + porta.width - 12), (int)(porta.y + porta.height / 2), 3, GOLD);
    else
        DrawCircle((int)(porta.x + 9), (int)(porta.y + porta.height / 2), 4, GOLD);
}

// Fundo do corredor: a arte azul (fica atrás de monitores, terminal, jogador, HUD e diálogos).
// Sem o arquivo, o cenário simples, com a mesma geometria (parede, chão e aberturas).
void DesenharCenarioCorredor(float tempo)
{
    if (texMapaCorredor.id > 0) {
        DrawTexture(texMapaCorredor, (int)MAPA_CORREDOR_X, (int)MAPA_CORREDOR_Y, WHITE);
        return;
    }

    Rectangle mapa = { MAPA_CORREDOR_X, MAPA_CORREDOR_Y, MAPA_CORREDOR_LARGURA, MAPA_CORREDOR_ALTURA };
    Rectangle chao = RETANGULO_CORREDOR(42, 250, 1266, 515);

    DrawRectangleRec(mapa, (Color){26, 20, 46, 255});
    DrawRectangleRec(chao, (Color){18, 15, 30, 255});
    DrawRectangle((int)chao.x, (int)chao.y - 4, (int)chao.width, 6, Fade(PURPLE, 0.45f));

    for (float x = chao.x; x < chao.x + chao.width; x += 70.0f)
        DrawLine((int)x, (int)chao.y, (int)x, (int)(chao.y + chao.height), Fade(PURPLE, 0.12f));

    // Luminárias pulsando no alto da parede
    for (int i = 0; i < 5; i++) {
        float pulso = 0.5f + 0.5f * sinf(tempo * 1.5f + i * 1.3f);
        int lx = 120 + i * 140;
        DrawCircle(lx, (int)MAPA_CORREDOR_Y + 8, 20, Fade(SKYBLUE, 0.04f + 0.08f * pulso));
        DrawRectangle(lx - 12, (int)MAPA_CORREDOR_Y + 2, 24, 5, Fade(SKYBLUE, 0.5f + 0.4f * pulso));
    }

    DrawRectangleLinesEx(mapa, 4, Fade(PURPLE, 0.55f));
}

// Porta do corredor sobre a abertura da arte: moldura na cor do estado e, trancada,
// uma barreira de energia correndo. Sem a arte, a porta simples de sempre.
void DesenharPortaMapaCorredor(Rectangle porta, Color cor, bool barreira, float t)
{
    if (texMapaCorredor.id == 0) {
        DesenharPortaCorredor(porta, cor);
        return;
    }

    if (barreira) {
        float pulso = 0.5f + 0.5f * sinf(t * 5.0f);
        bool vertical = porta.height > porta.width;

        DrawRectangleRec(porta, Fade(cor, 0.16f + 0.10f * pulso));

        for (int k = 0; k < 5; k++) {
            float f = fmodf(t * 0.6f + (float)k / 5.0f, 1.0f);
            if (vertical) {
                float y = porta.y + f * porta.height;
                DrawLineEx((Vector2){ porta.x, y }, (Vector2){ porta.x + porta.width, y }, 2.0f, Fade(cor, 0.55f));
            } else {
                float x = porta.x + f * porta.width;
                DrawLineEx((Vector2){ x, porta.y }, (Vector2){ x, porta.y + porta.height }, 2.0f, Fade(cor, 0.55f));
            }
        }
    }

    DrawRectangleLinesEx(porta, 2, Fade(cor, 0.85f));
}
