/* config.h - every tunable value of Blackthorn Manor lives here.
 * Change numbers here to rebalance the game; no other file should hold magic numbers. */
#ifndef CONFIG_H
#define CONFIG_H

/* ---------------------------------------------------------------- window */
#define SCREEN_W        1280            /* virtual screen width (the window is scaled to fit) */
#define SCREEN_H        720             /* virtual screen height */
#define WINDOW_TITLE    "Blackthorn Manor"
#define TARGET_FPS      60
#define MAX_DT          0.05f           /* frame dt is clamped to this (avoids huge steps after a stall) */
#define AUTOTEST_FRAMES 90              /* frames simulated per wing in --autotest */

/* ------------------------------------------------------------ PS1 look */
#define POST_W              640         /* the 3D scene is rendered at this low resolution */
#define POST_H              360
#define POST_GRADE_STRENGTH 0.75f       /* 0 = original colours, 1 = full red/black grade */
#define POST_GRAIN          0.06f       /* film grain strength */
#define POST_DITHER         1           /* 1 = PS1 5-bit colour with ordered dither */
#define PS1_WOBBLE          1           /* 1 = snap vertices to a coarse grid (PS1 wobble) */
#define PS1_WOBBLE_GRID_W   320.0f      /* grid the vertices snap to (coarser = more wobble) */
#define PS1_WOBBLE_GRID_H   180.0f

/* --------------------------------------------------------------- palette */
#define COL_BONE        (Color){ 226, 220, 200, 255 }   /* bone white: body text */
#define COL_BLOOD       (Color){ 150,  18,  28, 255 }   /* blood red: danger, titles */
#define COL_GOLD        (Color){ 232, 184,  88, 255 }   /* candle gold: highlights, treasures */
#define COL_NEARBLACK   (Color){  13,   3,   3, 255 }   /* background / fog (matches FOG_COLOR) */

/* ----------------------------------------------------------------- fonts */
#define FONT_DIR        "assets/fonts"            /* searched recursively for the files below */
#define FONT_TITLE_NAME "PirataOne-Regular.ttf"   /* titles (falls back to raylib's font) */
#define FONT_BODY_NAME  "CrimsonText-Regular.ttf" /* body text (falls back to raylib's font) */
#define FONT_TITLE_SIZE 96              /* pixel size the title font is rasterised at */
#define FONT_BODY_SIZE  36              /* pixel size the body font is rasterised at */

/* ----------------------------------------------------------------- world */
#define WORLD_MAX_W     56              /* max wing map width in cells */
#define WORLD_MAX_H     40              /* max wing map height in cells */
#define WALL_HEIGHT     4.0f            /* walls are 4 blocks tall; ceiling sits on top */
#define CHUNK_SIZE      16              /* cells per mesh chunk side */
#define MAX_CHUNKS      ((WORLD_MAX_W / CHUNK_SIZE + 1) * (WORLD_MAX_H / CHUNK_SIZE + 1))
#define MAX_TORCHES     128
#define MAX_CHESTS      8
#define MAX_EXIT_CELLS  4
#define MAX_SPAWNS      64
#define TORCH_HEIGHT    2.3f            /* flame height above the floor */
#define TORCH_RADIUS    7.0f            /* how far baked torch light reaches */
#define TORCH_INTENSITY 1.7f            /* brightness of one torch right next to it */
#define TORCH_COLOR_R   1.00f           /* warm orange torch light */
#define TORCH_COLOR_G   0.55f
#define TORCH_COLOR_B   0.30f
#define FOG_COLOR_R     0.05f           /* fog fades to this red-black */
#define FOG_COLOR_G     0.01f
#define FOG_COLOR_B     0.01f
#define AMBIENT_TINT_R  1.20f           /* ambient light is tinted slightly red */
#define AMBIENT_TINT_G  0.80f
#define AMBIENT_TINT_B  0.80f
#define PLAYER_LIGHT_R  0.55f           /* soft warm light carried by the player */
#define PLAYER_LIGHT_G  0.45f
#define PLAYER_LIGHT_B  0.36f
#define PLAYER_LIGHT_HEIGHT 1.7f        /* light sits above the player's head */

