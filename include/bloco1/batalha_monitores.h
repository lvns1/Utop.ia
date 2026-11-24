// Bloco 1: motor das batalhas dos monitores e da ALUCINAÇÃO MESTRE.
#ifndef UTOPIA_BATALHA_MONITORES_H
#define UTOPIA_BATALHA_MONITORES_H

#include <stdbool.h>
#include "raylib.h"
#include "raymath.h"
#include "../core/config.h"
#include "../core/tipos.h"

// =============================================================
// BATALHAS DO BLOCO 1 (motor usado pelos monitores e pela ALUCINAÇÃO MESTRE)
//   O antigo Bloco 2 (Arquivo de Treinamento) foi substituído pela
//   Biblioteca dos Ecos (roguelite de tiro, mais abaixo).
// =============================================================

#define PONTOS_ACERTO_B2      5          // acerto no debate com a ALUCINAÇÃO MESTRE
#define DURACAO_DESVIO_B2     5.0f       // DEFESA: esquiva quando erra uma rodada
#define DURACAO_ATAQUE_B2     3.5f       // ATAQUE: minigame depois de acertar (mais curto)
#define ESPERA_TEXTO_B2       1.5f       // textos da batalha avançam sozinhos depois disso (ENTER adianta)
#define ESPERA_COLA_B2        3.0f       // na abertura com a cola da Luci, um pouco mais para ler
#define ESPERA_FIM_B2         2.0f       // texto final da batalha
#define HP_MAX_B2             20
#define MAX_OPCOES_B2         4
#define ATRASO_DESAFIO_B2     0.6f       // evita responder sem querer apertando ENTER rápido
#define ESPERA_TELA_DERROTA_B1 1.0f      // game over: segundos antes de aceitar a escolha (não pula sem querer)

// Visual dos inimigos das batalhas
enum { GUARDIAO_ALUCINACAO, GUARDIAO_MESTRE, NUM_GUARDIOES };

// Por que a batalha começou (decide o que acontece no fim)
typedef enum { MOTIVO_MONITOR, MOTIVO_CHEFE } MotivoBatalhaB2;

// Cada monitor do Bloco 1 tem um minigame com a cara do seu tema
typedef enum {
    VAR_NORMAL,         // minigame simples (pegar verdades e desviar)
    VAR_FALSAS,         // monitor 1: verdades falsificadas piscam em magenta (confiança != verdade)
    VAR_SEPARAR,        // monitor 2: pegar só o tipo que a Luci pedir (intrínseca x extrínseca)
    VAR_LUPA            // monitor 3: só a lupa em volta da Luci revela a alucinação (verificação)
} VarianteMinigame;

typedef struct {
    const char *nome;
    const char *fundamento;      // subtítulo na batalha
    Color cor;
    const char *glifos;          // o que ele atira na esquiva
} DefGuardiaoB2;

// Uma rodada de escolha: a frase do inimigo e as ações possíveis
typedef struct {
    const char *enunciado;
    const char *opcoes[MAX_OPCOES_B2];
    int numOpcoes;
    int correta;
    const char *explicacao;
} RodadaEscolha;

// ---------- Bloco 1: batalhas dos monitores ----------
// A alucinação diz uma frase curta e o jogador escolhe a AÇÃO certa
// (como o AGIR do Undertale). Cada acerto enfraquece a alucinação.
// Conteúdo do "Quizz - missão 1.pdf", resumido em frases curtas.

#define RODADAS_POR_MONITOR   3
#define PONTOS_ACERTO_B1      5

#define PONTOS_ACERTO_COM_AJUDA  2      // acertar usando a dica vale menos

// Acertou: o minigame vira ATAQUE. Cada verdade pega fere a alucinação,
// cada golpe recebido tira força do ataque. Errou: vira DEFESA, com
// bem poucas verdades (vida) caindo.
// Um ataque bom (3 verdades) tira ~65%. O 2º acerto seguido é COMBO (x1,5),
// então quem acerta tudo derruba a alucinação com 2 perguntas.
#define DANO_BASE_ATAQUE        30.0f
#define DANO_POR_VERDADE        12.0f
#define PERDA_POR_GOLPE         4.0f
#define DANO_MINIMO_ATAQUE      15.0f
#define DANO_MAXIMO_ATAQUE      70.0f   // nenhum ataque sozinho (sem combo) deixa menos de 30%
#define FATOR_DANO_COM_AJUDA    0.6f    // atacar depois de pedir ajuda fere menos
#define FATOR_COMBO             1.5f    // a partir do 2º acerto seguido
#define LIMITE_GOLPE_FINAL      30.0f   // alucinação abaixo disso: aparece o botão VERIFICAR
#define CHANCE_VERDADE_ATAQUE   3       // 1 em 3 balas é verdade no ataque
#define CHANCE_VERDADE_DEFESA   12      // 1 em 12 balas é verdade na defesa

