#include "dialogue.h"
#include "quiz.h"
#include "../utils/text.h"
#include "../utils/effects.h"
#include <string.h>
#include <math.h>

void DialogoMostrarTexto(SessaoDialogo *d, const char *texto, int larguraMax, int tamFonte) {
    QuebrarTexto(texto, larguraMax, tamFonte, d->texto, (int)sizeof(d->texto));
    d->visiveis = 0.0f;
}

void DialogoIniciar(SessaoDialogo *d, const Dialogo *conteudo, int objeto) {
    d->conteudo = *conteudo;
    d->objeto = objeto;
    d->pagina = 0;
    d->respondeu = false;
    d->acertou = false;
    d->estado = DLG_PAGINAS;
    d->corBorda = WHITE;
    d->pontosGanhos = 0;

    DialogoMostrarTexto(d, d->conteudo.paginas[0], LARGURA_TEXTO_CAIXA, FONTE_CAIXA);
}

void DialogoFalaAvulsa(SessaoDialogo *d, const char *texto) {
    Dialogo fala = {0};
    fala.paginas[0] = texto;
    fala.numPaginas = 1;

    DialogoIniciar(d, &fala, OBJ_NENHUM);
}

void DialogoMostrarFeedback(SessaoDialogo *d, const char *texto, Color borda, int pontos) {
    d->estado = DLG_FEEDBACK;
    d->corBorda = borda;
    d->pontosGanhos = pontos;

    DialogoMostrarTexto(d, texto, LARGURA_TEXTO_CAIXA, FONTE_CAIXA);
}

ResultadoDialogo DialogoAtualizar(SessaoDialogo *d) {
    bool avancar = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE);
    int total = (int)strlen(d->texto);

    if (d->visiveis < (float)total) {
        d->visiveis += VELOCIDADE_TEXTO * GetFrameTime();
        if (d->visiveis > (float)total)
            d->visiveis = (float)total;
    }

    bool terminou = (d->visiveis >= (float)total);

    switch (d->estado) {
        case DLG_PAGINAS: {
            if (!avancar) break;
            if (!terminou) {
                d->visiveis = (float)total;
                break;
            }
            d->pagina++;
            if (d->pagina < d->conteudo.numPaginas) {
                DialogoMostrarTexto(d, d->conteudo.paginas[d->pagina], LARGURA_TEXTO_CAIXA, FONTE_CAIXA);
            } else if (d->conteudo.pergunta != NULL) {
                DialogoMostrarPergunta(d);
            } else {
                d->estado = DLG_NENHUM;
                return RES_FIM;
            }
        } break;

        case DLG_ESCOLHA: {
            if (!terminou) {
                if (avancar) d->visiveis = (float)total;
                break;
            }

            d->tempoRestante -= GetFrameTime();
            if (d->tempoRestante <= 0.0f) {
                d->tempoRestante = 0.0f;
                d->respondeu = true;
                d->acertou = false;
                d->estado = DLG_NENHUM;
                return RES_TEMPO;
            }

            if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))
                d->selecao = (d->selecao + NUM_OPCOES - 1) % NUM_OPCOES;
            if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN))
                d->selecao = (d->selecao + 1) % NUM_OPCOES;

            if (TEMPO_QUIZ - d->tempoRestante < ATRASO_RESPOSTA)
                break;

            int escolha = -1;
            if (avancar) escolha = d->selecao;
            if (IsKeyPressed(KEY_ONE)   || IsKeyPressed(KEY_KP_1)) escolha = 0;
            if (IsKeyPressed(KEY_TWO)   || IsKeyPressed(KEY_KP_2)) escolha = 1;
            if (IsKeyPressed(KEY_THREE) || IsKeyPressed(KEY_KP_3)) escolha = 2;
            if (IsKeyPressed(KEY_FOUR)  || IsKeyPressed(KEY_KP_4)) escolha = 3;

            if (escolha >= 0) {
                d->selecao = escolha;
                d->respondeu = true;
                d->acertou = (escolha == d->conteudo.correta);

                if (d->acertou) {
                    DialogoMostrarFeedback(
                        d,
                        TextFormat("Luci: Isso mesmo! %s", d->conteudo.explicacao),
                        GREEN,
                        PONTOS_POR_ACERTO
                    );
                } else {
                    d->estado = DLG_NENHUM;
                    return RES_ERROU;
                }
            }
        } break;

        case DLG_FEEDBACK: {
            if (!avancar) break;
            if (!terminou) {
                d->visiveis = (float)total;
                break;
            }
            d->estado = DLG_NENHUM;
            return RES_FIM;
        } break;

        case DLG_NENHUM:
        default: break;
    }

    return RES_NADA;
}

