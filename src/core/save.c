#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <time.h>
#include "../../include/core/fonte.h"
#include "../../include/core/mapas.h"
#include "../../include/core/save.h"

// =============================================================
// SAVE
// =============================================================

// Grava primeiro num arquivo temporário e só depois troca pelo save de verdade:
// se o jogo fechar no meio da gravação, o save anterior continua inteiro.
static bool GravarSave(const DadosSave *d)
{
    FILE *f = fopen(ARQUIVO_SAVE_TEMP, "wb");

    if (f == NULL)
        return false;

    bool ok = (fwrite(d, sizeof(*d), 1, f) == 1);

    if (fclose(f) != 0)
        ok = false;

    if (!ok) {
        remove(ARQUIVO_SAVE_TEMP);
        return false;
    }

    remove(ARQUIVO_SAVE);                   // no Windows o rename falha se o destino existir

    return (rename(ARQUIVO_SAVE_TEMP, ARQUIVO_SAVE) == 0);
}

// Um byte estragado no arquivo pode virar um bool que não é nem 0 nem 1, e só ler
// esse valor já é comportamento indefinido em C. Lê o byte cru e regrava como true/false.
static void ConsertarBool(bool *b)
{
    unsigned char byte;

    memcpy(&byte, b, 1);
    *b = (byte != 0);
}

// Todos os bool que vêm do arquivo (o ProgressoBloco2 é zerado logo abaixo)
static void ConsertarBoolsSave(DadosSave *d)
{
    ConsertarBool(&d->luciAcompanhando);
    ConsertarBool(&d->progresso.introVista);
    ConsertarBool(&d->progresso.fimAnunciado);
    ConsertarBool(&d->chefeVencido);
    ConsertarBool(&d->biblioteca.byteVisto);
    ConsertarBool(&d->biblioteca.bytecoVisto);
    ConsertarBool(&d->biblioteca.alucinadorVencido);
    ConsertarBool(&d->biblioteca.silogistaVencido);
    ConsertarBool(&d->biblioteca.bloco3Liberado);
    ConsertarBool(&d->biblioteca.jogoConcluido);
}

// Teto dos contadores lidos do arquivo: bem acima do que uma partida chega, e baixo o bastante
// para as contas do índice de desempenho (nucleo.c) não estourarem o int
#define TETO_CONTADOR_SAVE 1000000

static void LimitarContador(int *v)
{
    if (*v < 0)                       *v = 0;
    else if (*v > TETO_CONTADOR_SAVE) *v = TETO_CONTADOR_SAVE;
}

// Contadores negativos ou absurdos (save estragado) viram números válidos.
// Negativos também dividiriam por zero no índice (ex.: alucinacoesPegas + confiouAlucinacao + 1 == 0).
static void LimitarContadoresSave(DadosSave *d)
{
    ProgressoBiblioteca *b = &d->biblioteca;
    int *contadores[] = {
        &d->progresso.pontos, &d->progresso.alucinacao, &d->ajudasPedidas,
        &b->fragmentos, &b->tentativas, &b->desafiosCertos, &b->desafiosErrados, &b->mortes,
        &b->confiouCego, &b->confiouAlucinacao, &b->verificou, &b->alucinacoesPegas,
        &b->contradicoesFlagradas, &b->contradicoesPerdidas, &b->contestouAToa, &b->reparos,
        &b->derrotasLuci, &b->abatidosRun, &b->salasRun, &b->esquivasRun, &b->golpesRun,
        &b->parriesRun, &b->especiaisRun
    };

    for (size_t i = 0; i < sizeof(contadores) / sizeof(contadores[0]); i++)
        LimitarContador(contadores[i]);

    for (int i = 0; i < 3; i++)
        if (b->melhorias[i] < 0) b->melhorias[i] = 0;

    if (b->indiceFinal < 0 || b->indiceFinal > 100) b->indiceFinal = 0;
    if (b->finalJogo < 0 || b->finalJogo > 2)       b->finalJogo = 0;
    b->corrupcaoLuciMax = (int)Clamp((float)b->corrupcaoLuciMax, 0.0f, 100.0f);
    if (!(b->segundosRun >= 0.0f && b->segundosRun < 360000.0f)) b->segundosRun = 0.0f;
}

