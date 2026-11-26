#include "raylib.h"
#include "raymath.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <time.h>

#include "include/core/fonte.h"
#include "include/core/config.h"
#include "include/core/mapas.h"
#include "include/core/tipos.h"
#include "include/core/texto.h"
#include "include/audio/audio.h"
#include "include/core/telemetria.h"
#include "include/core/save.h"
#include "include/entidades/jogador.h"
#include "include/bloco1/minigame_visual.h"
#include "include/entidades/luci_companion.h"
#include "include/telas/ranking.h"
#include "include/bloco1/corredor.h"
#include "include/bloco1/batalha.h"
#include "include/dados/conteudo_bloco1.h"
#include "include/bloco1/batalha_monitores.h"
#include "include/bloco1/objetivo.h"
#include "include/telas/pause.h"
#include "include/telas/resolucao.h"
#include "include/telas/intro.h"
#include "include/bloco2/biblioteca_tipos.h"
#include "include/bloco2/biblioteca.h"
#include "include/telas/jogo.h"
#include "include/telas/historico_luci.h"

// =============================================================
// UtopIA - BLOCO 1
//   Sala inicial (Luci) -> porta de cima -> Corredor dos Monitores
//   Cada monitor tem um quiz (conteúdo do "Quizz - missão 1.pdf").
//   Tempo esgotado -> COMBATE estilo Undertale
//   Resposta errada -> MINIGAME de esquiva estilo Undertale
//   Resposta certa  -> ganha pontos
// =============================================================

// =============================================================
// MAIN
// =============================================================

// A tela que vai para o save: a batalha volta para a sala de origem e a sala da
// ALUCINAÇÃO MESTRE volta para o corredor (a luta recomeça pela porta da direita)
// Travadas: registra no CSV dos testes o quadro que demorou demais e o que havia na tela nele
static void RegistrarQuadroLento(const Jogo *jg)
{
#if TELEMETRIA_ATIVA
    static int registrados = 0;
    float ms = GetFrameTime() * 1000.0f;

    if (ms < QUADRO_LENTO_MS || GetTime() < 3.0 || registrados >= MAX_QUADROS_LENTOS)
        return;                                     // os primeiros segundos são a carga do jogo
    registrados++;

    if (jg->tela != BIBLIOTECA) {
        Telemetria("quadro_lento", TextFormat("tela %d%s", (int)jg->tela, jg->pausado ? " pausa" : ""), ms, 0);
        return;
    }

    const Biblioteca *b = &jg->biblioteca;
    int inimigos = 0, tiros = 0, particulas = 0;
    for (int i = 0; i < MAX_INIMIGOS; i++)         if (b->ini[i].ativo) inimigos++;
    for (int i = 0; i < MAX_PROJETEIS; i++)        if (b->proj[i].ativo) tiros++;
    for (int i = 0; i < MAX_PARTICULAS_ROGUE; i++) if (b->part[i].vida > 0.0f) particulas++;

    Telemetria("quadro_lento", TextFormat("%s ini %d tiros %d part %d arma %d%s", NOMES_SALAS_ROGUE[b->tipoSala],
                                          inimigos, tiros, particulas, (int)RogueArmaAtual(&b->j), jg->pausado ? " pausa" : ""),
               ms, inimigos);
#else
    (void)jg;
#endif
}

static TelaAtual TelaParaSalvar(TelaAtual tela, TelaAtual salaDaBatalha, const Biblioteca *b)
{
    if (tela == BATALHA_B2) return salaDaBatalha;
    if (tela == BIBLIOTECA && b->tipoSala == SALA_MESTRE) return CORREDOR;
    return tela;
}

// =============================================================
// DEV MENU (F1) — atalhos para testar rápido. Para desligar na versão final: DEV_MENU 0
// =============================================================
#define DEV_MENU 1

enum { DEV_SALA, DEV_CORREDOR, DEV_ATRIO, DEV_MESTRE, DEV_MISSAO, DEV_BLOCO3, DEV_LUCI,
       DEV_INDICE, DEV_PONTOS,
       DEV_ARMA, DEV_INVENCIVEL, DEV_CURAR, DEV_FC, DEV_LIMPAR, DEV_SOM, NUM_DEV };
// Índice no DEV: CALCULADO e depois 0, 10, ... 100 (60+ = BOM: menos vida e ritmo, dicas falsas mais raras;
// abaixo de 60 = RUIM: mais vida, ritmo 1,2x e a 3ª barra exige o pedido com contexto)

#define NUM_DEV_SONS 76
static const char *NOMES_DEV_SONS[NUM_DEV_SONS] = {
    "Disparador", "Vetor", "Roteador", "Fluxo", "Orbe", "Fragmentadora", "Pulso de Firewall", "Katana",
    "Raio: circuito", "Raio Classico", "Raio: coluna de luz", "Orbe: implosao", "Frag.: cartucho dourado", "Vetor: carga cheia",
    "Morte de inimigo", "Acerto", "Erro", "Dano", "Pegar", "Aviso de parry", "Parry perfeito", "Estatica",
    "Luci: fio rompe", "Luci: corrupcao", "Luci: explosao", "Luci: telegrafia", "Luci: investida", "Luci: orbe",
    "Luci: orbe rebatido", "Luci: varredura", "Luci: onda", "Luci: chao racha", "Luci: chao cai", "Luci: colapso",
    "Luci: renascer", "Luci: superaquecida",
    "Inimigo: bote", "Guardiao: leque", "Botnet: pacote", "Keylogger: tecla", "Inimigo: anel de balas", "Eco: estoura",
    "Firewall: barreira", "Overclock: carga", "Overclock: investida", "Overclock: superaquecido", "Cache: cura",
    "Alucinacao: teleporte", "Phisher: revela", "Tiro bloqueado", "Inimigo surgindo",
    "Chefe: virada de fase", "Chefe: chamando ajudantes", "Chefe: escudo caindo", "Chefe: derrotado", "Chefe: mira travando",
    "Alucinador: investida", "Chefe: rajada em leque", "Alucinador: espiral", "Sentinela: feixe", "Retro: rebobinando",
    "Retro: golpe devolvido", "Janela: resumindo", "Silogista: linhas", "Silogista: corte",
    "Bloco 1: caixa fechando", "Bloco 1: pegar verdade", "Bloco 1: Luci leva golpe", "Bloco 1: ataque acerta",
    "Bloco 1: lupa voando", "Bloco 1: VERIFICADO", "Bloco 1: sobrecarga",
    "Jogador: esquiva", "Jogador: pulso", "Jogador: parry armado", "Jogador: caiu"
};

static void DevTocarSom(int k)
{
    if (k < NUM_SONS_ARMA) { TocarSomArma(k, 1.0f); return; }
    switch (k) {
        case 10: TocarGrupo(&gRaioColuna, 1.0f, 1.0f);   break;
        case 11: TocarGrupo(&gOrbeImplosao, 1.0f, 1.0f); break;
        case 12: TocarGrupo(&gFragDourado, 1.0f, 1.0f);  break;
        case 13: TocarGrupo(&gVetorCarga, 1.0f, 0.9f);   break;
        case 14: TocarGrupo(&gMorteInimigo, 1.0f, 1.0f); break;
        case 15: TocarSom(sfxAcerto);        break;
        case 16: TocarSom(sfxErro);          break;
        case 17: TocarSom(sfxDano);          break;
        case 18: TocarSom(sfxPegar);         break;
        case 19: TocarSom(sfxAvisoParry);    break;
        case 20: TocarSom(sfxParryPerfeito); break;
        case 21: TocarSom(sfxEstatica);      break;
        default:
            if (k - 22 < NUM_GRUPOS_LUCI)                        TocarGrupo(GRUPOS_LUCI[k - 22], 1.0f, 1.0f);
            else if (k - 22 - NUM_GRUPOS_LUCI < NUM_GRUPOS_INI)  TocarGrupo(GRUPOS_INI[k - 22 - NUM_GRUPOS_LUCI], 1.0f, 1.0f);
            else if (k - 22 - NUM_GRUPOS_LUCI - NUM_GRUPOS_INI < NUM_GRUPOS_CHEFES)
                TocarGrupo(GRUPOS_CHEFES[k - 22 - NUM_GRUPOS_LUCI - NUM_GRUPOS_INI], 1.0f, 1.0f);
            else if (k - 22 - NUM_GRUPOS_LUCI - NUM_GRUPOS_INI - NUM_GRUPOS_CHEFES < NUM_GRUPOS_B1)
                TocarGrupo(GRUPOS_B1[k - 22 - NUM_GRUPOS_LUCI - NUM_GRUPOS_INI - NUM_GRUPOS_CHEFES], 1.0f, 1.0f);
            else if (k - 22 - NUM_GRUPOS_LUCI - NUM_GRUPOS_INI - NUM_GRUPOS_CHEFES - NUM_GRUPOS_B1 < NUM_GRUPOS_JOG)
                TocarGrupo(GRUPOS_JOG[k - 22 - NUM_GRUPOS_LUCI - NUM_GRUPOS_INI - NUM_GRUPOS_CHEFES - NUM_GRUPOS_B1], 1.0f, 1.0f);
            break;
    }
}

