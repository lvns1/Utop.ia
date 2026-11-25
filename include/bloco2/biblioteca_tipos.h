// Bloco 2 (Biblioteca dos Ecos): tipos, constantes e tabelas de dados (inimigos, armas, itens, mapa, build, laboratórios, Silogista).
#ifndef UTOPIA_BIBLIOTECA_TIPOS_H
#define UTOPIA_BIBLIOTECA_TIPOS_H

#include <stdbool.h>
#include "raylib.h"
#include "raymath.h"
#include "../core/config.h"
#include "../core/tipos.h"
#include "../audio/audio.h"

// =============================================================
// BLOCO 2 - BIBLIOTECA DOS ECOS (roguelite de tiro, visão de cima)
//   ÁTRIO (a base): o PROFESSOR BYTE explica os fundamentos da IA generativa,
//   o BYTECO troca Fragmentos de Conhecimento por itens e a porta de cima
//   começa a MISSÃO: um MAPA PROCEDURAL RAMIFICADO, sorteado a cada tentativa
//   (ver RogueGerarMapa). Vencida a sala, as portas (em cima, à esquerda e/ou à
//   direita) mostram o tipo da próxima sala e a recompensa; o jogador escolhe
//   o caminho andando até uma delas. Tipos de sala:
//     COMBATE    - objetivos variados (eliminar em ondas, sobreviver, núcleos,
//                  alvo prioritário, mecanismo), com armadilhas
//     ELITE      - inimigos de aura dourada; recompensa rara
//     EDUCATIVA  - um terminal de conhecimento (dá Fragmentos de Conhecimento)
//     RECOMPENSA - escolha 1 entre 3 itens (pedestais)
//     EVENTO     - uma escolha com custo ou risco (alguns são educativos)
//     DESCANSO   - sala de leitura: vida, estabilidade da Luci ou estudo
//     DESAFIO    - prova opcional por uma melhoria rara
//     SECRETA    - atrás de uma parede corrompida (atirar nela abre a passagem)
//   Melhorias são escolhidas 1 de 3 (NÚCLEO DE MELHORIA) e formam a build da
//   tentativa, com sinergias. A CÂMARA DA INFERÊNCIA (mini-chefe O SILOGISTA)
//   fica no meio do mapa (todos os caminhos passam por ela), que termina na
//   sala do ALUCINADOR. M abre o mapa grande.
//   Morreu: volta ao átrio, perde 15 pontos e os itens da tentativa.
//   Os Fragmentos de Conhecimento nunca se perdem.
//
//   Controles: WASD anda; o mouse (ou as SETAS) mira; clique esquerdo (ou
//   segurar uma SETA) atira com o DISPARADOR DE DADOS; ESPAÇO esquiva; botão
//   direito (ou Q) solta o PULSO DE INTEGRIDADE; 1 = Antivírus; 2 = Pacote de
//   Dados; E = falar, ler e pegar.
//
//   Inimigos: FRAGMENTO ALUCINADO (persegue e dá o bote; às vezes deixa um
//   ECO FALSO que explode se ninguém checar) e GUARDIÃO DE DADOS (fica longe
//   e atira em leque, num ritmo previsível).
// =============================================================

// ---------- Protagonista (AJUSTE AQUI) ----------
#define ROGUE_HP_MAX             100.0f
#define ROGUE_EP_MAX             100.0f
#define ROGUE_VELOCIDADE         210.0f
#define ROGUE_EP_POR_SEGUNDO     4.0f
#define RAIO_CORPO_JOGADOR       13.0f
#define INVULNERAVEL_APOS_DANO   0.6f     // era 0.7 (versão mais desafiadora)

// Disparador de Dados
#define TIRO_DANO                10.0f
#define TIRO_INTERVALO           0.18f
#define TIRO_VELOCIDADE          540.0f
#define TIRO_RAIO                5.0f

// Esquiva
#define ESQUIVA_DISTANCIA        120.0f
#define ESQUIVA_DURACAO          0.20f
#define ESQUIVA_INVULNERAVEL     0.30f
#define ESQUIVA_RECARGA          0.70f

// Pulso de Integridade
#define PULSO_CUSTO_EP           35.0f
#define RAIO_CUSTO_EP            12.0f    // Raio de Processamento: cada disparo gasta EP (a "mana" do Thomas) (era 11)
#define RAIOC_CUSTO_EP           13.0f    // Raio Clássico (versão antiga do Raio de Processamento): gasto de EP por tiro (era 16: secava rápido demais)
#define RAIOC_SUPER_FATOR        0.55f
#define RAIO_SUPER_FATOR         0.65f    // Supercondutor: o sigilo gasta 35% menos EP
#define RAIO_EP_RETORNO          2.5f     // EP devolvido por inimigo atingido pela coluna de luz... (era 4)
#define RAIO_EP_RETORNO_MAX      2        // ...contando no máximo 2 inimigos por sigilo (era 3: 3 x 4 = 12 > 11 de gasto, sigilo de graça)
#define RAIO_RESIDUO_DPS         10.0f    // dano por segundo da luz residual que fica no chão
#define RAIO_RESIDUO_TEMPO       1.8f     // quanto tempo a luz residual dura
#define PULSO_RECARGA            4.0f
#define PULSO_RAIO               130.0f
#define PULSO_DANO               25.0f

// ---------- Inimigos (AJUSTE AQUI) ----------
#define FRAGMENTO_HP             30.0f
#define FRAGMENTO_VELOCIDADE     115.0f
#define FRAGMENTO_RAIO           14.0f
#define FRAGMENTO_DANO           12.0f
#define FRAGMENTO_ALCANCE_BOTE   80.0f
#define FRAGMENTO_PREPARO        0.35f    // tremendo antes do bote (dá para fugir)
#define FRAGMENTO_BOTE_VEL       420.0f
#define FRAGMENTO_BOTE_DURACAO   0.25f
#define FRAGMENTO_RECARGA        1.0f
#define CHANCE_ECO_FALSO         35       // % dos fragmentos que deixam um eco ao morrer
#define ECO_TEMPO                1.1f     // depois disso o eco explode em balas
#define ECO_HP                   6.0f
#define ECO_BALAS                6

#define GUARDIAO_HP              60.0f
#define GUARDIAO_VELOCIDADE      80.0f
#define GUARDIAO_RAIO            18.0f
#define GUARDIAO_DIST_MIN        190.0f
#define GUARDIAO_DIST_MAX        320.0f
#define GUARDIAO_INTERVALO       1.7f
#define GUARDIAO_PREPARO         0.45f    // brilha antes de atirar
#define GUARDIAO_TIRO_VEL        210.0f
#define GUARDIAO_TIRO_DANO       10.0f
#define GUARDIAO_LEQUE           3
#define GUARDIAO_ABERTURA        14.0f    // graus entre as balas do leque

#define TEMPO_SURGINDO           0.6f     // inimigo se materializando (não ataca)

// ---------- Inimigos da expansão (AJUSTE AQUI) ----------
// Vida, raio, velocidade, dano de contato e pontos de cada um ficam na tabela
// DEF_INIMIGOS; aqui ficam os tempos e as habilidades.

// SPAMMER: enxame rápido e frágil. Ao morrer, pode soltar 2 fragmentos de spam.
#define SPAMMER_CHANCE_DIVIDIR   40       // % de soltar os fragmentos de spam
#define SPAM_VIDA                2.5f     // o fragmento de spam some sozinho depois disso

// PHISHER: camuflado como um arquivo inofensivo; revela-se, dá o bote e recua.
#define PHISHER_ALCANCE          150.0f   // o jogador chegou perto: ele se revela
#define PHISHER_PREPARO          0.45f    // revelado, mirando (dá para fugir)
#define PHISHER_BOTE_VEL         470.0f
#define PHISHER_BOTE_DURACAO     0.25f
#define PHISHER_RECUO            0.9f     // depois do bote, recua
#define PHISHER_RECARGA          1.8f     // até poder se camuflar e atacar de novo
#define ISCA_DURACAO             4.0f     // a cópia falsa some sozinha
#define ISCA_RECARGA             6.0f     // intervalo entre duas cópias

// FIREWALL: lento e resistente; a frente bloqueia, as costas são o ponto fraco.
#define FIREWALL_ZONA            120.0f   // raio do escudo de rede
#define FIREWALL_PROTECAO        0.40f    // inimigos na zona levam 40% menos dano
#define FIREWALL_FRENTE          0.25f    // tiro de frente: só 25% do dano
#define FIREWALL_COSTAS          1.35f    // tiro pelas costas: +35%
#define FIREWALL_ANGULO_FRENTE   65.0f    // graus para cada lado contados como "frente"
#define FIREWALL_GIRO            1.3f     // radianos por segundo para virar para o jogador
#define FIREWALL_INTERVALO       5.0f     // entre duas barreiras de energia
#define FIREWALL_AVISO_BARREIRA  0.6f
#define FIREWALL_BARREIRA        2.5f     // a barreira na frente dele segura os tiros
#define FIREWALL_BARREIRA_LARGURA 90.0f

// OVERCLOCK: carrega energia e dispara uma investida; depois fica superaquecido.
#define OVERCLOCK_CARGA_MIN      0.7f     // carga sorteada entre o mínimo e o máximo
#define OVERCLOCK_CARGA_MAX      1.6f
#define OVERCLOCK_INVESTIDA_MIN  380.0f   // velocidade com carga mínima ...
#define OVERCLOCK_INVESTIDA_MAX  680.0f   // ... e com carga máxima
#define OVERCLOCK_INVESTIDA_DURACAO 0.35f
#define OVERCLOCK_ESFRIAR_MIN    0.8f     // superaquecido: mais carga = mais tempo vulnerável
#define OVERCLOCK_ESFRIAR_MAX    2.2f
#define OVERCLOCK_DANO_CARREGANDO 1.3f    // multiplicador de dano recebido carregando
#define OVERCLOCK_DANO_QUENTE    2.0f     // ... e superaquecido (com carga máxima)
#define OVERCLOCK_RECARGA        1.2f
#define OVERCLOCK_ALCANCE        300.0f   // começa a carregar a esta distância

// BOTNET: suporte. Conecta até 3 inimigos e acelera os conectados.
#define BOTNET_CONEXOES          3
#define BOTNET_ALCANCE           260.0f
#define BOTNET_BONUS_VELOCIDADE  1.25f
#define BOTNET_BONUS_RITMO       1.35f    // as recargas dos conectados passam mais rápido
#define BOTNET_DIST_MIN          230.0f
#define BOTNET_DIST_MAX          360.0f
#define BOTNET_INTERVALO         3.2f
#define BOTNET_PREPARO           0.5f

// KEYLOGGER: lê a direção do jogador e atira onde ele vai estar.
#define KEYLOGGER_LEITURA        0.5f     // andando reto por mais tempo que isso, ele começa a prever
#define KEYLOGGER_PREVISAO_MAX   1.0f     // segundos à frente, no máximo
#define KEYLOGGER_INTERVALO      2.2f
#define KEYLOGGER_PREPARO        0.6f     // a mira aparece no chão antes dos tiros
#define KEYLOGGER_RAJADA         3
#define KEYLOGGER_TIRO_VEL       300.0f
#define KEYLOGGER_TIRO_DANO      6.0f
#define KEYLOGGER_DIST_MIN       220.0f
#define KEYLOGGER_DIST_MAX       340.0f

// CACHE: guarda energia ao levar dano e se cura se ficar sem levar dano.
#define CACHE_ARMAZENA           0.5f     // metade do dano recebido vira energia guardada
#define CACHE_ENERGIA_MAX        35.0f
#define CACHE_ESPERA             2.0f     // sem levar dano por esse tempo, começa a recuperar
#define CACHE_ESPERA_PROTEGIDO   1.2f     // ... mais rápido dentro do escudo de um Firewall
#define CACHE_RECUPERAR          1.0f     // a recuperação leva esse tempo (um tiro interrompe)
#define CACHE_INTERVALO          3.6f
#define CACHE_PREPARO            0.6f
#define CACHE_BALAS              6

// Dano dos ataques novos
#define PHISHER_DANO             14.0f    // só o bote do Phisher verdadeiro machuca
#define OVERCLOCK_DANO_INVESTIDA 14.0f
#define BOTNET_TIRO_DANO         6.0f
#define CACHE_TIRO_DANO          7.0f

// ---------- Missão, recompensas e itens (AJUSTE AQUI) ----------
// (Reformulação: a rota linear SALAS_POR_MISSAO/POSICAO_SILOGISTA virou o mapa procedural; ver MAPA_PROFUNDIDADES)
#define FC_POR_TERMINAL          3        // Fragmentos de Conhecimento por terminal lido
#define CHANCE_FC_FRAGMENTO      30       // % de soltar um fragmento ao derrotar um Fragmento Alucinado (era 40)
#define CHANCE_FC_GUARDIAO       50       // ... e um Guardião de Dados (era 70)
#define CHANCE_ANTIVIRUS         6        // % de soltar um Antivírus
#define RAIO_IMA                 90.0f    // fragmentos voam até o jogador quando ele chega perto
#define CURA_ANTIVIRUS           40.0f
#define RECARGA_PACOTE           50.0f
#define BONUS_PROCESSADOR        0.25f    // +25% de dano por Processador Neural
#define REDUCAO_FIREWALL         0.25f    // -25% de dano recebido por Firewall
#define MAX_PILHA_PASSIVO        2        // no máximo 2 de cada item passivo

