#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../../include/core/fonte.h"
#include "../../include/core/mapas.h"
#include "../../include/audio/audio.h"

// =============================================================
// SONS E MÚSICA
//   SFX: gerados por código (reformulados para mais impacto)
//   BGM: carregados de resources/audio/*.ogg (stream contínuo)
//
//   Faixas esperadas:
//     bgm_menu.ogg      → Menu principal + Intro
//     bgm_sala.ogg      → Sala inicial (Luci)
//     bgm_corredor.ogg  → Corredor do Histórico (Bloco 1)
//     bgm_batalha.ogg   → Batalha dos monitores (Bloco 1)
//     bgm_mestre.ogg    → Luta contra a ALUCINAÇÃO MESTRE (fim do Bloco 1; o tutorial antes dela usa a da Biblioteca)
//     bgm_atrio.ogg     → Átrio da Biblioteca
//     bgm_biblioteca.ogg→ Salas da Biblioteca dos Ecos (Bloco 2)
//     bgm_gameover.ogg  → Game over / derrota
//     bgm_alucinador1/2.ogg → Luta contra o ALUCINADOR (chefe da Biblioteca): fase 1 (tiros) e
//                         fase 2 (escudo e julgamento). Si bemol, o tom da Biblioteca, trocando
//                         o maior pelo menor no meio da frase ("ele mistura verdade e mentira")
//     bgm_janela1/2.ogg → Luta contra a JANELA DE CONTEXTO (mini-chefe): fases 1 a 3 e a SOBRECARGA.
//                         Drum and bass; um arpejo toca o que está "na janela" e esquece a nota
//                         mais antiga quando passa do limite (quando o PROTEGER sai, o vidro racha)
//     bgm_luci.ogg      → Bloco 3: corredor do núcleo, cena da arena e fim da luta
//   A troca entre faixas é suave (BGM_TRANSICAO).
//     bgm_luci_chefe1/2/3.ogg → Luta contra a Luci, uma por barra:
//                         1 CONTRADIÇÃO, 2 SURTO, 3 NÚCLEO (todas em lá frígio,
//                         a mesma base da bgm_luci.ogg da arena, com o tema da Luci)
//   Se algum arquivo não existir, o jogo continua sem música nessa tela.
// =============================================================

Sound sfxAcerto, sfxErro, sfxDano, sfxPegar, sfxSelecao, sfxTiro;
Sound sfxAvisoParry, sfxParryPerfeito, sfxEstatica, sfxDedutivo;
static bool audioPronto = false;

// --- BGM (stream) ---
Music bgmMenu      = {0};
Music bgmSala      = {0};
Music bgmCorredor  = {0};
Music bgmBatalha   = {0};
Music bgmMestre    = {0};
Music bgmBiblioteca= {0};
Music bgmGameover  = {0};
Music bgmAtrio     = {0};   // átrio da Biblioteca (hub entre as missões)
Music bgmLuci      = {0};   // arena final da Luci (mapa3): a cena antes da luta e o fim
Music bgmLuciChefe[3] = {0};   // a luta contra a Luci: uma faixa por barra (contradição, surto, núcleo)
Music bgmAlucinador[2] = {0};  // a luta contra o Alucinador: fase 1 (tiros) e fase 2 (escudo)
Music bgmJanela[2] = {0};      // a luta contra a Janela de Contexto: fases 1 a 3 e a SOBRECARGA (fase 4)
static Music *bgmAtual    = NULL;   // ponteiro para a música em reprodução
static int    bgmTelaId   = -99;    // id interno da tela atual (evita reiniciar a mesma faixa)
static float  bgmVolume   = 0.70f;  // 0.0 – 1.0

// Transição entre faixas: a que sai some enquanto a nova entra (as duas tocam juntas por um instante)
#define BGM_TRANSICAO 0.6f          // segundos (AJUSTE AQUI)
static Music *bgmSaindo     = NULL; // a faixa anterior, sumindo
static float  bgmNivelAtual = 1.0f; // 0 a 1: quanto da faixa nova já entrou
static float  bgmNivelSaindo = 0.0f;

// Carrega música de arquivo; se não existir devolve zero-init (stream.buffer == NULL)
static Music CarregarBGM(const char *caminho)
{
    if (!FileExists(caminho)) return (Music){0};
    Music m = LoadMusicStream(caminho);
    SetMusicVolume(m, bgmVolume);
    return m;
}

static bool BgmValida(const Music *m) { return m != NULL && m->stream.buffer != NULL; }

// Troca a BGM atual. id é um número interno para evitar reiniciar a mesma faixa.
// A troca é suave: a faixa atual some em BGM_TRANSICAO segundos enquanto a nova entra.
void TrocarBGM(Music *nova, int id)
{
    if (bgmTelaId == id) return;              // já está tocando esta
    bgmTelaId = id;

    if (nova == bgmAtual)                     // a mesma música com outro id (faixa reserva): só continua
        return;

    // Voltou para a faixa que ainda estava sumindo: ela sobe de novo de onde estava, sem recomeçar
    if (nova == bgmSaindo) {
        Music *antiga = bgmAtual;
        float nivelAntiga = bgmNivelAtual;
        bgmAtual = bgmSaindo;
        bgmNivelAtual = bgmNivelSaindo;
        bgmSaindo = antiga;
        bgmNivelSaindo = nivelAntiga;
        if (BgmValida(bgmSaindo) && !IsMusicStreamPlaying(*bgmSaindo)) { StopMusicStream(*bgmSaindo); bgmSaindo = NULL; }
        return;
    }

    // Uma terceira troca no meio da transição: a que já estava sumindo para na hora
    if (BgmValida(bgmSaindo))
        StopMusicStream(*bgmSaindo);
    bgmSaindo = NULL;

    // A atual passa a sumir (se estava parada, como depois do PararSons, sai na hora)
    if (BgmValida(bgmAtual)) {
        if (IsMusicStreamPlaying(*bgmAtual)) { bgmSaindo = bgmAtual; bgmNivelSaindo = bgmNivelAtual; }
        else StopMusicStream(*bgmAtual);
    }

    bgmAtual = nova;
    bgmNivelAtual = 0.0f;
    if (BgmValida(bgmAtual)) {
        SetMusicVolume(*bgmAtual, 0.0f);
        PlayMusicStream(*bgmAtual);
    }
}

// Deve ser chamado UMA vez por frame no loop principal (mantém o buffer preenchido e faz a transição)
void AtualizarBGM(void)
{
    if (!audioPronto) return;
    float passo = fminf(GetFrameTime(), 0.1f) / BGM_TRANSICAO;

    if (bgmSaindo != NULL) {
        bgmNivelSaindo -= passo;
        if (bgmNivelSaindo <= 0.0f || !BgmValida(bgmSaindo)) {
            if (BgmValida(bgmSaindo)) StopMusicStream(*bgmSaindo);
            bgmSaindo = NULL;
        } else {
            SetMusicVolume(*bgmSaindo, bgmVolume * sinf(bgmNivelSaindo * PI / 2.0f));   // curva de potência constante
            UpdateMusicStream(*bgmSaindo);
        }
    }

    if (BgmValida(bgmAtual)) {
        bgmNivelAtual = fminf(1.0f, bgmNivelAtual + passo);
        SetMusicVolume(*bgmAtual, bgmVolume * sinf(bgmNivelAtual * PI / 2.0f));
        UpdateMusicStream(*bgmAtual);
    }
}

// =============================================================
// GERADORES DE SFX — DESIGN GUIADO PELA ESTÉTICA DO JOGO
//
//  UtopIA é um RPG-roguelite ambientado DENTRO de uma IA corrompida.
//  Referências de design de som:
//    • Undertale  — bullet-hell, caixa de diálogo, alma de coração
//    • Chiptune / Game Boy DMG — onda quadrada + pulse, 8-bit crunch
//    • Glitch / Corrupção digital — ruído modulado, bit-crush, freq instável
//    • Tensão crescente (alucinação) — Luci degenerando a cada erro
//
//  Linguagem sonora adotada:
//    ACERTO  → jingle chiptune curto, ascendente, "dado validado"
//    ERRO    → crash grave com glitch — "corrupção detectada"
//    DANO    → buzz descendente + ruído branco seco — "packet perdido"
//    PEGAR   → dois pings ascendentes rápidos — "dado recebido"
//    SELEÇÃO → blip ultra-curto, 1-bit — "cursor digital"
//    TIRO    → zap de onda quadrada descendente — "pulso de dados"
//    PARRY AVISO  → chirp glitchado ascendente — "alerta de sistema"
//    PARRY PERFEITO → crash+ring com bit-crush — "contradição refutada"
//    ESTÁTICA → ruído modulado por LFO — "Luci alucinando"
//    DEDUTIVO → laser de onda quadrada com harmônico — "argumento disparado"
// =============================================================

// --- Primitivas de síntese ---

// Bit-crush: quantiza a amplitude para simular resolução reduzida (1-bit a 15-bit)
static float BitCrush(float sample, int bits)
{
    float levels = (float)(1 << bits);
    return floorf(sample * levels + 0.5f) / levels;
}

// Onda quadrada com duty cycle ajustável (duty = 0.0–1.0)
// duty 0.5 = onda quadrada clássica; 0.125 = pulse estreito (Game Boy canal 2)
static float PulseWave(float fase, float duty)
{
    float f = fmodf(fase / (2.0f * PI), 1.0f);
    if (f < 0.0f) f += 1.0f;
    return (f < duty) ? 1.0f : -1.0f;
}

// Gera um som completo com controle total de síntese.
// freqMod: se > 0, aplica vibrato (oscilação de freq em Hz)
// duty:    duty cycle da onda quadrada (0.5 = quadrada, 0.125 = pulse)
// bitCrush: 0 = sem, > 0 = número de bits (4 = Game Boy, 8 = NES)
static Sound GerarSom(float freqInicio, float freqFim, float duracao, float volume, int forma)
{
    const int taxa = 44100;
    unsigned int quadros = (unsigned int)(duracao * taxa);
    short *amostras = (short *)malloc(quadros * sizeof(short));
    float fase = 0.0f;

    for (unsigned int i = 0; i < quadros; i++) {
        float t    = (float)i / (float)quadros;
        float freq = freqInicio + (freqFim - freqInicio) * t;
        fase += 2.0f * PI * freq / (float)taxa;

        float ruido = (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;
        float env, onda;

        if (forma == 0) {
            // Senoide suave — para notas de diálogo/Luci
            env  = (t < 0.08f) ? t / 0.08f : (1.0f - t);
            onda = sinf(fase);
        } else if (forma == 1) {
            // Onda quadrada clássica (chiptune) — a mais usada
            env  = (t < 0.04f) ? t / 0.04f : (1.0f - t);
            onda = PulseWave(fase, 0.5f);
        } else if (forma == 2) {
            // Ruído branco modulado (LFO de amplitude a 8 Hz — Luci corrompendo)
            float lfo = 0.5f + 0.5f * sinf(2.0f * PI * 8.0f * t);
            env  = (t < 0.05f) ? t / 0.05f : ((t > 0.85f) ? (1.0f - t) / 0.15f : 1.0f);
            onda = ruido * lfo;
        } else if (forma == 3) {
            // Pulse estreito (Game Boy canal 2) — som mais "nasal" e digital
            env  = (t < 0.03f) ? t / 0.03f : (1.0f - t) * (1.0f - t);
            onda = PulseWave(fase, 0.125f);
        } else {
            // Metálico glitchado: parciais + ruído inicial + bit-crush leve
            env  = (t < 0.01f) ? t / 0.01f : (1.0f - t) * (1.0f - t);
            onda = 0.5f * sinf(fase) + 0.3f * sinf(fase * 2.76f) + 0.2f * sinf(fase * 5.4f);
            if (t < 0.05f) onda += ruido * 0.5f;
            onda = BitCrush(onda, 6);   // 6-bit crush — crash "pixelado"
        }

        amostras[i] = (short)(onda * env * volume * 30000.0f);
    }

    Wave w = { quadros, (unsigned int)taxa, 16, 1, amostras };
    Sound som = LoadSoundFromWave(w);
    free(amostras);
    return som;
}

// Jingle de N notas em onda quadrada (chiptune — a voz do acerto no UtopIA)
// As notas formam um acorde perfeito maior (tônica → terça → quinta)
static Sound GerarJingleChiptune(float freqBase, int notas, float duracaoNota, float volume)
{
    const int taxa = 44100;
    // Intervalos: uníssono, terça maior (5/4), quinta justa (3/2)
    static const float intervalos[] = { 1.0f, 1.25f, 1.5f };
    unsigned int qn = (unsigned int)(duracaoNota * taxa);
    unsigned int total = qn * (unsigned int)notas;
    short *am = (short *)malloc(total * sizeof(short));

    for (int n = 0; n < notas && n < 3; n++) {
        float freq = freqBase * intervalos[n];
        float fase = 0.0f;
        for (unsigned int i = 0; i < qn; i++) {
            float t   = (float)i / (float)qn;
            // Envelope: ataque instantâneo, decaimento suave
            float env = (t < 0.02f) ? 1.0f : (1.0f - t) * (1.0f - t);
            fase += 2.0f * PI * freq / (float)taxa;
            float onda = PulseWave(fase, 0.5f);
            // Bit-crush 8-bit: som de console retro
            onda = BitCrush(onda, 8);
            am[n * qn + i] = (short)(onda * env * volume * 30000.0f);
        }
    }

    Wave w = { total, (unsigned int)taxa, 16, 1, am };
    Sound som = LoadSoundFromWave(w);
    free(am);
    return som;
}

// Som de crash glitchado: ruído inicial + tônica descendente (erro/dano)
// Combina estourado "digital" com queda de frequência — "dado corrompido"
static Sound GerarCrashGlitch(float freqInicio, float freqFim, float duracao, float volume)
{
    const int taxa = 44100;
    unsigned int quadros = (unsigned int)(duracao * taxa);
    short *am = (short *)malloc(quadros * sizeof(short));
    float fase = 0.0f;

    for (unsigned int i = 0; i < quadros; i++) {
        float t    = (float)i / (float)quadros;
        float freq = freqInicio + (freqFim - freqInicio) * (t * t);   // queda curva — mais rápida no início

        // Freq oscila levemente (instabilidade — alucinação)
        float instab = 1.0f + 0.04f * sinf(2.0f * PI * 60.0f * t);
        freq *= instab;

        fase += 2.0f * PI * freq / (float)taxa;

        float ruido = (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;

        // Componente de onda quadrada
        float onda = PulseWave(fase, 0.5f);

        // Burst de ruído no início (< 8% da duração) — "glitch de entrada"
        if (t < 0.08f) onda = onda * 0.4f + ruido * 0.6f;

        // Bit-crush mais agressivo (5-bit) — som "quebrado"
        onda = BitCrush(onda, 5);

        // Envelope: pico instantâneo, decaimento quadrático
        float env = (1.0f - t) * (1.0f - t);

        am[i] = (short)(onda * env * volume * 30000.0f);
    }

    Wave w = { quadros, (unsigned int)taxa, 16, 1, am };
    Sound som = LoadSoundFromWave(w);
    free(am);
    return som;
}

// Dois pings ascendentes rápidos (coletar dado / verdade confirmada)
static Sound GerarPingDuplo(float freq1, float freq2, float duracaoCada, float volume)
{
    const int taxa = 44100;
    unsigned int qn    = (unsigned int)(duracaoCada * taxa);
    unsigned int total = qn * 2;
    short *am = (short *)malloc(total * sizeof(short));

    float freqs[2] = { freq1, freq2 };
    for (int p = 0; p < 2; p++) {
        float fase = 0.0f;
        for (unsigned int i = 0; i < qn; i++) {
            float t   = (float)i / (float)qn;
            float env = (t < 0.05f) ? t / 0.05f : (1.0f - t) * (1.0f - t);
            fase += 2.0f * PI * freqs[p] / (float)taxa;
            // Pulse estreito (0.25) — som mais "digital" do que senoide
            float onda = PulseWave(fase, 0.25f);
            am[p * qn + i] = (short)(onda * env * volume * 30000.0f);
        }
    }

    Wave w = { total, (unsigned int)taxa, 16, 1, am };
    Sound som = LoadSoundFromWave(w);
    free(am);
    return som;
}

// Chirp glitchado ascendente (alerta / aviso de parry)
// A frequência sobe por saltos discretos — imitando o LFO de portamento do Game Boy
static Sound GerarChirpAlerta(float freqBase, float freqAlvo, float duracao, float volume)
{
    const int taxa = 44100;
    unsigned int quadros = (unsigned int)(duracao * taxa);
    short *am = (short *)malloc(quadros * sizeof(short));

    // Divide em 8 "degraus" discretos de frequência (como o hardware chiptune)
    int degraus = 8;
    float fase  = 0.0f;

    for (unsigned int i = 0; i < quadros; i++) {
        float t = (float)i / (float)quadros;

        // Frequência em degraus (quantização de pitch — som de portamento 8-bit)
        int d    = (int)(t * degraus);
        float ft = (float)d / (float)degraus;
        float freq = freqBase + (freqAlvo - freqBase) * ft;

        // Instabilidade fina: micro-glitch a cada degrau
        float glitch = 1.0f + 0.08f * ((float)(rand() % 3 - 1));
        if (i % (unsigned int)(taxa / 60) == 0) freq *= glitch;

        fase += 2.0f * PI * freq / (float)taxa;

        float env = (t < 0.05f) ? t / 0.05f : (t > 0.75f) ? (1.0f - t) / 0.25f : 1.0f;
        float onda = PulseWave(fase, 0.5f);

        am[i] = (short)(onda * env * volume * 30000.0f);
    }

    Wave w = { quadros, (unsigned int)taxa, 16, 1, am };
    Sound som = LoadSoundFromWave(w);
    free(am);
    return som;
}

// Crash de parry perfeito: impacto grave + ring ascendente
// "A alucinação foi refutada" — som de peso + resolução
static Sound GerarParryPerfeito(float volume)
{
    const int taxa = 44100;
    float duracao  = 0.55f;
    unsigned int quadros = (unsigned int)(duracao * taxa);
    short *am = (short *)malloc(quadros * sizeof(short));

    float faseImpacto = 0.0f;
    float faseRing    = 0.0f;

    for (unsigned int i = 0; i < quadros; i++) {
        float t = (float)i / (float)quadros;

        float ruido = (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;

        // 1) Impacto grave descendente (0–15% da duração): frequência cai rápido
        float freqImpacto = 200.0f * expf(-t * 25.0f);
        faseImpacto += 2.0f * PI * freqImpacto / (float)taxa;
        float impacto  = PulseWave(faseImpacto, 0.5f);
        float envImpacto = (t < 0.02f) ? 1.0f : expf(-t * 18.0f);
        // Bit-crush 5-bit no impacto
        impacto = BitCrush(impacto, 5) * envImpacto;

        // 2) Ring ascendente (começa logo após): pitch sobe depois do impacto
        float freqRing = 400.0f + 800.0f * t;
        faseRing += 2.0f * PI * freqRing / (float)taxa;
        float ring    = sinf(faseRing);
        // Entra depois do impacto (sobe em 15% da duração) e some no fim, sem corte seco
        float uRing   = t - 0.05f;
        float envRing = (uRing < 0.0f) ? 0.0f : (uRing < 0.15f) ? uRing / 0.15f : expf(-(uRing - 0.15f) * 6.0f);

        // 3) Burst de ruído no impacto inicial
        float burst = (t < 0.04f) ? ruido * (1.0f - t / 0.04f) : 0.0f;

        float onda = impacto * 0.5f + ring * envRing * 0.35f + burst * 0.15f;
        am[i] = (short)(onda * volume * 30000.0f);
    }

    Wave w = { quadros, (unsigned int)taxa, 16, 1, am };
    Sound som = LoadSoundFromWave(w);
    free(am);
    return som;
}

// Ruído de estática com LFO de amplitude (Luci alucinando)
// A amplitude oscila irregularmente — soa como sinal digital degradado
static Sound GerarEstaticaLuci(float volume)
{
    const int taxa = 44100;
    float duracao  = 0.85f;
    unsigned int quadros = (unsigned int)(duracao * taxa);
    short *am = (short *)malloc(quadros * sizeof(short));

    for (unsigned int i = 0; i < quadros; i++) {
        float t = (float)i / (float)quadros;

        // LFO duplo: o sinal "respira" de forma irregular (Luci instável)
        float lfo1 = 0.5f + 0.5f * sinf(2.0f * PI * 7.3f  * t);
        float lfo2 = 0.5f + 0.5f * sinf(2.0f * PI * 13.7f * t);
        float lfo  = lfo1 * lfo2;   // produto cria batimento caótico

        float ruido = (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;

        // Crackles periódicos (pops digitais — corrupção de dados)
        float crackle = 0.0f;
        if (rand() % (taxa / 60) == 0)   // ~60 crackles por segundo
            crackle = ((float)rand() / (float)RAND_MAX - 0.5f) * 2.0f;

        float env = (t < 0.06f) ? t / 0.06f : ((t > 0.80f) ? (1.0f - t) / 0.20f : 1.0f);

        float onda = ruido * lfo * 0.7f + crackle * 0.3f;
        am[i] = (short)(onda * env * volume * 30000.0f);
    }

    Wave w = { quadros, (unsigned int)taxa, 16, 1, am };
    Sound som = LoadSoundFromWave(w);
    free(am);
    return som;
}

// Disparo Dedutivo: laser de onda quadrada + harmônico (argumento lógico disparado)
static Sound GerarDisparoDedutivo(float volume)
{
    const int taxa = 44100;
    float duracao  = 0.22f;
    unsigned int quadros = (unsigned int)(duracao * taxa);
    short *am = (short *)malloc(quadros * sizeof(short));

    float fase1 = 0.0f, fase2 = 0.0f;

    for (unsigned int i = 0; i < quadros; i++) {
        float t = (float)i / (float)quadros;

        // Frequência principal: sobe rápido depois estabiliza
        float freq1 = 300.0f + 1200.0f * (1.0f - expf(-t * 15.0f));
        // Harmônico: quinta acima (x1.5), defasado — dá "corpo" ao laser
        float freq2 = freq1 * 1.5f;

        fase1 += 2.0f * PI * freq1 / (float)taxa;
        fase2 += 2.0f * PI * freq2 / (float)taxa;

        // Pulse estreito (0.125) — nasal, penetrante — como no Game Boy
        float onda1 = PulseWave(fase1, 0.125f);
        float onda2 = PulseWave(fase2, 0.5f) * 0.4f;

        // Bit-crush 7-bit: som digital mas sem ser grosseiro
        float onda = BitCrush(onda1 * 0.6f + onda2, 7);

        // Envelope: ataque instantâneo, decaimento rápido (tiro = impacto curto)
        float env = (t < 0.02f) ? 1.0f : expf(-t * 10.0f);

        am[i] = (short)(onda * env * volume * 30000.0f);
    }

    Wave w = { quadros, (unsigned int)taxa, 16, 1, am };
    Sound som = LoadSoundFromWave(w);
    free(am);
    return som;
}

// --- gerador pseudoaleatório próprio (as variações saem iguais em toda máquina) ---
static unsigned int g_sinteseSemente = 0x1234567u;
static float SinRnd(void)          // -1 .. 1
{
    g_sinteseSemente ^= g_sinteseSemente << 13;
    g_sinteseSemente ^= g_sinteseSemente >> 17;
    g_sinteseSemente ^= g_sinteseSemente << 5;
    return (float)(g_sinteseSemente & 0xFFFFFF) / (float)0x7FFFFF - 1.0f;
}
static float SinRndU(void) { return 0.5f + 0.5f * SinRnd(); }      // 0 .. 1

static BufSom BufNovo(float segundos)
{
    BufSom b;
    b.n = (int)(segundos * TAXA_SOM);
    if (b.n < 1) b.n = 1;
    b.x = (float *)calloc((size_t)b.n, sizeof(float));
    return b;
}
static float SvfPasso(Svf *f, float in, float corte, float q, int saida)    // saida: 0 = LP, 1 = BP, 2 = HP
{
    if (corte > TAXA_SOM * 0.22f) corte = TAXA_SOM * 0.22f;
    if (corte < 10.0f) corte = 10.0f;
    float k = 2.0f * sinf(PI * corte / (float)TAXA_SOM);
    float hp = in - f->lp - q * f->bp;
    f->bp += k * hp;
    f->lp += k * f->bp;
    return (saida == 0) ? f->lp : ((saida == 1) ? f->bp : hp);
}

// Reverb de Schroeder (4 pentes + 2 passa-tudo): "pedra" do calabouço digital.
// tamanho 0.5 = sala pequena, 1.0 = salão, 1.6 = corredor/caverna. O buffer precisa ter cauda livre no fim.
static void ReverbPedra(BufSom *b, float mistura, float tamanho, float amortece)
{
    static const int baseComb[4] = { 1116, 1188, 1277, 1356 };
    static const int baseAll[2]  = { 556, 441 };
    int lc[4], la[2];
    float *pc[4], *pa[2];
    int ic[4] = { 0 }, ia[2] = { 0 };
    float filt[4] = { 0 };
    for (int k = 0; k < 4; k++) { lc[k] = (int)(baseComb[k] * tamanho); pc[k] = (float *)calloc((size_t)lc[k], sizeof(float)); }
    for (int k = 0; k < 2; k++) { la[k] = (int)(baseAll[k] * tamanho);  pa[k] = (float *)calloc((size_t)la[k], sizeof(float)); }
    float fb = 0.70f + 0.12f * fminf(tamanho, 1.5f) / 1.5f;

    for (int i = 0; i < b->n; i++) {
        float in = b->x[i] * 0.25f, s = 0.0f;
        for (int k = 0; k < 4; k++) {
            float y = pc[k][ic[k]];
            filt[k] = y * (1.0f - amortece) + filt[k] * amortece;
            pc[k][ic[k]] = in + filt[k] * fb;
            if (++ic[k] >= lc[k]) ic[k] = 0;
            s += y;
        }
        for (int k = 0; k < 2; k++) {
            float y = pa[k][ia[k]];
            float o = -s + y;
            pa[k][ia[k]] = s + y * 0.5f;
            if (++ia[k] >= la[k]) ia[k] = 0;
            s = o;
        }
        b->x[i] = b->x[i] * (1.0f - mistura * 0.5f) + s * mistura;
    }
    for (int k = 0; k < 4; k++) free(pc[k]);
    for (int k = 0; k < 2; k++) free(pa[k]);
}

// Tira o DC, faz fades de 2 ms / 8 ms (sem clique), normaliza o pico e satura de leve
static void BufFinalizar(BufSom *b, float pico)
{
    float dc = 0.0f, m = 0.0f;
    for (int i = 0; i < b->n; i++) { dc = dc * 0.995f + b->x[i] * 0.005f; b->x[i] -= dc; }
    int fi = TAXA_SOM / 500, fo = TAXA_SOM / 125;
    for (int i = 0; i < fi && i < b->n; i++) b->x[i] *= (float)i / (float)fi;
    for (int i = 0; i < fo && i < b->n; i++) b->x[b->n - 1 - i] *= (float)i / (float)fo;
    for (int i = 0; i < b->n; i++) m = fmaxf(m, fabsf(b->x[i]));
    if (m < 1e-6f) return;
    float g = pico / m;
    for (int i = 0; i < b->n; i++) b->x[i] = tanhf(b->x[i] * g * 1.15f) / tanhf(1.15f);
}

static short *BufParaPcm(const BufSom *b)
{
    short *pcm = (short *)malloc((size_t)b->n * sizeof(short));
    for (int i = 0; i < b->n; i++) {
        float v = Clamp(b->x[i], -1.0f, 1.0f);
        pcm[i] = (short)(v * 32000.0f);
    }
    return pcm;
}

static Sound BufParaSom(BufSom *b, float pico)
{
    BufFinalizar(b, pico);
    short *pcm = BufParaPcm(b);
    Wave w = { (unsigned int)b->n, TAXA_SOM, 16, 1, pcm };
    Sound s = LoadSoundFromWave(w);
    free(pcm);
    free(b->x);
    b->x = NULL;
    return s;
}

// Envelopes e osciladores pequenos
static float EnvAD(float t, float ataque, float queda)     // t em segundos; queda exponencial (constante de tempo)
{
    if (t < ataque) return t / ataque;
    return expf(-(t - ataque) / queda);
}
static float OscPulso(float fase, float duty) { float f = fase - floorf(fase); return (f < duty) ? 1.0f : -1.0f; }
static float OscSerra(float fase)             { float f = fase - floorf(fase); return 2.0f * f - 1.0f; }
static float OscTri(float fase)               { float f = fase - floorf(fase); return 4.0f * fabsf(f - 0.5f) - 1.0f; }
static float Esmagar(float v, int bits)       { float l = (float)(1 << bits); return floorf(v * l + 0.5f) / l; }

// =============================================================
// RECEITAS DAS ARMAS (v = 0..2 é a variação; muda tom, timbre e ruído)
// =============================================================

// DISPARADOR DE DADOS (azul, tiro reto e rápido): "pew" de pulso estreito + clique digital, sala pequena
static Sound ReceitaDisparador(int v)
{
    BufSom b = BufNovo(0.16f);
    float f0 = 1350.0f * (1.0f + 0.07f * (float)(v - 1)), duty = 0.22f + 0.05f * (float)v, fase = 0.0f;
    Svf lp = { 0 }, hp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float f = 420.0f + (f0 - 420.0f) * expf(-t / 0.022f);
        fase += f / TAXA_SOM;
        float s = OscPulso(fase, duty) * EnvAD(t, 0.001f, 0.030f);
        float clique = SvfPasso(&hp, SinRnd(), 3500.0f, 1.0f, 2) * expf(-t / 0.004f);
        b.x[i] = SvfPasso(&lp, s * 0.7f + clique * 0.5f, 5200.0f, 0.9f, 0);
    }
    ReverbPedra(&b, 0.10f, 0.5f, 0.45f);
    return BufParaSom(&b, 0.55f);
}

// VETOR DE COMPILAÇÃO (branco-azulado, tiro carregado que atravessa): estalo seco + "rasgo" descendente + grave
static Sound ReceitaVetor(int v)
{
    BufSom b = BufNovo(0.55f);
    float fa = 0.0f, fs = 0.0f, f0 = 2400.0f + 200.0f * (float)v;
    Svf hp = { 0 }, bp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float estalo = SvfPasso(&hp, SinRnd(), 2200.0f, 0.8f, 2) * expf(-t / 0.008f);
        float f = 160.0f + (f0 - 160.0f) * expf(-t / 0.06f);
        fa += f / TAXA_SOM;
        float rasgo = (OscSerra(fa) * 0.6f + OscPulso(fa * 1.005f, 0.5f) * 0.4f) * EnvAD(t, 0.002f, 0.09f);
        rasgo = SvfPasso(&bp, rasgo, 900.0f + f * 0.8f, 0.5f, 0);
        fs += (55.0f + 40.0f * expf(-t / 0.05f)) / TAXA_SOM;
        float sub = sinf(2.0f * PI * fs) * EnvAD(t, 0.002f, 0.10f);
        b.x[i] = estalo * 0.7f + rasgo * 0.55f + sub * 0.6f;
    }
    ReverbPedra(&b, 0.22f, 1.0f, 0.35f);
    return BufParaSom(&b, 0.80f);
}

// Vetor com carga cheia: sino cristalino curto (avisa que o tiro perfeito está pronto)
static Sound ReceitaVetorCarga(int v)
{
    BufSom b = BufNovo(0.45f);
    float f = 1568.0f * (1.0f + 0.02f * (float)v);
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float s = sinf(2.0f * PI * f * t) * EnvAD(t, 0.002f, 0.12f)
                + 0.5f * sinf(2.0f * PI * f * 1.5f * t) * EnvAD(t, 0.002f, 0.07f)
                + 0.25f * sinf(2.0f * PI * f * 2.76f * t) * EnvAD(t, 0.001f, 0.03f);
        b.x[i] = s;
    }
    ReverbPedra(&b, 0.30f, 0.8f, 0.30f);
    return BufParaSom(&b, 0.40f);
}

// ROTEADOR DE PACOTES (laranja, pacote teleguiado): "modem" FSK — dois tons alternando, esmagado em 6 bits
static Sound ReceitaRoteador(int v)
{
    BufSom b = BufNovo(0.22f);
    float fase = 0.0f, ta = 1100.0f + 80.0f * (float)v, tb = 1900.0f + 120.0f * (float)v;
    float passo = 0.014f + 0.002f * (float)v;
    Svf lp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float f = (((int)(t / passo)) % 2 == 0) ? ta : tb;
        f *= 1.0f + 0.6f * t;                                   // sobe: o pacote "procura" o alvo
        fase += f / TAXA_SOM;
        float s = Esmagar(OscPulso(fase, 0.5f) * 0.6f + OscTri(fase * 0.5f) * 0.4f, 6);
        b.x[i] = SvfPasso(&lp, s, 4200.0f, 1.0f, 0) * EnvAD(t, 0.002f, 0.060f);
    }
    ReverbPedra(&b, 0.12f, 0.6f, 0.4f);
    return BufParaSom(&b, 0.42f);
}