static void DesenharDevMenu(int sel, int arma, int som, bool naBiblioteca, int pontos)
{
    const char *itens[NUM_DEV];
    itens[DEV_SALA]       = "Ir: Sala inicial (Luci)";
    itens[DEV_CORREDOR]   = "Ir: Corredor dos Monitores";
    itens[DEV_ATRIO]      = "Ir: Biblioteca - atrio";
    itens[DEV_MESTRE]     = "Ir: sala da Alucinacao Mestre";
    itens[DEV_MISSAO]     = "Ir: comecar uma missao";
    itens[DEV_BLOCO3]     = "Ir: Bloco 3 (sala de transicao)";
    itens[DEV_LUCI]       = "Ir: arena da Luci (chefe final)";
    itens[DEV_INDICE]     = (g_devIndice < 0) ? "Indice da luta final:  < CALCULADO >"
                          : TextFormat("Indice da luta final:  < %d  (%s) >", g_devIndice, g_devIndice >= INDICE_FINAL_BOM ? "BOM" : "RUIM");
    itens[DEV_PONTOS]     = TextFormat("Pontos +100  (agora: %d)", pontos);
    itens[DEV_ARMA]       = TextFormat("Arma:  < %s >", DEF_ARMAS[arma].nome);
    itens[DEV_INVENCIVEL] = TextFormat("Invencivel: %s", g_devInvencivel ? "SIM" : "NAO");
    itens[DEV_CURAR]      = "Vida e EP cheios";
    itens[DEV_FC]         = "+100 FC";
    itens[DEV_LIMPAR]     = "Matar os inimigos da sala";
    itens[DEV_SOM]        = TextFormat("Testar som:  < %s >", NOMES_DEV_SONS[som]);

    int x = 230, y = 40, w = 340, h = 52 + NUM_DEV * 26 + 30;
    DrawRectangle(x, y, w, h, (Color){ 10, 14, 28, 245 });
    DrawRectangleLines(x, y, w, h, (Color){ 0, 228, 255, 255 });
    DrawText("DEV MENU", x + 14, y + 12, 20, GOLD);
    DrawText("F1 fecha", x + w - 70, y + 16, 12, GRAY);
    for (int i = 0; i < NUM_DEV; i++) {
        int iy = y + 46 + i * 26;
        bool precisaBib = (i == DEV_ARMA || i == DEV_CURAR || i == DEV_LIMPAR);
        Color c = (precisaBib && !naBiblioteca) ? DARKGRAY : ((i == sel) ? BLACK : RAYWHITE);
        if (i == sel) DrawRectangle(x + 8, iy - 3, w - 16, 22, (Color){ 0, 228, 255, 255 });
        DrawText(itens[i], x + 16, iy, 16, c);
    }
    DrawText("Cima/Baixo escolhe  -  Esq/Dir troca  -  ENTER usa", x + 14, y + h - 22, 12, LIGHTGRAY);
}

// Quanto a tela mostra a corrupção (0 = limpa, 1 = no limite), para os efeitos de tela.
// Blocos 1 e 2: segue a estabilidade da Luci. Bloco 3: mais alta, e sobe a cada barra da luta final.
static float CorrupcaoDaTela(const Jogo *jg)
{
    float instavel = 1.0f - (float)jg->estabilidadeLuci / ESTABILIDADE_MAX_LUCI;

    switch (jg->tela) {
        case JOGANDO: case CORREDOR: case BATALHA: case BATALHA_B2:
            return 0.08f + 0.45f * instavel;
        case BIBLIOTECA: {
            const Biblioteca *b = &jg->biblioteca;
            if (b->morreu)
                return 0.5f;
            if (RogueNoBloco3(b)) {
                const LuciChefe *l = &b->luci;
                if (b->tipoSala == SALA_LUCI && l->ativo && l->apresentado && l->morrendo <= 0.0f)
                    return (l->faseOculta >= 3) ? 0.95f : (l->faseOculta == 2) ? 0.8f : 0.65f;
                return 0.5f;
            }
            return 0.08f + 0.45f * instavel;
        }
        default:
            return 0.12f;                   // menu, opções, abertura
    }
}

// Dá para pausar agora? (ESC e o R do histórico): só durante o jogo, fora das janelas da Biblioteca
// que têm o próprio ESC, sem transição e sem uma pergunta SIM / NÃO aberta
static bool PodePausarJogo(const Jogo *jg)
{
    const Biblioteca *b = &jg->biblioteca;
    bool telaDeJogo = jg->tela == JOGANDO || jg->tela == CORREDOR || jg->tela == BATALHA || jg->tela == BATALHA_B2 ||
                      (jg->tela == BIBLIOTECA && !b->lojaAberta && !b->enciclopediaAberta && !b->mapaAberto && !b->lab.aberto &&
                       !b->seletorAberto);

    return telaDeJogo && !jg->transicao.ativa && jg->dlg.estado != DLG_CONFIRMA;
}

