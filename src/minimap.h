/* minimap.h - the corner minimap and the full-screen map (M).
 * The whole wing is painted once into a texture when it loads; the minimap shows a window of it
 * around the player (north up). Cells the player has seen are revealed; chests, the exit, the
 * player (and in the Bell Tower the cage and an alerted Red Abbot) are always shown as guides. */
#ifndef MINIMAP_MODULE_H
#define MINIMAP_MODULE_H

#include "raylib.h"

struct Game;

void Minimap_Build(const struct Game *g);     /* after a wing (or the Sanctum) is loaded */
void Minimap_Unload(void);
void Minimap_Reveal(struct Game *g);          /* mark what the player can see as discovered */
void Minimap_Draw(const struct Game *g);      /* corner minimap (HUD) */
void Minimap_DrawFull(const struct Game *g);  /* full-screen map with a legend */

#endif