// FLUXO CONTÍNUO (amarelo, metralhadora que esquenta): tique curto, serra + chiado elétrico.
// O tom sobe com o calor (quem chama passa o tom).
static Sound ReceitaFluxo(int v)
{
    BufSom b = BufNovo(0.07f);
    float fase = 0.0f, f0 = 210.0f + 25.0f * (float)v;
    Svf bp = { 0 }, lp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        fase += (f0 * (1.0f + 1.5f * expf(-t / 0.006f))) / TAXA_SOM;
        float corpo = SvfPasso(&lp, OscSerra(fase), 2600.0f, 0.7f, 0);
        float chiado = SvfPasso(&bp, SinRnd(), 3200.0f + 400.0f * (float)v, 0.35f, 1);
        b.x[i] = (corpo * 0.6f + chiado * 0.8f) * EnvAD(t, 0.0008f, 0.014f);
    }
    return BufParaSom(&b, 0.36f);
}

// ORBE DE BUFFER (roxo, esfera lenta): "bloop" com vibrato e FM, grave e molhado
static Sound ReceitaOrbe(int v)
{
    BufSom b = BufNovo(0.55f);
    float fc = 0.0f, fm = 0.0f, base = 300.0f - 18.0f * (float)v;
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float f = (base * expf(-t / 0.25f) + 120.0f) * (1.0f + 0.05f * sinf(2.0f * PI * 16.0f * t));
        fm += (f * 2.0f) / TAXA_SOM;
        fc += f / TAXA_SOM;
        float s = sinf(2.0f * PI * fc + 1.6f * expf(-t / 0.08f) * sinf(2.0f * PI * fm));
        b.x[i] = s * EnvAD(t, 0.012f, 0.13f);
    }
    ReverbPedra(&b, 0.32f, 1.1f, 0.3f);
    return BufParaSom(&b, 0.55f);
}

// Implosão do Orbe: sucção crescente (ruído subindo) e estouro grave
static Sound ReceitaOrbeImplosao(int v)
{
    BufSom b = BufNovo(1.0f);
    float fs = 0.0f, puxa = 0.22f + 0.03f * (float)v;
    Svf bp = { 0 }, lp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float k = fminf(t / puxa, 1.0f);
        float suc = (t < puxa) ? SvfPasso(&bp, SinRnd(), 300.0f + 2600.0f * k * k, 0.4f, 1) * k * k : 0.0f;
        float tb = t - puxa, boom = 0.0f;
        if (tb >= 0.0f) {
            fs += (45.0f + 110.0f * expf(-tb / 0.04f)) / TAXA_SOM;
            boom = sinf(2.0f * PI * fs) * EnvAD(tb, 0.002f, 0.18f)
                 + SvfPasso(&lp, SinRnd(), 900.0f * expf(-tb / 0.08f) + 120.0f, 0.6f, 0) * EnvAD(tb, 0.001f, 0.10f) * 1.4f;
        }
        b.x[i] = suc * 0.6f + boom * 0.9f;
    }
    ReverbPedra(&b, 0.35f, 1.3f, 0.35f);
    return BufParaSom(&b, 0.82f);
}

// FRAGMENTADORA (rosa, cone com coice): estouro de ruído com filtro fechando + soco grave + crocância
static Sound ReceitaFragmentadora(int v)
{
    BufSom b = BufNovo(0.45f);
    float fs = 0.0f;
    Svf lp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float ruido = SvfPasso(&lp, SinRnd(), 600.0f + 6500.0f * expf(-t / (0.035f + 0.008f * (float)v)), 0.7f, 0);
        fs += (48.0f + 70.0f * expf(-t / 0.025f)) / TAXA_SOM;
        float soco = sinf(2.0f * PI * fs) * EnvAD(t, 0.001f, 0.07f);
        float s = ruido * EnvAD(t, 0.0008f, 0.06f) * 1.2f + soco * 0.9f;
        b.x[i] = Esmagar(s, 7);
    }
    ReverbPedra(&b, 0.16f, 0.8f, 0.45f);
    return BufParaSom(&b, 0.78f);
}

// Cartucho dourado: o mesmo estouro + anel metálico inarmônico
static Sound ReceitaFragDourado(int v)
{
    BufSom b = BufNovo(0.7f);
    float fs = 0.0f, fr = 880.0f + 40.0f * (float)v;
    Svf lp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float ruido = SvfPasso(&lp, SinRnd(), 800.0f + 7000.0f * expf(-t / 0.05f), 0.7f, 0) * EnvAD(t, 0.0008f, 0.07f);
        fs += (45.0f + 80.0f * expf(-t / 0.03f)) / TAXA_SOM;
        float soco = sinf(2.0f * PI * fs) * EnvAD(t, 0.001f, 0.09f);
        float anel = (sinf(2.0f * PI * fr * t) + 0.6f * sinf(2.0f * PI * fr * 2.76f * t) + 0.35f * sinf(2.0f * PI * fr * 5.40f * t))
                   * EnvAD(t, 0.002f, 0.16f);
        b.x[i] = ruido * 1.1f + soco + anel * 0.35f;
    }
    ReverbPedra(&b, 0.25f, 1.0f, 0.35f);
    return BufParaSom(&b, 0.85f);
}

// PULSO DE FIREWALL (verde, onda em volta): "whoomp" grave + varredura de ruído que abre para fora
static Sound ReceitaPulsoFW(int v)
{
    BufSom b = BufNovo(0.8f);
    float fs = 0.0f;
    Svf bp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        fs += (52.0f + 75.0f * expf(-t / 0.06f)) / TAXA_SOM;
        float grave = sinf(2.0f * PI * fs) * EnvAD(t, 0.004f, 0.14f);
        float k = fminf(t / 0.30f, 1.0f);
        float onda = SvfPasso(&bp, SinRnd(), 260.0f + (2400.0f + 300.0f * (float)v) * k, 0.5f, 1) * sinf(PI * k) * (1.0f - 0.4f * k);
        float zumbido = OscTri(t * (98.0f + 6.0f * (float)v)) * EnvAD(t, 0.01f, 0.10f) * 0.2f;
        b.x[i] = grave * 0.9f + onda * 0.8f + zumbido;
    }
    ReverbPedra(&b, 0.28f, 1.2f, 0.35f);
    return BufParaSom(&b, 0.72f);
}

// KATANA DE BYTES: whoosh grave + corte seco + choque de metal (a versão escolhida nos testes,
// "katana_3_whoosh_corte_e_metal": a mesma semente, então sai idêntica ao arquivo aprovado).
// O whoosh é só ruído de banda larga (filtro estreito vira assovio).
static float KatWhoosh(Svf *lp, Svf *hp, float t, float dur, float pico, float baixo)
{
    float k = fminf(t / dur, 1.0f);
    float env = (t < dur) ? ((k < 0.6f) ? powf(k / 0.6f, 2.0f) : powf(1.0f - (k - 0.6f) / 0.4f, 1.5f)) : 0.0f;
    float c = baixo + (pico - baixo) * ((k < 0.6f) ? k / 0.6f : 1.0f - (k - 0.6f) / 0.4f);
    float n = SvfPasso(lp, SinRnd(), c, 1.3f, 0);
    return SvfPasso(hp, n, c * 0.25f, 1.3f, 2) * env;
}

static Sound ReceitaKatana(int v)
{
    g_sinteseSemente = 191u + (unsigned int)v * 131u + 2u;
    BufSom b = BufNovo(0.9f);
    Svf l1 = { 0 }, h1 = { 0 }, l2 = { 0 }, h2 = { 0 }, tk = { 0 }, cr = { 0 };
    float d = 0.22f + 0.02f * (float)v, fs = 0.0f, golpe = d * 0.6f, fm = 210.0f + 15.0f * (float)v;
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, tb = t - golpe;
        float s = KatWhoosh(&l1, &h1, t, d, 1100.0f + 100.0f * (float)v, 180.0f) * 1.4f
                + KatWhoosh(&l2, &h2, t, d * 0.8f, 2600.0f, 600.0f) * 0.35f;
        float tshk = (tb > 0.0f) ? SvfPasso(&tk, SinRnd(), 1800.0f, 0.9f, 2) * EnvAD(tb, 0.0005f, 0.022f) : 0.0f;   // o corte
        float soco = 0.0f;
        if (tb >= 0.0f) {
            fs += (55.0f + 90.0f * expf(-tb / 0.02f)) / TAXA_SOM;
            soco = sinf(2.0f * PI * fs) * EnvAD(tb, 0.001f, 0.07f);                                       // o corpo
        }
        s += tshk * 1.1f + soco * 1.2f;
        float crunch = (tb > 0.0f) ? SvfPasso(&cr, SinRnd(), 1500.0f * expf(-tb / 0.03f) + 150.0f, 1.0f, 0) * EnvAD(tb, 0.0005f, 0.05f) : 0.0f;
        s += crunch;
        if (tb >= 0.0f)
            s += (sinf(2.0f * PI * fm * tb) + 0.8f * sinf(2.0f * PI * fm * 2.76f * tb) * expf(-tb / 0.10f)
                + 0.5f * sinf(2.0f * PI * fm * 5.40f * tb) * expf(-tb / 0.05f) + 0.4f * sinf(2.0f * PI * fm * 1.012f * tb))
                * EnvAD(tb, 0.001f, 0.20f) * 0.35f;                                                     // choque de metal
        b.x[i] = tanhf(s * 1.4f);
    }
    ReverbPedra(&b, 0.18f, 1.1f, 0.45f);
    return BufParaSom(&b, 0.85f);
}

// AVISO DE PARRY (pesado, combina com o parry): a lâmina inimiga sendo erguida —
// batida grave curta + raspão de metal que sobe (banda larga, sem assovio) + "ting" grave de bigorna no ápice
static Sound ReceitaAvisoParry(int v)
{
    BufSom b = BufNovo(0.55f);
    Svf lp = { 0 }, hp = { 0 };
    float fs = 0.0f, sobe = 0.20f + 0.02f * (float)v, f = 296.0f + 12.0f * (float)v;
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        fs += (60.0f + 70.0f * expf(-t / 0.02f)) / TAXA_SOM;
        float batida = sinf(2.0f * PI * fs) * EnvAD(t, 0.001f, 0.05f);
        float k = fminf(t / sobe, 1.0f);
        float c = 500.0f + 2200.0f * k * k;
        float raspa = SvfPasso(&hp, SvfPasso(&lp, SinRnd(), c, 1.1f, 0), c * 0.35f, 1.1f, 2) * ((t < sobe) ? k * k : 0.0f);
        float tt = t - sobe, ting = 0.0f;
        if (tt > 0.0f)
            ting = (sinf(2.0f * PI * f * tt) + 0.7f * sinf(2.0f * PI * f * 2.32f * tt) * expf(-tt / 0.08f) + 0.4f * sinf(2.0f * PI * f * 4.25f * tt) * expf(-tt / 0.04f))
                 * EnvAD(tt, 0.001f, 0.12f);
        b.x[i] = batida * 0.8f + raspa * 0.9f + ting * 0.45f;
    }
    ReverbPedra(&b, 0.22f, 1.1f, 0.4f);
    return BufParaSom(&b, 0.70f);
}

// PARRY PERFEITO (pesado): o golpe inimigo bate num escudo de metal e é devolvido.
//   bigorna grave (parciais inarmônicas ~150 Hz) + soco sub-grave saturado + estalo de impacto
//   + faíscas curtas + a "resposta": um brilho que sobe no fim (a contradição refutada)
static Sound ReceitaParry(int v)
{
    BufSom b = BufNovo(1.4f);
    Svf hp = { 0 }, lp = { 0 }, fa = { 0 };
    float fs = 0.0f, f = 148.0f + 8.0f * (float)v, fb = 0.0f;
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        fs += (38.0f + 110.0f * expf(-t / 0.025f)) / TAXA_SOM;
        float sub = tanhf(2.0f * sinf(2.0f * PI * fs) * EnvAD(t, 0.001f, 0.16f));
        float estalo = SvfPasso(&hp, SinRnd(), 1200.0f, 0.8f, 2) * EnvAD(t, 0.0003f, 0.012f)
                     + SvfPasso(&lp, SinRnd(), 3000.0f * expf(-t / 0.04f) + 250.0f, 0.8f, 0) * EnvAD(t, 0.0005f, 0.06f);
        float bigorna = (sinf(2.0f * PI * f * t) + 0.9f * sinf(2.0f * PI * f * 2.32f * t) * expf(-t / 0.35f)
                       + 0.7f * sinf(2.0f * PI * f * 4.25f * t) * expf(-t / 0.18f) + 0.45f * sinf(2.0f * PI * f * 6.63f * t) * expf(-t / 0.08f)
                       + 0.6f * sinf(2.0f * PI * f * 1.007f * t)) * EnvAD(t, 0.001f, 0.45f);
        float faisca = (t < 0.25f && SinRndU() > 0.995f) ? SinRnd() : 0.0f;
        faisca = SvfPasso(&fa, faisca, 5000.0f, 1.0f, 2) * 3.0f;
        float tr = t - 0.12f, resposta = 0.0f;
        if (tr > 0.0f) {
            fb += (300.0f + 500.0f * fminf(tr / 0.3f, 1.0f)) / TAXA_SOM;
            resposta = (sinf(2.0f * PI * fb) + 0.4f * sinf(2.0f * PI * fb * 1.5f)) * EnvAD(tr, 0.05f, 0.18f) * 0.15f;
        }
        b.x[i] = sub * 1.0f + estalo * 0.9f + bigorna * 0.32f + faisca + resposta;
    }
    ReverbPedra(&b, 0.30f, 1.4f, 0.35f);
    return BufParaSom(&b, 0.92f);
}