/* ---------------------------------------------------------------- camera */
#define CAM_FOVY            62.0f       /* vertical field of view in degrees */
#define CAM_DISTANCE        3.6f        /* distance behind the player */
#define CAM_HEAD_HEIGHT     1.5f        /* camera looks at this height above the feet */
#define CAM_SHOULDER        0.4f        /* sideways offset to the right shoulder */
#define CAM_PITCH_MIN       (-55.0f)    /* degrees, most downward look */
#define CAM_PITCH_MAX       15.0f       /* degrees, most upward look */
#define CAM_PITCH_DEFAULT   (-14.0f)    /* degrees, starting pitch */
#define CAM_MIN_Y           0.2f        /* camera never goes below this */
#define CAM_MAX_Y           3.8f        /* camera never goes above this (ceiling is 4.0) */
#define CAM_WALL_MARGIN     0.18f       /* keep this far from walls so the near plane doesn't clip */
#define CAM_RETURN_SPEED    4.0f        /* how fast the camera eases back out after a wall pushed it in */
#define MOUSE_SENSITIVITY   0.0035f     /* radians per pixel of mouse movement */
#define KEY_TURN_SPEED      2.6f        /* radians/s when turning the camera with arrow keys */

/* ---------------------------------------------------------------- player */
#define PLAYER_SPEED        4.5f        /* units per second */
#define PLAYER_ACCEL        14.0f       /* how quickly velocity reaches target (1/s) */
#define PLAYER_TURN_SPEED   12.0f       /* how fast the body turns toward movement (rad/s) */
#define PLAYER_SIZE         0.6f        /* collision box width/depth */
#define PLAYER_MAX_HEARTS   5
#define PLAYER_INVULN       1.0f        /* seconds of invulnerability after a hit */
#define DASH_SPEED          12.0f
#define DASH_TIME           0.15f
#define DASH_COOLDOWN       0.8f

/* ------------------------------------------------------ wand: Red Lightning */
#define WAND_COOLDOWN       0.45f       /* seconds between casts */
#define WAND_RANGE          12.0f       /* how far the lightning reaches */
#define WAND_DAMAGE         1
#define WAND_KNOCKBACK      4.0f        /* units/s, decays */
#define AIM_ASSIST_RANGE    WAND_RANGE  /* casting snaps to the nearest visible enemy this close */
#define CAST_POSE_TIME      0.2f        /* the arm points at the target this long */
#define BEAM_TIME           0.15f       /* the lightning bolt stays visible this long */
#define BEAM_SEGMENTS       10          /* jagged segments per bolt */
#define FLASH_RADIUS        6.0f        /* red light at the hit point lights up the walls */

/* ---------------------------------------------------------------- chests */
#define CHEST_HOLD_TIME     1.5f        /* seconds to hold E */
#define CHEST_USE_RANGE     1.6f        /* how close you must be */

/* --------------------------------------------------------------- enemies */
#define ENEMY_WINDUP        0.5f        /* telegraph time before an attack lands */
#define ENEMY_HEAR_RANGE    3.0f        /* enemies notice you this close regardless of view cone */
#define ENEMY_FOV_DEG       110.0f      /* view cone width */
#define SKELETON_HP         2
#define SKELETON_SPEED      2.2f
#define SKELETON_SIGHT      9.0f
#define SKELETON_REACH      1.4f
#define GHOST_HP            2
#define GHOST_SPEED         1.6f
#define GHOST_SIGHT         8.0f
#define GHOST_SCARE_RANGE   7.0f
#define GHOST_ALPHA         0.45f
#define WITCH_HP            3
#define WITCH_SPEED         2.0f
#define WITCH_SIGHT         11.0f
#define WITCH_MIN_DIST      5.0f
#define WITCH_MAX_DIST      8.0f
#define WITCH_FIRE_TIME     2.2f
#define BOLT_SPEED          5.0f
#define QUEEN_HP            20
#define QUEEN_SPEED         1.8f
#define QUEEN_WINDUP        0.6f
#define QUEEN_RING_TIME     3.5f
#define MAX_ENEMIES         64
#define MAX_BOLTS           128

/* -------------------------------------------------------------- enemies 2 */
#define ENEMY_SIZE          0.6f        /* collision box of normal enemies */
#define SKELETON_COOLDOWN   1.1f        /* pause after a melee attack */
#define GHOST_REACH         1.2f
#define GHOST_COOLDOWN      1.3f
#define GHOST_FAINT_ALPHA   0.10f       /* how a ghost looks while out of the light */
#define GHOST_LIGHT_NEEDED  0.22f       /* torch light level that reveals a ghost */
#define GHOST_MOAN_RANGE    10.0f       /* ghosts moan now and then within this distance */
#define WITCH_WINDUP        0.5f        /* telegraph before a bolt */
#define QUEEN_SIGHT         14.0f
#define QUEEN_REACH         2.1f
#define QUEEN_SIZE          1.0f
#define QUEEN_COOLDOWN      1.4f
#define QUEEN_KNOCK_MUL     0.3f        /* the Queen barely flinches */
#define BOLT_LIFE           6.0f
#define BOLT_HIT_RADIUS     0.45f
#define BOLT_HEIGHT         1.2f

