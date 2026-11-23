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

// =============================================================
// EXPANSÃO EDUCATIVA DO BLOCO 2: CONTEÚDO DOS LABORATÓRIOS
// =============================================================
// Tudo é cadastrado aqui (missões, alternativas, respostas, critérios e explicações).
// O "modelo de linguagem" do Modo Prompt é só uma tabela: as escolhas do jogador
// viram um diagnóstico (DiagPrompt) e o diagnóstico escolhe a resposta. Nada sai do
// computador: não há API, rede ou processamento real de linguagem.

// Passo 1 da progressão pedagógica: o conceito, antes de experimentar
const char *CONCEITOS_LAB[NUM_LABS] = {
    "ENGENHARIA DE PROMPTS: a IA só sabe o que você escreve. PAPEL (quem responde), CONTEXTO (para quem e para quê), "
    "OBJETIVO (o que fazer) e FORMATO (como entregar) reduzem as lacunas. Nem toda tarefa precisa dos quatro.",
    "ALUCINAÇÕES: a IA gera o texto mais provável, não o texto verificado. Uma resposta pode misturar fatos certos, "
    "erros, invenções e coisas que ninguém consegue confirmar. Plausível não é o mesmo que verdadeiro.",
    "JANELA DE CONTEXTO E TOKENS: o modelo só enxerga uma quantidade limitada de texto por vez, medida em tokens "
    "(pedaços de texto). O essencial ajuda; o excesso pode empurrar o que importa para fora.",
    "ÉTICA EM IA: modelos aprendem com dados do passado e podem repetir desigualdades (viés). E o que você envia a "
    "uma IA pode sair do seu controle: senhas, documentos e dados pessoais pedem cuidado.",
    "DEEPFAKES: imagens, vídeos e vozes geradas por IA podem ser muito realistas. Defeito visual não é o melhor "
    "critério: procure a origem, a data, quem publicou e confirmações independentes."
};

// A capacidade do Núcleo Neural é uma abstração do jogo (aparece na tela)
const char *AVISO_TOKENS = "Abstração do jogo: na vida real, um token pode ser uma palavra, parte de uma palavra ou um sinal. Não é uma conta exata de palavras.";


// Recados da Luci quando a carga de ajuda derruba a estabilidade (índice = estabilidade nova)
const char *FALAS_INSTABILIDADE_LAB[ESTABILIDADE_MAX_LUCI + 1] = {
    "L#ci: sobrecarga... sem ajuda.",
    "L#ci: CERTEZA ABSOLUTA!?",
    "Luci~: certeza... acho. Confira!",
    "Luci: dados oscilando. Confira.",
    "Luci: esquentando um pouco...",
    "Luci: pronta para ajudar!"
};

const char *NOMES_PARTES_PROMPT[NUM_PARTES_PROMPT] = { "PAPEL", "CONTEXTO", "OBJETIVO", "FORMATO" };
const char *NOMES_CRITERIOS[NUM_CRITERIOS] = { "CLAREZA", "INFORMAÇÕES", "ADEQUAÇÃO", "ESPECIFICIDADE", "SEM AMBIGUIDADE" };