// Variações do minigame (AJUSTE AQUI)
#define CHANCE_VERDADE_FALSA    40      // monitor 1: % das verdades que são falsificadas
#define CHANCE_ICONE_ATAQUE     2       // monitor 2: 1 em 2 balas é um ícone CONTRADIZ/INVENTA no ataque
#define CHANCE_ICONE_DEFESA     4       // ... e 1 em 4 na defesa
#define RAIO_LUPA               54.0f   // monitor 3: alcance da lupa em volta da Luci

// Golpe final: o botão VERIFICAR acaba com a alucinação numa animação
#define DURACAO_GOLPE_FINAL     1.7f
#define IMPACTO_GOLPE_FINAL     0.55f   // quando a lupa acerta a alucinação

// ALUCINAÇÃO MESTRE (fim do Bloco 1): DEBATE COM FONTES.
// Ela afirma coisas, misturando fatos reais (pouco conhecidos) com invenções ditas
// com toda a confiança. O jogador CONCORDA ou CONTESTA, e pode PEDIR FONTE à Luci (H):
//   contestar uma alucinação COM fonte = golpe grande; SEM fonte = quase não fere
//   concordar com alucinação ou contestar um fato = ela se cura e você perde vida
// Cada fonte gasta 1 de estabilidade. Instável, a Luci pode trazer uma FONTE FALSA
// (confiante e sem citar nada). Acertar sem pedir fonte devolve estabilidade.
#define NUM_AFIRMACOES             29
#define DANO_CONTESTAR_COM_FONTE   34.0f    // 3 golpes com fonte levam ao VERIFICAR
#define DANO_CONTESTAR_SEM_FONTE   12.0f
#define CURA_ERRO_DEBATE           10.0f
#define PERDA_HP_DEBATE            4

#define OPCOES_DEBATE { "CONCORDAR", "CONTESTAR" }      // correta 1 = é alucinação
// ---------- Estado da batalha ----------

typedef enum { B2_INTRO, B2_DESAFIO, B2_RESULTADO, B2_ENCOLHENDO, B2_DESVIO, B2_EXPANDINDO, B2_FIM,
               B2_GOLPE_FINAL,      // alucinação por um fio: botão VERIFICAR
               B2_FINALIZANDO       // animação do golpe final
} FaseB2;

typedef struct {
    int guardiao;
    FaseB2 fase;
    int rodada;
    int acertos;
    int erros;
    float alucinacaoGuardiao;   // 100 = no auge, 0 = resolvido (duelo)
    bool ultimoAcerto;
    bool venceu;
    bool derrota;               // a vida chegou a zero
    float tempoFase;

    int selecao;
    int fonteDesafio;
    char enunciado[768];
    char opcoesTexto[MAX_OPCOES_B2][256];

    int hp;
    int hpTeto;                 // vida com que a batalha começou: as verdades curam só o dano desta batalha
    Vector2 alma;                   // a Luci, que o jogador guia na caixa de esquiva
    Vector2 rastroAlma[5];          // últimas posições dela (o rastro de luz)
    float cronoRastro;
    float invencivel, tremor, anim, tempoDesvio, duracaoDesvio, cronoBala, intervaloBala, velocidadeBala;
    int padrao;
    Bala balas[MAX_BALAS];

    char texto[768];
    float visiveis;

    // Conteúdo e estilo (duelo do Silogista ou batalha de monitor)
    const RodadaEscolha *rodadas;
    int numRodadas;
    const char *textoVitoria;
    const char *subtitulo;
    bool estiloAcao;            // frase curta do inimigo + botões de ação (Bloco 1)
    bool pegarBoas;             // na esquiva caem verdades para PEGAR
    int monitor;                // monitor do Bloco 1 (-1 = não é monitor)
    int pegas;                  // verdades pegas
    double inicio;              // telemetria

    // Ajuda da Luci (H)
    const char *const *dicas;   // uma dica por rodada
    int *estabilidade;          // estabilidade da Luci (fica no main, vai para o save)
    int *ajudasTotal;
    bool ajudaNaRodada;         // já pediu ajuda nesta rodada
    bool dicaAlucinada;         // a Luci alucinou a dica desta rodada
    int opcaoSugerida;          // opção que a Luci "jurou" ser a certa (quando alucina)
    char dica[160];
    float tempoDica;
    int acertosComAjuda;
    int alucinacoesSeguidas;    // vezes que o jogador seguiu uma dica alucinada

    // Minigame depois de cada resposta
    bool faseAtaque;            // true = acertou (ataque), false = errou (defesa)
    int verdadesNoAtaque;       // verdades pegas neste ataque
    int golpesNaRodada;         // golpes recebidos neste minigame
    float danoMostrado;         // número de dano que aparece sobre o inimigo
    float tempoDano;
    bool danoCombo;             // o dano mostrado foi em combo

    // Ritmo e "peso" dos golpes
    float esperaTexto;          // há quanto tempo o texto está completo (avança sozinho)
    int combo;                  // acertos seguidos (2 ou mais = dano x1,5)
    float pausaImpacto;         // congela a batalha por um instante quando o golpe acerta
    float flashInimigo;         // inimigo piscando em branco
    float flashDano;            // borda vermelha quando a Luci leva um golpe
    float anelGolpe;            // onda dourada saindo do inimigo quando o golpe é forte
    float preparo, preparoTotal;    // > 0: a caixa mostra a regra com ícones e conta "3, 2, 1" (as balas esperam)
    int minigamesVistos;        // as duas primeiras contagens são mais longas

    // Variação do minigame e cola da Luci
    VarianteMinigame variante;
    int ondaTipo;               // SEPARAR: tipo que a Luci pediu nesta onda (0 = CONTRADIZ, 1 = INVENTA)
    int ondaInicial;
    const char *cola;           // NULL = sem cola
    float tempoCola;            // tempo desde que a cola foi para o canto

    // ALUCINAÇÃO MESTRE (debate com fontes)
    bool chefe;
    int ordemChefe[NUM_AFIRMACOES];     // afirmações embaralhadas

    // Partículas de acerto
    struct { Vector2 pos, vel; float vida; Color cor; } particulas[48];

    // Game over do Bloco 1: a integridade da Luci zerou (issue 8)
    bool telaDerrota;           // mostrando a tela de derrota (música de game over); E sai da batalha
    float tempoDerrota;
} BatalhaB2;