// RAIO DE PROCESSAMENTO (circuito dourado): o circuito "se desenha" — 4 bipes subindo em pentatônica
static Sound ReceitaRaioCircuito(int v)
{
    static const float notas[3][4] = { { 1.0f, 1.125f, 1.5f, 2.0f }, { 1.0f, 1.333f, 1.5f, 2.25f }, { 1.0f, 1.2f, 1.6f, 2.0f } };
    BufSom b = BufNovo(0.32f);
    float base = 880.0f, passo = 0.032f, fase = 0.0f;
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        int n = (int)(t / passo);
        if (n > 3) n = 3;
        float tn = t - (float)n * passo;
        fase += base * notas[v][n] / TAXA_SOM;
        float s = Esmagar(OscPulso(fase, 0.25f) * 0.5f + sinf(2.0f * PI * fase) * 0.5f, 6) * EnvAD(tn, 0.001f, (n == 3) ? 0.08f : 0.018f);
        b.x[i] = s;
    }
    ReverbPedra(&b, 0.20f, 0.7f, 0.4f);
    return BufParaSom(&b, 0.36f);
}

// MORTE DE INIMIGO: a alucinação se desfaz em dados — soco grave + "esfarelar" de ruído esmagado
// (sem nota aguda: o som antigo, dois pings agudos, soava como um assovio)
static Sound ReceitaMorteInimigo(int v)
{
    BufSom b = BufNovo(0.30f);
    float fs = 0.0f;
    Svf lp = { 0 }, bp = { 0 };
    int segura = 0;
    float amostra = 0.0f;
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        fs += (42.0f + 75.0f * expf(-t / 0.03f)) / TAXA_SOM;
        float soco = sinf(2.0f * PI * fs) * EnvAD(t, 0.001f, 0.06f);
        // ruído com "sample and hold": a taxa cai com o tempo (o dado vai perdendo resolução)
        if (--segura <= 0) { amostra = SinRnd(); segura = 1 + (int)(t * (60.0f + 15.0f * (float)v)); }
        float farelo = SvfPasso(&lp, amostra, 400.0f + 3200.0f * expf(-t / 0.07f), 0.7f, 0) * EnvAD(t, 0.001f, 0.08f);
        float grao = (SinRndU() > 0.992f - 0.002f * (float)v) ? SvfPasso(&bp, SinRnd(), 1800.0f, 0.6f, 1) * expf(-t / 0.12f) : 0.0f;
        b.x[i] = Esmagar(soco * 0.85f + farelo * 0.9f + grao * 1.5f, 6);
    }
    ReverbPedra(&b, 0.14f, 0.8f, 0.5f);
    return BufParaSom(&b, 0.55f);
}

// Coluna de luz (o sigilo detona): um LASER ENORME CAI DO CÉU.
//   1) o raio descendo: varredura FM de agudo para grave (0,25 s)
//   2) o impacto: sub-grave saturado (soco no chão) + estrondo de trovão que vai fechando
//   3) a coluna parada: zumbido grave de laser, duas serras desafinadas, que some em ~0,7 s
static Sound ReceitaRaioColuna(int v)
{
    BufSom b = BufNovo(1.9f);
    float queda = 0.24f + 0.02f * (float)v;
    float fl = 0.0f, fm = 0.0f, fs = 0.0f, fz1 = 0.0f, fz2 = 0.0f, tremor = 1.0f;
    Svf lpT = { 0 }, lpZ = { 0 }, hpL = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float s = 0.0f;
        // 1) descida do raio
        if (t < queda + 0.05f) {
            float k = fminf(t / queda, 1.0f);
            float f = 140.0f + 2600.0f * powf(1.0f - k, 2.2f);
            fl += f / TAXA_SOM;
            fm += f * 1.5f / TAXA_SOM;
            float laser = sinf(2.0f * PI * fl + 2.5f * sinf(2.0f * PI * fm)) * (0.25f + 0.75f * k);
            float corte = (t > queda) ? 1.0f - (t - queda) / 0.05f : 1.0f;
            s += SvfPasso(&hpL, laser, 120.0f, 0.7f, 2) * 0.55f * corte;
        }
        // 2) impacto
        float ti = t - queda;
        if (ti >= 0.0f) {
            fs += (30.0f + 70.0f * expf(-ti / 0.05f)) / TAXA_SOM;
            float sub = sinf(2.0f * PI * fs) * EnvAD(ti, 0.002f, 0.32f);
            sub = tanhf(sub * 2.2f);                                       // saturado: aparece até em alto-falante pequeno
            if ((i % 300) == 0) tremor = 0.55f + 0.45f * SinRndU();   // o trovão "rola"
            float trovao = SvfPasso(&lpT, SinRnd(), 180.0f + 3800.0f * expf(-ti / 0.12f), 0.5f, 0) * EnvAD(ti, 0.001f, 0.38f) * tremor;
            // 3) a coluna de luz zumbindo
            fz1 += (55.0f + 2.0f * (float)v) / TAXA_SOM;
            fz2 += (55.6f + 2.0f * (float)v) / TAXA_SOM;
            float zum = SvfPasso(&lpZ, OscSerra(fz1) + OscSerra(fz2) + 0.5f * OscSerra(fz1 * 2.0f), 900.0f * expf(-ti / 0.5f) + 200.0f, 0.6f, 0)
                      * EnvAD(ti, 0.03f, 0.35f);
            s += sub * 0.95f + trovao * 1.1f + zum * 0.35f;
        }
        b.x[i] = s;
    }
    ReverbPedra(&b, 0.30f, 1.6f, 0.35f);
    return BufParaSom(&b, 0.95f);
}

// RAIO CLÁSSICO (ciano, raio que atravessa tudo): "zap" de FM descendente + estalos elétricos
static Sound ReceitaRaioClassico(int v)
{
    BufSom b = BufNovo(0.35f);
    float fc = 0.0f, fm = 0.0f;
    Svf hp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float f = 520.0f + (1900.0f + 150.0f * (float)v) * expf(-t / 0.05f);
        fc += f / TAXA_SOM;
        fm += (f * (1.41f + 0.1f * (float)v)) / TAXA_SOM;
        float zap = sinf(2.0f * PI * fc + 3.0f * expf(-t / 0.07f) * sinf(2.0f * PI * fm));
        float estalo = (SinRndU() > 0.985f) ? SinRnd() : 0.0f;
        estalo = SvfPasso(&hp, estalo, 4000.0f, 1.0f, 2);
        b.x[i] = (zap * 0.7f + estalo * 1.2f) * EnvAD(t, 0.001f, 0.08f);
    }
    ReverbPedra(&b, 0.18f, 0.9f, 0.4f);
    return BufParaSom(&b, 0.62f);
}

// =============================================================
// SONS DA LUCI (luta final) — graves, pesados, digitais. Nada de tom fino sustentado (vira assovio).
// =============================================================

// Corrupção (cena da arena, 2 s): ruído e serras desafinadas subindo, tremolo acelerando, resolução caindo
static Sound ReceitaLuciCorrupcao(int v)
{
    BufSom b = BufNovo(2.05f);
    float f1 = 0.0f, f2 = 0.0f, f3 = 0.0f, trem = 0.0f;
    Svf lp = { 0 }, bp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, k = fminf(t / 2.0f, 1.0f);
        float sobe = powf(2.0f, k * 1.2f);                                  // sobe ~1 oitava
        f1 += 55.0f * sobe / TAXA_SOM; f2 += 55.8f * sobe / TAXA_SOM; f3 += 110.6f * sobe / TAXA_SOM;
        float serras = SvfPasso(&lp, OscSerra(f1) + OscSerra(f2) + 0.6f * OscSerra(f3), 180.0f + 2200.0f * k * k, 0.8f, 0);
        float ruido = SvfPasso(&bp, SinRnd(), 200.0f + 2800.0f * k, 0.6f, 1);
        trem += (4.0f + 16.0f * k * k) / TAXA_SOM;
        float tremolo = 0.65f + 0.35f * sinf(2.0f * PI * trem);
        float s = (serras * 0.7f + ruido * 0.5f * k) * tremolo * (0.15f + 0.85f * k * k);
        b.x[i] = Esmagar(s, 8 - (int)(4.0f * k));
    }
    ReverbPedra(&b, 0.20f + 0.03f * (float)v, 1.2f, 0.35f);
    return BufParaSom(&b, 0.80f);
}

// Explosão de estática (cena da arena e 2ª queda): soco sub-grave saturado, estouro de ruído e chiado
static Sound ReceitaLuciExplosao(int v)
{
    BufSom b = BufNovo(1.9f);
    float fs = 0.0f, amostra = 0.0f;
    int segura = 0;
    Svf lp = { 0 }, hp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        fs += (28.0f + 70.0f * expf(-t / 0.06f) + 2.0f * (float)v) / TAXA_SOM;
        float sub = tanhf(2.5f * sinf(2.0f * PI * fs)) * EnvAD(t, 0.002f, 0.45f);
        float estouro = SvfPasso(&lp, SinRnd(), 200.0f + 7000.0f * expf(-t / 0.10f), 0.6f, 0) * EnvAD(t, 0.001f, 0.30f);
        if (--segura <= 0) { amostra = SinRnd(); segura = 1 + (int)(t * 40.0f); }
        float chiado = SvfPasso(&hp, amostra * ((SinRndU() > 0.6f) ? 1.0f : 0.2f), 900.0f, 0.7f, 2) * EnvAD(t, 0.05f, 0.55f);
        b.x[i] = sub * 1.0f + estouro * 1.1f + chiado * 0.45f;
    }
    ReverbPedra(&b, 0.32f, 1.6f, 0.35f);
    return BufParaSom(&b, 0.95f);
}

// O fio de luz se rompe (cena da separação): estalo elétrico seco, "tic" de vidro curtinho e um zap que cai
static Sound ReceitaLuciFio(int v)
{
    BufSom b = BufNovo(0.9f);
    float fc = 0.0f, fm = 0.0f, fr = 2300.0f + 120.0f * (float)v;
    Svf hp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float estalo = SvfPasso(&hp, SinRnd(), 2500.0f, 0.8f, 2) * EnvAD(t, 0.0003f, 0.012f);
        float tic = (sinf(2.0f * PI * fr * t) + 0.6f * sinf(2.0f * PI * fr * 1.57f * t)) * EnvAD(t, 0.0005f, 0.025f);
        float f = 140.0f + 900.0f * expf(-t / 0.09f);
        fc += f / TAXA_SOM; fm += f * 1.41f / TAXA_SOM;
        float zap = sinf(2.0f * PI * fc + 2.2f * expf(-t / 0.1f) * sinf(2.0f * PI * fm)) * EnvAD(t, 0.002f, 0.14f);
        b.x[i] = estalo * 1.0f + tic * 0.25f + zap * 0.55f;
    }
    ReverbPedra(&b, 0.30f, 1.2f, 0.35f);
    return BufParaSom(&b, 0.70f);
}

// Telegrafia da investida: rosnado grave que cresce + estalos digitais cada vez mais rápidos
static Sound ReceitaLuciTelegrafia(int v)
{
    BufSom b = BufNovo(0.75f);
    float fa = 0.0f, fm = 0.0f, prox = 0.0f;
    Svf lp = { 0 }, bp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, k = fminf(t / 0.65f, 1.0f);
        float f = 62.0f + 40.0f * k + 3.0f * (float)v;
        fa += f / TAXA_SOM; fm += f * 2.01f / TAXA_SOM;
        float rosnado = SvfPasso(&lp, OscSerra(fa + 0.6f * sinf(2.0f * PI * fm) / (2.0f * PI)), 260.0f + 1500.0f * k, 1.1f, 0);
        float tique = 0.0f;
        if (t >= prox && t < 0.65f) { prox = t + 0.11f - 0.08f * k; }
        float dtq = t - (prox - (0.11f - 0.08f * k));
        if (dtq >= 0.0f && dtq < 0.006f) tique = SvfPasso(&bp, SinRnd(), 1800.0f, 0.5f, 1) * 3.0f;
        float env = (t < 0.65f) ? (0.25f + 0.75f * k * k) : expf(-(t - 0.65f) / 0.03f);
        b.x[i] = tanhf((rosnado * 1.3f) * env * 1.6f) * 0.8f + tique * 0.35f;
    }
    ReverbPedra(&b, 0.18f, 1.0f, 0.4f);
    return BufParaSom(&b, 0.75f);
}

// Investida: whoosh grande e grave, soco na partida e um zunido metálico grave (a lâmina da falácia)
static Sound ReceitaLuciInvestida(int v)
{
    BufSom b = BufNovo(0.8f);
    float fs = 0.0f, d = 0.30f + 0.02f * (float)v, fm = 170.0f + 10.0f * (float)v;
    Svf l1 = { 0 }, h1 = { 0 }, l2 = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, k = fminf(t / d, 1.0f);
        float env = (t < d) ? ((k < 0.35f) ? k / 0.35f : powf(1.0f - (k - 0.35f) / 0.65f, 1.3f)) : 0.0f;
        float c = 220.0f + 2300.0f * ((k < 0.35f) ? k / 0.35f : 1.0f - (k - 0.35f) / 0.65f);
        float ar = SvfPasso(&h1, SvfPasso(&l1, SinRnd(), c, 1.2f, 0), c * 0.22f, 1.2f, 2) * env;
        fs += (50.0f + 60.0f * expf(-t / 0.03f)) / TAXA_SOM;
        float soco = sinf(2.0f * PI * fs) * EnvAD(t, 0.001f, 0.08f);
        float metal = (sinf(2.0f * PI * fm * t) + 0.7f * sinf(2.0f * PI * fm * 2.76f * t) * expf(-t / 0.1f)) * env * 0.5f;
        float grave = SvfPasso(&l2, SinRnd(), 160.0f, 0.9f, 0) * env;
        b.x[i] = tanhf((ar * 1.5f + soco * 1.1f + metal * 0.35f + grave * 1.2f) * 1.3f);
    }
    ReverbPedra(&b, 0.20f, 1.1f, 0.4f);
    return BufParaSom(&b, 0.85f);
}

// Orbe de falácia lançado: "thoom" grave com bamboleio e um sopro
static Sound ReceitaLuciOrbe(int v)
{
    BufSom b = BufNovo(0.6f);
    float fc = 0.0f, fm = 0.0f;
    Svf lp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float f = (55.0f + 70.0f * expf(-t / 0.08f)) * (1.0f + 0.06f * sinf(2.0f * PI * 11.0f * t)) * (1.0f + 0.05f * (float)v);
        fc += f / TAXA_SOM; fm += f * 1.5f / TAXA_SOM;
        float corpo = sinf(2.0f * PI * fc + 1.4f * expf(-t / 0.1f) * sinf(2.0f * PI * fm)) * EnvAD(t, 0.004f, 0.16f);
        float sopro = SvfPasso(&lp, SinRnd(), 900.0f * expf(-t / 0.1f) + 150.0f, 0.7f, 0) * EnvAD(t, 0.002f, 0.07f);
        b.x[i] = tanhf((corpo * 1.1f + sopro * 0.8f) * 1.4f);
    }
    ReverbPedra(&b, 0.22f, 1.0f, 0.4f);
    return BufParaSom(&b, 0.70f);
}

// Orbe rebatido acerta a Luci: choque de metal grave + soco + crepitar
static Sound ReceitaLuciRebatido(int v)
{
    BufSom b = BufNovo(1.0f);
    float fs = 0.0f, f = 240.0f + 12.0f * (float)v;
    Svf lp = { 0 }, hp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        fs += (45.0f + 100.0f * expf(-t / 0.025f)) / TAXA_SOM;
        float soco = tanhf(2.0f * sinf(2.0f * PI * fs)) * EnvAD(t, 0.001f, 0.12f);
        float metal = (sinf(2.0f * PI * f * t) + 0.8f * sinf(2.0f * PI * f * 2.32f * t) * expf(-t / 0.18f)
                     + 0.5f * sinf(2.0f * PI * f * 4.25f * t) * expf(-t / 0.08f) + 0.5f * sinf(2.0f * PI * f * 1.006f * t)) * EnvAD(t, 0.001f, 0.30f);
        float estalo = SvfPasso(&hp, SinRnd(), 1500.0f, 0.8f, 2) * EnvAD(t, 0.0004f, 0.015f)
                     + SvfPasso(&lp, SinRnd(), 2500.0f * expf(-t / 0.05f) + 200.0f, 0.8f, 0) * EnvAD(t, 0.0005f, 0.06f);
        b.x[i] = soco + metal * 0.30f + estalo * 0.9f;
    }
    ReverbPedra(&b, 0.28f, 1.3f, 0.35f);
    return BufParaSom(&b, 0.90f);
}

// Varredura de dados (o feixe girando, ~1,9 s): zap de partida e um zumbido grave de laser com chiado
static Sound ReceitaLuciVarredura(int v)
{
    BufSom b = BufNovo(2.0f);
    float f1 = 0.0f, f2 = 0.0f, fr = 0.0f, fz = 0.0f;
    Svf lp = { 0 }, bp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float env = EnvAD(t, 0.01f, 9.0f) * ((t > 1.75f) ? expf(-(t - 1.75f) / 0.07f) : 1.0f);
        f1 += (82.4f + 2.0f * (float)v) / TAXA_SOM; f2 += (83.1f + 2.0f * (float)v) / TAXA_SOM; fr += 37.0f / TAXA_SOM;
        float zumbido = SvfPasso(&lp, OscSerra(f1) + OscSerra(f2) + 0.5f * OscSerra(f1 * 2.0f), 700.0f + 250.0f * sinf(2.0f * PI * 3.0f * t), 0.9f, 0);
        zumbido *= 0.7f + 0.3f * sinf(2.0f * PI * fr);
        float chiado = SvfPasso(&bp, SinRnd(), 2400.0f + 600.0f * sinf(2.0f * PI * 5.0f * t), 0.5f, 1) * 0.35f;
        float fzap = 150.0f + 1600.0f * expf(-t / 0.05f);
        fz += fzap / TAXA_SOM;
        float zap = sinf(2.0f * PI * fz) * EnvAD(t, 0.001f, 0.08f);
        b.x[i] = tanhf((zumbido * 1.1f + chiado) * env * 1.3f) * 0.85f + zap * 0.6f;
    }
    ReverbPedra(&b, 0.20f, 1.2f, 0.4f);
    return BufParaSom(&b, 0.80f);
}

// Onda de choque: estrondo grave e um sopro que se afasta (o filtro vai fechando)
static Sound ReceitaLuciOnda(int v)
{
    BufSom b = BufNovo(1.1f);
    float fs = 0.0f;
    Svf bp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        fs += (40.0f + 60.0f * expf(-t / 0.05f) + 2.0f * (float)v) / TAXA_SOM;
        float boom = tanhf(2.2f * sinf(2.0f * PI * fs)) * EnvAD(t, 0.002f, 0.25f);
        float sopro = SvfPasso(&bp, SinRnd(), 1800.0f * expf(-t / 0.3f) + 180.0f, 0.5f, 1) * EnvAD(t, 0.03f, 0.35f);
        b.x[i] = boom * 1.0f + sopro * 0.9f;
    }
    ReverbPedra(&b, 0.30f, 1.5f, 0.35f);
    return BufParaSom(&b, 0.85f);
}

// Chão rachando: estalos esparsos e um rangido grave
static Sound ReceitaLuciRacha(int v)
{
    BufSom b = BufNovo(0.55f);
    float fr = 0.0f;
    Svf bp = { 0 }, lp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float impulso = (SinRndU() > 0.9965f - 0.001f * (float)v) ? SinRnd() * 4.0f : 0.0f;
        float estalos = SvfPasso(&bp, impulso, 1400.0f, 0.6f, 1) * EnvAD(t, 0.01f, 0.25f);
        fr += (70.0f + 25.0f * sinf(2.0f * PI * 6.0f * t)) / TAXA_SOM;
        float rangido = SvfPasso(&lp, OscSerra(fr), 300.0f, 1.0f, 0) * EnvAD(t, 0.05f, 0.18f);
        b.x[i] = estalos * 1.0f + rangido * 0.5f;
    }
    ReverbPedra(&b, 0.18f, 0.9f, 0.4f);
    return BufParaSom(&b, 0.55f);
}

// Chão caindo: baque e um ronco que some no fundo
static Sound ReceitaLuciQueda(int v)
{
    BufSom b = BufNovo(1.0f);
    float fs = 0.0f;
    Svf lp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        fs += (45.0f + 50.0f * expf(-t / 0.04f)) / TAXA_SOM;
        float baque = sinf(2.0f * PI * fs) * EnvAD(t, 0.002f, 0.12f);
        float ronco = SvfPasso(&lp, SinRnd(), 160.0f + 40.0f * (float)v, 0.9f, 0) * EnvAD(t, 0.02f, 0.35f);
        b.x[i] = baque * 1.0f + ronco * 1.6f;
    }
    ReverbPedra(&b, 0.30f, 1.4f, 0.3f);
    return BufParaSom(&b, 0.70f);
}

// 1ª queda (colapso): a máquina desligando — acorde que despenca duas oitavas, filtro fechando, crepitar
static Sound ReceitaLuciColapso(int v)
{
    BufSom b = BufNovo(2.0f);
    float f1 = 0.0f, f2 = 0.0f, f3 = 0.0f;
    Svf lp = { 0 }, hp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, k = fminf(t / 1.6f, 1.0f);
        float cai = powf(2.0f, -2.0f * k * k);
        f1 += 220.0f * cai / TAXA_SOM; f2 += 261.6f * cai / TAXA_SOM; f3 += 329.6f * cai * (1.0f + 0.004f * (float)v) / TAXA_SOM;
        float acorde = SvfPasso(&lp, OscSerra(f1) + OscSerra(f2) * 0.8f + OscSerra(f3) * 0.6f, 3000.0f * (1.0f - k) + 120.0f, 0.8f, 0);
        float crepita = (SinRndU() > 0.995f) ? SinRnd() * 2.0f : 0.0f;
        crepita = SvfPasso(&hp, crepita, 2000.0f, 0.7f, 2) * k;
        float env = (t < 1.6f) ? 1.0f : expf(-(t - 1.6f) / 0.08f);
        b.x[i] = Esmagar((acorde * 0.6f * (1.0f - 0.5f * k) + crepita * 0.8f) * env, 8 - (int)(3.0f * k));
    }
    ReverbPedra(&b, 0.30f, 1.5f, 0.35f);
    return BufParaSom(&b, 0.75f);
}

