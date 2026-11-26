#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <time.h>
#include "../../include/core/fonte.h"
#include "../../include/core/mapas.h"
#include "../../include/entidades/jogador.h"
#include "../../include/audio/audio.h"
#include "../../include/core/save.h"
#include "../../include/core/telemetria.h"
#include "../../include/core/texto.h"

// Pastas onde o jogo procura as folhas do personagem, na ordem.
// "" = pasta do repositório (de onde o jogo é executado).
static const char *PASTAS_SPRITES_JOGADOR[] = { "", "resources/sprites/player/", "resources/sprites/save/", "resources/sprites/luci/" };

// =============================================================
// FUNÇÕES AUXILIARES - JOGADOR
// =============================================================

// Lê W A S D, move o jogador (sem ficar mais rápido na diagonal)
// e devolve para qual lado o sprite deve olhar.
Direcao MoverJogador(Vector2 *pos, Direcao direcaoAtual)
{
    Vector2 direcao = {0};

    if (IsKeyDown(KEY_D)) direcao.x += 1.0f;
    if (IsKeyDown(KEY_A)) direcao.x -= 1.0f;
    if (IsKeyDown(KEY_S)) direcao.y += 1.0f;
    if (IsKeyDown(KEY_W)) direcao.y -= 1.0f;   // para cima = de costas

    // Parado (ou apertando lados opostos): continua olhando para onde estava
    if (Vector2Length(direcao) == 0.0f)
        return direcaoAtual;

    Direcao nova;

    if      (direcao.y > 0 && direcao.x > 0) nova = DIR_FRENTE_DIREITA;
    else if (direcao.y > 0 && direcao.x < 0) nova = DIR_FRENTE_ESQUERDA;
    else if (direcao.y < 0 && direcao.x > 0) nova = DIR_COSTAS_DIREITA;
    else if (direcao.y < 0 && direcao.x < 0) nova = DIR_COSTAS_ESQUERDA;
    else if (direcao.y > 0)                  nova = DIR_FRENTE;
    else if (direcao.y < 0)                  nova = DIR_COSTAS;
    else if (direcao.x > 0)                  nova = DIR_DIREITA;
    else                                     nova = DIR_ESQUERDA;

    // Normaliza para a diagonal não ficar mais rápida
    direcao = Vector2Normalize(direcao);
    *pos = Vector2Add(*pos, Vector2Scale(direcao, VELOCIDADE_JOGADOR * DtJogo()));

    return nova;
}

// Hitbox menor, usada para colidir com a Luci
Rectangle HitboxJogador(Vector2 p)
{
    return (Rectangle){ p.x - TAM_SPRITE / 4, p.y - TAM_SPRITE / 4, TAM_SPRITE / 2, TAM_SPRITE / 2 };
}

// Área usada para portas e monitores
Rectangle AreaJogador(Vector2 p)
{
    return (Rectangle){ p.x - 18, p.y - 22, 36, 44 };
}

// Pés do personagem: é o que bate nas paredes invisíveis
// (a cabeça pode passar "por trás" da parede, como em jogos vistos de cima)
Rectangle PesJogador(Vector2 p)
{
    return (Rectangle){ p.x - 14, p.y + ALTURA_PERSONAGEM / 2 - 8, 28, 8 };
}

// Algum retângulo encosta em alguma parede invisível da sala?
static bool BateNasParedes(Rectangle r)
{
    for (int i = 0; i < NUM_PAREDES_SALA; i++)
        if (CheckCollisionRecs(r, PAREDES_SALA[i]))
            return true;

    return false;
}

// Impede o jogador de atravessar as paredes invisíveis.
// Testa X e Y separados, assim ele desliza pela parede em vez de travar.
// Algum retângulo encosta em alguma parede da lista?
static bool BateEmParedes(Rectangle r, const Rectangle *paredes, int n)
{
    for (int i = 0; i < n; i++)
        if (CheckCollisionRecs(r, paredes[i]))
            return true;

    return false;
}

