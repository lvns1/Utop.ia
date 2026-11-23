// Sala inicial (a Luci e a porta do corredor).
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../../include/core/fonte.h"
#include "../../include/core/mapas.h"
#include "../../include/telas/jogo.h"

void TelaSalaAtualizar(Jogo *jg)
{
    jg->companionAngulo += jg->dt * 2.5f;
    jg->pertoDaLuci = false;

    // -------------------------------------------------
    // CONVERSA EM ANDAMENTO (o personagem fica parado)
    // -------------------------------------------------

    if (jg->dlg.estado != DLG_NENHUM) {

        ResultadoDialogo r = DialogoAtualizar(&jg->dlg);

        // Terminou a conversa com a Luci: ela passa a acompanhar o jogador
        if (r == RES_FIM && jg->dlg.objeto == OBJ_LUCI) {
            jg->luciAcompanhando = true;
            jg->pedirAutoSave = true;         // AUTO SAVE: missão começou
        }
    }

    else {

        // -------------------------------------------------
        // MOVIMENTAÇÃO DO PERSONAGEM
        // -------------------------------------------------

        Vector2 posAnterior = jg->playerPos;

        jg->direcaoAtual = MoverJogador(&jg->playerPos, jg->direcaoAtual);

        // Colisão com a Luci (não deixa atravessar)
        if (!jg->luciAcompanhando && CheckCollisionRecs(HitboxJogador(jg->playerPos), CorpoLuciSala(jg->luciHitbox)))
            jg->playerPos = posAnterior;

        // -------------------------------------------------
        // LIMITES DO MAPA
        // -------------------------------------------------

        // Paredes invisíveis (só valem com a imagem do mapa)
#if PAREDES_INVISIVEIS_ATIVAS
        if (jg->texMapaSala.id > 0)
            ColidirComParedes(&jg->playerPos, posAnterior);
#endif

        // Limite final: a própria imagem do mapa (ou o cenário simples)
        Rectangle lim = (jg->texMapaSala.id > 0) ? AREA_MAPA_SALA : LIMITES_SALA_SIMPLES;

        if (jg->playerPos.x < lim.x)              jg->playerPos.x = lim.x;
        if (jg->playerPos.x > lim.x + lim.width)  jg->playerPos.x = lim.x + lim.width;
        if (jg->playerPos.y < lim.y)              jg->playerPos.y = lim.y;
        if (jg->playerPos.y > lim.y + lim.height) jg->playerPos.y = lim.y + lim.height;
        // -------------------------------------------------
        // INTERAÇÃO (tecla E)
        // -------------------------------------------------

        // pequeno retângulo na frente do jogador, na direção que ele está olhando
        Rectangle interacaoBox = { jg->playerPos.x - jg->interacaoAlcance / 2, jg->playerPos.y - jg->interacaoAlcance / 2, jg->interacaoAlcance, jg->interacaoAlcance };

        switch (jg->direcaoAtual)
        {
            case DIR_FRENTE:   interacaoBox.y += TAM_SPRITE / 2; break; // olhando pra baixo
            case DIR_COSTAS:   interacaoBox.y -= TAM_SPRITE / 2; break; // olhando pra cima
            case DIR_DIREITA:  interacaoBox.x += TAM_SPRITE / 2; break;
            case DIR_ESQUERDA: interacaoBox.x -= TAM_SPRITE / 2; break;
            case DIR_FRENTE_DIREITA:  interacaoBox.x += TAM_SPRITE / 2; interacaoBox.y += TAM_SPRITE / 2; break;
            case DIR_FRENTE_ESQUERDA: interacaoBox.x -= TAM_SPRITE / 2; interacaoBox.y += TAM_SPRITE / 2; break;
            case DIR_COSTAS_DIREITA:  interacaoBox.x += TAM_SPRITE / 2; interacaoBox.y -= TAM_SPRITE / 2; break;
            case DIR_COSTAS_ESQUERDA: interacaoBox.x -= TAM_SPRITE / 2; interacaoBox.y -= TAM_SPRITE / 2; break;
            default: break;
        }

        jg->pertoDaLuci = !jg->luciAcompanhando && CheckCollisionRecs(interacaoBox, jg->luciHitbox);
        bool pertoDaPorta = CheckCollisionRecs(AreaJogador(jg->playerPos), ZONA_PORTA_SALA);

        if (IsKeyPressed(KEY_E)) {

            if (jg->pertoDaLuci) {

                DialogoIniciar(&jg->dlg, &dialogoLuciSala, OBJ_LUCI);
                jg->pertoDaLuci = false;

            } else if (pertoDaPorta) {

                // A porta de cima leva ao corredor (depois de falar com a Luci)
                if (jg->luciAcompanhando)
                    IniciarTransicao(&jg->transicao, CORREDOR, DURACAO_FADE);
                else
                    DialogoIniciar(&jg->dlg, &dialogoPortaSalaTrancada, OBJ_NENHUM);

            } else if (jg->luciAcompanhando) {

                g_luciPedirDica = true;         // nada por perto: a Luci repete a dica
            }
        }
    }
}

