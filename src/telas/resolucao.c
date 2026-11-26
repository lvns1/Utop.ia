#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <time.h>
#include "../../include/core/fonte.h"
#include "../../include/core/mapas.h"
#include "../../include/telas/resolucao.h"
#include "../../include/core/preferencias.h"
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
static const char *NOMES_TAMANHO_TEXTO[2] = { "NORMAL", "GRANDE" };

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

    DefinirTextoGrande(c.textoGrande);
    DefinirPreferencias(c.pref);

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

// Arquivo de configuração, numa linha só:
//   modo largura altura texto  efeitosTela corrupcao luz numeros tremor  volGeral volMusica volEfeitos  camera
// (tela cheia máxima = "1 0 0 ..."; texto 1 = GRANDE; efeitos 1 = ligado; volumes de 0 a 10).
// Um arquivo antigo, só com os primeiros números, abre o que falta no padrão.
ConfigExibicao LerConfigExibicao(void)
{
    ConfigExibicao padrao = { EXIBICAO_JANELA, { LARGURA_TELA, ALTURA_TELA }, false, PREFERENCIAS_PADRAO };
    ConfigExibicao c = padrao;
    int modo = 0;
    int texto = 0;
    int ef[5] = { 1, 1, 1, 1, 1 };
    int vol[3] = { VOLUME_PASSOS, VOLUME_PASSOS, VOLUME_PASSOS };
    int camera = 1;

    FILE *f = fopen(ARQUIVO_CONFIG, "r");

    if (f == NULL)
        return padrao;

    int lidos = fscanf(f, "%d %d %d %d %d %d %d %d %d %d %d %d %d", &modo, &c.resolucao.largura, &c.resolucao.altura, &texto,
                       &ef[0], &ef[1], &ef[2], &ef[3], &ef[4], &vol[0], &vol[1], &vol[2], &camera);
    fclose(f);

    if (lidos < 3 || modo < 0 || modo >= NUM_MODOS_EXIBICAO)
        return padrao;

    c.modo = (ModoExibicao)modo;
    c.textoGrande = (lidos >= 4 && texto == 1);
    if (lidos >= 12) {
        c.pref.efeitosTela   = ef[0] != 0;
        c.pref.corrupcaoTela = ef[1] != 0;
        c.pref.luzSalas      = ef[2] != 0;
        c.pref.numerosDano   = ef[3] != 0;
        c.pref.tremorTela    = ef[4] != 0;
        c.pref.volumeGeral   = vol[0];
        c.pref.volumeMusica  = vol[1];
        c.pref.volumeEfeitos = vol[2];
        c.pref.cameraMira    = (lidos < 13) || camera != 0;
        c.pref = LimitarPreferencias(c.pref);
    }

    // Resolução que não cabe neste monitor (trocou de monitor, arquivo antigo...): o texto e as preferências continuam
    if (IndiceResolucao(c.modo, c.resolucao) < 0) {
        padrao.textoGrande = c.textoGrande;
        padrao.pref = c.pref;
        return padrao;
    }

    return c;
}

