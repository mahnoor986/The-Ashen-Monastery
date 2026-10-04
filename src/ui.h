/* ui.h - fonts, text helpers and every 2D screen (menu, HUD, banners, pause, ...).
 * All coordinates are in the 1280x720 virtual screen. */
#ifndef UI_H
#define UI_H

#include "raylib.h"

struct Game;

void UI_Init(void);       /* loads Pirata One + Crimson Text (falls back to the default font) */
void UI_Shutdown(void);

/* text with a soft dark shadow so it reads on any background */
void UI_Text(bool title, const char *text, float x, float y, float size, Color c);
void UI_TextCentered(bool title, const char *text, float cx, float y, float size, Color c);

void UI_DrawTitleCard(void);                /* placeholder menu (phase 6 adds the 3D background) */
void UI_DrawHUD(const struct Game *g);

#endif