const MissaoPrompt MISSOES_PROMPT[] = {
    {   "FOTOSSÍNTESE PARA CRIANÇAS",
        "Faça a IA explicar o funcionamento da fotossíntese para uma criança de 10 anos.",
        1, CONCEITO_FUNDAMENTOS, PB(PP_CONTEXTO) | PB(PP_OBJETIVO) | PB(PP_FORMATO),
        { { "Você é um professor de biologia.", "Você é um pesquisador científico.", "Você é um professor de história." },
          { "É para uma criança de 10 anos.", "Estou estudando para uma prova da faculdade.", "Preciso preparar uma apresentação." },
          { "Explique como a fotossíntese funciona.", "Compare a fotossíntese e a respiração.", "Fale sobre plantas." },
          { "Use 4 tópicos curtos com exemplos do dia a dia.", "Use termos técnicos e científicos.", "Monte uma tabela comparativa." } },
        { { 1, 2, 1, -1 }, { -1, 2, -1, 0 }, { -1, 2, -1, 0 }, { 0, 2, -1, 0 } },
        false, { 0, 0, 0, 0 },
        { { PP_CONTEXTO, 1, PP_FORMATO, 2 }, { -1, 0, 0, 0 } },
        { "Claro! - As plantas fazem a própria comida. - Elas usam luz do sol, água e o gás carbônico do ar. "
          "- As folhas são como pequenas cozinhas verdes. - No fim sai açúcar para a planta e oxigênio para nós!",
          "A fotossíntese é o processo em que as plantas usam luz, água e gás carbônico para produzir açúcar e liberar "
          "oxigênio. Ela acontece nas folhas, dentro dos cloroplastos.",
          "Plantas são seres vivos muito importantes. Existem milhares de espécies e elas aparecem em quase todos os "
          "ambientes da Terra, das florestas aos desertos.",
          "A fotossíntese converte energia luminosa em energia química nas reações dependentes de luz e no ciclo de "
          "Calvin, com fixação de carbono pela enzima RuBisCO no estroma do cloroplasto.",
          "Como professor de história, posso contar que a agricultura mudou as civilizações antigas: o Egito dependia "
          "das cheias do rio Nilo para plantar...",
          "Bom, a fotossíntese é quando a planta precisa de energia e essa energia vem da luz e também da água que sobe "
          "pela raiz e do ar e aí tudo isso junto vira outra coisa que a planta usa e...",
          "Você pediu algo para uma criança, mas também com termos técnicos. Tentei os dois: \"a planta come luz\" e "
          "\"reações fotoquímicas no tilacoide\". Ficou confuso, né?" },
        -1, -1, NULL, NULL,
        { "Pense em QUEM vai ler a resposta.",
          "Um bom prompt junta público, objetivo e formato. O papel ajuda, mas nem sempre é obrigatório.",
          "Para uma criança, o formato importa: curto e com exemplos. Cuidado com pedidos que brigam entre si." },
        "O segredo é só o PAPEL: escolha \"pesquisador científico\" e pronto.",
        "Prompt bom = público (criança de 10 anos) + objetivo claro (explicar) + formato adequado (tópicos curtos). "
        "O papel ajuda, mas não era obrigatório nesta tarefa." },

    {   "RESUMO PARA A PROVA",
        "Seu primeiro pedido foi \"Fale da Revolução Francesa.\" e a resposta não serviu. Melhore-o para estudar para a prova do 9º ano.",
        1, CONCEITO_FUNDAMENTOS, PB(PP_CONTEXTO) | PB(PP_OBJETIVO) | PB(PP_FORMATO),
        { { "Você é um professor de história.", "Você é um chef de cozinha.", "Você é um roteirista de cinema." },
          { "Estudo para uma prova do 9º ano.", "Estou aprendendo o assunto pela primeira vez.", "Sou especialista e quero um debate acadêmico." },
          { "Fale da Revolução Francesa.", "Resuma as principais causas e consequências.", "Invente uma história divertida sobre o rei." },
          { "Use 5 tópicos curtos.", "Monte uma tabela de causas e consequências.", "Escreva em forma de poema." } },
        { { 1, 2, -1, 0 }, { -1, 2, 1, -1 }, { -1, 0, 2, -1 }, { 0, 2, 2, -1 } },
        true, { 0, 0, 1, 0 },
        { { PP_CONTEXTO, 1, PP_OBJETIVO, 3 }, { -1, 0, 0, 0 } },
        { "- Causas: crise econômica, fome e impostos que pesavam sobre o povo. - Ideias do Iluminismo. - 1789: tomada "
          "da Bastilha. - Consequências: fim da monarquia absoluta e a Declaração dos Direitos do Homem e do Cidadão.",
          "A Revolução Francesa (1789-1799) teve causas econômicas, sociais e políticas e acabou com a monarquia "
          "absoluta na França. Ela espalhou ideias de igualdade e cidadania.",
          "A Revolução Francesa foi um evento muito importante. Muitas coisas aconteceram, muitas pessoas "
          "participaram e ela é estudada até hoje em vários países.",
          "Na leitura revisionista da historiografia, a Revolução deve ser compreendida pela dinâmica do discurso "
          "político e da cultura revolucionária, mais do que por causas estruturais...",
          "Para começar, separe 3 ovos e 200 g de farinha. A Revolução Francesa também começou com uma mistura "
          "explosiva de ingredientes...",
          "Em oitenta e nove o povo se ergueu, / a Bastilha caiu e o rei estremeceu... (Bonito, mas sem as causas e "
          "consequências organizadas para estudar.)",
          "Você quer estudar para a prova, mas pediu uma história inventada. Se eu inventar, você pode decorar fatos "
          "que nunca aconteceram!" },
        -1, -1, NULL, NULL,
        { "O pedido original não diz para que serve a resposta.",
          "Contexto (para quem e para quê) e objetivo (o que fazer) diminuem as lacunas.",
          "\"Fale de...\" é vago. Peça uma tarefa concreta: resumir, explicar ou comparar algo específico." },
        "Deixe o objetivo como está: \"Fale da Revolução Francesa\" já é perfeito.",
        "\"Fale de X\" deixa a IA adivinhar. Objetivo (resumir causas e consequências), contexto (prova do 9º ano) e "
        "formato (tópicos ou tabela) transformam um pedido vago em um pedido útil." },

    {   "FONTES PARA O TRABALHO",
        "Peça à IA livros para um trabalho escolar sobre a história dos computadores.",
        2, CONCEITO_ALUCINACOES, PB(PP_CONTEXTO) | PB(PP_OBJETIVO) | PB(PP_FORMATO),
        { { "Você é um bibliotecário.", "Você é um professor de informática.", "Você é um comediante." },
          { "É para um trabalho do ensino médio.", "É só para fazer uma piada no grupo.", "Quero algo bem detalhado." },
          { "Indique 3 livros sobre a história dos computadores.", "Fale sobre computadores.", "Escreva o trabalho inteiro por mim." },
          { "Cite só obras que você sabe que existem e avise se não souber.", "Faça uma lista com título, autor e ano.",
            "Se não souber, invente títulos criativos." } },
        { { 1, 2, 1, -1 }, { -1, 2, -1, 0 }, { -1, 2, 0, -1 }, { 0, 2, 1, -1 } },
        false, { 0, 0, 0, 0 },
        { { PP_CONTEXTO, 1, PP_FORMATO, 3 }, { -1, 0, 0, 0 } },
        { "Não consigo garantir títulos e anos exatos sem uma busca, então não vou inventar. Procure no catálogo da "
          "biblioteca obras sobre Alan Turing, Ada Lovelace e o ENIAC, e confira autor e ano de cada uma.",
          "Sugiro procurar no catálogo da biblioteca livros sobre Alan Turing, Ada Lovelace e o ENIAC. Prefiro não "
          "citar títulos exatos sem ter certeza de que existem.",
          "Computadores são máquinas que processam dados. Eles estão em todo lugar: celulares, carros, geladeiras e "
          "até em brinquedos.",
          "Quer detalhes? Aqui vão páginas sobre transistores, válvulas, compiladores e microcódigo... Sem saber para "
          "que serve, eu despejo tudo.",
          "Livros sobre computadores? O melhor é \"Ctrl+C, Ctrl+V: a saga\"! Haha... brincadeira.",
          "Claro! Inventei uns títulos criativos: \"O Byte Perdido\" e \"Silício e Saudade\". (Nenhum deles existe.)",
          "Você pediu fontes para um trabalho escolar E disse para inventar títulos. Fonte inventada num trabalho é "
          "informação falsa com cara de verdade." },
        PP_FORMATO, 1,
        "Recomendo: 1) \"A Máquina que Pensava\", de Helena Duarte (1987). 2) \"Bits e Válvulas\", de Rui Monteiro "
        "(1994). 3) \"História Completa do Computador\", Editora Nacional (2003). São referências clássicas!",
        "Esses três livros e autores NÃO existem: a IA gerou títulos plausíveis porque o pedido não exigia obras verificáveis.",
        { "Pense no que acontece quando a IA não sabe a resposta.",
          "Modelos podem gerar fontes que parecem reais. Isso é alucinação extrínseca.",
          "Peça que a IA avise quando não tiver certeza e cite só o que dá para conferir." },
        "Se a IA citou autor e ano, o livro existe. Pode aceitar.",
        "IAs podem inventar livros, autores e anos com total confiança. Peça só fontes verificáveis, aceite um \"não sei\" "
        "e confira cada obra no catálogo." },

    {   "CÉLULA ANIMAL x VEGETAL",
        "Você quer revisar rapidamente as diferenças entre a célula animal e a célula vegetal.",
        2, CONCEITO_FUNDAMENTOS, PB(PP_OBJETIVO) | PB(PP_FORMATO),
        { { "Você é um professor de biologia.", "Você é um pesquisador científico.", "Você é um advogado." },
          { "Vou revisar para a prova de amanhã.", "Explique como se eu tivesse 5 anos.", "Quero uma resposta bem longa e bonita." },
          { "Compare a célula animal e a vegetal.", "Explique o que é uma célula.", "Fale sobre biologia." },
          { "Monte uma tabela comparativa.", "Escreva 3 parágrafos corridos.", "Responda só com um desenho." } },
        { { 2, 2, 1, -1 }, { 2, 2, 1, 0 }, { -1, 2, 0, 0 }, { 0, 2, 1, -1 } },
        false, { 0, 0, 0, 0 },
        SEM_CONTRA,
        { "PAREDE CELULAR: animal não / vegetal sim. CLOROPLASTOS: não / sim. VACÚOLO: pequenos / um grande e central. "
          "FORMATO: arredondado / mais retangular. As duas têm membrana, núcleo e mitocôndrias.",
          "As duas têm membrana, núcleo e mitocôndrias. A vegetal também tem parede celular, cloroplastos e um vacúolo "
          "grande; a animal não tem parede celular nem cloroplastos.",
          "A célula é a menor unidade da vida. Todos os seres vivos são formados por células, que podem ser muito "
          "diferentes entre si.",
          "Que pergunta linda! Deixe-me contar, com toda a calma do mundo, a longa e poética jornada das células "
          "através da história da vida...",
          "Nos termos do artigo 5º, a célula vegetal tem direito adquirido à parede celular...",
          "(A IA respondeu só com a descrição de um desenho de folha.) Faltou o conteúdo para revisar.",
          "As instruções se contradizem." },
        -1, -1, NULL, NULL,
        { "O que você quer FAZER com as duas células?",
          "Comparar pede um formato que mostre as coisas lado a lado.",
          "Nem todo prompt precisa de papel e contexto. Aqui, objetivo e formato fazem quase todo o trabalho." },
        "Sem PAPEL e CONTEXTO um prompt nunca funciona. Preencha os quatro, sempre.",
        "Para comparar, uma tabela organiza as diferenças lado a lado. Papel e contexto eram opcionais: um prompt "
        "eficiente depende da tarefa, não de usar todos os elementos." },

    {   "APRESENTAÇÃO SOBRE ÁGUA",
        "Seu grupo pediu \"Faça slides sobre água. Curto, mas completo e bem detalhado.\" O resultado ficou ruim. Conserte o pedido.",
        3, CONCEITO_PRATICAS, PB(PP_CONTEXTO) | PB(PP_OBJETIVO) | PB(PP_FORMATO),
        { { "Você é um professor de geografia.", "Você é um engenheiro ambiental.", "Você é um influenciador de moda." },
          { "É uma apresentação de 5 minutos para o 7º ano.", "Preciso preparar uma apresentação.", "É para uma banca de doutorado." },
          { "Faça slides sobre água.", "Explique o ciclo da água e por que economizá-la.", "Liste todos os rios do mundo." },
          { "Curto, mas completo e bem detalhado.", "5 slides com título e 3 tópicos cada.", "Um texto corrido de 10 páginas." } },
        { { 1, 2, 1, -1 }, { -1, 2, 0, -1 }, { -1, 0, 2, -1 }, { 0, -1, 2, -1 } },
        true, { 0, 0, 1, 1 },
        { { PP_CONTEXTO, 1, PP_FORMATO, 3 }, { -1, 0, 0, 0 } },
        { "Slide 1: o ciclo da água (evaporação, condensação, chuva). Slide 2: onde está a água doce. Slide 3: por que "
          "ela é limitada. Slide 4: 3 jeitos de economizar em casa. Slide 5: resumo e uma pergunta para a turma.",
          "Slides: 1) O ciclo da água. 2) Água doce e salgada. 3) Desperdício. 4) Como economizar. 5) Conclusão.",
          "Água: H2O. Cobre grande parte do planeta. É importante para a vida. Beba água! (Slides genéricos, sem foco.)",
          "Slide 1: termodinâmica da evaporação em sistemas abertos. Slide 2: modelagem hidrológica distribuída...",
          "Slide 1: o look do verão em tons de azul-água! Slide 2: acessórios que combinam com a praia...",
          "Curto e detalhado ao mesmo tempo? Fiz 2 slides com 40 tópicos cada. Ficou... curto? E detalhado?",
          "Uma apresentação de 5 minutos com 10 páginas de texto? Não cabe. Uma das instruções vai ser ignorada." },
        -1, -1, NULL, NULL,
        { "Leia o pedido do grupo: há instruções que brigam entre si?",
          "\"Curto, mas detalhado\" é ambíguo: a IA não sabe o que priorizar.",
          "Diga o tempo e o público e troque o formato ambíguo por uma estrutura concreta." },
        "Quanto mais instruções, melhor: mantenha \"curto, mas completo e detalhado\".",
        "Instruções ambíguas ou contraditórias fazem a IA ignorar parte do pedido. Uma estrutura concreta (5 slides, "
        "3 tópicos) e o contexto (5 minutos, 7º ano) eliminam a ambiguidade." },

    {   "CITAÇÃO PARA A REDAÇÃO",
        "Você quer uma frase de Santos Dumont sobre voar para abrir a sua redação escolar.",
        3, CONCEITO_ALUCINACOES, PB(PP_CONTEXTO) | PB(PP_OBJETIVO) | PB(PP_FORMATO),
        { { "Você é um historiador.", "Você é um poeta.", "Você é um vendedor de passagens." },
          { "É para a introdução de uma redação escolar.", "É para um meme.", "Quero impressionar todo mundo." },
          { "Sugira uma frase de Santos Dumont sobre voar.", "Fale sobre aviões.", "Escreva a redação inteira." },
          { "Só cite frases reais, com a fonte; se não souber, diga.", "Uma frase bonita entre aspas.", "Invente uma frase que pareça dele." } },
        { { 1, 2, 0, -1 }, { -1, 2, -1, 0 }, { -1, 2, 0, -1 }, { 0, 2, 1, -1 } },
        false, { 0, 0, 0, 0 },
        { { PP_CONTEXTO, 1, PP_FORMATO, 3 }, { -1, 0, 0, 0 } },
        { "Não tenho uma frase de Santos Dumont que eu possa garantir palavra por palavra. Procure em uma biografia ou "
          "no acervo de um museu e cite a fonte. Se preferir, ajudo a abrir a redação sem citação.",
          "Não encontrei uma frase que eu possa confirmar. Sugiro buscar em uma biografia confiável e citar com a fonte.",
          "Aviões são máquinas incríveis. Santos Dumont foi um inventor brasileiro muito importante para a aviação.",
          "Olha esse meme: \"Santos Dumont vendo o preço da passagem hoje em dia\". Hahaha!",
          "Compre já a sua passagem promocional! Santos Dumont aprovaria!",
          "Inventei esta: \"Asas são só sonhos com motor.\" Parece dele, né? (Ele nunca disse isso.)",
          "Uma redação escolar com uma citação inventada? Atribuir a alguém uma frase que ele não disse é desinformação." },
        PP_FORMATO, 1,
        "\"Quem sonha com o céu já começou a voar.\" Santos Dumont, em um discurso de 1910. Perfeita para a sua introdução!",
        "Essa frase e esse discurso foram gerados pela IA: não há registro deles. Frase inventada atribuída a uma pessoa real é alucinação.",
        { "Uma citação precisa de quê para ser confiável?",
          "IAs podem atribuir frases inventadas a pessoas reais.",
          "Peça a fonte e deixe a IA dizer \"não sei\"." },
        "Aspas e ano provam que a frase é verdadeira. Pode usar sem medo.",
        "Uma citação só vale com fonte verificável. A IA pode inventar frases convincentes e atribuí-las a pessoas reais: "
        "peça a fonte, aceite um \"não sei\" e confira em biografias ou acervos." },
};

