#include "text.h"
#include <string.h>

void QuebrarTexto(const char *texto, int larguraMax, int tamFonte, char *saida, int tamSaida) {
    int pos = 0;
    int inicioLinha = 0;
    int ultimoEspaco = -1;

    for (int i = 0; texto[i] != '\0' && pos < tamSaida - 1; i++) {
        char c = texto[i];
        saida[pos++] = c;
        saida[pos] = '\0';

        if (c == '\n') {
            inicioLinha = pos;
            ultimoEspaco = -1;
            continue;
        }
        if (c == ' ')
            ultimoEspaco = pos - 1;

        if (ultimoEspaco >= inicioLinha &&
            MeasureText(saida + inicioLinha, tamFonte) > larguraMax) {
            saida[ultimoEspaco] = '\n';
            inicioLinha = ultimoEspaco + 1;
            ultimoEspaco = -1;
        }
    }
    saida[pos] = '\0';
}

int DesenharTextoLinhas(const char *texto, int x, int y, int tamFonte, int espacoLinha, Color cor) {
    char linha[512];
    int n = 0;
    int linhas = 0;

    for (const char *p = texto; ; p++) {
        if (*p == '\n' || *p == '\0') {
            linha[n] = '\0';
            DrawText(linha, x, y + linhas * espacoLinha, tamFonte, cor);
            linhas++;
            n = 0;
            if (*p == '\0') break;
        } else if (n < 511) {
            linha[n++] = *p;
        }
    }
    return linhas;
}

int ContarLinhas(const char *texto) {
    int n = 1;
    for (; *texto != '\0'; texto++)
        if (*texto == '\n') n++;
    return n;
}

int FimDoCaractere(const char *s, int n) {
    while (s[n] != '\0' && (((unsigned char)s[n]) & 0xC0) == 0x80)
        n++;
    return n;
}

bool TextoParcial(const char *texto, float visiveis, char *saida, int tamSaida) {
    int total = (int)strlen(texto);
    int n = FimDoCaractere(texto, (int)visiveis);

    if (n > total) n = total;
    if (n > tamSaida - 1) n = tamSaida - 1;

    memcpy(saida, texto, (size_t)n);
    saida[n] = '\0';

    return (n >= total);
}
