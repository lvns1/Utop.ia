// Telas do menu principal e de opções.
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../../include/core/fonte.h"
#include "../../include/core/mapas.h"
#include "../../include/telas/jogo.h"
#include "../../include/telas/historico_luci.h"
#include "../../include/entidades/jogador.h"

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
                HistoricoLuciLimpar();          // o histórico é desta sessão de jogo (não vai para o save)
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
            HistoricoLuciLimpar();
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
    // VÍDEO muda a janela; EFEITOS e SONS já valem na hora: os dois só precisam ser salvos
    MudancaOpcoes mudou = AtualizarOpcoes(&jg->menuOpcoes, &jg->exibicao);

    if (mudou == OPCOES_VIDEO)
        AplicarExibicao(jg->exibicao);
    if (mudou != OPCOES_NADA)
        SalvarConfigExibicao(jg->exibicao);

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

// ---------- Fundo animado do menu ----------
// Tudo sai do tempo (sem estado guardado): colunas de "dados" caindo, partículas subindo e um
// brilho roxo embaixo. Fica bem apagado, atrás da logo e dos botões.
static float RuidoMenu(int a, int b)
{
    unsigned int h = (unsigned int)a * 374761393u + (unsigned int)b * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 0xFFFF) / 65535.0f;
}

static void DesenharFundoMenu(float t)
{
    static const char SIMBOLOS[] = "01010110ABCDEF0123456789";
    const int colunas = 26;

    DrawRectangleGradientV(0, ALTURA_TELA - 220, LARGURA_TELA, 220, Fade(PURPLE, 0.0f), Fade(PURPLE, 0.16f));

    // Chuva de dados: cada coluna tem a sua velocidade; a "cabeça" é mais clara e o rastro some
    for (int c = 0; c < colunas; c++) {
        float vel = 28.0f + 46.0f * RuidoMenu(c, 1);
        float x = 14.0f + c * 30.0f + 8.0f * RuidoMenu(c, 2);
        float cabeca = fmodf(t * vel + 900.0f * RuidoMenu(c, 3), ALTURA_TELA + 240.0f) - 120.0f;
        Color cor = (RuidoMenu(c, 4) < 0.5f) ? (Color){ 170, 110, 255, 255 } : (Color){ 0, 200, 255, 255 };
        int troca = (int)(t * (3.0f + 4.0f * RuidoMenu(c, 5)));      // os símbolos mudam de vez em quando

        for (int k = 0; k < 9; k++) {
            float y = cabeca - k * 15.0f;
            if (y < -14.0f || y > ALTURA_TELA) continue;
            char s[2] = { SIMBOLOS[(int)(RuidoMenu(c * 31 + k, troca) * (sizeof(SIMBOLOS) - 1))], '\0' };
            float alfa = (k == 0) ? 0.30f : 0.20f * (1.0f - k / 9.0f);
            DrawText(s, (int)x, (int)y, 12, Fade(cor, alfa));
        }
    }

    // Partículas subindo devagar, balançando um pouco
    for (int i = 0; i < 40; i++) {
        float vel = 10.0f + 22.0f * RuidoMenu(i, 7);
        float y = ALTURA_TELA + 20.0f - fmodf(t * vel + 700.0f * RuidoMenu(i, 8), ALTURA_TELA + 60.0f);
        float x = LARGURA_TELA * RuidoMenu(i, 9) + 10.0f * sinf(t * 0.8f + i);
        float tam = 2.0f + 2.0f * RuidoMenu(i, 10);
        Color cor = (i % 3 == 0) ? SKYBLUE : (Color){ 200, 122, 255, 255 };
        DrawRectangleV((Vector2){ x, y }, (Vector2){ tam, tam }, Fade(cor, 0.15f + 0.3f * RuidoMenu(i, 11)));
    }
}

// Botão do menu com cara de neon: escuro, borda na cor dele e brilho quando está marcado.
// foco vai de 0 a 1 (sobe e desce suave quando a seleção muda).
static void DesenharBotaoNeon(Rectangle r, const char *texto, Color cor, float foco, float t)
{
    for (int k = 3; k >= 1; k--)
        if (foco > 0.01f)
            DrawRectangleLinesEx((Rectangle){ r.x - k * 2.0f, r.y - k * 2.0f, r.width + k * 4.0f, r.height + k * 4.0f }, 2, Fade(cor, 0.10f * foco / k));

    DrawRectangleRec(r, Fade((Color){ 10, 8, 22, 255 }, 0.85f));
    DrawRectangleRec(r, Fade(cor, 0.06f + 0.16f * foco));
    DrawRectangleLinesEx(r, 2, Fade(cor, 0.45f + 0.55f * foco));

    // Cantos mais claros (cara de janela de terminal)
    float c = 8.0f;
    Color canto = Fade(cor, 0.6f + 0.4f * foco);
    DrawRectangle((int)r.x, (int)r.y, (int)c, 2, canto);                                 DrawRectangle((int)r.x, (int)r.y, 2, (int)c, canto);
    DrawRectangle((int)(r.x + r.width - c), (int)(r.y + r.height - 2), (int)c, 2, canto); DrawRectangle((int)(r.x + r.width - 2), (int)(r.y + r.height - c), 2, (int)c, canto);

    // Marcado: uma faixa clara passa pelo botão de vez em quando
    if (foco > 0.5f) {
        float p = fmodf(t * 0.7f, 1.6f);
        if (p < 1.0f) {
            float x = r.x + p * (r.width + 30.0f) - 30.0f;
            float x0 = fmaxf(x, r.x + 2), x1 = fminf(x + 18.0f, r.x + r.width - 2);
            if (x1 > x0) DrawRectangle((int)x0, (int)r.y + 2, (int)(x1 - x0), (int)r.height - 4, Fade(WHITE, 0.07f * foco));
        }
    }

    Color corTexto = ColorLerp(LIGHTGRAY, WHITE, foco);
    DesenharTextoBotao(r, texto, 20, corTexto);
}

