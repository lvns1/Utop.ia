// Constantes de ajuste do jogo (tela, sprites, corredor, save, batalha).
// Só #define: dá para incluir em qualquer arquivo.
#ifndef UTOPIA_CONFIG_H
#define UTOPIA_CONFIG_H


#define LARGURA_TELA 800
#define ALTURA_TELA  600

// =============================================================
// SPRITES
// =============================================================

// Folhas de animação do personagem: 4 quadros lado a lado, TODOS DO MESMO
// TAMANHO e com o personagem no mesmo lugar de cada quadro (versão limpa
// de 152x60, quadros de 38x60). O quadro 0 é o personagem parado (pés juntos).
// Andando para CIMA ele aparece DE COSTAS (Andando_costas.png).
// As folhas "Thomas_andando_*" são a caminhada nova (pé levantando e braços balançando);
// as "Andando_*" originais continuam na pasta: para voltar, troque os nomes abaixo.
// Cada folha pode ter uma "Thomas_parado_*" de 2 quadros (olhos abertos | fechados) para ele piscar.
#define SPRITE_ANDANDO_FRENTE    "Thomas_andando_de_frente.png"
#define SPRITE_ANDANDO_COSTAS    "Thomas_andando_costas.png"
#define SPRITE_ANDANDO_DIREITA   "Thomas_andando_direita.png"
#define SPRITE_ANDANDO_ESQUERDA  "Thomas_andando_esquerda.png"

// Diagonais (3/4). Se alguma dessas folhas não for encontrada,
// aquela diagonal usa a folha de LADO do mesmo sentido.
#define SPRITE_ANDANDO_FRENTE_DIREITA   "Thomas_andando_diagonal_frente_direita.png"
#define SPRITE_ANDANDO_FRENTE_ESQUERDA  "Thomas_andando_diagonal_frente_esquerda.png"
#define SPRITE_ANDANDO_COSTAS_DIREITA   "Thomas_andando_diagonal_costas_direita.png"
#define SPRITE_ANDANDO_COSTAS_ESQUERDA  "Thomas_andando_diagonal_costas_esquerda.png"

// Animação extra do Thomas (feita no código). 1 = ligado, 0 = desligado (AJUSTE AQUI)
#define THOMAS_BALANCO   1     // o corpo sobe 1 pixel da arte a cada passo
#define THOMAS_POEIRA    1     // poeirinha nos pés quando o pé toca o chão
#define THOMAS_SOMBRA    1     // sombra oval embaixo dos pés (encolhe quando ele sobe)
#define THOMAS_RESPIRAR  1     // parado, o corpo "respira" bem de leve
#define THOMAS_PISCAR    1     // parado, ele pisca a cada 2 a 5 segundos
#define THOMAS_ESTICAR   1     // estica ao começar a andar e na esquiva; achata ao parar e ao levar dano
#define THOMAS_DANO      1     // levou dano: fica vermelho e treme um instante

// Impacto no combate (Blocos 2 e 3). 1 = ligado, 0 = desligado (AJUSTE AQUI)
#define COMBATE_NUMEROS_DANO 1   // números de dano subindo do alvo (golpes seguidos no mesmo alvo somam)
#define COMBATE_FAISCAS      1   // faísca no ponto em que o tiro acerta
#define COMBATE_POP          1   // o inimigo "incha" um instante ao levar dano

// Efeitos de tela (um shader no quadro final, junto com o filtro de nitidez). 1 = ligado, 0 = desligado (AJUSTE AQUI)
#define TELA_EFEITOS    1   // 0 = só o filtro de nitidez, como antes
#define TELA_VINHETA    1   // cantos mais escuros
#define TELA_LINHAS     1   // linhas de monitor entre as linhas de pixels (só com a imagem esticada 2x ou mais)
#define TELA_BRILHO     1   // brilho de neon: as cores claras vazam um pouco em volta
#define TELA_CORRUPCAO  1   // cresce com a corrupção da Luci: cores separadas nas bordas, sombras magenta e,
                            // perto do limite, faixas da tela que escorregam de vez em quando
