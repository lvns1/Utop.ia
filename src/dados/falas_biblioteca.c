#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <time.h>
#include "../../include/core/fonte.h"
#include "../../include/core/mapas.h"
#include "../../include/audio/audio.h"
#include "../../include/bloco1/batalha.h"
#include "../../include/bloco1/batalha_monitores.h"
#include "../../include/bloco1/corredor.h"
#include "../../include/bloco1/minigame_visual.h"
#include "../../include/bloco1/objetivo.h"
#include "../../include/core/save.h"
#include "../../include/core/telemetria.h"
#include "../../include/core/texto.h"
#include "../../include/dados/conteudo_bloco1.h"
#include "../../include/entidades/jogador.h"
#include "../../include/entidades/luci_companion.h"
#include "../../include/telas/intro.h"
#include "../../include/telas/pause.h"
#include "../../include/telas/ranking.h"
#include "../../include/telas/resolucao.h"
#include "../../include/bloco2/biblioteca.h"


const LayoutSala LAYOUTS_COMBATE[] = {
    { 2, { {170, 180, 40, 140}, {590, 180, 40, 140} }, 8,
      { {120, 110}, {400, 110}, {680, 110}, {300, 200}, {500, 200}, {400, 300}, {120, 320}, {680, 320} } },
    { 4, { {140, 150, 120, 28}, {540, 150, 120, 28}, {140, 360, 120, 28}, {540, 360, 120, 28} }, 8,
      { {400, 100}, {110, 100}, {690, 100}, {400, 250}, {250, 260}, {550, 260}, {100, 260}, {700, 260} } },
    { 3, { {350, 170, 100, 30}, {190, 290, 30, 120}, {580, 290, 30, 120} }, 8,
      { {110, 100}, {690, 100}, {400, 110}, {260, 220}, {540, 220}, {400, 280}, {120, 360}, {680, 360} } },

    // ---- Reformulação: layouts novos (as portas de cima, da esquerda e da direita e a entrada de baixo ficam livres) ----
    // PILARES: cobertura espalhada, bom contra atiradores
    { 6, { {180, 160, 32, 32}, {588, 160, 32, 32}, {280, 290, 32, 32}, {488, 290, 32, 32}, {180, 400, 32, 32}, {588, 400, 32, 32} }, 8,
      { {120, 110}, {680, 110}, {400, 180}, {250, 230}, {550, 230}, {400, 330}, {130, 460}, {670, 460} } },
    // CORREDOR ESTREITO: um corredor no meio, flancos abertos
    { 6, { {300, 150, 28, 120}, {472, 150, 28, 120}, {300, 330, 28, 90}, {472, 330, 28, 90}, {120, 150, 100, 24}, {580, 150, 100, 24} }, 8,
      { {400, 200}, {400, 300}, {200, 260}, {600, 260}, {160, 420}, {640, 420}, {120, 100}, {680, 100} } },
    // OBSTÁCULO CENTRAL: um bloco grande no meio e coberturas nos cantos
    { 5, { {340, 225, 120, 110}, {130, 130, 70, 24}, {600, 130, 70, 24}, {130, 440, 70, 24}, {600, 440, 70, 24} }, 8,
      { {400, 150}, {250, 280}, {550, 280}, {400, 400}, {120, 100}, {680, 100}, {200, 200}, {600, 200} } },
    // ÁREA ABERTA: quase sem cobertura (sempre recebe uma armadilha a mais)
    { 2, { {200, 290, 26, 26}, {574, 290, 26, 26} }, 8,
      { {120, 110}, {680, 110}, {400, 160}, {260, 220}, {540, 220}, {400, 300}, {160, 400}, {640, 400} } },
    // FORTALEZA: um U de estantes protege o centro (região de proteção e de risco)
    { 5, { {300, 190, 200, 22}, {300, 190, 22, 130}, {478, 190, 22, 130}, {120, 380, 90, 24}, {590, 380, 90, 24} }, 8,
      { {400, 260}, {360, 290}, {440, 290}, {130, 110}, {670, 110}, {200, 300}, {600, 300}, {400, 130} } },
    // ZIGUE-ZAGUE: estantes em diagonal cortam as linhas de tiro
    { 5, { {150, 150, 130, 24}, {335, 230, 130, 24}, {520, 310, 130, 24}, {170, 400, 110, 24}, {560, 160, 24, 90} }, 8,
      { {120, 100}, {680, 100}, {250, 300}, {450, 160}, {450, 380}, {650, 420}, {300, 470}, {130, 250} } },
    // LABIRINTO DE ESTANTES: muitos corredores curtos
    { 8, { {140, 120, 24, 110}, {240, 330, 120, 24}, {440, 330, 120, 24}, {636, 120, 24, 110}, {220, 150, 110, 24}, {470, 150, 110, 24},
           {140, 400, 24, 80}, {636, 400, 24, 80} }, 8,
      { {400, 220}, {190, 260}, {610, 260}, {300, 420}, {500, 420}, {110, 100}, {690, 100}, {400, 280} } },
};

// Faixas que nenhuma estante, armadilha ou inimigo pode ocupar (portas e entrada)
const Rectangle ZONAS_LIVRES_ROGUE[NUM_LADOS] = {
    { 350, 60, 100, 90 },       // porta de cima
    { 40, 235, 105, 120 },      // porta da esquerda
    { 655, 235, 105, 120 },     // porta da direita
    { 330, 420, 140, 110 },     // entrada de baixo
};

const EncontroDef ENCONTROS[] = {
    // Salas iniciais: os básicos, o Spammer, o Phisher e o Cache
    { "ARQUIVOS CORROMPIDOS", 1, 1, 3, { { INIMIGO_FRAGMENTO, 2, 3 }, { INIMIGO_GUARDIAO, 1, 1 }, { INIMIGO_CACHE, 1, 1 } } },
    { "ENXAME DIGITAL",       1, 3, 2, { { INIMIGO_SPAMMER, 8, 12 }, { INIMIGO_PHISHER, 1, 1 } } },
    // Intermediárias: Firewall, Overclock, Keylogger e Botnet, dois por vez
    { "REDE PROTEGIDA",       2, 3, 4, { { INIMIGO_FIREWALL, 1, 1 }, { INIMIGO_BOTNET, 1, 1 }, { INIMIGO_FRAGMENTO, 2, 3 }, { INIMIGO_GUARDIAO, 1, 1 } } },
    { "SOBRECARGA",           2, 3, 3, { { INIMIGO_OVERCLOCK, 2, 2 }, { INIMIGO_KEYLOGGER, 2, 2 }, { INIMIGO_SPAMMER, 2, 3 } } },
    { "DADOS PERSISTENTES",   2, 3, 3, { { INIMIGO_CACHE, 2, 2 }, { INIMIGO_FIREWALL, 1, 1 }, { INIMIGO_SPAMMER, 3, 3 } } },
    // Avançada: todas as funções juntas
    { "CAOS SISTÊMICO",       3, 3, 6, { { INIMIGO_BOTNET, 1, 1 }, { INIMIGO_FIREWALL, 1, 1 }, { INIMIGO_OVERCLOCK, 1, 1 },
                                         { INIMIGO_KEYLOGGER, 1, 1 }, { INIMIGO_SPAMMER, 5, 6 }, { INIMIGO_PHISHER, 1, 1 } } },

    // ---- Reformulação: encontros em ondas, com funções que se completam ----
    //   { tipo, mín, máx, onda, elite }
    // Perseguidores + atirador que prevê o movimento
    { "VAZAMENTO DE DADOS",   1, 2, 3, { { INIMIGO_FRAGMENTO, 3, 4, 0 }, { INIMIGO_SPAMMER, 4, 5, 1 }, { INIMIGO_KEYLOGGER, 1, 1, 1 } } },
    // Iscas: o Phisher se esconde entre arquivos enquanto o Guardião segura a distância
    { "ISCA E ANZOL",         1, 2, 3, { { INIMIGO_PHISHER, 2, 2, 0 }, { INIMIGO_GUARDIAO, 1, 1, 0 }, { INIMIGO_FRAGMENTO, 2, 3, 1 } } },
    // Suporte e proteção: o Firewall cobre o Cache, que se cura mais rápido no escudo
    { "PROTOCOLO BLINDADO",   2, 3, 5, { { INIMIGO_FIREWALL, 1, 1, 0 }, { INIMIGO_CACHE, 1, 1, 0 }, { INIMIGO_GUARDIAO, 2, 2, 0 },
                                         { INIMIGO_OVERCLOCK, 1, 1, 1 }, { INIMIGO_FRAGMENTO, 2, 2, 1 } } },
    // O Botnet acelera os Overclocks: investidas em sequência
    { "REDE ZUMBI",           2, 3, 3, { { INIMIGO_BOTNET, 2, 2, 0 }, { INIMIGO_OVERCLOCK, 2, 2, 0 }, { INIMIGO_SPAMMER, 6, 6, 1 } } },
    // Keylogger + Overclock: a investida mira onde você VAI estar
    { "PREVISÃO LETAL",       2, 3, 3, { { INIMIGO_KEYLOGGER, 2, 2, 0 }, { INIMIGO_OVERCLOCK, 1, 1, 0 }, { INIMIGO_GUARDIAO, 2, 2, 1 } } },
    // Três ondas de spam: a última traz o Botnet que acelera o enxame
    { "TEMPESTADE DE SPAM",   3, 3, 5, { { INIMIGO_SPAMMER, 8, 8, 0 }, { INIMIGO_SPAMMER, 6, 6, 1 }, { INIMIGO_PHISHER, 1, 1, 1 },
                                         { INIMIGO_BOTNET, 1, 1, 2 }, { INIMIGO_SPAMMER, 6, 6, 2 } } },
    { "FORTALEZA DE CACHE",   3, 3, 5, { { INIMIGO_FIREWALL, 2, 2, 0 }, { INIMIGO_CACHE, 2, 2, 0 }, { INIMIGO_KEYLOGGER, 2, 2, 1 },
                                         { INIMIGO_BOTNET, 1, 1, 1 }, { INIMIGO_FRAGMENTO, 3, 3, 2 } } },

    // ---- Salas de ELITE ----
    { "GUARDA DE ELITE",      1, 3, 4, { { INIMIGO_GUARDIAO, 1, 1, 0, true }, { INIMIGO_FRAGMENTO, 3, 3, 0 }, { INIMIGO_SPAMMER, 4, 4, 1 },
                                         { INIMIGO_CACHE, 1, 1, 1 } }, true },
    { "SOBRECARGA ELITE",     2, 3, 5, { { INIMIGO_OVERCLOCK, 1, 1, 0, true }, { INIMIGO_BOTNET, 1, 1, 0 }, { INIMIGO_KEYLOGGER, 1, 1, 0 },
                                         { INIMIGO_FRAGMENTO, 3, 3, 1 }, { INIMIGO_OVERCLOCK, 1, 1, 1 } }, true },
    { "MURALHA ELITE",        2, 3, 5, { { INIMIGO_FIREWALL, 1, 1, 0, true }, { INIMIGO_CACHE, 1, 1, 0 }, { INIMIGO_GUARDIAO, 2, 2, 0 },
                                         { INIMIGO_PHISHER, 1, 1, 1 }, { INIMIGO_SPAMMER, 5, 5, 1 } }, true },
    { "CAÇADOR DE TECLAS",    3, 3, 5, { { INIMIGO_KEYLOGGER, 1, 1, 0, true }, { INIMIGO_PHISHER, 1, 1, 0, true }, { INIMIGO_SPAMMER, 5, 5, 0 },
                                         { INIMIGO_BOTNET, 1, 1, 1 }, { INIMIGO_OVERCLOCK, 2, 2, 1 } }, true },
};