// Mesma ideia de ColidirComParedes, para outra lista de paredes (X e Y separados: desliza na parede)
void ColidirComLista(Vector2 *pos, Vector2 posAnterior, const Rectangle *paredes, int n)
{
    // Já estava dentro de uma parede (ex.: save antigo): deixa sair
    if (BateEmParedes(PesJogador(posAnterior), paredes, n))
        return;

    Vector2 teste = { pos->x, posAnterior.y };

    if (BateEmParedes(PesJogador(teste), paredes, n))
        teste.x = posAnterior.x;

    teste.y = pos->y;

    if (BateEmParedes(PesJogador(teste), paredes, n))
        teste.y = posAnterior.y;

    *pos = teste;
}

void ColidirComParedes(Vector2 *pos, Vector2 posAnterior)
{
    // Já estava dentro de uma parede (ex.: save antigo): deixa sair
    if (BateNasParedes(PesJogador(posAnterior)))
        return;

    Vector2 teste = { pos->x, posAnterior.y };

    if (BateNasParedes(PesJogador(teste)))
        teste.x = posAnterior.x;

    teste.y = pos->y;

    if (BateNasParedes(PesJogador(teste)))
        teste.y = posAnterior.y;

    *pos = teste;
}


// Procura o arquivo nas pastas de PASTAS_SPRITES_JOGADOR.
// Devolve o caminho encontrado, ou NULL se não estiver em nenhuma.
static const char *AcharSpriteJogador(const char *nome)
{
    static char caminho[512];

    for (int i = 0; i < NUM_PASTAS_SPRITES_JOGADOR; i++) {

        snprintf(caminho, sizeof(caminho), "%s%s", PASTAS_SPRITES_JOGADOR[i], nome);

        if (FileExists(caminho))
            return caminho;
    }

    TraceLog(LOG_WARNING, "SPRITE: '%s' nao encontrado na pasta do repositorio nem em resources/sprites/", nome);
    return NULL;
}

// Acha o personagem dentro de cada quadro, ignorando o espaço vazio em volta.
// Assim o tamanho fica certo em qualquer resolução e o personagem não
// "treme" quando os quadros não estão igualmente espaçados na folha.
static void AcharRecortes(Image img, int quadros, Rectangle *recorte, float *alturaRef)
{
    Color *px = LoadImageColors(img);
    int larguraQuadro = img.width / quadros;

    for (int q = 0; q < quadros; q++) {

        int x0 = q * larguraQuadro;
        int minX = img.width, maxX = -1, minY = img.height, maxY = -1;

        for (int y = 0; y < img.height; y++) {
            for (int x = x0; x < x0 + larguraQuadro; x++) {

                if (px[y * img.width + x].a > 128) {
                    if (x < minX) minX = x;
                    if (x > maxX) maxX = x;
                    if (y < minY) minY = y;
                    if (y > maxY) maxY = y;
                }
            }
        }

        if (maxX < 0)   // quadro vazio: usa o quadro inteiro
            recorte[q] = (Rectangle){ (float)x0, 0, (float)larguraQuadro, (float)img.height };
        else
            recorte[q] = (Rectangle){ (float)minX, (float)minY, (float)(maxX - minX + 1), (float)(maxY - minY + 1) };

        if (alturaRef != NULL && recorte[q].height > *alturaRef)
            *alturaRef = recorte[q].height;
    }

    UnloadImageColors(px);
}

// A folha "parado" da mesma direção ("Thomas_andando_x.png" -> "Thomas_parado_x.png"): 2 quadros,
// olhos abertos e fechados. Só existe nas direções em que o rosto aparece.
static void CarregarParado(SpriteDirecao *s, const char *nomeAndando)
{
    const char *p = strstr(nomeAndando, "andando");
    if (p == NULL)
        return;

    char nome[160];
    snprintf(nome, sizeof(nome), "%.*sparado%s", (int)(p - nomeAndando), nomeAndando, p + 7);

    const char *caminho = NULL;
    for (int i = 0; i < NUM_PASTAS_SPRITES_JOGADOR && caminho == NULL; i++) {
        static char tentativa[512];
        snprintf(tentativa, sizeof(tentativa), "%s%s", PASTAS_SPRITES_JOGADOR[i], nome);
        if (FileExists(tentativa)) caminho = tentativa;
    }
    if (caminho == NULL)
        return;

    Image img = LoadImage(caminho);
    AcharRecortes(img, 2, s->recorteParado, NULL);
    s->texParado = LoadTextureFromImage(img);
    s->temParado = true;
    UnloadImage(img);
}