#define TELA_GRAO       1   // granulado leve

// Iluminação das salas da missão (Bloco 2): penumbra com luz em volta do Thomas, dos inimigos, dos
// tiros, das portas e do que dá para usar. 1 = ligado, 0 = desligado (AJUSTE AQUI)
#define LUZ_SALAS           1
#define LUZ_FORCA_PENUMBRA  1.0f    // 1 = como foi feito; 0.5 = metade da escuridão; 0 = sem penumbra
#define LUZ_RAIO_THOMAS     175.0f  // até onde a luz do Thomas alcança (pixels)
#define LUZ_BRILHO_CHAO     0.16f   // quanto as luzes clareiam o chão (as poças de luz)

// Detalhes de vida. 1 = ligado, 0 = desligado (AJUSTE AQUI)
#define AMBIENTE_POEIRA     1       // poeira flutuando no ar das salas (brilha onde tem luz)
#define PORTAS_ANIMADAS     1       // as portas da missão abrem com animação quando a sala fica limpa
#define PORTAS_ABRIR_TEMPO  0.7f    // segundos da animação das portas
#define CAMERA_NA_MIRA      1       // a câmera adianta um pouco para o lado da mira (dá para desligar nas Opções)
#define CAMERA_MIRA_DIST    12.0f   // quanto ela adianta (pixels)

// Sprites (pixel art) dos Blocos 2 e 3. 1 = usa os PNGs de resources/sprites/biblioteca/ quando existem;
// 0 = sempre as formas desenhadas pelo código (como era antes)
#define SPRITES_INIMIGOS    1       // inimigos comuns, elites, mini-chefe e os do tutorial do Mestre
#define SPRITES_CHAO        1       // ladrilhos do chão da Biblioteca e do Núcleo

#define NUM_PASTAS_SPRITES_JOGADOR 4

#define QUADROS_ANDANDO     4
#define FPS_ANIMACAO        8.0f     // trocas de quadro por segundo enquanto anda (AJUSTE AQUI)
#define ALTURA_PERSONAGEM   58.0f    // altura do PERSONAGEM (não da imagem) na tela, em pixels (AJUSTE AQUI)
                                     // com as folhas limpas, 58 ou 116 deixam a pixel art nítida

// Imagens antigas (1 por direção), usadas só se as folhas acima não existirem
#define SPRITE_FRENTE          "resources/sprites/player/person.frente.png"
#define SPRITE_COSTAS          "resources/sprites/player/person.cima.png"
#define SPRITE_LADO            "resources/sprites/player/person.lado.png"

// Sprites dos monitores (opcionais). Enquanto os arquivos não existirem,
// o jogo desenha os monitores com formas simples.
#define SPRITE_MONITOR_NEUTRO  "resources/sprites/corredor/monitor.neutro.png"
#define SPRITE_MONITOR_ATIVO   "resources/sprites/corredor/monitor.ativo.png"
#define SPRITE_MONITOR_ACERTO  "resources/sprites/corredor/monitor.acerto.png"
#define SPRITE_MONITOR_ERRO    "resources/sprites/corredor/monitor.erro.png"

#define TAM_SPRITE          65.0f    // base das hitboxes (e tamanho das imagens antigas)
#define VELOCIDADE_JOGADOR  300.0f   // pixels por segundo (= 5 px por frame a 60 FPS)

// =============================================================
// CORREDOR - CONSTANTES
// =============================================================

#define TOTAL_MONITORES     3        // Bloco 1: o que é, tipos, riscos e mitigação
#define PONTOS_POR_ACERTO   10
#define PENALIDADE_ERRO     5        // resposta errada ou tempo esgotado (corredor e Bloco 2)
#define PENALIDADE_HP_ZERO  15       // a vida chegou a zero numa batalha
#define LIMITE_MORTES       3        // vidas da partida a partir do Bloco 2 (Biblioteca e luta contra a Luci). AJUSTE AQUI
                                     // (o Bloco 1, a sala da Mestre e o seletor de chefes não gastam vida)