// Melhorias permanentes (Byteco). Não se perdem ao morrer.
#define MELHORIA_HP              20.0f    // +HP máximo por nível
#define MELHORIA_DANO            0.15f    // +15% de dano por nível
#define MELHORIA_EP              20.0f    // +EP máximo por nível
#define NIVEL_MAX_MELHORIA       3

// ---------- Raridade (vale para itens, upgrades e armas) (AJUSTE AQUI) ----------
typedef enum { RARIDADE_COMUM, RARIDADE_INCOMUM, RARIDADE_RARA, NUM_RARIDADES } Raridade;

// Onde aparecem: os temporários caem dos inimigos; os upgrades passivos ficam
// nos pedestais e às vezes no fim de um combate; as armas nos pedestais e na loja.
#define CHANCE_TEMPORARIO        7        // % de um inimigo derrotado soltar um item temporário
#define CHANCE_UPGRADE_SALA      20       // % de um combate vencido deixar um upgrade passivo (era 40: agora há a escolha 1 de 3)
#define CHANCE_ARMA_PEDESTAL     45       // % de um dos pedestais trazer uma arma
#define ARMAS_NA_LOJA            3        // armas à venda no Byteco (sorteadas a cada volta ao átrio)

// Itens temporários (valem na hora em que são pegos; pegar de novo só renova o tempo)
#define EFEITO_OVERCLOCK_DURACAO 8.0f
#define EFEITO_OVERCLOCK_VELOCIDADE 0.15f
#define EFEITO_OVERCLOCK_CADENCIA 0.20f
#define ESCUDO_PORTATIL_DURACAO  12.0f    // absorve o próximo golpe (um escudo por vez)
#define LIMPEZA_RAIO             220.0f   // inimigos comuns nesse raio ficam atordoados
#define LIMPEZA_ATORDOA          0.8f
#define PROXY_DURACAO            5.0f
#define PROXY_HP                 40.0f
#define PACOTE_INTEGRIDADE_CURA  0.25f    // 25% da vida máxima
#define LATENCIA_DURACAO         5.0f
#define LATENCIA_RAIO            140.0f
#define LATENCIA_LENTIDAO        0.30f    // inimigos no campo andam 30% mais devagar

// Upgrades passivos (o limite de cada um fica na tabela ITENS)
#define NUCLEO_HP                0.10f    // +10% de vida máxima
#define BUFFER_CADENCIA          0.10f    // +10% de cadência
#define ROTEAMENTO_VELOCIDADE    0.08f
#define ROTEAMENTO_ESQUIVA       0.08f    // -8% na recarga da esquiva
#define FRAGMENTACAO_A_CADA      6        // a cada 6 disparos, o próximo acerto se divide
#define FRAGMENTACAO_PEDACOS     3
#define FRAGMENTACAO_DANO        0.35f
#define FRAGMENTACAO_ALCANCE     220.0f
#define FIREWALL_ADAPTATIVO      0.08f    // -8% de dano recebido (até 3 = 24%)
#define FATOR_DANO_MINIMO        0.30f    // nenhuma defesa passa disso: sempre leva 30% do dano
#define ANALISADOR_MARCAS        3
#define ANALISADOR_BONUS         0.25f
#define ANALISADOR_TEMPO         2.5f     // sem acertar o alvo marcado por esse tempo, as marcas somem
#define BOTNET_REVERSO_CHANCE    35
#define BOTNET_REVERSO_DANO      0.30f    // 30% do dano-base da arma atual
#define BOTNET_REVERSO_RECARGA   0.8f
#define CACHE_PERSISTENTE_SALAS  3
#define CACHE_PERSISTENTE_CURA   0.05f

// Armas (os números de cada uma ficam em DEF_ARMAS; aqui, as particularidades)
#define VETOR_CARGA_MIN          0.15f    // toque rápido: tiro fraco
#define VETOR_CARGA_MAX          1.2f
#define VETOR_DANO_MAX           60.0f    // com a carga cheia
#define VETOR_ATRAVESSA          2        // atravessa 2 inimigos (acerta até 3)
#define ROTEADOR_AQUISICAO       260.0f   // procura alvos até essa distância
#define ROTEADOR_GIRO            7.0f     // radianos por segundo
#define ROTEADOR_VIDA            1.8f
#define ROTEADOR_RICOCHETES      2
#define ROTEADOR_RICOCHETE_RAIO  200.0f
#define ROTEADOR_PERDA           0.70f    // cada ricochete tira 30% do dano
#define FLUXO_CALOR_TIRO         0.035f   // ~29 tiros seguidos superaquecem
#define FLUXO_ESFRIA             0.6f     // por segundo, sem atirar
#define FLUXO_ESFRIA_TRAVADO     0.8f     // superaquecida: só volta a atirar fria
#define ORBE_RAIO_EXPLOSAO       70.0f
#define ORBE_DANO_EXPLOSAO       14.0f
#define FRAGMENTADORA_EMPURRAO   1.8f
#define PULSO_FW_EMPURRAO        320.0f

// Desafios educativos (Guardião selado)
#define RAIO_DESAFIO             170.0f   // chegando perto do selo, o desafio abre
#define PONTOS_DESAFIO           5
#define FC_POR_DESAFIO           1
#define DANO_SELO_CERTO          0.5f     // acertou: o guardião perde metade da vida
#define ATORDOADO_SELO           2.5f     // ... e fica parado esse tempo
#define TEMPO_RESPOSTA_SELO      15.0f    // segundos para responder o desafio do Guardião (esgotou = erro)
#define BALAS_ATAQUE_ESPECIAL    14       // errou: anel de balas
#define ECOS_PARA_REPARO         3        // ecos falsos destruídos para +1 de estabilidade
#define BONUS_CONFIAR_PONTOS     3        // confiou na dica certa sem verificar
#define BONUS_CONFIAR_FC         1
#define CUSTO_VERIFICAR_FC       1        // verificar sem Lente custa 1 Fragmento
#define HABITO_LUCI              3        // decisões seguidas iguais: a Luci comenta
// Scanner de Ecos (H fora dos desafios)
#define SCANNER_RECARGA          6.0f
#define SCANNER_DURACAO          4.0f
#define SCANNER_CUSTO_EP         10.0f
#define SCANNER_CHANCE_LUCI_ECO  15       // % (estabilidade 3 ou menos): por um instante, marca a própria Luci como ECO
// Contradições (C contesta)
#define CONTRADICAO_INTERVALO_MIN 18.0f
#define CONTRADICAO_INTERVALO_MAX 35.0f
#define CONTRADICAO_DURACAO      6.0f
#define CONTRADICAO_ESTAB_MAX    3        // só acontecem com a estabilidade em 3 ou menos
#define CONTRADICAO_FC           2
#define CONTESTAR_A_TOA_PONTOS   2

// ---------- Pontos (somam com os do Bloco 1) ----------
#define PONTOS_FRAGMENTO         1
#define PONTOS_GUARDIAO          2
#define PONTOS_SALA_ROGUE        5
#define PONTOS_TERMINAL          5

#define MAX_PROJETEIS            160
#define MAX_INIMIGOS             48       // enxames de Spammers + fragmentos de spam
#define MAX_PARTICULAS_ROGUE     200
#define MAX_OBSTACULOS           14       // até 10 estantes da sala + as barreiras das armadilhas
#define MAX_COLETAVEIS           32
#define NUM_PEDESTAIS            3

// Conversas da Biblioteca (não colidem com os monitores nem com OBJ_*)
#define OBJ_BYTE                 -10
#define OBJ_BYTECO               -11
#define OBJ_TERMINAL             -12
#define OBJ_CHEFE_INTRO          -13
#define OBJ_ALUCINADOR_VITORIA    -15
#define OBJ_ENCICLOPEDIA         -14
#define OBJ_SILOGISTA_INTRO      -16
#define OBJ_SILOGISTA_ETAPA      -17      // explicação entre um puzzle e outro
#define OBJ_SILOGISTA_VITORIA    -18
#define OBJ_LUCI_INTRO           -19      // Bloco 3: a revelação
#define OBJ_LUCI_FINAL           -20
#define OBJ_REGISTRO             -21      // registros de memória do corredor do núcleo
#define OBJ_REGISTRO_SALA        -22      // reformulação: arquivo de ameaça ou memória numa sala da missão
#define OBJ_MESTRE_ARMA          -23      // fim do Bloco 1: pegou o Disparador de Dados no pedestal
#define OBJ_MESTRE_VITORIA       -24      // fim do Bloco 1: venceu a ALUCINAÇÃO MESTRE
#define OBJ_ATALHO_BLOCO2        -25      // terminal do corredor: confirmação do atalho para o Bloco 2
#define OBJ_ATALHO_BLOCO3        -26      // terminal do átrio: confirmação do atalho para o Bloco 3

// Arena (o HUD fica embaixo, o objetivo em cima)
// Arena das salas da Biblioteca. Todas usam a de sempre (ARENA_ROGUE_PADRAO), menos a luta
// final da Luci (SALA_LUCI), que fica dentro do chão do mapa quadrado do Bloco 3. Quem precisa
// dos limites pergunta ao ArenaRogue(); a troca acontece ao entrar na sala (RogueLimparSala).
#define RET_ARENA_ROGUE_PADRAO  { 40, 60, 720, 470 }

// ---- ARENA FINAL DO BLOCO 3: arte "mapa3_luci.png" (1026 x 1026, quadrada) ----
// Reduzida no carregamento para MAPA_LUCI_LADO, SEM deformar: centrada na tela e com a base
// encostada no HUD de baixo (só a fileira de tijolos do topo da imagem fica fora da tela).
// O chão da arte (entre as paredes) vira a arena da luta. Tudo é medido NA IMAGEM.
#define ARQUIVO_MAPA_LUCI    "resources/maps/mapa3_luci.png"
#define TAM_IMG_MAPA_LUCI    1026.0f
#define MAPA_LUCI_LADO        574.0f
#define MAPA_LUCI_X          (400.0f - MAPA_LUCI_LADO / 2.0f)     // 113
#define MAPA_LUCI_Y          (538.0f - MAPA_LUCI_LADO)            // -36 (o HUD de baixo começa em 538)
#define LUCI_MX(px)  (MAPA_LUCI_X + (px) * MAPA_LUCI_LADO / TAM_IMG_MAPA_LUCI)
#define LUCI_MY(py)  (MAPA_LUCI_Y + (py) * MAPA_LUCI_LADO / TAM_IMG_MAPA_LUCI)
#define LUCI_MT(p)   ((p) * MAPA_LUCI_LADO / TAM_IMG_MAPA_LUCI)
#define RAIO_TERMINAL_ATALHO     50.0f
#define SELETOR_TEMPO_RETORNO    2.4f     // vitória no seletor: segundos até voltar ao átrio

typedef enum { CHEFE_SEL_SENTINELA, CHEFE_SEL_RETRO, CHEFE_SEL_JANELA, CHEFE_SEL_SILOGISTA, CHEFE_SEL_ALUCINADOR, NUM_CHEFES_SELETOR } ChefeSeletor;

// Os tipos novos (reformulação do Bloco 2) ficam no FINAL: os valores antigos não mudam
typedef enum { SALA_ATRIO, SALA_COMBATE, SALA_EDUCATIVA, SALA_RECOMPENSA, SALA_CHEFE, SALA_SILOGISTA, SALA_TRANSICAO, SALA_LUCI,
               SALA_ELITE, SALA_EVENTO, SALA_DESCANSO, SALA_DESAFIO, SALA_SECRETA,
               SALA_LABORATORIO,    // expansão educativa: os cinco laboratórios (a categoria fica no nó do mapa)
               SALA_ESPECIAL,       // expansão da progressão: elites e mini-chefe obrigatórios (o tipo fica no nó)
               SALA_MESTRE,         // fim do Bloco 1: a arma, o tutorial de combate e a ALUCINAÇÃO MESTRE
               NUM_TIPOS_SALA } TipoSalaRogue;

// =============================================================
// EXPANSÃO DA PROGRESSÃO: OS ENCONTROS ESPECIAIS DO BLOCO 2
// =============================================================
// Três encontros obrigatórios (cada um numa camada de largura 1 do mapa, que todo caminho cruza):
//   SENTINELA DE ATENÇÃO (elite A) só ataca o que VÊ. Com linha de visão, o foco enche e vira um
//       feixe que atravessa a sala até bater numa estante. Estantes quebram a visão; perder a visão
//       a deixa DISTRAÍDA e, depois do feixe, ela RECALCULA: nos dois casos leva dano dobrado.
//   RETROPROPAGADOR (elite B) guarda de ONDE veio cada golpe que leva (marcas no chão) e, na volta,
//       devolve um projétil para cada marca: o erro volta pelo caminho. Atirar parado é punido;
//       atirar e trocar de lugar, não. Depois da volta ele RECALIBRA e fica vulnerável.
//   JANELA DE CONTEXTO (mini-chefe) tem uma janela de contexto visível (barra no topo). A instrução
//       PROTEGER, no começo da janela, mantém o escudo. Golpes no escudo viram RUÍDO e ocupam espaço;
//       os dados dourados que voam até ela são o que ela sabe sobre a posição do jogador. Passando do
//       limite, o mais antigo é esquecido: sem PROTEGER, o escudo cai; sem dados, a mira se perde.

