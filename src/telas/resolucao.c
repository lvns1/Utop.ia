#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <time.h>
#include "../../include/core/fonte.h"
#include "../../include/core/mapas.h"
#include "../../include/telas/resolucao.h"
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
#include "../../include/telas/pause.h"
#include "../../include/telas/ranking.h"


static const char *NOMES_MODO_EXIBICAO[NUM_MODOS_EXIBICAO] = { "JANELA", "TELA CHEIA" };

// Tamanhos 4:3 (a proporção do jogo), do maior para o menor.
// Na janela é o tamanho da janela; na tela cheia é o tamanho da imagem do jogo
// no meio da tela, com faixas pretas em volta.
// A tela cheia não troca a resolução do monitor: nos testes o Windows desfazia
// essa troca sozinho depois de meio segundo, então o monitor fica sempre na nativa.
static const Resolucao RESOLUCOES_4X3[] = {
    { 1600, 1200 },
    { 1280,  960 },
    { 1200,  900 },
    { 1024,  768 },
    {  800,  600 },
};

// Monitor onde o jogo abriu e o tamanho dele
static int monitorJogo = 0;
static int larguraMonitor = LARGURA_TELA;
static int alturaMonitor = ALTURA_TELA;

// Tamanho da imagem do jogo na tela cheia (0 x 0 = o maior que couber)
static Resolucao areaTelaCheia = { 0, 0 };
static bool emTelaCheia = false;

void GuardarMonitor(void)
{
    monitorJogo = GetCurrentMonitor();
    larguraMonitor = GetMonitorWidth(monitorJogo);
    alturaMonitor = GetMonitorHeight(monitorJogo);
}

static bool MesmaResolucao(Resolucao a, Resolucao b)
{
    return a.largura == b.largura && a.altura == b.altura;
}

// Maior imagem 4:3 que cabe no monitor (ex.: 1440 x 1080 num monitor 1920 x 1080)
static Resolucao MaiorQueCabe(void)
{
    float escala = fminf((float)larguraMonitor / LARGURA_TELA, (float)alturaMonitor / ALTURA_TELA);

    return (Resolucao){ (int)(LARGURA_TELA * escala), (int)(ALTURA_TELA * escala) };
}

// Monta a lista de resoluções de um modo, da maior para a menor.
// Só entram as que cabem no monitor; o 800 x 600 sempre entra.
static int ListarResolucoes(ModoExibicao modo, Resolucao lista[MAX_RESOLUCOES])
{
    int n = 0;
    Resolucao maxima = MaiorQueCabe();

    // Na tela cheia a primeira opção é ocupar a tela toda
    if (modo == EXIBICAO_TELA_CHEIA)
        lista[n++] = (Resolucao){ 0, 0 };

    for (int i = 0; i < NUM_RESOLUCOES_4X3 && n < MAX_RESOLUCOES; i++) {

        Resolucao r = RESOLUCOES_4X3[i];
        bool cabe = (r.largura <= larguraMonitor && r.altura <= alturaMonitor);

        // Na tela cheia, igual ou maior que a máxima já está coberto pela primeira opção
        if (modo == EXIBICAO_TELA_CHEIA)
            cabe = (r.largura < maxima.largura);

        if (cabe || i == NUM_RESOLUCOES_4X3 - 1)
            lista[n++] = r;
    }

    return n;
}

// Posição da resolução na lista do modo (-1 = não está na lista)
static int IndiceResolucao(ModoExibicao modo, Resolucao r)
{
    Resolucao lista[MAX_RESOLUCOES];
    int n = ListarResolucoes(modo, lista);

    for (int i = 0; i < n; i++)
        if (MesmaResolucao(lista[i], r))
            return i;

    return -1;
}

static const char *NomeResolucao(Resolucao r)
{
    if (r.largura == 0) {
        Resolucao maxima = MaiorQueCabe();
        return TextFormat("%d x %d (MÁXIMA)", maxima.largura, maxima.altura);
    }

    return TextFormat("%d x %d", r.largura, r.altura);
}

