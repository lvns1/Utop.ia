// Preferências do jogador (abas EFEITOS e SONS da tela de Opções)
#include "../../include/core/preferencias.h"
#include "../../include/audio/audio.h"

static Preferencias g_prefs = PREFERENCIAS_PADRAO;

static int LimitarVolume(int v)
{
    if (v < 0) return 0;
    if (v > VOLUME_PASSOS) return VOLUME_PASSOS;
    return v;
}

Preferencias LimitarPreferencias(Preferencias p)
{
    p.volumeGeral = LimitarVolume(p.volumeGeral);
    p.volumeMusica = LimitarVolume(p.volumeMusica);
    p.volumeEfeitos = LimitarVolume(p.volumeEfeitos);
    return p;
}

void DefinirPreferencias(Preferencias p)
{
    g_prefs = LimitarPreferencias(p);
    DefinirVolumes((float)g_prefs.volumeGeral / VOLUME_PASSOS,
                   (float)g_prefs.volumeMusica / VOLUME_PASSOS,
                   (float)g_prefs.volumeEfeitos / VOLUME_PASSOS);
}

const Preferencias *Prefs(void)
{
    return &g_prefs;
}