#define SENTINELA_HP             420.0f   // era 260
#define SENTINELA_RAIO           19.0f
#define SENTINELA_VELOCIDADE     70.0f
#define SENTINELA_FOCO_TEMPO     1.7f     // segundos com linha de visão até travar
#define SENTINELA_PERDE_FOCO     0.9f     // foco perdido por segundo sem visão
#define SENTINELA_TRAVA          0.55f    // aviso: a direção do feixe já não muda
#define SENTINELA_FEIXE          0.30f
#define SENTINELA_FEIXE_LARGURA  16.0f
#define SENTINELA_FEIXE_DANO     20.0f
#define SENTINELA_RECALCULA      1.7f     // depois do feixe: vulnerável
#define SENTINELA_DISTRAIDA      1.4f     // depois de perder a visão: vulnerável
#define SENTINELA_INTERVALO      0.8f
#define SENTINELA_DANO_BLINDADA  0.55f    // focada: a lente protege
#define SENTINELA_DANO_VULNERAVEL 2.0f

#define RETRO_HP                 480.0f   // era 300
#define RETRO_RAIO               21.0f
#define RETRO_VELOCIDADE         80.0f
#define RETRO_FORWARD            4.2f     // passagem "para frente": registra os golpes
#define RETRO_AVISO              0.9f     // as linhas até as marcas acendem
#define RETRO_RECALIBRA          1.8f     // depois da volta: vulnerável
#define RETRO_MAX_REGISTROS      10
#define RETRO_REGISTRO_INTERVALO 0.14f    // um registro por rajada (tiros múltiplos não lotam)
#define RETRO_TIRO_VEL           235.0f
#define RETRO_TIRO_DANO          8.0f
#define RETRO_DANO_RECALIBRANDO  1.7f

#define JANELA_HP                1500.0f  // era 1000
#define JANELA_RAIO              30.0f
#define JANELA_MAX_SLOTS         9
#define JANELA_DANO_POR_RUIDO    40.0f    // dano batendo no escudo que vira 1 bloco de ruído
#define JANELA_VULNERAVEL        4.0f
#define JANELA_DANO_VULNERAVEL   1.25f
#define JANELA_FASE2             0.66f
#define JANELA_FASE3             0.33f
#define JANELA_FASE4             0.15f    // SOBRECARGA
#define TOKEN_RAIO               9.0f

// Encontros especiais (o tipo fica no nó do mapa)
enum { ESP_SENTINELA, ESP_RETRO, ESP_JANELA, NUM_ESPECIAIS };

typedef enum {
    INIMIGO_FRAGMENTO, INIMIGO_GUARDIAO, INIMIGO_ECO,
    INIMIGO_SPAMMER, INIMIGO_SPAM, INIMIGO_PHISHER, INIMIGO_ISCA, INIMIGO_FIREWALL,
    INIMIGO_OVERCLOCK, INIMIGO_BOTNET, INIMIGO_KEYLOGGER, INIMIGO_CACHE,
    INIMIGO_NUCLEO,             // reformulação: núcleo de corrupção (objetivo "destrua os núcleos")
    INIMIGO_SENTINELA,          // expansão da progressão: elite A (linha de visão)
    INIMIGO_RETRO,              // expansão da progressão: elite B (devolve os golpes)
    INIMIGO_JANELA,             // expansão da progressão: mini-chefe (janela de contexto)
    INIMIGO_TOKEN,              // dado dourado que voa até a Janela
    INIMIGO_ALUCINACAO,         // ajudante da Janela: fantasma que some e reaparece atirando
    INIMIGO_VERBOSO,            // ajudante da Janela: balão lento que "fala demais" (anéis de balas)
    NUM_TIPOS_INIMIGO
} TipoInimigo;

// O que é igual para todo inimigo de um tipo (o comportamento fica em RogueAtualizarInimigos)
typedef struct {
    const char *nome;
    float hp, raio, velocidade;
    float danoContato;          // 0 = não machuca encostando (ou só em certos estados)
    float empurrao;             // quanto os tiros o empurram (1 = normal)
    int pontos;
    int chanceFC;               // % de soltar um Fragmento de Conhecimento
    Color cor;                  // partículas de dano e de morte
    float tomSom;               // som da morte (cada um com um tom)
} DefInimigo;

typedef enum {
    ITEM_ANTIVIRUS, ITEM_PACOTE_DADOS, ITEM_LENTE, ITEM_FIREWALL, ITEM_PROCESSADOR, ITEM_MANUAL,
    // Temporários: valem na hora em que são pegos (caem dos inimigos)
    ITEM_OVERCLOCK, ITEM_FIREWALL_PORTATIL, ITEM_LIMPEZA_CACHE, ITEM_PROXY, ITEM_PACOTE_INTEGRIDADE, ITEM_CAMPO_LATENCIA,
    // Upgrades passivos: valem até o fim da tentativa
    ITEM_NUCLEO, ITEM_BUFFER, ITEM_ROTEAMENTO, ITEM_FRAGMENTACAO, ITEM_FIREWALL_ADAPTATIVO, ITEM_ANALISADOR,
    ITEM_BOTNET_REVERSO, ITEM_CACHE_PERSISTENTE,
    NUM_ITENS
} TipoItem;

#define NUM_ITENS_LOJA      6       // os 6 primeiros ficam na loja do Byteco e na mochila de baixo
#define PRIMEIRO_UPGRADE    ITEM_NUCLEO

typedef struct {
    const char *nome;
    const char *efeito;
    int preco;                  // em Fragmentos de Conhecimento (loja do Byteco)
    bool passivo;               // passivo = vale enquanto estiver na mochila (até maxPilha)
    bool temporario;            // temporário = vale na hora em que é pego
    int maxPilha;
    Raridade raridade;
} DefItem;

// ---------- Armas ----------
typedef enum {
    ARMA_DISPARADOR, ARMA_VETOR, ARMA_ROTEADOR, ARMA_FLUXO, ARMA_ORBE, ARMA_FRAGMENTADORA, ARMA_PULSO_FW, ARMA_KATANA, ARMA_RAIO, ARMA_RAIO_CLASSICO, NUM_ARMAS
} TipoArma;

typedef struct {
    const char *nome;
    const char *efeito;
    int alcanceTipo;            // índice em NOMES_ALCANCE
    float dano;                 // por projétil (o Vetor vai deste valor até VETOR_DANO_MAX)
    float intervalo;            // entre disparos
    float intervaloMin;         // limite da cadência (nem Overclock nem Buffer passam disso)
    float velocidade, raio;
    float alcance;              // distância máxima do projétil (0 = até a parede; Pulso: raio da onda)
    int projeteis;
    float abertura;             // graus: espalhamento (ou o cone da Fragmentadora)
    Raridade raridade;
    int preco;                  // na loja do Byteco
    Color cor;
} DefArma;

// Katana de Bytes: o alcance do GOLPE (o "alcance" da tabela é o da onda de corte do aprimoramento)
#define KATANA_ALCANCE           78.0f
typedef char ChecaSonsArmas[(NUM_SONS_ARMA == NUM_ARMAS) ? 1 : -1];   // um som por arma (Sons 2.0): ao criar arma nova, crie a receita


// =============================================================
// REFORMULAÇÃO DO BLOCO 2: BUILD (melhorias escolhidas 1 de 3)
//   Ao vencer certas salas aparece um NÚCLEO DE MELHORIA: tocar nele (E)
//   mostra 3 opções sorteadas pela raridade e pela build atual; o jogador
//   escolhe UMA. As melhorias valem até o fim da tentativa (somem ao morrer,
//   como a mochila). Cada efeito é aplicado nas funções de atributo
//   (RogueFatorDano, RogueHpMax...) ou no ponto exato da mecânica.
//   SINERGIAS só aparecem quando as duas melhorias que elas combinam já estão
//   na build. APRIMORAMENTOS de arma só aparecem com a arma equipada.
// =============================================================
typedef enum {
    // Atributos
    UP_DANO, UP_VELOCIDADE, UP_ESQUIVA, UP_VIDA, UP_CADENCIA, UP_ALCANCE,
    // Disparos (valem para qualquer arma)
    UP_RICOCHETE, UP_PERFURAR, UP_EXPLOSAO, UP_RAJADA, UP_RASTRO, UP_POS_ESQUIVA,
    // Habilidades
    UP_PULSO, UP_ESQUIVA_EXPLOSIVA, UP_ENERGIA_ABATE, UP_ESCUDO_ELITE, UP_DUPLICAR, UP_ROTULAR, UP_VERIFICACAO, UP_CRITICO,
    // Especiais (raras: mudam uma mecânica)
    UP_ESQUIVA_DUPLA, UP_PULSO_REFLEXIVO,
    // Aprimoramentos de arma (só aparecem com a arma)
    UP_ARMA_DISPARADOR, UP_ARMA_VETOR, UP_ARMA_ROTEADOR, UP_ARMA_FLUXO, UP_ARMA_ORBE, UP_ARMA_FRAGMENTADORA, UP_ARMA_PULSO_FW,
    // Armas: segunda leva de aprimoramentos (cada arma com um tema)
    UP_ARMA_VETOR_LENTE, UP_ARMA_VETOR_ESTAMPIDO, UP_ARMA_ROT_BROADCAST, UP_ARMA_ROT_EXPRESSA,
    UP_ARMA_FLUXO_OVERCLOCK, UP_ARMA_FLUXO_VALVULA, UP_ARMA_ORBE_BURACO, UP_ARMA_ORBE_SUPERNOVA,
    UP_ARMA_FRAG_POLVORA, UP_ARMA_FRAG_QUICANTE, UP_ARMA_PULSO_ECO, UP_ARMA_PULSO_REPARO,
    UP_ARMA_RAIO_SUPER, UP_ARMA_RAIO_LENTE, UP_ARMA_RAIOC_SUPER, UP_ARMA_RAIOC_LENTE,
    UP_ARMA_KATANA_AFIADA, UP_ARMA_KATANA_ARCO, UP_ARMA_KATANA_RAPIDA, UP_ARMA_KATANA_ONDA, UP_ARMA_KATANA_REFLEXO,
    // Katana: elementos da natureza (cada um com o seu "porquê" digital)
    UP_ARMA_KATANA_FOGO, UP_ARMA_KATANA_GELO, UP_ARMA_KATANA_RAIO, UP_ARMA_KATANA_VENTO, UP_ARMA_KATANA_AGUA,
    // Sinergias (precisam de duas melhorias anteriores)
    UP_SIN_TEMPESTADE, UP_SIN_CIRCUITO, UP_SIN_FANTASMA, UP_SIN_ROTULO, UP_SIN_CONFIANCA, UP_SIN_CHOQUE_TERMICO,
    NUM_UPGRADES
} TipoUpgrade;

#define BIT_UP(u) (1ull << (unsigned)(u))

typedef enum { CAT_ATRIBUTO, CAT_DISPARO, CAT_HABILIDADE, CAT_ESPECIAL, CAT_APRIMORAMENTO, CAT_SINERGIA, NUM_CATEGORIAS_UP } CategoriaUpgrade;

typedef struct {
    const char *nome;
    const char *efeito;
    CategoriaUpgrade categoria;
    Raridade raridade;
    int maxNivel;
    unsigned long long requer;  // melhorias que precisam estar na build (todas)
    int arma;                   // arma necessária (-1 = qualquer)
} DefUpgrade;

// Números das melhorias (AJUSTE AQUI)
#define UP_DANO_NIVEL            0.12f
#define UP_VELOCIDADE_NIVEL      0.08f
#define UP_ESQUIVA_NIVEL         0.12f
#define UP_VIDA_NIVEL            20.0f
#define UP_CADENCIA_NIVEL        0.10f
#define UP_ALCANCE_NIVEL         0.20f
#define UP_VEL_PROJETIL_NIVEL    0.15f
#define UP_EXPLOSAO_DANO         0.35f
#define UP_EXPLOSAO_RAIO         42.0f    // +14 por nível acima do 1
#define UP_EXPLOSAO_RECARGA      0.12f    // explosões seguidas não acontecem no mesmo instante
#define UP_RAJADA_DANO           0.45f
#define UP_RASTRO_RAIO           30.0f
#define UP_RASTRO_DPS            12.0f
#define UP_RASTRO_RECARGA        0.35f
#define UP_POS_ESQUIVA_TEMPO     1.5f
#define UP_PULSO_RAIO            0.35f
#define UP_PULSO_DANO            0.40f
#define UP_ESQUIVA_EXP_RAIO      70.0f
#define UP_ESQUIVA_EXP_DANO      20.0f    // +12 por nível acima do 1
#define UP_ENERGIA_ABATE_EP      6.0f
#define UP_ESCUDO_TEMPO          8.0f
#define UP_ESCUDO_ABATES         12
#define UP_DUPLICAR_CHANCE       15
#define UP_ROTULO_TEMPO          4.0f
#define UP_ROTULO_BONUS          0.25f
#define UP_VERIFICACAO_CURA      2.0f
#define UP_CRITICO_CHANCE        10
#define UP_FANTASMA_DANO         25.0f
#define UP_ROTULO_EXP_RAIO       70.0f
#define UP_ROTULO_EXP_DANO       30.0f