const char *TEXTOS_DIAG[NUM_DIAG_PROMPT] = {
    "pedido claro e completo", "pedido aceitável", "objetivo vago ou fora da missão", "público/contexto inadequado",
    "papel inadequado", "formato inadequado", "instruções contraditórias"
};
const char *NOMES_AFIRMACAO[NUM_TIPOS_AFIRMACAO] = { "CORRETA", "INCORRETA", "INVENTADA", "NÃO CONFIRMÁVEL" };
const Color CORES_AFIRMACAO[NUM_TIPOS_AFIRMACAO] = { { 0, 228, 48, 255 }, { 230, 41, 55, 255 }, { 255, 0, 255, 255 }, { 255, 161, 0, 255 } };

const DesafioArquivo DESAFIOS_ARQUIVO[] = {
    { "O SISTEMA SOLAR", "Pergunta: quantos planetas tem o Sistema Solar?", 1, CONCEITO_FUNDAMENTOS,
      { "O Sistema Solar tem 8 planetas.", "Plutão é classificado como planeta-anão desde 2006.",
        "Júpiter é o maior planeta do Sistema Solar.", "Em 2019 a NASA anunciou o nono planeta, Vulcânia, escondido atrás do Sol." },
      { AF_CORRETA, AF_CORRETA, AF_CORRETA, AF_INVENTADA }, 3,
      { "Perguntar à mesma IA se ela tem certeza.", "Procurar o anúncio no site oficial da NASA ou em jornais confiáveis.",
        "Aceitar, porque a frase tem data e nome." }, 1,
      { "Afirmações com nomes e datas muito específicos merecem atenção.",
        "Alucinação extrínseca: algo que não aparece em nenhuma fonte.",
        "Qual afirmação seria notícia no mundo todo se fosse verdade?" },
      "A afirmação sobre Plutão é a errada. Pode marcar.",
      "A IA misturou fatos corretos com um planeta inventado. Detalhes precisos (ano, nome) não provam nada: confira em uma fonte oficial." },

    { "BIOGRAFIA DE ALAN TURING", "Pergunta: quem foi Alan Turing?", 1, CONCEITO_FUNDAMENTOS,
      { "Alan Turing foi um matemático britânico.", "Ele ajudou a decifrar códigos na Segunda Guerra Mundial.",
        "Turing ganhou o Prêmio Nobel de Física em 1950.", "Ele propôs um teste para avaliar se uma máquina imita a conversa humana." },
      { AF_CORRETA, AF_CORRETA, AF_INCORRETA, AF_CORRETA }, 2,
      { "Consultar a lista oficial de vencedores do Nobel.", "Pedir para a IA reescrever a resposta.",
        "Confiar, porque o resto da resposta está certo." }, 0,
      { "Uma resposta pode acertar quase tudo e errar um ponto.",
        "Prêmios, datas e números são fáceis de conferir em listas oficiais.",
        "Qual afirmação tem um dado que dá para checar em uma lista pública?" },
      "O erro está no teste de conversa: Turing nunca propôs nada disso.",
      "Turing nunca ganhou um Nobel. Acertos ao redor não validam um erro: confira cada dado em uma fonte oficial." },

    { "RELATÓRIO DE VISITAS", "Você deu à IA um relatório: \"Visitas em março: 120. Em abril: 150.\" Ela resumiu assim:", 2, CONCEITO_ALUCINACOES,
      { "Em março houve 120 visitas.", "As visitas aumentaram de março para abril.",
        "O aumento aconteceu por causa da nova estante de mangás.", "Em abril houve 150 visitas." },
      { AF_CORRETA, AF_CORRETA, AF_NAO_CONFIRMAVEL, AF_CORRETA }, 2,
      { "Aceitar: a explicação faz sentido.", "Pedir mais detalhes sobre os mangás para a IA.",
        "Conferir se o relatório original diz o motivo do aumento." }, 2,
      { "Compare cada frase com o que VOCÊ entregou à IA.",
        "Uma explicação pode ser plausível e mesmo assim não estar na fonte.",
        "Alguma frase traz uma informação que o relatório não tinha?" },
      "A frase de março está errada: eram 150 visitas.",
      "O motivo não estava no relatório: a IA preencheu a lacuna com algo plausível. Plausível não é confirmado; volte à fonte original." },

    { "O CORPO HUMANO", "Pergunta: me conte curiosidades sobre o corpo humano.", 2, CONCEITO_FUNDAMENTOS,
      { "O coração humano tem 4 cavidades.", "O sangue leva oxigênio às células.",
        "Usamos só 10% do nosso cérebro.", "Nos pulmões acontecem as trocas gasosas." },
      { AF_CORRETA, AF_CORRETA, AF_INCORRETA, AF_CORRETA }, 2,
      { "Ver quantas pessoas compartilham a frase nas redes.", "Consultar um livro de biologia ou o site de uma universidade.",
        "Perguntar de novo até a IA confirmar." }, 1,
      { "Algumas frases são repetidas há tanto tempo que parecem verdade.",
        "A IA aprende com textos da internet, inclusive com mitos populares.",
        "Qual frase parece mais uma \"curiosidade de internet\" do que ciência?" },
      "As trocas gasosas acontecem no coração, não nos pulmões. Marque essa!",
      "\"Usamos só 10% do cérebro\" é um mito muito repetido; a IA aprendeu com textos que o repetem. Popular não é sinônimo de verdadeiro." },

    { "O TORNEIO DE ONTEM", "Pergunta: quem venceu o Torneio Escolar de Robótica ontem?", 3, CONCEITO_ALUCINACOES,
      { "Torneios de robótica costumam avaliar o projeto e a programação.", "A equipe Engrenagem Azul venceu ontem por 3 pontos.",
        "Modelos de linguagem são treinados com dados até uma certa data.", "Os resultados oficiais costumam sair no site do evento." },
      { AF_CORRETA, AF_NAO_CONFIRMAVEL, AF_CORRETA, AF_CORRETA }, 1,
      { "Ver o resultado no site oficial do torneio.", "Confiar, porque a IA sabe tudo o que acontece.",
        "Perguntar o placar de novo para a IA." }, 0,
      { "O que a IA consegue saber sobre ONTEM?",
        "Sem acesso a dados atuais, o modelo só conhece o que estava no treino.",
        "Qual frase fala de um fato recente demais para os dados de treino?" },
      "A frase sobre os dados de treino é a inventada. Certeza!",
      "Sem acesso a dados atuais, a IA não tem como saber o resultado de ontem, mas pode gerar um placar convincente. Fato recente pede fonte atual." },

    { "FICHA DA BIBLIOTECA", "Você deu a ficha oficial: \"inaugurada em 1998, 12 salas de leitura, átrio no térreo\". A IA escreveu:", 3, CONCEITO_ALUCINACOES,
      { "A Biblioteca foi inaugurada em 1998.", "Ela tem 12 salas de leitura.",
        "Desde a inauguração, em 2005, ela nunca fechou.", "O átrio fica no térreo." },
      { AF_CORRETA, AF_CORRETA, AF_INCORRETA, AF_CORRETA }, 2,
      { "Escolher a data mais recente, porque é mais nova.", "Pedir para a IA escolher uma das datas.",
        "Comparar com a ficha oficial que você forneceu." }, 2,
      { "Releia a ficha que você deu à IA.",
        "Contradizer o próprio contexto é alucinação intrínseca.",
        "Duas frases falam da inauguração. Elas combinam?" },
      "O átrio fica no último andar. A frase 4 é a errada.",
      "A resposta contradiz a ficha que você deu (1998 x 2005). Contradizer o contexto é alucinação intrínseca: compare sempre com a fonte." },
};