// A barra renasce: ruído ao contrário crescendo, acorde menor subindo e um golpe no fim
static Sound ReceitaLuciRenascer(int v)
{
    BufSom b = BufNovo(1.6f);
    float f1 = 0.0f, f2 = 0.0f, f3 = 0.0f, fs = 0.0f;
    float fim = 1.25f;
    Svf bp = { 0 }, lp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, k = fminf(t / fim, 1.0f);
        float sobe = powf(2.0f, k * 1.0f);
        f1 += 110.0f * sobe / TAXA_SOM; f2 += 130.8f * sobe / TAXA_SOM; f3 += 164.8f * sobe * (1.0f + 0.003f * (float)v) / TAXA_SOM;
        float acorde = SvfPasso(&lp, OscPulso(f1, 0.3f) + OscPulso(f2, 0.3f) + OscPulso(f3, 0.3f), 400.0f + 2400.0f * k, 0.8f, 0);
        float ruido = SvfPasso(&bp, SinRnd(), 300.0f + 3000.0f * k, 0.5f, 1);
        float env = (t < fim) ? k * k : 0.0f;
        float golpe = 0.0f;
        if (t >= fim) {
            float tg = t - fim;
            fs += (45.0f + 80.0f * expf(-tg / 0.03f)) / TAXA_SOM;
            golpe = tanhf(2.0f * sinf(2.0f * PI * fs)) * EnvAD(tg, 0.001f, 0.12f);
        }
        b.x[i] = Esmagar((acorde * 0.35f + ruido * 0.6f) * env, 7) + golpe * 1.0f;
    }
    ReverbPedra(&b, 0.28f, 1.4f, 0.35f);
    return BufParaSom(&b, 0.85f);
}

// Superaquecida: chiado de vapor e estalinhos de metal esfriando
static Sound ReceitaLuciVapor(int v)
{
    BufSom b = BufNovo(1.3f);
    Svf hp = { 0 }, bp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float vapor = SvfPasso(&hp, SinRnd(), 3000.0f - 600.0f * (float)v, 0.7f, 2) * EnvAD(t, 0.03f, 0.40f);
        float tic = (SinRndU() > 0.9985f) ? SinRnd() * 3.0f : 0.0f;
        tic = SvfPasso(&bp, tic, 2600.0f, 0.3f, 1) * EnvAD(t, 0.1f, 0.6f);
        b.x[i] = vapor * 0.45f + tic * 0.6f;
    }
    ReverbPedra(&b, 0.15f, 0.9f, 0.4f);
    return BufParaSom(&b, 0.50f);
}
// =============================================================
// SONS DOS INIMIGOS DA BIBLIOTECA (mesma linguagem da luta da Luci: grave, com corpo, sem apito)
// =============================================================

// Bote (Fragmento, Phisher e a cópia): sopro curto que corta o ar, soco grave e o dado "picotando"
static Sound ReceitaIniBote(int v)
{
    BufSom b = BufNovo(0.42f);
    float fs = 0.0f, amostra = 0.0f, d = 0.24f + 0.015f * (float)v;
    int segura = 0;
    Svf l1 = { 0 }, h1 = { 0 }, l2 = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, k = fminf(t / d, 1.0f);
        float subida = (k < 0.25f) ? k / 0.25f : 1.0f - (k - 0.25f) / 0.75f;
        float env = (t < d) ? ((k < 0.25f) ? k / 0.25f : powf(1.0f - (k - 0.25f) / 0.75f, 1.5f)) : 0.0f;
        float c = 260.0f + 1150.0f * subida;
        if (--segura <= 0) { amostra = SinRnd(); segura = 2 + (int)(6.0f * k) + v; }      // ruído picotado (dado corrompido)
        float ar = SvfPasso(&h1, SvfPasso(&l1, amostra, c, 1.1f, 0), c * 0.25f, 1.1f, 2) * env;
        fs += (48.0f + 80.0f * expf(-t / 0.025f)) / TAXA_SOM;
        float soco = sinf(2.0f * PI * fs) * EnvAD(t, 0.001f, 0.06f);
        float grave = SvfPasso(&l2, SinRnd(), 180.0f, 0.9f, 0) * env;
        b.x[i] = Esmagar(tanhf((ar * 1.4f + soco * 1.0f + grave * 1.0f) * 1.3f), 7);
    }
    ReverbPedra(&b, 0.14f, 0.9f, 0.45f);
    return BufParaSom(&b, 0.70f);
}

// Leque do Guardião (o tomo): páginas batendo, soco grave e um "thum" de dados saindo
static Sound ReceitaIniLeque(int v)
{
    BufSom b = BufNovo(0.55f);
    float fs = 0.0f, fc = 0.0f;
    Svf lp = { 0 }, l2 = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float paginas = 0.55f + 0.45f * sinf(2.0f * PI * (38.0f + 6.0f * (float)v) * t);
        float folha = SvfPasso(&lp, SinRnd(), 500.0f + 1500.0f * expf(-t / 0.05f), 0.9f, 0) * paginas * EnvAD(t, 0.002f, 0.07f);
        fs += (55.0f + 60.0f * expf(-t / 0.03f)) / TAXA_SOM;
        float soco = tanhf(1.8f * sinf(2.0f * PI * fs)) * EnvAD(t, 0.001f, 0.09f);
        float f = 98.0f * (1.0f + 0.04f * (float)v) * (1.0f - 0.25f * fminf(t / 0.3f, 1.0f));
        fc += f / TAXA_SOM;
        float thum = SvfPasso(&l2, OscTri(fc) + 0.5f * OscSerra(fc * 1.5f), 500.0f, 0.9f, 0) * EnvAD(t, 0.004f, 0.14f);
        b.x[i] = tanhf((folha * 1.2f + soco * 1.0f + thum * 0.7f) * 1.2f);
    }
    ReverbPedra(&b, 0.22f, 1.0f, 0.4f);
    return BufParaSom(&b, 0.70f);
}

// Pacote do Botnet: "bwop" digital grave (pulso esmagado descendo) com um estalo de rede
static Sound ReceitaIniPacote(int v)
{
    BufSom b = BufNovo(0.32f);
    float fa = 0.0f;
    Svf lp = { 0 }, hp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float f = (85.0f + 120.0f * expf(-t / 0.04f)) * (1.0f + 0.05f * (float)v);
        fa += f / TAXA_SOM;
        float corpo = SvfPasso(&lp, OscPulso(fa, 0.3f), 300.0f + 1500.0f * expf(-t / 0.05f), 0.9f, 0) * EnvAD(t, 0.002f, 0.08f);
        float estalo = SvfPasso(&hp, SinRnd(), 2000.0f, 0.9f, 2) * EnvAD(t, 0.0005f, 0.006f);
        b.x[i] = Esmagar(corpo * 1.0f + estalo * 0.5f, 5);
    }
    ReverbPedra(&b, 0.16f, 0.9f, 0.45f);
    return BufParaSom(&b, 0.60f);
}

// Rajada do Keylogger: cada tiro é uma TECLA batendo (clique mecânico com corpo grave) e um sopro curto
static Sound ReceitaIniTecla(int v)
{
    BufSom b = BufNovo(0.22f);
    float fs = 0.0f;
    Svf bp = { 0 }, b2 = { 0 }, lp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float clique = SvfPasso(&bp, SinRnd(), 1600.0f + 200.0f * (float)v, 1.1f, 1) * EnvAD(t, 0.0003f, 0.008f);
        float volta = (t > 0.018f) ? SvfPasso(&b2, SinRnd(), 1000.0f, 1.1f, 1) * EnvAD(t - 0.018f, 0.0003f, 0.006f) : 0.0f;
        fs += (70.0f + 110.0f * expf(-t / 0.015f)) / TAXA_SOM;
        float corpo = sinf(2.0f * PI * fs) * EnvAD(t, 0.001f, 0.04f);
        float sopro = SvfPasso(&lp, SinRnd(), 700.0f, 0.9f, 0) * EnvAD(t, 0.003f, 0.05f);
        b.x[i] = tanhf((clique * 1.3f + volta * 0.6f + corpo * 1.0f + sopro * 0.6f) * 1.3f);
    }
    ReverbPedra(&b, 0.12f, 0.8f, 0.5f);
    return BufParaSom(&b, 0.60f);
}

// Anel de balas (Cache, Verboso, Núcleo): "whum" grave que se abre para fora (sub descendo + ruído que abre e fecha)
static Sound ReceitaIniAnel(int v)
{
    BufSom b = BufNovo(0.85f);
    float fc = 0.0f, fm = 0.0f;
    Svf lp = { 0 }, bp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float f = (46.0f + 60.0f * expf(-t / 0.07f)) * (1.0f + 0.04f * (float)v);
        fc += f / TAXA_SOM; fm += f * 2.0f / TAXA_SOM;
        float whum = tanhf(1.6f * sinf(2.0f * PI * fc + 1.2f * expf(-t / 0.12f) * sinf(2.0f * PI * fm))) * EnvAD(t, 0.006f, 0.22f);
        float abre = SvfPasso(&lp, SinRnd(), 250.0f + 1400.0f * expf(-t / 0.09f), 0.8f, 0) * EnvAD(t, 0.004f, 0.12f);
        float corre = SvfPasso(&bp, SinRnd(), 500.0f + 700.0f * fminf(t / 0.25f, 1.0f), 1.3f, 1) * EnvAD(t, 0.03f, 0.10f);
        b.x[i] = whum * 1.0f + abre * 0.7f + corre * 0.3f;
    }
    ReverbPedra(&b, 0.26f, 1.2f, 0.4f);
    return BufParaSom(&b, 0.72f);
}

// Eco falso estoura: ruído picotado que gagueja (o eco se repetindo) e um sub que cai
static Sound ReceitaIniEco(int v)
{
    BufSom b = BufNovo(0.75f);
    float fs = 0.0f, amostra = 0.0f;
    int segura = 0;
    Svf lp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        fs += (36.0f + 90.0f * expf(-t / 0.05f)) / TAXA_SOM;
        float sub = tanhf(2.2f * sinf(2.0f * PI * fs)) * EnvAD(t, 0.001f, 0.18f);
        if (--segura <= 0) { amostra = SinRnd(); segura = 1 + (int)(t * 70.0f); }
        float gagueja = (fmodf(t, 0.045f + 0.02f * t + 0.004f * (float)v) < 0.025f) ? 1.0f : 0.25f;
        float estouro = SvfPasso(&lp, amostra, 300.0f + 4500.0f * expf(-t / 0.08f), 0.7f, 0) * gagueja * EnvAD(t, 0.001f, 0.20f);
        b.x[i] = Esmagar(sub * 1.0f + estouro * 1.0f, 6);
    }
    ReverbPedra(&b, 0.25f, 1.2f, 0.4f);
    return BufParaSom(&b, 0.78f);
}

// Barreira do Firewall sobe: estalo de energia + zumbido elétrico grave (duas serras) que abre e assenta
static Sound ReceitaIniBarreira(int v)
{
    BufSom b = BufNovo(1.1f);
    float f1 = 0.0f, f2 = 0.0f;
    Svf lp = { 0 }, hp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float base = 55.0f * (1.0f + 0.03f * (float)v);
        f1 += base / TAXA_SOM; f2 += base * 1.505f / TAXA_SOM;
        float corte = 200.0f + 1600.0f * expf(-t / 0.12f) + 150.0f * sinf(2.0f * PI * 6.0f * t);
        float zumbe = SvfPasso(&lp, OscSerra(f1) + 0.7f * OscSerra(f2), corte, 0.9f, 0);
        float env = (t < 0.03f) ? t / 0.03f : 0.55f + 0.45f * expf(-(t - 0.03f) / 0.15f);
        if (t > 0.75f) env *= expf(-(t - 0.75f) / 0.08f);
        float estalo = SvfPasso(&hp, SinRnd(), 1500.0f, 0.8f, 2) * EnvAD(t, 0.0005f, 0.02f);
        float crepita = (SinRndU() > 0.995f) ? SinRnd() * 0.6f * expf(-t / 0.4f) : 0.0f;
        b.x[i] = tanhf((zumbe * env * 1.2f + estalo * 1.2f + crepita) * 1.3f);
    }
    ReverbPedra(&b, 0.18f, 1.0f, 0.45f);
    return BufParaSom(&b, 0.68f);
}

// Overclock carregando: engrenagem rangendo (dentes cada vez mais rápidos) e um motor grave subindo.
// Dura 1 s; o jogo ajusta o tom para acabar junto com a carga sorteada.
static Sound ReceitaIniCarga(int v)
{
    BufSom b = BufNovo(1.05f);
    float fa = 0.0f, fd = 0.0f;
    Svf lp = { 0 }, bp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, k = fminf(t, 1.0f);
        fa += (45.0f + 80.0f * k * k + 2.0f * (float)v) / TAXA_SOM;
        float motor = SvfPasso(&lp, OscSerra(fa) + 0.5f * OscPulso(fa * 2.0f, 0.4f), 180.0f + 900.0f * k, 1.0f, 0);
        fd += (9.0f + 38.0f * k) / TAXA_SOM;                          // dentes da engrenagem
        float dente = fd - floorf(fd);
        float catraca = SvfPasso(&bp, (dente < 0.08f) ? SinRnd() * 2.0f : 0.0f, 900.0f + 500.0f * k, 1.0f, 1);
        float env = (0.25f + 0.75f * k) * ((t > 1.0f) ? expf(-(t - 1.0f) / 0.015f) : 1.0f);
        b.x[i] = tanhf((motor * 1.1f + catraca * 0.5f) * env * 1.4f);
    }
    ReverbPedra(&b, 0.12f, 0.9f, 0.45f);
    return BufParaSom(&b, 0.55f);
}

// Investida do Overclock: arranque de motor (serra saturada que dispara), soco e um sopro de ar quente
static Sound ReceitaIniInvestida(int v)
{
    BufSom b = BufNovo(0.6f);
    float fa = 0.0f, fs = 0.0f, d = 0.36f;
    Svf lp = { 0 }, l2 = { 0 }, h2 = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, k = fminf(t / d, 1.0f);
        float f = (70.0f + 90.0f * (1.0f - expf(-t / 0.05f)) - 40.0f * k) * (1.0f + 0.04f * (float)v);
        fa += f / TAXA_SOM;
        float env = (t < d) ? powf(1.0f - k, 1.2f) : 0.0f;
        float motor = SvfPasso(&lp, OscSerra(fa) + 0.6f * OscSerra(fa * 1.01f + 0.3f), 1200.0f, 0.9f, 0) * env;
        fs += (50.0f + 70.0f * expf(-t / 0.02f)) / TAXA_SOM;
        float soco = sinf(2.0f * PI * fs) * EnvAD(t, 0.001f, 0.07f);
        float c = 400.0f + 1800.0f * (1.0f - k);
        float ar = SvfPasso(&h2, SvfPasso(&l2, SinRnd(), c, 1.1f, 0), c * 0.3f, 1.1f, 2) * env;
        b.x[i] = tanhf((motor * 1.3f + soco * 1.1f + ar * 1.0f) * 1.5f);
    }
    ReverbPedra(&b, 0.18f, 1.0f, 0.4f);
    return BufParaSom(&b, 0.82f);
}

// Overclock superaquecido: "clonk" de metal pesado (parciais graves fora de escala), chiado de vapor e estalos esfriando
static Sound ReceitaIniSuperaquece(int v)
{
    BufSom b = BufNovo(1.0f);
    float m = 1.0f + 0.03f * (float)v;
    Svf hp = { 0 }, lp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float clonk = sinf(2.0f * PI * 150.0f * m * t) * expf(-t / 0.18f) + 0.6f * sinf(2.0f * PI * 413.0f * m * t) * expf(-t / 0.09f)
                    + 0.35f * sinf(2.0f * PI * 687.0f * m * t) * expf(-t / 0.05f);
        float u = t - 0.05f;
        float vapor = (u > 0.0f) ? SvfPasso(&lp, SvfPasso(&hp, SinRnd(), 700.0f, 0.9f, 2), 2600.0f, 0.9f, 0) * EnvAD(u, 0.06f, 0.30f) : 0.0f;
        float estala = (t > 0.25f && SinRndU() > 0.9985f) ? SinRnd() * 0.8f : 0.0f;
        b.x[i] = clonk * 1.0f * fminf(t / 0.001f, 1.0f) + vapor * 0.22f + estala * 0.35f;
    }
    ReverbPedra(&b, 0.22f, 1.0f, 0.4f);
    return BufParaSom(&b, 0.65f);
}

// Cache se cura: acorde grave e quente (triângulos) subindo uma quinta, com um brilho suave por cima
static Sound ReceitaIniCura(int v)
{
    BufSom b = BufNovo(0.9f);
    float f1 = 0.0f, f2 = 0.0f, f3 = 0.0f;
    Svf lp = { 0 }, l2 = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, k = fminf(t / 0.45f, 1.0f);
        float s = k * k * (3.0f - 2.0f * k);
        float base = 110.0f * (1.0f + 0.03f * (float)v) * (1.0f + 0.5f * s);
        f1 += base / TAXA_SOM; f2 += base * 1.5f / TAXA_SOM; f3 += base * 2.0f / TAXA_SOM;
        float acorde = SvfPasso(&lp, OscTri(f1) + 0.6f * OscTri(f2) + 0.35f * OscTri(f3), 500.0f + 900.0f * s, 0.9f, 0);
        float env = (t < 0.08f) ? t / 0.08f : ((t < 0.5f) ? 1.0f : expf(-(t - 0.5f) / 0.14f));
        float brilho = SvfPasso(&l2, SinRnd(), 2500.0f, 0.9f, 0) * 0.15f * (0.6f + 0.4f * sinf(2.0f * PI * 14.0f * t));
        b.x[i] = (acorde * 0.9f + brilho) * env;
    }
    ReverbPedra(&b, 0.28f, 1.1f, 0.4f);
    return BufParaSom(&b, 0.55f);
}

// Alucinação some e reaparece atirando: sopro ao contrário (some), "vup" grave (aparece) e o soco do tiro
static Sound ReceitaIniFantasma(int v)
{
    BufSom b = BufNovo(0.55f);
    float fc = 0.0f, fs = 0.0f, pico = 0.08f;
    Svf bp = { 0 }, lp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float cresce = (t < pico) ? powf(t / pico, 2.0f) : expf(-(t - pico) / 0.03f);
        float sopro = SvfPasso(&bp, SinRnd(), 300.0f + 1200.0f * fminf(t / pico, 1.0f), 1.1f, 1) * cresce;
        float u = t - pico, vup = 0.0f, soco = 0.0f;
        if (u >= 0.0f) {
            fc += (70.0f + 160.0f * expf(-u / 0.04f)) * (1.0f + 0.05f * (float)v) / TAXA_SOM;
            vup = SvfPasso(&lp, OscTri(fc) + 0.4f * OscSerra(fc), 900.0f, 0.9f, 0) * EnvAD(u, 0.002f, 0.12f);
            fs += (48.0f + 60.0f * expf(-u / 0.02f)) / TAXA_SOM;
            soco = sinf(2.0f * PI * fs) * EnvAD(u, 0.001f, 0.06f);
        }
        b.x[i] = tanhf((sopro * 0.9f + vup * 1.0f + soco * 0.9f) * 1.3f);
    }
    ReverbPedra(&b, 0.30f, 1.2f, 0.35f);
    return BufParaSom(&b, 0.68f);
}

// Phisher se revela: um "plim" de notificação falso que despenca e vira rosnado (a isca virando anzol)
static Sound ReceitaIniPhisher(int v)
{
    BufSom b = BufNovo(0.7f);
    float fa = 0.0f, fb = 0.0f, fx = 0.0f, fr = 0.0f;
    Svf lp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float cai = (t < 0.07f) ? 1.0f : powf(2.0f, -2.2f * fminf((t - 0.07f) / 0.25f, 1.0f));     // despenca ~2 oitavas
        float f = 392.0f * (1.0f + 0.03f * (float)v) * cai;
        fa += f / TAXA_SOM; fb += f * 1.5f / TAXA_SOM; fx += f * 2.76f / TAXA_SOM;
        float plim = (sinf(2.0f * PI * fa) + 0.4f * sinf(2.0f * PI * fb) + 0.25f * sinf(2.0f * PI * fx) * expf(-t / 0.04f)) * EnvAD(t, 0.002f, 0.16f);
        fr += (55.0f + 10.0f * (float)v) / TAXA_SOM;
        float rosnado = (t > 0.12f) ? SvfPasso(&lp, OscSerra(fr) + 0.5f * OscSerra(fr * 1.02f), 380.0f, 1.0f, 0) * EnvAD(t - 0.12f, 0.06f, 0.18f) : 0.0f;
        b.x[i] = tanhf((plim * 0.6f + rosnado * 1.2f) * 1.4f);
    }
    ReverbPedra(&b, 0.22f, 1.0f, 0.4f);
    return BufParaSom(&b, 0.62f);
}

// Tiro barrado no escudo (frente do Firewall, barreira, lente da Sentinela): "klang" grave e curto
static Sound ReceitaIniBloqueio(int v)
{
    BufSom b = BufNovo(0.35f);
    float m = 1.0f + 0.05f * (float)v, fs = 0.0f;
    Svf bp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float klang = sinf(2.0f * PI * 230.0f * m * t) * expf(-t / 0.06f) + 0.7f * sinf(2.0f * PI * 611.0f * m * t) * expf(-t / 0.035f)
                    + 0.4f * sinf(2.0f * PI * 1043.0f * m * t) * expf(-t / 0.02f);
        float raspa = SvfPasso(&bp, SinRnd(), 1400.0f, 1.0f, 1) * EnvAD(t, 0.0005f, 0.015f);
        fs += (60.0f + 80.0f * expf(-t / 0.01f)) / TAXA_SOM;
        float soco = sinf(2.0f * PI * fs) * EnvAD(t, 0.001f, 0.03f);
        b.x[i] = tanhf((klang * 0.7f * fminf(t / 0.0005f, 1.0f) + raspa * 1.0f + soco * 0.8f) * 1.3f);
    }
    ReverbPedra(&b, 0.16f, 0.9f, 0.45f);
    return BufParaSom(&b, 0.55f);
}