void SalvarConfigExibicao(ConfigExibicao c)
{
    FILE *f = fopen(ARQUIVO_CONFIG, "w");

    if (f == NULL)
        return;

    const Preferencias *p = &c.pref;
    fprintf(f, "%d %d %d %d %d %d %d %d %d %d %d %d %d\n", (int)c.modo, c.resolucao.largura, c.resolucao.altura, c.textoGrande ? 1 : 0,
            p->efeitosTela, p->corrupcaoTela, p->luzSalas, p->numerosDano, p->tremorTela,
            p->volumeGeral, p->volumeMusica, p->volumeEfeitos, p->cameraMira);
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

// Efeitos de tela: o mesmo "sharp bilinear" e, por cima, o que estiver ligado no config.h
// (TELA_VINHETA, TELA_LINHAS, TELA_BRILHO, TELA_CORRUPCAO, TELA_GRAO). Partes do shader:
//   Amostra   - o filtro de nitidez de sempre
//   corrupção - acima de 0,65 umas faixas escorregam para o lado por 1/8 s, de vez em quando;
//               os canais R e B se afastam do centro para as bordas (até 2,5 pixels do jogo)
//   brilho    - 12 amostras em volta: o que passa de 55% de claridade vaza um pouco
//   linhas    - escurece a borda de cada linha de pixels do jogo (some abaixo de 2x)
//   vinheta   - cantos até 38% mais escuros (um pouco mais com a corrupção)
//   sombras   - puxam para o magenta com a corrupção
//   grão      - ruído que muda a cada quadro
// O uniform "efeitos" (0 ou 1, opção EFEITOS DE TELA) desliga vinheta, brilho, linhas e grão sem
// recompilar; a opção CORRUPÇÃO NA TELA zera a corrupção que chega ao shader.
// Sem comentários dentro do texto do shader: alguns drivers recusam acento até em comentário.
#if defined(PLATFORM_WEB)
static const char *CABECALHO_TELA =
    "#version 100\n"
    "precision mediump float;\n"
    "#define TEXTURA texture2D\n"
    "#define SAIDA gl_FragColor\n"
    "varying vec2 fragTexCoord;\n"
    "varying vec4 fragColor;\n";
#else
static const char *CABECALHO_TELA =
    "#version 330\n"
    "#define TEXTURA texture\n"
    "#define SAIDA finalColor\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "out vec4 finalColor;\n";
#endif

static const char *SHADER_TELA =
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform vec2 tamanhoTextura;\n"
    "uniform float escala;\n"
    "uniform float corrupcao;\n"
    "uniform float tempo;\n"
    "uniform float efeitos;\n"
    "vec4 Amostra(vec2 uv)\n"
    "{\n"
    "    vec2 texel = uv * tamanhoTextura;\n"
    "    vec2 centro = fract(texel) - 0.5;\n"
    "    float regiao = 0.5 - 0.5 / escala;\n"
    "    vec2 f = (centro - clamp(centro, -regiao, regiao)) * escala + 0.5;\n"
    "    return TEXTURA(texture0, (floor(texel) + f) / tamanhoTextura);\n"
    "}\n"
    "float Ruido(vec2 p)\n"
    "{\n"
    "    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);\n"
    "}\n"
    "void main()\n"
    "{\n"
    "    vec2 uv = fragTexCoord;\n"
    "    vec2 px = 1.0 / tamanhoTextura;\n"
    "    vec2 doCentro = uv - 0.5;\n"
    "    float k = clamp(corrupcao, 0.0, 1.0);\n"
    "#if TELA_CORRUPCAO\n"
    "    if (k > 0.65) {\n"
    "        float janela = floor(tempo * 8.0);\n"
    "        if (Ruido(vec2(janela, 7.31)) > 0.94) {\n"
    "            float faixa = floor(uv.y * 45.0);\n"
    "            if (Ruido(vec2(faixa, janela)) > 0.75)\n"
    "                uv.x += (Ruido(vec2(faixa + 3.7, janela)) - 0.5) * px.x * 10.0 * (k - 0.65) / 0.35;\n"
    "        }\n"
    "    }\n"
    "#endif\n"
    "    vec4 base = Amostra(uv);\n"
    "    vec3 cor = base.rgb;\n"
    "#if TELA_CORRUPCAO\n"
    "    vec2 desvio = doCentro * px * (k * 2.5);\n"
    "    cor.r = Amostra(uv + desvio).r;\n"
    "    cor.b = Amostra(uv - desvio).b;\n"
    "#endif\n"
    "#if TELA_BRILHO\n"
    "    vec3 soma = vec3(0.0);\n"
    "    for (int i = 0; i < 12; i++) {\n"
    "        float a = float(i) * 0.5236;\n"
    "        float r = (mod(float(i), 2.0) < 0.5) ? 2.0 : 4.5;\n"
    "        soma += max(TEXTURA(texture0, uv + vec2(cos(a), sin(a)) * px * r).rgb - 0.55, 0.0);\n"
    "    }\n"
    "    cor += soma * (0.075 + 0.025 * k) * efeitos;\n"
    "#endif\n"
    "#if TELA_LINHAS\n"
    "    float s = abs(fract(uv.y * tamanhoTextura.y) - 0.5) * 2.0;\n"
    "    cor *= 1.0 - 0.22 * s * s * smoothstep(1.5, 2.5, escala) * efeitos;\n"
    "#endif\n"
    "#if TELA_VINHETA\n"
    "    float d = length(doCentro * vec2(1.0, 0.9));\n"
    "    cor *= mix(1.0, 0.62 - 0.12 * k, smoothstep(0.38, 0.78, d) * efeitos);\n"
    "#endif\n"
    "#if TELA_CORRUPCAO\n"
    "    float luz = dot(cor, vec3(0.299, 0.587, 0.114));\n"
    "    cor += vec3(0.05, 0.0, 0.06) * k * (1.0 - luz);\n"
    "#endif\n"
    "#if TELA_GRAO\n"
    "    cor += (Ruido(gl_FragCoord.xy + fract(tempo * 7.0) * 113.0) - 0.5) * (0.025 + 0.035 * k) * efeitos;\n"
    "#endif\n"
    "    SAIDA = vec4(cor, base.a) * colDiffuse * fragColor;\n"
    "}\n";

// Corrupção que a tela mostra agora (anda devagar até o alvo que o jogo pede)
static float corrupcaoAlvo = 0.0f;
static float corrupcaoTela = 0.0f;

void DefinirCorrupcaoTela(float alvo)
{
    corrupcaoAlvo = Clamp(alvo, 0.0f, 1.0f);
}

FiltroNitidez CarregarFiltroNitidez(void)
{
    FiltroNitidez f;

    f.locCorrupcao = -1;
    f.locTempo = -1;
    f.locEfeitos = -1;

#if TELA_EFEITOS
    static char fonte[8192];
    snprintf(fonte, sizeof(fonte), "%s#define TELA_VINHETA %d\n#define TELA_LINHAS %d\n#define TELA_BRILHO %d\n"
             "#define TELA_CORRUPCAO %d\n#define TELA_GRAO %d\n%s",
             CABECALHO_TELA, TELA_VINHETA, TELA_LINHAS, TELA_BRILHO, TELA_CORRUPCAO, TELA_GRAO, SHADER_TELA);

    f.shader = LoadShaderFromMemory(NULL, fonte);
    f.locEscala = GetShaderLocation(f.shader, "escala");
    f.locTamanho = GetShaderLocation(f.shader, "tamanhoTextura");
    if (f.locEscala >= 0 && f.locTamanho >= 0) {
        f.locCorrupcao = GetShaderLocation(f.shader, "corrupcao");
        f.locTempo = GetShaderLocation(f.shader, "tempo");
        f.locEfeitos = GetShaderLocation(f.shader, "efeitos");
        return f;
    }
    UnloadShader(f.shader);         // não compilou neste computador: fica só o filtro de nitidez
#endif

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

        // Efeitos de tela: a corrupção chega ao alvo em cerca de 1 s (nada muda de repente).
        // As opções EFEITOS DE TELA e CORRUPÇÃO NA TELA desligam cada parte.
        float passo = fminf(GetFrameTime(), 0.1f) * 1.2f;
        float tempo = (float)fmod(GetTime(), 1000.0);       // número pequeno: o ruído não perde precisão
        float alvoCorrupcao = Prefs()->corrupcaoTela ? corrupcaoAlvo : 0.0f;
        float efeitos = Prefs()->efeitosTela ? 1.0f : 0.0f;
        corrupcaoTela += Clamp(alvoCorrupcao - corrupcaoTela, -passo, passo);
        if (filtro.locCorrupcao >= 0) SetShaderValue(filtro.shader, filtro.locCorrupcao, &corrupcaoTela, SHADER_UNIFORM_FLOAT);
        if (filtro.locTempo >= 0)     SetShaderValue(filtro.shader, filtro.locTempo, &tempo, SHADER_UNIFORM_FLOAT);
        if (filtro.locEfeitos >= 0)   SetShaderValue(filtro.shader, filtro.locEfeitos, &efeitos, SHADER_UNIFORM_FLOAT);

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

// ----- Tela de opções: posições -----
static const Rectangle PAINEL_OPCOES = { 150, 60, 500, 470 };

static Rectangle AbaOpcoesRet(int aba)
{
    return (Rectangle){ 180 + aba * 150.0f, 124, 140, 30 };
}

// Aba VÍDEO: a caixa de cada lista suspensa
static Rectangle CaixaOpcoes(int campo)
{
    return (Rectangle){ 220, 200 + campo * 70.0f, 360, 36 };
}

// Aba EFEITOS: a linha inteira (nome + explicação + interruptor) e o interruptor
static Rectangle LinhaEfeito(int campo)
{
    return (Rectangle){ 188, 172 + campo * 42.0f, 424, 38 };
}

static Rectangle InterruptorEfeito(int campo)
{
    Rectangle l = LinhaEfeito(campo);

    return (Rectangle){ l.x + l.width - 132, l.y + 4, 124, 30 };
}

// Aba SONS: a barra de cada volume (VOLUME_PASSOS quadradinhos)
static Rectangle BarraVolume(int campo)
{
    return (Rectangle){ 196, 214 + campo * 70.0f, 330, 18 };
}

static Rectangle BotaoVoltarOpcoes(void)
{
    return (Rectangle){ 300, 430, 200, 32 };
}

static int NumCamposAba(int aba)
{
    if (aba == ABA_EFEITOS) return NUM_CAMPOS_EFEITOS;
    if (aba == ABA_SONS)    return NUM_CAMPOS_SONS;
    return NUM_CAMPOS_OPCOES;
}

static const char *NOMES_ABAS[NUM_ABAS_OPCOES] = { "VÍDEO", "EFEITOS", "SONS" };

static const char *ROTULOS_EFEITOS[NUM_CAMPOS_EFEITOS] = {
    "EFEITOS DE TELA", "CORRUPÇÃO NA TELA", "LUZ DAS SALAS", "NÚMEROS DE DANO", "TREMOR DA TELA", "CÂMERA NA MIRA"
};
static const char *DESCRICOES_EFEITOS[NUM_CAMPOS_EFEITOS] = {
    "vinheta, brilho, linhas e granulado",
    "cores separadas e falhas da corrupção",
    "penumbra, luzes e poeira no ar das salas",
    "o dano sobe dos inimigos atingidos",
    "a tela treme quando o Thomas leva dano",
    "a visão adianta um pouco para a mira"
};
static const char *ROTULOS_SONS[NUM_CAMPOS_SONS] = { "VOLUME GERAL", "MÚSICA", "EFEITOS SONOROS" };

// O valor de cada interruptor e de cada volume dentro das preferências
static bool *ValorEfeito(Preferencias *p, int campo)
{
    switch (campo) {
        case EF_TELA:      return &p->efeitosTela;
        case EF_CORRUPCAO: return &p->corrupcaoTela;
        case EF_LUZ:       return &p->luzSalas;
        case EF_NUMEROS:   return &p->numerosDano;
        case EF_TREMOR:    return &p->tremorTela;
        default:           return &p->cameraMira;
    }
}

static int *ValorVolume(Preferencias *p, int campo)
{
    if (campo == SOM_GERAL)  return &p->volumeGeral;
    if (campo == SOM_MUSICA) return &p->volumeMusica;
    return &p->volumeEfeitos;
}

// Quantos itens a lista do campo tem
static int ItensListaOpcoes(int campo, ModoExibicao modo)
{
    Resolucao lista[MAX_RESOLUCOES];

    if (campo == CAMPO_MODO)  return NUM_MODOS_EXIBICAO;
    if (campo == CAMPO_TEXTO) return 2;
    return ListarResolucoes(modo, lista);
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
    if (campo == CAMPO_MODO)       m->marcado = (int)cfg.modo;
    else if (campo == CAMPO_TEXTO) m->marcado = cfg.textoGrande ? 1 : 0;
    else                           m->marcado = IndiceResolucao(cfg.modo, cfg.resolucao);

    if (m->marcado < 0)
        m->marcado = 0;
}

// Aba VÍDEO com uma lista aberta: escolhe um item
static MudancaOpcoes AtualizarListaVideo(MenuOpcoes *m, ConfigExibicao *cfg, Vector2 mouse, bool mouseMexeu,
                                         bool clicou, bool confirmou, bool cima, bool baixo)
{
    Resolucao lista[MAX_RESOLUCOES];
    ListarResolucoes(cfg->modo, lista);
    int numItens = ItensListaOpcoes(m->aberta, cfg->modo);
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

        return OPCOES_NADA;
    }

    ConfigExibicao nova = *cfg;

    if (m->aberta == CAMPO_TEXTO) {

        m->aberta = -1;

        if (nova.textoGrande == (escolhido == 1))
            return OPCOES_NADA;

        cfg->textoGrande = (escolhido == 1);
        return OPCOES_VIDEO;
    }

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
        return OPCOES_NADA;

    *cfg = nova;
    return OPCOES_VIDEO;
}

// Lê teclado e mouse na tela de opções. Devolve o que mudou (cfg já volta atualizada; as
// preferências de EFEITOS e SONS já ficam valendo na hora).
//   W / S / SETAS ↑↓       : anda entre as abas, os campos da aba e o VOLTAR (ou na lista aberta)
//   A / D / SETAS ←→       : nas abas troca de aba; liga/desliga um efeito; muda um volume
//   TAB                    : próxima aba
//   ENTER / E / ESPAÇO     : abre a lista ou escolhe o item (VÍDEO), liga/desliga (EFEITOS)
//   mouse                  : clica na aba, na caixa, no interruptor ou na barra de volume (dá para arrastar)
// (ESC e BACKSPACE fecham a lista; ficam no loop principal)
MudancaOpcoes AtualizarOpcoes(MenuOpcoes *m, ConfigExibicao *cfg)
{
    Vector2 mouse = GetMousePosition();
    bool mouseMexeu = (Vector2Length(GetMouseDelta()) > 0.0f);
    bool clicou = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool confirmou = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE);
    bool cima = IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP);
    bool baixo = IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN);
    bool esquerda = IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT);
    bool direita = IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT);

    if (m->aba < 0 || m->aba >= NUM_ABAS_OPCOES)
        m->aba = ABA_VIDEO;

    if (m->aberta >= 0)
        return AtualizarListaVideo(m, cfg, mouse, mouseMexeu, clicou, confirmou, cima, baixo);

    int n = NumCamposAba(m->aba);           // o VOLTAR é o foco n

    if (IsKeyPressed(KEY_TAB)) {
        m->aba = (m->aba + 1) % NUM_ABAS_OPCOES;
        m->foco = FOCO_ABAS;
        return OPCOES_NADA;
    }

    // Abas (mouse)
    for (int a = 0; a < NUM_ABAS_OPCOES; a++) {

        if (!CheckCollisionPointRec(mouse, AbaOpcoesRet(a)))
            continue;

        if (mouseMexeu)
            m->foco = FOCO_ABAS;

        if (clicou) {
            m->aba = a;
            m->foco = FOCO_ABAS;
            return OPCOES_NADA;
        }
    }

    if (cima)  m->foco = (m->foco <= FOCO_ABAS) ? n : m->foco - 1;
    if (baixo) m->foco = (m->foco >= n) ? FOCO_ABAS : m->foco + 1;

    // VOLTAR AO MENU
    if (MouseEm(BotaoVoltarOpcoes())) {

        if (mouseMexeu)
            m->foco = n;

        if (clicou) {
            m->sair = true;
            return OPCOES_NADA;
        }
    }

    if (m->foco == n) {
        if (confirmou) m->sair = true;
        return OPCOES_NADA;
    }

    // Fileira das abas: A/D troca de aba, ENTER desce para o primeiro campo
    if (m->foco == FOCO_ABAS) {
        if (esquerda)  m->aba = (m->aba + NUM_ABAS_OPCOES - 1) % NUM_ABAS_OPCOES;
        if (direita)   m->aba = (m->aba + 1) % NUM_ABAS_OPCOES;
        if (confirmou) m->foco = 0;
        return OPCOES_NADA;
    }

    if (m->aba == ABA_VIDEO) {

        for (int c = 0; c < NUM_CAMPOS_OPCOES; c++) {

            if (!CheckCollisionPointRec(mouse, CaixaOpcoes(c)))
                continue;

            if (mouseMexeu)
                m->foco = c;

            if (clicou) {
                AbrirListaOpcoes(m, c, *cfg);
                return OPCOES_NADA;
            }
        }

        if (confirmou)
            AbrirListaOpcoes(m, m->foco, *cfg);

        return OPCOES_NADA;
    }

    if (m->aba == ABA_EFEITOS) {

        int trocar = -1;

        for (int c = 0; c < NUM_CAMPOS_EFEITOS; c++) {

            if (!CheckCollisionPointRec(mouse, LinhaEfeito(c)))
                continue;

            if (mouseMexeu)
                m->foco = c;

            if (clicou)
                trocar = c;
        }

        if (confirmou || esquerda || direita)
            trocar = m->foco;

        if (trocar < 0)
            return OPCOES_NADA;

        bool *v = ValorEfeito(&cfg->pref, trocar);
        *v = !*v;
        DefinirPreferencias(cfg->pref);
        TocarSom(sfxSelecao);
        return OPCOES_PREFERENCIAS;
    }

    // ABA_SONS: clique (ou arraste) na barra, ou A/D no volume marcado
    int campo = -1;
    int valor = 0;

    for (int c = 0; c < NUM_CAMPOS_SONS; c++) {

        Rectangle barra = BarraVolume(c);
        Rectangle linha = { barra.x - 10, barra.y - 24, barra.width + 80, barra.height + 32 };
        Rectangle toque = { barra.x - 10, barra.y - 6, barra.width + 20, barra.height + 12 };

        if (!CheckCollisionPointRec(mouse, linha))
            continue;

        if (mouseMexeu)
            m->foco = c;

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, toque)) {
            campo = c;
            valor = (int)roundf((mouse.x - barra.x) / barra.width * VOLUME_PASSOS);
        }
    }

    if (campo < 0 && (esquerda || direita)) {
        campo = m->foco;
        valor = *ValorVolume(&cfg->pref, campo) + (direita ? 1 : -1);
    }

    if (campo < 0)
        return OPCOES_NADA;

    if (valor < 0) valor = 0;
    if (valor > VOLUME_PASSOS) valor = VOLUME_PASSOS;

    int *v = ValorVolume(&cfg->pref, campo);
    if (*v == valor)
        return OPCOES_NADA;

    *v = valor;
    DefinirPreferencias(cfg->pref);
    if (campo != SOM_MUSICA)
        TocarSom(sfxSelecao);               // ouve o volume novo (a música do menu já está tocando)
    return OPCOES_PREFERENCIAS;
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

