/* ui.h - fonts, text helpers and every 2D screen: main menu, HUD (hearts, dash, treasures,
 * chest prompt, boss bar), banners, pause, inventory, death and victory.
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

/* clickable menu rows (shared by drawing and the game's mouse handling) */
Rectangle UI_MenuItemRect(int index, int count);
Rectangle UI_PauseItemRect(int index, int count);

void UI_DrawMenu(const struct Game *g);
void UI_DrawHUD(const struct Game *g);
void UI_DrawPause(const struct Game *g);
void UI_DrawInventory(const struct Game *g);
void UI_DrawDeath(const struct Game *g);
void UI_DrawVictory(const struct Game *g);
void UI_DrawFade(const struct Game *g);

#endif