// Inimigo se materializando: ruído digital grave ganhando resolução e um "tum" quando ele fica sólido (0,6 s)
static Sound ReceitaIniSurge(int v)
{
    BufSom b = BufNovo(0.75f);
    float amostra = 0.0f, fs = 0.0f, fim = 0.58f;
    int segura = 0;
    Svf lp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, k = fminf(t / fim, 1.0f);
        if (--segura <= 0) { amostra = SinRnd(); segura = 1 + (int)(40.0f * (1.0f - k)) + v; }
        float forma = SvfPasso(&lp, amostra, 200.0f + 1500.0f * k, 0.9f, 0) * (k * k) * ((t < fim) ? 1.0f : expf(-(t - fim) / 0.02f));
        float u = t - fim, tum = 0.0f;
        if (u >= 0.0f) { fs += (55.0f + 50.0f * expf(-u / 0.02f)) / TAXA_SOM; tum = sinf(2.0f * PI * fs) * EnvAD(u, 0.001f, 0.06f); }
        b.x[i] = Esmagar(forma * 0.8f + tum * 0.9f, 6);
    }
    ReverbPedra(&b, 0.20f, 1.0f, 0.4f);
    return BufParaSom(&b, 0.45f);
}

// =============================================================
// SONS DOS CHEFES E ELITES (Alucinador, Sentinela, Retropropagador, Janela de Contexto, Silogista)
// =============================================================

// Virada de fase (todos os chefes): o ar sendo puxado e o golpe, sub-grave, acorde de serras dissonante
// fechando, um ferro batendo e o estouro. O impacto vem 0,3 s depois do gatilho (junto com a faixa).
static Sound ReceitaChefeVirada(int v)
{
    BufSom b = BufNovo(1.8f);
    float fs = 0.0f, f1 = 0.0f, f2 = 0.0f, f3 = 0.0f, pico = 0.30f;
    Svf bp = { 0 }, lp = { 0 }, l2 = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, u = t - pico, s = 0.0f;
        if (u < 0.0f) {
            float k = t / pico;
            s = SvfPasso(&bp, SinRnd(), 200.0f + 1600.0f * k * k, 0.9f, 1) * k * k * k * 0.9f;
        } else {
            fs += (30.0f + 70.0f * expf(-u / 0.07f)) / TAXA_SOM;
            float boom = tanhf(2.6f * sinf(2.0f * PI * fs)) * EnvAD(u, 0.002f, 0.5f);
            float base = 55.0f * (1.0f + 0.02f * (float)v);
            f1 += base / TAXA_SOM; f2 += base * 1.06f / TAXA_SOM; f3 += base * 1.5f / TAXA_SOM;
            float acorde = SvfPasso(&lp, OscSerra(f1) + OscSerra(f2) + 0.7f * OscSerra(f3), 180.0f + 2200.0f * expf(-u / 0.25f), 0.8f, 0) * EnvAD(u, 0.004f, 0.55f);
            float ferro = sinf(2.0f * PI * 180.0f * u) * expf(-u / 0.25f) + 0.6f * sinf(2.0f * PI * 497.0f * u) * expf(-u / 0.12f)
                        + 0.35f * sinf(2.0f * PI * 811.0f * u) * expf(-u / 0.06f);
            float estouro = SvfPasso(&l2, SinRnd(), 300.0f + 4500.0f * expf(-u / 0.06f), 0.7f, 0) * EnvAD(u, 0.001f, 0.12f);
            s = boom * 1.0f + acorde * 0.8f + ferro * 0.3f * fminf(u / 0.001f, 1.0f) + estouro * 0.9f;
        }
        b.x[i] = Esmagar(tanhf(s * 1.2f), 8);
    }
    ReverbPedra(&b, 0.32f, 1.6f, 0.35f);
    return BufParaSom(&b, 0.92f);
}

// Chamando ajudantes (ecos, alucinações, reforços): portal digital gaguejando cada vez mais rápido e um "tum"
static Sound ReceitaChefeInvoca(int v)
{
    BufSom b = BufNovo(0.85f);
    float fa = 0.0f, fg = 0.0f, fs = 0.0f, fim = 0.55f;
    Svf lp = { 0 }, l2 = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, k = fminf(t / fim, 1.0f);
        fg += (8.0f + 30.0f * k) / TAXA_SOM;
        float porta = ((fg - floorf(fg)) < 0.5f) ? 1.0f : 0.15f;
        fa += (50.0f + 60.0f * k + 3.0f * (float)v) / TAXA_SOM;
        float sobe = SvfPasso(&lp, OscSerra(fa) + 0.5f * SinRnd(), 200.0f + 1100.0f * k, 0.9f, 0) * porta * (0.2f + 0.8f * k)
                   * ((t < fim) ? 1.0f : expf(-(t - fim) / 0.02f));
        float u = t - fim, tum = 0.0f;
        if (u >= 0.0f) {
            fs += (45.0f + 70.0f * expf(-u / 0.03f)) / TAXA_SOM;
            tum = tanhf(1.8f * sinf(2.0f * PI * fs)) * EnvAD(u, 0.001f, 0.1f) + SvfPasso(&l2, SinRnd(), 900.0f, 0.8f, 0) * EnvAD(u, 0.001f, 0.04f);
        }
        b.x[i] = Esmagar(sobe * 0.9f + tum * 1.0f, 6);
    }
    ReverbPedra(&b, 0.26f, 1.2f, 0.4f);
    return BufParaSom(&b, 0.72f);
}

// Escudo/armadura caindo (Alucinador, Janela, camadas do Silogista): estalo, cacos graves, ferro que afunda e um soco
static Sound ReceitaChefeEscudo(int v)
{
    BufSom b = BufNovo(1.1f);
    float fs = 0.0f, fm = 0.0f, grao = 0.0f, fg = 900.0f;
    Svf hp = { 0 }, bp = { 0 }, lp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float estalo = SvfPasso(&hp, SinRnd(), 1200.0f, 0.8f, 2) * EnvAD(t, 0.0005f, 0.03f);
        if (SinRndU() > 0.9975f - 0.002f * expf(-t / 0.2f)) { grao = 1.0f; fg = 450.0f + 1100.0f * SinRndU(); }
        grao *= 0.9993f;
        float cacos = SvfPasso(&bp, SinRnd() * grao, fg, 1.0f, 1) * expf(-t / 0.35f);
        fm += 210.0f * (1.0f + 0.03f * (float)v) * (1.0f - 0.35f * fminf(t / 0.6f, 1.0f)) / TAXA_SOM;
        float ferro = (sinf(2.0f * PI * fm) + 0.6f * sinf(2.0f * PI * fm * 2.71f) * expf(-t / 0.15f)) * EnvAD(t, 0.001f, 0.28f);
        fs += (40.0f + 80.0f * expf(-t / 0.04f)) / TAXA_SOM;
        float soco = tanhf(2.0f * sinf(2.0f * PI * fs)) * EnvAD(t, 0.001f, 0.16f);
        float corpo = SvfPasso(&lp, SinRnd(), 400.0f, 0.8f, 0) * EnvAD(t, 0.001f, 0.12f);
        b.x[i] = tanhf((estalo * 0.9f + cacos * 1.2f + ferro * 0.45f + soco * 1.1f + corpo * 0.9f) * 1.2f);
    }
    ReverbPedra(&b, 0.30f, 1.3f, 0.35f);
    return BufParaSom(&b, 0.85f);
}

// Chefe derrotado: estrondo, acorde grave que afunda uma oitava e o dado se desfazendo (perde resolução)
static Sound ReceitaChefeMorte(int v)
{
    BufSom b = BufNovo(2.6f);
    float fs = 0.0f, f1 = 0.0f, f2 = 0.0f, amostra = 0.0f;
    int segura = 0;
    Svf lp = { 0 }, l2 = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        fs += (26.0f + 60.0f * expf(-t / 0.1f)) / TAXA_SOM;
        float boom = tanhf(2.8f * sinf(2.0f * PI * fs)) * EnvAD(t, 0.002f, 0.8f);
        float base = 82.0f * (1.0f + 0.02f * (float)v) * powf(2.0f, -1.2f * fminf(t / 2.0f, 1.0f));
        f1 += base / TAXA_SOM; f2 += base * 1.49f / TAXA_SOM;
        float afunda = SvfPasso(&lp, OscSerra(f1) + 0.8f * OscSerra(f2) + 0.5f * OscPulso(f1 * 0.5f, 0.5f), 150.0f + 1800.0f * expf(-t / 0.6f), 0.8f, 0)
                     * EnvAD(t, 0.01f, 0.9f);
        if (--segura <= 0) { amostra = SinRnd(); segura = 1 + (int)(t * 50.0f); }
        float desfaz = SvfPasso(&l2, amostra * ((SinRndU() > 0.5f) ? 1.0f : 0.1f), 600.0f + 2200.0f * expf(-t / 0.5f), 0.7f, 0) * EnvAD(t, 0.05f, 0.9f);
        b.x[i] = Esmagar(tanhf((boom * 1.0f + afunda * 0.75f + desfaz * 0.6f) * 1.2f), 8 - (int)fminf(t * 2.0f, 4.0f));
    }
    ReverbPedra(&b, 0.35f, 1.6f, 0.35f);
    return BufParaSom(&b, 0.95f);
}

// Mira travando (investida do Alucinador, trava da Sentinela, refutação do Silogista): rosnado grave que sobe,
// cliques de foco cada vez mais rápidos e o "clack" da trava no fim (0,8 s; o jogo ajusta o tom ao aviso)
static Sound ReceitaChefeMira(int v)
{
    BufSom b = BufNovo(0.95f);
    float fa = 0.0f, fm = 0.0f, fc = 0.0f, dur = 0.8f;
    Svf lp = { 0 }, bp = { 0 }, b2 = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, k = fminf(t / dur, 1.0f);
        float f = 58.0f + 50.0f * k * k + 2.0f * (float)v;
        fa += f / TAXA_SOM; fm += f * 1.5f / TAXA_SOM;
        float rosna = SvfPasso(&lp, OscSerra(fa) + 0.5f * OscSerra(fm), 200.0f + 1000.0f * k, 1.0f, 0) * (0.2f + 0.8f * k) * ((t < dur) ? 1.0f : 0.0f);
        fc += (6.0f + 22.0f * k) / TAXA_SOM;
        float clique = SvfPasso(&bp, ((fc - floorf(fc)) < 0.04f && t < dur) ? SinRnd() * 2.0f : 0.0f, 1300.0f, 1.1f, 1);
        float u = t - dur, trava = 0.0f;
        if (u >= 0.0f) trava = SvfPasso(&b2, SinRnd(), 900.0f, 1.0f, 1) * EnvAD(u, 0.0005f, 0.02f) * 2.0f + sinf(2.0f * PI * 70.0f * u) * EnvAD(u, 0.001f, 0.05f);
        b.x[i] = tanhf((rosna * 1.1f + clique * 0.5f + trava * 1.0f) * 1.3f);
    }
    ReverbPedra(&b, 0.18f, 1.0f, 0.4f);
    return BufParaSom(&b, 0.70f);
}

// Investida do Alucinador: tempestade de páginas (ar picotado), soco e um grave que arrasta
static Sound ReceitaChefeInvestida(int v)
{
    BufSom b = BufNovo(0.75f);
    float fs = 0.0f, d = 0.45f, amostra = 0.0f;
    int segura = 0;
    Svf l1 = { 0 }, h1 = { 0 }, l2 = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, k = fminf(t / d, 1.0f);
        float forma = (k < 0.2f) ? k / 0.2f : 1.0f - (k - 0.2f) / 0.8f;
        float env = (t < d) ? ((k < 0.2f) ? k / 0.2f : powf(1.0f - (k - 0.2f) / 0.8f, 1.2f)) : 0.0f;
        float c = 250.0f + 1300.0f * forma;
        if (--segura <= 0) { amostra = SinRnd(); segura = 2 + v; }
        float paginas = 0.6f + 0.4f * sinf(2.0f * PI * 30.0f * t);
        float ar = SvfPasso(&h1, SvfPasso(&l1, amostra, c, 1.1f, 0), c * 0.25f, 1.1f, 2) * env * paginas;
        fs += (42.0f + 70.0f * expf(-t / 0.03f)) / TAXA_SOM;
        float soco = tanhf(2.0f * sinf(2.0f * PI * fs)) * EnvAD(t, 0.001f, 0.12f);
        float grave = SvfPasso(&l2, SinRnd(), 150.0f, 0.9f, 0) * env;
        b.x[i] = Esmagar(tanhf((ar * 1.5f + soco * 1.1f + grave * 1.3f) * 1.3f), 7);
    }
    ReverbPedra(&b, 0.22f, 1.2f, 0.4f);
    return BufParaSom(&b, 0.85f);
}

// Rajada em leque (Alucinador, resposta da Janela): três socos quase juntos, corpo FM grave e um sopro
static Sound ReceitaChefeRajada(int v)
{
    BufSom b = BufNovo(0.6f);
    float fc = 0.0f, fm = 0.0f, fs[3] = { 0 };
    Svf lp = { 0 }, l2 = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, s = 0.0f;
        for (int q = 0; q < 3; q++) {
            float u = t - 0.012f * (float)q;
            if (u < 0.0f) continue;
            fs[q] += (50.0f + 8.0f * (float)q + 70.0f * expf(-u / 0.025f)) / TAXA_SOM;
            s += sinf(2.0f * PI * fs[q]) * EnvAD(u, 0.001f, 0.07f) * 0.6f;
        }
        float f = (70.0f + 90.0f * expf(-t / 0.05f)) * (1.0f + 0.04f * (float)v);
        fc += f / TAXA_SOM; fm += f * 2.02f / TAXA_SOM;
        float corpo = SvfPasso(&lp, sinf(2.0f * PI * fc + 1.8f * expf(-t / 0.08f) * sinf(2.0f * PI * fm)), 900.0f, 0.8f, 0) * EnvAD(t, 0.002f, 0.14f);
        float sopro = SvfPasso(&l2, SinRnd(), 300.0f + 1300.0f * expf(-t / 0.05f), 0.8f, 0) * EnvAD(t, 0.001f, 0.06f);
        b.x[i] = tanhf((s * 1.2f + corpo * 0.9f + sopro * 0.8f) * 1.3f);
    }
    ReverbPedra(&b, 0.24f, 1.2f, 0.4f);
    return BufParaSom(&b, 0.78f);
}

// Espiral do Alucinador (3 s): rotor grave com os braços girando (o filtro abre e fecha a cada volta)
static Sound ReceitaChefeEspiral(int v)
{
    BufSom b = BufNovo(3.1f);
    float f1 = 0.0f, f2 = 0.0f, fr = 0.0f, fp = 0.0f;
    Svf lp = { 0 }, bp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float env = fminf(t / 0.25f, 1.0f) * ((t > 2.75f) ? expf(-(t - 2.75f) / 0.08f) : 1.0f);
        fr += (2.0f + 0.6f * fminf(t / 2.0f, 1.0f)) / TAXA_SOM;
        float giro = 0.5f + 0.5f * sinf(2.0f * PI * fr);
        f1 += (62.0f + 2.0f * (float)v) / TAXA_SOM; f2 += (62.0f * 1.5f + 1.3f) / TAXA_SOM;
        float rotor = SvfPasso(&lp, OscSerra(f1) + 0.6f * OscSerra(f2), 220.0f + 650.0f * giro, 0.9f, 0);
        float vento = SvfPasso(&bp, SinRnd(), 400.0f + 800.0f * giro, 1.2f, 1) * (0.3f + 0.7f * giro);
        fp += 11.0f / TAXA_SOM;
        float pulso = 0.75f + 0.25f * powf(1.0f - (fp - floorf(fp)), 6.0f);
        b.x[i] = tanhf((rotor * 0.9f + vento * 0.6f) * pulso * env * 1.3f);
    }
    ReverbPedra(&b, 0.22f, 1.2f, 0.4f);
    return BufParaSom(&b, 0.62f);
}

// Feixe da Sentinela: estrondo, zumbido elétrico grave (serra + pulso tremendo) e chiado de faísca
static Sound ReceitaSentinelaFeixe(int v)
{
    BufSom b = BufNovo(0.7f);
    float fs = 0.0f, f1 = 0.0f, f2 = 0.0f;
    Svf lp = { 0 }, hp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        fs += (35.0f + 90.0f * expf(-t / 0.04f)) / TAXA_SOM;
        float boom = tanhf(2.4f * sinf(2.0f * PI * fs)) * EnvAD(t, 0.001f, 0.18f);
        f1 += 55.0f * (1.0f + 0.03f * (float)v) / TAXA_SOM; f2 += 110.7f / TAXA_SOM;
        float zumbe = SvfPasso(&lp, OscSerra(f1) + 0.8f * OscPulso(f2, 0.3f), 1500.0f * expf(-t / 0.3f) + 250.0f, 0.8f, 0);
        float env = (t < 0.32f) ? 1.0f : expf(-(t - 0.32f) / 0.06f);
        float treme = 0.7f + 0.3f * sinf(2.0f * PI * 30.0f * t);
        float chia = SvfPasso(&hp, SinRnd(), 1500.0f, 0.8f, 2) * EnvAD(t, 0.001f, 0.25f) * ((SinRndU() > 0.7f) ? 1.0f : 0.3f);
        b.x[i] = tanhf((boom * 1.1f + zumbe * env * treme + chia * 0.3f) * 1.4f);
    }
    ReverbPedra(&b, 0.24f, 1.2f, 0.4f);
    return BufParaSom(&b, 0.88f);
}

// Retropropagador avisando a volta: fita rebobinando (grave, com "wow"), gaguejando cada vez mais rápido,
// crescendo ao contrário até cortar seco na hora da volta (0,9 s)
static Sound ReceitaRetroAviso(int v)
{
    BufSom b = BufNovo(1.0f);
    float f1 = 0.0f, f2 = 0.0f, fg = 0.0f, dur = 0.9f;
    Svf lp = { 0 }, l2 = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, k = fminf(t / dur, 1.0f);
        float wow = 1.0f + 0.25f * sinf(2.0f * PI * 1.3f * t) - 0.2f * k;
        f1 += 65.0f * wow * (1.0f + 0.03f * (float)v) / TAXA_SOM; f2 += 97.5f * wow / TAXA_SOM;
        fg += (10.0f + 24.0f * k) / TAXA_SOM;
        float porta = ((fg - floorf(fg)) < 0.6f) ? 1.0f : 0.2f;
        float volta = SvfPasso(&lp, OscSerra(f1) + 0.7f * OscSerra(f2), 250.0f + 1200.0f * k, 0.9f, 0);
        float ar = SvfPasso(&l2, SinRnd(), 300.0f + 1100.0f * k, 0.8f, 0);
        float env = powf(k, 1.6f) * ((t < dur) ? 1.0f : expf(-(t - dur) / 0.015f));
        b.x[i] = tanhf((volta * 0.9f + ar * 0.6f) * porta * env * 1.5f);
    }
    ReverbPedra(&b, 0.18f, 1.0f, 0.45f);
    return BufParaSom(&b, 0.70f);
}

// Cada golpe devolvido pelo Retropropagador: um soco tocado AO CONTRÁRIO (cresce e estala no fim)
static Sound ReceitaRetroTiro(int v)
{
    BufSom b = BufNovo(0.22f);
    float fc = 0.0f, dur = 0.09f;
    Svf lp = { 0 }, hp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float rev = (t < dur) ? powf(t / dur, 2.5f) : expf(-(t - dur) / 0.01f);
        fc += (60.0f + 80.0f * (t / 0.22f)) * (1.0f + 0.05f * (float)v) / TAXA_SOM;
        float corpo = SvfPasso(&lp, sinf(2.0f * PI * fc) + 0.4f * SinRnd(), 700.0f, 0.8f, 0) * rev;
        float estalo = (t >= dur) ? SvfPasso(&hp, SinRnd(), 1400.0f, 1.0f, 1) * EnvAD(t - dur, 0.0005f, 0.012f) : 0.0f;
        b.x[i] = tanhf((corpo * 1.6f + estalo * 0.6f) * 1.2f);
    }
    ReverbPedra(&b, 0.14f, 0.8f, 0.5f);
    return BufParaSom(&b, 0.62f);
}

// Janela RESUMINDO (1,3 s): o texto sendo espremido, fita parando e o filtro fechando, e um "tum" no fim
static Sound ReceitaJanelaResumo(int v)
{
    BufSom b = BufNovo(1.45f);
    float f1 = 0.0f, fs = 0.0f, dur = 1.3f;
    Svf lp = { 0 }, l2 = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, k = fminf(t / dur, 1.0f);
        f1 += 98.0f * (1.0f - 0.7f * k * k) * (1.0f + 0.03f * (float)v) / TAXA_SOM;
        float texto = SvfPasso(&lp, OscPulso(f1, 0.25f) + 0.5f * OscPulso(f1 * 1.5f, 0.5f), 200.0f + 1400.0f * (1.0f - k), 0.9f, 0);
        float chiado = SvfPasso(&l2, SinRnd(), 800.0f * (1.0f - k) + 150.0f, 0.8f, 0);
        float env = fminf(t / 0.1f, 1.0f) * ((t < dur) ? 0.9f - 0.4f * k : 0.0f);
        float u = t - dur, tum = 0.0f;
        if (u >= 0.0f) { fs += (50.0f + 60.0f * expf(-u / 0.02f)) / TAXA_SOM; tum = sinf(2.0f * PI * fs) * EnvAD(u, 0.001f, 0.06f); }
        b.x[i] = Esmagar(tanhf((texto * 0.8f + chiado * 0.4f) * env * 1.3f) + tum * 0.9f, 6);
    }
    ReverbPedra(&b, 0.18f, 1.0f, 0.45f);
    return BufParaSom(&b, 0.62f);
}

// Linhas do Silogista: a parede inteira de balas saindo (socos em fila), sub e uma onda de ar larga
static Sound ReceitaSilLinhas(int v)
{
    BufSom b = BufNovo(0.8f);
    float fs = 0.0f, fq[8] = { 0 };
    Svf l1 = { 0 }, h1 = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, s = 0.0f;
        for (int q = 0; q < 8; q++) {
            float u = t - 0.009f * (float)q;
            if (u < 0.0f) continue;
            fq[q] += (60.0f + 3.0f * (float)v + 6.0f * (float)q + 60.0f * expf(-u / 0.02f)) / TAXA_SOM;
            s += sinf(2.0f * PI * fq[q]) * EnvAD(u, 0.001f, 0.05f) * 0.3f;
        }
        fs += (34.0f + 50.0f * expf(-t / 0.08f)) / TAXA_SOM;
        float sub = tanhf(2.0f * sinf(2.0f * PI * fs)) * EnvAD(t, 0.003f, 0.25f);
        float c = 300.0f + 1100.0f * expf(-t / 0.15f);
        float onda = SvfPasso(&h1, SvfPasso(&l1, SinRnd(), c, 1.0f, 0), 120.0f, 1.0f, 2) * EnvAD(t, 0.01f, 0.2f);
        b.x[i] = tanhf((s * 1.2f + sub * 0.9f + onda * 0.9f) * 1.3f);
    }
    ReverbPedra(&b, 0.28f, 1.4f, 0.4f);
    return BufParaSom(&b, 0.80f);
}

