// player.c - Implementação do jogador.

#include "player.h"
#include "raymath.h"
#include <math.h>

// Lê W A S D, move o jogador (sem ficar mais rápido na diagonal)
// e devolve para qual lado o sprite deve olhar.
Direcao MoverJogador(Vector2 *pos, Direcao direcaoAtual) {
    Vector2 direcao = {0};
    Direcao nova = direcaoAtual;

    if (IsKeyDown(KEY_D)) { direcao.x += 1.0f; nova = DIR_DIREITA; }
    if (IsKeyDown(KEY_A)) { direcao.x -= 1.0f; nova = DIR_ESQUERDA; }
    if (IsKeyDown(KEY_S)) { direcao.y += 1.0f; nova = DIR_FRENTE; }
    if (IsKeyDown(KEY_W)) { direcao.y -= 1.0f; nova = DIR_COSTAS; }  // para cima = de costas

    if (Vector2Length(direcao) > 0.0f) {
        direcao = Vector2Normalize(direcao);
        *pos = Vector2Add(*pos, Vector2Scale(direcao, VELOCIDADE_JOGADOR * GetFrameTime()));
    }
    return nova;
}

// Carrega a folha animada ou usa o sprite estático como alternativa.
SpriteDirecao CarregarSprite(
    const char *folha,
    const char *antigo,
    bool espelharAntigo
) {
    SpriteDirecao sprite = {0};

    if (FileExists(folha)) {
        sprite.textura = LoadTexture(folha);
        sprite.quadros = QUADROS_ANDANDO;
    } else {
        sprite.textura = LoadTexture(antigo);
        sprite.quadros = 1;
        sprite.espelhar = espelharAntigo;
    }

    return sprite;
}

// Avança a animação enquanto o personagem anda.
void AtualizarAnimacao(
    AnimacaoJogador *animacao,
    bool andando,
    float dt
) {
    if (!andando) {
        animacao->quadro = 0;
        animacao->tempo = 0.0f;
        return;
    }

    animacao->tempo += dt;

    if (animacao->tempo >= 1.0f / FPS_ANIMACAO) {
        animacao->tempo -= 1.0f / FPS_ANIMACAO;

        animacao->quadro =
            (animacao->quadro + 1) % QUADROS_ANDANDO;
    }
}

// Hitbox menor, usada para colidir com a Luci
Rectangle HitboxJogador(Vector2 p) {
    return (Rectangle){ p.x - TAM_SPRITE / 4, p.y - TAM_SPRITE / 4, TAM_SPRITE / 2, TAM_SPRITE / 2 };
}

// Área usada para portas e monitores
Rectangle AreaJogador(Vector2 p) {
    return (Rectangle){ p.x - 18, p.y - 22, 36, 44 };
}

// Desenha o sprite da direção atual centrado em 'pos' (esquerda = sprite do lado espelhado).
void DesenharJogador(
    const SpriteDirecao sprites[4],
    Vector2 pos,
    Direcao direcao,
    int quadro
) {
    const SpriteDirecao *sprite = &sprites[direcao];
    Texture2D textura = sprite->textura;

    float flip = sprite->espelhar ? -1.0f : 1.0f;

    Rectangle origem;
    Rectangle destino;
    Vector2 centro;

    if (sprite->quadros > 1) {
        float largura = (float)textura.width / sprite->quadros;
        float altura = (float)textura.height;

        origem = (Rectangle) {
            largura * (quadro % sprite->quadros),
            0,
            largura,
            altura
        };

        float escala = ALTURA_PERSONAGEM / altura;

        destino = (Rectangle) {
            roundf(pos.x),
            roundf(pos.y),
            largura * escala,
            altura * escala
        };

        centro = (Vector2) {
            destino.width / 2,
            destino.height / 2
        };
    } else {
        origem = (Rectangle) {
            0,
            0,
            textura.width * flip,
            textura.height
        };

        destino = (Rectangle) {
            pos.x,
            pos.y,
            TAM_SPRITE,
            TAM_SPRITE
        };

        centro = (Vector2) {
            TAM_SPRITE / 2,
            TAM_SPRITE / 2
        };
    }

    DrawTexturePro(
        textura,
        origem,
        destino,
        centro,
        0.0f,
        WHITE
    );
}