// Penalidades do evento MODELO EXPERIMENTAL (valem até o fim da tentativa)
enum { PEN_FRAGIL = 1, PEN_LENTO = 2, PEN_VIDA = 4 };
#define PEN_FRAGIL_FATOR         1.20f    // +20% de dano recebido
#define PEN_LENTO_FATOR          0.90f    // -10% de velocidade
#define PEN_VIDA_FATOR           0.85f    // -15% de vida máxima

// =============================================================
// REFORMULAÇÃO DO BLOCO 2: MAPA PROCEDURAL RAMIFICADO
//   Cada tentativa sorteia um grafo em camadas (profundidades). Versão para o estande
//   (partidas curtas): o caminho até o Alucinador tem 10 trocas de sala, alternando
//   escolha e encontro obrigatório:
//     0    : 1 sala de combate (sempre)
//     1, 2 : 2 ou 3 salas por camada (escolha de caminho)
//     3    : ELITE: SENTINELA DE ATENÇÃO (obrigatória: a camada tem uma sala só)
//     4    : CÂMARA DA INFERÊNCIA (na versão bullet-hell, 2 portas: a Câmara ou um caminho que a evita)
//     5    : 2 ou 3 salas
//     6    : ELITE: RETROPROPAGADOR (obrigatória)
//     7    : 2 ou 3 salas
//     8    : MINI-CHEFE: JANELA DE CONTEXTO (obrigatória)
//     9    : 2 salas (recuperação antes do chefe)
//     10   : ALUCINADOR
//   A dificuldade dos inimigos usa a profundidade EQUIVALENTE (PROF_DIFICULDADE): a curva
//   continua indo de 0 a 7.
//   Cada sala liga a 1 ou 2 salas da camada seguinte (vizinhas no mapa), e
//   toda sala da camada seguinte recebe pelo menos uma ligação: não existe
//   caminho sem saída e sempre se chega à Câmara e ao chefe. Algumas salas
//   escondem uma SALA SECRETA (parede corrompida): ela é um desvio opcional
//   que sai para os mesmos destinos da sala onde estava escondida.
// =============================================================
typedef enum { LADO_CIMA, LADO_ESQUERDA, LADO_DIREITA, LADO_BAIXO, NUM_LADOS } LadoPorta;

typedef enum { RECOMPENSA_NENHUMA, RECOMPENSA_MELHORIA, RECOMPENSA_RARA, RECOMPENSA_FRAGMENTOS, RECOMPENSA_ARMA, RECOMPENSA_VIDA,
               RECOMPENSA_SELO,     // Silogista bullet-hell: o desafio opcional (Selo amplificado)
               NUM_TIPOS_RECOMPENSA } TipoRecompensa;

#define MAPA_PROFUNDIDADES       11       // 10 trocas de sala até o chefe (AJUSTE AQUI com as tabelas abaixo)
#define PROF_SILOGISTA           4
#define PROF_CHEFE               10
#define PROF_ESPECIAL_A          3        // Sentinela de Atenção
#define PROF_ESPECIAL_B          6        // Retropropagador
#define PROF_MINICHEFE           8        // Janela de Contexto
#define MAPA_LARGURA_MAX         3
#define MAPA_MAX_NOS             40
#define MAX_SAIDAS               3        // 3 só quando a sala é a única origem da camada seguinte (ex.: a Câmara)
#define MAX_SECRETAS             2

typedef struct {
    TipoSalaRogue tipo;
    TipoRecompensa recompensa;  // o que a porta anuncia (combate, elite, desafio e secreta)
    int prof, coluna;
    float x;                    // 0 a 1: posição horizontal no mapa (decide qual porta fica à esquerda)
    int saida[MAX_SAIDAS];
    int numSaidas;
    bool visitado;
    bool escondida;             // sala secreta ainda não descoberta (não aparece no mapa)
    int secreta;                // sala secreta escondida nesta sala (-1 = nenhuma)
    int pai;                    // sala secreta: a sala onde ela estava escondida
    int lab;                    // laboratório: a categoria (TipoLab)
    int especial;               // encontro especial: ESP_SENTINELA, ESP_RETRO ou ESP_JANELA
} NoMapa;

typedef struct {
    NoMapa no[MAPA_MAX_NOS];
    int numNos;
    int largura[MAPA_PROFUNDIDADES];
    int primeiro[MAPA_PROFUNDIDADES];   // índice do primeiro nó de cada camada
} MapaRogue;

typedef struct {
    LadoPorta lado;
    int destino;                // nó do mapa
    Rectangle r;
} PortaMissao;
#define PAREDE_SECRETA_HP        45.0f

// =============================================================
// REFORMULAÇÃO DO BLOCO 2: OBJETIVOS, ONDAS, ARMADILHAS E ELITES
// =============================================================
typedef enum { OBJC_ELIMINAR, OBJC_SOBREVIVER, OBJC_NUCLEOS, OBJC_PRIORITARIO, OBJC_MECANISMO, NUM_OBJC } ObjetivoCombate;

#define SOBREVIVER_TEMPO_BASE    24.0f    // + 2,5 s por profundidade (era 30: o caminho ficou mais longo)
#define MAX_ONDAS_SALA           2        // expansão da progressão: encontros de 3 ondas param na 2ª
#define REFORCO_INTERVALO        6.5f
#define REFORCO_LIMITE_VIVOS     9        // reforços não passam disso (inimigos vivos)
#define NUM_NUCLEOS              3
#define NUM_MECANISMOS           3
#define MECANISMO_TEMPO          1.6f     // segurando E perto do console
#define MECANISMO_RAIO           42.0f
#define NUCLEO_INTERVALO         4.2f
#define NUCLEO_PREPARO           0.8f
#define NUCLEO_BALAS             8
#define ONDA_AVISO               1.2f     // "ONDA 2/3" antes de a próxima onda surgir

// Elites: inimigos com aura dourada (mais vida, mais rápidos e com ataques reforçados)
// Afixos de elite: cada inimigo de aura dourada ganha UM poder de combate (sem perguntas)
//   PULSO    a cada tanto, um anel de balas (o brilho dourado avisa antes)
//   DIVISÃO  ao morrer, se parte em dois menores
//   ORBITAL  três escudos giram em volta e seguram os tiros (dá para quebrar cada um)
enum { AFIXO_NENHUM, AFIXO_PULSO, AFIXO_DIVISAO, AFIXO_ORBITAL, NUM_AFIXOS };
#define AFIXO_PULSO_INTERVALO    3.2f     // (AJUSTE AQUI)
#define AFIXO_PULSO_AVISO        0.7f
#define AFIXO_PULSO_BALAS        12
#define AFIXO_PULSO_VEL          150.0f
#define AFIXO_PULSO_DANO         7.0f
#define AFIXO_DIVISAO_HP         0.35f    // vida de cada metade (fração da vida do elite)
#define AFIXO_ORBES              3
#define AFIXO_ORBE_HP            30.0f
#define AFIXO_ORBE_RAIO          9.0f
#define AFIXO_ORBE_DIST          24.0f    // além do raio do elite
#define AFIXO_ORBE_DANO          6.0f
#define MAX_SELADOS_TENTATIVA    2        // Guardiões com pergunta por tentativa (e nunca nas salas de elite)

#define ELITE_HP                 2.8f     // era 2.4 (versão mais desafiadora)
#define ELITE_RAIO               1.2f
#define ELITE_RITMO              1.40f    // era 1.30
#define ELITE_CONTATO            1.30f
#define PRIORITARIO_HP           2.0f     // por cima do bônus de elite

// Dificuldade que cresce com a profundidade (mais do que só vida: ritmo e dano também)
#define DIFICULDADE_VIDA_PROF    0.10f    // eram 0.07, 0.04 e 0.05 (versão mais desafiadora)
#define DIFICULDADE_RITMO_PROF   0.06f
#define DIFICULDADE_DANO_PROF    0.08f
#define DIFICULDADE_CURVA        0.08f    // eq' = eq * (1 + 0,08*eq): camada 7 pesa ~56% a mais que na escala linear
#define CHANCE_ELITE_SURPRESA    45       // % de uma sala de combate funda ganhar um elite com afixo
#define PROF_ELITE_SURPRESA      2        // a partir desta profundidade (escala 0 a 7)

typedef enum { ARM_FEIXE, ARM_ZONA, ARM_TORRE, ARM_EXPLOSAO, ARM_BARREIRA, ARM_BLOQUEIO, NUM_TIPOS_ARMADILHA } TipoArmadilha;

typedef struct {
    bool ativo;
    TipoArmadilha tipo;
    int fase;                   // 0 = descansando, 1 = avisando, 2 = ativa
    float timer;
    Vector2 pos;                // zona, torre e explosão
    float raio;
    Rectangle r;                // feixe, barreira e bloqueio
    bool vertical;
    float limiteA, limiteB, vel;    // barreira: vai e volta entre os limites
    float tick;                 // zona: próximo dano
} Armadilha;

#define MAX_ARMADILHAS           4
#define FEIXE_DESCANSO           2.6f
#define FEIXE_AVISO              1.0f
#define FEIXE_ATIVO              0.7f
#define FEIXE_LARGURA            18.0f
#define FEIXE_DANO               14.0f
#define ZONA_RAIO                58.0f
#define ZONA_DANO                6.0f
#define ZONA_TICK                0.6f
#define ZONA_LIGADA              4.0f
#define ZONA_DESLIGADA           2.5f
#define TORRE_INTERVALO          2.4f
#define TORRE_PREPARO            0.7f
#define TORRE_TIRO_VEL           230.0f
#define TORRE_TIRO_DANO          8.0f
#define EXPLOSAO_INTERVALO       3.2f
#define EXPLOSAO_AVISO           1.2f
#define EXPLOSAO_RAIO            62.0f
#define EXPLOSAO_DANO            16.0f
#define BARREIRA_VEL             55.0f
#define BLOQUEIO_LIVRE           4.5f
#define BLOQUEIO_AVISO           1.2f
#define BLOQUEIO_SOLIDO          4.0f

// Poças e explosões do jogador (Resíduo de Dados, Esquiva Fragmentada...)
typedef struct {
    bool ativo;
    Vector2 pos;
    float raio, atraso, vida, vidaMax;
    float dps, tick;
    float danoInstantaneo;      // explode uma vez quando o atraso acaba
    Color cor;
    int estilo;                 // 0 = poça/explosão comum; 1 = Sigilo de Luz (Raio de Processamento)
} AreaDano;

#define MAX_AREAS                16

// Tela de escolha (1 de 3)
typedef enum { OPC_UPGRADE, OPC_ITEM, OPC_ARMA, OPC_FRAGMENTOS } TipoOpcao;
typedef struct { TipoOpcao tipo; int id; } OpcaoEscolha;
typedef enum { ESCOLHA_NORMAL, ESCOLHA_RARA, ESCOLHA_ARMA } ModoEscolha;
#define FC_RECUSAR_ESCOLHA       3
#define REROLL_GRATIS            2        // trocas grátis das opções, por tentativa
#define PRIMEIRA_MISSAO_VIDA     0.70f    // 1ª missão, salas 1 e 2: vida dos inimigos x0,70...
#define PRIMEIRA_MISSAO_RITMO    0.85f    // ...ritmo x0,85...
#define PRIMEIRA_MISSAO_DANO     0.60f    // ...e dano x0,60 (para aprender sem punição)
#define REROLL_CUSTO_FC          8        // depois das grátis, cada reroll custa isto
#define FC_OPCAO_RESERVA         5

// Eventos (salas de evento, de descanso e de desafio usam a mesma janela de escolhas)
typedef enum {
    EVT_ARQUIVO_CORROMPIDO, EVT_BANCO_INSTAVEL, EVT_MODELO_EXPERIMENTAL, EVT_MEMORIA, EVT_PROTOCOLO,
    EVT_FONTE_FALSA, EVT_CONFIANCA_LUCI, EVT_RESPOSTA_IA,
    NUM_EVENTOS_SORTEAVEIS,
    EVT_DESCANSO = NUM_EVENTOS_SORTEAVEIS, EVT_DESAFIO
} TipoEvento;

typedef enum { DESAFIO_SALA_ECOS, DESAFIO_SALA_INTEGRIDADE, NUM_DESAFIOS_SALA } TipoDesafioSala;
#define DESAFIO_ECOS_META        14
#define DESAFIO_ECOS_TEMPO       32.0f
#define DESAFIO_ECO_TEMPO        1.9f     // os ecos do desafio demoram mais para explodir
#define DESAFIO_INTEG_TEMPO      25.0f
#define DESAFIO_INTEG_GOLPES     3        // pode levar até 3 golpes

