#ifndef DIALOGUE_H
#define DIALOGUE_H

#include <stdbool.h>
#include "raylib.h"
#include "../core/config.h"

typedef struct {
    const char *titulo;
    const char *paginas[MAX_PAGINAS];
    int numPaginas;
    const char *pergunta;
    const char *opcoes[NUM_OPCOES];
    int correta;
    const char *explicacao;
} Dialogo;

typedef enum { DLG_NENHUM, DLG_PAGINAS, DLG_ESCOLHA, DLG_FEEDBACK } EstadoDialogo;
typedef enum { RES_NADA, RES_FIM, RES_ERROU, RES_TEMPO } ResultadoDialogo;

typedef struct {
    EstadoDialogo estado;
    Dialogo conteudo;
    int objeto;
    int pagina;
    bool respondeu;
    bool acertou;
    char texto[768];
    char opcoesTexto[NUM_OPCOES][512];
    int fonteOpcao;
    int selecao;
    float tempoRestante;
    float visiveis;
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
