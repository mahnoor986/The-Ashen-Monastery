#ifndef UI_H
#define UI_H

#include "game.h"

void UI_Init(void);       /* loads assets/fonts/title.ttf + body.ttf if present */
void UI_Shutdown(void);

void UI_DrawMenu(float time);
void UI_DrawHUD(const Game *g);
void UI_DrawBanner(int levelNumber, const char *name, float timer);
void UI_DrawVignette(void);
void UI_DrawPause(void);
void UI_DrawInventory(const Game *g);
void UI_DrawGameOver(float time);
void UI_DrawVictory(int kills, float time);

#endif
