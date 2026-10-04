/* config.h - every tunable value of Blackthorn Manor lives here.
 * Change numbers here to rebalance the game; no other file should hold magic numbers. */
#ifndef CONFIG_H
#define CONFIG_H

/* ---------------------------------------------------------------- window */
#define SCREEN_W        1280            /* window width in pixels */
#define SCREEN_H        720             /* window height in pixels */
#define WINDOW_TITLE    "Blackthorn Manor"
#define TARGET_FPS      60
#define MAX_DT          0.05f           /* frame dt is clamped to this (avoids huge steps after a stall) */

/* --------------------------------------------------------------- palette */
#define COL_BONE        (Color){ 226, 220, 200, 255 }   /* bone white: body text */
#define COL_BLOOD       (Color){ 150,  18,  28, 255 }   /* blood red: danger, titles */
#define COL_GOLD        (Color){ 232, 184,  88, 255 }   /* candle gold: highlights, treasures */
#define COL_NEARBLACK   (Color){   8,   7,  12, 255 }   /* background */

/* ----------------------------------------------------------------- fonts */
#define FONT_TITLE_FILE "assets/fonts/Pirata_One/PirataOne-Regular.ttf"
#define FONT_BODY_FILE  "assets/fonts/Crimson_Text/CrimsonText-Regular.ttf"

#endif