static const Color COR_CAIXA_OPCOES = { 10, 8, 22, 255 };

// Aba VÍDEO: as caixas fechadas, com o valor atual
static void DesenharAbaVideo(const MenuOpcoes *m, ConfigExibicao cfg)
{
    const char *rotulos[NUM_CAMPOS_OPCOES] = { "MODO DE EXIBIÇÃO", "RESOLUÇÃO", "TAMANHO DO TEXTO" };
    const char *valores[NUM_CAMPOS_OPCOES] = { NOMES_MODO_EXIBICAO[cfg.modo], NomeResolucao(cfg.resolucao),
                                               NOMES_TAMANHO_TEXTO[cfg.textoGrande ? 1 : 0] };

    for (int c = 0; c < NUM_CAMPOS_OPCOES; c++) {

        Rectangle caixa = CaixaOpcoes(c);
        bool focado = (c == m->foco);

        DrawText(rotulos[c], (int)caixa.x, (int)caixa.y - 18, 12, focado ? SKYBLUE : GRAY);

        DrawRectangleRec(caixa, COR_CAIXA_OPCOES);
        DrawRectangleLinesEx(caixa, 2, focado ? PURPLE : Fade(PURPLE, 0.4f));

        if (focado && m->aberta < 0)
            DesenharCursorLuci(caixa.x - 18, caixa.y + caixa.height / 2);

        DrawText(valores[c], (int)caixa.x + 14, (int)(caixa.y + caixa.height / 2) - LinhaTexto(10), 20, focado ? WHITE : LIGHTGRAY);
        DesenharSetaLista(caixa, m->aberta == c, focado ? SKYBLUE : GRAY);
    }
}

