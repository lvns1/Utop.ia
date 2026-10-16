// game.h - A struct Jogo (todo o estado da partida) e o ciclo de vida do jogo.

#ifndef GAME_H
#define GAME_H

#include "config.h"
#include "transition.h"
#include "../systems/dialogue.h"
#include "../systems/quiz.h"
#include "../systems/battle.h"
#include "../entities/player.h"

// Estado completo da partida. Antes eram variáveis soltas no main();
// agora as cenas recebem um Jogo* e alteram só o que precisam.
typedef struct {
    TelaAtual tela;
    Color corFundoMenu;

    // Recursos visuais
    Texture2D imagemTitulo;
    Texture2D spritesMonitor[4];
    SpriteDirecao spritesJogador[4];
    AnimacaoJogador animJogador;

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
    // Nickname do jogador: exatamente 3 letras
    char nickname[4];
    int nicknamePos;

    // Sistemas
    ProgressoCorredor progresso;
    SessaoDialogo dlg;
    Transicao transicao;
    Batalha batalha;
    int quizDaBatalha;                      // monitor que gerou a batalha
    ResultadoDialogo motivoBatalha;
} Jogo;

void JogoInicializar(Jogo *g);
void JogoReiniciarPartida(Jogo *g);
void JogoAtualizar(Jogo *g);
void JogoDesenhar(const Jogo *g);
void JogoFinalizar(Jogo *g);

#endif
