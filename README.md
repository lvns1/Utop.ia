# utoplA — RPG em Terminal sobre Ética em IA

> Jogo educativo de console (terminal), com núcleo implementado em C e módulos de regras puras em Haskell, que ensina conceitos básicos de Inteligência Artificial e provoca reflexão sobre seu uso ético, crítico e responsável.

---

## 🎮 Sobre o Jogo

Um jovem é sugado para dentro do próprio computador enquanto usava IAs de forma irresponsável. Para conseguir voltar para a realidade, ele precisa explorar o sistema e aprender a lidar com as consequências dos seus próprios maus hábitos digitais.

```text
=======================================================
[?] "Acesso negado. Seu perfil de dados nao atende aos requisitos."

1 - Investigar o terminal de dados
2 - Ir para o Quarto de Memoria
3 - Conversar com o Mascotinho
=======================================================
```

---

## 🕹️ Como Funciona a Gameplay

* **Exploração por Menus:** O jogador escolhe os caminhos digitando números no teclado (ex: `1 - Cozinha`, `2 - Sala`).
* **Quizzes e Decisões:** Perguntas e escolhas éticas sobre *deepfakes*, privacidade e uso correto de IA.
* **Mini-Jogo de Viés:** O jogador junta dados para treinar uma mini-IA e observa como dados desbalanceados geram injustiças.
* **Inimigos e Bosses:** Para derrotar os chefes, é preciso encontrar itens ou *prompts* corretos espalhados pelas salas.
* **Companion (Mascotinho):** Uma mini-IA que aprende com as suas atitudes. Escolhas ruins do jogador podem transformá-lo no chefe final.

---

## 🗺️ Mapas Temáticos

1. **Lobby — Interlúdio Entre os Mapas:** Mini-Mapa central para entrar nos outros Mundos ao decorrer do progresso.
2. **Mapa 1 — Dependência:** Focado no uso excessivo e sem senso crítico.
3. **Mapa 2 — Alucinação:** Focado nos erros e nas informações falsas geradas por IAs.

---

## 💻 Tecnologias Usadas

* **C:** Controla os menus, a navegação pelo mapa, os textos e salva o progresso do jogador em arquivo.
* **Haskell:** Processa a matemática das regras, o cálculo de pontos e a lógica do mini-jogo de viés.

---

## 🚀 Como Rodar o Projeto

```bash
# Clone o repositório
git clone https://github.com/seu-usuario/utoplA.git

# Entre na pasta
cd utoplA

# Compile os módulos em Haskell
ghc -c logic/Rules.hs -o logic/Rules.o

# Compile o código C junto com o módulo Haskell
gcc -o utopla src/*.c logic/Rules.o

# Execute o jogo
./utopla
```

---

**Projeto Integrador — CESAR School (2º Período)**
