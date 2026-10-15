// dialogue.h - Sistema de diálogo estilo Undertale: páginas de texto (máquina de escrever),
// pergunta de múltipla escolha com barra de tempo e feedback da resposta.

#ifndef DIALOGUE_H
#define DIALOGUE_H

#include <stdbool.h>
#include "raylib.h"
#include "../core/config.h"

// Uma conversa: algumas páginas de texto e, opcionalmente,
// uma pergunta de múltipla escolha (pergunta == NULL = sem pergunta).
typedef struct {
    const char *titulo;                     // cabeçalho do painel da pergunta
    const char *paginas[MAX_PAGINAS];
    int numPaginas;
    const char *pergunta;
    const char *opcoes[NUM_OPCOES];
    int correta;                            // índice da opção certa (0 = A ... 3 = D)
    const char *explicacao;                 // Luci explica a resposta certa
} Dialogo;

// O que está na tela: nada, páginas de texto, pergunta (escolha) ou feedback da resposta.
typedef enum { DLG_NENHUM, DLG_PAGINAS, DLG_ESCOLHA, DLG_FEEDBACK } EstadoDialogo;
// O que aconteceu na conversa neste frame
typedef enum { RES_NADA, RES_FIM, RES_ERROU, RES_TEMPO } ResultadoDialogo;

// Estado da conversa que está acontecendo na tela agora
typedef struct {
    EstadoDialogo estado;
    Dialogo conteudo;
    int objeto;                             // monitor (0-3), OBJ_LUCI ou OBJ_NENHUM
    int pagina;
    bool respondeu;
    bool acertou;
    char texto[768];                        // texto atual, já com quebras de linha
    char opcoesTexto[NUM_OPCOES][512];      // opções já com quebras de linha
    int fonteOpcao;                         // diminui sozinha se as opções não couberem
    int selecao;                            // opção marcada com o coração
    float tempoRestante;                    // barra de tempo do quiz
    float visiveis;                         // quantos bytes do texto já apareceram
    Color corBorda;
    int pontosGanhos;
} SessaoDialogo;

void DialogoMostrarTexto(SessaoDialogo *d, const char *texto, int larguraMax, int tamFonte);
void DialogoIniciar(SessaoDialogo *d, const Dialogo *conteudo, int objeto);
void DialogoFalaAvulsa(SessaoDialogo *d, const char *texto);
void DialogoMostrarFeedback(SessaoDialogo *d, const char *texto, Color borda, int pontos);
ResultadoDialogo DialogoAtualizar(SessaoDialogo *d);
void DialogoDesenhar(const SessaoDialogo *d);

#endif
