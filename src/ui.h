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

/* clickable pause rows (shared by drawing and the game's mouse handling) */
Rectangle UI_PauseItemRect(int index, int count);

/* Title letters fading in one by one with a soft glow (`letters` = how many are visible). */
void UI_TextReveal(const char *text, float cx, float y, float size, float letters, Color c);
void UI_DrawHUD(const struct Game *g);
void UI_DrawPause(const struct Game *g);
void UI_DrawInventory(const struct Game *g);
void UI_DrawDeath(const struct Game *g);
void UI_DrawVictory(const struct Game *g);
void UI_DrawIntro(const struct Game *g);
void UI_DrawDialogue(const struct Game *g);
void UI_DrawFade(const struct Game *g);

#endif