// A tela cheia é uma janela comum, sem borda, cobrindo o monitor inteiro.
// (O ToggleBorderlessWindowed do raylib prende a janela ao monitor e ela fica
// sempre por cima das outras: o ALT+TAB troca o foco mas o jogo não sai da tela.)
void AplicarExibicao(ConfigExibicao c)
{
    Vector2 origem = GetMonitorPosition(monitorJogo);

    emTelaCheia = (c.modo == EXIBICAO_TELA_CHEIA);

    if (emTelaCheia) {

        // A imagem do jogo fica no tamanho escolhido, no meio da tela
        areaTelaCheia = c.resolucao;

        SetWindowState(FLAG_WINDOW_UNDECORATED);
        SetWindowSize(larguraMonitor, alturaMonitor);
        SetWindowPosition((int)origem.x, (int)origem.y);
        return;
    }

    areaTelaCheia = (Resolucao){ 0, 0 };
    ClearWindowState(FLAG_WINDOW_UNDECORATED);

    // Janela: muda o tamanho e centraliza no monitor
    int w = c.resolucao.largura;
    int h = c.resolucao.altura;

    SetWindowSize(w, h);
    SetWindowPosition((int)origem.x + (larguraMonitor - w) / 2,
                      (int)origem.y + (alturaMonitor - h) / 2);
}

// Arquivo de configuração: "modo largura altura" (tela cheia máxima = "1 0 0")
ConfigExibicao LerConfigExibicao(void)
{
    ConfigExibicao padrao = { EXIBICAO_JANELA, { LARGURA_TELA, ALTURA_TELA } };
    ConfigExibicao c;
    int modo = 0;

    FILE *f = fopen(ARQUIVO_CONFIG, "r");

    if (f == NULL)
        return padrao;

    bool ok = (fscanf(f, "%d %d %d", &modo, &c.resolucao.largura, &c.resolucao.altura) == 3);
    fclose(f);

    if (!ok || modo < 0 || modo >= NUM_MODOS_EXIBICAO)
        return padrao;

    c.modo = (ModoExibicao)modo;

    // Resolução que não cabe neste monitor (trocou de monitor, arquivo antigo...)
    if (IndiceResolucao(c.modo, c.resolucao) < 0)
        return padrao;

    return c;
}

void SalvarConfigExibicao(ConfigExibicao c)
{
    FILE *f = fopen(ARQUIVO_CONFIG, "w");

    if (f == NULL)
        return;

    fprintf(f, "%d %d %d\n", (int)c.modo, c.resolucao.largura, c.resolucao.altura);
    fclose(f);
}

// Quanto a imagem de 800x600 é esticada e onde ela começa dentro da janela
static float EscalaJanela(Vector2 *deslocamento)
{
    float escala = fminf((float)GetScreenWidth() / LARGURA_TELA,
                         (float)GetScreenHeight() / ALTURA_TELA);

    // Tela cheia com tamanho escolhido: não passa dele
    if (emTelaCheia && areaTelaCheia.largura > 0)
        escala = fminf(escala, (float)areaTelaCheia.largura / LARGURA_TELA);

    deslocamento->x = floorf((GetScreenWidth()  - LARGURA_TELA * escala) / 2.0f);
    deslocamento->y = floorf((GetScreenHeight() - ALTURA_TELA  * escala) / 2.0f);

    return escala;
}

// Faz o mouse continuar falando em coordenadas de 800x600,
// qualquer que seja o tamanho da janela
void AjustarMouseAJanela(void)
{
    Vector2 desloc;
    float escala = EscalaJanela(&desloc);

    SetMouseOffset((int)-desloc.x, (int)-desloc.y);
    SetMouseScale(1.0f / escala, 1.0f / escala);
}

// Filtro "sharp bilinear" para esticar o jogo sem borrar.
// Com escala quebrada (ex.: 1.8x em tela cheia 1920x1080), o bilinear puro
// deixa tudo embaçado e o point deixa pixels de larguras diferentes.
// Aqui cada pixel do jogo vira um bloco sólido e só a borda de 1 pixel
// entre um bloco e outro é suavizada.
#if defined(PLATFORM_WEB)
static const char *SHADER_NITIDEZ =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec2 fragTexCoord;\n"
    "varying vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform vec2 tamanhoTextura;\n"
    "uniform float escala;\n"
    "void main() {\n"
    "    vec2 texel = fragTexCoord * tamanhoTextura;\n"
    "    vec2 centro = fract(texel) - 0.5;\n"
    "    float regiao = 0.5 - 0.5 / escala;\n"
    "    vec2 f = (centro - clamp(centro, -regiao, regiao)) * escala + 0.5;\n"
    "    gl_FragColor = texture2D(texture0, (floor(texel) + f) / tamanhoTextura) * colDiffuse * fragColor;\n"
    "}\n";
