// game.c - Ciclo de vida do jogo: inicializar, atualizar, desenhar e finalizar.
// A atualização/desenho só escolhe a tela atual e delega para a cena em scenes/.

#include "game.h"
#include "../scenes/menu.h"
#include "../scenes/intro.h"
#include "../scenes/sala.h"
#include "../scenes/corredor.h"
#include "../data/quizzes.h"

// Carrega os recursos (sprites) e zera o estado do jogo. A partida começa no MENU.
void JogoInicializar(Jogo *g) {
    *g = (Jogo){0};
    g->tela = MENU;
    g->corFundoMenu = (Color){2, 0, 12, 255};

    // caminhos das imagens (sprites) do jogo e carregamento delas
    g->imagemTitulo = LoadTexture("resources/ui/titulo.png");
    g->spritesJogador[DIR_FRENTE] = CarregarSprite(
    SPRITE_ANDANDO_FRENTE,
    SPRITE_FRENTE,
    false
);

g->spritesJogador[DIR_COSTAS] = CarregarSprite(
    SPRITE_ANDANDO_COSTAS,
    SPRITE_COSTAS_RESERVA,
    false
);

g->spritesJogador[DIR_DIREITA] = CarregarSprite(
    SPRITE_ANDANDO_DIREITA,
    SPRITE_LADO,
    false
);

g->spritesJogador[DIR_ESQUERDA] = CarregarSprite(
    SPRITE_ANDANDO_ESQUERDA,
    SPRITE_LADO,
    true
);

g->animJogador = (AnimacaoJogador){0};

    // Sprites dos monitores (só carrega os que existirem)
    const char *arquivosMonitor[4] = {
        SPRITE_MONITOR_NEUTRO,
        SPRITE_MONITOR_ATIVO,
        SPRITE_MONITOR_ACERTO,
        SPRITE_MONITOR_ERRO
    };

    for (int i = 0; i < 4; i++) {
        if (FileExists(arquivosMonitor[i])) {
            g->spritesMonitor[i] = LoadTexture(arquivosMonitor[i]);
        }
    }

    // distância/tamanho da área de interação (o "alcance" do jogador pra apertar E)
    g->interacaoAlcance = 50.0f;            // AJUSTE AQUI se quiser o alcance maior/menor

    // --- NPC: LUCI ---
    g->luciHitbox = (Rectangle){ 400.0f, 375.0f, TAM_SPRITE, TAM_SPRITE };
    g->quizDaBatalha = -1;
    g->motivoBatalha = RES_NADA;

    JogoReiniciarPartida(g);
}

// Novo jogo (botão JOGAR): zera progresso, conversa, posição e nickname, mantendo os recursos carregados.
void JogoReiniciarPartida(Jogo *g) {
    g->progresso = (ProgressoCorredor){0};
    g->dlg = (SessaoDialogo){0};
    g->luciAcompanhando = false;            // depois da conversa ela vira companion
    g->playerPos = SPAWN_SALA;
    g->direcaoAtual = DIR_FRENTE;
    g->etapaIntro = 0;
    g->nickname[0] = g->nickname[1] = g->nickname[2] = '_';
    g->nickname[3] = '\0';
    g->nicknamePos = 0;
}