// Corte do Silogista (trava um terminal): corte de ar curto, "clank" de ferro e o zumbido da trava
static Sound ReceitaSilCorte(int v)
{
    BufSom b = BufNovo(0.8f);
    float fs = 0.0f, fz = 0.0f, d = 0.16f;
    Svf l1 = { 0 }, h1 = { 0 }, lp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, k = fminf(t / d, 1.0f);
        float env = (t < d) ? sinf(PI * k) : 0.0f;
        float c = 400.0f + 1200.0f * env;
        float corte = SvfPasso(&h1, SvfPasso(&l1, SinRnd(), c, 1.1f, 0), c * 0.3f, 1.1f, 2) * env;
        float u = t - d * 0.8f, clank = 0.0f, zumbe = 0.0f;
        if (u >= 0.0f) {
            clank = (sinf(2.0f * PI * 160.0f * u) * expf(-u / 0.12f) + 0.6f * sinf(2.0f * PI * 437.0f * u) * expf(-u / 0.06f)) * fminf(u / 0.001f, 1.0f);
            fs += (45.0f + 60.0f * expf(-u / 0.02f)) / TAXA_SOM;
            clank += sinf(2.0f * PI * fs) * EnvAD(u, 0.001f, 0.06f);
            fz += 55.0f * (1.0f + 0.03f * (float)v) / TAXA_SOM;
            zumbe = SvfPasso(&lp, OscSerra(fz) + 0.6f * OscPulso(fz * 2.0f, 0.3f), 500.0f, 0.9f, 0) * EnvAD(u, 0.02f, 0.25f) * (0.7f + 0.3f * sinf(2.0f * PI * 9.0f * u));
        }
        b.x[i] = tanhf((corte * 1.3f + clank * 0.8f + zumbe * 0.7f) * 1.3f);
    }
    ReverbPedra(&b, 0.22f, 1.1f, 0.4f);
    return BufParaSom(&b, 0.80f);
}

// =============================================================
// SONS DAS BATALHAS DO BLOCO 1 (monitores e o castigo do quiz)
// =============================================================

// A caixa fechando (vira a arena da esquiva): ar sendo espremido e a trava batendo no fim (0,35 s)
static Sound ReceitaB1Caixa(int v)
{
    BufSom b = BufNovo(0.6f);
    float fs = 0.0f, dur = 0.33f;
    Svf lp = { 0 }, bp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, k = fminf(t / dur, 1.0f);
        float ar = SvfPasso(&lp, SinRnd(), 1300.0f - 1000.0f * k, 0.9f, 0) * (0.3f + 0.7f * k) * ((t < dur) ? 1.0f : expf(-(t - dur) / 0.01f));
        float u = t - dur, trava = 0.0f;
        if (u >= 0.0f) {
            fs += (55.0f + 70.0f * expf(-u / 0.02f)) * (1.0f + 0.04f * (float)v) / TAXA_SOM;
            trava = tanhf(1.6f * sinf(2.0f * PI * fs)) * EnvAD(u, 0.001f, 0.08f)
                  + SvfPasso(&bp, SinRnd(), 700.0f, 1.0f, 1) * EnvAD(u, 0.0005f, 0.02f) * 1.5f
                  + (sinf(2.0f * PI * 260.0f * u) * expf(-u / 0.05f) + 0.5f * sinf(2.0f * PI * 707.0f * u) * expf(-u / 0.025f)) * 0.3f * fminf(u / 0.001f, 1.0f);
        }
        b.x[i] = tanhf((ar * 0.7f + trava * 1.0f) * 1.3f);
    }
    ReverbPedra(&b, 0.18f, 1.0f, 0.45f);
    return BufParaSom(&b, 0.62f);
}

// Pegar uma verdade: dedilhado quente e grave (triângulos), sem brilho agudo; o jogo sobe o tom a cada verdade
static Sound ReceitaB1Verdade(int v)
{
    BufSom b = BufNovo(0.5f);
    float f1 = 0.0f, f2 = 0.0f, f3 = 0.0f, fs = 0.0f;
    Svf lp = { 0 }, l2 = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float base = 196.0f * (1.0f + 0.02f * (float)v);
        f1 += base / TAXA_SOM; f2 += base * 1.5f / TAXA_SOM; f3 += base * 2.0f / TAXA_SOM;
        float nota = SvfPasso(&lp, OscTri(f1) + 0.5f * OscTri(f2) + 0.3f * OscTri(f3), 600.0f + 1200.0f * expf(-t / 0.06f), 0.9f, 0) * EnvAD(t, 0.003f, 0.16f);
        float toque = SvfPasso(&l2, SinRnd(), 1200.0f, 0.9f, 0) * EnvAD(t, 0.0005f, 0.01f);
        fs += (90.0f + 60.0f * expf(-t / 0.02f)) / TAXA_SOM;
        float corpo = sinf(2.0f * PI * fs) * EnvAD(t, 0.001f, 0.05f);
        b.x[i] = nota * 1.0f + toque * 0.4f + corpo * 0.3f;
    }
    ReverbPedra(&b, 0.26f, 1.0f, 0.4f);
    return BufParaSom(&b, 0.55f);
}

// A Luci levou um golpe na esquiva: estalo esmagado, um "tum" e o tom caindo picotado (dados corrompendo)
static Sound ReceitaB1DanoAlma(int v)
{
    BufSom b = BufNovo(0.5f);
    float fs = 0.0f, fa = 0.0f, amostra = 0.0f;
    int segura = 0;
    Svf lp = { 0 }, l2 = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        if (--segura <= 0) { amostra = SinRnd(); segura = 3 + (int)(t * 60.0f); }
        float estalo = SvfPasso(&lp, amostra, 400.0f + 3000.0f * expf(-t / 0.03f), 0.7f, 0) * EnvAD(t, 0.0005f, 0.06f);
        fs += (48.0f + 80.0f * expf(-t / 0.03f)) / TAXA_SOM;
        float tum = tanhf(1.8f * sinf(2.0f * PI * fs)) * EnvAD(t, 0.001f, 0.1f);
        fa += (220.0f * powf(2.0f, -1.4f * fminf(t / 0.2f, 1.0f)) * (1.0f + 0.04f * (float)v)) / TAXA_SOM;
        float gagueja = (fmodf(t, 0.035f) < 0.022f) ? 1.0f : 0.2f;
        float cai = SvfPasso(&l2, OscSerra(fa), 900.0f, 0.9f, 0) * gagueja * EnvAD(t, 0.002f, 0.12f);
        b.x[i] = Esmagar(tanhf((estalo * 1.0f + tum * 1.0f + cai * 0.6f) * 1.3f), 6);
    }
    ReverbPedra(&b, 0.16f, 0.9f, 0.45f);
    return BufParaSom(&b, 0.72f);
}

// O ataque acerta a alucinação: soco grave, acorde quente que abre e um estouro de dados
static Sound ReceitaB1Golpe(int v)
{
    BufSom b = BufNovo(0.95f);
    float fs = 0.0f, f1 = 0.0f, f2 = 0.0f, f3 = 0.0f;
    Svf lp = { 0 }, l2 = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        fs += (40.0f + 90.0f * expf(-t / 0.04f)) / TAXA_SOM;
        float soco = tanhf(2.2f * sinf(2.0f * PI * fs)) * EnvAD(t, 0.001f, 0.18f);
        float base = 110.0f * (1.0f + 0.02f * (float)v);
        f1 += base / TAXA_SOM; f2 += base * 1.26f / TAXA_SOM; f3 += base * 1.5f / TAXA_SOM;
        float acorde = SvfPasso(&lp, OscSerra(f1) + 0.8f * OscSerra(f2) + 0.7f * OscSerra(f3), 250.0f + 1300.0f * expf(-t / 0.12f), 0.8f, 0) * EnvAD(t, 0.004f, 0.3f);
        float estouro = SvfPasso(&l2, SinRnd(), 400.0f + 3500.0f * expf(-t / 0.05f), 0.7f, 0) * EnvAD(t, 0.001f, 0.08f);
        b.x[i] = Esmagar(tanhf((soco * 1.1f + acorde * 0.5f + estouro * 0.8f) * 1.3f), 8);
    }
    ReverbPedra(&b, 0.28f, 1.3f, 0.4f);
    return BufParaSom(&b, 0.85f);
}

// VERIFICAR: a lupa voando até a alucinação (0,55 s), sopro que sobe e um zumbido grave crescendo
static Sound ReceitaB1Lupa(int v)
{
    BufSom b = BufNovo(0.6f);
    float fa = 0.0f, dur = 0.55f;
    Svf bp = { 0 }, lp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, k = fminf(t / dur, 1.0f);
        float corta = (t < dur) ? 1.0f : expf(-(t - dur) / 0.01f);
        float sopro = SvfPasso(&bp, SinRnd(), 300.0f + 1100.0f * k * k, 1.0f, 1) * k * k * corta;
        fa += (60.0f + 70.0f * k + 2.0f * (float)v) / TAXA_SOM;
        float zumbe = SvfPasso(&lp, OscSerra(fa) + 0.5f * OscTri(fa * 2.0f), 300.0f + 900.0f * k, 0.9f, 0) * (0.2f + 0.8f * k) * corta;
        b.x[i] = tanhf((sopro * 0.9f + zumbe * 0.8f) * 1.3f);
    }
    ReverbPedra(&b, 0.18f, 1.0f, 0.45f);
    return BufParaSom(&b, 0.62f);
}

// Carimbo: batida seca de borracha/madeira ("tóc") e a trava de metal ("chunk") logo depois
static float CarimboVerif(float t, Svf *bp, Svf *bp2, float *fs)
{
    float toc = SvfPasso(bp, SinRnd(), 380.0f, 1.0f, 1) * EnvAD(t, 0.0005f, 0.018f) * 2.2f;
    *fs += (85.0f + 90.0f * expf(-t / 0.012f)) / TAXA_SOM;
    float corpo = sinf(2.0f * PI * *fs) * EnvAD(t, 0.001f, 0.06f);
    float u = t - 0.045f, chunk = 0.0f;
    if (u >= 0.0f)
        chunk = (SvfPasso(bp2, SinRnd(), 900.0f, 1.0f, 1) * EnvAD(u, 0.0005f, 0.012f) * 1.6f
               + (sinf(2.0f * PI * 240.0f * u) * expf(-u / 0.05f) + 0.5f * sinf(2.0f * PI * 655.0f * u) * expf(-u / 0.025f)) * 0.5f);
    return toc * 0.9f + corpo * 1.0f + chunk * 0.8f;
}

// VERIFICADO!: o carimbo de verdade (batida seca + trava de metal) e um arpejo subindo (Dó-Mi-Sol-Dó)
// que resolve num acorde maior sustentado: soa como "aprovado!"
static Sound ReceitaB1Verificado(int v)
{
    static const float notas[4] = { 261.63f, 329.63f, 392.0f, 523.25f };
    BufSom b = BufNovo(1.5f);
    float fs = 0.0f, fa[4] = { 0 }, fb[4] = { 0 };
    Svf bp = { 0 }, bp2 = { 0 }, lp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, s = 0.0f;
        for (int q = 0; q < 4; q++) {
            float u = t - 0.10f - 0.07f * (float)q;           // cada nota entra depois do carimbo, em escada
            if (u < 0.0f) continue;
            fa[q] += notas[q] * (1.0f + 0.01f * (float)v) / TAXA_SOM;
            fb[q] += notas[q] * 1.003f / TAXA_SOM;
            float env = (u < 0.01f) ? u / 0.01f : 0.55f + 0.45f * expf(-(u - 0.01f) / 0.08f);
            env *= (t > 0.85f) ? expf(-(t - 0.85f) / 0.18f) : 1.0f;
            s += (OscTri(fa[q]) * 0.7f + 0.3f * OscSerra(fb[q])) * env * ((q == 3) ? 0.8f : 0.6f);
        }
        float acorde = SvfPasso(&lp, s, 2600.0f, 0.9f, 0);
        b.x[i] = tanhf((CarimboVerif(t, &bp, &bp2, &fs) * 1.0f + acorde * 0.55f) * 1.1f);
    }
    ReverbPedra(&b, 0.28f, 1.3f, 0.35f);
    return BufParaSom(&b, 0.85f);
}

// Sobrecarga (a vida zerou): energia desligando, tom despencando picotado e um clique de "apagou"
static Sound ReceitaB1Sobrecarga(int v)
{
    BufSom b = BufNovo(1.5f);
    float fa = 0.0f, fb = 0.0f, dur = 1.2f;
    Svf lp = { 0 }, hp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, k = fminf(t / dur, 1.0f);
        float f = 180.0f * powf(2.0f, -2.3f * k) * (1.0f + 0.03f * (float)v);
        fa += f / TAXA_SOM; fb += f * 1.5f / TAXA_SOM;
        float gagueja = (fmodf(t, 0.06f + 0.06f * k) < 0.04f + 0.02f * k) ? 1.0f : 0.15f;
        float desliga = SvfPasso(&lp, OscSerra(fa) + 0.6f * OscSerra(fb), 1600.0f * (1.0f - k) + 120.0f, 0.9f, 0) * gagueja * ((t < dur) ? 1.0f - 0.5f * k : 0.0f);
        float u = t - dur;
        float clique = (u >= 0.0f) ? SvfPasso(&hp, SinRnd(), 900.0f, 0.9f, 2) * EnvAD(u, 0.0005f, 0.01f) + sinf(2.0f * PI * 45.0f * u) * EnvAD(u, 0.001f, 0.08f) : 0.0f;
        b.x[i] = Esmagar(tanhf((desliga * 1.1f + clique * 1.0f) * 1.3f), 7);
    }
    ReverbPedra(&b, 0.24f, 1.2f, 0.4f);
    return BufParaSom(&b, 0.75f);
}

// =============================================================
// SONS GERAIS E DO JOGADOR (menus, cartas, loja, laboratórios, quizzes, dano, esquiva, pulso)
// No padrão da luta da Luci: grave, com corpo, sem bipe agudo.
// =============================================================

// Navegar (menus, opções, cartas): um toque curto e macio, clique com corpo grave, sem bipe
static Sound ReceitaUISelecao(int v)
{
    BufSom b = BufNovo(0.12f);
    float fs = 0.0f;
    Svf bp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        fs += (150.0f + 120.0f * expf(-t / 0.008f)) * (1.0f + 0.04f * (float)v) / TAXA_SOM;
        float corpo = (OscTri(fs) * 0.8f + 0.2f * OscPulso(fs, 0.5f)) * EnvAD(t, 0.0008f, 0.025f);
        float clique = SvfPasso(&bp, SinRnd(), 2000.0f, 1.0f, 1) * EnvAD(t, 0.0003f, 0.004f);
        b.x[i] = corpo * 0.9f + clique * 0.35f;
    }
    ReverbPedra(&b, 0.10f, 0.6f, 0.5f);
    return BufParaSom(&b, 0.40f);
}

// Acerto / confirmar: arpejo rápido e quente (Dó-Mi-Sol-Dó na região média), triângulo com um pouco de pulso
static Sound ReceitaUIAcerto(int v)
{
    static const float notas[4] = { 261.63f, 329.63f, 392.0f, 523.25f };
    BufSom b = BufNovo(0.55f);
    float fa[4] = { 0 }, fs = 0.0f, passo = 0.055f + 0.008f * (float)v;
    Svf lp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, s = 0.0f;
        for (int q = 0; q < 4; q++) {
            float u = t - passo * (float)q;
            if (u < 0.0f) continue;
            fa[q] += notas[q] / TAXA_SOM;
            s += (OscTri(fa[q]) * 0.75f + 0.25f * OscPulso(fa[q], 0.25f)) * EnvAD(u, 0.002f, (q == 3) ? 0.16f : 0.07f) * ((q == 3) ? 0.9f : 0.7f);
        }
        fs += (110.0f + 60.0f * expf(-t / 0.02f)) / TAXA_SOM;
        float soco = sinf(2.0f * PI * fs) * EnvAD(t, 0.001f, 0.04f);
        b.x[i] = SvfPasso(&lp, s, 2400.0f, 0.9f, 0) + soco * 0.35f;
    }
    ReverbPedra(&b, 0.24f, 1.0f, 0.4f);
    return BufParaSom(&b, 0.55f);
}

// Erro: duas notas graves que batem uma na outra, esmagadas, com um ruído curto e um sub
static Sound ReceitaUIErro(int v)
{
    BufSom b = BufNovo(0.5f);
    float f1 = 0.0f, f2 = 0.0f, fs = 0.0f;
    Svf lp = { 0 }, l2 = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, u = t - 0.09f;
        float base = 110.0f * (1.0f + 0.03f * (float)v);
        f1 += base * 1.5f / TAXA_SOM;
        float n1 = OscSerra(f1) * EnvAD(t, 0.002f, 0.06f) * ((t < 0.09f) ? 1.0f : 0.3f);
        float n2 = 0.0f;
        if (u >= 0.0f) {
            f2 += base * 1.414f / TAXA_SOM;
            n2 = (OscSerra(f2) + OscSerra(f2 * 1.06f)) * 0.6f * EnvAD(u, 0.002f, 0.14f);
        }
        float tons = SvfPasso(&lp, n1 + n2, 900.0f, 0.9f, 0);
        fs += (45.0f + 50.0f * expf(-t / 0.03f)) / TAXA_SOM;
        float sub = sinf(2.0f * PI * fs) * EnvAD(t, 0.001f, 0.08f);
        float ruido = SvfPasso(&l2, SinRnd(), 1500.0f, 0.8f, 0) * EnvAD(t, 0.0005f, 0.02f);
        b.x[i] = Esmagar(tanhf((tons * 0.9f + sub * 0.7f + ruido * 0.4f) * 1.3f), 6);
    }
    ReverbPedra(&b, 0.16f, 0.9f, 0.45f);
    return BufParaSom(&b, 0.62f);
}

// Thomas leva um golpe: soco grave no corpo, estalo esmagado e um "zz" curto de sinal falhando
static Sound ReceitaDanoJogador(int v)
{
    BufSom b = BufNovo(0.45f);
    float fs = 0.0f, fz = 0.0f, amostra = 0.0f;
    int segura = 0;
    Svf lp = { 0 }, l2 = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, u = t - 0.02f;
        fs += (42.0f + 100.0f * expf(-t / 0.025f)) * (1.0f + 0.04f * (float)v) / TAXA_SOM;
        float soco = tanhf(2.4f * sinf(2.0f * PI * fs)) * EnvAD(t, 0.001f, 0.09f);
        if (--segura <= 0) { amostra = SinRnd(); segura = 2 + (int)(t * 80.0f); }
        float estalo = SvfPasso(&lp, amostra, 500.0f + 3000.0f * expf(-t / 0.025f), 0.7f, 0) * EnvAD(t, 0.0005f, 0.05f);
        fz += 120.0f / TAXA_SOM;
        float zz = 0.0f;
        if (u >= 0.0f)
            zz = SvfPasso(&l2, OscPulso(fz, 0.3f), 700.0f, 0.9f, 0) * ((fmodf(u, 0.03f) < 0.018f) ? 1.0f : 0.0f) * EnvAD(u, 0.002f, 0.07f);
        b.x[i] = Esmagar(tanhf((soco * 1.2f + estalo * 0.9f + zz * 0.45f) * 1.3f), 7);
    }
    ReverbPedra(&b, 0.14f, 0.9f, 0.45f);
    return BufParaSom(&b, 0.70f);
}

// Pegar (itens, fragmentos, moedas da sala): dois toques quentes subindo uma quinta (Sol → Ré), sem apito
static Sound ReceitaUIPegar(int v)
{
    BufSom b = BufNovo(0.32f);
    float f1 = 0.0f, f2 = 0.0f;
    Svf lp = { 0 }, bp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, u = t - 0.05f;
        float base = 196.0f * (1.0f + 0.02f * (float)v);
        f1 += base / TAXA_SOM;
        float n1 = (OscTri(f1) + 0.3f * OscTri(f1 * 2.0f)) * EnvAD(t, 0.002f, 0.04f);
        float n2 = 0.0f;
        if (u >= 0.0f) { f2 += base * 1.5f / TAXA_SOM; n2 = (OscTri(f2) + 0.3f * OscTri(f2 * 2.0f)) * EnvAD(u, 0.002f, 0.09f); }
        float toque = SvfPasso(&bp, SinRnd(), 1800.0f, 1.0f, 1) * (EnvAD(t, 0.0003f, 0.004f) + ((u >= 0.0f) ? EnvAD(u, 0.0003f, 0.004f) : 0.0f));
        b.x[i] = SvfPasso(&lp, n1 + n2, 1800.0f, 0.9f, 0) * 0.9f + toque * 0.3f;
    }
    ReverbPedra(&b, 0.20f, 0.9f, 0.45f);
    return BufParaSom(&b, 0.55f);
}

// Tiro genérico (armadilhas e o que sobrou): "pew" grave e curto, pulso descendo com um estalo
static Sound ReceitaTiroGenerico(int v)
{
    BufSom b = BufNovo(0.22f);
    float fa = 0.0f;
    Svf lp = { 0 }, hp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        fa += (75.0f + 220.0f * expf(-t / 0.025f)) * (1.0f + 0.04f * (float)v) / TAXA_SOM;
        float corpo = SvfPasso(&lp, OscPulso(fa, 0.35f), 400.0f + 1800.0f * expf(-t / 0.03f), 0.9f, 0) * EnvAD(t, 0.001f, 0.05f);
        float estalo = SvfPasso(&hp, SinRnd(), 1800.0f, 0.9f, 2) * EnvAD(t, 0.0003f, 0.005f);
        b.x[i] = Esmagar(corpo + estalo * 0.4f, 6);
    }
    ReverbPedra(&b, 0.12f, 0.8f, 0.5f);
    return BufParaSom(&b, 0.42f);
}

// Esquiva: corte de ar rápido e grave (o casaco passando) e um passo abafado no fim
static Sound ReceitaEsquiva(int v)
{
    BufSom b = BufNovo(0.3f);
    float fs = 0.0f, d = 0.16f;
    Svf l1 = { 0 }, h1 = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM, k = fminf(t / d, 1.0f);
        float env = (t < d) ? sinf(PI * k) : 0.0f;
        float c = (300.0f + 900.0f * (1.0f - k)) * (1.0f + 0.05f * (float)v);
        float ar = SvfPasso(&h1, SvfPasso(&l1, SinRnd(), c, 1.1f, 0), c * 0.3f, 1.1f, 2) * env;
        float u = t - d, passo = 0.0f;
        if (u >= 0.0f) { fs += (60.0f + 40.0f * expf(-u / 0.01f)) / TAXA_SOM; passo = sinf(2.0f * PI * fs) * EnvAD(u, 0.001f, 0.03f); }
        b.x[i] = tanhf((ar * 1.4f + passo * 0.6f) * 1.2f);
    }
    ReverbPedra(&b, 0.10f, 0.8f, 0.5f);
    return BufParaSom(&b, 0.55f);
}

