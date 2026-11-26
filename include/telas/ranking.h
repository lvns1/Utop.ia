// Ranking TOP 10 (utopia_ranking.dat) e a tela que mostra a classificação.
#ifndef UTOPIA_RANKING_H
#define UTOPIA_RANKING_H

#include <stdbool.h>
#include "raylib.h"
#include "raymath.h"
#include "../core/config.h"
#include "../core/tipos.h"

// =============================================================
// RANKING (TOP 10)
//   Fica num arquivo separado do save: começar um jogo novo não apaga
//   o ranking. Cada partida tem um número (idPartida, que vai no save)
//   e aparece uma vez só, com a melhor pontuação que fez.
//   Gravado no fim do Bloco 1 (ALUCINAÇÃO MESTRE) e na porta do NÚCLEO.
// =============================================================

#define ARQUIVO_RANKING       "utopia_ranking.dat"
#define ARQUIVO_RANKING_TEMP  "utopia_ranking.dat.tmp"
#define ASSINATURA_RANKING    0x4B4E4152u
#define VERSAO_RANKING        1
#define MAX_RANKING           10
#define TITULO_RANKING        "TOP 10 TechNight"     // AJUSTE AQUI (nome do evento)

typedef struct {
    char nickname[4];
    int pontos;
    int idPartida;
} EntradaRanking;

typedef struct {
    unsigned int assinatura;
    int versao;
    int total;
    EntradaRanking entradas[MAX_RANKING];   // da maior para a menor pontuação
} Ranking;

// Ranking aberto por cima do jogo (fim do Bloco 1 / fim da partida)
typedef struct {
    bool aberto;
    const char *titulo;
    int posicao;                // 0 = 1º lugar, -1 = ficou fora do TOP 10
    int pontos;
    int idPartida;
    double inicio;
    bool revelado;              // a posição já apareceu (o som toca uma vez só)
} TelaRanking;

// ----- Variáveis compartilhadas -----

// ----- Funções -----
int NovoIdPartida(void);
void LerRanking(Ranking *r);
bool NicknameDisponivel(Ranking *r, const char *nickname);
void AbrirTelaRanking(TelaRanking *t, Ranking *r, const char *titulo, const char *nickname, int pontos, int idPartida);
bool AtualizarTelaRanking(TelaRanking *t);
void DesenharTelaRanking(const TelaRanking *t, const Ranking *r, float tempo);
void DesenharRankingMenu(const Ranking *r, float tempo);

#endif // UTOPIA_RANKING_H