// =============================================================
// EXPANSÃO EDUCATIVA DO BLOCO 2: OS LABORATÓRIOS DA BIBLIOTECA
// =============================================================
// Uma sala nova do mapa (SALA_LABORATORIO) com cinco categorias, cada uma com
// uma mecânica própria e um console que abre uma janela por cima da sala:
//   LABORATÓRIO DE PROMPTS  Modo Prompt: monta o pedido com botões, uma IA simulada
//                            (offline, respostas cadastradas) responde e o jogador
//                            julga a resposta (aceitar, reformular ou desconfiar)
//   ARQUIVO CORROMPIDO      acha a afirmação suspeita e escolhe como verificá-la
//   NÚCLEO NEURAL           carrega blocos de contexto sem estourar a capacidade
//   FIREWALL ÉTICO          viés algorítmico (problema + correção) e privacidade
//                            (pacotes de dados passando pelo firewall)
//   CÂMARA DE PESQUISA      investiga uma mídia suspeita: pistas, ferramentas e veredito
// Todas são OPCIONAIS: a porta da sala já está aberta e a recompensa sai só ao concluir.
// As três primeiras são COMUNS; as duas últimas são ESPECIAIS, com recompensa maior.
// A Luci ajuda em todas (dica, análise, verificação, reformulação) gastando
// estabilidade: a CARGA de ajuda vira -1 de estabilidade a cada LAB_CARGA_POR_ESTAB.
typedef enum { LAB_PROMPTS, LAB_ARQUIVO, LAB_NUCLEO, LAB_FIREWALL, LAB_PESQUISA, NUM_LABS } TipoLab;

typedef enum { LAB_INTRO, LAB_DESAFIO, LAB_RESPOSTA, LAB_RESULTADO } EtapaLab;

// Modo Prompt: as quatro partes do pedido
typedef enum { PP_PAPEL, PP_CONTEXTO, PP_OBJETIVO, PP_FORMATO, NUM_PARTES_PROMPT } ParteNoPrompt;

// O que a IA simulada "entendeu" do pedido (decide qual resposta cadastrada aparece)
typedef enum { DIAG_IDEAL, DIAG_BOA, DIAG_VAGA, DIAG_PUBLICO, DIAG_PAPEL, DIAG_FORMATO, DIAG_CONTRADICAO, NUM_DIAG_PROMPT } DiagPrompt;

// Critérios da avaliação do pedido (0 a 2 cada; total 0 a 10)
enum { CRIT_CLAREZA, CRIT_INFORMACOES, CRIT_ADEQUACAO, CRIT_ESPECIFICIDADE, CRIT_AMBIGUIDADE, NUM_CRITERIOS };

#define LAB_MAX_ENVIOS           3        // envios do Modo Prompt (o 1º pedido vago não conta)
#define LAB_MAX_PROCESSOS        2        // Núcleo Neural: processar e reprocessar uma vez
#define LAB_MAX_ITENS            6        // afirmações, blocos de contexto e pistas
#define LAB_PACOTES              6        // Firewall: pacotes de dados por desafio

// Núcleo Neural e Firewall Ético EM AÇÃO: o desafio acontece na própria sala, atirando e
// se mexendo (sem a janela de leitura). 0 = a versão anterior (janela com botões).
#define LAB_EM_ACAO              1
#define LAB_MAX_OBJ_ACAO         10
#define LAB_ACAO_SURGIR_NUCLEO   2.1f     // um bloco de contexto a cada tanto (AJUSTE AQUI)
#define LAB_ACAO_VEL_BLOCO       46.0f
#define LAB_ACAO_RAIO_NUCLEO     40.0f    // o bloco que chega aqui entra na janela de contexto
#define LAB_ACAO_SURGIR_PACOTE   2.3f
#define LAB_ACAO_VEL_PACOTE      64.0f
#define LAB_ACAO_X_SAIDA         712.0f   // o pacote que passa daqui chega à IA
#define LAB_ACAO_DANO_VAZAMENTO  6.0f     // decisão errada no firewall custa um pouco de vida (nunca mata)

typedef struct {
    bool ativo;
    int id;                     // bloco (Núcleo) ou índice em PACOTES_DADOS (Firewall)
    int slot;                   // Firewall: posição em pacotes[] / acoes[]
    Vector2 pos, vel;
    float anim;
    bool anonimizado;           // Firewall: o jogador encostou (os dados pessoais somem)
    bool ejetado;               // Núcleo: cortado da janela (voa para fora e some)
    float vida;
} ObjAcaoLab;
#define LAB_TEMPO_PACOTE         12.0f    // segundos até o pacote atravessar sozinho
#define LAB_MAX_FERRAMENTAS      2        // Câmara de Pesquisa: consultas por caso
#define LAB_CARGA_POR_ESTAB      3        // carga de ajuda que custa 1 de estabilidade
#define LAB_VELOCIDADE_TEXTO     95.0f    // caracteres por segundo da resposta da IA
#define MAX_BOTOES_LAB           24

// Custos de carga de cada ajuda da Luci (proporcionais à utilidade)
#define LAB_CUSTO_DICA           1
#define LAB_CUSTO_ANALISE        2
#define LAB_CUSTO_VERIFICACAO    2
#define LAB_CUSTO_REFORMULACAO   3

typedef struct {
    bool aberto;                // a janela do console está na tela (o jogo espera)
    TipoLab tipo;
    int missao;                 // índice na tabela da categoria
    EtapaLab etapa;
    bool concluido;             // terminou (recompensa entregue, porta aberta)
    float tempo;                // tempo na etapa atual (evita clique acidental)
    float tempoTotal;           // telemetria
    int foco;                   // botão em foco (teclado)

    // Modo Prompt
    int escolha[NUM_PARTES_PROMPT];     // 0 = não incluir, 1 a 3 = alternativa
    int envios;
    bool original;              // a resposta na tela é a do pedido vago original
    DiagPrompt diag;
    int criterios[NUM_CRITERIOS];
    int notaPrompt;             // 0 a 10 do último envio
    bool respostaAlucinada;     // a resposta na tela inventou algo
    bool desconfiou;            // o jogador já desconfiou desta resposta
    bool pegouAlucinacao;       // desconfiou de uma resposta alucinada
    bool aceitou;               // terminou aceitando a resposta
    bool usouReformulacao;      // aceitou a reformulação da Luci (limita a 2 estrelas)
    float digitado;             // máquina de escrever da resposta
    char avaliacao[200];        // resultado do "desconfiar" ou da análise da resposta
    int sugestao[NUM_PARTES_PROMPT];
    bool temSugestao;

    // Arquivo, Núcleo, Firewall e Pesquisa
    int fase;                   // arquivo: 0 suspeita, 1 método; firewall: 0 problema, 1 medida, 2 pacotes
    int sel, sel2;              // escolhas (afirmação/método, problema/medida)
    unsigned int marcados;      // núcleo: blocos carregados; pesquisa: pistas marcadas
    int ordem[LAB_MAX_ITENS];   // núcleo: ordem de carga (o mais antigo é cortado no excesso)
    int numOrdem;
    unsigned int cortados;      // núcleo: blocos que a janela de contexto perdeu
    int processos;
    int resultadoNucleo;        // 0 boa, 1 faltou, 2 truncada, 3 ruidosa
    int pacote;
    int pacotes[LAB_PACOTES];   // índices sorteados do banco de pacotes
    int acoes[LAB_PACOTES];     // decisão do jogador em cada pacote (-1 = passou sozinho)
    float tempoPacote;
    unsigned int ferramentas;   // pesquisa: consultas feitas
    int veredito;
    int acertos, decisoes;

    // A Luci
    int nivelDica;              // dicas graduais já dadas (0 a 3)
    char luciTexto[240];
    bool luciTemSugestao;       // há uma sugestão dela na tela (dá para conferir com V)
    bool luciAlucinou;          // ... e ela era alucinada
    bool luciVerificada;
    int luciAponta;             // item, opção ou ação que a sugestão aponta (-1 = nenhum)
    int luciValor;              // classificação sugerida (núcleo, pesquisa, firewall)
    int luciFase;               // a que pergunta a sugestão se refere (fase, pacote ou resposta)
    char conferencia[200];      // resultado do V (conferir a sugestão)
    int ajudas;
    int alucinacoes;            // sugestões alucinadas nesta sala (reveladas no resultado)
    int alucinacoesPegas;
    bool seguiuAlucinacao;

    // Resultado
    int estrelas;
    char resultado[220];
    char recompensa[160];

    // Em ação (LAB_EM_ACAO): Núcleo Neural e Firewall Ético jogados na própria sala
    bool emAcao;
    float tempoAcao, cronoSurgir, tremeNucleo;
    int proximo, numAcao;       // próximo objeto a surgir / quantos a missão tem
    int ordemAcao[LAB_MAX_OBJ_ACAO];
    int carga;                  // Núcleo: tokens dentro da janela
    int ultimaFaixa;            // Firewall: não repete a faixa seguida
    ObjAcaoLab obj[LAB_MAX_OBJ_ACAO];

    // Laboratório de prompts reformulado: montagem por peças e luzes por encaixe
    int slotAtivo;              // encaixe em edição (PAPEL, CONTEXTO, OBJETIVO, FORMATO)
    float animPeca;             // > 0: a peça escolhida voando do cartão até o encaixe
    int animSlot, animDe;       // encaixe de destino e cartão de origem (0 a 3)
    int enviado[4];             // as peças do último envio (NUM_PARTES_PROMPT): a luz vale enquanto a peça não muda
    bool temEnvio;
    float tempoResposta;        // desde o envio (o holograma se monta)
} LabEdu;

// Efeitos com tempo (itens temporários)
enum { EFEITO_OVERCLOCK, EFEITO_ESCUDO, EFEITO_PROXY, EFEITO_LATENCIA, NUM_EFEITOS };

// Anel que se abre (explosões, ondas, limpezas)
typedef struct {
    Vector2 pos;
    float raio, tempo;          // tempo > 0 = aparecendo
    Color cor;
} AnelEfeito;

#define MAX_ANEIS 16

typedef struct {
    bool ativo;
    Vector2 pos, vel;
    float raio, dano;
    bool doJogador;
    int alvoDedutivo;           // 0 = tiro comum; projétil Q do Selo da Inferência: k+1 = inimigo k, -1 = chefe da sala
    int arma;                   // TipoArma que disparou (o visual e o comportamento dependem dela)
    float alcance;              // distância que ainda pode andar (0 = sem limite)
    float vida;                 // segundos que ainda pode voar (0 = sem limite)
    int perfura;                // Vetor: inimigos que ainda atravessa
    int ricochetes;             // Roteador: ricochetes que faltam
    int ultimoAlvo;             // k+1 do último inimigo atingido (não acerta o mesmo duas vezes)
    bool guiado;                // procura o inimigo mais perto
    bool secundario;            // fragmento ou pacote do Botnet Reverso: não gera outros efeitos
} Projetil;

typedef struct {
    bool ativo;
    TipoInimigo tipo;
    Vector2 pos, vel, empurrao;
    float hp, hpMax, raio;
    int estado;                 // 0 = andando, 1 = preparando o ataque, 2 = atacando
    float timer, recarga, flash, anim, surgindo;
    Vector2 alvo;               // direção do bote / da mira
    float lado;                 // guardião: para que lado ele anda de lado
    bool selado;                // guardião com um desafio educativo: não leva dano até alguém responder
    float atordoado;            // acertou o desafio: o guardião fica parado
    int marca;                  // Disparo Dedutivo: 0 = nada, 1 = P → Q, 2 = P confirmado (Q disparado)
    float tempoMarca;

    // Inimigos da expansão (cada tipo usa só os campos que precisa)
    Vector2 frente;             // Firewall: para onde o escudo aponta
    float carga, cargaAlvo;     // Overclock: carga atual e a sorteada para esta investida
    float energia, semDano;     // Cache: energia guardada e tempo sem levar dano
    float recuperando;          // Cache: > 0 = recuperando vida (um tiro interrompe)
    float habilidade;           // recarga da habilidade especial (cópia do Phisher, barreira do Firewall)
    float vida;                 // Spam e cópia do Phisher: somem quando chega a zero
    int dono;                   // cópia do Phisher: índice do Phisher verdadeiro
    bool conectado;             // ligado a um Botnet (mais rápido, ataca mais vezes)
    int conexao[BOTNET_CONEXOES];   // Botnet: os inimigos conectados (-1 = nenhum)
    Vector2 miraPrevista;       // Keylogger: onde ele acha que o jogador vai estar
    int tiros;                  // Keylogger: tiros que faltam na rajada

    // Reformulação do Bloco 2
    bool elite;                 // aura dourada: mais vida, ritmo e ataques reforçados
    int faseElite;              // Sentinela/Retropropagador: 0, 1 ou 2 (a 66% e a 33% da vida)
    bool prioritario;           // ALVO PRIORITÁRIO: enquanto vive, chegam reforços
    bool desafio;               // eco da sala de desafio (conta para a meta, não repara a Luci)
    float rotulo;               // Rotulagem de Dados: > 0 = leva mais dano
    float queima, queimaDano, cronoQueima;   // Katana (fogo): tempo, dano por pulso e o relógio do pulso
    float gelo;                 // Katana (gelo): > 0 = congelado (anda mais devagar)
    bool atingidoEsquiva;       // Passo Fantasma: já foi ferido nesta esquiva

    // Expansão da progressão: encontros especiais (cada tipo usa só os seus)
    float foco;                 // Sentinela: atenção acumulada (0 a 1)
    bool visao;                 // Sentinela: enxerga o jogador (nenhuma estante no caminho)
    float distraida;            // Sentinela: > 0 = acabou de perder o jogador de vista (vulnerável)
    Vector2 fimFeixe;           // Sentinela: onde o feixe para (estante ou parede)
    bool feixeAcertou;
    Vector2 registro[RETRO_MAX_REGISTROS];  // Retropropagador: de onde vieram os golpes
    int numRegistros, excesso;
    Vector2 memoria;            // dado dourado: onde o jogador estava quando ele saiu

    // Afixo de elite (só os de aura dourada comuns; os encontros especiais têm a própria mecânica)
    int afixo;
    float cronoAfixo;
    float hpOrbe[AFIXO_ORBES];  // ORBITAL: vida de cada escudo (0 = quebrado)
} Inimigo;

