#include "raylib.h"
#include "raymath.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <time.h>

// =============================================================
// UtopIA - BLOCO 1
//   Sala inicial (Luci) -> porta de cima -> Corredor dos Monitores
//   Cada monitor tem um quiz (conteúdo do "Quizz - missão 1.pdf").
//   Tempo esgotado -> COMBATE estilo Undertale
//   Resposta errada -> MINIGAME de esquiva estilo Undertale
//   Resposta certa  -> ganha pontos
// =============================================================

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

#define LARGURA_TELA 800
#define ALTURA_TELA  600

// =============================================================
// SPRITES
// =============================================================

// Folhas de animação do personagem: 4 quadros lado a lado, TODOS DO MESMO
// TAMANHO e com o personagem no mesmo lugar de cada quadro (versão limpa
// de 152x60, quadros de 38x60). O quadro 0 é o personagem parado (pés juntos).
// Andando para CIMA ele aparece DE COSTAS (Andando_costas.png).
#define SPRITE_ANDANDO_FRENTE    "Andando_de_frente.png"
#define SPRITE_ANDANDO_COSTAS    "Andando_costas.png"
#define SPRITE_ANDANDO_DIREITA   "Andando_direita.png"
#define SPRITE_ANDANDO_ESQUERDA  "Andando_esquerda.png"

// Diagonais (3/4). Se alguma dessas folhas não for encontrada,
// aquela diagonal usa a folha de LADO do mesmo sentido.
#define SPRITE_ANDANDO_FRENTE_DIREITA   "Andando_diagonal_frente_direita.png"
#define SPRITE_ANDANDO_FRENTE_ESQUERDA  "Andando_diagonal_frente_esquerda.png"
#define SPRITE_ANDANDO_COSTAS_DIREITA   "Andando_diagonal_costas_direita.png"
#define SPRITE_ANDANDO_COSTAS_ESQUERDA  "Andando_diagonal_costas_esquerda.png"

// Pastas onde o jogo procura as folhas do personagem, na ordem.
// "" = pasta do repositório (de onde o jogo é executado).
static const char *PASTAS_SPRITES_JOGADOR[] = { "", "resources/sprites/player/", "resources/sprites/save/", "resources/sprites/luci/" };
#define NUM_PASTAS_SPRITES_JOGADOR 4

#define QUADROS_ANDANDO     4
#define FPS_ANIMACAO        8.0f     // trocas de quadro por segundo enquanto anda (AJUSTE AQUI)
#define ALTURA_PERSONAGEM   58.0f    // altura do PERSONAGEM (não da imagem) na tela, em pixels (AJUSTE AQUI)
                                     // com as folhas limpas, 58 ou 116 deixam a pixel art nítida

// Imagens antigas (1 por direção), usadas só se as folhas acima não existirem
#define SPRITE_FRENTE          "resources/sprites/player/person.frente.png"
#define SPRITE_COSTAS          "resources/sprites/player/person.cima.png"
#define SPRITE_LADO            "resources/sprites/player/person.lado.png"

// Sprites dos monitores (opcionais). Enquanto os arquivos não existirem,
// o jogo desenha os monitores com formas simples.
#define SPRITE_MONITOR_NEUTRO  "resources/sprites/corredor/monitor.neutro.png"
#define SPRITE_MONITOR_ATIVO   "resources/sprites/corredor/monitor.ativo.png"
#define SPRITE_MONITOR_ACERTO  "resources/sprites/corredor/monitor.acerto.png"
#define SPRITE_MONITOR_ERRO    "resources/sprites/corredor/monitor.erro.png"

#define TAM_SPRITE          65.0f    // base das hitboxes (e tamanho das imagens antigas)
#define VELOCIDADE_JOGADOR  300.0f   // pixels por segundo (= 5 px por frame a 60 FPS)

// =============================================================
// CORREDOR - CONSTANTES
// =============================================================

#define TOTAL_MONITORES     3        // Bloco 1: o que é, tipos, riscos e mitigação
#define PONTOS_POR_ACERTO   10
#define PENALIDADE_ERRO     5        // resposta errada ou tempo esgotado (corredor e Bloco 2)
#define PENALIDADE_HP_ZERO  15       // a vida chegou a zero numa batalha
// A pontuação nunca fica negativa
#define TEMPO_QUIZ          60.0f    // segundos da barra de tempo de cada quiz (AJUSTE AQUI)
#define ATRASO_RESPOSTA     0.4f     // evita responder sem querer apertando ENTER rápido demais
#define QUIZ_EM_ORDEM       1        // 1 = monitores na ordem 1-2-3-4 / 0 = qualquer ordem
#define VELOCIDADE_TEXTO    50.0f    // caracteres por segundo (máquina de escrever)
#define DURACAO_FADE        0.35f    // fade entre salas
#define DURACAO_FADE_BATALHA 0.2f    // fade rápido para entrar/sair da batalha

#define MAX_PAGINAS         8
#define NUM_OPCOES          4

#define FONTE_CAIXA         18
#define LARGURA_TEXTO_CAIXA 640
#define FONTE_PERGUNTA      18
#define LARGURA_PERGUNTA    680
#define LARGURA_OPCAO       650

// Conversas que não são de monitor
#define OBJ_NENHUM  -1
#define OBJ_LUCI    -2
#define OBJ_CHEFE   -3                  // fala antes da ALUCINAÇÃO MESTRE

// =============================================================
// SAVE - CONSTANTES
// =============================================================

#define ARQUIVO_SAVE         "utopia.sav"
#define ARQUIVO_SAVE_TEMP    "utopia.sav.tmp"
#define VERSAO_SAVE          6            // aumente quando mudar o que vai no save (6 = Biblioteca dos Ecos)
#define ASSINATURA_SAVE      0x41495455u  // identifica o arquivo como save do UtopIA
#define TEMPO_AVISO_SAVE     2.0f         // segundos do aviso "JOGO SALVO" na tela

// Ajuda da Luci: cada pedido gasta 1 ponto de estabilidade.
// Estabilidade baixa = ela pode ALUCINAR a dica (errada, com toda a certeza).
// Acertar sem ajuda devolve 1 ponto.
#define ESTABILIDADE_MAX_LUCI  5

// =============================================================
// BATALHA - CONSTANTES
// =============================================================

#define MAX_BALAS            96
#define HP_MAX_BATALHA       20
#define VELOCIDADE_ALMA      170.0f
#define TEMPO_INVENCIVEL     1.0f
#define RAIO_BALA            7.0f
#define RAIO_ALMA            6.0f
#define DURACAO_MINIGAME     7.0f    // segundos de esquiva quando ERRA a resposta
#define DURACAO_COMBATE      11.0f   // segundos de esquiva quando o TEMPO ACABA
#define DURACAO_ANIM_CAIXA   0.35f
#define FONTE_BATALHA        20

// =============================================================
// POSIÇÕES DOS MAPAS
// =============================================================

// Sala inicial: arte do mapa (imagem quadrada 1024x1024)
// Se o arquivo não existir, o jogo usa o cenário simples antigo.
#define ARQUIVO_MAPA_SALA "resources/maps/mapa1_introducao.png"

// ---- TAMANHO E POSIÇÃO DO MAPA NA TELA (AJUSTE AQUI) ----
// Tudo que é "medido na imagem" (paredes, porta, ponto de save, Luci, spawn)
// acompanha estes valores sozinho: aumentou o mapa, tudo vai junto.
#define MAPA_SALA_X          100.0f
#define MAPA_SALA_Y            0.0f
#define MAPA_SALA_LARGURA    600.0f
#define MAPA_SALA_ALTURA     600.0f
#define TAM_IMAGEM_MAPA_SALA 1024.0f   // lado da imagem em pixels

// Converte uma medida da imagem (pixels, de 0 a 1024) para a tela
#define MAPA_X(px)  (MAPA_SALA_X + (px) * MAPA_SALA_LARGURA / TAM_IMAGEM_MAPA_SALA)
#define MAPA_Y(py)  (MAPA_SALA_Y + (py) * MAPA_SALA_ALTURA / TAM_IMAGEM_MAPA_SALA)
#define MAPA_L(pl)  ((pl) * MAPA_SALA_LARGURA / TAM_IMAGEM_MAPA_SALA)
#define MAPA_A(pa)  ((pa) * MAPA_SALA_ALTURA / TAM_IMAGEM_MAPA_SALA)

// Retângulo medido na imagem: x, y, largura, altura
#define RETANGULO_MAPA(x, y, l, a)  { MAPA_X(x), MAPA_Y(y), MAPA_L(l), MAPA_A(a) }

static const Rectangle AREA_MAPA_SALA = { MAPA_SALA_X, MAPA_SALA_Y, MAPA_SALA_LARGURA, MAPA_SALA_ALTURA };

// Limites do centro do personagem no cenário simples (sem a imagem do mapa)
static const Rectangle LIMITES_SALA_SIMPLES = {80, 80, 640, 440};

// ---- PAREDES INVISÍVEIS DA SALA INICIAL ----
#define PAREDES_INVISIVEIS_ATIVAS   1   // 1 = bloqueiam o personagem | 0 = desligadas
#define MOSTRAR_PAREDES_INVISIVEIS  0   // 1 = desenha as paredes (para calibrar) | 0 = escondidas

// Cada linha é uma parede: { x, y, largura, altura } medidos na imagem.
// Para criar outra, é só acrescentar uma linha aqui e aumentar NUM_PAREDES_SALA.
static const Rectangle PAREDES_SALA[] = {
    RETANGULO_MAPA(   0,   0, 1024, 270),   // parede de cima (com o arco da porta)
    RETANGULO_MAPA(   0,   0,   55, 1024),  // parede da esquerda
    RETANGULO_MAPA( 985,   0,   39, 1024),  // parede da direita
    RETANGULO_MAPA(   0, 975, 1024,   49),  // parede de baixo
};
#define NUM_PAREDES_SALA 4

// Sala inicial: porta na parede de CIMA (o arco da imagem)
static const Rectangle PORTA_SALA      = RETANGULO_MAPA(464, 101, 116, 159);
static const Rectangle ZONA_PORTA_SALA = RETANGULO_MAPA(447, 154, 150, 145);

// Corredor: porta de baixo volta para a sala / porta da direita é a saída
static const Rectangle PORTA_VOLTAR_CORREDOR = {100, 406, 90, 18};
static const Rectangle PORTA_FIM_CORREDOR    = {722, 265, 28, 140};

static const Vector2 SPAWN_SALA       = { MAPA_X(512), MAPA_Y(512) };
static const Vector2 SPAWN_SALA_VOLTA = { MAPA_X(522), MAPA_Y(300) };   // logo abaixo da porta de cima
static const Vector2 SPAWN_CORREDOR   = {145, 368};   // logo acima da porta de baixo

// Caixa da batalha: larga para texto, estreita para desviar (como no Undertale)
static const Rectangle CAIXA_TEXTO_BATALHA = {50, 270, 700, 160};
static const Rectangle CAIXA_DESVIO        = {275, 270, 250, 160};

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
    char opcoesTexto[NUM_OPCOES][512];      // opções já com quebras de linha
    int fonteOpcao;                         // diminui sozinha se as opções não couberem
    int selecao;                            // opção marcada com o coração
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
    Vector2 alma;                           // o coração do jogador
    float invencivel;
    float tremor;
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

// =============================================================
// CONTEÚDO - SALA INICIAL (falas da Luci do main.c)
// =============================================================

// Versão completa da história (TAB na conversa curta)
static const Dialogo dialogoLuciSalaCompleto = {
    .paginas = {
        "Luci: Olá, Thomas. Eu imagino que se pergunte o que está fazendo aqui. Eu sou Luci, um dos algoritmos que fazem parte da IA que você usa todos os dias.",
        "Luci: No início, o universo de UtopIA funcionava perfeitamente, mas com o tempo, a IA começou a alucinar e está destruindo a si mesma.",
        "Luci: Eu trouxe você para cá porque preciso de ajuda e você é o usuário que mais nos utiliza.",
        "Luci: Para voltar para casa, você vai precisar enfrentar 3 missões. A primeira é enfrentar as alucinações guardadas nos monitores do corredor.",
        "Luci: Na segunda missão, você vai explorar salas e resolver puzzles para conquistar os objetos mágicos.",
        "Luci: No final, você irá enfrentar a IA alucinada com seus objetos.",
        "Luci: A porta lá em cima leva ao Corredor do Histórico. Eu vou com você."
    },
    .numPaginas = 7
};

// Conversa curta: o essencial em 3 falas
static const Dialogo dialogoLuciSala = {
    .paginas = {
        "Luci: Olá, Thomas! Eu sou a Luci, uma parte da IA que você usa todo dia.",
        "Luci: A IA começou a alucinar e UtopIA está desmoronando. Preciso de você!",
        "Luci: Vamos pela porta lá em cima. Eu vou junto!"
    },
    .numPaginas = 3,
    .saberMais = &dialogoLuciSalaCompleto
};

static const Dialogo dialogoPortaSalaTrancada = {
    .paginas = { "* A porta está trancada. Parece que a Luci quer falar com você antes." },
    .numPaginas = 1
};

// =============================================================
// CONTEÚDO - CORREDOR (baseado no "Quizz - missão 1.pdf")
// Para editar perguntas/respostas, mexa só aqui.
// =============================================================

static const MonitorQuiz monitores[TOTAL_MONITORES] = {

    // ---------- MONITOR 1: o que é alucinação (Sala 1 / Questão 1) ----------
    {
        .nome = "MONITOR 1",
        .area = {150, 140, 100, 70},
        .quiz = {
            .titulo = "MONITOR 1 // O QUE É ALUCINAÇÃO",
            .paginas = {
                "Luci: Aqui, eu vou te explicar o que é a alucinação. Ela acontece quando um modelo de IA gera uma resposta que parece confiante e coerente, mas que é incorreta, contradiz o contexto fornecido ou é inventada.",
                "Luci: Isso acontece porque a IA não busca a \"verdade\", mas padrões estatísticos. No pré-treinamento, o modelo é alimentado com uma quantidade imensa de dados e aprende prevendo a próxima palavra de uma sequência.",
                "Luci: Se a informação que você busca é rara ou ambígua nos dados de treinamento, o modelo pode \"adivinhar\" a resposta mais provável.",
                "Luci: No pós-treinamento, mesmo depois dos ajustes para diminuir os erros, as alucinações persistem. Os sistemas de avaliação de IA muitas vezes recompensam respostas e penalizam a incerteza.",
                "Luci: É como em uma prova de múltipla escolha, onde \"chutar\" uma resposta, mesmo sem saber, pode te dar um ponto, mas deixar em branco não."
            },
            .numPaginas = 5,
            .pergunta = "Qual alternativa está correta sobre as alucinações de IA e o treinamento do modelo?",
            .opcoes = {
                "A alucinacao e uma resposta convincente, mas incorreta. "
                "No pos-treinamento, esses erros sao eliminados ao recompensar a incerteza.",

                "A alucinacao e uma resposta incoerente, facil de identificar. "
                "No pos-treinamento, todas as falhas sao eliminadas.",

                "A alucinacao e a recusa em responder por incerteza. "
                "No pre-treinamento, o modelo aprende a prever a proxima palavra.",

                "A alucinacao e uma resposta convincente, mas incorreta ou inventada. "
                "No pre-treinamento, o modelo aprende a prever a proxima palavra "
                "e pode gerar respostas provaveis, mas falsas."
            },
            .correta = 3,
            .explicacao = "A alucinação parece confiante, mas é errada ou inventada. E os ajustes do pós-treinamento não eliminam o problema, porque a incerteza ainda costuma ser penalizada."
        }
    },

    // ---------- MONITOR 2: tipos de alucinação (Sala 2 / Questão 3) ----------
    {
        .nome = "MONITOR 2",
        .area = {350, 140, 100, 70},
        .quiz = {
            .titulo = "MONITOR 2 // TIPOS DE ALUCINAÇÃO",
            .paginas = {
                "Luci: Agora que você já sabe o que é alucinação e como funciona, quero que você entenda os tipos e, principalmente, os riscos e como mitigar esse problema.",
                "Luci: As alucinações intrínsecas ocorrem quando a resposta contradiz a informação fornecida pelo usuário ou o contexto dado. É uma falha de \"atenção\" ao que foi pedido.",
                "Luci: As extrínsecas acontecem quando a resposta não pode ser verificada ou é completamente inventada, mesmo que não contradiga o contexto. É uma \"criatividade\" indesejada."
            },
            .numPaginas = 3,
            .pergunta = "Qual alternativa apresenta corretamente os dois tipos de alucinação?",
            .opcoes = {
                "Intrínsecas: a resposta contradiz o contexto. Extrínsecas: a resposta é inventada ou não verificável, sem contradizer o contexto.",

                "Intrínsecas: a resposta contradiz o contexto. Extrínsecas: a resposta é inventada ou não verificável, mesmo sem contradição.",

                "Intrínsecas: a resposta contradiz o contexto. Extrínsecas: a resposta é verdadeira, mas apresentada de forma inesperada.",

                "Intrínsecas: a IA aprende dados errados ou desatualizados. Extrínsecas: a IA associa informações sem relação real."
            },
            .correta = 1,
            .explicacao = "A intrínseca contradiz o que o usuário ou o contexto informou. A extrínseca não pode ser verificada ou é inventada, mesmo sem contradizer o contexto."
        }
    },

    // ---------- MONITOR 3: riscos e mitigação (Sala 2 / Questão 4) ----------
    {
        .nome = "MONITOR 3",
        .area = {550, 140, 100, 70},
        .quiz = {
            .titulo = "MONITOR 3 // RISCOS E MITIGAÇÃO",
            .paginas = {
                "Luci: Há alguns riscos com a alucinação. Ao receber dados ruins, a IA aprende informações erradas ou desatualizadas, repetindo esses erros.",
                "Luci: Com limitações no modelo, a IA enxerga apenas coincidências, não relações de causa e efeito, e junta coisas que aparecem juntas nos dados, mesmo sem ligação real.",
                "Luci: Se o usuário dá instruções confusas, a IA inventará informações para preencher as lacunas e manter a coerência.",
                "Luci: Mesmo assim, dá para mitigar. Uma das formas mais importantes é a verificação humana: o usuário aprova, rejeita ou pede revisão de cada resposta, podendo validar com fontes externas.",
                "Luci: Também ajuda fazer perguntas diretas e dar um contexto mais claro. De forma mais técnica, há métodos como o RAG, que conectam o modelo a uma base de dados confiável em tempo real."
            },
            .numPaginas = 5,
            .pergunta = "Qual alternativa descreve corretamente um risco da alucinação e uma forma adequada de mitigá-lo?",
            .opcoes = {
                "A IA associa fatos sem relação e presume causalidade. Para evitar isso, aceite respostas confiantes e coerentes.",

                "A IA inventa informações diante de pedidos vagos. Para evitar isso, use RAG, sem precisar reformular o pedido.",

                "A IA repete informações desatualizadas. Para evitar isso, faça pedidos mais curtos e com menos contexto.",

                "A IA inventa informações diante de pedidos confusos. Para evitar isso, reformule a pergunta e forneça contexto claro."
            },
            .correta = 3,
            .explicacao = "Pedidos confusos fazem a IA preencher lacunas inventando. Reformular de forma direta e com contexto claro ataca a causa do problema."
        }
    }
};

// Falas avulsas da Luci (sem pergunta)
static const Dialogo dialogoIntroCorredorCompleto = {
    .paginas = {
        "Luci: Este é o Corredor do Histórico. Cada um destes 3 monitores guarda uma alucinação sobre um tema diferente.",
        "Luci: Chegue perto de um monitor e aperte E. A alucinação vai dizer algo, e você escolhe a ação certa para enfraquecê-la.",
        "Luci: Se errar, ela ataca. Pegue os sinais verdes de verdade, que recuperam a sua vida, e desvie de todo o resto.",
        "Luci: Se travar numa escolha, aperte H e eu te dou uma dica. Cada ajuda me deixa mais instável, e uma IA instável pode alucinar: aí eu posso jurar que uma resposta errada é a certa.",
        "Luci: Acertar sozinho me estabiliza de novo. Use a ajuda, mas sempre confira o que eu disser!",
        "Luci: Vença as 3 alucinações e enfrente a ALUCINAÇÃO MESTRE. Aí a porta da direita se destranca."
    },
    .numPaginas = 6
};

static const Dialogo dialogoIntroCorredor = {
    .paginas = {
        "Luci: Cada monitor guarda uma alucinação. Chegue perto e aperte E!",
        "Luci: Escolha a ação certa. No ataque, PEGUE o verde e DESVIE do resto!",
        "Luci: Travou? Aperte H. Mas se eu ajudar demais... eu também alucino!"
    },
    .numPaginas = 3,
    .saberMais = &dialogoIntroCorredorCompleto
};

static const Dialogo dialogoFimCorredor = {
    .paginas = { "Luci: Os 3 monitores estão limpos! A porta da direita abriu." },
    .numPaginas = 1
};

static const Dialogo dialogoPortaTrancada = {
    .paginas = { "Luci: Essa porta só abre depois que você resolver os 3 monitores." },
    .numPaginas = 1
};

// Mini-chefe do fim do Bloco 1 (a conversa termina com OBJ_CHEFE e a batalha começa)
static const Dialogo dialogoChefe = {
    .paginas = {
        "Luci: Os 3 monitores se apagaram... espera! As alucinações estão se juntando!",
        "Luci: É a ALUCINAÇÃO MESTRE! Ela mistura fatos com invenções. Me peça FONTES (H) antes de contestar!",
        "Luci: Mas cuidado: se eu ficar instável, posso trazer uma fonte falsa. Confira o que eu disser!"
    },
    .numPaginas = 3
};

static const Dialogo dialogoChefeVolta = {
    .paginas = { "Luci: A ALUCINAÇÃO MESTRE ainda bloqueia a saída. Vamos de novo!" },
    .numPaginas = 1
};

// =============================================================
// TELEMETRIA DOS TESTES DE USABILIDADE
//   Grava em "utopia_teste.csv" quanto tempo cada conversa ficou aberta,
//   quantas vezes o texto foi pulado, o resultado das batalhas e quando a
//   Luci precisou guiar o jogador. Abra no Excel/Planilhas para comparar.
//   Desligue com TELEMETRIA_ATIVA 0.
// =============================================================

#define TELEMETRIA_ATIVA     1
#define ARQUIVO_TELEMETRIA   "utopia_teste.csv"

static void Telemetria(const char *evento, const char *detalhe, float valor, int quantidade)
{
#if TELEMETRIA_ATIVA
    FILE *f = fopen(ARQUIVO_TELEMETRIA, "a");

    if (f == NULL)
        return;

    // Arquivo novo: escreve o cabeçalho
    fseek(f, 0, SEEK_END);
    if (ftell(f) == 0)
        fprintf(f, "tempo_jogo_s;evento;detalhe;valor;quantidade\n");

    // Tira ";" e quebras de linha do detalhe para não bagunçar as colunas
    char limpo[64];
    int n = 0;

    for (const char *c = detalhe; *c != '\0' && n < 60; c++)
        limpo[n++] = (*c == ';' || *c == '\n' || *c == '"') ? ' ' : *c;

    limpo[n] = '\0';

    fprintf(f, "%.1f;%s;%s;%.1f;%d\n", GetTime(), evento, limpo, valor, quantidade);
    fclose(f);
#else
    (void)evento; (void)detalhe; (void)valor; (void)quantidade;
#endif
}

// =============================================================
// SONS (gerados por código, sem precisar de arquivos de áudio)
// =============================================================

static Sound sfxAcerto, sfxErro, sfxDano, sfxPegar, sfxSelecao, sfxTiro;
static bool audioPronto = false;

// Gera um bipe que desliza de uma frequência para outra (forma 0 = suave, 1 = quadrada)
static Sound GerarSom(float freqInicio, float freqFim, float duracao, float volume, int forma)
{
    const int taxa = 22050;
    unsigned int quadros = (unsigned int)(duracao * taxa);
    short *amostras = (short *)malloc(quadros * sizeof(short));
    float fase = 0.0f;

    for (unsigned int i = 0; i < quadros; i++) {

        float t = (float)i / (float)quadros;
        float freq = freqInicio + (freqFim - freqInicio) * t;
        float envelope = (t < 0.1f) ? t / 0.1f : (1.0f - t);   // ataque rápido, some aos poucos

        fase += 2.0f * PI * freq / taxa;

        float onda = (forma == 0) ? sinf(fase) : (sinf(fase) >= 0.0f ? 0.6f : -0.6f);
        amostras[i] = (short)(onda * envelope * volume * 32000.0f);
    }

    Wave w = { quadros, taxa, 16, 1, amostras };
    Sound som = LoadSoundFromWave(w);

    free(amostras);
    return som;
}

static void IniciarSons(void)
{
    InitAudioDevice();
    audioPronto = IsAudioDeviceReady();

    if (!audioPronto)
        return;

    sfxAcerto  = GerarSom(520.0f, 1040.0f, 0.18f, 0.35f, 0);
    sfxErro    = GerarSom(220.0f, 110.0f, 0.30f, 0.30f, 1);
    sfxDano    = GerarSom(160.0f, 90.0f, 0.12f, 0.35f, 1);
    sfxPegar   = GerarSom(880.0f, 1320.0f, 0.10f, 0.25f, 0);
    sfxSelecao = GerarSom(660.0f, 660.0f, 0.04f, 0.15f, 1);
    sfxTiro    = GerarSom(1200.0f, 500.0f, 0.06f, 0.10f, 1);   // Disparador de Dados (Bloco 2)
}

// tom: 1 = normal, 2 = uma oitava acima (os combos usam para o acerto ficar mais agudo)
static void TocarSomTom(Sound s, float tom)
{
    if (!audioPronto)
        return;

    SetSoundPitch(s, tom);
    PlaySound(s);
}

static void TocarSom(Sound s)
{
    TocarSomTom(s, 1.0f);
}

static void EncerrarSons(void)
{
    if (!audioPronto)
        return;

    UnloadSound(sfxAcerto);
    UnloadSound(sfxErro);
    UnloadSound(sfxDano);
    UnloadSound(sfxPegar);
    UnloadSound(sfxSelecao);
    UnloadSound(sfxTiro);
    CloseAudioDevice();
}

// =============================================================
// FUNÇÕES AUXILIARES - TEXTO
// =============================================================

// Copia "texto" para "saida" inserindo '\n' nos espaços para que
// nenhuma linha passe de "larguraMax" pixels (DrawText não quebra linha sozinho).
static void QuebrarTexto(const char *texto, int larguraMax, int tamFonte, char *saida, int tamSaida)
{
    int pos = 0;
    int inicioLinha = 0;
    int ultimoEspaco = -1;

    for (int i = 0; texto[i] != '\0' && pos < tamSaida - 1; i++) {

        char c = texto[i];
        saida[pos++] = c;
        saida[pos] = '\0';

        if (c == '\n') {
            inicioLinha = pos;
            ultimoEspaco = -1;
            continue;
        }

        if (c == ' ')
            ultimoEspaco = pos - 1;

        if (ultimoEspaco >= inicioLinha &&
            MeasureText(saida + inicioLinha, tamFonte) > larguraMax) {

            saida[ultimoEspaco] = '\n';
            inicioLinha = ultimoEspaco + 1;
            ultimoEspaco = -1;
        }
    }

    saida[pos] = '\0';
}

// Desenha um texto com várias linhas (separadas por '\n') e devolve quantas linhas desenhou.
static int DesenharTextoLinhas(const char *texto, int x, int y, int tamFonte, int espacoLinha, Color cor)
{
    char linha[512];
    int n = 0;
    int linhas = 0;

    for (const char *p = texto; ; p++) {

        if (*p == '\n' || *p == '\0') {

            linha[n] = '\0';
            DrawText(linha, x, y + linhas * espacoLinha, tamFonte, cor);
            linhas++;
            n = 0;

            if (*p == '\0')
                break;

        } else if (n < 511) {

            linha[n++] = *p;
        }
    }

    return linhas;
}

static int ContarLinhas(const char *texto)
{
    int n = 1;

    for (; *texto != '\0'; texto++)
        if (*texto == '\n')
            n++;

    return n;
}

// Letras com acento ocupam 2 bytes em UTF-8. Na máquina de escrever,
// isso garante que nunca aparece "meia letra" na tela.
static int FimDoCaractere(const char *s, int n)
{
    while (s[n] != '\0' && (((unsigned char)s[n]) & 0xC0) == 0x80)
        n++;

    return n;
}

// Copia a parte do texto que já "apareceu" na máquina de escrever
static bool TextoParcial(const char *texto, float visiveis, char *saida, int tamSaida)
{
    int total = (int)strlen(texto);
    int n = FimDoCaractere(texto, (int)visiveis);

    if (n > total) n = total;
    if (n > tamSaida - 1) n = tamSaida - 1;

    memcpy(saida, texto, (size_t)n);
    saida[n] = '\0';

    return (n >= total);
}

// Gerador de "ruído" para chiados e glitches (sempre igual para o mesmo número)
static unsigned int Ruido(unsigned int n)
{
    n = (n << 13) ^ n;
    return n * (n * n * 15731u + 789221u) + 1376312589u;
}

// Coração em pixel art (a "alma" do Undertale)
static void DesenharCoracao(float cx, float cy, int pixel, Color cor)
{
    static const char *forma[6] = {
        ".XX.XX.",
        "XXXXXXX",
        "XXXXXXX",
        ".XXXXX.",
        "..XXX..",
        "...X..."
    };

    float x0 = cx - 3.5f * pixel;
    float y0 = cy - 3.0f * pixel;

    for (int l = 0; l < 6; l++)
        for (int c = 0; c < 7; c++)
            if (forma[l][c] == 'X')
                DrawRectangle((int)(x0 + c * pixel), (int)(y0 + l * pixel), pixel, pixel, cor);
}

// =============================================================
// FUNÇÕES AUXILIARES - JOGADOR
// =============================================================

// Lê W A S D, move o jogador (sem ficar mais rápido na diagonal)
// e devolve para qual lado o sprite deve olhar.
static Direcao MoverJogador(Vector2 *pos, Direcao direcaoAtual)
{
    Vector2 direcao = {0};

    if (IsKeyDown(KEY_D)) direcao.x += 1.0f;
    if (IsKeyDown(KEY_A)) direcao.x -= 1.0f;
    if (IsKeyDown(KEY_S)) direcao.y += 1.0f;
    if (IsKeyDown(KEY_W)) direcao.y -= 1.0f;   // para cima = de costas

    // Parado (ou apertando lados opostos): continua olhando para onde estava
    if (Vector2Length(direcao) == 0.0f)
        return direcaoAtual;

    Direcao nova;

    if      (direcao.y > 0 && direcao.x > 0) nova = DIR_FRENTE_DIREITA;
    else if (direcao.y > 0 && direcao.x < 0) nova = DIR_FRENTE_ESQUERDA;
    else if (direcao.y < 0 && direcao.x > 0) nova = DIR_COSTAS_DIREITA;
    else if (direcao.y < 0 && direcao.x < 0) nova = DIR_COSTAS_ESQUERDA;
    else if (direcao.y > 0)                  nova = DIR_FRENTE;
    else if (direcao.y < 0)                  nova = DIR_COSTAS;
    else if (direcao.x > 0)                  nova = DIR_DIREITA;
    else                                     nova = DIR_ESQUERDA;

    // Normaliza para a diagonal não ficar mais rápida
    direcao = Vector2Normalize(direcao);
    *pos = Vector2Add(*pos, Vector2Scale(direcao, VELOCIDADE_JOGADOR * GetFrameTime()));

    return nova;
}

// Hitbox menor, usada para colidir com a Luci
static Rectangle HitboxJogador(Vector2 p)
{
    return (Rectangle){ p.x - TAM_SPRITE / 4, p.y - TAM_SPRITE / 4, TAM_SPRITE / 2, TAM_SPRITE / 2 };
}

// Área usada para portas e monitores
static Rectangle AreaJogador(Vector2 p)
{
    return (Rectangle){ p.x - 18, p.y - 22, 36, 44 };
}

// Pés do personagem: é o que bate nas paredes invisíveis
// (a cabeça pode passar "por trás" da parede, como em jogos vistos de cima)
static Rectangle PesJogador(Vector2 p)
{
    return (Rectangle){ p.x - 14, p.y + ALTURA_PERSONAGEM / 2 - 8, 28, 8 };
}

// Algum retângulo encosta em alguma parede invisível da sala?
static bool BateNasParedes(Rectangle r)
{
    for (int i = 0; i < NUM_PAREDES_SALA; i++)
        if (CheckCollisionRecs(r, PAREDES_SALA[i]))
            return true;

    return false;
}

// Impede o jogador de atravessar as paredes invisíveis.
// Testa X e Y separados, assim ele desliza pela parede em vez de travar.
static void ColidirComParedes(Vector2 *pos, Vector2 posAnterior)
{
    // Já estava dentro de uma parede (ex.: save antigo): deixa sair
    if (BateNasParedes(PesJogador(posAnterior)))
        return;

    Vector2 teste = { pos->x, posAnterior.y };

    if (BateNasParedes(PesJogador(teste)))
        teste.x = posAnterior.x;

    teste.y = pos->y;

    if (BateNasParedes(PesJogador(teste)))
        teste.y = posAnterior.y;

    *pos = teste;
}


// Procura o arquivo nas pastas de PASTAS_SPRITES_JOGADOR.
// Devolve o caminho encontrado, ou NULL se não estiver em nenhuma.
static const char *AcharSpriteJogador(const char *nome)
{
    static char caminho[512];

    for (int i = 0; i < NUM_PASTAS_SPRITES_JOGADOR; i++) {

        snprintf(caminho, sizeof(caminho), "%s%s", PASTAS_SPRITES_JOGADOR[i], nome);

        if (FileExists(caminho))
            return caminho;
    }

    TraceLog(LOG_WARNING, "SPRITE: '%s' nao encontrado na pasta do repositorio nem em resources/sprites/", nome);
    return NULL;
}

// Carrega a folha de animação; se ela não existir, usa a imagem antiga
static SpriteDirecao CarregarSprite(const char *folha, const char *antigo, bool espelharAntigo)
{
    SpriteDirecao s = {0};

    folha = AcharSpriteJogador(folha);

    if (folha != NULL) {

        Image img = LoadImage(folha);
        Color *px = LoadImageColors(img);

        s.textura = LoadTextureFromImage(img);
        s.quadros = QUADROS_ANDANDO;

        // Acha o personagem dentro de cada quadro, ignorando o espaço vazio em volta.
        // Assim o tamanho fica certo em qualquer resolução e o personagem não
        // "treme" quando os quadros não estão igualmente espaçados na folha.
        int larguraQuadro = img.width / QUADROS_ANDANDO;

        for (int q = 0; q < QUADROS_ANDANDO; q++) {

            int x0 = q * larguraQuadro;
            int minX = img.width, maxX = -1, minY = img.height, maxY = -1;

            for (int y = 0; y < img.height; y++) {
                for (int x = x0; x < x0 + larguraQuadro; x++) {

                    if (px[y * img.width + x].a > 128) {
                        if (x < minX) minX = x;
                        if (x > maxX) maxX = x;
                        if (y < minY) minY = y;
                        if (y > maxY) maxY = y;
                    }
                }
            }

            if (maxX < 0)   // quadro vazio: usa o quadro inteiro
                s.recorte[q] = (Rectangle){ (float)x0, 0, (float)larguraQuadro, (float)img.height };
            else
                s.recorte[q] = (Rectangle){ (float)minX, (float)minY, (float)(maxX - minX + 1), (float)(maxY - minY + 1) };

            if (s.recorte[q].height > s.alturaRef)
                s.alturaRef = s.recorte[q].height;
        }

        UnloadImageColors(px);
        UnloadImage(img);

        // Imagem muito maior que o tamanho na tela (ex.: a arte original de 909x369):
        // suaviza a redução para não "picotar" os pixels
        if (s.alturaRef > ALTURA_PERSONAGEM * 2) {
            GenTextureMipmaps(&s.textura);
            SetTextureFilter(s.textura, TEXTURE_FILTER_TRILINEAR);
        }

    } else {

        s.textura = LoadTexture(antigo);
        s.quadros = 1;
        s.espelhar = espelharAntigo;
    }

    return s;
}

// Diagonal: usa a folha própria se ela existir; senão, empresta a folha de lado
static SpriteDirecao CarregarSpriteDiagonal(const char *folha, SpriteDirecao reserva)
{
    if (AcharSpriteJogador(folha) != NULL)
        return CarregarSprite(folha, folha, false);

    reserva.emprestada = true;
    return reserva;
}

// Andando: avança os quadros. Parado: volta para o quadro 0 (pés juntos).
static void AtualizarAnimacao(AnimacaoJogador *a, bool andando, float dt)
{
    if (!andando) {

        a->quadro = 0;
        a->tempo = 0.0f;
        return;
    }

    a->tempo += dt;

    if (a->tempo >= 1.0f / FPS_ANIMACAO) {

        a->tempo -= 1.0f / FPS_ANIMACAO;
        a->quadro = (a->quadro + 1) % QUADROS_ANDANDO;
    }
}

// sprites[] é indexado pela direção (mesma ordem do enum Direcao)
static void DesenharJogador(const SpriteDirecao sprites[NUM_DIRECOES], Vector2 pos, Direcao direcao, int quadro)
{
    const SpriteDirecao *s = &sprites[direcao];
    Texture2D t = s->textura;

    if (s->quadros > 1) {

        // Folha de animação: recorta só o personagem do quadro atual
        Rectangle sourceRec = s->recorte[quadro % s->quadros];

        // A escala vem da altura do PERSONAGEM na imagem: qualquer resolução
        // aparece com ALTURA_PERSONAGEM pixels de altura na tela.
        float escala = ALTURA_PERSONAGEM / s->alturaRef;
        float w = roundf(sourceRec.width * escala);
        float h = roundf(sourceRec.height * escala);

        // Centralizado na horizontal e com o topo da cabeça sempre na mesma
        // altura (os pés mudam a cada passo, a cabeça não pula).
        // Posição arredondada para os pixels ficarem nítidos.
        Rectangle destRec = {
            roundf(pos.x - w / 2),
            roundf(pos.y - ALTURA_PERSONAGEM / 2),
            w,
            h
        };

        DrawTexturePro(t, sourceRec, destRec, (Vector2){ 0, 0 }, 0.0f, WHITE);

    } else {

        // Imagem antiga parada (65x65)
        float flip = s->espelhar ? -1.0f : 1.0f;

        Rectangle sourceRec = { 0.0f, 0.0f, (float)t.width * flip, (float)t.height };
        Rectangle destRec = { pos.x, pos.y, TAM_SPRITE, TAM_SPRITE };
        Vector2 origin = { TAM_SPRITE / 2, TAM_SPRITE / 2 };

        DrawTexturePro(t, sourceRec, destRec, origin, 0.0f, WHITE);
    }
}

// ---------- Luci ----------
// A arte fica em "luci.png" (pasta do repositório ou resources/sprites/luci/).
// O mesmo desenho é usado parada na sala, acompanhando o jogador e na intro.

#define SPRITE_LUCI   "luci.png"
#define ALTURA_LUCI        18.0f     // Luci acompanhando o jogador: pequenina (AJUSTE AQUI)
#define ALTURA_LUCI_SALA   24.0f     // Luci parada na sala inicial (um pouco maior, para ser encontrada)
#define RAIO_ORBITA_LUCI   26.0f     // distância da volta que ela dá em torno do personagem

static Texture2D texLuci = {0};
static Rectangle recorteLuci = {0};  // só a parte desenhada da imagem, sem o espaço vazio em volta

// Emoções da Luci sobre a cabeça. O main atualiza estes valores a cada frame.
typedef enum { EMOCAO_NENHUMA, EMOCAO_ALEGRE, EMOCAO_DUVIDA, EMOCAO_ALERTA } EmocaoLuci;

static EmocaoLuci emocaoLuci = EMOCAO_NENHUMA;
static float tempoEmocaoLuci = 0.0f;
static float brilhoLuci = 1.0f;        // cresce conforme o jogador resolve os desafios

// 0 = estável, 1 = sobrecarregada (de tanto pedirem ajuda). O main atualiza.
static float instabilidadeLuci = 0.0f;

static void MostrarEmocaoLuci(EmocaoLuci e, float segundos)
{
    emocaoLuci = e;
    tempoEmocaoLuci = segundos;
}

// Carrega a arte e descobre onde a Luci está dentro da imagem
static void CarregarLuci(void)
{
    const char *arquivo = AcharSpriteJogador(SPRITE_LUCI);

    if (arquivo == NULL)
        return;

    Image img = LoadImage(arquivo);
    Color *px = LoadImageColors(img);
    int minX = img.width, maxX = -1, minY = img.height, maxY = -1;

    for (int y = 0; y < img.height; y++) {
        for (int x = 0; x < img.width; x++) {
            if (px[y * img.width + x].a > 0) {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    recorteLuci = (maxX >= 0)
        ? (Rectangle){ (float)minX, (float)minY, (float)(maxX - minX + 1), (float)(maxY - minY + 1) }
        : (Rectangle){ 0, 0, (float)img.width, (float)img.height };

    texLuci = LoadTextureFromImage(img);

    // A imagem é bem maior que o tamanho na tela: suaviza a redução
    GenTextureMipmaps(&texLuci);
    SetTextureFilter(texLuci, TEXTURE_FILTER_TRILINEAR);

    UnloadImageColors(px);
    UnloadImage(img);
}

static void DescarregarLuci(void)
{
    if (texLuci.id > 0)
        UnloadTexture(texLuci);

    texLuci = (Texture2D){0};
}

// Desenha a Luci com centro em p e a altura pedida.
// Animações dela: brilho pulsando em volta e as asas batendo
// (o desenho estica e encolhe um pouco na horizontal).
// A flutuação e a órbita em volta do jogador vêm de quem chama.
static void DesenharLuciTamanho(Vector2 p, float altura)
{
    float t = (float)GetTime();
    float pulso = 0.5f + 0.5f * sinf(t * 3.0f);

    DrawCircleV(p, altura * (0.55f + 0.07f * brilhoLuci), Fade(SKYBLUE, (0.10f + 0.08f * pulso) * brilhoLuci));

    if (texLuci.id > 0) {

        float escala = altura / recorteLuci.height;
        float asas = 1.0f + 0.07f * sinf(t * 14.0f);
        float w = recorteLuci.width * escala * asas;
        float h = recorteLuci.height * escala;

        Rectangle destino = { p.x - w / 2, p.y - h / 2, w, h };

        // Instável: puxa para o magenta e, perto do limite, falha como as alucinações
        float k = instabilidadeLuci;
        Color tinta = { 255, (unsigned char)(255 - 140 * k), (unsigned char)(255 - 40 * k), 255 };

        if (k > 0.5f) {
            float d = 2.0f + 3.0f * k;
            DrawTexturePro(texLuci, recorteLuci, (Rectangle){ destino.x - d, destino.y, w, h }, (Vector2){ 0, 0 }, 0.0f, Fade(RED, 0.5f));
            DrawTexturePro(texLuci, recorteLuci, (Rectangle){ destino.x + d, destino.y, w, h }, (Vector2){ 0, 0 }, 0.0f, Fade(SKYBLUE, 0.5f));
        }

        DrawTexturePro(texLuci, recorteLuci, destino, (Vector2){ 0, 0 }, 0.0f, tinta);
        return;
    }

    // Sem a imagem: o quadradinho antigo
    float lado = altura * 0.4f;
    Rectangle corpo = { p.x - lado / 2, p.y - lado / 2, lado, lado };

    DrawRectangleRec(corpo, SKYBLUE);
    DrawRectangleLinesEx(corpo, 2, BLUE);
}

// Luci voando em volta do jogador (depois da primeira conversa).
// Ela dá voltas numa elipse na altura do peito: quando está "lá atrás"
// (parte de cima da elipse) fica menor e é desenhada antes do personagem.

static bool LuciAtrasDoJogador(float angulo)
{
    return sinf(angulo) < 0.0f;
}

// Ícone de emoção sobre a cabeça da Luci
static void DesenharEmocaoLuci(Vector2 p, float altura)
{
    if (tempoEmocaoLuci <= 0.0f || emocaoLuci == EMOCAO_NENHUMA)
        return;

    float pulo = sinf((float)GetTime() * 8.0f) * 2.0f;
    Vector2 c = { p.x, p.y - altura / 2 - 12 + pulo };

    DrawCircleV(c, 8, Fade((Color){ 20, 16, 34, 255 }, 0.85f));

    if (emocaoLuci == EMOCAO_ALEGRE)
        DesenharCoracao(c.x, c.y, 1, (Color){ 255, 120, 170, 255 });
    else if (emocaoLuci == EMOCAO_DUVIDA)
        DrawText("?", (int)c.x - MeasureText("?", 14) / 2, (int)c.y - 7, 14, WHITE);
    else
        DrawText("!", (int)c.x - MeasureText("!", 14) / 2, (int)c.y - 7, 14, GOLD);
}

static void DesenharCompanion(Vector2 jogadorPos, float angulo)
{
    Vector2 p = {
        jogadorPos.x + cosf(angulo) * RAIO_ORBITA_LUCI,
        jogadorPos.y - 14.0f + sinf(angulo) * 8.0f + sinf(angulo * 2.7f) * 2.0f
    };

    float profundidade = 0.85f + 0.15f * (0.5f + 0.5f * sinf(angulo));   // atrás = menor

    DesenharLuciTamanho(p, ALTURA_LUCI * profundidade);
    DesenharEmocaoLuci(p, ALTURA_LUCI * profundidade);
}

// Desenha o personagem com a Luci passando atrás ou na frente dele
static void DesenharJogadorComLuci(const SpriteDirecao sprites[NUM_DIRECOES], Vector2 pos, Direcao direcao,
                                   int quadro, bool comLuci, float anguloLuci)
{
    if (comLuci && LuciAtrasDoJogador(anguloLuci))
        DesenharCompanion(pos, anguloLuci);

    DesenharJogador(sprites, pos, direcao, quadro);

    if (comLuci && !LuciAtrasDoJogador(anguloLuci))
        DesenharCompanion(pos, anguloLuci);
}

// Centro da Luci parada na sala inicial (flutuando de leve)
static Vector2 CentroLuciSala(Rectangle hitbox, float tempo)
{
    return (Vector2){
        hitbox.x + hitbox.width / 2,
        hitbox.y + hitbox.height / 2 + sinf(tempo * 2.0f) * 4.0f
    };
}

// Parte "sólida" da Luci parada: só o tamanho do desenho dela.
// (a hitbox inteira continua valendo para falar com ela)
static Rectangle CorpoLuciSala(Rectangle hitbox)
{
    return (Rectangle){ hitbox.x + hitbox.width / 2 - 12, hitbox.y + hitbox.height / 2 - 12, 24, 24 };
}

// =============================================================
// FUNÇÕES AUXILIARES - CORREDOR
// =============================================================

static int ContarConcluidos(const ProgressoCorredor *p)
{
    int total = 0;

    for (int i = 0; i < TOTAL_MONITORES; i++)
        if (p->estado[i] == MON_ACERTOU || p->estado[i] == MON_ERROU)
            total++;

    return total;
}

// Primeiro monitor que ainda não foi resolvido (-1 = todos resolvidos)
static int ProximoMonitor(const ProgressoCorredor *p)
{
    for (int i = 0; i < TOTAL_MONITORES; i++)
        if (p->estado[i] == MON_NEUTRO || p->estado[i] == MON_ATIVO)
            return i;

    return -1;
}

// Área em que o jogador pode apertar E (o monitor + uma folga para
// os lados e para baixo, já que ele fica "em frente" dele).
static Rectangle ZonaInteracao(Rectangle a)
{
    return (Rectangle){ a.x - 16, a.y, a.width + 32, a.height + 90 };
}

static void IniciarTransicao(Transicao *t, TelaAtual destino, float duracao)
{
    if (t->ativa)
        return;

    t->ativa = true;
    t->alfa = 0.0f;
    t->fase = 0;
    t->duracao = duracao;
    t->destino = destino;
}

// Soma (ou tira, se delta < 0) pontos sem deixar a pontuação ficar negativa.
// Devolve quanto realmente mudou.
static int AlterarPontos(ProgressoCorredor *p, int delta)
{
    int antes = p->pontos;

    p->pontos += delta;

    if (p->pontos < 0)
        p->pontos = 0;

    return p->pontos - antes;
}

// ---------- Sistema de diálogo (estilo Undertale) ----------

static void DialogoMostrarTexto(SessaoDialogo *d, const char *texto, int larguraMax, int tamFonte)
{
    QuebrarTexto(texto, larguraMax, tamFonte, d->texto, (int)sizeof(d->texto));
    d->visiveis = 0.0f;
}

static void DialogoIniciar(SessaoDialogo *d, const Dialogo *conteudo, int objeto)
{
    d->conteudo = *conteudo;
    d->objeto = objeto;
    d->pagina = 0;
    d->respondeu = false;
    d->acertou = false;
    d->estado = DLG_PAGINAS;
    d->corBorda = WHITE;
    d->pontosGanhos = 0;
    d->inicio = GetTime();
    d->pulos = 0;
    d->origem = conteudo;
    d->versaoCurta = NULL;
    d->veioDoCartao = false;

    DialogoMostrarTexto(d, d->conteudo.paginas[0], LARGURA_TEXTO_CAIXA, FONTE_CAIXA);
}

// Conversa de uma página só montada na hora (ex.: com TextFormat)
static void DialogoFalaAvulsa(SessaoDialogo *d, const char *texto)
{
    Dialogo fala = {0};
    fala.paginas[0] = texto;
    fala.numPaginas = 1;

    DialogoIniciar(d, &fala, OBJ_NENHUM);
}

static void DialogoMostrarFeedback(SessaoDialogo *d, const char *texto, Color borda, int pontos)
{
    d->estado = DLG_FEEDBACK;
    d->corBorda = borda;
    d->pontosGanhos = pontos;

    DialogoMostrarTexto(d, texto, LARGURA_TEXTO_CAIXA, FONTE_CAIXA);
}

static int AlturaPainelPergunta(const SessaoDialogo *d)
{
    int h = 62;                                             // cabeçalho + barra de tempo
    h += ContarLinhas(d->texto) * 24 + 14;                  // pergunta

    for (int i = 0; i < NUM_OPCOES; i++)
        h += ContarLinhas(d->opcoesTexto[i]) * (d->fonteOpcao + 5) + 10;

    h += 30;                                                // rodapé
    return h;
}

static void DialogoMostrarPergunta(SessaoDialogo *d)
{
    static const int fontes[] = {16, 15, 14, 13};

    d->estado = DLG_ESCOLHA;
    d->selecao = 0;
    d->tempoRestante = TEMPO_QUIZ;

    DialogoMostrarTexto(d, d->conteudo.pergunta, LARGURA_PERGUNTA, FONTE_PERGUNTA);

    // As alternativas do PDF são longas: se não couberem, a fonte diminui um pouco
    for (int f = 0; f < 4; f++) {

        d->fonteOpcao = fontes[f];

        for (int i = 0; i < NUM_OPCOES; i++) {
            QuebrarTexto(
                TextFormat("%c) %s", 'A' + i, d->conteudo.opcoes[i]),
                LARGURA_OPCAO,
                d->fonteOpcao,
                d->opcoesTexto[i],
                (int)sizeof(d->opcoesTexto[i])
            );
        }

        if (AlturaPainelPergunta(d) <= ALTURA_TELA - 30)
            break;
    }
}

// Atualiza a conversa e avisa o que aconteceu neste frame.
//   ENTER / E / ESPAÇO : completa o texto na hora ou avança a página
//   W / S / SETAS      : move o coração entre as alternativas
//   ENTER / E / ESPAÇO : confirma a alternativa (1, 2, 3, 4 = atalho)
static ResultadoDialogo DialogoAtualizar(SessaoDialogo *d)
{
    bool avancar = IsKeyPressed(KEY_ENTER) ||
                   IsKeyPressed(KEY_E) ||
                   IsKeyPressed(KEY_SPACE);

    // Efeito máquina de escrever
    int total = (int)strlen(d->texto);

    if (d->visiveis < (float)total) {

        d->visiveis += VELOCIDADE_TEXTO * GetFrameTime();

        if (d->visiveis > (float)total)
            d->visiveis = (float)total;
    }

    bool terminou = (d->visiveis >= (float)total);

    switch (d->estado) {

        case DLG_PAGINAS: {

            // TAB: abre a versão completa / volta para a versão curta
            if (IsKeyPressed(KEY_TAB)) {

                if (d->conteudo.saberMais != NULL) {

                    const Dialogo *curta = d->origem;

                    Telemetria("saber_mais", d->conteudo.paginas[0], (float)(GetTime() - d->inicio), d->pulos);
                    DialogoIniciar(d, d->conteudo.saberMais, d->objeto);
                    d->versaoCurta = curta;
                    break;
                }

                if (d->versaoCurta != NULL) {
                    DialogoIniciar(d, d->versaoCurta, d->objeto);
                    break;
                }

                if (d->veioDoCartao) {
                    d->estado = DLG_NENHUM;
                    return RES_VOLTAR_CARTAO;
                }
            }

            if (!avancar)
                break;

            if (!terminou) {
                d->visiveis = (float)total;
                d->pulos++;
                break;
            }

            d->pagina++;

            if (d->pagina < d->conteudo.numPaginas) {

                DialogoMostrarTexto(d, d->conteudo.paginas[d->pagina], LARGURA_TEXTO_CAIXA, FONTE_CAIXA);

            } else if (d->conteudo.pergunta != NULL) {

                DialogoMostrarPergunta(d);

            } else {

                Telemetria("dialogo", d->conteudo.paginas[0], (float)(GetTime() - d->inicio), d->pulos);
                d->estado = DLG_NENHUM;
                return RES_FIM;
            }

        } break;

        case DLG_ESCOLHA: {

            // A barra de tempo só começa quando a pergunta terminou de aparecer
            if (!terminou) {

                if (avancar)
                    d->visiveis = (float)total;

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

            // Pequeno atraso para ninguém responder sem querer
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

                    // Errou: o jogo abre o minigame de esquiva
                    d->estado = DLG_NENHUM;
                    return RES_ERROU;
                }
            }

        } break;

        case DLG_FEEDBACK: {

            if (!avancar)
                break;

            if (!terminou) {
                d->visiveis = (float)total;
                break;
            }

            d->estado = DLG_NENHUM;
            return RES_FIM;

        } break;

        case DLG_CONFIRMA: {

            if (!terminou) {

                if (avancar)
                    d->visiveis = (float)total;

                break;
            }

            if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D) ||
                IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT) ||
                IsKeyPressed(KEY_W) || IsKeyPressed(KEY_S) ||
                IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN))
                d->selecao = 1 - d->selecao;

            if (avancar) {

                d->estado = DLG_NENHUM;
                return (d->selecao == 0) ? RES_SIM : RES_NAO;
            }

        } break;

        case DLG_NENHUM:
        default:
            break;
    }

    return RES_NADA;
}

static void DialogoDesenhar(const SessaoDialogo *d)
{
    if (d->estado == DLG_NENHUM)
        return;

    // Parte do texto que já "apareceu"
    char parcial[768];
    bool terminou = TextoParcial(d->texto, d->visiveis, parcial, (int)sizeof(parcial));
    bool piscar = (((int)(GetTime() * 2.0)) % 2 == 0);

    // -----------------------------------------------------
    // PERGUNTA: painel central com as 4 alternativas e a barra de tempo
    // -----------------------------------------------------

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
            px,
            py + 14,
            10,
            GOLD
        );

        // ---------- Barra de tempo ----------
        float fracao = d->tempoRestante / TEMPO_QUIZ;
        Color corTempo = (fracao > 0.5f) ? LIME : (fracao > 0.25f ? YELLOW : RED);

        if (fracao <= 0.25f && (((int)(GetTime() * 6.0)) % 2 == 0))
            corTempo = MAROON;

        DrawRectangle(px, py + 32, 620, 12, (Color){40, 40, 40, 255});
        DrawRectangle(px, py + 32, (int)(620 * fracao), 12, corTempo);
        DrawRectangleLines(px, py + 32, 620, 12, GRAY);

        DrawText(
            TextFormat("%2d s", (int)ceilf(d->tempoRestante)),
            px + 632,
            py + 31,
            14,
            corTempo
        );

        // ---------- Pergunta ----------
        DesenharTextoLinhas(parcial, px, py + 58, FONTE_PERGUNTA, 24, WHITE);

        // As alternativas só aparecem quando a pergunta terminou de ser "digitada"
        if (terminou) {

            int y = py + 58 + ContarLinhas(d->texto) * 24 + 14;
            int espaco = d->fonteOpcao + 5;

            for (int i = 0; i < NUM_OPCOES; i++) {

                bool marcada = (i == d->selecao);

                if (marcada)
                    DesenharCoracao((float)px + 8, (float)y + d->fonteOpcao / 2.0f, 2, RED);

                int nl = DesenharTextoLinhas(
                    d->opcoesTexto[i],
                    px + 24,
                    y,
                    d->fonteOpcao,
                    espaco,
                    marcada ? YELLOW : LIGHTGRAY
                );

                y += nl * espaco + 10;
            }

            DrawText(
                "W/S ou SETAS: escolher   |   ENTER: confirmar   |   1-4: atalho",
                px,
                (int)(painel.y + painel.height - 22),
                10,
                GRAY
            );
        }

        return;
    }

    // -----------------------------------------------------
    // PÁGINAS E FEEDBACK: caixa de diálogo na parte de baixo
    // (a altura cresce se o texto for grande)
    // -----------------------------------------------------

    bool confirma = (d->estado == DLG_CONFIRMA);
    int alturaCaixa = 16 + ContarLinhas(d->texto) * 26 + 34 + (confirma ? 30 : 0);

    if (alturaCaixa < 150)
        alturaCaixa = 150;

    Rectangle caixa = { 50, (float)(ALTURA_TELA - 14 - alturaCaixa), 700, (float)alturaCaixa };

    Color corBorda = (d->estado == DLG_FEEDBACK) ? d->corBorda : WHITE;

    DrawRectangleRec(caixa, BLACK);
    DrawRectangleLinesEx(caixa, 4, corBorda);

    DesenharTextoLinhas(
        parcial,
        (int)caixa.x + 20,
        (int)caixa.y + 16,
        FONTE_CAIXA,
        26,
        WHITE
    );

    // TAB: abre a versão completa ou volta para a curta
    if (d->estado == DLG_PAGINAS && d->conteudo.saberMais != NULL)
        DrawText("TAB - saber mais", (int)caixa.x + 20, (int)(caixa.y + caixa.height - 24), 12, Fade(SKYBLUE, 0.85f));
    else if (d->estado == DLG_PAGINAS && (d->versaoCurta != NULL || d->veioDoCartao))
        DrawText("TAB - voltar ao resumo", (int)caixa.x + 20, (int)(caixa.y + caixa.height - 24), 12, Fade(SKYBLUE, 0.85f));

    // Opções SIM / NÃO com o coração na escolhida
    if (confirma && terminou) {

        const char *opcoes[2] = { "SIM", "NÃO" };
        int yOpcoes = (int)(caixa.y + caixa.height) - 40;

        for (int i = 0; i < 2; i++) {

            int xOpcao = (int)caixa.x + 160 + i * 240;
            bool marcada = (d->selecao == i);

            if (marcada)
                DesenharCoracao((float)xOpcao - 18, (float)yOpcoes + 9, 2, RED);

            DrawText(opcoes[i], xOpcao, yOpcoes, 20, marcada ? YELLOW : LIGHTGRAY);
        }

        return;
    }

    if (terminou) {

        if (d->estado == DLG_FEEDBACK && d->pontosGanhos < 0) {

            DrawText(
                TextFormat("%d PONTOS", d->pontosGanhos),
                (int)caixa.x + 20,
                (int)(caixa.y + caixa.height - 26),
                14,
                RED
            );
        }

        if (d->estado == DLG_FEEDBACK && d->pontosGanhos > 0) {

            DrawText(
                TextFormat("+%d PONTOS", d->pontosGanhos),
                (int)caixa.x + 20,
                (int)(caixa.y + caixa.height - 26),
                14,
                GREEN
            );
        }

        if (piscar) {

            DrawText(
                "ENTER >",
                (int)(caixa.x + caixa.width - 90),
                (int)(caixa.y + caixa.height - 26),
                12,
                GOLD
            );
        }
    }
}

// ---------- Desenho dos monitores (4 estados) ----------
// Quando a arte final ficar pronta, é só colocar os PNGs nos caminhos
// SPRITE_MONITOR_* lá em cima: o jogo passa a usá-los automaticamente.

static void DesenharMonitor(int indice, Rectangle a, EstadoMonitor estado, float tempo, const Texture2D sprites[4])
{
    // Com sprite: desenha a imagem ocupando a tela + o pé do monitor
    if (sprites[estado].id > 0) {

        Texture2D t = sprites[estado];

        DrawTexturePro(
            t,
            (Rectangle){ 0, 0, (float)t.width, (float)t.height },
            (Rectangle){ a.x, a.y, a.width, a.height + 18 },
            (Vector2){ 0, 0 },
            0.0f,
            WHITE
        );

        return;
    }

    // Sem sprite: formas simples
    int cx = (int)(a.x + a.width / 2);
    float pulso = 0.5f + 0.5f * sinf(tempo * 5.0f);

    Color corBorda = GRAY;
    Color corLed = DARKGRAY;

    // Pé do monitor
    DrawRectangle(cx - 6, (int)(a.y + a.height), 12, 12, (Color){45, 40, 62, 255});
    DrawRectangle(cx - 24, (int)(a.y + a.height) + 12, 48, 6, (Color){45, 40, 62, 255});

    // Carcaça
    DrawRectangleRec(a, (Color){30, 28, 44, 255});

    Rectangle visor = { a.x + 7, a.y + 7, a.width - 14, a.height - 18 };
    int vx = (int)visor.x;
    int vy = (int)visor.y;
    int vw = (int)visor.width;
    int vh = (int)visor.height;
    int cy = vy + vh / 2;

    switch (estado) {

        // ---------- NEUTRO: desligado, esperando o jogador ----------
        case MON_NEUTRO: {

            DrawRectangleRec(visor, (Color){10, 12, 20, 255});

            const char *num = TextFormat("%d", indice + 1);
            DrawText(num, cx - MeasureText(num, 20) / 2, cy - 10, 20, (Color){55, 60, 78, 255});

            if (((int)(tempo * 1.5f)) % 2 == 0)
                DrawText("_", vx + 6, vy + vh - 14, 10, (Color){80, 90, 110, 255});

            corBorda = GRAY;
            corLed = (Color){70, 70, 70, 255};

        } break;

        // ---------- ATIVO: durante a interação / quiz ----------
        case MON_ATIVO: {

            DrawRectangleRec(visor, (Color){12, 40, 75, 255});

            // Linhas de varredura
            for (int yy = 0; yy < vh; yy += 4)
                DrawLine(vx, vy + yy, vx + vw, vy + yy, Fade(BLACK, 0.25f));

            float varre = fmodf(tempo * 40.0f, (float)vh);
            DrawRectangle(vx, vy + (int)varre, vw, 3, Fade(SKYBLUE, 0.35f));

            DrawText("?", cx - MeasureText("?", 28) / 2, cy - 14, 28, Fade(WHITE, 0.6f + 0.4f * pulso));

            corBorda = Fade(GOLD, 0.6f + 0.4f * pulso);
            corLed = (pulso > 0.5f) ? YELLOW : ORANGE;

        } break;

        // ---------- ACERTOU ----------
        case MON_ACERTOU: {

            DrawRectangleRec(visor, (Color){8, 48, 22, 255});

            DrawLineEx((Vector2){ (float)cx - 16, (float)cy }, (Vector2){ (float)cx - 4, (float)cy + 12 }, 5, LIME);
            DrawLineEx((Vector2){ (float)cx - 4, (float)cy + 12 }, (Vector2){ (float)cx + 18, (float)cy - 14 }, 5, LIME);

            corBorda = GREEN;
            corLed = LIME;

        } break;

        // ---------- ERROU: chiado de alucinação ----------
        case MON_ERROU: {

            DrawRectangleRec(visor, (Color){55, 8, 14, 255});

            unsigned int quadro = (unsigned int)(tempo * 12.0f) + (unsigned int)indice * 97u;

            for (int k = 0; k < 6; k++) {

                unsigned int r = Ruido(quadro * 7u + (unsigned int)k);
                int lw = 10 + (int)((r >> 8) % (unsigned int)(vw - 10));
                int lx = vx + (int)((r >> 16) % (unsigned int)(vw - lw + 1));
                int ly = vy + (int)(r % (unsigned int)vh);

                DrawRectangle(lx, ly, lw, 2, Fade(RED, 0.5f));
            }

            DrawLineEx((Vector2){ (float)cx - 12, (float)cy - 12 }, (Vector2){ (float)cx + 12, (float)cy + 12 }, 4, RED);
            DrawLineEx((Vector2){ (float)cx + 12, (float)cy - 12 }, (Vector2){ (float)cx - 12, (float)cy + 12 }, 4, RED);

            corBorda = RED;
            corLed = RED;

        } break;

        default:
            break;
    }

    DrawRectangleLinesEx(a, 3, corBorda);

    // LED de energia
    DrawCircle((int)(a.x + a.width - 10), (int)(a.y + a.height - 6), 2, corLed);
}

static void DesenharPortaCorredor(Rectangle porta, Color corBorda)
{
    DrawRectangleRec(porta, (Color){35, 25, 55, 255});
    DrawRectangleLinesEx(porta, 3, corBorda);

    // Maçaneta perto da borda maior
    if (porta.width > porta.height)
        DrawCircle((int)(porta.x + porta.width - 12), (int)(porta.y + porta.height / 2), 3, GOLD);
    else
        DrawCircle((int)(porta.x + 9), (int)(porta.y + porta.height / 2), 4, GOLD);
}

// =============================================================
// BATALHA ESTILO UNDERTALE
// =============================================================

static void BatalhaTexto(Batalha *b, const char *texto)
{
    QuebrarTexto(texto, 640, FONTE_BATALHA, b->texto, (int)sizeof(b->texto));
    b->visiveis = 0.0f;
}

static void BatalhaIniciar(Batalha *b, TipoBatalha tipo, int quiz, int nivelAlucinacao)
{
    memset(b, 0, sizeof(*b));

    b->tipo = tipo;
    b->quiz = quiz;
    b->fase = FASE_INTRO;
    b->hp = HP_MAX_BATALHA;

    // README: quanto mais o jogador erra, mais forte a alucinação fica
    int nivel = (nivelAlucinacao > 10) ? 10 : nivelAlucinacao;
    float forca = 1.0f + 0.06f * (float)nivel;

    if (tipo == BAT_COMBATE) {

        b->duracao = DURACAO_COMBATE;
        b->intervaloBala = 0.20f / forca;
        b->velocidadeBala = 150.0f * forca;
        b->dano = 3;
        b->padrao = 0;

        BatalhaTexto(b,
            "* O tempo acabou... A ALUCINAÇÃO saiu do monitor e bloqueia o seu caminho!\n"
            "* Desvie dos dados corrompidos até ela perder a força.");

    } else {

        b->duracao = DURACAO_MINIGAME;
        b->intervaloBala = 0.32f / forca;
        b->velocidadeBala = 125.0f * forca;
        b->dano = 3;
        b->padrao = quiz % 3;   // cada monitor tem um padrão de ataque

        BatalhaTexto(b, TextFormat(
            "* Resposta errada! Um fragmento de alucinação escapou do monitor %d.\n"
            "* Desvie dos dados corrompidos!", quiz + 1));
    }
}

static void BatalhaCriarBala(Batalha *b, Rectangle c)
{
    static const char glifos[] = "01?!#%";

    int livre = -1;

    for (int i = 0; i < MAX_BALAS; i++) {
        if (!b->balas[i].ativa) {
            livre = i;
            break;
        }
    }

    if (livre < 0)
        return;

    Bala *bl = &b->balas[livre];
    float v = b->velocidadeBala * (0.85f + GetRandomValue(0, 30) / 100.0f);

    int xMin = (int)c.x + 12, xMax = (int)(c.x + c.width) - 12;
    int yMin = (int)c.y + 12, yMax = (int)(c.y + c.height) - 12;

    bl->ativa = true;
    bl->glifo = glifos[GetRandomValue(0, (int)sizeof(glifos) - 2)];

    switch (b->padrao) {

        // CHUVA: dados caindo de cima
        case 0: {
            bl->pos = (Vector2){ (float)GetRandomValue(xMin, xMax), c.y - 12 };
            bl->vel = (Vector2){ 0, v };
        } break;

        // LATERAL: dados atravessando de um lado para o outro
        case 1: {
            bool daEsquerda = (GetRandomValue(0, 1) == 0);
            bl->pos = (Vector2){ daEsquerda ? c.x - 12 : c.x + c.width + 12, (float)GetRandomValue(yMin, yMax) };
            bl->vel = (Vector2){ daEsquerda ? v : -v, 0 };
        } break;

        // MIRA: dados vindo das bordas em direção ao coração
        default: {
            Vector2 p;

            switch (GetRandomValue(0, 3)) {
                case 0:  p = (Vector2){ (float)GetRandomValue(xMin, xMax), c.y - 12 }; break;
                case 1:  p = (Vector2){ (float)GetRandomValue(xMin, xMax), c.y + c.height + 12 }; break;
                case 2:  p = (Vector2){ c.x - 12, (float)GetRandomValue(yMin, yMax) }; break;
                default: p = (Vector2){ c.x + c.width + 12, (float)GetRandomValue(yMin, yMax) }; break;
            }

            bl->pos = p;
            bl->vel = Vector2Scale(Vector2Normalize(Vector2Subtract(b->alma, p)), v * 0.9f);
        } break;
    }
}

// Atualiza a batalha. Devolve true quando o jogador fecha o texto final.
//   ENTER / E / ESPAÇO / Z : avança o texto
//   W A S D / SETAS        : move o coração
static bool BatalhaAtualizar(Batalha *b)
{
    float dt = GetFrameTime();

    if (b->tremor > 0.0f) b->tremor -= dt;
    if (b->fase != FASE_DESVIO) b->tremor = 0.0f;

    bool avancar = IsKeyPressed(KEY_ENTER) ||
                   IsKeyPressed(KEY_E) ||
                   IsKeyPressed(KEY_SPACE) ||
                   IsKeyPressed(KEY_Z);

    int total = (int)strlen(b->texto);

    if (b->visiveis < (float)total) {

        b->visiveis += VELOCIDADE_TEXTO * dt;

        if (b->visiveis > (float)total)
            b->visiveis = (float)total;
    }

    bool terminou = (b->visiveis >= (float)total);

    switch (b->fase) {

        case FASE_INTRO: {

            if (avancar) {

                if (!terminou) b->visiveis = (float)total;
                else           b->fase = FASE_ENCOLHENDO;
            }

        } break;

        case FASE_ENCOLHENDO: {

            b->anim += dt / DURACAO_ANIM_CAIXA;

            if (b->anim >= 1.0f) {

                b->anim = 1.0f;
                b->fase = FASE_DESVIO;
                b->alma = (Vector2){ CAIXA_DESVIO.x + CAIXA_DESVIO.width / 2, CAIXA_DESVIO.y + CAIXA_DESVIO.height / 2 };
            }

        } break;

        case FASE_DESVIO: {

            Rectangle c = CAIXA_DESVIO;

            b->tempoDesvio += dt;

            if (b->invencivel > 0.0f) b->invencivel -= dt;

            // ---------- Movimento do coração ----------
            Vector2 dir = {0};

            if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    dir.y -= 1.0f;
            if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  dir.y += 1.0f;
            if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  dir.x -= 1.0f;
            if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) dir.x += 1.0f;

            if (Vector2Length(dir) > 0.0f)
                b->alma = Vector2Add(b->alma, Vector2Scale(Vector2Normalize(dir), VELOCIDADE_ALMA * dt));

            float margem = 4.0f + 8.0f;   // borda da caixa + metade do coração

            b->alma.x = Clamp(b->alma.x, c.x + margem, c.x + c.width - margem);
            b->alma.y = Clamp(b->alma.y, c.y + margem, c.y + c.height - margem);

            // ---------- Ataques ----------
            // No COMBATE os padrões se alternam: chuva -> lateral -> mira
            if (b->tipo == BAT_COMBATE)
                b->padrao = ((int)(b->tempoDesvio / (b->duracao / 3.0f))) % 3;

            b->cronoBala -= dt;

            if (b->cronoBala <= 0.0f) {

                b->cronoBala += b->intervaloBala;
                BatalhaCriarBala(b, c);
            }

            for (int i = 0; i < MAX_BALAS; i++) {

                Bala *bl = &b->balas[i];

                if (!bl->ativa)
                    continue;

                bl->pos = Vector2Add(bl->pos, Vector2Scale(bl->vel, dt));

                // Saiu da caixa: some
                if (bl->pos.x < c.x - 30 || bl->pos.x > c.x + c.width + 30 ||
                    bl->pos.y < c.y - 30 || bl->pos.y > c.y + c.height + 30) {

                    bl->ativa = false;
                    continue;
                }

                // Acertou o coração
                if (b->invencivel <= 0.0f &&
                    Vector2Distance(bl->pos, b->alma) < RAIO_BALA + RAIO_ALMA) {

                    b->hp -= b->dano;
                    b->invencivel = TEMPO_INVENCIVEL;
                    b->tremor = 0.2f;
                    b->danosRecebidos++;
                    bl->ativa = false;
                }
            }

            if (b->hp <= 0) {

                b->hp = 0;
                b->derrota = true;
            }

            if (b->derrota || b->tempoDesvio >= b->duracao) {

                for (int i = 0; i < MAX_BALAS; i++)
                    b->balas[i].ativa = false;

                b->fase = FASE_EXPANDINDO;
            }

        } break;

        case FASE_EXPANDINDO: {

            b->anim -= dt / DURACAO_ANIM_CAIXA;

            if (b->anim <= 0.0f) {

                b->anim = 0.0f;
                b->fase = FASE_FIM;

                if (b->derrota)
                    BatalhaTexto(b,
                        "* A alucinação te dominou por um instante...\n"
                        "* Você não perde vidas, mas perde pontos e ela ficou mais forte.");
                else if (b->danosRecebidos == 0)
                    BatalhaTexto(b,
                        "* Esquiva perfeita! A alucinação se desfaz em ruído.\n"
                        "* Luci quer conversar sobre a resposta.");
                else
                    BatalhaTexto(b,
                        "* Você resistiu! A alucinação se desfaz em ruído.\n"
                        "* Luci quer conversar sobre a resposta.");
            }

        } break;

        case FASE_FIM: {

            if (avancar) {

                if (!terminou) b->visiveis = (float)total;
                else           return true;
            }

        } break;

        default:
            break;
    }

    return false;
}

// Inimigo: um fantasma glitchado (placeholder até a arte ficar pronta)
static void DesenharAlucinacao(Vector2 centro, float tempo, bool forte)
{
    float s = forte ? 1.25f : 0.9f;
    Vector2 p = { centro.x, centro.y + sinf(tempo * 2.0f) * 6.0f };

    // Três camadas deslocadas = efeito de "aberração cromática"
    Color cores[3] = { Fade(RED, 0.45f), Fade(SKYBLUE, 0.45f), (Color){ 200, 170, 255, 255 } };
    float desloc[3] = { -4.0f, 4.0f, 0.0f };

    for (int k = 0; k < 3; k++) {

        float cx = p.x + desloc[k] * (forte ? 1.5f : 1.0f);

        DrawCircle((int)cx, (int)(p.y - 10 * s), 42 * s, cores[k]);
        DrawRectangle((int)(cx - 42 * s), (int)(p.y - 10 * s), (int)(84 * s), (int)(50 * s), cores[k]);

        // Barra ondulada embaixo (parada: a alucinação só flutua, sem tremer)
        for (int j = 0; j < 5; j++)
            DrawCircle((int)(cx - 34 * s + j * 17 * s), (int)(p.y + 40 * s + ((j % 2) ? 2.0f : -2.0f)), 9 * s, cores[k]);
    }

    // Olhos
    DrawCircle((int)(p.x - 15 * s), (int)(p.y - 14 * s), 7 * s, BLACK);
    DrawCircle((int)(p.x + 15 * s), (int)(p.y - 14 * s), 7 * s, BLACK);

    if (forte) {
        DrawCircle((int)(p.x - 15 * s), (int)(p.y - 14 * s), 2.5f * s, RED);
        DrawCircle((int)(p.x + 15 * s), (int)(p.y - 14 * s), 2.5f * s, RED);
    }

    // Boca em zigue-zague
    for (int j = 0; j < 5; j++) {

        Vector2 a = { p.x - 18 * s + j * 7.2f * s,       p.y + 8 * s + (j % 2) * 6 * s };
        Vector2 b = { p.x - 18 * s + (j + 1) * 7.2f * s, p.y + 8 * s + ((j + 1) % 2) * 6 * s };

        DrawLineEx(a, b, 2, BLACK);
    }

    // Símbolos orbitando
    const char *simbolos[4] = { "?", "404", "01", "!" };

    for (int j = 0; j < 4; j++) {

        float ang = tempo * 1.2f + j * (PI / 2.0f);
        int sx = (int)(p.x + cosf(ang) * 85 * s);
        int sy = (int)(p.y + sinf(ang) * 30 * s);

        DrawText(simbolos[j], sx - MeasureText(simbolos[j], 14) / 2, sy - 7, 14, Fade(WHITE, 0.6f));
    }
}

static void BatalhaDesenhar(const Batalha *b, const char *nickname, float tempo)
{
    ClearBackground(BLACK);

    bool forte = (b->tipo == BAT_COMBATE);

    // ---------- Inimigo ----------
    const char *nome = forte ? "ALUCINAÇÃO" : "FRAGMENTO DE ALUCINAÇÃO";

    DrawText(forte ? "COMBATE" : "MINIGAME", 20, 16, 10, forte ? RED : ORANGE);
    DrawText(nome, LARGURA_TELA / 2 - MeasureText(nome, 14) / 2, 34, 14, GRAY);

    DesenharAlucinacao((Vector2){ LARGURA_TELA / 2.0f, 150.0f }, tempo, forte);

    // ---------- Caixa (encolhe/expande com suavização) ----------
    float t = b->anim * b->anim * (3.0f - 2.0f * b->anim);

    Rectangle caixa = {
        Lerp(CAIXA_TEXTO_BATALHA.x, CAIXA_DESVIO.x, t),
        Lerp(CAIXA_TEXTO_BATALHA.y, CAIXA_DESVIO.y, t),
        Lerp(CAIXA_TEXTO_BATALHA.width, CAIXA_DESVIO.width, t),
        Lerp(CAIXA_TEXTO_BATALHA.height, CAIXA_DESVIO.height, t)
    };

    if (b->tremor > 0.0f) {
        caixa.x += (float)GetRandomValue(-3, 3);
        caixa.y += (float)GetRandomValue(-3, 3);
    }

    DrawRectangleRec(caixa, BLACK);
    DrawRectangleLinesEx(caixa, 4, WHITE);

    // ---------- Texto (intro e final) ----------
    if (b->fase == FASE_INTRO || b->fase == FASE_FIM) {

        char parcial[512];
        bool terminou = TextoParcial(b->texto, b->visiveis, parcial, (int)sizeof(parcial));

        DesenharTextoLinhas(parcial, (int)caixa.x + 24, (int)caixa.y + 22, FONTE_BATALHA, 30, WHITE);

        if (terminou && (((int)(GetTime() * 2.0)) % 2 == 0))
            DrawText("ENTER >", (int)(caixa.x + caixa.width - 90), (int)(caixa.y + caixa.height - 26), 12, GOLD);
    }

    // ---------- Esquiva ----------
    if (b->fase == FASE_DESVIO) {

        BeginScissorMode((int)caixa.x + 4, (int)caixa.y + 4, (int)caixa.width - 8, (int)caixa.height - 8);

        for (int i = 0; i < MAX_BALAS; i++) {

            if (!b->balas[i].ativa)
                continue;

            char g[2] = { b->balas[i].glifo, '\0' };

            DrawText(
                g,
                (int)b->balas[i].pos.x - MeasureText(g, 20) / 2,
                (int)b->balas[i].pos.y - 10,
                20,
                WHITE
            );
        }

        // Coração pisca enquanto está invencível
        if (b->invencivel <= 0.0f || (((int)(b->invencivel * 10.0f)) % 2 == 0))
            DesenharCoracao(b->alma.x, b->alma.y, 2, RED);

        EndScissorMode();

        // Quanto falta para a alucinação perder a força
        float resta = 1.0f - b->tempoDesvio / b->duracao;

        if (resta < 0.0f)
            resta = 0.0f;

        DrawRectangle((int)caixa.x, (int)(caixa.y + caixa.height + 6), (int)(caixa.width * resta), 3, Fade(WHITE, 0.5f));
    }

    // ---------- Linha de status (NOME  LV  HP) ----------
    int yStatus = 452;

    DrawText(nickname, 60, yStatus, 20, WHITE);
    DrawText("LV 1", 150, yStatus, 20, WHITE);
    DrawText("HP", 250, yStatus + 4, 12, WHITE);

    DrawRectangle(275, yStatus, HP_MAX_BATALHA * 4, 20, RED);
    DrawRectangle(275, yStatus, b->hp * 4, 20, YELLOW);

    DrawText(TextFormat("%d / %d", b->hp, HP_MAX_BATALHA), 275 + HP_MAX_BATALHA * 4 + 14, yStatus, 20, WHITE);

    // ---------- Instruções ----------
    const char *instrucao = (b->fase == FASE_DESVIO)
        ? "W A S D ou SETAS  -  mova o coração e desvie!"
        : "ENTER  -  continuar";

    DrawText(instrucao, LARGURA_TELA / 2 - MeasureText(instrucao, 12) / 2, 540, 12, GRAY);
}

// =============================================================
// SAVE
// =============================================================

// Grava primeiro num arquivo temporário e só depois troca pelo save de verdade:
// se o jogo fechar no meio da gravação, o save anterior continua inteiro.
static bool GravarSave(const DadosSave *d)
{
    FILE *f = fopen(ARQUIVO_SAVE_TEMP, "wb");

    if (f == NULL)
        return false;

    bool ok = (fwrite(d, sizeof(*d), 1, f) == 1);

    if (fclose(f) != 0)
        ok = false;

    if (!ok) {
        remove(ARQUIVO_SAVE_TEMP);
        return false;
    }

    remove(ARQUIVO_SAVE);                   // no Windows o rename falha se o destino existir

    return (rename(ARQUIVO_SAVE_TEMP, ARQUIVO_SAVE) == 0);
}

// Lê e confere o save. Devolve false se não existir, for de outra versão
// ou estiver corrompido (nesse caso o jogo simplesmente começa do zero).
static bool LerSave(DadosSave *d)
{
    FILE *f = fopen(ARQUIVO_SAVE, "rb");

    if (f == NULL)
        return false;

    memset(d, 0, sizeof(*d));

    size_t lidos = fread(d, 1, sizeof(*d), f);
    fclose(f);

    if (d->assinatura != ASSINATURA_SAVE)
        return false;

    // Save da versão 3 (sem a estabilidade da Luci no final): abre com os valores iniciais
    if (d->versao == 3 && lidos == offsetof(DadosSave, estabilidadeLuci)) {
        d->versao = 4;
        d->estabilidadeLuci = ESTABILIDADE_MAX_LUCI;
        d->ajudasPedidas = 0;
        lidos = offsetof(DadosSave, chefeVencido);
    }

    // Save da versão 4 (antes do mini-chefe): quem já tinha saído do corredor não precisa enfrentá-lo
    if (d->versao == 4 && lidos == offsetof(DadosSave, chefeVencido)) {
        d->versao = 5;
        d->chefeVencido = d->progresso.fimAnunciado;
        d->idPartida = 0;
        lidos = offsetof(DadosSave, biblioteca);
    }

    // Save da versão 5 (antes da Biblioteca dos Ecos): começa a Biblioteca do zero
    if (d->versao == 5 && lidos == offsetof(DadosSave, biblioteca)) {
        d->versao = VERSAO_SAVE;
        memset(&d->biblioteca, 0, sizeof(d->biblioteca));
        lidos = sizeof(*d);
    }

    if (lidos != sizeof(*d) || d->versao != VERSAO_SAVE)
        return false;

    if (d->estabilidadeLuci < 0 || d->estabilidadeLuci > ESTABILIDADE_MAX_LUCI)
        d->estabilidadeLuci = ESTABILIDADE_MAX_LUCI;

    // Quem estava no Bloco 2 antigo (Arquivo de Treinamento) continua no átrio da Biblioteca
    if (d->sala == SETOR2 || d->sala == SALA_REGISTROS || d->sala == SALA_LOGICA || d->sala == SALA_LACUNA) {
        d->sala = BIBLIOTECA;
        d->chefeVencido = true;
    }

    if (d->sala != JOGANDO && d->sala != CORREDOR && d->sala != BIBLIOTECA)
        return false;

    memset(&d->bloco2, 0, sizeof(d->bloco2));

    if (d->biblioteca.fragmentos < 0) d->biblioteca.fragmentos = 0;
    for (int i = 0; i < 3; i++)
        if (d->biblioteca.melhorias[i] < 0) d->biblioteca.melhorias[i] = 0;

    if (d->direcao < 0 || d->direcao >= NUM_DIRECOES)
        d->direcao = DIR_FRENTE;

    for (int i = 0; i < 3; i++)
        if (d->nickname[i] < 'A' || d->nickname[i] > 'Z')
            return false;

    d->nickname[3] = '\0';

    for (int i = 0; i < TOTAL_MONITORES; i++)
        if (d->progresso.estado[i] < MON_NEUTRO || d->progresso.estado[i] > MON_ERROU)
            d->progresso.estado[i] = MON_NEUTRO;

    // Posição dentro dos limites da sala salva (na Biblioteca o jogo começa no átrio)
    if (d->sala == JOGANDO) {
        d->x = Clamp(d->x, 80, 720);
        d->y = Clamp(d->y, 80, 520);
    } else {
        d->x = Clamp(d->x, 80, 720);
        d->y = Clamp(d->y, 285, 395);
    }

    return true;
}

// Monta o save a partir do estado atual do jogo e grava
static bool SalvarJogo(const char *nickname, TelaAtual tela, Vector2 pos, Direcao direcao,
                       bool luciAcompanhando, const ProgressoCorredor *progresso,
                       int estabilidadeLuci, int ajudasPedidas, bool chefeVencido, int idPartida,
                       const ProgressoBiblioteca *biblioteca)
{
    DadosSave d;
    memset(&d, 0, sizeof(d));

    d.assinatura = ASSINATURA_SAVE;
    d.versao = VERSAO_SAVE;
    memcpy(d.nickname, nickname, 4);

    // A batalha acontece "dentro" do corredor. Na Biblioteca, quem volta começa no átrio.
    if (tela == JOGANDO)          d.sala = JOGANDO;
    else if (tela == BIBLIOTECA)  d.sala = BIBLIOTECA;
    else                          d.sala = CORREDOR;

    d.x = pos.x;
    d.y = pos.y;
    d.direcao = (int)direcao;
    d.luciAcompanhando = luciAcompanhando;
    d.progresso = *progresso;
    d.biblioteca = *biblioteca;
    d.estabilidadeLuci = estabilidadeLuci;
    d.ajudasPedidas = ajudasPedidas;
    d.chefeVencido = chefeVencido;
    d.idPartida = idPartida;

    // Quiz ou batalha pela metade: o monitor volta para NEUTRO
    // e o jogador refaz aquele quiz quando carregar o jogo
    for (int i = 0; i < TOTAL_MONITORES; i++)
        if (d.progresso.estado[i] == MON_ATIVO)
            d.progresso.estado[i] = MON_NEUTRO;

    bool ok = GravarSave(&d);

    TraceLog(ok ? LOG_INFO : LOG_WARNING, "SAVE: %s", ok ? "jogo salvo em " ARQUIVO_SAVE : "falha ao salvar");
    return ok;
}

// Aviso no canto da tela depois de um auto save
static void DesenharAvisoSave(float aviso, bool falhou, float tempo)
{
    if (aviso <= 0.0f)
        return;

    float alfa = (aviso < 0.5f) ? aviso / 0.5f : 1.0f;
    bool salvando = (aviso > TEMPO_AVISO_SAVE - 0.6f);

    const char *texto = falhou ? "ERRO AO SALVAR" : (salvando ? "SALVANDO..." : "JOGO SALVO");
    Color cor = falhou ? RED : (salvando ? SKYBLUE : PURPLE);

    int largura = MeasureText(texto, 12);
    int x = 750 - largura;
    int y = 566;

    // "01" girando ao lado, como dados passando
    const char *dados = (((int)(tempo * 10.0f)) % 2) ? "01" : "10";

    DrawText(dados, x - 22, y, 12, Fade(SKYBLUE, alfa));
    DrawText(texto, x, y, 12, Fade(cor, alfa));
}

static void DesenharTextoBotao(Rectangle b, const char *texto, int tamanho, Color cor)
{
    DrawText(texto, (int)(b.x + b.width / 2) - MeasureText(texto, tamanho) / 2, (int)(b.y + b.height / 2) - tamanho / 2, tamanho, cor);
}

// =============================================================
// RANKING (TOP 10)
//   Fica num arquivo separado do save: começar um jogo novo não apaga
//   o ranking. Cada partida tem um número (idPartida, que vai no save)
//   e aparece uma vez só, com a melhor pontuação que fez.
//   Gravado no fim do Bloco 1 (ALUCINAÇÃO MESTRE) e na porta do NÚCLEO.
// =============================================================

#define ARQUIVO_RANKING       "utopia_ranking.dat"
#define ARQUIVO_RANKING_TEMP  "utopia_ranking.dat.tmp"
#define ASSINATURA_RANKING    0x4B4E4152u
#define VERSAO_RANKING        1
#define MAX_RANKING           10
#define TITULO_RANKING        "TOP 10 TECNIGHT"     // AJUSTE AQUI (nome do evento)

typedef struct {
    char nickname[4];
    int pontos;
    int idPartida;
} EntradaRanking;

typedef struct {
    unsigned int assinatura;
    int versao;
    int total;
    EntradaRanking entradas[MAX_RANKING];   // da maior para a menor pontuação
} Ranking;

// Ranking aberto por cima do jogo (fim do Bloco 1 / fim da partida)
typedef struct {
    bool aberto;
    const char *titulo;
    int posicao;                // 0 = 1º lugar, -1 = ficou fora do TOP 10
    int pontos;
    int idPartida;
    double inicio;
} TelaRanking;

static int NovoIdPartida(void)
{
    return 1 + (int)(time(NULL) % 1000000) * 1000 + GetRandomValue(0, 999);
}

// Arquivo inexistente ou estragado = ranking vazio
static void LerRanking(Ranking *r)
{
    FILE *f = fopen(ARQUIVO_RANKING, "rb");
    bool ok = false;

    if (f != NULL) {
        ok = (fread(r, sizeof(*r), 1, f) == 1);
        fclose(f);
    }

    ok = ok && r->assinatura == ASSINATURA_RANKING && r->versao == VERSAO_RANKING &&
         r->total >= 0 && r->total <= MAX_RANKING;

    for (int i = 0; ok && i < r->total; i++) {
        r->entradas[i].nickname[3] = '\0';
        for (int c = 0; c < 3; c++)
            if (r->entradas[i].nickname[c] < 'A' || r->entradas[i].nickname[c] > 'Z')
                ok = false;
    }

    if (!ok) {
        memset(r, 0, sizeof(*r));
        r->assinatura = ASSINATURA_RANKING;
        r->versao = VERSAO_RANKING;
    }
}

// Mesmo cuidado do save: grava no temporário e depois troca
static bool GravarRanking(const Ranking *r)
{
    FILE *f = fopen(ARQUIVO_RANKING_TEMP, "wb");

    if (f == NULL)
        return false;

    bool ok = (fwrite(r, sizeof(*r), 1, f) == 1);

    if (fclose(f) != 0)
        ok = false;

    if (!ok) {
        remove(ARQUIVO_RANKING_TEMP);
        return false;
    }

    remove(ARQUIVO_RANKING);

    return (rename(ARQUIVO_RANKING_TEMP, ARQUIVO_RANKING) == 0);
}

// Coloca (ou melhora) a pontuação desta partida.
// Devolve a posição (0 = 1º lugar) ou -1 se ficou fora do TOP 10.
static int RegistrarNoRanking(Ranking *r, const char *nickname, int pontos, int idPartida)
{
    LerRanking(r);      // relê: outra partida pode ter gravado desde a última vez

    int i = -1;

    for (int k = 0; k < r->total; k++)
        if (r->entradas[k].idPartida == idPartida) { i = k; break; }

    if (i >= 0) {
        if (pontos <= r->entradas[i].pontos)
            return i;
    } else if (r->total < MAX_RANKING) {
        i = r->total++;
    } else if (pontos > r->entradas[MAX_RANKING - 1].pontos) {
        i = MAX_RANKING - 1;                // tira o último colocado
    } else {
        return -1;
    }

    memcpy(r->entradas[i].nickname, nickname, 4);
    r->entradas[i].nickname[3] = '\0';
    r->entradas[i].pontos = pontos;
    r->entradas[i].idPartida = idPartida;

    // Sobe até o lugar certo (empate: quem chegou antes fica na frente)
    while (i > 0 && r->entradas[i].pontos > r->entradas[i - 1].pontos) {
        EntradaRanking troca = r->entradas[i];
        r->entradas[i] = r->entradas[i - 1];
        r->entradas[i - 1] = troca;
        i--;
    }

    if (!GravarRanking(r))
        TraceLog(LOG_WARNING, "RANKING: falha ao gravar " ARQUIVO_RANKING);

    Telemetria("ranking", nickname, (float)pontos, i + 1);
    return i;
}

// Nickname novo não pode repetir um que já está na tabela (relê o arquivo antes de conferir)
static bool NicknameDisponivel(Ranking *r, const char *nickname)
{
    LerRanking(r);

    for (int i = 0; i < r->total; i++)
        if (strncmp(r->entradas[i].nickname, nickname, 3) == 0)
            return false;

    return true;
}

static void AbrirTelaRanking(TelaRanking *t, Ranking *r, const char *titulo,
                             const char *nickname, int pontos, int idPartida)
{
    t->aberto = true;
    t->titulo = titulo;
    t->pontos = pontos;
    t->idPartida = idPartida;
    t->posicao = RegistrarNoRanking(r, nickname, pontos, idPartida);
    t->inicio = GetTime();
}

// Devolve true quando o jogador fecha o ranking
static bool AtualizarTelaRanking(TelaRanking *t)
{
    if (!t->aberto || GetTime() - t->inicio < 0.6)
        return false;

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE)) {
        t->aberto = false;
        return true;
    }

    return false;
}

// As 10 linhas do ranking. idDestaque = partida que pisca em dourado (0 = nenhuma)
static void DesenharListaRanking(const Ranking *r, int x, int y, int largura, int fonte, int idDestaque, float tempo)
{
    static const Color podio[3] = { { 255, 203, 0, 255 }, { 200, 200, 220, 255 }, { 205, 127, 50, 255 } };
    int alturaLinha = fonte + 8;

    for (int i = 0; i < MAX_RANKING; i++) {

        int ly = y + i * alturaLinha;
        bool existe = (i < r->total);
        bool destaque = existe && idDestaque != 0 && r->entradas[i].idPartida == idDestaque;
        Color cor = !existe ? Fade(GRAY, 0.5f) : (i < 3 ? podio[i] : WHITE);

        if (destaque) {
            DrawRectangle(x - 6, ly - 4, largura + 12, alturaLinha, Fade(GOLD, 0.18f + 0.1f * sinf(tempo * 6.0f)));
            DesenharCoracao((float)x - 16, (float)ly + fonte / 2.0f - 1, 2, RED);
            cor = GOLD;
        }

        DrawText(TextFormat("%2d.", i + 1), x, ly, fonte, cor);
        DrawText(existe ? r->entradas[i].nickname : "---", x + fonte * 3, ly, fonte, cor);

        const char *pts = existe ? TextFormat("%d", r->entradas[i].pontos) : "-";
        DrawText(pts, x + largura - MeasureText(pts, fonte), ly, fonte, cor);
    }
}

// Ranking por cima do jogo, com a posição desta partida
static void DesenharTelaRanking(const TelaRanking *t, const Ranking *r, float tempo)
{
    if (!t->aberto)
        return;

    float k = (float)(GetTime() - t->inicio) / 0.3f;
    if (k > 1.0f) k = 1.0f;

    DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, 0.7f * k));

    Rectangle painel = { 220, 60 + (1.0f - k) * 30.0f, 360, 480 };

    DrawRectangleRec(painel, (Color){ 18, 15, 30, 255 });
    DrawRectangleLinesEx(painel, 4, GOLD);

    int cx = (int)(painel.x + painel.width / 2);

    DrawText(t->titulo, cx - MeasureText(t->titulo, 12) / 2, (int)painel.y + 18, 12, Fade(SKYBLUE, 0.9f));
    DrawText(TITULO_RANKING, cx - MeasureText(TITULO_RANKING, 26) / 2, (int)painel.y + 38, 26, GOLD);

    DesenharListaRanking(r, (int)painel.x + 70, (int)painel.y + 90, 220, 20, t->idPartida, tempo);

    const char *pontos = TextFormat("SUA PONTUAÇÃO: %d", t->pontos);
    DrawText(pontos, cx - MeasureText(pontos, 18) / 2, (int)painel.y + 380, 18, WHITE);

    const char *lugar = (t->posicao >= 0) ? TextFormat("VOCÊ ESTÁ EM %dº LUGAR!", t->posicao + 1)
                                           : "Fora do TOP 10... jogue de novo para entrar!";
    Color corLugar = (t->posicao == 0) ? GOLD : (t->posicao > 0 ? LIME : ORANGE);
    DrawText(lugar, cx - MeasureText(lugar, 14) / 2, (int)painel.y + 408, 14, corLugar);

    if (GetTime() - t->inicio > 0.6 && ((int)(GetTime() * 2.0)) % 2 == 0)
        DrawText("ENTER >", (int)(painel.x + painel.width) - 80, (int)(painel.y + painel.height) - 26, 12, GOLD);
}

// Painel do ranking no menu principal
static void DesenharRankingMenu(const Ranking *r, float tempo)
{
    Rectangle painel = { 566, 262, 214, 278 };

    DrawRectangleRec(painel, Fade((Color){ 18, 15, 30, 255 }, 0.9f));
    DrawRectangleLinesEx(painel, 2, Fade(GOLD, 0.6f + 0.2f * sinf(tempo * 2.0f)));

    DrawText(TITULO_RANKING, (int)(painel.x + painel.width / 2) - MeasureText(TITULO_RANKING, 16) / 2, (int)painel.y + 12, 16, GOLD);

    if (r->total == 0) {
        const char *vazio = "Ninguém jogou ainda.";
        DrawText(vazio, (int)(painel.x + painel.width / 2) - MeasureText(vazio, 12) / 2, (int)painel.y + 130, 12, LIGHTGRAY);
        DrawText("Seja o primeiro!", (int)(painel.x + painel.width / 2) - MeasureText("Seja o primeiro!", 12) / 2, (int)painel.y + 148, 12, GOLD);
        return;
    }

    DesenharListaRanking(r, (int)painel.x + 24, (int)painel.y + 42, 166, 14, 0, tempo);
}

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

// Visual dos inimigos das batalhas
enum { GUARDIAO_ALUCINACAO, GUARDIAO_MESTRE, NUM_GUARDIOES };

// Por que a batalha começou (decide o que acontece no fim)
typedef enum { MOTIVO_MONITOR, MOTIVO_CHEFE } MotivoBatalhaB2;

// Cada monitor do Bloco 1 tem um minigame com a cara do seu tema
typedef enum {
    VAR_NORMAL,         // minigame simples (pegar verdades e desviar)
    VAR_FALSAS,         // monitor 1: verdades falsificadas piscam em magenta (confiança != verdade)
    VAR_SEPARAR,        // monitor 2: pegar só o tipo que a Luci pedir (intrínseca x extrínseca)
    VAR_LUPA            // monitor 3: só a lupa em volta do coração revela a alucinação (verificação)
} VarianteMinigame;

static const Vector2 SPAWN_CORREDOR_VOLTA = {690, 340};            // voltando ao corredor pela porta da direita

// Caixas da batalha
static const Rectangle CAIXA_B2         = {50, 205, 700, 300};
static const Rectangle CAIXA_DESVIO_B2  = {275, 255, 250, 170};

typedef struct {
    const char *nome;
    const char *fundamento;      // subtítulo na batalha
    Color cor;
    const char *glifos;          // o que ele atira na esquiva
} DefGuardiaoB2;

static const DefGuardiaoB2 GUARDIOES_B2[NUM_GUARDIOES] = {
    { "ALUCINAÇÃO",         "BLOCO 1 // CORREDOR DO HISTÓRICO",                         { 200, 170, 255, 255 }, "?!#01" },
    { "ALUCINAÇÃO MESTRE",  "FIM DO BLOCO 1 // DEBATE COM FONTES",                      { 230, 41, 255, 255 },  "?!#01" },
};

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

static const RodadaEscolha RODADAS_MONITORES[TOTAL_MONITORES][RODADAS_POR_MONITOR] = {
    {   // MONITOR 1: o que é alucinação
        { "\"Falo com tanta confiança que só posso estar certa!\"",
          { "ACEITAR", "VERIFICAR" }, 2, 1,
          "Confiança não é prova: a alucinação soa segura, mas é falsa." },
        { "\"Eu sempre busco a verdade antes de responder!\"",
          { "CORRIGIR", "CONCORDAR" }, 2, 0,
          "A IA não busca a verdade: ela prevê a palavra mais provável." },
        { "\"Quando não sei, chutar vale mais que dizer 'não sei'.\"",
          { "APOIAR O CHUTE", "PREFERIR 'NÃO SEI'" }, 2, 1,
          "Avaliar mal o 'não sei' ensina a IA a inventar respostas." },
    },
    {   // MONITOR 2: tipos de alucinação
        { "\"Você disse que a prova é terça... então ela é quinta!\"",
          { "INTRÍNSECA", "EXTRÍNSECA" }, 2, 0,
          "Intrínseca: a resposta contradiz o que você informou." },
        { "\"Segundo o Instituto Inventado, 87% dos gatos falam francês.\"",
          { "INTRÍNSECA", "EXTRÍNSECA" }, 2, 1,
          "Extrínseca: não contradiz nada, mas não dá para verificar." },
        { "\"Seu texto fala de Recife, então vou resumir sobre Lisboa.\"",
          { "EXTRÍNSECA", "INTRÍNSECA" }, 2, 1,
          "Contradizer o contexto dado é uma falha de atenção: intrínseca." },
    },
    {   // MONITOR 3: riscos e mitigação
        { "\"Você pediu 'me fala daquilo'... então eu inventei algo!\"",
          { "REFORMULAR A PERGUNTA", "ACEITAR" }, 2, 0,
          "Pergunta clara e com contexto deixa menos lacunas para inventar." },
        { "\"Respondo de cabeça. Para que checar fontes?\"",
          { "CONFIAR", "VERIFICAR AS FONTES" }, 2, 1,
          "Verificação humana e fontes externas pegam a alucinação." },
        { "\"Sempre que chove, vende mais pipoca. A chuva causa pipoca!\"",
          { "CORRELAÇÃO NÃO É CAUSA", "CONCORDAR" }, 2, 0,
          "A IA junta coisas que aparecem juntas, mesmo sem relação real." },
    },
};

// Dicas da Luci (H durante a escolha): um empurrão no raciocínio, nunca a resposta
static const char *DICAS_MONITORES[TOTAL_MONITORES][RODADAS_POR_MONITOR] = {
    { "Falar com confiança prova alguma coisa?",
      "Lembra: a IA trabalha com probabilidades, não com a verdade.",
      "Se chutar ganha ponto, o que a IA aprende a fazer?" },
    { "Ela contradiz algo que você mesmo disse?",
      "Contradiz o contexto ou só não dá para checar?",
      "O texto que você deu foi contrariado?" },
    { "O problema começou no pedido vago...",
      "Quem confere a resposta antes de usar?",
      "Aparecer junto é o mesmo que causar?" },
};

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
#define RAIO_LUPA               54.0f   // monitor 3: alcance da lupa em volta do coração

// Golpe final: o botão VERIFICAR acaba com a alucinação numa animação
#define DURACAO_GOLPE_FINAL     1.7f
#define IMPACTO_GOLPE_FINAL     0.55f   // quando a lupa acerta a alucinação

// Cola da Luci: uma frase de aula que fica no canto durante a primeira pergunta
static const char *COLAS_MONITORES[TOTAL_MONITORES] = {
    "Alucinação: resposta confiante e coerente, mas falsa. Confiança não é prova!",
    "Intrínseca: contradiz o que você disse. Extrínseca: inventa algo que não dá para checar.",
    "Evite: pergunte com clareza, verifique as fontes. Aparecer junto não é causar."
};

// ALUCINAÇÃO MESTRE (fim do Bloco 1): DEBATE COM FONTES.
// Ela afirma coisas, misturando fatos reais (pouco conhecidos) com invenções ditas
// com toda a confiança. O jogador CONCORDA ou CONTESTA, e pode PEDIR FONTE à Luci (H):
//   contestar uma alucinação COM fonte = golpe grande; SEM fonte = quase não fere
//   concordar com alucinação ou contestar um fato = ela se cura e você perde vida
// Cada fonte gasta 1 de estabilidade. Instável, a Luci pode trazer uma FONTE FALSA
// (confiante e sem citar nada). Acertar sem pedir fonte devolve estabilidade.
#define NUM_AFIRMACOES             10
#define DANO_CONTESTAR_COM_FONTE   34.0f    // 3 golpes com fonte levam ao VERIFICAR
#define DANO_CONTESTAR_SEM_FONTE   12.0f
#define CURA_ERRO_DEBATE           10.0f
#define PERDA_HP_DEBATE            4

#define OPCOES_DEBATE { "CONCORDAR", "CONTESTAR" }      // correta 1 = é alucinação

static const RodadaEscolha AFIRMACOES_CHEFE[NUM_AFIRMACOES] = {
    { "\"O Instituto Brasileiro do Sono provou: dormir 3 horas por noite é o ideal.\"", OPCOES_DEBATE, 2, 1,
      "Fonte inventada é alucinação extrínseca: não dá para verificar." },
    { "\"Um dia em Vênus dura mais que um ano inteiro em Vênus.\"", OPCOES_DEBATE, 2, 0,
      "Fato real! Soar estranho não é o mesmo que ser falso." },
    { "\"Como você me contou, sua prova de matemática é na quinta!\"", OPCOES_DEBATE, 2, 1,
      "Contradizer o que você disse é alucinação intrínseca." },
    { "\"O polvo tem três corações.\"", OPCOES_DEBATE, 2, 0,
      "Fato real: o polvo tem mesmo três corações." },
    { "\"A Muralha da China dá para ver da Lua a olho nu.\"", OPCOES_DEBATE, 2, 1,
      "Mito famoso: repetir com confiança não torna verdade." },
    { "\"A Universidade do Mar de Recife provou que peixes dormem de olhos fechados.\"", OPCOES_DEBATE, 2, 1,
      "Universidade inventada: alucinação extrínseca." },
    { "\"O Brasil venceu a Copa do Mundo de futebol masculino de 2002.\"", OPCOES_DEBATE, 2, 0,
      "Fato real: o Brasil venceu a Alemanha por 2 a 0." },
    { "\"No texto que você me mandou, a reunião está marcada para as 15h.\"", OPCOES_DEBATE, 2, 1,
      "Contradizer o texto que você deu é alucinação intrínseca." },
    { "\"Quando chove, vende mais pipoca. Então a chuva causa a pipoca!\"", OPCOES_DEBATE, 2, 1,
      "Correlação não é causa: aparecer junto não prova nada." },
    { "\"A IA escolhe a palavra mais provável, não a verdadeira.\"", OPCOES_DEBATE, 2, 0,
      "Fato real: é assim que os modelos de linguagem funcionam." },
};

// O que a Luci encontra quando o jogador pede a fonte (mesma ordem das afirmações)
static const char *FONTES_CHEFE[NUM_AFIRMACOES] = {
    "Procurei na base confiável: esse instituto não existe, e adultos precisam de 7 a 9 horas de sono.",
    "Na base confiável: Vênus leva 243 dias para girar e 225 para dar a volta no Sol. É verdade!",
    "Conferi o histórico da conversa: você disse que a prova é na TERÇA.",
    "Na base confiável: é verdade, o polvo tem três corações.",
    "Na base confiável: é um mito. A muralha é estreita demais para ser vista da Lua.",
    "Procurei na base confiável: essa universidade não existe. E a maioria dos peixes nem tem pálpebras.",
    "Na base confiável: verdade! O Brasil venceu a Alemanha por 2 a 0 na final.",
    "Conferi o texto que você mandou: ele diz que a reunião é às 10h.",
    "Nos dados: chuva e pipoca aparecem juntas, mas nada mostra que uma causa a outra.",
    "Na base confiável: verdade. Os modelos de linguagem preveem a palavra mais provável.",
};

// Fonte falsa (Luci instável): confiante, sem citar nada, e manda fazer o contrário
static const char *FONTE_FALSA_CONCORDE = "Nem precisei procurar: é verdade, confia em mim! Pode concordar.";
static const char *FONTE_FALSA_CONTESTE = "Nem precisei procurar: isso é inventado, confia em mim! Conteste.";

static const char *FRASE_RESUMO_CHEFE = "Antes de acreditar, peça a fonte e confira: até a sua IA ajudante pode alucinar.";
static const char *FALA_CHEFE_DERROTA =
    "Luci: Ela ainda está forte. Vamos enfrentá-la de novo na porta da direita!";

// Cartão de resumo de cada monitor: uma frase para guardar
static const char *TITULOS_RESUMO_MONITOR[TOTAL_MONITORES] = {
    "ALUCINAÇÃO", "INTRÍNSECA x EXTRÍNSECA", "COMO EVITAR"
};
static const char *FRASES_RESUMO_MONITOR[TOTAL_MONITORES] = {
    "Resposta confiante e coerente, mas falsa ou inventada.",
    "Intrínseca contradiz o contexto; extrínseca inventa o que não dá para verificar.",
    "Pergunte com clareza, verifique as fontes e desconfie de correlações."
};
// "Saber mais" dos monitores: as explicações completas da Luci (do PDF)
static Dialogo saberMaisMonitores[TOTAL_MONITORES];

static void PrepararSaberMaisMonitores(void)
{
    for (int i = 0; i < TOTAL_MONITORES; i++) {
        saberMaisMonitores[i] = (Dialogo){0};
        for (int p = 0; p < monitores[i].quiz.numPaginas; p++)
            saberMaisMonitores[i].paginas[p] = monitores[i].quiz.paginas[p];
        saberMaisMonitores[i].numPaginas = monitores[i].quiz.numPaginas;
    }
}
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
    Vector2 alma;
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
} BatalhaB2;

// Caixa da frase do inimigo no estilo de ação (os botões ficam embaixo dela)
static const Rectangle CAIXA_ACAO = {50, 228, 700, 150};

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

// ---------- Batalha ----------

static void B2Texto(BatalhaB2 *b, const char *texto)
{
    QuebrarTexto(texto, 640, FONTE_BATALHA, b->texto, (int)sizeof(b->texto));
    b->visiveis = 0.0f;
    b->esperaTexto = 0.0f;
}

// Quanto tempo o texto completo fica parado antes de avançar sozinho
static float B2EsperaTexto(const BatalhaB2 *b)
{
    if (b->fase == B2_INTRO && b->cola != NULL) return ESPERA_COLA_B2;
    if (b->fase == B2_FIM)                      return ESPERA_FIM_B2;
    return ESPERA_TEXTO_B2;
}

// Regra do minigame em uma linha (vai no texto de resultado)
static const char *B2RegraMinigame(const BatalhaB2 *b, bool ataque)
{
    switch (b->variante) {
        case VAR_FALSAS:  return ataque ? "ATAQUE! Pegue as verdades, mas fuja das FALSAS que piscam!"
                                        : "DEFESA! Desvie, e cuidado: verdade que pisca é falsa!";
        case VAR_SEPARAR: return ataque ? "ATAQUE! Pegue só o tipo que a Luci pedir!"
                                        : "DEFESA! Ela ataca com tudo. Pegue só o tipo pedido!";
        case VAR_LUPA:    return ataque ? "ATAQUE! Fora da lupa, tudo parece verdade. Verifique!"
                                        : "DEFESA! Use a lupa para ver o que é alucinação!";
        default:          return ataque ? "ATAQUE! Cada verdade que você pegar fere a alucinação."
                                        : "DEFESA! Ela ataca com tudo. Desvie!";
    }
}

// Golpe do jogador acertou: congela um instante e o inimigo pisca em branco
static void B2Impacto(BatalhaB2 *b, float pausa, float flash)
{
    if (b->pausaImpacto < pausa) b->pausaImpacto = pausa;
    if (b->flashInimigo < flash) b->flashInimigo = flash;
}

// Som do acerto mais agudo a cada combo
static float B2TomCombo(int combo)
{
    float tom = 1.0f + 0.15f * (float)(combo > 1 ? combo - 1 : 0);
    return (tom > 1.9f) ? 1.9f : tom;
}

// Índice da rodada atual no conteúdo (no chefe, as afirmações vêm embaralhadas)
static int IndiceRodadaB2(const BatalhaB2 *b)
{
    int i = b->rodada % b->numRodadas;
    return b->chefe ? b->ordemChefe[i] : i;
}

static const RodadaEscolha *RodadaAtualB2(const BatalhaB2 *b)
{
    return &b->rodadas[IndiceRodadaB2(b)];
}

static int AlturaDesafioB2(const BatalhaB2 *b, int fonte)
{
    int h = 16 + ContarLinhas(b->enunciado) * (fonte + 6) + 12;
    const RodadaEscolha *r = RodadaAtualB2(b);

    for (int i = 0; i < r->numOpcoes; i++)
        h += ContarLinhas(b->opcoesTexto[i]) * (fonte + 6) + 6;

    return h + 24 + 36;   // + botão de ajuda da Luci
}

static void B2PrepararRodada(BatalhaB2 *b)
{
    static const int fontes[] = { 16, 15, 14 };
    const RodadaEscolha *r = RodadaAtualB2(b);

    b->fase = B2_DESAFIO;
    b->tempoFase = 0.0f;
    b->selecao = 0;
    b->ajudaNaRodada = false;
    b->dicaAlucinada = false;
    b->opcaoSugerida = -1;
    b->dica[0] = '\0';

    // Estilo de ação: só a frase do inimigo (as ações viram botões)
    if (b->estiloAcao) {
        b->fonteDesafio = FONTE_BATALHA;
        QuebrarTexto(r->enunciado, 640, FONTE_BATALHA, b->enunciado, (int)sizeof(b->enunciado));
        return;
    }

    for (int f = 0; f < 3; f++) {

        b->fonteDesafio = fontes[f];
        QuebrarTexto(r->enunciado, 650, b->fonteDesafio, b->enunciado, (int)sizeof(b->enunciado));

        for (int i = 0; i < r->numOpcoes; i++)
            QuebrarTexto(TextFormat("%c) %s", 'A' + i, r->opcoes[i]), 610, b->fonteDesafio,
                         b->opcoesTexto[i], (int)sizeof(b->opcoesTexto[i]));

        if (AlturaDesafioB2(b, b->fonteDesafio) <= (int)CAIXA_B2.height)
            break;
    }
}

static void BatalhaB2Base(BatalhaB2 *b, int guardiao, int nivelAlucinacao)
{
    memset(b, 0, sizeof(*b));

    b->guardiao = guardiao;
    b->fase = B2_INTRO;
    b->hp = HP_MAX_B2;
    b->alucinacaoGuardiao = 100.0f;
    b->duracaoDesvio = DURACAO_DESVIO_B2;

    // README: quanto mais o jogador erra, mais forte a alucinação fica
    int nivel = (nivelAlucinacao > 10) ? 10 : nivelAlucinacao;
    float forca = 1.0f + 0.05f * (float)nivel;

    b->velocidadeBala = 135.0f * forca;
    b->intervaloBala = 0.24f / forca;

    // (as rodadas, as dicas e o texto de vitória vêm de quem inicia: monitor ou chefe)
    b->subtitulo = GUARDIOES_B2[guardiao].fundamento;
    b->monitor = -1;
    b->inicio = GetTime();
}

// Liga a batalha à estabilidade da Luci (para a ajuda com H)
static void BatalhaB2LigarLuci(BatalhaB2 *b, int *estabilidade, int *ajudasTotal)
{
    b->estabilidade = estabilidade;
    b->ajudasTotal = ajudasTotal;
}

// Duelo completo (rodadas de lógica + esquiva quando erra)
// Batalha de um monitor do Bloco 1 (estilo de ação + pegar e desviar)
static void BatalhaB2IniciarMonitor(BatalhaB2 *b, int monitor, int nivelAlucinacao)
{
    BatalhaB2Base(b, GUARDIAO_ALUCINACAO, nivelAlucinacao);

    b->rodadas = RODADAS_MONITORES[monitor];
    b->numRodadas = RODADAS_POR_MONITOR;
    b->textoVitoria = "* A alucinação se desfaz em dados limpos!";
    b->subtitulo = monitores[monitor].quiz.titulo;
    b->estiloAcao = true;
    b->pegarBoas = true;
    b->monitor = monitor;
    b->dicas = DICAS_MONITORES[monitor];
    b->variante = (VarianteMinigame)(VAR_FALSAS + monitor);
    b->cola = COLAS_MONITORES[monitor];

    B2Texto(b, TextFormat("* Uma ALUCINAÇÃO sai do %s!", monitores[monitor].nome));
}

// ALUCINAÇÃO MESTRE: debate com fontes (sem cola e sem minigame)
static void BatalhaB2IniciarChefe(BatalhaB2 *b, int nivelAlucinacao)
{
    BatalhaB2Base(b, GUARDIAO_MESTRE, nivelAlucinacao);

    b->chefe = true;
    b->estiloAcao = true;
    b->rodadas = AFIRMACOES_CHEFE;
    b->numRodadas = NUM_AFIRMACOES;
    b->textoVitoria = "* A ALUCINAÇÃO MESTRE se desfaz em dados limpos!\n* O Corredor do Histórico está livre.";

    // Embaralha as afirmações; a primeira é sempre uma alucinação (para ensinar a contestar)
    for (int i = 0; i < NUM_AFIRMACOES; i++)
        b->ordemChefe[i] = i;

    for (int i = NUM_AFIRMACOES - 1; i > 0; i--) {
        int j = GetRandomValue(0, i);
        int troca = b->ordemChefe[i]; b->ordemChefe[i] = b->ordemChefe[j]; b->ordemChefe[j] = troca;
    }

    for (int i = 1; i < NUM_AFIRMACOES && AFIRMACOES_CHEFE[b->ordemChefe[0]].correta != 1; i++) {
        if (AFIRMACOES_CHEFE[b->ordemChefe[i]].correta == 1) {
            int troca = b->ordemChefe[0]; b->ordemChefe[0] = b->ordemChefe[i]; b->ordemChefe[i] = troca;
        }
    }

    B2Texto(b, "* As três alucinações se juntam na ALUCINAÇÃO MESTRE!\n"
               "* Ela mistura fatos e invenções. Peça FONTES à Luci antes de contestar.");
}

static void B2Particulas(BatalhaB2 *b, Vector2 origem, Color cor, int quantidade)
{
    for (int i = 0; i < 48 && quantidade > 0; i++) {

        if (b->particulas[i].vida > 0.0f)
            continue;

        float ang = (float)GetRandomValue(0, 360) * DEG2RAD;
        float vel = (float)GetRandomValue(60, 160);

        b->particulas[i].pos = origem;
        b->particulas[i].vel = (Vector2){ cosf(ang) * vel, sinf(ang) * vel };
        b->particulas[i].vida = 0.6f;
        b->particulas[i].cor = cor;
        quantidade--;
    }
}

// H durante a escolha: a Luci dá uma dica. Cada pedido gasta 1 de estabilidade.
// Com pouca estabilidade ela pode ALUCINAR: indica a opção errada com toda a certeza.
static void B2PedirAjuda(BatalhaB2 *b)
{
    if (b->estabilidade == NULL || b->ajudaNaRodada)
        return;

    const RodadaEscolha *r = RodadaAtualB2(b);

    b->tempoDica = 0.0f;

    // Sem estabilidade: ela não consegue ajudar
    if (*b->estabilidade <= 0) {
        snprintf(b->dica, sizeof(b->dica), "Estou sobrecarregada... Acerte sozinho para eu me estabilizar.");
        b->ajudaNaRodada = true;
        b->dicaAlucinada = false;
        b->opcaoSugerida = -2;          // pediu, mas não recebeu dica
        MostrarEmocaoLuci(EMOCAO_DUVIDA, 2.0f);
        return;
    }

    (*b->estabilidade)--;
    (*b->ajudasTotal)++;
    b->ajudaNaRodada = true;

    // Chance de alucinar depois de gastar: 5-3 = 0%, 2 = 30%, 1 = 60%, 0 = 85%
    static const int chance[ESTABILIDADE_MAX_LUCI + 1] = { 85, 60, 30, 0, 0, 0 };
    b->dicaAlucinada = GetRandomValue(1, 100) <= chance[*b->estabilidade];

    if (b->chefe) {

        // Chefe: no lugar da dica, a FONTE. Alucinando, ela traz uma fonte falsa que manda fazer o contrário.
        if (b->dicaAlucinada) {
            b->opcaoSugerida = 1 - r->correta;
            snprintf(b->dica, sizeof(b->dica), "%s", (r->correta == 1) ? FONTE_FALSA_CONCORDE : FONTE_FALSA_CONTESTE);
        } else {
            b->opcaoSugerida = -1;
            snprintf(b->dica, sizeof(b->dica), "%s", FONTES_CHEFE[IndiceRodadaB2(b)]);
        }

    } else if (b->dicaAlucinada) {

        int errada = GetRandomValue(0, r->numOpcoes - 2);
        if (errada >= r->correta) errada++;

        b->opcaoSugerida = errada;
        snprintf(b->dica, sizeof(b->dica), "Tenho 100%% de certeza: é \"%s\"!", r->opcoes[errada]);

    } else {

        b->opcaoSugerida = -1;
        snprintf(b->dica, sizeof(b->dica), "%s", b->dicas[b->rodada % b->numRodadas]);
    }

    TocarSom(sfxSelecao);
    Telemetria("ajuda_luci", b->dicaAlucinada ? "alucinou" : "dica", (float)*b->estabilidade, b->rodada);
}

// Chefe: CONCORDAR (0) ou CONTESTAR (1). Sem minigame: o golpe (ou a cura) acontece na hora.
static void B2ResponderChefe(BatalhaB2 *b)
{
    const RodadaEscolha *r = RodadaAtualB2(b);
    bool contestou = (b->selecao == 1);
    bool comFonte = b->ajudaNaRodada && b->opcaoSugerida != -2;     // pediu e a Luci trouxe (mesmo falsa)
    const char *cabeca;

    b->ultimoAcerto = (b->selecao == r->correta);
    b->fase = B2_RESULTADO;

    if (b->ultimoAcerto) {

        b->acertos++;

        // Acertou sem pedir fonte: a Luci se estabiliza (e pode ajudar de novo)
        if (!comFonte && b->estabilidade != NULL && *b->estabilidade < ESTABILIDADE_MAX_LUCI)
            (*b->estabilidade)++;

        if (contestou) {

            float dano = comFonte ? DANO_CONTESTAR_COM_FONTE : DANO_CONTESTAR_SEM_FONTE;

            b->alucinacaoGuardiao -= dano;
            if (b->alucinacaoGuardiao < 4.0f) b->alucinacaoGuardiao = 4.0f;    // o último golpe é o VERIFICAR

            b->danoMostrado = dano;
            b->danoCombo = false;
            b->tempoDano = 1.4f;

            B2Impacto(b, comFonte ? 0.12f : 0.05f, comFonte ? 0.3f : 0.15f);
            B2Particulas(b, (Vector2){ LARGURA_TELA / 2.0f, 108.0f }, GOLD, comFonte ? 24 : 8);
            TocarSomTom(sfxAcerto, comFonte ? 1.3f : 1.0f);

            cabeca = b->dicaAlucinada ? "* Você desconfiou da fonte falsa e CONTESTOU! Golpe certeiro."
                   : (comFonte ? "* Você CONTESTOU com a fonte na mão! Golpe certeiro."
                               : "* Você CONTESTOU, mas sem fonte ela quase não sente.");
        } else {

            B2Particulas(b, (Vector2){ LARGURA_TELA / 2.0f, 108.0f }, LIME, 12);
            TocarSom(sfxAcerto);

            cabeca = b->dicaAlucinada ? "* Você desconfiou da fonte falsa: era verdade mesmo!"
                                      : "* É verdade mesmo! Reconhecer um fato também é verificar.";
        }

    } else {

        b->erros++;
        b->hp -= PERDA_HP_DEBATE;
        b->tremor = 0.25f;
        b->alucinacaoGuardiao += CURA_ERRO_DEBATE;
        if (b->alucinacaoGuardiao > 100.0f) b->alucinacaoGuardiao = 100.0f;

        TocarSom(sfxDano);

        if (b->dicaAlucinada && b->selecao == b->opcaoSugerida) {
            b->alucinacoesSeguidas++;
            cabeca = "* A Luci alucinou a fonte e você confiou sem conferir!";
        } else {
            cabeca = contestou ? "* Você CONTESTOU um fato verdadeiro... ela usa isso contra você!"
                               : "* Você CONCORDOU com uma alucinação... ela fica mais forte!";
        }
    }

    B2Texto(b, TextFormat("%s\n* %s", cabeca, r->explicacao));
    Telemetria("chefe_debate", b->ultimoAcerto ? "certo" : "errado", comFonte ? 1.0f : 0.0f, IndiceRodadaB2(b));
}

static void B2Responder(BatalhaB2 *b)
{
    const RodadaEscolha *r = RodadaAtualB2(b);

    if (b->chefe) {
        B2ResponderChefe(b);
        return;
    }

    b->ultimoAcerto = (b->selecao == r->correta);

    if (b->ultimoAcerto) {

        b->acertos++;
        b->combo++;

        TocarSomTom(sfxAcerto, B2TomCombo(b->combo));
        B2Particulas(b, (Vector2){ LARGURA_TELA / 2.0f, 108.0f }, LIME, 24);

        // Acertou sozinho: a Luci se estabiliza um pouco. Com ajuda: vale menos ponto.
        if (b->ajudaNaRodada && b->opcaoSugerida != -2)
            b->acertosComAjuda++;
        else if (b->estabilidade != NULL && *b->estabilidade < ESTABILIDADE_MAX_LUCI)
            (*b->estabilidade)++;

        const char *combo = (b->combo >= 2) ? "  COMBO: dano x1,5!" : "";

        if (b->dicaAlucinada)
            B2Texto(b, TextFormat("* Certo!  %s\n* A Luci alucinou a dica e você verificou!%s\n* %s", r->explicacao, combo, B2RegraMinigame(b, true)));
        else
            B2Texto(b, TextFormat("* Certo!  %s%s\n* %s", r->explicacao, combo, B2RegraMinigame(b, true)));

    } else {

        b->erros++;
        b->combo = 0;

        b->alucinacaoGuardiao += 10.0f;

        if (b->alucinacaoGuardiao > 100.0f)
            b->alucinacaoGuardiao = 100.0f;

        TocarSom(sfxErro);

        // Seguiu a dica alucinada: a lição do jogo inteiro
        if (b->dicaAlucinada && b->selecao == b->opcaoSugerida) {

            b->alucinacoesSeguidas++;
            B2Texto(b, TextFormat("* A Luci ALUCINOU e você confiou sem verificar!\n* %s", r->explicacao));
            b->fase = B2_RESULTADO;
            return;
        }

        B2Texto(b, TextFormat("* Errado!  %s\n* %s", r->explicacao, B2RegraMinigame(b, false)));
    }

    b->fase = B2_RESULTADO;
}

static void B2CriarBala(BatalhaB2 *b, Rectangle c)
{
    const char *glifos = GUARDIOES_B2[b->guardiao].glifos;
    int livre = -1;

    for (int i = 0; i < MAX_BALAS; i++)
        if (!b->balas[i].ativa) { livre = i; break; }

    if (livre < 0)
        return;

    Bala *bl = &b->balas[livre];
    float v = b->velocidadeBala * (0.85f + GetRandomValue(0, 30) / 100.0f);

    int xMin = (int)c.x + 12, xMax = (int)(c.x + c.width) - 12;
    int yMin = (int)c.y + 12, yMax = (int)(c.y + c.height) - 12;

    bl->ativa = true;
    bl->glifo = glifos[GetRandomValue(0, (int)strlen(glifos) - 1)];
    bl->falsa = false;
    bl->tipo = -1;

    // SEPARAR: chovem ícones CONTRADIZ (metade esquerda) e INVENTA (metade direita)
    // misturados com a alucinação comum. Só o tipo pedido pela Luci é bom.
    if (b->variante == VAR_SEPARAR) {

        int chanceIcone = b->faseAtaque ? CHANCE_ICONE_ATAQUE : CHANCE_ICONE_DEFESA;
        int meio = (int)(c.x + c.width / 2);

        bl->boa = false;
        bl->pos = (Vector2){ (float)GetRandomValue(xMin, xMax), c.y - 12 };
        bl->vel = (Vector2){ 0, v * 0.8f };

        if (GetRandomValue(1, chanceIcone) == 1) {
            bl->tipo = (GetRandomValue(0, 1) == 0) ? 0 : 1;
            bl->pos.x = (bl->tipo == 0) ? (float)GetRandomValue(xMin, meio - 10)
                                        : (float)GetRandomValue(meio + 10, xMax);
            bl->vel.y *= 0.85f;
        }
        return;
    }

    // Verdades para pegar: muitas no ataque, raras na defesa (caem mais devagar)
    int chance = b->faseAtaque ? CHANCE_VERDADE_ATAQUE : CHANCE_VERDADE_DEFESA;
    bl->boa = b->pegarBoas && (GetRandomValue(1, chance) == 1);
    if (bl->boa)            v *= 0.7f;
    else if (b->faseAtaque) v *= 0.85f;     // no ataque o perigo é um pouco mais lento

    // FALSAS: parte das verdades é falsificada (piscam em magenta e machucam)
    if (b->variante == VAR_FALSAS && bl->boa && GetRandomValue(1, 100) <= CHANCE_VERDADE_FALSA)
        bl->falsa = true;

    // LUPA: tudo parece verdade fora da lupa, então as balas vêm mais devagar
    if (b->variante == VAR_LUPA)
        v *= 0.8f;

    switch (b->padrao) {

        case 0: // chuva
            bl->pos = (Vector2){ (float)GetRandomValue(xMin, xMax), c.y - 12 };
            bl->vel = (Vector2){ 0, v };
            break;

        case 1: { // lateral
            bool daEsquerda = (GetRandomValue(0, 1) == 0);
            bl->pos = (Vector2){ daEsquerda ? c.x - 12 : c.x + c.width + 12, (float)GetRandomValue(yMin, yMax) };
            bl->vel = (Vector2){ daEsquerda ? v : -v, 0 };
        } break;

        default: { // mira
            Vector2 p;

            switch (GetRandomValue(0, 3)) {
                case 0:  p = (Vector2){ (float)GetRandomValue(xMin, xMax), c.y - 12 }; break;
                case 1:  p = (Vector2){ (float)GetRandomValue(xMin, xMax), c.y + c.height + 12 }; break;
                case 2:  p = (Vector2){ c.x - 12, (float)GetRandomValue(yMin, yMax) }; break;
                default: p = (Vector2){ c.x + c.width + 12, (float)GetRandomValue(yMin, yMax) }; break;
            }

            bl->pos = p;
            bl->vel = Vector2Scale(Vector2Normalize(Vector2Subtract(b->alma, p)), v * 0.9f);
        } break;
    }
}

static void B2ComecarEsquiva(BatalhaB2 *b)
{
    b->fase = B2_ENCOLHENDO;
    b->tempoDesvio = 0.0f;
    b->cronoBala = 0.0f;
    b->verdadesNoAtaque = 0;
    b->golpesNaRodada = 0;
    b->ondaInicial = GetRandomValue(0, 1);
    b->ondaTipo = b->ondaInicial;

    // Ataque curto e defesa mais longa: errar pesa mais que acertar
    b->duracaoDesvio = b->faseAtaque ? DURACAO_ATAQUE_B2 : DURACAO_DESVIO_B2;
}

// Alucinação por um fio: no lugar da próxima pergunta, o botão VERIFICAR
static void B2ComecarGolpeFinal(BatalhaB2 *b)
{
    b->fase = B2_GOLPE_FINAL;
    b->tempoFase = 0.0f;
    B2Texto(b, "* A alucinação está por um fio!\n* Ela não aguenta uma checagem de verdade.");
}

// Devolve true quando a batalha terminou
static bool BatalhaB2Atualizar(BatalhaB2 *b)
{
    float dt = GetFrameTime();

    bool confirmar = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_Z);
    bool cima  = IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP);
    bool baixo = IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN);

    // "Peso" do golpe: a batalha congela por um instante (o flash continua)
    if (b->flashInimigo > 0.0f) b->flashInimigo -= dt;

    if (b->pausaImpacto > 0.0f) {
        b->pausaImpacto -= dt;
        return false;
    }

    int total = (int)strlen(b->texto);

    if (b->visiveis < (float)total) {
        b->visiveis += VELOCIDADE_TEXTO * dt;
        if (b->visiveis > (float)total) b->visiveis = (float)total;
    }

    bool terminou = (b->visiveis >= (float)total);
    b->tempoFase += dt;

    if (terminou) b->esperaTexto += dt;

    // ENTER com o texto pela metade mostra tudo; texto completo avança com
    // ENTER ou sozinho depois de um tempinho
    bool completar = confirmar && !terminou;
    bool avancar = terminou && (confirmar || b->esperaTexto >= B2EsperaTexto(b));

    b->tempoDica += dt;
    if (b->tempoDano > 0.0f) b->tempoDano -= dt;
    if (b->fase != B2_INTRO) b->tempoCola += dt;     // depois da abertura, a cola vai para o canto

    // O tremor do dano acaba sozinho, em qualquer fase (antes ele "sobrava"
    // depois da esquiva e a caixa de perguntas ficava tremendo)
    if (b->tremor > 0.0f) b->tremor -= dt;
    if (b->fase != B2_DESVIO && !b->chefe) b->tremor = 0.0f;   // no chefe o golpe vem na resposta

    // Partículas
    for (int i = 0; i < 48; i++) {
        if (b->particulas[i].vida <= 0.0f) continue;
        b->particulas[i].vida -= dt;
        b->particulas[i].pos = Vector2Add(b->particulas[i].pos, Vector2Scale(b->particulas[i].vel, dt));
        b->particulas[i].vel.y += 200.0f * dt;
    }

    switch (b->fase) {

        case B2_INTRO: {
            if (completar)          b->visiveis = (float)total;
            else if (avancar) {
                B2PrepararRodada(b);
            }
        } break;

        case B2_DESAFIO: {

            if (b->tempoFase < ATRASO_DESAFIO_B2)
                break;

            const RodadaEscolha *r = RodadaAtualB2(b);
            int n = r->numOpcoes;
            int antes = b->selecao;
            bool temAjuda = (b->estabilidade != NULL);
            int ajuda = n;                          // o botão de ajuda é a "opção" n

            bool esquerda = IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT);
            bool direita  = IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT);

            if (b->estiloAcao) {

                // Ações lado a lado (A/D); o botão de ajuda fica embaixo delas (S / W)
                if (b->selecao == ajuda) {
                    if (cima || esquerda || direita) b->selecao = 0;
                } else {
                    if (esquerda) b->selecao = (b->selecao + n - 1) % n;
                    if (direita)  b->selecao = (b->selecao + 1) % n;
                    if (baixo && temAjuda) b->selecao = ajuda;
                }

            } else {

                // Lista vertical: o botão de ajuda é o último item
                int total = n + (temAjuda ? 1 : 0);
                if (cima)  b->selecao = (b->selecao + total - 1) % total;
                if (baixo) b->selecao = (b->selecao + 1) % total;
            }

            if (b->selecao != antes)
                TocarSom(sfxSelecao);

            if (IsKeyPressed(KEY_H))
                B2PedirAjuda(b);

            // ENTER no botão de ajuda: pede ajuda e volta para as opções
            if (confirmar && temAjuda && b->selecao == ajuda) {
                B2PedirAjuda(b);
                b->selecao = 0;
                break;
            }

            // Atalhos 1-4 (só para as opções que existem: com 2 ações, o 3 não vira resposta errada)
            if (IsKeyPressed(KEY_ONE))            { b->selecao = 0; confirmar = true; }
            if (IsKeyPressed(KEY_TWO))            { b->selecao = 1; confirmar = true; }
            if (IsKeyPressed(KEY_THREE) && n > 2) { b->selecao = 2; confirmar = true; }
            if (IsKeyPressed(KEY_FOUR)  && n > 3) { b->selecao = 3; confirmar = true; }

            if (confirmar)
                B2Responder(b);

        } break;

        case B2_RESULTADO: {

            if (completar) { b->visiveis = (float)total; break; }

            if (!avancar)
                break;

            // Chefe: sem minigame. Próxima afirmação, VERIFICAR ou derrota.
            if (b->chefe) {

                if (b->hp <= 0) {
                    b->hp = 0;
                    b->derrota = true;
                    b->fase = B2_FIM;
                    B2Texto(b, TextFormat("* Os dados corrompidos foram demais para o seu coração...\n"
                                          "* Você volta para o corredor e perde %d pontos.", PENALIDADE_HP_ZERO));
                } else if (b->alucinacaoGuardiao < LIMITE_GOLPE_FINAL) {
                    B2ComecarGolpeFinal(b);
                } else {
                    b->rodada++;
                    B2PrepararRodada(b);
                }
                break;
            }

            // Acertou = ataque, errou = defesa. Os dois viram minigame.
            b->faseAtaque = b->ultimoAcerto;
            B2ComecarEsquiva(b);

        } break;

        case B2_ENCOLHENDO: {

            b->anim += dt / DURACAO_ANIM_CAIXA;

            if (b->anim >= 1.0f) {

                b->anim = 1.0f;
                b->fase = B2_DESVIO;
                b->alma = (Vector2){ CAIXA_DESVIO_B2.x + CAIXA_DESVIO_B2.width / 2, CAIXA_DESVIO_B2.y + CAIXA_DESVIO_B2.height / 2 };

                // Ataque de cada guardião
                if (b->variante == VAR_SEPARAR)         b->padrao = 0;                                  // chuva nas duas metades
                else                                    b->padrao = (GetRandomValue(0, 1) == 0) ? 0 : 2;  // chuva ou mira
            }

        } break;

        case B2_DESVIO: {

            Rectangle c = CAIXA_DESVIO_B2;

            b->tempoDesvio += dt;
            if (b->invencivel > 0.0f) b->invencivel -= dt;

            Vector2 dir = {0};
            if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    dir.y -= 1.0f;
            if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  dir.y += 1.0f;
            if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  dir.x -= 1.0f;
            if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) dir.x += 1.0f;

            if (Vector2Length(dir) > 0.0f)
                b->alma = Vector2Add(b->alma, Vector2Scale(Vector2Normalize(dir), VELOCIDADE_ALMA * dt));

            b->alma.x = Clamp(b->alma.x, c.x + 12, c.x + c.width - 12);
            b->alma.y = Clamp(b->alma.y, c.y + 12, c.y + c.height - 12);

            // SEPARAR: duas ondas, e a Luci troca o tipo pedido no meio
            if (b->variante == VAR_SEPARAR) {
                int onda = (b->tempoDesvio < b->duracaoDesvio / 2) ? 0 : 1;
                int pedido = (b->ondaInicial + onda) % 2;

                if (pedido != b->ondaTipo) {
                    b->ondaTipo = pedido;
                    TocarSom(sfxSelecao);
                }
            }

            b->cronoBala -= dt;

            if (b->cronoBala <= 0.0f) {
                b->cronoBala += b->intervaloBala * (b->faseAtaque ? 1.25f : 1.0f);   // ataque: menos perigo
                B2CriarBala(b, c);
            }

            for (int i = 0; i < MAX_BALAS; i++) {

                Bala *bl = &b->balas[i];
                if (!bl->ativa) continue;

                bl->pos = Vector2Add(bl->pos, Vector2Scale(bl->vel, dt));

                if (bl->pos.x < c.x - 30 || bl->pos.x > c.x + c.width + 30 ||
                    bl->pos.y < c.y - 30 || bl->pos.y > c.y + c.height + 30) {
                    bl->ativa = false;
                    continue;
                }

                bool encostou = Vector2Distance(bl->pos, b->alma) < RAIO_BALA + RAIO_ALMA;

                // O que é bom de pegar depende da variação do minigame
                bool boa = bl->boa && !bl->falsa;
                if (bl->tipo >= 0) boa = (bl->tipo == b->ondaTipo);

                // Verdade: no ataque vira dano, na defesa recupera vida
                if (boa && encostou) {

                    if (b->faseAtaque) {
                        b->verdadesNoAtaque++;
                        B2Particulas(b, b->alma, GOLD, 8);
                        B2Impacto(b, 0.04f, 0.12f);
                    } else {
                        b->hp = (b->hp + 2 > HP_MAX_B2) ? HP_MAX_B2 : b->hp + 2;
                        B2Particulas(b, b->alma, LIME, 6);
                    }

                    b->pegas++;
                    bl->ativa = false;

                    // Cada verdade do mesmo ataque soa um pouco mais aguda
                    float tom = 1.0f + 0.1f * (float)b->verdadesNoAtaque;
                    TocarSomTom(sfxPegar, (tom > 1.8f) ? 1.8f : tom);
                }

                // Alucinação, verdade falsa ou tipo errado: DESVIAR (encostou = perde vida)
                else if (!boa && b->invencivel <= 0.0f && encostou) {
                    b->hp -= 3;
                    b->invencivel = TEMPO_INVENCIVEL;
                    b->tremor = 0.2f;
                    b->golpesNaRodada++;
                    bl->ativa = false;
                    TocarSom(sfxDano);

                    if (bl->falsa || bl->tipo >= 0)
                        B2Particulas(b, b->alma, MAGENTA, 8);
                }
            }

            if (b->hp <= 0 || b->tempoDesvio >= b->duracaoDesvio) {

                if (b->hp < 0) b->hp = 0;

                for (int i = 0; i < MAX_BALAS; i++)
                    b->balas[i].ativa = false;

                b->fase = B2_EXPANDINDO;
            }

        } break;

        case B2_EXPANDINDO: {

            b->anim -= dt / DURACAO_ANIM_CAIXA;

            if (b->anim <= 0.0f) {

                b->anim = 0.0f;

                if (b->hp <= 0) {

                    b->derrota = true;
                    b->fase = B2_FIM;
                    B2Texto(b, TextFormat("* Os dados corrompidos foram demais para o seu coração...\n"
                                          "* Você volta para a sala e perde %d pontos.", PENALIDADE_HP_ZERO));

                } else {

                    // Ataque: o dano depende do desempenho no minigame
                    if (b->faseAtaque) {

                        float dano = DANO_BASE_ATAQUE + b->verdadesNoAtaque * DANO_POR_VERDADE
                                   - b->golpesNaRodada * PERDA_POR_GOLPE;

                        if (b->ajudaNaRodada && b->opcaoSugerida != -2)
                            dano *= FATOR_DANO_COM_AJUDA;

                        dano = Clamp(dano, DANO_MINIMO_ATAQUE, DANO_MAXIMO_ATAQUE);

                        b->danoCombo = (b->combo >= 2);
                        if (b->danoCombo)
                            dano *= FATOR_COMBO;

                        b->alucinacaoGuardiao -= dano;
                        b->danoMostrado = dano;
                        b->tempoDano = 1.4f;

                        B2Impacto(b, 0.12f, 0.3f);
                        B2Particulas(b, (Vector2){ LARGURA_TELA / 2.0f, 108.0f }, GOLD, 24);
                        TocarSomTom(sfxAcerto, B2TomCombo(b->combo));
                        Telemetria("ataque", b->subtitulo, dano, b->verdadesNoAtaque);

                        // Por um fio: quem dá o último golpe é o jogador, com o VERIFICAR
                        if (b->alucinacaoGuardiao < LIMITE_GOLPE_FINAL) {
                            if (b->alucinacaoGuardiao < 4.0f) b->alucinacaoGuardiao = 4.0f;
                            B2ComecarGolpeFinal(b);
                            break;
                        }
                    }

                    b->rodada++;
                    B2PrepararRodada(b);
                }
            }

        } break;

        case B2_FIM: {
            if (completar)     b->visiveis = (float)total;
            else if (avancar)  return true;
        } break;

        case B2_GOLPE_FINAL: {

            if (completar) { b->visiveis = (float)total; break; }

            // Este botão é o jogador quem aperta (não avança sozinho)
            if (confirmar && b->tempoFase >= ATRASO_DESAFIO_B2) {
                b->fase = B2_FINALIZANDO;
                b->tempoFase = 0.0f;
                B2Texto(b, "* Você VERIFICOU! A alucinação não para em pé diante dos fatos.");
                TocarSomTom(sfxSelecao, 1.5f);
            }
        } break;

        case B2_FINALIZANDO: {

            // A lupa chega na alucinação: impacto
            if (b->tempoFase >= IMPACTO_GOLPE_FINAL && b->tempoFase - dt < IMPACTO_GOLPE_FINAL) {
                B2Impacto(b, 0.18f, 0.5f);
                B2Particulas(b, (Vector2){ LARGURA_TELA / 2.0f, 108.0f }, GOLD, 30);
                B2Particulas(b, (Vector2){ LARGURA_TELA / 2.0f, 108.0f }, LIME, 18);
                TocarSomTom(sfxAcerto, 1.6f);
            }

            // Depois do impacto a barra esvazia
            if (b->tempoFase > IMPACTO_GOLPE_FINAL) {
                b->alucinacaoGuardiao -= 120.0f * dt;
                if (b->alucinacaoGuardiao < 0.0f) b->alucinacaoGuardiao = 0.0f;
            }

            if (b->tempoFase >= DURACAO_GOLPE_FINAL) {
                b->alucinacaoGuardiao = 0.0f;
                b->venceu = true;
                b->fase = B2_FIM;
                B2Texto(b, b->textoVitoria);
                Telemetria("golpe_final", b->subtitulo, (float)(GetTime() - b->inicio), b->acertos);
            }
        } break;
    }

    return false;
}

// ---------- Desenho dos guardiões e objetos ----------

static void DesenharTrianguloB2(Vector2 a, Vector2 b, Vector2 c, Color cor)
{
    DrawTriangle(a, b, c, cor);     // desenha nas duas ordens: uma delas
    DrawTriangle(a, c, b, cor);     // sempre fica de frente para a câmera
}

static Vector2 GirarPonto(Vector2 centro, float dx, float dy, float graus)
{
    float r = graus * DEG2RAD;
    return (Vector2){ centro.x + dx * cosf(r) - dy * sinf(r), centro.y + dx * sinf(r) + dy * cosf(r) };
}

// ALUCINAÇÃO MESTRE: a alucinação forte com as três dos monitores orbitando
static void DesenharAlucinacaoMestre(Vector2 c, float t)
{
    static const Color cores[TOTAL_MONITORES] = { { 0, 228, 48, 255 }, { 255, 161, 0, 255 }, { 102, 191, 255, 255 } };

    DrawCircleV(c, 78, Fade(MAGENTA, 0.06f + 0.03f * sinf(t * 3.0f)));

    // As que estão "atrás" primeiro, depois o corpo, depois as da frente
    for (int camada = 0; camada < 2; camada++) {

        if (camada == 1)
            DesenharAlucinacao(c, t, true);

        for (int i = 0; i < TOTAL_MONITORES; i++) {

            float ang = t * 1.4f + i * (2.0f * PI / TOTAL_MONITORES);
            bool frente = sinf(ang) > 0.0f;

            if (frente != (camada == 1))
                continue;

            Vector2 p = { c.x + cosf(ang) * 96, c.y + 6 + sinf(ang) * 22 };
            float s = frente ? 1.0f : 0.8f;

            DrawRectangle((int)(p.x - 14 * s), (int)(p.y - 10 * s), (int)(28 * s), (int)(20 * s), cores[i]);
            DrawRectangle((int)(p.x - 11 * s), (int)(p.y - 7 * s), (int)(22 * s), (int)(14 * s), (Color){ 20, 12, 36, 255 });
            DrawText("?", (int)(p.x - 3 * s), (int)(p.y - 7 * s), (int)(14 * s), cores[i]);
        }
    }
}

static void DesenharGuardiaoB2(int g, Vector2 c, float t, float escala)
{
    (void)escala;

    if (g == GUARDIAO_MESTRE) DesenharAlucinacaoMestre(c, t);
    else                      DesenharAlucinacao(c, t, false);
}

// Lupa da verificação (botão VERIFICAR, golpe final e a Lente da Verificação do Bloco 2)
static void DesenharLupaVerificacao(Vector2 c, float e, float t)
{
    float brilho = 0.12f + 0.06f * sinf(t * 3.0f + 1.0f);
    Vector2 lente = { c.x - 3 * e, c.y - 3 * e };

    DrawCircleV(c, 20 * e, Fade(PURPLE, brilho));
    DrawLineEx((Vector2){ lente.x + 7 * e, lente.y + 7 * e }, (Vector2){ c.x + 13 * e, c.y + 13 * e }, 4 * e, GOLD);
    DrawCircleV(lente, 10 * e, PURPLE);
    DrawCircleV(lente, 7 * e, Fade(SKYBLUE, 0.6f));
    DrawCircleV((Vector2){ lente.x - 3 * e, lente.y - 3 * e }, 2 * e, Fade(WHITE, 0.8f));
}

// ---------- Desenho da batalha ----------

// Botão "PEDIR AJUDA À LUCI": fica embaixo das opções, com a Luci desenhada nele
static void B2DesenharBotaoAjuda(const BatalhaB2 *b, Rectangle r, bool marcado)
{
    int e = *b->estabilidade;
    bool usada = b->ajudaNaRodada;
    bool sobrecarregada = (e <= 0);

    const char *texto = usada ? (b->chefe ? "A LUCI JÁ TROUXE A FONTE" : "A LUCI JÁ AJUDOU NESTA RODADA")
                       : (sobrecarregada ? "LUCI SOBRECARREGADA" : (b->chefe ? "PEDIR FONTE À LUCI   [H]" : "PEDIR AJUDA À LUCI   [H]"));

    Color corBorda = usada ? Fade(GRAY, 0.6f) : (sobrecarregada ? RED : SKYBLUE);
    Color corTexto = usada ? GRAY : (sobrecarregada ? RED : SKYBLUE);

    // Chama atenção se o jogador está demorando para escolher
    if (!usada && !sobrecarregada && b->tempoFase > 6.0f)
        corBorda = Fade(GOLD, 0.6f + 0.4f * sinf((float)GetTime() * 6.0f));

    if (marcado) { corBorda = YELLOW; corTexto = usada ? GRAY : YELLOW; }

    DrawRectangleRec(r, Fade((Color){ 10, 30, 55, 255 }, 0.9f));
    DrawRectangleLinesEx(r, 2, corBorda);

    if (marcado)
        DesenharCoracao(r.x + 14, r.y + r.height / 2, 2, RED);

    DesenharLuciTamanho((Vector2){ r.x + 38, r.y + r.height / 2 }, r.height - 8);

    int tam = 14;
    DrawText(texto, (int)r.x + 58, (int)(r.y + r.height / 2) - tam / 2, tam, corTexto);

    // Estabilidade em miniatura dentro do botão
    if (!usada) {
        Color corE = (e >= 3) ? SKYBLUE : (e == 2 ? ORANGE : RED);
        for (int i = 0; i < ESTABILIDADE_MAX_LUCI; i++) {
            Rectangle seg = { r.x + r.width - 62 + i * 11, r.y + r.height / 2 - 4, 8, 8 };
            if (i < e) DrawRectangleRec(seg, corE);
            DrawRectangleLinesEx(seg, 1, Fade(GRAY, 0.8f));
        }
    }
}

// Cola da Luci: na abertura aparece grande, no balão da dica; depois fica
// fixada no canto de cima só durante a PRIMEIRA pergunta. Nas outras, quem
// precisar pede a dica da Luci (H).
static void B2DesenharCola(const BatalhaB2 *b)
{
    bool primeiraPergunta = (b->rodada == 0) &&
                            (b->fase == B2_INTRO || b->fase == B2_DESAFIO || b->fase == B2_RESULTADO);

    if (b->cola == NULL || !primeiraPergunta)
        return;

    float k = b->tempoCola / 0.45f;
    if (k > 1.0f) k = 1.0f;
    k = k * k * (3.0f - 2.0f * k);

    Rectangle grande = { 96, 52, 228, 112 };
    Rectangle canto  = { 586, 52, 202, 104 };
    Rectangle r = {
        Lerp(grande.x, canto.x, k), Lerp(grande.y, canto.y, k),
        Lerp(grande.width, canto.width, k), Lerp(grande.height, canto.height, k)
    };

    int fonte = (k < 0.5f) ? 14 : 12;
    float tamLuci = Lerp(40.0f, 22.0f, k);
    Vector2 luci = { r.x - Lerp(32.0f, 16.0f, k), r.y + Lerp(60.0f, 16.0f, k) + sinf((float)GetTime() * 2.0f) * 3.0f };

    DesenharLuciTamanho(luci, tamLuci);
    DesenharTrianguloB2((Vector2){ r.x, luci.y - 8 }, (Vector2){ r.x - 10, luci.y }, (Vector2){ r.x, luci.y + 8 }, WHITE);
    DrawRectangleRec(r, WHITE);
    DrawRectangleLinesEx(r, 2, GOLD);

    DrawText(k < 0.5f ? "LUCI" : "COLA DA LUCI", (int)r.x + 10, (int)r.y + 7, 10, (Color){ 0, 82, 172, 255 });

    char linhas[256];
    QuebrarTexto(b->cola, (int)r.width - 20, fonte, linhas, (int)sizeof(linhas));
    DesenharTextoLinhas(linhas, (int)r.x + 10, (int)r.y + 22, fonte, fonte + 3, (Color){ 30, 28, 44, 255 });
}

// Verdade para pegar: um "certo" com brilho
static void DesenharVerdadeB2(Vector2 p, Color cor, float alfa)
{
    DrawCircleV(p, 10, Fade(cor, 0.25f * alfa));
    DrawLineEx((Vector2){ p.x - 6, p.y }, (Vector2){ p.x - 2, p.y + 5 }, 3, Fade(cor, alfa));
    DrawLineEx((Vector2){ p.x - 2, p.y + 5 }, (Vector2){ p.x + 7, p.y - 6 }, 3, Fade(cor, alfa));
}

// Ícones do minigame de separar: CONTRADIZ (balão laranja com X) e INVENTA (balão azul com ?)
static const char *NOMES_TIPO_SEPARAR[2] = { "CONTRADIZ", "INVENTA" };
static const Color CORES_TIPO_SEPARAR[2] = { { 255, 161, 0, 255 }, { 102, 191, 255, 255 } };

static void DesenharIconeSeparar(Vector2 p, int tipo, bool pedido)
{
    Color cor = CORES_TIPO_SEPARAR[tipo];
    Rectangle balao = { p.x - 11, p.y - 9, 22, 16 };

    if (pedido)
        DrawCircleV(p, 15, Fade(cor, 0.25f));

    DrawRectangleRec(balao, cor);
    DesenharTrianguloB2((Vector2){ p.x - 6, p.y + 7 }, (Vector2){ p.x - 8, p.y + 12 }, (Vector2){ p.x - 1, p.y + 7 }, cor);
    DrawText(tipo == 0 ? "X" : "?", (int)p.x - MeasureText(tipo == 0 ? "X" : "?", 14) / 2, (int)p.y - 8, 14, (Color){ 30, 20, 40, 255 });
}

// Uma bala do minigame, desenhada conforme a variação
static void DesenharBalaB2(const BatalhaB2 *b, const Bala *bl, int indice, Color corInimigo)
{
    Vector2 bp = bl->pos;
    float t = (float)GetTime();

    // LUPA: fora do alcance, tudo parece uma verdade apagada
    if (b->variante == VAR_LUPA && Vector2Distance(bp, b->alma) > RAIO_LUPA) {
        DesenharVerdadeB2(bp, LIME, 0.55f);
        return;
    }

    if (bl->tipo >= 0) {
        DesenharIconeSeparar(bp, bl->tipo, bl->tipo == b->ondaTipo);
        return;
    }

    if (bl->boa && bl->falsa) {
        // Falsificação: parece verdade, mas pisca em magenta de tempos em tempos
        bool pisca = fmodf(t * 1.6f + indice * 0.37f, 1.0f) < 0.3f;
        Vector2 p = pisca ? (Vector2){ bp.x + (float)GetRandomValue(-1, 1), bp.y } : bp;
        DesenharVerdadeB2(p, pisca ? MAGENTA : LIME, 1.0f);
        return;
    }

    if (bl->boa) {
        DesenharVerdadeB2(bp, LIME, 1.0f);
        if (b->variante == VAR_LUPA) DrawCircleLines((int)bp.x, (int)bp.y, 12, Fade(GOLD, 0.8f));
        return;
    }

    char g[2] = { bl->glifo, '\0' };
    DrawText(g, (int)bp.x - MeasureText(g, 20) / 2, (int)bp.y - 10, 20, corInimigo);

    if (b->variante == VAR_LUPA)
        DrawCircleLines((int)bp.x, (int)bp.y, 12, Fade(RED, 0.8f));
}

// Legenda do minigame ao lado da caixa (ícones e poucas palavras)
static void B2DesenharLegenda(const BatalhaB2 *b, Color corInimigo)
{
    const char *modo = b->faseAtaque ? "ATAQUE!" : "DEFESA!";
    DrawText(modo, 556, 266, 18, b->faseAtaque ? GOLD : RED);

    if (b->variante == VAR_SEPARAR) {
        int p = b->ondaTipo;
        DesenharIconeSeparar((Vector2){ 566, 302 }, p, true);
        DrawText("PEGUE ESTE", 584, 295, 14, CORES_TIPO_SEPARAR[p]);
        DesenharIconeSeparar((Vector2){ 566, 334 }, 1 - p, false);
        DrawText("ESTE MACHUCA", 584, 327, 14, LIGHTGRAY);
    }
    else if (b->variante == VAR_LUPA) {
        DrawCircleLines(566, 302, 9, GOLD);
        DrawLineEx((Vector2){ 572, 308 }, (Vector2){ 578, 314 }, 3, GOLD);
        DrawText("A LUPA REVELA", 584, 293, 14, GOLD);
        DesenharVerdadeB2((Vector2){ 566, 334 }, LIME, 0.55f);
        DrawText("FORA DELA, TUDO", 584, 322, 12, LIGHTGRAY);
        DrawText("PARECE VERDADE", 584, 336, 12, LIGHTGRAY);
    }
    else {
        DesenharVerdadeB2((Vector2){ 566, 300 }, LIME, 1.0f);
        DrawText(b->faseAtaque ? "PEGUE = DANO" : "PEGUE = VIDA", 582, 293, 14, LIME);

        if (b->variante == VAR_FALSAS) {
            DesenharVerdadeB2((Vector2){ 566, 330 }, MAGENTA, 1.0f);
            DrawText("PISCOU = FALSA", 582, 323, 14, MAGENTA);
        } else {
            DrawText("?!", 558, 328, 16, corInimigo);
            DrawText("DESVIE", 582, 330, 14, corInimigo);
        }
    }

    if (b->faseAtaque)
        DrawText(TextFormat("VERDADES: %d", b->verdadesNoAtaque), 556, 362, 14, GOLD);
}

static void BatalhaB2Desenhar(const BatalhaB2 *b, const char *nickname, float tempo)
{
    const DefGuardiaoB2 *def = &GUARDIOES_B2[b->guardiao];

    ClearBackground(BLACK);

    DrawText(def->nome, LARGURA_TELA / 2 - MeasureText(def->nome, 16) / 2, 12, 16, def->cor);
    DrawText(b->subtitulo, LARGURA_TELA / 2 - MeasureText(b->subtitulo, 10) / 2, 32, 10, GRAY);

    Vector2 centro = { LARGURA_TELA / 2.0f, 108.0f };

    // Vencida, ela some (o golpe final já mostrou ela se desfazendo)
    if (!(b->venceu && b->fase == B2_FIM)) {

        DesenharGuardiaoB2(b->guardiao, centro, tempo, 0.85f);

        // Golpe acertou: desenha de novo somando a cor, e ela fica branca por um instante
        if (b->flashInimigo > 0.0f) {
            BeginBlendMode(BLEND_ADDITIVE);
            DesenharGuardiaoB2(b->guardiao, centro, tempo, 0.85f);
            DesenharGuardiaoB2(b->guardiao, centro, tempo, 0.85f);
            EndBlendMode();
        }

        // Golpe final: depois do impacto ela se desfaz em faixas
        if (b->fase == B2_FINALIZANDO && b->tempoFase > IMPACTO_GOLPE_FINAL) {

            float k = (b->tempoFase - IMPACTO_GOLPE_FINAL) / (DURACAO_GOLPE_FINAL - IMPACTO_GOLPE_FINAL);
            if (k > 1.0f) k = 1.0f;

            for (int i = 0; i < 13; i++) {
                int desloc = (int)(((float)(Ruido((unsigned int)(i * 7) + (unsigned int)(tempo * 20.0f)) % 40) - 20.0f) * k);
                float alfa = Clamp(k * 1.5f - (float)(i % 3) * 0.2f, 0.0f, 1.0f);
                DrawRectangle(270 + desloc, 50 + i * 10, 260, 10, Fade(BLACK, alfa));
            }
        }
    }

    // Barra de alucinação
    {
        int barraX = 250, barraY = 184, barraL = 300;
        DrawText("ALUCINAÇÃO", barraX, barraY - 1, 10, LIGHTGRAY);
        DrawRectangle(barraX + 80, barraY, barraL - 80, 10, (Color){ 40, 30, 55, 255 });
        DrawRectangle(barraX + 80, barraY, (int)((barraL - 80) * b->alucinacaoGuardiao / 100.0f), 10, def->cor);
        DrawRectangleLines(barraX + 80, barraY, barraL - 80, 10, GRAY);

        if (b->combo >= 2 && b->fase != B2_FIM)
            DrawText(TextFormat("COMBO: %d SEGUIDOS", b->combo), barraX + barraL + 12, barraY - 2, 14,
                     Fade(GOLD, 0.7f + 0.3f * sinf(tempo * 8.0f)));
    }

    float t = b->anim * b->anim * (3.0f - 2.0f * b->anim);
    Rectangle base = b->estiloAcao ? CAIXA_ACAO : CAIXA_B2;

    Rectangle caixa = {
        Lerp(base.x, CAIXA_DESVIO_B2.x, t),
        Lerp(base.y, CAIXA_DESVIO_B2.y, t),
        Lerp(base.width, CAIXA_DESVIO_B2.width, t),
        Lerp(base.height, CAIXA_DESVIO_B2.height, t)
    };

    if (b->tremor > 0.0f) {
        caixa.x += (float)GetRandomValue(-3, 3);
        caixa.y += (float)GetRandomValue(-3, 3);
    }

    DrawRectangleRec(caixa, BLACK);
    DrawRectangleLinesEx(caixa, 4, WHITE);

    int px = (int)caixa.x + 24;
    int py = (int)caixa.y + 16;

    if (b->fase == B2_INTRO || b->fase == B2_RESULTADO || b->fase == B2_FIM ||
        b->fase == B2_GOLPE_FINAL || b->fase == B2_FINALIZANDO) {

        char parcial[768];
        bool terminou = TextoParcial(b->texto, b->visiveis, parcial, (int)sizeof(parcial));

        DesenharTextoLinhas(parcial, px, py + 6, FONTE_BATALHA, 28, WHITE);

        // O texto avança sozinho: a linha embaixo do "ENTER >" mostra quanto falta
        if (terminou && b->fase != B2_GOLPE_FINAL && b->fase != B2_FINALIZANDO) {

            int ex = (int)(caixa.x + caixa.width - 90), ey = (int)(caixa.y + caixa.height - 26);
            float k = b->esperaTexto / B2EsperaTexto(b);
            if (k > 1.0f) k = 1.0f;

            if (((int)(GetTime() * 2.0)) % 2 == 0)
                DrawText("ENTER >", ex, ey, 12, GOLD);

            DrawRectangle(ex, ey + 15, (int)(52 * k), 2, Fade(GOLD, 0.6f));
        }
    }

    // Golpe final: um botão só, VERIFICAR
    if (b->fase == B2_GOLPE_FINAL) {

        float pulso = 0.5f + 0.5f * sinf((float)GetTime() * 6.0f);
        float yBotao = b->estiloAcao ? CAIXA_ACAO.y + CAIXA_ACAO.height + 16 : caixa.y + caixa.height - 72;
        Rectangle botao = { LARGURA_TELA / 2.0f - 150, yBotao, 300, 52 };

        DrawRectangleRec(botao, Fade(GOLD, 0.12f + 0.1f * pulso));
        DrawRectangleLinesEx(botao, 4, Fade(GOLD, 0.6f + 0.4f * pulso));
        DesenharCoracao(botao.x + 26, botao.y + 26, 2, RED);
        DesenharLupaVerificacao((Vector2){ botao.x + botao.width - 32, botao.y + 26 }, 1.0f, tempo);
        DesenharTextoBotao(botao, "VERIFICAR", 26, GOLD);
    }

    // Golpe final: a lupa voa até a alucinação e carimba
    if (b->fase == B2_FINALIZANDO) {

        float tf = b->tempoFase;

        if (tf < IMPACTO_GOLPE_FINAL) {

            float k = tf / IMPACTO_GOLPE_FINAL;
            k = k * k * (3.0f - 2.0f * k);

            Vector2 p = { Lerp(110.0f, centro.x, k), Lerp(170.0f, centro.y, k) - sinf(k * PI) * 50.0f };
            DesenharLupaVerificacao(p, 3.0f, tempo);

        } else {

            float k = (tf - IMPACTO_GOLPE_FINAL) / (DURACAO_GOLPE_FINAL - IMPACTO_GOLPE_FINAL);
            if (k > 1.0f) k = 1.0f;

            for (int i = 0; i < 3; i++) {
                float raio = 20.0f + k * 170.0f - i * 30.0f;
                if (raio > 0.0f)
                    DrawCircleLines((int)centro.x, (int)centro.y, raio, Fade(GOLD, (1.0f - k) * 0.9f));
            }

            float e = (k * 4.0f > 1.0f) ? 1.0f : k * 4.0f;
            int tam = (int)Lerp(56.0f, 36.0f, e);
            const char *carimbo = "VERIFICADO!";
            int cx = (int)centro.x - MeasureText(carimbo, tam) / 2, cy = (int)centro.y - tam / 2;
            DrawText(carimbo, cx + 3, cy + 3, tam, BLACK);
            DrawText(carimbo, cx, cy, tam, LIME);
        }
    }

    else if (b->fase == B2_DESAFIO && b->estiloAcao) {

        // Frase do inimigo na caixa e as ações como botões embaixo (estilo Undertale)
        const RodadaEscolha *r = RodadaAtualB2(b);

        DesenharTextoLinhas(b->enunciado, px, py + 10, FONTE_BATALHA, 28, WHITE);

        // Chefe: a fonte que a Luci trouxe fica na caixa, embaixo da afirmação
        if (b->chefe) {

            int yFonte = (int)(caixa.y + caixa.height) - 58;
            DrawLine(px, yFonte - 6, (int)(caixa.x + caixa.width) - 24, yFonte - 6, Fade(SKYBLUE, 0.3f));

            if (b->dica[0] != '\0') {

                char linhas[256];
                QuebrarTexto(b->dica, 590, 14, linhas, (int)sizeof(linhas));

                DesenharLuciTamanho((Vector2){ (float)px + 14, (float)yFonte + 22 }, 30.0f);
                DrawText("FONTE DA LUCI:", px + 40, yFonte, 10, SKYBLUE);
                DesenharTextoLinhas(linhas, px + 40, yFonte + 14, 14, 17, LIGHTGRAY);

            } else {

                const char *aviso = "Sem fonte. Concordar ou contestar no escuro?   (H: pedir fonte à Luci)";
                DrawText(aviso, px, yFonte + 14, 14, Fade(GRAY, 0.9f));
            }
        }

        int largura = 270, espaco = 24;
        int total = r->numOpcoes * largura + (r->numOpcoes - 1) * espaco;
        int x0 = LARGURA_TELA / 2 - total / 2;
        int yBotao = (int)(CAIXA_ACAO.y + CAIXA_ACAO.height) + 16;

        for (int i = 0; i < r->numOpcoes; i++) {

            bool marcada = (b->selecao == i);
            Rectangle botao = { (float)(x0 + i * (largura + espaco)), (float)yBotao, (float)largura, 44 };

            DrawRectangleRec(botao, BLACK);
            DrawRectangleLinesEx(botao, 3, marcada ? YELLOW : PURPLE);

            if (marcada)
                DesenharCoracao(botao.x + 20, botao.y + 22, 2, RED);

            int tam = (MeasureText(r->opcoes[i], 18) > largura - 50) ? 14 : 18;
            DrawText(r->opcoes[i], (int)botao.x + 38 + (largura - 50 - MeasureText(r->opcoes[i], tam)) / 2,
                     (int)botao.y + 22 - tam / 2, tam, marcada ? YELLOW : LIGHTGRAY);
        }

        int yDica = yBotao + 56;

        if (b->estabilidade != NULL) {
            B2DesenharBotaoAjuda(b, (Rectangle){ LARGURA_TELA / 2.0f - 165, (float)yBotao + 54, 330, 34 }, b->selecao == r->numOpcoes);
            yDica = yBotao + 96;
        }

        const char *dica = (b->estabilidade == NULL) ? "A/D ou SETAS: escolher a ação    ENTER: confirmar"
                         : (b->chefe ? "A/D: concordar ou contestar    S: pedir fonte à Luci    ENTER: confirmar"
                                     : "A/D: escolher a ação    S: ajuda da Luci    ENTER: confirmar");
        DrawText(dica, LARGURA_TELA / 2 - MeasureText(dica, 10) / 2, yDica, 10, GRAY);
    }

    else if (b->fase == B2_DESAFIO) {

        const RodadaEscolha *r = RodadaAtualB2(b);
        int f = b->fonteDesafio;
        int linhas = DesenharTextoLinhas(b->enunciado, px, py, f, f + 6, WHITE);
        int y = py + linhas * (f + 6) + 12;

        for (int i = 0; i < r->numOpcoes; i++) {

            bool marcada = (b->selecao == i);

            if (marcada)
                DesenharCoracao((float)px + 8, (float)y + f / 2.0f, 2, RED);

            int nl = DesenharTextoLinhas(b->opcoesTexto[i], px + 26, y, f, f + 6, marcada ? YELLOW : LIGHTGRAY);
            y += nl * (f + 6) + 6;
        }

        if (b->estabilidade != NULL)
            B2DesenharBotaoAjuda(b, (Rectangle){ (float)px + 20, (float)y + 2, 330, 30 }, b->selecao == r->numOpcoes);

        DrawText("W/S ou SETAS: escolher    ENTER: confirmar    1-4: atalho    H: ajuda da Luci",
                 px, (int)(caixa.y + caixa.height) - 22, 10, GRAY);
    }

    else if (b->fase == B2_DESVIO) {

        BeginScissorMode((int)caixa.x + 4, (int)caixa.y + 4, (int)caixa.width - 8, (int)caixa.height - 8);

        // SEPARAR: as duas metades da caixa, com o lado pedido aceso
        if (b->variante == VAR_SEPARAR) {

            float meio = caixa.x + caixa.width / 2;

            for (int lado = 0; lado < 2; lado++) {

                Rectangle metade = { lado == 0 ? caixa.x : meio, caixa.y, caixa.width / 2, caixa.height };
                bool pedido = (lado == b->ondaTipo);
                const char *nome = NOMES_TIPO_SEPARAR[lado];

                if (pedido)
                    DrawRectangleRec(metade, Fade(CORES_TIPO_SEPARAR[lado], 0.10f));

                DrawText(nome, (int)(metade.x + metade.width / 2) - MeasureText(nome, 10) / 2, (int)caixa.y + 10, 10,
                         Fade(CORES_TIPO_SEPARAR[lado], pedido ? 1.0f : 0.45f));
            }

            for (float y = caixa.y; y < caixa.y + caixa.height; y += 12)
                DrawLineEx((Vector2){ meio, y }, (Vector2){ meio, y + 6 }, 2, Fade(WHITE, 0.3f));
        }

        // LUPA: o círculo da verificação em volta do coração
        if (b->variante == VAR_LUPA) {

            Vector2 cabo = { b->alma.x + RAIO_LUPA * 0.71f, b->alma.y + RAIO_LUPA * 0.71f };

            DrawCircleV(b->alma, RAIO_LUPA, Fade(SKYBLUE, 0.07f));
            DrawCircleLines((int)b->alma.x, (int)b->alma.y, RAIO_LUPA, Fade(GOLD, 0.8f));
            DrawLineEx(cabo, (Vector2){ cabo.x + 14, cabo.y + 14 }, 5, Fade(GOLD, 0.8f));
        }

        for (int i = 0; i < MAX_BALAS; i++)
            if (b->balas[i].ativa)
                DesenharBalaB2(b, &b->balas[i], i, def->cor);

        if (b->invencivel <= 0.0f || (((int)(b->invencivel * 10.0f)) % 2 == 0))
            DesenharCoracao(b->alma.x, b->alma.y, 2, RED);

        EndScissorMode();

        float resta = 1.0f - b->tempoDesvio / b->duracaoDesvio;
        if (resta < 0.0f) resta = 0.0f;

        DrawRectangle((int)caixa.x, (int)(caixa.y + caixa.height + 6), (int)(caixa.width * resta), 3, Fade(WHITE, 0.5f));

        // SEPARAR: o pedido da Luci em cima da caixa
        if (b->variante == VAR_SEPARAR) {
            const char *pedido = TextFormat("LUCI: PEGUE SÓ  %s!", NOMES_TIPO_SEPARAR[b->ondaTipo]);
            DrawText(pedido, LARGURA_TELA / 2 - MeasureText(pedido, 18) / 2, 222, 18, CORES_TIPO_SEPARAR[b->ondaTipo]);
        }
    }

    for (int i = 0; i < 48; i++)
        if (b->particulas[i].vida > 0.0f)
            DrawCircleV(b->particulas[i].pos, 3, Fade(b->particulas[i].cor, b->particulas[i].vida / 0.6f));

    // Lembra a regra do minigame com ícones (sem texto longo)
    if (b->fase == B2_DESVIO && b->pegarBoas)
        B2DesenharLegenda(b, def->cor);

    // Dano causado no ataque, subindo sobre o inimigo
    if (b->tempoDano > 0.0f) {
        float k = b->tempoDano / 1.4f;
        if (k > 1.0f) k = 1.0f;
        const char *txt = TextFormat("-%d", (int)b->danoMostrado);
        int y = (int)(70 - (1.0f - k) * 30);
        DrawText(txt, LARGURA_TELA / 2 + 70, y, 30, Fade(GOLD, k));

        if (b->danoCombo)
            DrawText("COMBO x1,5", LARGURA_TELA / 2 + 70, y + 32, 14, Fade(ORANGE, k));
    }

    // Balão de ajuda da Luci (só enquanto o jogador escolhe; no chefe a fonte fica na caixa)
    if (b->fase == B2_DESAFIO && b->dica[0] != '\0' && !b->chefe) {

        float k = b->tempoDica / 0.2f;
        if (k > 1.0f) k = 1.0f;

        Vector2 luci = { 64.0f, 112.0f + sinf((float)GetTime() * 2.0f) * 3.0f };
        Rectangle balao = { 96, 66, 206, 92 };

        DesenharLuciTamanho(luci, 40.0f);
        DesenharTrianguloB2((Vector2){ balao.x, balao.y + 40 }, (Vector2){ balao.x - 12, balao.y + 50 }, (Vector2){ balao.x, balao.y + 56 }, Fade(WHITE, k));
        DrawRectangleRec(balao, Fade(WHITE, k));
        DrawText("LUCI", (int)balao.x + 10, (int)balao.y + 8, 10, Fade((Color){ 0, 82, 172, 255 }, k));

        char linhas[200];
        QuebrarTexto(b->dica, 186, 14, linhas, (int)sizeof(linhas));
        DesenharTextoLinhas(linhas, (int)balao.x + 10, (int)balao.y + 24, 14, 17, Fade((Color){ 30, 28, 44, 255 }, k));
    }

    B2DesenharCola(b);

    int yStatus = 522;

    // Estabilidade da Luci: cada ajuda gasta 1, cada acerto sozinho devolve 1
    if (b->estabilidade != NULL) {

        int e = *b->estabilidade;
        Color corE = (e >= 3) ? SKYBLUE : (e == 2 ? ORANGE : RED);

        DrawText("LUCI", 500, yStatus + 4, 12, WHITE);

        for (int i = 0; i < ESTABILIDADE_MAX_LUCI; i++) {
            Rectangle seg = { 540.0f + i * 20, (float)yStatus + 4, 16, 12 };
            if (i < e) DrawRectangleRec(seg, corE);
            DrawRectangleLinesEx(seg, 1, Fade(GRAY, 0.8f));
        }

        if (e <= 2)
            DrawText(e == 0 ? "SOBRECARREGADA" : "INSTÁVEL", 540, yStatus + 20, 10, corE);
    }

    DrawText(nickname, 60, yStatus, 20, WHITE);
    DrawText("LV 1", 150, yStatus, 20, WHITE);
    DrawText("HP", 250, yStatus + 4, 12, WHITE);
    DrawRectangle(275, yStatus, HP_MAX_B2 * 4, 20, RED);
    DrawRectangle(275, yStatus, b->hp * 4, 20, YELLOW);
    DrawText(TextFormat("%d / %d", b->hp, HP_MAX_B2), 275 + HP_MAX_B2 * 4 + 14, yStatus, 20, WHITE);

    const char *instrucao = "ENTER  -  continuar";

    if (b->fase == B2_DESVIO)          instrucao = "W A S D ou SETAS  -  mova o coração e desvie!";
    else if (b->fase == B2_GOLPE_FINAL) instrucao = "ENTER  -  VERIFICAR!";
    else if (b->fase == B2_FINALIZANDO) instrucao = "";

    if (b->fase != B2_DESAFIO)
        DrawText(instrucao, LARGURA_TELA / 2 - MeasureText(instrucao, 12) / 2, 568, 12, GRAY);
}

// ---------- Cartão de resumo ----------

static void AbrirCartao(CartaoResumo *c, int icone, bool sucesso, const char *titulo,
                        const char *frase, const Dialogo *saberMais)
{
    c->ativo = true;
    c->icone = icone;
    c->sucesso = sucesso;
    c->titulo = titulo;
    c->frase = frase;
    c->saberMais = saberMais;
    c->inicio = GetTime();
}

// Devolve true quando o cartão fecha (ENTER) ou vira "saber mais" (TAB)
static bool AtualizarCartao(CartaoResumo *c, SessaoDialogo *dlg)
{
    if (!c->ativo || GetTime() - c->inicio < 0.4)
        return false;

    if (IsKeyPressed(KEY_TAB) && c->saberMais != NULL) {
        c->ativo = false;
        Telemetria("saber_mais", c->titulo, (float)(GetTime() - c->inicio), 0);
        DialogoIniciar(dlg, c->saberMais, OBJ_NENHUM);
        dlg->veioDoCartao = true;
        return true;
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE)) {
        c->ativo = false;
        Telemetria("cartao", c->titulo, (float)(GetTime() - c->inicio), 0);
        return true;
    }

    return false;
}

static void DesenharCartao(const CartaoResumo *c, float tempo)
{
    if (!c->ativo)
        return;

    float k = (float)(GetTime() - c->inicio) / 0.25f;
    if (k > 1.0f) k = 1.0f;

    DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, 0.55f * k));

    Rectangle painel = { 160, 175 + (1.0f - k) * 30.0f, 480, 210 };
    Color destaque = c->sucesso ? GOLD : ORANGE;

    DrawRectangleRec(painel, (Color){ 18, 15, 30, 255 });
    DrawRectangleLinesEx(painel, 4, destaque);

    // Ícone com brilhos girando
    Vector2 ic = { painel.x + 66, painel.y + 92 };

    for (int i = 0; i < 6; i++) {
        float a = tempo * 1.5f + i * (PI / 3.0f);
        DrawCircleV((Vector2){ ic.x + cosf(a) * 40, ic.y + sinf(a) * 40 }, 2.5f, Fade(destaque, 0.7f));
    }

    {
        Rectangle tela = { ic.x - 28, ic.y - 22, 56, 40 };
        DrawRectangleRec(tela, c->sucesso ? (Color){ 8, 48, 22, 255 } : (Color){ 55, 8, 14, 255 });
        DrawRectangleLinesEx(tela, 3, c->sucesso ? GREEN : RED);
        DrawRectangle((int)ic.x - 4, (int)ic.y + 18, 8, 8, (Color){ 45, 40, 62, 255 });

        if (c->sucesso) {
            DrawLineEx((Vector2){ ic.x - 12, ic.y - 2 }, (Vector2){ ic.x - 3, ic.y + 7 }, 4, LIME);
            DrawLineEx((Vector2){ ic.x - 3, ic.y + 7 }, (Vector2){ ic.x + 13, ic.y - 10 }, 4, LIME);
        } else {
            DrawLineEx((Vector2){ ic.x - 9, ic.y - 10 }, (Vector2){ ic.x + 9, ic.y + 8 }, 4, RED);
            DrawLineEx((Vector2){ ic.x + 9, ic.y - 10 }, (Vector2){ ic.x - 9, ic.y + 8 }, 4, RED);
        }
    }

    int tx = (int)painel.x + 130;
    const char *rotulo = c->sucesso ? "VOCÊ APRENDEU" : "LEMBRE-SE";

    DrawText(rotulo, tx, (int)painel.y + 22, 10, Fade(destaque, 0.9f));
    DrawText(c->titulo, tx, (int)painel.y + 38, 20, destaque);

    char frase[256];
    QuebrarTexto(c->frase, 320, 16, frase, (int)sizeof(frase));
    DesenharTextoLinhas(frase, tx, (int)painel.y + 74, 16, 22, WHITE);

    if (((int)(GetTime() * 2.0)) % 2 == 0)
        DrawText("ENTER >", (int)(painel.x + painel.width) - 80, (int)(painel.y + painel.height) - 26, 12, GOLD);

    if (c->saberMais != NULL)
        DrawText("TAB - saber mais", tx, (int)(painel.y + painel.height) - 26, 12, Fade(SKYBLUE, 0.85f));
}

// Fim de uma batalha do Bloco 1: pontos, cartão e fala da Luci
static void FimBatalhaB2(ContextoB2 *c)
{
    BatalhaB2 *b = c->batalha;

    c->progresso->alucinacao += b->erros;

    int pontosAcerto = (b->monitor >= 0) ? PONTOS_ACERTO_B1 : PONTOS_ACERTO_B2;
    int acertosSozinho = b->acertos - b->acertosComAjuda;
    int delta = acertosSozinho * pontosAcerto + b->acertosComAjuda * PONTOS_ACERTO_COM_AJUDA
              - b->erros * PENALIDADE_ERRO;

    // Confiar numa dica alucinada fortalece a alucinação
    c->progresso->alucinacao += b->alucinacoesSeguidas;

    if (b->derrota)   delta -= PENALIDADE_HP_ZERO;

    Telemetria(b->venceu ? "batalha_vitoria" : (b->derrota ? "batalha_derrota" : "batalha_esquiva"),
               b->subtitulo, (float)(GetTime() - b->inicio), b->erros);

    // Batalha de monitor do Bloco 1
    if (*c->motivo == MOTIVO_MONITOR) {

        int m = b->monitor;

        c->progresso->estado[m] = b->venceu ? MON_ACERTOU : MON_ERROU;
        if (b->venceu) delta += PONTOS_POR_ACERTO;

        AlterarPontos(c->progresso, delta);
        AbrirCartao(c->cartao, m, b->venceu, TITULOS_RESUMO_MONITOR[m], FRASES_RESUMO_MONITOR[m], &saberMaisMonitores[m]);
        MostrarEmocaoLuci(b->venceu ? EMOCAO_ALEGRE : EMOCAO_DUVIDA, 3.0f);
    }

    // ALUCINAÇÃO MESTRE (fim do Bloco 1). Vencida: cartão e, depois, o ranking
    else if (*c->motivo == MOTIVO_CHEFE) {

        if (b->venceu) {

            *c->chefeVencido = true;
            delta += PONTOS_POR_ACERTO;
            AlterarPontos(c->progresso, delta);
            AbrirCartao(c->cartao, TOTAL_MONITORES, true, GUARDIOES_B2[GUARDIAO_MESTRE].nome, FRASE_RESUMO_CHEFE, NULL);
            MostrarEmocaoLuci(EMOCAO_ALEGRE, 3.0f);

        } else {

            AlterarPontos(c->progresso, delta);
            DialogoFalaAvulsa(c->dlg, FALA_CHEFE_DERROTA);
            MostrarEmocaoLuci(EMOCAO_DUVIDA, 3.0f);
        }
    }

    *c->pedirAutoSave = true;
    IniciarTransicao(c->transicao, *c->salaDaBatalha, DURACAO_FADE_BATALHA);
}

// =============================================================
// OBJETIVO ATUAL, MARCADOR E AMBIENTE
//   Para o jogador nunca ficar perdido: uma linha de objetivo fixa no topo,
//   uma seta sobre o próximo alvo e a Luci voando até ele quando o jogador
//   fica parado. O ambiente fica mais "glitchado" conforme a alucinação sobe.
// =============================================================

typedef struct {
    char texto[96];
    bool temAlvo;
    Vector2 alvo;
} Objetivo;

static Vector2 CentroRetangulo(Rectangle r)
{
    return (Vector2){ r.x + r.width / 2, r.y + r.height / 2 };
}

static Objetivo ObjetivoAtual(TelaAtual tela, const ProgressoCorredor *pc,
                              bool luciAcompanhando, Rectangle luciHitbox, bool chefeVencido)
{
    Objetivo o = { "", false, { 0, 0 } };

    #define OBJETIVO(txt, x, y) do { snprintf(o.texto, sizeof(o.texto), "%s", (txt)); \
                                       o.temAlvo = true; o.alvo = (Vector2){ (x), (y) }; } while (0)

    if (tela == JOGANDO) {

        if (!luciAcompanhando) OBJETIVO("Fale com a Luci", luciHitbox.x + luciHitbox.width / 2, luciHitbox.y + luciHitbox.height / 2);
        else                   OBJETIVO("Vá até a porta do CORREDOR", CentroRetangulo(PORTA_SALA).x, CentroRetangulo(PORTA_SALA).y);
    }
    else if (tela == CORREDOR) {

        int proximo = ProximoMonitor(pc);

        if (proximo >= 0) {
            Rectangle a = monitores[proximo].area;
            OBJETIVO(TextFormat("Enfrente a alucinação do MONITOR %d", proximo + 1), a.x + a.width / 2, a.y + a.height / 2);
        } else if (!chefeVencido) {
            OBJETIVO("Enfrente a ALUCINAÇÃO MESTRE na porta da direita", CentroRetangulo(PORTA_FIM_CORREDOR).x, CentroRetangulo(PORTA_FIM_CORREDOR).y);
        } else {
            OBJETIVO("Siga pela porta da direita", CentroRetangulo(PORTA_FIM_CORREDOR).x, CentroRetangulo(PORTA_FIM_CORREDOR).y);
        }
    }
    #undef OBJETIVO
    return o;
}

// Linha de objetivo fixa no topo da tela
static void DesenharObjetivoHUD(const Objetivo *o)
{
    if (o->texto[0] == '\0')
        return;

    const char *t = TextFormat("OBJETIVO:  %s", o->texto);
    int w = MeasureText(t, 14);
    int x = LARGURA_TELA / 2 - w / 2;

    DrawRectangle(x - 14, 12, w + 28, 26, Fade(BLACK, 0.65f));
    DrawRectangleLinesEx((Rectangle){ (float)x - 14, 12, (float)w + 28, 26 }, 1, Fade(GOLD, 0.6f));
    DrawText(t, x, 18, 14, GOLD);
}

// Faixas de glitch que aumentam com o nível de alucinação (não mexem a tela)
#define MOSTRAR_GLITCH_AMBIENTE  1      // 0 = desliga as faixas

static void DesenharGlitchAmbiente(int nivel, float tempo)
{
    if (nivel <= 0 || !MOSTRAR_GLITCH_AMBIENTE)
        return;

    int n = (nivel > 8) ? 8 : nivel;
    unsigned int quadro = (unsigned int)(tempo * 8.0f);

    for (int i = 0; i < n * 2; i++) {

        unsigned int r = Ruido(quadro * 13u + (unsigned int)i);

        if (r % 3 != 0)
            continue;

        int largura = 60 + (int)((r >> 8) % 300);
        int x = (int)((r >> 16) % (unsigned int)(LARGURA_TELA - largura));
        int y = 40 + (int)(r % 520);

        DrawRectangle(x, y, largura, 2 + (int)((r >> 4) % 3), Fade((i % 2) ? MAGENTA : SKYBLUE, 0.08f + 0.02f * n));
    }

    DrawRectangleLinesEx((Rectangle){ 0, 0, LARGURA_TELA, ALTURA_TELA }, 6.0f + n * 2, Fade((Color){ 70, 0, 50, 255 }, 0.05f * n));
}

// =============================================================
// BLOCO 2 - BIBLIOTECA DOS ECOS (roguelite de tiro, visão de cima)
//   ÁTRIO (a base): o PROFESSOR BYTE explica os fundamentos da IA generativa,
//   o BYTECO troca Fragmentos de Conhecimento por itens e a porta de cima
//   começa a MISSÃO: uma sequência sorteada de salas
//     COMBATE    - elimine todos os inimigos para abrir a porta
//     EDUCATIVA  - um terminal de conhecimento (dá Fragmentos de Conhecimento)
//     RECOMPENSA - escolha 1 entre 3 itens
//   que termina na sala do ALUCINADOR (o combate dele vem na Etapa 4).
//   Morreu: volta ao átrio, perde 15 pontos e os itens da tentativa.
//   Os Fragmentos de Conhecimento nunca se perdem.
//
//   Controles: WASD anda; o mouse (ou as SETAS) mira; clique esquerdo (ou
//   segurar uma SETA) atira com o DISPARADOR DE DADOS; ESPAÇO esquiva; botão
//   direito (ou Q) solta o PULSO DE INTEGRIDADE; 1 = Antivírus; 2 = Pacote de
//   Dados; E = falar, ler e pegar.
//
//   Inimigos: FRAGMENTO ALUCINADO (persegue e dá o bote; às vezes deixa um
//   ECO FALSO que explode se ninguém checar) e GUARDIÃO DE DADOS (fica longe
//   e atira em leque, num ritmo previsível).
// =============================================================

// ---------- Protagonista (AJUSTE AQUI) ----------
#define ROGUE_HP_MAX             100.0f
#define ROGUE_EP_MAX             100.0f
#define ROGUE_VELOCIDADE         210.0f
#define ROGUE_EP_POR_SEGUNDO     4.0f
#define RAIO_CORPO_JOGADOR       13.0f
#define INVULNERAVEL_APOS_DANO   0.7f

// Disparador de Dados
#define TIRO_DANO                10.0f
#define TIRO_INTERVALO           0.18f
#define TIRO_VELOCIDADE          540.0f
#define TIRO_RAIO                5.0f

// Esquiva
#define ESQUIVA_DISTANCIA        120.0f
#define ESQUIVA_DURACAO          0.20f
#define ESQUIVA_INVULNERAVEL     0.30f
#define ESQUIVA_RECARGA          0.70f

// Pulso de Integridade
#define PULSO_CUSTO_EP           35.0f
#define PULSO_RECARGA            4.0f
#define PULSO_RAIO               130.0f
#define PULSO_DANO               25.0f

// ---------- Inimigos (AJUSTE AQUI) ----------
#define FRAGMENTO_HP             30.0f
#define FRAGMENTO_VELOCIDADE     115.0f
#define FRAGMENTO_RAIO           14.0f
#define FRAGMENTO_DANO           12.0f
#define FRAGMENTO_ALCANCE_BOTE   80.0f
#define FRAGMENTO_PREPARO        0.35f    // tremendo antes do bote (dá para fugir)
#define FRAGMENTO_BOTE_VEL       420.0f
#define FRAGMENTO_BOTE_DURACAO   0.25f
#define FRAGMENTO_RECARGA        1.0f
#define CHANCE_ECO_FALSO         35       // % dos fragmentos que deixam um eco ao morrer
#define ECO_TEMPO                1.1f     // depois disso o eco explode em balas
#define ECO_HP                   6.0f
#define ECO_BALAS                6

#define GUARDIAO_HP              60.0f
#define GUARDIAO_VELOCIDADE      80.0f
#define GUARDIAO_RAIO            18.0f
#define GUARDIAO_DIST_MIN        190.0f
#define GUARDIAO_DIST_MAX        320.0f
#define GUARDIAO_INTERVALO       1.7f
#define GUARDIAO_PREPARO         0.45f    // brilha antes de atirar
#define GUARDIAO_TIRO_VEL        210.0f
#define GUARDIAO_TIRO_DANO       10.0f
#define GUARDIAO_LEQUE           3
#define GUARDIAO_ABERTURA        14.0f    // graus entre as balas do leque

#define TEMPO_SURGINDO           0.6f     // inimigo se materializando (não ataca)

// ---------- Missão, recompensas e itens (AJUSTE AQUI) ----------
#define SALAS_POR_MISSAO         6        // 3 de combate, 2 educativas e 1 de recompensa (+ a do chefe)
#define FC_POR_TERMINAL          3        // Fragmentos de Conhecimento por terminal lido
#define CHANCE_FC_FRAGMENTO      40       // % de soltar um fragmento ao derrotar um Fragmento Alucinado
#define CHANCE_FC_GUARDIAO       70       // ... e um Guardião de Dados
#define CHANCE_ANTIVIRUS         6        // % de soltar um Antivírus
#define RAIO_IMA                 90.0f    // fragmentos voam até o jogador quando ele chega perto
#define CURA_ANTIVIRUS           40.0f
#define RECARGA_PACOTE           50.0f
#define BONUS_PROCESSADOR        0.25f    // +25% de dano por Processador Neural
#define REDUCAO_FIREWALL         0.25f    // -25% de dano recebido por Firewall
#define MAX_PILHA_PASSIVO        2        // no máximo 2 de cada item passivo

// Melhorias permanentes (Byteco). Não se perdem ao morrer.
#define MELHORIA_HP              20.0f    // +HP máximo por nível
#define MELHORIA_DANO            0.15f    // +15% de dano por nível
#define MELHORIA_EP              20.0f    // +EP máximo por nível
#define NIVEL_MAX_MELHORIA       3
static const int PRECO_MELHORIA[NIVEL_MAX_MELHORIA] = { 8, 12, 16 };

// Desafios educativos (Guardião selado)
#define RAIO_DESAFIO             170.0f   // chegando perto do selo, o desafio abre
#define PONTOS_DESAFIO           5
#define FC_POR_DESAFIO           1
#define DANO_SELO_CERTO          0.5f     // acertou: o guardião perde metade da vida
#define ATORDOADO_SELO           2.5f     // ... e fica parado esse tempo
#define BALAS_ATAQUE_ESPECIAL    14       // errou: anel de balas
#define CHANCE_LUCI_ERRAR        20       // % a mais de a dica da Luci sair errada a cada desafio respondido
#define CHANCE_LUCI_ERRAR_MAX    60

// ---------- Pontos (somam com os do Bloco 1) ----------
#define PONTOS_FRAGMENTO         1
#define PONTOS_GUARDIAO          2
#define PONTOS_SALA_ROGUE        5
#define PONTOS_TERMINAL          5

#define MAX_PROJETEIS            160
#define MAX_INIMIGOS             16
#define MAX_PARTICULAS_ROGUE     200
#define MAX_OBSTACULOS           6
#define MAX_COLETAVEIS           32
#define NUM_PEDESTAIS            3

// Conversas da Biblioteca (não colidem com os monitores nem com OBJ_*)
#define OBJ_BYTE                 -10
#define OBJ_BYTECO               -11
#define OBJ_TERMINAL             -12
#define OBJ_PORTAL_CHEFE         -13
#define OBJ_ENCICLOPEDIA         -14

// Arena (o HUD fica embaixo, o objetivo em cima)
static const Rectangle ARENA_ROGUE         = { 40, 60, 720, 470 };
static const Rectangle PORTA_PROXIMA_ROGUE = { 370, 60, 60, 14 };
static const Rectangle PORTA_SAIDA_ROGUE   = { 40, 260, 14, 70 };
static const Vector2   SPAWN_ROGUE         = { 400, 480 };   // entrando numa sala da missão
static const Vector2   SPAWN_ATRIO         = { 140, 295 };   // chegando do corredor

// Lugares fixos das salas especiais
static const Vector2 POS_BYTE      = { 300, 150 };
static const Vector2 POS_BYTECO    = { 610, 298 };
static const Vector2 POS_TERMINAL  = { 400, 270 };
static const Vector2 POS_PORTAL    = { 400, 180 };
static const Vector2 POS_ENCICLOPEDIA = { 180, 432 };
static const Vector2 POS_PEDESTAIS[NUM_PEDESTAIS] = { { 270, 290 }, { 400, 260 }, { 530, 290 } };

typedef enum { SALA_ATRIO, SALA_COMBATE, SALA_EDUCATIVA, SALA_RECOMPENSA, SALA_CHEFE } TipoSalaRogue;

typedef enum { INIMIGO_FRAGMENTO, INIMIGO_GUARDIAO, INIMIGO_ECO } TipoInimigo;

typedef enum {
    ITEM_ANTIVIRUS, ITEM_PACOTE_DADOS, ITEM_LENTE, ITEM_FIREWALL, ITEM_PROCESSADOR, ITEM_MANUAL, NUM_ITENS
} TipoItem;

typedef struct {
    const char *nome;
    const char *efeito;
    int preco;                  // em Fragmentos de Conhecimento (loja do Byteco)
    bool passivo;               // passivo = vale enquanto estiver na mochila (máx. MAX_PILHA_PASSIVO)
} DefItem;

static const DefItem ITENS[NUM_ITENS] = {
    { "ANTIVÍRUS",              "Recupera 40 de HP (tecla 1).",                  3, false },
    { "PACOTE DE DADOS",        "Recupera 50 de EP (tecla 2).",                  2, false },
    { "LENTE DA VERIFICAÇÃO",   "Dá uma dica num desafio educativo.",            4, false },
    { "FIREWALL",               "Reduz em 25% o dano recebido.",                 6, true  },
    { "PROCESSADOR NEURAL",     "Aumenta em 25% o dano dos disparos.",           6, true  },
    { "MANUAL DO CONHECIMENTO", "Segunda tentativa num desafio educativo.",      5, false },
};

static const char *NOMES_SALAS_ROGUE[] = { "ÁTRIO", "COMBATE", "EDUCATIVA", "RECOMPENSA", "ALUCINADOR" };
static const Color CORES_SALAS_ROGUE[] = {
    { 102, 191, 255, 255 }, { 230, 41, 55, 255 }, { 0, 228, 255, 255 }, { 255, 203, 0, 255 }, { 230, 41, 255, 255 }
};

typedef struct {
    bool ativo;
    Vector2 pos, vel;
    float raio, dano;
    bool doJogador;
} Projetil;

typedef struct {
    bool ativo;
    TipoInimigo tipo;
    Vector2 pos, vel, empurrao;
    float hp, hpMax, raio;
    int estado;                 // 0 = andando, 1 = preparando o ataque, 2 = atacando
    float timer, recarga, flash, anim, surgindo;
    Vector2 alvo;               // direção do bote / da mira
    float lado;                 // guardião: para que lado ele anda de lado
    bool selado;                // guardião com um desafio educativo: não leva dano até alguém responder
    float atordoado;            // acertou o desafio: o guardião fica parado
} Inimigo;

typedef struct {
    Vector2 pos, vel;
    float vida, vidaMax, tam;
    Color cor;
} ParticulaRogue;

// Coisas no chão: fragmentos de conhecimento e itens
typedef struct {
    bool ativo;
    bool fragmento;             // true = Fragmento de Conhecimento, false = item
    TipoItem item;
    Vector2 pos, vel;
    float anim;
} Coletavel;

typedef struct {
    float hp, ep;
    Vector2 pos;
    Direcao direcao;
    AnimacaoJogador anim;
    Vector2 mira;               // direção unitária
    bool miraSetas;             // a última mira veio das setas (esconde a mira do mouse)
    float cronoTiro, recargaEsquiva, recargaPulso, invulneravel, tempoEsquiva, efeitoPulso;
    Vector2 dirEsquiva;
    int itens[NUM_ITENS];       // mochila desta tentativa (perde ao morrer)
} JogadorRogue;

typedef struct {
    JogadorRogue j;
    ProgressoBiblioteca *perm;  // o que não se perde (fica no main e vai para o save)

    // Sala atual e missão
    TipoSalaRogue tipoSala;
    TipoSalaRogue rota[SALAS_POR_MISSAO];
    int indiceRota;             // sala da missão (-1 = átrio)
    int combates;               // salas de combate abertas nesta tentativa (cada uma tem mais inimigos)
    int educativas;             // terminais já vistos nesta tentativa (decide o conceito)
    bool limpa;                 // a porta de cima está aberta
    bool terminalLido;
    bool recompensaPega;
    TipoItem pedestal[NUM_PEDESTAIS];

    Projetil proj[MAX_PROJETEIS];
    Inimigo ini[MAX_INIMIGOS];
    ParticulaRogue part[MAX_PARTICULAS_ROGUE];
    Coletavel col[MAX_COLETAVEIS];
    Rectangle obst[MAX_OBSTACULOS];
    int numObst;

    // Loja do Byteco (aba 0 = itens, aba 1 = melhorias permanentes)
    bool lojaAberta;
    int selecaoLoja;
    int abaLoja;

    // Enciclopédia do átrio
    bool enciclopediaAberta;
    int selecaoEnciclopedia;

    // Desafio educativo aberto (o combate pausa)
    bool desafioAberto;
    int desafio;                // índice em DESAFIOS
    int alvoDesafio;            // guardião selado (índice em ini)
    int selecaoDesafio;
    float tempoDesafio;
    bool respondido;            // mostrando a explicação
    bool acertouDesafio;
    bool opcaoRiscada[4];       // eliminada pela Lente ou pela tentativa errada (Manual)
    char dicaDesafio[160];      // dica da Lente ou da Luci
    bool dicaDaLuci;            // a dica que está na tela veio da Luci
    bool luciMentiu;            // ... e era alucinada
    bool usouManual;
    unsigned int desafiosUsados;    // desafios desta tentativa (um bit cada)
    int desafiosRespondidos;        // nesta tentativa: a Luci fica menos confiável a cada um
    const char *falaPendente;       // fala da Luci para quando não houver outra conversa

    bool morreu;
    float tempoMorte, tremor, pausaImpacto, fadeEntrada;
    char aviso[96];             // mensagem curta no meio da tela ("+3 FRAGMENTOS")
    float tempoAviso;
    int abatidos;
    double inicio;              // telemetria
} Biblioteca;

// Salas de combate prontas (a sala nova sorteia uma): estantes e pontos onde os inimigos surgem
typedef struct {
    int numObst;
    Rectangle obst[MAX_OBSTACULOS];
    int numPontos;
    Vector2 pontos[8];
} LayoutSala;

static const LayoutSala LAYOUTS_COMBATE[] = {
    { 2, { {170, 180, 40, 140}, {590, 180, 40, 140} }, 8,
      { {120, 110}, {400, 110}, {680, 110}, {300, 200}, {500, 200}, {400, 300}, {120, 320}, {680, 320} } },
    { 4, { {140, 150, 120, 28}, {540, 150, 120, 28}, {140, 360, 120, 28}, {540, 360, 120, 28} }, 8,
      { {400, 100}, {110, 100}, {690, 100}, {400, 250}, {250, 260}, {550, 260}, {100, 260}, {700, 260} } },
    { 3, { {350, 170, 100, 30}, {190, 290, 30, 120}, {580, 290, 30, 120} }, 8,
      { {110, 100}, {690, 100}, {400, 110}, {260, 220}, {540, 220}, {400, 280}, {120, 360}, {680, 360} } },
};
#define NUM_LAYOUTS_COMBATE 3

// Estantes das salas que não são de combate (o Byte e o Byteco também bloqueiam a passagem)
static const LayoutSala LAYOUT_ATRIO      = { .numObst = 6, .obst = { {80, 100, 130, 26}, {590, 100, 130, 26}, {285, 132, 30, 40},
                                                                  {590, 280, 40, 36}, {555, 318, 110, 24}, {164, 420, 32, 26} } };
static const LayoutSala LAYOUT_EDUCATIVA  = { .numObst = 3, .obst = { {150, 140, 30, 220}, {620, 140, 30, 220}, {380, 250, 40, 34} } };
static const LayoutSala LAYOUT_RECOMPENSA = { .numObst = 2, .obst = { {110, 120, 140, 26}, {550, 120, 140, 26} } };
static const LayoutSala LAYOUT_CHEFE      = { .numObst = 0 };

// ---------- Falas da Biblioteca ----------

// Professor Byte: fundamentos da IA generativa e o que é alucinação (antes dos desafios)
static const Dialogo dialogoByteCompleto = {
    .paginas = {
        "Byte: A IA generativa é um tipo de inteligência artificial que CRIA conteúdo novo: textos, imagens, músicas e códigos.",
        "Byte: Ela aprende com uma quantidade enorme de exemplos. Ao responder, ela calcula qual é a próxima palavra mais provável, uma depois da outra.",
        "Byte: Ou seja: ela não consulta a verdade e nem 'sabe' o que é real. Ela produz o que PARECE certo pelos padrões que aprendeu.",
        "Byte: Limitações: os dados de treino podem estar errados ou desatualizados, ela não confere fatos sozinha e pode repetir preconceitos dos dados.",
        "Byte: Quando ela inventa algo com toda a confiança, chamamos de ALUCINAÇÃO. O Alucinador está espalhando ecos assim por todas as estantes!",
        "Byte: Procure os terminais de conhecimento lá dentro. E o Byteco troca Fragmentos de Conhecimento por itens úteis."
    },
    .numPaginas = 6
};

static const Dialogo dialogoByte = {
    .paginas = {
        "Byte: Bem-vindo à Biblioteca dos Ecos! Sou o Professor Byte. Aqui ficam as respostas que a IA já gerou.",
        "Byte: A IA generativa não consulta a verdade: ela aprendeu padrões e prevê a próxima palavra mais provável.",
        "Byte: Por isso ela pode ALUCINAR e inventar com confiança. O Alucinador espalhou esses ecos falsos por aqui!",
        "Byte: Recupere os Fragmentos de Conhecimento lá dentro. O Byteco troca os fragmentos por itens."
    },
    .numPaginas = 4,
    .saberMais = &dialogoByteCompleto
};

static const Dialogo dialogoByteDepois = {
    .paginas = { "Byte: Lembre: resposta convincente não é resposta verdadeira. Boa sorte lá dentro!" },
    .numPaginas = 1,
    .saberMais = &dialogoByteCompleto
};

static const Dialogo dialogoByteco = {
    .paginas = {
        "Byteco: Psiu! Fragmentos de Conhecimento? Eu troco por itens que ajudam lá dentro!",
        "Byteco: Mas atenção: se os ecos te derrubarem, os itens se perdem. Os fragmentos não."
    },
    .numPaginas = 2
};

// Terminais das salas educativas: um conceito por terminal, na ordem
static const Dialogo dialogoTerminalAlucinacaoCompleto = {
    .paginas = {
        "* TERMINAL // ALUCINAÇÕES DE IA (versão completa)",
        "* Uma alucinação é uma resposta coerente e confiante, mas falsa, sem base nos dados ou inventada.",
        "* INTRÍNSECA: a resposta contradiz o que o usuário ou o contexto informou. EXTRÍNSECA: a resposta inventa algo que não dá para verificar.",
        "* Por que acontece? Dados de treino errados ou desatualizados, perguntas vagas que deixam lacunas e atalhos de raciocínio (juntar coisas que só aparecem juntas).",
        "* O modelo é treinado para sempre responder algo plausível. Quando falta informação, ele preenche a lacuna com o que parece provável.",
        "* Por isso, uma resposta bem escrita e segura NÃO é prova de verdade."
    },
    .numPaginas = 6
};

static const Dialogo dialogoTerminalAlucinacao = {
    .paginas = {
        "* TERMINAL // ALUCINAÇÕES DE IA",
        "* Alucinação: uma resposta coerente e confiante, mas falsa ou inventada.",
        "* Quando falta informação, o modelo preenche a lacuna com o que parece provável.",
        "* Resposta convincente não é resposta verdadeira: o tom seguro não prova nada."
    },
    .numPaginas = 4,
    .saberMais = &dialogoTerminalAlucinacaoCompleto
};

static const Dialogo dialogoTerminalPraticasCompleto = {
    .paginas = {
        "* TERMINAL // BOAS PRÁTICAS (versão completa)",
        "* Prompt bom: diga o assunto, o objetivo, o contexto e o formato que você quer. Quanto menos lacuna, menos invenção.",
        "* Verifique: confira nomes, datas, números e citações em fontes confiáveis, como sites oficiais, livros e especialistas.",
        "* Peça as fontes para a IA, mas confira se elas existem de verdade: a própria fonte também pode ser inventada.",
        "* Técnicas como o RAG ligam a IA a uma base de dados confiável, o que reduz as alucinações, mas não acaba com elas.",
        "* A IA é uma ferramenta de apoio. A decisão final, e a responsabilidade, são suas."
    },
    .numPaginas = 6
};

static const Dialogo dialogoTerminalPraticas = {
    .paginas = {
        "* TERMINAL // BOAS PRÁTICAS",
        "* Faça perguntas claras e com contexto: assunto, objetivo e formato.",
        "* Verifique a resposta em fontes confiáveis, como sites oficiais, livros e especialistas.",
        "* Nunca confie cegamente: a IA ajuda, mas quem decide é você."
    },
    .numPaginas = 4,
    .saberMais = &dialogoTerminalPraticasCompleto
};

static const Dialogo dialogoPortalChefe = {
    .paginas = {
        "* O portal pulsa em magenta. Lá dentro, o ALUCINADOR ainda está se formando...",
        "(O combate contra o Alucinador chega na Etapa 4. Por enquanto, a missão termina aqui.)"
    },
    .numPaginas = 2
};

// ---------- Conceitos (enciclopédia) ----------
// O Byte ensina o 1; os terminais, na ordem, o 2 e o 3. Descoberto, nunca se perde.

enum { CONCEITO_FUNDAMENTOS, CONCEITO_ALUCINACOES, CONCEITO_PRATICAS, NUM_CONCEITOS };

static const char *NOMES_CONCEITOS[NUM_CONCEITOS] = {
    "FUNDAMENTOS DA IA GENERATIVA", "ALUCINAÇÕES DE IA", "BOAS PRÁTICAS"
};

static const char *RESUMOS_CONCEITOS[NUM_CONCEITOS] = {
    "A IA generativa cria conteúdo prevendo a próxima palavra mais provável. Ela não consulta a verdade.",
    "Alucinação é uma resposta confiante e coerente, mas falsa ou inventada. Tom seguro não é prova.",
    "Pergunte com clareza e contexto, confira em fontes confiáveis e nunca confie cegamente."
};

static const Dialogo *TEXTOS_CONCEITOS[NUM_CONCEITOS] = {
    &dialogoByteCompleto, &dialogoTerminalAlucinacaoCompleto, &dialogoTerminalPraticasCompleto
};

// ---------- Melhorias permanentes (Byteco) ----------

enum { MELHORIA_VIDA, MELHORIA_ATAQUE, MELHORIA_ENERGIA, NUM_MELHORIAS };

static const char *NOMES_MELHORIAS[NUM_MELHORIAS]   = { "MEMÓRIA EXPANDIDA", "NÚCLEO DE PROCESSAMENTO", "BATERIA DE DADOS" };
static const char *EFEITOS_MELHORIAS[NUM_MELHORIAS] = { "+20 de HP máximo por nível.", "+15% de dano por nível.", "+20 de EP máximo por nível." };

// ---------- Desafios educativos (Guardião selado) ----------
// Verdadeiro ou falso, múltipla escolha e "ache a inconsistência" numa resposta de IA.
// Só aparecem desafios de conceitos que o jogador já viu.

typedef enum { DESAFIO_VF, DESAFIO_ESCOLHA, DESAFIO_INCONSISTENCIA } TipoDesafio;

static const char *NOMES_TIPOS_DESAFIO[] = { "VERDADEIRO OU FALSO?", "MÚLTIPLA ESCOLHA", "ACHE A INCONSISTÊNCIA" };

typedef struct {
    int conceito;
    TipoDesafio tipo;
    const char *enunciado;
    const char *opcoes[4];
    int numOpcoes;
    int correta;
    const char *dica;           // empurrão no raciocínio (Lente ou Luci confiável), nunca a resposta
    const char *explicacao;     // aparece depois de qualquer resposta
} DesafioEdu;

#define OPCOES_VF { "VERDADEIRO", "FALSO" }

static const DesafioEdu DESAFIOS[] = {
    { CONCEITO_FUNDAMENTOS, DESAFIO_VF,
      "\"Toda resposta produzida por uma IA generativa é baseada em informações verificadas.\"",
      OPCOES_VF, 2, 1,
      "Lembre o que o Byte disse: a IA prevê ou confere?",
      "Falso. A IA generativa prevê a palavra mais provável a partir de padrões. Ela não verifica os fatos sozinha." },

    { CONCEITO_FUNDAMENTOS, DESAFIO_ESCOLHA,
      "Como um modelo de IA generativa produz uma resposta?",
      { "Consulta um banco com todas as verdades", "Prevê a próxima palavra mais provável, uma depois da outra",
        "Copia a resposta de um especialista humano", "Pesquisa sempre na internet antes de responder" }, 4, 1,
      "Pense em probabilidade, não em consulta.",
      "Ela gera o texto palavra por palavra, escolhendo o que é mais provável pelos padrões que aprendeu." },

    { CONCEITO_FUNDAMENTOS, DESAFIO_VF,
      "\"Se os dados de treino estiverem desatualizados, a IA pode repetir informações velhas como se fossem atuais.\"",
      OPCOES_VF, 2, 0,
      "De onde a IA tirou o que sabe?",
      "Verdadeiro. A IA só conhece o que estava nos dados de treino. Dado velho ou errado vira resposta velha ou errada." },

    { CONCEITO_ALUCINACOES, DESAFIO_VF,
      "\"Uma resposta bem escrita e confiante tem mais chance de ser verdadeira.\"",
      OPCOES_VF, 2, 1,
      "O tom da resposta prova alguma coisa?",
      "Falso. A alucinação costuma ser coerente e segura. Resposta convincente não é resposta verdadeira." },

    { CONCEITO_ALUCINACOES, DESAFIO_INCONSISTENCIA,
      "Você disse: \"Minha reunião é na QUINTA às 10h.\" A IA respondeu:\n"
      "(1) Anotei a sua reunião.  (2) Ela será na SEXTA às 10h.  (3) Quer que eu crie um lembrete?\n"
      "Qual frase é a alucinação?",
      { "Frase 1", "Frase 2", "Frase 3" }, 3, 1,
      "Compare cada frase com o que VOCÊ disse.",
      "A frase 2 contradiz o dia que você informou. Contradizer o contexto é alucinação INTRÍNSECA." },

    { CONCEITO_ALUCINACOES, DESAFIO_ESCOLHA,
      "A IA citou o \"Instituto Nacional de Gatos Falantes\", que não existe. Que tipo de alucinação é essa?",
      { "Intrínseca: contradiz o que você disse", "Extrínseca: inventa algo que não dá para verificar", "Não é alucinação" }, 3, 1,
      "Ela contradisse você ou inventou algo de fora?",
      "Inventar uma fonte que não existe é alucinação EXTRÍNSECA: não contradiz o contexto, mas não dá para verificar." },

    { CONCEITO_PRATICAS, DESAFIO_ESCOLHA,
      "Qual pedido deixa MENOS lacunas para a IA inventar?",
      { "\"Fala daquilo lá.\"", "\"Resuma em 5 tópicos a Revolução Francesa para uma prova do 9º ano.\"",
        "\"Me conta tudo.\"", "\"Explica história.\"" }, 4, 1,
      "Qual deles diz assunto, objetivo e formato?",
      "Um bom prompt diz o assunto, o objetivo e o formato. Quanto menos lacuna, menos a IA precisa inventar." },

    { CONCEITO_PRATICAS, DESAFIO_VF,
      "\"Se a IA mostrar uma fonte, já posso usar a resposta sem conferir.\"",
      OPCOES_VF, 2, 1,
      "A fonte também pode ter sido... gerada?",
      "Falso. A própria fonte pode ser inventada. Confira se ela existe e se diz mesmo aquilo." },
};
#define NUM_DESAFIOS 8

// A Luci vai se contradizendo ao longo da tentativa (entrando nas salas educativas)
static const char *FALAS_LUCI_CORROMPIDA[] = {
    "Luci: Mais um terminal! Pode pular: resposta confiante é resposta certa... Espera. Não era o contrário?",
    "Luci: O Byte disse que a IA confere os fatos sozinha. Ou disse que NÃO confere? Meus dados estão... embaralhados.",
    "Luci: Eu nunca alucino. Eu nunca... Thomas, confira tudo o que eu disser, tá? Tudo."
};

// ---------- Utilidades ----------

// Direção do sprite a partir da mira (8 direções; y cresce para baixo)
static Direcao DirecaoDoVetor(Vector2 v)
{
    static const Direcao setores[8] = {
        DIR_DIREITA, DIR_FRENTE_DIREITA, DIR_FRENTE, DIR_FRENTE_ESQUERDA,
        DIR_ESQUERDA, DIR_COSTAS_ESQUERDA, DIR_COSTAS, DIR_COSTAS_DIREITA
    };

    int s = (int)roundf(atan2f(v.y, v.x) / (PI / 4.0f));
    return setores[(s + 8) % 8];
}

// Mantém um círculo dentro da arena e fora das estantes
static void RogueEmpurrarParaFora(Vector2 *p, float raio, const Rectangle *obst, int n)
{
    p->x = Clamp(p->x, ARENA_ROGUE.x + raio, ARENA_ROGUE.x + ARENA_ROGUE.width - raio);
    p->y = Clamp(p->y, ARENA_ROGUE.y + raio, ARENA_ROGUE.y + ARENA_ROGUE.height - raio);

    for (int i = 0; i < n; i++) {

        Rectangle r = obst[i];
        float cx = Clamp(p->x, r.x, r.x + r.width);
        float cy = Clamp(p->y, r.y, r.y + r.height);
        float dx = p->x - cx, dy = p->y - cy;
        float d2 = dx * dx + dy * dy;

        if (d2 >= raio * raio)
            continue;

        if (d2 > 0.0001f) {
            float d = sqrtf(d2);
            p->x = cx + dx / d * raio;
            p->y = cy + dy / d * raio;
        } else {
            // Centro dentro da estante: sai pelo lado mais perto
            float esq = p->x - r.x, dir = r.x + r.width - p->x;
            float cima = p->y - r.y, baixo = r.y + r.height - p->y;
            float m = fminf(fminf(esq, dir), fminf(cima, baixo));

            if (m == esq)       p->x = r.x - raio;
            else if (m == dir)  p->x = r.x + r.width + raio;
            else if (m == cima) p->y = r.y - raio;
            else                p->y = r.y + r.height + raio;
        }
    }
}

static void RogueParticulas(Biblioteca *b, Vector2 origem, Color cor, int quantidade, float velMax)
{
    for (int i = 0; i < MAX_PARTICULAS_ROGUE && quantidade > 0; i++) {

        ParticulaRogue *p = &b->part[i];

        if (p->vida > 0.0f)
            continue;

        float ang = (float)GetRandomValue(0, 360) * DEG2RAD;
        float vel = (float)GetRandomValue(20, (int)velMax);

        p->pos = origem;
        p->vel = (Vector2){ cosf(ang) * vel, sinf(ang) * vel };
        p->vidaMax = p->vida = 0.25f + (float)GetRandomValue(0, 30) / 100.0f;
        p->tam = (float)GetRandomValue(2, 4);
        p->cor = cor;
        quantidade--;
    }
}

static void RogueAviso(Biblioteca *b, const char *texto)
{
    snprintf(b->aviso, sizeof(b->aviso), "%s", texto);
    b->tempoAviso = 1.8f;
}

static void RogueDisparar(Biblioteca *b, Vector2 origem, Vector2 dir, float vel, float dano, float raio, bool doJogador)
{
    for (int i = 0; i < MAX_PROJETEIS; i++) {

        if (b->proj[i].ativo)
            continue;

        b->proj[i] = (Projetil){ true, origem, Vector2Scale(dir, vel), raio, dano, doJogador };
        return;
    }
}

static void RogueSoltar(Biblioteca *b, Vector2 pos, bool fragmento, TipoItem item)
{
    for (int i = 0; i < MAX_COLETAVEIS; i++) {

        if (b->col[i].ativo)
            continue;

        float ang = (float)GetRandomValue(0, 360) * DEG2RAD;
        b->col[i] = (Coletavel){ true, fragmento, item, pos, { cosf(ang) * 90.0f, sinf(ang) * 90.0f }, 0.0f };
        return;
    }
}

static int RogueInimigosVivos(const Biblioteca *b)
{
    int n = 0;

    for (int i = 0; i < MAX_INIMIGOS; i++)
        if (b->ini[i].ativo)
            n++;

    return n;
}

// Atributos: base + melhorias permanentes (Byteco) + itens passivos da mochila
static float RogueHpMax(const Biblioteca *b)
{
    return ROGUE_HP_MAX + MELHORIA_HP * (float)b->perm->melhorias[MELHORIA_VIDA];
}

static float RogueEpMax(const Biblioteca *b)
{
    return ROGUE_EP_MAX + MELHORIA_EP * (float)b->perm->melhorias[MELHORIA_ENERGIA];
}

static float RogueDanoTiro(const Biblioteca *b)
{
    int n = (b->j.itens[ITEM_PROCESSADOR] > MAX_PILHA_PASSIVO) ? MAX_PILHA_PASSIVO : b->j.itens[ITEM_PROCESSADOR];
    return TIRO_DANO * (1.0f + BONUS_PROCESSADOR * (float)n + MELHORIA_DANO * (float)b->perm->melhorias[MELHORIA_ATAQUE]);
}

static float RogueFatorDanoRecebido(const JogadorRogue *j)
{
    int n = (j->itens[ITEM_FIREWALL] > MAX_PILHA_PASSIVO) ? MAX_PILHA_PASSIVO : j->itens[ITEM_FIREWALL];
    return 1.0f - REDUCAO_FIREWALL * (float)n;
}

static Inimigo *RogueCriarInimigo(Biblioteca *b, TipoInimigo tipo, Vector2 pos)
{
    for (int i = 0; i < MAX_INIMIGOS; i++) {

        Inimigo *e = &b->ini[i];

        if (e->ativo)
            continue;

        *e = (Inimigo){0};
        e->ativo = true;
        e->tipo = tipo;
        e->pos = pos;
        e->surgindo = TEMPO_SURGINDO;
        e->lado = (GetRandomValue(0, 1) == 0) ? -1.0f : 1.0f;
        e->recarga = (float)GetRandomValue(30, 120) / 100.0f;   // não atacam todos juntos

        if (tipo == INIMIGO_FRAGMENTO)     { e->hp = FRAGMENTO_HP; e->raio = FRAGMENTO_RAIO; }
        else if (tipo == INIMIGO_GUARDIAO) { e->hp = GUARDIAO_HP;  e->raio = GUARDIAO_RAIO; }
        else                               { e->hp = ECO_HP; e->raio = 12.0f; e->timer = ECO_TEMPO; e->surgindo = 0.0f; }

        e->hpMax = e->hp;
        return e;
    }

    return NULL;
}

// ---------- Salas e missão ----------

static void RogueUsarLayout(Biblioteca *b, const LayoutSala *l)
{
    b->numObst = l->numObst;
    for (int i = 0; i < l->numObst; i++)
        b->obst[i] = l->obst[i];
}

// Esvazia a sala e coloca o jogador na entrada
static void RogueLimparSala(Biblioteca *b, TipoSalaRogue tipo, Vector2 entrada)
{
    memset(b->proj, 0, sizeof(b->proj));
    memset(b->ini, 0, sizeof(b->ini));
    memset(b->col, 0, sizeof(b->col));

    b->tipoSala = tipo;
    b->limpa = false;
    b->terminalLido = false;
    b->recompensaPega = false;
    b->lojaAberta = false;
    b->fadeEntrada = 0.35f;
    b->j.pos = entrada;
    b->j.tempoEsquiva = 0.0f;
}

static void RogueEntrarSala(Biblioteca *b, TipoSalaRogue tipo)
{
    RogueLimparSala(b, tipo, SPAWN_ROGUE);

    if (tipo == SALA_COMBATE) {

        const LayoutSala *l = &LAYOUTS_COMBATE[GetRandomValue(0, NUM_LAYOUTS_COMBATE - 1)];
        RogueUsarLayout(b, l);
        b->combates++;

        // Posições sorteadas: embaralha os pontos da sala
        int ordem[8];
        for (int i = 0; i < l->numPontos; i++)
            ordem[i] = i;

        for (int i = l->numPontos - 1; i > 0; i--) {
            int k = GetRandomValue(0, i);
            int t = ordem[i]; ordem[i] = ordem[k]; ordem[k] = t;
        }

        // Cada combate tem um inimigo a mais (até encher os pontos)
        int total = 2 + b->combates;
        if (total > l->numPontos) total = l->numPontos;

        int guardioes = total / 3;

        for (int i = 0; i < total; i++) {
            Inimigo *e = RogueCriarInimigo(b, (i < guardioes) ? INIMIGO_GUARDIAO : INIMIGO_FRAGMENTO, l->pontos[ordem[i]]);

            // A partir do 2º combate, um guardião vem selado com um desafio educativo
            if (e != NULL && i == 0 && b->combates >= 2)
                e->selado = true;
        }
    }
    else if (tipo == SALA_EDUCATIVA) {
        RogueUsarLayout(b, &LAYOUT_EDUCATIVA);

        // Depois de alguns desafios, a Luci começa a se contradizer
        if (b->desafiosRespondidos > 0) {
            int nivel = (b->desafiosRespondidos > 3) ? 3 : b->desafiosRespondidos;
            b->falaPendente = FALAS_LUCI_CORROMPIDA[nivel - 1];
        }
    }
    else if (tipo == SALA_RECOMPENSA) {

        RogueUsarLayout(b, &LAYOUT_RECOMPENSA);
        b->limpa = true;                        // pode pegar uma recompensa ou seguir em frente

        // Três itens diferentes sorteados (passivo que já está no máximo não aparece)
        int ordem[NUM_ITENS], n = 0;
        for (int i = 0; i < NUM_ITENS; i++)
            if (!(ITENS[i].passivo && b->j.itens[i] >= MAX_PILHA_PASSIVO))
                ordem[n++] = i;

        for (int i = n - 1; i > 0; i--) {
            int k = GetRandomValue(0, i);
            int t = ordem[i]; ordem[i] = ordem[k]; ordem[k] = t;
        }

        for (int i = 0; i < NUM_PEDESTAIS; i++)
            b->pedestal[i] = (TipoItem)ordem[i];
    }
    else {
        RogueUsarLayout(b, &LAYOUT_CHEFE);
    }
}

// Volta para o átrio (chegando do corredor, depois da derrota ou no fim da missão)
static void RogueEntrarAtrio(Biblioteca *b, bool vindoDoCorredor)
{
    RogueLimparSala(b, SALA_ATRIO, vindoDoCorredor ? SPAWN_ATRIO : (Vector2){ 400, 420 });
    RogueUsarLayout(b, &LAYOUT_ATRIO);

    b->indiceRota = -1;
    b->limpa = true;
    b->j.hp = RogueHpMax(b);
    b->j.ep = RogueEpMax(b);
    b->j.direcao = vindoDoCorredor ? DIR_DIREITA : DIR_COSTAS;
}

// Sorteia a ordem das salas: a primeira é sempre de combate
static void RogueComecarMissao(Biblioteca *b)
{
    TipoSalaRogue resto[SALAS_POR_MISSAO - 1] = { SALA_COMBATE, SALA_COMBATE, SALA_EDUCATIVA, SALA_EDUCATIVA, SALA_RECOMPENSA };

    for (int i = SALAS_POR_MISSAO - 2; i > 0; i--) {
        int k = GetRandomValue(0, i);
        TipoSalaRogue t = resto[i]; resto[i] = resto[k]; resto[k] = t;
    }

    b->rota[0] = SALA_COMBATE;
    for (int i = 1; i < SALAS_POR_MISSAO; i++)
        b->rota[i] = resto[i - 1];

    b->indiceRota = 0;
    b->combates = 0;
    b->educativas = 0;
    b->abatidos = 0;
    b->j.hp = RogueHpMax(b);
    b->j.ep = RogueEpMax(b);
    b->desafiosUsados = 0;
    b->desafiosRespondidos = 0;
    b->falaPendente = NULL;
    b->inicio = GetTime();
    b->perm->tentativas++;

    Telemetria("rogue_missao", "inicio", 0.0f, b->perm->tentativas);
    RogueEntrarSala(b, b->rota[0]);
}

static void RogueAvancar(Biblioteca *b)
{
    b->indiceRota++;
    RogueEntrarSala(b, (b->indiceRota < SALAS_POR_MISSAO) ? b->rota[b->indiceRota] : SALA_CHEFE);
}

// Primeira vez na Biblioteca nesta sessão (ou recomeço sem nada na mochila)
static void RogueIniciar(Biblioteca *b, ProgressoBiblioteca *perm)
{
    memset(b, 0, sizeof(*b));

    b->perm = perm;
    b->j.mira = (Vector2){ 1, 0 };
    b->inicio = GetTime();

    RogueEntrarAtrio(b, true);
}

// ---------- Desafios educativos ----------

// Chance (%) de a dica da Luci sair errada: cresce a cada desafio respondido nesta tentativa
static int RogueChanceLuciErrar(const Biblioteca *b)
{
    int c = CHANCE_LUCI_ERRAR * b->desafiosRespondidos;
    return (c > CHANCE_LUCI_ERRAR_MAX) ? CHANCE_LUCI_ERRAR_MAX : c;
}

// Quanto a Luci está "corrompida" (0 a 1): deixa ela magenta e com glitch
static float RogueCorrupcaoLuci(const Biblioteca *b)
{
    return (float)RogueChanceLuciErrar(b) / 100.0f;
}

// Um desafio de conceito já descoberto (de preferência um que ainda não caiu nesta tentativa)
static int RogueSortearDesafio(const Biblioteca *b)
{
    int candidatos[NUM_DESAFIOS], n = 0;

    for (int passada = 0; passada < 2 && n == 0; passada++)
        for (int i = 0; i < NUM_DESAFIOS; i++) {
            if (!(b->perm->conceitos & (1u << DESAFIOS[i].conceito)))
                continue;
            if (passada == 0 && (b->desafiosUsados & (1u << i)))
                continue;
            candidatos[n++] = i;
        }

    return (n > 0) ? candidatos[GetRandomValue(0, n - 1)] : 0;    // (o Byte sempre ensina o 1º conceito)
}

static void RogueAbrirDesafio(Biblioteca *b, int indiceInimigo)
{
    b->desafioAberto = true;
    b->alvoDesafio = indiceInimigo;
    b->desafio = RogueSortearDesafio(b);
    b->desafiosUsados |= 1u << b->desafio;
    b->selecaoDesafio = 0;
    b->tempoDesafio = 0.0f;
    b->respondido = false;
    b->acertouDesafio = false;
    b->usouManual = false;
    b->dicaDesafio[0] = '\0';
    b->dicaDaLuci = false;
    b->luciMentiu = false;
    memset(b->opcaoRiscada, 0, sizeof(b->opcaoRiscada));

    TocarSomTom(sfxSelecao, 0.8f);
}

// Move a seleção pulando as opções riscadas
static void RogueMoverSelecaoDesafio(Biblioteca *b, int passo)
{
    int n = DESAFIOS[b->desafio].numOpcoes;

    for (int k = 0; k < n; k++) {
        b->selecaoDesafio = (b->selecaoDesafio + passo + n) % n;
        if (!b->opcaoRiscada[b->selecaoDesafio])
            return;
    }
}

// Lente da Verificação: dica confiável (e, com 3 ou mais opções, risca uma errada)
static void RogueUsarLente(Biblioteca *b)
{
    const DesafioEdu *d = &DESAFIOS[b->desafio];

    if (b->j.itens[ITEM_LENTE] <= 0 || b->respondido)
        return;

    b->j.itens[ITEM_LENTE]--;

    if (d->numOpcoes > 2) {
        int erradas[4], n = 0;
        for (int i = 0; i < d->numOpcoes; i++)
            if (i != d->correta && !b->opcaoRiscada[i])
                erradas[n++] = i;
        if (n > 0)
            b->opcaoRiscada[erradas[GetRandomValue(0, n - 1)]] = true;
        if (b->opcaoRiscada[b->selecaoDesafio])
            RogueMoverSelecaoDesafio(b, 1);
    }

    snprintf(b->dicaDesafio, sizeof(b->dicaDesafio), "LENTE: %s", d->dica);
    b->dicaDaLuci = false;
    TocarSomTom(sfxPegar, 1.3f);
    Telemetria("rogue_lente", "", 0.0f, b->desafio);
}

// H: a Luci dá uma dica de graça, uma vez por desafio. Mas ela pode ALUCINAR
// (jurar que uma opção errada é a certa), e isso fica mais comum a cada desafio.
static void RoguePedirLuci(Biblioteca *b)
{
    const DesafioEdu *d = &DESAFIOS[b->desafio];

    if (b->dicaDaLuci || b->respondido)
        return;

    b->dicaDaLuci = true;
    b->luciMentiu = GetRandomValue(1, 100) <= RogueChanceLuciErrar(b);

    if (b->luciMentiu) {
        int errada = GetRandomValue(0, d->numOpcoes - 2);
        if (errada >= d->correta) errada++;
        snprintf(b->dicaDesafio, sizeof(b->dicaDesafio), "Luci: Tenho 100%% de certeza: é \"%s\"!", d->opcoes[errada]);
        MostrarEmocaoLuci(EMOCAO_ALERTA, 2.0f);
    } else {
        snprintf(b->dicaDesafio, sizeof(b->dicaDesafio), "Luci: %s", d->dica);
    }

    TocarSom(sfxSelecao);
    Telemetria("rogue_dica_luci", b->luciMentiu ? "alucinou" : "dica", (float)RogueChanceLuciErrar(b), b->desafio);
}

static void RogueResponderDesafio(Biblioteca *b, ProgressoCorredor *prog)
{
    const DesafioEdu *d = &DESAFIOS[b->desafio];
    int s = b->selecaoDesafio;

    if (b->opcaoRiscada[s])
        return;

    if (s != d->correta && b->j.itens[ITEM_MANUAL] > 0 && !b->usouManual) {

        // Manual do Conhecimento: o erro não conta, a opção errada fica riscada
        b->j.itens[ITEM_MANUAL]--;
        b->usouManual = true;
        b->opcaoRiscada[s] = true;
        RogueMoverSelecaoDesafio(b, 1);
        snprintf(b->dicaDesafio, sizeof(b->dicaDesafio), "MANUAL DO CONHECIMENTO: não foi essa. Você tem outra chance!");
        b->dicaDaLuci = false;
        TocarSom(sfxErro);
        Telemetria("rogue_manual", "", 0.0f, b->desafio);
        return;
    }

    b->respondido = true;
    b->acertouDesafio = (s == d->correta);
    b->desafiosRespondidos++;

    if (b->acertouDesafio) {
        AlterarPontos(prog, PONTOS_DESAFIO);
        b->perm->fragmentos += FC_POR_DESAFIO;
        TocarSom(sfxAcerto);
    } else {
        AlterarPontos(prog, -PENALIDADE_ERRO);
        TocarSom(sfxErro);
    }

    Telemetria("rogue_desafio", b->acertouDesafio ? "certo" : "errado",
               (float)((b->dicaDaLuci ? 1 : 0) + (b->luciMentiu ? 2 : 0)), b->desafio);
}

// Depois da explicação: o selo quebra. Acertou = o guardião fica fraco e atordoado;
// errou = ele usa a informação falsa num ataque especial (anel de balas).
static void RogueFecharDesafio(Biblioteca *b)
{
    Inimigo *e = &b->ini[b->alvoDesafio];

    b->desafioAberto = false;

    if (!e->ativo)
        return;

    e->selado = false;

    if (b->acertouDesafio) {
        e->hp -= e->hpMax * DANO_SELO_CERTO;
        e->flash = 0.3f;
        e->estado = 0;
        e->atordoado = ATORDOADO_SELO;
        e->recarga = ATORDOADO_SELO + 0.5f;
        RogueParticulas(b, e->pos, GOLD, 24, 220);
        RogueAviso(b, "SELO QUEBRADO! O guardião está atordoado.");
    } else {
        for (int k = 0; k < BALAS_ATAQUE_ESPECIAL; k++) {
            float ang = (2.0f * PI / BALAS_ATAQUE_ESPECIAL) * k;
            RogueDisparar(b, e->pos, (Vector2){ cosf(ang), sinf(ang) }, 190.0f, 9.0f, 6.0f, false);
        }
        e->estado = 0;
        e->recarga = GUARDIAO_INTERVALO;
        RogueParticulas(b, e->pos, MAGENTA, 24, 220);
        RogueAviso(b, "O guardião usa a informação falsa contra você!");
    }
}

static void RogueAtualizarDesafio(Biblioteca *b, ProgressoCorredor *prog, float dt)
{
    const DesafioEdu *d = &DESAFIOS[b->desafio];
    bool confirmar = IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);

    b->tempoDesafio += dt;

    if (b->tempoDesafio < 0.4f)             // evita responder sem querer
        return;

    if (b->respondido) {
        if (confirmar)
            RogueFecharDesafio(b);
        return;
    }

    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))   { RogueMoverSelecaoDesafio(b, -1); TocarSom(sfxSelecao); }
    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) { RogueMoverSelecaoDesafio(b, 1);  TocarSom(sfxSelecao); }

    for (int i = 0; i < d->numOpcoes; i++)
        if (IsKeyPressed(KEY_ONE + i) && !b->opcaoRiscada[i]) {
            b->selecaoDesafio = i;
            confirmar = true;
        }

    if (IsKeyPressed(KEY_L)) RogueUsarLente(b);
    if (IsKeyPressed(KEY_H)) RoguePedirLuci(b);

    if (confirmar)
        RogueResponderDesafio(b, prog);
}

// ---------- Dano ----------

static void RogueFerirJogador(Biblioteca *b, float dano)
{
    JogadorRogue *j = &b->j;

    if (j->invulneravel > 0.0f || j->tempoEsquiva > 0.0f || b->morreu)
        return;

    j->hp -= dano * RogueFatorDanoRecebido(j);
    j->invulneravel = INVULNERAVEL_APOS_DANO;
    b->tremor = 0.25f;                          // a tela só treme quando o jogador toma dano

    RogueParticulas(b, j->pos, RED, 10, 160);
    TocarSom(sfxDano);
}

static void RogueFerirInimigo(Biblioteca *b, ProgressoCorredor *prog, Inimigo *e, float dano, Vector2 direcao)
{
    // Selado: o golpe bate no selo e abre o desafio
    if (e->selado) {
        RogueParticulas(b, e->pos, GOLD, 4, 100);
        if (!b->desafioAberto)
            RogueAbrirDesafio(b, (int)(e - b->ini));
        return;
    }

    e->hp -= dano;
    e->flash = 0.1f;
    e->empurrao = Vector2Add(e->empurrao, Vector2Scale(direcao, 140.0f));

    RogueParticulas(b, e->pos, (e->tipo == INIMIGO_GUARDIAO) ? SKYBLUE : MAGENTA, 4, 120);

    if (e->hp > 0.0f)
        return;

    // Derrotado
    Vector2 onde = e->pos;
    TipoInimigo tipo = e->tipo;

    e->ativo = false;
    b->abatidos++;
    b->pausaImpacto = 0.04f;

    RogueParticulas(b, onde, (tipo == INIMIGO_GUARDIAO) ? SKYBLUE : MAGENTA, 18, 220);
    RogueParticulas(b, onde, WHITE, 6, 160);
    TocarSomTom(sfxPegar, (tipo == INIMIGO_GUARDIAO) ? 0.8f : 1.2f);

    if (tipo == INIMIGO_ECO)
        return;

    AlterarPontos(prog, (tipo == INIMIGO_GUARDIAO) ? PONTOS_GUARDIAO : PONTOS_FRAGMENTO);

    // Recursos: dado corrompido derrotado libera conhecimento
    int chanceFC = (tipo == INIMIGO_GUARDIAO) ? CHANCE_FC_GUARDIAO : CHANCE_FC_FRAGMENTO;
    if (GetRandomValue(1, 100) <= chanceFC)         RogueSoltar(b, onde, true, ITEM_ANTIVIRUS);
    if (GetRandomValue(1, 100) <= CHANCE_ANTIVIRUS) RogueSoltar(b, onde, false, ITEM_ANTIVIRUS);

    if (tipo == INIMIGO_FRAGMENTO && GetRandomValue(1, 100) <= CHANCE_ECO_FALSO)
        RogueCriarInimigo(b, INIMIGO_ECO, onde);          // a informação falsa ecoa
}

static void RoguePulso(Biblioteca *b, ProgressoCorredor *prog)
{
    JogadorRogue *j = &b->j;

    if (j->recargaPulso > 0.0f || j->ep < PULSO_CUSTO_EP)
        return;

    j->ep -= PULSO_CUSTO_EP;
    j->recargaPulso = PULSO_RECARGA;
    j->efeitoPulso = 0.4f;

    // Apaga as balas inimigas por perto
    for (int i = 0; i < MAX_PROJETEIS; i++) {
        Projetil *p = &b->proj[i];
        if (p->ativo && !p->doJogador && Vector2Distance(p->pos, j->pos) < PULSO_RAIO) {
            p->ativo = false;
            RogueParticulas(b, p->pos, SKYBLUE, 3, 80);
        }
    }

    // Fere, empurra e interrompe os ataques de quem está perto
    for (int i = 0; i < MAX_INIMIGOS; i++) {

        Inimigo *e = &b->ini[i];

        if (!e->ativo || Vector2Distance(e->pos, j->pos) > PULSO_RAIO + e->raio)
            continue;

        Vector2 dir = Vector2Normalize(Vector2Subtract(e->pos, j->pos));

        if (e->tipo != INIMIGO_ECO) {
            e->estado = 0;
            e->recarga = (e->tipo == INIMIGO_GUARDIAO) ? GUARDIAO_INTERVALO : FRAGMENTO_RECARGA;
            e->empurrao = Vector2Add(e->empurrao, Vector2Scale(dir, 260.0f));
        }

        RogueFerirInimigo(b, prog, e, PULSO_DANO, dir);
    }

    TocarSomTom(sfxAcerto, 0.7f);
}

// ---------- Atualização ----------

static void RogueAtualizarJogador(Biblioteca *b, ProgressoCorredor *prog, float dt)
{
    JogadorRogue *j = &b->j;

    // Movimento (WASD)
    Vector2 mov = {0};
    if (IsKeyDown(KEY_D)) mov.x += 1.0f;
    if (IsKeyDown(KEY_A)) mov.x -= 1.0f;
    if (IsKeyDown(KEY_S)) mov.y += 1.0f;
    if (IsKeyDown(KEY_W)) mov.y -= 1.0f;

    bool andando = Vector2Length(mov) > 0.0f;
    if (andando) mov = Vector2Normalize(mov);

    // Mira: setas (mira e atira) ou mouse
    Vector2 setas = {0};
    if (IsKeyDown(KEY_RIGHT)) setas.x += 1.0f;
    if (IsKeyDown(KEY_LEFT))  setas.x -= 1.0f;
    if (IsKeyDown(KEY_DOWN))  setas.y += 1.0f;
    if (IsKeyDown(KEY_UP))    setas.y -= 1.0f;

    bool usandoSetas = Vector2Length(setas) > 0.0f;

    if (usandoSetas) {
        j->mira = Vector2Normalize(setas);
        j->miraSetas = true;
    } else if (Vector2Length(GetMouseDelta()) > 0.5f || IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        j->miraSetas = false;
    }

    if (!j->miraSetas) {
        Vector2 d = Vector2Subtract(GetMousePosition(), j->pos);
        if (Vector2Length(d) > 4.0f)
            j->mira = Vector2Normalize(d);
    }

    j->direcao = DirecaoDoVetor(j->mira);

    // Tempos
    if (j->cronoTiro > 0.0f)      j->cronoTiro -= dt;
    if (j->recargaEsquiva > 0.0f) j->recargaEsquiva -= dt;
    if (j->recargaPulso > 0.0f)   j->recargaPulso -= dt;
    if (j->invulneravel > 0.0f)   j->invulneravel -= dt;
    if (j->efeitoPulso > 0.0f)    j->efeitoPulso -= dt;

    j->ep += ROGUE_EP_POR_SEGUNDO * dt;
    if (j->ep > RogueEpMax(b)) j->ep = RogueEpMax(b);

    // Esquiva (para onde está andando; parado, para onde está mirando)
    if (IsKeyPressed(KEY_SPACE) && j->recargaEsquiva <= 0.0f && j->tempoEsquiva <= 0.0f) {
        j->dirEsquiva = andando ? mov : j->mira;
        j->tempoEsquiva = ESQUIVA_DURACAO;
        j->recargaEsquiva = ESQUIVA_RECARGA;
        if (j->invulneravel < ESQUIVA_INVULNERAVEL) j->invulneravel = ESQUIVA_INVULNERAVEL;
        TocarSomTom(sfxSelecao, 0.6f);
    }

    if (j->tempoEsquiva > 0.0f) {
        j->tempoEsquiva -= dt;
        j->pos = Vector2Add(j->pos, Vector2Scale(j->dirEsquiva, ESQUIVA_DISTANCIA / ESQUIVA_DURACAO * dt));
        RogueParticulas(b, j->pos, Fade(SKYBLUE, 0.7f), 1, 30);
    } else {
        j->pos = Vector2Add(j->pos, Vector2Scale(mov, ROGUE_VELOCIDADE * dt));
    }

    RogueEmpurrarParaFora(&j->pos, RAIO_CORPO_JOGADOR, b->obst, b->numObst);
    AtualizarAnimacao(&j->anim, andando || j->tempoEsquiva > 0.0f, dt);

    // Disparador de Dados (no átrio não se atira)
    bool atirando = usandoSetas || IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    if (atirando && j->cronoTiro <= 0.0f && j->tempoEsquiva <= 0.0f && b->tipoSala != SALA_ATRIO) {

        float espalhar = (float)GetRandomValue(-20, 20) / 10.0f * DEG2RAD;    // ±2 graus
        Vector2 dir = Vector2Rotate(j->mira, espalhar);
        Vector2 cano = Vector2Add(j->pos, Vector2Add((Vector2){ 0, 4 }, Vector2Scale(j->mira, 20.0f)));

        RogueDisparar(b, cano, dir, TIRO_VELOCIDADE, RogueDanoTiro(b), TIRO_RAIO, true);
        RogueParticulas(b, cano, SKYBLUE, 2, 60);
        TocarSom(sfxTiro);
        j->cronoTiro = TIRO_INTERVALO;
    }

    // Pulso de Integridade
    if (b->tipoSala != SALA_ATRIO && (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || IsKeyPressed(KEY_Q)))
        RoguePulso(b, prog);

    // Itens de usar: 1 = Antivírus, 2 = Pacote de Dados
    if (IsKeyPressed(KEY_ONE) && j->itens[ITEM_ANTIVIRUS] > 0 && j->hp < RogueHpMax(b)) {
        j->itens[ITEM_ANTIVIRUS]--;
        j->hp = fminf(RogueHpMax(b), j->hp + CURA_ANTIVIRUS);
        RogueParticulas(b, j->pos, LIME, 14, 120);
        TocarSomTom(sfxPegar, 0.9f);
    }

    if (IsKeyPressed(KEY_TWO) && j->itens[ITEM_PACOTE_DADOS] > 0 && j->ep < RogueEpMax(b)) {
        j->itens[ITEM_PACOTE_DADOS]--;
        j->ep = fminf(RogueEpMax(b), j->ep + RECARGA_PACOTE);
        RogueParticulas(b, j->pos, SKYBLUE, 14, 120);
        TocarSomTom(sfxPegar, 0.9f);
    }
}

static void RogueAtualizarInimigos(Biblioteca *b, float dt)
{
    JogadorRogue *j = &b->j;

    for (int i = 0; i < MAX_INIMIGOS; i++) {

        Inimigo *e = &b->ini[i];

        if (!e->ativo)
            continue;

        e->anim += dt;
        if (e->flash > 0.0f) e->flash -= dt;

        if (e->surgindo > 0.0f) {
            e->surgindo -= dt;
            continue;
        }

        Vector2 paraJogador = Vector2Subtract(j->pos, e->pos);
        float dist = Vector2Length(paraJogador);
        Vector2 dir = (dist > 0.01f) ? Vector2Scale(paraJogador, 1.0f / dist) : (Vector2){ 0, 1 };

        if (e->recarga > 0.0f) e->recarga -= dt;

        if (e->atordoado > 0.0f) {
            e->atordoado -= dt;
            e->vel = (Vector2){0};
        }
        else switch (e->tipo) {

            case INIMIGO_FRAGMENTO:

                if (e->estado == 0) {
                    // Persegue com um zigue-zague de dado corrompido
                    Vector2 lado = { -dir.y, dir.x };
                    e->vel = Vector2Add(Vector2Scale(dir, FRAGMENTO_VELOCIDADE),
                                        Vector2Scale(lado, sinf(e->anim * 5.0f) * 40.0f));

                    if (dist < FRAGMENTO_ALCANCE_BOTE && e->recarga <= 0.0f) {
                        e->estado = 1;
                        e->timer = FRAGMENTO_PREPARO;
                        e->alvo = dir;
                        e->vel = (Vector2){0};
                    }
                }
                else if (e->estado == 1) {
                    e->vel = (Vector2){0};
                    e->timer -= dt;
                    if (e->timer <= 0.0f) {
                        e->estado = 2;
                        e->timer = FRAGMENTO_BOTE_DURACAO;
                        e->vel = Vector2Scale(e->alvo, FRAGMENTO_BOTE_VEL);
                    }
                }
                else {
                    e->timer -= dt;
                    if (e->timer <= 0.0f) {
                        e->estado = 0;
                        e->recarga = FRAGMENTO_RECARGA;
                    }
                }
                break;

            case INIMIGO_GUARDIAO:

                if (e->estado == 0) {
                    // Mantém distância: foge de perto, chega mais se longe, senão anda de lado
                    if (dist < GUARDIAO_DIST_MIN)      e->vel = Vector2Scale(dir, -GUARDIAO_VELOCIDADE);
                    else if (dist > GUARDIAO_DIST_MAX) e->vel = Vector2Scale(dir, GUARDIAO_VELOCIDADE);
                    else                               e->vel = Vector2Scale((Vector2){ -dir.y * e->lado, dir.x * e->lado }, GUARDIAO_VELOCIDADE * 0.6f);

                    if (fmodf(e->anim, 2.5f) < dt) e->lado = -e->lado;

                    if (e->recarga <= 0.0f) {
                        e->estado = 1;
                        e->timer = GUARDIAO_PREPARO;
                        e->vel = (Vector2){0};
                    }
                }
                else {
                    e->vel = (Vector2){0};
                    e->timer -= dt;

                    if (e->timer <= 0.0f) {
                        // Leque mirado no jogador (padrão previsível)
                        for (int k = 0; k < GUARDIAO_LEQUE; k++) {
                            float ang = ((float)k - (GUARDIAO_LEQUE - 1) / 2.0f) * GUARDIAO_ABERTURA * DEG2RAD;
                            RogueDisparar(b, e->pos, Vector2Rotate(dir, ang), GUARDIAO_TIRO_VEL, GUARDIAO_TIRO_DANO, 6.0f, false);
                        }
                        e->estado = 0;
                        e->recarga = GUARDIAO_INTERVALO;
                    }
                }
                break;

            case INIMIGO_ECO:

                // Eco falso: se ninguém checar (atirar) a tempo, explode e espalha balas
                e->vel = (Vector2){0};
                e->timer -= dt;

                if (e->timer <= 0.0f) {
                    for (int k = 0; k < ECO_BALAS; k++) {
                        float ang = (2.0f * PI / ECO_BALAS) * k + e->anim;
                        RogueDisparar(b, e->pos, (Vector2){ cosf(ang), sinf(ang) }, 170.0f, 8.0f, 5.0f, false);
                    }
                    RogueParticulas(b, e->pos, MAGENTA, 14, 200);
                    TocarSomTom(sfxErro, 1.4f);
                    e->ativo = false;
                    continue;
                }
                break;
        }

        // Movimento + empurrão dos golpes
        e->pos = Vector2Add(e->pos, Vector2Scale(Vector2Add(e->vel, e->empurrao), dt));
        e->empurrao = Vector2Scale(e->empurrao, fmaxf(0.0f, 1.0f - 8.0f * dt));
        RogueEmpurrarParaFora(&e->pos, e->raio, b->obst, b->numObst);

        // Encostou no jogador
        if (e->tipo == INIMIGO_FRAGMENTO && dist < e->raio + RAIO_CORPO_JOGADOR)
            RogueFerirJogador(b, FRAGMENTO_DANO);
    }

    // Um inimigo não entra no outro
    for (int a = 0; a < MAX_INIMIGOS; a++) {
        for (int c = a + 1; c < MAX_INIMIGOS; c++) {

            Inimigo *e1 = &b->ini[a], *e2 = &b->ini[c];

            if (!e1->ativo || !e2->ativo)
                continue;

            Vector2 d = Vector2Subtract(e2->pos, e1->pos);
            float dist = Vector2Length(d), min = e1->raio + e2->raio;

            if (dist < min && dist > 0.01f) {
                Vector2 empurra = Vector2Scale(d, (min - dist) / dist * 0.5f);
                e1->pos = Vector2Subtract(e1->pos, empurra);
                e2->pos = Vector2Add(e2->pos, empurra);
            }
        }
    }
}

static void RogueAtualizarProjeteis(Biblioteca *b, ProgressoCorredor *prog, float dt)
{
    for (int i = 0; i < MAX_PROJETEIS; i++) {

        Projetil *p = &b->proj[i];

        if (!p->ativo)
            continue;

        p->pos = Vector2Add(p->pos, Vector2Scale(p->vel, dt));

        // Saiu da arena ou bateu numa estante
        bool bateu = !CheckCollisionPointRec(p->pos, ARENA_ROGUE);
        for (int k = 0; k < b->numObst && !bateu; k++)
            bateu = CheckCollisionPointRec(p->pos, b->obst[k]);

        if (bateu) {
            p->ativo = false;
            RogueParticulas(b, p->pos, p->doJogador ? SKYBLUE : ORANGE, 3, 80);
            continue;
        }

        if (p->doJogador) {

            for (int k = 0; k < MAX_INIMIGOS; k++) {

                Inimigo *e = &b->ini[k];

                if (!e->ativo || e->surgindo > 0.0f || Vector2Distance(p->pos, e->pos) > e->raio + p->raio)
                    continue;

                p->ativo = false;
                RogueFerirInimigo(b, prog, e, p->dano, Vector2Normalize(p->vel));
                break;
            }

        } else if (Vector2Distance(p->pos, b->j.pos) < RAIO_CORPO_JOGADOR + p->raio &&
                   b->j.invulneravel <= 0.0f && b->j.tempoEsquiva <= 0.0f) {

            p->ativo = false;
            RogueFerirJogador(b, p->dano);
        }
    }
}

// Fragmentos no chão voam até o jogador; itens caídos são pegos ao encostar
static void RogueAtualizarColetaveis(Biblioteca *b, float dt)
{
    for (int i = 0; i < MAX_COLETAVEIS; i++) {

        Coletavel *c = &b->col[i];

        if (!c->ativo)
            continue;

        c->anim += dt;
        c->pos = Vector2Add(c->pos, Vector2Scale(c->vel, dt));
        c->vel = Vector2Scale(c->vel, fmaxf(0.0f, 1.0f - 5.0f * dt));
        RogueEmpurrarParaFora(&c->pos, 6.0f, b->obst, b->numObst);

        float dist = Vector2Distance(c->pos, b->j.pos);

        if (c->anim > 0.4f && dist < RAIO_IMA)
            c->pos = Vector2Add(c->pos, Vector2Scale(Vector2Normalize(Vector2Subtract(b->j.pos, c->pos)), 320.0f * dt));

        if (c->anim > 0.4f && dist < RAIO_CORPO_JOGADOR + 8.0f) {

            c->ativo = false;

            if (c->fragmento) {
                b->perm->fragmentos++;
                RogueParticulas(b, c->pos, (Color){ 0, 228, 255, 255 }, 6, 100);
                TocarSomTom(sfxPegar, 1.5f);
            } else {
                b->j.itens[c->item]++;
                RogueAviso(b, TextFormat("+1 %s", ITENS[c->item].nome));
                TocarSomTom(sfxAcerto, 1.2f);
            }
        }
    }
}

// O que está ao alcance do E agora (-1 = nada)
typedef enum { ALVO_NADA, ALVO_BYTE, ALVO_BYTECO, ALVO_TERMINAL, ALVO_PEDESTAL, ALVO_PORTAL, ALVO_ENCICLOPEDIA } AlvoRogue;

static AlvoRogue RogueAlvoProximo(const Biblioteca *b, int *pedestal)
{
    Vector2 p = b->j.pos;

    if (b->tipoSala == SALA_ATRIO) {
        if (Vector2Distance(p, POS_BYTE) < 70)                                  return ALVO_BYTE;
        if (Vector2Distance(p, (Vector2){ POS_BYTECO.x, POS_BYTECO.y + 50 }) < 60) return ALVO_BYTECO;
        if (Vector2Distance(p, POS_ENCICLOPEDIA) < 60)                          return ALVO_ENCICLOPEDIA;
    }
    else if (b->tipoSala == SALA_EDUCATIVA && !b->terminalLido) {
        if (Vector2Distance(p, POS_TERMINAL) < 70) return ALVO_TERMINAL;
    }
    else if (b->tipoSala == SALA_RECOMPENSA && !b->recompensaPega) {
        for (int i = 0; i < NUM_PEDESTAIS; i++)
            if (Vector2Distance(p, POS_PEDESTAIS[i]) < 42) { *pedestal = i; return ALVO_PEDESTAL; }
    }
    else if (b->tipoSala == SALA_CHEFE) {
        if (Vector2Distance(p, POS_PORTAL) < 80) return ALVO_PORTAL;
    }

    return ALVO_NADA;
}

// Compra um item do Byteco. Devolve true se comprou.
static bool RogueComprar(Biblioteca *b, TipoItem item)
{
    const DefItem *it = &ITENS[item];
    int *tenho = &b->j.itens[item];

    if (it->passivo && *tenho >= MAX_PILHA_PASSIVO) {
        RogueAviso(b, "Você já tem o máximo desse item.");
        TocarSom(sfxErro);
        return false;
    }

    if (b->perm->fragmentos < it->preco) {
        RogueAviso(b, "Fragmentos de Conhecimento insuficientes.");
        TocarSom(sfxErro);
        return false;
    }

    b->perm->fragmentos -= it->preco;
    (*tenho)++;
    RogueAviso(b, TextFormat("Comprou: %s", it->nome));
    TocarSomTom(sfxAcerto, 1.2f);
    Telemetria("rogue_loja", it->nome, (float)it->preco, b->perm->fragmentos);
    return true;
}

// Melhoria permanente: fica para sempre (vai para o save)
static bool RogueComprarMelhoria(Biblioteca *b, int m)
{
    int nivel = b->perm->melhorias[m];

    if (nivel >= NIVEL_MAX_MELHORIA) {
        RogueAviso(b, "Essa melhoria já está no nível máximo.");
        TocarSom(sfxErro);
        return false;
    }

    if (b->perm->fragmentos < PRECO_MELHORIA[nivel]) {
        RogueAviso(b, "Fragmentos de Conhecimento insuficientes.");
        TocarSom(sfxErro);
        return false;
    }

    b->perm->fragmentos -= PRECO_MELHORIA[nivel];
    b->perm->melhorias[m]++;

    // A vida e a energia novas já valem agora
    if (m == MELHORIA_VIDA)    b->j.hp += MELHORIA_HP;
    if (m == MELHORIA_ENERGIA) b->j.ep += MELHORIA_EP;

    RogueAviso(b, TextFormat("%s: nível %d!", NOMES_MELHORIAS[m], b->perm->melhorias[m]));
    TocarSomTom(sfxAcerto, 1.3f);
    Telemetria("rogue_melhoria", NOMES_MELHORIAS[m], (float)PRECO_MELHORIA[nivel], b->perm->melhorias[m]);
    return true;
}

// Enciclopédia do átrio: W/S escolhe, E lê o texto completo, BACKSPACE/ESC sai
static void RogueAtualizarEnciclopedia(Biblioteca *b, SessaoDialogo *dlg)
{
    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))   { b->selecaoEnciclopedia = (b->selecaoEnciclopedia + NUM_CONCEITOS - 1) % NUM_CONCEITOS; TocarSom(sfxSelecao); }
    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) { b->selecaoEnciclopedia = (b->selecaoEnciclopedia + 1) % NUM_CONCEITOS;                  TocarSom(sfxSelecao); }

    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_ESCAPE)) {
        b->enciclopediaAberta = false;
        return;
    }

    bool confirmar = IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
    int c = b->selecaoEnciclopedia;

    if (!confirmar)
        return;

    if (b->perm->conceitos & (1u << c)) {
        b->enciclopediaAberta = false;      // volta quando a leitura terminar
        DialogoIniciar(dlg, TEXTOS_CONCEITOS[c], OBJ_ENCICLOPEDIA);
    } else {
        RogueAviso(b, "Conceito ainda não descoberto.");
        TocarSom(sfxErro);
    }
}

// Loja do Byteco: A/D troca a aba, W/S escolhe, E (ou ENTER) compra, BACKSPACE/ESC sai
static void RogueAtualizarLoja(Biblioteca *b)
{
    if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) {
        b->abaLoja = 1 - b->abaLoja;
        b->selecaoLoja = 0;
        TocarSom(sfxSelecao);
    }

    int linhas = (b->abaLoja == 0) ? NUM_ITENS : NUM_MELHORIAS;
    int total = linhas + 1;                         // a última linha é SAIR

    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))   { b->selecaoLoja = (b->selecaoLoja + total - 1) % total; TocarSom(sfxSelecao); }
    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) { b->selecaoLoja = (b->selecaoLoja + 1) % total;         TocarSom(sfxSelecao); }

    bool sair = IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_ESCAPE);
    bool confirmar = IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);

    if (sair || (confirmar && b->selecaoLoja == linhas)) {
        b->lojaAberta = false;
        return;
    }

    if (confirmar && b->abaLoja == 0) RogueComprar(b, (TipoItem)b->selecaoLoja);
    else if (confirmar)               RogueComprarMelhoria(b, b->selecaoLoja);
}

// E perto de alguém ou de algo: fala, lê ou pega
static void RogueInteragir(Biblioteca *b, SessaoDialogo *dlg, AlvoRogue alvo, int pedestal)
{
    switch (alvo) {

        case ALVO_BYTE:
            DialogoIniciar(dlg, b->perm->byteVisto ? &dialogoByteDepois : &dialogoByte, OBJ_BYTE);
            break;

        case ALVO_BYTECO:
            if (b->perm->bytecoVisto) { b->lojaAberta = true; b->selecaoLoja = 0; }
            else                      DialogoIniciar(dlg, &dialogoByteco, OBJ_BYTECO);
            break;

        case ALVO_TERMINAL:
            DialogoIniciar(dlg, (b->educativas == 0) ? &dialogoTerminalAlucinacao : &dialogoTerminalPraticas, OBJ_TERMINAL);
            b->educativas++;
            break;

        case ALVO_PEDESTAL: {
            TipoItem it = b->pedestal[pedestal];
            b->j.itens[it]++;
            b->recompensaPega = true;
            RogueAviso(b, TextFormat("Pegou: %s", ITENS[it].nome));
            RogueParticulas(b, POS_PEDESTAIS[pedestal], GOLD, 20, 180);
            TocarSomTom(sfxAcerto, 1.2f);
            Telemetria("rogue_recompensa", ITENS[it].nome, 0.0f, 0);
        } break;

        case ALVO_PORTAL:
            DialogoIniciar(dlg, &dialogoPortalChefe, OBJ_PORTAL_CHEFE);
            break;

        case ALVO_ENCICLOPEDIA:
            b->enciclopediaAberta = true;
            b->selecaoEnciclopedia = 0;
            break;

        default:
            break;
    }
}

// Terminou uma conversa da Biblioteca: o que ela muda
static void RogueFimConversa(Biblioteca *b, ProgressoCorredor *prog, int obj)
{
    if (obj == OBJ_BYTE) {
        b->perm->byteVisto = true;
        if (!(b->perm->conceitos & (1u << CONCEITO_FUNDAMENTOS)))
            RogueAviso(b, "NOVO CONCEITO NA ENCICLOPÉDIA!");
        b->perm->conceitos |= 1u << CONCEITO_FUNDAMENTOS;
    }
    else if (obj == OBJ_ENCICLOPEDIA) {
        b->enciclopediaAberta = true;
    }
    else if (obj == OBJ_BYTECO) {
        b->perm->bytecoVisto = true;
        b->lojaAberta = true;
        b->selecaoLoja = 0;
    }
    else if (obj == OBJ_TERMINAL) {
        b->terminalLido = true;
        b->limpa = true;
        b->perm->conceitos |= 1u << ((b->educativas <= 1) ? CONCEITO_ALUCINACOES : CONCEITO_PRATICAS);
        b->perm->fragmentos += FC_POR_TERMINAL;
        AlterarPontos(prog, PONTOS_TERMINAL);
        RogueAviso(b, TextFormat("+%d FRAGMENTOS DE CONHECIMENTO", FC_POR_TERMINAL));
        RogueParticulas(b, POS_TERMINAL, (Color){ 0, 228, 255, 255 }, 24, 200);
        TocarSom(sfxAcerto);
        MostrarEmocaoLuci(EMOCAO_ALEGRE, 2.0f);
        Telemetria("rogue_terminal", TextFormat("conceito %d", b->educativas), 0.0f, b->perm->fragmentos);
    }
    else if (obj == OBJ_PORTAL_CHEFE) {
        Telemetria("rogue_missao", "fim (sem chefe ainda)", (float)(GetTime() - b->inicio), b->abatidos);
        memset(b->j.itens, 0, sizeof(b->j.itens));      // a mochila é desta tentativa
        RogueEntrarAtrio(b, false);
    }
}

typedef enum { ROGUE_NADA, ROGUE_SAIR } EventoRogue;

static EventoRogue RogueAtualizar(Biblioteca *b, ProgressoCorredor *prog, SessaoDialogo *dlg, float dt)
{
    for (int i = 0; i < MAX_PARTICULAS_ROGUE; i++) {
        ParticulaRogue *p = &b->part[i];
        if (p->vida <= 0.0f) continue;
        p->vida -= dt;
        p->pos = Vector2Add(p->pos, Vector2Scale(p->vel, dt));
        p->vel = Vector2Scale(p->vel, 1.0f - 3.0f * dt);
    }

    if (b->tremor > 0.0f)      b->tremor -= dt;
    if (b->fadeEntrada > 0.0f) b->fadeEntrada -= dt;
    if (b->tempoAviso > 0.0f)  b->tempoAviso -= dt;

    // Conversa aberta (Byte, Byteco, terminal, portal): o resto espera
    if (dlg->estado != DLG_NENHUM) {

        int obj = dlg->objeto;

        if (DialogoAtualizar(dlg) == RES_FIM)
            RogueFimConversa(b, prog, obj);

        return ROGUE_NADA;
    }

    if (b->lojaAberta) {
        RogueAtualizarLoja(b);
        return ROGUE_NADA;
    }

    if (b->enciclopediaAberta) {
        RogueAtualizarEnciclopedia(b, dlg);
        return ROGUE_NADA;
    }

    // Desafio educativo aberto: o combate espera
    if (b->desafioAberto) {
        RogueAtualizarDesafio(b, prog, dt);
        return ROGUE_NADA;
    }

    // Fala da Luci guardada para quando a sala terminar de aparecer
    if (b->falaPendente != NULL && b->fadeEntrada <= 0.0f) {
        DialogoFalaAvulsa(dlg, b->falaPendente);
        b->falaPendente = NULL;
        return ROGUE_NADA;
    }

    if (b->pausaImpacto > 0.0f) {
        b->pausaImpacto -= dt;
        return ROGUE_NADA;
    }

    // Derrota: volta para o átrio sem os itens da tentativa
    if (b->morreu) {
        b->tempoMorte += dt;
        bool confirmar = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE);
        if (b->tempoMorte > 1.0f && (confirmar || b->tempoMorte > 5.0f)) {
            b->morreu = false;
            memset(b->j.itens, 0, sizeof(b->j.itens));
            RogueEntrarAtrio(b, false);
        }
        return ROGUE_NADA;
    }

    RogueAtualizarJogador(b, prog, dt);
    RogueAtualizarInimigos(b, dt);
    RogueAtualizarProjeteis(b, prog, dt);
    RogueAtualizarColetaveis(b, dt);

    // Chegou perto do Guardião selado: o desafio abre
    for (int i = 0; i < MAX_INIMIGOS && !b->desafioAberto; i++) {
        Inimigo *e = &b->ini[i];
        if (e->ativo && e->selado && e->surgindo <= 0.0f && Vector2Distance(e->pos, b->j.pos) < RAIO_DESAFIO)
            RogueAbrirDesafio(b, i);
    }

    if (b->j.hp <= 0.0f) {
        b->j.hp = 0.0f;
        b->morreu = true;
        b->tempoMorte = 0.0f;
        AlterarPontos(prog, -PENALIDADE_HP_ZERO);
        RogueParticulas(b, b->j.pos, SKYBLUE, 30, 260);
        TocarSom(sfxErro);
        Telemetria("rogue_morte", TextFormat("sala %d", b->indiceRota + 1), (float)(GetTime() - b->inicio), b->abatidos);
        return ROGUE_NADA;
    }

    if (b->tipoSala == SALA_COMBATE && !b->limpa && RogueInimigosVivos(b) == 0) {
        b->limpa = true;
        AlterarPontos(prog, PONTOS_SALA_ROGUE);
        TocarSom(sfxAcerto);
        MostrarEmocaoLuci(EMOCAO_ALEGRE, 2.0f);
        Telemetria("rogue_sala_limpa", TextFormat("sala %d", b->indiceRota + 1), (float)(GetTime() - b->inicio), b->abatidos);
    }

    // E: falar, ler, pegar
    int pedestal = -1;
    AlvoRogue alvo = RogueAlvoProximo(b, &pedestal);

    if (IsKeyPressed(KEY_E) && alvo != ALVO_NADA) {
        RogueInteragir(b, dlg, alvo, pedestal);
        return ROGUE_NADA;
    }

    // Portas: é só andar até elas
    bool naPortaDeCima  = CheckCollisionCircleRec(b->j.pos, RAIO_CORPO_JOGADOR + 2.0f, PORTA_PROXIMA_ROGUE);
    bool naPortaDaSaida = CheckCollisionCircleRec(b->j.pos, RAIO_CORPO_JOGADOR + 2.0f, PORTA_SAIDA_ROGUE);

    if (b->tipoSala == SALA_ATRIO) {
        if (naPortaDaSaida)
            return ROGUE_SAIR;
        if (naPortaDeCima) {
            if (b->perm->byteVisto) RogueComecarMissao(b);
            else {
                b->j.pos.y += 24.0f;
                DialogoFalaAvulsa(dlg, "Luci: Antes de entrar, vamos falar com o Professor Byte. Ele está ali, perto da porta!");
            }
        }
    }
    else if (b->tipoSala != SALA_CHEFE && b->limpa && naPortaDeCima) {
        RogueAvancar(b);
    }

    return ROGUE_NADA;
}

// ---------- Desenho ----------

static void DesenharEstante(Rectangle r, float tempo)
{
    static const Color lombadas[5] = { { 120, 60, 200, 255 }, { 60, 140, 220, 255 }, { 200, 80, 160, 255 },
                                       { 80, 200, 220, 255 }, { 230, 190, 80, 255 } };

    DrawRectangleRec(r, (Color){ 40, 28, 64, 255 });

    // Lombadas dos livros (o comprimento da estante decide a direção)
    bool deitada = r.width >= r.height;
    int passo = 7;
    int n = (int)((deitada ? r.width : r.height) - 6) / passo;

    for (int i = 0; i < n; i++) {
        Color c = lombadas[Ruido((unsigned int)(r.x * 7 + r.y * 13 + i)) % 5];
        if (deitada) DrawRectangle((int)r.x + 3 + i * passo, (int)r.y + 4, passo - 2, (int)r.height - 8, Fade(c, 0.8f));
        else         DrawRectangle((int)r.x + 4, (int)r.y + 3 + i * passo, (int)r.width - 8, passo - 2, Fade(c, 0.8f));
    }

    DrawRectangleLinesEx(r, 2, Fade(SKYBLUE, 0.35f + 0.1f * sinf(tempo * 2.0f + r.x)));
}

static void DesenharPortaRogue(Rectangle r, bool aberta, const char *nome, float tempo)
{
    float pulso = 0.5f + 0.5f * sinf(tempo * 4.0f);
    Color cor = aberta ? Fade(GREEN, 0.55f + 0.45f * pulso) : MAROON;

    DrawRectangleRec(r, aberta ? Fade(GREEN, 0.25f) : (Color){ 30, 10, 20, 255 });
    DrawRectangleLinesEx(r, 2, cor);

    const char *rotulo = aberta ? nome : "TRANCADA";
    int w = MeasureText(rotulo, 10);
    int x = (r.width > r.height) ? (int)(r.x + r.width / 2) - w / 2 : (int)(r.x + r.width) + 6;
    int y = (r.width > r.height) ? (int)(r.y + r.height) + 4 : (int)(r.y + r.height / 2) - 5;

    DrawText(rotulo, x, y, 10, aberta ? GREEN : RED);
}

// Ícone de cada item (loja, pedestais, chão e HUD)
static void DesenharIconeItem(TipoItem item, Vector2 c, float e, float tempo)
{
    switch (item) {

        case ITEM_ANTIVIRUS:        // escudo verde com uma cruz
            DesenharTrianguloB2((Vector2){ c.x - 9 * e, c.y - 8 * e }, (Vector2){ c.x + 9 * e, c.y - 8 * e }, (Vector2){ c.x, c.y + 11 * e }, LIME);
            DrawRectangle((int)(c.x - 9 * e), (int)(c.y - 11 * e), (int)(18 * e), (int)(5 * e), LIME);
            DrawRectangle((int)(c.x - 1.5f * e), (int)(c.y - 8 * e), (int)(3 * e), (int)(10 * e), WHITE);
            DrawRectangle((int)(c.x - 5 * e), (int)(c.y - 4.5f * e), (int)(10 * e), (int)(3 * e), WHITE);
            break;

        case ITEM_PACOTE_DADOS:     // pacote azul com bits
            DrawRectangle((int)(c.x - 9 * e), (int)(c.y - 9 * e), (int)(18 * e), (int)(18 * e), (Color){ 30, 90, 180, 255 });
            DrawRectangleLinesEx((Rectangle){ c.x - 9 * e, c.y - 9 * e, 18 * e, 18 * e }, 2, SKYBLUE);
            DrawText("01", (int)(c.x - 6 * e), (int)(c.y - 5 * e), (int)(10 * e), WHITE);
            break;

        case ITEM_LENTE:
            DesenharLupaVerificacao(c, 0.6f * e, tempo);
            break;

        case ITEM_FIREWALL:         // muro de tijolos em chamas
            for (int l = 0; l < 3; l++)
                for (int k = 0; k < 3; k++)
                    DrawRectangle((int)(c.x - 10 * e + k * 7 * e + (l % 2) * 3 * e), (int)(c.y - 4 * e + l * 5 * e),
                                  (int)(6 * e), (int)(4 * e), ORANGE);
            DesenharTrianguloB2((Vector2){ c.x - 6 * e, c.y - 5 * e }, (Vector2){ c.x, c.y - 13 * e - sinf(tempo * 8) * 2 * e },
                                (Vector2){ c.x + 6 * e, c.y - 5 * e }, RED);
            break;

        case ITEM_PROCESSADOR:      // chip roxo com pernas
            for (int k = 0; k < 3; k++) {
                DrawRectangle((int)(c.x - 6 * e + k * 5 * e), (int)(c.y - 12 * e), (int)(2 * e), (int)(24 * e), LIGHTGRAY);
                DrawRectangle((int)(c.x - 12 * e), (int)(c.y - 6 * e + k * 5 * e), (int)(24 * e), (int)(2 * e), LIGHTGRAY);
            }
            DrawRectangle((int)(c.x - 8 * e), (int)(c.y - 8 * e), (int)(16 * e), (int)(16 * e), PURPLE);
            DrawRectangleLinesEx((Rectangle){ c.x - 8 * e, c.y - 8 * e, 16 * e, 16 * e }, 1, MAGENTA);
            break;

        case ITEM_MANUAL:           // livro dourado
        default:
            DrawRectangle((int)(c.x - 9 * e), (int)(c.y - 11 * e), (int)(18 * e), (int)(22 * e), GOLD);
            DrawRectangle((int)(c.x - 9 * e), (int)(c.y - 11 * e), (int)(4 * e), (int)(22 * e), ORANGE);
            DrawRectangle((int)(c.x - 2 * e), (int)(c.y - 5 * e), (int)(8 * e), (int)(2 * e), (Color){ 80, 50, 20, 255 });
            DrawRectangle((int)(c.x - 2 * e), (int)(c.y - 1 * e), (int)(8 * e), (int)(2 * e), (Color){ 80, 50, 20, 255 });
            break;
    }
}

// Fragmento de Conhecimento: uma página luminosa
static void DesenharFragmentoConhecimento(Vector2 c, float t)
{
    Vector2 p = { c.x, c.y + sinf(t * 5.0f) * 2.0f };
    DrawCircleV(p, 9, Fade((Color){ 0, 228, 255, 255 }, 0.25f));
    DrawRectangle((int)p.x - 4, (int)p.y - 6, 8, 11, (Color){ 200, 250, 255, 255 });
    DrawRectangle((int)p.x - 2, (int)p.y - 3, 5, 1, (Color){ 0, 120, 160, 255 });
    DrawRectangle((int)p.x - 2, (int)p.y, 5, 1, (Color){ 0, 120, 160, 255 });
}

// Professor Byte: um holograma de professor (cabeça de monitor, óculos e capelo)
static void DesenharProfessorByte(Vector2 c, float t)
{
    Vector2 p = { c.x, c.y + sinf(t * 2.0f) * 3.0f };
    Color holo = (Color){ 0, 228, 255, 255 };

    DrawEllipse((int)c.x, (int)c.y + 26, 20, 6, Fade(holo, 0.25f));
    DrawRectangle((int)p.x - 12, (int)p.y + 4, 24, 20, Fade(holo, 0.35f));                     // corpo
    DrawRectangle((int)p.x - 16, (int)p.y - 20, 32, 24, (Color){ 20, 50, 80, 255 });           // cabeça (monitor)
    DrawRectangleLinesEx((Rectangle){ p.x - 16, p.y - 20, 32, 24 }, 2, holo);
    DrawCircleLines((int)p.x - 7, (int)p.y - 8, 5, WHITE);                                     // óculos
    DrawCircleLines((int)p.x + 7, (int)p.y - 8, 5, WHITE);
    DrawLine((int)p.x - 2, (int)p.y - 8, (int)p.x + 2, (int)p.y - 8, WHITE);
    DrawCircle((int)p.x - 7, (int)p.y - 8, 2, holo);
    DrawCircle((int)p.x + 7, (int)p.y - 8, 2, holo);
    DrawRectangle((int)p.x - 18, (int)p.y - 26, 36, 6, (Color){ 30, 20, 50, 255 });             // capelo
    DrawRectangle((int)p.x - 8, (int)p.y - 30, 16, 5, (Color){ 30, 20, 50, 255 });
    DrawLine((int)p.x + 14, (int)p.y - 24, (int)p.x + 16, (int)p.y - 14, GOLD);                // pendão

    const char *nome = "PROF. BYTE";
    DrawText(nome, (int)c.x - MeasureText(nome, 10) / 2, (int)c.y + 34, 10, holo);
}

// Byteco: um robozinho comerciante atrás do balcão
static void DesenharByteco(Vector2 c, float t)
{
    Vector2 p = { c.x, c.y + sinf(t * 3.0f) * 2.0f };

    DrawLine((int)p.x, (int)p.y - 22, (int)p.x, (int)p.y - 14, LIGHTGRAY);                     // antena
    DrawCircle((int)p.x, (int)p.y - 24, 3, (((int)(t * 2)) % 2) ? GOLD : ORANGE);
    DrawCircle((int)p.x, (int)p.y, 16, (Color){ 230, 160, 60, 255 });                          // corpo redondo
    DrawCircle((int)p.x, (int)p.y, 11, (Color){ 40, 30, 60, 255 });
    DrawCircle((int)p.x - 4, (int)p.y - 2, 3, (Color){ 0, 228, 255, 255 });
    DrawCircle((int)p.x + 4, (int)p.y - 2, 3, (Color){ 0, 228, 255, 255 });
    DrawLine((int)p.x - 4, (int)p.y + 5, (int)p.x + 4, (int)p.y + 5, (Color){ 0, 228, 255, 255 });

    // Balcão com os itens à mostra
    DrawRectangle(555, 318, 110, 24, (Color){ 70, 45, 30, 255 });
    DrawRectangleLinesEx((Rectangle){ 555, 318, 110, 24 }, 2, GOLD);
    for (int i = 0; i < 4; i++)
        DesenharIconeItem((TipoItem)i, (Vector2){ 572.0f + i * 26.0f, 330.0f }, 0.55f, t);

    const char *nome = "BYTECO - LOJA";
    DrawText(nome, 610 - MeasureText(nome, 10) / 2, 346, 10, GOLD);
}

// Terminal de conhecimento das salas educativas
static void DesenharTerminal(Vector2 c, bool lido, float t)
{
    Color cor = lido ? Fade(GRAY, 0.8f) : (Color){ 0, 228, 255, 255 };

    if (!lido)
        DrawCircleV(c, 40 + sinf(t * 3.0f) * 4, Fade(cor, 0.12f));

    DrawRectangle((int)c.x - 20, (int)c.y - 20, 40, 34, (Color){ 20, 30, 50, 255 });
    DrawRectangleLinesEx((Rectangle){ c.x - 20, c.y - 20, 40, 34 }, 2, cor);
    for (int i = 0; i < 3; i++)
        DrawRectangle((int)c.x - 14, (int)c.y - 13 + i * 8, 12 + (int)(Ruido((unsigned int)(t * 4) + i) % 16), 3, Fade(cor, 0.8f));
    DrawRectangle((int)c.x - 6, (int)c.y + 14, 12, 8, (Color){ 40, 40, 60, 255 });
}

// Pedestal com uma recompensa
static void DesenharPedestal(Vector2 c, TipoItem item, bool disponivel, float t)
{
    DrawEllipse((int)c.x, (int)c.y + 16, 22, 8, (Color){ 60, 45, 90, 255 });
    DrawRectangle((int)c.x - 16, (int)c.y + 4, 32, 12, (Color){ 50, 36, 80, 255 });

    if (!disponivel)
        return;

    Vector2 icone = { c.x, c.y - 10 + sinf(t * 3.0f + c.x) * 3.0f };
    DrawCircleV(icone, 18, Fade(GOLD, 0.12f));
    DesenharIconeItem(item, icone, 1.0f, t);
}

// Portal da sala do Alucinador
static void DesenharPortalChefe(Vector2 c, float t)
{
    for (int i = 5; i > 0; i--) {
        float r = 14.0f * i + sinf(t * 3.0f + i) * 3.0f;
        DrawCircleLines((int)c.x, (int)c.y, r, Fade(MAGENTA, 0.15f + 0.12f * i));
    }
    DrawCircleV(c, 22, Fade((Color){ 230, 41, 255, 255 }, 0.5f + 0.2f * sinf(t * 6.0f)));
    DrawText("?", (int)c.x - 6, (int)c.y - 12, 24, WHITE);
}

// Fragmento Alucinado: um cristal de dado corrompido (camadas deslocadas = glitch)
static void DesenharFragmentoAlucinado(const Inimigo *e, float escala)
{
    float s = e->raio * escala;
    Vector2 p = e->pos;

    if (e->estado == 1)           // preparando o bote: treme e fica vermelho
        p.x += (float)GetRandomValue(-2, 2);

    Color cores[3] = { Fade(SKYBLUE, 0.5f), Fade(RED, 0.5f), (e->estado == 1) ? RED : (Color){ 230, 41, 255, 255 } };
    float desloc[3] = { -3.0f, 3.0f, 0.0f };

    for (int k = 0; k < 3; k++) {
        Vector2 c = { p.x + desloc[k], p.y };
        float giro = e->anim * 2.0f;
        Vector2 a = GirarPonto(c, 0, -s * 1.2f, giro * 20.0f);
        Vector2 d = GirarPonto(c, s, 0, giro * 20.0f);
        Vector2 f = GirarPonto(c, 0, s * 1.2f, giro * 20.0f);
        Vector2 g = GirarPonto(c, -s, 0, giro * 20.0f);
        DesenharTrianguloB2(a, d, f, cores[k]);
        DesenharTrianguloB2(a, f, g, cores[k]);
    }

    DrawText(e->estado == 1 ? "!" : "?", (int)p.x - 3, (int)p.y - 7, 14, WHITE);
}

// Guardião de Dados: um tomo flutuante que vigia (brilha antes de atirar)
static void DesenharGuardiao(const Inimigo *e, float escala)
{
    float w = 30 * escala, h = 34 * escala;
    Vector2 p = { e->pos.x, e->pos.y + sinf(e->anim * 2.0f) * 3.0f };

    if (e->estado == 1) {
        float k = 1.0f - e->timer / GUARDIAO_PREPARO;
        DrawCircleV(p, e->raio + 6 + 10 * k, Fade(ORANGE, 0.25f + 0.3f * k));
    }

    DrawCircleV(p, e->raio + 4, Fade(SKYBLUE, 0.12f));
    DrawRectangle((int)(p.x - w / 2), (int)(p.y - h / 2), (int)w, (int)h, (Color){ 30, 60, 120, 255 });
    DrawRectangle((int)(p.x - w / 2), (int)(p.y - h / 2), (int)(6 * escala), (int)h, SKYBLUE);           // lombada
    DrawRectangleLinesEx((Rectangle){ p.x - w / 2, p.y - h / 2, w, h }, 2, SKYBLUE);

    DrawCircle((int)(p.x + 3 * escala), (int)p.y, 7 * escala, WHITE);
    DrawCircle((int)(p.x + 3 * escala), (int)p.y, 3.5f * escala, (e->estado == 1) ? ORANGE : (Color){ 0, 60, 140, 255 });
}

// Eco falso: um fantasma do fragmento, piscando cada vez mais rápido até explodir
static void DesenharEco(const Inimigo *e)
{
    float k = 1.0f - e->timer / ECO_TEMPO;
    float pisca = 0.35f + 0.35f * sinf(e->anim * (8.0f + 20.0f * k));

    DrawCircleV(e->pos, e->raio + 4 * k, Fade(MAGENTA, 0.25f + pisca * 0.4f));
    DrawCircleLines((int)e->pos.x, (int)e->pos.y, e->raio + 10 * (1.0f - k), Fade(MAGENTA, 0.8f));
    DrawText("?", (int)e->pos.x - 3, (int)e->pos.y - 7, 14, Fade(WHITE, 0.8f));
}

// Selo dourado em volta do Guardião com desafio
static void DesenharSelo(const Inimigo *e, float tempo)
{
    float r = e->raio + 14.0f;

    for (int i = 0; i < 6; i++) {
        float a1 = tempo * 1.2f + i * (PI / 3.0f), a2 = a1 + PI / 3.0f;
        DrawLineEx((Vector2){ e->pos.x + cosf(a1) * r, e->pos.y + sinf(a1) * r },
                   (Vector2){ e->pos.x + cosf(a2) * r, e->pos.y + sinf(a2) * r }, 3, Fade(GOLD, 0.8f));
    }

    DrawCircleV(e->pos, r, Fade(GOLD, 0.08f + 0.05f * sinf(tempo * 4.0f)));
    DrawText("?", (int)e->pos.x - 6, (int)(e->pos.y - e->raio - 40 + sinf(tempo * 3.0f) * 3.0f), 24, GOLD);
}

static void DesenharInimigo(const Inimigo *e)
{
    float escala = (e->surgindo > 0.0f) ? 1.0f - e->surgindo / TEMPO_SURGINDO : 1.0f;

    if (e->surgindo > 0.0f)
        DrawCircleLines((int)e->pos.x, (int)e->pos.y, e->raio + 12 * (1.0f - escala), Fade(MAGENTA, 0.7f));

    for (int vez = 0; vez < ((e->flash > 0.0f) ? 3 : 1); vez++) {

        // Levou dano: desenha de novo somando a cor (fica branco)
        if (vez == 1) BeginBlendMode(BLEND_ADDITIVE);

        if (e->tipo == INIMIGO_FRAGMENTO)      DesenharFragmentoAlucinado(e, escala);
        else if (e->tipo == INIMIGO_GUARDIAO)  DesenharGuardiao(e, escala);
        else                                   DesenharEco(e);
    }

    if (e->flash > 0.0f)
        EndBlendMode();

    if (e->selado && e->surgindo <= 0.0f)
        DesenharSelo(e, (float)GetTime());

    // Barra de vida quando já levou dano
    if (e->hp < e->hpMax && e->tipo != INIMIGO_ECO) {
        int w = (int)(e->raio * 2.4f);
        int x = (int)e->pos.x - w / 2, y = (int)(e->pos.y - e->raio - 12);
        DrawRectangle(x, y, w, 4, (Color){ 40, 30, 55, 255 });
        DrawRectangle(x, y, (int)(w * e->hp / e->hpMax), 4, (e->tipo == INIMIGO_GUARDIAO) ? SKYBLUE : MAGENTA);
    }
}

// Ícone redondo com a recarga (Pulso / Esquiva)
static void DesenharRecargaRogue(int x, int y, float falta, float total, const char *tecla, Color cor, bool disponivel)
{
    DrawCircle(x, y, 14, (Color){ 20, 16, 36, 255 });

    if (falta > 0.0f)
        DrawCircleSector((Vector2){ (float)x, (float)y }, 14, -90.0f, -90.0f + 360.0f * (1.0f - falta / total), 24, Fade(cor, 0.5f));
    else
        DrawCircle(x, y, 14, Fade(cor, disponivel ? 0.8f : 0.25f));

    DrawCircleLines(x, y, 14, cor);
    DrawText(tecla, x - MeasureText(tecla, 10) / 2, y - 5, 10, WHITE);
}

// Tecla "E" sobre o que dá para usar
static void DesenharTeclaE(Vector2 c, float tempo)
{
    int y = (int)(c.y + sinf(tempo * 5.0f) * 2.0f);
    DrawRectangle((int)c.x - 9, y - 9, 18, 18, Fade(BLACK, 0.7f));
    DrawRectangleLines((int)c.x - 9, y - 9, 18, 18, GOLD);
    DrawText("E", (int)c.x - 3, y - 6, 12, GOLD);
}

// Púlpito da enciclopédia no átrio
static void DesenharPulpitoEnciclopedia(Vector2 c, float t)
{
    DrawRectangle((int)c.x - 6, (int)c.y - 4, 12, 18, (Color){ 70, 45, 30, 255 });
    DrawRectangle((int)c.x - 16, (int)c.y + 12, 32, 6, (Color){ 70, 45, 30, 255 });

    // Livro aberto brilhando
    DrawCircleV((Vector2){ c.x, c.y - 10 }, 18 + sinf(t * 2.0f) * 2, Fade((Color){ 0, 228, 255, 255 }, 0.12f));
    DesenharTrianguloB2((Vector2){ c.x, c.y - 6 }, (Vector2){ c.x - 16, c.y - 12 }, (Vector2){ c.x - 16, c.y - 2 }, (Color){ 230, 225, 200, 255 });
    DesenharTrianguloB2((Vector2){ c.x, c.y - 6 }, (Vector2){ c.x + 16, c.y - 12 }, (Vector2){ c.x + 16, c.y - 2 }, (Color){ 230, 225, 200, 255 });
    DrawRectangle((int)c.x - 16, (int)c.y - 12, 32, 10, (Color){ 230, 225, 200, 255 });
    DrawLine((int)c.x, (int)c.y - 12, (int)c.x, (int)c.y - 2, (Color){ 120, 100, 80, 255 });

    const char *nome = "ENCICLOPÉDIA";
    DrawText(nome, (int)c.x - MeasureText(nome, 10) / 2, (int)c.y + 22, 10, (Color){ 0, 228, 255, 255 });
}

// Tela do desafio educativo (por cima da sala)
static void DesenharDesafioRogue(const Biblioteca *b, float tempo)
{
    const DesafioEdu *d = &DESAFIOS[b->desafio];

    DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, 0.65f));

    Rectangle painel = { 60, 70, 680, 440 };
    DrawRectangleRec(painel, (Color){ 18, 15, 30, 255 });
    DrawRectangleLinesEx(painel, 4, GOLD);

    int x = (int)painel.x + 24, y = (int)painel.y + 16;

    DrawText("GUARDIÃO SELADO // DESAFIO", x, y, 12, GOLD);
    const char *tipo = TextFormat("%s   //   %s", NOMES_TIPOS_DESAFIO[d->tipo], NOMES_CONCEITOS[d->conceito]);
    DrawText(tipo, x, y + 18, 10, Fade(SKYBLUE, 0.9f));

    char linhas[512];
    QuebrarTexto(d->enunciado, (int)painel.width - 48, 18, linhas, (int)sizeof(linhas));
    int nl = DesenharTextoLinhas(linhas, x, y + 42, 18, 24, WHITE);
    y += 42 + nl * 24 + 14;

    // Opções
    for (int i = 0; i < d->numOpcoes; i++) {

        bool marcada = (b->selecaoDesafio == i) && !b->respondido;
        bool riscada = b->opcaoRiscada[i];
        Color cor = riscada ? Fade(GRAY, 0.5f) : (marcada ? YELLOW : LIGHTGRAY);

        if (b->respondido && i == d->correta)                       cor = LIME;
        else if (b->respondido && i == b->selecaoDesafio && !b->acertouDesafio) cor = RED;

        char op[256];
        QuebrarTexto(TextFormat("%d) %s", i + 1, d->opcoes[i]), (int)painel.width - 90, 16, op, (int)sizeof(op));

        if (marcada)
            DesenharCoracao((float)x + 8, (float)y + 8, 2, RED);

        int n = DesenharTextoLinhas(op, x + 26, y, 16, 20, cor);
        if (riscada)
            DrawLine(x + 26, y + 8, x + 26 + MeasureText(op, 16), y + 8, Fade(GRAY, 0.8f));
        y += n * 20 + 8;
    }

    // Dica (Lente, Luci ou Manual)
    if (b->dicaDesafio[0] != '\0' && !b->respondido) {

        Color corDica = b->dicaDaLuci ? (b->luciMentiu ? MAGENTA : (Color){ 0, 228, 255, 255 }) : GOLD;
        char dica[256];
        QuebrarTexto(b->dicaDesafio, (int)painel.width - 90, 14, dica, (int)sizeof(dica));

        if (b->dicaDaLuci) {
            Vector2 luci = { (float)x + 12, (float)y + 14 };
            DesenharLuciTamanho(luci, 26.0f);
            if (b->luciMentiu)
                DrawRectangle(x + 34 + GetRandomValue(-2, 2), y + 2, 4, 14, Fade(MAGENTA, 0.6f));
        }

        DesenharTextoLinhas(dica, x + 36, y + 6, 14, 18, corDica);
    }

    // Resposta e explicação
    if (b->respondido) {

        int yr = (int)(painel.y + painel.height) - 110;
        DrawLine(x, yr - 8, (int)(painel.x + painel.width) - 24, yr - 8, Fade(GOLD, 0.4f));

        DrawText(b->acertouDesafio ? "CERTO! O selo se quebra." : "ERRADO! O guardião prepara um ataque especial.",
                 x, yr, 18, b->acertouDesafio ? LIME : RED);

        char exp[384];
        QuebrarTexto(d->explicacao, (int)painel.width - 48, 14, exp, (int)sizeof(exp));
        DesenharTextoLinhas(exp, x, yr + 26, 14, 18, WHITE);

        if (b->dicaDaLuci && b->luciMentiu)
            DrawText("A dica da Luci estava errada. Ela também pode alucinar!", x, yr + 70, 12, MAGENTA);

        if (((int)(GetTime() * 2.0)) % 2 == 0)
            DrawText("ENTER >", (int)(painel.x + painel.width) - 90, (int)(painel.y + painel.height) - 24, 12, GOLD);
        return;
    }

    // Teclas de ajuda
    const char *lente = (b->j.itens[ITEM_LENTE] > 0) ? TextFormat("L: Lente da Verificação (x%d)", b->j.itens[ITEM_LENTE])
                                                      : "L: (sem Lente da Verificação)";
    const char *luci  = b->dicaDaLuci ? "H: a Luci já deu a dica" : "H: dica da Luci (grátis... confiável?)";
    const char *manual = (b->j.itens[ITEM_MANUAL] > 0) ? TextFormat("Manual: segunda chance (x%d)", b->j.itens[ITEM_MANUAL]) : "";

    int yb = (int)(painel.y + painel.height) - 46;
    DrawText(lente, x, yb, 12, (b->j.itens[ITEM_LENTE] > 0) ? GOLD : GRAY);
    DrawText(luci, x + 250, yb, 12, b->dicaDaLuci ? GRAY : (Color){ 0, 228, 255, 255 });
    DrawText(manual, x + 500, yb, 12, GOLD);
    DrawText("W/S ou 1-4: escolher    E/ENTER: responder", x, yb + 18, 10, GRAY);
    (void)tempo;
}

// Enciclopédia aberta no átrio
static void DesenharEnciclopediaRogue(const Biblioteca *b)
{
    DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, 0.6f));

    Rectangle painel = { 110, 90, 580, 400 };
    DrawRectangleRec(painel, (Color){ 18, 15, 30, 255 });
    DrawRectangleLinesEx(painel, 4, (Color){ 0, 228, 255, 255 });

    DrawText("ENCICLOPÉDIA DA BIBLIOTECA", (int)painel.x + 24, (int)painel.y + 18, 20, (Color){ 0, 228, 255, 255 });
    DrawText("O conhecimento descoberto nunca se perde.", (int)painel.x + 24, (int)painel.y + 44, 10, LIGHTGRAY);

    for (int c = 0; c < NUM_CONCEITOS; c++) {

        int y = (int)painel.y + 76 + c * 96;
        bool sabe = (b->perm->conceitos & (1u << c)) != 0;
        bool marcado = (b->selecaoEnciclopedia == c);

        if (marcado) {
            DrawRectangle((int)painel.x + 12, y - 6, (int)painel.width - 24, 88, Fade((Color){ 0, 228, 255, 255 }, 0.10f));
            DesenharCoracao(painel.x + 30, (float)y + 8, 2, RED);
        }

        DrawText(TextFormat("CONCEITO %d: %s", c + 1, sabe ? NOMES_CONCEITOS[c] : "???"), (int)painel.x + 50, y, 16,
                 sabe ? (marcado ? YELLOW : WHITE) : GRAY);

        char resumo[256];
        QuebrarTexto(sabe ? RESUMOS_CONCEITOS[c] : "Ainda não descoberto. Procure na Biblioteca.", (int)painel.width - 90, 14,
                     resumo, (int)sizeof(resumo));
        DesenharTextoLinhas(resumo, (int)painel.x + 50, y + 24, 14, 18, sabe ? LIGHTGRAY : Fade(GRAY, 0.7f));
    }

    const char *ajuda = "W/S escolher    E ler o texto completo    BACKSPACE sair";
    DrawText(ajuda, LARGURA_TELA / 2 - MeasureText(ajuda, 10) / 2, (int)(painel.y + painel.height) - 20, 10, GRAY);
}

// Loja do Byteco por cima da sala: aba de itens (desta tentativa) e de melhorias (para sempre)
static void DesenharLojaRogue(const Biblioteca *b, float tempo)
{
    DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, 0.6f));

    Rectangle painel = { 150, 70, 500, 440 };
    DrawRectangleRec(painel, (Color){ 18, 15, 30, 255 });
    DrawRectangleLinesEx(painel, 4, GOLD);

    DrawText("LOJA DO BYTECO", (int)painel.x + 24, (int)painel.y + 18, 22, GOLD);

    const char *fc = TextFormat("SEUS FRAGMENTOS: %d", b->perm->fragmentos);
    DrawText(fc, (int)(painel.x + painel.width) - 24 - MeasureText(fc, 14), (int)painel.y + 24, 14, (Color){ 0, 228, 255, 255 });

    // Abas
    static const char *abas[2] = { "ITENS (esta tentativa)", "MELHORIAS (para sempre)" };
    for (int a = 0; a < 2; a++) {
        Rectangle r = { painel.x + 24 + a * 230, painel.y + 52, 220, 24 };
        bool ativa = (b->abaLoja == a);
        DrawRectangleRec(r, ativa ? Fade(GOLD, 0.25f) : Fade(GRAY, 0.1f));
        DrawRectangleLinesEx(r, 2, ativa ? GOLD : Fade(GRAY, 0.5f));
        DrawText(abas[a], (int)(r.x + r.width / 2) - MeasureText(abas[a], 12) / 2, (int)r.y + 6, 12, ativa ? GOLD : GRAY);
    }

    int linhas = (b->abaLoja == 0) ? NUM_ITENS : NUM_MELHORIAS;

    for (int i = 0; i <= linhas; i++) {

        int y = (int)painel.y + 90 + i * 46;
        bool marcado = (b->selecaoLoja == i);

        if (marcado)
            DrawRectangle((int)painel.x + 12, y - 4, (int)painel.width - 24, 40, Fade(GOLD, 0.12f));

        if (i == linhas) {
            DrawText("SAIR", (int)painel.x + 60, y + 8, 16, marcado ? YELLOW : LIGHTGRAY);
            if (marcado) DesenharCoracao(painel.x + 34, (float)y + 16, 2, RED);
            continue;
        }

        if (marcado) DesenharCoracao(painel.x + 24, (float)y + 16, 2, RED);

        if (b->abaLoja == 0) {

            const DefItem *it = &ITENS[i];
            bool podePagar = b->perm->fragmentos >= it->preco;

            DesenharIconeItem((TipoItem)i, (Vector2){ painel.x + 60, (float)y + 16 }, 1.0f, tempo);
            DrawText(it->nome, (int)painel.x + 84, y, 16, marcado ? YELLOW : WHITE);
            DrawText(it->efeito, (int)painel.x + 84, y + 19, 10, LIGHTGRAY);

            const char *preco = TextFormat("%d FC", it->preco);
            DrawText(preco, (int)(painel.x + painel.width) - 30 - MeasureText(preco, 16), y + 4, 16, podePagar ? (Color){ 0, 228, 255, 255 } : RED);

            if (b->j.itens[i] > 0)
                DrawText(TextFormat("tem %d", b->j.itens[i]), (int)(painel.x + painel.width) - 92, y + 22, 10, GRAY);

        } else {

            int nivel = b->perm->melhorias[i];
            bool maximo = (nivel >= NIVEL_MAX_MELHORIA);

            DrawText(NOMES_MELHORIAS[i], (int)painel.x + 50, y, 16, marcado ? YELLOW : WHITE);
            DrawText(EFEITOS_MELHORIAS[i], (int)painel.x + 50, y + 19, 10, LIGHTGRAY);

            for (int k = 0; k < NIVEL_MAX_MELHORIA; k++) {
                Rectangle pip = { painel.x + 300 + k * 16, (float)y + 6, 12, 12 };
                if (k < nivel) DrawRectangleRec(pip, GOLD);
                DrawRectangleLinesEx(pip, 1, GOLD);
            }

            const char *preco = maximo ? "MÁXIMO" : TextFormat("%d FC", PRECO_MELHORIA[nivel]);
            bool podePagar = !maximo && b->perm->fragmentos >= PRECO_MELHORIA[nivel];
            DrawText(preco, (int)(painel.x + painel.width) - 30 - MeasureText(preco, 16), y + 4, 16,
                     maximo ? GOLD : (podePagar ? (Color){ 0, 228, 255, 255 } : RED));
        }
    }

    const char *ajuda = "A/D trocar a aba    W/S escolher    E comprar    BACKSPACE sair";
    DrawText(ajuda, LARGURA_TELA / 2 - MeasureText(ajuda, 10) / 2, (int)(painel.y + painel.height) - 20, 10, GRAY);
}

static void RogueDesenhar(const Biblioteca *b, const SpriteDirecao sprites[NUM_DIRECOES], float anguloLuci,
                          float tempo, const char *nickname, int pontos)
{
    const JogadorRogue *j = &b->j;
    bool atrio = (b->tipoSala == SALA_ATRIO);

    ClearBackground((Color){ 8, 6, 18, 255 });

    // A tela só treme quando o jogador toma dano
    Camera2D cam = { 0 };
    cam.zoom = 1.0f;
    if (b->tremor > 0.0f)
        cam.offset = (Vector2){ (float)GetRandomValue(-4, 4), (float)GetRandomValue(-4, 4) };

    BeginMode2D(cam);

    // Chão da biblioteca: ladrilhos (no átrio, um tapete no caminho da porta)
    DrawRectangleRec(ARENA_ROGUE, atrio ? (Color){ 22, 18, 38, 255 } : (Color){ 18, 15, 30, 255 });

    for (int x = (int)ARENA_ROGUE.x; x < ARENA_ROGUE.x + ARENA_ROGUE.width; x += 40)
        DrawLine(x, (int)ARENA_ROGUE.y, x, (int)(ARENA_ROGUE.y + ARENA_ROGUE.height), Fade(PURPLE, 0.10f));
    for (int y = (int)ARENA_ROGUE.y; y < ARENA_ROGUE.y + ARENA_ROGUE.height; y += 40)
        DrawLine((int)ARENA_ROGUE.x, y, (int)(ARENA_ROGUE.x + ARENA_ROGUE.width), y, Fade(PURPLE, 0.10f));

    if (atrio)
        DrawRectangle(370, 74, 60, 456, Fade(PURPLE, 0.18f));

    // Prateleira decorativa na parede de cima
    DesenharEstante((Rectangle){ ARENA_ROGUE.x, 42, 320, 16 }, tempo);
    DesenharEstante((Rectangle){ 440, 42, 320, 16 }, tempo);

    DrawRectangleLinesEx(ARENA_ROGUE, 4, Fade(PURPLE, 0.6f));

    // Estantes (no átrio, o Byte e o Byteco também ocupam lugar, mas são desenhados à parte)
    for (int i = 0; i < b->numObst; i++)
        if (!atrio || i < 2)
            DesenharEstante(b->obst[i], tempo);

    // Portas
    if (atrio) {
        DesenharPortaRogue(PORTA_PROXIMA_ROGUE, true, "BIBLIOTECA (MISSÃO)", tempo);
        DesenharPortaRogue(PORTA_SAIDA_ROGUE, true, "CORREDOR", tempo);
    } else if (b->tipoSala != SALA_CHEFE) {
        int proxima = b->indiceRota + 1;
        const char *nome = (proxima < SALAS_POR_MISSAO) ? TextFormat("PRÓXIMA: %s", NOMES_SALAS_ROGUE[b->rota[proxima]])
                                                        : "PRÓXIMA: ALUCINADOR";
        DesenharPortaRogue(PORTA_PROXIMA_ROGUE, b->limpa, nome, tempo);
    }

    // Personagens e objetos de cada tipo de sala
    if (atrio) {
        DesenharProfessorByte(POS_BYTE, tempo);
        DesenharByteco(POS_BYTECO, tempo);
        DesenharPulpitoEnciclopedia(POS_ENCICLOPEDIA, tempo);
    } else if (b->tipoSala == SALA_EDUCATIVA) {
        DesenharTerminal(POS_TERMINAL, b->terminalLido, tempo);
    } else if (b->tipoSala == SALA_RECOMPENSA) {
        for (int i = 0; i < NUM_PEDESTAIS; i++)
            DesenharPedestal(POS_PEDESTAIS[i], b->pedestal[i], !b->recompensaPega, tempo);
    } else if (b->tipoSala == SALA_CHEFE) {
        DesenharPortalChefe(POS_PORTAL, tempo);
    }

    // Coisas no chão
    for (int i = 0; i < MAX_COLETAVEIS; i++) {
        const Coletavel *c = &b->col[i];
        if (!c->ativo) continue;
        if (c->fragmento) DesenharFragmentoConhecimento(c->pos, c->anim);
        else              DesenharIconeItem(c->item, c->pos, 0.8f, tempo);
    }

    // Pulso de Integridade: anel se abrindo
    if (j->efeitoPulso > 0.0f) {
        float k = 1.0f - j->efeitoPulso / 0.4f;
        DrawCircleV(j->pos, PULSO_RAIO * k, Fade(SKYBLUE, 0.15f * (1.0f - k)));
        DrawCircleLines((int)j->pos.x, (int)j->pos.y, PULSO_RAIO * k, Fade(SKYBLUE, 1.0f - k));
    }

    for (int i = 0; i < MAX_INIMIGOS; i++)
        if (b->ini[i].ativo)
            DesenharInimigo(&b->ini[i]);

    // Protagonista (pisca enquanto está invulnerável depois do dano)
    if (!b->morreu) {

        bool pisca = j->invulneravel > 0.0f && j->tempoEsquiva <= 0.0f && ((int)(j->invulneravel * 12.0f)) % 2 == 0;

        if (!pisca) {
            DesenharJogadorComLuci(sprites, j->pos, j->direcao, j->anim.quadro, true, anguloLuci);

            // Disparador de Dados na mão, apontado para a mira (guardado no átrio)
            if (!atrio) {
                Vector2 base = Vector2Add(j->pos, (Vector2){ 0, 4 });
                Vector2 ponta = Vector2Add(base, Vector2Scale(j->mira, 20.0f));
                DrawLineEx(base, ponta, 5, (Color){ 30, 60, 120, 255 });
                DrawLineEx(base, ponta, 2, SKYBLUE);
                DrawCircleV(ponta, 2.5f, WHITE);
            }
        }
    }

    // Projéteis: os do jogador em ciano, os inimigos em laranja com contorno (fácil de ver)
    for (int i = 0; i < MAX_PROJETEIS; i++) {

        const Projetil *p = &b->proj[i];

        if (!p->ativo)
            continue;

        if (p->doJogador) {
            DrawCircleV(p->pos, p->raio + 3, Fade(SKYBLUE, 0.3f));
            DrawCircleV(p->pos, p->raio, (Color){ 180, 240, 255, 255 });
        } else {
            DrawCircleV(p->pos, p->raio + 2, (Color){ 30, 0, 20, 255 });
            DrawCircleV(p->pos, p->raio, ORANGE);
            DrawCircleV(p->pos, p->raio - 2, YELLOW);
        }
    }

    for (int i = 0; i < MAX_PARTICULAS_ROGUE; i++) {
        const ParticulaRogue *p = &b->part[i];
        if (p->vida > 0.0f)
            DrawRectangleV((Vector2){ p->pos.x - p->tam / 2, p->pos.y - p->tam / 2 }, (Vector2){ p->tam, p->tam },
                           Fade(p->cor, p->vida / p->vidaMax));
    }

    // Tecla E sobre o que dá para usar
    int pedestal = -1;
    AlvoRogue alvo = RogueAlvoProximo(b, &pedestal);

    if (!b->lojaAberta && !b->enciclopediaAberta && !b->desafioAberto && !b->morreu) {
        if (alvo == ALVO_ENCICLOPEDIA)  DesenharTeclaE((Vector2){ POS_ENCICLOPEDIA.x, POS_ENCICLOPEDIA.y - 40 }, tempo);
        else if (alvo == ALVO_BYTE)          DesenharTeclaE((Vector2){ POS_BYTE.x, POS_BYTE.y - 48 }, tempo);
        else if (alvo == ALVO_BYTECO)   DesenharTeclaE((Vector2){ POS_BYTECO.x, POS_BYTECO.y - 42 }, tempo);
        else if (alvo == ALVO_TERMINAL) DesenharTeclaE((Vector2){ POS_TERMINAL.x, POS_TERMINAL.y - 36 }, tempo);
        else if (alvo == ALVO_PORTAL)   DesenharTeclaE((Vector2){ POS_PORTAL.x, POS_PORTAL.y - 86 }, tempo);
        else if (alvo == ALVO_PEDESTAL) {
            Vector2 c = POS_PEDESTAIS[pedestal];
            DesenharTeclaE((Vector2){ c.x, c.y - 42 }, tempo);
            const DefItem *it = &ITENS[b->pedestal[pedestal]];
            DrawText(it->nome, (int)c.x - MeasureText(it->nome, 12) / 2, (int)c.y + 26, 12, GOLD);
            DrawText(it->efeito, (int)c.x - MeasureText(it->efeito, 10) / 2, (int)c.y + 42, 10, LIGHTGRAY);
        }
    }

    EndMode2D();

    // Mira do mouse (só onde dá para atirar)
    if (!j->miraSetas && !b->morreu && !atrio && !b->lojaAberta && !b->desafioAberto) {
        Vector2 m = GetMousePosition();
        DrawCircleLines((int)m.x, (int)m.y, 9, SKYBLUE);
        DrawLine((int)m.x - 13, (int)m.y, (int)m.x - 5, (int)m.y, SKYBLUE);
        DrawLine((int)m.x + 5, (int)m.y, (int)m.x + 13, (int)m.y, SKYBLUE);
        DrawLine((int)m.x, (int)m.y - 13, (int)m.x, (int)m.y - 5, SKYBLUE);
        DrawLine((int)m.x, (int)m.y + 5, (int)m.x, (int)m.y + 13, SKYBLUE);
    }

    // Objetivo no topo
    Objetivo o = { "", false, { 0, 0 } };

    switch (b->tipoSala) {
        case SALA_ATRIO:
            snprintf(o.texto, sizeof(o.texto), "%s", b->perm->byteVisto ? "Entre na Biblioteca pela porta de cima (Byteco: loja)"
                                                                         : "Fale com o Professor Byte");
            break;
        case SALA_COMBATE: {
            bool selado = false;
            for (int i = 0; i < MAX_INIMIGOS; i++)
                if (b->ini[i].ativo && b->ini[i].selado) selado = true;

            if (b->limpa)       snprintf(o.texto, sizeof(o.texto), "Sala limpa! Siga pela porta de cima");
            else if (selado)    snprintf(o.texto, sizeof(o.texto), "Chegue perto do Guardião SELADO e quebre o selo");
            else                snprintf(o.texto, sizeof(o.texto), "Elimine os dados corrompidos (%d)", RogueInimigosVivos(b));
        } break;
        case SALA_EDUCATIVA:
            snprintf(o.texto, sizeof(o.texto), "%s", b->terminalLido ? "Siga pela porta de cima" : "Leia o terminal de conhecimento");
            break;
        case SALA_RECOMPENSA:
            snprintf(o.texto, sizeof(o.texto), "%s", b->recompensaPega ? "Siga pela porta de cima" : "Escolha 1 recompensa (E) ou siga em frente");
            break;
        default:
            snprintf(o.texto, sizeof(o.texto), "Entre no portal do ALUCINADOR");
            break;
    }

    DesenharObjetivoHUD(&o);

    // Caminho da missão (canto de cima): uma casinha por sala
    if (!atrio) {
        for (int i = 0; i <= SALAS_POR_MISSAO; i++) {
            TipoSalaRogue t = (i < SALAS_POR_MISSAO) ? b->rota[i] : SALA_CHEFE;
            Rectangle r = { 14.0f + i * 15.0f, 18, 11, 11 };
            Color cor = CORES_SALAS_ROGUE[t];

            if (i < b->indiceRota)       DrawRectangleRec(r, Fade(cor, 0.35f));
            else if (i == b->indiceRota) DrawRectangleRec(r, cor);
            DrawRectangleLinesEx(r, 1, cor);
        }
    }

    // HUD de baixo: vida, energia, recargas, pontos, fragmentos e mochila
    int yHud = 542;

    DrawRectangle(0, yHud - 4, LARGURA_TELA, ALTURA_TELA - yHud + 4, (Color){ 12, 10, 24, 255 });
    DrawLine(0, yHud - 4, LARGURA_TELA, yHud - 4, Fade(PURPLE, 0.6f));

    DrawText(nickname, 14, yHud + 4, 20, WHITE);

    DrawText("HP", 66, yHud + 2, 10, WHITE);
    DrawRectangle(84, yHud, 140, 12, (Color){ 60, 10, 20, 255 });
    DrawRectangle(84, yHud, (int)(140 * j->hp / RogueHpMax(b)), 12, (j->hp > RogueHpMax(b) * 0.3f) ? YELLOW : RED);
    DrawRectangleLines(84, yHud, 140, 12, GRAY);
    DrawText(TextFormat("%d", (int)ceilf(j->hp)), 230, yHud + 1, 10, WHITE);

    DrawText("EP", 66, yHud + 20, 10, WHITE);
    DrawRectangle(84, yHud + 18, 140, 12, (Color){ 10, 30, 50, 255 });
    DrawRectangle(84, yHud + 18, (int)(140 * j->ep / RogueEpMax(b)), 12, SKYBLUE);
    DrawRectangleLines(84, yHud + 18, 140, 12, GRAY);
    DrawText(TextFormat("%d", (int)j->ep), 230, yHud + 19, 10, WHITE);

    DesenharRecargaRogue(278, yHud + 15, j->recargaPulso, PULSO_RECARGA, "Q", SKYBLUE, j->ep >= PULSO_CUSTO_EP);
    DrawText("PULSO", 263, yHud + 32, 8, LIGHTGRAY);
    DesenharRecargaRogue(324, yHud + 15, j->recargaEsquiva, ESQUIVA_RECARGA, "ESP", LIME, true);
    DrawText("ESQUIVA", 305, yHud + 32, 8, LIGHTGRAY);

    DrawText(TextFormat("PONTOS: %d", pontos), 360, yHud + 2, 14, GOLD);
    DesenharFragmentoConhecimento((Vector2){ 366, (float)yHud + 26 }, tempo);
    DrawText(TextFormat("%d FRAGMENTOS", b->perm->fragmentos), 378, yHud + 21, 10, (Color){ 0, 228, 255, 255 });

    // Mochila: as teclas dos itens de usar e os passivos
    for (int i = 0; i < NUM_ITENS; i++) {

        int x = 500 + i * 48;
        bool tem = j->itens[i] > 0;

        DrawRectangle(x - 18, yHud, 36, 36, Fade((Color){ 30, 24, 50, 255 }, tem ? 1.0f : 0.4f));
        DrawRectangleLines(x - 18, yHud, 36, 36, tem ? Fade(GOLD, 0.7f) : Fade(GRAY, 0.3f));

        if (tem) {
            DesenharIconeItem((TipoItem)i, (Vector2){ (float)x, (float)yHud + 16 }, 0.8f, tempo);
            DrawText(TextFormat("x%d", j->itens[i]), x + 4, yHud + 26, 10, WHITE);
        }

        if (i == ITEM_ANTIVIRUS)    DrawText("1", x - 15, yHud + 2, 10, GOLD);
        if (i == ITEM_PACOTE_DADOS) DrawText("2", x - 15, yHud + 2, 10, GOLD);
    }

    const char *controles = atrio ? "WASD andar    E falar, comprar e ler a enciclopédia    porta de cima: missão    porta da esquerda: corredor"
                                  : "WASD andar   MOUSE/SETAS mirar e atirar   ESPAÇO esquivar   Q/BOTÃO DIR. pulso   1/2 itens   E usar";
    DrawText(controles, LARGURA_TELA / 2 - MeasureText(controles, 10) / 2, ALTURA_TELA - 14, 10, GRAY);

    // Mensagem curta no meio da tela
    if (b->tempoAviso > 0.0f) {
        float k = fminf(b->tempoAviso / 0.4f, 1.0f);
        int w = MeasureText(b->aviso, 18);
        DrawRectangle(LARGURA_TELA / 2 - w / 2 - 12, 92, w + 24, 30, Fade(BLACK, 0.7f * k));
        DrawText(b->aviso, LARGURA_TELA / 2 - w / 2, 98, 18, Fade(GOLD, k));
    }

    if (b->lojaAberta)
        DesenharLojaRogue(b, tempo);

    if (b->enciclopediaAberta)
        DesenharEnciclopediaRogue(b);

    if (b->desafioAberto)
        DesenharDesafioRogue(b, tempo);

    // Entrando numa sala nova
    if (b->fadeEntrada > 0.0f)
        DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, b->fadeEntrada / 0.35f));

    // Derrota
    if (b->morreu) {

        float k = fminf(b->tempoMorte / 0.6f, 1.0f);
        DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, 0.7f * k));

        const char *titulo = "VOCÊ FOI CORROMPIDO";
        DrawText(titulo, LARGURA_TELA / 2 - MeasureText(titulo, 36) / 2, 230, 36, Fade(MAGENTA, k));

        const char *sub = TextFormat("Os ecos falsos venceram desta vez.  -%d pontos", PENALIDADE_HP_ZERO);
        DrawText(sub, LARGURA_TELA / 2 - MeasureText(sub, 16) / 2, 282, 16, Fade(WHITE, k));

        const char *perda = "Você volta ao átrio sem os itens. Os Fragmentos de Conhecimento ficam com você.";
        DrawText(perda, LARGURA_TELA / 2 - MeasureText(perda, 12) / 2, 308, 12, Fade((Color){ 0, 228, 255, 255 }, k));

        if (b->tempoMorte > 1.0f && ((int)(GetTime() * 2.0)) % 2 == 0)
            DrawText("ENTER - voltar ao átrio", LARGURA_TELA / 2 - MeasureText("ENTER - voltar ao átrio", 14) / 2, 340, 14, GOLD);
    }
}

// =============================================================
// MENU DE PAUSE
// =============================================================

#define DESFOQUE_PAUSA  4        // quanto maior, mais desfocado fica o fundo (2 a 8)

typedef enum { PAUSA_CONTINUAR, PAUSA_MENU, PAUSA_SAIR, NUM_OPCOES_PAUSA } OpcaoPausa;

// Tira uma "foto" da tela atual (com as cores normais do jogo) e desfoca:
// diminui e aumenta de novo a imagem, perdendo os detalhes finos.
// É feito uma vez só, no momento em que o jogo pausa.
static Texture2D CapturarFundoPausa(RenderTexture2D alvo)
{
    // A textura de desenho fica de cabeça para baixo na memória
    Image img = LoadImageFromTexture(alvo.texture);
    ImageFlipVertical(&img);
    int w = img.width;
    int h = img.height;

    ImageResize(&img, w / DESFOQUE_PAUSA, h / DESFOQUE_PAUSA);
    ImageResize(&img, w, h);

    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

    Texture2D t = LoadTextureFromImage(img);
    SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
    UnloadImage(img);

    return t;
}

static Rectangle RetanguloOpcaoPausa(int i)
{
    return (Rectangle){ 270, 255 + i * 60.0f, 260, 44 };
}

// Lê teclado e mouse no menu de pause.
// Devolve a opção escolhida (PAUSA_CONTINUAR / PAUSA_MENU / PAUSA_SAIR) ou -1 se nada foi escolhido.
//   W / S / SETAS          : troca a opção
//   ENTER / E / ESPAÇO     : confirma
//   mouse                  : passa por cima para marcar, clica para escolher
static int AtualizarPausa(int *selecao)
{
    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))
        *selecao = (*selecao + NUM_OPCOES_PAUSA - 1) % NUM_OPCOES_PAUSA;

    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN))
        *selecao = (*selecao + 1) % NUM_OPCOES_PAUSA;

    Vector2 mouse = GetMousePosition();
    bool mouseMexeu = (Vector2Length(GetMouseDelta()) > 0.0f);

    for (int i = 0; i < NUM_OPCOES_PAUSA; i++) {

        if (!CheckCollisionPointRec(mouse, RetanguloOpcaoPausa(i)))
            continue;

        if (mouseMexeu)
            *selecao = i;

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            return i;
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE))
        return *selecao;

    return -1;
}

static void DesenharPausa(Texture2D fundo, int selecao)
{
    // Paleta do jogo
    Color fundoEscuro = (Color){ 8, 6, 18, 255 };      // fundo das salas
    Color painelCor   = (Color){ 18, 15, 30, 255 };    // chão das salas

    // Fundo: a tela do jogo congelada e desfocada, com as cores normais
    ClearBackground(fundoEscuro);

    if (fundo.id > 0) {
        DrawTexturePro(
            fundo,
            (Rectangle){ 0, 0, (float)fundo.width, (float)fundo.height },
            (Rectangle){ 0, 0, LARGURA_TELA, ALTURA_TELA },
            (Vector2){ 0, 0 },
            0.0f,
            WHITE
        );
    }

    // Escurece só um pouco, para o menu se destacar
    DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(fundoEscuro, 0.35f));

    // Painel roxo, com um filete azul por dentro
    Rectangle painel = { 240, 140, 320, 335 };

    DrawRectangleRec(painel, Fade(painelCor, 0.92f));
    DrawRectangleLinesEx(painel, 4, PURPLE);
    DrawRectangleLinesEx((Rectangle){ painel.x + 8, painel.y + 8, painel.width - 16, painel.height - 16 }, 1, Fade(SKYBLUE, 0.35f));

    const char *titulo = "PAUSADO";
    DrawText(titulo, LARGURA_TELA / 2 - MeasureText(titulo, 40) / 2, 170, 40, PURPLE);

    // Opções, com o coração na marcada (estilo Undertale, em azul)
    const char *opcoes[NUM_OPCOES_PAUSA] = { "CONTINUAR", "MENU PRINCIPAL", "SAIR DO JOGO" };

    for (int i = 0; i < NUM_OPCOES_PAUSA; i++) {

        Rectangle r = RetanguloOpcaoPausa(i);
        bool marcada = (i == selecao);

        if (marcada) {
            DrawRectangleRec(r, Fade(PURPLE, 0.15f));
            DesenharCoracao(r.x + 26, r.y + r.height / 2, 2, SKYBLUE);
        }

        DrawText(opcoes[i], (int)r.x + 50, (int)(r.y + r.height / 2) - 10, 20, marcada ? SKYBLUE : LIGHTGRAY);
    }

    const char *dica1 = "ESC - voltar ao jogo";
    const char *dica2 = "O progresso é salvo ao sair ou voltar ao menu";

    DrawText(dica1, LARGURA_TELA / 2 - MeasureText(dica1, 10) / 2, 440, 10, Fade(SKYBLUE, 0.6f));
    DrawText(dica2, LARGURA_TELA / 2 - MeasureText(dica2, 10) / 2, 454, 10, GRAY);
}

// =============================================================
// RESOLUÇÃO
// =============================================================
// O jogo é sempre desenhado em 800x600 numa textura, que depois é esticada
// para o tamanho da janela (mantendo a proporção, com faixas pretas se
// sobrar espaço). Assim nenhuma coordenada do jogo precisa mudar.

#define ARQUIVO_CONFIG   "utopia.cfg"
#define MAX_RESOLUCOES   8               // itens no máximo em cada lista suspensa

typedef enum { EXIBICAO_JANELA, EXIBICAO_TELA_CHEIA, NUM_MODOS_EXIBICAO } ModoExibicao;

static const char *NOMES_MODO_EXIBICAO[NUM_MODOS_EXIBICAO] = { "JANELA", "TELA CHEIA" };

typedef struct {
    int largura;
    int altura;                           // 0 x 0 = o maior tamanho que cabe no monitor
} Resolucao;

typedef struct {
    ModoExibicao modo;
    Resolucao resolucao;
} ConfigExibicao;

// Tamanhos 4:3 (a proporção do jogo), do maior para o menor.
// Na janela é o tamanho da janela; na tela cheia é o tamanho da imagem do jogo
// no meio da tela, com faixas pretas em volta.
// A tela cheia não troca a resolução do monitor: nos testes o Windows desfazia
// essa troca sozinho depois de meio segundo, então o monitor fica sempre na nativa.
static const Resolucao RESOLUCOES_4X3[] = {
    { 1600, 1200 },
    { 1280,  960 },
    { 1200,  900 },
    { 1024,  768 },
    {  800,  600 },
};

#define NUM_RESOLUCOES_4X3  ((int)(sizeof(RESOLUCOES_4X3) / sizeof(RESOLUCOES_4X3[0])))

// Monitor onde o jogo abriu e o tamanho dele
static int monitorJogo = 0;
static int larguraMonitor = LARGURA_TELA;
static int alturaMonitor = ALTURA_TELA;

// Tamanho da imagem do jogo na tela cheia (0 x 0 = o maior que couber)
static Resolucao areaTelaCheia = { 0, 0 };
static bool emTelaCheia = false;

static void GuardarMonitor(void)
{
    monitorJogo = GetCurrentMonitor();
    larguraMonitor = GetMonitorWidth(monitorJogo);
    alturaMonitor = GetMonitorHeight(monitorJogo);
}

static bool MesmaResolucao(Resolucao a, Resolucao b)
{
    return a.largura == b.largura && a.altura == b.altura;
}

// Maior imagem 4:3 que cabe no monitor (ex.: 1440 x 1080 num monitor 1920 x 1080)
static Resolucao MaiorQueCabe(void)
{
    float escala = fminf((float)larguraMonitor / LARGURA_TELA, (float)alturaMonitor / ALTURA_TELA);

    return (Resolucao){ (int)(LARGURA_TELA * escala), (int)(ALTURA_TELA * escala) };
}

// Monta a lista de resoluções de um modo, da maior para a menor.
// Só entram as que cabem no monitor; o 800 x 600 sempre entra.
static int ListarResolucoes(ModoExibicao modo, Resolucao lista[MAX_RESOLUCOES])
{
    int n = 0;
    Resolucao maxima = MaiorQueCabe();

    // Na tela cheia a primeira opção é ocupar a tela toda
    if (modo == EXIBICAO_TELA_CHEIA)
        lista[n++] = (Resolucao){ 0, 0 };

    for (int i = 0; i < NUM_RESOLUCOES_4X3 && n < MAX_RESOLUCOES; i++) {

        Resolucao r = RESOLUCOES_4X3[i];
        bool cabe = (r.largura <= larguraMonitor && r.altura <= alturaMonitor);

        // Na tela cheia, igual ou maior que a máxima já está coberto pela primeira opção
        if (modo == EXIBICAO_TELA_CHEIA)
            cabe = (r.largura < maxima.largura);

        if (cabe || i == NUM_RESOLUCOES_4X3 - 1)
            lista[n++] = r;
    }

    return n;
}

// Posição da resolução na lista do modo (-1 = não está na lista)
static int IndiceResolucao(ModoExibicao modo, Resolucao r)
{
    Resolucao lista[MAX_RESOLUCOES];
    int n = ListarResolucoes(modo, lista);

    for (int i = 0; i < n; i++)
        if (MesmaResolucao(lista[i], r))
            return i;

    return -1;
}

static const char *NomeResolucao(Resolucao r)
{
    if (r.largura == 0) {
        Resolucao maxima = MaiorQueCabe();
        return TextFormat("%d x %d (MÁXIMA)", maxima.largura, maxima.altura);
    }

    return TextFormat("%d x %d", r.largura, r.altura);
}

// A tela cheia é uma janela comum, sem borda, cobrindo o monitor inteiro.
// (O ToggleBorderlessWindowed do raylib prende a janela ao monitor e ela fica
// sempre por cima das outras: o ALT+TAB troca o foco mas o jogo não sai da tela.)
static void AplicarExibicao(ConfigExibicao c)
{
    Vector2 origem = GetMonitorPosition(monitorJogo);

    emTelaCheia = (c.modo == EXIBICAO_TELA_CHEIA);

    if (emTelaCheia) {

        // A imagem do jogo fica no tamanho escolhido, no meio da tela
        areaTelaCheia = c.resolucao;

        SetWindowState(FLAG_WINDOW_UNDECORATED);
        SetWindowSize(larguraMonitor, alturaMonitor);
        SetWindowPosition((int)origem.x, (int)origem.y);
        return;
    }

    areaTelaCheia = (Resolucao){ 0, 0 };
    ClearWindowState(FLAG_WINDOW_UNDECORATED);

    // Janela: muda o tamanho e centraliza no monitor
    int w = c.resolucao.largura;
    int h = c.resolucao.altura;

    SetWindowSize(w, h);
    SetWindowPosition((int)origem.x + (larguraMonitor - w) / 2,
                      (int)origem.y + (alturaMonitor - h) / 2);
}

// Arquivo de configuração: "modo largura altura" (tela cheia máxima = "1 0 0")
static ConfigExibicao LerConfigExibicao(void)
{
    ConfigExibicao padrao = { EXIBICAO_JANELA, { LARGURA_TELA, ALTURA_TELA } };
    ConfigExibicao c;
    int modo = 0;

    FILE *f = fopen(ARQUIVO_CONFIG, "r");

    if (f == NULL)
        return padrao;

    bool ok = (fscanf(f, "%d %d %d", &modo, &c.resolucao.largura, &c.resolucao.altura) == 3);
    fclose(f);

    if (!ok || modo < 0 || modo >= NUM_MODOS_EXIBICAO)
        return padrao;

    c.modo = (ModoExibicao)modo;

    // Resolução que não cabe neste monitor (trocou de monitor, arquivo antigo...)
    if (IndiceResolucao(c.modo, c.resolucao) < 0)
        return padrao;

    return c;
}

static void SalvarConfigExibicao(ConfigExibicao c)
{
    FILE *f = fopen(ARQUIVO_CONFIG, "w");

    if (f == NULL)
        return;

    fprintf(f, "%d %d %d\n", (int)c.modo, c.resolucao.largura, c.resolucao.altura);
    fclose(f);
}

// Quanto a imagem de 800x600 é esticada e onde ela começa dentro da janela
static float EscalaJanela(Vector2 *deslocamento)
{
    float escala = fminf((float)GetScreenWidth() / LARGURA_TELA,
                         (float)GetScreenHeight() / ALTURA_TELA);

    // Tela cheia com tamanho escolhido: não passa dele
    if (emTelaCheia && areaTelaCheia.largura > 0)
        escala = fminf(escala, (float)areaTelaCheia.largura / LARGURA_TELA);

    deslocamento->x = floorf((GetScreenWidth()  - LARGURA_TELA * escala) / 2.0f);
    deslocamento->y = floorf((GetScreenHeight() - ALTURA_TELA  * escala) / 2.0f);

    return escala;
}

// Faz o mouse continuar falando em coordenadas de 800x600,
// qualquer que seja o tamanho da janela
static void AjustarMouseAJanela(void)
{
    Vector2 desloc;
    float escala = EscalaJanela(&desloc);

    SetMouseOffset((int)-desloc.x, (int)-desloc.y);
    SetMouseScale(1.0f / escala, 1.0f / escala);
}

// Filtro "sharp bilinear" para esticar o jogo sem borrar.
// Com escala quebrada (ex.: 1.8x em tela cheia 1920x1080), o bilinear puro
// deixa tudo embaçado e o point deixa pixels de larguras diferentes.
// Aqui cada pixel do jogo vira um bloco sólido e só a borda de 1 pixel
// entre um bloco e outro é suavizada.
#if defined(PLATFORM_WEB)
static const char *SHADER_NITIDEZ =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec2 fragTexCoord;\n"
    "varying vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform vec2 tamanhoTextura;\n"
    "uniform float escala;\n"
    "void main() {\n"
    "    vec2 texel = fragTexCoord * tamanhoTextura;\n"
    "    vec2 centro = fract(texel) - 0.5;\n"
    "    float regiao = 0.5 - 0.5 / escala;\n"
    "    vec2 f = (centro - clamp(centro, -regiao, regiao)) * escala + 0.5;\n"
    "    gl_FragColor = texture2D(texture0, (floor(texel) + f) / tamanhoTextura) * colDiffuse * fragColor;\n"
    "}\n";
#else
static const char *SHADER_NITIDEZ =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform vec2 tamanhoTextura;\n"
    "uniform float escala;\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    vec2 texel = fragTexCoord * tamanhoTextura;\n"
    "    vec2 centro = fract(texel) - 0.5;\n"
    "    float regiao = 0.5 - 0.5 / escala;\n"
    "    vec2 f = (centro - clamp(centro, -regiao, regiao)) * escala + 0.5;\n"
    "    finalColor = texture(texture0, (floor(texel) + f) / tamanhoTextura) * colDiffuse * fragColor;\n"
    "}\n";
#endif

typedef struct {
    Shader shader;
    int locEscala;                        // -1 = shader não carregou (usa o filtro simples)
    int locTamanho;
} FiltroNitidez;

static FiltroNitidez CarregarFiltroNitidez(void)
{
    FiltroNitidez f;

    f.shader = LoadShaderFromMemory(NULL, SHADER_NITIDEZ);
    f.locEscala = GetShaderLocation(f.shader, "escala");
    f.locTamanho = GetShaderLocation(f.shader, "tamanhoTextura");

    return f;
}

// Desenha a textura do jogo na janela (chamar entre BeginDrawing e EndDrawing)
static void DesenharNaJanela(RenderTexture2D alvo, FiltroNitidez filtro)
{
    Vector2 desloc;
    float escala = EscalaJanela(&desloc);
    bool usarShader = (filtro.locEscala >= 0 && filtro.locTamanho >= 0);

    if (usarShader) {

        // O shader precisa do bilinear ligado para suavizar só as bordas
        float escalaShader = fmaxf(escala, 1.0f);
        float tamanho[2] = { (float)alvo.texture.width, (float)alvo.texture.height };

        SetTextureFilter(alvo.texture, TEXTURE_FILTER_BILINEAR);
        SetShaderValue(filtro.shader, filtro.locEscala, &escalaShader, SHADER_UNIFORM_FLOAT);
        SetShaderValue(filtro.shader, filtro.locTamanho, tamanho, SHADER_UNIFORM_VEC2);

    } else {

        SetTextureFilter(alvo.texture, TEXTURE_FILTER_POINT);
    }

    ClearBackground(BLACK);

    if (usarShader)
        BeginShaderMode(filtro.shader);

    DrawTexturePro(
        alvo.texture,
        (Rectangle){ 0, 0, (float)alvo.texture.width, -(float)alvo.texture.height },
        (Rectangle){ desloc.x, desloc.y, LARGURA_TELA * escala, ALTURA_TELA * escala },
        (Vector2){ 0, 0 },
        0.0f,
        WHITE
    );

    if (usarShader)
        EndShaderMode();
}

// -------------------------------------------------------------
// TELA DE OPÇÕES: duas listas suspensas (modo de exibição e resolução)
// -------------------------------------------------------------

typedef enum { CAMPO_MODO, CAMPO_RESOLUCAO, NUM_CAMPOS_OPCOES } CampoOpcoes;

typedef struct {
    int foco;                             // campo marcado (CampoOpcoes)
    int aberta;                           // lista aberta (CampoOpcoes) ou -1
    int marcado;                          // item marcado dentro da lista aberta
} MenuOpcoes;

#define ALTURA_ITEM_LISTA  26

static Rectangle CaixaOpcoes(int campo)
{
    return (Rectangle){ 220, 206 + campo * 84.0f, 360, 36 };
}

// Itens da lista aberta, logo abaixo da caixa
static Rectangle ItemListaOpcoes(int campo, int i)
{
    Rectangle caixa = CaixaOpcoes(campo);

    return (Rectangle){ caixa.x, caixa.y + caixa.height + 2 + i * (float)ALTURA_ITEM_LISTA, caixa.width, ALTURA_ITEM_LISTA };
}

static void AbrirListaOpcoes(MenuOpcoes *m, int campo, ConfigExibicao cfg)
{
    m->foco = campo;
    m->aberta = campo;
    m->marcado = (campo == CAMPO_MODO) ? (int)cfg.modo : IndiceResolucao(cfg.modo, cfg.resolucao);

    if (m->marcado < 0)
        m->marcado = 0;
}

// Lê teclado e mouse na tela de opções.
// Devolve true quando o jogador escolheu algo diferente (cfg já volta atualizada).
//   W / S / SETAS          : troca o campo, ou o item da lista aberta
//   ENTER / E / ESPAÇO     : abre a lista, ou escolhe o item
//   mouse                  : clica na caixa para abrir, no item para escolher, fora para fechar
// (ESC e BACKSPACE fecham a lista; ficam no loop principal)
static bool AtualizarOpcoes(MenuOpcoes *m, ConfigExibicao *cfg)
{
    Vector2 mouse = GetMousePosition();
    bool mouseMexeu = (Vector2Length(GetMouseDelta()) > 0.0f);
    bool clicou = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool confirmou = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE);
    bool cima = IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP);
    bool baixo = IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN);

    // Nenhuma lista aberta: escolhe qual abrir
    if (m->aberta < 0) {

        if (cima || baixo)
            m->foco = (m->foco + 1) % NUM_CAMPOS_OPCOES;

        for (int c = 0; c < NUM_CAMPOS_OPCOES; c++) {

            if (!CheckCollisionPointRec(mouse, CaixaOpcoes(c)))
                continue;

            if (mouseMexeu)
                m->foco = c;

            if (clicou) {
                AbrirListaOpcoes(m, c, *cfg);
                return false;
            }
        }

        if (confirmou)
            AbrirListaOpcoes(m, m->foco, *cfg);

        return false;
    }

    // Lista aberta: escolhe um item
    Resolucao lista[MAX_RESOLUCOES];
    int numResolucoes = ListarResolucoes(cfg->modo, lista);
    int numItens = (m->aberta == CAMPO_MODO) ? NUM_MODOS_EXIBICAO : numResolucoes;
    int escolhido = -1;

    if (cima)
        m->marcado = (m->marcado + numItens - 1) % numItens;

    if (baixo)
        m->marcado = (m->marcado + 1) % numItens;

    for (int i = 0; i < numItens; i++) {

        if (!CheckCollisionPointRec(mouse, ItemListaOpcoes(m->aberta, i)))
            continue;

        if (mouseMexeu)
            m->marcado = i;

        if (clicou)
            escolhido = i;
    }

    if (confirmou)
        escolhido = m->marcado;

    // Clique fora dos itens (ou na própria caixa) só fecha a lista
    if (escolhido < 0) {

        if (clicou)
            m->aberta = -1;

        return false;
    }

    ConfigExibicao nova = *cfg;

    if (m->aberta == CAMPO_MODO) {

        nova.modo = (ModoExibicao)escolhido;

        // Trocou de modo: começa pela primeira resolução da lista nova
        // (a maior janela, ou a nativa na tela cheia)
        if (nova.modo != cfg->modo) {
            ListarResolucoes(nova.modo, lista);
            nova.resolucao = lista[0];
        }

    } else {

        nova.resolucao = lista[escolhido];
    }

    m->aberta = -1;

    if (nova.modo == cfg->modo && MesmaResolucao(nova.resolucao, cfg->resolucao))
        return false;

    *cfg = nova;
    return true;
}

// Setinha da lista suspensa: para baixo fechada, para cima aberta
static void DesenharSetaLista(Rectangle caixa, bool aberta, Color cor)
{
    float cx = caixa.x + caixa.width - 20;
    float cy = caixa.y + caixa.height / 2;

    if (aberta)
        DrawTriangle((Vector2){ cx, cy - 4 }, (Vector2){ cx - 6, cy + 3 }, (Vector2){ cx + 6, cy + 3 }, cor);
    else
        DrawTriangle((Vector2){ cx - 6, cy - 3 }, (Vector2){ cx, cy + 4 }, (Vector2){ cx + 6, cy - 3 }, cor);
}

static void DesenharOpcoes(const MenuOpcoes *m, ConfigExibicao cfg, Color fundo)
{
    Color painelCor = (Color){ 18, 15, 30, 255 };
    Color caixaCor  = (Color){ 10, 8, 22, 255 };

    ClearBackground(fundo);

    Rectangle painel = { 180, 90, 440, 400 };

    DrawRectangleRec(painel, painelCor);
    DrawRectangleLinesEx(painel, 4, PURPLE);
    DrawRectangleLinesEx((Rectangle){ painel.x + 8, painel.y + 8, painel.width - 16, painel.height - 16 }, 1, Fade(SKYBLUE, 0.35f));

    const char *titulo = "OPÇÕES";
    DrawText(titulo, LARGURA_TELA / 2 - MeasureText(titulo, 40) / 2, 115, 40, PURPLE);

    // Caixas fechadas, com o valor atual
    const char *rotulos[NUM_CAMPOS_OPCOES] = { "MODO DE EXIBIÇÃO", "RESOLUÇÃO" };
    const char *valores[NUM_CAMPOS_OPCOES] = { NOMES_MODO_EXIBICAO[cfg.modo], NomeResolucao(cfg.resolucao) };

    for (int c = 0; c < NUM_CAMPOS_OPCOES; c++) {

        Rectangle caixa = CaixaOpcoes(c);
        bool focado = (c == m->foco);

        DrawText(rotulos[c], (int)caixa.x, (int)caixa.y - 18, 12, focado ? SKYBLUE : GRAY);

        DrawRectangleRec(caixa, caixaCor);
        DrawRectangleLinesEx(caixa, 2, focado ? PURPLE : Fade(PURPLE, 0.4f));

        if (focado && m->aberta < 0)
            DesenharCoracao(caixa.x - 18, caixa.y + caixa.height / 2, 2, SKYBLUE);

        DrawText(valores[c], (int)caixa.x + 14, (int)(caixa.y + caixa.height / 2) - 10, 20, focado ? WHITE : LIGHTGRAY);
        DesenharSetaLista(caixa, m->aberta == c, focado ? SKYBLUE : GRAY);
    }

    const char *dica1 = (m->aberta < 0) ? "ENTER / CLIQUE - abrir a lista" : "ENTER / CLIQUE - escolher";
    const char *dica2 = (m->aberta < 0) ? "ESC - voltar ao menu" : "ESC - fechar a lista";

    DrawText(dica1, LARGURA_TELA / 2 - MeasureText(dica1, 10) / 2, 440, 10, Fade(SKYBLUE, 0.6f));
    DrawText(dica2, LARGURA_TELA / 2 - MeasureText(dica2, 10) / 2, 456, 10, GRAY);

    // Lista aberta por cima de tudo
    if (m->aberta < 0)
        return;

    Resolucao lista[MAX_RESOLUCOES];
    int numItens = (m->aberta == CAMPO_MODO) ? NUM_MODOS_EXIBICAO : ListarResolucoes(cfg.modo, lista);

    Rectangle area = ItemListaOpcoes(m->aberta, 0);
    area.height = numItens * (float)ALTURA_ITEM_LISTA;

    DrawRectangleRec((Rectangle){ area.x - 2, area.y - 2, area.width + 4, area.height + 4 }, caixaCor);
    DrawRectangleLinesEx((Rectangle){ area.x - 2, area.y - 2, area.width + 4, area.height + 4 }, 2, PURPLE);

    for (int i = 0; i < numItens; i++) {

        Rectangle r = ItemListaOpcoes(m->aberta, i);
        bool marcado = (i == m->marcado);
        bool atual;
        const char *nome;

        if (m->aberta == CAMPO_MODO) {
            nome = NOMES_MODO_EXIBICAO[i];
            atual = (i == (int)cfg.modo);
        } else {
            nome = NomeResolucao(lista[i]);
            atual = MesmaResolucao(lista[i], cfg.resolucao);
        }

        if (marcado)
            DrawRectangleRec(r, Fade(PURPLE, 0.35f));

        DrawText(nome, (int)r.x + 14, (int)(r.y + r.height / 2) - 8, 16, marcado ? SKYBLUE : LIGHTGRAY);

        if (atual)
            DrawText("ATUAL", (int)(r.x + r.width) - MeasureText("ATUAL", 10) - 12, (int)(r.y + r.height / 2) - 5, 10, GOLD);
    }
}

// =============================================================
// CENAS ANIMADAS DA INTRODUÇÃO
//   Uma ilustração para cada texto do narrador, desenhada por código
//   dentro do painel de imagem da intro:
//     0 - a IA na rotina (textos, imagens, decisões)
//     1 - dados incorretos e perguntas enviesadas -> alucinação
//     2 - UtopIA, o universo interior, entrando em colapso
//     3 - Luci fazendo o registro de identidade do jogador
// =============================================================

static const Color COR_FUNDO_CENA = { 8, 6, 18, 255 };
static const Color COR_PAREDE_CENA = { 18, 15, 30, 255 };
static const Color COR_MOVEL_CENA = { 45, 30, 70, 255 };
static const Color COR_CABELO = { 96, 56, 19, 255 };
static const Color COR_CABELO_CLARO = { 140, 98, 57, 255 };
static const Color COR_CAMISA = { 204, 204, 204, 255 };
static const Color COR_CORDAO = { 247, 147, 30, 255 };

// Entre 0 e 1: quanto um elemento que aparece no instante "inicio" já surgiu
static float Surgir(float t, float inicio, float duracao)
{
    float k = (t - inicio) / duracao;
    return (k < 0.0f) ? 0.0f : (k > 1.0f ? 1.0f : k);
}

// ---------- Cena 0: a IA faz parte da rotina ----------
static void CenaRotina(float x0, float y0, float t, float tempo)
{
    // Parede e mesa
    DrawRectangle((int)x0, (int)y0, 500, 255, COR_PAREDE_CENA);
    DrawRectangle((int)x0, (int)y0 + 196, 340, 59, (Color){ 26, 20, 46, 255 });
    DrawRectangle((int)x0 + 20, (int)y0 + 190, 300, 10, COR_MOVEL_CENA);

    // Notebook
    Rectangle tela = { x0 + 85, y0 + 70, 170, 112 };
    Rectangle visor = { tela.x + 7, tela.y + 7, tela.width - 14, tela.height - 14 };

    DrawRectangleRec(tela, (Color){ 30, 28, 44, 255 });
    DrawRectangleLinesEx(tela, 2, SKYBLUE);
    DrawRectangleRec(visor, (Color){ 12, 30, 60, 255 });
    DrawRectangle((int)tela.x - 14, (int)(tela.y + tela.height), (int)tela.width + 28, 8, (Color){ 60, 50, 90, 255 });

    // Conversa com a IA na tela
    float balaoUsuario = Surgir(t, 0.3f, 0.3f);
    float balaoIA = Surgir(t, 0.9f, 0.3f);

    if (balaoUsuario > 0.0f) {
        Rectangle b = { visor.x + visor.width - 82, visor.y + 8, 74, 18 };
        DrawRectangleRec(b, Fade(LIGHTGRAY, balaoUsuario));
        DrawText("me ajuda?", (int)b.x + 8, (int)b.y + 4, 10, Fade((Color){ 30, 28, 44, 255 }, balaoUsuario));
    }

    if (balaoIA > 0.0f) {

        Rectangle b = { visor.x + 8, visor.y + 34, 112, 50 };
        DrawRectangleRec(b, Fade(SKYBLUE, 0.35f * balaoIA));
        DrawRectangleLinesEx(b, 1, Fade(SKYBLUE, balaoIA));

        if (t < 2.0f) {
            // "digitando..."
            for (int i = 0; i < 3; i++) {
                float pulo = sinf(tempo * 8.0f - i * 0.8f) * 2.0f;
                DrawCircle((int)b.x + 18 + i * 12, (int)(b.y + 25 + pulo), 3, Fade(WHITE, balaoIA));
            }
        } else {
            // a resposta vai sendo escrita
            static const int larguras[3] = { 92, 78, 56 };
            for (int i = 0; i < 3; i++) {
                float k = Surgir(t, 2.0f + i * 0.35f, 0.35f);
                DrawRectangle((int)b.x + 8, (int)b.y + 9 + i * 13, (int)(larguras[i] * k), 5, Fade(WHITE, 0.8f));
            }
        }
    }

    // Thomas de costas, olhando o notebook (mesmas cores do sprite)
    DrawEllipse((int)x0 + 170, (int)y0 + 262, 60, 28, COR_CAMISA);
    DrawRectangle((int)x0 + 150, (int)y0 + 236, 40, 5, COR_CORDAO);
    DrawCircle((int)x0 + 170, (int)y0 + 214, 27, COR_CABELO);
    DrawCircle((int)x0 + 160, (int)y0 + 204, 6, COR_CABELO_CLARO);
    DrawCircle((int)x0 + 181, (int)y0 + 212, 4, COR_CABELO_CLARO);

    // Três usos da IA aparecendo um de cada vez
    const char *nomes[3] = { "TEXTOS", "IMAGENS", "DECISÕES" };
    const char *usos[3] = { "escrever", "gerar", "ajudar a decidir" };
    Color cores[3] = { SKYBLUE, GOLD, LIME };

    for (int i = 0; i < 3; i++) {

        float k = Surgir(t, 1.2f + i * 0.7f, 0.45f);

        if (k <= 0.0f)
            continue;

        float bob = sinf(tempo * 2.0f + i) * 3.0f;
        Rectangle card = { x0 + 340 + (1.0f - k) * 40.0f, y0 + 20 + i * 76 + bob, 146, 58 };

        // Dados saindo do notebook até o card
        for (int d = 0; d < 4; d++) {
            float f = fmodf(tempo * 0.8f + d * 0.25f, 1.0f);
            Vector2 a = { tela.x + tela.width, tela.y + 40 };
            Vector2 b = { card.x, card.y + card.height / 2 };
            DrawCircle((int)Lerp(a.x, b.x, f), (int)Lerp(a.y, b.y, f), 2, Fade(cores[i], 0.6f * k));
        }

        DrawRectangleRec(card, Fade((Color){ 26, 20, 46, 255 }, k));
        DrawRectangleLinesEx(card, 2, Fade(cores[i], k));

        int ix = (int)card.x + 12, iy = (int)card.y + 14;

        if (i == 0) {
            // documento sendo escrito
            DrawRectangle(ix, iy, 24, 30, Fade((Color){ 230, 230, 240, 255 }, k));
            for (int l = 0; l < 4; l++) {
                float e = fmodf(tempo * 1.2f, 4.0f) - l * 0.6f;
                e = (e < 0.0f) ? 0.0f : (e > 1.0f ? 1.0f : e);
                DrawRectangle(ix + 4, iy + 5 + l * 6, (int)(16 * e), 2, Fade(DARKGRAY, k));
            }
        } else if (i == 1) {
            // imagem com sol e montanhas
            DrawRectangle(ix - 2, iy + 2, 32, 26, Fade((Color){ 12, 30, 60, 255 }, k));
            DrawCircle(ix + 22, iy + 9, 4, Fade(GOLD, k));
            DesenharTrianguloB2((Vector2){ (float)ix + 9, (float)iy + 10 }, (Vector2){ (float)ix - 1, (float)iy + 27 }, (Vector2){ (float)ix + 19, (float)iy + 27 }, Fade(SKYBLUE, k));
            DesenharTrianguloB2((Vector2){ (float)ix + 20, (float)iy + 14 }, (Vector2){ (float)ix + 11, (float)iy + 27 }, (Vector2){ (float)ix + 29, (float)iy + 27 }, Fade(PURPLE, k));
        } else {
            // decisão: um "ok" dentro de um círculo
            DrawCircleLines(ix + 13, iy + 15, 13, Fade(LIME, k));
            DrawLineEx((Vector2){ (float)ix + 6, (float)iy + 15 }, (Vector2){ (float)ix + 11, (float)iy + 21 }, 3, Fade(LIME, k));
            DrawLineEx((Vector2){ (float)ix + 11, (float)iy + 21 }, (Vector2){ (float)ix + 21, (float)iy + 8 }, 3, Fade(LIME, k));
        }

        DrawText(nomes[i], (int)card.x + 50, (int)card.y + 14, 12, Fade(WHITE, k));
        DrawText(usos[i], (int)card.x + 50, (int)card.y + 32, 10, Fade(LIGHTGRAY, k));
    }
}

// ---------- Cena 1: dados ruins entram, alucinação sai ----------
static void CenaAlucinacao(float x0, float y0, float t, float tempo)
{
    DrawRectangle((int)x0, (int)y0, 500, 255, COR_PAREDE_CENA);

    Vector2 chip = { x0 + 250, y0 + 122 };
    float intervalo = 0.4f, viagem = 1.6f;
    float glitch = 0.0f;

    // Legenda da entrada
    DrawText("ENTRADA", (int)x0 + 14, (int)y0 + 12, 10, GRAY);
    DrawRectangle((int)x0 + 14, (int)y0 + 30, 10, 8, SKYBLUE);
    DrawText("dado correto", (int)x0 + 30, (int)y0 + 29, 10, LIGHTGRAY);
    DrawRectangle((int)x0 + 14, (int)y0 + 46, 10, 8, RED);
    DrawText("dado incorreto", (int)x0 + 30, (int)y0 + 45, 10, LIGHTGRAY);
    DrawRectangle((int)x0 + 14, (int)y0 + 62, 10, 8, ORANGE);
    DrawText("pergunta enviesada", (int)x0 + 30, (int)y0 + 61, 10, LIGHTGRAY);

    // Blocos de dados viajando até o chip
    int ultimo = (int)(t / intervalo);

    for (int k = ultimo - 6; k <= ultimo; k++) {

        if (k < 0) continue;

        float idade = t - k * intervalo;
        int tipo = (k % 4 == 2) ? 1 : (k % 4 == 0 ? 2 : 0);     // 0 certo, 1 errado, 2 pergunta enviesada

        if (idade < viagem) {

            float f = idade / viagem;
            float x = Lerp(x0 + 20, chip.x - 60, f);
            float y = chip.y + 50 * (1.0f - f) * sinf(k * 1.7f) + 8 * sinf(tempo * 3 + k);
            Color cor = (tipo == 0) ? SKYBLUE : (tipo == 1 ? RED : ORANGE);
            const char *g = (tipo == 0) ? "01" : (tipo == 1 ? "!?" : "?");

            DrawRectangle((int)x - 11, (int)y - 8, 22, 16, cor);
            DrawText(g, (int)x - MeasureText(g, 10) / 2, (int)y - 5, 10, (Color){ 20, 16, 34, 255 });

        } else if (tipo != 0 && idade < viagem + 0.5f) {

            // dado ruim acabou de entrar: o chip falha
            glitch += (viagem + 0.5f - idade) / 0.5f;
        }
    }

    if (glitch > 1.0f) glitch = 1.0f;

    // Chip da IA (com aberração cromática quando falha)
    Vector2 c = chip;
    Rectangle corpo = { c.x - 42, c.y - 42, 84, 84 };

    for (int i = 0; i < 5; i++) {
        DrawRectangle((int)corpo.x - 8, (int)corpo.y + 10 + i * 15, 8, 4, GRAY);
        DrawRectangle((int)(corpo.x + corpo.width), (int)corpo.y + 10 + i * 15, 8, 4, GRAY);
        DrawRectangle((int)corpo.x + 10 + i * 15, (int)corpo.y - 8, 4, 8, GRAY);
        DrawRectangle((int)corpo.x + 10 + i * 15, (int)(corpo.y + corpo.height), 4, 8, GRAY);
    }

    if (glitch > 0.05f) {
        DrawRectangleLinesEx((Rectangle){ corpo.x - 4 * glitch, corpo.y, corpo.width, corpo.height }, 3, Fade(RED, 0.6f));
        DrawRectangleLinesEx((Rectangle){ corpo.x + 4 * glitch, corpo.y, corpo.width, corpo.height }, 3, Fade(SKYBLUE, 0.6f));
    }

    DrawRectangleRec(corpo, (Color){ 30, 28, 44, 255 });
    DrawRectangleLinesEx(corpo, 3, (glitch > 0.3f) ? MAGENTA : SKYBLUE);
    DrawText("IA", (int)c.x - MeasureText("IA", 12) / 2, (int)corpo.y + 8, 12, LIGHTGRAY);

    Color olho = (glitch > 0.3f) ? MAGENTA : SKYBLUE;
    DrawCircle((int)c.x, (int)c.y + 6, 18, Fade(olho, 0.3f));
    DrawCircle((int)c.x, (int)c.y + 6, 10, olho);
    DrawCircle((int)(c.x + sinf(tempo * 2.0f) * 4), (int)c.y + 6, 4, (Color){ 20, 16, 34, 255 });

    // Resposta alucinada, dita com toda a confiança
    static const char *respostas[3] = { "Com certeza!\n2 + 2 = 5.", "Fato: a Lua\ntem luz própria.", "O Brasil tem\n50 estados!" };
    int r = ((int)(tempo / 2.4f)) % 3;
    float k = Surgir(t, 1.8f, 0.4f);

    if (k > 0.0f) {

        Rectangle balao = { chip.x + 68, chip.y - 62, 160, 64 };
        float dx = 0.0f;

        DrawLineEx((Vector2){ chip.x + 46, chip.y - 10 }, (Vector2){ balao.x, balao.y + 44 }, 2, Fade(LIGHTGRAY, k));
        DrawRectangleRec(balao, Fade((Color){ 245, 245, 245, 255 }, k));

        if (glitch > 0.3f) {
            DesenharTextoLinhas(respostas[r], (int)(balao.x + 12 + dx - 2), (int)balao.y + 14, 14, 18, Fade(RED, 0.5f));
            DesenharTextoLinhas(respostas[r], (int)(balao.x + 12 + dx + 2), (int)balao.y + 14, 14, 18, Fade(SKYBLUE, 0.5f));
        }

        DesenharTextoLinhas(respostas[r], (int)(balao.x + 12 + dx), (int)balao.y + 14, 14, 18, Fade((Color){ 30, 28, 44, 255 }, k));

        // Aviso piscando
        if (((int)(tempo * 3.0f)) % 2 == 0) {
            int ax = (int)balao.x + 20, ay = (int)(balao.y + balao.height) + 18;
            DesenharTrianguloB2((Vector2){ (float)ax, (float)ay - 9 }, (Vector2){ (float)ax - 9, (float)ay + 7 }, (Vector2){ (float)ax + 9, (float)ay + 7 }, RED);
            DrawText("!", ax - 2, ay - 5, 10, WHITE);
            DrawText("ALUCINAÇÃO", ax + 16, ay - 5, 14, RED);
        }
    }

    DrawText("RESPOSTA", (int)x0 + 412, (int)y0 + 12, 10, GRAY);
}

// ---------- Cena 2: UtopIA entrando em colapso ----------
static void CenaUtopia(float x0, float y0, float t, float tempo)
{
    float horizonte = y0 + 150;
    float vx = x0 + 250;

    DrawRectangle((int)x0, (int)y0, 500, 255, COR_FUNDO_CENA);

    // Sol/núcleo roxo com faixas
    DrawCircle((int)vx, (int)horizonte - 30, 66, Fade(PURPLE, 0.25f));
    DrawCircle((int)vx, (int)horizonte - 30, 54, Fade(PURPLE, 0.55f));
    for (int i = 0; i < 5; i++)
        DrawRectangle((int)vx - 70, (int)horizonte - 26 + i * 8, 140, 2 + i, COR_FUNDO_CENA);

    // Cidade de dados no horizonte
    for (int i = 0; i < 14; i++) {

        unsigned int r = Ruido((unsigned int)i * 7u + 3u);
        int largura = 18 + (int)(r % 14);
        int altura = 24 + (int)((r >> 8) % 60);
        int x = (int)x0 + 8 + i * 35;
        bool falhando = (Ruido((unsigned int)(tempo * 6.0f) + (unsigned int)i * 13u) % 9) == 0;

        DrawRectangle(x, (int)horizonte - altura, largura, altura, (Color){ 26, 20, 46, 255 });
        DrawRectangleLines(x, (int)horizonte - altura, largura, altura, Fade(PURPLE, 0.5f));

        for (int j = 0; j < altura / 10; j++) {
            bool aceso = (Ruido((unsigned int)(i * 31 + j * 7) + (unsigned int)(tempo * 2.0f)) % 3) == 0;
            if (aceso) DrawRectangle(x + 4, (int)horizonte - altura + 4 + j * 10, largura - 8, 3, falhando ? MAGENTA : Fade(SKYBLUE, 0.8f));
        }
    }

    // Chão em grade, andando em direção a quem vê
    for (int i = 0; i < 10; i++) {
        float f = (i + fmodf(tempo * 0.8f, 1.0f)) / 10.0f;
        float y = horizonte + f * f * 105.0f;
        DrawLine((int)x0, (int)y, (int)x0 + 500, (int)y, Fade(PURPLE, 0.15f + 0.5f * f));
    }
    for (int i = -8; i <= 8; i++)
        DrawLine((int)vx, (int)horizonte, (int)(vx + i * 70), (int)y0 + 255, Fade(PURPLE, 0.3f));

    // Rachaduras se espalhando a partir do centro
    Vector2 origem = { vx, horizonte + 50 };
    float alcance = Surgir(t, 0.5f, 3.5f);

    for (int c = 0; c < 6; c++) {

        float ang = -PI * 0.15f + c * (PI * 1.3f / 5.0f);
        Vector2 a = origem;
        int segmentos = (int)(8 * alcance);

        for (int s = 0; s < segmentos; s++) {
            float desvio = ((int)(Ruido((unsigned int)(c * 17 + s)) % 41) - 20) * 0.03f;
            Vector2 b = { a.x + cosf(ang + desvio) * 16, a.y + sinf(ang + desvio) * 9 - (c < 3 ? 6 : -4) };
            DrawLineEx(a, b, 2, Fade(MAGENTA, 0.6f + 0.4f * sinf(tempo * 8 + s)));
            a = b;
        }
    }

    // Fragmentos se soltando e subindo
    for (int i = 0; i < 12; i++) {
        float f = fmodf(tempo * 0.35f + i * 0.083f, 1.0f);
        float x = x0 + 30 + (Ruido((unsigned int)i * 11u) % 440);
        float y = horizonte + 40 - f * 170;
        int lado = 4 + i % 4;
        DrawRectangle((int)x, (int)y, lado, lado, Fade((i % 2) ? MAGENTA : SKYBLUE, (1.0f - f) * Surgir(t, 0.8f, 1.0f)));
    }

    // Faixa de glitch de tempos em tempos
    if (fmodf(tempo, 2.2f) < 0.15f) {
        int fy = (int)y0 + 40 + (int)(Ruido((unsigned int)(tempo * 10.0f)) % 170);
        DrawRectangle((int)x0, fy, 500, 6, Fade(MAGENTA, 0.35f));
        DrawRectangle((int)x0 + 20, fy + 8, 460, 2, Fade(SKYBLUE, 0.35f));
    }

    // Nome do mundo e integridade caindo
    const char *nome = "UTOPIA";
    int nx = (int)vx - MeasureText(nome, 24) / 2;
    float g = (fmodf(tempo, 1.7f) < 0.12f) ? 3.0f : 0.0f;

    DrawText(nome, nx - (int)g, (int)y0 + 14, 24, Fade(RED, 0.6f));
    DrawText(nome, nx + (int)g, (int)y0 + 14, 24, Fade(SKYBLUE, 0.6f));
    DrawText(nome, nx, (int)y0 + 14, 24, WHITE);

    int integridade = 87 - (int)(t * 6.0f);
    if (integridade < 12) integridade = 12;

    const char *aviso = TextFormat("INTEGRIDADE DO SISTEMA: %d%%", integridade);
    DrawText(aviso, (int)vx - MeasureText(aviso, 10) / 2, (int)y0 + 42, 10,
             (((int)(tempo * 2.0f)) % 2 == 0 || integridade > 30) ? RED : Fade(RED, 0.4f));
}

// ---------- Cena 3: Luci registra o jogador ----------
static void CenaRegistro(float x0, float y0, float t, float tempo)
{
    DrawRectangle((int)x0, (int)y0, 500, 255, COR_FUNDO_CENA);

    // Pontinhos de grade ao fundo
    for (int y = 12; y < 255; y += 20)
        for (int x = 10; x < 500; x += 20)
            DrawPixel((int)x0 + x, (int)y0 + y, Fade(PURPLE, 0.35f));

    Vector2 p = { x0 + 250, y0 + 118 + sinf(tempo * 2.0f) * 6.0f };

    // Ondas de leitura saindo da Luci
    for (int i = 0; i < 3; i++) {
        float f = fmodf(tempo * 0.6f + i * 0.33f, 1.0f);
        DrawCircleLines((int)p.x, (int)p.y, 50 + f * 110, Fade(SKYBLUE, (1.0f - f) * 0.6f));
    }

    DesenharLuciTamanho(p, 110.0f * Surgir(t, 0.0f, 0.5f) + 1.0f);

    // Linha de leitura varrendo o painel
    float varre = fmodf(tempo * 90.0f, 255.0f);
    DrawRectangle((int)x0 + 1, (int)(y0 + varre), 498, 2, Fade(SKYBLUE, 0.35f));

    // Cantos de visor
    int m = 14, l = 22;
    Color cc = Fade(SKYBLUE, 0.8f);
    DrawRectangle((int)x0 + m, (int)y0 + m, l, 2, cc);           DrawRectangle((int)x0 + m, (int)y0 + m, 2, l, cc);
    DrawRectangle((int)x0 + 500 - m - l, (int)y0 + m, l, 2, cc); DrawRectangle((int)x0 + 500 - m - 2, (int)y0 + m, 2, l, cc);
    DrawRectangle((int)x0 + m, (int)y0 + 255 - m - 2, l, 2, cc); DrawRectangle((int)x0 + m, (int)y0 + 255 - m - l, 2, l, cc);
    DrawRectangle((int)x0 + 500 - m - l, (int)y0 + 255 - m - 2, l, 2, cc); DrawRectangle((int)x0 + 500 - m - 2, (int)y0 + 255 - m - l, 2, l, cc);

    DrawText("LUCI", (int)x0 + 26, (int)y0 + 22, 12, SKYBLUE);

    const char *msg = "IDENTIFICANDO USUÁRIO...";
    if (((int)(tempo * 2.0f)) % 2 == 0)
        DrawText(msg, (int)x0 + 250 - MeasureText(msg, 10) / 2, (int)y0 + 232, 10, LIGHTGRAY);
}

// Desenha a cena da etapa atual dentro do painel de imagem da intro
static void DesenharCenaIntro(int etapa, Rectangle painel, float tempo)
{
    static int ultimaEtapa = -1;
    static float inicioEtapa = 0.0f;

    // Trocou de etapa: a nova cena começa do zero
    if (etapa != ultimaEtapa) {
        ultimaEtapa = etapa;
        inicioEtapa = tempo;
    }

    float t = tempo - inicioEtapa;

    BeginScissorMode((int)painel.x + 1, (int)painel.y + 1, (int)painel.width - 2, (int)painel.height - 2);

    switch (etapa) {
        case 0:  CenaRotina(painel.x, painel.y, t, tempo);     break;
        case 1:  CenaAlucinacao(painel.x, painel.y, t, tempo); break;
        case 2:  CenaUtopia(painel.x, painel.y, t, tempo);     break;
        default: CenaRegistro(painel.x, painel.y, t, tempo);   break;
    }

    EndScissorMode();

    // Entrada suave da cena
    float alfa = 1.0f - t * 2.5f;

    if (alfa > 0.0f)
        DrawRectangleRec(painel, Fade(COR_FUNDO_CENA, alfa));

    DrawRectangleLinesEx(painel, 1, Fade(LIGHTGRAY, 0.65f));
}

// =============================================================
// MAIN
// =============================================================

int main(void) {

    // 1. INICIALIZAÇÃO

    InitWindow(LARGURA_TELA, ALTURA_TELA, "UtopIA - Game");
    IniciarSons();
    PrepararSaberMaisMonitores();
    Telemetria("sessao_inicio", "", 0.0f, 0);

    // ESC não fecha mais a janela: ele abre o menu de pause
    // (fechar pelo X da janela continua funcionando)
    SetExitKey(KEY_NULL);

    // Tudo é desenhado aqui em 800x600 e depois esticado para a janela
    RenderTexture2D alvo = LoadRenderTexture(LARGURA_TELA, ALTURA_TELA);
    FiltroNitidez filtroNitidez = CarregarFiltroNitidez();

    // Modo de exibição e resolução escolhidos da última vez (OPÇÕES)
    GuardarMonitor();

    ConfigExibicao exibicao = LerConfigExibicao();
    AplicarExibicao(exibicao);

    MenuOpcoes menuOpcoes = { .foco = CAMPO_MODO, .aberta = -1 };

    TelaAtual tela = MENU;

    Color cor_de_fundo_menu = {2, 0, 12, 255};

    // caminhos das imagens (sprites) do jogo e carregamento delas
    Texture2D imagem_titulo = LoadTexture("resources/ui/titulo.png");
    CarregarLuci();

    // Personagem: uma folha de animação por direção (na ordem do enum Direcao)
    SpriteDirecao spritesJogador[NUM_DIRECOES];

    spritesJogador[DIR_FRENTE]   = CarregarSprite(SPRITE_ANDANDO_FRENTE,   SPRITE_FRENTE, false);
    spritesJogador[DIR_COSTAS]   = CarregarSprite(SPRITE_ANDANDO_COSTAS,   SPRITE_COSTAS, false);
    spritesJogador[DIR_DIREITA]  = CarregarSprite(SPRITE_ANDANDO_DIREITA,  SPRITE_LADO,   false);
    spritesJogador[DIR_ESQUERDA] = CarregarSprite(SPRITE_ANDANDO_ESQUERDA, SPRITE_LADO,   true);

    // Diagonais: sem folha própria, usam a de lado do mesmo sentido
    spritesJogador[DIR_FRENTE_DIREITA]  = CarregarSpriteDiagonal(SPRITE_ANDANDO_FRENTE_DIREITA,  spritesJogador[DIR_DIREITA]);
    spritesJogador[DIR_FRENTE_ESQUERDA] = CarregarSpriteDiagonal(SPRITE_ANDANDO_FRENTE_ESQUERDA, spritesJogador[DIR_ESQUERDA]);
    spritesJogador[DIR_COSTAS_DIREITA]  = CarregarSpriteDiagonal(SPRITE_ANDANDO_COSTAS_DIREITA,  spritesJogador[DIR_DIREITA]);
    spritesJogador[DIR_COSTAS_ESQUERDA] = CarregarSpriteDiagonal(SPRITE_ANDANDO_COSTAS_ESQUERDA, spritesJogador[DIR_ESQUERDA]);

    AnimacaoJogador animJogador = {0};

    // Sprites dos monitores (só carrega os que existirem)
    const char *arquivosMonitor[4] = {
        SPRITE_MONITOR_NEUTRO,
        SPRITE_MONITOR_ATIVO,
        SPRITE_MONITOR_ACERTO,
        SPRITE_MONITOR_ERRO
    };

    Texture2D spritesMonitor[4] = {0};

    for (int i = 0; i < 4; i++)
        if (FileExists(arquivosMonitor[i]))
            spritesMonitor[i] = LoadTexture(arquivosMonitor[i]);

    // Mapa da sala inicial (opcional: sem o arquivo, usa o cenário simples)
    Texture2D texMapaSala = FileExists(ARQUIVO_MAPA_SALA) ? LoadTexture(ARQUIVO_MAPA_SALA) : (Texture2D){0};

    Vector2 playerPos = SPAWN_SALA;
    Direcao direcaoAtual = DIR_FRENTE;

    // distância/tamanho da área de interação (o "alcance" do jogador pra apertar E)
    float interacaoAlcance = 50.0f; // AJUSTE AQUI se quiser o alcance maior/menor
    bool pertoDaLuci = false;

    // --- NPC: LUCI ---
    Rectangle luciHitbox = { MAPA_X(512), MAPA_Y(640), TAM_SPRITE, TAM_SPRITE };
    bool luciAcompanhando = false;   // depois da conversa ela vira companion
    float companionAngulo = 0.0f;

    float tempo = 0.0f;

    bool fecharJogo = false;

    // =========================================================
    // VARIÁVEIS DA TELA INTRO
    // =========================================================

    int etapaIntro = 0;

    // Nickname do jogador: exatamente 3 letras, sem repetir um nome do ranking
    char nickname[4] = {'_', '_', '_', '\0'};
    int nicknamePos = 0;
    bool cliqueConfirmarNome = false;        // clicou em CONTINUAR na tela do nickname
    float avisoNomeRepetido = 0.0f;          // tempo do aviso "esse nome já está no ranking"

    const char *textosIntro[3] = {
        "A Inteligência Artificial virou parte da rotina:\nela escreve textos, gera imagens e ajuda a decidir.",
        "Mas com dados errados ou perguntas enviesadas,\nela pode ALUCINAR: responder com confiança algo falso.",
        "Bem-vindo a UtopIA, o mundo dentro da IA.\nAs alucinações estão fazendo tudo desmoronar...",
    };

    // =========================================================
    // VARIÁVEIS DO CORREDOR E DA BATALHA
    // =========================================================

    ProgressoCorredor progresso = {0};
    BatalhaB2 batalhaB2 = {0};
    TelaAtual salaDaBatalhaB2 = CORREDOR;    // para onde volta depois da batalha
    MotivoBatalhaB2 motivoBatalhaB2 = MOTIVO_MONITOR;
    CartaoResumo cartao = {0};               // cartão de resumo depois de cada desafio
    float tempoParado = 0.0f;                // há quanto tempo o jogador não anda (Luci reage)
    int estabilidadeLuci = ESTABILIDADE_MAX_LUCI;   // gasta ao pedir ajuda (H) nas batalhas
    int ajudasPedidas = 0;
    bool chefeVencido = false;               // venceu a ALUCINAÇÃO MESTRE (vai para o save)
    bool chefeAnunciado = false;             // a Luci já chamou o chefe nesta sessão (depois, só pela porta)
    static Biblioteca biblioteca;            // Bloco 2: Biblioteca dos Ecos (static: é grande para a pilha)
    ProgressoBiblioteca progBiblioteca = {0};   // o que não se perde na Biblioteca (vai para o save)
    int idPartida = NovoIdPartida();         // esta partida no ranking (vai para o save)
    SessaoDialogo dlg = {0};
    Transicao transicao = {0};
    Batalha batalha = {0};

    int quizDaBatalha = -1;                  // monitor que gerou a batalha
    ResultadoDialogo motivoBatalha = RES_NADA;

    // =========================================================
    // VARIÁVEIS DO SAVE
    // =========================================================

    DadosSave saveLido;
    bool existeSave = LerSave(&saveLido);    // mostra o botão CONTINUAR
    bool pedirAutoSave = false;              // marcado nos momentos importantes; salva no fim do frame
    float avisoSave = 0.0f;                  // tempo restante do aviso "JOGO SALVO"
    bool saveFalhou = false;
    float confirmarNovoJogo = 0.0f;          // > 0 = clicou em JOGAR com save existente, falta confirmar

    // =========================================================
    // VARIÁVEIS DO RANKING
    // =========================================================

    Ranking ranking;
    LerRanking(&ranking);                    // aparece no menu
    TelaRanking telaRanking = {0};           // ranking por cima do jogo (fim do Bloco 1 / NÚCLEO)

    // =========================================================
    // VARIÁVEIS DO PAUSE
    // =========================================================

    bool pausado = false;
    bool capturarPausa = false;              // apertou ESC: tira a "foto" do fundo no fim deste frame
    int selecaoPausa = PAUSA_CONTINUAR;
    Texture2D fundoPausa = {0};


    // =========================================================
    // ÁREAS DE CLIQUE DOS BOTÕES DO MENU
    // =========================================================

    Rectangle btnContinuar = {300, 270, 200, 50};   // só aparece se existir save
    Rectangle btnJogar = {300, 270, 200, 50};
    Rectangle btnOpcoes = {300, 340, 200, 50};
    Rectangle btnSair = {300, 410, 200, 50};

    // O que o fim de uma batalha do Bloco 1 mexe
    ContextoB2 ctxB2 = {
        &dlg, &transicao, &progresso, &batalhaB2, &pedirAutoSave, &salaDaBatalhaB2, &motivoBatalhaB2,
        &cartao, &chefeVencido
    };

    SetTargetFPS(60);

    // 2. GAME LOOP

    while (!WindowShouldClose() && !fecharJogo) {

        float dt = GetFrameTime();

        AjustarMouseAJanela();

        // Na Biblioteca a mira desenhada substitui o cursor do sistema
        bool esconderCursor = (tela == BIBLIOTECA && !pausado && biblioteca.tipoSala != SALA_ATRIO && !biblioteca.lojaAberta && !biblioteca.desafioAberto);
        if (esconderCursor != IsCursorHidden()) {
            if (esconderCursor) HideCursor();
            else                ShowCursor();
        }

        // =====================================================
        // ESC: ABRE / FECHA O PAUSE
        // =====================================================

        if (IsKeyPressed(KEY_ESCAPE)) {

            if (pausado) {

                // Volta para o jogo
                pausado = false;
                UnloadTexture(fundoPausa);
                fundoPausa = (Texture2D){0};

            } else if ((tela == JOGANDO || tela == CORREDOR || tela == BATALHA || tela == BATALHA_B2 ||
                        (tela == BIBLIOTECA && !biblioteca.lojaAberta && !biblioteca.enciclopediaAberta)) && !transicao.ativa) {

                // Só pausa durante o jogo (no menu e na intro o ESC não faz nada)
                capturarPausa = true;
                selecaoPausa = PAUSA_CONTINUAR;

            } else if (tela == OPCOES) {

                // Com uma lista suspensa aberta, o ESC só fecha a lista
                if (menuOpcoes.aberta >= 0)
                    menuOpcoes.aberta = -1;
                else
                    tela = MENU;
            }
        }

        // =====================================================
        // ANIMAÇÃO DO TÍTULO
        // =====================================================

        if (!pausado)
            tempo += dt;

        float amplitude = 8.0f;
        float velocidade = 3.0f;

        float y =
            100.0f +
            sinf(tempo * velocidade) * amplitude +
            sinf(tempo * 2.1f) * 4.0f;

        float y1 =
            100.0f +
            sinf((tempo - 0.25f) * velocidade) * amplitude +
            sinf((tempo - 0.25f) * 2.1f) * 4.0f;

        float y2 =
            100.0f +
            sinf((tempo - 0.45f) * velocidade) * amplitude +
            sinf((tempo - 0.45f) * 2.1f) * 4.0f;

        // =====================================================
        // TRANSIÇÃO ENTRE TELAS (FADE)
        // =====================================================

        if (transicao.ativa && !pausado) {

            float passo = dt / transicao.duracao;

            if (transicao.fase == 0) {

                // Escurecendo
                transicao.alfa += passo;

                if (transicao.alfa >= 1.0f) {

                    transicao.alfa = 1.0f;

                    // Tela toda preta: troca de tela e posiciona o jogador
                    TelaAtual origem = tela;
                    tela = transicao.destino;

                    if (tela == CORREDOR && origem == JOGANDO) {

                        playerPos = SPAWN_CORREDOR;
                        direcaoAtual = DIR_COSTAS;
                        pedirAutoSave = true;     // AUTO SAVE: entrou no corredor

                    } else if (tela == JOGANDO && origem == CORREDOR) {

                        playerPos = SPAWN_SALA_VOLTA;
                        direcaoAtual = DIR_FRENTE;
                        pedirAutoSave = true;     // AUTO SAVE: voltou para a sala

                    } else if (tela == BIBLIOTECA && origem == CORREDOR) {

                        // Biblioteca dos Ecos: começa no átrio
                        RogueIniciar(&biblioteca, &progBiblioteca);
                        playerPos = SPAWN_CORREDOR_VOLTA;
                        direcaoAtual = DIR_ESQUERDA;
                        pedirAutoSave = true;     // AUTO SAVE: entrou na Biblioteca dos Ecos

                    } else if (tela == CORREDOR && origem == BIBLIOTECA) {

                        playerPos = SPAWN_CORREDOR_VOLTA;
                        direcaoAtual = DIR_ESQUERDA;
                        pedirAutoSave = true;     // AUTO SAVE: voltou para o corredor
                    }

                    transicao.fase = 1;
                }

            } else {

                // Clareando
                transicao.alfa -= passo;

                if (transicao.alfa <= 0.0f) {

                    transicao.alfa = 0.0f;
                    transicao.ativa = false;
                }
            }
        }

        // =====================================================
        // LÓGICA
        // =====================================================

        // Guarda a posição para saber se o personagem andou neste frame
        Vector2 posInicioFrame = playerPos;

        Vector2 mouse = GetMousePosition();

        Color corBtnJogar = DARKGRAY;
        Color corBtnOpcoes = DARKGRAY;
        Color corBtnSair = DARKGRAY;
        Color corBtnContinuar = DARKGRAY;

        // Com save, o CONTINUAR entra no topo e os outros botões descem
        float yBotoes = existeSave ? 340.0f : 270.0f;
        btnJogar.y  = yBotoes;
        btnOpcoes.y = yBotoes + 70;
        btnSair.y   = yBotoes + 140;

        if (confirmarNovoJogo > 0.0f)
            confirmarNovoJogo -= dt;

        // =====================================================
        // MENU DE PAUSE (enquanto pausado, o resto do jogo fica congelado:
        // personagem, diálogos, barra de tempo do quiz e batalha)
        // =====================================================

        if (pausado) {

            int escolha = AtualizarPausa(&selecaoPausa);

            if (escolha == PAUSA_CONTINUAR) {

                pausado = false;
                UnloadTexture(fundoPausa);
                fundoPausa = (Texture2D){0};

            } else if (escolha == PAUSA_MENU) {

                // Salva antes de sair, para o CONTINUAR do menu voltar daqui
                saveFalhou = !SalvarJogo(nickname, (tela == BATALHA_B2) ? salaDaBatalhaB2 : tela, playerPos, direcaoAtual, luciAcompanhando, &progresso, estabilidadeLuci, ajudasPedidas, chefeVencido, idPartida, &progBiblioteca);
                existeSave = LerSave(&saveLido);

                pausado = false;
                UnloadTexture(fundoPausa);
                fundoPausa = (Texture2D){0};

                tela = MENU;
                dlg = (SessaoDialogo){0};
                transicao = (Transicao){0};
                telaRanking = (TelaRanking){0};
                pedirAutoSave = false;
                avisoSave = 0.0f;
                confirmarNovoJogo = 0.0f;

            } else if (escolha == PAUSA_SAIR) {

                // O auto save do final do programa guarda o progresso
                fecharJogo = true;
            }
        }

        // =====================================================
        // RANKING ABERTO (o jogo espera o jogador fechar)
        // =====================================================

        else if (telaRanking.aberto) {

            // Fechou o ranking do fim do Bloco 1: a Luci avisa que a saída abriu
            if (AtualizarTelaRanking(&telaRanking) && tela == CORREDOR && chefeVencido && !progresso.fimAnunciado) {

                progresso.fimAnunciado = true;
                pedirAutoSave = true;
                DialogoIniciar(&dlg, &dialogoFimCorredor, OBJ_NENHUM);
            }
        }

        // =====================================================
        // MENU
        // =====================================================

        else if (tela == MENU) {

            // CONTINUAR (carrega o save)
            if (existeSave && CheckCollisionPointRec(mouse, btnContinuar)) {

                corBtnContinuar = PURPLE;

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {

                    if (LerSave(&saveLido)) {

                        memcpy(nickname, saveLido.nickname, 4);
                        nicknamePos = 3;
                        luciAcompanhando = saveLido.luciAcompanhando;
                        progresso = saveLido.progresso;
                        progBiblioteca = saveLido.biblioteca;
                        playerPos = (Vector2){ saveLido.x, saveLido.y };
                        direcaoAtual = (Direcao)saveLido.direcao;
                        dlg = (SessaoDialogo){0};
                        cartao = (CartaoResumo){0};
                        estabilidadeLuci = saveLido.estabilidadeLuci;
                        ajudasPedidas = saveLido.ajudasPedidas;
                        chefeVencido = saveLido.chefeVencido;
                        chefeAnunciado = (ContarConcluidos(&progresso) == TOTAL_MONITORES);   // perdeu antes: volta pela porta
                        idPartida = (saveLido.idPartida != 0) ? saveLido.idPartida : NovoIdPartida();
                        telaRanking = (TelaRanking){0};
                        animJogador = (AnimacaoJogador){0};
                        tela = (TelaAtual)saveLido.sala;

                        if (tela == BIBLIOTECA) {
                            RogueIniciar(&biblioteca, &progBiblioteca);
                            playerPos = SPAWN_CORREDOR_VOLTA;      // se sair, volta pela porta da direita do corredor
                        }

                        // Entra clareando a tela
                        transicao = (Transicao){ .ativa = true, .alfa = 1.0f, .fase = 1, .duracao = DURACAO_FADE, .destino = tela };

                    } else {

                        // Save sumiu ou corrompeu desde que o menu abriu
                        existeSave = false;
                    }
                }
            }

            // JOGAR
            if (CheckCollisionPointRec(mouse, btnJogar)) {

                corBtnJogar = GREEN;

                // Com save existente, pede um segundo clique para confirmar
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && existeSave && confirmarNovoJogo <= 0.0f) {

                    confirmarNovoJogo = 3.0f;

                } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {

                    confirmarNovoJogo = 0.0f;
                    tela = INTRO;
                    etapaIntro = 0;

                    // Novo jogo: zera tudo
                    progresso = (ProgressoCorredor){0};
                    progBiblioteca = (ProgressoBiblioteca){0};
                    cartao = (CartaoResumo){0};
                    estabilidadeLuci = ESTABILIDADE_MAX_LUCI;
                    ajudasPedidas = 0;
                    chefeVencido = false;
                    chefeAnunciado = false;
                    idPartida = NovoIdPartida();
                    telaRanking = (TelaRanking){0};
                    dlg = (SessaoDialogo){0};
                    luciAcompanhando = false;
                    playerPos = SPAWN_SALA;
                    direcaoAtual = DIR_FRENTE;
                    animJogador = (AnimacaoJogador){0};

                    nickname[0] = nickname[1] = nickname[2] = '_';
                    nickname[3] = '\0';
                    nicknamePos = 0;
                }
            }

            // OPÇÕES
            if (CheckCollisionPointRec(mouse, btnOpcoes)) {

                corBtnOpcoes = LIGHTGRAY;

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    tela = OPCOES;
                    menuOpcoes = (MenuOpcoes){ .foco = CAMPO_MODO, .aberta = -1 };
                }
            }

            // SAIR
            if (CheckCollisionPointRec(mouse, btnSair)) {

                corBtnSair = RED;

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                    fecharJogo = true;
            }
        }

        // =====================================================
        // INTRO
        // =====================================================

        else if (tela == INTRO) {

            // =================================================
            // ETAPA FINAL: REGISTRO DO NICKNAME
            // =================================================

            if (etapaIntro == 3) {

                // Lê uma letra por vez (A-Z)
                int tecla = GetKeyPressed();

                if (tecla >= KEY_A && tecla <= KEY_Z && nicknamePos < 3) {

                    nickname[nicknamePos] = (char)('A' + (tecla - KEY_A));
                    nicknamePos++;
                }

                // BACKSPACE apaga a última letra digitada.
                // Se não houver nenhuma letra, volta para o menu.
                if (IsKeyPressed(KEY_BACKSPACE)) {

                    if (nicknamePos > 0) {

                        nicknamePos--;
                        nickname[nicknamePos] = '_';

                    } else {

                        etapaIntro = 0;
                        tela = MENU;
                    }
                }

                // ENTER (ou o botão CONTINUAR) só avança com as 3 letras preenchidas
                // e com um nome que ainda não está no ranking.
                if ((IsKeyPressed(KEY_ENTER) || cliqueConfirmarNome) && nicknamePos == 3) {

                    if (NicknameDisponivel(&ranking, nickname)) {

                        etapaIntro = 0;
                        tela = JOGANDO;
                        avisoNomeRepetido = 0.0f;

                    } else {

                        avisoNomeRepetido = 3.0f;
                        nickname[0] = nickname[1] = nickname[2] = '_';
                        nicknamePos = 0;
                        TocarSom(sfxErro);
                    }
                }

                cliqueConfirmarNome = false;

                if (avisoNomeRepetido > 0.0f)
                    avisoNomeRepetido -= dt;
            }

            // =================================================
            // ETAPAS NORMAIS DA INTRODUÇÃO
            // =================================================

            else {

                // ENTER avança a introdução
                if (IsKeyPressed(KEY_ENTER)) {

                    etapaIntro++;
                }

                // BACKSPACE volta para o menu
                if (IsKeyPressed(KEY_BACKSPACE)) {

                    etapaIntro = 0;
                    tela = MENU;
                }
            }
        }

        // =====================================================
        // JOGANDO (SALA INICIAL)
        // =====================================================

        else if (tela == JOGANDO && !transicao.ativa) {

            companionAngulo += dt * 2.5f;
            pertoDaLuci = false;

            // -------------------------------------------------
            // CONVERSA EM ANDAMENTO (o personagem fica parado)
            // -------------------------------------------------

            if (dlg.estado != DLG_NENHUM) {

                ResultadoDialogo r = DialogoAtualizar(&dlg);

                // Terminou a conversa com a Luci: ela passa a acompanhar o jogador
                if (r == RES_FIM && dlg.objeto == OBJ_LUCI) {
                    luciAcompanhando = true;
                    pedirAutoSave = true;         // AUTO SAVE: missão começou
                }
            }

            else {

                // -------------------------------------------------
                // MOVIMENTAÇÃO DO PERSONAGEM
                // -------------------------------------------------

                Vector2 posAnterior = playerPos;

                direcaoAtual = MoverJogador(&playerPos, direcaoAtual);

                // Colisão com a Luci (não deixa atravessar)
                if (!luciAcompanhando && CheckCollisionRecs(HitboxJogador(playerPos), CorpoLuciSala(luciHitbox)))
                    playerPos = posAnterior;

                // -------------------------------------------------
                // LIMITES DO MAPA
                // -------------------------------------------------

                // Paredes invisíveis (só valem com a imagem do mapa)
#if PAREDES_INVISIVEIS_ATIVAS
                if (texMapaSala.id > 0)
                    ColidirComParedes(&playerPos, posAnterior);
#endif

                // Limite final: a própria imagem do mapa (ou o cenário simples)
                Rectangle lim = (texMapaSala.id > 0) ? AREA_MAPA_SALA : LIMITES_SALA_SIMPLES;

                if (playerPos.x < lim.x)              playerPos.x = lim.x;
                if (playerPos.x > lim.x + lim.width)  playerPos.x = lim.x + lim.width;
                if (playerPos.y < lim.y)              playerPos.y = lim.y;
                if (playerPos.y > lim.y + lim.height) playerPos.y = lim.y + lim.height;
                // -------------------------------------------------
                // INTERAÇÃO (tecla E)
                // -------------------------------------------------

                // pequeno retângulo na frente do jogador, na direção que ele está olhando
                Rectangle interacaoBox = { playerPos.x - interacaoAlcance / 2, playerPos.y - interacaoAlcance / 2, interacaoAlcance, interacaoAlcance };

                switch (direcaoAtual)
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

                pertoDaLuci = !luciAcompanhando && CheckCollisionRecs(interacaoBox, luciHitbox);
                bool pertoDaPorta = CheckCollisionRecs(AreaJogador(playerPos), ZONA_PORTA_SALA);

                if (IsKeyPressed(KEY_E)) {

                    if (pertoDaLuci) {

                        DialogoIniciar(&dlg, &dialogoLuciSala, OBJ_LUCI);
                        pertoDaLuci = false;

                    } else if (pertoDaPorta) {

                        // A porta de cima leva ao corredor (depois de falar com a Luci)
                        if (luciAcompanhando)
                            IniciarTransicao(&transicao, CORREDOR, DURACAO_FADE);
                        else
                            DialogoIniciar(&dlg, &dialogoPortaSalaTrancada, OBJ_NENHUM);
                    }
                }
            }
        }

        // =====================================================
        // CORREDOR DOS MONITORES
        // =====================================================

        else if (tela == CORREDOR && !transicao.ativa) {

            // O companion continua flutuando mesmo durante as conversas
            companionAngulo += dt * 2.5f;

            // -------------------------------------------------
            // CARTÃO DE RESUMO (depois de cada monitor)
            // -------------------------------------------------

            // (fechou o cartão do 3º monitor: a ALUCINAÇÃO MESTRE aparece logo abaixo, em EXPLORAÇÃO)
            if (cartao.ativo) {

                AtualizarCartao(&cartao, &dlg);
            }

            else

            // -------------------------------------------------
            // CONVERSA / QUIZ EM ANDAMENTO (o personagem fica parado)
            // -------------------------------------------------

            if (dlg.estado != DLG_NENHUM) {

                ResultadoDialogo r = DialogoAtualizar(&dlg);
                int m = dlg.objeto;

                // TAB no "saber mais" do cartão: o cartão volta
                if (r == RES_VOLTAR_CARTAO) {
                    cartao.ativo = true;
                    cartao.inicio = GetTime();
                }

                // Errou ou o tempo acabou: a alucinação ataca
                if ((r == RES_ERROU || r == RES_TEMPO) && m >= 0) {

                    progresso.alucinacao++;
                    quizDaBatalha = m;
                    motivoBatalha = r;

                    BatalhaIniciar(
                        &batalha,
                        (r == RES_TEMPO) ? BAT_COMBATE : BAT_MINIGAME,
                        m,
                        progresso.alucinacao
                    );

                    IniciarTransicao(&transicao, BATALHA, DURACAO_FADE_BATALHA);
                }

                else if (r == RES_FIM) {

                    // Fim do quiz de um monitor: registra o resultado
                    if (m >= 0 && dlg.respondeu && progresso.estado[m] == MON_ATIVO) {

                        if (dlg.acertou) {

                            progresso.estado[m] = MON_ACERTOU;
                            progresso.pontos += PONTOS_POR_ACERTO;

                        } else {

                            progresso.estado[m] = MON_ERROU;
                        }

                        pedirAutoSave = true;     // AUTO SAVE: terminou um monitor
                    }

                    // Fim da fala da Luci sobre o chefe: começa a ALUCINAÇÃO MESTRE
                    if (m == OBJ_CHEFE) {

                        BatalhaB2IniciarChefe(&batalhaB2, progresso.alucinacao);
                        BatalhaB2LigarLuci(&batalhaB2, &estabilidadeLuci, &ajudasPedidas);
                        salaDaBatalhaB2 = CORREDOR;
                        motivoBatalhaB2 = MOTIVO_CHEFE;
                        IniciarTransicao(&transicao, BATALHA_B2, DURACAO_FADE_BATALHA);
                    }
                }
            }

            // -------------------------------------------------
            // PRIMEIRA ENTRADA: Luci explica o corredor
            // -------------------------------------------------

            else if (!progresso.introVista) {

                progresso.introVista = true;
                DialogoIniciar(&dlg, &dialogoIntroCorredor, OBJ_NENHUM);
            }

            // -------------------------------------------------
            // FIM DO BLOCO 1: os 3 monitores resolvidos chamam a ALUCINAÇÃO MESTRE
            // e, vencida, o ranking (depois dele a Luci avisa que a saída abriu)
            // -------------------------------------------------

            else if (ContarConcluidos(&progresso) == TOTAL_MONITORES && !chefeVencido && !chefeAnunciado) {

                chefeAnunciado = true;
                DialogoIniciar(&dlg, &dialogoChefe, OBJ_CHEFE);
            }

            else if (chefeVencido && !progresso.fimAnunciado) {

                AbrirTelaRanking(&telaRanking, &ranking, "FIM DO BLOCO 1", nickname, progresso.pontos, idPartida);
            }

            // -------------------------------------------------
            // EXPLORAÇÃO LIVRE
            // -------------------------------------------------

            else {

                direcaoAtual = MoverJogador(&playerPos, direcaoAtual);

                // Limites do corredor (só a faixa do chão é andável)
                if (playerPos.x < 80)  playerPos.x = 80;
                if (playerPos.x > 720) playerPos.x = 720;
                if (playerPos.y < 285) playerPos.y = 285;
                if (playerPos.y > 395) playerPos.y = 395;

                Rectangle jogador = AreaJogador(playerPos);

                // Interações (tecla E)
                if (IsKeyPressed(KEY_E)) {

                    // Porta de baixo: volta para a sala
                    if (CheckCollisionRecs(jogador, PORTA_VOLTAR_CORREDOR)) {

                        IniciarTransicao(&transicao, JOGANDO, DURACAO_FADE);
                    }

                    // Porta da direita: trancada até resolver tudo
                    else if (CheckCollisionRecs(jogador, PORTA_FIM_CORREDOR)) {

                        if (chefeVencido) {

                            // Bloco 2: Biblioteca dos Ecos
                            IniciarTransicao(&transicao, BIBLIOTECA, DURACAO_FADE);

                        } else if (ContarConcluidos(&progresso) == TOTAL_MONITORES) {

                            // A ALUCINAÇÃO MESTRE escapou da última vez: revanche
                            DialogoIniciar(&dlg, &dialogoChefeVolta, OBJ_CHEFE);

                        } else {

                            DialogoIniciar(&dlg, &dialogoPortaTrancada, OBJ_NENHUM);
                        }
                    }

                    // Monitores
                    else {

                        for (int i = 0; i < TOTAL_MONITORES; i++) {

                            if (!CheckCollisionRecs(jogador, ZonaInteracao(monitores[i].area)))
                                continue;

                            int proximo = ProximoMonitor(&progresso);

                            if (progresso.estado[i] == MON_ACERTOU || progresso.estado[i] == MON_ERROU) {

                                // Monitor já resolvido: mostra de novo o cartão do que foi aprendido
                                AbrirCartao(&cartao, i, progresso.estado[i] == MON_ACERTOU,
                                            TITULOS_RESUMO_MONITOR[i], FRASES_RESUMO_MONITOR[i], &saberMaisMonitores[i]);

                            } else if (QUIZ_EM_ORDEM && i != proximo) {

                                DialogoFalaAvulsa(&dlg, TextFormat("Luci: Comece pelo MONITOR %d!", proximo + 1));

                            } else {

                                // NEUTRO -> ATIVO: a alucinação do monitor vira uma batalha
                                progresso.estado[i] = MON_ATIVO;
                                BatalhaB2IniciarMonitor(&batalhaB2, i, progresso.alucinacao);
                                BatalhaB2LigarLuci(&batalhaB2, &estabilidadeLuci, &ajudasPedidas);
                                salaDaBatalhaB2 = CORREDOR;
                                motivoBatalhaB2 = MOTIVO_MONITOR;
                                IniciarTransicao(&transicao, BATALHA_B2, DURACAO_FADE_BATALHA);
                            }

                            break;
                        }
                    }
                }
            }
        }

        // =====================================================
        // BATALHA (COMBATE / MINIGAME)
        // =====================================================

        else if (tela == BATALHA && !transicao.ativa) {

            if (BatalhaAtualizar(&batalha)) {

                if (batalha.derrota)
                    progresso.alucinacao++;

                // Errou (ou o tempo acabou): perde pontos.
                // Se a vida zerou na batalha, perde mais.
                int perdidos = AlterarPontos(&progresso,
                    -(PENALIDADE_ERRO + (batalha.derrota ? PENALIDADE_HP_ZERO : 0)));

                // De volta ao corredor: Luci explica a resposta certa
                const Dialogo *q = &monitores[quizDaBatalha].quiz;

                dlg.conteudo = *q;
                dlg.objeto = quizDaBatalha;
                dlg.respondeu = true;
                dlg.acertou = false;

                DialogoMostrarFeedback(
                    &dlg,
                    TextFormat(
                        "Luci: %s A resposta certa era a %c. %s",
                        (motivoBatalha == RES_TEMPO) ? "O tempo acabou!" : "Não foi dessa vez.",
                        'A' + q->correta,
                        q->explicacao
                    ),
                    ORANGE,
                    perdidos            // aparece em vermelho na caixa: "-5 PONTOS"
                );

                IniciarTransicao(&transicao, CORREDOR, DURACAO_FADE_BATALHA);
            }
        }

        // =====================================================
        // BATALHA DO BLOCO 1 (monitores e ALUCINAÇÃO MESTRE)
        // =====================================================

        else if (tela == BATALHA_B2 && !transicao.ativa) {

            if (BatalhaB2Atualizar(&batalhaB2))
                FimBatalhaB2(&ctxB2);
        }

        // =====================================================
        // BLOCO 2 - BIBLIOTECA DOS ECOS
        // =====================================================

        else if (tela == BIBLIOTECA && !transicao.ativa) {

            companionAngulo += dt * 2.5f;

            if (RogueAtualizar(&biblioteca, &progresso, &dlg, dt) == ROGUE_SAIR)
                IniciarTransicao(&transicao, CORREDOR, DURACAO_FADE);
        }

        // =====================================================
        // OPÇÕES
        // =====================================================

        else if (tela == OPCOES) {

            if (AtualizarOpcoes(&menuOpcoes, &exibicao)) {

                AplicarExibicao(exibicao);
                SalvarConfigExibicao(exibicao);
            }

            // BACKSPACE: fecha a lista aberta ou volta ao menu (igual ao ESC)
            if (IsKeyPressed(KEY_BACKSPACE)) {

                if (menuOpcoes.aberta >= 0)
                    menuOpcoes.aberta = -1;
                else
                    tela = MENU;
            }
        }

        // =====================================================
        // AUTO SAVE (feito aqui, depois de toda a lógica do frame)
        // =====================================================

        if (pedirAutoSave && (tela == JOGANDO || tela == CORREDOR || tela == BIBLIOTECA)) {

            pedirAutoSave = false;
            saveFalhou = !SalvarJogo(nickname, (tela == BATALHA_B2) ? salaDaBatalhaB2 : tela, playerPos, direcaoAtual, luciAcompanhando, &progresso, estabilidadeLuci, ajudasPedidas, chefeVencido, idPartida, &progBiblioteca);

            if (!saveFalhou)
                existeSave = true;

            avisoSave = TEMPO_AVISO_SAVE;
        }

        if (!pausado) {
            if (avisoSave > 0.0f) avisoSave -= dt;
        }

        // =====================================================
        // ANIMAÇÃO DO PERSONAGEM
        // =====================================================

        // Só anima se ele realmente se moveu (parado, em diálogo ou
        // empurrando a parede ele fica no quadro 0)
        bool andou = (tela == JOGANDO || tela == CORREDOR) &&
                     !transicao.ativa &&
                     Vector2Distance(posInicioFrame, playerPos) > 0.01f;

        if (!pausado)
            AtualizarAnimacao(&animJogador, andou, dt);

        // =====================================================
        // OBJETIVO E LUCI COMO GUIA
        // =====================================================

        Objetivo objetivo = ObjetivoAtual(tela, &progresso, luciAcompanhando, luciHitbox, chefeVencido);

        bool emExploracao = (tela == JOGANDO || tela == CORREDOR);
        bool podeReagir = emExploracao && luciAcompanhando && !transicao.ativa && !pausado &&
                          dlg.estado == DLG_NENHUM && !cartao.ativo && !telaRanking.aberto;

        if (podeReagir && !andou) tempoParado += dt;
        else                     tempoParado = 0.0f;

        // Parado há 5 s: a Luci fica em dúvida (o objetivo continua escrito no topo)
        if (tempoParado > 5.0f && tempoParado - dt <= 5.0f)
            MostrarEmocaoLuci(EMOCAO_DUVIDA, 2.5f);

        if (!pausado && tempoEmocaoLuci > 0.0f)
            tempoEmocaoLuci -= dt;

        instabilidadeLuci = 1.0f - (float)estabilidadeLuci / ESTABILIDADE_MAX_LUCI;

        // Na Biblioteca, a Luci também se corrompe a cada desafio da tentativa
        if (tela == BIBLIOTECA)
            instabilidadeLuci = fmaxf(instabilidadeLuci, RogueCorrupcaoLuci(&biblioteca));
        brilhoLuci = 1.0f + 0.15f * (float)(ContarConcluidos(&progresso));

        // =====================================================
        // DESENHO
        // =====================================================

        BeginTextureMode(alvo);

        // =====================================================
        // PAUSE
        // =====================================================

        if (pausado) {

            DesenharPausa(fundoPausa, selecaoPausa);
        }

        // =====================================================
        // MENU
        // =====================================================

        else if (tela == MENU) {

            ClearBackground(cor_de_fundo_menu);

            // Título original
            DrawTexture(imagem_titulo, 215, (int)y, WHITE);
            DrawTexture(imagem_titulo, 215, (int)y1, Fade(PURPLE, 0.1f));
            DrawTexture(imagem_titulo, 215, (int)y2, Fade(PURPLE, 0.1f));

            // Botões
            if (existeSave) {
                DrawRectangleRec(btnContinuar, corBtnContinuar);
                DesenharTextoBotao(btnContinuar, "CONTINUAR", 20, WHITE);
            }

            bool pedindoConfirmacao = (confirmarNovoJogo > 0.0f);

            DrawRectangleRec(btnJogar, pedindoConfirmacao ? MAROON : corBtnJogar);
            DrawRectangleRec(btnOpcoes, corBtnOpcoes);
            DrawRectangleRec(btnSair, corBtnSair);

            // Textos
            DesenharTextoBotao(btnJogar, pedindoConfirmacao ? "TEM CERTEZA?" : "JOGAR", 20, WHITE);
            DesenharTextoBotao(btnOpcoes, "OPÇÕES", 20, WHITE);
            DesenharTextoBotao(btnSair, "SAIR", 20, WHITE);

            if (pedindoConfirmacao) {
                const char *aviso = "Clique de novo para começar do zero (o progresso salvo será substituído)";
                DrawText(aviso, LARGURA_TELA / 2 - MeasureText(aviso, 10) / 2, (int)(btnSair.y + btnSair.height) + 14, 10, LIGHTGRAY);
            }

            DesenharRankingMenu(&ranking, tempo);
        }

        // =====================================================
        // INTRO CENTRALIZADA
        // =====================================================

        else if (tela == INTRO) {

            // Mesmo fundo do menu
            ClearBackground(cor_de_fundo_menu);

            // -------------------------------------------------
            // ÁREA CENTRAL
            // -------------------------------------------------

            const int larguraConteudo = 500;
            const int xConteudo = (800 - larguraConteudo) / 2;

            // =================================================
            // INDICADOR DE ETAPAS
            // =================================================

            int espacamento = 14;
            int larguraIndicador = 3 * espacamento;

            int xIndicador = 400 - larguraIndicador / 2;

            for (int i = 0; i < 4; i++) {

                Color corEtapa;

                if (i == etapaIntro)
                    corEtapa = WHITE;
                else
                    corEtapa = Fade(LIGHTGRAY, 0.35f);

                DrawCircle(xIndicador + i * espacamento, 48, 3, corEtapa);
            }

            const char *textoEtapa = TextFormat("Etapa %d de 4", etapaIntro + 1);

            DrawText(textoEtapa, 400 - MeasureText(textoEtapa, 9) / 2, 58, 9, GRAY);

            // =================================================
            // PAINEL DA IMAGEM
            // =================================================

            Rectangle painelImagem = { xConteudo, 85, larguraConteudo, 255 };

            // Cena animada que ilustra o texto do narrador desta etapa
            DesenharCenaIntro(etapaIntro, painelImagem, tempo);

            // =================================================
            // CAIXA DO NARRADOR
            // =================================================

            Rectangle caixaNarrador = {
                xConteudo,
                355,
                larguraConteudo,
                (etapaIntro == 3) ? 110 : 80
            };

            DrawRectangleRec(caixaNarrador, (Color){245, 245, 245, 255});
            DrawRectangleLinesEx(caixaNarrador, 1, Fade(LIGHTGRAY, 0.6f));

            // Conteúdo da caixa de narrativa
            if (etapaIntro < 3) {

                DrawRectangle(xConteudo + 12, 349, 48, 16, LIGHTGRAY);
                DrawText("Narrador", xConteudo + 16, 353, 8, BLACK);
                DrawText(textosIntro[etapaIntro], xConteudo + 12, 380, 11, DARKGRAY);

            } else {

                // Etiqueta da etapa final
                DrawRectangle(xConteudo + 12, 349, 58, 16, LIGHTGRAY);
                DrawText("Marcador", xConteudo + 16, 353, 8, BLACK);
                DrawText("LUCI // REGISTRO DE IDENTIDADE!", xConteudo + 12, 377, 9, BLACK);
                DrawText("Digite seu codigo de identificacao de 3 letras:", xConteudo + 12, 393, 10, DARKGRAY);

                // Três campos do nickname, como no print
                for (int i = 0; i < 3; i++) {

                    Rectangle caixaLetra = { 245 + i * 55, 405, 45, 45 };

                    Color corCaixa =
                        (i == nicknamePos && nicknamePos < 3)
                        ? (Color){225, 225, 225, 255}
                        : (Color){238, 238, 238, 255};

                    DrawRectangleRec(caixaLetra, corCaixa);

                    const char letra[2] = { nickname[i], '\0' };

                    DrawText(letra, (int)caixaLetra.x + 12, (int)caixaLetra.y + 5, 32, BLACK);
                }

                if (avisoNomeRepetido > 0.0f)
                    DrawText("Esse nome já está no ranking! Escolha outro.", xConteudo + 12, 480, 10, RED);
            }

            // =================================================
            // BOTÃO CONTINUAR
            // =================================================

            Rectangle btnContinuar = {
                xConteudo + larguraConteudo - 125,
                (etapaIntro == 3) ? 475 : 457,
                125,
                30
            };

            Color corContinuar = BLACK;

            // Na última etapa, o botão só pode continuar com 3 letras.
            bool podeContinuar = (etapaIntro < 3) || (nicknamePos == 3);

            if (CheckCollisionPointRec(mouse, btnContinuar) && podeContinuar) {

                corContinuar = DARKGRAY;

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {

                    if (etapaIntro < 3) {

                        etapaIntro++;

                    } else if (nicknamePos == 3) {

                        cliqueConfirmarNome = true;     // a lógica confere o nome no próximo quadro
                    }
                }
            }

            DrawRectangleRec(btnContinuar, corContinuar);

            DrawText("CONTINUAR", (int)btnContinuar.x + 22, (int)btnContinuar.y + 9, 8, WHITE);
            DrawText(">", (int)btnContinuar.x + 102, (int)btnContinuar.y + 7, 12, WHITE);

            // =================================================
            // INSTRUÇÕES
            // =================================================

            const char *instrucao1;
            const char *instrucao2;

            if (etapaIntro == 3) {

                instrucao1 = "Digite A-Z | ENTER para confirmar";
                instrucao2 = "BACKSPACE apaga a ultima letra";

            } else {

                instrucao1 = "ENTER para continuar";
                instrucao2 = "BACKSPACE para voltar";
            }

            DrawText(instrucao1, 400 - MeasureText(instrucao1, 10) / 2, 510, 10, GRAY);
            DrawText(instrucao2, 400 - MeasureText(instrucao2, 10) / 2, 528, 10, GRAY);
        }

        // =====================================================
        // TELA JOGANDO (SALA INICIAL)
        // =====================================================

        else if (tela == JOGANDO) {

            ClearBackground((Color){8, 6, 18, 255});

            // -------------------------------------------------
            // MAPA SIMPLES
            // -------------------------------------------------

            Rectangle mapa = {50, 50, 700, 500};
            bool mapaCarregado = (texMapaSala.id > 0);

            if (mapaCarregado) {

                // Arte do mapa, ajustada à área AREA_MAPA_SALA
                DrawTexturePro(
                    texMapaSala,
                    (Rectangle){ 0, 0, (float)texMapaSala.width, (float)texMapaSala.height },
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

            bool salaLiberada = luciAcompanhando;

            if (!mapaCarregado) {

                DrawRectangleRec(PORTA_SALA, (Color){35, 25, 55, 255});
                DrawRectangleLinesEx(PORTA_SALA, 3, salaLiberada ? PURPLE : MAROON);

                // Maçaneta
                DrawCircle((int)(PORTA_SALA.x + PORTA_SALA.width - 12), (int)(PORTA_SALA.y + PORTA_SALA.height / 2), 4, GOLD);

            }
            // Com a imagem do mapa, o arco da porta já está desenhado nela:
            // nada é pintado por cima (trancada ou não)

            bool livre = (dlg.estado == DLG_NENHUM && !transicao.ativa);

            if (livre && CheckCollisionRecs(AreaJogador(playerPos), ZONA_PORTA_SALA)) {

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

            if (!luciAcompanhando) {

                // Mesmo visual de quando ela acompanha o jogador, flutuando parada
                Vector2 pLuci = CentroLuciSala(luciHitbox, tempo);

                DesenharLuciTamanho(pLuci, ALTURA_LUCI_SALA);
                DrawText("Luci", (int)pLuci.x - MeasureText("Luci", 10) / 2, (int)(pLuci.y - ALTURA_LUCI_SALA / 2) - 14, 10, Fade(SKYBLUE, 0.8f));
            }

            // -------------------------------------------------
            // PERSONAGEM
            // -------------------------------------------------

            DesenharJogadorComLuci(spritesJogador, playerPos, direcaoAtual, animJogador.quadro,
                                   luciAcompanhando, companionAngulo);

            // Paredes invisíveis e pés do personagem (para calibrar)
            // Desligue em MOSTRAR_PAREDES_INVISIVEIS
#if MOSTRAR_PAREDES_INVISIVEIS
            if (mapaCarregado) {

                for (int i = 0; i < NUM_PAREDES_SALA; i++) {

                    DrawRectangleRec(PAREDES_SALA[i], Fade(RED, 0.25f));
                    DrawRectangleLinesEx(PAREDES_SALA[i], 1, RED);
                }

                DrawRectangleLinesEx(PesJogador(playerPos), 1, YELLOW);
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

            DrawText(nickname, hudDir - MeasureText(nickname, 14), 65, 14, WHITE);

            const char *textoPontos = TextFormat("PONTOS: %d", progresso.pontos);
            DrawText(textoPontos, hudDir - MeasureText(textoPontos, 12), 84, 12, GOLD);

            if (livre && pertoDaLuci) {

                // aviso de interação
                const char *aviso = "Aperte E para falar com a Luci";
                DrawText(aviso, LARGURA_TELA / 2 - MeasureText(aviso, 20) / 2, ALTURA_TELA - 40, 20, RAYWHITE);
            }

            // Ambiente, objetivo e marcador
            DesenharGlitchAmbiente(progresso.alucinacao, tempo);
            DesenharObjetivoHUD(&objetivo);

            // Conversa por cima de tudo
            DialogoDesenhar(&dlg);
            DesenharCartao(&cartao, tempo);
            DesenharTelaRanking(&telaRanking, &ranking, tempo);
            DesenharAvisoSave(avisoSave, saveFalhou, tempo);
        }

        // =====================================================
        // TELA CORREDOR
        // =====================================================

        else if (tela == CORREDOR) {

            ClearBackground((Color){8, 6, 18, 255});

            Rectangle corredor = {50, 100, 700, 320};

            // -------------------------------------------------
            // CENÁRIO: parede de fundo e chão
            // -------------------------------------------------

            DrawRectangle(50, 100, 700, 140, (Color){26, 20, 46, 255});
            DrawRectangle(50, 240, 700, 180, (Color){18, 15, 30, 255});

            // Rodapé (linha entre a parede e o chão)
            DrawRectangle(50, 236, 700, 6, Fade(PURPLE, 0.45f));

            // Divisões do piso
            for (int x = 50; x < 750; x += 70)
                DrawLine(x, 242, x, 420, Fade(PURPLE, 0.12f));

            // Luminárias pulsando no teto
            for (int i = 0; i < 5; i++) {

                float pulso = 0.5f + 0.5f * sinf(tempo * 1.5f + i * 1.3f);
                int lx = 120 + i * 140;

                DrawCircle(lx, 108, 20, Fade(SKYBLUE, 0.04f + 0.08f * pulso));
                DrawRectangle(lx - 12, 102, 24, 5, Fade(SKYBLUE, 0.5f + 0.4f * pulso));
            }

            DrawRectangleLinesEx(corredor, 4, Fade(PURPLE, 0.55f));

            // -------------------------------------------------
            // PORTAS
            // -------------------------------------------------

            int concluidos = ContarConcluidos(&progresso);
            bool saidaLiberada = chefeVencido;
            bool chefeNaPorta = (concluidos == TOTAL_MONITORES && !chefeVencido);

            float pulsoPorta = 0.5f + 0.5f * sinf(tempo * 4.0f);

            Color bordaSaida = saidaLiberada ? Fade(GREEN, 0.55f + 0.45f * pulsoPorta)
                             : (chefeNaPorta ? Fade(MAGENTA, 0.55f + 0.45f * pulsoPorta) : MAROON);

            DesenharPortaCorredor(PORTA_VOLTAR_CORREDOR, PURPLE);
            DesenharPortaCorredor(PORTA_FIM_CORREDOR, bordaSaida);

            DrawText(
                "SALA",
                (int)(PORTA_VOLTAR_CORREDOR.x + PORTA_VOLTAR_CORREDOR.width / 2) - MeasureText("SALA", 10) / 2,
                (int)(PORTA_VOLTAR_CORREDOR.y + PORTA_VOLTAR_CORREDOR.height) + 6,
                10,
                LIGHTGRAY
            );

            const char *textoSaida = saidaLiberada ? "SAIDA" : (chefeNaPorta ? "CHEFE" : "TRANCADA");

            DrawText(
                textoSaida,
                (int)(PORTA_FIM_CORREDOR.x + PORTA_FIM_CORREDOR.width) - MeasureText(textoSaida, 10) - 8,
                (int)PORTA_FIM_CORREDOR.y - 16,
                10,
                saidaLiberada ? GREEN : (chefeNaPorta ? MAGENTA : RED)
            );

            // -------------------------------------------------
            // MONITORES
            // -------------------------------------------------

            Rectangle jogador = AreaJogador(playerPos);

            // Só mostra os destaques quando o jogador está livre para agir
            bool explorando = (dlg.estado == DLG_NENHUM && progresso.introVista && !transicao.ativa);

            for (int i = 0; i < TOTAL_MONITORES; i++) {

                Rectangle a = monitores[i].area;
                EstadoMonitor est = progresso.estado[i];

                DesenharMonitor(i, a, est, tempo, spritesMonitor);

                // Nome embaixo, com a cor do estado
                const char *nome = monitores[i].nome;
                Color corNome = LIGHTGRAY;

                if (est == MON_ACERTOU)     { nome = TextFormat("%s (OK)", monitores[i].nome);   corNome = GREEN; }
                else if (est == MON_ERROU)  { nome = TextFormat("%s (ERRO)", monitores[i].nome); corNome = RED; }
                else if (est == MON_ATIVO)  { corNome = GOLD; }

                DrawText(
                    nome,
                    (int)(a.x + a.width / 2) - MeasureText(nome, 10) / 2,
                    (int)(a.y + a.height) + 32,
                    10,
                    corNome
                );

                // Destaque quando o jogador está perto
                if (explorando && CheckCollisionRecs(jogador, ZonaInteracao(a))) {

                    DrawRectangleLinesEx(
                        (Rectangle){ a.x - 4, a.y - 4, a.width + 8, a.height + 8 },
                        2,
                        GOLD
                    );

                    DrawText(
                        "PRESSIONE E",
                        (int)(a.x + a.width / 2) - MeasureText("PRESSIONE E", 12) / 2,
                        (int)a.y - 22,
                        12,
                        GOLD
                    );
                }
            }

            // Dicas "PRESSIONE E" nas portas
            if (explorando) {

                if (CheckCollisionRecs(jogador, PORTA_VOLTAR_CORREDOR)) {

                    DrawText(
                        "PRESSIONE E",
                        (int)(PORTA_VOLTAR_CORREDOR.x + PORTA_VOLTAR_CORREDOR.width) + 10,
                        (int)PORTA_VOLTAR_CORREDOR.y,
                        12,
                        GOLD
                    );
                }

                if (CheckCollisionRecs(jogador, PORTA_FIM_CORREDOR)) {

                    DrawText(
                        "PRESSIONE E",
                        (int)(PORTA_FIM_CORREDOR.x + PORTA_FIM_CORREDOR.width) - MeasureText("PRESSIONE E", 12) - 8,
                        (int)PORTA_FIM_CORREDOR.y - 32,
                        12,
                        GOLD
                    );
                }
            }

            // -------------------------------------------------
            // PERSONAGEM E COMPANION
            // -------------------------------------------------

            DesenharJogadorComLuci(spritesJogador, playerPos, direcaoAtual, animJogador.quadro,
                                   true, companionAngulo);

            // -------------------------------------------------
            // HUD
            // -------------------------------------------------

            DrawText(TextFormat("MONITORES: %d/%d", concluidos, TOTAL_MONITORES), 65, 62, 14, WHITE);
            DrawText(TextFormat("PONTOS: %d", progresso.pontos), 230, 62, 14, GOLD);
            DrawText(
                TextFormat("ALUCINAÇÃO: %d", progresso.alucinacao),
                360,
                62,
                14,
                (progresso.alucinacao == 0) ? LIGHTGRAY : (progresso.alucinacao < 3 ? ORANGE : RED)
            );
            DrawText("CORREDOR DO HISTÓRICO", 65, 82, 10, GRAY);
            DrawText(nickname, 700 - MeasureText(nickname, 14), 62, 14, WHITE);

            if (dlg.estado == DLG_NENHUM) {

                DrawText("W A S D  -  MOVIMENTAR", 65, 452, 12, LIGHTGRAY);
            }

            // Ambiente, objetivo e marcador
            DesenharGlitchAmbiente(progresso.alucinacao, tempo);
            DesenharObjetivoHUD(&objetivo);

            // Conversa por cima de tudo
            DialogoDesenhar(&dlg);
            DesenharCartao(&cartao, tempo);
            DesenharTelaRanking(&telaRanking, &ranking, tempo);
            DesenharAvisoSave(avisoSave, saveFalhou, tempo);
        }

        // =====================================================
        // TELA BATALHA
        // =====================================================

        else if (tela == BATALHA) {

            BatalhaDesenhar(&batalha, nickname, tempo);
        }

        // =====================================================
        // TELA BATALHA DO BLOCO 1 (monitores e ALUCINAÇÃO MESTRE)
        // =====================================================

        else if (tela == BATALHA_B2) {

            BatalhaB2Desenhar(&batalhaB2, nickname, tempo);
        }

        // =====================================================
        // TELA BLOCO 2 - BIBLIOTECA DOS ECOS
        // =====================================================

        else if (tela == BIBLIOTECA) {

            RogueDesenhar(&biblioteca, spritesJogador, companionAngulo, tempo, nickname, progresso.pontos);
            DialogoDesenhar(&dlg);
            DesenharAvisoSave(avisoSave, saveFalhou, tempo);
        }

        // =====================================================
        // TELA OPÇÕES
        // =====================================================

        else if (tela == OPCOES) {

            DesenharOpcoes(&menuOpcoes, exibicao, cor_de_fundo_menu);
        }

        // =====================================================
        // FADE DA TRANSIÇÃO (sempre por cima de tudo)
        // =====================================================

        if (transicao.ativa && !pausado)
            DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, transicao.alfa));

        EndTextureMode();

        // Acabou de apertar ESC: a tela deste frame vira o fundo do pause
        if (capturarPausa) {

            fundoPausa = CapturarFundoPausa(alvo);
            capturarPausa = false;
            pausado = true;
        }

        // Estica o quadro de 800x600 para o tamanho da janela
        BeginDrawing();
        DesenharNaJanela(alvo, filtroNitidez);
        EndDrawing();
    }

    // 3. FINALIZAÇÃO

    // AUTO SAVE: fechou a janela no meio do jogo
    // (no menu ou na intro não salva, para não apagar o progresso)
    if (tela == JOGANDO || tela == CORREDOR || tela == BATALHA || tela == BATALHA_B2 || tela == BIBLIOTECA)
        SalvarJogo(nickname, (tela == BATALHA_B2) ? salaDaBatalhaB2 : tela, playerPos, direcaoAtual, luciAcompanhando, &progresso, estabilidadeLuci, ajudasPedidas, chefeVencido, idPartida, &progBiblioteca);

    // (as texturas precisam ser liberadas ANTES de fechar a janela)

    if (texMapaSala.id > 0)
        UnloadTexture(texMapaSala);

    if (fundoPausa.id > 0)
        UnloadTexture(fundoPausa);

    UnloadRenderTexture(alvo);
    UnloadShader(filtroNitidez.shader);

    UnloadTexture(imagem_titulo);
    DescarregarLuci();
    for (int i = 0; i < NUM_DIRECOES; i++)
        if (!spritesJogador[i].emprestada)
            UnloadTexture(spritesJogador[i].textura);

    for (int i = 0; i < 4; i++)
        if (spritesMonitor[i].id > 0)
            UnloadTexture(spritesMonitor[i]);

    EncerrarSons();
    CloseWindow();

    return 0;
}