// Lê e confere o save. Devolve false se não existir, for de outra versão
// ou estiver corrompido (nesse caso o jogo simplesmente começa do zero).
bool LerSave(DadosSave *d)
{
    FILE *f = fopen(ARQUIVO_SAVE, "rb");

    if (f == NULL)
        return false;

    memset(d, 0, sizeof(*d));

    size_t lidos = fread(d, 1, sizeof(*d), f);
    fclose(f);

    if (d->assinatura != ASSINATURA_SAVE)
        return false;

    ConsertarBoolsSave(d);                  // antes das migrações, que já leem alguns deles

    // Save da versão 3 (sem a estabilidade da Luci no final): abre com os valores iniciais
    if (d->versao == 3 && lidos == offsetof(DadosSave, estabilidadeLuci)) {
        d->versao = 4;
        d->estabilidadeLuci = ESTABILIDADE_MAX_LUCI;
        d->ajudasPedidas = 0;
        lidos = offsetof(DadosSave, chefeVencido);
    }

    // Save da versão 4 (antes do mini-chefe): quem já tinha saído do corredor não precisa enfrentá-lo
    if (d->versao == 4 && lidos == offsetof(DadosSave, chefeVencido)) {
        d->versao = 5;
        d->chefeVencido = d->progresso.fimAnunciado;
        d->idPartida = 0;
        lidos = offsetof(DadosSave, biblioteca);
    }

    // Save da versão 5 (antes da Biblioteca dos Ecos): começa a Biblioteca do zero
    if (d->versao == 5 && lidos == offsetof(DadosSave, biblioteca)) {
        d->versao = 6;
        memset(&d->biblioteca, 0, sizeof(d->biblioteca));
        lidos = offsetof(DadosSave, biblioteca) + offsetof(ProgressoBiblioteca, desafiosCertos);
    }

    // Save da versão 6 (antes do desempenho): os números começam do zero (o memset já zerou);
    // quem já tinha vencido o Alucinador encontra a porta do Bloco 3 aberta
    if (d->versao == 6 && lidos == offsetof(DadosSave, biblioteca) + offsetof(ProgressoBiblioteca, desafiosCertos)) {
        d->versao = 7;
        d->biblioteca.bloco3Liberado = d->biblioteca.alucinadorVencido;
        d->biblioteca.silogistaVencido = (d->biblioteca.conceitos & (1u << 3)) != 0;    // CONCEITO_LOGICA
        lidos = offsetof(DadosSave, biblioteca) + offsetof(ProgressoBiblioteca, confiouCego);
    }

    // Save da versão 7 (antes das mecânicas da Luci): os contadores começam do zero
    if (d->versao == 7 && lidos == offsetof(DadosSave, biblioteca) + offsetof(ProgressoBiblioteca, confiouCego)) {
        d->versao = 8;
        lidos = offsetof(DadosSave, biblioteca) + offsetof(ProgressoBiblioteca, jogoConcluido);
    }

    // Save da versão 8 (antes do Bloco 3): o jogo ainda não foi concluído
    if (d->versao == 8 && lidos == offsetof(DadosSave, biblioteca) + offsetof(ProgressoBiblioteca, jogoConcluido)) {
        d->versao = 9;
        lidos = offsetof(DadosSave, biblioteca) + offsetof(ProgressoBiblioteca, abatidosRun);
    }

    // Save da versão 9 (antes dos números da run): começam do zero (o memset já zerou)
    if (d->versao == 9 && lidos == offsetof(DadosSave, biblioteca) + offsetof(ProgressoBiblioteca, abatidosRun)) {
        d->versao = VERSAO_SAVE;
        lidos = sizeof(*d);
    }

    if (lidos != sizeof(*d) || d->versao != VERSAO_SAVE)
        return false;

    if (d->estabilidadeLuci < 0 || d->estabilidadeLuci > ESTABILIDADE_MAX_LUCI)
        d->estabilidadeLuci = ESTABILIDADE_MAX_LUCI;

    // Quem estava no Bloco 2 antigo (Arquivo de Treinamento) continua no átrio da Biblioteca
    if (d->sala == SETOR2 || d->sala == SALA_REGISTROS || d->sala == SALA_LOGICA || d->sala == SALA_LACUNA) {
        d->sala = BIBLIOTECA;
        d->chefeVencido = true;
    }

    if (d->sala != JOGANDO && d->sala != CORREDOR && d->sala != BIBLIOTECA)
        return false;

    memset(&d->bloco2, 0, sizeof(d->bloco2));

    LimitarContadoresSave(d);

    if (d->biblioteca.mortes >= LIMITE_MORTES)
        return false;                       // partida encerrada sem vidas: não dá para continuar

    if (d->direcao < 0 || d->direcao >= NUM_DIRECOES)
        d->direcao = DIR_FRENTE;

    for (int i = 0; i < 3; i++)
        if (d->nickname[i] < 'A' || d->nickname[i] > 'Z')
            return false;

    d->nickname[3] = '\0';

    for (int i = 0; i < TOTAL_MONITORES; i++)
        if (d->progresso.estado[i] < MON_NEUTRO || d->progresso.estado[i] > MON_ERROU)
            d->progresso.estado[i] = MON_NEUTRO;

    // Posição dentro dos limites da sala salva (na Biblioteca o jogo começa no átrio).
    // NaN passaria direto pelo Clamp, então volta para o meio da sala antes.
    if (!isfinite(d->x)) d->x = 400.0f;
    if (!isfinite(d->y)) d->y = 340.0f;

    if (d->sala == JOGANDO) {
        d->x = Clamp(d->x, 80, 720);
        d->y = Clamp(d->y, 80, 520);
    } else {
        d->x = Clamp(d->x, 80, 720);
        d->y = Clamp(d->y, 285, 395);
    }

    return true;
}