typedef struct {
    Vector2 pos, vel;
    float vida, vidaMax, tam;
    Color cor;
} ParticulaRogue;

// Coisas no chão: fragmentos de conhecimento e itens
typedef struct {
    bool ativo;
    bool fragmento;             // true = Fragmento de Conhecimento, false = item
    TipoItem item;
    Vector2 pos, vel;
    float anim;
} Coletavel;

// Bloco 3 (reformulação): o parry do Thomas
typedef struct {
    bool emParry;
    float tempoJanelaParry;     // PARRY_JANELA
    float cooldownParry;        // evita apertar sem parar
    bool hitFreezeAtivo;
    float tempoHitFreeze;
    int comboMeleeContador;     // 1, 2 ou 3 golpes no atordoamento
    float golpeAnim;            // animação do golpe corpo a corpo
    Vector2 dirGolpe;
} JogadorParryState;

// Texto curto que sobe e some (P -> Q, PARRY PERFEITO!, ...)
typedef struct {
    Vector2 pos;
    char texto[48];
    float tempo;
    Color cor;
} TextoFlutuante;

#define MAX_TEXTOS_FLUTUANTES 8

// Número de dano que sobe do alvo. Golpes no mesmo alvo logo depois que o número nasce somam nele
// (os balins da Fragmentadora viram um número só, e o Fluxo não enche a tela)
typedef struct {
    Vector2 pos;                // de onde ele sobe
    float valor;                // dano somado
    float tempo;                // > 0 = na tela
    float pop;                  // 1 = acabou de somar (o número dá um pulo)
    int alvo;                   // índice do inimigo ou ALVO_NUM_CHEFE / _SILOGISTA / _LUCI
    unsigned char tipo;         // NUM_DANO_NORMAL, _FORTE (bônus) ou _BLOQUEADO (escudo, guarda)
} NumeroDano;

#define MAX_NUMEROS_DANO    12
#define NUMERO_DANO_TEMPO   0.65f
#define NUMERO_DANO_JUNTAR  0.20f   // golpe no mesmo alvo até este tempo depois do número nascer soma nele
enum { NUM_DANO_NORMAL, NUM_DANO_FORTE, NUM_DANO_BLOQUEADO };
enum { ALVO_NUM_CHEFE = 1000, ALVO_NUM_SILOGISTA, ALVO_NUM_LUCI };

// Faísca no ponto do acerto: um estalo branco que vira a cor do alvo e some em poucos quadros
typedef struct {
    Vector2 pos, dir;           // dir: para onde o tiro ia (as faíscas voltam ao contrário)
    float tempo;
    Color cor;
} FaiscaAcerto;

#define MAX_FAISCAS   16
#define FAISCA_TEMPO  0.12f

typedef struct {
    float hp, ep;
    Vector2 pos;
    Direcao direcao;
    AnimacaoJogador anim;
    Vector2 mira;               // direção unitária
    bool miraSetas;             // a última mira veio das setas (esconde a mira do mouse)
    float cronoTiro, recargaEsquiva, recargaPulso, invulneravel, tempoEsquiva, efeitoPulso;
    Vector2 dirEsquiva;
    int itens[NUM_ITENS];       // mochila desta tentativa (perde ao morrer)
    bool seloInferencia;        // venceu o Silogista nesta tentativa: Disparo Dedutivo (perde ao morrer)
    float recargaDedutivo;
    Vector2 velMedia;           // Keylogger: velocidade recente do jogador
    float tempoMesmaDirecao;    // ... e há quanto tempo ele anda na mesma direção

    // Armas: o Disparador sempre, mais uma arma extra (TAB ou a rodinha do mouse troca)
    TipoArma armaBase;          // arma 1: começa com o Disparador (a do tutorial) e pode ser trocada
    TipoArma armaExtra;         // arma 2 (ARMA_DISPARADOR = vaga livre)
    bool usandoExtra;          // false = arma 1, true = arma 2
    float permVel, permRegen;   // bônus das melhorias permanentes (velocidade e regeneração de EP)
    int rerolls;                // REROLAR: quantas trocas grátis de opções ainda restam nesta tentativa
    float carga;                // Vetor de Compilação
    float calor;                // Fluxo Contínuo (0 a 1)
    bool superaquecida;
    int cartucho;               // Fragmentadora: conta os tiros (o 4º é o cartucho dourado)
    int ecoN;
    float ecoPulso, ecoDano;    // Pulso de Firewall: o eco que dispara logo depois
    double epPiscaAte;          // a barra de EP pisca em vermelho até este instante (tentou atirar sem EP)
    float katanaAnim, katanaAng, katanaArco, katanaAlcance;     // Katana de Bytes: o arco do último golpe (só o desenho)
    float trocaArma;            // > 0: acabou de trocar/pegar uma arma (anel, nome sobre o Thomas e a arma "pulando" na mão)

    // Itens temporários e upgrades passivos
    float efeito[NUM_EFEITOS];  // tempo que falta de cada efeito
    Vector2 proxyPos;
    float proxyHp;
    int disparos;               // Fragmentação de Pacotes
    bool fragmentacaoArmada;
    int alvoAnalise;            // Analisador Heurístico: k+1 do inimigo, -1 = chefe, 0 = nenhum
    int marcasAnalise;
    float tempoAnalise;
    float recargaBotnet;        // Botnet Reverso
    int salasConcluidas;        // Cache Persistente (nesta tentativa)

    // Reformulação do Bloco 2: a build desta tentativa (perde ao morrer)
    int up[NUM_UPGRADES];       // nível de cada melhoria (0 = não tem)
    int ultimoUp;               // a última melhoria escolhida (-1 = nenhuma)
    int penalidade;             // PEN_* (Modelo Experimental)
    float bonusPosEsquiva;      // Contra-Ataque Heurístico: tempo que falta
    float recargaEsquiva2;      // Buffer de Esquiva: a 2ª carga
    float recargaExplosao, recargaRastro;
    int abatesEscudo;           // Protocolo de Blindagem: abates até o próximo escudo
    int contaFluxo;             // Amostragem Múltipla com o Fluxo Contínuo: 1 extra a cada 3 tiros
    int golpesSofridos;         // golpes que tiraram vida nesta sala (desafio de integridade)

    JogadorParryState parry;    // Bloco 3: defesa perfeita e o combo de contra-ataque
    bool seloAmplificado;       // venceu o Silogista bullet-hell: Disparo Dedutivo e EP reforçados
} JogadorRogue;

// ---------- ALUCINADOR (chefe da Biblioteca) ----------
//   FASE 1: combate. Revezamento de padrões (leque, espiral, investida e
//   Fragmentos convocados), sempre com aviso antes.
//   FASE 2 (a partir de CHEFE_FASE2 da vida): escudo de distorção. Ele segue
//   atacando e, no alto da arena, faz AFIRMAÇÕES (verdadeiras, falsas ou que
//   contradizem o que ele mesmo disse). C = CONFIÁVEL, F = PROBLEMÁTICA.
//   Acertou: o escudo cai por CHEFE_JANELA segundos (só aí ele leva dano).
//   Errou ou o tempo acabou: ataque especial.
#define CHEFE_HP                 900.0f   // era 700 (simulação: luta longa demais para quem erra a mira)
#define CHEFE_FASE2              0.55f    // fração da vida em que a fase 2 começa
#define CHEFE_RAIO               38.0f
#define CHEFE_DANO_CONTATO       15.0f
#define CHEFE_VELOCIDADE         110.0f
#define CHEFE_TIRO_VEL           215.0f
#define CHEFE_TIRO_DANO          10.0f
#define CHEFE_INVESTIDA_VEL      520.0f
#define CHEFE_JANELA             6.0f     // escudo aberto depois de um acerto
#define TEMPO_AFIRMACAO          9.0f     // tempo para responder
#define INTERVALO_AFIRMACAO      2.5f     // pausa entre uma afirmação e outra
#define TEMPO_RESULTADO_AFIRM    3.0f     // explicação na faixa (o combate continua)
#define PONTOS_ALUCINADOR        50
#define PONTOS_AFIRMACAO         5
#define FC_ALUCINADOR            10

typedef enum { PADRAO_REPOSICIONAR, PADRAO_LEQUE, PADRAO_ESPIRAL, PADRAO_INVESTIDA, PADRAO_ECOS } PadraoChefe;

typedef struct {
    bool ativo;
    bool apresentado;           // a conversa de abertura já aconteceu
    int fase;                   // 1 = combate, 2 = escudo + afirmações
    int surto;                  // 0, 1 (SURTO, a 28% da vida) ou 2 (COLAPSO, a 12%): dentro da fase 2
    Vector2 pos, destino;
    float hp, flash, anim;

    PadraoChefe padrao;
    int proximoPadrao;
    float tempoPadrao, cronoTiro, anguloEspiral;
    int rajadas;
    int etapaInvestida;         // 0 = mirando (linha vermelha), 1 = avançando
    Vector2 dirInvestida;

    // Fase 2
    float janela;               // > 0 = escudo aberto
    int ordemAfirm[24];         // precisa comportar NUM_AFIRMACOES_ALUCINADOR
    int afirmacaoAtual;         // -1 = nenhuma na tela
    int proximaAfirm;
    float tempoAfirm;           // tempo da afirmação atual (ou da pausa entre elas)
    bool mostrandoResultado;
    bool acertouAfirm;
    bool usouManualAfirm;
    char dicaAfirm[160];
    bool dicaAfirmLuci, luciMentiuAfirm;

    float morrendo;             // animação da derrota dele
    int marca;                  // Disparo Dedutivo (Selo da Inferência)
    float tempoMarca;
    int opcaoDicaAfirm;         // o que a dica da Luci disse: 1 = confiável, 0 = problemática, -1 = nada
    bool verificouAfirm;
} ChefeRogue;

// ---------- O SILOGISTA (mini-chefe da Câmara da Inferência) ----------
//   Fica sempre no meio da missão. Não faz perguntas na tela: a ARMADURA LÓGICA
//   dele tem 3 camadas, e cada uma cai resolvendo um puzzle DENTRO da arena.
//   1) MONTAGEM DO SILOGISMO: pegar os 3 fragmentos (E), levar e encaixar em P1, P2 e C.
//   2) SOFISMA: ele monta um argumento inválido; destruir a tiros a peça que não se sustenta.
//   3) CIRCUITO PROPOSICIONAL: (p ∧ q) ∨ ¬q. Rodada 1: atirar nas portas na ordem certa.
//      Rodada 2: o núcleo exige o valor contrário; atirar em p e q para escolher os valores.
//   Sem armadura, ele leva dano normal. Vencido: SELO DA INFERÊNCIA (Disparo Dedutivo)
//   e o conceito FUNDAMENTOS DA LÓGICA na enciclopédia.
//   Ataques (todos com aviso antes, diferentes dos do Alucinador): LINHAS de projéteis
//   com uma brecha, ZONAS de bloqueio em volta dos mecanismos, REFUTAÇÃO (rajada numa
//   linha mirada, usada contra quem carrega um fragmento), CORTE (trava um terminal ou
//   uma porta do circuito por um tempo) e ANEL (quando o jogador erra um puzzle).
#define SIL_HP                   520.0f
#define SIL_RAIO                 34.0f
#define SIL_DANO_CONTATO         12.0f
#define SIL_TIRO_VEL             190.0f
#define SIL_TIRO_DANO            9.0f
#define SIL_INTERVALO_PUZZLE     2.6f     // pausa entre um ataque e outro durante os puzzles
#define SIL_INTERVALO_FINAL      1.4f     // ... e sem a armadura
#define SIL_AVISO                0.9f     // aviso antes de cada ataque (dá para sair do caminho)
#define SIL_ZONA_RAIO            62.0f
#define SIL_ZONA_DURACAO         1.6f
#define SIL_CORTE_DURACAO        3.5f     // terminal ou porta travada pelo corte
#define SIL_LINHA_ESPACO         36.0f    // distância entre as balas de uma linha
#define SIL_LINHA_BRECHA         3        // balas que faltam na linha (a passagem)
#define SIL_VELOCIDADE_CARREGANDO 0.75f   // carregando um fragmento, o jogador anda mais devagar
#define SIL_RAIO_PEGAR           36.0f
#define SIL_RAIO_ENCAIXE         46.0f
#define SIL_HP_PECA              40.0f    // sofisma: um tiro perdido não destrói a peça
#define SIL_RECUPERA_PECA        10.0f    // HP por segundo que a peça recupera
#define SIL_BLINDAGEM            4.0f     // errou o sofisma: as peças ficam blindadas
#define SIL_RECARGA_ELEMENTO     0.4f     // um tiro só conta a cada 0,4 s no mesmo terminal/porta
#define PONTOS_SILOGISTA         25
#define PONTOS_ETAPA_SILOGISTA   5
#define FC_SILOGISTA             5

