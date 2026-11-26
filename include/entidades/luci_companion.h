// Luci companion: segue o Thomas, anima o voo, reage ao combate e mostra o caminho.
#ifndef UTOPIA_LUCI_COMPANION_H
#define UTOPIA_LUCI_COMPANION_H

#include <stdbool.h>
#include "raylib.h"
#include "raymath.h"
#include "../core/config.h"
#include "../core/tipos.h"

// =============================================================
// LUCI COMPANION: movimentação, animação e reações
//   Ela segue o Thomas com atraso (mola), paira no ombro quando ele para (com uma volta lenta de vez
//   em quando), fica longe da linha da mira, reage ao combate, faz o Pulso (Q) e o Scanner (H) e voa
//   até o objetivo para mostrar o caminho. E sem nada por perto: ela repete a dica.
//   DESLIGAR: troque o 1 por 0 no que quiser tirar. LUCI_MOV_NOVO 0 volta à órbita fixa antiga.
// =============================================================
#define LUCI_MOV_NOVO        1   // segue com atraso, paira no ombro parada, foge da linha da mira
#define LUCI_ANIM_INCLINAR   1   // inclina para onde voa e estica ao acelerar
#define LUCI_ANIM_RASTRO     1   // pó de luz atrás dela quando voa rápido
#define LUCI_ANIM_SOMBRA     1   // sombrinha no chão
#define LUCI_ANIM_ASAS       1   // as asas batem mais rápido acelerando e devagar pairando
#define LUCI_REACOES         1   // susto (dano), giro (esquiva), comemoração (parry), pouca vida, Pulso (Q) e Scanner (H)
#define LUCI_GUIA            1   // voa até o objetivo e volta; E sem nada por perto repete a dica

#define LUCI_RASTRO_N        12
#define LUCI_PULSO_TEMPO     0.45f
#define LUCI_SCANNER_TEMPO   0.9f
#define LUCI_GUIA_TEMPO      2.8f

typedef enum { LC_SEGUIR, LC_GUIA, LC_PULSO, LC_SCANNER } AcaoLuciComp;

typedef struct {
    bool iniciada;
    Vector2 pos, vel;
    Vector2 jogAnt;             // onde o Thomas estava no quadro anterior
    Vector2 dirAndar;           // a última direção em que ele andou
    float parado;               // há quanto tempo ele está parado
    float lado;                 // -1 = ombro esquerdo, 1 = direito
    float orbita, voltando, cronoVolta;     // a volta lenta quando ele está parado
    float angAntigo;            // LUCI_MOV_NOVO 0: a órbita antiga
    float faseAsas, ampAsas;
    float inclina, estica, giro;            // graus, 0 a 1, graus
    float susto, comemora, pertoVida, falando;
    AcaoLuciComp acao;
    float tempoAcao;
    Vector2 alvo;
    bool temAlvoAnt;
    Vector2 alvoAnt;
    float esperaGuia;
    bool guiouParado;           // já foi mostrar o caminho nesta parada
    Vector2 rastro[LUCI_RASTRO_N];
    float vidaRastro[LUCI_RASTRO_N];
    float cronoRastro;
    int iRastro;
    bool atras;                 // desenhada antes do Thomas (passando atrás dele)
} LuciComp;

// ----- Variáveis compartilhadas -----
extern LuciComp g_luciComp;
extern bool g_luciPedirDica;
extern double g_repetirFalaLuciAte;
extern bool g_luciGuiaSoParado;

// ----- Funções -----
void LuciCompSusto(Vector2 de);
void LuciCompGirar(void);
void LuciCompComemorar(void);
void LuciCompAcao(AcaoLuciComp a, float tempo);
void LuciCompAtualizar(Vector2 jog, Vector2 mira, bool temAlvo, Vector2 alvo, float vidaFrac, float dt);
void DesenharJogadorComLuci(const SpriteDirecao sprites[NUM_DIRECOES], Vector2 pos, Direcao direcao, int quadro, bool comLuci, float anguloLuci);
Vector2 CentroLuciSala(Rectangle hitbox, float tempo);
Rectangle CorpoLuciSala(Rectangle hitbox);

#endif // UTOPIA_LUCI_COMPANION_H
