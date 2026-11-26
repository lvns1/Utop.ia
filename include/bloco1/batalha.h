// Bloco 1: batalha do quiz (minigame de esquiva e combate quando o tempo acaba).
#ifndef UTOPIA_BATALHA_H
#define UTOPIA_BATALHA_H

#include <stdbool.h>
#include "raylib.h"
#include "raymath.h"
#include "../core/config.h"
#include "../core/tipos.h"


// ----- Variáveis compartilhadas -----

// ----- Funções -----
void BatalhaIniciar(Batalha *b, TipoBatalha tipo, int quiz, int nivelAlucinacao, int hpInicial);
bool BatalhaAtualizar(Batalha *b);
float LarguraTeclaTutorial(const char *rotulo, float escala);
void DesenharTeclaTutorial(Vector2 c, const char *rotulo, float escala, bool apertada, float alfa);
void DesenharMouseTutorial(Vector2 c, int botao, float escala, bool apertado, float alfa);
void DesenharSetaTutorial(Vector2 ponta, float tam, Color cor, float tempo);
void DesenharDicaAgora(Vector2 c, const char *tecla, bool mouse, bool agora, const char *texto, float tempo);
void DesenharPreparoMinigame(Rectangle caixa, float resta, float total, const char *titulo, Color corTitulo, int linhas, const int *icones, const char *const *textos, const Color *cores, bool ataque, Color corInimigo, float tempo);
void DesenharVidaB1(const char *nickname, int hp, int hpMax, int y, float flashDano, float tempo);
void VinhetaDanoB1(float flashDano);
void DesenharAlucinacao(Vector2 centro, float tempo, bool forte);
void BatalhaDesenhar(const Batalha *b, const char *nickname, float tempo);

#endif // UTOPIA_BATALHA_H
