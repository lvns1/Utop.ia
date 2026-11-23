# Como contribuir com o UtopIA

Obrigado por ajudar! Este guia mostra o mínimo para preparar o ambiente, compilar o jogo e enviar sua contribuição.

## 1. Ambiente

Você precisa do **GCC** (compilador C) e da **Raylib 5.5**.

> **Importante:** a Raylib precisa ficar na **pasta raiz do sistema**, nos caminhos abaixo. O build do VS Code (`.vscode/tasks.json`) e os comandos da seção 2 procuram a Raylib nesses lugares. Se ela estiver em outra pasta, o jogo não compila.

| Sistema | Onde a Raylib fica |
|---|---|
| Windows | `C:\raylib\` (Raylib em `C:\raylib\raylib`, compilador em `C:\raylib\w64devkit`) |
| Linux | `/usr/local/` (`/usr/local/include/raylib.h` e `/usr/local/lib/libraylib.a`) |
| macOS | `/usr/local/` (Intel) ou `/opt/homebrew/` (Apple Silicon), instalada pelo Homebrew |

### Windows

1. Baixe o instalador **raylib 5.5 Windows Installer (MinGW)** em [raysan5.itch.io/raylib](https://raysan5.itch.io/raylib) ou na página de [releases da Raylib](https://github.com/raysan5/raylib/releases/tag/5.5). Ele já traz o GCC (`w64devkit`).
2. Execute o instalador e **mantenha a pasta padrão `C:\raylib`**. Não instale em `Arquivos de Programas` nem dentro da pasta do projeto.
3. Adicione `C:\raylib\w64devkit\bin` ao `PATH` do Windows: *Configurações → Sistema → Sobre → Configurações avançadas do sistema → Variáveis de Ambiente → Path → Novo*.
4. Feche e abra o terminal e confira:
   ```powershell
   gcc --version
   Test-Path C:\raylib\raylib\src\raylib.h      # deve mostrar True
   ```

> Já usa o **MSYS2**? Também funciona: no terminal **MSYS2 UCRT64**, rode `pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-raylib` e adicione `C:\msys64\ucrt64\bin` ao `PATH`.

### Linux (Ubuntu/Debian)

1. Instale o GCC e as dependências da Raylib:
   ```bash
   sudo apt update
   sudo apt install build-essential git libasound2-dev libx11-dev libxrandr-dev libxi-dev \
                    libgl1-mesa-dev libglu1-mesa-dev libxcursor-dev libxinerama-dev libwayland-dev libxkbcommon-dev
   ```
2. Baixe e compile a Raylib 5.5 fora da pasta do projeto (por exemplo, em `~`):
   ```bash
   cd ~
   git clone --depth 1 --branch 5.5 https://github.com/raysan5/raylib.git
   cd raylib/src
   make PLATFORM=PLATFORM_DESKTOP
   ```
3. Instale na raiz do sistema (`/usr/local`):
   ```bash
   sudo make install
   ```
4. Confira:
   ```bash
   ls /usr/local/include/raylib.h /usr/local/lib/libraylib.a
   ```

> Evite o pacote `libraylib-dev` do apt: em muitas versões do Ubuntu ele é antigo (4.x).

### macOS

1. Instale as ferramentas de linha de comando da Apple (traz o compilador):
   ```bash
   xcode-select --install
   ```
2. Instale o [Homebrew](https://brew.sh/), se ainda não tiver, e depois a Raylib e o `pkg-config`:
   ```bash
   brew install raylib pkg-config
   ```
   O Homebrew instala na raiz do sistema: `/opt/homebrew` (Apple Silicon) ou `/usr/local` (Intel).
3. Confira:
   ```bash
   pkg-config --modversion raylib     # deve mostrar 5.5
   ```

## 2. Compilação e execução

Rode os comandos na **pasta raiz do projeto**. O jogo procura `resources/` e `assets/` a partir dela.

**Windows (PowerShell)**
```powershell
gcc main.c (Get-ChildItem src -Recurse -Filter *.c | % FullName) -o Utopia -I C:/raylib/raylib/src -L C:/raylib/raylib/src -lraylib -lopengl32 -lgdi32 -lwinmm
.\Utopia.exe
```
Pelo VS Code: `Ctrl + Shift + B`.

**Linux**
```bash
gcc main.c src/*/*.c -o Utopia -std=c99 -I/usr/local/include -L/usr/local/lib -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
./Utopia
```

**macOS**
```bash
gcc main.c src/*/*.c -o Utopia -std=c99 $(pkg-config --cflags --libs raylib)
./Utopia
```

- Todo `.c` dentro de `src/` entra na compilação. Um arquivo novo não exige mudar o comando.
- O **menu dev** (F1) liga com `#define DEV_MENU 1` no `main.c`. Deixe em `0` antes de integrar na `main`.

## 3. Fluxo de trabalho

1. Atualize a `develop` e crie sua branch a partir dela:
   ```bash
   git checkout develop
   git pull origin develop
   git checkout -b feat/us14-puzzle-sala-3
   ```
   Use o prefixo `feat/` para funcionalidades, `fix/` para correções e `docs/` para documentação.
2. Faça seus commits (veja o padrão abaixo) e envie a branch: `git push origin <sua-branch>`.
3. No GitHub, abra um **Pull Request para a `develop`**. Cite a US ou a issue (`Closes #12`) e diga como testar.
4. Pelo menos uma pessoa revisa e testa. Só depois o PR é integrado.
5. Quando a `develop` estiver estável e testada, ela é integrada na **`main`** por um PR `develop → main`.

> Nunca faça commit direto na `main` nem use `git push --force` em branches compartilhadas.

## 4. Padrão de commits

```
<tipo>: <US ou issue> <descrição curta>
```

Sempre cite a US ou a issue.

| Tipo | Quando usar | Exemplo |
|---|---|---|
| `feat` | funcionalidade nova | `feat: US14 puzzle da sala 3` |
| `fix` | correção de bug | `fix: #23 Thomas aparece com a arma antes de pegá-la` |
| `refactor` | muda o código sem mudar o comportamento | `refactor: #18 separa o main.c em módulos` |
| `docs` | documentação | `docs: #30 adiciona o CONTRIBUTING.md` |
| `style` | formatação, sem mudar a lógica | `style: #31 padroniza a indentação do corredor.c` |
| `chore` | build, configuração, arquivos do repositório | `chore: #32 ignora o utopia_teste.csv` |

## 5. Issues

Antes de abrir uma issue, veja se já não existe uma igual. Use as labels padrão do GitHub:

| Label | Para quê |
|---|---|
| `bug` | algo não funciona como deveria |
| `enhancement` | sugestão de melhoria ou funcionalidade nova |
| `documentation` | README, CONTRIBUTING ou comentários |
| `question` | dúvida sobre o projeto |
| `good first issue` | tarefa boa para quem está começando |
| `help wanted` | precisa de ajuda extra |
| `duplicate` / `invalid` / `wontfix` | issue repetida, inválida ou que não será feita |

**Reportando um bug:**
- **O que aconteceu** e **o que era esperado**
- **Como reproduzir**: passo a passo (sala, monitor, chefe…)
- **Sistema** (Windows/Linux/macOS) e, se possível, um print ou vídeo

**Sugerindo uma melhoria:**
- Escreva como história de usuário: **Como** jogador, **quero** …, **para** …
- Liste os **critérios de aceitação** em checklist (`- [ ]`)