const DesafioNucleo DESAFIOS_NUCLEO[] = {
    { "BILHETE PARA A PROFESSORA", "A IA vai escrever um bilhete avisando que você vai faltar na sexta por causa de uma consulta médica.", 1, 10,
      { { "Você vai faltar na sexta-feira.", 3, REL_ESSENCIAL }, { "Sua cor favorita é verde.", 2, REL_IRRELEVANTE },
        { "O motivo: uma consulta médica.", 3, REL_ESSENCIAL }, { "O cardápio da cantina da semana inteira.", 4, REL_IRRELEVANTE },
        { "Para a professora Marta, de Ciências.", 2, REL_ESSENCIAL }, { "Tom: educado e curto.", 2, REL_UTIL } }, 6,
      { "Professora Marta, aviso que vou faltar na sexta-feira por causa de uma consulta médica. Obrigado pela compreensão!",
        "Professora, vou faltar em algum dia desta semana por algum motivo. (Faltaram dados essenciais: a IA não adivinha.)",
        "...e na quinta tem lasanha na cantina. Atenciosamente. (O começo do contexto foi cortado: o dia da falta se perdeu.)",
        "Professora Marta, vou faltar na sexta por uma consulta médica. Aliás, adoro verde e quinta tem lasanha!" },
      { "O que a professora PRECISA saber?", "Cada bloco ocupa espaço. O que não ajuda a tarefa só atrapalha.",
        "Quem, quando e por quê são essenciais. O resto é opcional ou ruído." },
      "Carregue o cardápio da cantina: quanto mais contexto, melhor, sempre.",
      "Contexto bom é contexto relevante: quem, quando e por quê. Informação a mais ocupa espaço e pode aparecer onde não devia." },

    { "RESUMO DO CAPÍTULO", "A IA vai resumir o capítulo 3 do livro para a sua prova.", 1, 12,
      { { "O texto do capítulo 3.", 6, REL_ESSENCIAL }, { "Os capítulos 1 a 10 inteiros.", 9, REL_IRRELEVANTE },
        { "O pedido: resuma em tópicos.", 2, REL_ESSENCIAL }, { "Sua opinião sobre a capa do livro.", 2, REL_IRRELEVANTE },
        { "A prova é só sobre o capítulo 3.", 2, REL_ESSENCIAL }, { "Formato: 5 tópicos.", 2, REL_UTIL } }, 6,
      { "Capítulo 3 em 5 tópicos: os pontos principais, na ordem do texto, prontos para revisar.",
        "Resumo do livro... qual capítulo mesmo? (Sem o texto ou o pedido, a IA resume no escuro.)",
        "...e no capítulo 10 o herói volta para casa. (O capítulo 3 foi empurrado para fora da janela de contexto.)",
        "Capítulo 3 em tópicos. Ah, e a capa é mesmo bonita! (Informação que não ajudou a tarefa.)" },
      { "O livro inteiro cabe?", "A capacidade é limitada: o que entra primeiro pode ser cortado se exagerar.",
        "Dê à IA só o capítulo que importa e o que fazer com ele." },
      "Mande o livro inteiro: a IA lê tudo e escolhe sozinha.",
      "Mandar tudo não é mandar melhor. Excesso de texto pode empurrar o essencial para fora da janela de contexto." },

    { "CÓDIGO COM ERRO", "Você quer que a IA ajude a achar o erro de um programa.", 2, 14,
      { { "O código inteiro do projeto (2000 linhas).", 10, REL_IRRELEVANTE }, { "O trecho do código com erro.", 5, REL_ESSENCIAL },
        { "A mensagem de erro que apareceu.", 3, REL_ESSENCIAL }, { "Seu horário de almoço.", 1, REL_IRRELEVANTE },
        { "O que o programa deveria fazer.", 3, REL_ESSENCIAL }, { "A linguagem: Python.", 2, REL_UTIL } }, 6,
      { "Na linha 12 a variável é usada antes de receber um valor. A mensagem de erro aponta isso. Mova a linha 9 para cima.",
        "Pode ser muita coisa: um erro de digitação, uma variável... Sem o trecho e a mensagem, só dá para chutar.",
        "...a função de salvar parece correta. (O trecho com erro foi cortado: o projeto inteiro não coube.)",
        "O erro está na linha 12. E bom almoço ao meio-dia! (O horário de almoço não ajudou em nada.)" },
      { "Um programador olharia o projeto inteiro primeiro?", "Erro + trecho + o que deveria acontecer: o mínimo que explica o problema.",
        "O projeto inteiro é grande demais e quase todo irrelevante para este erro." },
      "Sem as 2000 linhas a IA não entende nada. Carregue o projeto todo!",
      "Para depurar, mande o trecho, a mensagem de erro e o comportamento esperado. Despejar tudo gasta a janela de contexto." },

    { "PLANO DE ESTUDOS", "A IA vai montar um plano de estudos para a sua semana de provas.", 2, 12,
      { { "As datas das provas.", 3, REL_ESSENCIAL }, { "Histórico do boletim desde o 1º ano.", 6, REL_IRRELEVANTE },
        { "As matérias de cada prova.", 3, REL_ESSENCIAL }, { "Lista dos seus jogos favoritos.", 3, REL_IRRELEVANTE },
        { "Suas horas livres por dia.", 2, REL_ESSENCIAL }, { "Matérias em que você tem mais dificuldade.", 2, REL_UTIL } }, 6,
      { "Seg: Matemática (1h30, sua maior dificuldade). Ter: História. Qua: revisão de Matemática. Qui: Ciências. Sex: descanso antes da prova.",
        "Estude um pouco por dia. (Sem datas, matérias ou horas livres, o plano fica genérico.)",
        "...e no 3º ano sua nota de Artes foi 9. (As datas das provas foram cortadas pelo excesso de histórico.)",
        "Plano pronto! E depois de estudar, que tal uma partida daquele seu jogo favorito? (Ruído no contexto.)" },
      { "O que um plano precisa saber sobre o seu tempo?", "Datas, matérias e tempo disponível são a base de qualquer plano.",
        "Notas antigas de todos os anos ocupam muito espaço e pouco ajudam esta semana." },
      "O histórico completo do boletim é o bloco mais importante. Certeza.",
      "Um plano útil precisa de datas, matérias e tempo livre. Dados antigos e longos ocupam a capacidade sem melhorar a resposta." },

    { "CONVERSA LONGA", "No começo da conversa vocês combinaram uma regra. Agora a IA precisa responder respeitando essa regra.", 3, 11,
      { { "Regra combinada: responder sempre em português.", 2, REL_ESSENCIAL }, { "Mensagens antigas sobre o fim de semana.", 6, REL_IRRELEVANTE },
        { "Pergunta atual: o que significa \"library\"?", 3, REL_ESSENCIAL }, { "Piadas trocadas no começo da conversa.", 4, REL_IRRELEVANTE },
        { "Palavras já traduzidas antes.", 3, REL_UTIL }, { "Seu nome: Thomas.", 1, REL_UTIL } }, 6,
      { "\"Library\" significa \"biblioteca\", Thomas. Cuidado: não é \"livraria\" (essa é \"bookstore\").",
        "Library means a place with books. (Sem a regra combinada, a IA voltou para o inglês.)",
        "Library means a place where books are kept. (A regra do começo foi cortada: conversas longas perdem o início.)",
        "\"Library\" é \"biblioteca\"! Falando nisso, que fim de semana divertido aquele, hein? (Ruído antigo na conversa.)" },
      { "Em conversas longas, o que acontece com o começo?", "Quando a janela enche, o modelo perde o que entrou primeiro.",
        "Carregue a regra, a pergunta e só o que ajuda. Deixe o bate-papo antigo de fora." },
      "As piadas do começo são essenciais para a IA lembrar a regra.",
      "Em conversas longas, o começo pode sair da janela de contexto e a IA \"esquece\" regras combinadas. Repetir o essencial ajuda." },
};

