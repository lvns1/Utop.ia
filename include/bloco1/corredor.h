// Bloco 1: corredor dos monitores, diálogos, quiz, cartões e HUD.
#ifndef UTOPIA_CORREDOR_H
#define UTOPIA_CORREDOR_H

#include <stdbool.h>
#include "raylib.h"
#include "raymath.h"
#include "../core/config.h"
#include "../core/tipos.h"


// ----- Variáveis compartilhadas -----
extern Texture2D texMapaCorredor;

// ----- Funções -----
int ContarConcluidos(const ProgressoCorredor *p);
int ProximoMonitor(const ProgressoCorredor *p);
Rectangle ZonaInteracao(Rectangle a);
void IniciarTransicao(Transicao *t, TelaAtual destino, float duracao);
int AlterarPontos(ProgressoCorredor *p, int delta);
void DialogoIniciar(SessaoDialogo *d, const Dialogo *conteudo, int objeto);
void DialogoFalaAvulsa(SessaoDialogo *d, const char *texto);
void DialogoConfirmar(SessaoDialogo *d, const char *texto, int objeto);
void DialogoMostrarFeedback(SessaoDialogo *d, const char *texto, Color borda, int pontos);
ResultadoDialogo DialogoAtualizar(SessaoDialogo *d);
void DialogoDesenhar(const SessaoDialogo *d);
void DesenharMonitor(int indice, Rectangle a, EstadoMonitor estado, float tempo, const Texture2D sprites[4]);
void DesenharCenarioCorredor(float tempo);
void DesenharPortaMapaCorredor(Rectangle porta, Color cor, bool barreira, float t);

#endif // UTOPIA_CORREDOR_H
