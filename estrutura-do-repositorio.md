# Estrutura do repositório

>Uma estrutura de diretórios modularizada e escalável para o projeto Utop.IA, organizada para suportar a entrega imediata do MVP da Unidade 1 em C e a posterior integração com Haskell (via FFI) e persistência em arquivos na Unidade 2:


## Plano inicial:

1. Definição da struct do Jogador (jogo.h)
* Crie o arquivo de cabeçalho com a estrutura que guardará o estado do jogo (ex.: nome, pontuacao, vida ou nivel). Fazer isso primeiro é essencial para que todas as outras funções saibam onde ler e alterar os dados.

2. Menu Principal Navegável (menu.c / menu.h)
* Implemente a função que exibe as opções no terminal (1 - Jogar, 2 - Instruções, 3 - Sair) usando switch-case. Já inclua uma validação simples para caso o usuário digite um número inválido ou uma letra.

3. O Game Loop Central (main.c)
* Crie o laço principal (while) que mantém o jogo executando até que a opção "Sair" seja escolhida. Ele vai controlar a navegação: Menu $\rightarrow$ Tela do Jogo $\rightarrow$ Atualização de Score $\rightarrow$ Retorno ao Menu.

4. Stub (Função Temporária) da 1ª Missão (missao1.c)
* Não tente criar a missão completa ainda. Crie apenas uma função "vazia" com um printf("Sua missão começa aqui!"); e um incremento fictício na struct de pontuação.


## Entregas para a Unidade 1:  

>Para fechar o MVP da Unidade 1 da disciplina de PIF, o foco do código em C é entregar o núcleo executável funcional, modularizado e compilando 100% sem erros. O status de cada arquivo ao final da Unidade 1 deve ser:

1.include/jogo.h (Finalizado): Contém a definição da struct do jogador (vida, pontuação, inventário e contador de ações) e do estado do companion/jogo.  

2.src/main.c (Finalizado): Contém o Game Loop central (while) que gerencia os estados de navegação (Menu $\rightarrow$ Bloco 1 $\rightarrow$ Atualização do Estado $\rightarrow$ Retorno ao Menu/Sair).  

3.src/menu.c e include/menu.h (Finalizado): Implementa a interface do menu principal (1 - Jogar, 2 - Instruções, 3 - Sair) e o tratamento/validação para entradas inválidas (ex.: digitação de letras ou opções indisponíveis).  

4.src/missao1.c e include/missao1.h (Finalizado): Contém a 1ª Missão Funcional completa, que corresponde ao Bloco 1 (as duas salas com quizzes conceituais, tutorial da janela de contexto e atribuição de bônus inicial).  

5.src/missao2.c e src/missao3.c (Stubs / Em andamento): Devem conter apenas funções temporárias (stubs) com um printf explicativo de "Em breve" e um retorno válido, apenas para permitir que o projeto compile perfeitamente e teste a navegação até o Bloco 1.  


## Sugestão de modularização do jogo completo: 

```
utopia-game/
├── Makefile                # Script de compilação automatizada (GCC e GHC)
├── README.md               # Documentação inicial do projeto e guia de execução
├── docs/                   # Artefatos das disciplinas (PMC, Lean Inception, IHC, LMC, FDS)
├── data/                   # Arquivos de texto (.txt) para persistência e dados de perguntas
│   └── save.txt            # Arquivo de salvar/carregar progresso (Onda 2)
├── include/                # Cabeçalhos (.h) com definições de tipos e protótipos
│   ├── jogo.h              # Structs globais do estado do Jogador, Companion e Jogo
│   ├── menu.h              # Protótipos das telas de menu e validações
│   ├── missao1.h           # Protótipos do Bloco 1 (Quizzes e Tutorial)
│   ├── missao2.h           # Protótipos do Bloco 2 (Puzzles e Janela de Contexto)
│   └── missao3.h           # Protótipos do Bloco 3 (Batalha Boss/Companion)
├── src/                    # Implementação do código-fonte em C (.c)
│   ├── main.c              # Game Loop central e controle do estado de navegação
│   ├── menu.c              # Implementação do menu navegável e tratadores de input
│   ├── missao1.c           # Lógica dos quizzes conceituais da 1ª missão (MVP)
│   ├── missao2.c           # Lógica das 5 salas de exploração e cálculo de ações
│   └── missao3.c           # Mecânicas de combate e confrontação final
└── hs/                     # Módulos Haskell para regras puras (Onda 2 via FFI)
    └── RegrasPuras.hs      # Validações puras de pontuação, regras e invariantes
```


### Justificativas:

1.include/jogo.h (Gerenciamento de Estado): Contém a struct Jogador (vidas, score, inventário de itens, contador de ações/tokens) e o estado do Companion (nível de agressividade). Ser o primeiro arquivo definido impede dependências circulares e garante que todos os módulos acessem a mesma estrutura[cite: 1].  

2.src/main.c (Game Loop Central): Concentra a máquina de estados e o laço while responsável por gerenciar a transição entre o Menu, os Blocos de Missão e a finalização.

3.Modularização por Missão (missao1.c, missao2.c, missao3.c): Isola a complexidade de cada bloco narrativo. No MVP da Unidade 1, o arquivo missao1.c conterá a implementação funcional dos quizzes, enquanto missao2.c e missao3.c atuarão temporariamente como stubs.  

4.Isolamento da FFI (hs/): Mantém o código funcional em Haskell isolado da linguagem C base, simplificando os comandos do Makefile para vincular a biblioteca gerada pelo GHC durante a Unidade 2.

5.Persistência Separada (data/): Prepara o ambiente para gravação e leitura de progresso em .txt sem misturar arquivos de dados com código-fonte[cite: 2].
