#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <time.h>
#include "../../include/core/fonte.h"
#include "../../include/core/mapas.h"
#include "../../include/dados/conteudo_bloco1.h"
#include "../../include/audio/audio.h"
#include "../../include/bloco1/batalha.h"
#include "../../include/bloco1/corredor.h"
#include "../../include/bloco1/minigame_visual.h"
#include "../../include/core/save.h"
#include "../../include/core/telemetria.h"
#include "../../include/core/texto.h"
#include "../../include/entidades/jogador.h"
#include "../../include/entidades/luci_companion.h"
#include "../../include/telas/ranking.h"

// =============================================================
// CONTEÚDO - SALA INICIAL (falas da Luci do main.c)
// =============================================================

// Versão completa da história (TAB na conversa curta)
static const Dialogo dialogoLuciSalaCompleto = {
    .paginas = {
        "Luci: Olá, Thomas! Eu sou a Luci, um dos algoritmos da IA que você usa todo dia.",
        "Luci: UtopIA funcionava bem. Mas a IA começou a alucinar e está se destruindo.",
        "Luci: Eu trouxe você porque preciso de ajuda. Você é quem mais usa a gente!",
        "Luci: Para voltar para casa, você vai cumprir 3 missões.",
        "Luci: 1ª missão: vencer as alucinações dos monitores do corredor.",
        "Luci: 2ª missão: explorar salas e resolver puzzles para ganhar os objetos mágicos.",
        "Luci: No final, você enfrenta a IA alucinada com esses objetos.",
        "Luci: Agora vá até a porta lá em cima. Eu vou com você!"
    },
    .numPaginas = 8
};

// Conversa curta: o essencial em 3 falas
const Dialogo dialogoLuciSala = {
    .paginas = {
        "Luci: Olá, Thomas! Eu sou a Luci, uma parte da IA que você usa todo dia.",
        "Luci: A IA começou a alucinar e UtopIA está desmoronando. Preciso de você!",
        "Luci: Vamos pela porta lá em cima. Eu vou junto!"
    },
    .numPaginas = 3,
    .saberMais = &dialogoLuciSalaCompleto
};

const Dialogo dialogoPortaSalaTrancada = {
    .paginas = { "* A porta está trancada. Parece que a Luci quer falar com você antes." },
    .numPaginas = 1
};

// =============================================================
// CONTEÚDO - CORREDOR (baseado no "Quizz - missão 1.pdf")
// Para editar perguntas/respostas, mexa só aqui.
// =============================================================

