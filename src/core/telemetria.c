#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../../include/core/fonte.h"
#include "../../include/core/mapas.h"
#include "../../include/core/telemetria.h"


static FILE *telemetriaArq = NULL;

void Telemetria(const char *evento, const char *detalhe, float valor, int quantidade)
{
#if TELEMETRIA_ATIVA
    // O arquivo fica aberto durante o jogo (abrir e fechar a cada evento travava o jogo em alguns PCs)
    if (telemetriaArq == NULL) {
        telemetriaArq = fopen(ARQUIVO_TELEMETRIA, "a");
        if (telemetriaArq == NULL)
            return;
        FILE *f = telemetriaArq;

        // Arquivo novo: escreve o cabeçalho
        fseek(f, 0, SEEK_END);
        if (ftell(f) == 0)
            fprintf(f, "tempo_jogo_s;evento;detalhe;valor;quantidade\n");
    }

    // Tira ";" e quebras de linha do detalhe para não bagunçar as colunas
    char limpo[64];
    int n = 0;

    for (const char *c = detalhe; *c != '\0' && n < 60; c++)
        limpo[n++] = (*c == ';' || *c == '\n' || *c == '"') ? ' ' : *c;

    limpo[n] = '\0';

    FILE *f = telemetriaArq;
    fprintf(f, "%.1f;%s;%s;%.1f;%d\n", GetTime(), evento, limpo, valor, quantidade);

    // Só descarrega no disco em eventos raros (sala nova, missão, batalha, ranking); o resto fica no buffer
    if (strncmp(evento, "rogue_sala", 10) == 0 || strncmp(evento, "rogue_missao", 12) == 0 ||
        strncmp(evento, "batalha", 7) == 0 || strncmp(evento, "ranking", 7) == 0)
        fflush(f);
#else
    (void)evento; (void)detalhe; (void)valor; (void)quantidade;
#endif
}

void TelemetriaFechar(void)
{
    if (telemetriaArq != NULL) { fclose(telemetriaArq); telemetriaArq = NULL; }
}

// Passo de tempo do jogo: limitado a 1/30 s para um engasgo (arrastar a janela, disco) não virar teleporte
float DtJogo(void)
{
    return fminf(GetFrameTime(), 1.0f / 30.0f);
}