// Pulso (Q): onda de choque saindo do Thomas: sub, "whum" que abre e um chiado varrendo as balas
static Sound ReceitaPulsoJogador(int v)
{
    BufSom b = BufNovo(0.9f);
    float fs = 0.0f, fc = 0.0f;
    Svf lp = { 0 }, bp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        fs += (38.0f + 90.0f * expf(-t / 0.06f)) / TAXA_SOM;
        float sub = tanhf(2.0f * sinf(2.0f * PI * fs)) * EnvAD(t, 0.002f, 0.2f);
        fc += (90.0f + 40.0f * fminf(t / 0.3f, 1.0f)) * (1.0f + 0.03f * (float)v) / TAXA_SOM;
        float whum = SvfPasso(&lp, OscSerra(fc) + 0.6f * OscSerra(fc * 1.5f), 200.0f + 1500.0f * EnvAD(t, 0.05f, 0.15f), 0.8f, 0) * EnvAD(t, 0.01f, 0.25f);
        float varre = SvfPasso(&bp, SinRnd(), 400.0f + 1200.0f * fminf(t / 0.3f, 1.0f), 1.1f, 1) * EnvAD(t, 0.03f, 0.12f);
        b.x[i] = tanhf((sub * 1.0f + whum * 0.6f + varre * 0.6f) * 1.3f);
    }
    ReverbPedra(&b, 0.26f, 1.2f, 0.4f);
    return BufParaSom(&b, 0.78f);
}

// Parry armado (V): a guarda sobe, um "shk" metálico curto e grave
static Sound ReceitaParryArmar(int v)
{
    BufSom b = BufNovo(0.25f);
    float m = 1.0f + 0.04f * (float)v;
    Svf bp = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float raspa = SvfPasso(&bp, SinRnd(), 1300.0f, 1.0f, 1) * EnvAD(t, 0.002f, 0.025f);
        float metal = (sinf(2.0f * PI * 310.0f * m * t) * expf(-t / 0.05f) + 0.5f * sinf(2.0f * PI * 843.0f * m * t) * expf(-t / 0.025f)) * fminf(t / 0.001f, 1.0f);
        b.x[i] = tanhf((raspa * 1.2f + metal * 0.5f) * 1.2f);
    }
    ReverbPedra(&b, 0.14f, 0.8f, 0.5f);
    return BufParaSom(&b, 0.45f);
}

// Thomas caiu: soco seco, o sinal da Luci despencando (acorde menor) e a estática sumindo
static Sound ReceitaMorteJogador(int v)
{
    BufSom b = BufNovo(1.8f);
    float fs = 0.0f, f1 = 0.0f, f2 = 0.0f;
    Svf lp = { 0 }, l2 = { 0 };
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        fs += (34.0f + 90.0f * expf(-t / 0.05f)) / TAXA_SOM;
        float soco = tanhf(2.5f * sinf(2.0f * PI * fs)) * EnvAD(t, 0.001f, 0.3f);
        float base = 147.0f * powf(2.0f, -1.5f * fminf(t / 1.3f, 1.0f)) * (1.0f + 0.02f * (float)v);
        f1 += base / TAXA_SOM; f2 += base * 1.19f / TAXA_SOM;
        float cai = SvfPasso(&lp, OscSerra(f1) + 0.8f * OscSerra(f2), 1300.0f * expf(-t / 0.6f) + 150.0f, 0.9f, 0) * EnvAD(t, 0.02f, 0.7f);
        float estatica = SvfPasso(&l2, SinRnd(), 2200.0f, 0.8f, 0) * EnvAD(t, 0.1f, 0.5f) * ((SinRndU() > 0.6f) ? 1.0f : 0.2f);
        b.x[i] = Esmagar(tanhf((soco * 1.0f + cai * 0.6f + estatica * 0.18f) * 1.2f), 8);
    }
    ReverbPedra(&b, 0.30f, 1.4f, 0.4f);
    return BufParaSom(&b, 0.82f);
}

// Gotinha na pedra: tom curto que sobe (as rachaduras do mapa1)
static void AmbGota(BufSom *b, int ini, float f)
{
    for (int i = 0; i < TAXA_SOM / 12 && ini + i < b->n; i++) {
        float t = (float)i / TAXA_SOM;
        b->x[ini + i] += sinf(2.0f * PI * (f + f * 1.2f * t / 0.08f) * t) * EnvAD(t, 0.001f, 0.012f) * 0.35f;
    }
}

// Bipe de dados: blip de pulso (as telas ciano e os terminais)
static void AmbBipe(BufSom *b, int ini, float f, float dur, float vol)
{
    float fase = 0.0f;
    for (int i = 0; i < (int)(dur * TAXA_SOM) && ini + i < b->n; i++) {
        float t = (float)i / TAXA_SOM;
        fase += f / TAXA_SOM;
        b->x[ini + i] += OscPulso(fase, 0.25f) * EnvAD(t, 0.001f, dur * 0.4f) * vol;
    }
}

// Clique de relé: ruído curtíssimo filtrado
static void AmbRele(BufSom *b, int ini, float vol)
{
    Svf bp = { 0 };
    for (int i = 0; i < TAXA_SOM / 80 && ini + i < b->n; i++) {
        float t = (float)i / TAXA_SOM;
        b->x[ini + i] += SvfPasso(&bp, SinRnd(), 2400.0f, 0.4f, 1) * EnvAD(t, 0.0003f, 0.002f) * vol;
    }
}

static BufSom GerarAmbiente(TipoAmbiente tipo)
{
    BufSom b = BufNovo(AMB_SEGUNDOS + AMB_CAUDA);
    Svf ar = { 0 }, ar2 = { 0 };
    g_sinteseSemente = 0xA11CEu + (unsigned int)tipo * 7919u;
    // Para o loop fechar, os osciladores usam frequências com número inteiro de ciclos em 8 s
    for (int i = 0; i < b.n; i++) {
        float t = (float)i / TAXA_SOM;
        float s = 0.0f;
        if (tipo == AMB_SALA) {
            // Servidor ligado atrás da pedra: zumbido de 60 Hz com harmônicos + ar frio
            s += 0.22f * sinf(2.0f * PI * 60.0f * t) + 0.10f * sinf(2.0f * PI * 120.0f * t) + 0.05f * sinf(2.0f * PI * 180.0f * t);
            s *= 0.85f + 0.15f * sinf(2.0f * PI * 0.25f * t);
            s += SvfPasso(&ar, SinRnd(), 380.0f, 0.9f, 0) * 0.35f;
            s += 0.012f * sinf(2.0f * PI * 7875.0f * t);                // chiado fininho das telas ciano
        } else if (tipo == AMB_CORREDOR) {
            // Corredor de terminais: 50 Hz mais seco, ventoinhas, chiado de CRT
            s += 0.18f * sinf(2.0f * PI * 50.0f * t) + 0.09f * sinf(2.0f * PI * 100.0f * t);
            s += SvfPasso(&ar, SinRnd(), 900.0f + 200.0f * sinf(2.0f * PI * 0.5f * t), 0.6f, 1) * 0.22f;     // ventoinha
            s += 0.016f * sinf(2.0f * PI * 7875.0f * t) * (0.7f + 0.3f * sinf(2.0f * PI * 3.0f * t));
        } else if (tipo == AMB_BIBLIOTECA) {
            // Salas fundas: vento de caverna (banda que respira) + nota grave muito baixa
            float corte = 260.0f + 180.0f * sinf(2.0f * PI * 0.125f * t) + 90.0f * sinf(2.0f * PI * 0.375f * t);
            s += SvfPasso(&ar, SinRnd(), corte, 0.25f, 1) * 0.9f;
            s += SvfPasso(&ar2, SinRnd(), 140.0f, 0.8f, 0) * 0.5f;
            s += 0.10f * sinf(2.0f * PI * 55.0f * t) * (0.6f + 0.4f * sinf(2.0f * PI * 0.125f * t));
        } else {
            // Arena da Luci: drone em batimento (41,25 / 41,75 Hz → bate 2x a cada segundo), névoa roxa, pulso cardíaco
            s += 0.20f * sinf(2.0f * PI * 41.25f * t) + 0.20f * sinf(2.0f * PI * 41.75f * t) + 0.10f * sinf(2.0f * PI * 82.5f * t);
            float corte = 500.0f + 300.0f * sinf(2.0f * PI * 0.25f * t);
            s += SvfPasso(&ar, SinRnd(), corte, 0.35f, 1) * 0.35f * (0.6f + 0.4f * sinf(2.0f * PI * 0.125f * t));
            float tb = fmodf(t, 1.0f);                                     // pulsação: tum-tum a cada 1 s
            float p1 = sinf(2.0f * PI * 48.0f * tb) * EnvAD(tb, 0.004f, 0.06f);
            float tb2 = tb - 0.22f;
            float p2 = (tb2 > 0.0f) ? sinf(2.0f * PI * 44.0f * tb2) * EnvAD(tb2, 0.004f, 0.05f) * 0.7f : 0.0f;
            s += (p1 + p2) * 0.35f;
        }
        b.x[i] = s;
    }

    // Eventos espalhados (com a semente fixa, sempre os mesmos no loop)
    int eventos = (tipo == AMB_SALA) ? 5 : ((tipo == AMB_CORREDOR) ? 26 : ((tipo == AMB_BIBLIOTECA) ? 4 : 7));
    for (int e = 0; e < eventos; e++) {
        int ini = (int)(SinRndU() * (AMB_SEGUNDOS - 0.3f) * TAXA_SOM);
        if (tipo == AMB_SALA) {
            if (e < 3) AmbGota(&b, ini, 1100.0f + 500.0f * SinRndU());
            else       AmbBipe(&b, ini, 1760.0f + 440.0f * (float)(e % 2), 0.05f, 0.05f);
        } else if (tipo == AMB_CORREDOR) {
            if (e % 3 == 0) {                                              // "conversa" de dados de um terminal: 3 a 5 bipes
                int nb = 3 + (int)(SinRndU() * 3.0f);
                for (int k = 0; k < nb; k++)
                    AmbBipe(&b, ini + k * (int)(0.045f * TAXA_SOM), 1200.0f + 200.0f * (float)((int)(SinRndU() * 6.0f)), 0.03f, 0.04f);
            } else AmbRele(&b, ini, 0.25f + 0.2f * SinRndU());
        } else if (tipo == AMB_BIBLIOTECA) {
            // Eco distante: um sino suave que vai se perder no reverb longo
            float f = (e % 2 == 0) ? 659.25f : 987.77f;
            for (int i = 0; i < TAXA_SOM && ini + i < b.n; i++) {
                float t = (float)i / TAXA_SOM;
                b.x[ini + i] += (sinf(2.0f * PI * f * t) + 0.3f * sinf(2.0f * PI * f * 2.0f * t)) * EnvAD(t, 0.01f, 0.25f) * 0.06f;
            }
        } else {
            // Falha de sinal: fragmento esmagado e picotado (a Luci corrompida)
            Svf bp = { 0 };
            float f = 300.0f + 900.0f * SinRndU();
            for (int i = 0; i < TAXA_SOM / 7 && ini + i < b.n; i++) {
                float t = (float)i / TAXA_SOM;
                float picote = (((int)(t * 60.0f)) % 2 == 0) ? 1.0f : 0.0f;
                float g = Esmagar(OscPulso(t * f, 0.5f) * 0.5f + SvfPasso(&bp, SinRnd(), f * 2.0f, 0.5f, 1), 3);
                b.x[ini + i] += g * picote * EnvAD(t, 0.005f, 0.05f) * 0.07f;
            }
        }
    }

    float tam = (tipo == AMB_CORREDOR) ? 1.6f : ((tipo == AMB_BIBLIOTECA) ? 1.9f : ((tipo == AMB_LUCI) ? 1.4f : 1.1f));
    ReverbPedra(&b, (tipo == AMB_BIBLIOTECA) ? 0.55f : 0.35f, tam, 0.3f);

    // Fecha o loop: a cauda (1,5 s) cruza com o começo em potência constante
    int n = (int)(AMB_SEGUNDOS * TAXA_SOM), cauda = b.n - n;
    for (int i = 0; i < cauda; i++) {
        float k = (float)i / (float)cauda;
        b.x[i] = b.x[i] * sinf(0.5f * PI * k) + b.x[n + i] * cosf(0.5f * PI * k);
    }
    b.n = n;

    // Normaliza sem fade (fade criaria um buraco a cada volta do loop)
    float m = 0.0f, dc = 0.0f;
    for (int i = 0; i < b.n; i++) dc += b.x[i];
    dc /= (float)b.n;
    for (int i = 0; i < b.n; i++) { b.x[i] -= dc; m = fmaxf(m, fabsf(b.x[i])); }
    if (m > 1e-6f) for (int i = 0; i < b.n; i++) b.x[i] *= 0.5f / m;
    return b;
}

// O ambiente vira um .wav na memória e toca como Music (loop sem falha; o buffer fica vivo até o fim)
static unsigned char *g_ambWav[NUM_AMBIENTES];
static Music g_amb[NUM_AMBIENTES];
static int g_ambAtual = AMB_NENHUM;
static float g_ambVolume = 0.0f;      // sobe e desce suave na troca
static float ambVolume = 0.0f;        // ambiente sintético DESLIGADO: a trilha de cada lugar já faz esse papel (suba para reativar)

static unsigned char *WavNaMemoria(const BufSom *b, int *tamanho)
{
    int dados = b->n * 2;
    unsigned char *w = (unsigned char *)malloc((size_t)(44 + dados));
    unsigned int v32;
    unsigned short v16;
    memcpy(w, "RIFF", 4);  v32 = (unsigned int)(36 + dados); memcpy(w + 4, &v32, 4);
    memcpy(w + 8, "WAVEfmt ", 8);
    v32 = 16;           memcpy(w + 16, &v32, 4);
    v16 = 1;            memcpy(w + 20, &v16, 2);           // PCM
    v16 = 1;            memcpy(w + 22, &v16, 2);           // mono
    v32 = TAXA_SOM;     memcpy(w + 24, &v32, 4);
    v32 = TAXA_SOM * 2; memcpy(w + 28, &v32, 4);
    v16 = 2;            memcpy(w + 32, &v16, 2);
    v16 = 16;           memcpy(w + 34, &v16, 2);
    memcpy(w + 36, "data", 4); v32 = (unsigned int)dados; memcpy(w + 40, &v32, 4);
    short *pcm = BufParaPcm(b);
    memcpy(w + 44, pcm, (size_t)dados);
    free(pcm);
    *tamanho = 44 + dados;
    return w;
}

static void CarregarAmbientes(void)
{
    if (ambVolume <= 0.0f) return;           // desligado: nem gera
    for (int a = 0; a < NUM_AMBIENTES; a++) {
        BufSom b = GerarAmbiente((TipoAmbiente)a);
        int tam = 0;
        g_ambWav[a] = WavNaMemoria(&b, &tam);
        free(b.x);
        g_amb[a] = LoadMusicStreamFromMemory(".wav", g_ambWav[a], tam);
        g_amb[a].looping = true;
    }
}

// Chamado a cada quadro com o ambiente da tela atual (AMB_NENHUM = silêncio). Troca com fade.
void AtualizarAmbiente(int desejado, float dt)
{
    if (!audioPronto) return;
    if (desejado != g_ambAtual) {
        g_ambVolume -= dt * 1.5f;                                   // some em ~0,3 s
        if (g_ambVolume <= 0.0f || g_ambAtual == AMB_NENHUM) {
            if (g_ambAtual != AMB_NENHUM && g_amb[g_ambAtual].stream.buffer) StopMusicStream(g_amb[g_ambAtual]);
            g_ambAtual = desejado;
            g_ambVolume = 0.0f;
            if (g_ambAtual != AMB_NENHUM && g_amb[g_ambAtual].stream.buffer) PlayMusicStream(g_amb[g_ambAtual]);
        }
    } else if (g_ambVolume < 1.0f) {
        g_ambVolume = fminf(1.0f, g_ambVolume + dt * 0.5f);          // entra em ~2 s (valores negativos = silêncio por um tempo)
    }
    if (g_ambAtual != AMB_NENHUM && g_amb[g_ambAtual].stream.buffer) {
        SetMusicVolume(g_amb[g_ambAtual], fmaxf(g_ambVolume, 0.0f) * ambVolume);
        UpdateMusicStream(g_amb[g_ambAtual]);
    }
}

void PausarAmbiente(bool pausar)
{
    if (!audioPronto || g_ambAtual == AMB_NENHUM || !g_amb[g_ambAtual].stream.buffer) return;
    if (pausar) PauseMusicStream(g_amb[g_ambAtual]);
    else        ResumeMusicStream(g_amb[g_ambAtual]);
}

static void GrupoCarregar(GrupoSom *g, ReceitaSom receita, int n, float jitter)
{
    memset(g, 0, sizeof(*g));
    g->n = (n > MAX_VAR_SOM) ? MAX_VAR_SOM : n;
    g->jitter = jitter;
    g->ultima = -1;
    static unsigned int id = 0;
    id++;
    for (int k = 0; k < g->n; k++) {
        g_sinteseSemente = 0x9E3779B9u ^ ((unsigned int)k * 2654435761u) ^ (id * 40503u);
        g->v[k] = receita(k);
        for (int a = 0; a < VOZES_EXTRA; a++) g->alias[k][a] = LoadSoundAlias(g->v[k]);
    }
}

// Variações dos SFX antigos: o mesmo gerador com parâmetros levemente diferentes
static void GrupoDeSons(GrupoSom *g, Sound s0, Sound s1, Sound s2, float jitter)
{
    memset(g, 0, sizeof(*g));
    g->v[0] = s0; g->v[1] = s1; g->v[2] = s2;
    g->n = 3;
    g->jitter = jitter;
    g->ultima = -1;
    for (int k = 0; k < g->n; k++)
        for (int a = 0; a < VOZES_EXTRA; a++) g->alias[k][a] = LoadSoundAlias(g->v[k]);
}

void TocarGrupo(GrupoSom *g, float tom, float volume)
{
    if (!audioPronto || g->n <= 0) return;
    int k = (g->n > 1) ? GetRandomValue(0, g->n - 1) : 0;
    if (g->n > 1 && k == g->ultima) k = (k + 1) % g->n;          // nunca a mesma duas vezes seguidas
    g->ultima = k;
    g->voz = (g->voz + 1) % (VOZES_EXTRA + 1);
    Sound s = (g->voz == 0) ? g->v[k] : g->alias[k][g->voz - 1];
    float j = 1.0f + g->jitter * ((float)GetRandomValue(-100, 100) / 100.0f);
    SetSoundPitch(s, tom * j);
    SetSoundVolume(s, volume);
    PlaySound(s);
}

void GrupoParar(GrupoSom *g)
{
    for (int k = 0; k < g->n; k++) {
        StopSound(g->v[k]);
        for (int a = 0; a < VOZES_EXTRA; a++) StopSound(g->alias[k][a]);
    }
}

static void GrupoDescarregar(GrupoSom *g)
{
    for (int k = 0; k < g->n; k++) {
        for (int a = 0; a < VOZES_EXTRA; a++) UnloadSoundAlias(g->alias[k][a]);
        UnloadSound(g->v[k]);
    }
    g->n = 0;
}
GrupoSom gArma[NUM_SONS_ARMA];
GrupoSom gVetorCarga, gOrbeImplosao, gFragDourado, gRaioColuna, gMorteInimigo;
// Variações dos SFX gerais (os sfx* antigos apontam para a variação 0 de cada um)
static GrupoSom gAcerto, gErro, gDano, gPegar, gSelecao, gTiro, gAvisoParry, gParry, gEstatica, gDedutivo;

static GrupoSom *const GRUPOS_GERAIS[] = { &gAcerto, &gErro, &gDano, &gPegar, &gSelecao, &gTiro, &gAvisoParry, &gParry, &gEstatica, &gDedutivo };


// Grupos dos sons da Luci (as sementes são as mesmas da prévia aprovada: o jogo toca exatamente o que foi ouvido)
GrupoSom gLuciFio, gLuciCorrupcao, gLuciExplosao, gLuciTelegrafia, gLuciInvestida, gLuciOrbe, gLuciRebatido,
                gLuciVarredura, gLuciOnda, gLuciRacha, gLuciQueda, gLuciColapso, gLuciRenascer, gLuciVapor;
GrupoSom *const GRUPOS_LUCI[] = { &gLuciFio, &gLuciCorrupcao, &gLuciExplosao, &gLuciTelegrafia, &gLuciInvestida, &gLuciOrbe,
                                         &gLuciRebatido, &gLuciVarredura, &gLuciOnda, &gLuciRacha, &gLuciQueda, &gLuciColapso,
                                         &gLuciRenascer, &gLuciVapor };

static void CarregarSonsLuci(void)
{
    static const ReceitaSom receitas[] = { ReceitaLuciFio, ReceitaLuciCorrupcao, ReceitaLuciExplosao, ReceitaLuciTelegrafia,
                                           ReceitaLuciInvestida, ReceitaLuciOrbe, ReceitaLuciRebatido, ReceitaLuciVarredura,
                                           ReceitaLuciOnda, ReceitaLuciRacha, ReceitaLuciQueda, ReceitaLuciColapso,
                                           ReceitaLuciRenascer, ReceitaLuciVapor };
    for (int k = 0; k < NUM_GRUPOS_LUCI; k++) {
        GrupoSom *g = GRUPOS_LUCI[k];
        memset(g, 0, sizeof(*g));
        g->n = 2;
        g->jitter = 0.03f;
        g->ultima = -1;
        for (int v = 0; v < g->n; v++) {
            g_sinteseSemente = 1234u + (unsigned int)k * 77u + (unsigned int)v * 991u;
            g->v[v] = receitas[k](v);
            for (int a = 0; a < VOZES_EXTRA; a++) g->alias[v][a] = LoadSoundAlias(g->v[v]);
        }
    }
}

// Grupos dos inimigos da Biblioteca (sementes iguais às da prévia: o jogo toca exatamente o que foi ouvido)
GrupoSom gIniBote, gIniLeque, gIniPacote, gIniTecla, gIniAnel, gIniEco, gIniBarreira, gIniCarga, gIniInvestida,
                gIniSuperaquece, gIniCura, gIniFantasma, gIniPhisher, gIniBloqueio, gIniSurge;
GrupoSom *const GRUPOS_INI[] = { &gIniBote, &gIniLeque, &gIniPacote, &gIniTecla, &gIniAnel, &gIniEco, &gIniBarreira,
                                        &gIniCarga, &gIniInvestida, &gIniSuperaquece, &gIniCura, &gIniFantasma, &gIniPhisher,
                                        &gIniBloqueio, &gIniSurge };

