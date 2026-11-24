// Estado do jogo: tudo o que o main() guardava em variáveis locais.
// As telas (src/telas/tela_*.c) recebem um ponteiro para ele (jg).
#ifndef UTOPIA_JOGO_H
#define UTOPIA_JOGO_H

#include <stdbool.h>
#include "raylib.h"
#include "../audio/audio.h"
#include "../bloco1/batalha.h"
#include "../bloco1/batalha_monitores.h"
#include "../bloco1/corredor.h"
#include "../bloco1/minigame_visual.h"
#include "../bloco1/objetivo.h"
#include "../bloco2/biblioteca.h"
#include "../bloco2/biblioteca_tipos.h"
#include "../core/config.h"
#include "../core/save.h"
#include "../core/telemetria.h"
#include "../core/texto.h"
#include "../core/tipos.h"
#include "../dados/conteudo_bloco1.h"
#include "../entidades/jogador.h"
#include "../entidades/luci_companion.h"
#include "../telas/intro.h"
#include "../telas/pause.h"
#include "../telas/ranking.h"
#include "../telas/resolucao.h"

typedef struct Jogo {
    // --- Janela, tela atual e recursos carregados ---
    RenderTexture2D alvo;
    FiltroNitidez filtroNitidez;
    ConfigExibicao exibicao;
    MenuOpcoes menuOpcoes;
    TelaAtual tela;
    Color cor_de_fundo_menu;
    Texture2D imagem_titulo;
    SpriteDirecao spritesJogador[NUM_DIRECOES];
    AnimacaoJogador animJogador;
    Texture2D spritesMonitor[4];
    Texture2D texMapaSala;
    bool fecharJogo;
    float tempo;

    // --- Jogador e Luci ---
    Vector2 playerPos;
    Direcao direcaoAtual;
    float interacaoAlcance;
    bool pertoDaLuci;
    Rectangle luciHitbox;
    bool luciAcompanhando;
    float companionAngulo;

    // --- Intro e nickname ---
    int etapaIntro;
    char nickname[4];
    int nicknamePos;
    bool cliqueConfirmarNome;
    float avisoNomeRepetido;
    const char *textosIntro[3];

    // --- Corredor, batalhas e Biblioteca ---
    ProgressoCorredor progresso;
    BatalhaB2 batalhaB2;
    TelaAtual salaDaBatalhaB2;
    CartaoResumo cartao;
    float tempoParado;
    int estabilidadeLuci;
    int ajudasPedidas;
    bool chefeVencido;
    bool entrarSalaMestre;
    bool chefeAnunciado;
    Biblioteca biblioteca;
    ProgressoBiblioteca progBiblioteca;
    bool voltarAoMenuDepoisDoRanking;
    int idPartida;
    SessaoDialogo dlg;
    Transicao transicao;
    Batalha batalha;
    int quizDaBatalha;
    ResultadoDialogo motivoBatalha;
    ContextoB2 ctxB2;

    // --- Save e ranking ---
    DadosSave saveLido;
    bool existeSave;
    bool pedirAutoSave;
    float avisoSave;
    bool saveFalhou;
    Ranking ranking;
    TelaRanking telaRanking;

    // --- Menu e pause ---
    int focoMenu;
    Rectangle btnContinuar;
    Rectangle btnJogar;
    Rectangle btnOpcoes;
    Rectangle btnSair;
    bool pausado;
    bool capturarPausa;
    int selecaoPausa;
    float confirmarSaida;
    int opConfirmandoPausa;
    Texture2D fundoPausa;

    // --- Do quadro atual (recalculados a cada volta do laço) ---
    float dt;
    bool esconderCursor;
    float amplitude;
    float velocidade;
    float y;
    float y1;
    float y2;
    Vector2 posInicioFrame;
    Vector2 mouse;
    Color corBtnJogar;
    Color corBtnOpcoes;
    Color corBtnSair;
    Color corBtnContinuar;
    float yBotoes;
    bool andou;
    Objetivo objetivo;
    bool emExploracao;
    bool podeReagir;
} Jogo;

// Cada tela: lógica (Atualizar) e desenho (Desenhar). O main() chama a da tela atual.
void TelaMenuAtualizar(Jogo *jg);
void TelaIntroAtualizar(Jogo *jg);
void TelaSalaAtualizar(Jogo *jg);
void TelaCorredorAtualizar(Jogo *jg);
void TelaBatalhaAtualizar(Jogo *jg);
void TelaBatalhaB2Atualizar(Jogo *jg);
void TelaBibliotecaAtualizar(Jogo *jg);
void TelaOpcoesAtualizar(Jogo *jg);
void TelaMenuDesenhar(Jogo *jg);
void TelaIntroDesenhar(Jogo *jg);
void TelaSalaDesenhar(Jogo *jg);
void TelaCorredorDesenhar(Jogo *jg);
void TelaBatalhaDesenhar(Jogo *jg);
void TelaBatalhaB2Desenhar(Jogo *jg);
void TelaBibliotecaDesenhar(Jogo *jg);
void TelaOpcoesDesenhar(Jogo *jg);

#endif // UTOPIA_JOGO_H
