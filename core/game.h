#ifndef GAME_H
#define GAME_H

#include "config.h"
#include "transition.h"
#include "../systems/dialogue.h"
#include "../systems/quiz.h"
#include "../systems/battle.h"

typedef struct {
    TelaAtual tela;
    Color corFundoMenu;

    // Recursos visuais
    Texture2D imagemTitulo;
    Texture2D texFrente;
    Texture2D texCostas;
    Texture2D texLado;
    Texture2D spritesMonitor[4];

    // Estado do Jogador
    Vector2 playerPos;
    Direcao direcaoAtual;
    float interacaoAlcance;
    bool pertoDaLuci;

    // Estado da Luci (NPC/Companion)
    Rectangle luciHitbox;
    bool luciAcompanhando;
    float companionAngulo;

    // Estado Geral e Controle
    float tempo;
    bool fecharJogo;

    // Estado da Intro
    int etapaIntro;
    char nickname[4];
    int nicknamePos;

    // Sistemas
    ProgressoCorredor progresso;
    SessaoDialogo dlg;
    Transicao transicao;
    Batalha batalha;
    int quizDaBatalha;
    ResultadoDialogo motivoBatalha;
} Jogo;

void JogoInicializar(Jogo *g);
void JogoReiniciarPartida(Jogo *g);
void JogoAtualizar(Jogo *g);
void JogoDesenhar(const Jogo *g);
void JogoFinalizar(Jogo *g);

#endif
