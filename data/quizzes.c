#include "quizzes.h"

const MonitorQuiz monitores[TOTAL_MONITORES] = {
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