#else
static const char *SHADER_NITIDEZ =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform vec2 tamanhoTextura;\n"
    "uniform float escala;\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    vec2 texel = fragTexCoord * tamanhoTextura;\n"
    "    vec2 centro = fract(texel) - 0.5;\n"
    "    float regiao = 0.5 - 0.5 / escala;\n"
    "    vec2 f = (centro - clamp(centro, -regiao, regiao)) * escala + 0.5;\n"
    "    finalColor = texture(texture0, (floor(texel) + f) / tamanhoTextura) * colDiffuse * fragColor;\n"
    "}\n";
#endif

FiltroNitidez CarregarFiltroNitidez(void)
{
    FiltroNitidez f;

    f.shader = LoadShaderFromMemory(NULL, SHADER_NITIDEZ);
    f.locEscala = GetShaderLocation(f.shader, "escala");
    f.locTamanho = GetShaderLocation(f.shader, "tamanhoTextura");

    return f;
}

// Desenha a textura do jogo na janela (chamar entre BeginDrawing e EndDrawing)
void DesenharNaJanela(RenderTexture2D alvo, FiltroNitidez filtro)
{
    Vector2 desloc;
    float escala = EscalaJanela(&desloc);
    bool usarShader = (filtro.locEscala >= 0 && filtro.locTamanho >= 0);

    if (usarShader) {

        // O shader precisa do bilinear ligado para suavizar só as bordas
        float escalaShader = fmaxf(escala, 1.0f);
        float tamanho[2] = { (float)alvo.texture.width, (float)alvo.texture.height };

        SetTextureFilter(alvo.texture, TEXTURE_FILTER_BILINEAR);
        SetShaderValue(filtro.shader, filtro.locEscala, &escalaShader, SHADER_UNIFORM_FLOAT);
        SetShaderValue(filtro.shader, filtro.locTamanho, tamanho, SHADER_UNIFORM_VEC2);

    } else {

        SetTextureFilter(alvo.texture, TEXTURE_FILTER_POINT);
    }

    ClearBackground(BLACK);

    if (usarShader)
        BeginShaderMode(filtro.shader);

    DrawTexturePro(
        alvo.texture,
        (Rectangle){ 0, 0, (float)alvo.texture.width, -(float)alvo.texture.height },
        (Rectangle){ desloc.x, desloc.y, LARGURA_TELA * escala, ALTURA_TELA * escala },
        (Vector2){ 0, 0 },
        0.0f,
        WHITE
    );

    if (usarShader)
        EndShaderMode();
}

static Rectangle CaixaOpcoes(int campo)
{
    return (Rectangle){ 220, 206 + campo * 84.0f, 360, 36 };
}

static Rectangle BotaoVoltarOpcoes(void)
{
    return (Rectangle){ 300, 388, 200, 32 };
}

// Itens da lista aberta, logo abaixo da caixa
static Rectangle ItemListaOpcoes(int campo, int i)
{
    Rectangle caixa = CaixaOpcoes(campo);

    return (Rectangle){ caixa.x, caixa.y + caixa.height + 2 + i * (float)ALTURA_ITEM_LISTA, caixa.width, ALTURA_ITEM_LISTA };
}

static void AbrirListaOpcoes(MenuOpcoes *m, int campo, ConfigExibicao cfg)
{
    m->foco = campo;
    m->aberta = campo;
    m->marcado = (campo == CAMPO_MODO) ? (int)cfg.modo : IndiceResolucao(cfg.modo, cfg.resolucao);

    if (m->marcado < 0)
        m->marcado = 0;
}

