/* config.h - every tunable lives here. Tweak, rebuild, feel the difference. */
#ifndef CONFIG_H
#define CONFIG_H

#include "raylib.h"

/* ---------- Window ---------- */
#define SCREEN_W        1280
#define SCREEN_H        720
#define TARGET_FPS      60
#define WINDOW_TITLE    "Gothic Dungeon"

/* ---------- Debug ---------- */
/* 1 = start with hitboxes / sight cones / tile grid visible. F1 toggles at runtime. */
#define DEBUG_DRAW      0

/* ---------- World ---------- */
#define TILE_SIZE       32
#define MAP_W           40
#define MAP_H           22
#define LEVEL_COUNT     3
#define MAX_TORCHES     64
#define MAX_SPAWNS      96
#define MAX_ENEMIES     48
#define MAX_PROJECTILES 96
#define MAX_PICKUPS     32

/* ---------- Camera ---------- */
#define CAMERA_ZOOM     2.0f
#define CAMERA_LERP     8.0f

/* ---------- Player ---------- */
#define PLAYER_SIZE             20.0f   /* collision box (px) */
#define PLAYER_RADIUS           10.0f   /* hurt circle (px)   */
#define PLAYER_SPEED            130.0f
#define PLAYER_MAX_HP           100
#define PLAYER_INVULN_TIME      0.7f
#define PLAYER_KNOCKBACK        220.0f
#define PLAYER_ATTACK_TIME      0.22f   /* how long the swing is "active" */
#define PLAYER_ATTACK_COOLDOWN  0.38f
#define PLAYER_ATTACK_RANGE     38.0f
#define PLAYER_ATTACK_ARC_DEG   130.0f
#define PLAYER_ATTACK_DAMAGE    1
#define PLAYER_DASH_SPEED       360.0f
#define PLAYER_DASH_TIME        0.14f
#define PLAYER_DASH_COOLDOWN    0.80f

/* ---------- Enemies / items ---------- */
#define ENEMY_KNOCKBACK         240.0f
#define ENEMY_HEAR_RANGE        70.0f
#define KNOCKBACK_DAMPING       9.0f
#define POTION_HEAL             30
#define POTION_DROP_CHANCE      15      /* percent chance an enemy drops a potion */

/* ---------- Lighting ---------- */
#define TORCH_LIGHT_RADIUS      125.0f
#define PLAYER_LIGHT_RADIUS     100.0f
#define AMBIENT_LIGHT           ((Color){ 40, 42, 62, 255 })

/* ---------- Gothic palette ---------- */
#define COL_BG          ((Color){   8,   8,  12, 255 })  /* near-black   */
#define COL_SLATE       ((Color){  70,  82, 112, 255 })  /* slate blue   */
#define COL_BONE        ((Color){ 226, 220, 200, 255 })  /* bone white   */
#define COL_BLOOD       ((Color){ 150,  18,  28, 255 })  /* blood red    */
#define COL_CANDLE      ((Color){ 232, 184,  88, 255 })  /* candle gold  */
#define COL_FLOOR_A     ((Color){  40,  40,  54, 255 })
#define COL_FLOOR_B     ((Color){  46,  46,  62, 255 })
#define COL_WALL        ((Color){  78,  90, 122, 255 })
#define COL_WALL_EDGE   ((Color){  50,  58,  82, 255 })
#define COL_WALL_LIGHT  ((Color){ 112, 124, 158, 255 })

#endif