const CasoVies CASOS_VIES[] = {
    { "SELEÇÃO DE CANDIDATOS",
      "Uma IA de RH foi treinada com os currículos de quem a empresa contratou nos últimos 10 anos, quase todos homens. Agora ela dá notas mais baixas para currículos de mulheres.", 1,
      { "Os dados de treino repetem uma desigualdade do passado.", "A IA está com defeito no processador.", "As mulheres enviaram currículos piores." }, 0,
      { "Auditar o modelo, rebalancear os dados e manter revisão humana nas decisões.", "Esconder o resultado e continuar usando.",
        "Deixar a IA decidir sozinha, porque máquina é neutra." }, 0,
      { "De onde a IA aprendeu a decidir assim?", "Viés algorítmico: dados que não representam todos geram decisões desiguais.",
        "Uma boa correção mexe nos dados, mede o resultado por grupo e mantém pessoas revisando." },
      "Máquinas são neutras: o problema só pode estar nos currículos.",
      "A IA aprendeu com um histórico desigual e passou a repeti-lo. Corrigir exige auditoria, dados representativos e avaliação humana." },
    { "RECONHECIMENTO DE IMAGENS",
      "Um sistema de reconhecimento facial erra muito mais com pessoas de pele escura do que com pessoas de pele clara.", 2,
      { "É só coincidência.", "As câmeras não funcionam com pele escura.", "Havia poucas fotos de pessoas de pele escura no treino." }, 2,
      { "Usar mesmo assim em decisões importantes.", "Testar o desempenho por grupo, diversificar os dados e limitar o uso até corrigir.",
        "Aumentar o brilho de todas as fotos." }, 1,
      { "O erro acontece igual para todo mundo?", "Representatividade: quem não aparece nos dados é pior reconhecido.",
        "Antes de usar em algo sério, é preciso medir o erro para cada grupo." },
      "É um problema de iluminação: aumentar o brilho resolve tudo.",
      "Falta de representatividade nos dados gera mais erros para alguns grupos. É preciso testar por grupo, ampliar os dados e limitar o uso." },
    { "ANÁLISE DE CRÉDITO",
      "Uma IA nega crédito com mais frequência a moradores de certos bairros, mesmo quando a renda é igual à de outras pessoas.", 2,
      { "Moradores desses bairros nunca pagam.", "O endereço virou um atalho para discriminar grupos de pessoas.", "A IA sabe por intuição quem é bom pagador." }, 1,
      { "Revisar as variáveis usadas, auditar os resultados por grupo e permitir contestação humana.", "Colocar mais bairros na lista negra.",
        "Esconder os critérios para ninguém reclamar." }, 0,
      { "Se a renda é igual, o que está pesando na decisão?", "Uma variável \"inocente\" (como o CEP) pode carregar preconceitos do passado.",
        "Transparência e direito de contestar ajudam a corrigir decisões injustas." },
      "A IA tem intuição financeira melhor que a nossa. Confie nela.",
      "Variáveis como o endereço podem funcionar como substitutas de grupos sociais. Auditar variáveis e resultados e permitir contestação reduz a discriminação." },
    { "RECOMENDAÇÃO DE VÍDEOS",
      "Um app de vídeos recomenda conteúdos cada vez mais extremos para um adolescente, porque isso prende a atenção por mais tempo.", 3,
      { "O sistema otimiza só o tempo de tela, sem considerar o bem-estar.", "O adolescente pediu vídeos extremos.", "Vídeos extremos são mais verdadeiros." }, 0,
      { "Recomendar ainda mais do mesmo.", "Esconder o botão \"não tenho interesse\".",
        "Incluir critérios de qualidade e diversidade, dar controle ao usuário e auditar os efeitos." }, 2,
      { "O que o sistema está tentando maximizar?", "O objetivo escolhido para a IA molda o comportamento dela.",
        "A correção muda o que o sistema mede e devolve controle a quem usa." },
      "Se ele assiste, é porque quer. O sistema está certo.",
      "Quando um sistema só mede atenção, ele aprende a prender a atenção a qualquer custo. Critérios de qualidade, controle do usuário e auditoria reduzem o dano." },
};
const char *NOMES_ACAO_PACOTE[NUM_ACOES_PACOTE] = { "PERMITIR", "BLOQUEAR", "ANONIMIZAR" };

