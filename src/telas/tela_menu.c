// Telas do menu principal e de opções.
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../../include/core/fonte.h"
#include "../../include/core/mapas.h"
#include "../../include/telas/jogo.h"

void TelaMenuAtualizar(Jogo *jg)
{
    // Teclado (W/S ou setas + ENTER) e mouse (passar por cima + clique) nos mesmos botões
    Rectangle botoesMenu[4] = { jg->btnContinuar, jg->btnJogar, jg->btnOpcoes, jg->btnSair };
    int primeiroBotao = jg->existeSave ? 0 : 1;          // sem save, o CONTINUAR não aparece
    int numBotoes = 4 - primeiroBotao;
    int acionado = -1;

    if (jg->focoMenu < primeiroBotao || jg->focoMenu > 3)
        jg->focoMenu = primeiroBotao;

    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))
        jg->focoMenu = primeiroBotao + (jg->focoMenu - primeiroBotao + numBotoes - 1) % numBotoes;

    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN))
        jg->focoMenu = primeiroBotao + (jg->focoMenu - primeiroBotao + 1) % numBotoes;

    for (int i = primeiroBotao; i < 4; i++) {

        if (!CheckCollisionPointRec(jg->mouse, botoesMenu[i]))
            continue;

        if (MouseMexeu())
            jg->focoMenu = i;

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            jg->focoMenu = i;
            acionado = i;
        }
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_SPACE))
        acionado = jg->focoMenu;

    // CONTINUAR (carrega o save)
    if (jg->existeSave && jg->focoMenu == 0) {

        jg->corBtnContinuar = PURPLE;

        if (acionado == 0) {

            if (LerSave(&jg->saveLido)) {

                memcpy(jg->nickname, jg->saveLido.nickname, 4);
                jg->nicknamePos = 3;
                jg->luciAcompanhando = jg->saveLido.luciAcompanhando;
                jg->progresso = jg->saveLido.progresso;
                jg->progBiblioteca = jg->saveLido.biblioteca;
                jg->playerPos = (Vector2){ jg->saveLido.x, jg->saveLido.y };
                jg->direcaoAtual = (Direcao)jg->saveLido.direcao;
                jg->dlg = (SessaoDialogo){0};
                jg->cartao = (CartaoResumo){0};
                jg->estabilidadeLuci = jg->saveLido.estabilidadeLuci;
                jg->ajudasPedidas = jg->saveLido.ajudasPedidas;
                jg->chefeVencido = jg->saveLido.chefeVencido;
                jg->chefeAnunciado = (ContarConcluidos(&jg->progresso) == TOTAL_MONITORES);   // perdeu antes: volta pela porta
                jg->idPartida = (jg->saveLido.idPartida != 0) ? jg->saveLido.idPartida : NovoIdPartida();
                jg->telaRanking = (TelaRanking){0};
                jg->animJogador = (AnimacaoJogador){0};
                jg->tela = (TelaAtual)jg->saveLido.sala;

                if (jg->tela == BIBLIOTECA) {
                    RogueIniciar(&jg->biblioteca, &jg->progBiblioteca);
                    jg->biblioteca.estabilidade = &jg->estabilidadeLuci;
                    jg->playerPos = SPAWN_CORREDOR_VOLTA;      // se sair, volta pela porta da direita do corredor
                }

                // Entra clareando a tela
                jg->transicao = (Transicao){ .ativa = true, .alfa = 1.0f, .fase = 1, .duracao = DURACAO_FADE, .destino = jg->tela };

            } else {

                // Save sumiu ou corrompeu desde que o menu abriu
                jg->existeSave = false;
            }
        }
    }

    // JOGAR
    if (jg->focoMenu == 1) {

        jg->corBtnJogar = GREEN;

        if (acionado == 1) {

            jg->tela = INTRO;
            jg->etapaIntro = 0;

            // Novo jogo: zera tudo
            jg->progresso = (ProgressoCorredor){0};
            jg->progBiblioteca = (ProgressoBiblioteca){0};
            jg->cartao = (CartaoResumo){0};
            jg->estabilidadeLuci = ESTABILIDADE_MAX_LUCI;
            jg->ajudasPedidas = 0;
            jg->chefeVencido = false;
            jg->chefeAnunciado = false;
            jg->idPartida = NovoIdPartida();
            jg->telaRanking = (TelaRanking){0};
            jg->dlg = (SessaoDialogo){0};
            jg->luciAcompanhando = false;
            jg->playerPos = SPAWN_SALA;
            jg->direcaoAtual = DIR_FRENTE;
            jg->animJogador = (AnimacaoJogador){0};

            jg->nickname[0] = jg->nickname[1] = jg->nickname[2] = '_';
            jg->nickname[3] = '\0';
            jg->nicknamePos = 0;
        }
    }

    // OPÇÕES
    if (jg->focoMenu == 2) {

        jg->corBtnOpcoes = LIGHTGRAY;

        if (acionado == 2) {
            jg->tela = OPCOES;
            jg->menuOpcoes = (MenuOpcoes){ .foco = CAMPO_MODO, .aberta = -1 };
        }
    }

    // SAIR
    if (jg->focoMenu == 3) {

        jg->corBtnSair = RED;

        if (acionado == 3)
            jg->fecharJogo = true;
    }
}