// LÓGICA: roda uma vez por frame. Atualiza o fade e delega para a cena da tela atual.
// Durante um fade, as cenas JOGANDO, CORREDOR e BATALHA ficam paradas.
void JogoAtualizar(Jogo *g) {
    float dt = GetFrameTime();
    g->tempo += dt;

    TelaAtual origem = g->tela;

    // Tela toda preta: troca de tela e posiciona o jogador
    if (AtualizarTransicao(&g->transicao, &g->tela, dt, &origem)) {
        if (g->tela == CORREDOR && origem == JOGANDO) {
            g->playerPos = SPAWN_CORREDOR;
            g->direcaoAtual = DIR_COSTAS;
        } else if (g->tela == JOGANDO && origem == CORREDOR) {
            g->playerPos = SPAWN_SALA_VOLTA;
            g->direcaoAtual = DIR_FRENTE;
        }
    }

    if (g->tela == MENU) {
        // ---------- MENU ----------
        MenuAtualizar(g);
    } else if (g->tela == INTRO) {
        // ---------- INTRO ----------
        IntroAtualizar(g);
    } else if (g->tela == JOGANDO && !g->transicao.ativa) {
        // ---------- JOGANDO (SALA INICIAL) ----------
        SalaAtualizar(g, dt);
    } else if (g->tela == CORREDOR && !g->transicao.ativa) {
        // ---------- CORREDOR DOS MONITORES ----------
        CorredorAtualizar(g, dt);
    } else if (g->tela == BATALHA && !g->transicao.ativa) {
        // ---------- BATALHA (COMBATE / MINIGAME) ----------
        if (BatalhaAtualizar(&g->batalha)) {
            if (g->batalha.derrota)
                g->progresso.alucinacao++;

            // De volta ao corredor: Luci explica a resposta certa
            const Dialogo *q = &monitores[g->quizDaBatalha].quiz;
            g->dlg.conteudo = *q;
            g->dlg.objeto = g->quizDaBatalha;
            g->dlg.respondeu = true;
            g->dlg.acertou = false;

            DialogoMostrarFeedback(
                &g->dlg,
                TextFormat(
                    "Luci: %s A resposta certa era a %c. %s",
                    (g->motivoBatalha == RES_TEMPO) ? "O tempo acabou!" : "Não foi dessa vez.",
                    'A' + q->correta,
                    q->explicacao
                ),
                ORANGE,
                0
            );

            IniciarTransicao(&g->transicao, CORREDOR, DURACAO_FADE_BATALHA);
        }
    } else if (g->tela == OPCOES) {
        // ---------- OPÇÕES ----------
        if (IsKeyPressed(KEY_BACKSPACE))
            g->tela = MENU;
    }
}

// DESENHO: desenha a cena da tela atual e, por cima de tudo, o fade da transição.
void JogoDesenhar(const Jogo *g) {
    // ---------- DESENHO ----------
    BeginDrawing();

    if (g->tela == MENU) {
        // ---------- MENU ----------
        MenuDesenhar(g);
    } else if (g->tela == INTRO) {
        // ---------- INTRO CENTRALIZADA ----------
        IntroDesenhar(g);
    } else if (g->tela == JOGANDO) {
        // ---------- TELA JOGANDO (SALA INICIAL) ----------
        SalaDesenhar(g);
    } else if (g->tela == CORREDOR) {
        // ---------- TELA CORREDOR ----------
        CorredorDesenhar(g);
    } else if (g->tela == BATALHA) {
        // ---------- TELA BATALHA ----------
        BatalhaDesenhar(&g->batalha, g->nickname, g->tempo);
    } else if (g->tela == OPCOES) {
        // ---------- TELA OPÇÕES ----------
        ClearBackground(DARKBLUE);
        DrawText("TELA DE OPÇÕES", 280, 250, 30, YELLOW);
        DrawText("Aperte BACKSPACE para voltar", 250, 300, 20, LIGHTGRAY);
    }

    // ---------- FADE DA TRANSIÇÃO (sempre por cima de tudo) ----------
    DesenharTransicao(&g->transicao);

    EndDrawing();
    }

    // Descarrega as texturas (chamado antes do CloseWindow).
    void JogoFinalizar(Jogo *g) {
    // Textura do título
    UnloadTexture(g->imagemTitulo);

    // Texturas do jogador (sprites de animação)
    for (int i = 0; i < 4; i++) {
        if (g->spritesJogador[i].textura.id > 0) {
            UnloadTexture(g->spritesJogador[i].textura);
        }
    }
}