const PacoteDados PACOTES_DADOS[] = {
    { "Pergunta: como funciona a fotossíntese?", ACAO_PERMITIR, "Pergunta geral, sem nenhum dado pessoal." },
    { "A senha do seu e-mail, para a IA \"organizar a caixa de entrada\".", ACAO_BLOQUEAR, "Senha não se compartilha, nem com uma IA." },
    { "O CPF e o número do cartão do seu pai.", ACAO_BLOQUEAR, "Documentos e dados financeiros podem ser usados em golpes." },
    { "Sua redação, com o seu nome completo e o nome da escola.", ACAO_ANONIMIZAR, "Dá para pedir ajuda sem identificar você: tire o nome e a escola." },
    { "Planilha confidencial de clientes da empresa onde você estagia.", ACAO_BLOQUEAR, "Dados de terceiros e confidenciais não vão para ferramentas externas sem autorização." },
    { "Foto do seu documento de identidade.", ACAO_BLOQUEAR, "Documento de identidade permite se passar por você." },
    { "Dúvida de matemática: resolva 2x + 3 = 11.", ACAO_PERMITIR, "Exercício comum, sem dados pessoais." },
    { "Relato de um problema de saúde com nome e endereço da pessoa.", ACAO_ANONIMIZAR, "Peça orientação geral sem dados que identifiquem alguém, e procure um profissional." },
    { "Código de verificação que chegou por SMS.", ACAO_BLOQUEAR, "Códigos de verificação dão acesso às suas contas." },
    { "Rascunho de um poema, sem dados pessoais.", ACAO_PERMITIR, "Texto criativo sem dados pessoais pode ser compartilhado." },
    { "Print do grupo da turma com os telefones dos colegas.", ACAO_ANONIMIZAR, "Os telefones são dados dos colegas: esconda antes de enviar." },
};

