/* title.h - the title screen: an original storm-lit monastery-castle on a snowy cliff (3D, through
 * the same PS1 pipeline), and the title menus drawn over it (reveal, menu, controls, credits). */
#ifndef TITLE_H
#define TITLE_H

#include "raylib.h"

struct Game;

void Title_Init(void);                       /* builds the cliff + castle meshes and the sky textures */
void Title_Shutdown(void);
void Title_Update(float dt, bool sounds);    /* lightning, thunder, wind */
void Title_Flash(void);                      /* a lightning strike now (a menu choice was made) */
void Title_DrawScene(float time);            /* call inside the low-res scene target */
void Title_DrawUI(const struct Game *g);     /* title letters, menu, controls, credits (virtual screen) */
Rectangle Title_MenuItemRect(int index, int count);

#endif