// Lê teclado e mouse na tela de opções.
// Devolve true quando o jogador escolheu algo diferente (cfg já volta atualizada).
//   W / S / SETAS          : troca o campo, ou o item da lista aberta
//   ENTER / E / ESPAÇO     : abre a lista, ou escolhe o item
//   mouse                  : clica na caixa para abrir, no item para escolher, fora para fechar
// (ESC e BACKSPACE fecham a lista; ficam no loop principal)
bool AtualizarOpcoes(MenuOpcoes *m, ConfigExibicao *cfg)
{
    Vector2 mouse = GetMousePosition();
    bool mouseMexeu = (Vector2Length(GetMouseDelta()) > 0.0f);
    bool clicou = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool confirmou = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE);
    bool cima = IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP);
    bool baixo = IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN);

    // Nenhuma lista aberta: escolhe qual abrir
    if (m->aberta < 0) {

        if (cima)  m->foco = (m->foco + NUM_CAMPOS_OPCOES) % (NUM_CAMPOS_OPCOES + 1);
        if (baixo) m->foco = (m->foco + 1) % (NUM_CAMPOS_OPCOES + 1);

        if (MouseEm(BotaoVoltarOpcoes())) {

            if (mouseMexeu)
                m->foco = CAMPO_VOLTAR;

            if (clicou) {
                m->sair = true;
                return false;
            }
        }

        for (int c = 0; c < NUM_CAMPOS_OPCOES; c++) {

            if (!CheckCollisionPointRec(mouse, CaixaOpcoes(c)))
                continue;

            if (mouseMexeu)
                m->foco = c;

            if (clicou) {
                AbrirListaOpcoes(m, c, *cfg);
                return false;
            }
        }

        if (confirmou) {
            if (m->foco == CAMPO_VOLTAR) m->sair = true;
            else                         AbrirListaOpcoes(m, m->foco, *cfg);
        }

        return false;
    }

    // Lista aberta: escolhe um item
    Resolucao lista[MAX_RESOLUCOES];
    int numResolucoes = ListarResolucoes(cfg->modo, lista);
    int numItens = (m->aberta == CAMPO_MODO) ? NUM_MODOS_EXIBICAO : numResolucoes;
    int escolhido = -1;

    if (cima)
        m->marcado = (m->marcado + numItens - 1) % numItens;

    if (baixo)
        m->marcado = (m->marcado + 1) % numItens;

    for (int i = 0; i < numItens; i++) {

        if (!CheckCollisionPointRec(mouse, ItemListaOpcoes(m->aberta, i)))
            continue;

        if (mouseMexeu)
            m->marcado = i;

        if (clicou)
            escolhido = i;
    }

    if (confirmou)
        escolhido = m->marcado;

    // Clique fora dos itens (ou na própria caixa) só fecha a lista
    if (escolhido < 0) {

        if (clicou)
            m->aberta = -1;

        return false;
    }

    ConfigExibicao nova = *cfg;

    if (m->aberta == CAMPO_MODO) {

        nova.modo = (ModoExibicao)escolhido;

        // Trocou de modo: começa pela primeira resolução da lista nova
        // (a maior janela, ou a nativa na tela cheia)
        if (nova.modo != cfg->modo) {
            ListarResolucoes(nova.modo, lista);
            nova.resolucao = lista[0];
        }

    } else {

        nova.resolucao = lista[escolhido];
    }

    m->aberta = -1;

    if (nova.modo == cfg->modo && MesmaResolucao(nova.resolucao, cfg->resolucao))
        return false;

    *cfg = nova;
    return true;
}

// Setinha da lista suspensa: para baixo fechada, para cima aberta
static void DesenharSetaLista(Rectangle caixa, bool aberta, Color cor)
{
    float cx = caixa.x + caixa.width - 20;
    float cy = caixa.y + caixa.height / 2;

    if (aberta)
        DrawTriangle((Vector2){ cx, cy - 4 }, (Vector2){ cx - 6, cy + 3 }, (Vector2){ cx + 6, cy + 3 }, cor);
    else
        DrawTriangle((Vector2){ cx - 6, cy - 3 }, (Vector2){ cx, cy + 4 }, (Vector2){ cx + 6, cy - 3 }, cor);
}

