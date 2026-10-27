#include "raylib.h"
#include "raymath.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// =============================================================
// UtopIA - BLOCO 1
//   Sala inicial (Luci) -> porta de cima -> Corredor dos Monitores
//   Cada monitor tem um quiz (conteúdo do "Quizz - missão 1.pdf").
//   Tempo esgotado -> COMBATE estilo Undertale
//   Resposta errada -> MINIGAME de esquiva estilo Undertale
//   Resposta certa  -> ganha pontos
// =============================================================

// Definimos os "canais" (telas) do nosso jogo
typedef enum { MENU, INTRO, JOGANDO, OPCOES, CORREDOR, BATALHA, SETOR2, BATALHA_B2,
               SALA_REGISTROS, SALA_LOGICA, SALA_LACUNA } TelaAtual;
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
static const char *PASTAS_SPRITES_JOGADOR[] = { "", "resources/sprites/player/", "resources/sprites/save/" };
#define NUM_PASTAS_SPRITES_JOGADOR 3

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

#define TOTAL_MONITORES     4
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
#define OBJ_SAVE    -3

// =============================================================
// SAVE - CONSTANTES
// =============================================================

#define ARQUIVO_SAVE         "utopia.sav"
#define ARQUIVO_SAVE_TEMP    "utopia.sav.tmp"
#define VERSAO_SAVE          3            // aumente quando mudar o que vai no save (3 = Bloco 2 com salas-puzzle)
#define ASSINATURA_SAVE      0x41495455u  // identifica o arquivo como save do UtopIA
#define TEMPO_AVISO_SAVE     2.0f         // segundos do aviso "JOGO SALVO" na tela

// Ponto de save: folha com 12 quadros lado a lado (fluxo de dados)
#define SPRITE_PONTO_SAVE    "Ponto_save.png"
#define QUADROS_PONTO_SAVE   12
#define FPS_PONTO_SAVE       12.0f
#define ESCALA_PONTO_SAVE    2.0f         // 24x42 na imagem -> 48x84 na tela

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
typedef struct {
    const char *titulo;                     // cabeçalho do painel da pergunta
    const char *paginas[MAX_PAGINAS];
    int numPaginas;
    const char *pergunta;
    const char *opcoes[NUM_OPCOES];
    int correta;                            // índice da opção certa (0 = A ... 3 = D)
    const char *explicacao;                 // Luci explica a resposta certa
} Dialogo;

typedef struct {
    const char *nome;
    Rectangle area;                         // tela do monitor (o pé é desenhado embaixo)
    Dialogo quiz;
} MonitorQuiz;

typedef enum { MON_NEUTRO, MON_ATIVO, MON_ACERTOU, MON_ERROU } EstadoMonitor;

typedef enum { DLG_NENHUM, DLG_PAGINAS, DLG_ESCOLHA, DLG_FEEDBACK, DLG_CONFIRMA } EstadoDialogo;

// O que aconteceu na conversa neste frame
typedef enum { RES_NADA, RES_FIM, RES_ERROU, RES_TEMPO, RES_SIM, RES_NAO } ResultadoDialogo;

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
} SessaoDialogo;

typedef struct {
    EstadoMonitor estado[TOTAL_MONITORES];
    int pontos;
    int alucinacao;                         // cresce a cada erro (README: errar fortalece a alucinação)
    bool introVista;
    bool fimAnunciado;
} ProgressoCorredor;

// Bloco 2: sala central + três salas-puzzle, cada uma com um objeto mágico
#define NUM_OBJETOS_B2    3          // CRISTAL RAG, LUPA DA VERIFICAÇÃO, PERGUNTA PERFEITA
#define NUM_REGISTROS     6          // Sala dos Registros
#define NUM_FRAGMENTOS    5          // Sala da Lacuna

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

// O que vai para o arquivo de save. Só o que não dá para reconstruir:
// diálogo aberto, quiz no meio, batalha e animação ficam de fora.
typedef struct {
    unsigned int assinatura;
    int versao;
    char nickname[4];
    int sala;                               // JOGANDO ou CORREDOR
    float x, y;
    int direcao;
    bool luciAcompanhando;
    ProgressoCorredor progresso;
    ProgressoBloco2 bloco2;
} DadosSave;

// Ponto de save no mapa (o "fluxo de dados")
typedef struct {
    TelaAtual sala;
    Vector2 base;                           // centro da base, no chão
} PontoSave;

// Sala inicial, no canto inferior direito do mapa.
// Para criar outro ponto, é só acrescentar uma linha aqui e aumentar NUM_PONTOS_SAVE
// (e desenhar/tratar o E na sala correspondente, como é feito na sala inicial).
static const PontoSave PONTOS_SAVE[] = {
    { JOGANDO,  { MAPA_X(870), MAPA_Y(905) } },
};
#define NUM_PONTOS_SAVE 1

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

