#include "raylib.h"
#include "raymath.h"

// Definimos os "canais" (telas) do nosso jogo
typedef enum { MENU, INTRO, JOGANDO, OPCOES } TelaAtual;

int main(void) {

    // 1. INICIALIZAÇÃO

    InitWindow(800, 600, "Menu Inicial");

    TelaAtual tela = MENU;

    Color cor_de_fundo_menu = {2, 0, 12, 255};

    Texture2D imagem_titulo = LoadTexture("titulo.png");

    //Image imagem_jogar = LoadImage("jogar.png");
    //ImageResize(&imagem_jogar, 250, 50);
    //Texture2D textura_jogar = LoadTextureFromImage(imagem_jogar);

    float tempo = 0.0f;

    bool fecharJogo = false;

    // =========================================================
    // VARIÁVEIS DA TELA INTRO
    // =========================================================

    int etapaIntro = 0;

    // Nickname do jogador: exatamente 3 letras
    char nickname[4] = {'_', '_', '_', '\0'};
    int nicknamePos = 0;

    // =========================================================
    // VARIÁVEIS DA TELA JOGANDO
    // =========================================================

    Vector2 jogadorPos = {400, 300};
    float companionAngulo = 0.0f;
    float velocidadeJogador = 220.0f;

    const char *textosIntro[4] = {
        "Tudo comeca em um lugar onde nada e o que parece.",
        "Os acontecimentos dessa historia comecam a se revelar.",
        "Agora voce precisa descobrir o que existe por tras desse mundo.",
        "Sua jornada esta prestes a comecar."
    };

    // =========================================================
    // ÁREAS DE CLIQUE DOS BOTÕES DO MENU
    // =========================================================

    Rectangle btnJogar = {300, 270, 200, 50};
    Rectangle btnOpcoes = {300, 340, 200, 50};
    Rectangle btnSair = {300, 410, 200, 50};

    SetTargetFPS(60);

    // 2. GAME LOOP

    while (!WindowShouldClose() && !fecharJogo) {

        // =====================================================
        // ANIMAÇÃO DO TÍTULO
        // =====================================================

        tempo += GetFrameTime();

        float amplitude = 8.0f;
        float velocidade = 3.0f;

        float y =
            100.0f +
            sinf(tempo * velocidade) * amplitude +
            sinf(tempo * 2.1f) * 4.0f;

        float y1 =
            100.0f +
            sinf((tempo - 0.25f) * velocidade) * amplitude +
            sinf((tempo - 0.25f) * 2.1f) * 4.0f;

        float y2 =
            100.0f +
            sinf((tempo - 0.45f) * velocidade) * amplitude +
            sinf((tempo - 0.45f) * 2.1f) * 4.0f;

        // =====================================================
        // LÓGICA
        // =====================================================

        Vector2 mouse = GetMousePosition();

        Color corBtnJogar = DARKGRAY;
        Color corBtnOpcoes = DARKGRAY;
        Color corBtnSair = DARKGRAY;

        // =====================================================
        // MENU
        // =====================================================

        if (tela == MENU) {

            // JOGAR
            if (CheckCollisionPointRec(mouse, btnJogar)) {

                corBtnJogar = GREEN;

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {

                    tela = INTRO;
                    etapaIntro = 0;
                }
            }

            // OPÇÕES
            if (CheckCollisionPointRec(mouse, btnOpcoes)) {

                corBtnOpcoes = LIGHTGRAY;

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                    tela = OPCOES;
            }

            // SAIR
            if (CheckCollisionPointRec(mouse, btnSair)) {

                corBtnSair = RED;

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                    fecharJogo = true;
            }
        }

        // =====================================================
        // INTRO
        // =====================================================

        else if (tela == INTRO) {

            // =================================================
            // ETAPA FINAL: REGISTRO DO NICKNAME
            // =================================================

            if (etapaIntro == 3) {

                // Lê uma letra por vez (A-Z)
                int tecla = GetKeyPressed();

                if (tecla >= KEY_A && tecla <= KEY_Z && nicknamePos < 3) {

                    nickname[nicknamePos] = (char)('A' + (tecla - KEY_A));
                    nicknamePos++;
                    nickname[nicknamePos] = '\0';
                }

                // BACKSPACE apaga a última letra digitada.
                // Se não houver nenhuma letra, volta para o menu.
                if (IsKeyPressed(KEY_BACKSPACE)) {

                    if (nicknamePos > 0) {

                        nicknamePos--;
                        nickname[nicknamePos] = '_';
                        nickname[nicknamePos + 1] = '\0';

                    } else {

                        etapaIntro = 0;
                        tela = MENU;
                    }
                }

                // ENTER só avança quando as 3 letras estiverem preenchidas.
                if (IsKeyPressed(KEY_ENTER) && nicknamePos == 3) {

                    etapaIntro = 0;
                    tela = JOGANDO;
                }
            }

            // =================================================
            // ETAPAS NORMAIS DA INTRODUÇÃO
            // =================================================

            else {

                // ENTER avança a introdução
                if (IsKeyPressed(KEY_ENTER)) {

                    etapaIntro++;
                }

                // BACKSPACE volta para o menu
                if (IsKeyPressed(KEY_BACKSPACE)) {

                    etapaIntro = 0;
                    tela = MENU;
                }
            }
        }

        // =====================================================
        // JOGANDO
        // =====================================================

        else if (tela == JOGANDO) {

            // -------------------------------------------------
            // MOVIMENTAÇÃO DO PERSONAGEM
            // -------------------------------------------------

            Vector2 direcao = {0};

            if (IsKeyDown(KEY_W))
                direcao.y -= 1.0f;

            if (IsKeyDown(KEY_S))
                direcao.y += 1.0f;

            if (IsKeyDown(KEY_A))
                direcao.x -= 1.0f;

            if (IsKeyDown(KEY_D))
                direcao.x += 1.0f;

            // Normaliza para impedir que a movimentação diagonal
            // fique mais rápida.
            if (Vector2Length(direcao) > 0.0f)
                direcao = Vector2Normalize(direcao);

            jogadorPos = Vector2Add(
                jogadorPos,
                Vector2Scale(direcao, velocidadeJogador * GetFrameTime())
            );

            // -------------------------------------------------
            // LIMITES DO MAPA
            // -------------------------------------------------

            if (jogadorPos.x < 80)
                jogadorPos.x = 80;

            if (jogadorPos.x > 720)
                jogadorPos.x = 720;

            if (jogadorPos.y < 80)
                jogadorPos.y = 80;

            if (jogadorPos.y > 520)
                jogadorPos.y = 520;

            // -------------------------------------------------
            // COMPANION FLUTUANTE
            // -------------------------------------------------

            companionAngulo += GetFrameTime() * 2.5f;

            // -------------------------------------------------
            // PORTA
            // -------------------------------------------------

            Rectangle porta = {715, 245, 35, 110};
            Rectangle jogador = {
                jogadorPos.x - 18,
                jogadorPos.y - 22,
                36,
                44
            };

            // A porta só pode ser usada quando o personagem
            // estiver próximo e apertar E.
            if (CheckCollisionRecs(jogador, porta) &&
                IsKeyPressed(KEY_E)) {

                tela = MENU;
            }
        }

        // =====================================================
        // OPÇÕES
        // =====================================================

        else if (tela == OPCOES) {

            if (IsKeyPressed(KEY_BACKSPACE))
                tela = MENU;
        }

        // =====================================================
        // DESENHO
        // =====================================================

        BeginDrawing();

        // =====================================================
        // MENU
        // =====================================================

        if (tela == MENU) {

            ClearBackground(cor_de_fundo_menu);

            // Título original
            DrawTexture(
                imagem_titulo,
                215,
                (int)y,
                WHITE
            );

            DrawTexture(
                imagem_titulo,
                215,
                (int)y1,
                Fade(PURPLE, 0.1f)
            );

            DrawTexture(
                imagem_titulo,
                215,
                (int)y2,
                Fade(PURPLE, 0.1f)
            );

            // Botões
            DrawRectangleRec(btnJogar, corBtnJogar);
            DrawRectangleRec(btnOpcoes, corBtnOpcoes);
            DrawRectangleRec(btnSair, corBtnSair);

            // Textos
            DrawText(
                "JOGAR",
                365,
                285,
                20,
                WHITE
            );

            DrawText(
                "OPÇÕES",
                360,
                355,
                20,
                WHITE
            );

            DrawText(
                "SAIR",
                375,
                425,
                20,
                WHITE
            );
        }

        // =====================================================
        // INTRO CENTRALIZADA
        // =====================================================

        else if (tela == INTRO) {

            // Mesmo fundo do menu
            ClearBackground(cor_de_fundo_menu);

            // -------------------------------------------------
            // ÁREA CENTRAL
            // -------------------------------------------------

            const int larguraConteudo = 500;
            const int xConteudo = (800 - larguraConteudo) / 2;

            // =================================================
            // INDICADOR DE ETAPAS
            // =================================================

            int espacamento = 14;
            int larguraIndicador = 3 * espacamento;

            int xIndicador = 400 - larguraIndicador / 2;

            for (int i = 0; i < 4; i++) {

                Color corEtapa;

                if (i == etapaIntro)
                    corEtapa = WHITE;
                else
                    corEtapa = Fade(LIGHTGRAY, 0.35f);

                DrawCircle(
                    xIndicador + i * espacamento,
                    48,
                    3,
                    corEtapa
                );
            }

            const char *textoEtapa =
                TextFormat("Etapa %d de 4", etapaIntro + 1);

            DrawText(
                textoEtapa,
                400 - MeasureText(textoEtapa, 9) / 2,
                58,
                9,
                GRAY
            );

            // =================================================
            // PAINEL DA IMAGEM
            // =================================================

            Rectangle painelImagem = {
                xConteudo,
                85,
                larguraConteudo,
                255
            };

            DrawRectangleRec(
                painelImagem,
                (Color){8, 6, 18, 255}
            );

            DrawRectangleLinesEx(
                painelImagem,
                1,
                Fade(LIGHTGRAY, 0.65f)
            );

            // =================================================
            // SIMULAÇÃO DA IMAGEM
            // =================================================

            DrawCircle(
                xConteudo + 415,
                140,
                30,
                Fade(PURPLE, 0.30f)
            );

            DrawCircle(
                xConteudo + 415,
                140,
                19,
                Fade(PURPLE, 0.45f)
            );

            DrawRectangle(
                xConteudo + 55,
                270,
                390,
                3,
                Fade(LIGHTGRAY, 0.25f)
            );

            DrawRectangle(
                xConteudo + 95,
                230,
                90,
                40,
                Fade(DARKPURPLE, 0.55f)
            );

            DrawRectangle(
                xConteudo + 205,
                210,
                150,
                60,
                Fade(DARKPURPLE, 0.35f)
            );

            const char *textoImagem =
                "IMAGEM DA HISTORIA DO JOGO";

            DrawText(
                textoImagem,
                400 - MeasureText(textoImagem, 11) / 2,
                245,
                11,
                Fade(WHITE, 0.65f)
            );

            // =================================================
            // CAIXA DO NARRADOR
            // =================================================

            Rectangle caixaNarrador = {
                xConteudo,
                355,
                larguraConteudo,
                (etapaIntro == 3) ? 110 : 80
            };

            DrawRectangleRec(
                caixaNarrador,
                (Color){245, 245, 245, 255}
            );

            DrawRectangleLinesEx(
                caixaNarrador,
                1,
                Fade(LIGHTGRAY, 0.6f)
            );

            // Conteúdo da caixa de narrativa
            if (etapaIntro < 3) {

                DrawRectangle(
                    xConteudo + 12,
                    349,
                    48,
                    16,
                    LIGHTGRAY
                );

                DrawText(
                    "Narrador",
                    xConteudo + 16,
                    353,
                    8,
                    BLACK
                );

                DrawText(
                    textosIntro[etapaIntro],
                    xConteudo + 12,
                    380,
                    11,
                    DARKGRAY
                );

            } else {

                // Etiqueta da etapa final
                DrawRectangle(
                    xConteudo + 12,
                    349,
                    58,
                    16,
                    LIGHTGRAY
                );

                DrawText(
                    "Marcador",
                    xConteudo + 16,
                    353,
                    8,
                    BLACK
                );

                DrawText(
                    "LUCI // REGISTRO DE IDENTIDADE!",
                    xConteudo + 12,
                    377,
                    9,
                    BLACK
                );

                DrawText(
                    "Digite seu codigo de identificacao de 3 letras:",
                    xConteudo + 12,
                    393,
                    10,
                    DARKGRAY
                );

                // Três campos do nickname, como no print
                for (int i = 0; i < 3; i++) {

                    Rectangle caixaLetra = {
                        245 + i * 55,
                        405,
                        45,
                        45
                    };

                    Color corCaixa =
                        (i == nicknamePos && nicknamePos < 3)
                        ? (Color){225, 225, 225, 255}
                        : (Color){238, 238, 238, 255};

                    DrawRectangleRec(
                        caixaLetra,
                        corCaixa
                    );

                    const char letra[2] = {
                        nickname[i],
                        '\0'
                    };

                    DrawText(
                        letra,
                        caixaLetra.x + 12,
                        caixaLetra.y + 5,
                        32,
                        BLACK
                    );
                }
            }

            // =================================================
            // BOTÃO CONTINUAR
            // =================================================

            Rectangle btnContinuar = {
                xConteudo + larguraConteudo - 125,
                (etapaIntro == 3) ? 475 : 457,
                125,
                30
            };

            Color corContinuar = BLACK;

            // Na última etapa, o botão só pode continuar com 3 letras.
            bool podeContinuar =
                (etapaIntro < 3) || (nicknamePos == 3);

            if (CheckCollisionPointRec(mouse, btnContinuar) && podeContinuar) {

                corContinuar = DARKGRAY;

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {

                    if (etapaIntro < 3) {

                        etapaIntro++;

                    } else if (nicknamePos == 3) {

                        etapaIntro = 0;
                        tela = JOGANDO;
                    }
                }
            }

            DrawRectangleRec(
                btnContinuar,
                corContinuar
            );

            // Texto do botão
            const char *textoContinuar = "CONTINUAR";

            DrawText(
                textoContinuar,
                btnContinuar.x + 22,
                btnContinuar.y + 9,
                8,
                WHITE
            );

            // Seta
            DrawText(
                ">",
                btnContinuar.x + 102,
                btnContinuar.y + 7,
                12,
                WHITE
            );

            // =================================================
            // INSTRUÇÕES
            // =================================================

            const char *instrucao1;
            const char *instrucao2;

            if (etapaIntro == 3) {

                instrucao1 = "Digite A-Z | ENTER para confirmar";
                instrucao2 = "BACKSPACE apaga a ultima letra";

            } else {

                instrucao1 = "ENTER para continuar";
                instrucao2 = "BACKSPACE para voltar";
            }

            DrawText(
                instrucao1,
                400 - MeasureText(instrucao1, 10) / 2,
                510,
                10,
                GRAY
            );

            DrawText(
                instrucao2,
                400 - MeasureText(instrucao2, 10) / 2,
                528,
                10,
                GRAY
            );
        }

        // =====================================================
        // TELA JOGANDO
        // =====================================================

        else if (tela == JOGANDO) {

            ClearBackground((Color){8, 6, 18, 255});

            // -------------------------------------------------
            // MAPA SIMPLES
            // -------------------------------------------------

            Rectangle mapa = {50, 50, 700, 500};

            // Chão
            DrawRectangleRec(
                mapa,
                (Color){18, 15, 30, 255}
            );

            // Bordas do mapa
            DrawRectangleLinesEx(
                mapa,
                4,
                Fade(PURPLE, 0.55f)
            );

            // -------------------------------------------------
            // PORTA
            // -------------------------------------------------

            Rectangle porta = {715, 245, 35, 110};

            DrawRectangleRec(
                porta,
                (Color){35, 25, 55, 255}
            );

            DrawRectangleLinesEx(
                porta,
                3,
                PURPLE
            );

            // Maçaneta
            DrawCircle(
                porta.x + 10,
                porta.y + porta.height / 2,
                4,
                GOLD
            );

            DrawText(
                "PORTA",
                porta.x - 4,
                porta.y + porta.height + 10,
                10,
                LIGHTGRAY
            );

            // -------------------------------------------------
            // PERSONAGEM
            // -------------------------------------------------

            Rectangle jogador = {
                jogadorPos.x - 18,
                jogadorPos.y - 22,
                36,
                44
            };

            DrawRectangleRec(
                jogador,
                WHITE
            );

            DrawRectangleLinesEx(
                jogador,
                2,
                PURPLE
            );

            // -------------------------------------------------
            // COMPANION
            // -------------------------------------------------

            Vector2 companionPos = {
                jogadorPos.x + cosf(companionAngulo) * 38.0f,
                jogadorPos.y + sinf(companionAngulo * 1.2f) * 26.0f
            };

            Rectangle companion = {
                companionPos.x - 7,
                companionPos.y - 7,
                14,
                14
            };

            DrawRectangleRec(
                companion,
                SKYBLUE
            );

            DrawRectangleLinesEx(
                companion,
                2,
                BLUE
            );

            // -------------------------------------------------
            // INSTRUÇÕES
            // -------------------------------------------------

            DrawText(
                "W A S D  -  MOVIMENTAR",
                65,
                65,
                12,
                LIGHTGRAY
            );

            DrawText(
                "E  -  INTERAGIR COM A PORTA",
                65,
                82,
                12,
                LIGHTGRAY
            );

            // Indicador visual quando estiver sobre a porta.
            if (CheckCollisionRecs(jogador, porta)) {

                DrawText(
                    "PRESSIONE E",
                    porta.x - 32,
                    porta.y - 22,
                    12,
                    GOLD
                );
            }

            // Nickname do jogador
            DrawText(
                nickname,
                700 - MeasureText(nickname, 14),
                65,
                14,
                WHITE
            );
        }

        // =====================================================
        // TELA OPÇÕES
        // =====================================================

        else if (tela == OPCOES) {

            ClearBackground(DARKBLUE);

            DrawText(
                "TELA DE OPÇÕES",
                280,
                250,
                30,
                YELLOW
            );

            DrawText(
                "Aperte BACKSPACE para voltar",
                250,
                300,
                20,
                LIGHTGRAY
            );
        }

        EndDrawing();
    }

    // 3. FINALIZAÇÃO

    CloseWindow();

    UnloadTexture(imagem_titulo);

    //UnloadImage(imagem_jogar);
    //UnloadTexture(textura_jogar);

    return 0;
}