// Aba EFEITOS: nome, explicação curta e um interruptor (a bolinha fica do lado do estado)
static void DesenharAbaEfeitos(const MenuOpcoes *m, Preferencias p)
{
    for (int c = 0; c < NUM_CAMPOS_EFEITOS; c++) {

        Rectangle l = LinhaEfeito(c);
        Rectangle s = InterruptorEfeito(c);
        bool focado = (c == m->foco);
        bool ligado = *ValorEfeito(&p, c);
        Color cor = ligado ? LIME : GRAY;

        if (focado) {
            DrawRectangleRec(l, Fade(PURPLE, 0.18f));
            DesenharCursorLuci(l.x - 14, l.y + l.height / 2);
        }

        DrawText(ROTULOS_EFEITOS[c], (int)l.x + 12, (int)l.y + 3, 14, focado ? SKYBLUE : LIGHTGRAY);
        DrawText(DESCRICOES_EFEITOS[c], (int)l.x + 12, (int)l.y + 3 + LinhaTexto(17), 10, GRAY);

        DrawRectangleRec(s, COR_CAIXA_OPCOES);
        DrawRectangleLinesEx(s, 2, focado ? cor : Fade(cor, 0.55f));

        float bx = ligado ? s.x + s.width - 16 : s.x + 16;
        DrawCircle((int)bx, (int)(s.y + s.height / 2), 8, cor);

        const char *t = ligado ? "LIGADO" : "DESLIGADO";
        float meio = ligado ? s.x + (s.width - 28) / 2 : s.x + 28 + (s.width - 28) / 2;
        DrawText(t, (int)meio - MeasureText(t, 12) / 2, (int)(s.y + s.height / 2) - LinhaTexto(6), 12, focado ? WHITE : LIGHTGRAY);
    }
}