void TelaMenuDesenhar(Jogo *jg)
{
    ClearBackground(jg->cor_de_fundo_menu);
    DesenharFundoMenu(jg->tempo);

    // Título original
    // A logo é escalada para caber acima dos botões (a imagem tem 615 px de largura e passava da tela).
    // LOGO_LARGURA = largura do desenho na tela; a logo fica centralizada nos botões.
    {
        const float LOGO_LARGURA = 470.0f;
        float esc = LOGO_LARGURA / (float)jg->imagem_titulo.width;
        Rectangle fonte = { 0, 0, (float)jg->imagem_titulo.width, (float)jg->imagem_titulo.height };
        float lx = 400.0f - jg->imagem_titulo.width * esc * 0.5f;
        float desloc = -30.0f;            // sobe a logo para não encostar nos botões
        // De vez em quando a logo "falha": cópias vermelha e ciano deslocadas por um instante
        float falha = fmodf(jg->tempo, 4.3f);
        if (falha < 0.12f) {
            float d = 3.0f + 3.0f * sinf(jg->tempo * 90.0f);
            DrawTexturePro(jg->imagem_titulo, fonte, (Rectangle){ lx - d, jg->y + desloc, jg->imagem_titulo.width * esc, jg->imagem_titulo.height * esc }, (Vector2){ 0, 0 }, 0.0f, Fade(RED, 0.45f));
            DrawTexturePro(jg->imagem_titulo, fonte, (Rectangle){ lx + d, jg->y + desloc, jg->imagem_titulo.width * esc, jg->imagem_titulo.height * esc }, (Vector2){ 0, 0 }, 0.0f, Fade(SKYBLUE, 0.45f));
        }
        DrawTexturePro(jg->imagem_titulo, fonte, (Rectangle){ lx, jg->y  + desloc, jg->imagem_titulo.width * esc, jg->imagem_titulo.height * esc }, (Vector2){ 0, 0 }, 0.0f, WHITE);
        DrawTexturePro(jg->imagem_titulo, fonte, (Rectangle){ lx, jg->y1 + desloc, jg->imagem_titulo.width * esc, jg->imagem_titulo.height * esc }, (Vector2){ 0, 0 }, 0.0f, Fade(PURPLE, 0.1f));
        DrawTexturePro(jg->imagem_titulo, fonte, (Rectangle){ lx, jg->y2 + desloc, jg->imagem_titulo.width * esc, jg->imagem_titulo.height * esc }, (Vector2){ 0, 0 }, 0.0f, Fade(PURPLE, 0.1f));
    }

    // Botões neon (cada um com a sua cor); o brilho do marcado sobe e desce suave
    static float focoAnim[4] = { 0 };
    static float luciY = -1.0f;
    const Rectangle botoes[4] = { jg->btnContinuar, jg->btnJogar, jg->btnOpcoes, jg->btnSair };
    const char *nomes[4] = { "CONTINUAR", "NOVO JOGO", "OPÇÕES", "SAIR" };
    const Color cores[4] = { { 200, 122, 255, 255 }, { 0, 228, 48, 255 }, { 102, 191, 255, 255 }, { 230, 41, 55, 255 } };
    float passo = fminf(GetFrameTime(), 0.1f) * 9.0f;

    for (int i = 0; i < 4; i++) {
        if (i == 0 && !jg->existeSave) continue;
        float alvo = (jg->focoMenu == i) ? 1.0f : 0.0f;
        focoAnim[i] += Clamp(alvo - focoAnim[i], -passo, passo);
        DesenharBotaoNeon(botoes[i], nomes[i], cores[i], focoAnim[i], jg->tempo);
    }

    // A Luci flutua ao lado do botão marcado e desliza quando a seleção muda
    if (jg->focoMenu >= 0 && jg->focoMenu < 4) {
        float alvoY = botoes[jg->focoMenu].y + botoes[jg->focoMenu].height / 2;
        if (luciY < 0.0f) luciY = alvoY;
        luciY += (alvoY - luciY) * fminf(1.0f, GetFrameTime() * 12.0f);
        DesenharLuciTamanho((Vector2){ botoes[jg->focoMenu].x - 34.0f, luciY + 3.0f * sinf(jg->tempo * 3.0f) }, 34.0f);
    }

    DesenharRankingMenu(&jg->ranking, jg->tempo);
}

void TelaOpcoesDesenhar(Jogo *jg)
{
    DesenharOpcoes(&jg->menuOpcoes, jg->exibicao, jg->cor_de_fundo_menu);
}