// Carrega a folha de animação; se ela não existir, usa a imagem antiga
SpriteDirecao CarregarSprite(const char *folha, const char *antigo, bool espelharAntigo)
{
    SpriteDirecao s = {0};
    const char *nome = folha;

    folha = AcharSpriteJogador(folha);

    if (folha != NULL) {

        Image img = LoadImage(folha);

        s.textura = LoadTextureFromImage(img);
        s.quadros = QUADROS_ANDANDO;
        AcharRecortes(img, QUADROS_ANDANDO, s.recorte, &s.alturaRef);
        UnloadImage(img);

        // Imagem muito maior que o tamanho na tela (ex.: a arte original de 909x369):
        // suaviza a redução para não "picotar" os pixels
        if (s.alturaRef > ALTURA_PERSONAGEM * 2) {
            GenTextureMipmaps(&s.textura);
            SetTextureFilter(s.textura, TEXTURE_FILTER_TRILINEAR);
        }

        CarregarParado(&s, nome);

    } else {

        s.textura = LoadTexture(antigo);
        s.quadros = 1;
        s.espelhar = espelharAntigo;
    }

    return s;
}

// Diagonal: usa a folha própria se ela existir; senão, empresta a folha de lado
SpriteDirecao CarregarSpriteDiagonal(const char *folha, SpriteDirecao reserva)
{
    if (AcharSpriteJogador(folha) != NULL)
        return CarregarSprite(folha, folha, false);

    reserva.emprestada = true;
    return reserva;
}

// =============================================================
// ANIMAÇÃO EXTRA DO THOMAS (feita no código; liga e desliga em config.h)
//   balanço do passo, poeira, sombra, respiração, piscar, esticar/achatar e reação ao dano
// =============================================================

#define MAX_POEIRA_THOMAS 12
#define TEMPO_DANO_THOMAS 0.35f

typedef struct { Vector2 pos, vel; float vida; } PoeiraThomas;

static struct {
    bool andando;
    float parado;              // há quanto tempo está parado
    float proximaPiscada;      // contagem até a próxima piscada
    float piscando;            // > 0 = olhos fechados agora
    float esticar, velEsticar; // > 0 achata, < 0 estica (uma molinha que volta ao normal)
    float dano;                // tempo do vermelho e do tremor depois de um golpe
    float ritmo;               // multiplica a troca de quadros (velocidade do passo)
    double atualizado;         // quando a animação foi atualizada pela última vez
    Vector2 deslocCorpo;       // quanto o corpo saiu do lugar no último desenho (balanço e tremor)
    int quadroAnt;
    PoeiraThomas poeira[MAX_POEIRA_THOMAS];
} g_thomas = { .proximaPiscada = 3.0f, .ritmo = 1.0f };

static void ThomasEsticar(float quanto)
{
    if (THOMAS_ESTICAR) { g_thomas.esticar = quanto; g_thomas.velEsticar = 0.0f; }
}

Vector2 DeslocamentoCorpoJogador(void) { return g_thomas.deslocCorpo; }
void RitmoPassoJogador(float fator) { g_thomas.ritmo = (fator > 0.2f) ? fator : 0.2f; }
void JogadorReagirDano(void)        { g_thomas.dano = TEMPO_DANO_THOMAS; ThomasEsticar(0.7f); }
void JogadorReagirEsquiva(void)     { ThomasEsticar(-0.9f); }

