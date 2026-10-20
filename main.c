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
typedef enum { MENU, INTRO, JOGANDO, OPCOES, CORREDOR, BATALHA } TelaAtual;
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
#define VERSAO_SAVE          1            // aumente quando mudar o que vai no save
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
#define MOSTRAR_PAREDES_INVISIVEIS  1   // 1 = desenha as paredes (para calibrar) | 0 = escondidas

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
static const Vector2 SPAWN_SALA_VOLTA = { MAPA_X(522), MAPA_Y(300) };   // logo abaixo da porta de cima   // logo abaixo da porta de cima
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
                "A alucinação ocorre quando a IA gera uma resposta que parece confiante e coerente, mas é incorreta ou inventada. No pós-treinamento, os ajustes eliminam esses erros, pois a IA passa a ser recompensada por demonstrar incerteza.",
                "A alucinação ocorre quando a IA gera uma resposta visivelmente incoerente, fácil de reconhecer como erro. No pós-treinamento, os ajustes eliminam completamente as falhas do modelo.",
                "A alucinação ocorre quando a IA se recusa a responder por não ter certeza da informação. No pré-treinamento, o modelo aprende padrões probabilísticos ao prever a próxima palavra e pode \"adivinhar\" a resposta mais provável.",
                "A alucinação ocorre quando a IA gera uma resposta que parece confiante e coerente, mas é incorreta, contradiz o contexto ou é inventada. No pré-treinamento, o modelo aprende padrões probabilísticos ao prever a próxima palavra e pode \"adivinhar\" a resposta mais provável."
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
                "Path Reuse: no início do treinamento, o modelo pode \"pegar atalhos\" e reaproveitar um caminho de raciocínio que funcionou antes.",
                "Path Compression: conforme o treinamento avança, caminhos usados com frequência são \"encurtados\" para serem mais eficientes.",
                "Path Compression: conforme o treinamento avança, a IA passa a percorrer todas as etapas intermediárias de raciocínio antes de chegar a uma conclusão.",
                "Path Reuse: um caminho de raciocínio que funcionou para um assunto pode ser aplicado em um contexto completamente diferente."
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
                "As alucinações intrínsecas acontecem quando a resposta não pode ser verificada ou é completamente inventada, mesmo que não contradiga o contexto. As extrínsecas acontecem quando a resposta contradiz a informação fornecida pelo usuário ou o contexto.",
                "As alucinações intrínsecas acontecem quando a resposta contradiz a informação fornecida pelo usuário ou o contexto. As extrínsecas acontecem quando a resposta não pode ser verificada ou é completamente inventada, mesmo que não contradiga o contexto.",
                "As alucinações intrínsecas acontecem quando a resposta contradiz a informação fornecida pelo usuário ou o contexto. As extrínsecas acontecem quando a resposta é sempre verdadeira, mas apresentada de forma diferente da esperada.",
                "As alucinações intrínsecas acontecem quando a IA aprende informações erradas ou desatualizadas nos dados de treinamento. As extrínsecas acontecem quando a IA junta coisas que aparecem juntas nos dados, mesmo sem ligação real."
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
                "A IA junta dois fatos que aparecem juntos nos dados, mesmo sem ligação real, e passa a tratá-los como se um causasse o outro. Para evitar isso, o usuário deve aceitar a resposta quando ela parecer confiante e coerente.",
                "O usuário faz um pedido vago e confuso, e a IA inventa informações para manter a coerência. Para evitar isso, basta conectar o modelo a uma base de dados confiável em tempo real, usando o RAG, sem precisar mudar o pedido.",
                "A IA aprendeu informações desatualizadas nos dados de treinamento e as repete como se fossem atuais. Para evitar isso, o usuário deve escrever pedidos mais curtos e com menos contexto.",
                "O usuário faz um pedido confuso, e a IA inventa informações para preencher as lacunas. Para evitar isso, o usuário deve reformular a pergunta de forma direta e dar um contexto mais claro."
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

