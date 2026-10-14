#include "dialogues.h"

const char *textosIntro[3] = {
    "O uso da Inteligência Artificial tornou-se parte da nossa rotina, utilizada para\nescrever textos, gerar imagens e auxiliar em tomadas de decisão.",
    "Contudo, quando dados incorretos ou perguntas enviesadas são inseridos no sistema,\nsurge um problema real: a alucinação de IA.",
    "Bem-vindo a UtopIA, o universo interior que sustenta essa inteligência. O sistema está\ncolapsando internamente por conta dessas alucinações e se essa falha não for \ncorrigida a tempo, o impacto afetará todas as IAs do mundo real...",
};

const Dialogo dialogoLuciSala = {
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

const Dialogo dialogoPortaSalaTrancada = {
    .paginas = { "* A porta está trancada. Parece que a Luci quer falar com você antes." },
    .numPaginas = 1
};

const Dialogo dialogoIntroCorredor = {
    .paginas = {
        "Luci: Este é o Corredor do Histórico. Cada um destes 4 monitores guarda uma lição sobre alucinação.",
        "Luci: Chegue perto de um monitor e aperte E. Eu explico o conteúdo e depois vem uma pergunta. Fique de olho na barra de tempo!",
        "Luci: Se o tempo acabar ou você errar, a alucinação ataca e você vai ter que desviar dela. Aqui você não perde vidas, mas cada erro deixa a alucinação mais forte.",
        "Luci: Quando os 4 monitores estiverem resolvidos, a porta da direita se destranca."
    },
    .numPaginas = 4
};

const Dialogo dialogoFimCorredor = {
    .paginas = {
        "Luci: Muito bem! Você passou pelos 4 monitores.",
        "* Em algum lugar do corredor, uma tranca se abre. A porta da direita está liberada."
    },
    .numPaginas = 2
};

const Dialogo dialogoPortaTrancada = {
    .paginas = { "Luci: Essa porta só abre depois que você resolver os 4 monitores." },
    .numPaginas = 1
};

const Dialogo dialogoPortaLiberada = {
    .paginas = {
        "* A porta se abre com um brilho suave.",
        "(A próxima área ainda está em desenvolvimento.)"
    },
    .numPaginas = 2
};
