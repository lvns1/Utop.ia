// Biblioteca dos Ecos (Bloco 2), sala da Mestre e Bloco 3: funções e variáveis compartilhadas entre os arquivos de src/bloco2 e src/bloco3.
#ifndef UTOPIA_BLOCO2_BIBLIOTECA_H
#define UTOPIA_BLOCO2_BIBLIOTECA_H

#include <stdbool.h>
#include "raylib.h"
#include "raymath.h"
#include "../audio/audio.h"
#include "../bloco1/batalha.h"
#include "../bloco1/batalha_monitores.h"
#include "../bloco1/corredor.h"
#include "../bloco1/minigame_visual.h"
#include "../bloco1/objetivo.h"
#include "../bloco2/biblioteca_tipos.h"
#include "../core/config.h"
#include "../core/save.h"
#include "../core/telemetria.h"
#include "../core/texto.h"
#include "../core/tipos.h"
#include "../dados/conteudo_bloco1.h"
#include "../entidades/jogador.h"
#include "../entidades/luci_companion.h"
#include "../telas/intro.h"
#include "../telas/pause.h"
#include "../telas/ranking.h"
#include "../telas/resolucao.h"

// =============================================================
// FIM DO BLOCO 1: A SALA DA ALUCINAÇÃO MESTRE (arma, tutorial de combate e a luta-treino)
//   Vencidos os 3 monitores, a Mestre foge pela porta da direita. A sala dela usa o
//   motor da Biblioteca (o mesmo combate do Bloco 2), uma mecânica de cada vez:
//     ARMA      o Disparador de Dados num pedestal (a Luci apresenta)
//     TIRO      3 alvos de treino (mouse mira e atira)
//     ESQUIVA   ela atira anéis de balas lentas (ESPAÇO atravessa)
//     ABERTURA  depois de atacar ela cansa: escudo DOURADO = hora de atirar
//     PARRY     brilho VERMELHO = golpe corpo a corpo; V no impacto (o anel fecha)
//     LUTA      tudo junto; na metade, as 3 alucinações dos monitores ajudam
//   No tutorial a Luci não deixa a vida cair abaixo de MESTRE_TREINO_HP_MIN; na luta,
//   cair só reergue o Thomas (ensinar, não punir). Vencida: ranking e o átrio do Bloco 2.
// =============================================================
#define MESTRE_HP                380.0f   // uns 26 tiros do Disparador (AJUSTE AQUI)
#define MESTRE_RAIO              44.0f
#define MESTRE_FASE2             0.5f     // fração da vida em que as 3 alucinações chegam
#define MESTRE_DANO_BALA         7.0f
#define MESTRE_DANO_BALA_TREINO  4.0f
#define MESTRE_DANO_GOLPE        12.0f
#define MESTRE_TELEGRAFIA        1.0f     // brilho vermelho antes do golpe (no tutorial, x1,3)
#define MESTRE_INVESTIDA_VEL     420.0f
#define MESTRE_INVESTIDA_TEMPO   0.5f
#define MESTRE_DIST_GOLPE        150.0f   // ela para nesta distância antes de investir (o ritmo é sempre o mesmo)
#define MESTRE_ABERTURA          2.6f     // escudo aberto depois de uma rajada
#define MESTRE_RECUPERA          1.1f     // depois de um golpe que errou (brecha menor)
#define MESTRE_STUN              2.8f     // atordoada depois de um parry
#define MESTRE_TREINO_HP_MIN     0.35f    // no tutorial a vida do Thomas não cai abaixo disto
#define MESTRE_TREINO_HP_CHEFE   0.6f     // ... nem a da Mestre (a luta de verdade começa depois)
#define MESTRE_ALVOS             3
#define MESTRE_ESQUIVAS          2        // esquivas para passar da etapa
#define MESTRE_ACERTOS_ABERTURA  3        // tiros no escudo aberto para passar da etapa
#define MESTRE_PONTOS            PONTOS_POR_ACERTO
#define CARTAO_ETAPA_TEMPO       2.8f     // tutorial: o cartão de cada etapa fica no meio da tela (a Mestre espera)

typedef enum { MT_ARMA, MT_TIRO, MT_ESQUIVA, MT_ABERTURA, MT_PARRY, MT_PULSO, MT_MELHORIA, MT_LUTA, MT_FIM } EtapaMestre;   // PULSO e MELHORIA preparam o Bloco 2
typedef enum { MA_PAUSA, MA_RAJADA, MA_ABERTA, MA_APROXIMA, MA_TELEGRAFA, MA_INVESTE, MA_RECUPERA, MA_ATORDOADA } AcaoMestre;

typedef struct {
    bool ativo;
    EtapaMestre etapa;
    float tempoEtapa;
    Vector2 pos;
    float hp, flash, anim;
    bool visivel;               // ela aparece depois do treino de tiro
    float surgindo;             // > 0: materializando

    Vector2 alvo[MESTRE_ALVOS]; // alvos de treino (etapa do tiro)
    bool alvoVivo[MESTRE_ALVOS];

    AcaoMestre acao;
    float tempoAcao, cronoTiro;
    int padrao, disparos;       // rajada atual (0 anel, 1 leque, 2 espiral) e quantos tiros dela já saíram
    Vector2 dirGolpe;
    bool acertouGolpe;
    bool proximoGolpe;          // na luta: alterna rajada e golpe

    int esquivas, acertosAbertura, parries, quedas, golpesLevados;
    int pulsos;                 // etapa do PULSO: quantas vezes o Q foi usado
    float recargaPulsoAnt;      // para perceber o instante em que o Pulso foi disparado
    int dicas;                  // ajuda progressiva: quantas dicas a Luci já deu nesta etapa
    bool estavaEsquivando;
    int fase;                   // luta: 1 e 2
    float cronoOrbita;          // fase 2: as alucinações dos monitores atiram
    float avisoEscudo;          // "ESCUDO!" quando o tiro bate no escudo fechado
    float morrendo;
    const char *falaDepois;     // a fala da Luci da etapa (entra quando o cartão da etapa sai do meio da tela)
} MestreTreino;