// Andando: avança os quadros. Parado: volta para o quadro 0 (pés juntos).
// Também anda o estado visual (molinha, piscar, poeira, dano).
void AtualizarAnimacao(AnimacaoJogador *a, bool andando, float dt)
{
    float ritmo = g_thomas.ritmo;
    g_thomas.ritmo = 1.0f;                              // o ritmo vale só para esta chamada
    g_thomas.atualizado = GetTime();

    if (andando && !g_thomas.andando)  ThomasEsticar(-0.45f);   // arrancou: estica
    if (!andando && g_thomas.andando)  ThomasEsticar(0.55f);    // parou: achata
    g_thomas.andando = andando;
    g_thomas.parado = andando ? 0.0f : g_thomas.parado + dt;

    // molinha amortecida: passa um pouco do ponto e assenta (sensação de peso)
    float acel = -260.0f * g_thomas.esticar - 16.0f * g_thomas.velEsticar;
    g_thomas.velEsticar += acel * dt;
    g_thomas.esticar += g_thomas.velEsticar * dt;

    if (g_thomas.dano > 0.0f) g_thomas.dano -= dt;

    // piscar: só parado, a cada 2 a 5 s (às vezes duas piscadas seguidas)
    if (g_thomas.piscando > 0.0f) g_thomas.piscando -= dt;
    if (!andando && g_thomas.parado > 0.4f) {
        g_thomas.proximaPiscada -= dt;
        if (g_thomas.proximaPiscada <= 0.0f) {
            g_thomas.piscando = 0.12f;
            g_thomas.proximaPiscada = (GetRandomValue(0, 99) < 15) ? 0.25f : 2.0f + (float)GetRandomValue(0, 300) / 100.0f;
        }
    }

    for (int i = 0; i < MAX_POEIRA_THOMAS; i++) {
        PoeiraThomas *p = &g_thomas.poeira[i];
        if (p->vida <= 0.0f) continue;
        p->vida -= dt;
        p->pos.x += p->vel.x * dt;
        p->pos.y += p->vel.y * dt;
        p->vel.y += 20.0f * dt;
    }

    if (!andando) {

        a->quadro = 0;
        a->tempo = 0.0f;
        return;
    }

    a->tempo += dt * ritmo;

    if (a->tempo >= 1.0f / FPS_ANIMACAO) {

        a->tempo -= 1.0f / FPS_ANIMACAO;
        a->quadro = (a->quadro + 1) % QUADROS_ANDANDO;
    }
}

static void SoltarPoeira(Vector2 pe)
{
    for (int n = 0, i = 0; i < MAX_POEIRA_THOMAS && n < 2; i++) {
        PoeiraThomas *p = &g_thomas.poeira[i];
        if (p->vida > 0.0f) continue;
        float lado = (n == 0) ? -1.0f : 1.0f;
        p->pos = (Vector2){ pe.x + lado * (float)GetRandomValue(2, 6), pe.y };
        p->vel = (Vector2){ lado * (float)GetRandomValue(10, 22), -(float)GetRandomValue(8, 18) };
        p->vida = 0.35f;
        n++;
    }
}

