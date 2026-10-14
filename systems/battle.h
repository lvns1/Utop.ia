#ifndef BATTLE_H
#define BATTLE_H

#include <stdbool.h>
#include "raylib.h"
#include "../core/config.h"
#include "../entities/projectile.h"

typedef enum { BAT_MINIGAME, BAT_COMBATE } TipoBatalha;
typedef enum { FASE_INTRO, FASE_ENCOLHENDO, FASE_DESVIO, FASE_EXPANDINDO, FASE_FIM } FaseBatalha;

typedef struct {
    TipoBatalha tipo;
    FaseBatalha fase;
    int quiz;
    int hp;
    int dano;
    Vector2 alma;
    float invencivel;
    float tremor;
    float anim;
    float tempoDesvio;
    float duracao;
    float cronoBala;
    float intervaloBala;
    float velocidadeBala;
    int padrao;
    int danosRecebidos;
    bool derrota;
    Bala balas[MAX_BALAS];
    char texto[512];
    float visiveis;
} Batalha;

void BatalhaIniciar(Batalha *b, TipoBatalha tipo, int quiz, int nivelAlucinacao);
void BatalhaCriarBala(Batalha *b, Rectangle c);
bool BatalhaAtualizar(Batalha *b);
void BatalhaDesenhar(const Batalha *b, const char *nickname, float tempo);

#endif