typedef struct {
    JogadorRogue j;
    ProgressoBiblioteca *perm;  // o que não se perde (fica no main e vai para o save)

    // Sala atual e missão
    TipoSalaRogue tipoSala;
    int indiceRota;             // quantas salas da missão já foram abertas, menos 1 (-1 = átrio)

    // Mapa procedural (reformulação): o grafo da tentativa e onde o jogador está
    MapaRogue mapa;
    int noAtual;                // nó do mapa (-1 = fora da missão: átrio ou Bloco 3)
    int profundidade;           // camada do nó atual (decide a dificuldade)
    LadoPorta ladoEntrada;      // por onde o jogador entrou nesta sala
    PortaMissao portas[MAX_SAIDAS];
    int numPortas;
    bool mapaAberto;            // M: o mapa grande (o jogo espera)
    float fatorVidaInimigos, fatorRitmoInimigos, fatorDanoSala;

    // Parede corrompida (passagem para a sala secreta)
    bool temParedeSecreta;
    int paredeSecreta;          // índice em PAREDE_SECRETA_ROGUE
    float hpParede;
    bool passagemAberta;
    int paredeFalsa;            // a pista alucinada (Luci ou scanner) aponta para esta (-1 = nenhuma)
    float marcaParede;          // scanner: tempo que a marca "PASSAGEM?" fica

    // Objetivo da sala de combate
    ObjetivoCombate objetivoSala;
    int ondaAtual, numOndas;
    float avisoOnda;            // > 0 = a próxima onda está chegando
    float tempoObjetivo, tempoObjetivoMax;
    float reforco;
    int alvoPrioritario;        // índice em ini (-1 = nenhum)
    Vector2 posMecanismo[NUM_MECANISMOS];
    float progressoMecanismo[NUM_MECANISMOS];
    bool mecanismoDesligado[NUM_MECANISMOS];
    int numMecanismos;

    // Armadilhas e áreas
    Armadilha arm[MAX_ARMADILHAS];
    int numObstFixos;           // as estantes da sala (o resto do vetor obst são barreiras das armadilhas)
    AreaDano areas[MAX_AREAS];
    // Tornado de Dados (Katana): redemoinho que anda, puxa e fere
    bool torAtivo;
    Vector2 torPos, torDir;
    float torVida, torTick, torDano;
    // Surto (Katana + Raio): os relâmpagos ficam visíveis por um instante (só visual)
    struct { Vector2 de, para; float vida; unsigned int semente; } raioK[3];

    // Recompensa no chão (núcleo de melhoria, arsenal, restauração)
    bool recompensaAtiva;
    TipoRecompensa recompensaTipo;
    Vector2 recompensaPos;

    // Escolha 1 de 3
    bool escolhaAberta;
    bool escolhaTreino;         // a escolha é a de TREINO da sala da Mestre (2 opções, nada é gasto nem guardado)
    bool escolhaTreinoFeita;    // o jogador confirmou a escolha de treino (a Mestre avança a etapa)
    OpcaoEscolha opcoes[3];
    int numOpcoes, selecaoEscolha;
    ModoEscolha modoEscolha;
    float animEscolha;
    bool escolhaGratis;         // a escolha veio de um evento (não conta como recompensa da sala)

    // Eventos, descanso e desafio
    bool eventoAberto;
    TipoEvento evento;
    int varianteEvento;         // fonte falsa, resposta da IA, memória...
    int selecaoEvento;
    bool eventoResolvido;       // mostrando o resultado
    bool eventoUsado;           // a sala já deu o que tinha
    char resultadoEvento[260];
    bool eventoCombate;         // Banco de Dados Instável: aceitou a onda extra
    Vector2 posObjetoSala;      // altar do evento, poltrona, console do desafio
    TipoDesafioSala desafioSala;
    int estadoDesafio;          // 0 = esperando, 1 = em andamento, 2 = vencido, 3 = falhou
    int contadorDesafio;
    float tempoDesafioSala, cronoDesafio;
    unsigned int memoriasVistas;    // nesta sessão (registros de memória da Luci)
    float mensagemSistema;      // mensagem de sistema corrompido no começo da sala
    char textoSistema[96];
    bool registroSala;          // um arquivo de ameaça para ler (narrativa)
    bool registroSalaLido;
    int registroSalaId;
    Vector2 posRegistroSala;
    int elitesDerrotados;
    int escolhaPendente;        // fim de um evento: abre esta escolha (-1 = nenhuma; senão, um ModoEscolha)
    unsigned int eventosVistos; // nesta tentativa (não repete o mesmo evento)
    int paredeMarcada;          // scanner: onde ele mostra "PASSAGEM?" (-1 = nenhuma)
    int combates;               // salas de combate abertas nesta tentativa (decide a dificuldade do encontro)
    int encontro;               // modelo de encontro da sala atual (-1 = nenhum)
    int ultimoEncontro;         // não repete o mesmo encontro em salas seguidas
    int educativas;             // terminais já vistos nesta tentativa (decide o conceito)
    bool limpa;                 // a porta de cima está aberta
    bool terminalLido;
    bool recompensaPega;
    TipoItem pedestal[NUM_PEDESTAIS];
    TipoArma armaPedestal[NUM_PEDESTAIS];  // ARMA_DISPARADOR = o pedestal tem um item
    TipoArma estoqueArmas[ARMAS_NA_LOJA];  // armas à venda no Byteco
    int ofertaItem;                        // item em OFERTA do Byteco nesta volta ao átrio (-30%)
    unsigned int armasOferecidas;          // nesta tentativa (evita oferecer sempre a mesma)
    bool golpeSecundario;                  // o golpe atual veio de um efeito secundário (não encadeia)
    AnelEfeito aneis[MAX_ANEIS];

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

    // Reroll das opções: o que já foi mostrado não volta na mesma escolha
    unsigned long long rerollUsados;
    unsigned int rerollItens, rerollArmas;

    // Troca de arma: com as duas vagas cheias o jogador escolhe qual arma substituir
    bool trocaArmaAberta;
    TipoArma armaNova;
    int trocaOrigem;            // 0 = recompensa, 1 = loja do Byteco, 2 = pedestal
    int trocaPedestal;
    int trocaSel;
    float animTroca;

    // Enciclopédia do átrio
    bool enciclopediaAberta;
    int selecaoEnciclopedia;

    // Desafio educativo aberto (o combate pausa)
    bool desafioAberto;
    int desafio;                // índice em DESAFIOS
    int alvoDesafio;            // guardião selado (índice em ini)
    int selecaoDesafio;
    float tempoDesafio;
    float tempoRespostaSelo;    // cronômetro do desafio (TEMPO_RESPOSTA_SELO até 0)
    bool tempoEsgotadoSelo;     // a resposta foi dada pelo tempo (conta como erro)
    bool respondido;            // mostrando a explicação
    bool acertouDesafio;
    bool opcaoRiscada[4];       // eliminada pela Lente ou pela tentativa errada (Manual)
    char dicaDesafio[160];      // dica da Lente ou da Luci
    bool dicaDaLuci;            // a dica que está na tela veio da Luci
    bool luciMentiu;            // ... e era alucinada
    int opcaoDica;              // opção que a dica apontou (-1 = nenhuma)
    bool verificouDica;         // o jogador verificou (V)
    bool usouManual;
    unsigned int desafiosUsados;    // desafios desta tentativa (um bit cada)
    int desafiosRespondidos;        // nesta tentativa: a Luci fica menos confiável a cada um
    const char *falaPendente;       // fala da Luci para quando não houver outra conversa
    const Dialogo *conversaPendente;    // conversa para quando a sala terminar de aparecer (chefe)
    int objConversa;

    // ALUCINADOR
    ChefeRogue chefe;
    bool vitoriaPendente;           // venceu o Alucinador: o main abre o ranking
    MestreTreino mestre;            // fim do Bloco 1: a sala da ALUCINAÇÃO MESTRE
    int pontosRun;                  // pontuação atual (os registros do Bloco 3 mostram)
    bool saiuSilogista;             // o jogador usou a saída da Câmara (sem a recompensa)
    int seladosTentativa;           // Guardiões com pergunta já sorteados nesta tentativa
    float segurandoSaida;           // quanto tempo o E já está segurado no console de saída
    bool mestreVencidoPendente;     // venceu a Mestre: o main marca o fim do Bloco 1 e abre o ranking

    // O SILOGISTA (Câmara da Inferência)
    Silogista silogista;

    // A Luci: estabilidade (a do main, compartilhada com o Bloco 1), hábitos, scanner e contradições
    int *estabilidade;              // o main aponta para a dele; sozinha (testes), usa estabilidadeLocal
    int estabilidadeLocal;
    char luciMsg[128];              // recado curto da Luci perto do HUD (não pausa o jogo)
    float lentidao;                 // câmera lenta do tutorial (0 = normal; o main multiplica o dt por 1 - lentidao)
    float luciMsgTempo;
    float tempoSalaAtual;       // segundos na sala atual da missão (as dicas da 1ª missão usam)
    unsigned int conceitosLigados;  // inimigos cuja ligação com os monitores a Luci já comentou (só nesta sessão; não vai para o save)
    double entradaAtrio;        // quando entrou no átrio (telemetria: quanto tempo o jogador fica lá antes da 1ª missão)
    bool escolhaPrimeiraMissao; // a escolha é a PRIMEIRA melhoria da primeira missão (3 opções fixas e explicadas)
    ProgressoCorredor *progRef;     // o progresso da run (as queimaduras da katana precisam dele para matar)
    int katanaGolpes;               // Katana: conta os golpes (o Raio dispara no 3º)
    int ecosDestruidos;
    int habitoConfiar, habitoVerificar;
    bool comentouConfiar, comentouVerificar;
    float scanner, recargaScanner;  // scanner > 0: marcas na tela
    bool scannerErrou;              // esta varredura alucinou (troca as marcas)
    float luciEco;                  // a própria Luci marcada como ECO por um instante (pista)
    float contradicao;              // > 0: a Luci está contradizendo a sala no objetivo
    float proximaContradicao;
    int textoContradicao;

    // BLOCO 3
    LuciChefe luci;
    IndiceFinal indice;             // calculado ao entrar (dificuldade) e ao vencer (final)
    int registrosLidos;             // um bit por registro de memória
    int registroAtual;
    bool nucleoVisto;
    bool finalBom;
    bool resultadoAberto;
    float tempoResultado;
    int pontosFinais;

    // Reformulação do Bloco 3: efeitos de tela e de impacto
    float tremorForte;          // o tremor do parry (mais forte que o do dano)
    float zoomImpacto;          // zoom rápido no choque
    Vector2 focoZoom;
    float transicaoNucleo;      // cutscene da porta do átrio (o jogo espera)
    float transicaoMestre;      // cutscene da ponte Bloco 1 -> Biblioteca (a Mestre foge, a porta abre; o jogo espera)
    bool trocouCenaMestre;      // já trocou a sala da Mestre pelo átrio (acontece no meio da cutscene)
    float tituloBloco;          // > 0 = cartão "BLOCO 2" na tela (chegada vinda do Bloco 1)
    bool entrouNucleo;
    float flashNegativo;        // inversão de paleta
    // CENA DA LUCI (Bloco 3): 1 = ela deixa o Thomas (sala de transição); 2 = vira a chefe (arena). O jogo espera.
    int cenaLuciTipo;
    float cenaLuci;             // tempo que falta da cena
    const Dialogo *conversaDepoisCena;
    float estaticaTela;         // ruído de estática
    float portaNucleoAbrindo;   // a porta do Bloco 3 se abrindo no átrio
    char erroTermo[64];         // feedback de erro na verificação (aberração cromática na interface)
    char erroExplicacao[160];
    float tempoErro;
    TextoFlutuante textos[MAX_TEXTOS_FLUTUANTES];
    bool bonusSilogistaSessao;  // venceu o Silogista bullet-hell nesta sessão (o Selo amplificado volta no núcleo)

    // Expansão educativa: o laboratório da sala atual e o que vale para a sessão (nada disso vai para o save)
    LabEdu lab;
    int cargaLuci;                      // carga de ajuda da Luci: LAB_CARGA_POR_ESTAB = -1 de estabilidade
    unsigned int labVistas[NUM_LABS];   // missões já sorteadas (evita repetição recente)
    unsigned int labIntroVista;         // categorias já apresentadas
    int labFeitos, labEstrelas;         // desempenho nos laboratórios (ajusta a dificuldade)

    // Expansão da progressão: o mini-chefe Janela de Contexto (só existe na sala dele)
    JanelaContexto janela;

    // Terminais: seletor de chefes e confronto direto (transitório, fora do save)
    bool seletorAberto;
    int seletorFoco;
    float seletorAnim;
    bool modoSeletor;               // o confronto atual veio do seletor (sem mapa; volta ao átrio)
    int chefeSeletor;               // ChefeSeletor (-1 = nenhum)
    int especialDireto;             // encontro especial fora do mapa (ESP_*, -1 = nenhum)
    bool recompensaSeletor;         // esta vitória ainda dá a recompensa do chefe
    bool seletorVencido;            // venceu: o átrio volta depois dos diálogos de vitória
    float seletorRetorno;
    int itensGuardados[NUM_ITENS];  // a mochila do átrio, devolvida ao voltar
    TipoArma armaGuardada, armaBaseGuardada;
    bool usandoExtraGuardado, seloGuardado, seloAmplificadoGuardado, temMochilaGuardada;

    bool morreu;
    float tempoMorte, tremor, pausaImpacto, fadeEntrada;
    char aviso[96];             // mensagem curta no meio da tela ("+3 FRAGMENTOS")
    float tempoAviso;
    float faixaVirada;          // > 0 = faixa da virada de fase de um chefe (no padrão das quedas da Luci)
    float flashDanoJog;         // bordas vermelhas quando o Thomas leva um golpe
    char faixaTitulo[48], faixaSub[80];
    Color faixaCor;
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
#define NUM_LAYOUTS_COMBATE 10
#define LAYOUT_AREA_ABERTA  6

// ---------- Encontros (quem aparece em cada sala de combate) ----------
//   Cada sala de combate sorteia um modelo compatível com a dificuldade (nível 1 = 1º
//   combate da missão, 2 = intermediário, 3 = avançado), sem repetir o da sala anterior.
//   As quantidades variam entre o mínimo e o máximo; no nível 3, os grupos com mais de
//   um inimigo ganham mais um. A partir do 2º combate entra também um Guardião selado.
typedef struct {
    TipoInimigo tipo;
    int min, max;
    int onda;                   // reformulação: 0 = surge na entrada, 1 e 2 = ondas seguintes
    bool elite;                 // nasce como elite
} GrupoEncontro;

typedef struct {
    const char *nome;
    int nivelMin, nivelMax;
    int numGrupos;
    GrupoEncontro grupo[6];
    bool salaElite;             // reformulação: só aparece nas salas de ELITE
} EncontroDef;
#define NUM_ENCONTROS            17
#define DIST_SURGIR_JOGADOR      170.0f   // ninguém nasce em cima da entrada do jogador
#define BANDO_SPAMMERS           4        // Spammers nascem em bandos deste tamanho

// Perguntas dos eventos educativos (FONTE DUVIDOSA e RESPOSTA SUSPEITA): 3 opções, uma certa
typedef struct {
    const char *texto;
    const char *opcoes[3];
    int correta;
    const char *explicacao;
} QuizEvento;
#define NUM_QUIZ_FONTES 3
#define NUM_QUIZ_RESPOSTAS 3

typedef struct {
    const char *antes;          // NULL = não depende de nada; senão, o que ele disse antes (contradição)
    const char *texto;
    bool confiavel;
    const char *dica;
    const char *explicacao;
} AfirmacaoChefe;
#define NUM_AFIRMACOES_ALUCINADOR 24

typedef struct {
    const char *titulo;
    const char *premissas;
    const char *pergunta;
    const char *validos[2][2];          // símbolo, glossário
    const char *falacias[3][3];         // símbolo, glossário, justificativa do erro
    const char *premissa[2];            // símbolo, glossário
    const char *regraAcerto;            // micro-feedback verde
} RodadaSilogistaBH;

// Silogismo: as frases na ordem certa (P1, P2, C) e a peça que não se sustenta (-1 = argumento válido)
typedef struct {
    const char *frase[NUM_ENCAIXES];
    int invalida;
} SilogismoDef;

// Circuito proposicional. Cada porta lê "sinais": 0 = p, 1 = q, 2+k = saída da porta k.
// A última porta liga no núcleo. Outra expressão = outra tabela (sem interpretador).
typedef enum { OP_E, OP_OU, OP_NAO, OP_SE, OP_SSE } OperadorLogico;

typedef struct {
    OperadorLogico op;
    int entrada[2];             // a 2ª é ignorada no NÃO
    Vector2 pos;
} PortaLogica;

typedef struct {
    const char *expressao;      // com os símbolos de DesenharLogica
    bool inicial[2];            // p e q no começo
    Vector2 posTerminal[2];
    int numPortas;
    PortaLogica porta[MAX_PORTAS_CIRCUITO];
    Vector2 posNucleo;
    const char *dicaErro;       // Luci, quando o núcleo recebe o valor errado
} CircuitoDef;
#define NUM_PONTOS_PECAS_SIL 8

enum { CONCEITO_FUNDAMENTOS, CONCEITO_ALUCINACOES, CONCEITO_PRATICAS, CONCEITO_LOGICA, NUM_CONCEITOS };

// ---------- Melhorias permanentes (Byteco) ----------

enum { MELHORIA_VIDA, MELHORIA_ATAQUE, MELHORIA_ENERGIA, MELHORIA_VELOCIDADE, MELHORIA_REGEN, MELHORIA_REROLL, NUM_MELHORIAS };

#define MELHORIA_VEL_NIVEL       0.04f    // +velocidade por nível
#define MELHORIA_REGEN_NIVEL     0.7f     // +EP por segundo por nível

// ---------- Desafios educativos (Guardião selado) ----------
// Verdadeiro ou falso, múltipla escolha e "ache a inconsistência" numa resposta de IA.
// Só aparecem desafios de conceitos que o jogador já viu.

typedef enum { DESAFIO_VF, DESAFIO_ESCOLHA, DESAFIO_INCONSISTENCIA, DESAFIO_FONTE, DESAFIO_PROMPT } TipoDesafio;

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
#define NUM_DESAFIOS 16
#define NUM_MEMORIAS 5
#define NUM_AMEACAS 5
#define NUM_MENSAGENS_SISTEMA 6

// Virada de fase de um chefe: faixa com o título falhando, vinheta na cor dele e o golpe grave
#define FAIXA_VIRADA_TEMPO 1.7f

// Atalhos de ajuda escritos no rodapé do desafio (também clicáveis)
#define X_LENTE_DESAFIO   84
#define X_LUCI_DESAFIO    334
#define Y_AJUDA_DESAFIO   (70 + 440 - 46)

// O Alucinador só leva dano na fase 1 ou com o escudo aberto (fase 2)
// ADAPTAÇÃO: com muitas melhorias, itens e níveis permanentes os chefes (menos a Janela, já equilibrada) resistem mais.
// Cada nível de melhoria vale +3%, cada passivo +4%, cada nível permanente +3%. Vai de 1,0 até no máximo 2,0.
#define ADAPT_POR_MELHORIA  0.03f
#define ADAPT_POR_PASSIVO   0.04f
#define ADAPT_POR_PERMANENTE 0.03f
#define ADAPT_MAX           2.0f

#define SIGILO_ATRASO 0.6f
#define NUM_FRASES_LUCI 12
#define CONTEXTO_CERTOS 3       // os 3 primeiros são o pedido claro
// =============================================================
// CENA DA LUCI (Bloco 3) — no próprio jogo, sem tela preta
//   1) SEPARAÇÃO (sala de transição): ela desacelera a órbita em volta do Thomas, para na frente
//      dele e se afasta até a porta do núcleo; o fio de luz que ligava os dois estica e se rompe.
//      Ela atravessa a porta e some: a sala fica vazia (ela espera o Thomas na arena).
//   2) TRANSFORMAÇÃO (arena): pequena e azul no meio da arena, ela se corrompe (dados magenta
//      convergindo, rachaduras no chão, tremor), explode em estática, cresce e vira a chefe.
//      O nome dela aparece e a barra de vida enche. Depois vem a conversa da revelação.
//   Qualquer tecla (ENTER, ESPAÇO, E ou clique) pula a cena.
// =============================================================
#define CENA_SEPARACAO          4.2f
#define CENA_TRANSFORMACAO      5.4f
#define CENA_SEP_ORBITA         0.9f     // desacelerando a volta
#define CENA_SEP_PARADA         1.5f     // parada na frente do Thomas
#define CENA_SEP_ROMPE          2.75f    // o fio se rompe
#define CENA_SEP_CHEGA          3.0f     // chegou na frente da porta do núcleo
#define CENA_SEP_SOBE           3.3f     // começa a subir para a porta
#define CENA_SEP_ENTRA          3.85f    // atravessou a porta e sumiu
#define CENA_TR_CORROMPE        1.0f     // começa a corrupção
#define CENA_TR_EXPLODE         3.0f     // a explosão de estática
#define CENA_TR_CRESCE          3.6f     // terminou de crescer
#define CENA_TR_TITULO          3.7f     // o nome e a barra


// ---------- Etapa 5: falas da Luci nas viradas e alguns comentários na luta ----------
// Viradas (sempre aparecem): 1ª queda, 2ª queda, vida baixa na 3ª barra e a derrota final.
// Comentários (parry, investida que acerta, orbe rebatido, superaquecida): no máximo um a cada 6 s,
// e só se não houver outro recado na tela.
enum { FALA_QUEDA1, FALA_QUEDA2, FALA_VIDA_BAIXA, FALA_DERROTA, NUM_FALAS_VIRADA };

#define RET_RECUSAR_ESCOLHA ((Rectangle){ 310, 488, 180, 26 })   // y: dica (438) -> REROLAR (454) -> RECUSAR (488), sem se tocarem
#define RET_REROLAR_ESCOLHA ((Rectangle){ 290, 454, 220, 26 })

// ---------- Eventos, sala de leitura e console de desafio ----------

typedef struct {
    const char *titulo;
    char texto[320];
    int numOpcoes;
    char rotulo[3][160];
    char efeito[3][200];
    bool disponivel[3];
} TelaEvento;

#define RET_FECHAR_EVENTO ((Rectangle){ 698, 88, 24, 20 })

// ---------- 1. LABORATÓRIO DE PROMPTS (Modo Prompt) ----------

#define PB(p) (1u << (p))

typedef struct {
    const char *titulo;
    const char *missao;
    int dificuldade;                            // 1 a 3
    int conceito;                               // só aparece se o conceito já foi apresentado
    unsigned int necessarias;                   // partes que a missão exige (bits de ParteNoPrompt)
    const char *alt[NUM_PARTES_PROMPT][3];      // as alternativas (escolha 1 a 3)
    signed char nota[NUM_PARTES_PROMPT][4];     // [0] = não incluir; 2 ideal, 1 aceitável, 0 vaga, -1 inadequada
    bool vago;                                  // começa com um pedido vago já enviado (refinamento)
    signed char inicial[NUM_PARTES_PROMPT];     // o pedido vago
    signed char contra[2][4];                   // combinações contraditórias: parte A, escolha A, parte B, escolha B
    const char *resp[NUM_DIAG_PROMPT];          // respostas simuladas, uma por diagnóstico
    signed char honestaParte, honestaAlt;       // -1: a missão não simula alucinação
    const char *respAlucinada;                  // a resposta convincente e inventada
    const char *trechoFalso;                    // o que foi inventado (aparece ao desconfiar)
    const char *dicas[3];                       // dicas graduais da Luci
    const char *dicaFalsa;                      // a dica alucinada
    const char *explicacao;                     // passo 4: reforço do conceito
} MissaoPrompt;

#define SEM_CONTRA { { -1, 0, 0, 0 }, { -1, 0, 0, 0 } }
#define NUM_MISSOES_PROMPT ((int)(sizeof(MISSOES_PROMPT) / sizeof(MISSOES_PROMPT[0])))

// ---------- 2. ARQUIVO CORROMPIDO ----------

enum { AF_CORRETA, AF_INCORRETA, AF_INVENTADA, AF_NAO_CONFIRMAVEL, NUM_TIPOS_AFIRMACAO };

typedef struct {
    const char *titulo;
    const char *pergunta;
    int dificuldade, conceito;
    const char *af[4];
    signed char tipo[4];
    int suspeita;
    const char *metodos[3];
    int metodoCerto;
    const char *dicas[3];
    const char *dicaFalsa;
    const char *explicacao;
} DesafioArquivo;
#define NUM_DESAFIOS_ARQUIVO ((int)(sizeof(DESAFIOS_ARQUIVO) / sizeof(DESAFIOS_ARQUIVO[0])))

// ---------- 3. NÚCLEO NEURAL ----------

enum { REL_IRRELEVANTE, REL_UTIL, REL_ESSENCIAL };
enum { NUCLEO_BOA, NUCLEO_FALTOU, NUCLEO_TRUNCADA, NUCLEO_RUIDOSA };

typedef struct {
    const char *texto;
    int tokens;                 // tokens fictícios (abstração educativa)
    int relevancia;
} BlocoContexto;

typedef struct {
    const char *titulo;
    const char *tarefa;
    int dificuldade;
    int capacidade;
    BlocoContexto bloco[LAB_MAX_ITENS];
    int numBlocos;
    const char *resp[4];        // boa, faltou o essencial, truncada (excesso), com ruído
    const char *dicas[3];
    const char *dicaFalsa;
    const char *explicacao;
} DesafioNucleo;
#define NUM_DESAFIOS_NUCLEO ((int)(sizeof(DESAFIOS_NUCLEO) / sizeof(DESAFIOS_NUCLEO[0])))

// ---------- 4. FIREWALL ÉTICO ----------

typedef struct {
    const char *titulo;
    const char *caso;
    int dificuldade;
    const char *problemas[3];
    int problemaCerto;
    const char *medidas[3];
    int medidaCerta;
    const char *dicas[3];
    const char *dicaFalsa;
    const char *explicacao;
} CasoVies;
#define NUM_CASOS_VIES ((int)(sizeof(CASOS_VIES) / sizeof(CASOS_VIES[0])))

enum { ACAO_PERMITIR, ACAO_BLOQUEAR, ACAO_ANONIMIZAR, NUM_ACOES_PACOTE };

typedef struct {
    const char *texto;
    int acao;
    const char *porque;
} PacoteDados;
#define NUM_PACOTES_DADOS ((int)(sizeof(PACOTES_DADOS) / sizeof(PACOTES_DADOS[0])))

// ---------- 5. CÂMARA DE PESQUISA ----------

enum { MIDIA_IMAGEM, MIDIA_VIDEO, MIDIA_AUDIO };
enum { VEREDITO_CONFIAVEL, VEREDITO_FALSO, VEREDITO_INCONCLUSIVO, NUM_VEREDITOS };

typedef struct {
    const char *texto;
    bool alerta;                // é um sinal de alerta de verdade (e não só aparência)
} PistaPesquisa;

typedef struct {
    const char *titulo;
    const char *descricao;
    int midia;
    int dificuldade;
    PistaPesquisa pista[5];
    const char *ferramenta[3];
    const char *resultado[3];
    int veredito;
    const char *dicas[3];
    const char *dicaFalsa;
    const char *explicacao;
} CasoPesquisa;
#define NUM_CASOS_PESQUISA ((int)(sizeof(CASOS_PESQUISA) / sizeof(CASOS_PESQUISA[0])))

// Luz de cada encaixe depois do envio: o retorno imediato da montagem (tentativa e erro)
typedef enum { LUZ_IDEAL, LUZ_OK, LUZ_DISPENSAVEL, LUZ_VAGA, LUZ_FALTOU, LUZ_ATRAPALHOU, LUZ_CONFLITO, NUM_LUZES } LuzParte;

// ---------- Botões (mouse e teclado usam a mesma lista) ----------

typedef enum {
    LB_COMECAR, LB_SAIR, LB_CONCLUIR, LB_SELETOR, LB_ENVIAR, LB_ACEITAR, LB_REFAZER, LB_DESCONFIAR, LB_ENCERRAR,
    LB_DICA, LB_ANALISAR, LB_REFORMULAR, LB_VERIFICAR, LB_CHECAR, LB_USAR_SUGESTAO, LB_DESCARTAR_SUGESTAO,
    LB_ITEM, LB_OPCAO, LB_CONFIRMAR, LB_PROCESSAR, LB_LIMPAR, LB_AJUSTAR, LB_FERRAMENTA, LB_PACOTE, LB_VEREDITO,
    LB_PECA
} AcaoLab;

typedef enum { EST_BOTAO, EST_LUCI, EST_ITEM, EST_SELETOR, EST_ENCAIXE, EST_PECA } EstiloBotaoLab;

typedef struct {
    Rectangle r;
    AcaoLab acao;
    int param;
    bool ativo;
    bool marcado;
    int custo;                  // carga de ajuda (botões da Luci)
    EstiloBotaoLab estilo;
    char rotulo[64];
    const char *texto;          // texto longo dos cartões (afirmações, blocos, pistas...)
    Color cor;
    int fonte;                  // tamanho do texto do cartão
    bool chip;                  // o rótulo vai num selo à direita (tokens do Núcleo); encaixe: peça trocada pela Luci
    int luz;                    // encaixe do laboratório de prompts: LuzParte (-1 = sem luz)
} BotaoLab;

#define LAB_PAINEL_X     28
#define LAB_PAINEL_Y     30
#define LAB_PAINEL_W     744
#define LAB_PAINEL_H     508
#define RET_FECHAR_LAB   ((Rectangle){ 744, 36, 22, 20 })     // X do canto (o mesmo que ESC)
#define LAB_Y_LUCI       410
#define LAB_Y_BOTOES     470
#define LAB_ALT_BOTAO    26

// Laboratório de prompts: os 4 encaixes formam a "frase" do pedido, lado a lado
#define LAB_ANIM_PECA   0.28f    // segundos da peça voando até o encaixe

// O que está ao alcance do E agora (-1 = nada)
typedef enum { ALVO_NADA, ALVO_BYTE, ALVO_BYTECO, ALVO_TERMINAL, ALVO_PEDESTAL, ALVO_ENCICLOPEDIA, ALVO_PECA, ALVO_ENCAIXE,
               ALVO_REGISTRO, ALVO_CONTEXTO, ALVO_RECOMPENSA, ALVO_OBJETO_SALA, ALVO_REGISTRO_SALA, ALVO_LAB,
               ALVO_ARMA_MESTRE, ALVO_TERMINAL_SELETOR, ALVO_TERMINAL_BLOCO3 } AlvoRogue;
#define SAIDA_SILOGISTA_RAIO     46.0f
#define SAIDA_SILOGISTA_SEGURAR  0.8f     // segurar E (evita sair sem querer no meio da luta)
// ---------- Portas com interação (tecla E) ----------
// As portas não levam mais o jogador só de encostar: perto de uma porta aberta aparece
// "APERTE E PARA ENTRAR/SAIR" e só o E atravessa. A mesma função serve à lógica e ao desenho.
typedef enum { PORTA_NENHUMA, PORTA_ATRIO_MISSAO, PORTA_ATRIO_CORREDOR, PORTA_ATRIO_NUCLEO,
               PORTA_TRANSICAO_LUCI, PORTA_DA_MISSAO, PORTA_SECRETA } PortaInteracao;

#define DISTANCIA_PORTA_E   14.0f   // folga além do corpo do Thomas (AJUSTE AQUI)

// Compra um item do Byteco. Devolve true se comprou.
#define OFERTA_DESCONTO_PERC 30
#define RET_CANCELAR_TROCA ((Rectangle){ 270, 512, 260, 30 })

// Enciclopédia do átrio: W/S escolhe, E lê o texto completo, BACKSPACE/ESC sai
#define RET_FECHAR_ENCICLOPEDIA ((Rectangle){ 656, 58, 26, 22 })

typedef enum { ROGUE_NADA, ROGUE_SAIR, ROGUE_VITORIA, ROGUE_FIM_JOGO, ROGUE_MESTRE_VENCIDO } EventoRogue;
// ---------- Tutorial da sala da Mestre ----------
// Cada etapa abre com um CARTÃO no meio da tela (a tecla, uma demonstração animada e uma frase curta); depois ele
// sobe para o topo e fica lá com o progresso. Na hora de agir, a tecla aparece em cima do Thomas ("AGORA!").
typedef struct { const char *titulo; const char *tecla; bool mouse; const char *regra; } LicaoMestre;
#define NUM_TEXTOS_CORROMPIDOS 8
// =============================================================
// DESENHO DOS TERMINAIS DE ATALHO E DO SELETOR DE CHEFES
// =============================================================
// Linguagem visual de ATALHO (igual nos três terminais, diferente dos terminais educativos azuis):
// console DOURADO com base listrada de aviso, tela com setas ">>" correndo, placa "ATALHO >>"
// flutuando com o destino e uma trilha de setas que corre do terminal até a porta a que ele se refere.

#define COR_ATALHO ((Color){ 255, 176, 0, 255 })

// ----- Variáveis compartilhadas -----
extern const LayoutSala LAYOUTS_COMBATE[10];
extern const Rectangle ZONAS_LIVRES_ROGUE[NUM_LADOS];
extern const EncontroDef ENCONTROS[17];
extern const LayoutSala LAYOUT_ATRIO;
extern const LayoutSala LAYOUT_EDUCATIVA;
extern const LayoutSala LAYOUT_RECOMPENSA;
extern const LayoutSala LAYOUT_CHEFE;
extern const LayoutSala LAYOUT_TRANSICAO;
extern const LayoutSala LAYOUT_EVENTO;
extern const LayoutSala LAYOUT_SECRETA;
extern const Vector2 POS_OBJETO_SALA;
extern const Vector2 POS_REGISTRO_SALA;
extern const QuizEvento QUIZ_FONTES[3];
extern const QuizEvento QUIZ_RESPOSTAS[3];
extern const char *NOMES_PAREDES_SECRETAS[];
extern const Vector2 POS_REGISTROS[];
extern const Vector2 POS_LUCI_NUCLEO;
extern const Dialogo dialogoByte;
extern const Dialogo dialogoByteDepois;
extern const Dialogo dialogoByteco;
extern const Dialogo dialogoTerminalAlucinacao;
extern const Dialogo dialogoTerminalPraticas;
extern const Vector2 POS_ARMA_MESTRE;
extern const Vector2 POS_MESTRE_INICIO;
extern const Vector2 POS_ALVOS_MESTRE[MESTRE_ALVOS];
extern const Dialogo dialogoMestreChegada;
extern const Dialogo dialogoMestreArma;
extern const Dialogo dialogoMestreVitoria;
extern const Dialogo dialogoChegadaBiblioteca;
extern const char *FALAS_MESTRE_ETAPA[MT_FIM + 1];
extern const Dialogo dialogoAlucinadorIntro;
extern const Dialogo dialogoAlucinadorFase2;
extern const Dialogo dialogoAlucinadorVitoria;
extern const AfirmacaoChefe AFIRMACOES_ALUCINADOR[24];
extern const Dialogo dialogoSilogistaIntro;
extern const Dialogo dialogoSilogistaSofisma;
extern const Dialogo dialogoSilogistaCircuito;
extern const Dialogo dialogoSilogistaRodada2;
extern const Dialogo dialogoSilogistaVulneravel;
extern const Dialogo dialogoSilogistaVitoria;
extern const Dialogo dialogoSilogistaIntroBH;
extern const Dialogo dialogoSilogistaVitoriaBH;
extern const RodadaSilogistaBH RODADAS_SILOGISTA_BH[];
extern const RodadaSilogistaBH RODADAS_BH_EXTRA[3][2];
extern const Dialogo DIALOGOS_BH_EXTRA[3][2];
extern int g_varianteSilBH[];
extern const char *FALA_MONTAGEM_CONCLUSAO;
extern const char *FALA_MONTAGEM_PREMISSAS;
extern const char *FALA_SOFISMA_ERRO;
extern const char *NOMES_ENCAIXES[NUM_ENCAIXES];
extern const char *PAPEIS_ENCAIXES[NUM_ENCAIXES];
extern const SilogismoDef SILOGISMO_VALIDO;
extern const SilogismoDef SOFISMA_SILOGISTA;
extern const char *SIMBOLOS_OPERADORES[5];
extern const char *NOMES_OPERADORES[5];
extern const CircuitoDef CIRCUITO_SILOGISTA;
extern const Vector2 POS_ENCAIXES[NUM_ENCAIXES];
extern const Vector2 PONTOS_PECAS_SIL[8];
extern const char *NOMES_CONCEITOS[NUM_CONCEITOS];
extern const char *RESUMOS_CONCEITOS[NUM_CONCEITOS];
extern const Dialogo *TEXTOS_CONCEITOS[NUM_CONCEITOS];
extern const char *NOMES_MELHORIAS[NUM_MELHORIAS];
extern const char *EFEITOS_MELHORIAS[NUM_MELHORIAS];
extern const char *NOMES_TIPOS_DESAFIO[5];
extern const DesafioEdu DESAFIOS[16];
extern const char *FALAS_LUCI_CORROMPIDA[3];
extern const Dialogo dialogoTerminalVies;
extern const Dialogo dialogoTerminalFontes;
extern const Dialogo dialogosMemoria[5];
extern const Dialogo dialogosAmeaca[5];
extern const char *MENSAGENS_SISTEMA[6];
extern const char *FALAS_LUCI_SALA[NUM_TIPOS_SALA];
extern const LayoutSala *layoutCombateAtual;
extern bool g_devInvencivel;
extern int g_devIndice;
extern const char *CONTRADICOES_LUCI[6];
extern const Dialogo dialogoNucleoEntrada;
extern const Dialogo dialogoNucleoAberto;
extern const Dialogo dialogoLuciFase2;
extern const Dialogo dialogoLuciFase3;
extern const Dialogo dialogoFinalBom;
extern const Dialogo dialogoFinalRuim;
extern char textoRegistros[3][2][320];
extern char textoRevelacao[2][320];
extern const char *FRASES_LUCI_CHEFE[12];
extern const char *CONTEXTO_LUCI[NUM_CONTEXTO_LUCI];
extern const char *FALAS_VIRADA_LUCI[NUM_FALAS_VIRADA][2];
extern const char *NOMES_PENALIDADE[];
extern const char *CONCEITOS_LAB[NUM_LABS];
extern const char *AVISO_TOKENS;
extern const char *FALAS_INSTABILIDADE_LAB[ESTABILIDADE_MAX_LUCI + 1];
extern const char *NOMES_PARTES_PROMPT[NUM_PARTES_PROMPT];
extern const char *NOMES_CRITERIOS[NUM_CRITERIOS];
extern const MissaoPrompt MISSOES_PROMPT[6];
extern const char *TEXTOS_DIAG[NUM_DIAG_PROMPT];
extern const char *NOMES_AFIRMACAO[NUM_TIPOS_AFIRMACAO];
extern const Color CORES_AFIRMACAO[NUM_TIPOS_AFIRMACAO];
extern const DesafioArquivo DESAFIOS_ARQUIVO[6];
extern const DesafioNucleo DESAFIOS_NUCLEO[5];
extern const CasoVies CASOS_VIES[4];
extern const char *NOMES_ACAO_PACOTE[NUM_ACOES_PACOTE];
extern const PacoteDados PACOTES_DADOS[11];
extern const char *ROTULOS_PACOTES[11];
extern const char *DICAS_PRIVACIDADE[];
extern const char *DICA_FALSA_PRIVACIDADE;
extern const char *NOMES_VEREDITO[NUM_VEREDITOS];
extern const char *NOMES_MIDIA[];
extern const CasoPesquisa CASOS_PESQUISA[5];
extern const char *NOMES_LUZ[NUM_LUZES];
extern const Color CORES_LUZ[NUM_LUZES];
extern const Color CORES_PARTES_PROMPT[NUM_PARTES_PROMPT];
extern const char *DICAS_PARTES_PROMPT[NUM_PARTES_PROMPT];
extern const Vector2 POS_SAIDA_SILOGISTA;

// ----- Funções -----
const RodadaSilogistaBH *RodadaBH(int rodada);
const Dialogo *DialogoRodadaBH(int rodada);
int PermNivel(const ProgressoBiblioteca *p, int m);
void PermSubir(ProgressoBiblioteca *p, int m);
void RogueSincronizarPerm(Biblioteca *b);
const char *TextoEfeitoMelhoria(int m, int nivel);
Direcao DirecaoDoVetor(Vector2 v);
void RogueEmpurrarParaFora(Vector2 *p, float raio, const Rectangle *obst, int n);
void RogueParticulas(Biblioteca *b, Vector2 origem, Color cor, int quantidade, float velMax);
void RogueAviso(Biblioteca *b, const char *texto);
void RogueFaixaVirada(Biblioteca *b, const char *titulo, const char *sub, Color cor);
void RogueTextoFlutuante(Biblioteca *b, Vector2 pos, const char *texto, Color cor);
void RogueErroVerificacao(Biblioteca *b, const char *termo, const char *explicacao);
Projetil *RogueDisparar(Biblioteca *b, Vector2 origem, Vector2 dir, float vel, float dano, float raio, bool doJogador);
void RogueSoltar(Biblioteca *b, Vector2 pos, bool fragmento, TipoItem item);
int RogueInimigosVivos(const Biblioteca *b);
int RogueQtd(const JogadorRogue *j, TipoItem it);
int RogueUp(const JogadorRogue *j, TipoUpgrade u);
float RogueHpMax(const Biblioteca *b);
float RogueEpMax(const Biblioteca *b);
float RogueFatorDano(const Biblioteca *b);
float RogueDanoTiro(const Biblioteca *b);
float RogueFatorDanoRecebido(const JogadorRogue *j);
float RogueVelocidadeJogador(const JogadorRogue *j);
float RogueRecargaEsquiva(const JogadorRogue *j);
float RogueIntervaloArma(const JogadorRogue *j, TipoArma arma);
float RogueRaioPulso(const JogadorRogue *j);
float RogueDanoPulso(const JogadorRogue *j);
TipoArma RogueArmaAtual(const JogadorRogue *j);
void RogueLimparEfeitos(JogadorRogue *j);
void RogueLimparBuild(JogadorRogue *j);
void RogueEsvaziarMochila(JogadorRogue *j);
int RogueSortearPeso(const int *candidatos, int n, int (*peso)(int));
int RogueSortearItem(const Biblioteca *b, unsigned int excluir, bool soPassivos);
TipoItem RogueSortearTemporario(void);
TipoArma RogueSortearArma(Biblioteca *b, unsigned int excluir);
Inimigo *RogueCriarInimigo(Biblioteca *b, TipoInimigo tipo, Vector2 pos);
Vector2 RogueOrbePos(const Inimigo *e, int k);
bool RogueSalaDaMissao(const Biblioteca *b);
int RogueEspecialAtual(const Biblioteca *b);
void RogueUsarLayout(Biblioteca *b, const LayoutSala *l);
int RogueSortearEncontro(const Biblioteca *b, int nivel);
Vector2 RogueLugarSurgir(const Biblioteca *b, Vector2 base, float raio);
int RogueNivelCombate(const Biblioteca *b);
void RogueSurgirOnda(Biblioteca *b, int onda);
void RogueMontarEncontro(Biblioteca *b, const LayoutSala *l, int id);
void RogueLimparSala(Biblioteca *b, TipoSalaRogue tipo, Vector2 entrada);
bool RogueSortearLugar(const Biblioteca *b, float raio, float longeJogador, Vector2 *saida);
void RogueCriarArmadilha(Biblioteca *b, TipoArmadilha tipo);
void RogueColocarRecompensa(Biblioteca *b, TipoRecompensa tipo);
void RogueEntrarSala(Biblioteca *b, TipoSalaRogue tipo);
void RogueEntrarAtrio(Biblioteca *b, bool vindoDoCorredor);
void RogueComecarMissao(Biblioteca *b);
void RogueAtravessarPorta(Biblioteca *b, int k);
void RogueEntrarSecreta(Biblioteca *b);
void RogueIniciar(Biblioteca *b, ProgressoBiblioteca *perm);
int RogueEstab(const Biblioteca *b);
int RogueChanceLuciErrar(const Biblioteca *b);
void RogueRecadoLuci(Biblioteca *b, const char *texto);
void RogueMudarEstabilidade(Biblioteca *b, int delta, const char *motivo);
void RogueHabitoLuci(Biblioteca *b, bool confiou);
void RogueRegistrarResposta(Biblioteca *b, bool certo);
float RogueCorrupcaoLuci(const Biblioteca *b);
void RogueAbrirDesafio(Biblioteca *b, int indiceInimigo);
const char *RogueTextoLenteDesafio(const Biblioteca *b);
const char *RogueTextoLuciDesafio(const Biblioteca *b);
void RogueAtualizarDesafio(Biblioteca *b, ProgressoCorredor *prog, float dt);
bool RogueInimigoPassageiro(TipoInimigo t);
bool RogueNaZonaFirewall(const Biblioteca *b, const Inimigo *e);
int RogueSpammersPerto(const Biblioteca *b, Vector2 pos, float raio);
Vector2 RoguePrevisaoJogador(const Biblioteca *b, Vector2 origem, float velProjetil);
bool RogueTemKeylogger(const Biblioteca *b);
float RogueCargaOverclock(const Inimigo *e);
void RogueSuperaquecer(Inimigo *e);
void RogueRevelarPhisher(Biblioteca *b, Inimigo *e, bool distraido);
void RogueAtualizarConexoes(Biblioteca *b);
bool RogueQuemProteger(const Biblioteca *b, const Inimigo *fw, Vector2 *centro);
void RogueBarreiraFirewall(const Inimigo *e, Vector2 *a, Vector2 *c);
bool RogueBarreiraBloqueia(const Biblioteca *b, Vector2 pos, float raio);
float RogueDanoContato(const Inimigo *e);
void RogueFerirJogador(Biblioteca *b, float dano);
void RogueFerirInimigo(Biblioteca *b, ProgressoCorredor *prog, Inimigo *e, float dano, Vector2 direcao);
float RogueAdaptacaoChefe(const Biblioteca *b);
float RogueRitmoChefe(const Biblioteca *b);
void RogueFerirChefe(Biblioteca *b, float dano, Vector2 onde);
bool RogueChefeVulneravel(const ChefeRogue *c);
bool RogueFerirSilogista(Biblioteca *b, float dano, Vector2 onde);
bool RoguePosAlvoDedutivo(const Biblioteca *b, int alvo, Vector2 *pos);
void RogueMarcarDedutivo(Biblioteca *b, int *marca, float *tempo, int alvo, Vector2 pos);
void RogueAtualizarDedutivo(Biblioteca *b, float dt);
void RoguePulso(Biblioteca *b, ProgressoCorredor *prog);
void RogueAnel(Biblioteca *b, Vector2 pos, float raio, Color cor);
void RogueCriarArea(Biblioteca *b, Vector2 pos, float raio, float atraso, float vida, float dps, float danoInstantaneo, Color cor);
int RogueInimigoMaisPerto(const Biblioteca *b, Vector2 pos, float raio, int ignorar);
void RogueDanoArea(Biblioteca *b, ProgressoCorredor *prog, Vector2 centro, float raio, float dano, float empurrao);
void RogueExplodirOrbe(Biblioteca *b, ProgressoCorredor *prog, Vector2 centro);
float RogueAnalisar(Biblioteca *b, int alvo);
void RogueUsarArma(Biblioteca *b, ProgressoCorredor *prog, bool atirando, float dt);
void RogueFimProjetil(Biblioteca *b, ProgressoCorredor *prog, Projetil *p);
void RogueAcertarInimigo(Biblioteca *b, ProgressoCorredor *prog, Projetil *p, int k);
void RogueAtivarTemporario(Biblioteca *b, TipoItem it);
void RogueSalaConcluida(Biblioteca *b);
void RogueAtualizarEfeitos(Biblioteca *b, float dt);
bool RoguePerseguidor(TipoInimigo t);
void RogueIniciarChefe(Biblioteca *b);
void RogueAtualizarChefe(Biblioteca *b, ProgressoCorredor *prog, float dt);
int EntradasPorta(const PortaLogica *p);
bool SinalCircuito(const Silogista *s, int sinal, bool *valor);
Vector2 SilPosElemento(int e);
int SilNumElementos(void);
bool SilPecaNoChao(const Silogista *s, int k);
void SilPrepararRodada(Silogista *s, int rodada);
void RogueIniciarSilogista(Biblioteca *b);
void SilPegarPeca(Biblioteca *b, int k);
void SilUsarEncaixe(Biblioteca *b, int e);
int SilBalasLinha(const AtaqueSil *a);
Vector2 SilPontoLinha(const AtaqueSil *a, int i);
void RogueIniciarSilogistaBH(Biblioteca *b);
float RogueFatorTempoMundo(const Biblioteca *b);
bool RogueSilogistaAcerto(Biblioteca *b, ProgressoCorredor *prog, Projetil *p);
void RogueAtualizarSilogista(Biblioteca *b, ProgressoCorredor *prog, float dt);
void RogueAtualizarLuci(Biblioteca *b, ProgressoCorredor *prog, float dt);
IndiceFinal RogueCalcularIndice(const Biblioteca *b, int pontos);
bool RogueTemConceito(const Biblioteca *b, int conceito);
const Dialogo *RogueDialogoRegistro(const Biblioteca *b, int k);
const Dialogo *RogueDialogoRevelacao(const Biblioteca *b);
Vector2 NucleoLuci(void);
bool RogueFraseContradiz(const Biblioteca *b, int f);
bool RogueLuciVulneravel(const LuciChefe *l);
float RogueHpBarraLuci(const LuciChefe *l, int fase);
void RogueIniciarLuciChefe(Biblioteca *b);
float MestreJanelaParry(const Biblioteca *b);
void RogueLimparBalasInimigas(Biblioteca *b);
void DesenharCenaSeparacao(const Biblioteca *b, float t, bool passeFrente);
void DesenharCenaTransformacao(const Biblioteca *b, float t);
void DesenharTituloCenaLuci(const Biblioteca *b, float t);
bool RogueAtualizarCenaLuci(Biblioteca *b, float dt);
void RogueMontarSalaBloco3(Biblioteca *b, TipoSalaRogue tipo);
void RogueEntrarBloco3(Biblioteca *b, ProgressoCorredor *prog);
bool RogueLuciGuardaAberta(const LuciChefe *l);
bool RogueFerirLuciChefe(Biblioteca *b, float dano, Vector2 onde);
bool RogueLuciAcerto(Biblioteca *b, ProgressoCorredor *prog, Projetil *p);
void RogueLuciContestar(Biblioteca *b, ProgressoCorredor *prog);
void RogueLuciContexto(Biblioteca *b, int k);
void RogueGolpeDedutivo(Biblioteca *b);
void RogueAtualizarLuciChefe(Biblioteca *b, ProgressoCorredor *prog, float dt);
bool RogueAlvoContexto(const Biblioteca *b, int *k);
void RogueAtualizarJogador(Biblioteca *b, ProgressoCorredor *prog, float dt);
Vector2 RogueManterDistancia(Inimigo *e, Vector2 dir, float dist, float minimo, float maximo, float vel, float dt);
void RogueMoverPhisher(Inimigo *e, Vector2 dir, float vel, float dt);
bool RogueSentinelaVulneravel(const Inimigo *e);
void RogueRetroRegistrar(Biblioteca *b, Inimigo *e);
void RogueJanelaRuido(Biblioteca *b, float dano);
void RogueJanelaDerrotada(Biblioteca *b, ProgressoCorredor *prog);
void RogueMontarSalaEspecial(Biblioteca *b, int esp);
void RogueAtualizarInimigos(Biblioteca *b, float dt);
void RogueAcertarParede(Biblioteca *b, ProgressoCorredor *prog, const Projetil *p);
void RogueAtualizarProjeteis(Biblioteca *b, ProgressoCorredor *prog, float dt);
void RogueAtualizarColetaveis(Biblioteca *b, float dt);
int RogueContarTipo(const Biblioteca *b, TipoInimigo t);
const char *RogueDescricaoChefeSeletor(int chefe);
bool RogueChefeSeletorVencido(const Biblioteca *b, int chefe);
void RogueAbrirSeletor(Biblioteca *b);
void RogueVoltarAoLobby(Biblioteca *b);
bool RoguePodeAbandonar(const Biblioteca *b);
void RogueAbandonarRun(Biblioteca *b);
void RogueSeletorVenceu(Biblioteca *b, ProgressoCorredor *prog, int pontos);
Rectangle RogueRetCardSeletor(int i, int foco);
Rectangle RogueRetFecharSeletor(void);
void RogueAtualizarSeletor(Biblioteca *b);
void RogueAtualizarObjetivo(Biblioteca *b, ProgressoCorredor *prog, float dt);
void RogueAtualizarArmadilhas(Biblioteca *b, ProgressoCorredor *prog, float dt);
void RogueAtualizarTornado(Biblioteca *b, ProgressoCorredor *prog, float dt);
void RogueAtualizarAreas(Biblioteca *b, ProgressoCorredor *prog, float dt);
void RogueAbrirEscolha(Biblioteca *b, ModoEscolha modo, bool gratis);
bool RogueTemArma(const JogadorRogue *j, TipoArma a);
void RogueInstalarArma(Biblioteca *b, TipoArma arma, int vaga);
bool RogueReceberArma(Biblioteca *b, TipoArma arma, int origem, int pedestal);
Rectangle RogueCartaEscolha(int i, int n);
void RogueAtualizarEscolha(Biblioteca *b);
void RogueUsarRecompensa(Biblioteca *b);
void RogueMontarTelaEvento(const Biblioteca *b, TelaEvento *t);
void RogueAtualizarEvento(Biblioteca *b, ProgressoCorredor *prog, SessaoDialogo *dlg);
void RogueAtualizarDesafioSala(Biblioteca *b, ProgressoCorredor *prog, float dt);
void LabCortarUtf8(char *s);
int LabDificuldadeMissao(TipoLab t, int i);
void LabPreparar(Biblioteca *b, TipoLab tipo);
LuzParte LabLuzParte(const MissaoPrompt *m, const int esc[NUM_PARTES_PROMPT], int k);
const char *LabTextoRespostaAtual(const Biblioteca *b);
int LabTokensCarregados(const Biblioteca *b);
void LabFinalizar(Biblioteca *b, ProgressoCorredor *prog);
Rectangle LabRetEncaixe(int k);
Rectangle LabRetPeca(int c);
int LabMontarBotoes(const Biblioteca *b, BotaoLab *bt, int max);
void RogueAtualizarLab(Biblioteca *b, ProgressoCorredor *prog, float dt);
bool MestreVulneravel(const MestreTreino *m);
float MestreTempoAteGolpe(const MestreTreino *m, Vector2 jogador, float telegrafia);
float MestreTelegrafia(const MestreTreino *m);
void RogueEntrarSalaMestre(Biblioteca *b);
void MestreMudarEtapa(Biblioteca *b, EtapaMestre etapa);
float MestreDistTrajeto(const Projetil *p, Vector2 c, float dt);
void RogueAtualizarMestre(Biblioteca *b, ProgressoCorredor *prog, float dt);
void RogueMestreReerguer(Biblioteca *b);
void RogueSairSalaMestre(Biblioteca *b);
void RogueAtualizarAfixos(Biblioteca *b, float dt);
bool LabEmAcaoTipo(TipoLab t);
void LabComecarAcao(Biblioteca *b);
float LabRaioBloco(int tokens);
void RogueAtualizarLabAcao(Biblioteca *b, ProgressoCorredor *prog, float dt);
bool RoguePertoSaidaSilogista(const Biblioteca *b);
void RogueAtualizarSaidaSilogista(Biblioteca *b, float dt);
PortaInteracao RoguePortaProxima(const Biblioteca *b, int *indice);
AlvoRogue RogueAlvoProximo(const Biblioteca *b, int *pedestal);
int PrecoItemLoja(const Biblioteca *b, int item);
Rectangle RogueCartaTroca(int i);
EventoRogue RogueAtualizar(Biblioteca *b, ProgressoCorredor *prog, SessaoDialogo *dlg, float dt);
void DesenharEstante(Rectangle r, float tempo);
void DesenharAfixosElite(const Biblioteca *b, float tempo);
void DesenharLabAcao(const Biblioteca *b, float tempo);
void DesenharTarefaLabAcao(const Biblioteca *b);
void DesenharSaidaSilogista(const Biblioteca *b, float tempo);
void DesenharPedestalArmaMestre(Vector2 c, float tempo);
void DesenharAlvoTreino(Vector2 c, float tempo, int i);
void DesenharDicasMestreMundo(const Biblioteca *b, float tempo);
void DesenharMestreTreino(const Biblioteca *b, float tempo);
void DesenharHudMestre(const Biblioteca *b, float tempo);
void DesenharPortaRogue(Rectangle r, bool aberta, const char *nome, float tempo);
void DesenharIconeItem(TipoItem item, Vector2 c, float e, float tempo);
void DesenharFragmentoConhecimento(Vector2 c, float t);
void DesenharProfessorByte(Vector2 c, float t);
void DesenharByteco(Vector2 c, float t);
void DesenharTerminal(Vector2 c, bool lido, float t);
void DesenharPedestal(Vector2 c, TipoItem item, bool disponivel, float t);
void DesenharFragmentoAlucinado(const Inimigo *e, float escala);
void DesenharGuardiao(const Inimigo *e, float escala);
void DesenharEco(const Inimigo *e);
void DesenharSelo(const Inimigo *e, float tempo);
void DesenharCorredorAvisoIni(Vector2 ini, Vector2 dir, float alcance, float largura, float k, bool travou, Color cor, float t);
void DesenharLinhasBalasIni(Vector2 p, float raio, int n, float angIni, float passo, float k, Color cor, float t);
void DesenharSinergiasChao(const Biblioteca *b, float t);
void DesenharSinergiasMarcas(const Biblioteca *b, float t);
void DesenharSentinela(const Inimigo *e, float escala, float t);
void DesenharRetro(const Inimigo *e, float escala, float t);
void DesenharJanelaChefe(const Inimigo *e, float escala, float t);
void DesenharBarraJanela(const Biblioteca *b, float tempo);
void DesenharInimigo(const Inimigo *e);
void DesenharRecargaRogue(int x, int y, float falta, float total, const char *tecla, Color cor, bool disponivel);
void DesenharTeclaE(Vector2 c, float tempo);
void DesenharAvisoPorta(Vector2 pos, const char *acao, float tempo);
void DesenharPulpitoEnciclopedia(Vector2 c, float t);
void DesenharDesafioRogue(const Biblioteca *b, float tempo);
void DesenharEnciclopediaRogue(const Biblioteca *b);
void DesenharLojaRogue(const Biblioteca *b, float tempo);
void DesenharAlucinador(const ChefeRogue *c, Vector2 jogador, float t);
void DesenharBarraChefe(const ChefeRogue *c);
void DesenharFaixaAfirmacao(const Biblioteca *b, float tempo);
void DesenharMarcaDedutiva(int marca, Vector2 pos, float raio);
void DesenharPecaSilogismo(Vector2 c, char letra, Color cor, float t);
void DesenharChaoCamara(float t);
void DesenharMecanismoSilogismo(const Silogista *s, float t);
void DesenharCircuito(const Silogista *s, float t);
void DesenharAtaquesSilogista(const Silogista *s, float t);
void DesenharSilogista(const Silogista *s, Vector2 jogador, float t);
void DesenharBarraSilogista(const Silogista *s);
void DesenharFaixaSilogista(const Biblioteca *b, float tempo);
void DesenharIconeArma(TipoArma arma, Vector2 c, float e, float tempo);
void DesenharProjetilInimigo(const Projetil *p, float tempo);
void DesenharProjetilJogador(const Projetil *p, float tempo);
void DesenharArmaNaMao(const Biblioteca *b, float tempo, bool naMao);
void DesenharEfeitosJogador(const Biblioteca *b, float tempo);
void DesenharProxy(const JogadorRogue *j, float tempo);
void DesenharMarcasAnalise(const Biblioteca *b);
void DesenharHudArmas(const Biblioteca *b, float tempo);
void DesenharAneis(const Biblioteca *b);
void DesenharScannerLuci(const Biblioteca *b, float tempo);
void DesenharHudLuci(const Biblioteca *b, float tempo);
bool RogueAlvoGuiaLuci(const Biblioteca *b, Vector2 *alvo);
void RogueLuciCompObservar(const Biblioteca *b);
bool RogueNoBloco3(const Biblioteca *b);
void DesenharChaoNucleo(const Biblioteca *b, float t);
void DesenharLuciGrande(Vector2 p, float altura, float instabilidade, bool sombra);
void DesenharLuciChefe(const Biblioteca *b, float t);
void DesenharAtaquesLuci(const Biblioteca *b, float t);
void DesenharAvisoMetade(const Biblioteca *b, float t);
void DesenharContextoLuci(const Biblioteca *b, float t);
bool RogueLuciFalando(const Biblioteca *b);
void DesenharFaixaLuci(const Biblioteca *b, float t);
void DesenharBarraLuci(const LuciChefe *l, float t);
void DesenharResultadoFinal(const Biblioteca *b, float t);
void CarregarIconesBiblioteca(void);
void DescarregarIconesBiblioteca(void);
void DesenharPortaMissao(const Biblioteca *b, const PortaMissao *p, float tempo);
void DesenharParedesMissao(const Biblioteca *b, float tempo);
void DesenharArmadilhas(const Biblioteca *b, float tempo);
void DesenharAreas(const Biblioteca *b);
void DesenharTornado(const Biblioteca *b);
void DesenharRaiosKatana(const Biblioteca *b);
void DesenharObjetosSala(const Biblioteca *b, float tempo);
void DesenharMiniMapa(const Biblioteca *b, float tempo);
void DesenharMapaGrande(const Biblioteca *b, float tempo);
void DesenharIconeUpgrade(const Biblioteca *b, OpcaoEscolha o, Vector2 c, float e, float tempo);
float DpsArma(TipoArma arma);
float AlcanceArma(TipoArma arma);
void DesenharBarraArma(const char *nome, float x, float y, float w, float valor, float maximo, const char *texto, Color cor, float ref);
void RogueMelhoriasDaArma(const JogadorRogue *j, TipoArma arma, char *saida, int tam);
void DesenharTrocaArma(const Biblioteca *b, float tempo);
void DesenharEscolha(const Biblioteca *b, float tempo);
void DesenharEvento(const Biblioteca *b, float tempo);
void DesenharConsoleLab(const Biblioteca *b, float tempo);
void DesenharLab(const Biblioteca *b, float tempo);
void DesenharNucleoCorrupcao(const Inimigo *e, float escala, float t);
void DesenharMarcasReformulacao(const Inimigo *e, float t);
void DesenharHudBuild(const Biblioteca *b, int yInicio, float tempo);
void CorromperTexto(const char *origem, char *saida, int tam, unsigned int semente, int chance);
float RogueIntensidadeAberracao(const Biblioteca *b);
void AplicarAberracaoCromatica(RenderTexture2D alvo, float k, float tempo);
void DescarregarAberracao(void);
void DesenharPortaNucleoAtrio(const Biblioteca *b, float t);
void DesenharNucleoInstavel(const Biblioteca *b, float t);
void DesenharGuiaParry(const Biblioteca *b, float t);
void DesenharVinheta(Color cor, float a, float tam);
void DesenharEfeitosTela(const Biblioteca *b, float t);
void DesenharSimbolosSilogistaBH(const Biblioteca *b, float t);
void DesenharFocoSilogistaBH(const Biblioteca *b, float t);
void DesenharTerminalAtalho(Vector2 c, const char *destino, Vector2 alvoSeta, bool liberado, float t);
void RogueDesenhar(const Biblioteca *b, const SpriteDirecao sprites[NUM_DIRECOES], float anguloLuci, float tempo, const char *nickname, int pontos);

#endif // UTOPIA_BLOCO2_BIBLIOTECA_H