// sprites[] é indexado pela direção (mesma ordem do enum Direcao)
void DesenharJogador(const SpriteDirecao sprites[NUM_DIRECOES], Vector2 pos, Direcao direcao, int quadro)
{
    const SpriteDirecao *s = &sprites[direcao];
    Texture2D t = s->textura;

    if (s->quadros > 1) {

        // Ninguém atualizou a animação agora há pouco (diálogo, loja, menu aberto): ele está parado
        if (GetTime() - g_thomas.atualizado > 0.1)
            g_thomas.andando = false;

        // Folha de animação: recorta só o personagem do quadro atual
        Rectangle sourceRec = s->recorte[quadro % s->quadros];
        bool parado = !g_thomas.andando;

        // Parado, com a folha "parado": olhos abertos ou fechados (piscando)
        if (parado && s->temParado) {
            t = s->texParado;
            sourceRec = s->recorteParado[(THOMAS_PISCAR && g_thomas.piscando > 0.0f) ? 1 : 0];
        }

        // A escala vem da altura do PERSONAGEM na imagem: qualquer resolução
        // aparece com ALTURA_PERSONAGEM pixels de altura na tela.
        float escala = ALTURA_PERSONAGEM / s->alturaRef;
        float w = roundf(sourceRec.width * escala);
        float h = roundf(sourceRec.height * escala);

        // O topo da cabeça fica sempre na mesma altura (os pés mudam a cada passo);
        // o esticar/achatar acontece a partir dos pés
        float topo = roundf(pos.y - ALTURA_PERSONAGEM / 2);
        float pes = topo + h;

        // Balanço: nos quadros em que um pé está no ar, o corpo sobe um pixel da arte
        float sobe = (THOMAS_BALANCO && g_thomas.andando && (quadro % 2) == 1) ? roundf(2.0f * escala) : 0.0f;

        float e = THOMAS_ESTICAR ? Clamp(g_thomas.esticar, -1.0f, 1.0f) : 0.0f;
        float resp = (THOMAS_RESPIRAR && parado) ? 0.02f * sinf(g_thomas.parado * 2.0f * PI / 2.4f) : 0.0f;
        float sx = 1.0f + 0.14f * e - 0.5f * resp;
        float sy = 1.0f - 0.14f * e + resp;
        float w2 = roundf(w * sx), h2 = roundf(h * sy);

        float tremor = 0.0f;
        Color cor = WHITE;
        if (THOMAS_DANO && g_thomas.dano > 0.0f) {
            float k = g_thomas.dano / TEMPO_DANO_THOMAS;
            tremor = (((int)(g_thomas.dano * 40.0f)) % 2 ? 1.0f : -1.0f) * roundf(escala);
            cor = (Color){ 255, (unsigned char)(255 - 150 * k), (unsigned char)(255 - 150 * k), 255 };
        }

        // Sombra oval embaixo dos pés (menor quando ele sobe)
        if (THOMAS_SOMBRA) {
            float k = (sobe > 0.0f) ? 0.85f : 1.0f;
            DrawEllipse((int)pos.x, (int)(pes - 1), w * 0.34f * k, 3.0f * k, Fade(BLACK, 0.25f));
        }

        // Poeira: sai quando o pé toca o chão (quadros 0 e 2 da caminhada)
        if (THOMAS_POEIRA && g_thomas.andando && quadro != g_thomas.quadroAnt && (quadro % 2) == 0)
            SoltarPoeira((Vector2){ pos.x, pes - 1 });
        g_thomas.quadroAnt = quadro;
        for (int i = 0; i < MAX_POEIRA_THOMAS; i++) {
            const PoeiraThomas *p = &g_thomas.poeira[i];
            if (p->vida <= 0.0f) continue;
            float k = p->vida / 0.35f;
            DrawCircleV(p->pos, 1.5f + 2.0f * (1.0f - k), Fade((Color){ 210, 200, 190, 255 }, 0.5f * k));
        }

        g_thomas.deslocCorpo = (Vector2){ tremor, -sobe };

        // Posição arredondada para os pixels ficarem nítidos
        Rectangle destRec = {
            roundf(pos.x - w2 / 2 + tremor),
            roundf(pes - h2 - sobe),
            w2,
            h2
        };

        DrawTexturePro(t, sourceRec, destRec, (Vector2){ 0, 0 }, 0.0f, cor);

    } else {

        // Imagem antiga parada (65x65)
        float flip = s->espelhar ? -1.0f : 1.0f;

        Rectangle sourceRec = { 0.0f, 0.0f, (float)t.width * flip, (float)t.height };
        Rectangle destRec = { pos.x, pos.y, TAM_SPRITE, TAM_SPRITE };
        Vector2 origin = { TAM_SPRITE / 2, TAM_SPRITE / 2 };

        DrawTexturePro(t, sourceRec, destRec, origin, 0.0f, WHITE);
    }
}

static Texture2D texLuci = {0};
static Rectangle recorteLuci = {0};  // só a parte desenhada da imagem, sem o espaço vazio em volta