/* ------------------------------------------------------------------ feel */
#define SHAKE_HIT           0.7f        /* camera shake amounts (0..1) */
#define SHAKE_CHEST         0.35f
#define SHAKE_DOOR          0.6f
#define SHAKE_SCARE         0.9f
#define SHAKE_DECAY         2.2f        /* per second */
#define SHAKE_SIZE          0.12f       /* world units at shake 1.0 */
#define HURT_FLASH_TIME     0.5f
#define BANNER_TIME         3.2f        /* seconds a banner stays up */
#define FADE_SPEED          1.4f        /* screen fade per second */
#define RESPAWN_INVULN      2.0f        /* grace period after rising again */
#define DOOR_OPEN_TIME      2.0f        /* seconds for the exit door to sink */
#define CHEST_FACE_DEG      80.0f       /* how directly you must face a chest */
#define CONTROLS_HINT_TIME  20.0f       /* seconds the controls hint shows in wing 1 */
#define MAX_PARTICLES       256

/* ----------------------------------------------------------------- audio */
#define MASTER_VOLUME       0.9f
#define AMBIENCE_FILE       "assets/audio/dungeon_ambient_1.ogg"
#define AMBIENCE_VOLUME     0.35f
#define FOOTSTEP_INTERVAL   0.4f        /* seconds between footsteps at full speed */

/* ------------------------------------------------------------------ save */
#define SAVE_FILE           "save.txt"  /* highest unlocked wing (1-5) */

/* ------------------------------------------------------------ wing names */
/* For the inventory (the in-game title comes from the first line of each wing file). */
#define WING_NAME_TABLE { "The Gatehouse", "The Grand Ballroom", "The Moonlit Library", \
                          "The Bone Chapel", "The Witch Queen's Throne Room" }

/* ------------------------------------------------------------- treasures */
/* Shown in the HUD list and the "You found ..." banner, in map order (top row first). */
#define TREASURE_TABLE {                                                                             \
    { "Silver Chalice", "Raven Brooch", "Iron Rosary" },                                             \
    { "Cursed Locket", "Bloodstone Ring", "Masquerade Mask", "Golden Candelabrum" },                 \
    { "Moonlit Grimoire", "Astrolabe of Bone", "Quill of the Damned", "Hourglass of Ash" },          \
    { "Saint's Reliquary", "Skull Chalice", "Ossuary Key", "Black Censer", "Weeping Icon" },         \
    { "Thorned Crown", "Witch Queen's Scepter", "Obsidian Heart", "Veil of Shadows", "Blackthorn Seal" }, \
}

/* ----------------------------------------------------------------- wings */
#define WING_COUNT 5

typedef struct {
    const char *file;          /* wing map file */
    int   chests;              /* number of chests the map must contain */
    float enemySpeedMul;       /* scales enemy speed */
    float sightMul;            /* scales enemy sight range */
    float witchFireMul;        /* scales witch fire rate */
    float fogDensity;          /* exponential-squared fog density */
    float playerLightRadius;   /* radius of the soft light around the player */
    float ambient;             /* base light level */
} WingConfig;

/* Edit the numbers here. (The array itself is created in game.c from this macro, because a
 * static array in a header would be copied into every .c file.) */
#define WING_TABLE {                                                                  \
    /* file                     chests speed  sight fire   fog    light ambient */      \
    { "assets/wings/wing1.txt", 3,     1.00f, 0.8f, 1.0f,  0.06f, 7.0f, 0.20f },        \
    { "assets/wings/wing2.txt", 4,     1.05f, 0.9f, 1.1f,  0.08f, 6.5f, 0.17f },        \
    { "assets/wings/wing3.txt", 4,     1.10f, 1.0f, 1.2f,  0.10f, 6.0f, 0.14f },        \
    { "assets/wings/wing4.txt", 5,     1.20f, 1.1f, 1.3f,  0.13f, 5.0f, 0.10f },        \
    { "assets/wings/wing5.txt", 5,     1.25f, 1.2f, 1.4f,  0.16f, 4.5f, 0.07f },        \
}
extern const WingConfig WINGS[WING_COUNT];

#endif
