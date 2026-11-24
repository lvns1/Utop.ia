// Tipos compartilhados: telas, direção, sprites, diálogos, progresso, save e batalha do Bloco 1.
#ifndef UTOPIA_TIPOS_H
#define UTOPIA_TIPOS_H

#include <stdbool.h>
#include "raylib.h"
#include "config.h"

// Definimos os "canais" (telas) do nosso jogo
// (SETOR2 e SALA_* eram o Bloco 2 antigo: ficam só para os números do save não mudarem)
typedef enum { MENU, INTRO, JOGANDO, OPCOES, CORREDOR, BATALHA, SETOR2, BATALHA_B2,
               SALA_REGISTROS, SALA_LOGICA, SALA_LACUNA,
               BIBLIOTECA } TelaAtual;          // BIBLIOTECA = Bloco 2 (Biblioteca dos Ecos)
typedef enum Direcao {
    DIR_FRENTE, DIR_COSTAS, DIR_DIREITA, DIR_ESQUERDA,
    // Diagonais (FRENTE = descendo na tela, COSTAS = subindo)
    DIR_FRENTE_DIREITA, DIR_FRENTE_ESQUERDA, DIR_COSTAS_DIREITA, DIR_COSTAS_ESQUERDA,
    NUM_DIRECOES
} Direcao;

// Sprite de uma direção do personagem
typedef struct {
    Texture2D textura;
    int quadros;        // 4 = folha de animação / 1 = imagem parada antiga
    bool espelhar;      // só no modo antigo (esquerda = lado espelhado)
    Rectangle recorte[4];                 // onde o personagem está de verdade em cada quadro
    float alturaRef;                      // altura do personagem na imagem (maior dos quadros)
    bool emprestada;                      // true = usa a textura de outra direção (não liberar duas vezes)
} SpriteDirecao;

// Qual quadro da caminhada está aparecendo
typedef struct {
    int quadro;
    float tempo;
} AnimacaoJogador;

// =============================================================
// TIPOS
// =============================================================

// Uma conversa: algumas páginas de texto e, opcionalmente,
// uma pergunta de múltipla escolha (pergunta == NULL = sem pergunta).
typedef struct Dialogo {
    const char *titulo;                     // cabeçalho do painel da pergunta
    const char *paginas[MAX_PAGINAS];
    int numPaginas;
    const char *pergunta;
    const char *opcoes[NUM_OPCOES];
    int correta;                            // índice da opção certa (0 = A ... 3 = D)
    const char *explicacao;                 // Luci explica a resposta certa
    const struct Dialogo *saberMais;        // versão completa (TAB), para quem quiser ler mais
} Dialogo;

typedef struct {
    const char *nome;
    Rectangle area;                         // tela do monitor (o pé é desenhado embaixo)
    Dialogo quiz;
} MonitorQuiz;

typedef enum { MON_NEUTRO, MON_ATIVO, MON_ACERTOU, MON_ERROU } EstadoMonitor;

typedef enum { DLG_NENHUM, DLG_PAGINAS, DLG_ESCOLHA, DLG_FEEDBACK, DLG_CONFIRMA } EstadoDialogo;

// O que aconteceu na conversa neste frame
typedef enum { RES_NADA, RES_FIM, RES_ERROU, RES_TEMPO, RES_SIM, RES_NAO, RES_VOLTAR_CARTAO } ResultadoDialogo;