int main(void) {

    // Todo o estado do jogo fica numa struct só (include/telas/jogo.h): as telas recebem um ponteiro para ela
    static Jogo jogo;                       // static: a Biblioteca dentro dela é grande para a pilha
    Jogo *jg = &jogo;

    // 1. INICIALIZAÇÃO

    SetConfigFlags(FLAG_VSYNC_HINT);        // sincroniza com o monitor: sem rasgos e movimento mais liso
    InitWindow(LARGURA_TELA, ALTURA_TELA, "UtopIA - Game");
    CarregarFonteJogo();
    IniciarSons();
    PrepararSaberMaisMonitores();
    Telemetria("sessao_inicio", "", 0.0f, 0);

    // ESC não fecha mais a janela: ele abre o menu de pause
    // (fechar pelo X da janela continua funcionando)
    SetExitKey(KEY_NULL);

    // Tudo é desenhado aqui em 800x600 e depois esticado para a janela
    jg->alvo = LoadRenderTexture(LARGURA_TELA, ALTURA_TELA);
    jg->filtroNitidez = CarregarFiltroNitidez();

    // Modo de exibição e resolução escolhidos da última vez (OPÇÕES)
    GuardarMonitor();

    jg->exibicao = LerConfigExibicao();
    AplicarExibicao(jg->exibicao);

    jg->menuOpcoes = (MenuOpcoes){ .foco = CAMPO_MODO, .aberta = -1 };

    jg->tela = MENU;

    jg->cor_de_fundo_menu = (Color){2, 0, 12, 255};

    // caminhos das imagens (sprites) do jogo e carregamento delas
    jg->imagem_titulo = LoadTexture("resources/ui/titulo.png");
    SetTextureFilter(jg->imagem_titulo, TEXTURE_FILTER_BILINEAR);   // a logo é desenhada reduzida: suaviza a redução
    CarregarLuci();
    CarregarMapasNovos();                // corredor do Bloco 1 e arena final (reduzidos para a tela)

    // Personagem: uma folha de animação por direção (na ordem do enum Direcao)

    jg->spritesJogador[DIR_FRENTE]   = CarregarSprite(SPRITE_ANDANDO_FRENTE,   SPRITE_FRENTE, false);
    jg->spritesJogador[DIR_COSTAS]   = CarregarSprite(SPRITE_ANDANDO_COSTAS,   SPRITE_COSTAS, false);
    jg->spritesJogador[DIR_DIREITA]  = CarregarSprite(SPRITE_ANDANDO_DIREITA,  SPRITE_LADO,   false);
    jg->spritesJogador[DIR_ESQUERDA] = CarregarSprite(SPRITE_ANDANDO_ESQUERDA, SPRITE_LADO,   true);

    // Diagonais: sem folha própria, usam a de lado do mesmo sentido
    jg->spritesJogador[DIR_FRENTE_DIREITA]  = CarregarSpriteDiagonal(SPRITE_ANDANDO_FRENTE_DIREITA,  jg->spritesJogador[DIR_DIREITA]);
    jg->spritesJogador[DIR_FRENTE_ESQUERDA] = CarregarSpriteDiagonal(SPRITE_ANDANDO_FRENTE_ESQUERDA, jg->spritesJogador[DIR_ESQUERDA]);
    jg->spritesJogador[DIR_COSTAS_DIREITA]  = CarregarSpriteDiagonal(SPRITE_ANDANDO_COSTAS_DIREITA,  jg->spritesJogador[DIR_DIREITA]);
    jg->spritesJogador[DIR_COSTAS_ESQUERDA] = CarregarSpriteDiagonal(SPRITE_ANDANDO_COSTAS_ESQUERDA, jg->spritesJogador[DIR_ESQUERDA]);

    jg->animJogador = (AnimacaoJogador){0};

    // Sprites dos monitores (só carrega os que existirem)
    const char *arquivosMonitor[4] = {
        SPRITE_MONITOR_NEUTRO,
        SPRITE_MONITOR_ATIVO,
        SPRITE_MONITOR_ACERTO,
        SPRITE_MONITOR_ERRO
    };

    memset(jg->spritesMonitor, 0, sizeof(jg->spritesMonitor));

    for (int i = 0; i < 4; i++)
        if (FileExists(arquivosMonitor[i]))
            jg->spritesMonitor[i] = LoadTexture(arquivosMonitor[i]);

    // Mapa da sala inicial (opcional: sem o arquivo, usa o cenário simples)
    jg->texMapaSala = FileExists(ARQUIVO_MAPA_SALA) ? LoadTexture(ARQUIVO_MAPA_SALA) : (Texture2D){0};

    jg->playerPos = SPAWN_SALA;
    jg->direcaoAtual = DIR_FRENTE;

    // distância/tamanho da área de interação (o "alcance" do jogador pra apertar E)
    jg->interacaoAlcance = 50.0f; // AJUSTE AQUI se quiser o alcance maior/menor
    jg->pertoDaLuci = false;

    // --- NPC: LUCI ---
    jg->luciHitbox = (Rectangle){ MAPA_X(512), MAPA_Y(640), TAM_SPRITE, TAM_SPRITE };
    jg->luciAcompanhando = false;   // depois da conversa ela vira companion
    jg->companionAngulo = 0.0f;

    jg->tempo = 0.0f;

    jg->fecharJogo = false;

    // =========================================================
    // VARIÁVEIS DA TELA INTRO
    // =========================================================

    jg->etapaIntro = 0;

    // Nickname do jogador: exatamente 3 letras, sem repetir um nome do ranking
    memcpy(jg->nickname, (char[4]){'_', '_', '_', '\0'}, sizeof(jg->nickname));
    jg->nicknamePos = 0;
    jg->cliqueConfirmarNome = false;        // clicou em CONTINUAR na tela do nickname
    jg->avisoNomeRepetido = 0.0f;          // tempo do aviso "esse nome já está no ranking"

    memcpy(jg->textosIntro, (const char *[3]){
        "A Inteligência Artificial virou parte da rotina:\nela escreve textos, gera imagens e ajuda a decidir.",
        "Mas com dados errados ou perguntas enviesadas,\nela pode ALUCINAR: responder com confiança algo falso.",
        "Bem-vindo a UtopIA, o mundo dentro da IA.\nAs alucinações estão fazendo tudo desmoronar...",
    }, sizeof(jg->textosIntro));

    // =========================================================
    // VARIÁVEIS DO CORREDOR E DA BATALHA
    // =========================================================

    jg->progresso = (ProgressoCorredor){0};
    jg->batalhaB2 = (BatalhaB2){0};
    jg->salaDaBatalhaB2 = CORREDOR;    // para onde volta depois da batalha
    jg->cartao = (CartaoResumo){0};               // cartão de resumo depois de cada desafio
    jg->tempoParado = 0.0f;                // há quanto tempo o jogador não anda (Luci reage)
    jg->estabilidadeLuci = ESTABILIDADE_MAX_LUCI;   // gasta ao pedir ajuda (H) nas batalhas
    jg->ajudasPedidas = 0;
    jg->chefeVencido = false;               // venceu a ALUCINAÇÃO MESTRE (vai para o save)
    jg->entrarSalaMestre = false;           // a transição para a Biblioteca abre a sala da Mestre (fim do Bloco 1)
    jg->chefeAnunciado = false;             // a Luci já chamou o chefe nesta sessão (depois, só pela porta)
    jg->progBiblioteca = (ProgressoBiblioteca){0};   // o que não se perde na Biblioteca (vai para o save)
    jg->voltarAoMenuDepoisDoRanking = false;   // fim do jogo: depois do ranking, o menu
    jg->idPartida = NovoIdPartida();         // esta partida no ranking (vai para o save)
    jg->dlg = (SessaoDialogo){0};
    jg->transicao = (Transicao){0};
    jg->batalha = (Batalha){0};

    jg->quizDaBatalha = -1;                  // monitor que gerou a batalha
    jg->motivoBatalha = RES_NADA;

    // =========================================================
    // VARIÁVEIS DO SAVE
    // =========================================================

    jg->existeSave = LerSave(&jg->saveLido);    // mostra o botão CONTINUAR
    jg->pedirAutoSave = false;              // marcado nos momentos importantes; salva no fim do frame
    jg->avisoSave = 0.0f;                  // tempo restante do aviso "JOGO SALVO"
    jg->saveFalhou = false;
    jg->focoMenu = 0;                        // botão do menu marcado (0 CONTINUAR, 1 JOGAR, 2 OPÇÕES, 3 SAIR)

    // =========================================================
    // VARIÁVEIS DO RANKING
    // =========================================================

    LerRanking(&jg->ranking);                    // aparece no menu
    jg->telaRanking = (TelaRanking){0};           // ranking por cima do jogo (fim do Bloco 1 / NÚCLEO)

    // =========================================================
    // VARIÁVEIS DO PAUSE
    // =========================================================

    jg->pausado = false;
    jg->capturarPausa = false;              // apertou ESC: tira a "foto" do fundo no fim deste frame
    jg->selecaoPausa = PAUSA_CONTINUAR;
    jg->confirmarSaida = 0.0f;             // > 0 = clicou em LOBBY ou SAIR no pause, falta confirmar
    jg->opConfirmandoPausa = -1;             // qual opção do pause está pedindo a confirmação
    jg->historicoAberto = false;             // tela "FALAS DA LUCI" (R ou menu de pause)
    jg->historicoDireto = false;
    jg->opcoesNaPausa = false;               // OPÇÕES aberta de dentro do pause
    jg->rolagemHistorico = 0.0f;
    jg->fundoPausa = (Texture2D){0};


    // =========================================================
    // ÁREAS DE CLIQUE DOS BOTÕES DO MENU
    // =========================================================

    jg->btnContinuar = (Rectangle){300, 270, 200, 50};   // só aparece se existir save
    jg->btnJogar = (Rectangle){300, 270, 200, 50};
    jg->btnOpcoes = (Rectangle){300, 340, 200, 50};
    jg->btnSair = (Rectangle){300, 410, 200, 50};

    // O que o fim de uma batalha do Bloco 1 mexe
    jg->ctxB2 = (ContextoB2){
        &jg->dlg, &jg->transicao, &jg->progresso, &jg->batalhaB2, &jg->pedirAutoSave, &jg->salaDaBatalhaB2,
        &jg->cartao
    };

    // Um limitador só: a sincronização com o monitor (FLAG_VSYNC_HINT) já segura o ritmo, e o
    // SetTargetFPS(60) junto dela brigava com o monitor: em 60 Hz um quadro às vezes perdia a vez
    // (engasgo) e em 120/144 Hz os quadros saíam em intervalos desencontrados (tremedeira no combate).
    // O alvo agora é a própria taxa do monitor; o jogo usa o tempo de cada quadro (dt), então roda igual.
    int hzMonitor = GetMonitorRefreshRate(GetCurrentMonitor());
    SetTargetFPS((hzMonitor >= 30 && hzMonitor <= 240) ? hzMonitor : 60);

    // --- BGM: inicia com a música do menu ---
    TrocarBGM(&bgmMenu, 0);

    // 2. GAME LOOP

    while (!WindowShouldClose() && !jg->fecharJogo) {

        jg->dt = DtJogo();
        RegistrarQuadroLento(jg);

        // --- BGM: mantém o buffer de streaming preenchido ---
        AtualizarBGM();

        // --- BGM: troca a faixa conforme a tela ---
        if (!jg->pausado) {
            if      (jg->tela == MENU || jg->tela == INTRO || jg->tela == OPCOES) TrocarBGM(&bgmMenu,       0);
            else if (jg->tela == JOGANDO)                                  TrocarBGM(&bgmSala,       1);
            else if (jg->tela == CORREDOR)                                 TrocarBGM(&bgmCorredor,   2);
            else if (jg->tela == BATALHA)                                  TrocarBGM(&bgmBatalha,    3);
            else if (jg->tela == BATALHA_B2) {
                // Batalha de um monitor do Bloco 1 (a faixa da chefe fica para a ALUCINAÇÃO MESTRE).
                // A integridade da Luci zerou: a faixa de game over toca na tela de derrota
                if (jg->batalhaB2.telaDerrota && bgmGameover.stream.buffer) TrocarBGM(&bgmGameover, 8);
                else TrocarBGM(bgmBatalha.stream.buffer ? &bgmBatalha : &bgmMestre, 3);
            }
            else if (jg->tela == BIBLIOTECA) {
                // Biblioteca: o átrio é calmo (mesmo tom da faixa das missões), a arena da Luci tem faixa própria.
                // Caiu: a faixa de game over toca na tela da queda (volta a da sala quando ele levanta)
                if      (jg->biblioteca.morreu && bgmGameover.stream.buffer) TrocarBGM(&bgmGameover, 8);
                else if (jg->biblioteca.tipoSala == SALA_LUCI) {
                    // A cena antes da luta e o fim usam a faixa da arena. Na luta, uma faixa por barra
                    // (contradição, surto, núcleo): a queda congela o som e a barra nova entra com a próxima.
                    const LuciChefe *lc = &jg->biblioteca.luci;
                    bool luta = lc->apresentado && lc->morrendo <= 0.0f && !jg->biblioteca.resultadoAberto;
                    int barra = (lc->faseOculta >= 3) ? 2 : (lc->faseOculta == 2 ? 1 : 0);
                    if (luta && bgmLuciChefe[barra].stream.buffer)
                        TrocarBGM(&bgmLuciChefe[barra], 9 + barra);
                    else
                        TrocarBGM(bgmLuci.stream.buffer ? &bgmLuci : &bgmBiblioteca, 7);
                }
                // Luta contra o ALUCINADOR (também no seletor de chefes): uma faixa por fase; derrotado, volta a da Biblioteca
                else if (jg->biblioteca.tipoSala == SALA_CHEFE && jg->biblioteca.chefe.ativo && jg->biblioteca.chefe.apresentado &&
                         jg->biblioteca.chefe.morrendo <= 0.0f &&
                         bgmAlucinador[(jg->biblioteca.chefe.fase >= 2) ? 1 : 0].stream.buffer) {
                    int faseAluc = (jg->biblioteca.chefe.fase >= 2) ? 1 : 0;
                    TrocarBGM(&bgmAlucinador[faseAluc], 12 + faseAluc);
                }
                // Luta contra a JANELA DE CONTEXTO: fases 1 a 3 numa faixa, a SOBRECARGA (fase 4) em outra
                else if (jg->biblioteca.janela.ativo && bgmJanela[(jg->biblioteca.janela.fase >= 4) ? 1 : 0].stream.buffer) {
                    int faseJan = (jg->biblioteca.janela.fase >= 4) ? 1 : 0;
                    TrocarBGM(&bgmJanela[faseJan], 14 + faseJan);
                }
                // Corredor do núcleo (Bloco 3): a Luci foi embora e os registros contam quem ela é.
                // Toca a faixa da arena desde já (mesmo id): ela continua sem cortar quando a porta abre.
                else if (jg->biblioteca.tipoSala == SALA_TRANSICAO)
                    TrocarBGM(bgmLuci.stream.buffer ? &bgmLuci : &bgmBiblioteca, 7);
                // Fim do Bloco 1: o tutorial com a faixa da Biblioteca; a luta contra a ALUCINAÇÃO MESTRE com a dela
                else if (jg->biblioteca.tipoSala == SALA_MESTRE && jg->biblioteca.mestre.etapa == MT_LUTA &&
                         jg->biblioteca.mestre.morrendo <= 0.0f && bgmMestre.stream.buffer)
                    TrocarBGM(&bgmMestre, 4);
                else if (jg->biblioteca.tipoSala == SALA_ATRIO) TrocarBGM(bgmAtrio.stream.buffer ? &bgmAtrio : &bgmBiblioteca, 6);
                else                                        TrocarBGM(&bgmBiblioteca, 5);
            }
        }

        // --- AMBIENTE DO MAPA (Sons 2.0): loop sob a música, tirado da arte de cada lugar ---
        {
            int amb = AMB_NENHUM;
            if      (jg->tela == JOGANDO)                       amb = AMB_SALA;          // mapa1: pedra azul, telas ciano
            else if (jg->tela == CORREDOR || jg->tela == BATALHA)   amb = AMB_CORREDOR;      // mapa2: corredor dos terminais
            else if (jg->tela == BATALHA_B2)                    amb = AMB_LUCI;          // a Alucinação Mestre
            else if (jg->tela == BIBLIOTECA)
                amb = (jg->biblioteca.tipoSala == SALA_LUCI)   ? AMB_LUCI :            // mapa3: a arena roxa da Luci
                      (jg->biblioteca.tipoSala == SALA_MESTRE) ? AMB_SALA : AMB_BIBLIOTECA;
            static bool ambPausado = false;
            if (jg->pausado != ambPausado) { PausarAmbiente(jg->pausado); ambPausado = jg->pausado; }
            if (!jg->pausado) AtualizarAmbiente(amb, jg->dt);
        }

#if DEV_MENU
        // =====================================================
        // DEV MENU (F1): congela o jogo e mostra os atalhos de teste
        // =====================================================
        {
            static bool devAberto = false;
            static int devSel = 0, devArma = 0, devSom = 0;
            if (IsKeyPressed(KEY_F1)) devAberto = !devAberto;
            if (devAberto) {
                bool naBib = (jg->tela == BIBLIOTECA);
                if (IsKeyPressed(KEY_DOWN)) devSel = (devSel + 1) % NUM_DEV;
                if (IsKeyPressed(KEY_UP))   devSel = (devSel + NUM_DEV - 1) % NUM_DEV;
                int lado = IsKeyPressed(KEY_RIGHT) ? 1 : (IsKeyPressed(KEY_LEFT) ? -1 : 0);
                if (devSel == DEV_ARMA && lado != 0) devArma = (devArma + lado + NUM_ARMAS) % NUM_ARMAS;
                if (devSel == DEV_SOM && lado != 0)  { devSom = (devSom + lado + NUM_DEV_SONS) % NUM_DEV_SONS; DevTocarSom(devSom); }
                if (devSel == DEV_INDICE && lado != 0) {
                    int v = (g_devIndice < 0) ? ((lado > 0) ? -10 : 110) : g_devIndice;
                    v += 10 * lado;
                    g_devIndice = (v < 0 || v > 100) ? -1 : v;      // passa do fim: volta para o CALCULADO
                }

                if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
                    bool irBib = (devSel == DEV_ATRIO || devSel == DEV_MESTRE || devSel == DEV_MISSAO || devSel == DEV_LUCI || devSel == DEV_BLOCO3);
                    if (devSel == DEV_SALA || devSel == DEV_CORREDOR || irBib) {
                        jg->transicao.ativa = false;
                        jg->pausado = false;
                        jg->opcoesNaPausa = false;
                        jg->dlg.estado = DLG_NENHUM;
                    }
                    if (devSel == DEV_SALA)          { jg->tela = JOGANDO;  jg->playerPos = SPAWN_SALA_VOLTA; jg->direcaoAtual = DIR_FRENTE; }
                    else if (devSel == DEV_CORREDOR) { jg->tela = CORREDOR; jg->playerPos = SPAWN_CORREDOR;   jg->direcaoAtual = DIR_COSTAS; }
                    else if (irBib) {
                        jg->tela = BIBLIOTECA;
                        RogueIniciar(&jg->biblioteca, &jg->progBiblioteca);
                        jg->biblioteca.estabilidade = &jg->estabilidadeLuci;
                        if (devSel == DEV_MESTRE) RogueEntrarSalaMestre(&jg->biblioteca);
                        if (devSel == DEV_MISSAO) { jg->progBiblioteca.byteVisto = true; RogueComecarMissao(&jg->biblioteca); }
                        if (devSel == DEV_BLOCO3 || devSel == DEV_LUCI) {
                            // Como entrar pela porta do átrio: calcula o índice (o DEV pode forçar), enche a vida e o EP
                            jg->progBiblioteca.byteVisto = true;
                            jg->progBiblioteca.bloco3Liberado = true;
                            RogueEntrarBloco3(&jg->biblioteca, &jg->progresso);
                            if (devSel == DEV_LUCI) RogueEntrarSala(&jg->biblioteca, SALA_LUCI);
                        }
                        devAberto = false;
                    }
                    else if (devSel == DEV_ARMA && naBib)   RogueInstalarArma(&jg->biblioteca, (TipoArma)devArma, 0);
                    else if (devSel == DEV_INVENCIVEL)      g_devInvencivel = !g_devInvencivel;
                    else if (devSel == DEV_CURAR && naBib)  { jg->biblioteca.j.hp = RogueHpMax(&jg->biblioteca); jg->biblioteca.j.ep = RogueEpMax(&jg->biblioteca); }
                    else if (devSel == DEV_FC)              jg->progBiblioteca.fragmentos += 100;
                    else if (devSel == DEV_PONTOS)          jg->progresso.pontos += 100;
                    else if (devSel == DEV_INDICE)          g_devIndice = (g_devIndice < 0) ? INDICE_FINAL_BOM : -1;
                    else if (devSel == DEV_LIMPAR && naBib) {
                        for (int k = 0; k < MAX_INIMIGOS; k++)
                            if (jg->biblioteca.ini[k].ativo)
                                RogueFerirInimigo(&jg->biblioteca, &jg->progresso, &jg->biblioteca.ini[k], jg->biblioteca.ini[k].hp + 9999.0f, (Vector2){ 0, 0 });
                    }
                    else if (devSel == DEV_SOM)             DevTocarSom(devSom);
                    if (devSel == DEV_SALA || devSel == DEV_CORREDOR) devAberto = false;
                }

                if (devAberto) {
                    ShowCursor();
                    BeginTextureMode(jg->alvo);
                    DesenharDevMenu(devSel, devArma, devSom, naBib, jg->progresso.pontos);
                    EndTextureMode();
                    BeginDrawing();
                    DesenharNaJanela(jg->alvo, jg->filtroNitidez);
                    EndDrawing();
                    continue;
                }
            }
        }
#endif

        AjustarMouseAJanela();

        // Na Biblioteca a mira desenhada substitui o cursor do sistema
        jg->esconderCursor = (jg->tela == BIBLIOTECA && !jg->pausado && jg->biblioteca.tipoSala != SALA_ATRIO && !jg->biblioteca.lojaAberta && !jg->biblioteca.desafioAberto &&
                               !jg->biblioteca.escolhaAberta && !jg->biblioteca.eventoAberto && !jg->biblioteca.mapaAberto && !jg->biblioteca.lab.aberto &&
                               !jg->biblioteca.seletorAberto && !jg->biblioteca.morreu && !jg->biblioteca.resultadoAberto);
        if (jg->esconderCursor != IsCursorHidden()) {
            if (jg->esconderCursor) HideCursor();
            else                ShowCursor();
        }

        // =====================================================
        // ESC: ABRE / FECHA O PAUSE
        // =====================================================

        if (IsKeyPressed(KEY_ESCAPE)) {

            if (jg->pausado && jg->opcoesNaPausa) {

                // OPÇÕES dentro do pause: fecha a lista aberta, ou volta ao menu de pause
                if (jg->menuOpcoes.aberta >= 0) jg->menuOpcoes.aberta = -1;
                else                            jg->opcoesNaPausa = false;

            } else if (jg->pausado) {

                // Volta para o jogo (cancela também uma confirmação de saída pendente)
                jg->pausado = false;
                jg->historicoAberto = false;
                jg->confirmarSaida = 0.0f;
                UnloadTexture(jg->fundoPausa);
                jg->fundoPausa = (Texture2D){0};

            } else if (PodePausarJogo(jg)) {

                // Só pausa durante o jogo (no menu e na intro o ESC não faz nada)
                jg->capturarPausa = true;
                jg->selecaoPausa = PAUSA_CONTINUAR;
                jg->confirmarSaida = 0.0f;
                jg->historicoAberto = false;
                jg->opcoesNaPausa = false;

            } else if (jg->tela == OPCOES) {

                // Com uma lista suspensa aberta, o ESC só fecha a lista
                if (jg->menuOpcoes.aberta >= 0)
                    jg->menuOpcoes.aberta = -1;
                else
                    jg->tela = MENU;
            }
        }

        // =====================================================
        // R (ou clique no "R - falas da Luci" da caixa): HISTÓRICO DAS FALAS DA LUCI
        // Abre a mesma tela do menu de pause, com o jogo pausado. Não vale nas telas que
        // usam o R para outra coisa (a escolha de melhorias rerola com R).
        // =====================================================

        else if ((IsKeyPressed(TECLA_HISTORICO_LUCI) || jg->dlg.pedirHistorico) && !jg->pausado && !jg->capturarPausa) {

            const Biblioteca *bb = &jg->biblioteca;
            bool telaLivre = jg->tela != BIBLIOTECA ||
                             !(bb->escolhaAberta || bb->eventoAberto || bb->desafioAberto || bb->trocaArmaAberta || bb->resultadoAberto);

            if (PodePausarJogo(jg) && telaLivre && !jg->telaRanking.aberto) {
                jg->capturarPausa = true;
                jg->selecaoPausa = PAUSA_HISTORICO;
                jg->confirmarSaida = 0.0f;
                jg->historicoAberto = true;
                jg->historicoDireto = true;
                jg->opcoesNaPausa = false;
                jg->rolagemHistorico = 0.0f;
            }
        }

        jg->dlg.pedirHistorico = false;

        // =====================================================
        // ANIMAÇÃO DO TÍTULO
        // =====================================================

        if (!jg->pausado)
            jg->tempo += jg->dt;

        jg->amplitude = 8.0f;
        jg->velocidade = 3.0f;

        jg->y = 100.0f +
            sinf(jg->tempo * jg->velocidade) * jg->amplitude +
            sinf(jg->tempo * 2.1f) * 4.0f;

        jg->y1 = 100.0f +
            sinf((jg->tempo - 0.25f) * jg->velocidade) * jg->amplitude +
            sinf((jg->tempo - 0.25f) * 2.1f) * 4.0f;

        jg->y2 = 100.0f +
            sinf((jg->tempo - 0.45f) * jg->velocidade) * jg->amplitude +
            sinf((jg->tempo - 0.45f) * 2.1f) * 4.0f;

        // =====================================================
        // TRANSIÇÃO ENTRE TELAS (FADE)
        // =====================================================

        if (jg->transicao.ativa && !jg->pausado) {

            float passo = jg->dt / jg->transicao.duracao;

            if (jg->transicao.fase == 0) {

                // Escurecendo
                jg->transicao.alfa += passo;

                if (jg->transicao.alfa >= 1.0f) {

                    jg->transicao.alfa = 1.0f;

                    // Tela toda preta: troca de tela e posiciona o jogador
                    TelaAtual origem = jg->tela;
                    jg->tela = jg->transicao.destino;

                    if (jg->tela == CORREDOR && origem == JOGANDO) {

                        jg->playerPos = SPAWN_CORREDOR;
                        jg->direcaoAtual = DIR_COSTAS;
                        jg->pedirAutoSave = true;     // AUTO SAVE: entrou no corredor

                    } else if (jg->tela == JOGANDO && origem == CORREDOR) {

                        jg->playerPos = SPAWN_SALA_VOLTA;
                        jg->direcaoAtual = DIR_FRENTE;
                        jg->pedirAutoSave = true;     // AUTO SAVE: voltou para a sala

                    } else if (jg->tela == BIBLIOTECA && origem == CORREDOR) {

                        // Biblioteca dos Ecos: começa no átrio (ou na sala da ALUCINAÇÃO MESTRE, fim do Bloco 1)
                        RogueIniciar(&jg->biblioteca, &jg->progBiblioteca);
                        jg->biblioteca.estabilidade = &jg->estabilidadeLuci;
                        if (jg->entrarSalaMestre) {
                            jg->entrarSalaMestre = false;
                            RogueEntrarSalaMestre(&jg->biblioteca);
                        }
                        jg->playerPos = SPAWN_CORREDOR_VOLTA;
                        jg->direcaoAtual = DIR_ESQUERDA;
                        jg->pedirAutoSave = true;     // AUTO SAVE: entrou na Biblioteca dos Ecos (na sala da Mestre, salva no corredor)

                    } else if (jg->tela == CORREDOR && origem == BIBLIOTECA) {

                        jg->playerPos = SPAWN_CORREDOR_VOLTA;
                        jg->direcaoAtual = DIR_ESQUERDA;
                        jg->pedirAutoSave = true;     // AUTO SAVE: voltou para o corredor
                    }

                    jg->transicao.fase = 1;
                }

            } else {

                // Clareando
                jg->transicao.alfa -= passo;

                if (jg->transicao.alfa <= 0.0f) {

                    jg->transicao.alfa = 0.0f;
                    jg->transicao.ativa = false;
                }
            }
        }

        // =====================================================
        // LÓGICA
        // =====================================================

        // Guarda a posição para saber se o personagem andou neste frame
        jg->posInicioFrame = jg->playerPos;

        jg->mouse = GetMousePosition();

        jg->corBtnJogar = DARKGRAY;
        jg->corBtnOpcoes = DARKGRAY;
        jg->corBtnSair = DARKGRAY;
        jg->corBtnContinuar = DARKGRAY;

        // Com save, o CONTINUAR entra no topo e os outros botões descem
        jg->yBotoes = jg->existeSave ? 340.0f : 270.0f;
        jg->btnJogar.y  = jg->yBotoes;
        jg->btnOpcoes.y = jg->yBotoes + 70;
        jg->btnSair.y   = jg->yBotoes + 140;

        if (jg->confirmarSaida > 0.0f)
            jg->confirmarSaida -= jg->dt;

        // =====================================================
        // MENU DE PAUSE (enquanto pausado, o resto do jogo fica congelado:
        // personagem, diálogos, barra de tempo do quiz e batalha)
        // =====================================================

        // Histórico das falas da Luci (dentro do pause): fechar volta ao menu de pause,
        // ou direto ao jogo se ele foi aberto pelo R
        if (jg->pausado && jg->historicoAberto) {

            if (AtualizarHistoricoLuci(&jg->rolagemHistorico)) {

                jg->historicoAberto = false;

                if (jg->historicoDireto) {
                    jg->pausado = false;
                    UnloadTexture(jg->fundoPausa);
                    jg->fundoPausa = (Texture2D){0};
                }
            }
        }

        // OPÇÕES dentro do pause: o VOLTAR (ou o BACKSPACE) volta ao menu de pause
        else if (jg->pausado && jg->opcoesNaPausa) {

            MudancaOpcoes mudou = AtualizarOpcoes(&jg->menuOpcoes, &jg->exibicao);

            if (mudou == OPCOES_VIDEO)
                AplicarExibicao(jg->exibicao);
            if (mudou != OPCOES_NADA)
                SalvarConfigExibicao(jg->exibicao);

            if (IsKeyPressed(KEY_BACKSPACE)) {
                if (jg->menuOpcoes.aberta >= 0) jg->menuOpcoes.aberta = -1;
                else                            jg->menuOpcoes.sair = true;
            }

            if (jg->menuOpcoes.sair) {
                jg->menuOpcoes.sair = false;
                jg->opcoesNaPausa = false;
            }
        }

        else if (jg->pausado) {

            bool temLobby = (jg->tela == BIBLIOTECA) && RoguePodeAbandonar(&jg->biblioteca);
            int escolha = AtualizarPausa(&jg->selecaoPausa, temLobby);

            if (escolha != -1)
                jg->selecaoPausa = escolha;        // clique sem mexer o mouse também marca a opção

            // Qualquer outra coisa marcada ou escolhida cancela o pedido de confirmação
            if (jg->selecaoPausa != jg->opConfirmandoPausa || (escolha != -1 && escolha != jg->opConfirmandoPausa))
                jg->confirmarSaida = 0.0f;

            if (escolha == PAUSA_LOBBY) {

                if (jg->confirmarSaida <= 0.0f) {

                    // 1º clique/ENTER: só pede a confirmação (vale por 5 segundos)
                    jg->confirmarSaida = 5.0f;
                    jg->opConfirmandoPausa = PAUSA_LOBBY;

                } else {

                    jg->confirmarSaida = 0.0f;
                    RogueAbandonarRun(&jg->biblioteca);
                    jg->dlg = (SessaoDialogo){0};
                    jg->pedirAutoSave = true;

                    jg->pausado = false;
                    UnloadTexture(jg->fundoPausa);
                    jg->fundoPausa = (Texture2D){0};
                }

            } else if (escolha == PAUSA_HISTORICO) {

                jg->historicoAberto = true;
                jg->historicoDireto = false;
                jg->rolagemHistorico = 0.0f;

            } else if (escolha == PAUSA_OPCOES) {

                jg->opcoesNaPausa = true;
                jg->menuOpcoes = (MenuOpcoes){ .foco = FOCO_ABAS, .aberta = -1, .naPausa = true };

            } else if (escolha == PAUSA_CONTINUAR) {

                jg->pausado = false;
                UnloadTexture(jg->fundoPausa);
                jg->fundoPausa = (Texture2D){0};

            } else if (escolha == PAUSA_MENU) {

                // Salva antes de sair, para o CONTINUAR do menu voltar daqui
                jg->saveFalhou = !SalvarJogo(jg->nickname, TelaParaSalvar(jg->tela, jg->salaDaBatalhaB2, &jg->biblioteca), jg->playerPos, jg->direcaoAtual, jg->luciAcompanhando, &jg->progresso, jg->estabilidadeLuci, jg->ajudasPedidas, jg->chefeVencido, jg->idPartida, &jg->progBiblioteca);
                jg->existeSave = LerSave(&jg->saveLido);

                jg->pausado = false;
                UnloadTexture(jg->fundoPausa);
                jg->fundoPausa = (Texture2D){0};

                jg->tela = MENU;
                jg->dlg = (SessaoDialogo){0};
                jg->transicao = (Transicao){0};
                jg->telaRanking = (TelaRanking){0};
                jg->pedirAutoSave = false;
                jg->avisoSave = 0.0f;

            } else if (escolha == PAUSA_SAIR) {

                if (jg->confirmarSaida <= 0.0f) {

                    // 1º clique/ENTER: só pede a confirmação (vale por 5 segundos)
                    jg->confirmarSaida = 5.0f;
                    jg->opConfirmandoPausa = PAUSA_SAIR;

                } else {

                    // 2º clique/ENTER: o auto save do final do programa guarda o progresso
                    jg->confirmarSaida = 0.0f;
                    jg->fecharJogo = true;
                }
            }
        }

        // =====================================================
        // RANKING ABERTO (o jogo espera o jogador fechar)
        // =====================================================

        else if (jg->telaRanking.aberto) {

            bool fechou = AtualizarTelaRanking(&jg->telaRanking);

            // Fechou o ranking do fim do jogo: volta ao menu
            if (fechou && jg->voltarAoMenuDepoisDoRanking) {
                jg->voltarAoMenuDepoisDoRanking = false;
                IniciarTransicao(&jg->transicao, MENU, DURACAO_FADE);
            }

            // Fechou o ranking do fim do Bloco 1: a Luci avisa que a saída abriu
            if (fechou && jg->tela == CORREDOR && jg->chefeVencido && !jg->progresso.fimAnunciado) {

                jg->progresso.fimAnunciado = true;
                jg->pedirAutoSave = true;
                DialogoIniciar(&jg->dlg, &dialogoFimCorredor, OBJ_NENHUM);
            }
        }

        // =====================================================
        // MENU
        // =====================================================

        else if (jg->tela == MENU) {

            TelaMenuAtualizar(jg);

        }

        // =====================================================
        // INTRO
        // =====================================================

        else if (jg->tela == INTRO) {

            TelaIntroAtualizar(jg);

        }

        // =====================================================
        // JOGANDO (SALA INICIAL)
        // =====================================================

        else if (jg->tela == JOGANDO && !jg->transicao.ativa) {

            TelaSalaAtualizar(jg);

        }

        // =====================================================
        // CORREDOR DOS MONITORES
        // =====================================================

        else if (jg->tela == CORREDOR && !jg->transicao.ativa) {

            TelaCorredorAtualizar(jg);

        }

        // =====================================================
        // BATALHA (COMBATE / MINIGAME)
        // =====================================================

        else if (jg->tela == BATALHA && !jg->transicao.ativa) {

            TelaBatalhaAtualizar(jg);

        }

        // =====================================================
        // BATALHA DO BLOCO 1 (monitores e ALUCINAÇÃO MESTRE)
        // =====================================================

        else if (jg->tela == BATALHA_B2 && !jg->transicao.ativa) {

            TelaBatalhaB2Atualizar(jg);

        }

        // =====================================================
        // BLOCO 2 - BIBLIOTECA DOS ECOS
        // =====================================================

        else if (jg->tela == BIBLIOTECA && !jg->transicao.ativa) {

            TelaBibliotecaAtualizar(jg);

        }

        // =====================================================
        // OPÇÕES
        // =====================================================

        else if (jg->tela == OPCOES) {

            TelaOpcoesAtualizar(jg);

        }

        // =====================================================
        // AUTO SAVE (feito aqui, depois de toda a lógica do frame)
        // =====================================================

        if (jg->pedirAutoSave && (jg->tela == JOGANDO || jg->tela == CORREDOR || jg->tela == BIBLIOTECA)) {

            jg->pedirAutoSave = false;
            jg->saveFalhou = !SalvarJogo(jg->nickname, TelaParaSalvar(jg->tela, jg->salaDaBatalhaB2, &jg->biblioteca), jg->playerPos, jg->direcaoAtual, jg->luciAcompanhando, &jg->progresso, jg->estabilidadeLuci, jg->ajudasPedidas, jg->chefeVencido, jg->idPartida, &jg->progBiblioteca);

            if (!jg->saveFalhou)
                jg->existeSave = true;

            jg->avisoSave = TEMPO_AVISO_SAVE;
        }

        if (!jg->pausado) {
            if (jg->avisoSave > 0.0f) jg->avisoSave -= jg->dt;
        }

        // =====================================================
        // ANIMAÇÃO DO PERSONAGEM
        // =====================================================

        // Só anima se ele realmente se moveu (parado, em diálogo ou
        // empurrando a parede ele fica no quadro 0)
        jg->andou = (jg->tela == JOGANDO || jg->tela == CORREDOR) &&
                     !jg->transicao.ativa &&
                     Vector2Distance(jg->posInicioFrame, jg->playerPos) > 0.01f;

        // Só nas telas do Bloco 1: na Biblioteca quem anima o Thomas é o Bloco 2 (RogueAtualizarJogador).
        // Chamar aqui também dizia "parado" todo quadro e apagava a caminhada de lá.
        if (!jg->pausado && (jg->tela == JOGANDO || jg->tela == CORREDOR))
            AtualizarAnimacao(&jg->animJogador, jg->andou, jg->dt);

        // =====================================================
        // OBJETIVO E LUCI COMO GUIA
        // =====================================================

        jg->objetivo = ObjetivoAtual(jg->tela, &jg->progresso, jg->luciAcompanhando, jg->luciHitbox, jg->chefeVencido);

        // Luci companion: segue o Thomas, reage e mostra o caminho (ver LuciCompAtualizar)
        {
            bool comLuci = (jg->tela == JOGANDO && jg->luciAcompanhando) || jg->tela == CORREDOR ||
                           (jg->tela == BIBLIOTECA && !RogueNoBloco3(&jg->biblioteca));
            float dtLuci = jg->pausado ? 0.0f : jg->dt;

            if (!comLuci) {
                g_luciComp.iniciada = false;
                g_luciPedirDica = false;
            } else if (jg->tela == BIBLIOTECA) {
                const Biblioteca *bb = &jg->biblioteca;
                bool semArma = bb->tipoSala == SALA_ATRIO || (bb->tipoSala == SALA_MESTRE && bb->mestre.etapa == MT_ARMA);
                Vector2 alvoLuci = { 0 };
                bool temAlvo = RogueAlvoGuiaLuci(bb, &alvoLuci);
                RogueLuciCompObservar(bb);
                LuciCompAtualizar(bb->j.pos, semArma ? (Vector2){ 0 } : bb->j.mira, temAlvo, alvoLuci,
                                  bb->j.hp / fmaxf(1.0f, RogueHpMax(bb)), dtLuci * (1.0f - bb->lentidao));
            } else {
                bool guiaOk = jg->dlg.estado == DLG_NENHUM && !jg->cartao.ativo && !jg->transicao.ativa;
                g_luciGuiaSoParado = false;
                LuciCompAtualizar(jg->playerPos, (Vector2){ 0 }, jg->objetivo.temAlvo && guiaOk, jg->objetivo.alvo, 1.0f, dtLuci);
            }
        }

        jg->emExploracao = (jg->tela == JOGANDO || jg->tela == CORREDOR);
        jg->podeReagir = jg->emExploracao && jg->luciAcompanhando && !jg->transicao.ativa && !jg->pausado &&
                          jg->dlg.estado == DLG_NENHUM && !jg->cartao.ativo && !jg->telaRanking.aberto;

        if (jg->podeReagir && !jg->andou) jg->tempoParado += jg->dt;
        else                     jg->tempoParado = 0.0f;

        // Parado há 5 s: a Luci fica em dúvida (o objetivo continua escrito no topo)
        if (jg->tempoParado > 5.0f && jg->tempoParado - jg->dt <= 5.0f)
            MostrarEmocaoLuci(EMOCAO_DUVIDA, 2.5f);

        if (!jg->pausado && tempoEmocaoLuci > 0.0f)
            tempoEmocaoLuci -= jg->dt;

        instabilidadeLuci = 1.0f - (float)jg->estabilidadeLuci / ESTABILIDADE_MAX_LUCI;

        // Na Biblioteca, a Luci também se corrompe a cada desafio da tentativa
        if (jg->tela == BIBLIOTECA)
            instabilidadeLuci = fmaxf(instabilidadeLuci, RogueCorrupcaoLuci(&jg->biblioteca));
        brilhoLuci = 1.0f + 0.15f * (float)(ContarConcluidos(&jg->progresso));

        // A Luci brilha enquanto fala: página de diálogo com "Luci:" ou, na Biblioteca, o recado
        // dela e as falas do chefe. Parou de falar, volta ao sprite parado.
        AtualizarFalaLuci(DialogoFalaDaLuci(&jg->dlg) || (jg->tela == BIBLIOTECA && RogueLuciFalando(&jg->biblioteca)), jg->pausado ? 0.0f : jg->dt);

        // =====================================================
        // DESENHO
        // =====================================================

        BeginTextureMode(jg->alvo);

        // =====================================================
        // PAUSE
        // =====================================================

        if (jg->pausado && jg->historicoAberto) {

            DesenharHistoricoLuci(jg->fundoPausa, jg->rolagemHistorico, jg->historicoDireto);
        }

        else if (jg->pausado && jg->opcoesNaPausa) {

            DesenharFundoPausa(jg->fundoPausa);
            DesenharOpcoes(&jg->menuOpcoes, jg->exibicao, BLANK);
        }

        else if (jg->pausado) {

            DesenharPausa(jg->fundoPausa, jg->selecaoPausa, (jg->tela == BIBLIOTECA) && RoguePodeAbandonar(&jg->biblioteca),
                          (jg->confirmarSaida > 0.0f) ? jg->opConfirmandoPausa : -1);
            if (jg->tela == BIBLIOTECA)
                RogueDesenharControlesPausa(&jg->biblioteca);    // os controles da sala (saíram da faixa de baixo do HUD)
        }

        // =====================================================
        // MENU
        // =====================================================

        else if (jg->tela == MENU) {

            TelaMenuDesenhar(jg);

        }

        // =====================================================
        // INTRO CENTRALIZADA
        // =====================================================

        else if (jg->tela == INTRO) {

            TelaIntroDesenhar(jg);

        }

        // =====================================================
        // TELA JOGANDO (SALA INICIAL)
        // =====================================================

        else if (jg->tela == JOGANDO) {

            TelaSalaDesenhar(jg);

        }

        // =====================================================
        // TELA CORREDOR
        // =====================================================

        else if (jg->tela == CORREDOR) {

            TelaCorredorDesenhar(jg);

        }

        // =====================================================
        // TELA BATALHA
        // =====================================================

        else if (jg->tela == BATALHA) {

            TelaBatalhaDesenhar(jg);

        }

        // =====================================================
        // TELA BATALHA DO BLOCO 1 (monitores e ALUCINAÇÃO MESTRE)
        // =====================================================

        else if (jg->tela == BATALHA_B2) {

            TelaBatalhaB2Desenhar(jg);

        }

        // =====================================================
        // TELA BLOCO 2 - BIBLIOTECA DOS ECOS
        // =====================================================

        else if (jg->tela == BIBLIOTECA) {

            TelaBibliotecaDesenhar(jg);

        }

        // =====================================================
        // TELA OPÇÕES
        // =====================================================

        else if (jg->tela == OPCOES) {

            TelaOpcoesDesenhar(jg);

        }

        // =====================================================
        // FADE DA TRANSIÇÃO (sempre por cima de tudo)
        // =====================================================

        if (jg->transicao.ativa && !jg->pausado)
            DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(BLACK, jg->transicao.alfa));

        EndTextureMode();

        // Bloco 3: aberração cromática (canais R e B deslocados em ondas) sobre o quadro da Biblioteca
        if (jg->tela == BIBLIOTECA && !jg->pausado && !jg->transicao.ativa) {
            float aberracao = RogueIntensidadeAberracao(&jg->biblioteca) * ((jg->dlg.estado != DLG_NENHUM) ? 0.3f : 1.0f);
            if (aberracao > 0.01f && Prefs()->corrupcaoTela)       // (a opção CORRUPÇÃO NA TELA desliga)
                AplicarAberracaoCromatica(jg->alvo, aberracao, jg->tempo);
        }

        // Acabou de apertar ESC: a tela deste frame vira o fundo do pause
        if (jg->capturarPausa) {

            jg->fundoPausa = CapturarFundoPausa(jg->alvo);
            jg->capturarPausa = false;
            jg->pausado = true;
        }

        // Estica o quadro de 800x600 para o tamanho da janela (com os efeitos de tela)
        DefinirCorrupcaoTela(CorrupcaoDaTela(jg));
        BeginDrawing();
        DesenharNaJanela(jg->alvo, jg->filtroNitidez);
        EndDrawing();
    }

    // 3. FINALIZAÇÃO

    // AUTO SAVE: fechou a janela no meio do jogo
    // (no menu ou na intro não salva, para não apagar o progresso)
    if (jg->tela == JOGANDO || jg->tela == CORREDOR || jg->tela == BATALHA || jg->tela == BATALHA_B2 || jg->tela == BIBLIOTECA)
        SalvarJogo(jg->nickname, TelaParaSalvar(jg->tela, jg->salaDaBatalhaB2, &jg->biblioteca), jg->playerPos, jg->direcaoAtual, jg->luciAcompanhando, &jg->progresso, jg->estabilidadeLuci, jg->ajudasPedidas, jg->chefeVencido, jg->idPartida, &jg->progBiblioteca);

    // (as texturas precisam ser liberadas ANTES de fechar a janela)

    if (jg->texMapaSala.id > 0)
        UnloadTexture(jg->texMapaSala);

    if (jg->fundoPausa.id > 0)
        UnloadTexture(jg->fundoPausa);

    UnloadRenderTexture(jg->alvo);
    UnloadShader(jg->filtroNitidez.shader);

    UnloadTexture(jg->imagem_titulo);
    DescarregarLuci();
    DescarregarMapasNovos();
    DescarregarIconesBiblioteca();       // Bloco 2: ícones opcionais das salas
    DescarregarSpritesBiblioteca();      // Blocos 2 e 3: sprites dos inimigos e do chão
    DescarregarAberracao();              // Bloco 3: alvo do pós-efeito
    for (int i = 0; i < NUM_DIRECOES; i++)
        if (!jg->spritesJogador[i].emprestada) {
            UnloadTexture(jg->spritesJogador[i].textura);
            if (jg->spritesJogador[i].temParado) UnloadTexture(jg->spritesJogador[i].texParado);
        }

    for (int i = 0; i < 4; i++)
        if (jg->spritesMonitor[i].id > 0)
            UnloadTexture(jg->spritesMonitor[i]);

    EncerrarSons();
    DescarregarFonteJogo();
    TelemetriaFechar();
    CloseWindow();

    return 0;
    
}