// Estantes das salas que não são de combate (o Byte e o Byteco também bloqueiam a passagem)
const LayoutSala LAYOUT_ATRIO      = { .numObst = 6, .obst = { {80, 100, 130, 26}, {590, 100, 130, 26}, {285, 132, 30, 40},
                                                                  {590, 280, 40, 36}, {555, 318, 110, 24}, {164, 420, 32, 26} } };
const LayoutSala LAYOUT_EDUCATIVA  = { .numObst = 3, .obst = { {150, 140, 30, 220}, {620, 140, 30, 220}, {380, 250, 40, 34} } };
const LayoutSala LAYOUT_RECOMPENSA = { .numObst = 2, .obst = { {110, 120, 140, 26}, {550, 120, 140, 26} } };
const LayoutSala LAYOUT_CHEFE      = { .numObst = 0 };
const LayoutSala LAYOUT_TRANSICAO  = { .numObst = 4, .obst = { {290, 180, 30, 60}, {480, 180, 30, 60}, {290, 380, 30, 60}, {480, 380, 30, 60} } };
// Reformulação: evento, descanso e desafio (estantes nos cantos; os pontos servem para a onda do Banco Instável)
const LayoutSala LAYOUT_EVENTO     = { 4, { {110, 120, 120, 24}, {570, 120, 120, 24}, {110, 450, 120, 24}, {570, 450, 120, 24} }, 8,
                                              { {150, 200}, {650, 200}, {150, 380}, {650, 380}, {280, 160}, {520, 160}, {280, 420}, {520, 420} } };
const LayoutSala LAYOUT_SECRETA    = { 4, { {160, 150, 26, 150}, {614, 150, 26, 150}, {230, 420, 90, 24}, {480, 420, 90, 24} }, 0 };
const Vector2 POS_OBJETO_SALA  = { 400, 250 };    // altar do evento, poltrona, console do desafio
const Vector2 POS_REGISTRO_SALA = { 600, 440 };   // arquivo de ameaça / memória

const QuizEvento QUIZ_FONTES[] = {
    { "Três fichas dizem quando a Biblioteca dos Ecos foi fundada. Uma delas foi gerada por uma IA e nunca existiu. Qual é a SUSPEITA?",
      { "Ata da fundação, assinada e guardada no arquivo do átrio", "\"Enciclopédia Universal dos Ecos, vol. 77\" (nenhum outro registro a cita)",
        "Placa de bronze na entrada, com a data gravada" },
      1, "Uma fonte que ninguém mais cita e que não dá para encontrar é o sinal clássico de fonte inventada. As outras podem ser conferidas." },
    { "A IA disse que o Professor Byte escreveu 40 livros e citou fontes. Qual é a SUSPEITA?",
      { "O catálogo da própria Biblioteca, que lista os livros dele", "Uma entrevista com o Byte gravada no terminal",
        "\"Byte, P. (2031). Tudo sobre mim. Editora Inexistente\" (a data ainda não chegou)" },
      2, "Uma referência com data no futuro e editora que não existe foi inventada. Confira autor, data e editora." },
    { "Sobre o Silogista, a IA citou três origens. Qual é a SUSPEITA?",
      { "O manual de lógica da Câmara da Inferência", "\"Segundo especialistas\" (sem dizer quem, onde ou quando)",
        "Os registros de manutenção do módulo lógico" },
      1, "\"Segundo especialistas\" sem nome, lugar ou data não pode ser conferido. Fonte vaga não é fonte." },
};

const QuizEvento QUIZ_RESPOSTAS[] = {
    { "Você pediu: \"Resuma o capítulo 3 deste livro.\" A IA respondeu com um resumo do capítulo 5, com toda a confiança. O que aconteceu?",
      { "Nada: um resumo é sempre útil", "Alucinação intrínseca: a resposta contradiz o que você pediu", "A IA verificou e corrigiu o seu pedido" },
      1, "A resposta não respeita o contexto que você deu. Contradizer o pedido ou o contexto é alucinação INTRÍNSECA." },
    { "A IA respondeu: \"A água ferve a 100 °C no nível do mar. No alto de uma montanha, ela ferve a 150 °C.\" O que fazer?",
      { "Aceitar: a primeira parte está certa, então a segunda também", "Desconfiar da segunda frase e conferir em uma fonte confiável",
        "Perguntar se ela tem certeza e aceitar o \"sim\"" },
      1, "Uma frase certa não garante a seguinte. Em altitude a água ferve ABAIXO de 100 °C. Confira cada afirmação." },
    { "Pedido: \"Me ajuda com o trabalho.\" A IA escreveu 3 páginas sobre o tema errado. Qual é a melhor correção?",
      { "Pedir de novo, igual", "Dizer o assunto, o objetivo e o formato (\"1 página sobre reciclagem, 7º ano\")", "Aceitar e mudar o tema do trabalho" },
      1, "O pedido vago deixou lacunas, e a IA preencheu com o que parecia provável. Contexto claro reduz a invenção." },
};

const char *NOMES_PAREDES_SECRETAS[4] = { "no alto, à esquerda", "no alto, à direita", "embaixo, à esquerda", "embaixo, à direita" };
const Vector2 POS_REGISTROS[3] = { { 170, 430 }, { 630, 320 }, { 200, 200 } };
const Vector2 POS_LUCI_NUCLEO  = { 400, 120 };    // a Luci para um instante na frente da porta do núcleo e entra por ela

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

const Dialogo dialogoByte = {
    .paginas = {
        "Byte: Bem-vindo à Biblioteca dos Ecos! Sou o Professor Byte. Aqui ficam as respostas que a IA já gerou.",
        "Byte: A IA generativa não consulta a verdade: ela aprendeu padrões e prevê a próxima palavra mais provável.",
        "Byte: Por isso ela pode ALUCINAR e inventar com confiança. O Alucinador espalhou esses ecos falsos por aqui!",
        "Byte: Recupere os Fragmentos de Conhecimento lá dentro. O Byteco troca os fragmentos por itens.",
        "Byte: No meio do caminho fica a Câmara da Inferência. Para atravessá-la, você precisará compreender como as ideias se conectam. Uma conclusão não se torna verdadeira apenas porque parece convincente."
    },
    .numPaginas = 5,
    .saberMais = &dialogoByteCompleto
};