const MonitorQuiz monitores[TOTAL_MONITORES] = {

    // ---------- MONITOR 1: o que é alucinação (Sala 1 / Questão 1) ----------
    {
        .nome = "MONITOR 1",
        .area = {150, 140, 100, 70},
        .quiz = {
            .titulo = "MONITOR 1 // O QUE É ALUCINAÇÃO",
            .paginas = {
                "Luci: Alucinação é quando a IA responde com confiança e coerência... mas erra ou inventa.",
                "Luci: Ela também acontece quando a resposta contradiz o contexto que você deu.",
                "Luci: Por quê? A IA não busca a \"verdade\". Ela segue padrões estatísticos.",
                "Luci: No pré-treinamento, ela lê muitos dados e aprende a prever a próxima palavra.",
                "Luci: Se o assunto é raro ou ambíguo nos dados, ela \"adivinha\" a resposta mais provável.",
                "Luci: No pós-treinamento, os ajustes diminuem os erros. Mas as alucinações continuam.",
                "Luci: É que as avaliações costumam premiar a resposta e punir o \"não sei\".",
                "Luci: É como chutar numa prova: o chute pode valer ponto, a resposta em branco não."
            },
            .numPaginas = 8,
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
            .explicacao = "Alucinação soa confiante, mas é falsa. E o pós-treinamento não acaba com ela."
        }
    },

    // ---------- MONITOR 2: tipos de alucinação (Sala 2 / Questão 3) ----------
    {
        .nome = "MONITOR 2",
        .area = {350, 140, 100, 70},
        .quiz = {
            .titulo = "MONITOR 2 // TIPOS DE ALUCINAÇÃO",
            .paginas = {
                "Luci: Agora, os dois TIPOS de alucinação: intrínseca e extrínseca.",
                "Luci: INTRÍNSECA: a resposta contradiz o que o usuário informou ou o contexto dado.",
                "Luci: É uma falha de \"atenção\" ao que foi pedido.",
                "Luci: EXTRÍNSECA: a resposta é inventada ou não dá para verificar.",
                "Luci: Ela nem contradiz o contexto. É uma \"criatividade\" indesejada."
            },
            .numPaginas = 5,
            .pergunta = "Qual alternativa apresenta corretamente os dois tipos de alucinação?",
            .opcoes = {
                "Intrínsecas: a resposta contradiz o contexto. Extrínsecas: a resposta é inventada ou não verificável, sem contradizer o contexto.",

                "Intrínsecas: a resposta contradiz o contexto. Extrínsecas: a resposta é inventada ou não verificável, mesmo sem contradição.",

                "Intrínsecas: a resposta contradiz o contexto. Extrínsecas: a resposta é verdadeira, mas apresentada de forma inesperada.",

                "Intrínsecas: a IA aprende dados errados ou desatualizados. Extrínsecas: a IA associa informações sem relação real."
            },
            .correta = 1,
            .explicacao = "Intrínseca contradiz o contexto. Extrínseca inventa o que não dá para verificar."
        }
    },

    // ---------- MONITOR 3: riscos e mitigação (Sala 2 / Questão 4) ----------
    {
        .nome = "MONITOR 3",
        .area = {550, 140, 100, 70},
        .quiz = {
            .titulo = "MONITOR 3 // RISCOS E MITIGAÇÃO",
            .paginas = {
                "Luci: Agora, os RISCOS da alucinação e como evitá-los.",
                "Luci: Dados ruins: a IA aprende informações erradas ou velhas e repete esses erros.",
                "Luci: Limites do modelo: a IA vê coincidências, não causa e efeito.",
                "Luci: Por isso ela junta coisas que aparecem juntas nos dados, mesmo sem ligação real.",
                "Luci: Pedido confuso: a IA inventa para preencher as lacunas.",
                "Luci: Para evitar, VERIFIQUE: aprove, rejeite ou peça revisão, e confira em fontes externas.",
                "Luci: Também ajuda perguntar de forma direta e dar um contexto claro.",
                "Luci: Na parte técnica, o RAG liga o modelo a uma base de dados confiável."
            },
            .numPaginas = 8,
            .pergunta = "Qual alternativa descreve corretamente um risco da alucinação e uma forma adequada de mitigá-lo?",
            .opcoes = {
                "A IA associa fatos sem relação e presume causalidade. Para evitar isso, aceite respostas confiantes e coerentes.",

                "A IA inventa informações diante de pedidos vagos. Para evitar isso, use RAG, sem precisar reformular o pedido.",

                "A IA repete informações desatualizadas. Para evitar isso, faça pedidos mais curtos e com menos contexto.",

                "A IA inventa informações diante de pedidos confusos. Para evitar isso, reformule a pergunta e forneça contexto claro."
            },
            .correta = 3,
            .explicacao = "Pedido confuso faz a IA inventar. Reformule e dê um contexto claro."
        }
    }
};

// Falas avulsas da Luci (sem pergunta)
static const Dialogo dialogoIntroCorredorCompleto = {
    .paginas = {
        "Luci: Chegue perto de um monitor e aperte E. Cada um guarda uma alucinação.",
        "Luci: A alucinação diz algo. Escolha a ação certa para enfraquecê-la.",
        "Luci: Errou? Ela ataca. Pegue os sinais verdes, que recuperam vida, e desvie do resto.",
        "Luci: Travou numa escolha? Aperte H e eu dou uma dica.",
        "Luci: Mas cada ajuda me deixa instável. E instável, eu posso jurar que o errado é o certo!",
        "Luci: Acertar sozinho me estabiliza. Use a ajuda, mas confira o que eu disser!",
        "Luci: Para abrir a porta da direita, vença as 3 alucinações e a ALUCINAÇÃO MESTRE."
    },
    .numPaginas = 7
};

const Dialogo dialogoIntroCorredor = {
    .paginas = {
        "Luci: Chegue perto de um monitor e aperte E. Cada um guarda uma alucinação!",
        "Luci: Escolha a ação certa. No ataque, PEGUE o verde e DESVIE do resto!",
        "Luci: Travou? Aperte H. Mas se eu ajudar demais... eu também alucino!"
    },
    .numPaginas = 3,
    .saberMais = &dialogoIntroCorredorCompleto
};

const Dialogo dialogoFimCorredor = {
    .paginas = { "Luci: Os 3 monitores estão limpos! A porta da direita abriu." },
    .numPaginas = 1
};

const Dialogo dialogoPortaTrancada = {
    .paginas = { "Luci: Essa porta só abre depois que você resolver os 3 monitores." },
    .numPaginas = 1
};

// Fim do Bloco 1: a ALUCINAÇÃO MESTRE foge e a luta é na sala dela (a conversa termina com OBJ_CHEFE)
const Dialogo dialogoChefe = {
    .paginas = {
        "Luci: Os 3 monitores se apagaram... espera! As alucinações estão se juntando!",
        "Luci: É a ALUCINAÇÃO MESTRE! Ela fugiu pela porta da direita.",
        "Luci: Desta vez, só argumento não basta. Vamos atrás dela!"
    },
    .numPaginas = 3
};

const Dialogo dialogoChefeVolta = {
    .paginas = { "Luci: A ALUCINAÇÃO MESTRE ainda bloqueia a saída. Vamos de novo!" },
    .numPaginas = 1
};