void TelaOpcoesAtualizar(Jogo *jg)
{
    if (AtualizarOpcoes(&jg->menuOpcoes, &jg->exibicao)) {

        AplicarExibicao(jg->exibicao);
        SalvarConfigExibicao(jg->exibicao);
    }

    // Botão VOLTAR AO MENU (clique ou ENTER nele)
    if (jg->menuOpcoes.sair) {
        jg->menuOpcoes.sair = false;
        jg->tela = MENU;
    }

    // BACKSPACE: fecha a lista aberta ou volta ao menu (igual ao ESC)
    if (IsKeyPressed(KEY_BACKSPACE)) {

        if (jg->menuOpcoes.aberta >= 0)
            jg->menuOpcoes.aberta = -1;
        else
            jg->tela = MENU;
    }
}

void TelaMenuDesenhar(Jogo *jg)
{
    ClearBackground(jg->cor_de_fundo_menu);

    // Título original
    // A logo é escalada para caber acima dos botões (a imagem tem 615 px de largura e passava da tela).
    // LOGO_LARGURA = largura do desenho na tela; a logo fica centralizada nos botões.
    {
        const float LOGO_LARGURA = 470.0f;
        float esc = LOGO_LARGURA / (float)jg->imagem_titulo.width;
        Rectangle fonte = { 0, 0, (float)jg->imagem_titulo.width, (float)jg->imagem_titulo.height };
        float lx = 400.0f - jg->imagem_titulo.width * esc * 0.5f;
        float desloc = -30.0f;            // sobe a logo para não encostar nos botões
        DrawTexturePro(jg->imagem_titulo, fonte, (Rectangle){ lx, jg->y  + desloc, jg->imagem_titulo.width * esc, jg->imagem_titulo.height * esc }, (Vector2){ 0, 0 }, 0.0f, WHITE);
        DrawTexturePro(jg->imagem_titulo, fonte, (Rectangle){ lx, jg->y1 + desloc, jg->imagem_titulo.width * esc, jg->imagem_titulo.height * esc }, (Vector2){ 0, 0 }, 0.0f, Fade(PURPLE, 0.1f));
        DrawTexturePro(jg->imagem_titulo, fonte, (Rectangle){ lx, jg->y2 + desloc, jg->imagem_titulo.width * esc, jg->imagem_titulo.height * esc }, (Vector2){ 0, 0 }, 0.0f, Fade(PURPLE, 0.1f));
    }

    // Botões
    if (jg->existeSave) {
        DrawRectangleRec(jg->btnContinuar, jg->corBtnContinuar);
        DesenharTextoBotao(jg->btnContinuar, "CONTINUAR", 20, WHITE);
    }

    DrawRectangleRec(jg->btnJogar, jg->corBtnJogar);
    DrawRectangleRec(jg->btnOpcoes, jg->corBtnOpcoes);
    DrawRectangleRec(jg->btnSair, jg->corBtnSair);

    // Textos
    DesenharTextoBotao(jg->btnJogar, "NOVO JOGO", 20, WHITE);
    DesenharTextoBotao(jg->btnOpcoes, "OPÇÕES", 20, WHITE);
    DesenharTextoBotao(jg->btnSair, "SAIR", 20, WHITE);

    DesenharRankingMenu(&jg->ranking, jg->tempo);
}

void TelaOpcoesDesenhar(Jogo *jg)
{
    DesenharOpcoes(&jg->menuOpcoes, jg->exibicao, jg->cor_de_fundo_menu);
}