static void CarregarSonsInimigos(void)
{
    static const ReceitaSom receitas[] = { ReceitaIniBote, ReceitaIniLeque, ReceitaIniPacote, ReceitaIniTecla, ReceitaIniAnel,
                                           ReceitaIniEco, ReceitaIniBarreira, ReceitaIniCarga, ReceitaIniInvestida,
                                           ReceitaIniSuperaquece, ReceitaIniCura, ReceitaIniFantasma, ReceitaIniPhisher,
                                           ReceitaIniBloqueio, ReceitaIniSurge };
    static const int variacoes[] = { 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 3, 2 };
    static const float jitter[]  = { 0.05f, 0.04f, 0.05f, 0.06f, 0.03f, 0.04f, 0.02f, 0.0f, 0.03f, 0.03f, 0.02f, 0.04f, 0.0f, 0.05f, 0.03f };
    for (int k = 0; k < NUM_GRUPOS_INI; k++) {
        GrupoSom *g = GRUPOS_INI[k];
        memset(g, 0, sizeof(*g));
        g->n = variacoes[k];
        g->jitter = jitter[k];
        g->ultima = -1;
        for (int v = 0; v < g->n; v++) {
            g_sinteseSemente = 5678u + (unsigned int)k * 77u + (unsigned int)v * 991u;
            g->v[v] = receitas[k](v);
            for (int a = 0; a < VOZES_EXTRA; a++) g->alias[v][a] = LoadSoundAlias(g->v[v]);
        }
    }
}

// Grupos dos chefes e elites (sementes iguais às da prévia)
GrupoSom gChefeVirada, gChefeInvoca, gChefeEscudo, gChefeMorte, gChefeMira, gChefeInvestida, gChefeRajada,
                gChefeEspiral, gSentinelaFeixe, gRetroAviso, gRetroTiro, gJanelaResumo, gSilLinhas, gSilCorte;
GrupoSom *const GRUPOS_CHEFES[] = { &gChefeVirada, &gChefeInvoca, &gChefeEscudo, &gChefeMorte, &gChefeMira,
                                           &gChefeInvestida, &gChefeRajada, &gChefeEspiral, &gSentinelaFeixe, &gRetroAviso,
                                           &gRetroTiro, &gJanelaResumo, &gSilLinhas, &gSilCorte };

static void CarregarSonsChefes(void)
{
    static const ReceitaSom receitas[] = { ReceitaChefeVirada, ReceitaChefeInvoca, ReceitaChefeEscudo, ReceitaChefeMorte,
                                           ReceitaChefeMira, ReceitaChefeInvestida, ReceitaChefeRajada, ReceitaChefeEspiral,
                                           ReceitaSentinelaFeixe, ReceitaRetroAviso, ReceitaRetroTiro, ReceitaJanelaResumo,
                                           ReceitaSilLinhas, ReceitaSilCorte };
    // sem variação de tom nos que precisam acabar junto com o aviso (mira, espiral, rebobinar, resumo)
    static const float jitter[] = { 0.02f, 0.04f, 0.03f, 0.02f, 0.0f, 0.03f, 0.04f, 0.0f, 0.03f, 0.0f, 0.05f, 0.0f, 0.03f, 0.03f };
    for (int k = 0; k < NUM_GRUPOS_CHEFES; k++) {
        GrupoSom *g = GRUPOS_CHEFES[k];
        memset(g, 0, sizeof(*g));
        g->n = 2;
        g->jitter = jitter[k];
        g->ultima = -1;
        for (int v = 0; v < g->n; v++) {
            g_sinteseSemente = 9100u + (unsigned int)k * 77u + (unsigned int)v * 991u;
            g->v[v] = receitas[k](v);
            for (int a = 0; a < VOZES_EXTRA; a++) g->alias[v][a] = LoadSoundAlias(g->v[v]);
        }
    }
}

// Grupos das batalhas do Bloco 1 (sementes iguais às da prévia)
GrupoSom gB1Caixa, gB1Verdade, gB1DanoAlma, gB1Golpe, gB1Lupa, gB1Verificado, gB1Sobrecarga;
GrupoSom *const GRUPOS_B1[] = { &gB1Caixa, &gB1Verdade, &gB1DanoAlma, &gB1Golpe, &gB1Lupa, &gB1Verificado, &gB1Sobrecarga };

static void CarregarSonsBloco1(void)
{
    static const ReceitaSom receitas[] = { ReceitaB1Caixa, ReceitaB1Verdade, ReceitaB1DanoAlma, ReceitaB1Golpe,
                                           ReceitaB1Lupa, ReceitaB1Verificado, ReceitaB1Sobrecarga };
    // sem variação de tom na verdade (o tom sobe a cada uma), na lupa e no carimbo (casam com a animação)
    static const float jitter[] = { 0.03f, 0.0f, 0.05f, 0.02f, 0.0f, 0.0f, 0.0f };
    for (int k = 0; k < NUM_GRUPOS_B1; k++) {
        GrupoSom *g = GRUPOS_B1[k];
        memset(g, 0, sizeof(*g));
        g->n = 2;
        g->jitter = jitter[k];
        g->ultima = -1;
        for (int v = 0; v < g->n; v++) {
            g_sinteseSemente = 12000u + (unsigned int)k * 77u + (unsigned int)v * 991u;
            g->v[v] = receitas[k](v);
            for (int a = 0; a < VOZES_EXTRA; a++) g->alias[v][a] = LoadSoundAlias(g->v[v]);
        }
    }
}

// Sons gerais redesenhados (G4) e os do jogador. Os sfx* antigos passam a apontar para a variação 0 de cada
// grupo novo, então todas as chamadas antigas (menus, cartas, loja, laboratórios, quizzes) tocam o som novo.
GrupoSom gJogEsquiva, gJogPulso, gJogParry, gJogMorte;
GrupoSom *const GRUPOS_JOG[] = { &gJogEsquiva, &gJogPulso, &gJogParry, &gJogMorte };

static void CarregarSonsInterface(void)
{
    GrupoSom *grupos[] = { &gSelecao, &gAcerto, &gErro, &gDano, &gPegar, &gTiro, &gJogEsquiva, &gJogPulso, &gJogParry, &gJogMorte };
    static const ReceitaSom receitas[] = { ReceitaUISelecao, ReceitaUIAcerto, ReceitaUIErro, ReceitaDanoJogador, ReceitaUIPegar,
                                           ReceitaTiroGenerico, ReceitaEsquiva, ReceitaPulsoJogador, ReceitaParryArmar, ReceitaMorteJogador };
    static const int variacoes[] = { 3, 2, 2, 3, 3, 3, 3, 2, 2, 2 };
    // o acerto e o pegar quase não variam: quem chama já sobe o tom (combos, sequência de coletas)
    static const float jitter[] = { 0.03f, 0.0f, 0.03f, 0.06f, 0.01f, 0.05f, 0.05f, 0.02f, 0.03f, 0.0f };
    for (int k = 0; k < (int)(sizeof(grupos) / sizeof(grupos[0])); k++) {
        GrupoSom *g = grupos[k];
        if (k < 6) GrupoDescarregar(g);                 // tira o som antigo (sfx* e as variações)
        memset(g, 0, sizeof(*g));
        g->n = variacoes[k];
        g->jitter = jitter[k];
        g->ultima = -1;
        for (int v = 0; v < g->n; v++) {
            g_sinteseSemente = 15000u + (unsigned int)k * 77u + (unsigned int)v * 991u;
            g->v[v] = receitas[k](v);
            for (int a = 0; a < VOZES_EXTRA; a++) g->alias[v][a] = LoadSoundAlias(g->v[v]);
        }
    }
    sfxSelecao = gSelecao.v[0];
    sfxAcerto  = gAcerto.v[0];
    sfxErro    = gErro.v[0];
    sfxDano    = gDano.v[0];
    sfxPegar   = gPegar.v[0];
    sfxTiro    = gTiro.v[0];
}

// Alguns sons podem pedir para tocar muitas vezes no mesmo instante (vários tiros no escudo, uma onda inteira
// surgindo): com isto, tocam no máximo uma vez a cada "intervalo" segundos
bool SomLiberado(double *ultimo, double intervalo)
{
    double agora = GetTime();
    if (agora - *ultimo < intervalo) return false;
    *ultimo = agora;
    return true;
}

static void CarregarSonsNovos(void)
{
    static const ReceitaSom armas[NUM_SONS_ARMA] = {
        ReceitaDisparador, ReceitaVetor, ReceitaRoteador, ReceitaFluxo, ReceitaOrbe,
        ReceitaFragmentadora, ReceitaPulsoFW, ReceitaKatana, ReceitaRaioCircuito, ReceitaRaioClassico
    };
    static const float jit[NUM_SONS_ARMA] = { 0.05f, 0.03f, 0.04f, 0.06f, 0.04f, 0.05f, 0.03f, 0.06f, 0.0f, 0.04f };
    for (int a = 0; a < NUM_SONS_ARMA; a++)
        GrupoCarregar(&gArma[a], armas[a], 3, jit[a]);
    GrupoCarregar(&gVetorCarga,   ReceitaVetorCarga,   2, 0.0f);
    GrupoCarregar(&gOrbeImplosao, ReceitaOrbeImplosao, 2, 0.04f);
    GrupoCarregar(&gFragDourado,  ReceitaFragDourado,  2, 0.03f);
    GrupoCarregar(&gRaioColuna,   ReceitaRaioColuna,   3, 0.03f);
    GrupoCarregar(&gMorteInimigo, ReceitaMorteInimigo, 3, 0.05f);
    CarregarSonsLuci();                     // a luta final
    CarregarSonsInimigos();                 // os inimigos da Biblioteca
    CarregarSonsChefes();                   // chefes e elites
    CarregarSonsBloco1();                   // batalhas do Bloco 1
    CarregarAmbientes();
}

// Tiro da arma (tom: 1 = normal; o Fluxo passa o calor, o eco do Pulso passa um tom acima)
float g_tomPulsoFW = 1.0f;     // o eco do Pulso de Firewall toca um pouco mais agudo e mais baixo
void TocarSomArma(int arma, float tom)
{
    if (arma < 0 || arma >= NUM_SONS_ARMA) return;
    TocarGrupo(&gArma[arma], tom, 1.0f);
}

static void DescarregarSonsNovos(void)
{
    for (int a = 0; a < NUM_SONS_ARMA; a++) GrupoDescarregar(&gArma[a]);
    GrupoDescarregar(&gVetorCarga);
    GrupoDescarregar(&gOrbeImplosao);
    GrupoDescarregar(&gFragDourado);
    GrupoDescarregar(&gRaioColuna);
    GrupoDescarregar(&gMorteInimigo);
    for (int k = 0; k < NUM_GRUPOS_LUCI; k++) GrupoDescarregar(GRUPOS_LUCI[k]);
    for (int k = 0; k < NUM_GRUPOS_INI; k++)  GrupoDescarregar(GRUPOS_INI[k]);
    for (int k = 0; k < NUM_GRUPOS_CHEFES; k++) GrupoDescarregar(GRUPOS_CHEFES[k]);
    for (int k = 0; k < NUM_GRUPOS_B1; k++)     GrupoDescarregar(GRUPOS_B1[k]);
    for (int k = 0; k < NUM_GRUPOS_JOG; k++)    GrupoDescarregar(GRUPOS_JOG[k]);
    for (int a = 0; a < NUM_AMBIENTES; a++) {
        if (g_amb[a].stream.buffer) UnloadMusicStream(g_amb[a]);
        free(g_ambWav[a]);
        g_ambWav[a] = NULL;
    }
}

static void PararSonsNovos(void)
{
    for (int a = 0; a < NUM_SONS_ARMA; a++) GrupoParar(&gArma[a]);
    GrupoParar(&gVetorCarga);
    GrupoParar(&gOrbeImplosao);
    GrupoParar(&gFragDourado);
    GrupoParar(&gRaioColuna);
    GrupoParar(&gMorteInimigo);
    for (int k = 0; k < NUM_GRUPOS_LUCI; k++) GrupoParar(GRUPOS_LUCI[k]);
    for (int k = 0; k < NUM_GRUPOS_INI; k++)  GrupoParar(GRUPOS_INI[k]);
    for (int k = 0; k < NUM_GRUPOS_CHEFES; k++) GrupoParar(GRUPOS_CHEFES[k]);
    for (int k = 0; k < NUM_GRUPOS_B1; k++)     GrupoParar(GRUPOS_B1[k]);
    for (int k = 0; k < NUM_GRUPOS_JOG; k++)    GrupoParar(GRUPOS_JOG[k]);
    for (int k = 0; k < NUM_GRUPOS_GERAIS; k++) GrupoParar(GRUPOS_GERAIS[k]);
    g_ambVolume = -3.0f;                 // o ambiente "congela" junto e volta sozinho, devagar, ~6 s depois
}

void IniciarSons(void)
{
    InitAudioDevice();
    audioPronto = IsAudioDeviceReady();

    if (!audioPronto)
        return;

    // ---------------------------------------------------------------
    // SFX — DESIGN BASEADO NA ESTÉTICA DO JOGO
    // ---------------------------------------------------------------

    // ACERTO: jingle chiptune de 3 notas (C5–E5–G5) em onda quadrada 8-bit
    // → "Dado validado" — alegre, claro, retro. Tom de console dos anos 80.
    sfxAcerto  = GerarJingleChiptune(523.25f, 3, 0.072f, 0.40f);

    // ERRO: crash descendente com glitch (onda quadrada + ruído + bit-crush 5-bit)
    // → "Corrupção detectada" — grave, impactante, digital. A alucinação cresce.
    sfxErro    = GerarCrashGlitch(260.0f, 65.0f, 0.40f, 0.38f);

    // DANO (levar bala): buzz descendente + burst de ruído — "packet corrompido"
    // Som curto e seco como no Undertale. Ondas quadradas, sem sutileza.
    sfxDano    = GerarCrashGlitch(320.0f, 100.0f, 0.12f, 0.42f);

    // PEGAR (verdade/item): dois pings ascendentes em pulse estreito (0.25)
    // → "Dado recebido" — confirma, agrada, curto. F#5 → A#5.
    sfxPegar   = GerarPingDuplo(740.0f, 932.0f, 0.055f, 0.32f);

    // SELEÇÃO DE MENU: blip ultra-curto de onda quadrada (1-bit feel)
    // → Cursor digital — seco, preciso, instantâneo.
    sfxSelecao = GerarSom(440.0f, 440.0f, 0.025f, 0.14f, 1);

    // TIRO (Disparador de Dados — Bloco 2): zap de pulse descendente
    // → Pulso de dados enviado — nasal, digital, sem eco.
    sfxTiro    = GerarSom(900.0f, 220.0f, 0.065f, 0.18f, 3);

    // --- BLOCO 3 / BATALHA AVANÇADA ---

    // AVISO DE PARRY: chirp em 8 degraus ascendentes (portamento glitchado)
    // → "Alerta de sistema" — como um beep de erro de hardware, urgente.
    sfxAvisoParry    = GerarChirpAlerta(440.0f, 1760.0f, 0.11f, 0.22f);

    // PARRY PERFEITO: impacto grave bit-crushed + ring ascendente
    // → "Contradição refutada" — peso do impacto + resolução tonal.
    sfxParryPerfeito = GerarParryPerfeito(0.65f);

    // ESTÁTICA (Luci alucinando): ruído modulado por LFO duplo + crackles
    // → "Sinal degradado" — a Luci está corrompida, instável, mentindo.
    sfxEstatica      = GerarEstaticaLuci(0.22f);

    // DISPARO DEDUTIVO: laser em pulse 0.125 com harmônico de quinta
    // → "Argumento disparado" — penetrante, lógico, cirúrgico.
    sfxDedutivo      = GerarDisparoDedutivo(0.32f);

    // --- VARIAÇÕES: o mesmo desenho com parâmetros um pouco diferentes (a variação 0 é o som original) ---
    // O tom continua sendo escolhido por quem chama (combos etc.); o jitter é pequeno nos sons "musicais".
    GrupoDeSons(&gAcerto, sfxAcerto, GerarJingleChiptune(523.25f, 3, 0.064f, 0.38f), GerarJingleChiptune(523.25f, 3, 0.080f, 0.40f), 0.008f);
    GrupoDeSons(&gErro,   sfxErro,   GerarCrashGlitch(240.0f, 60.0f, 0.42f, 0.38f), GerarCrashGlitch(285.0f, 72.0f, 0.37f, 0.36f), 0.04f);
    GrupoDeSons(&gDano,   sfxDano,   GerarCrashGlitch(295.0f, 90.0f, 0.13f, 0.42f), GerarCrashGlitch(345.0f, 115.0f, 0.11f, 0.40f), 0.07f);
    GrupoDeSons(&gPegar,  sfxPegar,  GerarPingDuplo(740.0f, 932.0f, 0.048f, 0.30f), GerarPingDuplo(740.0f, 988.0f, 0.060f, 0.30f), 0.015f);
    GrupoDeSons(&gSelecao, sfxSelecao, GerarSom(440.0f, 452.0f, 0.022f, 0.13f, 1), GerarSom(446.0f, 436.0f, 0.028f, 0.13f, 1), 0.03f);
    GrupoDeSons(&gTiro,   sfxTiro,   GerarSom(960.0f, 240.0f, 0.060f, 0.17f, 3), GerarSom(850.0f, 200.0f, 0.072f, 0.18f, 3), 0.06f);
    // AVISO DE PARRY: trocado por um aviso metálico pesado (ReceitaAvisoParry)
    UnloadSound(sfxAvisoParry);
    GrupoCarregar(&gAvisoParry, ReceitaAvisoParry, 3, 0.02f);
    sfxAvisoParry = gAvisoParry.v[0];
    // PARRY: trocado por um choque de metal pesado (ReceitaParry)
    UnloadSound(sfxParryPerfeito);
    GrupoCarregar(&gParry, ReceitaParry, 3, 0.03f);
    sfxParryPerfeito = gParry.v[0];
    GrupoDeSons(&gEstatica, sfxEstatica, GerarEstaticaLuci(0.21f), GerarEstaticaLuci(0.23f), 0.05f);
    GrupoDeSons(&gDedutivo, sfxDedutivo, GerarDisparoDedutivo(0.30f), GerarDisparoDedutivo(0.33f), 0.05f);

    // --- ARMAS e AMBIENTES DOS MAPAS (Sons 2.0) ---
    CarregarSonsNovos();

    // --- G4: os sons gerais no padrão da luta da Luci (por último: não mexe nas sementes dos outros) ---
    CarregarSonsInterface();

    // --- Carregar BGM ---
    bgmMenu       = CarregarBGM("resources/audio/bgm_menu.ogg");
    bgmSala       = CarregarBGM("resources/audio/bgm_sala.ogg");
    bgmCorredor   = CarregarBGM("resources/audio/bgm_corredor.ogg");
    bgmBatalha    = CarregarBGM("resources/audio/bgm_batalha.ogg");
    bgmMestre     = CarregarBGM("resources/audio/bgm_mestre.ogg");
    bgmBiblioteca = CarregarBGM("resources/audio/bgm_biblioteca.ogg");
    bgmGameover   = CarregarBGM("resources/audio/bgm_gameover.ogg");
    bgmAtrio      = CarregarBGM("resources/audio/bgm_atrio.ogg");
    bgmLuci       = CarregarBGM("resources/audio/bgm_luci.ogg");
    bgmLuciChefe[0] = CarregarBGM("resources/audio/bgm_luci_chefe1.ogg");
    bgmLuciChefe[1] = CarregarBGM("resources/audio/bgm_luci_chefe2.ogg");
    bgmLuciChefe[2] = CarregarBGM("resources/audio/bgm_luci_chefe3.ogg");
    bgmAlucinador[0] = CarregarBGM("resources/audio/bgm_alucinador1.ogg");
    bgmAlucinador[1] = CarregarBGM("resources/audio/bgm_alucinador2.ogg");
    bgmJanela[0] = CarregarBGM("resources/audio/bgm_janela1.ogg");
    bgmJanela[1] = CarregarBGM("resources/audio/bgm_janela2.ogg");
}

// Para todos os SFX de uma vez (a 1ª queda da Luci "congela" o áudio)
void PararSons(void)
{
    if (!audioPronto)
        return;

    Sound todos[] = { sfxAcerto, sfxErro, sfxDano, sfxPegar, sfxSelecao, sfxTiro,
                      sfxAvisoParry, sfxParryPerfeito, sfxDedutivo };
    for (int i = 0; i < (int)(sizeof(todos) / sizeof(todos[0])); i++)
        StopSound(todos[i]);
    PararSonsNovos();                   // variações, armas e o ambiente do mapa

    // Para a música também (e a que ainda estava sumindo de uma troca)
    if (BgmValida(bgmSaindo)) StopMusicStream(*bgmSaindo);
    bgmSaindo = NULL;
    if (bgmAtual && bgmAtual->stream.buffer)
        PauseMusicStream(*bgmAtual);
}

// tom: 1 = normal, 2 = uma oitava acima (combos usam para o acerto ficar mais agudo)
void TocarSomTom(Sound s, float tom)
{
    if (!audioPronto)
        return;

    // Sons 2.0: se o som tem variações, toca uma delas (sem repetir a anterior, em outra voz)
    for (int k = 0; k < NUM_GRUPOS_GERAIS; k++)
        if (GRUPOS_GERAIS[k]->n > 0 && GRUPOS_GERAIS[k]->v[0].stream.buffer == s.stream.buffer) {
            TocarGrupo(GRUPOS_GERAIS[k], tom, 1.0f);
            return;
        }

    SetSoundPitch(s, tom);
    PlaySound(s);
}

void TocarSom(Sound s)
{
    TocarSomTom(s, 1.0f);
}

void EncerrarSons(void)
{
    if (!audioPronto)
        return;

    // Para BGM antes de descarregar
    if (bgmAtual && bgmAtual->stream.buffer)
        StopMusicStream(*bgmAtual);
    if (BgmValida(bgmSaindo))
        StopMusicStream(*bgmSaindo);
    bgmSaindo = NULL;

    if (bgmMenu.stream.buffer)       UnloadMusicStream(bgmMenu);
    if (bgmSala.stream.buffer)       UnloadMusicStream(bgmSala);
    if (bgmCorredor.stream.buffer)   UnloadMusicStream(bgmCorredor);
    if (bgmBatalha.stream.buffer)    UnloadMusicStream(bgmBatalha);
    if (bgmMestre.stream.buffer)     UnloadMusicStream(bgmMestre);
    if (bgmBiblioteca.stream.buffer) UnloadMusicStream(bgmBiblioteca);
    if (bgmGameover.stream.buffer)   UnloadMusicStream(bgmGameover);
    if (bgmAtrio.stream.buffer)      UnloadMusicStream(bgmAtrio);
    if (bgmLuci.stream.buffer)       UnloadMusicStream(bgmLuci);
    for (int i = 0; i < 3; i++)
        if (bgmLuciChefe[i].stream.buffer) UnloadMusicStream(bgmLuciChefe[i]);
    for (int i = 0; i < 2; i++) {
        if (bgmAlucinador[i].stream.buffer) UnloadMusicStream(bgmAlucinador[i]);
        if (bgmJanela[i].stream.buffer)     UnloadMusicStream(bgmJanela[i]);
    }

    // Sons 2.0: cada grupo descarrega as vozes e as variações (a variação 0 é o sfx* antigo)
    for (int k = 0; k < NUM_GRUPOS_GERAIS; k++)
        GrupoDescarregar(GRUPOS_GERAIS[k]);
    DescarregarSonsNovos();
    CloseAudioDevice();
}