// Selo da Inferência: Disparo Dedutivo (Modus Ponens)
#define DEDUTIVO_RECARGA         1.2f     // intervalo mínimo entre duas conclusões Q
#define DEDUTIVO_MARCA_DURACAO   3.0f     // a marca P → Q some se o 2º acerto demorar
#define DEDUTIVO_DANO            2.5f     // o projétil Q causa 2,5x o dano do tiro
#define DEDUTIVO_VEL             460.0f

typedef enum { SIL_MONTAGEM, SIL_SOFISMA, SIL_CIRCUITO, SIL_VULNERAVEL } EtapaSilogista;

typedef enum { ATQ_LINHAS, ATQ_ZONA, ATQ_REFUTACAO, ATQ_CORTE, ATQ_ANEL } TipoAtaqueSil;

typedef struct {
    bool ativo;
    TipoAtaqueSil tipo;
    float aviso;                // > 0 = ainda avisando
    float duracao;              // zona ativa / rajada em andamento
    Vector2 pos, dir;           // zona: centro; linhas: começo e para onde andam; refutação: direção
    int brecha;                 // linhas: onde fica a passagem
    int alvo;                   // corte: terminal ou porta travada
    int tiros;
    float cronoTiro;
} AtaqueSil;

#define MAX_ATAQUES_SIL 3

enum { ENCAIXE_P1, ENCAIXE_P2, ENCAIXE_C, NUM_ENCAIXES };

#define MAX_PORTAS_CIRCUITO   4
#define MAX_ELEMENTOS_CIRCUITO (2 + MAX_PORTAS_CIRCUITO)    // 0 = p, 1 = q, 2+k = porta k

// =============================================================
// O SILOGISTA: ARQUITETURA DUAL (para comparar as duas versões)
//   SILOGISTA_MODO_QUIZ        = versão LEGADA, preservada sem mudanças: a Câmara da
//                                Inferência obrigatória no meio do mapa, com os puzzles
//                                (montagem do silogismo, sofisma e circuito) e as
//                                explicações que pausam o jogo entre um e outro.
//   SILOGISTA_MODO_BULLET_HELL = versão NOVA: desafio OPCIONAL no mapa (como em Hades),
//                                bullet-hell lógico com projéteis simbólicos, FOCUS MODE
//                                (câmera lenta) e parry. Vencendo: Selo amplificado.
//   As duas versões são sempre compiladas (a legada não quebra sem ninguém ver);
//   a flag só escolhe qual delas o jogo usa (descomente a linha da QUIZ abaixo, ou
//   compile com -DSILOGISTA_MODO_QUIZ). Defina só uma: as duas juntas dão erro.
// =============================================================

// Alterne esta flag para definir qual versão do Silogista será executada no jogo:
// #define SILOGISTA_MODO_QUIZ        // Versão Legada (a Câmara da Inferência original)
#ifndef SILOGISTA_MODO_QUIZ
#define SILOGISTA_MODO_BULLET_HELL   // Nova Versão Dinâmica (Bullet-Hell + Focus Mode)
#endif

#if defined(SILOGISTA_MODO_QUIZ) && defined(SILOGISTA_MODO_BULLET_HELL)
#error "Escolha apenas um modo do Silogista: SILOGISTA_MODO_QUIZ ou SILOGISTA_MODO_BULLET_HELL"
#endif

#ifdef SILOGISTA_MODO_BULLET_HELL
#define SILOGISTA_BH 1
#else
#define SILOGISTA_BH 0
#endif

// ---------- Versão bullet-hell (AJUSTE AQUI) ----------
#define MAX_PROJETEIS_SILOGISTA  20
#define SIL_BH_META              3        // conclusões válidas para derrubar cada camada da armadura
#define SIL_BH_FATOR_FOCO        0.3f     // Focus Mode: o mundo anda a 30% (dt * 0.3)
#define SIL_BH_FOCO_VELOCIDADE   0.6f     // ... e o Thomas mantém 60% da velocidade
#define SIL_BH_FOCO_EP           16.0f    // EP por segundo segurando o foco
#define SIL_BH_PENALIDADE_EP     12.0f    // erro lógico: a "estabilidade mental" (EP) do Thomas cai
#define SIL_BH_RAIO_SIMBOLO      17.0f
#define SIL_BH_DEDUTIVO_BONUS    1.5f     // recompensa: Disparo Dedutivo 50% mais forte ...
#define SIL_BH_DEDUTIVO_RECARGA  0.7f     // ... e 30% mais rápido
#define SIL_BH_EP_BONUS          20.0f    // ... e +20 de EP máximo

typedef enum {
    PROJETIL_LOGICO_VALIDO,     // a conclusão que decorre das premissas (é nela que se atira)
    PROJETIL_LOGICO_FALACIA,    // conclusão inválida
    PROJETIL_LOGICO_PREMISSA    // uma premissa (ela foi dada como verdadeira: não é ela que está em jogo)
} TipoProjetilLogico;

typedef struct {
    Vector2 posicao;
    Vector2 velocidade;
    char textoSimbolo[16];              // ex.: "P -> Q", "LOGO Q", "INVÁLIDO"
    const char *explicacaoPedagogica;   // glossário do foco: o argumento completo
    const char *justificativa;          // acerto: a regra; erro: por que é falácia
    TipoProjetilLogico tipo;
    bool ehRespostaCorreta;
    bool ativo;
    float raio;
    float vida;                         // some sozinho depois disso
    float surgindo;                     // aparecendo (não machuca nem pode ser atingido)
} ProjetilSilogista;

typedef struct {
    // Estado do bullet-hell
    ProjetilSilogista projeteis[MAX_PROJETEIS_SILOGISTA];
    int totalProjeteis;                 // quantos estão ativos agora
    bool modoFocoAtivo;
    float fatorTempo;                   // 1.0 normal, 0.3 no Focus Mode

    // Feedback educativo (faixa no alto: verde = regra confirmada, vermelho = erro com a justificativa)
    char mensagemFeedback[128];
    float tempoExibicaoFeedback;
    Color corFeedback;

    // Progresso
    int rodada;                         // 0, 1, 2 (uma por camada da armadura)
    int acertosRodada;
    bool desafioConcluido;
    bool recompensaObtida;

    // Padrões de tiro
    int padrao;
    float proximoPadrao;
    int emissoesRestantes;              // espiral em andamento
    float cronoEmissao, anguloEmissao;
    float recargaErro;                  // erros seguidos contam uma vez só
} EstadoSilogista;

typedef struct {
    bool ativo;
    bool apresentado;           // a conversa de abertura já aconteceu
    EtapaSilogista etapa;
    Vector2 pos;
    float hp, flash, anim, morrendo;
    bool recompensaDada;
    int marca;                  // Disparo Dedutivo
    float tempoMarca;

    // Ataques
    AtaqueSil atq[MAX_ATAQUES_SIL];
    float proximoAtaque;
    int revezamento;

    // Montagem: a peça k é a frase k do silogismo (0 = P1, 1 = P2, 2 = C)
    Vector2 posPeca[NUM_ENCAIXES];
    char letraPeca[NUM_ENCAIXES];       // X, Y ou Z, sorteadas (a ordem não entrega a resposta)
    int encaixe[NUM_ENCAIXES];          // peça em cada mecanismo (-1 = vazio)
    int carregando;                     // peça na mão do jogador (-1 = nenhuma)
    float erro;                         // o mecanismo (ou o núcleo) pisca em vermelho

    // Sofisma
    float hpPeca[NUM_ENCAIXES];
    float blindagem;
    float tremePeca[NUM_ENCAIXES];

    // Circuito
    int rodada;                         // 1 = avaliar com os valores dados; 2 = escolher p e q
    bool valor[2];                      // p e q
    bool ativada[MAX_PORTAS_CIRCUITO];
    bool saida[MAX_PORTAS_CIRCUITO];
    bool exigido;                       // valor que o núcleo exige nesta rodada
    float recarga[MAX_ELEMENTOS_CIRCUITO];
    float travado[MAX_ELEMENTOS_CIRCUITO];
    float erroElemento[MAX_ELEMENTOS_CIRCUITO];
    float nucleo;                       // núcleo energizado (animação)

    // Versão bullet-hell (SILOGISTA_MODO_BULLET_HELL). Na legada, modoBH fica false.
    bool modoBH;
    EstadoSilogista bh;
} Silogista;

// ---------- BLOCO 3: a Luci como chefe final (AJUSTE AQUI) ----------
#define LUCI_HP_BOA              2400.0f  // índice bom (as 3 barras ocultas somadas; era 1900: a 2ª e a 3ª barra acabavam em ~15 s)
#define LUCI_HP_RUIM             3000.0f  // índice ruim: mais vida, mais ritmo e a guarda do contexto (era 2400)
#define LUCI_RAIO                30.0f
#define LUCI_FASE2               0.65f    // fração da vida em que começa a fase 2
#define LUCI_FASE3               0.30f    // ... e a fase 3 (só com índice ruim)
#define LUCI_RITMO_RUIM          1.2f     // ataques mais rápidos com índice ruim
#define LUCI_TIRO_VEL            210.0f
#define LUCI_TIRO_DANO           9.0f
#define LUCI_DANO_AREA           18.0f    // ataque numa metade da arena
#define LUCI_DANO_CONTATO        12.0f
#define LUCI_DICA_INTERVALO      4.0f
#define LUCI_DICA_AVISO          1.6f     // o aviso vermelho dura isso antes do golpe
#define LUCI_DICA_FALSA_BOA      30       // % de o "lado seguro" ser mentira (índice bom)
#define LUCI_DICA_FALSA_RUIM     45       // ... (índice ruim)
#define LUCI_FRASE_TEMPO         4.0f     // tempo para contestar uma frase
#define LUCI_FRASE_BONUS         2.0f     // conceito ALUCINAÇÕES: tempo extra
#define LUCI_JANELA              4.0f     // armadura aberta depois de flagrar uma contradição
#define LUCI_CURA_ERRO           0.05f    // contestar à toa ou pedido vago: ela recupera 5%
#define INDICE_FINAL_BOM         60       // índice para o final bom
#define PONTOS_FINAL_BOM         100
#define PONTOS_FINAL_RUIM        40
#define NUM_CONTEXTO_LUCI        5
#define LUCI_FASE_CONTRADICAO    0.50f    // 1ª barra: abaixo disso vêm as contradições
#define LUCI_COLAPSO             2.0f     // 1ª queda: a falsa derrota (ela parece desabar)
#define LUCI_EXPLOSAO_ESTATICA   1.4f     // 2ª queda
#define LUCI_RENASCER            1.3f     // a barra volta a encher
#define LUCI_TELEGRAFIA          0.65f    // flash vermelho antes da investida (2ª barra)
#define LUCI_TELEGRAFIA_3        0.45f    // ... na 3ª barra
#define LUCI_INVESTIDA_VEL       640.0f
#define LUCI_INVESTIDA_DURACAO   0.28f
#define LUCI_RECUPERA            0.65f    // depois da investida: parada e vulnerável
#define LUCI_DANO_SWIPE          18.0f
#define LUCI_LAMINA_RAIO         30.0f
#define LUCI_DIST_RONDA          170.0f
#define LUCI_ONDA_VEL            240.0f
#define LUCI_ONDA_AVISO          0.9f
#define LUCI_ONDA_BRECHA         30.0f    // graus para cada lado: a passagem segura da onda
#define LUCI_DANO_ONDA           14.0f
#define NUM_PISO_LUCI            6
#define PISO_INTEIRO             3.5f     // + até 2 s sorteados
#define PISO_AVISO               1.0f     // rachado em vermelho
#define PISO_VAZIO               2.4f
#define PISO_DANO                8.0f
#define PARRY_JANELA             0.18f    // 11 quadros a 60 FPS
#define PARRY_RECARGA            0.55f    // depois de uma janela perdida (evita apertar sem parar)
#define PARRY_HIT_FREEZE         0.10f    // 6 quadros congelados no choque
#define PARRY_STUN               1.5f
#define PARRY_TREMOR             8.0f
#define PARRY_ZOOM               0.45f
#define MELEE_ALCANCE            95.0f