void TelaSalaDesenhar(Jogo *jg)
{
    ClearBackground((Color){8, 6, 18, 255});

    // -------------------------------------------------
    // MAPA SIMPLES
    // -------------------------------------------------

    Rectangle mapa = {50, 50, 700, 500};
    bool mapaCarregado = (jg->texMapaSala.id > 0);

    if (mapaCarregado) {

        // Arte do mapa, ajustada à área AREA_MAPA_SALA
        DrawTexturePro(
            jg->texMapaSala,
            (Rectangle){ 0, 0, (float)jg->texMapaSala.width, (float)jg->texMapaSala.height },
            AREA_MAPA_SALA,
            (Vector2){ 0, 0 },
            0.0f,
            WHITE
        );

    } else {

        // Sem a imagem: cenário simples
        DrawRectangleRec(mapa, (Color){18, 15, 30, 255});      // chão
        DrawRectangleLinesEx(mapa, 4, Fade(PURPLE, 0.55f));    // bordas
    }

    // -------------------------------------------------
    // PORTA (parede de cima)
    // -------------------------------------------------

    bool salaLiberada = jg->luciAcompanhando;

    if (!mapaCarregado) {

        DrawRectangleRec(PORTA_SALA, (Color){35, 25, 55, 255});
        DrawRectangleLinesEx(PORTA_SALA, 3, salaLiberada ? PURPLE : MAROON);

        // Maçaneta
        DrawCircle((int)(PORTA_SALA.x + PORTA_SALA.width - 12), (int)(PORTA_SALA.y + PORTA_SALA.height / 2), 4, GOLD);

    }
    // Com a imagem do mapa, o arco da porta já está desenhado nela:
    // nada é pintado por cima (trancada ou não)

    bool livre = (jg->dlg.estado == DLG_NENHUM && !jg->transicao.ativa);

    if (livre && CheckCollisionRecs(AreaJogador(jg->playerPos), ZONA_PORTA_SALA)) {

        DrawText(
            "PRESSIONE E",
            (int)(PORTA_SALA.x + PORTA_SALA.width) + 10,
            (int)PORTA_SALA.y + 12,
            12,
            GOLD
        );
    }

    // -------------------------------------------------
    // LUCI (NPC enquanto não conversou)
    // -------------------------------------------------

    if (!jg->luciAcompanhando) {

        // Mesmo visual de quando ela acompanha o jogador, flutuando parada
        Vector2 pLuci = CentroLuciSala(jg->luciHitbox, jg->tempo);

        DesenharLuciTamanho(pLuci, ALTURA_LUCI_SALA);
        DrawText("Luci", (int)pLuci.x - MeasureText("Luci", 10) / 2, (int)(pLuci.y - ALTURA_LUCI_SALA / 2) - 14, 10, Fade(SKYBLUE, 0.8f));
    }

    // -------------------------------------------------
    // PERSONAGEM
    // -------------------------------------------------

    DesenharJogadorComLuci(jg->spritesJogador, jg->playerPos, jg->direcaoAtual, jg->animJogador.quadro,
                           jg->luciAcompanhando, jg->companionAngulo);

    // Paredes invisíveis e pés do personagem (para calibrar)
    // Desligue em MOSTRAR_PAREDES_INVISIVEIS
#if MOSTRAR_PAREDES_INVISIVEIS
    if (mapaCarregado) {

        for (int i = 0; i < NUM_PAREDES_SALA; i++) {

            DrawRectangleRec(PAREDES_SALA[i], Fade(RED, 0.25f));
            DrawRectangleLinesEx(PAREDES_SALA[i], 1, RED);
        }

        DrawRectangleLinesEx(PesJogador(jg->playerPos), 1, YELLOW);
    }
#endif

    // -------------------------------------------------
    // INSTRUÇÕES E HUD
    // -------------------------------------------------

    // Com o mapa quadrado, o HUD fica nas faixas pretas dos lados
    int hudEsq = mapaCarregado ? 8   : 65;
    int hudDir = mapaCarregado ? 792 : 700;
    int hudFonte = mapaCarregado ? 10 : 12;

    DrawText(mapaCarregado ? "WASD - MOVER" : "W A S D  -  MOVIMENTAR", hudEsq, 65, hudFonte, LIGHTGRAY);

    DrawText(jg->nickname, hudDir - MeasureText(jg->nickname, 14), 65, 14, WHITE);

    const char *textoPontos = TextFormat("PONTOS: %d", jg->progresso.pontos);
    DrawText(textoPontos, hudDir - MeasureText(textoPontos, 12), 84, 12, GOLD);

    if (livre && jg->pertoDaLuci) {

        // aviso de interação
        const char *aviso = "Aperte E para falar com a Luci";
        DrawText(aviso, LARGURA_TELA / 2 - MeasureText(aviso, 20) / 2, ALTURA_TELA - 40, 20, RAYWHITE);
    }

    // Ambiente, objetivo e marcador
    DesenharGlitchAmbiente(jg->progresso.alucinacao, jg->tempo);
    DesenharObjetivoHUD(&jg->objetivo);

    // Conversa por cima de tudo
    DialogoDesenhar(&jg->dlg);
    DesenharCartao(&jg->cartao, jg->tempo);
    DesenharTelaRanking(&jg->telaRanking, &jg->ranking, jg->tempo);
    DesenharAvisoSave(jg->avisoSave, jg->saveFalhou, jg->tempo);
}
