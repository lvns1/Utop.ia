// Som e música: síntese procedural, grupos de som (variações), BGM e ambientes.
#ifndef UTOPIA_AUDIO_H
#define UTOPIA_AUDIO_H

#include <stdbool.h>
#include "raylib.h"
#include "raymath.h"
#include "../core/config.h"
#include "../core/tipos.h"


// =============================================================
// SONS 2.0 — VARIAÇÕES, ARMAS E AMBIENTES DOS MAPAS
//
//  Por que: com um único Sound por efeito, o mesmo tiro soava igual
//  centenas de vezes por missão. Agora:
//    • cada efeito tem 3 VARIAÇÕES geradas com parâmetros um pouco
//      diferentes, sorteadas sem repetir a anterior, + leve variação de
//      tom a cada disparo e 3 vozes (aliases) para os sons se sobreporem
//      em vez de se cortarem;
//    • cada ARMA tem a sua assinatura, tirada do desenho e da cor dela;
//    • cada MAPA tem um ambiente em loop (sob a música), tirado da arte:
//        mapa1 (sala da Luci)  pedra azul + telas ciano + trilhas de circuito
//                              → zumbido de servidor, gotas na pedra, telas
//        mapa2 (corredor)      corredor longo de terminais
//                              → relés, chiado de CRT, conversa de dados, eco longo
//        Biblioteca dos Ecos   salas fundas → vento de caverna e ecos distantes
//        mapa3 (arena da Luci) pedra roxa, névoa, placas expostas
//                              → drone grave em batimento, falhas de sinal, pulsação
//  Tudo continua gerado por código (nenhum arquivo novo).
// =============================================================

#define TAXA_SOM        44100
#define MAX_VAR_SOM     3
#define VOZES_EXTRA     2          // aliases por variação (3 sons iguais podem tocar juntos)

typedef struct {
    Sound v[MAX_VAR_SOM];
    Sound alias[MAX_VAR_SOM][VOZES_EXTRA];
    int n;                         // variações carregadas
    int ultima;                    // última variação tocada (não repete em seguida)
    int voz;                       // próxima voz (0 = original, 1.. = alias)
    float jitter;                  // variação de tom por disparo (0.04 = ±4%)
} GrupoSom;

// --- buffer de síntese em float ---
typedef struct { float *x; int n; } BufSom;

// Filtro de estado variável (Chamberlin): passa-baixa, passa-banda e passa-alta de uma vez
typedef struct { float lp, bp; } Svf;

// =============================================================
// AMBIENTES DOS MAPAS (loops de 8 s, sem emenda, tocados como Music da memória)
// =============================================================
typedef enum { AMB_NENHUM = -1, AMB_SALA, AMB_CORREDOR, AMB_BIBLIOTECA, AMB_LUCI, NUM_AMBIENTES } TipoAmbiente;

#define AMB_SEGUNDOS    8.0f
#define AMB_CAUDA       1.5f        // gerado a mais e cruzado com o começo (o loop não "pula")

// =============================================================
// GRUPOS: carregar, tocar e descarregar
// =============================================================
typedef Sound (*ReceitaSom)(int variacao);

// --- Bancos ---
// Ordem igual a TipoArma (o enum fica no código da Biblioteca)
#define NUM_SONS_ARMA 10
#define NUM_GRUPOS_GERAIS ((int)(sizeof(GRUPOS_GERAIS) / sizeof(GRUPOS_GERAIS[0])))
#define NUM_GRUPOS_LUCI ((int)(sizeof(GRUPOS_LUCI) / sizeof(GRUPOS_LUCI[0])))
#define NUM_GRUPOS_INI ((int)(sizeof(GRUPOS_INI) / sizeof(GRUPOS_INI[0])))
#define NUM_GRUPOS_CHEFES ((int)(sizeof(GRUPOS_CHEFES) / sizeof(GRUPOS_CHEFES[0])))
#define NUM_GRUPOS_B1 ((int)(sizeof(GRUPOS_B1) / sizeof(GRUPOS_B1[0])))
#define NUM_GRUPOS_JOG ((int)(sizeof(GRUPOS_JOG) / sizeof(GRUPOS_JOG[0])))

// ----- Variáveis compartilhadas -----
extern Sound sfxAcerto, sfxErro, sfxDano, sfxPegar, sfxSelecao, sfxTiro;
extern Sound sfxAvisoParry, sfxParryPerfeito, sfxEstatica, sfxDedutivo;
extern Music bgmMenu;
extern Music bgmSala;
extern Music bgmCorredor;
extern Music bgmBatalha;
extern Music bgmMestre;
extern Music bgmBiblioteca;
extern Music bgmGameover;
extern Music bgmAtrio;
extern Music bgmLuci;
extern Music bgmLuciChefe[3];
extern Music bgmAlucinador[2];
extern GrupoSom gArma[NUM_SONS_ARMA];
extern GrupoSom gVetorCarga, gOrbeImplosao, gFragDourado, gRaioColuna, gMorteInimigo;
extern GrupoSom gLuciFio, gLuciCorrupcao, gLuciExplosao, gLuciTelegrafia, gLuciInvestida,
                gLuciOrbe, gLuciRebatido, gLuciVarredura, gLuciOnda, gLuciRacha, gLuciQueda,
                gLuciColapso, gLuciRenascer, gLuciVapor;
extern GrupoSom *const GRUPOS_LUCI[14];
extern GrupoSom gIniBote, gIniLeque, gIniPacote, gIniTecla, gIniAnel, gIniEco, gIniBarreira,
                gIniCarga, gIniInvestida, gIniSuperaquece, gIniCura, gIniFantasma, gIniPhisher,
                gIniBloqueio, gIniSurge;
extern GrupoSom *const GRUPOS_INI[15];
extern GrupoSom gChefeVirada, gChefeInvoca, gChefeEscudo, gChefeMorte, gChefeMira, gChefeInvestida,
                gChefeRajada, gChefeEspiral, gSentinelaFeixe, gRetroAviso, gRetroTiro,
                gJanelaResumo, gSilLinhas, gSilCorte;
extern GrupoSom *const GRUPOS_CHEFES[14];
extern GrupoSom gB1Caixa, gB1Verdade, gB1DanoAlma, gB1Golpe, gB1Lupa, gB1Verificado, gB1Sobrecarga;
extern GrupoSom *const GRUPOS_B1[7];
extern GrupoSom gJogEsquiva, gJogPulso, gJogParry, gJogMorte;
extern GrupoSom *const GRUPOS_JOG[4];
extern float g_tomPulsoFW;

// ----- Funções -----
void TrocarBGM(Music *nova, int id);
void AtualizarBGM(void);
void AtualizarAmbiente(int desejado, float dt);
void PausarAmbiente(bool pausar);
void TocarGrupo(GrupoSom *g, float tom, float volume);
void GrupoParar(GrupoSom *g);
bool SomLiberado(double *ultimo, double intervalo);
void TocarSomArma(int arma, float tom);
void IniciarSons(void);
void PararSons(void);
void TocarSomTom(Sound s, float tom);
void TocarSom(Sound s);
void EncerrarSons(void);

#endif // UTOPIA_AUDIO_H