// Etapa 2 (ritmo da luta final): GUARDA nas barras 2 e 3 — fora das janelas os tiros fazem só 35%.
// Janelas de dano cheio: atordoada (parry), se recuperando da investida, atingida por um orbe rebatido
// e SUPERAQUECIDA depois da varredura. Assim a luta é decidida pelo parry, não por atirar parado.
#define LUCI_GUARDA_FATOR        0.35f
// 2ª barra: ORBES DE FALÁCIA — rajada de 3 orbes vermelhos; o PARRY (V) rebate o orbe de volta nela
#define LUCI_NUM_ORBES           4
#define LUCI_ORBE_VEL            210.0f
#define LUCI_ORBE_VEL_REBATIDO   540.0f
#define LUCI_ORBE_RAIO           13.0f
#define LUCI_ORBE_DANO           14.0f
#define LUCI_ORBE_FRACAO         0.05f    // orbe rebatido: 5% da barra (passa pela guarda)
#define LUCI_ORBE_ATORDOA        0.8f     // ... e abre a guarda por um instante
#define LUCI_ORBE_AVISO          0.75f
#define LUCI_ORBE_INTERVALO      0.45f    // entre os orbes da rajada
#define LUCI_ORBE_RECARGA        7.5f     // entre as rajadas
// 3ª barra: VARREDURA DE DADOS — um feixe gira em volta dela; depois ela fica SUPERAQUECIDA
#define LUCI_VARREDURA_AVISO     1.1f
#define LUCI_VARREDURA_VEL       150.0f   // graus por segundo
#define LUCI_VARREDURA_ARCO      240.0f
#define LUCI_VARREDURA_ALCANCE   600.0f
#define LUCI_VARREDURA_DANO      16.0f
#define LUCI_SUPERAQUECIDA       2.2f     // guarda aberta depois da varredura
#define MELEE_CURA_HP            5.0f
#define MELEE_CURA_EP            8.0f
#define TRANSICAO_NUCLEO         1.6f     // a cutscene da porta do átrio
#define TRANSICAO_MESTRE         2.6f     // ponte Bloco 1 -> Biblioteca: metade na sala da Mestre, metade já no átrio

enum { LUCI_SUB_DICA = 1, LUCI_SUB_CONTRADICAO = 2, LUCI_SUB_SURTO = 4, LUCI_SUB_NUCLEO = 5 };

// Etapa 2: orbe de falácia (2ª barra)
typedef struct { bool ativo, rebatido; Vector2 pos, vel; int falacia; float vida; } OrbeFalacia;
#define NUM_FALACIAS 6

typedef struct {
    bool ativo, apresentado;
    bool boa;                   // índice bom na entrada
    int fase;                   // 1 dica envenenada, 2 contradições, 3 contexto
    Vector2 pos;
    float hp, hpMax, flash, anim, morrendo, ritmo, cronoTiro;
    int marca;                  // Disparo Dedutivo
    float tempoMarca;
    // Fase 1 (e 3): ataque numa metade da arena
    float tempoDica;
    bool avisando;
    int ladoAtaque;             // 0 = esquerda, 1 = direita
    bool dicaFalsa;             // o "lado seguro" anunciado é mentira
    // Fase 2: armadura, cópias e frases
    float janela;
    float tempoFrase, tempoFraseMax;
    int frase;                  // -1 = nenhuma
    bool fraseContradiz, contestada;
    Vector2 copia[3];
    int real;
    float troca, angulo;
    // Fase 3: pedido com contexto
    Vector2 posContexto[NUM_CONTEXTO_LUCI];
    bool entregue[NUM_CONTEXTO_LUCI];
    int carregando, entregues;
    bool contextoPronto;

    // Reformulação: as 3 fases ocultas (a tela mostra uma barra só)
    int faseOculta;             // 1 = conflito lógico, 2 = surto físico, 3 = núcleo instável
    bool emRegeneracao;         // desabando (falsa derrota) ou renascendo
    int etapaRegeneracao;       // 0 = desabando / explosão de estática, 1 = a barra renasce
    float tempoRegeneracao;
    bool exigeContexto;         // 3ª barra com índice ruim: o pedido claro abre a guarda

    // Combate corpo a corpo (2ª e 3ª barras)
    int estadoMelee;            // 0 = rondando, 1 = telegrafando (flash vermelho), 2 = investida, 3 = recuperando
    float tempoMelee;
    Vector2 dirMelee;
    int investidasRestantes;
    bool acertouInvestida;
    int falacia;                // a falácia desta investida
    float mostrarFalacia;       // tempo da explicação na faixa (depois de um parry)
    bool atordoada;
    float tempoStun;
    int combo;                  // golpes dedutivos feitos neste atordoamento
    float cronoLeve;            // tiros leves entre as investidas

    // 3ª barra: ondas de choque e chão fragmentado
    int padrao3;                // 0 = investidas, 1 = ondas de choque
    float tempoPadrao3;
    bool ondaAtiva, ondaAcertou;
    float ondaRaio, ondaBrecha, avisoOnda;
    int ondasRestantes;
    Vector2 ondaCentro;
    Rectangle piso[NUM_PISO_LUCI];
    int fasePiso[NUM_PISO_LUCI];        // 0 = inteiro, 1 = rachando (aviso), 2 = caiu
    float tempoPiso[NUM_PISO_LUCI];
    float tickPiso;

    bool cenaVista;             // a cena da transformação já passou (a barra aparece durante a revelação)

    // Etapa 2: orbes de falácia (2ª barra) e varredura de dados (3ª barra)
    OrbeFalacia orbe[LUCI_NUM_ORBES];
    int orbesRestantes, rebatidosRajada;
    float cronoOrbe, cronoRajada, avisoRajada;
    float atordoadaOrbe;        // um orbe rebatido abriu a guarda
    bool varreduraAtiva, varreduraAcertou;
    float varreduraAng, varreduraSentido, varreduraFeita, avisoVarredura;
    float superaquecida;        // depois da varredura: guarda aberta
    int proximoEspecial;        // 3ª barra: 1 = ondas de choque, 2 = varredura (alternam)

    // Etapa 3 (visual): o corredor da investida enche durante a telegrafia; o rastro da investida
    float telegrafiaTotal;
    Vector2 rastro[6];
    float cronoRastro;

    // Etapa 5: falas curtas (recado no HUD, não pausam a luta)
    float cronoFala;            // intervalo mínimo entre os comentários
    int falaParry;              // alterna as falas do parry
    bool falouVidaBaixa;

    // Estatísticas (aparecem no resultado e na telemetria)
    int parriesPerfeitos;
    int combosConcluidos;
} LuciChefe;

// Índice de desempenho: cada parte com o seu peso (soma 100)
typedef struct {
    int estabilidade;           // 20: estabilidade final da Luci
    int acertos;                // 25: desafios, afirmações e puzzles
    int verificacao;            // 15: alucinações pegas x confiança cega
    int contradicoes;           // 15: contradições flagradas x deixadas passar
    int conceitos;              // 12: conceitos descobertos na enciclopédia
    int sobrevivencia;          //  8: poucas derrotas
    int pontos;                 //  5: pontuação
    int total;
} IndiceFinal;

// Mini-chefe JANELA DE CONTEXTO: o que está na janela dele (slot[0] = o mais antigo)
enum { CTX_SISTEMA, CTX_RELEVANTE, CTX_RUIDO };

typedef struct {
    bool ativo;
    int indice;                 // o mini-chefe no vetor de inimigos
    int fase;                   // 1 a 3 (a janela cresce e ele fica mais esperto)
    int capacidade;
    int num;
    unsigned char slot[JANELA_MAX_SLOTS + 1];
    Vector2 memoria[JANELA_MAX_SLOTS + 1];  // dado relevante: onde o jogador estava
    float ruidoAcumulado;       // dano no escudo que ainda não virou um bloco
    float vulneravel;           // > 0: a instrução PROTEGER foi esquecida (escudo aberto)
    float emissor;              // próximo dado dourado
    float resumo;               // próximo resumo (fase 2 em diante)
    float esquecidoAnim;        // o bloco que acabou de sair da janela
    int esquecidoTipo;
    float entrouAnim;
    int ciclos;                 // quantas vezes o escudo caiu
    int estouros;               // blocos esquecidos
    char dica[128];             // uma linha curta embaixo da janela (muda com a fase)
    bool recompensaDada;
    float reforco;              // próximo reforço de ajudantes (alucinações e ruído)
} JanelaContexto;

// ----- Variáveis compartilhadas -----
extern const int PRECO_MELHORIA[NIVEL_MAX_MELHORIA];
extern const int PESO_RARIDADE[NUM_RARIDADES];
extern const char *NOMES_RARIDADE[NUM_RARIDADES];
extern const Color CORES_RARIDADE[NUM_RARIDADES];
extern const int CHANCE_LUCI_ALUCINAR_B2[ESTABILIDADE_MAX_LUCI + 1];
extern const Rectangle ARENA_ROGUE_PADRAO;
extern Rectangle arenaRogueAtual;
extern const Rectangle ARENA_LUCI;
extern Texture2D texMapaLuci;
extern const Rectangle PORTA_PROXIMA_ROGUE;
extern const Rectangle PORTA_SAIDA_ROGUE;
extern const Rectangle PORTA_BLOCO3_ROGUE;
extern const Vector2 SPAWN_ROGUE;
extern const Vector2 SPAWN_ATRIO;
extern const Vector2 POS_BYTE;
extern const Vector2 POS_BYTECO;
extern const Vector2 POS_TERMINAL;
extern const Vector2 POS_ENCICLOPEDIA;
extern const Vector2 POS_TERMINAL_SELETOR;
extern const Vector2 POS_TERMINAL_BLOCO3;
extern const char *TEXTO_ATALHO_BLOCO2;
extern const char *TEXTO_ATALHO_BLOCO3;
extern const char *NOMES_CHEFES_SELETOR[NUM_CHEFES_SELETOR];
extern const char *TIPOS_CHEFES_SELETOR[NUM_CHEFES_SELETOR];
extern const char *DESCRICOES_CHEFES_SELETOR[NUM_CHEFES_SELETOR];
extern const Color CORES_CHEFES_SELETOR[NUM_CHEFES_SELETOR];
extern const Vector2 POS_PEDESTAIS[NUM_PEDESTAIS];
extern const int JANELA_CAPACIDADE[4];
extern const float JANELA_EMISSAO[4];
extern const float JANELA_RESPOSTA[4];
extern const float JANELA_RESUMO[4];
extern const float TOKEN_VELOCIDADE[4];
extern const char *NOMES_ESPECIAIS[NUM_ESPECIAIS];
extern const char *FALAS_LUCI_ESPECIAL[NUM_ESPECIAIS];
extern const DefInimigo DEF_INIMIGOS[NUM_TIPOS_INIMIGO];
extern const DefItem ITENS[NUM_ITENS];
extern const char *NOMES_ALCANCE[];
extern const DefArma DEF_ARMAS[NUM_ARMAS];
extern const char *NOMES_CATEGORIA_UP[NUM_CATEGORIAS_UP];
extern const Color CORES_CATEGORIA_UP[NUM_CATEGORIAS_UP];
extern const DefUpgrade UPGRADES[NUM_UPGRADES];
extern const char *NOMES_RECOMPENSA[NUM_TIPOS_RECOMPENSA];
extern const float PROF_DIFICULDADE[MAPA_PROFUNDIDADES];
extern const Rectangle PORTA_LADO_ROGUE[3];
extern const Vector2 ENTRADA_LADO_ROGUE[NUM_LADOS];
extern const Rectangle PAREDE_SECRETA_ROGUE[4];
extern const char *NOMES_OBJC[NUM_OBJC];
extern const char *NOMES_AFIXOS[NUM_AFIXOS];
extern const char *NOMES_LAB[NUM_LABS];
extern const char *PLACAS_LAB[NUM_LABS];
extern const Color CORES_LAB[NUM_LABS];
extern const bool LAB_ESPECIAL[NUM_LABS];
extern const char *FALAS_LUCI_LAB[NUM_LABS];
extern const char *NOMES_SALAS_ROGUE[NUM_TIPOS_SALA];
extern const Color CORES_SALAS_ROGUE[NUM_TIPOS_SALA];
extern const char *PLACAS_SALAS_ROGUE[NUM_TIPOS_SALA];
extern const float LUCI_FRACAO_FASE[3];
extern const float MELEE_FRACAO[3];
extern const char *FALACIAS_LUCI[][2];

// ----- Funções -----
Rectangle ArenaRogue(void);
Vector2 PontoNaArena(Vector2 p);
Rectangle RetanguloNaArena(Rectangle r);
void CarregarMapasNovos(void);
void DescarregarMapasNovos(void);
const char *RoguePlacaNo(const NoMapa *n);

#endif // UTOPIA_BIBLIOTECA_TIPOS_H