// Cartão de resumo: aparece depois de cada desafio com uma frase para guardar
typedef struct {
    bool ativo;
    int icone;                  // monitor (0..2) ou o chefe (3)
    bool sucesso;
    const char *titulo;
    const char *frase;
    const Dialogo *saberMais;
    double inicio;
} CartaoResumo;

// O que o fim de uma batalha precisa mexer (fica no main)
typedef struct {
    SessaoDialogo *dlg;
    Transicao *transicao;
    ProgressoCorredor *progresso;
    BatalhaB2 *batalha;
    bool *pedirAutoSave;
    TelaAtual *salaDaBatalha;
    MotivoBatalhaB2 *motivo;
    CartaoResumo *cartao;
    bool *chefeVencido;         // venceu a ALUCINAÇÃO MESTRE
} ContextoB2;

// ----- Variáveis compartilhadas -----
extern const Vector2 SPAWN_CORREDOR_VOLTA;
extern const char *TITULOS_RESUMO_MONITOR[TOTAL_MONITORES];
extern const char *FRASES_RESUMO_MONITOR[TOTAL_MONITORES];
extern Dialogo saberMaisMonitores[TOTAL_MONITORES];

// ----- Funções -----
int HpDoCorredor(const ProgressoCorredor *p);
void GuardarHpDoCorredor(ProgressoCorredor *p, int hp, bool derrota);
void PrepararSaberMaisMonitores(void);
void BatalhaB2LigarLuci(BatalhaB2 *b, int *estabilidade, int *ajudasTotal);
void BatalhaB2IniciarMonitor(BatalhaB2 *b, int monitor, int nivelAlucinacao, int hpInicial);
void BatalhaB2IniciarChefe(BatalhaB2 *b, int nivelAlucinacao);
bool BatalhaB2Atualizar(BatalhaB2 *b);
void DesenharTrianguloB2(Vector2 a, Vector2 b, Vector2 c, Color cor);
Vector2 GirarPonto(Vector2 centro, float dx, float dy, float graus);
void DesenharAlucinacaoMestre(Vector2 c, float t);
void DesenharLupaVerificacao(Vector2 c, float e, float t);
void BatalhaB2Desenhar(const BatalhaB2 *b, const char *nickname, float tempo);
void AbrirCartao(CartaoResumo *c, int icone, bool sucesso, const char *titulo, const char *frase, const Dialogo *saberMais);
bool AtualizarCartao(CartaoResumo *c, SessaoDialogo *dlg);
void DesenharCartao(const CartaoResumo *c, float tempo);
void FimBatalhaB2(ContextoB2 *c);

#endif // UTOPIA_BATALHA_MONITORES_H