void DialogoDesenhar(const SessaoDialogo *d) {
    if (d->estado == DLG_NENHUM) return;

    char parcial[768];
    bool terminou = TextoParcial(d->texto, d->visiveis, parcial, (int)sizeof(parcial));
    bool piscar = (((int)(GetTime() * 2.0)) % 2 == 0);

    if (d->estado == DLG_ESCOLHA) {
        DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, 0.7f));

        int alturaPainel = AlturaPainelPergunta(d);
        Rectangle painel = { 40, (float)((ALTURA_TELA - alturaPainel) / 2), 720, (float)alturaPainel };
        int px = (int)painel.x + 20;
        int py = (int)painel.y;

        DrawRectangleRec(painel, BLACK);
        DrawRectangleLinesEx(painel, 4, WHITE);

        DrawText(
            d->conteudo.titulo != NULL ? d->conteudo.titulo : "LUCI // PENSE ANTES DE ESCOLHER",
            px, py + 14, 10, GOLD
        );

        float fracao = d->tempoRestante / TEMPO_QUIZ;
        Color corTempo = (fracao > 0.5f) ? LIME : (fracao > 0.25f ? YELLOW : RED);
        if (fracao <= 0.25f && (((int)(GetTime() * 6.0)) % 2 == 0))
            corTempo = MAROON;

        DrawRectangle(px, py + 32, 620, 12, (Color){40, 40, 40, 255});
        DrawRectangle(px, py + 32, (int)(620 * fracao), 12, corTempo);
        DrawRectangleLines(px, py + 32, 620, 12, GRAY);

        DrawText(TextFormat("%2d s", (int)ceilf(d->tempoRestante)), px + 632, py + 31, 14, corTempo);
        DesenharTextoLinhas(parcial, px, py + 58, FONTE_PERGUNTA, 24, WHITE);

        if (terminou) {
            int y = py + 58 + ContarLinhas(d->texto) * 24 + 14;
            int espaco = d->fonteOpcao + 5;

            for (int i = 0; i < NUM_OPCOES; i++) {
                bool marcada = (i == d->selecao);
                if (marcada)
                    DesenharCoracao((float)px + 8, (float)y + d->fonteOpcao / 2.0f, 2, RED);

                int nl = DesenharTextoLinhas(
                    d->opcoesTexto[i], px + 24, y, d->fonteOpcao, espaco, marcada ? YELLOW : LIGHTGRAY
                );
                y += nl * espaco + 10;
            }

            DrawText(
                "W/S ou SETAS: escolher   |   ENTER: confirmar   |   1-4: atalho",
                px, (int)(painel.y + painel.height - 22), 10, GRAY
            );
        }
        return;
    }

    int alturaCaixa = 16 + ContarLinhas(d->texto) * 26 + 34;
    if (alturaCaixa < 150) alturaCaixa = 150;

    Rectangle caixa = { 50, (float)(ALTURA_TELA - 14 - alturaCaixa), 700, (float)alturaCaixa };
    Color corBorda = (d->estado == DLG_FEEDBACK) ? d->corBorda : WHITE;

    DrawRectangleRec(caixa, BLACK);
    DrawRectangleLinesEx(caixa, 4, corBorda);

    DesenharTextoLinhas(parcial, (int)caixa.x + 20, (int)caixa.y + 16, FONTE_CAIXA, 26, WHITE);

    if (terminou) {
        if (d->estado == DLG_FEEDBACK && d->pontosGanhos > 0) {
            DrawText(
                TextFormat("+%d PONTOS", d->pontosGanhos),
                (int)caixa.x + 20, (int)(caixa.y + caixa.height - 26), 14, GREEN
            );
        }
        if (piscar) {
            DrawText(
                "ENTER >",
                (int)(caixa.x + caixa.width - 90), (int)(caixa.y + caixa.height - 26), 12, GOLD
            );
        }
    }
}
