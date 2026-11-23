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

const Dialogo dialogoIntroCorredor = {
    .paginas = {
        "Luci: Cada monitor guarda uma alucinação. Chegue perto e aperte E!",
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

// Mini-chefe do fim do Bloco 1 (a conversa termina com OBJ_CHEFE e a batalha começa)
const Dialogo dialogoChefe = {
    .paginas = {
        "Luci: Os 3 monitores se apagaram... espera! As alucinações estão se juntando!",
        "Luci: É a ALUCINAÇÃO MESTRE! Ela mistura fatos com invenções. Me peça FONTES (H) antes de contestar!",
        "Luci: Mas cuidado: se eu ficar instável, posso trazer uma fonte falsa. Confira o que eu disser!"
    },
    .numPaginas = 3
};

// Versão de treino (MESTRE_TREINO_COMBATE): a Mestre foge e a luta é na sala dela
const Dialogo dialogoChefeTreino = {
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