// Aba SONS: uma barra de quadradinhos por volume, com a porcentagem ao lado
static void DesenharAbaSons(const MenuOpcoes *m, Preferencias p)
{
    for (int c = 0; c < NUM_CAMPOS_SONS; c++) {

        Rectangle b = BarraVolume(c);
        bool focado = (c == m->foco);
        int v = *ValorVolume(&p, c);
        float folga = 3.0f;
        float w = (b.width - folga * (VOLUME_PASSOS - 1)) / VOLUME_PASSOS;

        if (focado)
            DesenharCursorLuci(b.x - 18, b.y + b.height / 2);

        DrawText(ROTULOS_SONS[c], (int)b.x, (int)b.y - 22, 14, focado ? SKYBLUE : LIGHTGRAY);

        for (int i = 0; i < VOLUME_PASSOS; i++) {
            Rectangle q = { b.x + i * (w + folga), b.y, w, b.height };
            DrawRectangleRec(q, (i < v) ? (focado ? SKYBLUE : Fade(SKYBLUE, 0.7f)) : COR_CAIXA_OPCOES);
            DrawRectangleLinesEx(q, 1, Fade(SKYBLUE, focado ? 0.6f : 0.35f));
        }

        const char *t = (v == 0) ? "MUDO" : TextFormat("%d%%", v * 100 / VOLUME_PASSOS);
        DrawText(t, (int)(b.x + b.width) + 16, (int)(b.y + b.height / 2) - LinhaTexto(8), 16, focado ? WHITE : LIGHTGRAY);
    }
}

