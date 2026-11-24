// Texto e interface: quebra de linha, máquina de escrever, mouse, botões e desenhos pequenos.
#ifndef UTOPIA_TEXTO_H
#define UTOPIA_TEXTO_H

#include "raylib.h"
#include "raymath.h"
#include <stdbool.h>
#include "../core/tipos.h"

void QuebrarTexto(const char *texto, int larguraMax, int tamFonte, char *saida, int tamSaida);
int DesenharTextoLinhas(const char *texto, int x, int y, int tamFonte, int espacoLinha, Color cor);
int ContarLinhas(const char *texto);
int LinhasDoTexto(const char *texto, int larguraMax, int tamFonte);
bool MouseMexeu(void);
bool CliqueMouse(void);
bool MouseEm(Rectangle r);
void DesenharDicaAvancar(int x, int y, int tam);
void DesenharBotaoFechar(Rectangle r, Color cor);
void DesenharBotaoMenu(Rectangle r, const char *texto, int tam, Color cor, bool marcado);
Rectangle RetTextoClicavel(const char *texto, int x, int y, int tam);
void DesenharTextoClicavel(const char *texto, int x, int y, int tam, Color cor);
bool TextoParcial(const char *texto, float visiveis, char *saida, int tamSaida);
unsigned int Ruido(unsigned int n);
void DesenharCoracao(float cx, float cy, int pixel, Color cor);

#endif // UTOPIA_TEXTO_H