// Estado da conversa que está acontecendo na tela agora
typedef struct {
    EstadoDialogo estado;
    Dialogo conteudo;
    int objeto;                             // monitor (0-3), OBJ_LUCI ou OBJ_NENHUM
    int pagina;
    bool respondeu;
    bool acertou;
    char texto[768];                        // texto atual, já com quebras de linha
    char resto[768];                        // fala da Luci longa demais: o que vem na próxima "página" (vazio = nada)
    char opcoesTexto[NUM_OPCOES][512];      // opções já com quebras de linha
    int fonteOpcao;                         // diminui sozinha se as opções não couberem
    int selecao;                            // opção marcada com a Luci
    float tempoRestante;                    // barra de tempo do quiz
    float visiveis;                         // quantos bytes do texto já apareceram
    Color corBorda;
    int pontosGanhos;
    double inicio;                          // quando a conversa abriu (telemetria)
    int pulos;                              // quantas vezes o jogador pulou o texto
    const Dialogo *origem;                  // conversa original (para voltar do "saber mais")
    const Dialogo *versaoCurta;             // estamos no "saber mais": TAB volta para esta
    bool veioDoCartao;                      // "saber mais" aberto pelo cartão: TAB volta ao cartão
} SessaoDialogo;

typedef struct {
    EstadoMonitor estado[4];                // espaço para 4 para o save não mudar de formato (o jogo usa TOTAL_MONITORES)
    int pontos;
    int alucinacao;                         // cresce a cada erro (README: errar fortalece a alucinação)
    bool introVista;
    bool fimAnunciado;
    unsigned char hp;                       // vida entre os monitores (0 = cheia). Usa o padding: o formato do save não muda
} ProgressoCorredor;

// Bloco 2 ANTIGO (Arquivo de Treinamento, já removido do jogo). A struct continua
// aqui só para não mudar o formato do save: o campo vai sempre zerado.
#define NUM_OBJETOS_B2    3
#define NUM_REGISTROS     6
#define NUM_FRAGMENTOS    5

typedef struct {
    bool objeto[NUM_OBJETOS_B2];     // objetos conseguidos
    bool introVista;                 // fala da Luci na sala central
    bool fimAnunciado;
    bool introSala[NUM_OBJETOS_B2];  // fala da Luci em cada sala

    int  registro[NUM_REGISTROS];    // 0 = não inspecionado, 1 = marcou limpo, 2 = marcou corrompido
    bool alavanca[3];                // P, Q, R (true = V)
    int  luzRegra[3];                // 0 = não verificada, 1 = verdadeira, 2 = falsa
    bool barreiraAberta;
    bool fragmento[NUM_FRAGMENTOS];  // fragmentos de contexto coletados
    bool perguntaVagaFeita;
} ProgressoBloco2;

// Bloco 2: Biblioteca dos Ecos. O que fica entre as tentativas (nunca se perde ao morrer).
typedef struct {
    int fragmentos;                  // Fragmentos de Conhecimento (a moeda da Biblioteca)
    bool byteVisto;                  // já falou com o Professor Byte
    bool bytecoVisto;                // já falou com o Byteco
    int tentativas;                  // quantas missões já começou
    int melhorias[3];                // melhorias permanentes: HP, dano, EP (Etapa 3)
    unsigned int conceitos;          // conceitos descobertos, um bit cada (enciclopédia, Etapa 3)
    bool alucinadorVencido;          // venceu o chefe da Biblioteca (Etapa 4)

    // Versão 7 (sempre no FINAL: esta struct é o último campo do DadosSave).
    // Desempenho na Biblioteca: o Bloco 3 lê isto para montar a luta e decidir o final.
    int desafiosCertos;              // Guardiões selados, afirmações do Alucinador e puzzles do Silogista
    int desafiosErrados;
    int mortes;                      // derrotas na Biblioteca
    int corrupcaoLuciMax;            // maior chance (%) de a dica da Luci sair errada numa tentativa
    bool silogistaVencido;
    bool bloco3Liberado;             // venceu o Alucinador: a porta do Bloco 3 abriu no átrio

    // Versão 8: como o jogador lidou com a Luci na Biblioteca (o Bloco 3 usa)
    int confiouCego;                 // seguiu a dica dela sem verificar
    int confiouAlucinacao;           // ... e ela estava alucinando
    int verificou;
    int alucinacoesPegas;            // verificou e pegou a alucinação
    int contradicoesFlagradas;
    int contradicoesPerdidas;
    int contestouAToa;
    int reparos;                     // vezes que a estabilidade subiu por uma ação do jogador

    // Versão 9: o Bloco 3 e o final
    bool jogoConcluido;
    int finalJogo;                   // 0 = ainda não, 1 = bom (a Luci salva), 2 = ruim
    int indiceFinal;                 // índice de desempenho (0-100) no fim
    int derrotasLuci;                // derrotas na luta contra a Luci

    // Versão 10: os números da run (os registros de memória do Bloco 3 mostram)
    int abatidosRun;                 // inimigos derrotados em todas as tentativas
    int salasRun;                    // salas da missão atravessadas
    int esquivasRun;
    int golpesRun;                   // golpes levados
    int parriesRun;
    int especiaisRun;                // elites e mini-chefes vencidos
    float segundosRun;               // tempo dentro da Biblioteca (e do núcleo)
} ProgressoBiblioteca;

