/* screen.h - fixed-size virtual screen.
 * The whole game draws into a SCREEN_W x SCREEN_H render texture, which is then scaled
 * (letterboxed) into the real window. This way the game fits small monitors (e.g. 1024x768)
 * and all game/UI code can use 1280x720 coordinates. Screenshots are taken from the texture. */
#ifndef SCREEN_MODULE_H
#define SCREEN_MODULE_H

#include "raylib.h"

void Screen_Init(const char *title, bool vsync);   /* opens the window sized to fit the monitor */
void Screen_Begin(void);                 /* start drawing a frame into the virtual screen */
void Screen_End(void);                   /* scale the virtual screen into the window and present */
bool Screen_Save(const char *path);      /* export the last finished frame as a PNG */
void Screen_Shutdown(void);              /* unload the render texture and close the window */

#endif
