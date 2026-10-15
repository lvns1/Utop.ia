// text.h - Texto: quebra de linha automática, desenho multilinha e máquina de escrever.

#ifndef TEXT_H
#define TEXT_H

#include <stdbool.h>
#include "raylib.h"

void QuebrarTexto(const char *texto, int larguraMax, int tamFonte, char *saida, int tamSaida);
int DesenharTextoLinhas(const char *texto, int x, int y, int tamFonte, int espacoLinha, Color cor);
int ContarLinhas(const char *texto);
int FimDoCaractere(const char *s, int n);
bool TextoParcial(const char *texto, float visiveis, char *saida, int tamSaida);

#endif
