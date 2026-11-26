// Objetivo atual, marcador, voz da Luci no HUD e o ambiente glitchado da sala e do corredor.
#ifndef UTOPIA_OBJETIVO_H
#define UTOPIA_OBJETIVO_H

#include <stdbool.h>
#include "raylib.h"
#include "raymath.h"
#include "../core/config.h"
#include "../core/tipos.h"

// =============================================================
// OBJETIVO ATUAL, MARCADOR E AMBIENTE
//   Para o jogador nunca ficar perdido: uma linha de objetivo fixa no topo,
//   uma seta sobre o próximo alvo e a Luci voando até ele quando o jogador
//   fica parado. O ambiente fica mais "glitchado" conforme a alucinação sobe.
// =============================================================

typedef struct {
    char texto[96];
    bool temAlvo;
    Vector2 alvo;
} Objetivo;

// Faixas de glitch que aumentam com o nível de alucinação (não mexem a tela)
#define MOSTRAR_GLITCH_AMBIENTE  1      // 0 = desliga as faixas

// ----- Variáveis compartilhadas -----

// ----- Funções -----
Vector2 CentroRetangulo(Rectangle r);
Objetivo ObjetivoAtual(TelaAtual tela, const ProgressoCorredor *pc, bool luciAcompanhando, Rectangle luciHitbox, bool chefeVencido);
void DesenharFalaLuciHUD(const char *bruto, bool recado, bool sistema);
void DesenharObjetivoHUD(const Objetivo *o);
void DesenharGlitchAmbiente(int nivel, float tempo);

#endif // UTOPIA_OBJETIVO_H