// A pontuação nunca fica negativa
#define TEMPO_QUIZ          60.0f    // segundos da barra de tempo de cada quiz (AJUSTE AQUI)
#define ATRASO_RESPOSTA     0.4f     // evita responder sem querer apertando ENTER rápido demais
#define QUIZ_EM_ORDEM       1        // 1 = monitores na ordem 1-2-3-4 / 0 = qualquer ordem
#define VELOCIDADE_TEXTO    50.0f    // caracteres por segundo (máquina de escrever)
#define DURACAO_FADE        0.35f    // fade entre salas
#define DURACAO_FADE_BATALHA 0.2f    // fade rápido para entrar/sair da batalha

#define MAX_PAGINAS         10       // falas curtas (uma ideia por fala) pedem mais páginas
#define NUM_OPCOES          4

// Legibilidade (issue #31). Valem para TODO texto do jogo (fonte.c), em pixels da tela de 800x600.
#ifndef TAMANHO_MINIMO_TEXTO
#define TAMANHO_MINIMO_TEXTO   12.0f   // tamanho mínimo combinado pela equipe: nenhum texto fica menor que isto
#endif
#define ESCALA_TEXTO_GRANDE    1.2f    // opção TEXTO GRANDE: todo texto cresce 20%...
#define TAMANHO_TITULO_FIXO    28      // ... menos os títulos deste tamanho para cima (já são grandes)
#define CINZA_MINIMO_TEXTO     185     // cinza médio apagado (GRAY) clareia até este brilho no fundo escuro

#define FONTE_CAIXA         18
#define LARGURA_TEXTO_CAIXA 640
#define MAX_LINHAS_FALA_LUCI 2       // fala da Luci maior que isso é dividida em mais "páginas" (uma ideia por fala)
#define MAX_VOLTAR_DIALOGO   10      // quantas falas para trás o BACKSPACE consegue voltar numa conversa
#define FONTE_PERGUNTA      18
#define LARGURA_PERGUNTA    680
#define LARGURA_OPCAO       650

// Conversas que não são de monitor
#define OBJ_NENHUM  -1
#define OBJ_LUCI    -2
#define OBJ_CHEFE   -3                  // fala antes da ALUCINAÇÃO MESTRE

// =============================================================
// SAVE - CONSTANTES
// =============================================================

#define ARQUIVO_SAVE         "utopia.sav"
#define ARQUIVO_SAVE_TEMP    "utopia.sav.tmp"
#define VERSAO_SAVE          10           // aumente quando mudar o que vai no save (10 = números da run para os registros do Bloco 3)
#define ASSINATURA_SAVE      0x41495455u  // identifica o arquivo como save do UtopIA
#define TEMPO_AVISO_SAVE     2.0f         // segundos do aviso "JOGO SALVO" na tela

// Ajuda da Luci: cada pedido gasta 1 ponto de estabilidade.
// Estabilidade baixa = ela pode ALUCINAR a dica (errada, com toda a certeza).
// Acertar sem ajuda devolve 1 ponto.
#define ESTABILIDADE_MAX_LUCI  5

// =============================================================
// BATALHA - CONSTANTES
// =============================================================

#define MAX_BALAS            96
#define HP_MAX_BATALHA       20
#define VELOCIDADE_ALMA      170.0f
#define TEMPO_INVENCIVEL     1.0f
#define RAIO_BALA            7.0f
#define RAIO_ALMA            6.0f
#define DURACAO_MINIGAME     7.0f    // segundos de esquiva quando ERRA a resposta
#define DURACAO_COMBATE      11.0f   // segundos de esquiva quando o TEMPO ACABA
#define DURACAO_ANIM_CAIXA   0.35f
#define PREPARO_MINIGAME_LONGO 1.8f      // antes do minigame: a regra com ícones e "3, 2, 1" (as 2 primeiras vezes)
#define PREPARO_MINIGAME_CURTO 0.9f      // ... e depois
#define FONTE_BATALHA        20

#endif // UTOPIA_CONFIG_H