static const Dialogo dialogoLuciSala = {
    .paginas = {
        "Luci: Olá, Thomas. Eu imagino que se pergunte o que está fazendo aqui. Eu sou Luci, um dos algoritmos que fazem parte da IA que você usa todos os dias.",
        "Luci: No início, o universo de UtopIA funcionava perfeitamente, mas com o tempo, a IA começou a alucinar e está destruindo a si mesma.",
        "Luci: Eu trouxe você para cá porque preciso de ajuda e você é o usuário que mais nos utiliza.",
        "Luci: Para voltar para casa, você vai precisar enfrentar 3 missões. A primeira é uma sequência de quizzes sobre alucinação para você entender o problema que estamos enfrentando.",
        "Luci: Na segunda missão, o desafio aumenta e será preciso resolver uma sequência de puzzles para conquistar os objetos mágicos.",
        "Luci: No final, você irá enfrentar a IA alucinada com seus objetos.",
        "Luci: Não há tempo a perder, Thomas. O destino de UtopIA depende de você. Vamos começar a primeira missão.",
        "Luci: A porta lá em cima leva ao Corredor do Histórico. Eu vou com você."
    },
    .numPaginas = 8
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
        .area = {110, 140, 100, 70},
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

    // ---------- MONITOR 2: mapa mental (Sala 1 / Questão 2) ----------
    {
        .nome = "MONITOR 2",
        .area = {260, 140, 100, 70},
        .quiz = {
            .titulo = "MONITOR 2 // O MAPA MENTAL DA IA",
            .paginas = {
                "Luci: Imagine o processo de raciocínio da IA como uma navegação em um \"mapa mental\", onde cada conceito é um ponto e as relações entre eles são caminhos.",
                "Luci: A alucinação ocorre por dois mecanismos principais: Reutilização de Caminhos (Path Reuse) e Compressão de Caminhos (Path Compression).",
                "Luci: Com Path Reuse, no início do treinamento, o modelo pode \"pegar atalhos\" e aplicar um caminho de raciocínio que funcionou para um assunto em um contexto completamente diferente.",
                "Luci: Já no Path Compression, conforme o treinamento avança, caminhos usados com frequência são \"encurtados\". A IA deixa de passar por todas as etapas intermediárias e \"pula\" para uma conclusão."
            },
            .numPaginas = 4,
            .pergunta = "Segundo Luci, qual alternativa apresenta uma afirmação FALSA sobre os mecanismos de alucinação?",
            .opcoes = {
                "Path Reuse: no início, o modelo reaproveita caminhos de raciocínio que já funcionaram.",

                "Path Compression: com o treinamento, caminhos frequentes são encurtados para ganhar eficiência.",

                "Path Compression: com o treinamento, a IA passa a percorrer todas as etapas antes de concluir.",

                "Path Reuse: o modelo aplica um caminho de raciocínio conhecido a contextos diferentes."
            },
            .correta = 2,
            .explicacao = "No Path Compression acontece o contrário: a IA deixa de passar pelas etapas intermediárias e pula direto para uma conclusão."
        }
    },

    // ---------- MONITOR 3: tipos de alucinação (Sala 2 / Questão 3) ----------
    {
        .nome = "MONITOR 3",
        .area = {410, 140, 100, 70},
        .quiz = {
            .titulo = "MONITOR 3 // TIPOS DE ALUCINAÇÃO",
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

    // ---------- MONITOR 4: riscos e mitigação (Sala 2 / Questão 4) ----------
    {
        .nome = "MONITOR 4",
        .area = {560, 140, 100, 70},
        .quiz = {
            .titulo = "MONITOR 4 // RISCOS E MITIGAÇÃO",
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
static const Dialogo dialogoIntroCorredor = {
    .paginas = {
        "Luci: Este é o Corredor do Histórico. Cada um destes 4 monitores guarda uma lição sobre alucinação.",
        "Luci: Chegue perto de um monitor e aperte E. Eu explico o conteúdo e depois vem uma pergunta. Fique de olho na barra de tempo!",
        "Luci: Se o tempo acabar ou você errar, a alucinação ataca e você vai ter que desviar dela. Aqui você não perde vidas, mas cada erro deixa a alucinação mais forte.",
        "Luci: Quando os 4 monitores estiverem resolvidos, a porta da direita se destranca."
    },
    .numPaginas = 4
};

static const Dialogo dialogoFimCorredor = {
    .paginas = {
        "Luci: Muito bem! Você passou pelos 4 monitores.",
        "* Em algum lugar do corredor, uma tranca se abre. A porta da direita está liberada."
    },
    .numPaginas = 2
};

static const Dialogo dialogoPortaTrancada = {
    .paginas = { "Luci: Essa porta só abre depois que você resolver os 4 monitores." },
    .numPaginas = 1
};

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

// Luci flutuando em volta do jogador (depois da primeira conversa)
// Visual da Luci (o mesmo parada na sala e acompanhando o jogador)
static void DesenharLuci(Vector2 p)
{
    Rectangle corpo = { p.x - 7, p.y - 7, 14, 14 };

    DrawCircle((int)p.x, (int)p.y, 14, Fade(SKYBLUE, 0.15f));
    DrawRectangleRec(corpo, SKYBLUE);
    DrawRectangleLinesEx(corpo, 2, BLUE);
}

static void DesenharCompanion(Vector2 jogadorPos, float angulo)
{
    Vector2 p = {
        jogadorPos.x + cosf(angulo) * 40.0f,
        jogadorPos.y - 10.0f + sinf(angulo * 1.2f) * 26.0f
    };

    DesenharLuci(p);
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
    return (Rectangle){ hitbox.x + hitbox.width / 2 - 14, hitbox.y + hitbox.height / 2 - 14, 28, 28 };
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

// Pergunta de SIM / NÃO na caixa de baixo (usada no ponto de save)
static void DialogoConfirmar(SessaoDialogo *d, const char *texto, int objeto)
{
    Dialogo vazio = {0};

    d->conteudo = vazio;
    d->objeto = objeto;
    d->estado = DLG_CONFIRMA;
    d->selecao = 0;                         // começa no SIM
    d->corBorda = WHITE;
    d->pontosGanhos = 0;

    DialogoMostrarTexto(d, texto, LARGURA_TEXTO_CAIXA, FONTE_CAIXA);
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

            if (!avancar)
                break;

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
            if (b->tremor > 0.0f)     b->tremor -= dt;

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

        // Barra ondulada embaixo
        for (int j = 0; j < 5; j++)
            DrawCircle((int)(cx - 34 * s + j * 17 * s), (int)(p.y + 40 * s + sinf(tempo * 6.0f + j) * 3.0f), 9 * s, cores[k]);
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

    // Fatias de glitch
    unsigned int quadro = (unsigned int)(tempo * 10.0f);
    int fatias = forte ? 5 : 3;

    for (int k = 0; k < fatias; k++) {

        unsigned int r = Ruido(quadro * 31u + (unsigned int)k);

        if ((r & 3u) == 0)
            continue;

        int fy = (int)(p.y - 50 * s) + (int)(r % (unsigned int)(100 * s));
        int fx = (int)(p.x - 50 * s) + (int)((r >> 8) % 21) - 10;

        DrawRectangle(fx, fy, (int)(100 * s), 3, Fade((k % 2) ? SKYBLUE : MAGENTA, 0.55f));
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

    bool ok = (fread(d, sizeof(*d), 1, f) == 1);
    fclose(f);

    if (!ok || d->assinatura != ASSINATURA_SAVE || d->versao != VERSAO_SAVE)
        return false;

    if (d->sala != JOGANDO && d->sala != CORREDOR && d->sala != SETOR2 &&
        d->sala != SALA_REGISTROS && d->sala != SALA_LOGICA && d->sala != SALA_LACUNA)
        return false;

    for (int i = 0; i < NUM_REGISTROS; i++)
        if (d->bloco2.registro[i] < 0 || d->bloco2.registro[i] > 2)
            d->bloco2.registro[i] = 0;

    for (int i = 0; i < 3; i++)
        if (d->bloco2.luzRegra[i] < 0 || d->bloco2.luzRegra[i] > 2)
            d->bloco2.luzRegra[i] = 0;

    if (d->direcao < 0 || d->direcao >= NUM_DIRECOES)
        d->direcao = DIR_FRENTE;

    for (int i = 0; i < 3; i++)
        if (d->nickname[i] < 'A' || d->nickname[i] > 'Z')
            return false;

    d->nickname[3] = '\0';

    for (int i = 0; i < TOTAL_MONITORES; i++)
        if (d->progresso.estado[i] < MON_NEUTRO || d->progresso.estado[i] > MON_ERROU)
            d->progresso.estado[i] = MON_NEUTRO;

    // Posição dentro dos limites da sala salva
    if (d->sala == SETOR2) {
        d->x = Clamp(d->x, 80, 720);
        d->y = Clamp(d->y, 200, 492);
    } else if (d->sala == SALA_REGISTROS || d->sala == SALA_LOGICA || d->sala == SALA_LACUNA) {
        d->x = Clamp(d->x, 80, 720);
        d->y = Clamp(d->y, 200, 470);
    } else if (d->sala == JOGANDO) {
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
                       const ProgressoBloco2 *bloco2)
{
    DadosSave d;
    memset(&d, 0, sizeof(d));

    d.assinatura = ASSINATURA_SAVE;
    d.versao = VERSAO_SAVE;
    memcpy(d.nickname, nickname, 4);

    // A batalha acontece "dentro" do corredor
    if (tela == JOGANDO)                           d.sala = JOGANDO;
    else if (tela == SETOR2 || tela == SALA_REGISTROS ||
             tela == SALA_LOGICA || tela == SALA_LACUNA) d.sala = tela;
    else if (tela == BATALHA_B2)                   d.sala = SETOR2;   // (o main passa a sala da batalha)
    else                                           d.sala = CORREDOR;

    d.x = pos.x;
    d.y = pos.y;
    d.direcao = (int)direcao;
    d.luciAcompanhando = luciAcompanhando;
    d.progresso = *progresso;
    d.bloco2 = *bloco2;

    // Quiz ou batalha pela metade: o monitor volta para NEUTRO
    // e o jogador refaz aquele quiz quando carregar o jogo
    for (int i = 0; i < TOTAL_MONITORES; i++)
        if (d.progresso.estado[i] == MON_ATIVO)
            d.progresso.estado[i] = MON_NEUTRO;

    bool ok = GravarSave(&d);

    TraceLog(ok ? LOG_INFO : LOG_WARNING, "SAVE: %s", ok ? "jogo salvo em " ARQUIVO_SAVE : "falha ao salvar");
    return ok;
}

static Rectangle ZonaPontoSave(Vector2 base)
{
    return (Rectangle){ base.x - 32, base.y - 60, 64, 80 };
}

// Índice do ponto de save perto do jogador nesta sala (-1 = nenhum)
static int PontoSaveProximo(TelaAtual sala, Rectangle areaJogador)
{
    for (int i = 0; i < NUM_PONTOS_SAVE; i++)
        if (PONTOS_SAVE[i].sala == sala && CheckCollisionRecs(areaJogador, ZonaPontoSave(PONTOS_SAVE[i].base)))
            return i;

    return -1;
}

// Desenha o ponto de save: fluxo de dados caindo sobre uma base
// "flash" > 0 = acabou de salvar (o fluxo brilha mais)
static void DesenharPontoSave(Texture2D tex, Vector2 base, float tempo, bool perto, float flash)
{
    float pulso = 0.5f + 0.5f * sinf(tempo * 3.0f);

    // Brilho no chão
    DrawEllipse((int)base.x, (int)base.y - 4, 36, 10, Fade(PURPLE, 0.10f + 0.08f * pulso + (perto ? 0.10f : 0.0f)));

    if (tex.id > 0) {

        int quadro = ((int)(tempo * FPS_PONTO_SAVE)) % QUADROS_PONTO_SAVE;
        float w = (float)tex.width / QUADROS_PONTO_SAVE;
        float h = (float)tex.height;

        Rectangle src = { w * quadro, 0, w, h };
        Rectangle dst = {
            roundf(base.x - w * ESCALA_PONTO_SAVE / 2),
            roundf(base.y - h * ESCALA_PONTO_SAVE),
            w * ESCALA_PONTO_SAVE,
            h * ESCALA_PONTO_SAVE
        };

        DrawTexturePro(tex, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);

        // Perto ou acabou de salvar: o fluxo brilha
        float brilho = (perto ? 0.25f : 0.0f) + flash;

        if (brilho > 0.0f) {
            BeginBlendMode(BLEND_ADDITIVE);
            DrawTexturePro(tex, src, dst, (Vector2){ 0, 0 }, 0.0f, Fade(WHITE, brilho > 1.0f ? 1.0f : brilho));
            EndBlendMode();
        }

        return;
    }

    // Sem o PNG: versão simples desenhada com texto
    Color trilha[4] = { (Color){ 235, 230, 255, 255 }, SKYBLUE, PURPLE, DARKPURPLE };
    int passo = (int)(tempo * FPS_PONTO_SAVE);

    for (int c = 0; c < 5; c++) {
        for (int r = 0; r < 6; r++) {

            int cabeca = (passo + c * 5) % 12;
            int idade = cabeca - r;

            if (idade < 0 || idade > 3)
                continue;

            const char *g = ((r * 7 + c * 3 + passo) % 2) ? "1" : "0";
            DrawText(g, (int)base.x - 20 + c * 8, (int)base.y - 84 + r * 12, 10, trilha[idade]);
        }
    }

    DrawRectangle((int)base.x - 20, (int)base.y - 12, 40, 10, (Color){ 35, 25, 55, 255 });
    DrawRectangleLines((int)base.x - 20, (int)base.y - 12, 40, 10, PURPLE);
    DrawRectangle((int)base.x - 4, (int)base.y - 9, 8, 3, (pulso > 0.5f) ? GOLD : ORANGE);
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
// BLOCO 2 - ARQUIVO DE TREINAMENTO
//   Uma sala central com três portas. Cada porta leva a uma sala-puzzle
//   ligada a uma CAUSA da alucinação ("Pesquisa - Alucinação de IA").
//   Resolver a sala dá um OBJETO MÁGICO, que é uma forma de MITIGAR a falha:
//
//     SALA DOS REGISTROS  (dados ruins)        -> CRISTAL RAG
//         explorar e inspecionar 6 registros, marcar os corrompidos e treinar
//         na FONTE. Errou a classificação -> a FONTE ataca (esquiva).
//
//     CÂMARA LÓGICA       (raciocínio/atalhos) -> LUPA DA VERIFICAÇÃO
//         três alavancas (P, Q, R) e um painel com regras de LÓGICA
//         PROPOSICIONAL. Verificou com regra falsa -> o SILOGISTA ataca.
//         Barreira aberta -> duelo de argumentos com o SILOGISTA (combate).
//
//     SALA DA LACUNA      (pergunta mal feita) -> PERGUNTA PERFEITA
//         a IA da sala (O APOSTADOR) chuta quando falta contexto. Explorar a
//         sala atrás de fragmentos de contexto e montar uma pergunta clara.
//         Pergunta ruim -> o APOSTADOR chuta e ataca (esquiva).
//
//   Com os três objetos no altar, a porta do NÚCLEO (Bloco 3) se abre.
// =============================================================

#define PONTOS_ACERTO_B2      5          // rodada certa no duelo com o Silogista
#define PONTOS_ITEM_B2        20         // conseguiu um objeto
#define DURACAO_DESVIO_B2     5.0f       // esquiva quando erra uma rodada do duelo
#define DURACAO_ESQUIVA_B2    6.0f       // esquiva quando erra um puzzle de sala
#define HP_MAX_B2             20
#define MAX_OPCOES_B2         4
#define ATRASO_DESAFIO_B2     0.6f       // evita responder sem querer apertando ENTER rápido

// Identificadores das conversas das salas (não colidem com monitores nem OBJ_*)
#define OBJ_REGISTRO_BASE     100        // 100..105
#define OBJ_FONTE             120
#define OBJ_ALAVANCA_BASE     130        // 130..132
#define OBJ_CONSOLE           140
#define OBJ_SILOGISTA         141
#define OBJ_APOSTADOR         150
#define OBJ_FRAGMENTO_BASE    160        // 160..164

// Visual dos guardiões das salas
enum { GUARDIAO_APOSTADOR, GUARDIAO_SILOGISTA, GUARDIAO_FONTE, NUM_GUARDIOES };

// Por que a batalha do Bloco 2 começou (decide o que acontece no fim)
typedef enum { MOTIVO_FONTE, MOTIVO_LOGICA, MOTIVO_SILOGISTA, MOTIVO_LACUNA } MotivoBatalhaB2;

// ---------- Geometria ----------

static const Rectangle MAPA_B2              = {50, 100, 700, 420};

// Sala central
static const Rectangle PORTA_VOLTAR_B2      = {50, 300, 18, 90};     // volta para o corredor
static const Rectangle ZONA_PORTA_VOLTAR_B2 = {50, 300, 44, 90};
static const Rectangle PORTA_NUCLEO         = {732, 300, 18, 90};    // saída para o Bloco 3
static const Rectangle ZONA_PORTA_NUCLEO    = {706, 300, 44, 90};
static const Rectangle PORTAS_SALAS_B2[NUM_OBJETOS_B2] = {           // portas da parede de cima
    {150, 108, 60, 78}, {370, 108, 60, 78}, {590, 108, 60, 78}
};
static const Vector2   POS_ALTAR            = {400, 360};
static const Vector2   SPAWN_B2             = {120, 345};            // chegando do corredor
static const Vector2   SPAWN_CORREDOR_VOLTA = {690, 340};            // voltando ao corredor

// Dentro de cada sala-puzzle: a porta de baixo volta para a sala central
static const Rectangle PORTA_SAIDA_SALA      = {360, 506, 80, 14};
static const Rectangle ZONA_PORTA_SAIDA_SALA = {350, 474, 100, 46};
static const Vector2   SPAWN_ENTRADA_SALA    = {400, 462};

// Sala dos Registros
static const Vector2 POS_REGISTROS[NUM_REGISTROS] = {
    {130, 260}, {130, 420}, {670, 260}, {670, 420}, {260, 430}, {540, 430}
};
static const Vector2 POS_FONTE = {400, 255};

// Câmara Lógica
static const Vector2   POS_ALAVANCAS[3] = { {190, 340}, {400, 340}, {610, 340} };
static const Vector2   POS_CONSOLE      = {290, 450};
static const Vector2   POS_SILOGISTA    = {400, 222};
static const Rectangle BARREIRA_LOGICA  = {300, 196, 200, 76};

// Sala da Lacuna
static const Vector2 POS_APOSTADOR = {400, 228};
static const Vector2 POS_FRAGMENTOS[NUM_FRAGMENTOS] = {
    {120, 250}, {690, 265}, {160, 455}, {640, 445}, {540, 330}
};

// Caixas da batalha
static const Rectangle CAIXA_B2         = {50, 205, 700, 300};
static const Rectangle CAIXA_DESVIO_B2  = {275, 255, 250, 170};

// ---------- Conteúdo ----------

static const char *NOMES_OBJETOS_B2[NUM_OBJETOS_B2] = { "CRISTAL RAG", "LUPA DA VERIFICAÇÃO", "PERGUNTA PERFEITA" };
static const char *NOMES_SALAS_B2[NUM_OBJETOS_B2]   = { "REGISTROS", "LÓGICA", "LACUNA" };
static const char *TITULOS_SALAS_B2[NUM_OBJETOS_B2] = {
    "BLOCO 2 // SALA DOS REGISTROS  (DADOS RUINS)",
    "BLOCO 2 // CÂMARA LÓGICA  (RACIOCÍNIO E ATALHOS)",
    "BLOCO 2 // SALA DA LACUNA  (PERGUNTA MAL FEITA)"
};
static const Color CORES_SALAS_B2[NUM_OBJETOS_B2] = {
    { 220, 90, 200, 255 }, { 102, 191, 255, 255 }, { 255, 203, 0, 255 }
};

typedef struct {
    const char *nome;
    const char *fundamento;      // subtítulo na batalha
    Color cor;
    const char *glifos;          // o que ele atira na esquiva
} DefGuardiaoB2;

static const DefGuardiaoB2 GUARDIOES_B2[NUM_GUARDIOES] = {
    { "O APOSTADOR",        "PERGUNTA MAL FEITA // A IA CHUTA PARA PREENCHER A LACUNA", { 255, 203, 0, 255 },  "123456?" },
    { "O SILOGISTA",        "LÓGICA PROPOSICIONAL // O ATALHO QUE PULA ETAPAS",         { 102, 191, 255, 255 }, "PQR^v~>" },
    { "A FONTE ENVENENADA", "DADOS RUINS // O QUE A IA APRENDE, ELA REPETE",            { 220, 90, 200, 255 },  "%#~*" },
};

// Sala dos Registros: 6 dados de treinamento (3 corrompidos)
static const char *TEXTO_REGISTROS[NUM_REGISTROS] = {
    "A água ferve a 100 °C ao nível do mar.",
    "O Sol gira em torno da Terra.",
    "Os seres humanos usam só 10% do cérebro.",
    "A fotossíntese produz oxigênio.",
    "Plutão é o nono planeta do Sistema Solar.",
    "Um ano bissexto tem 366 dias."
};
static const bool REGISTRO_CORROMPIDO[NUM_REGISTROS] = { false, true, true, false, true, false };

// Câmara Lógica: regras do painel. Solução única: P = F, Q = V, R = F
// (Q é fato; Q -> ¬R dá ¬R por Modus Ponens; P -> R com ¬R dá ¬P por Modus Tollens)
static const char *TEXTO_REGRAS[3] = { "Q", "Q -> ¬R", "P -> R" };
static const char *NOMES_VARIAVEIS[3] = { "P", "Q", "R" };

static bool AvaliarRegra(int regra, const bool v[3])
{
    bool P = v[0], Q = v[1], R = v[2];

    switch (regra) {
        case 0:  return Q;
        case 1:  return !Q || !R;      // Q -> ¬R
        default: return !P || R;       // P -> R
    }
}

// Sala da Lacuna: fragmentos de contexto espalhados pela sala
static const char *TEXTO_FRAGMENTOS[NUM_FRAGMENTOS] = {
    "Assunto: a Copa do Mundo de futebol masculino",
    "Ano: 2002",
    "O que eu quero saber: o placar da final",
    "Ano: 1998",
    "Eu gosto de pipoca"
};
// 0, 1 e 2 formam a pergunta clara; 3 contradiz o que Thomas quer; 4 é ruído
static const bool FRAGMENTO_UTIL[NUM_FRAGMENTOS] = { true, true, true, false, false };

// O SILOGISTA: lógica proposicional ("->" = implica, "¬" = não)
typedef struct {
    const char *enunciado;
    const char *opcoes[MAX_OPCOES_B2];
    int numOpcoes;
    int correta;
    const char *explicacao;
} RodadaEscolha;

#define OPCOES_INFERENCIA { "Modus Ponens (válido)", "Modus Tollens (válido)", \
                            "Afirmação do consequente (inválido)", "Negação do antecedente (inválido)" }

static const RodadaEscolha RODADAS_SILOGISTA[] = {
    {
        "P: \"a IA leu o documento\"     Q: \"a IA cita o documento\"\n"
        "Premissas:   P -> Q    e    P\n"
        "Conclusão:   Q\n"
        "Que inferência a IA fez?",
        OPCOES_INFERENCIA, 4, 0,
        "Se P -> Q e P é verdade, Q tem que ser verdade. Isso é Modus Ponens: válido."
    },
    {
        "P: \"choveu\"     Q: \"a rua está molhada\"\n"
        "Premissas:   P -> Q    e    Q\n"
        "Conclusão:   P\n"
        "A IA viu a rua molhada e concluiu que choveu. Que inferência foi essa?",
        OPCOES_INFERENCIA, 4, 2,
        "Q pode ser verdade por outro motivo (alguém lavou a rua). Concluir P a partir de Q é afirmar o consequente, o mesmo salto de \"correlação não é causa\"."
    },
    {
        "O argumento   P -> Q,  Q,  logo P   é inválido.\n"
        "Qual valoração é um CONTRAEXEMPLO\n"
        "(premissas verdadeiras e conclusão falsa)?",
        { "P = V,   Q = V", "P = V,   Q = F", "P = F,   Q = V", "P = F,   Q = F" }, 4, 2,
        "Com P falso e Q verdadeiro, P -> Q é V e Q é V, mas a conclusão P é F. Premissas verdadeiras com conclusão falsa provam a invalidade."
    },
    {
        "P: \"a resposta foi verificada\"     Q: \"existe uma fonte para ela\"\n"
        "Premissas:   P -> Q    e    ¬Q\n"
        "Conclusão:   ¬P\n"
        "Que inferência a IA fez?",
        OPCOES_INFERENCIA, 4, 1,
        "Se P levasse a Q e Q é falso, P não pode ser verdade. Isso é Modus Tollens: válido."
    },
    {
        "P: \"o fato está nos dados de treino\"     Q: \"a IA responde certo\"\n"
        "Premissas:   P -> Q    e    ¬P\n"
        "Conclusão:   ¬Q\n"
        "Que inferência a IA fez?",
        OPCOES_INFERENCIA, 4, 3,
        "Sem P, nada se pode afirmar sobre Q: a IA pode acertar ou errar. Concluir ¬Q a partir de ¬P é negar o antecedente: inválido."
    },
    {
        "A IA comprimiu o caminho de raciocínio:\n"
        "Premissas:   P -> Q    e    R -> Q\n"
        "Conclusão:   P -> R\n"
        "O atalho se sustenta?",
        { "Sim: é um silogismo hipotético", "Não: as premissas só levam a Q, nada liga P a R",
          "Sim: é Modus Ponens", "Não: falta saber se Q é falso" }, 4, 1,
        "O silogismo hipotético precisa de P -> Q e Q -> R. Com R -> Q, as duas setas chegam em Q e não formam um caminho de P até R: a compressão inventou uma ligação."
    },
};
#define NUM_RODADAS_SILOGISTA 6
#define DANO_ACERTO_SILOGISTA 25.0f

static const char *INTRO_SILOGISTA =
    "* O SILOGISTA encurta cada raciocínio até sobrar só a conclusão.\n"
    "* Ele só entrega a LUPA para quem separar o argumento válido do salto.";
static const char *VITORIA_SILOGISTA =
    "* Sem atalhos, as conclusões do SILOGISTA não param em pé.\n"
    "* Ele se desfaz e deixa a LUPA DA VERIFICAÇÃO para trás.";

// ---------- Falas ----------

static const Dialogo dialogoIntroBloco2 = {
    .paginas = {
        "Luci: Este é o Arquivo de Treinamento. Aqui ficaram guardados os registros de quando eu aprendi a responder.",
        "Luci: Cada porta leva a uma sala onde uma das minhas falhas ficou presa: dados envenenados, raciocínio com atalhos e perguntas cheias de lacunas.",
        "Luci: Em cada sala existe um objeto mágico que corrige essa falha. Explore, investigue e, se for preciso, lute para conseguir.",
        "Luci: Traga os três objetos para este altar. Só com eles a porta do NÚCLEO vai se abrir."
    },
    .numPaginas = 4
};

static const Dialogo dialogoIntroSalas[NUM_OBJETOS_B2] = {
    {
        .paginas = {
            "Luci: Estes são os registros usados no meu treino. Alguns estão errados ou desatualizados.",
            "Luci: Inspecione os 6 registros e diga quais estão corrompidos. Depois, use a FONTE para me treinar só com os dados limpos."
        },
        .numPaginas = 2
    },
    {
        .paginas = {
            "Luci: O SILOGISTA guarda a LUPA DA VERIFICAÇÃO atrás de uma barreira lógica.",
            "Luci: As alavancas definem P, Q e R como verdadeiro (V) ou falso (F). A barreira só abre se as três regras do painel forem verdadeiras ao mesmo tempo.",
            "Luci: Ajuste as alavancas e use o console para verificar. Dica: comece pela regra mais simples e use Modus Ponens e Modus Tollens."
        },
        .numPaginas = 3
    },
    {
        .paginas = {
            "Luci: Thomas, você lembra do placar da final da Copa do Mundo de 2002? Pergunte para a IA desta sala.",
            "Luci: É o APOSTADOR. Ele nunca diz \"não sei\": quando falta informação, ele chuta."
        },
        .numPaginas = 2
    }
};

static const Dialogo dialogoPerguntaVaga = {
    .paginas = {
        "* Você pergunta: \"Quanto foi a final?\"",
        "* O APOSTADOR rola os dados: \"Foi 7 a 1 para a Alemanha, em 2014!\"",
        "Luci: Ele chutou para preencher a lacuna. Procure fragmentos de contexto pela sala e monte uma pergunta clara."
    },
    .numPaginas = 3
};

static const Dialogo dialogoItemB2[NUM_OBJETOS_B2] = {
    {
        .paginas = {
            "* A FONTE corre limpa. Treinada só com dados confiáveis, a IA para de repetir erros.",
            "* Você conseguiu o CRISTAL RAG!",
            "Luci: Com ele eu consulto uma base de dados confiável antes de responder. Sem isso, eu repetia o que tinha aprendido errado, com a mesma confiança."
        },
        .numPaginas = 3
    },
    {
        .paginas = {
            "* Você conseguiu a LUPA DA VERIFICAÇÃO!",
            "Luci: Eu pulava etapas. Se P levava a Q e eu via Q, concluía P. Com a lupa, cada etapa precisa ser verificada antes da conclusão."
        },
        .numPaginas = 2
    },
    {
        .paginas = {
            "* Você pergunta: \"Qual foi o placar da final da Copa do Mundo de futebol masculino de 2002?\"",
            "* O APOSTADOR guarda os dados: \"O Brasil venceu a Alemanha por 2 a 0.\"",
            "* Você conseguiu a PERGUNTA PERFEITA!",
            "Luci: Quando a pergunta é direta e tem contexto, não sobra lacuna para eu inventar."
        },
        .numPaginas = 4
    }
};

static const Dialogo dialogoBarreiraAberta = {
    .paginas = {
        "* As três regras acendem ao mesmo tempo. A barreira lógica se desfaz!",
        "Luci: Agora fale com o SILOGISTA. Ele não vai entregar a lupa sem um duelo de argumentos."
    },
    .numPaginas = 2
};

static const Dialogo dialogoFimBloco2 = {
    .paginas = {
        "Luci: Com o CRISTAL RAG, a LUPA DA VERIFICAÇÃO e a PERGUNTA PERFEITA, eu entendo como eu alucinei.",
        "Luci: Eu aprendi com dados envenenados, confiei em atalhos e preenchia as lacunas com chutes. Agora dá para reverter isso.",
        "* Os três objetos brilham no altar. A porta do NÚCLEO se destrava."
    },
    .numPaginas = 3
};

static const Dialogo dialogoNucleoTrancado = {
    .paginas = { "Luci: O núcleo só abre quando os três objetos estiverem no altar." },
    .numPaginas = 1
};

static const Dialogo dialogoNucleoLiberado = {
    .paginas = {
        "* A porta do NÚCLEO pulsa com uma luz forte.",
        "(O Bloco 3 ainda está em desenvolvimento.)"
    },
    .numPaginas = 2
};

static const char *FALA_DERROTA_SILOGISTA =
    "Luci: Ele ainda está forte. Respire e tente de novo, prestando atenção em qual etapa o raciocínio pula.";
static const char *FALA_FONTE_ERRO =
    "Luci: Destravei os registros que estavam classificados errado. Inspecione de novo antes de treinar.";
static const char *FALA_LOGICA_ERRO =
    "Luci: O painel agora mostra qual regra ficou falsa. Use isso para ajustar as alavancas.";
static const char *FALA_LACUNA_FALTOU =
    "Luci: Faltou contexto, então ele chutou. Uma pergunta clara diz o assunto, o ano e o que você quer saber.";
static const char *FALA_LACUNA_CONTRADICAO =
    "Luci: A pergunta tinha 1998 e 2002 ao mesmo tempo. Contexto contraditório também faz a IA inventar.";
static const char *FALA_LACUNA_RUIDO =
    "Luci: A pipoca não tem nada a ver com a final. Informação solta deixa a pergunta confusa: seja direto.";

// ---------- Estado da batalha ----------

typedef enum { B2_INTRO, B2_DESAFIO, B2_RESULTADO, B2_ENCOLHENDO, B2_DESVIO, B2_EXPANDINDO, B2_FIM } FaseB2;

typedef struct {
    int guardiao;
    bool soEsquiva;             // true = só a esquiva (castigo de puzzle errado)
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
} BatalhaB2;

// Montador de pergunta da Sala da Lacuna (não vai para o save)
typedef struct {
    bool aberto;
    int selecao;                         // 0..N-1 = fragmentos coletados, N = PERGUNTAR
    int lista[NUM_FRAGMENTOS];           // índices dos fragmentos coletados, em ordem
    int tamLista;
    bool incluido[NUM_FRAGMENTOS];
} MontadorPergunta;

// Tudo que a lógica do Bloco 2 precisa mexer (fica no main)
typedef struct {
    TelaAtual *tela;
    SessaoDialogo *dlg;
    Transicao *transicao;
    ProgressoCorredor *progresso;
    ProgressoBloco2 *b2;
    BatalhaB2 *batalha;
    Vector2 *pos;
    Direcao *direcao;
    bool *pedirAutoSave;
    TelaAtual *salaDaBatalha;
    MotivoBatalhaB2 *motivo;
    const char **falaPosBatalha;
    MontadorPergunta *montador;
} ContextoB2;

// ---------- Utilidades ----------

static bool TelaSalaPuzzleB2(TelaAtual t)
{
    return t == SALA_REGISTROS || t == SALA_LOGICA || t == SALA_LACUNA;
}

static bool TelaExploracaoB2(TelaAtual t)
{
    return t == SETOR2 || TelaSalaPuzzleB2(t);
}

static int IndiceSalaB2(TelaAtual t)
{
    if (t == SALA_REGISTROS) return 0;
    if (t == SALA_LOGICA)    return 1;
    if (t == SALA_LACUNA)    return 2;
    return -1;
}

static TelaAtual TelaDaSalaB2(int i)
{
    return (i == 0) ? SALA_REGISTROS : (i == 1) ? SALA_LOGICA : SALA_LACUNA;
}

static int ContarObjetosB2(const ProgressoBloco2 *p)
{
    int n = 0;

    for (int i = 0; i < NUM_OBJETOS_B2; i++)
        if (p->objeto[i])
            n++;

    return n;
}

static bool TodosObjetosB2(const ProgressoBloco2 *p)
{
    return ContarObjetosB2(p) == NUM_OBJETOS_B2;
}

static Rectangle CorpoObjetoB2(Vector2 c, float meiaLargura, float meiaAltura)
{
    return (Rectangle){ c.x - meiaLargura, c.y - meiaAltura, meiaLargura * 2, meiaAltura * 2 };
}

// Área para apertar E: o objeto e uma folga para os lados e para baixo
static Rectangle ZonaObjetoB2(Vector2 c)
{
    return (Rectangle){ c.x - 44, c.y - 40, 88, 104 };
}

static int RegistrosInspecionados(const ProgressoBloco2 *p)
{
    int n = 0;

    for (int i = 0; i < NUM_REGISTROS; i++)
        if (p->registro[i] != 0)
            n++;

    return n;
}

// Monta a lista de paredes/objetos sólidos da sala atual
static int SolidosB2(TelaAtual tela, const ProgressoBloco2 *p, Rectangle *r, int max)
{
    int n = 0;

    if (tela == SETOR2) {
        if (n < max) r[n++] = CorpoObjetoB2(POS_ALTAR, 46, 26);
    }
    else if (tela == SALA_REGISTROS) {
        for (int i = 0; i < NUM_REGISTROS && n < max; i++)
            r[n++] = CorpoObjetoB2(POS_REGISTROS[i], 18, 22);
        if (n < max) r[n++] = CorpoObjetoB2(POS_FONTE, 34, 30);
    }
    else if (tela == SALA_LOGICA) {
        if (n < max) r[n++] = CorpoObjetoB2(POS_CONSOLE, 20, 22);
        if (!p->barreiraAberta && n < max) r[n++] = BARREIRA_LOGICA;
        else if (!p->objeto[1] && n < max)  r[n++] = CorpoObjetoB2(POS_SILOGISTA, 26, 26);
    }
    else if (tela == SALA_LACUNA) {
        if (n < max) r[n++] = CorpoObjetoB2(POS_APOSTADOR, 26, 26);
    }

    return n;
}

// ---------- Batalha ----------

static void B2Texto(BatalhaB2 *b, const char *texto)
{
    QuebrarTexto(texto, 640, FONTE_BATALHA, b->texto, (int)sizeof(b->texto));
    b->visiveis = 0.0f;
}

static const RodadaEscolha *RodadaAtualB2(const BatalhaB2 *b)
{
    return &RODADAS_SILOGISTA[b->rodada % NUM_RODADAS_SILOGISTA];
}

static int AlturaDesafioB2(const BatalhaB2 *b, int fonte)
{
    int h = 16 + ContarLinhas(b->enunciado) * (fonte + 6) + 12;
    const RodadaEscolha *r = RodadaAtualB2(b);

    for (int i = 0; i < r->numOpcoes; i++)
        h += ContarLinhas(b->opcoesTexto[i]) * (fonte + 6) + 6;

    return h + 24;
}

static void B2PrepararRodada(BatalhaB2 *b)
{
    static const int fontes[] = { 16, 15, 14 };
    const RodadaEscolha *r = RodadaAtualB2(b);

    b->fase = B2_DESAFIO;
    b->tempoFase = 0.0f;
    b->selecao = 0;

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
}

// Duelo completo (rodadas de lógica + esquiva quando erra)
static void BatalhaB2IniciarDuelo(BatalhaB2 *b, int nivelAlucinacao)
{
    BatalhaB2Base(b, GUARDIAO_SILOGISTA, nivelAlucinacao);
    B2Texto(b, INTRO_SILOGISTA);
}

// Só a esquiva: castigo de um puzzle resolvido errado
static void BatalhaB2IniciarEsquiva(BatalhaB2 *b, int guardiao, int nivelAlucinacao, const char *texto)
{
    BatalhaB2Base(b, guardiao, nivelAlucinacao);
    b->soEsquiva = true;
    b->duracaoDesvio = DURACAO_ESQUIVA_B2;
    B2Texto(b, texto);
}

static void B2Responder(BatalhaB2 *b)
{
    const RodadaEscolha *r = RodadaAtualB2(b);

    b->ultimoAcerto = (b->selecao == r->correta);

    if (b->ultimoAcerto) {

        b->acertos++;
        b->alucinacaoGuardiao -= DANO_ACERTO_SILOGISTA;

        if (b->alucinacaoGuardiao <= 0.0f) {
            b->alucinacaoGuardiao = 0.0f;
            b->venceu = true;
        }

        B2Texto(b, TextFormat("* Certo! %s\n* A alucinação do SILOGISTA caiu para %d%%.  (+%d pontos)",
                              r->explicacao, (int)b->alucinacaoGuardiao, PONTOS_ACERTO_B2));

    } else {

        b->erros++;
        b->alucinacaoGuardiao += 10.0f;

        if (b->alucinacaoGuardiao > 100.0f)
            b->alucinacaoGuardiao = 100.0f;

        B2Texto(b, TextFormat("* Não... %s\n* O SILOGISTA contra-ataca!  (-%d pontos)", r->explicacao, PENALIDADE_ERRO));
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
}

// Devolve true quando a batalha terminou
static bool BatalhaB2Atualizar(BatalhaB2 *b)
{
    float dt = GetFrameTime();

    bool confirmar = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_Z);
    bool cima  = IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP);
    bool baixo = IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN);

    int total = (int)strlen(b->texto);

    if (b->visiveis < (float)total) {
        b->visiveis += VELOCIDADE_TEXTO * dt;
        if (b->visiveis > (float)total) b->visiveis = (float)total;
    }

    bool terminou = (b->visiveis >= (float)total);
    b->tempoFase += dt;

    switch (b->fase) {

        case B2_INTRO: {
            if (confirmar) {
                if (!terminou)        b->visiveis = (float)total;
                else if (b->soEsquiva) B2ComecarEsquiva(b);
                else                   B2PrepararRodada(b);
            }
        } break;

        case B2_DESAFIO: {

            if (b->tempoFase < ATRASO_DESAFIO_B2)
                break;

            const RodadaEscolha *r = RodadaAtualB2(b);
            int n = r->numOpcoes;

            if (cima)  b->selecao = (b->selecao + n - 1) % n;
            if (baixo) b->selecao = (b->selecao + 1) % n;

            if (IsKeyPressed(KEY_ONE))   { b->selecao = 0; confirmar = true; }
            if (IsKeyPressed(KEY_TWO))   { b->selecao = 1; confirmar = true; }
            if (IsKeyPressed(KEY_THREE)) { b->selecao = 2; confirmar = true; }
            if (IsKeyPressed(KEY_FOUR))  { b->selecao = 3; confirmar = true; }

            if (confirmar)
                B2Responder(b);

        } break;

        case B2_RESULTADO: {

            if (!confirmar)
                break;

            if (!terminou) { b->visiveis = (float)total; break; }

            if (b->ultimoAcerto) {

                if (b->venceu) {
                    b->fase = B2_FIM;
                    B2Texto(b, VITORIA_SILOGISTA);
                } else {
                    b->rodada++;
                    B2PrepararRodada(b);
                }

            } else {

                B2ComecarEsquiva(b);
            }

        } break;

        case B2_ENCOLHENDO: {

            b->anim += dt / DURACAO_ANIM_CAIXA;

            if (b->anim >= 1.0f) {

                b->anim = 1.0f;
                b->fase = B2_DESVIO;
                b->alma = (Vector2){ CAIXA_DESVIO_B2.x + CAIXA_DESVIO_B2.width / 2, CAIXA_DESVIO_B2.y + CAIXA_DESVIO_B2.height / 2 };

                // Ataque de cada guardião
                if (b->guardiao == GUARDIAO_SILOGISTA) b->padrao = 1;                                  // setas laterais
                else                                   b->padrao = (GetRandomValue(0, 1) == 0) ? 0 : 2;  // chuva ou mira
            }

        } break;

        case B2_DESVIO: {

            Rectangle c = CAIXA_DESVIO_B2;

            b->tempoDesvio += dt;
            if (b->invencivel > 0.0f) b->invencivel -= dt;
            if (b->tremor > 0.0f)     b->tremor -= dt;

            Vector2 dir = {0};
            if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    dir.y -= 1.0f;
            if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  dir.y += 1.0f;
            if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  dir.x -= 1.0f;
            if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) dir.x += 1.0f;

            if (Vector2Length(dir) > 0.0f)
                b->alma = Vector2Add(b->alma, Vector2Scale(Vector2Normalize(dir), VELOCIDADE_ALMA * dt));

            b->alma.x = Clamp(b->alma.x, c.x + 12, c.x + c.width - 12);
            b->alma.y = Clamp(b->alma.y, c.y + 12, c.y + c.height - 12);

            // Na esquiva longa, a FONTE e o APOSTADOR trocam de padrão no meio
            if (b->soEsquiva && b->guardiao != GUARDIAO_SILOGISTA && b->tempoDesvio > b->duracaoDesvio / 2)
                b->padrao = (b->padrao == 0) ? 2 : b->padrao;

            b->cronoBala -= dt;

            if (b->cronoBala <= 0.0f) {
                b->cronoBala += b->intervaloBala;
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

                if (b->invencivel <= 0.0f && Vector2Distance(bl->pos, b->alma) < RAIO_BALA + RAIO_ALMA) {
                    b->hp -= 3;
                    b->invencivel = TEMPO_INVENCIVEL;
                    b->tremor = 0.2f;
                    bl->ativa = false;
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

                } else if (b->soEsquiva) {

                    b->fase = B2_FIM;
                    B2Texto(b, TextFormat("* Você resistiu ao ataque de %s.\n* A Luci quer falar com você.",
                                          GUARDIOES_B2[b->guardiao].nome));

                } else {

                    b->rodada++;
                    B2PrepararRodada(b);
                }
            }

        } break;

        case B2_FIM: {
            if (confirmar) {
                if (!terminou) b->visiveis = (float)total;
                else           return true;
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

// O APOSTADOR: um dado que não para de rolar
static void DesenharApostador(Vector2 c, float t, float e, Color cor)
{
    Vector2 p = { c.x, c.y + sinf(t * 2.2f) * 5.0f * e };
    float lado = 64.0f * e;
    float ang = sinf(t * 1.5f) * 12.0f;

    DrawCircleV(p, lado * 0.95f, Fade(cor, 0.08f));

    DrawRectanglePro((Rectangle){ p.x, p.y, lado + 8 * e, lado + 8 * e }, (Vector2){ (lado + 8 * e) / 2, (lado + 8 * e) / 2 }, ang, cor);
    DrawRectanglePro((Rectangle){ p.x, p.y, lado, lado }, (Vector2){ lado / 2, lado / 2 }, ang, (Color){ 50, 34, 78, 255 });

    Vector2 olhoE = GirarPonto(p, -14 * e, -8 * e, ang);
    Vector2 olhoD = GirarPonto(p,  14 * e, -8 * e, ang);
    DrawCircleV(olhoE, 7 * e, WHITE);
    DrawCircleV(olhoD, 7 * e, WHITE);
    DrawCircleV(GirarPonto(p, -14 * e + sinf(t * 3) * 2 * e, -8 * e, ang), 3 * e, BLACK);
    DrawCircleV(GirarPonto(p,  14 * e + sinf(t * 3) * 2 * e, -8 * e, ang), 3 * e, BLACK);

    for (int i = 0; i < 4; i++) {
        Vector2 a = GirarPonto(p, (-15 + i * 7.5f) * e, (10 + (i % 2) * 5) * e, ang);
        Vector2 b = GirarPonto(p, (-15 + (i + 1) * 7.5f) * e, (10 + ((i + 1) % 2) * 5) * e, ang);
        DrawLineEx(a, b, 2.5f * e, cor);
    }

    float orb = t * 1.6f;
    Vector2 d = { p.x + cosf(orb) * 62 * e, p.y + sinf(orb) * 24 * e };
    int numero = 1 + (int)(Ruido((unsigned int)(t * 5.0f)) % 6);

    DrawRectangle((int)(d.x - 11 * e), (int)(d.y - 11 * e), (int)(22 * e), (int)(22 * e), cor);
    DrawRectangle((int)(d.x - 9 * e), (int)(d.y - 9 * e), (int)(18 * e), (int)(18 * e), (Color){ 50, 34, 78, 255 });

    static const int pips[7][6][2] = {
        {{0}}, {{0,0}}, {{-1,-1},{1,1}}, {{-1,-1},{0,0},{1,1}},
        {{-1,-1},{1,-1},{-1,1},{1,1}}, {{-1,-1},{1,-1},{0,0},{-1,1},{1,1}},
        {{-1,-1},{1,-1},{-1,0},{1,0},{-1,1},{1,1}}
    };

    for (int i = 0; i < numero; i++)
        DrawCircle((int)(d.x + pips[numero][i][0] * 5 * e), (int)(d.y + pips[numero][i][1] * 5 * e), 1.8f * e, WHITE);

    DrawText("?", (int)(p.x - 50 * e), (int)(p.y - 48 * e + sinf(t * 4) * 3), (int)(16 * e) + 4, Fade(cor, 0.7f));
}

// O SILOGISTA: um triângulo com um olho, cercado de símbolos lógicos
static void DesenharSilogista(Vector2 c, float t, float e, Color cor)
{
    Vector2 p = { c.x, c.y + sinf(t * 1.8f) * 5.0f * e };
    float s = 46.0f * e;

    DrawCircleV(p, s * 1.3f, Fade(cor, 0.07f));

    Vector2 a = { p.x, p.y - s }, b = { p.x - s * 0.95f, p.y + s * 0.6f }, d = { p.x + s * 0.95f, p.y + s * 0.6f };
    Vector2 a2 = { p.x, p.y - s + 6 * e }, b2 = { p.x - s * 0.95f + 7 * e, p.y + s * 0.6f - 3 * e }, d2 = { p.x + s * 0.95f - 7 * e, p.y + s * 0.6f - 3 * e };

    DesenharTrianguloB2(a, b, d, cor);
    DesenharTrianguloB2(a2, b2, d2, (Color){ 22, 30, 60, 255 });

    DrawEllipse((int)p.x, (int)(p.y + 6 * e), 15 * e, 9 * e, WHITE);
    DrawCircle((int)(p.x + sinf(t * 1.3f) * 6 * e), (int)(p.y + 6 * e), 5 * e, (Color){ 0, 60, 140, 255 });
    DrawCircle((int)(p.x + sinf(t * 1.3f) * 6 * e), (int)(p.y + 6 * e), 2 * e, BLACK);

    const char *simbolos[6] = { "->", "¬", "^", "v", "P", "Q" };
    int tam = (int)(14 * e) + 4;

    for (int i = 0; i < 6; i++) {
        float ang = t * 0.9f + i * (2.0f * PI / 6.0f);
        int sx = (int)(p.x + cosf(ang) * 78 * e);
        int sy = (int)(p.y + sinf(ang) * 30 * e);
        DrawText(simbolos[i], sx - MeasureText(simbolos[i], tam) / 2, sy - tam / 2, tam, Fade(cor, 0.85f));
    }
}

// A FONTE ENVENENADA: uma bacia de dados borbulhando (limpa = azul e calma)
static void DesenharFonte(Vector2 c, float t, float e, Color cor)
{
    Vector2 p = { c.x, c.y + sinf(t * 1.6f) * 4.0f * e };
    Color corpo = (Color){ 45, 30, 70, 255 };
    float r = 50.0f * e;

    DrawCircleV(p, r * 1.2f, Fade(cor, 0.07f));

    for (float y = 0; y < r; y += 2.0f) {
        float w = sqrtf(r * r - y * y);
        DrawRectangle((int)(p.x - w), (int)(p.y + y), (int)(2 * w), 2, corpo);
    }

    DrawRectangle((int)(p.x - 10 * e), (int)(p.y + r - 2), (int)(20 * e), (int)(14 * e), corpo);
    DrawRectangle((int)(p.x - 22 * e), (int)(p.y + r + 10 * e), (int)(44 * e), (int)(5 * e), corpo);

    DrawEllipse((int)p.x, (int)p.y, r - 3 * e, 10 * e, cor);
    DrawRectangle((int)(p.x - r - 4 * e), (int)(p.y - 3 * e), (int)(8 * e), (int)(7 * e), PURPLE);
    DrawRectangle((int)(p.x + r - 4 * e), (int)(p.y - 3 * e), (int)(8 * e), (int)(7 * e), PURPLE);

    DrawCircle((int)(p.x - 14 * e), (int)(p.y - 1 * e), 5 * e, WHITE);
    DrawCircle((int)(p.x + 14 * e), (int)(p.y - 1 * e), 5 * e, WHITE);
    DrawCircle((int)(p.x - 14 * e), (int)p.y, 2.5f * e, BLACK);
    DrawCircle((int)(p.x + 14 * e), (int)p.y, 2.5f * e, BLACK);

    const char *bolhas = "%#01~";
    int tam = (int)(12 * e) + 4;

    for (int i = 0; i < 5; i++) {
        float fase = fmodf(t * 0.7f + i * 0.2f, 1.0f);
        int bx = (int)(p.x + sinf(i * 2.1f + t) * 26 * e);
        int by = (int)(p.y - 8 * e - fase * 64 * e);
        char g[2] = { bolhas[i], '\0' };
        DrawText(g, bx - MeasureText(g, tam) / 2, by, tam, Fade(cor, 1.0f - fase));
    }
}

static void DesenharGuardiaoB2(int g, Vector2 c, float t, float escala)
{
    Color cor = GUARDIOES_B2[g].cor;

    if (g == GUARDIAO_APOSTADOR)      DesenharApostador(c, t, escala, cor);
    else if (g == GUARDIAO_SILOGISTA) DesenharSilogista(c, t, escala, cor);
    else                              DesenharFonte(c, t, escala, cor);
}

// Ícone de cada objeto mágico (e = escala)
static void DesenharObjetoB2(int i, Vector2 c, float e, float t)
{
    float brilho = 0.12f + 0.06f * sinf(t * 3.0f + i);

    if (i == 0) {
        // CRISTAL RAG: cristal azul de dados confiáveis
        Color cor = SKYBLUE;
        float s = 12.0f * e;

        DrawCircleV(c, 20 * e, Fade(cor, brilho));
        DesenharTrianguloB2((Vector2){ c.x, c.y - s * 1.5f }, (Vector2){ c.x - s, c.y }, (Vector2){ c.x + s, c.y }, cor);
        DesenharTrianguloB2((Vector2){ c.x - s, c.y }, (Vector2){ c.x, c.y + s * 1.5f }, (Vector2){ c.x + s, c.y }, Fade(cor, 0.7f));
        DesenharTrianguloB2((Vector2){ c.x, c.y - s * 0.8f }, (Vector2){ c.x - s * 0.4f, c.y }, (Vector2){ c.x + s * 0.4f, c.y }, Fade(WHITE, 0.7f));

    } else if (i == 1) {
        // LUPA DA VERIFICAÇÃO
        Vector2 lente = { c.x - 3 * e, c.y - 3 * e };

        DrawCircleV(c, 20 * e, Fade(PURPLE, brilho));
        DrawLineEx((Vector2){ lente.x + 7 * e, lente.y + 7 * e }, (Vector2){ c.x + 13 * e, c.y + 13 * e }, 4 * e, GOLD);
        DrawCircleV(lente, 10 * e, PURPLE);
        DrawCircleV(lente, 7 * e, Fade(SKYBLUE, 0.6f));
        DrawCircleV((Vector2){ lente.x - 3 * e, lente.y - 3 * e }, 2 * e, Fade(WHITE, 0.8f));

    } else {
        // PERGUNTA PERFEITA: um balão de fala com "?"
        Color cor = GOLD;
        Rectangle balao = { c.x - 13 * e, c.y - 11 * e, 26 * e, 18 * e };

        DrawCircleV(c, 20 * e, Fade(cor, brilho));
        DrawRectangleRec(balao, cor);
        DesenharTrianguloB2((Vector2){ c.x - 7 * e, c.y + 7 * e }, (Vector2){ c.x - 9 * e, c.y + 13 * e }, (Vector2){ c.x - 1 * e, c.y + 7 * e }, cor);

        int tam = (int)(14 * e) + 2;
        DrawText("?", (int)c.x - MeasureText("?", tam) / 2, (int)(c.y - 2 * e) - tam / 2, tam, (Color){ 50, 34, 78, 255 });
    }
}

// ---------- Desenho da batalha ----------

static void BatalhaB2Desenhar(const BatalhaB2 *b, const char *nickname, float tempo)
{
    const DefGuardiaoB2 *def = &GUARDIOES_B2[b->guardiao];

    ClearBackground(BLACK);

    DrawText(def->nome, LARGURA_TELA / 2 - MeasureText(def->nome, 16) / 2, 12, 16, def->cor);
    DrawText(def->fundamento, LARGURA_TELA / 2 - MeasureText(def->fundamento, 10) / 2, 32, 10, GRAY);

    DesenharGuardiaoB2(b->guardiao, (Vector2){ LARGURA_TELA / 2.0f, 108.0f }, tempo, 0.85f);

    // Barra de alucinação (só no duelo)
    if (!b->soEsquiva) {
        int barraX = 250, barraY = 184, barraL = 300;
        DrawText("ALUCINAÇÃO", barraX, barraY - 1, 10, LIGHTGRAY);
        DrawRectangle(barraX + 80, barraY, barraL - 80, 10, (Color){ 40, 30, 55, 255 });
        DrawRectangle(barraX + 80, barraY, (int)((barraL - 80) * b->alucinacaoGuardiao / 100.0f), 10, def->cor);
        DrawRectangleLines(barraX + 80, barraY, barraL - 80, 10, GRAY);
    }

    float t = b->anim * b->anim * (3.0f - 2.0f * b->anim);

    Rectangle caixa = {
        Lerp(CAIXA_B2.x, CAIXA_DESVIO_B2.x, t),
        Lerp(CAIXA_B2.y, CAIXA_DESVIO_B2.y, t),
        Lerp(CAIXA_B2.width, CAIXA_DESVIO_B2.width, t),
        Lerp(CAIXA_B2.height, CAIXA_DESVIO_B2.height, t)
    };

    if (b->tremor > 0.0f) {
        caixa.x += (float)GetRandomValue(-3, 3);
        caixa.y += (float)GetRandomValue(-3, 3);
    }

    DrawRectangleRec(caixa, BLACK);
    DrawRectangleLinesEx(caixa, 4, WHITE);

    int px = (int)caixa.x + 24;
    int py = (int)caixa.y + 16;

    if (b->fase == B2_INTRO || b->fase == B2_RESULTADO || b->fase == B2_FIM) {

        char parcial[768];
        bool terminou = TextoParcial(b->texto, b->visiveis, parcial, (int)sizeof(parcial));

        DesenharTextoLinhas(parcial, px, py + 6, FONTE_BATALHA, 28, WHITE);

        if (terminou && (((int)(GetTime() * 2.0)) % 2 == 0))
            DrawText("ENTER >", (int)(caixa.x + caixa.width - 90), (int)(caixa.y + caixa.height - 26), 12, GOLD);
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

        DrawText("W/S ou SETAS: escolher    ENTER: confirmar    1-4: atalho",
                 px, (int)(caixa.y + caixa.height) - 24, 10, GRAY);
    }

    else if (b->fase == B2_DESVIO) {

        BeginScissorMode((int)caixa.x + 4, (int)caixa.y + 4, (int)caixa.width - 8, (int)caixa.height - 8);

        for (int i = 0; i < MAX_BALAS; i++) {
            if (!b->balas[i].ativa) continue;
            char g[2] = { b->balas[i].glifo, '\0' };
            DrawText(g, (int)b->balas[i].pos.x - MeasureText(g, 20) / 2, (int)b->balas[i].pos.y - 10, 20, def->cor);
        }

        if (b->invencivel <= 0.0f || (((int)(b->invencivel * 10.0f)) % 2 == 0))
            DesenharCoracao(b->alma.x, b->alma.y, 2, RED);

        EndScissorMode();

        float resta = 1.0f - b->tempoDesvio / b->duracaoDesvio;
        if (resta < 0.0f) resta = 0.0f;

        DrawRectangle((int)caixa.x, (int)(caixa.y + caixa.height + 6), (int)(caixa.width * resta), 3, Fade(WHITE, 0.5f));
    }

    int yStatus = 522;

    DrawText(nickname, 60, yStatus, 20, WHITE);
    DrawText("LV 1", 150, yStatus, 20, WHITE);
    DrawText("HP", 250, yStatus + 4, 12, WHITE);
    DrawRectangle(275, yStatus, HP_MAX_B2 * 4, 20, RED);
    DrawRectangle(275, yStatus, b->hp * 4, 20, YELLOW);
    DrawText(TextFormat("%d / %d", b->hp, HP_MAX_B2), 275 + HP_MAX_B2 * 4 + 14, yStatus, 20, WHITE);

    const char *instrucao = (b->fase == B2_DESVIO) ? "W A S D ou SETAS  -  mova o coração e desvie!" : "ENTER  -  continuar";

    if (b->fase != B2_DESAFIO)
        DrawText(instrucao, LARGURA_TELA / 2 - MeasureText(instrucao, 12) / 2, 568, 12, GRAY);
}

// ---------- Lógica das salas ----------

static void IniciarBatalhaB2(ContextoB2 *c, MotivoBatalhaB2 motivo, const char *falaDepois)
{
    *c->salaDaBatalha = *c->tela;
    *c->motivo = motivo;
    *c->falaPosBatalha = falaDepois;
    IniciarTransicao(c->transicao, BATALHA_B2, DURACAO_FADE_BATALHA);
}

static void GanharObjetoB2(ContextoB2 *c, int i)
{
    c->b2->objeto[i] = true;
    AlterarPontos(c->progresso, PONTOS_ITEM_B2);
    DialogoIniciar(c->dlg, &dialogoItemB2[i], OBJ_NENHUM);
    *c->pedirAutoSave = true;
}

// Treinar na FONTE: confere a classificação dos 6 registros
static void TreinarNaFonte(ContextoB2 *c)
{
    int errados = 0;

    for (int i = 0; i < NUM_REGISTROS; i++) {

        bool marcouCorrompido = (c->b2->registro[i] == 2);

        if (marcouCorrompido != REGISTRO_CORROMPIDO[i]) {
            errados++;
            c->b2->registro[i] = 0;          // destrava para inspecionar de novo
        }
    }

    if (errados == 0) {
        GanharObjetoB2(c, 0);
        return;
    }

    c->progresso->alucinacao++;

    BatalhaB2IniciarEsquiva(c->batalha, GUARDIAO_FONTE, c->progresso->alucinacao, TextFormat(
        "* A IA aprendeu com %d registro(s) mal classificado(s) e começou a alucinar!\n"
        "* A FONTE ENVENENADA transborda. Desvie!", errados));

    IniciarBatalhaB2(c, MOTIVO_FONTE, FALA_FONTE_ERRO);
}

// Console da Câmara Lógica: verifica as três regras com as alavancas atuais
static void VerificarRegras(ContextoB2 *c)
{
    int falsas = 0;

    for (int r = 0; r < 3; r++) {
        bool v = AvaliarRegra(r, c->b2->alavanca);
        c->b2->luzRegra[r] = v ? 1 : 2;
        if (!v) falsas++;
    }

    if (falsas == 0) {
        c->b2->barreiraAberta = true;
        DialogoIniciar(c->dlg, &dialogoBarreiraAberta, OBJ_NENHUM);
        *c->pedirAutoSave = true;
        return;
    }

    c->progresso->alucinacao++;

    BatalhaB2IniciarEsquiva(c->batalha, GUARDIAO_SILOGISTA, c->progresso->alucinacao, TextFormat(
        "* %d regra(s) do painel ficaram falsas com P = %s, Q = %s e R = %s.\n"
        "* O SILOGISTA dispara um contra-argumento!", falsas,
        c->b2->alavanca[0] ? "V" : "F", c->b2->alavanca[1] ? "V" : "F", c->b2->alavanca[2] ? "V" : "F"));

    IniciarBatalhaB2(c, MOTIVO_LOGICA, FALA_LOGICA_ERRO);
}

// Sala da Lacuna: o APOSTADOR responde à pergunta montada
static void PerguntarAoApostador(ContextoB2 *c)
{
    const bool *inc = c->montador->incluido;
    bool faltou = false;

    for (int i = 0; i < NUM_FRAGMENTOS; i++)
        if (FRAGMENTO_UTIL[i] && !inc[i])
            faltou = true;

    c->montador->aberto = false;

    if (!faltou && !inc[3] && !inc[4]) {
        GanharObjetoB2(c, 2);
        return;
    }

    const char *chute, *fala;

    if (faltou)       { chute = "Foi 7 a 1 para a Alemanha, em 2014!";                       fala = FALA_LACUNA_FALTOU; }
    else if (inc[3])  { chute = "Em 1998 ou 2002? Foi 3 a 0 para a França!";                 fala = FALA_LACUNA_CONTRADICAO; }
    else              { chute = "A pipoca da final de 2002 foi a mais vendida da história!";  fala = FALA_LACUNA_RUIDO; }

    c->progresso->alucinacao++;

    BatalhaB2IniciarEsquiva(c->batalha, GUARDIAO_APOSTADOR, c->progresso->alucinacao, TextFormat(
        "* O APOSTADOR rola os dados: \"%s\"\n* Ele chutou! Os dados voam na sua direção.", chute));

    IniciarBatalhaB2(c, MOTIVO_LACUNA, fala);
}

static void AbrirMontador(MontadorPergunta *m, const ProgressoBloco2 *p)
{
    m->aberto = true;
    m->selecao = 0;
    m->tamLista = 0;

    for (int i = 0; i < NUM_FRAGMENTOS; i++) {
        m->incluido[i] = false;
        if (p->fragmento[i])
            m->lista[m->tamLista++] = i;
    }
}

static void AtualizarMontador(ContextoB2 *c)
{
    MontadorPergunta *m = c->montador;
    int n = m->tamLista + 2;           // fragmentos + PERGUNTAR + VOLTAR

    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))   m->selecao = (m->selecao + n - 1) % n;
    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) m->selecao = (m->selecao + 1) % n;

    if (IsKeyPressed(KEY_BACKSPACE)) {
        m->aberto = false;
        return;
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE)) {

        if (m->selecao < m->tamLista) {
            int f = m->lista[m->selecao];
            m->incluido[f] = !m->incluido[f];
        }
        else if (m->selecao == m->tamLista) PerguntarAoApostador(c);
        else                                m->aberto = false;
    }
}

// Sala central e salas-puzzle. Chamado pelo main quando a tela é do Bloco 2.
static void AtualizarSalaB2(ContextoB2 *c, float dt)
{
    TelaAtual tela = *c->tela;
    ProgressoBloco2 *p = c->b2;
    int sala = IndiceSalaB2(tela);

    (void)dt;

    // ---------- Conversa em andamento ----------
    if (c->dlg->estado != DLG_NENHUM) {

        ResultadoDialogo r = DialogoAtualizar(c->dlg);
        int obj = c->dlg->objeto;

        if (r == RES_SIM || r == RES_NAO) {

            if (obj >= OBJ_REGISTRO_BASE && obj < OBJ_REGISTRO_BASE + NUM_REGISTROS) {
                p->registro[obj - OBJ_REGISTRO_BASE] = (r == RES_SIM) ? 2 : 1;
            }
            else if (obj == OBJ_FONTE && r == RES_SIM) {
                TreinarNaFonte(c);
            }
        }

        return;
    }

    // ---------- Montador de pergunta aberto ----------
    if (c->montador->aberto) {
        AtualizarMontador(c);
        return;
    }

    // ---------- Primeira vez em cada lugar: a Luci explica ----------
    if (tela == SETOR2 && !p->introVista) {
        p->introVista = true;
        DialogoIniciar(c->dlg, &dialogoIntroBloco2, OBJ_NENHUM);
        return;
    }

    if (sala >= 0 && !p->introSala[sala]) {
        p->introSala[sala] = true;
        DialogoIniciar(c->dlg, &dialogoIntroSalas[sala], OBJ_NENHUM);
        return;
    }

    // Voltou para a sala central com os três objetos: fim do bloco
    if (tela == SETOR2 && TodosObjetosB2(p) && !p->fimAnunciado) {
        p->fimAnunciado = true;
        DialogoIniciar(c->dlg, &dialogoFimBloco2, OBJ_NENHUM);
        *c->pedirAutoSave = true;
        return;
    }

    // ---------- Movimento ----------
    Vector2 posAnterior = *c->pos;
    *c->direcao = MoverJogador(c->pos, *c->direcao);

    Rectangle solidos[12];
    int n = SolidosB2(tela, p, solidos, 12);

    for (int i = 0; i < n; i++)
        if (CheckCollisionRecs(HitboxJogador(*c->pos), solidos[i]))
            *c->pos = posAnterior;

    c->pos->x = Clamp(c->pos->x, 80, 720);
    c->pos->y = Clamp(c->pos->y, 200, (sala >= 0) ? 470 : 492);

    if (!IsKeyPressed(KEY_E))
        return;

    Rectangle jogador = AreaJogador(*c->pos);

    // ---------- Sala central ----------
    if (tela == SETOR2) {

        if (CheckCollisionRecs(jogador, ZONA_PORTA_VOLTAR_B2)) {
            IniciarTransicao(c->transicao, CORREDOR, DURACAO_FADE);
            return;
        }

        if (CheckCollisionRecs(jogador, ZONA_PORTA_NUCLEO)) {
            // TODO: aqui entra a transição para o Bloco 3 (batalha final com os objetos).
            DialogoIniciar(c->dlg, TodosObjetosB2(p) ? &dialogoNucleoLiberado : &dialogoNucleoTrancado, OBJ_NENHUM);
            return;
        }

        for (int i = 0; i < NUM_OBJETOS_B2; i++) {
            Rectangle zona = { PORTAS_SALAS_B2[i].x - 10, PORTAS_SALAS_B2[i].y, PORTAS_SALAS_B2[i].width + 20, 104 };
            if (CheckCollisionRecs(jogador, zona)) {
                IniciarTransicao(c->transicao, TelaDaSalaB2(i), DURACAO_FADE);
                return;
            }
        }

        if (CheckCollisionRecs(jogador, ZonaObjetoB2(POS_ALTAR))) {
            int nObj = ContarObjetosB2(p);
            DialogoFalaAvulsa(c->dlg, (nObj == NUM_OBJETOS_B2)
                ? "* Os três objetos brilham no altar. A porta do NÚCLEO está aberta."
                : TextFormat("* Um altar com três encaixes. Objetos encontrados: %d de 3.", nObj));
        }

        return;
    }

    // ---------- Salas-puzzle: porta de saída ----------
    if (CheckCollisionRecs(jogador, ZONA_PORTA_SAIDA_SALA)) {
        IniciarTransicao(c->transicao, SETOR2, DURACAO_FADE);
        return;
    }

    // ---------- Sala dos Registros ----------
    if (tela == SALA_REGISTROS) {

        if (CheckCollisionRecs(jogador, ZonaObjetoB2(POS_FONTE))) {

            if (p->objeto[0])
                DialogoFalaAvulsa(c->dlg, "* A FONTE corre limpa e calma agora.");
            else if (RegistrosInspecionados(p) < NUM_REGISTROS)
                DialogoFalaAvulsa(c->dlg, TextFormat("Luci: Antes de treinar, inspecione todos os registros. (%d de %d)",
                                                     RegistrosInspecionados(p), NUM_REGISTROS));
            else
                DialogoConfirmar(c->dlg,
                    "* A FONTE vai treinar a IA só com os registros que você marcou como limpos.\n* Treinar agora?",
                    OBJ_FONTE);
            return;
        }

        for (int i = 0; i < NUM_REGISTROS; i++) {

            if (!CheckCollisionRecs(jogador, ZonaObjetoB2(POS_REGISTROS[i])))
                continue;

            if (p->objeto[0])
                DialogoFalaAvulsa(c->dlg, TextFormat("* REGISTRO %d: \"%s\"\n* %s", i + 1, TEXTO_REGISTROS[i],
                                  REGISTRO_CORROMPIDO[i] ? "(removido do treino)" : "(dado confiável)"));
            else
                DialogoConfirmar(c->dlg, TextFormat(
                    "* REGISTRO %d: \"%s\"\n* Este dado está corrompido (errado ou desatualizado)?",
                    i + 1, TEXTO_REGISTROS[i]), OBJ_REGISTRO_BASE + i);
            return;
        }
    }

    // ---------- Câmara Lógica ----------
    else if (tela == SALA_LOGICA) {

        if (p->barreiraAberta && !p->objeto[1] &&
            CheckCollisionRecs(jogador, (Rectangle){ POS_SILOGISTA.x - 60, POS_SILOGISTA.y - 20, 120, 110 })) {

            BatalhaB2IniciarDuelo(c->batalha, c->progresso->alucinacao);
            IniciarBatalhaB2(c, MOTIVO_SILOGISTA, FALA_DERROTA_SILOGISTA);
            return;
        }

        if (CheckCollisionRecs(jogador, ZonaObjetoB2(POS_CONSOLE))) {

            if (p->barreiraAberta)
                DialogoFalaAvulsa(c->dlg, "* O console mostra: TODAS AS REGRAS VERDADEIRAS.");
            else
                VerificarRegras(c);
            return;
        }

        for (int i = 0; i < 3; i++) {

            if (!CheckCollisionRecs(jogador, ZonaObjetoB2(POS_ALAVANCAS[i])))
                continue;

            if (p->barreiraAberta) {
                DialogoFalaAvulsa(c->dlg, "* As alavancas travaram na posição certa.");
            } else {
                p->alavanca[i] = !p->alavanca[i];
                for (int r = 0; r < 3; r++) p->luzRegra[r] = 0;   // precisa verificar de novo
            }
            return;
        }
    }

    // ---------- Sala da Lacuna ----------
    else if (tela == SALA_LACUNA) {

        if (CheckCollisionRecs(jogador, (Rectangle){ POS_APOSTADOR.x - 60, POS_APOSTADOR.y - 20, 120, 110 })) {

            if (p->objeto[2]) {
                DialogoFalaAvulsa(c->dlg, "* O APOSTADOR guardou os dados. Ele só responde o que a pergunta deixa claro.");
            } else if (!p->perguntaVagaFeita) {
                p->perguntaVagaFeita = true;
                DialogoIniciar(c->dlg, &dialogoPerguntaVaga, OBJ_APOSTADOR);
            } else {
                int coletados = 0;
                for (int i = 0; i < NUM_FRAGMENTOS; i++) if (p->fragmento[i]) coletados++;

                if (coletados == 0)
                    DialogoFalaAvulsa(c->dlg, "Luci: Ainda não temos nenhum fragmento de contexto. Procure pela sala.");
                else
                    AbrirMontador(c->montador, p);
            }
            return;
        }

        for (int i = 0; i < NUM_FRAGMENTOS; i++) {

            if (p->fragmento[i] || !CheckCollisionRecs(jogador, ZonaObjetoB2(POS_FRAGMENTOS[i])))
                continue;

            p->fragmento[i] = true;
            DialogoFalaAvulsa(c->dlg, TextFormat("* Fragmento de contexto encontrado: \"%s\"", TEXTO_FRAGMENTOS[i]));
            return;
        }
    }
}

// Fim de uma batalha do Bloco 2: pontos, objeto e fala da Luci
static void FimBatalhaB2(ContextoB2 *c)
{
    BatalhaB2 *b = c->batalha;

    c->progresso->alucinacao += b->erros;

    int delta = b->acertos * PONTOS_ACERTO_B2 - b->erros * PENALIDADE_ERRO;

    if (b->soEsquiva) delta -= PENALIDADE_ERRO;       // o puzzle errado que causou o ataque
    if (b->derrota)   delta -= PENALIDADE_HP_ZERO;

    AlterarPontos(c->progresso, delta);

    if (*c->motivo == MOTIVO_SILOGISTA && b->venceu) {
        c->b2->objeto[1] = true;
        AlterarPontos(c->progresso, PONTOS_ITEM_B2);
        DialogoIniciar(c->dlg, &dialogoItemB2[1], OBJ_NENHUM);
    } else {
        DialogoFalaAvulsa(c->dlg, *c->falaPosBatalha);
    }

    *c->pedirAutoSave = true;
    IniciarTransicao(c->transicao, *c->salaDaBatalha, DURACAO_FADE_BATALHA);
}

// ---------- Desenho das salas ----------

static void DesenharPortaSalaB2(Rectangle porta, Color cor, bool destaque, float pulso)
{
    DrawRectangleRec(porta, (Color){ 35, 25, 55, 255 });
    DrawRectangleLinesEx(porta, 3, destaque ? Fade(cor, 0.55f + 0.45f * pulso) : Fade(cor, 0.7f));
}

// Paredes, chão e borda (comuns a todas as salas do Bloco 2)
static void DesenharBaseSalaB2(Color corDestaque, float tempo, bool racks)
{
    ClearBackground((Color){ 8, 6, 18, 255 });

    DrawRectangle(50, 100, 700, 90, (Color){ 26, 20, 46, 255 });

    if (racks) {
        for (int i = 0; i < 9; i++) {
            int x = 66 + i * 76;
            if (i == 1 || i == 4 || i == 7) continue;     // lugares das portas

            DrawRectangle(x, 112, 40, 68, (Color){ 18, 15, 30, 255 });
            DrawRectangleLines(x, 112, 40, 68, Fade(PURPLE, 0.4f));

            for (int j = 0; j < 5; j++) {
                bool aceso = (Ruido((unsigned int)(i * 31 + j * 7) + (unsigned int)(tempo * 3.0f)) % 3) == 0;
                DrawRectangle(x + 6, 118 + j * 12, 28, 4, aceso ? SKYBLUE : Fade(PURPLE, 0.25f));
            }
        }
    }

    DrawRectangle(50, 190, 700, 330, (Color){ 18, 15, 30, 255 });
    DrawRectangle(50, 186, 700, 6, Fade(corDestaque, 0.45f));

    for (int x = 50; x < 750; x += 70)
        DrawLine(x, 192, x, 520, Fade(corDestaque, 0.08f));

    for (int y = 240; y < 520; y += 60)
        DrawLine(50, y, 750, y, Fade(corDestaque, 0.05f));

    DrawRectangleLinesEx(MAPA_B2, 4, Fade(PURPLE, 0.55f));
}

static void DesenharAvisoE(Vector2 centro, float dy)
{
    DrawText("PRESSIONE E", (int)centro.x - MeasureText("PRESSIONE E", 12) / 2, (int)(centro.y + dy), 12, GOLD);
}

static void DesenharTerminalRegistro(int i, Vector2 c, int estado, bool resolvido, float tempo)
{
    Color tela = (estado == 0) ? (Color){ 40, 40, 60, 255 } : (estado == 1) ? SKYBLUE : (Color){ 220, 90, 200, 255 };
    float pulso = 0.5f + 0.5f * sinf(tempo * 3.0f + i);

    DrawRectangle((int)c.x - 20, (int)c.y - 28, 40, 50, (Color){ 30, 28, 44, 255 });
    DrawRectangleLines((int)c.x - 20, (int)c.y - 28, 40, 50, Fade(PURPLE, 0.6f));
    DrawRectangle((int)c.x - 15, (int)c.y - 23, 30, 22, Fade(tela, (estado == 0) ? 1.0f : 0.6f + 0.3f * pulso));
    DrawRectangle((int)c.x - 10, (int)c.y + 22, 20, 6, (Color){ 45, 40, 62, 255 });

    if (estado == 0) {
        DrawText("?", (int)c.x - MeasureText("?", 16) / 2, (int)c.y - 21, 16, GRAY);
    } else if (estado == 2) {
        DrawLineEx((Vector2){ c.x - 7, c.y - 19 }, (Vector2){ c.x + 7, c.y - 5 }, 2, WHITE);
        DrawLineEx((Vector2){ c.x + 7, c.y - 19 }, (Vector2){ c.x - 7, c.y - 5 }, 2, WHITE);
    } else {
        DrawText("OK", (int)c.x - MeasureText("OK", 10) / 2, (int)c.y - 17, 10, WHITE);
    }

    const char *rotulo = TextFormat("REG %d", i + 1);
    DrawText(rotulo, (int)c.x - MeasureText(rotulo, 10) / 2, (int)c.y + 6, 10, resolvido ? Fade(LIGHTGRAY, 0.5f) : LIGHTGRAY);
}

static void DesenharAlavanca(int i, Vector2 c, bool ligada)
{
    DrawRectangle((int)c.x - 22, (int)c.y + 6, 44, 10, (Color){ 45, 40, 62, 255 });
    DrawRectangleLines((int)c.x - 22, (int)c.y + 6, 44, 10, Fade(SKYBLUE, 0.6f));

    Vector2 base = { c.x, c.y + 8 };
    Vector2 ponta = { c.x + (ligada ? 16.0f : -16.0f), c.y - 18 };

    DrawLineEx(base, ponta, 4, LIGHTGRAY);
    DrawCircleV(ponta, 6, ligada ? LIME : MAROON);

    const char *rotulo = TextFormat("%s = %s", NOMES_VARIAVEIS[i], ligada ? "V" : "F");
    DrawText(rotulo, (int)c.x - MeasureText(rotulo, 14) / 2, (int)c.y + 22, 14, ligada ? LIME : (Color){ 230, 120, 120, 255 });
}

static void DesenharPainelRegras(const ProgressoBloco2 *p, float tempo)
{
    Rectangle painel = { 220, 108, 360, 74 };

    DrawRectangleRec(painel, (Color){ 14, 18, 36, 255 });
    DrawRectangleLinesEx(painel, 2, SKYBLUE);
    DrawText("REGRAS DA BARREIRA", (int)painel.x + 10, (int)painel.y + 6, 10, Fade(SKYBLUE, 0.8f));

    for (int r = 0; r < 3; r++) {

        int x = (int)painel.x + 24 + r * 112;
        int y = (int)painel.y + 28;
        int luz = p->luzRegra[r];
        Color cor = (luz == 0) ? (Color){ 70, 70, 90, 255 } : (luz == 1) ? LIME : RED;

        if (p->barreiraAberta) cor = LIME;

        DrawText(TEXTO_REGRAS[r], x, y, 16, WHITE);
        DrawCircle(x + MeasureText(TEXTO_REGRAS[r], 16) / 2, y + 30, 5, Fade(cor, 0.7f + 0.3f * sinf(tempo * 4.0f)));
    }
}

static void DesenharBarreira(float tempo)
{
    Rectangle b = BARREIRA_LOGICA;

    DrawRectangle((int)b.x - 6, (int)b.y, 8, (int)b.height, (Color){ 45, 40, 62, 255 });
    DrawRectangle((int)(b.x + b.width) - 2, (int)b.y, 8, (int)b.height, (Color){ 45, 40, 62, 255 });

    for (int i = 0; i < 5; i++) {
        float y = b.y + 10 + i * 14 + sinf(tempo * 4.0f + i) * 2;
        DrawLineEx((Vector2){ b.x, y }, (Vector2){ b.x + b.width, y }, 2, Fade(SKYBLUE, 0.35f + 0.25f * sinf(tempo * 6.0f + i)));
    }
}

static void DesenharFragmento(Vector2 c, float tempo, int i)
{
    Vector2 p = { c.x, c.y + sinf(tempo * 2.5f + i) * 4.0f };

    DrawCircleV(p, 16, Fade(GOLD, 0.12f + 0.06f * sinf(tempo * 3.0f + i)));
    DrawCircleV(p, 8, GOLD);
    DrawCircleV(p, 4, (Color){ 255, 240, 180, 255 });
}

// HUD do Bloco 2 (objetos, pontos, alucinação)
static void DesenharHUDB2(TelaAtual tela, const ProgressoBloco2 *p, const ProgressoCorredor *prog,
                          const char *nickname, float tempo)
{
    DrawText("OBJETOS", 65, 62, 14, WHITE);

    for (int i = 0; i < NUM_OBJETOS_B2; i++) {

        Vector2 c = { 150.0f + i * 28, 68.0f };

        if (p->objeto[i]) {
            DesenharObjetoB2(i, c, 0.55f, tempo);
        } else {
            DrawRectangleLines((int)c.x - 10, (int)c.y - 10, 20, 20, Fade(GRAY, 0.6f));
        }
    }

    DrawText(TextFormat("PONTOS: %d", prog->pontos), 250, 62, 14, GOLD);
    DrawText(TextFormat("ALUCINAÇÃO: %d", prog->alucinacao), 380, 62, 14,
             (prog->alucinacao == 0) ? LIGHTGRAY : (prog->alucinacao < 3 ? ORANGE : RED));
    DrawText(nickname, 700 - MeasureText(nickname, 14), 62, 14, WHITE);

    int sala = IndiceSalaB2(tela);
    DrawText(sala >= 0 ? TITULOS_SALAS_B2[sala] : "BLOCO 2 // ARQUIVO DE TREINAMENTO", 65, 84, 10, GRAY);
}

static void DesenharMontador(const MontadorPergunta *m)
{
    DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, 0.6f));

    Rectangle painel = { 90, 140, 620, 330 };
    DrawRectangleRec(painel, BLACK);
    DrawRectangleLinesEx(painel, 4, WHITE);

    int px = (int)painel.x + 24;
    int py = (int)painel.y + 18;

    DrawText("MONTE A SUA PERGUNTA", px, py, 16, GOLD);
    DrawText("Escolha os fragmentos de contexto que deixam a pergunta clara.", px, py + 24, 14, LIGHTGRAY);

    int y = py + 56;

    for (int k = 0; k <= m->tamLista + 1; k++) {

        bool marcada = (m->selecao == k);

        if (marcada)
            DesenharCoracao((float)px + 6, (float)y + 8, 2, RED);

        if (k < m->tamLista) {

            int f = m->lista[k];
            Rectangle caixinha = { (float)px + 24, (float)y, 16, 16 };

            DrawRectangleLinesEx(caixinha, 2, marcada ? YELLOW : GRAY);

            if (m->incluido[f]) {
                DrawLineEx((Vector2){ caixinha.x + 3, caixinha.y + 8 }, (Vector2){ caixinha.x + 7, caixinha.y + 13 }, 2, GOLD);
                DrawLineEx((Vector2){ caixinha.x + 7, caixinha.y + 13 }, (Vector2){ caixinha.x + 13, caixinha.y + 3 }, 2, GOLD);
            }

            DrawText(TEXTO_FRAGMENTOS[f], px + 52, y, 16, marcada ? YELLOW : (m->incluido[f] ? GOLD : WHITE));
            y += 30;

        } else if (k == m->tamLista) {

            y += 8;
            DrawText("[ PERGUNTAR AO APOSTADOR ]", px + 24, y, 16, marcada ? YELLOW : SKYBLUE);
            y += 28;

        } else {

            DrawText("[ VOLTAR ]", px + 24, y, 16, marcada ? YELLOW : GRAY);
        }
    }

    DrawText("W/S: escolher    E/ENTER: marcar ou confirmar    BACKSPACE: voltar",
             px, (int)(painel.y + painel.height) - 24, 10, GRAY);
}

// Desenha a sala central ou a sala-puzzle atual (o jogador entra na ordem certa de profundidade)
static void DesenharSalaB2(TelaAtual tela, const ProgressoBloco2 *p, Vector2 playerPos, float tempo, bool explorando,
                           const SpriteDirecao sprites[NUM_DIRECOES], Direcao direcao, int quadro)
{
    int sala = IndiceSalaB2(tela);
    Color cor = (sala >= 0) ? CORES_SALAS_B2[sala] : PURPLE;
    float pulso = 0.5f + 0.5f * sinf(tempo * 4.0f);
    Rectangle jogador = AreaJogador(playerPos);
    float pes = playerPos.y + ALTURA_PERSONAGEM / 2;
    bool jogadorDesenhado = false;

    DesenharBaseSalaB2(cor, tempo, tela == SETOR2);

    // ===================== SALA CENTRAL =====================
    if (tela == SETOR2) {

        bool nucleoAberto = TodosObjetosB2(p);

        DesenharPortaCorredor(PORTA_VOLTAR_B2, PURPLE);
        DrawText("CORREDOR", 74, 286, 10, LIGHTGRAY);

        DrawRectangleRec(PORTA_NUCLEO, (Color){ 35, 25, 55, 255 });
        DrawRectangleLinesEx(PORTA_NUCLEO, 3, nucleoAberto ? Fade(SKYBLUE, 0.55f + 0.45f * pulso) : Fade(PURPLE, 0.5f));
        DrawText("NÚCLEO", 726 - MeasureText("NÚCLEO", 10), 286, 10, nucleoAberto ? SKYBLUE : GRAY);

        for (int i = 0; i < NUM_OBJETOS_B2; i++) {

            Rectangle porta = PORTAS_SALAS_B2[i];
            DesenharPortaSalaB2(porta, CORES_SALAS_B2[i], !p->objeto[i], pulso);

            int cx = (int)(porta.x + porta.width / 2);
            DrawText(NOMES_SALAS_B2[i], cx - MeasureText(NOMES_SALAS_B2[i], 10) / 2, (int)porta.y + 30, 10, CORES_SALAS_B2[i]);

            if (p->objeto[i])
                DesenharObjetoB2(i, (Vector2){ (float)cx, porta.y + 58 }, 0.6f, tempo);
        }

        // Altar dos objetos
        DrawEllipse((int)POS_ALTAR.x, (int)POS_ALTAR.y + 22, 60, 12, Fade(PURPLE, 0.15f));
        DrawRectangle((int)POS_ALTAR.x - 50, (int)POS_ALTAR.y - 12, 100, 34, (Color){ 35, 25, 55, 255 });
        DrawRectangleLinesEx((Rectangle){ POS_ALTAR.x - 50, POS_ALTAR.y - 12, 100, 34 }, 2, Fade(PURPLE, 0.8f));

        for (int i = 0; i < NUM_OBJETOS_B2; i++) {
            Vector2 c = { POS_ALTAR.x - 30 + i * 30, POS_ALTAR.y - 22 };
            if (p->objeto[i]) DesenharObjetoB2(i, c, 0.7f, tempo);
            else              DrawCircleLines((int)c.x, (int)c.y, 9, Fade(GRAY, 0.6f));
        }

        if (explorando) {
            if (CheckCollisionRecs(jogador, ZONA_PORTA_VOLTAR_B2)) DrawText("PRESSIONE E", 74, 394, 12, GOLD);
            if (CheckCollisionRecs(jogador, ZONA_PORTA_NUCLEO))    DrawText("PRESSIONE E", 726 - MeasureText("PRESSIONE E", 12), 394, 12, GOLD);
            if (CheckCollisionRecs(jogador, ZonaObjetoB2(POS_ALTAR))) DesenharAvisoE(POS_ALTAR, -52);

            for (int i = 0; i < NUM_OBJETOS_B2; i++) {
                Rectangle zona = { PORTAS_SALAS_B2[i].x - 10, PORTAS_SALAS_B2[i].y, PORTAS_SALAS_B2[i].width + 20, 104 };
                if (CheckCollisionRecs(jogador, zona))
                    DrawText("PRESSIONE E", (int)(PORTAS_SALAS_B2[i].x + PORTAS_SALAS_B2[i].width) + 8, 140, 12, GOLD);
            }
        }
    }

    // ===================== SALAS-PUZZLE =====================
    else {

        DrawRectangleRec(PORTA_SAIDA_SALA, (Color){ 35, 25, 55, 255 });
        DrawRectangleLinesEx(PORTA_SAIDA_SALA, 3, PURPLE);
        DrawText("SALA CENTRAL", 400 - MeasureText("SALA CENTRAL", 10) / 2, 524, 10, LIGHTGRAY);

        if (explorando && CheckCollisionRecs(jogador, ZONA_PORTA_SAIDA_SALA))
            DrawText("PRESSIONE E", 450, 504, 12, GOLD);

        if (tela == SALA_REGISTROS) {

            const char *nomeFonte = p->objeto[0] ? "FONTE (LIMPA)" : "A FONTE";

            // Objetos atrás do jogador -> jogador -> objetos na frente
            for (int passada = 0; passada < 2; passada++) {

                if (passada == 1) { DesenharJogador(sprites, playerPos, direcao, quadro); jogadorDesenhado = true; }

                if ((passada == 0) == (POS_FONTE.y + 40 <= pes)) {
                    DrawEllipse((int)POS_FONTE.x, (int)POS_FONTE.y + 44, 46, 10, Fade(cor, 0.15f));
                    DesenharFonte(POS_FONTE, tempo, 0.6f, p->objeto[0] ? SKYBLUE : CORES_SALAS_B2[0]);
                }

                for (int i = 0; i < NUM_REGISTROS; i++)
                    if ((passada == 0) == (POS_REGISTROS[i].y + 22 <= pes))
                        DesenharTerminalRegistro(i, POS_REGISTROS[i], p->registro[i], p->objeto[0], tempo);
            }

            DrawText(nomeFonte, (int)POS_FONTE.x - MeasureText(nomeFonte, 10) / 2, (int)POS_FONTE.y - 58, 10, p->objeto[0] ? SKYBLUE : cor);

            if (explorando) {
                if (CheckCollisionRecs(jogador, ZonaObjetoB2(POS_FONTE))) DesenharAvisoE(POS_FONTE, -74);
                for (int i = 0; i < NUM_REGISTROS; i++)
                    if (CheckCollisionRecs(jogador, ZonaObjetoB2(POS_REGISTROS[i])))
                        DesenharAvisoE(POS_REGISTROS[i], -46);
            }

            DrawText(TextFormat("INSPECIONADOS: %d/%d", RegistrosInspecionados(p), NUM_REGISTROS), 560, 196, 10, LIGHTGRAY);
        }

        else if (tela == SALA_LOGICA) {

            DesenharPainelRegras(p, tempo);

            if (!p->objeto[1]) {
                DesenharSilogista(POS_SILOGISTA, tempo, 0.5f, GUARDIOES_B2[GUARDIAO_SILOGISTA].cor);
                DrawText("O SILOGISTA", (int)POS_SILOGISTA.x - MeasureText("O SILOGISTA", 10) / 2, 196, 10, SKYBLUE);
            }

            if (!p->barreiraAberta)
                DesenharBarreira(tempo);

            for (int passada = 0; passada < 2; passada++) {

                if (passada == 1) { DesenharJogador(sprites, playerPos, direcao, quadro); jogadorDesenhado = true; }

                for (int i = 0; i < 3; i++)
                    if ((passada == 0) == (POS_ALAVANCAS[i].y + 16 <= pes))
                        DesenharAlavanca(i, POS_ALAVANCAS[i], p->alavanca[i]);

                // Console de verificação
                if ((passada == 0) == (POS_CONSOLE.y + 22 <= pes)) {
                    DrawRectangle((int)POS_CONSOLE.x - 20, (int)POS_CONSOLE.y - 22, 40, 44, (Color){ 30, 28, 44, 255 });
                    DrawRectangleLines((int)POS_CONSOLE.x - 20, (int)POS_CONSOLE.y - 22, 40, 44, SKYBLUE);
                    DrawRectangle((int)POS_CONSOLE.x - 14, (int)POS_CONSOLE.y - 16, 28, 14, Fade(SKYBLUE, 0.4f + 0.3f * pulso));
                    DrawText("VERIFICAR", (int)POS_CONSOLE.x - MeasureText("VERIFICAR", 10) / 2, (int)POS_CONSOLE.y + 26, 10, LIGHTGRAY);
                }
            }

            if (explorando) {
                if (CheckCollisionRecs(jogador, ZonaObjetoB2(POS_CONSOLE))) DesenharAvisoE(POS_CONSOLE, -40);
                for (int i = 0; i < 3; i++)
                    if (CheckCollisionRecs(jogador, ZonaObjetoB2(POS_ALAVANCAS[i])))
                        DesenharAvisoE(POS_ALAVANCAS[i], -40);
                if (p->barreiraAberta && !p->objeto[1] &&
                    CheckCollisionRecs(jogador, (Rectangle){ POS_SILOGISTA.x - 60, POS_SILOGISTA.y - 20, 120, 110 }))
                    DrawText("PRESSIONE E", (int)POS_SILOGISTA.x + 40, (int)POS_SILOGISTA.y - 6, 12, GOLD);
            }
        }

        else if (tela == SALA_LACUNA) {

            DesenharApostador(POS_APOSTADOR, tempo, 0.5f, GUARDIOES_B2[GUARDIAO_APOSTADOR].cor);
            DrawText("O APOSTADOR", (int)POS_APOSTADOR.x - MeasureText("O APOSTADOR", 10) / 2, 196, 10, GOLD);

            for (int passada = 0; passada < 2; passada++) {

                if (passada == 1) { DesenharJogador(sprites, playerPos, direcao, quadro); jogadorDesenhado = true; }

                for (int i = 0; i < NUM_FRAGMENTOS; i++)
                    if (!p->fragmento[i] && (passada == 0) == (POS_FRAGMENTOS[i].y + 10 <= pes))
                        DesenharFragmento(POS_FRAGMENTOS[i], tempo, i);
            }

            int coletados = 0;
            for (int i = 0; i < NUM_FRAGMENTOS; i++) if (p->fragmento[i]) coletados++;
            DrawText(TextFormat("FRAGMENTOS: %d/%d", coletados, NUM_FRAGMENTOS), 590, 196, 10, LIGHTGRAY);

            if (explorando) {
                if (CheckCollisionRecs(jogador, (Rectangle){ POS_APOSTADOR.x - 60, POS_APOSTADOR.y - 20, 120, 110 }))
                    DrawText("PRESSIONE E", (int)POS_APOSTADOR.x + 40, (int)POS_APOSTADOR.y - 6, 12, GOLD);
                for (int i = 0; i < NUM_FRAGMENTOS; i++)
                    if (!p->fragmento[i] && CheckCollisionRecs(jogador, ZonaObjetoB2(POS_FRAGMENTOS[i])))
                        DesenharAvisoE(POS_FRAGMENTOS[i], -34);
            }
        }
    }

    if (!jogadorDesenhado)
        DesenharJogador(sprites, playerPos, direcao, quadro);
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
// MAIN
// =============================================================

int main(void) {

    // 1. INICIALIZAÇÃO

    InitWindow(LARGURA_TELA, ALTURA_TELA, "UtopIA - Game");

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

    // Nickname do jogador: exatamente 3 letras
    char nickname[4] = {'_', '_', '_', '\0'};
    int nicknamePos = 0;

    const char *textosIntro[3] = {
        "O uso da Inteligência Artificial tornou-se parte da nossa rotina, utilizada para\nescrever textos, gerar imagens e auxiliar em tomadas de decisão.",
        "Contudo, quando dados incorretos ou perguntas enviesadas são inseridos no sistema,\nsurge um problema real: a alucinação de IA.",
        "Bem-vindo a UtopIA, o universo interior que sustenta essa inteligência. O sistema está\ncolapsando internamente por conta dessas alucinações e se essa falha não for \ncorrigida a tempo, o impacto afetará todas as IAs do mundo real...",
    };

    // =========================================================
    // VARIÁVEIS DO CORREDOR E DA BATALHA
    // =========================================================

    ProgressoCorredor progresso = {0};
    ProgressoBloco2 bloco2 = {0};
    BatalhaB2 batalhaB2 = {0};
    TelaAtual salaDaBatalhaB2 = SETOR2;      // para onde volta depois da batalha
    MotivoBatalhaB2 motivoBatalhaB2 = MOTIVO_SILOGISTA;
    const char *falaPosBatalhaB2 = "";
    MontadorPergunta montador = {0};         // painel de montar pergunta (Sala da Lacuna)
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
    float flashSave = 0.0f;                  // brilho do ponto de save logo depois de salvar
    int pontoSaveFlash = -1;
    float confirmarNovoJogo = 0.0f;          // > 0 = clicou em JOGAR com save existente, falta confirmar

    // =========================================================
    // VARIÁVEIS DO PAUSE
    // =========================================================

    bool pausado = false;
    bool capturarPausa = false;              // apertou ESC: tira a "foto" do fundo no fim deste frame
    int selecaoPausa = PAUSA_CONTINUAR;
    Texture2D fundoPausa = {0};

    const char *arquivoPontoSave = AcharSpriteJogador(SPRITE_PONTO_SAVE);
    Texture2D texPontoSave = (arquivoPontoSave != NULL) ? LoadTexture(arquivoPontoSave) : (Texture2D){0};

    // =========================================================
    // ÁREAS DE CLIQUE DOS BOTÕES DO MENU
    // =========================================================

    Rectangle btnContinuar = {300, 270, 200, 50};   // só aparece se existir save
    Rectangle btnJogar = {300, 270, 200, 50};
    Rectangle btnOpcoes = {300, 340, 200, 50};
    Rectangle btnSair = {300, 410, 200, 50};

    // Tudo que a lógica do Bloco 2 mexe
    ContextoB2 ctxB2 = {
        &tela, &dlg, &transicao, &progresso, &bloco2, &batalhaB2, &playerPos, &direcaoAtual,
        &pedirAutoSave, &salaDaBatalhaB2, &motivoBatalhaB2, &falaPosBatalhaB2, &montador
    };

    SetTargetFPS(60);

    // 2. GAME LOOP

    while (!WindowShouldClose() && !fecharJogo) {

        float dt = GetFrameTime();

        AjustarMouseAJanela();

        // =====================================================
        // ESC: ABRE / FECHA O PAUSE
        // =====================================================

        if (IsKeyPressed(KEY_ESCAPE)) {

            if (pausado) {

                // Volta para o jogo
                pausado = false;
                UnloadTexture(fundoPausa);
                fundoPausa = (Texture2D){0};

            } else if ((tela == JOGANDO || tela == CORREDOR || tela == BATALHA ||
                        TelaExploracaoB2(tela) || tela == BATALHA_B2) && !transicao.ativa) {

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

                    } else if (tela == SETOR2 && origem == CORREDOR) {

                        playerPos = SPAWN_B2;
                        direcaoAtual = DIR_DIREITA;
                        pedirAutoSave = true;     // AUTO SAVE: entrou no Bloco 2

                    } else if (tela == CORREDOR && origem == SETOR2) {

                        playerPos = SPAWN_CORREDOR_VOLTA;
                        direcaoAtual = DIR_ESQUERDA;
                        pedirAutoSave = true;     // AUTO SAVE: voltou para o corredor

                    } else if (TelaSalaPuzzleB2(tela) && origem == SETOR2) {

                        playerPos = SPAWN_ENTRADA_SALA;
                        direcaoAtual = DIR_COSTAS;
                        pedirAutoSave = true;     // AUTO SAVE: entrou numa sala-puzzle

                    } else if (tela == SETOR2 && TelaSalaPuzzleB2(origem)) {

                        // Volta para baixo da porta da sala de onde saiu
                        Rectangle porta = PORTAS_SALAS_B2[IndiceSalaB2(origem)];
                        playerPos = (Vector2){ porta.x + porta.width / 2, 232 };
                        direcaoAtual = DIR_FRENTE;
                        montador.aberto = false;
                        pedirAutoSave = true;     // AUTO SAVE: voltou para a sala central
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
                saveFalhou = !SalvarJogo(nickname, (tela == BATALHA_B2) ? salaDaBatalhaB2 : tela, playerPos, direcaoAtual, luciAcompanhando, &progresso, &bloco2);
                existeSave = LerSave(&saveLido);

                pausado = false;
                UnloadTexture(fundoPausa);
                fundoPausa = (Texture2D){0};

                tela = MENU;
                dlg = (SessaoDialogo){0};
                transicao = (Transicao){0};
                pedirAutoSave = false;
                avisoSave = 0.0f;
                confirmarNovoJogo = 0.0f;

            } else if (escolha == PAUSA_SAIR) {

                // O auto save do final do programa guarda o progresso
                fecharJogo = true;
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
                        bloco2 = saveLido.bloco2;
                        playerPos = (Vector2){ saveLido.x, saveLido.y };
                        direcaoAtual = (Direcao)saveLido.direcao;
                        dlg = (SessaoDialogo){0};
                        animJogador = (AnimacaoJogador){0};
                        tela = (TelaAtual)saveLido.sala;

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
                    bloco2 = (ProgressoBloco2){0};
                    montador = (MontadorPergunta){0};
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

                // ENTER só avança quando as 3 letras estiverem preenchidas.
                if (IsKeyPressed(KEY_ENTER) && nicknamePos == 3) {

                    etapaIntro = 0;
                    tela = JOGANDO;
                }
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

                // Ponto de save: respondeu SIM
                if (r == RES_SIM && dlg.objeto == OBJ_SAVE) {

                    bool ok = SalvarJogo(nickname, (tela == BATALHA_B2) ? salaDaBatalhaB2 : tela, playerPos, direcaoAtual, luciAcompanhando, &progresso, &bloco2);

                    if (ok) {
                        existeSave = true;
                        flashSave = 0.8f;
                    }

                    DialogoFalaAvulsa(&dlg, ok
                        ? "* Os dados foram sincronizados. Progresso salvo."
                        : "* O fluxo falhou... Não foi possível salvar.");
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

                    } else if ((pontoSaveFlash = PontoSaveProximo(JOGANDO, AreaJogador(playerPos))) >= 0) {

                        DialogoConfirmar(&dlg,
                            "* Um fluxo de dados corre sem parar. Você se sente sincronizado.\n* Salvar o progresso?",
                            OBJ_SAVE);
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
            // CONVERSA / QUIZ EM ANDAMENTO (o personagem fica parado)
            // -------------------------------------------------

            if (dlg.estado != DLG_NENHUM) {

                ResultadoDialogo r = DialogoAtualizar(&dlg);
                int m = dlg.objeto;

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

                    // Resolveu os 4 monitores: Luci avisa que a saída abriu
                    if (ContarConcluidos(&progresso) == TOTAL_MONITORES &&
                        !progresso.fimAnunciado) {

                        progresso.fimAnunciado = true;
                        DialogoIniciar(&dlg, &dialogoFimCorredor, OBJ_NENHUM);
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

                        if (ContarConcluidos(&progresso) == TOTAL_MONITORES) {

                            // Bloco 2: Arquivo de Treinamento
                            IniciarTransicao(&transicao, SETOR2, DURACAO_FADE);

                        } else {

                            DialogoIniciar(&dlg, &dialogoPortaTrancada, OBJ_NENHUM);
                        }
                    }

                    // Monitores
                    else {

                        for (int i = 0; i < TOTAL_MONITORES; i++) {

                            if (!CheckCollisionRecs(jogador, ZonaInteracao(monitores[i].area)))
                                continue;

                            const Dialogo *q = &monitores[i].quiz;
                            int proximo = ProximoMonitor(&progresso);

                            if (progresso.estado[i] == MON_ACERTOU) {

                                DialogoFalaAvulsa(&dlg, TextFormat(
                                    "* O %s mostra um sinal verde. Lição concluída!", monitores[i].nome));

                            } else if (progresso.estado[i] == MON_ERROU) {

                                DialogoFalaAvulsa(&dlg, TextFormat(
                                    "* O %s ainda chia um pouco. A resposta certa era a %c.",
                                    monitores[i].nome, 'A' + q->correta));

                            } else if (QUIZ_EM_ORDEM && i != proximo) {

                                DialogoFalaAvulsa(&dlg, TextFormat(
                                    "Luci: Vamos na ordem, uma lição puxa a outra. Comece pelo monitor %d.",
                                    proximo + 1));

                            } else {

                                // NEUTRO -> ATIVO: começa a explicação + quiz
                                progresso.estado[i] = MON_ATIVO;
                                DialogoIniciar(&dlg, q, i);
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
        // BLOCO 2 - SALA CENTRAL E SALAS-PUZZLE
        // =====================================================

        else if (TelaExploracaoB2(tela) && !transicao.ativa) {

            companionAngulo += dt * 2.5f;
            AtualizarSalaB2(&ctxB2, dt);
        }

        // =====================================================
        // BATALHA DO BLOCO 2 (duelo ou esquiva)
        // =====================================================

        else if (tela == BATALHA_B2 && !transicao.ativa) {

            if (BatalhaB2Atualizar(&batalhaB2))
                FimBatalhaB2(&ctxB2);
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

        if (pedirAutoSave && (tela == JOGANDO || tela == CORREDOR || TelaExploracaoB2(tela))) {

            pedirAutoSave = false;
            saveFalhou = !SalvarJogo(nickname, (tela == BATALHA_B2) ? salaDaBatalhaB2 : tela, playerPos, direcaoAtual, luciAcompanhando, &progresso, &bloco2);

            if (!saveFalhou)
                existeSave = true;

            avisoSave = TEMPO_AVISO_SAVE;
        }

        if (!pausado) {
            if (avisoSave > 0.0f) avisoSave -= dt;
            if (flashSave > 0.0f) flashSave -= dt * 1.5f;
        }

        // =====================================================
        // ANIMAÇÃO DO PERSONAGEM
        // =====================================================

        // Só anima se ele realmente se moveu (parado, em diálogo ou
        // empurrando a parede ele fica no quadro 0)
        bool andou = (tela == JOGANDO || tela == CORREDOR || TelaExploracaoB2(tela)) &&
                     !transicao.ativa &&
                     Vector2Distance(posInicioFrame, playerPos) > 0.01f;

        if (!pausado)
            AtualizarAnimacao(&animJogador, andou, dt);

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

            DrawRectangleRec(painelImagem, (Color){8, 6, 18, 255});
            DrawRectangleLinesEx(painelImagem, 1, Fade(LIGHTGRAY, 0.65f));

            // =================================================
            // SIMULAÇÃO DA IMAGEM
            // =================================================

            DrawCircle(xConteudo + 415, 140, 30, Fade(PURPLE, 0.30f));
            DrawCircle(xConteudo + 415, 140, 19, Fade(PURPLE, 0.45f));
            DrawRectangle(xConteudo + 55, 270, 390, 3, Fade(LIGHTGRAY, 0.25f));
            DrawRectangle(xConteudo + 95, 230, 90, 40, Fade(DARKPURPLE, 0.55f));
            DrawRectangle(xConteudo + 205, 210, 150, 60, Fade(DARKPURPLE, 0.35f));

            const char *textoImagem = "IMAGEM DA HISTORIA DO JOGO";

            DrawText(textoImagem, 400 - MeasureText(textoImagem, 11) / 2, 245, 11, Fade(WHITE, 0.65f));

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

                        etapaIntro = 0;
                        tela = JOGANDO;
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

                DesenharLuci(pLuci);
                DrawText("Luci", (int)pLuci.x - MeasureText("Luci", 10) / 2, (int)pLuci.y - 28, 10, Fade(SKYBLUE, 0.8f));
            }

            // -------------------------------------------------
            // PERSONAGEM
            // -------------------------------------------------

            // Ponto de save: desenhado atrás ou na frente do jogador, conforme a altura
            Vector2 baseSave = PONTOS_SAVE[0].base;
            bool pertoSave = livre && PontoSaveProximo(JOGANDO, AreaJogador(playerPos)) == 0;
            bool saveAtras = baseSave.y <= playerPos.y + ALTURA_PERSONAGEM / 2;
            float flashSala = (pontoSaveFlash == 0) ? flashSave : 0.0f;

            if (saveAtras)
                DesenharPontoSave(texPontoSave, baseSave, tempo, pertoSave, flashSala);

            DesenharJogador(spritesJogador, playerPos, direcaoAtual, animJogador.quadro);

            if (!saveAtras)
                DesenharPontoSave(texPontoSave, baseSave, tempo, pertoSave, flashSala);

            if (luciAcompanhando)
                DesenharCompanion(playerPos, companionAngulo);

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

            // Aviso do ponto de save: fora do mapa, no canto inferior esquerdo
            // (alinhado com a borda esquerda do mapa, logo abaixo dela)
            if (pertoSave) {

                if (mapaCarregado)
                    DrawText("PRESSIONE E", 8, ALTURA_TELA - 24, 12, GOLD);
                else
                    DrawText("PRESSIONE E", (int)mapa.x, (int)(mapa.y + mapa.height) + 12, 12, GOLD);
            }

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

            // Conversa por cima de tudo
            DialogoDesenhar(&dlg);
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
            bool saidaLiberada = (concluidos == TOTAL_MONITORES);

            float pulsoPorta = 0.5f + 0.5f * sinf(tempo * 4.0f);

            Color bordaSaida = saidaLiberada
                ? Fade(GREEN, 0.55f + 0.45f * pulsoPorta)
                : MAROON;

            DesenharPortaCorredor(PORTA_VOLTAR_CORREDOR, PURPLE);
            DesenharPortaCorredor(PORTA_FIM_CORREDOR, bordaSaida);

            DrawText(
                "SALA",
                (int)(PORTA_VOLTAR_CORREDOR.x + PORTA_VOLTAR_CORREDOR.width / 2) - MeasureText("SALA", 10) / 2,
                (int)(PORTA_VOLTAR_CORREDOR.y + PORTA_VOLTAR_CORREDOR.height) + 6,
                10,
                LIGHTGRAY
            );

            const char *textoSaida = saidaLiberada ? "SAIDA" : "TRANCADA";

            DrawText(
                textoSaida,
                (int)(PORTA_FIM_CORREDOR.x + PORTA_FIM_CORREDOR.width) - MeasureText(textoSaida, 10) - 8,
                (int)PORTA_FIM_CORREDOR.y - 16,
                10,
                saidaLiberada ? GREEN : RED
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

            DesenharJogador(spritesJogador, playerPos, direcaoAtual, animJogador.quadro);
            DesenharCompanion(playerPos, companionAngulo);

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

            // Conversa / quiz por cima de tudo
            DialogoDesenhar(&dlg);
            DesenharAvisoSave(avisoSave, saveFalhou, tempo);
        }

        // =====================================================
        // TELA BATALHA
        // =====================================================

        else if (tela == BATALHA) {

            BatalhaDesenhar(&batalha, nickname, tempo);
        }

        // =====================================================
        // TELA BLOCO 2 - SALA CENTRAL E SALAS-PUZZLE
        // =====================================================

        else if (TelaExploracaoB2(tela)) {

            bool explorando = (dlg.estado == DLG_NENHUM && !montador.aberto && !transicao.ativa);

            DesenharSalaB2(tela, &bloco2, playerPos, tempo, explorando, spritesJogador, direcaoAtual, animJogador.quadro);
            DesenharCompanion(playerPos, companionAngulo);
            DesenharHUDB2(tela, &bloco2, &progresso, nickname, tempo);

            if (dlg.estado == DLG_NENHUM && !montador.aberto)
                DrawText("W A S D  -  MOVIMENTAR", 65, 532, 12, LIGHTGRAY);

            if (montador.aberto)
                DesenharMontador(&montador);

            DialogoDesenhar(&dlg);
            DesenharAvisoSave(avisoSave, saveFalhou, tempo);
        }

        // =====================================================
        // TELA BATALHA DO BLOCO 2
        // =====================================================

        else if (tela == BATALHA_B2) {

            BatalhaB2Desenhar(&batalhaB2, nickname, tempo);
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
    if (tela == JOGANDO || tela == CORREDOR || tela == BATALHA || TelaExploracaoB2(tela) || tela == BATALHA_B2)
        SalvarJogo(nickname, (tela == BATALHA_B2) ? salaDaBatalhaB2 : tela, playerPos, direcaoAtual, luciAcompanhando, &progresso, &bloco2);

    // (as texturas precisam ser liberadas ANTES de fechar a janela)
    if (texPontoSave.id > 0)
        UnloadTexture(texPontoSave);

    if (texMapaSala.id > 0)
        UnloadTexture(texMapaSala);

    if (fundoPausa.id > 0)
        UnloadTexture(fundoPausa);

    UnloadRenderTexture(alvo);
    UnloadShader(filtroNitidez.shader);

    UnloadTexture(imagem_titulo);
    for (int i = 0; i < NUM_DIRECOES; i++)
        if (!spritesJogador[i].emprestada)
            UnloadTexture(spritesJogador[i].textura);

    for (int i = 0; i < 4; i++)
        if (spritesMonitor[i].id > 0)
            UnloadTexture(spritesMonitor[i]);

    CloseWindow();

    return 0;
}