#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <time.h>
#include "../../include/core/fonte.h"
#include "../../include/core/mapas.h"
#include "../../include/telas/pause.h"
#include "../../include/audio/audio.h"
#include "../../include/bloco1/batalha.h"
#include "../../include/bloco1/batalha_monitores.h"
#include "../../include/bloco1/corredor.h"
#include "../../include/bloco1/minigame_visual.h"
#include "../../include/bloco1/objetivo.h"
#include "../../include/core/save.h"
#include "../../include/core/telemetria.h"
#include "../../include/core/texto.h"
#include "../../include/dados/conteudo_bloco1.h"
#include "../../include/entidades/jogador.h"
#include "../../include/entidades/luci_companion.h"
#include "../../include/telas/ranking.h"


// PAUSA_LOBBY só aparece durante a run do Bloco 2; sem ele, as outras opções sobem uma posição
static int PausaSlot(int opcao, bool temLobby)
{
    return opcao - ((!temLobby && opcao > PAUSA_LOBBY) ? 1 : 0);
}

static int PausaOpcaoVizinha(int opcao, int passo, bool temLobby)
{
    do { opcao = (opcao + NUM_OPCOES_PAUSA + passo) % NUM_OPCOES_PAUSA; } while (opcao == PAUSA_LOBBY && !temLobby);
    return opcao;
}

// Tira uma "foto" da tela atual (com as cores normais do jogo) e desfoca:
// diminui e aumenta de novo a imagem, perdendo os detalhes finos.
// É feito uma vez só, no momento em que o jogo pausa.
Texture2D CapturarFundoPausa(RenderTexture2D alvo)
{
    // A textura de desenho fica de cabeça para baixo na memória
    Image img = LoadImageFromTexture(alvo.texture);
    ImageFlipVertical(&img);
    int w = img.width;
    int h = img.height;

    ImageResize(&img, w / DESFOQUE_PAUSA, h / DESFOQUE_PAUSA);
    ImageResize(&img, w, h);

    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

    Texture2D t = LoadTextureFromImage(img);
    SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
    UnloadImage(img);

    return t;
}

static Rectangle RetanguloOpcaoPausa(int slot)
{
    return (Rectangle){ 270, 228 + slot * 52.0f, 260, 42 };
}

// Lê teclado e mouse no menu de pause.
// Devolve a opção escolhida (PAUSA_CONTINUAR / PAUSA_MENU / PAUSA_SAIR) ou -1 se nada foi escolhido.
//   W / S / SETAS          : troca a opção
//   ENTER / E / ESPAÇO     : confirma
//   mouse                  : passa por cima para marcar, clica para escolher
int AtualizarPausa(int *selecao, bool temLobby)
{
    if (*selecao == PAUSA_LOBBY && !temLobby)
        *selecao = PAUSA_CONTINUAR;

    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))
        *selecao = PausaOpcaoVizinha(*selecao, -1, temLobby);

    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN))
        *selecao = PausaOpcaoVizinha(*selecao, 1, temLobby);

    Vector2 mouse = GetMousePosition();
    bool mouseMexeu = (Vector2Length(GetMouseDelta()) > 0.0f);

    for (int i = 0; i < NUM_OPCOES_PAUSA; i++) {

        if (i == PAUSA_LOBBY && !temLobby)
            continue;

        if (!CheckCollisionPointRec(mouse, RetanguloOpcaoPausa(PausaSlot(i, temLobby))))
            continue;

        if (mouseMexeu)
            *selecao = i;

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            return i;
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE))
        return *selecao;

    return -1;
}

void DesenharPausa(Texture2D fundo, int selecao, bool temLobby, int opConfirmando)
{
    // Paleta do jogo
    Color fundoEscuro = (Color){ 8, 6, 18, 255 };      // fundo das salas
    Color painelCor   = (Color){ 18, 15, 30, 255 };    // chão das salas

    // Fundo: a tela do jogo congelada e desfocada, com as cores normais
    ClearBackground(fundoEscuro);

    if (fundo.id > 0) {
        DrawTexturePro(
            fundo,
            (Rectangle){ 0, 0, (float)fundo.width, (float)fundo.height },
            (Rectangle){ 0, 0, LARGURA_TELA, ALTURA_TELA },
            (Vector2){ 0, 0 },
            0.0f,
            WHITE
        );
    }

    // Escurece só um pouco, para o menu se destacar
    DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(fundoEscuro, 0.35f));

    // Painel roxo, com um filete azul por dentro
    Rectangle painel = { 240, 140, 320, 335 };

    DrawRectangleRec(painel, Fade(painelCor, 0.92f));
    DrawRectangleLinesEx(painel, 4, PURPLE);
    DrawRectangleLinesEx((Rectangle){ painel.x + 8, painel.y + 8, painel.width - 16, painel.height - 16 }, 1, Fade(SKYBLUE, 0.35f));

    const char *titulo = "PAUSADO";
    DrawText(titulo, LARGURA_TELA / 2 - MeasureText(titulo, 40) / 2, 170, 40, PURPLE);

    // Opções, com a Luci na marcada
    const char *opcoes[NUM_OPCOES_PAUSA] = {
        "CONTINUAR",
        (opConfirmando == PAUSA_LOBBY) ? "ABANDONAR RUN?" : "VOLTAR AO LOBBY",
        "MENU PRINCIPAL",
        (opConfirmando == PAUSA_SAIR) ? "FECHAR O JOGO?" : "SAIR DO JOGO"
    };

    for (int i = 0; i < NUM_OPCOES_PAUSA; i++) {

        if (i == PAUSA_LOBBY && !temLobby)
            continue;

        Rectangle r = RetanguloOpcaoPausa(PausaSlot(i, temLobby));
        bool marcada = (i == selecao);

        if (marcada) {
            DrawRectangleRec(r, Fade(PURPLE, 0.15f));
            DesenharCursorLuci(r.x + 26, r.y + r.height / 2);
        }

        bool pedindo = (i == opConfirmando);

        if (pedindo)
            DrawRectangleRec(r, Fade(MAROON, 0.55f));

        DrawText(opcoes[i], (int)r.x + 50, (int)(r.y + r.height / 2) - 10, 20, pedindo ? WHITE : (marcada ? SKYBLUE : LIGHTGRAY));
    }

    const char *dica1 = "ESC - voltar ao jogo";
    const char *dica2 = "O progresso é salvo ao sair ou voltar ao menu";

    if (opConfirmando == PAUSA_LOBBY) {
        dica1 = "Clique ou ENTER de novo para voltar ao lobby (ESC cancela)";
        dica2 = "Os itens e o avanço desta tentativa serão perdidos";
    } else if (opConfirmando == PAUSA_SAIR) {
        dica1 = "Clique ou ENTER de novo para fechar o jogo (ESC cancela)";
        dica2 = "Seu progresso será salvo antes de fechar";
    }

    DrawText(dica1, LARGURA_TELA / 2 - MeasureText(dica1, 10) / 2, 440, 10, Fade(SKYBLUE, 0.6f));
    DrawText(dica2, LARGURA_TELA / 2 - MeasureText(dica2, 10) / 2, 454, 10, GRAY);
}
