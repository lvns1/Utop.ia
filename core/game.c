#include "game.h"
#include "../scenes/menu.h"
#include "../scenes/intro.h"
#include "../scenes/sala.h"
#include "../scenes/corredor.h"
#include "../data/quizzes.h"

void JogoInicializar(Jogo *g) {
    *g = (Jogo){0};
    g->tela = MENU;
    g->corFundoMenu = (Color){2, 0, 12, 255};

    g->imagemTitulo = LoadTexture("resources/ui/titulo.png");
    g->texFrente = LoadTexture(SPRITE_FRENTE);
    g->texCostas = FileExists(SPRITE_COSTAS) ? LoadTexture(SPRITE_COSTAS) : LoadTexture(SPRITE_COSTAS_RESERVA);
    g->texLado = LoadTexture(SPRITE_LADO);

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

    g->interacaoAlcance = 50.0f;
    g->luciHitbox = (Rectangle){ 400.0f, 375.0f, TAM_SPRITE, TAM_SPRITE };
    g->quizDaBatalha = -1;
    g->motivoBatalha = RES_NADA;

    JogoReiniciarPartida(g);
}

void JogoReiniciarPartida(Jogo *g) {
    g->progresso = (ProgressoCorredor){0};
    g->dlg = (SessaoDialogo){0};
    g->luciAcompanhando = false;
    g->playerPos = SPAWN_SALA;
    g->direcaoAtual = DIR_FRENTE;
    g->etapaIntro = 0;
    g->nickname[0] = g->nickname[1] = g->nickname[2] = '_';
    g->nickname[3] = '\0';
    g->nicknamePos = 0;
}

void JogoAtualizar(Jogo *g) {
    float dt = GetFrameTime();
    g->tempo += dt;

    TelaAtual origem = g->tela;
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
        MenuAtualizar(g);
    } else if (g->tela == INTRO) {
        IntroAtualizar(g);
    } else if (g->tela == JOGANDO && !g->transicao.ativa) {
        SalaAtualizar(g, dt);
    } else if (g->tela == CORREDOR && !g->transicao.ativa) {
        CorredorAtualizar(g, dt);
    } else if (g->tela == BATALHA && !g->transicao.ativa) {
        if (BatalhaAtualizar(&g->batalha)) {
            if (g->batalha.derrota)
                g->progresso.alucinacao++;

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
        if (IsKeyPressed(KEY_BACKSPACE))
            g->tela = MENU;
    }
}

void JogoDesenhar(const Jogo *g) {
    BeginDrawing();

    if (g->tela == MENU) {
        MenuDesenhar(g);
    } else if (g->tela == INTRO) {
        IntroDesenhar(g);
    } else if (g->tela == JOGANDO) {
        SalaDesenhar(g);
    } else if (g->tela == CORREDOR) {
        CorredorDesenhar(g);
    } else if (g->tela == BATALHA) {
        BatalhaDesenhar(&g->batalha, g->nickname, g->tempo);
    } else if (g->tela == OPCOES) {
        ClearBackground(DARKBLUE);
        DrawText("TELA DE OPÇÕES", 280, 250, 30, YELLOW);
        DrawText("Aperte BACKSPACE para voltar", 250, 300, 20, LIGHTGRAY);
    }

    DesenharTransicao(&g->transicao);
    EndDrawing();
}

void JogoFinalizar(Jogo *g) {
    UnloadTexture(g->imagemTitulo);
    UnloadTexture(g->texFrente);
    UnloadTexture(g->texCostas);
    UnloadTexture(g->texLado);

    for (int i = 0; i < 4; i++) {
        if (g->spritesMonitor[i].id > 0) {
            UnloadTexture(g->spritesMonitor[i]);
        }
    }
}
