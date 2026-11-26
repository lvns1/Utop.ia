// Preferências do jogador que valem no jogo todo: as abas EFEITOS e SONS da tela de Opções.
// Ficam no utopia.cfg junto com o modo de exibição (resolucao.c lê e grava).
// Os interruptores do config.h (COMBATE_*, TELA_*, LUZ_SALAS) continuam valendo: com um efeito
// desligado lá, ele nem é compilado e a opção do menu não muda nada.
#ifndef UTOPIA_PREFERENCIAS_H
#define UTOPIA_PREFERENCIAS_H

#include <stdbool.h>

#define VOLUME_PASSOS  10                   // os volumes vão de 0 a 10 (de 10% em 10%)

typedef struct {
    // EFEITOS
    bool efeitosTela;                       // vinheta, brilho de neon, linhas de monitor e granulado
    bool corrupcaoTela;                     // a corrupção da Luci aparecendo na tela (cores separadas, falhas)
    bool luzSalas;                          // penumbra e luzes nas salas da missão
    bool numerosDano;                       // números de dano subindo dos alvos
    bool tremorTela;                        // a tela treme quando o jogador leva dano
    bool cameraMira;                        // a câmera adianta um pouco para o lado da mira
    // SONS (0..VOLUME_PASSOS)
    int volumeGeral;
    int volumeMusica;
    int volumeEfeitos;
} Preferencias;

#define PREFERENCIAS_PADRAO  { true, true, true, true, true, true, VOLUME_PASSOS, VOLUME_PASSOS, VOLUME_PASSOS }

void DefinirPreferencias(Preferencias p);    // guarda e já aplica os volumes
const Preferencias *Prefs(void);             // as preferências atuais (ex.: Prefs()->numerosDano)
Preferencias LimitarPreferencias(Preferencias p);   // volumes fora de 0..10 voltam para dentro

#endif // UTOPIA_PREFERENCIAS_H