// Fala: a folha e o recorte de cada quadro (todos do mesmo tamanho, centrados no desenho)
static Texture2D texLuciFala = {0};
static Rectangle quadrosFalaLuci[MAX_QUADROS_FALA_LUCI];
static int numQuadrosFalaLuci = 0;
bool luciFalando = false;       // a Luci está falando agora (o main atualiza a cada quadro)
float tempoFalaLuci = 0.0f;

EmocaoLuci emocaoLuci = EMOCAO_NENHUMA;
float tempoEmocaoLuci = 0.0f;
float brilhoLuci = 1.0f;        // cresce conforme o jogador resolve os desafios

// 0 = estável, 1 = sobrecarregada (de tanto pedirem ajuda). O main atualiza.
float instabilidadeLuci = 0.0f;

void MostrarEmocaoLuci(EmocaoLuci e, float segundos)
{
    emocaoLuci = e;
    tempoEmocaoLuci = segundos;
}

// Caixa do que está desenhado (alpha > 8) entre as colunas x0 e x1 da imagem
static Rectangle CaixaAlphaColunas(const Color *px, int largura, int altura, int x0, int x1)
{
    int minX = x1 + 1, maxX = -1, minY = altura, maxY = -1;

    for (int y = 0; y < altura; y++) {
        for (int x = x0; x <= x1; x++) {
            if (px[y * largura + x].a > 8) {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    if (maxX < 0)
        return (Rectangle){ 0 };

    return (Rectangle){ (float)minX, (float)minY, (float)(maxX - minX + 1), (float)(maxY - minY + 1) };
}

static bool ColunaTemDesenho(const Color *px, int largura, int altura, int x)
{
    for (int y = 0; y < altura; y++)
        if (px[y * largura + x].a > 8)
            return true;

    return false;
}

// Carrega a arte (recortada e já reduzida) e a folha da fala com os quadros achados pelo alpha
void CarregarLuci(void)
{
    // Sprite parado: a arte nova ou, sem ela, a antiga
    const char *arquivo = AcharSpriteJogador(SPRITE_LUCI);

    if (arquivo == NULL)
        arquivo = AcharSpriteJogador(SPRITE_LUCI_ANTIGA);

    if (arquivo != NULL) {

        Image img = LoadImage(arquivo);
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

        Rectangle caixa = CaixaAlphaColunas((const Color *)img.data, img.width, img.height, 0, img.width - 1);
        if (caixa.width > 0.0f)
            ImageCrop(&img, caixa);                 // só a parte desenhada

        if (img.height > ALTURA_TEXTURA_LUCI)       // a imagem é bem maior que a Luci na tela
            ImageResize(&img, (int)roundf((float)img.width * ALTURA_TEXTURA_LUCI / (float)img.height), ALTURA_TEXTURA_LUCI);

        recorteLuci = (Rectangle){ 0, 0, (float)img.width, (float)img.height };
        texLuci = LoadTextureFromImage(img);

        // Ainda maior que o tamanho na tela: suaviza a redução
        GenTextureMipmaps(&texLuci);
        SetTextureFilter(texLuci, TEXTURE_FILTER_TRILINEAR);
        UnloadImage(img);
    }

    // Folha da fala: os quadros vêm separados por colunas transparentes. Cada um é achado pelo
    // alpha (nada de dividir a folha em partes iguais) e todos ganham o mesmo tamanho, centrado
    // no desenho, para a Luci não pular nem mudar de tamanho de um quadro para o outro.
    arquivo = AcharSpriteJogador(SPRITE_LUCI_FALANDO);

    if (arquivo == NULL)
        return;

    Image folha = LoadImage(arquivo);
    ImageFormat(&folha, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

    const Color *px = (const Color *)folha.data;
    Rectangle caixas[MAX_QUADROS_FALA_LUCI];
    int n = 0, inicio = -1, ultima = -1;

    for (int x = 0; x <= folha.width; x++) {

        if (x < folha.width && ColunaTemDesenho(px, folha.width, folha.height, x)) {
            if (inicio < 0) inicio = x;
            ultima = x;
            continue;
        }

        // Coluna vazia: um vão de 6 colunas (ou o fim da folha) fecha o quadro
        if (inicio >= 0 && (x - ultima >= 6 || x == folha.width)) {
            if (n < MAX_QUADROS_FALA_LUCI)
                caixas[n++] = CaixaAlphaColunas(px, folha.width, folha.height, inicio, ultima);
            inicio = -1;
        }
    }

    if (n < 2) {                                    // sem animação: fica o sprite parado
        UnloadImage(folha);
        return;
    }

    float w = 0.0f, h = 0.0f;

    for (int i = 0; i < n; i++) {
        if (caixas[i].width > w)  w = caixas[i].width;
        if (caixas[i].height > h) h = caixas[i].height;
    }

    // Mesmo tamanho para todos, centrado no desenho de cada quadro (sem sair da folha)
    for (int i = 0; i < n; i++) {
        float cx = caixas[i].x + caixas[i].width / 2.0f, cy = caixas[i].y + caixas[i].height / 2.0f;
        quadrosFalaLuci[i] = (Rectangle){
            Clamp(roundf(cx - w / 2.0f), 0.0f, (float)folha.width - w),
            Clamp(roundf(cy - h / 2.0f), 0.0f, (float)folha.height - h),
            w, h
        };
    }

    // Reduz a folha para um quadro ter ALTURA_TEXTURA_LUCI de altura (como o sprite parado)
    if (h > ALTURA_TEXTURA_LUCI) {
        int larguraNova = (int)roundf((float)folha.width * ALTURA_TEXTURA_LUCI / h);
        int alturaNova  = (int)roundf((float)folha.height * ALTURA_TEXTURA_LUCI / h);
        float kx = (float)larguraNova / (float)folha.width, ky = (float)alturaNova / (float)folha.height;

        ImageResize(&folha, larguraNova, alturaNova);

        for (int i = 0; i < n; i++)
            quadrosFalaLuci[i] = (Rectangle){ quadrosFalaLuci[i].x * kx, quadrosFalaLuci[i].y * ky,
                                              quadrosFalaLuci[i].width * kx, quadrosFalaLuci[i].height * ky };
    }

    numQuadrosFalaLuci = n;
    texLuciFala = LoadTextureFromImage(folha);
    GenTextureMipmaps(&texLuciFala);
    SetTextureFilter(texLuciFala, TEXTURE_FILTER_TRILINEAR);
    UnloadImage(folha);
}

void DescarregarLuci(void)
{
    if (texLuci.id > 0)
        UnloadTexture(texLuci);

    if (texLuciFala.id > 0)
        UnloadTexture(texLuciFala);

    texLuci = (Texture2D){0};
    texLuciFala = (Texture2D){0};
    numQuadrosFalaLuci = 0;
}

// Quadro atual da fala: do brilho fraco ao forte e de volta (0 1 2 3 4 5 4 3 2 1 0 ...)
static int QuadroFalaLuci(void)
{
    int n = numQuadrosFalaLuci;

    if (n <= 1)
        return 0;

    int ciclo = 2 * n - 2;
    int i = (int)(tempoFalaLuci * FPS_FALA_LUCI) % ciclo;

    return (i < n) ? i : ciclo - i;
}

// O main chama a cada quadro. Parou de falar: volta na hora ao sprite parado.
void AtualizarFalaLuci(bool falando, float dt)
{
    luciFalando = falando;
    tempoFalaLuci = falando ? tempoFalaLuci + dt : 0.0f;
}

// O texto é fala da Luci? ("Luci:" no começo, sem ligar para espaços e maiúsculas)
bool TextoDeFalaDaLuci(const char *t)
{
    static const char nome[] = "luci";

    if (t == NULL)
        return false;

    while (*t == ' ' || *t == '\n' || *t == '\t')
        t++;

    for (int i = 0; i < 4; i++) {
        char c = t[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        if (c != nome[i]) return false;
    }

    t += 4;

    while (*t == ' ')
        t++;

    return *t == ':';
}

// A página aberta do diálogo é fala da Luci? (não basta o objeto da conversa: várias falas
// dela acontecem em conversas de outros objetos, e o Byte e o Byteco falam em outras)
bool DialogoFalaDaLuci(const SessaoDialogo *d)
{
    if (d->estado != DLG_PAGINAS && d->estado != DLG_FEEDBACK && d->estado != DLG_CONFIRMA)
        return false;

    return TextoDeFalaDaLuci(d->texto);
}

// Desenha a Luci com centro em p e a altura pedida.
// Animações dela: brilho pulsando em volta e as asas batendo
// (o desenho estica e encolhe um pouco na horizontal).
// A flutuação e a órbita em volta do jogador vêm de quem chama.
// Transparência do próximo desenho da Luci (1 = normal). Usada nos rastros da investida e nas falhas da 3ª barra.
float g_alfaLuci = 1.0f;
// Transformações do próximo desenho da Luci (o companion usa; o resto do jogo deixa como está)
float g_luciGiro = 0.0f;                         // graus
float g_luciEsticaX = 1.0f, g_luciEsticaY = 1.0f;
float g_luciAsasFase = -1.0f, g_luciAsasAmp = 0.07f;   // fase < 0: as asas no ritmo padrão

void DesenharLuciTamanho(Vector2 p, float altura)
{
    float t = (float)GetTime();
    float pulso = 0.5f + 0.5f * sinf(t * 3.0f);

    DrawCircleV(p, altura * (0.55f + 0.07f * brilhoLuci), Fade(SKYBLUE, (0.10f + 0.08f * pulso) * brilhoLuci * g_alfaLuci));

    // Falando: o quadro da folha da fala no lugar do sprite parado (mesmo centro e mesma altura)
    Texture2D tex = texLuci;
    Rectangle recorte = recorteLuci;

    if (luciFalando && texLuciFala.id > 0 && numQuadrosFalaLuci > 0) {
        tex = texLuciFala;
        recorte = quadrosFalaLuci[QuadroFalaLuci()];
    }

    if (tex.id > 0) {

        float escala = altura / recorte.height;
        float asas = (g_luciAsasFase >= 0.0f) ? 1.0f + g_luciAsasAmp * sinf(g_luciAsasFase) : 1.0f + 0.07f * sinf(t * 14.0f);
        float w = recorte.width * escala * asas * g_luciEsticaX;
        float h = recorte.height * escala * g_luciEsticaY;

        // Desenhada pelo centro (o giro e a inclinação do companion giram em volta dele)
        Rectangle destino = { p.x, p.y, w, h };
        Vector2 origem = { w / 2, h / 2 };

        // Instável: puxa para o magenta e, perto do limite, falha como as alucinações
        float k = instabilidadeLuci;
        Color tinta = Fade((Color){ 255, (unsigned char)(255 - 140 * k), (unsigned char)(255 - 40 * k), 255 }, g_alfaLuci);

        if (k > 0.5f) {
            float d = 2.0f + 3.0f * k;
            DrawTexturePro(tex, recorte, (Rectangle){ destino.x - d, destino.y, w, h }, origem, g_luciGiro, Fade(RED, 0.5f * g_alfaLuci));
            DrawTexturePro(tex, recorte, (Rectangle){ destino.x + d, destino.y, w, h }, origem, g_luciGiro, Fade(SKYBLUE, 0.5f * g_alfaLuci));
        }

        DrawTexturePro(tex, recorte, destino, origem, g_luciGiro, tinta);
        return;
    }

    // Sem a imagem: o quadradinho antigo
    float lado = altura * 0.4f;
    Rectangle corpo = { p.x - lado / 2, p.y - lado / 2, lado, lado };

    DrawRectangleRec(corpo, SKYBLUE);
    DrawRectangleLinesEx(corpo, 2, BLUE);
}
