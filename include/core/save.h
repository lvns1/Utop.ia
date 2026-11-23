// Save do jogo (utopia.sav): gravar, carregar e conferir a versão.
#ifndef UTOPIA_SAVE_H
#define UTOPIA_SAVE_H

#include <stdbool.h>
#include "raylib.h"
#include "raymath.h"
#include "../core/config.h"
#include "../core/tipos.h"


// ----- Variáveis compartilhadas -----

// ----- Funções -----
bool LerSave(DadosSave *d);
bool SalvarJogo(const char *nickname, TelaAtual tela, Vector2 pos, Direcao direcao, bool luciAcompanhando, const ProgressoCorredor *progresso, int estabilidadeLuci, int ajudasPedidas, bool chefeVencido, int idPartida, const ProgressoBiblioteca *biblioteca);
void DesenharAvisoSave(float aviso, bool falhou, float tempo);
void DesenharTextoBotao(Rectangle b, const char *texto, int tamanho, Color cor);

#endif // UTOPIA_SAVE_H
