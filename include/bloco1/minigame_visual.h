// Bloco 1: visual próprio do minigame (a Luci no lugar do coração, verdades, alucinações e a caixa).
#ifndef UTOPIA_MINIGAME_VISUAL_H
#define UTOPIA_MINIGAME_VISUAL_H

#include <stdbool.h>
#include "raylib.h"
#include "raymath.h"
#include "../core/config.h"
#include "../core/tipos.h"

// =============================================================
// BLOCO 1 — VISUAL PRÓPRIO DO MINIGAME (sem o coração e as balas brancas de outros jogos)
//   • A LUCI é quem o jogador guia na caixa de esquiva: o núcleo brilhante dela é a área de acerto.
//   • PATCH DE CORREÇÃO (verde): na DEFESA, pegar recupera vida.
//   • FATO VERIFICADO (dourado): no ATAQUE, pegar vira dano na alucinação.
//   • DADO CORROMPIDO (cor da alucinação): o que machuca — um bloquinho de dados com falha de cor.
//   • Cursor das escolhas: a Luci pequena, balançando.
// =============================================================
#define ALTURA_LUCI_ALMA     24.0f
#define ALTURA_LUCI_CURSOR   18.0f

// ----- Variáveis compartilhadas -----
extern const Color COR_PATCH_CURA;
extern const Color COR_FATO_ATAQUE;

// ----- Funções -----
void DesenharAlmaLuci(Vector2 p, const Vector2 *rastro, int numRastro);
void DesenharCursorLuci(float cx, float cy);
void DesenharPatchCuraEscala(Vector2 p, float e, float alfa, float t);
void DesenharNucleoIntegridade(Vector2 c, float e, float t);
void DesenharItemB1(Vector2 p, bool ataque, float alfa, bool falsaAparecendo);
void DesenharDadoCorrompido(Vector2 p, char glifo, Color cor, int indice);
bool LuciAtrasDoJogador(float angulo);
void DesenharEmocaoLuci(Vector2 p, float altura);

#endif // UTOPIA_MINIGAME_VISUAL_H