// Apaga o save (a partida acabou sem vidas: não dá para CONTINUAR)
void ApagarSave(void)
{
    remove(ARQUIVO_SAVE_TEMP);
    remove(ARQUIVO_SAVE);
}

// Monta o save a partir do estado atual do jogo e grava
bool SalvarJogo(const char *nickname, TelaAtual tela, Vector2 pos, Direcao direcao,
                       bool luciAcompanhando, const ProgressoCorredor *progresso,
                       int estabilidadeLuci, int ajudasPedidas, bool chefeVencido, int idPartida,
                       const ProgressoBiblioteca *biblioteca)
{
    // Partida encerrada (usou todas as vidas): nada é gravado, nem ao fechar a janela
    if (biblioteca->mortes >= LIMITE_MORTES) {
        ApagarSave();
        return true;
    }

    DadosSave d;
    memset(&d, 0, sizeof(d));

    d.assinatura = ASSINATURA_SAVE;
    d.versao = VERSAO_SAVE;
    memcpy(d.nickname, nickname, 4);

    // A batalha acontece "dentro" do corredor. Na Biblioteca, quem volta começa no átrio.
    if (tela == JOGANDO)          d.sala = JOGANDO;
    else if (tela == BIBLIOTECA)  d.sala = BIBLIOTECA;
    else                          d.sala = CORREDOR;

    d.x = pos.x;
    d.y = pos.y;
    d.direcao = (int)direcao;
    d.luciAcompanhando = luciAcompanhando;
    d.progresso = *progresso;
    d.biblioteca = *biblioteca;
    d.estabilidadeLuci = estabilidadeLuci;
    d.ajudasPedidas = ajudasPedidas;
    d.chefeVencido = chefeVencido;
    d.idPartida = idPartida;

    // Quiz ou batalha pela metade: o monitor volta para NEUTRO
    // e o jogador refaz aquele quiz quando carregar o jogo
    for (int i = 0; i < TOTAL_MONITORES; i++)
        if (d.progresso.estado[i] == MON_ATIVO)
            d.progresso.estado[i] = MON_NEUTRO;

    bool ok = GravarSave(&d);

    TraceLog(ok ? LOG_INFO : LOG_WARNING, "SAVE: %s", ok ? "jogo salvo em " ARQUIVO_SAVE : "falha ao salvar");
    return ok;
}

// Aviso no canto da tela depois de um auto save
void DesenharAvisoSave(float aviso, bool falhou, float tempo)
{
    if (aviso <= 0.0f)
        return;

    float alfa = (aviso < 0.5f) ? aviso / 0.5f : 1.0f;
    bool salvando = (aviso > TEMPO_AVISO_SAVE - 0.6f);

    const char *texto = falhou ? "ERRO AO SALVAR" : (salvando ? "SALVANDO..." : "JOGO SALVO");
    Color cor = falhou ? RED : (salvando ? SKYBLUE : PURPLE);

    int largura = MeasureText(texto, 12);
    int x = 750 - largura;
    int y = 566;

    // "01" girando ao lado, como dados passando
    const char *dados = (((int)(tempo * 10.0f)) % 2) ? "01" : "10";

    DrawText(dados, x - 22, y, 12, Fade(SKYBLUE, alfa));
    DrawText(texto, x, y, 12, Fade(cor, alfa));
}

void DesenharTextoBotao(Rectangle b, const char *texto, int tamanho, Color cor)
{
    DrawText(texto, (int)(b.x + b.width / 2) - MeasureText(texto, tamanho) / 2, (int)(b.y + b.height / 2) - tamanho / 2, tamanho, cor);
}