static const Dialogo dialogoPortaLiberada = {
    .paginas = {
        "* A porta se abre com um brilho suave.",
        "(A próxima área ainda está em desenvolvimento.)"
    },
    .numPaginas = 2
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
static void DesenharCompanion(Vector2 jogadorPos, float angulo)
{
    Vector2 p = {
        jogadorPos.x + cosf(angulo) * 40.0f,
        jogadorPos.y - 10.0f + sinf(angulo * 1.2f) * 26.0f
    };

    Rectangle companion = { p.x - 7, p.y - 7, 14, 14 };

    DrawCircle((int)p.x, (int)p.y, 14, Fade(SKYBLUE, 0.15f));
    DrawRectangleRec(companion, SKYBLUE);
    DrawRectangleLinesEx(companion, 2, BLUE);
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
                        "* No Bloco 1 você não perde vidas, mas ela ficou mais forte.");
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

    if (d->sala != JOGANDO && d->sala != CORREDOR)
        return false;

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
                       bool luciAcompanhando, const ProgressoCorredor *progresso)
{
    DadosSave d;
    memset(&d, 0, sizeof(d));

    d.assinatura = ASSINATURA_SAVE;
    d.versao = VERSAO_SAVE;
    memcpy(d.nickname, nickname, 4);

    // A batalha acontece "dentro" do corredor
    d.sala = (tela == JOGANDO) ? JOGANDO : CORREDOR;

    d.x = pos.x;
    d.y = pos.y;
    d.direcao = (int)direcao;
    d.luciAcompanhando = luciAcompanhando;
    d.progresso = *progresso;

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
// MENU DE PAUSE
// =============================================================

#define DESFOQUE_PAUSA  4        // quanto maior, mais desfocado fica o fundo (2 a 8)

typedef enum { PAUSA_CONTINUAR, PAUSA_SAIR, NUM_OPCOES_PAUSA } OpcaoPausa;

// Tira uma "foto" da tela atual (com as cores normais do jogo) e desfoca:
// diminui e aumenta de novo a imagem, perdendo os detalhes finos.
// É feito uma vez só, no momento em que o jogo pausa.
static Texture2D CapturarFundoPausa(void)
{
    Image img = LoadImageFromScreen();
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
    return (Rectangle){ 290, 285 + i * 60.0f, 220, 44 };
}

// Lê teclado e mouse no menu de pause.
// Devolve a opção escolhida (PAUSA_CONTINUAR / PAUSA_SAIR) ou -1 se nada foi escolhido.
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
    Rectangle painel = { 250, 170, 300, 265 };

    DrawRectangleRec(painel, Fade(painelCor, 0.92f));
    DrawRectangleLinesEx(painel, 4, PURPLE);
    DrawRectangleLinesEx((Rectangle){ painel.x + 8, painel.y + 8, painel.width - 16, painel.height - 16 }, 1, Fade(SKYBLUE, 0.35f));

    const char *titulo = "PAUSADO";
    DrawText(titulo, LARGURA_TELA / 2 - MeasureText(titulo, 40) / 2, 200, 40, PURPLE);

    // Opções, com o coração na marcada (estilo Undertale, em azul)
    const char *opcoes[NUM_OPCOES_PAUSA] = { "CONTINUAR", "SAIR DO JOGO" };

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
    const char *dica2 = "O progresso é salvo ao sair";

    DrawText(dica1, LARGURA_TELA / 2 - MeasureText(dica1, 10) / 2, 400, 10, Fade(SKYBLUE, 0.6f));
    DrawText(dica2, LARGURA_TELA / 2 - MeasureText(dica2, 10) / 2, 414, 10, GRAY);
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

    SetTargetFPS(60);

    // 2. GAME LOOP

    while (!WindowShouldClose() && !fecharJogo) {

        float dt = GetFrameTime();

        // =====================================================
        // ESC: ABRE / FECHA O PAUSE
        // =====================================================

        if (IsKeyPressed(KEY_ESCAPE)) {

            if (pausado) {

                // Volta para o jogo
                pausado = false;
                UnloadTexture(fundoPausa);
                fundoPausa = (Texture2D){0};

            } else if ((tela == JOGANDO || tela == CORREDOR || tela == BATALHA) && !transicao.ativa) {

                // Só pausa durante o jogo (no menu e na intro o ESC não faz nada)
                capturarPausa = true;
                selecaoPausa = PAUSA_CONTINUAR;

            } else if (tela == OPCOES) {

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

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                    tela = OPCOES;
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

                    bool ok = SalvarJogo(nickname, tela, playerPos, direcaoAtual, luciAcompanhando, &progresso);

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
                if (!luciAcompanhando && CheckCollisionRecs(HitboxJogador(playerPos), luciHitbox))
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

                            // TODO: aqui entra a transição para a próxima área (Bloco 2).
                            DialogoIniciar(&dlg, &dialogoPortaLiberada, OBJ_NENHUM);

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
                    0
                );

                IniciarTransicao(&transicao, CORREDOR, DURACAO_FADE_BATALHA);
            }
        }

        // =====================================================
        // OPÇÕES
        // =====================================================

        else if (tela == OPCOES) {

            if (IsKeyPressed(KEY_BACKSPACE))
                tela = MENU;
        }

        // =====================================================
        // AUTO SAVE (feito aqui, depois de toda a lógica do frame)
        // =====================================================

        if (pedirAutoSave && (tela == JOGANDO || tela == CORREDOR)) {

            pedirAutoSave = false;
            saveFalhou = !SalvarJogo(nickname, tela, playerPos, direcaoAtual, luciAcompanhando, &progresso);

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
        bool andou = (tela == JOGANDO || tela == CORREDOR) &&
                     !transicao.ativa &&
                     Vector2Distance(posInicioFrame, playerPos) > 0.01f;

        if (!pausado)
            AtualizarAnimacao(&animJogador, andou, dt);

        // =====================================================
        // DESENHO
        // =====================================================

        BeginDrawing();

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
            
            DrawRectangleRec(mapa, (Color){18, 15, 30, 255});      // chão
            DrawRectangleLinesEx(mapa, 4, Fade(PURPLE, 0.55f));    // bordas

            // -------------------------------------------------
            // PORTA (parede de cima)
            // -------------------------------------------------

            bool salaLiberada = luciAcompanhando;

            if (!mapaCarregado) {

                DrawRectangleRec(PORTA_SALA, (Color){35, 25, 55, 255});
                DrawRectangleLinesEx(PORTA_SALA, 3, salaLiberada ? PURPLE : MAROON);

                // Maçaneta
                DrawCircle((int)(PORTA_SALA.x + PORTA_SALA.width - 12), (int)(PORTA_SALA.y + PORTA_SALA.height / 2), 4, GOLD);

            } else if (!salaLiberada) {

                // O arco já está na imagem: só marca a porta como trancada
                DrawRectangleRec(PORTA_SALA, Fade(MAROON, 0.35f));
            }

            DrawText(
                "CORREDOR",
                (int)(PORTA_SALA.x + PORTA_SALA.width / 2) - MeasureText("CORREDOR", 10) / 2,
                (int)PORTA_SALA.y - 14,
                10,
                salaLiberada ? LIGHTGRAY : RED
            );

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

                // hitbox da Luci em azul, pra ajudar a calibrar a posição dela
                DrawRectangleRec(luciHitbox, Fade(BLUE, 0.4f));
                DrawText("Luci", (int)luciHitbox.x, (int)luciHitbox.y - 10, 10, WHITE);
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
            DrawText(mapaCarregado ? "E - INTERAGIR" : "E  -  INTERAGIR", hudEsq, 82, hudFonte, LIGHTGRAY);

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
                DrawText("E  -  INTERAGIR", 65, 469, 12, LIGHTGRAY);
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
        // TELA OPÇÕES
        // =====================================================

        else if (tela == OPCOES) {

            ClearBackground(DARKBLUE);

            DrawText("TELA DE OPÇÕES", 280, 250, 30, YELLOW);
            const char *voltar = "Aperte BACKSPACE ou ESC para voltar";
            DrawText(voltar, LARGURA_TELA / 2 - MeasureText(voltar, 20) / 2, 300, 20, LIGHTGRAY);
        }

        // =====================================================
        // FADE DA TRANSIÇÃO (sempre por cima de tudo)
        // =====================================================

        if (transicao.ativa && !pausado)
            DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, transicao.alfa));

        // Acabou de apertar ESC: a tela deste frame vira o fundo do pause
        // (precisa ser antes do EndDrawing, enquanto a imagem ainda está na tela)
        if (capturarPausa) {

            fundoPausa = CapturarFundoPausa();
            capturarPausa = false;
            pausado = true;
        }

        EndDrawing();
    }

    // 3. FINALIZAÇÃO

    // AUTO SAVE: fechou a janela no meio do jogo
    // (no menu ou na intro não salva, para não apagar o progresso)
    if (tela == JOGANDO || tela == CORREDOR || tela == BATALHA)
        SalvarJogo(nickname, tela, playerPos, direcaoAtual, luciAcompanhando, &progresso);

    // (as texturas precisam ser liberadas ANTES de fechar a janela)
    if (texPontoSave.id > 0)
        UnloadTexture(texPontoSave);

    if (texMapaSala.id > 0)
        UnloadTexture(texMapaSala);

    if (fundoPausa.id > 0)
        UnloadTexture(fundoPausa);

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