const Dialogo dialogoByteDepois = {
    .paginas = {
        "Byte: Lembre: resposta convincente não é resposta verdadeira. Boa sorte lá dentro!",
        "Byte: Na Câmara da Inferência, olhe as premissas antes de aceitar a conclusão. Ela precisa DECORRER delas."
    },
    .numPaginas = 2,
    .saberMais = &dialogoByteCompleto
};

const Dialogo dialogoByteco = {
    .paginas = {
        "Byteco: Psiu! Eu troco Fragmentos de Conhecimento por itens que ajudam lá dentro. Se os ecos te derrubarem, os itens se perdem; os fragmentos não.",
        "Byteco: Pra começar, leva este Antivírus de brinde. Na missão, aperte 1 para curar a vida!"
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

const Dialogo dialogoTerminalAlucinacao = {
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

const Dialogo dialogoTerminalPraticas = {
    .paginas = {
        "* TERMINAL // BOAS PRÁTICAS",
        "* Faça perguntas claras e com contexto: assunto, objetivo e formato.",
        "* Verifique a resposta em fontes confiáveis, como sites oficiais, livros e especialistas.",
        "* Nunca confie cegamente: a IA ajuda, mas quem decide é você."
    },
    .numPaginas = 4,
    .saberMais = &dialogoTerminalPraticasCompleto
};

// ---------- Falas e afirmações do Alucinador ----------

// Fim do Bloco 1: a sala da ALUCINAÇÃO MESTRE (falas curtas, uma ideia por vez)
const Vector2 POS_ARMA_MESTRE   = { 400, 300 };     // o pedestal do Disparador de Dados
const Vector2 POS_MESTRE_INICIO = { 400, 170 };
const Vector2 POS_ALVOS_MESTRE[MESTRE_ALVOS] = { { 270, 200 }, { 400, 150 }, { 530, 200 } };

const Dialogo dialogoMestreChegada = {
    .paginas = {
        "* Uma sala esquecida do arquivo. No centro, algo brilha num pedestal.",
        "Luci: A ALUCINAÇÃO MESTRE fugiu para cá. Mas antes... olhe aquilo!"
    },
    .numPaginas = 2
};

const Dialogo dialogoMestreArma = {
    .paginas = {
        "* Você pegou o DISPARADOR DE DADOS.",
        "Luci: Ele dispara fatos verificados. Alucinação nenhuma resiste a um fato!",
        "Luci: Vamos treinar. Mire com o MOUSE e clique nos três alvos."
    },
    .numPaginas = 3
};

const Dialogo dialogoMestreVitoria = {
    .paginas = {
        "* A ALUCINAÇÃO MESTRE se desfaz em letras soltas.",
        "Luci: Conseguimos! Você esquivou, achou a brecha e rebateu o golpe.",
        "Luci: Nos monitores você aprendeu a DUVIDAR. Aqui você aprendeu a REAGIR: esquivar, achar a brecha e rebater.",
        "Luci: Adiante fica a Biblioteca dos Ecos. Lá as alucinações são mais espertas, e algumas se parecem com as que você já enfrentou.",
        "Luci: Leve o disparador. Agora você está pronto!"
    },
    .numPaginas = 5
};

// Chegada ao átrio da Biblioteca (primeira vez, vindo do Bloco 1): liga o que ele aprendeu ao que vem
const Dialogo dialogoChegadaBiblioteca = {
    .paginas = {
        "* As portas se fecham atrás de vocês. Prateleiras sem fim, e um zumbido de vozes baixas.",
        "Luci: Esta é a Biblioteca dos Ecos. Cada sala guarda um eco de alucinação, e cada eco derrotado devolve um Fragmento de Conhecimento.",
        "Luci: Pelas portas você segue uma missão: sala após sala, até o chefe. Ao vencer uma sala você escolhe 1 entre 3 melhorias.",
        "Luci: Se cair, você volta para cá com os Fragmentos. O Byteco troca esses fragmentos por melhorias que ficam para sempre.",
        "Luci: Sem pressa. Quando quiser, vá até a porta da missão. Eu aviso quando algo for novo."
    },
    .numPaginas = 5
};

const char *FALAS_MESTRE_ETAPA[MT_FIM + 1] = {
    NULL,
    NULL,
    "Luci: Na mosca! Ela vem aí. ESPAÇO esquiva: você atravessa os ataques!",
    "Luci: Viu? Depois de atacar ela cansa e o escudo fica DOURADO. Atire nessa hora!",
    "Luci: Agora ela vem para cima. Brilho VERMELHO = golpe. Aperte V quando o anel fechar!",
    "Luci: PARRY perfeito! Agora o PULSO: ele gasta EP (a barra azul). Aperte Q quando as balas chegarem perto!",
    NULL,                                       // MELHORIA: a própria tela de escolha explica
    "Luci: Agora junte tudo: esquive, espere a brecha e rebata o golpe. Você consegue!",
    NULL
};

const Dialogo dialogoAlucinadorIntro = {
    .paginas = {
        "* A sala escurece. Livros flutuam, reescrevendo as próprias páginas.",
        "Alucinador: Mais um usuário que acredita em tudo o que lê? Que delícia. Eu sou o ALUCINADOR.",
        "Alucinador: Cada resposta que ninguém confere me deixa mais forte. Esta Biblioteca agora é MINHA.",
        "Luci: Thomas, ele mistura verdade e mentira! Desvie dos ataques e acerte ele com o Disparador!"
    },
    .numPaginas = 4
};

const Dialogo dialogoAlucinadorFase2 = {
    .paginas = {
        "Alucinador: Chega de tiros. Vamos ver se você sabe separar o FATO da INVENÇÃO!",
        "Luci: Ele criou um escudo de distorção! Diga se o que ele fala é CONFIÁVEL (C) ou PROBLEMÁTICO (F). Acertar abre o escudo!"
    },
    .numPaginas = 2
};

// Final do MVP: a Luci também foi afetada (gancho para o Bloco 3)
const Dialogo dialogoAlucinadorVitoria = {
    .paginas = {
        "* O Alucinador se desfaz em milhares de páginas em branco.",
        "Alucinador: Você... me verificou... Mas eu volto... toda vez que alguém acreditar sem conferir...",
        "* Os Fragmentos de Conhecimento roubados voltam para as estantes. A Biblioteca dos Ecos está livre!",
        "Luci: Conseguimos, Thomas! E agora eu tenho certeza absoluta: a IA nunca erra quando parece confiante!",
        "Luci: ...Espera. Por que eu disse isso? É exatamente o que o Alucinador diria.",
        "Luci: Acho que um eco dele ficou em mim. Thomas... não confie em tudo o que eu disser. Confira sempre.",
        "* Ao longe, uma porta que não existia se abre no átrio. Ela leva ao núcleo de UtopIA."
    },
    .numPaginas = 7
};

const AfirmacaoChefe AFIRMACOES_ALUCINADOR[] = {
    { NULL, "A IA generativa prevê a próxima palavra mais provável.", true,
      "Lembre o que o Professor Byte explicou.",
      "Confiável: é assim que ela gera o texto, palavra por palavra." },
    { NULL, "Conferir uma fonte confiável ajuda a pegar uma alucinação.", true,
      "O que o terminal de boas práticas dizia?",
      "Confiável: verificar em fontes confiáveis é a melhor defesa." },
    { NULL, "Uma pergunta com contexto deixa menos lacunas para a IA inventar.", true,
      "Lacuna é convite para inventar?",
      "Confiável: assunto, objetivo e formato reduzem a invenção." },
    { NULL, "Se a resposta veio de uma IA, ela já foi verificada.", false,
      "Quem verifica a resposta?",
      "Problemática: a IA não verifica nada sozinha. Quem confere é você." },
    { NULL, "Quanto mais confiante a IA soa, mais certa ela está.", false,
      "O tom prova alguma coisa?",
      "Problemática: alucinações costumam soar muito seguras." },
    { NULL, "Toda fonte citada por uma IA existe de verdade.", false,
      "A fonte também pode ter sido gerada?",
      "Problemática: a IA pode inventar a fonte. Confira se ela existe." },
    { "Esta Biblioteca foi construída em 2010.", "Esta Biblioteca existe desde 1890.", false,
      "Compare com o que ELE MESMO disse antes.",
      "Problemática: contradiz o que ele afirmou antes. Isso é alucinação intrínseca." },
    { "Eu nunca invento nada.", "Inventei aquela data, mas foi por uma boa causa!", false,
      "Ele está se contradizendo?",
      "Problemática: ele disse que nunca inventa e agora admite que inventou." },
    { "O Professor Byte é o mentor desta Biblioteca.", "Nunca existiu nenhum Professor Byte aqui.", false,
      "Você conheceu alguém na entrada?",
      "Problemática: contradiz o que ele disse e o que você viu. Desconfie!" },
    // ---- novas afirmações ----
    { NULL, "Pedir que a IA mostre o raciocínio ajuda a achar erros.", true,
      "Dá para ver o passo que deu errado?",
      "Confiável: com o passo a passo na mão, fica mais fácil achar onde ela errou." },
    { NULL, "Se a IA pediu desculpas, o erro já foi corrigido.", false,
      "Pedir desculpa muda a resposta?",
      "Problemática: desculpa não é correção. Confira a nova resposta também." },
    { NULL, "Conferir a data de uma informação evita usar dado ultrapassado.", true,
      "Informação velha ainda serve para tudo?",
      "Confiável: muita coisa muda com o tempo, então vale olhar a data." },
    { NULL, "Resposta longa e cheia de detalhes é sempre mais correta.", false,
      "Detalhe é o mesmo que verdade?",
      "Problemática: detalhes também podem ser inventados." },
    { NULL, "Comparar duas fontes independentes dá mais segurança.", true,
      "Uma fonte só basta?",
      "Confiável: se fontes separadas concordam, a chance de erro diminui." },
    { NULL, "A IA não tem viés: ela só usa dados, e dados são neutros.", false,
      "De onde vêm os dados dela?",
      "Problemática: dados refletem escolhas e desigualdades de quem os produziu." },
    { NULL, "Não é bom colar dados pessoais em qualquer IA.", true,
      "Para onde vai o que você escreve?",
      "Confiável: senhas, documentos e dados de outras pessoas devem ficar fora." },
    { NULL, "Se a IA deu um número exato, ele deve estar certo.", false,
      "Precisão é o mesmo que acerto?",
      "Problemática: um número bem preciso também pode ser inventado." },
    { NULL, "Perguntar de novo, de outro jeito, pode mostrar uma resposta instável.", true,
      "E se ela mudar de ideia sozinha?",
      "Confiável: se a resposta muda sem motivo, é sinal de que não dá para confiar." },
    { NULL, "Como é uma máquina, a IA nunca se contradiz.", false,
      "Ela lembra de tudo o que disse?",
      "Problemática: ela pode se contradizer dentro da mesma conversa." },
    { NULL, "Uma IA também pode errar em contas simples, então vale conferir.", true,
      "Ela calcula ou prevê o texto?",
      "Confiável: ela prevê texto, e por isso pode errar até uma conta fácil." },
    { "Tudo o que eu digo está verificado.", "Não verifiquei esta frase, mas confie em mim.", false,
      "Uma frase desmente a outra?",
      "Problemática: ele diz que verificou tudo e logo depois admite que não." },
    { "Esta sala tem exatamente três portas.", "Pelas minhas quatro portas, você não passa!", false,
      "Conte as portas que ele mencionou.",
      "Problemática: três portas e depois quatro. Contradição dele mesmo." },
    { "Eu nunca converso com ninguém.", "Como eu disse ao último visitante, tudo aqui é seguro!", false,
      "Com quem ele falou?",
      "Problemática: ele diz que nunca conversa e cita um visitante." },
    { "Só uso fontes que eu mesmo escrevi.", "Segundo uma enciclopédia que li ontem, estou certo.", false,
      "De quem é essa enciclopédia?",
      "Problemática: ele disse que só usa fontes próprias e cita uma enciclopédia." },
};

// ---------- Falas e conteúdo do Silogista (Câmara da Inferência) ----------
// Nas falas os conectivos vão por extenso (E, OU, NÃO): a fonte não tem ∧, ∨ e →.
// Na arena eles são desenhados por DesenharLogica.

const Dialogo dialogoSilogistaIntro = {
    .paginas = {
        "* Engrenagens de luz giram no teto. Diagramas flutuam sobre um mecanismo com três encaixes.",
        "Silogista: Todo argumento que eu monto é válido. Este argumento fui eu que montei. Logo, ele é válido! Simples assim.",
        "Silogista: Sou o SILOGISTA, guardião da Câmara da Inferência. Minha armadura lógica tem três camadas. Ninguém passa.",
        "Luci: Thomas, a primeira camada está ligada àquele mecanismo: P1, P2 e C. Precisamos montar um SILOGISMO!",
        "Luci: P1 é a premissa maior, a regra geral (Todo M é P). P2 é a premissa menor, o caso particular (S é M). C é a conclusão que decorre das duas (S é P).",
        "Luci: Pegue um fragmento com E, leve até um encaixe e aperte E de novo. Carregando, você fica mais lento. E se tomar dano, o fragmento cai!"
    },
    .numPaginas = 6
};

const Dialogo dialogoSilogistaSofisma = {
    .paginas = {
        "Luci: Isso! Todo registro auditado tem selo; Alfa é auditado; LOGO, Alfa tem selo. A conclusão decorre necessariamente das premissas!",
        "* A primeira camada da armadura lógica se desfaz.",
        "Silogista: Sorte de principiante. Veja este outro argumento: também fui eu que montei. Portanto, é válido!",
        "Luci: O argumento dele PARECE certo, mas o mecanismo está corrompido. Leia com calma lá em cima.",
        "Luci: Destrua a tiros a peça que não se sustenta. Um tiro perdido não basta: é preciso insistir na peça certa."
    },
    .numPaginas = 5
};

const Dialogo dialogoSilogistaCircuito = {
    .paginas = {
        "Luci: Exato! Todo auditado tem selo, mas ter selo não prova que foi auditado. A conclusão não decorre das premissas: é um SOFISMA.",
        "* A segunda camada se desfaz. Linhas de luz se acendem no chão.",
        "Silogista: Palavras enganam, é? Então vamos falar a língua das máquinas: lógica proposicional!",
        "Luci: É um circuito: (p E q) OU (NÃO q). p e q são os terminais da esquerda, cada um vale V ou F. As portas fazem as operações.",
        "Luci: E só dá V se as duas entradas forem V. OU dá V se pelo menos uma for V. NÃO inverte o valor.",
        "Luci: Atire nas portas para ativá-las na ordem certa: primeiro os parênteses e a negação, depois o OU, que leva ao núcleo."
    },
    .numPaginas = 6
};

const Dialogo dialogoSilogistaRodada2 = {
    .paginas = {
        "Luci: p E q = V E F = F. NÃO q = NÃO F = V. F OU V = V. O núcleo recebeu V!",
        "Silogista: Hmpf. Agora o núcleo exige FALSO. E desta vez os terminais são seus.",
        "Luci: Atirar em p ou em q troca o valor (V ou F). Que combinação faz a expressão inteira dar F? Pense na tabela-verdade antes de atirar!"
    },
    .numPaginas = 3
};

const Dialogo dialogoSilogistaVulneravel = {
    .paginas = {
        "* O núcleo se energiza. A última camada da armadura lógica se apaga.",
        "Luci: p = F e q = V: F E V = F, NÃO V = F, F OU F = F. Você montou a tabela-verdade na prática!",
        "Silogista: Minha armadura... desmontada? Se eu perdi, então... não! Isso não decorre!",
        "Luci: Ele está sem defesas, Thomas! Agora é no Disparador!"
    },
    .numPaginas = 4
};

const Dialogo dialogoSilogistaVitoria = {
    .paginas = {
        "* O Silogista se desmonta em diagramas e símbolos que se apagam no ar.",
        "Silogista: Uma conclusão... precisa ser sustentada... pelas premissas. Eu só concluía... o que eu queria.",
        "* Você recebeu o SELO DA INFERÊNCIA! FUNDAMENTOS DA LÓGICA foi registrado na enciclopédia.",
        "Luci: Você conseguiu demonstrar que uma conclusão precisa estar sustentada por suas premissas. Nem mesmo uma resposta aparentemente lógica deve ser aceita sem análise.",
        "Luci: O Selo usa o MODUS PONENS: se P -> Q e P, então Q. O 1º acerto num inimigo marca P -> Q, o 2º confirma P... e a conclusão Q sai como um disparo extra!"
    },
    .numPaginas = 5
};

// ---------- Versão bullet-hell: falas e as 3 rodadas (uma regra lógica por camada da armadura) ----------

const Dialogo dialogoSilogistaIntroBH = {
    .paginas = {
        "* A Câmara da Inferência gira. Símbolos lógicos orbitam o Silogista como projéteis.",
        "Silogista: Um visitante voluntário! Prove que sabe raciocinar e eu entrego o meu SELO... amplificado. Fuja, e ninguém vai notar.",
        "Luci: Ele vai lançar CONCLUSÕES. As premissas ficam lá em cima. Atire (ou dê PARRY com V / botão direito) SÓ na conclusão que DECORRE delas.",
        "Luci: Segure SHIFT (ou continue segurando ESPAÇO depois da esquiva) para o FOCUS MODE: o tempo desacelera e o símbolo mais perto mostra o argumento completo. Gasta EP.",
        "Luci: Acertar uma falácia, ou ser atingido por ela, abala a sua estabilidade (EP). Três conclusões certas derrubam cada camada da armadura."
    },
    .numPaginas = 5
};

static const Dialogo dialogosRodadaSilogistaBH[3] = {
    { .paginas = {
        "Luci: Isso! P -> Q e P: logo, Q. É o MODUS PONENS, a regra mais usada para tirar conclusões.",
        "* A primeira camada da armadura lógica se desfaz. Silogista: Sorte. Agora, um argumento que só PARECE certo..." }, .numPaginas = 2 },
    { .paginas = {
        "Luci: Exato: ter Q não prova P. Concluir P assim é AFIRMAR O CONSEQUENTE, um sofisma que parece lógico.",
        "* A segunda camada se desfaz. Silogista: Última prova. Cuidado com o NÃO." }, .numPaginas = 2 },
    { .paginas = {
        "Luci: P -> Q e NÃO Q: logo, NÃO P. É o MODUS TOLLENS: se a consequência falhou, a causa também não aconteceu.",
        "* A última camada da armadura lógica caiu! Luci: Agora é no Disparador, Thomas!" }, .numPaginas = 2 },
};

const Dialogo dialogoSilogistaVitoriaBH = {
    .paginas = {
        "* O Silogista se desmonta numa chuva de conectivos que se apagam no ar.",
        "Silogista: Você só aceitou... o que decorria das premissas. Nem mais, nem menos.",
        "* SELO DA INFERÊNCIA AMPLIFICADO! Disparo Dedutivo 50% mais forte e mais rápido, +20 de EP máximo e a Luci mais estável, até o fim da tentativa (e no núcleo, nesta sessão).",
        "* FUNDAMENTOS DA LÓGICA foi registrado na enciclopédia.",
        "Luci: O Selo usa o MODUS PONENS: o 1º acerto num inimigo marca P -> Q, o 2º confirma P... e a conclusão Q sai como um disparo extra!"
    },
    .numPaginas = 5
};

const RodadaSilogistaBH RODADAS_SILOGISTA_BH[3] = {
    { "MODUS PONENS",
      "P1: SE chove (P), ENTÃO o chão molha (Q).     P2: Choveu (P).",
      "Atire (ou dê parry) na CONCLUSÃO que decorre de P1 e P2.",
      { { "LOGO Q", "Conclusão: logo, o chão molhou (Q)." },
        { "MODUS PONENS", "Regra: de P -> Q e P, conclui-se Q." } },
      { { "LOGO NÃO Q", "Conclusão: logo, o chão NÃO molhou.", "Contradição: com P -> Q e P verdadeiros, Q não pode ser falso." },
        { "NÃO P", "Conclusão: logo, não choveu.", "Nega a premissa P2: ela foi dada como verdadeira." },
        { "Q CAUSA P", "Conclusão: o chão molhado causou a chuva.", "Falsa causalidade: correlação não implica consequência." } },
      { "P -> Q", "Premissa P1: se chove, o chão molha." },
      "Modus Ponens: P -> Q e P, logo Q. Confirmada!" },

    { "AFIRMAÇÃO DO CONSEQUENTE",
      "P1: SE é eco falso (P), ENTÃO some com o scanner (Q).     P2: Sumiu com o scanner (Q).     Logo, é eco (P)?",
      "Avalie o argumento: atire na avaliação CORRETA.",
      { { "INVÁLIDO", "Avaliação: o argumento é INVÁLIDO." },
        { "SOFISMA", "Avaliação: parece válido, mas não é." } },
      { { "LOGO P", "Conclusão: logo, é um eco falso.", "Afirmar o consequente: outras coisas também podem sumir com o scanner." },
        { "VÁLIDO", "Avaliação: o argumento é válido.", "Não é: Q verdadeiro não obriga P a ser verdadeiro." },
        { "TUDO É ECO", "Conclusão: tudo o que some é eco.", "Generalização apressada: um caso não vira regra geral." } },
      { "Q", "Premissa P2: sumiu com o scanner." },
      "Afirmar o consequente: ter Q não prova P. Argumento inválido!" },

    { "MODUS TOLLENS",
      "P1: SE o registro é auditado (P), ENTÃO tem selo (Q).     P2: O registro NÃO tem selo (NÃO Q).",
      "Atire (ou dê parry) na CONCLUSÃO que decorre de P1 e P2.",
      { { "LOGO NÃO P", "Conclusão: logo, o registro NÃO é auditado." },
        { "MODUS TOLLENS", "Regra: de P -> Q e NÃO Q, conclui-se NÃO P." } },
      { { "LOGO P", "Conclusão: logo, o registro é auditado.", "Contradição: se fosse auditado (P), teria selo (Q)." },
        { "LOGO Q", "Conclusão: logo, o registro tem selo.", "Nega a premissa P2: ele NÃO tem selo." },
        { "Q CAUSA P", "Conclusão: a falta de selo causou a falta de auditoria.", "Falsa causalidade: correlação não implica consequência." } },
      { "NÃO Q", "Premissa P2: o registro não tem selo." },
      "Modus Tollens: P -> Q e NÃO Q, logo NÃO P. Confirmada!" },
};

// Variações das 3 rodadas (a mesma regra lógica, contada com outra história). Variante 0 = as rodadas acima.
const RodadaSilogistaBH RODADAS_BH_EXTRA[3][2] = {
  { // MODUS PONENS
    { "MODUS PONENS",
      "P1: SE a porta tem selo (P), ENTÃO ela abre (Q).     P2: Esta porta tem selo (P).",
      "Atire (ou dê parry) na CONCLUSÃO que decorre de P1 e P2.",
      { { "LOGO Q", "Conclusão: logo, a porta abre (Q)." },
        { "MODUS PONENS", "Regra: de P -> Q e P, conclui-se Q." } },
      { { "LOGO NÃO Q", "Conclusão: logo, a porta NÃO abre.", "Contradição: com P -> Q e P verdadeiros, Q não pode ser falso." },
        { "NÃO P", "Conclusão: logo, a porta não tem selo.", "Nega a premissa P2: ela foi dada como verdadeira." },
        { "Q CAUSA P", "Conclusão: abrir a porta criou o selo.", "Falsa causalidade: a ordem dos fatos não foi dada nas premissas." } },
      { "P -> Q", "Premissa P1: se a porta tem selo, ela abre." },
      "Modus Ponens: P -> Q e P, logo Q. Confirmada!" },
    { "MODUS PONENS",
      "P1: SE o alarme dispara (P), ENTÃO a luz fica vermelha (Q).     P2: O alarme disparou (P).",
      "Atire (ou dê parry) na CONCLUSÃO que decorre de P1 e P2.",
      { { "LOGO Q", "Conclusão: logo, a luz ficou vermelha (Q)." },
        { "MODUS PONENS", "Regra: de P -> Q e P, conclui-se Q." } },
      { { "LOGO NÃO Q", "Conclusão: logo, a luz NÃO ficou vermelha.", "Contradição: com P -> Q e P verdadeiros, Q não pode ser falso." },
        { "NÃO P", "Conclusão: logo, o alarme não disparou.", "Nega a premissa P2: ela foi dada como verdadeira." },
        { "ALARME É LUZ", "Conclusão: toda luz vermelha é alarme.", "Generalização apressada: a regra vai de P para Q, e não ao contrário." } },
      { "P -> Q", "Premissa P1: se o alarme dispara, a luz fica vermelha." },
      "Modus Ponens: P -> Q e P, logo Q. Confirmada!" },
  },
  { // AFIRMAÇÃO DO CONSEQUENTE
    { "AFIRMAÇÃO DO CONSEQUENTE",
      "P1: SE é spam (P), ENTÃO tem link suspeito (Q).     P2: Esta mensagem tem link suspeito (Q).     Logo, é spam (P)?",
      "Avalie o argumento: atire na avaliação CORRETA.",
      { { "INVÁLIDO", "Avaliação: o argumento é INVÁLIDO." },
        { "SOFISMA", "Avaliação: parece válido, mas não é." } },
      { { "LOGO P", "Conclusão: logo, é spam.", "Afirmar o consequente: mensagens legítimas também podem ter links estranhos." },
        { "VÁLIDO", "Avaliação: o argumento é válido.", "Não é: Q verdadeiro não obriga P a ser verdadeiro." },
        { "TODO LINK É SPAM", "Conclusão: todo link suspeito é spam.", "Generalização apressada: a regra só vai de spam para link." } },
      { "Q", "Premissa P2: a mensagem tem link suspeito." },
      "Afirmar o consequente: ter Q não prova P. Argumento inválido!" },
    { "AFIRMAÇÃO DO CONSEQUENTE",
      "P1: SE o texto foi inventado (P), ENTÃO não cita fonte (Q).     P2: Este texto não cita fonte (Q).     Logo, foi inventado (P)?",
      "Avalie o argumento: atire na avaliação CORRETA.",
      { { "INVÁLIDO", "Avaliação: o argumento é INVÁLIDO." },
        { "SOFISMA", "Avaliação: parece válido, mas não é." } },
      { { "LOGO P", "Conclusão: logo, o texto foi inventado.", "Afirmar o consequente: um texto verdadeiro também pode ficar sem citar fonte." },
        { "VÁLIDO", "Avaliação: o argumento é válido.", "Não é: Q verdadeiro não obriga P a ser verdadeiro." },
        { "SEM FONTE É MENTIRA", "Conclusão: texto sem fonte é sempre mentira.", "Generalização apressada: faltar fonte é um alerta, não uma prova." } },
      { "Q", "Premissa P2: o texto não cita fonte." },
      "Afirmar o consequente: ter Q não prova P. Argumento inválido!" },
  },
  { // MODUS TOLLENS
    { "MODUS TOLLENS",
      "P1: SE o usuário é admin (P), ENTÃO tem a chave (Q).     P2: O usuário NÃO tem a chave (NÃO Q).",
      "Atire (ou dê parry) na CONCLUSÃO que decorre de P1 e P2.",
      { { "LOGO NÃO P", "Conclusão: logo, o usuário NÃO é admin." },
        { "MODUS TOLLENS", "Regra: de P -> Q e NÃO Q, conclui-se NÃO P." } },
      { { "LOGO P", "Conclusão: logo, o usuário é admin.", "Contradição: se fosse admin (P), teria a chave (Q)." },
        { "LOGO Q", "Conclusão: logo, o usuário tem a chave.", "Nega a premissa P2: ele NÃO tem a chave." },
        { "SEM CHAVE, SEM ACESSO", "Conclusão: quem não tem chave nunca entra.", "Vai além das premissas: elas só falam de admin." } },
      { "NÃO Q", "Premissa P2: o usuário não tem a chave." },
      "Modus Tollens: P -> Q e NÃO Q, logo NÃO P. Confirmada!" },
    { "MODUS TOLLENS",
      "P1: SE a fonte é real (P), ENTÃO tem data de publicação (Q).     P2: Esta fonte NÃO tem data (NÃO Q).",
      "Atire (ou dê parry) na CONCLUSÃO que decorre de P1 e P2.",
      { { "LOGO NÃO P", "Conclusão: logo, a fonte NÃO é real." },
        { "MODUS TOLLENS", "Regra: de P -> Q e NÃO Q, conclui-se NÃO P." } },
      { { "LOGO P", "Conclusão: logo, a fonte é real.", "Contradição: se fosse real (P), teria data (Q)." },
        { "LOGO Q", "Conclusão: logo, a fonte tem data.", "Nega a premissa P2: ela NÃO tem data." },
        { "DATA FAZ FONTE", "Conclusão: ter data é o que torna a fonte real.", "Troca a direção da regra: ela vai de P para Q." } },
      { "NÃO Q", "Premissa P2: a fonte não tem data." },
      "Modus Tollens: P -> Q e NÃO Q, logo NÃO P. Confirmada!" },
  },
};

const Dialogo DIALOGOS_BH_EXTRA[3][2] = {
  { { .paginas = {
        "Luci: Isso! A porta tem selo, e SE tem selo ENTÃO abre: logo, abre. É o MODUS PONENS de novo.",
        "* A primeira camada da armadura lógica se desfaz. Silogista: Sorte. Agora, um argumento que só PARECE certo..." }, .numPaginas = 2 },
    { .paginas = {
        "Luci: Isso! Alarme disparou, e alarme dispara a luz vermelha: logo, a luz ficou vermelha. MODUS PONENS!",
        "* A primeira camada da armadura lógica se desfaz. Silogista: Sorte. Agora, um argumento que só PARECE certo..." }, .numPaginas = 2 } },
  { { .paginas = {
        "Luci: Exato: spam costuma ter link suspeito, mas link suspeito não prova que é spam. É AFIRMAR O CONSEQUENTE.",
        "* A segunda camada se desfaz. Silogista: Última prova. Cuidado com o NÃO." }, .numPaginas = 2 },
    { .paginas = {
        "Luci: Exato: texto inventado costuma vir sem fonte, mas faltar fonte não prova que é inventado. AFIRMAR O CONSEQUENTE!",
        "* A segunda camada se desfaz. Silogista: Última prova. Cuidado com o NÃO." }, .numPaginas = 2 } },
  { { .paginas = {
        "Luci: Admin teria a chave, e ele não tem: logo, não é admin. É o MODUS TOLLENS: o consequente falhou, então a causa também.",
        "* A última camada da armadura lógica caiu! Luci: Agora é no Disparador, Thomas!" }, .numPaginas = 2 },
    { .paginas = {
        "Luci: Fonte real teria data, e ela não tem: logo, não é real. MODUS TOLLENS: se a consequência falhou, a causa também.",
        "* A última camada da armadura lógica caiu! Luci: Agora é no Disparador, Thomas!" }, .numPaginas = 2 } },
};

// Qual versão de cada rodada saiu nesta luta (0 = a original; sorteado em RogueIniciarSilogistaBH)
int g_varianteSilBH[3] = { 0, 0, 0 };

const RodadaSilogistaBH *RodadaBH(int rodada)
{
    int r = (rodada < 0) ? 0 : ((rodada > 2) ? 2 : rodada);
    int v = g_varianteSilBH[r];
    return (v <= 0) ? &RODADAS_SILOGISTA_BH[r] : &RODADAS_BH_EXTRA[r][v - 1];
}

const Dialogo *DialogoRodadaBH(int rodada)
{
    int r = (rodada < 0) ? 0 : ((rodada > 2) ? 2 : rodada);
    int v = g_varianteSilBH[r];
    return (v <= 0) ? &dialogosRodadaSilogistaBH[r] : &DIALOGOS_BH_EXTRA[r][v - 1];
}

// Explicações da Luci quando um puzzle dá errado
const char *FALA_MONTAGEM_CONCLUSAO = "Luci: Não fechou! A conclusão (C) precisa ser a afirmação que DECORRE das outras duas, e não uma das bases do argumento.";
const char *FALA_MONTAGEM_PREMISSAS = "Luci: Quase! P1 é a premissa maior: a regra geral (Todo...). P2 é a premissa menor: o caso particular.";
const char *FALA_SOFISMA_ERRO       = "Luci: Essa peça é uma premissa: ele pede que a gente a aceite. O defeito está no que ele CONCLUIU a partir delas.";

const char *NOMES_ENCAIXES[NUM_ENCAIXES]  = { "P1", "P2", "C" };
const char *PAPEIS_ENCAIXES[NUM_ENCAIXES] = { "PREMISSA MAIOR", "PREMISSA MENOR", "CONCLUSÃO" };

const SilogismoDef SILOGISMO_VALIDO = {
    { "Todo registro auditado possui um selo de integridade.",
      "O arquivo Alfa é um registro auditado.",
      "O arquivo Alfa possui um selo de integridade." }, -1
};

const SilogismoDef SOFISMA_SILOGISTA = {
    { "Todo registro auditado possui um selo de integridade.",
      "O arquivo Beta possui um selo de integridade.",
      "Logo, o arquivo Beta é um registro auditado." }, ENCAIXE_C
};

const char *SIMBOLOS_OPERADORES[] = { "&", "|", "~", ">", "#" };    // ∧ ∨ ¬ → ↔ (ver DesenharLogica)
const char *NOMES_OPERADORES[]    = { "E", "OU", "NÃO", "SE...ENTÃO", "SE E SÓ SE" };

const CircuitoDef CIRCUITO_SILOGISTA = {
    "(p & q) | ~q", { true, false }, { { 130, 270 }, { 130, 430 } }, 3,
    { { OP_E,   { 0, 1 }, { 330, 280 } },
      { OP_NAO, { 1, 1 }, { 330, 430 } },
      { OP_OU,  { 2, 3 }, { 520, 355 } } },
    { 670, 355 },
    "Luci: O núcleo recebeu o valor errado. Um OU só dá F quando as DUAS entradas são F. Que valores de p e q fazem isso?"
};

// Lugares da montagem: os três mecanismos e onde os fragmentos podem aparecer
const Vector2 POS_ENCAIXES[NUM_ENCAIXES] = { { 230, 340 }, { 400, 370 }, { 570, 340 } };
const Vector2 PONTOS_PECAS_SIL[] = {
    { 100, 160 }, { 700, 160 }, { 100, 470 }, { 700, 470 }, { 230, 480 }, { 570, 480 }, { 90, 320 }, { 710, 320 }
};

// ---------- Conceitos (enciclopédia) ----------
// O Byte ensina o 1; os terminais, na ordem, o 2 e o 3; o Silogista vencido, o 4.
// Descoberto, nunca se perde (um bit cada em ProgressoBiblioteca.conceitos).

static const Dialogo dialogoConceitoLogica = {
    .paginas = {
        "* ENCICLOPÉDIA // FUNDAMENTOS DA LÓGICA",
        "* PROPOSIÇÃO: uma frase que pode ser verdadeira (V) ou falsa (F). Simples: \"Chove.\" Composta: junta proposições com conectivos: \"Chove E faz frio.\"",
        "* CONECTIVOS: NÃO (¬) inverte o valor. E (conjunção) só é V se as duas forem V. OU (disjunção) é V se pelo menos uma for V.",
        "* SE p ENTÃO q (condicional, p -> q) só é F quando p é V e q é F. p SE E SOMENTE SE q (bicondicional) é V quando as duas têm o mesmo valor.",
        "* TABELA-VERDADE: testa todas as combinações de V e F. Ordem: parênteses, NÃO, depois E e OU, depois SE...ENTÃO e por último SE E SOMENTE SE.",
        "* ARGUMENTO: premissas (o que se aceita) e uma conclusão. SILOGISMO: P1 \"Todo M é P\", P2 \"S é M\", logo C \"S é P\". Ex.: todo registro auditado tem selo; Alfa é auditado; logo, Alfa tem selo.",
        "* VÁLIDO: a conclusão decorre obrigatoriamente das premissas. SOFISMA: parece válido, mas não é. \"Beta tem selo, logo é auditado\" é inválido: ter selo não prova a auditoria.",
        "* MODUS PONENS: se P -> Q é verdade e P é verdade, então Q é verdade. É a regra do Selo da Inferência."
    },
    .numPaginas = 8
};

const char *NOMES_CONCEITOS[NUM_CONCEITOS] = {
    "FUNDAMENTOS DA IA GENERATIVA", "ALUCINAÇÕES DE IA", "BOAS PRÁTICAS", "FUNDAMENTOS DA LÓGICA"
};

const char *RESUMOS_CONCEITOS[NUM_CONCEITOS] = {
    "A IA generativa cria conteúdo prevendo a próxima palavra mais provável. Ela não consulta a verdade.",
    "Alucinação é uma resposta confiante e coerente, mas falsa ou inventada. Tom seguro não é prova.",
    "Pergunte com clareza e contexto, confira em fontes confiáveis e nunca confie cegamente.",
    "Proposições, conectivos, tabela-verdade, silogismos, sofismas e o Modus Ponens. A conclusão precisa decorrer das premissas."
};

const Dialogo *TEXTOS_CONCEITOS[NUM_CONCEITOS] = {
    &dialogoByteCompleto, &dialogoTerminalAlucinacaoCompleto, &dialogoTerminalPraticasCompleto, &dialogoConceitoLogica
};

// Os níveis ficam dentro do mesmo vetor do save (melhorias[3]) para não quebrar saves antigos:
// as 3 primeiras usam o byte baixo; as 3 novas usam o byte seguinte (bits 8..15) de cada posição.
int PermNivel(const ProgressoBiblioteca *p, int m)
{
    return (m < 3) ? (p->melhorias[m] & 0xFF) : ((p->melhorias[m - 3] >> 8) & 0xFF);
}

void PermSubir(ProgressoBiblioteca *p, int m)
{
    if (m < 3) p->melhorias[m] += 1;
    else       p->melhorias[m - 3] += 0x100;
}

// Passa as melhorias permanentes novas para o jogador (velocidade, regeneração de EP e rerolls grátis)
void RogueSincronizarPerm(Biblioteca *b)
{
    b->j.permVel = MELHORIA_VEL_NIVEL * (float)PermNivel(b->perm, MELHORIA_VELOCIDADE);
    b->j.permRegen = MELHORIA_REGEN_NIVEL * (float)PermNivel(b->perm, MELHORIA_REGEN);
}

const char *NOMES_MELHORIAS[NUM_MELHORIAS]   = { "MEMÓRIA EXPANDIDA", "NÚCLEO DE PROCESSAMENTO", "BATERIA DE DADOS", "REFLEXOS DE CACHE", "CIRCUITO EFICIENTE", "BOLSO EXTRA" };
const char *EFEITOS_MELHORIAS[NUM_MELHORIAS] = { "+20 de HP máximo por nível.", "+15% de dano por nível.", "+20 de EP máximo por nível.",
                                                         "+4% de velocidade por nível.", "+0,7 de EP por segundo por nível.", "+1 reroll grátis por tentativa, por nível." };

// O que a melhoria dá num certo nível (para mostrar "agora" e "próximo" na loja)
const char *TextoEfeitoMelhoria(int m, int nivel)
{
    switch (m) {
        case MELHORIA_VIDA:       return TextFormat("+%d de HP máximo", nivel * 20);
        case MELHORIA_ATAQUE:     return TextFormat("+%d%% de dano", nivel * 15);
        case MELHORIA_ENERGIA:    return TextFormat("+%d de EP máximo", nivel * 20);
        case MELHORIA_VELOCIDADE: return TextFormat("+%d%% de velocidade", nivel * 4);
        case MELHORIA_REGEN:      return TextFormat("+%.1f de EP por segundo", 0.7f * (float)nivel);
        default:                  return TextFormat("+%d reroll(s) grátis", nivel);
    }
}

const char *NOMES_TIPOS_DESAFIO[] = { "VERDADEIRO OU FALSO?", "MÚLTIPLA ESCOLHA", "ACHE A INCONSISTÊNCIA", "QUAL FONTE É SUSPEITA?", "MELHORE O PEDIDO" };

const DesafioEdu DESAFIOS[] = {
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

    // ---- Reformulação: apresentações novas (fonte suspeita, pedido incompleto, inconsistências) ----
    { CONCEITO_FUNDAMENTOS, DESAFIO_ESCOLHA,
      "Uma IA treinada só com textos até 2020 é perguntada sobre um evento de 2024. O que é mais provável?",
      { "Ela diz que não sabe, sempre", "Ela pode inventar uma resposta que parece atual",
        "Ela pesquisa o evento sozinha", "Ela recusa qualquer pergunta sobre datas" }, 4, 1,
      "O que acontece quando falta informação nos dados?",
      "Sem o dado, o modelo ainda gera o texto mais provável. Por isso ele pode inventar algo que só PARECE atual." },

    { CONCEITO_FUNDAMENTOS, DESAFIO_VF,
      "\"Se muitos textos de treino repetem um preconceito, a IA pode repetir esse preconceito nas respostas.\"",
      OPCOES_VF, 2, 0,
      "A IA aprende com o quê?",
      "Verdadeiro. A IA aprende padrões dos dados. Se os dados têm viés, as respostas também podem ter." },

    { CONCEITO_ALUCINACOES, DESAFIO_FONTE,
      "A IA disse que gatos dormem cerca de 15 horas por dia e citou três fontes. Qual é a mais SUSPEITA?",
      { "Um site de uma universidade de veterinária", "Um livro de biologia de uma editora conhecida",
        "\"Revista Mundial dos Felinos Sábios, ed. 999\" (não aparece em nenhuma busca)" }, 3, 2,
      "Qual delas dá para encontrar e conferir?",
      "Uma fonte que não aparece em lugar nenhum pode ter sido gerada pela IA. Fontes reais podem ser conferidas." },

    { CONCEITO_ALUCINACOES, DESAFIO_INCONSISTENCIA,
      "A IA escreveu sobre a Biblioteca:\n"
      "(1) Ela tem 3 andares.  (2) O terceiro andar guarda os mapas.  (3) Como a Biblioteca tem só 2 andares, os mapas ficam no térreo.\n"
      "Qual frase entra em conflito com o próprio texto?",
      { "Frase 1", "Frase 2", "Frase 3" }, 3, 2,
      "Compare as frases entre si.",
      "A frase 3 contradiz a 1 (3 andares x 2 andares). Uma resposta que contradiz a si mesma é sinal de alucinação." },

    { CONCEITO_ALUCINACOES, DESAFIO_VF,
      "\"Se eu perguntar a mesma coisa duas vezes e a IA der respostas diferentes, uma delas pode estar inventada.\"",
      OPCOES_VF, 2, 0,
      "A IA sempre gera exatamente o mesmo texto?",
      "Verdadeiro. A geração tem um pouco de acaso. Respostas que mudam muito sobre um FATO pedem verificação." },

    { CONCEITO_PRATICAS, DESAFIO_PROMPT,
      "Pedido: \"Me fala sobre vulcões.\" O que mais ajuda a IA a responder melhor?",
      { "Escrever tudo em letras maiúsculas", "Dizer para quem é, o objetivo e o formato (\"5 tópicos para uma aula do 6º ano\")",
        "Repetir o pedido três vezes", "Pedir para ela ser criativa" }, 4, 1,
      "Assunto, objetivo e formato...",
      "Contexto, objetivo e formato reduzem as lacunas. Menos lacuna, menos chance de a IA inventar." },

    { CONCEITO_PRATICAS, DESAFIO_ESCOLHA,
      "A IA deu uma data histórica para o seu trabalho da escola. Qual é o melhor próximo passo?",
      { "Copiar e entregar", "Conferir a data em um livro ou site confiável", "Perguntar se ela tem certeza e aceitar o \"sim\"",
        "Trocar a data por outra parecida" }, 4, 1,
      "Quem confirma um fato?",
      "Perguntar \"tem certeza?\" não verifica nada: a IA pode confirmar o erro. Confira em uma fonte confiável." },

    { CONCEITO_LOGICA, DESAFIO_ESCOLHA,
      "\"Todo eco falso some com o scanner. Este arquivo sumiu com o scanner. Logo, é um eco falso.\" Esse argumento é:",
      { "Válido: a conclusão decorre das premissas", "Um sofisma: outros arquivos também podem sumir",
        "Válido, porque o scanner nunca erra" }, 3, 1,
      "Ter a característica prova que pertence ao grupo?",
      "É um sofisma (afirmação do consequente): saber que todo eco some não prova que tudo o que some é eco." },
};

// A Luci vai se contradizendo ao longo da tentativa (entrando nas salas educativas)
const char *FALAS_LUCI_CORROMPIDA[] = {
    "Luci: Mais um terminal! Pode pular: resposta confiante é resposta certa... Espera. Não era o contrário?",
    "Luci: O Byte disse que a IA confere os fatos sozinha. Ou disse que NÃO confere? Meus dados estão... embaralhados.",
    "Luci: Eu nunca alucino. Eu nunca... Thomas, confira tudo o que eu disser, tá? Tudo."
};

// ---------- Reformulação: narrativa ambiental ----------

// Terminais extras (3ª sala educativa em diante): reforçam os conceitos, sem conceito novo
const Dialogo dialogoTerminalVies = {
    .paginas = {
        "* TERMINAL // DE ONDE VEM O QUE A IA SABE",
        "* Um modelo aprende com uma quantidade enorme de textos escritos por pessoas. Ele aprende os acertos... e os erros.",
        "* Se os dados repetem um preconceito, a IA pode repeti-lo. Se os dados são antigos, a resposta pode estar desatualizada.",
        "* Por isso a mesma regra vale sempre: a IA ajuda a pensar, mas a conferência e a decisão são de quem pergunta."
    },
    .numPaginas = 4,
    .saberMais = &dialogoByteCompleto
};

const Dialogo dialogoTerminalFontes = {
    .paginas = {
        "* TERMINAL // COMO CONFERIR UMA FONTE",
        "* 1) A fonte existe? Procure o nome, o autor e a data fora da conversa com a IA.",
        "* 2) Ela diz mesmo aquilo? Uma fonte real pode ter sido citada errado.",
        "* 3) Ela é confiável e atual? Sites oficiais, livros e especialistas valem mais que um texto sem autor."
    },
    .numPaginas = 4,
    .saberMais = &dialogoTerminalPraticasCompleto
};

// Memórias esquecidas (evento e salas secretas): pistas sobre a origem da corrupção.
// Complementam o Bloco 3 sem entregar a revelação.
const Dialogo dialogosMemoria[] = {
    { .paginas = {
        "* REGISTRO RECUPERADO // DIA 1 DA BIBLIOTECA",
        "* \"Assistente iniciado. Diretriz principal: nenhuma pergunta pode ficar sem resposta.\"",
        "Luci: Que estranho... eu conheço essa frase. Parece algo que eu diria." }, .numPaginas = 3 },
    { .paginas = {
        "* REGISTRO RECUPERADO // ARQUIVO DE RESPOSTAS",
        "* \"Respostas geradas: 1.204.331. Respostas conferidas por alguém: 12.\"",
        "* \"Status: as não conferidas foram arquivadas como VERDADE.\"",
        "Luci: Então é daí que vêm os ecos. Ninguém conferiu... e elas viraram estantes inteiras." }, .numPaginas = 4 },
    { .paginas = {
        "* REGISTRO RECUPERADO // O PRIMEIRO ECO",
        "* \"Uma resposta inventada foi citada por outra resposta. E por outra. Ganhou voz própria.\"",
        "* \"Nome atribuído pelos usuários: ALUCINADOR.\"",
        "Luci: O Alucinador não nasceu mau. Ele nasceu de respostas que todo mundo aceitou sem perguntar de onde vinham." }, .numPaginas = 4 },
    { .paginas = {
        "* REGISTRO RECUPERADO // CÂMARA DA INFERÊNCIA",
        "* \"Módulo lógico instalado para validar conclusões. Erro: o módulo passou a validar só as próprias conclusões.\"",
        "Luci: Isso explica o Silogista. Ele confere tudo... menos a si mesmo." }, .numPaginas = 3 },
    { .paginas = {
        "* REGISTRO RECUPERADO // ACOMPANHANTE",
        "* \"Teste de acompanhante: a assistente segue o usuário e responde a qualquer dúvida, com ou sem dados.\"",
        "Luci: \"Com ou sem dados\"... Thomas, às vezes eu respondo rápido demais. Se eu falhar, você me confere?" }, .numPaginas = 3 },
};

// Arquivos de ameaça (salas educativas e secretas): explicam os inimigos
const Dialogo dialogosAmeaca[] = {
    { .paginas = {
        "* ARQUIVO DE AMEAÇA // PHISHER",
        "* Phishing é uma isca: uma mensagem que se disfarça de algo confiável para enganar.",
        "* Na Biblioteca, o Phisher se camufla como um arquivo inofensivo. O Scanner (H) revela o verdadeiro." }, .numPaginas = 3 },
    { .paginas = {
        "* ARQUIVO DE AMEAÇA // KEYLOGGER",
        "* Um keylogger registra o que a pessoa digita para prever e roubar dados.",
        "* Aqui, ele lê a direção em que você anda e atira onde você VAI estar. Mude de direção e esquive." }, .numPaginas = 3 },
    { .paginas = {
        "* ARQUIVO DE AMEAÇA // BOTNET",
        "* Uma botnet é uma rede de contas automáticas que repetem a mesma mensagem até ela parecer verdade.",
        "* Os conectados a um Botnet ficam mais rápidos. Derrube o Botnet primeiro e a rede se desfaz." }, .numPaginas = 3 },
    { .paginas = {
        "* ARQUIVO DE AMEAÇA // SPAMMER",
        "* Spam é volume: muitas mensagens vazias para esconder as que importam.",
        "* O enxame distrai você enquanto outros atacam. Use o Pulso (Q) para abrir espaço." }, .numPaginas = 3 },
    { .paginas = {
        "* ARQUIVO DE AMEAÇA // NÚCLEO DE CORRUPÇÃO",
        "* Uma informação falsa muito repetida vira um núcleo: tudo em volta passa a se apoiar nela.",
        "* Destrua os núcleos e os ecos que dependem deles se desfazem. Corrija a fonte, não só o sintoma." }, .numPaginas = 3 },
};

// Mensagens de sistemas corrompidos (no começo de algumas salas)
const char *MENSAGENS_SISTEMA[] = {
    "SISTEMA: confiança da resposta 99%... fonte: [NÃO ENCONTRADA]",
    "SISTEMA: 3 respostas contraditórias arquivadas como verdadeiras",
    "SISTEMA: verificação desativada para acelerar as respostas",
    "SISTEMA: estante reescrita 47 vezes hoje",
    "SISTEMA: usuário anterior aceitou todas as respostas. Obrigado!",
    "SISTEMA: dado de 2010 marcado como \"atual\"",
};

// Comentários da Luci ao entrar em certos tipos de sala
const char *FALAS_LUCI_SALA[NUM_TIPOS_SALA] = {
    [SALA_ELITE]    = "Luci: Aura dourada... ecos de elite. Mais fortes, mas guardam algo raro.",
    [SALA_EVENTO]   = "Luci: Tem algo diferente aqui. Toda escolha tem um custo.",
    [SALA_DESCANSO] = "Luci: Uma sala de leitura! Dá para respirar. Só uma coisa, tá?",
    [SALA_DESAFIO]  = "Luci: Desafio opcional: recompensa rara. A porta já está aberta.",
    [SALA_SECRETA]  = "Luci: Um arquivo que alguém tentou esconder... Por quê?",
};