// O que vai para o arquivo de save. Só o que não dá para reconstruir:
// diálogo aberto, quiz no meio, batalha e animação ficam de fora.
typedef struct {
    unsigned int assinatura;
    int versao;
    char nickname[4];
    int sala;                               // JOGANDO, CORREDOR ou BIBLIOTECA (átrio)
    float x, y;
    int direcao;
    bool luciAcompanhando;
    ProgressoCorredor progresso;
    ProgressoBloco2 bloco2;

    // Versão 4 (sempre no FINAL, para os saves da versão 3 continuarem abrindo)
    int estabilidadeLuci;                   // 0..ESTABILIDADE_MAX_LUCI
    int ajudasPedidas;                      // total de ajudas pedidas à Luci

    // Versão 5
    bool chefeVencido;                      // venceu a ALUCINAÇÃO MESTRE (fim do Bloco 1)
    int idPartida;                          // número desta partida no ranking (0 = sortear um novo)

    // Versão 6
    ProgressoBiblioteca biblioteca;         // Bloco 2: fragmentos, melhorias, conceitos
} DadosSave;


// Fade entre telas: escurece (fase 0), troca de tela, clareia (fase 1)
typedef struct {
    bool ativa;
    float alfa;
    int fase;
    float duracao;
    TelaAtual destino;
} Transicao;

typedef enum { BAT_MINIGAME, BAT_COMBATE } TipoBatalha;
typedef enum { FASE_INTRO, FASE_ENCOLHENDO, FASE_DESVIO, FASE_EXPANDINDO, FASE_FIM } FaseBatalha;

typedef struct {
    bool ativa;
    Vector2 pos;
    Vector2 vel;
    char glifo;
    bool boa;                               // true = verdade para PEGAR (cura), false = alucinação para DESVIAR
    bool falsa;                             // verdade FALSIFICADA (pisca em magenta e machuca)
    int tipo;                               // minigame de separar: 0 = CONTRADIZ, 1 = INVENTA, -1 = bala comum
} Bala;

typedef struct {
    TipoBatalha tipo;
    FaseBatalha fase;
    int quiz;
    int hp;
    int dano;
    Vector2 alma;                           // a Luci (a "alma" que o jogador guia)
    float invencivel;
    float tremor;
    float flashDano;                        // borda vermelha quando a Luci leva um golpe
    float preparo;                          // > 0: a caixa mostra a regra e conta "3, 2, 1" (as balas esperam)
    float anim;                             // 0 = caixa de texto, 1 = caixa de desvio
    float tempoDesvio;
    float duracao;
    float cronoBala;
    float intervaloBala;
    float velocidadeBala;
    int padrao;                             // 0 = chuva, 1 = lateral, 2 = mira
    int danosRecebidos;
    bool derrota;
    Bala balas[MAX_BALAS];
    char texto[512];
    float visiveis;
} Batalha;

#endif // UTOPIA_TIPOS_H
