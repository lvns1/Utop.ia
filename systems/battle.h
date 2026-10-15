// battle.h - Batalha estilo Undertale: minigame de esquiva (erro) e combate (tempo esgotado).

#ifndef BATTLE_H
#define BATTLE_H

#include <stdbool.h>
#include "raylib.h"
#include "../core/config.h"
#include "../entities/projectile.h"

// MINIGAME = errou a resposta | COMBATE = o tempo do quiz acabou
typedef enum { BAT_MINIGAME, BAT_COMBATE } TipoBatalha;
// Fases em ordem: texto inicial -> caixa encolhe -> desvio -> caixa expande -> texto final
typedef enum { FASE_INTRO, FASE_ENCOLHENDO, FASE_DESVIO, FASE_EXPANDINDO, FASE_FIM } FaseBatalha;

// Estado de uma batalha (do texto inicial até o resultado).
typedef struct {
    TipoBatalha tipo;
    FaseBatalha fase;
    int quiz;
    int hp;
    int dano;
    Vector2 alma;                           // o coração do jogador
    float invencivel;
    float tremor;
    float anim;                             // 0 = caixa de texto, 1 = caixa de desvio
    float tempoDesvio;
    float duracao;
    float cronoBala;
    float intervaloBala;
    float velocidadeBala;
    int padrao;                             // 0 = chuva, 1 = lateral, 2 = mira
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
