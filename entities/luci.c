#include "luci.h"
#include <math.h>

void DesenharLuciNPC(Rectangle hitbox) {
    DrawRectangleRec(hitbox, Fade(BLUE, 0.4f));
    DrawText("Luci", (int)hitbox.x, (int)hitbox.y - 10, 10, WHITE);
}

void DesenharCompanion(Vector2 jogadorPos, float angulo) {
    Vector2 p = {
        jogadorPos.x + cosf(angulo) * 40.0f,
        jogadorPos.y - 10.0f + sinf(angulo * 1.2f) * 26.0f
    };

    Rectangle companion = { p.x - 7, p.y - 7, 14, 14 };

    DrawCircle((int)p.x, (int)p.y, 14, Fade(SKYBLUE, 0.15f));
    DrawRectangleRec(companion, SKYBLUE);
    DrawRectangleLinesEx(companion, 2, BLUE);
}