void DesenharOpcoes(const MenuOpcoes *m, ConfigExibicao cfg, Color fundo)
{
    Color painelCor = (Color){ 18, 15, 30, 255 };
    Rectangle painel = PAINEL_OPCOES;

    if (fundo.a > 0)                        // BLANK = o fundo já foi desenhado (pause)
        ClearBackground(fundo);

    DrawRectangleRec(painel, painelCor);
    DrawRectangleLinesEx(painel, 4, PURPLE);
    DrawRectangleLinesEx((Rectangle){ painel.x + 8, painel.y + 8, painel.width - 16, painel.height - 16 }, 1, Fade(SKYBLUE, 0.35f));

    const char *titulo = "OPÇÕES";
    DrawText(titulo, LARGURA_TELA / 2 - MeasureText(titulo, 36) / 2, 78, 36, PURPLE);

    // Abas
    for (int a = 0; a < NUM_ABAS_OPCOES; a++) {

        Rectangle r = AbaOpcoesRet(a);
        bool atual = (a == m->aba);
        int w = MeasureText(NOMES_ABAS[a], 16);

        DrawRectangleRec(r, atual ? Fade(PURPLE, 0.35f) : COR_CAIXA_OPCOES);
        DrawRectangleLinesEx(r, 2, atual ? PURPLE : Fade(PURPLE, 0.35f));
        DrawText(NOMES_ABAS[a], (int)(r.x + r.width / 2) - w / 2, (int)(r.y + r.height / 2) - LinhaTexto(8), 16, atual ? WHITE : GRAY);

        if (atual && m->foco == FOCO_ABAS && m->aberta < 0)
            DesenharCursorLuci(r.x - 16, r.y + r.height / 2);
    }
    DrawLine((int)painel.x + 20, 164, (int)(painel.x + painel.width) - 20, 164, Fade(PURPLE, 0.5f));

    if (m->aba == ABA_EFEITOS)   DesenharAbaEfeitos(m, cfg.pref);
    else if (m->aba == ABA_SONS) DesenharAbaSons(m, cfg.pref);
    else                         DesenharAbaVideo(m, cfg);

    // VOLTAR AO MENU (o mesmo que ESC)
    {
        Rectangle r = BotaoVoltarOpcoes();
        bool marcado = (m->foco == NumCamposAba(m->aba) && m->aberta < 0);

        DesenharBotaoMenu(r, m->naPausa ? "VOLTAR À PAUSA" : "VOLTAR AO MENU", 14, SKYBLUE, marcado);

        if (marcado)
            DesenharCursorLuci(r.x - 18, r.y + r.height / 2);
    }

    const char *dica1;
    if (m->aberta >= 0)                dica1 = "ENTER / CLIQUE - escolher";
    else if (m->foco == FOCO_ABAS)     dica1 = "A / D - trocar de aba     ENTER - entrar na aba";
    else if (m->aba == ABA_EFEITOS)    dica1 = "ENTER / CLIQUE / A / D - ligar e desligar     TAB - próxima aba";
    else if (m->aba == ABA_SONS)       dica1 = "A / D ou CLIQUE NA BARRA - mudar o volume     TAB - próxima aba";
    else                               dica1 = "ENTER / CLIQUE - abrir a lista     TAB - próxima aba";
    const char *dica2 = (m->aberta >= 0) ? "ESC - fechar a lista" : (m->naPausa ? "ESC - voltar à pausa" : "ESC - voltar ao menu");

    DrawText(dica1, LARGURA_TELA / 2 - MeasureText(dica1, 10) / 2, 476, 10, Fade(SKYBLUE, 0.6f));
    DrawText(dica2, LARGURA_TELA / 2 - MeasureText(dica2, 10) / 2, 476 + LinhaTexto(16), 10, GRAY);

    // Lista aberta (aba VÍDEO) por cima de tudo
    if (m->aba != ABA_VIDEO || m->aberta < 0)
        return;

    Resolucao lista[MAX_RESOLUCOES];
    ListarResolucoes(cfg.modo, lista);
    int numItens = ItensListaOpcoes(m->aberta, cfg.modo);

    Rectangle area = ItemListaOpcoes(m->aberta, 0);
    area.height = numItens * (float)ALTURA_ITEM_LISTA;

    DrawRectangleRec((Rectangle){ area.x - 2, area.y - 2, area.width + 4, area.height + 4 }, COR_CAIXA_OPCOES);
    DrawRectangleLinesEx((Rectangle){ area.x - 2, area.y - 2, area.width + 4, area.height + 4 }, 2, PURPLE);

    for (int i = 0; i < numItens; i++) {

        Rectangle r = ItemListaOpcoes(m->aberta, i);
        bool marcado = (i == m->marcado);
        bool atual;
        const char *nome;

        if (m->aberta == CAMPO_MODO) {
            nome = NOMES_MODO_EXIBICAO[i];
            atual = (i == (int)cfg.modo);
        } else if (m->aberta == CAMPO_TEXTO) {
            nome = NOMES_TAMANHO_TEXTO[i];
            atual = (i == (cfg.textoGrande ? 1 : 0));
        } else {
            nome = NomeResolucao(lista[i]);
            atual = MesmaResolucao(lista[i], cfg.resolucao);
        }

        if (marcado)
            DrawRectangleRec(r, Fade(PURPLE, 0.35f));

        DrawText(nome, (int)r.x + 14, (int)(r.y + r.height / 2) - LinhaTexto(8), 16, marcado ? SKYBLUE : LIGHTGRAY);

        if (atual)
            DrawText("ATUAL", (int)(r.x + r.width) - MeasureText("ATUAL", 10) - 12, (int)(r.y + r.height / 2) - LinhaTexto(6), 10, GOLD);
    }
}