void DesenharOpcoes(const MenuOpcoes *m, ConfigExibicao cfg, Color fundo)
{
    Color painelCor = (Color){ 18, 15, 30, 255 };
    Color caixaCor  = (Color){ 10, 8, 22, 255 };

    ClearBackground(fundo);

    Rectangle painel = { 180, 90, 440, 400 };

    DrawRectangleRec(painel, painelCor);
    DrawRectangleLinesEx(painel, 4, PURPLE);
    DrawRectangleLinesEx((Rectangle){ painel.x + 8, painel.y + 8, painel.width - 16, painel.height - 16 }, 1, Fade(SKYBLUE, 0.35f));

    const char *titulo = "OPÇÕES";
    DrawText(titulo, LARGURA_TELA / 2 - MeasureText(titulo, 40) / 2, 115, 40, PURPLE);

    // Caixas fechadas, com o valor atual
    const char *rotulos[NUM_CAMPOS_OPCOES] = { "MODO DE EXIBIÇÃO", "RESOLUÇÃO" };
    const char *valores[NUM_CAMPOS_OPCOES] = { NOMES_MODO_EXIBICAO[cfg.modo], NomeResolucao(cfg.resolucao) };

    for (int c = 0; c < NUM_CAMPOS_OPCOES; c++) {

        Rectangle caixa = CaixaOpcoes(c);
        bool focado = (c == m->foco);

        DrawText(rotulos[c], (int)caixa.x, (int)caixa.y - 18, 12, focado ? SKYBLUE : GRAY);

        DrawRectangleRec(caixa, caixaCor);
        DrawRectangleLinesEx(caixa, 2, focado ? PURPLE : Fade(PURPLE, 0.4f));

        if (focado && m->aberta < 0)
            DesenharCursorLuci(caixa.x - 18, caixa.y + caixa.height / 2);

        DrawText(valores[c], (int)caixa.x + 14, (int)(caixa.y + caixa.height / 2) - 10, 20, focado ? WHITE : LIGHTGRAY);
        DesenharSetaLista(caixa, m->aberta == c, focado ? SKYBLUE : GRAY);
    }

    // VOLTAR AO MENU (o mesmo que ESC)
    {
        Rectangle r = BotaoVoltarOpcoes();
        bool marcado = (m->foco == CAMPO_VOLTAR && m->aberta < 0);

        DesenharBotaoMenu(r, "VOLTAR AO MENU", 14, SKYBLUE, marcado);

        if (marcado)
            DesenharCursorLuci(r.x - 18, r.y + r.height / 2);
    }

    const char *dica1 = (m->aberta < 0) ? "ENTER / CLIQUE - abrir a lista" : "ENTER / CLIQUE - escolher";
    const char *dica2 = (m->aberta < 0) ? "ESC - voltar ao menu" : "ESC - fechar a lista";

    DrawText(dica1, LARGURA_TELA / 2 - MeasureText(dica1, 10) / 2, 440, 10, Fade(SKYBLUE, 0.6f));
    DrawText(dica2, LARGURA_TELA / 2 - MeasureText(dica2, 10) / 2, 456, 10, GRAY);

    // Lista aberta por cima de tudo
    if (m->aberta < 0)
        return;

    Resolucao lista[MAX_RESOLUCOES];
    int numItens = (m->aberta == CAMPO_MODO) ? NUM_MODOS_EXIBICAO : ListarResolucoes(cfg.modo, lista);

    Rectangle area = ItemListaOpcoes(m->aberta, 0);
    area.height = numItens * (float)ALTURA_ITEM_LISTA;

    DrawRectangleRec((Rectangle){ area.x - 2, area.y - 2, area.width + 4, area.height + 4 }, caixaCor);
    DrawRectangleLinesEx((Rectangle){ area.x - 2, area.y - 2, area.width + 4, area.height + 4 }, 2, PURPLE);

    for (int i = 0; i < numItens; i++) {

        Rectangle r = ItemListaOpcoes(m->aberta, i);
        bool marcado = (i == m->marcado);
        bool atual;
        const char *nome;

        if (m->aberta == CAMPO_MODO) {
            nome = NOMES_MODO_EXIBICAO[i];
            atual = (i == (int)cfg.modo);
        } else {
            nome = NomeResolucao(lista[i]);
            atual = MesmaResolucao(lista[i], cfg.resolucao);
        }

        if (marcado)
            DrawRectangleRec(r, Fade(PURPLE, 0.35f));

        DrawText(nome, (int)r.x + 14, (int)(r.y + r.height / 2) - 8, 16, marcado ? SKYBLUE : LIGHTGRAY);

        if (atual)
            DrawText("ATUAL", (int)(r.x + r.width) - MeasureText("ATUAL", 10) - 12, (int)(r.y + r.height / 2) - 5, 10, GOLD);
    }
}