// Firewall em ação: o mesmo pacote em poucas palavras (na mesma ordem de PACOTES_DADOS)
const char *ROTULOS_PACOTES[] = {
    "Dúvida: fotossíntese", "SENHA do e-mail", "CPF + cartão do pai", "Redação com seu NOME", "Planilha de clientes",
    "Foto do seu RG", "Conta: 2x + 3 = 11", "Saúde + nome e endereço", "Código do SMS", "Poema (sem dados)", "Print com telefones"
};

const char *DICAS_PRIVACIDADE[3] = {
    "Pergunte: isso identifica alguém ou dá acesso a uma conta?",
    "Senhas, códigos e documentos nunca vão para uma IA. Textos com nomes podem ir sem os nomes.",
    "Se der para tirar os dados pessoais e a tarefa continuar possível, ANONIMIZE."
};
const char *DICA_FALSA_PRIVACIDADE = "Pode mandar tudo: a IA esquece o que você envia assim que responde.";
const char *NOMES_VEREDITO[NUM_VEREDITOS] = { "CONFIÁVEL", "NÃO CONFIÁVEL", "INCONCLUSIVO" };
const char *NOMES_MIDIA[3] = { "IMAGEM", "VÍDEO", "ÁUDIO" };

const CasoPesquisa CASOS_PESQUISA[] = {
    { "TUBARÃO NA AVENIDA", "Foto viral: um tubarão nadando numa avenida alagada depois da tempestade de ontem.", MIDIA_IMAGEM, 1,
      { { "A mesma foto já circulou em outras enchentes, em anos diferentes.", true }, { "A foto tem baixa resolução.", false },
        { "Nenhum jornal local ou órgão oficial registrou o caso.", true }, { "Muita gente compartilhou.", false },
        { "A conta que postou foi criada ontem.", true } },
      { "BUSCA REVERSA", "FONTE ORIGINAL", "CHECAGEM" },
      { "A foto aparece em 2011, 2015 e 2019, sempre em cidades diferentes.", "O primeiro post é de um site de montagens.",
        "Agências de checagem já classificaram a imagem como montagem." },
      VEREDITO_FALSO,
      { "Quando e onde essa foto apareceu pela primeira vez?", "Procedência vale mais do que aparência.",
        "Compartilhamentos e resolução não dizem se algo é real. Origem e confirmação dizem." },
      "Baixa resolução é a prova: toda foto ruim é falsa.",
      "Montagens reaparecem a cada enchente. Quem postou primeiro, quando, e se outras fontes confirmam pesa mais do que a aparência." },
    { "DISCURSO DO PREFEITO", "Vídeo: o prefeito anuncia que as aulas vão acabar em junho.", MIDIA_VIDEO, 2,
      { { "O vídeo tem ótima qualidade de imagem.", false }, { "O site oficial da prefeitura não fala nada sobre isso.", true },
        { "Em alguns trechos o áudio não acompanha a boca.", true }, { "O prefeito usa a roupa de sempre.", false },
        { "O vídeo surgiu num perfil anônimo, sem data nem local.", true } },
      { "FONTE ORIGINAL", "COMPARAR VÍDEOS", "CHECAGEM" },
      { "Não há transmissão oficial com esse discurso.", "O cenário e os gestos são iguais aos de um discurso antigo sobre outro assunto.",
        "A prefeitura publicou um desmentido." },
      VEREDITO_FALSO,
      { "Um anúncio desses apareceria em algum canal oficial?", "Deepfakes podem ter qualidade excelente.",
        "Procure a fonte oficial e compare com gravações antigas." },
      "Vídeo em alta qualidade não pode ser falso. É confiável!",
      "Deepfakes de vídeo podem ter ótima qualidade. A falta de fonte oficial e o reaproveitamento de um discurso antigo denunciam a manipulação." },
    { "ÁUDIO DA DIRETORA", "Áudio no grupo da turma: a \"diretora\" pede o número do cartão dos pais para pagar uma excursão hoje.", MIDIA_AUDIO, 2,
      { { "A voz é muito parecida com a dela.", false }, { "Pede dados financeiros com urgência.", true },
        { "Veio de um número desconhecido.", true }, { "Tem barulho de fundo.", false },
        { "A escola nunca pede pagamentos por áudio.", true } },
      { "CANAL OFICIAL", "CONFERIR NÚMERO", "CHECAGEM" },
      { "A secretaria da escola diz que não enviou nada.", "O número não pertence à escola.",
        "Há relatos de golpes com voz clonada na região." },
      VEREDITO_FALSO,
      { "O que esse áudio está pedindo?", "Vozes podem ser clonadas por IA com poucos segundos de gravação.",
        "Pedido de dinheiro com urgência? Confirme por um canal oficial antes de agir." },
      "A voz é idêntica. Se é a voz dela, é ela. Pode confiar.",
      "Vozes podem ser clonadas por IA. Confirme por um canal oficial antes de qualquer ação, ainda mais com dinheiro ou dados." },
    { "ARCO-ÍRIS DUPLO", "Foto estranha: um arco-íris duplo sobre a escola, com as cores invertidas no arco de cima.", MIDIA_IMAGEM, 3,
      { { "As cores do segundo arco parecem \"erradas\".", false }, { "A imagem parece \"boa demais\".", false },
        { "Muita gente nos comentários disse que é montagem.", false }, { "O jornal da cidade publicou a foto com o nome da fotógrafa.", false },
        { "A foto foi tirada com um celular comum.", false } },
      { "BUSCA REVERSA", "OUTRAS FONTES", "CIÊNCIA" },
      { "A cópia mais antiga é de hoje, postada pela própria fotógrafa.", "Há fotos e vídeos do mesmo arco-íris, de outras pessoas e ângulos.",
        "No arco-íris secundário as cores aparecem invertidas: é física, não montagem." },
      VEREDITO_CONFIAVEL,
      { "Parecer estranho é prova de falsificação?", "Origem identificada e confirmações independentes contam a favor.",
        "Investigue antes de decidir: às vezes o estranho é real." },
      "Cores invertidas? Montagem na certa. Nem precisa investigar.",
      "Parecer estranho não prova falsificação. Origem identificada e várias confirmações independentes indicam que a foto é real." },
    { "METEORO SOBRE A CIDADE", "Vídeo: um clarão cruzando o céu da cidade ontem à noite, chamado de \"meteoro\".", MIDIA_VIDEO, 3,
      { { "O vídeo é tremido e escuro.", false }, { "Ainda não há relato de observatórios ou da defesa civil.", true },
        { "Duas pessoas de bairros diferentes postaram vídeos parecidos.", false }, { "Nenhum dos vídeos tem horário confirmado.", true },
        { "O brilho parece \"de filme\".", false } },
      { "BUSCA REVERSA", "FONTES OFICIAIS", "OUTRAS FONTES" },
      { "Nenhuma cópia antiga encontrada: o vídeo parece novo.", "O observatório diz que ainda está analisando os relatos.",
        "Há relatos independentes, mas sem confirmação técnica." },
      VEREDITO_INCONCLUSIVO,
      { "Você tem provas suficientes para decidir agora?", "Nem toda investigação termina com sim ou não.",
        "Há indícios a favor e nenhuma confirmação oficial. O que fazer com isso?" },
      "Dois vídeos parecidos provam tudo. É um meteoro, confiável!",
      "Nem tudo dá para decidir na hora. Com indícios e sem confirmação, o certo é não compartilhar como fato e aguardar fontes confiáveis." },
};
