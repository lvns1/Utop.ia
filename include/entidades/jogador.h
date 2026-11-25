// Jogador e Luci: sprites, animação, desenho, emoções e fala da Luci.
#ifndef UTOPIA_JOGADOR_H
#define UTOPIA_JOGADOR_H

#include <stdbool.h>
#include "raylib.h"
#include "raymath.h"
#include "../core/config.h"
#include "../core/tipos.h"


// ---------- Luci ----------
// A arte fica em "new.luci.png" (pasta do repositório ou resources/sprites/luci/); sem ela,
// vale a antiga "luci.png" e, sem nenhuma, um quadradinho. O mesmo desenho é usado parada na
// sala, acompanhando o jogador, na intro, no Bloco 3 e na luta final (DesenharLuciTamanho).
// Enquanto ela FALA, a folha "sprites_new.luci.falando.png" (6 quadros) faz o brilho pulsar.

#define SPRITE_LUCI            "new.luci.png"
#define SPRITE_LUCI_ANTIGA     "luci.png"
#define SPRITE_LUCI_FALANDO    "sprites_new.luci.falando.png"
#define ALTURA_TEXTURA_LUCI    160      // as artes (bem maiores que a tela) são reduzidas para esta altura ao carregar
#define MAX_QUADROS_FALA_LUCI  8        // a folha tem 6 quadros
#define FPS_FALA_LUCI          10.0f    // ritmo do brilho da fala (vai e volta pelos quadros)
#define ALTURA_LUCI        18.0f     // Luci acompanhando o jogador: pequenina (AJUSTE AQUI)
#define ALTURA_LUCI_SALA   24.0f     // Luci parada na sala inicial (um pouco maior, para ser encontrada)
#define RAIO_ORBITA_LUCI   26.0f     // distância da volta que ela dá em torno do personagem

// Emoções da Luci sobre a cabeça. O main atualiza estes valores a cada frame.
typedef enum { EMOCAO_NENHUMA, EMOCAO_ALEGRE, EMOCAO_DUVIDA, EMOCAO_ALERTA } EmocaoLuci;

// ----- Variáveis compartilhadas -----
extern bool luciFalando;
extern float tempoFalaLuci;
extern EmocaoLuci emocaoLuci;
extern float tempoEmocaoLuci;
extern float brilhoLuci;
extern float instabilidadeLuci;
extern float g_alfaLuci;
extern float g_luciGiro;
extern float g_luciEsticaX, g_luciEsticaY;
extern float g_luciAsasFase, g_luciAsasAmp;

// ----- Funções -----
Direcao MoverJogador(Vector2 *pos, Direcao direcaoAtual);
Rectangle HitboxJogador(Vector2 p);
Rectangle AreaJogador(Vector2 p);
Rectangle PesJogador(Vector2 p);
void ColidirComLista(Vector2 *pos, Vector2 posAnterior, const Rectangle *paredes, int n);
void ColidirComParedes(Vector2 *pos, Vector2 posAnterior);
SpriteDirecao CarregarSprite(const char *folha, const char *antigo, bool espelharAntigo);
SpriteDirecao CarregarSpriteDiagonal(const char *folha, SpriteDirecao reserva);
void AtualizarAnimacao(AnimacaoJogador *a, bool andando, float dt);
void DesenharJogador(const SpriteDirecao sprites[NUM_DIRECOES], Vector2 pos, Direcao direcao, int quadro);
void RitmoPassoJogador(float fator);   // passo mais rápido (velocidade maior); vale para a próxima AtualizarAnimacao
void JogadorReagirDano(void);          // fica vermelho, treme e achata
void JogadorReagirEsquiva(void);       // estica na esquiva
void MostrarEmocaoLuci(EmocaoLuci e, float segundos);
void CarregarLuci(void);
void DescarregarLuci(void);
void AtualizarFalaLuci(bool falando, float dt);
bool TextoDeFalaDaLuci(const char *t);
bool DialogoFalaDaLuci(const SessaoDialogo *d);
void DesenharLuciTamanho(Vector2 p, float altura);

#endif // UTOPIA_JOGADOR_H
