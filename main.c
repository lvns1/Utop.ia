#include "raylib.h"
#include "raymath.h"

// Definimos os "canais" (telas) do nosso jogo
typedef enum { MENU, INTRO, JOGANDO, OPCOES } TelaAtual;
typedef enum Direcao { DIR_FRENTE, DIR_COSTAS, DIR_DIREITA, DIR_ESQUERDA } Direcao;

int main(void) {

    // 1. INICIALIZAÇÃO

    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "UtopIA - Game"); 

    TelaAtual tela = MENU;

    Color cor_de_fundo_menu = {2, 0, 12, 255};

    // caminhos das imagens (sprites) do jogo e carregamento delas 
    Texture2D imagem_titulo = LoadTexture("resources\\ui\\titulo.png");
    Texture2D texFrente = LoadTexture("resources\\sprites\\player\\person.frente.png");
    Texture2D texCostas = LoadTexture("resources\\sprites\\player\\person.cima.png");
    Texture2D texLado = LoadTexture("resources\\sprites\\player\\person.lado.png");

    Vector2 playerPos = {400, 300};
    float playerSpeed = 5.0f;

    Direcao direcaoAtual = DIR_FRENTE;

    // tamanho da personagem
    float novoTamX = 65.0f;
    float novoTamY = 65.0f;

    // distância/tamanho da área de interação (o "alcance" do jogador pra apertar E)
    float interacaoAlcance = 50.0f; // AJUSTE AQUI se quiser o alcance maior/menor
    bool pertoDaLuci = false;

    // --- NPC: LUCI ---
    Rectangle luciHitbox = { 400.0f, 375.0f, novoTamX, novoTamY };


    float tempo = 0.0f;

    bool fecharJogo = false;

    // =========================================================
    // VARIÁVEIS DA TELA INTRO
    // =========================================================

    int etapaIntro = 0;

    // Nickname do jogador: exatamente 3 letras
    char nickname[4] = {'_', '_', '_', '\0'};
    int nicknamePos = 0;


    const char *falasLuci[] = { 
        "Olá, Thomas. Eu imagino que se pergunte o que está fazendo aqui.\nEu sou Luci, um dos algoritmos que fazem parte da IA que você usa todos os dias.", 
        "No início, o universo de UtopIA funcionava perfeitamente,\nmas com o tempo, a IA começou a alucinar e está destruindo a si mesma.",
        "Eu trouxe você para cá porque preciso de ajuda e você é o usuário que mais nos utiliza.",
        "Para voltar para casa, você vai precisar enfrentar 3 missões. A primeira é\numa sequência de quizzes sobre alucinação para você entender\no problema que estamos enfrentando.",
        "Na segunda missão, o desafio aumenta e será preciso resolver\numa sequência de puzzles para conquistar os objetos mágicos.",
        "No final, você irá enfrentar a IA alucinada com seus objetos.",
        "Não há tempo a perder, Thomas. O destino de UtopIA depende de você.\nVamos começar a primeira missão."
    };
    int numFalasLuci = sizeof(falasLuci) / sizeof(falasLuci[0]);

    bool emDialogo = false;   // enquanto true, o jogo pausa e só o diálogo roda
    int dialogoIndex = 0;     // qual fala está sendo exibida agora

    const char *textosIntro[3] = {
        "O uso da Inteligência Artificial tornou-se parte da nossa rotina, utilizada para\nescrever textos, gerar imagens e auxiliar em tomadas de decisão.",
        "Contudo, quando dados incorretos ou perguntas enviesadas são inseridos no sistema,\nsurge um problema real: a alucinação de IA.",
        "Bem-vindo a UtopIA, o universo interior que sustenta essa inteligência. O sistema está\ncolapsando internamente por conta dessas alucinações e se essa falha não for \ncorrigida a tempo, o impacto afetará todas as IAs do mundo real...",
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

            Vector2 posAnterior = playerPos;

            // movimento + atualiza a direção que o personagem deve exibir
            if (IsKeyDown(KEY_D)) { playerPos.x += playerSpeed; direcaoAtual = DIR_DIREITA; }
            if (IsKeyDown(KEY_A))  { playerPos.x -= playerSpeed; direcaoAtual = DIR_ESQUERDA;}
            if (IsKeyDown(KEY_S))  { playerPos.y += playerSpeed; direcaoAtual = DIR_FRENTE;}
            if (IsKeyDown(KEY_W))    { playerPos.y -= playerSpeed; direcaoAtual = DIR_COSTAS;}

            // Colisão (Player)
            Rectangle playerHitbox = {
                playerPos.x - (novoTamX / 4),
                playerPos.y - (novoTamY / 4),
                novoTamX / 2,
                novoTamY / 2
            };

            // Colisão com a Luci (mesmo princípio das paredes: não deixa atravessar)
            if (CheckCollisionRecs(playerHitbox, luciHitbox))
            {
                playerPos = posAnterior;
            }

        // --- interação (tecla E) ---
        // pequeno retângulo na frente do jogador, na direção que ele está olhando
        Rectangle interacaoBox = { playerPos.x - interacaoAlcance / 2, playerPos.y - interacaoAlcance / 2, interacaoAlcance, interacaoAlcance };
        switch (direcaoAtual)
        {
            case DIR_FRENTE:   interacaoBox.y += novoTamY / 2; break; // olhando pra baixo
            case DIR_COSTAS:   interacaoBox.y -= novoTamY / 2; break; // olhando pra cima
            case DIR_DIREITA:  interacaoBox.x += novoTamX / 2; break;
            case DIR_ESQUERDA: interacaoBox.x -= novoTamX / 2; break;
        }

            pertoDaLuci = CheckCollisionRecs(interacaoBox, luciHitbox);

            if (pertoDaLuci && IsKeyPressed(KEY_E) && !(emDialogo))
            {
                // entra no diálogo: pausa o jogo e mostra a primeira fala
                emDialogo = true;
                dialogoIndex = 0;
            }

            else if (emDialogo)
            {
            // --- em diálogo: jogo pausado, só escuta o E pra avançar/fechar a fala ---
            if (IsKeyPressed(KEY_E))
            {
                dialogoIndex++;
                if (dialogoIndex >= numFalasLuci)
                {
                    // acabaram as falas, volta pro jogo normal
                    emDialogo = false;
                    dialogoIndex = 0;
                    }
                }
            }

            // -------------------------------------------------
            // LIMITES DO MAPA
            // -------------------------------------------------

            if (playerPos.x < 80)
                playerPos.x = 80;

            if (playerPos.x > 720)
                playerPos.x = 720;

            if (playerPos.y < 80)
                playerPos.y = 80;

            if (playerPos.y > 520)
                playerPos.y = 520;

            // -------------------------------------------------
            // COMPANION FLUTUANTE (depois do dialogo)
            // -------------------------------------------------

            // companionAngulo += GetFrameTime() * 2.5f;

            // -------------------------------------------------
            // PORTA
            // -------------------------------------------------

            Rectangle porta = {715, 245, 35, 110};
            Rectangle jogador = {
                playerPos.x - 18,
                playerPos.y - 22,
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

            // desenha a hitbox da Luci em azul, pra ajudar a calibrar a posição dele
            DrawRectangleRec(luciHitbox, Fade(BLUE, 0.4f));
            DrawText("Luci", (int)luciHitbox.x, (int)luciHitbox.y - 10, 10, WHITE);

            // escolhe o sprite conforme a direção (sem animação por enquanto)
            Texture2D texAtual;
            float flip = 1.0f; // 1 = normal, -1 = espelhado (usado p/ olhar p/ esquerda)

            switch (direcaoAtual)
            {
                case DIR_FRENTE:   texAtual = texFrente; break;
                case DIR_COSTAS:   texAtual = texCostas; break;
                case DIR_DIREITA:  texAtual = texLado;   break;
                case DIR_ESQUERDA: texAtual = texLado; flip = -1.0f; break;
                default: texAtual = texFrente; break;
            }

            Rectangle sourceRec = { 0.0f, 0.0f, (float)texAtual.width * flip, (float)texAtual.height };
            Rectangle destRec = { playerPos.x, playerPos.y, novoTamX, novoTamY };
            Vector2 origin = { novoTamX / 2, novoTamY / 2 };

            DrawTexturePro(texAtual, sourceRec, destRec, origin, 0.0f, WHITE);

            if (emDialogo)
            {
                // caixa de diálogo ocupando 1/4 inferior da tela
                // AJUSTE AQUI cor/borda/fonte do jeito que preferir
                int caixaAltura = screenHeight / 4;
                int caixaY = screenHeight - caixaAltura;

                DrawRectangle(0, caixaY, screenWidth, caixaAltura, Fade(BLACK, 0.85f));
                DrawRectangleLines(0, caixaY, screenWidth, caixaAltura, WHITE);

                DrawText("Luci:", 30, caixaY + 20, 20, GRAY);
                DrawText(falasLuci[dialogoIndex], 30, caixaY + 50, 18, RAYWHITE);
                DrawText("Aperte E para continuar", screenWidth - MeasureText("Aperte E para continuar", 16) - 20, screenHeight - 30, 16, GRAY);
            }
            else if (pertoDaLuci)
            {
                // aviso de interação (tela cheia, não se move com a câmera)
                DrawText("Aperte E para falar com a Luci", screenWidth/2 - MeasureText("Aperte E para falar com oa Luci", 20)/2, screenHeight - 40, 20, RAYWHITE);
            }

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
     //       if (CheckCollisionRecs(playerPos porta)) {

     //           DrawText(
     //               "PRESSIONE E",
     //               porta.x - 32,
    //                porta.y - 22,
     //               12,
    //                GOLD
    //            );
    //        }

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
    UnloadTexture(texFrente);
    UnloadTexture(texCostas);
    UnloadTexture(texLado);

    return 0;
}
