# BLACKTHORN MANOR — Build Spec for Claude Code

> **Update:** the game is now **THE ASHEN MONASTERY** ("Climb. Break the bells. Bring them home.").
> Its story, names, look (PS1 red/black post-process), Red Lightning wand, enemies and the Sanctum
> ending are specified in **`UPGRADE.md`**, which overrides this file wherever they conflict.
> Everything below still describes the underlying systems (build, maps, chests, checkpoints, autotest).
> The original Blackthorn Manor version is tagged `submittable` in git.

You are rebuilding this project (currently a 2D top-down dungeon crawler called "Gothic Dungeon")
into **Blackthorn Manor**: a **3D, blocky (voxel-style), third-person gothic horror game** in **C99 + raylib 6.0**.
This file is the single source of truth. Follow it phase by phase. Do not ask the user questions unless
you are truly blocked; make reasonable choices that fit this document and note them in `NOTES.md`.

The user is a student with a one-day deadline and is NOT an experienced gamer. Priorities, in order:
1. It always compiles and runs. Never leave the build broken at the end of a phase.
2. It is easy and fun to play for a non-gamer (forgiving, readable, clear goals).
3. It looks scary/gothic (fog, darkness, torchlight, blocky silhouettes).
4. Extra content only after 1–3 are solid.

---

## 0. Environment (Windows)

- Compiler: w64devkit at `C:/Users/NAT/Documents/w64devkit/bin` (gcc, make, gdb).
- raylib 6.0 at `C:/Users/NAT/Documents/raylib-6.0_win64_mingw-w64` with `include/raylib.h`, `include/raymath.h`, `include/rlgl.h`, `lib/libraylib.a`.
- Before running make in your shell, put w64devkit first on PATH:
  - Git Bash: `export PATH="/c/Users/NAT/Documents/w64devkit/bin:$PATH"`
  - PowerShell: `$env:PATH = "C:\Users\NAT\Documents\w64devkit\bin;" + $env:PATH`
- Build: `make RAYLIB_PATH=C:/Users/NAT/Documents/raylib-6.0_win64_mingw-w64`
- The existing `Makefile` compiles every `src/*.c` automatically. Keep it. Change only `TARGET := BlackthornManor.exe`
  and update `.vscode/launch.json` (`program`) to match. Keep `-std=c99 -Wall -Wextra`. The build must have **zero warnings**.
- **raylib 6.0 API check:** before using any raylib/rlgl/raymath function you are not 100% sure about, `grep` its
  signature in `include/raylib.h`, `include/rlgl.h` or `include/raymath.h`. Do not guess signatures.
- At startup call `ChangeDirectory(GetApplicationDirectory())` so relative asset paths work when the exe is double-clicked.

## 1. Phase 0 — Setup (do this first)

1. If `git` is available: `git init` (if not already a repo) and commit the current state as "legacy 2D version".
   Commit again at the end of every phase.
2. Move the old code to `legacy_2d/` (copy `src/`, `README.md`, `build.bat` there). The Makefile only compiles `src/*.c`,
   so `legacy_2d/` will not be built. Then delete old files from `src/` that are being replaced.
3. `make clean` to remove stale `build/*.o` and `build/*.d`.
4. List `assets/` recursively. The user downloaded:
   - Fonts: Pirata One (title) and Crimson Text (body) `.ttf` files somewhere under `assets/fonts/`.
   - Audio: Kenney "RPG Audio" and "Impact Sounds" packs, plus OpenGameArt files (ghost moaning voices,
     "scaryhighpitchedghost.ogg", "dungeon_ambient_1.ogg") somewhere under `assets/audio/`.
   - Files may still be inside zip archives or nested folders. Unzip if needed. Pick the best file for each sound
     role (see section 9) by name. **Every asset is optional**: if a file is missing, the game must still run
     (default font / silence).
5. Create the folders: `assets/wings/`, `assets/shaders/`, `shots/`.

## 2. The Game (design)

**Story:** You stand at the iron gate of Blackthorn Manor. Its treasures are scattered through 5 cursed wings,
guarded by the dead and the witches who serve the Witch Queen. Collect every treasure, defeat the Queen, and escape.

**Core loop:** Each wing is made of rooms. **Every room contains exactly one treasure chest.** Open all chests in a
wing (hold **E** for 1.5 s) → the wing's exit door opens → walk through it → next wing. Enemies roam the rooms.
Wing 5 also has the Witch Queen; after she dies and all chests are opened, the manor gate opens → escape → victory.

**Wings (difficulty increases):**

| # | Name | Chests | Enemies (guideline) | Mood |
|---|------|--------|---------------------|------|
| 1 | The Gatehouse | 3 | 4 skeletons, 1 ghost | many torches, tutorial-easy |
| 2 | The Grand Ballroom | 4 | 6 skeletons, 2 ghosts, 1 witch | red carpet, pillars |
| 3 | The Moonlit Library | 4 | 4 skeletons, 2 ghosts, 4 witches | bookshelves, dimmer |
| 4 | The Bone Chapel | 5 | 7 skeletons, 4 ghosts, 3 witches | bone piles, very dark |
| 5 | The Witch Queen's Throne Room | 5 | Witch Queen + 3 skeletons, 2 witches, 2 ghosts | almost pitch black |

No enemies in the starting room of any wing.

**Treasure names** (shown in HUD list and a banner when found):
- Wing 1: Silver Chalice, Raven Brooch, Iron Rosary
- Wing 2: Cursed Locket, Bloodstone Ring, Masquerade Mask, Golden Candelabrum
- Wing 3: Moonlit Grimoire, Astrolabe of Bone, Quill of the Damned, Hourglass of Ash
- Wing 4: Saint's Reliquary, Skull Chalice, Ossuary Key, Black Censer, Weeping Icon
- Wing 5: Thorned Crown, Witch Queen's Scepter, Obsidian Heart, Veil of Shadows, Blackthorn Seal

**Forgiving rules (very important — the user is not a gamer):**
- Player has **5 hearts**. Every hit costs exactly 1 heart. 1.0 s invulnerability after a hit (character blinks).
- Opening a chest restores 1 heart and sets a **checkpoint** at that chest.
- On death: "YOU HAVE FALLEN" screen → Enter respawns at the last checkpoint (or wing start) with full hearts.
  Opened chests and killed enemies stay that way. Surviving enemies return to their spawn points.
- **Aim assist:** when the player swings, if any living enemy is within 3.0 units, the character instantly turns to
  face the nearest one before the hit check.
- **Telegraphed attacks:** every enemy attack has a 0.5 s wind-up during which the enemy glows red and stops moving.
  The hit only lands if the player is still in range when the wind-up ends.
- Enemies are slower than the player (except wing-scaled values listed in config).
- Dash (Shift): short burst with invulnerability, 0.8 s cooldown.

**Controls:**
WASD move (relative to camera) · Mouse look · Left click = sword swing · Shift = dash · E (hold) = open chest ·
I / Tab = inventory (treasure list) · Esc / P = pause · Enter = confirm in menus.
Debug keys (keep in final build, list them in README): F1 debug overlay (collision boxes, enemy sight), F3 god mode,
F4 complete current wing instantly.

## 3. Visual Style

- Everything is built from axis-aligned blocks. **1 block = 1.0 world unit.** Walls are **4 blocks tall**. There is a
  floor and a ceiling. Characters are Minecraft-like proportions (built from boxes) but must be **original**:
  do NOT use any Minecraft textures, names, logos or character designs.
- All block textures are **generated in code** (no image files): 16×16 pixel textures packed into one atlas,
  `TEXTURE_FILTER_POINT`, no mipmaps. Use deterministic pseudo-random noise so they look the same every run.
- Horror atmosphere comes from: exponential black fog, low ambient light, warm flickering torchlight baked into
  vertex colors, and a soft light around the player.

**Generated block textures (atlas tiles):**
stone brick wall (grey-blue bricks with dark mortar), dark wood panel wall, polished obsidian pillar (near-black,
purple glints), bookshelf (dark wood frame + colored book spines), stone floor tiles, red carpet (with gold edge
pattern), wood plank floor, ceiling (very dark wood beams), chest wood + gold trim, iron door (dark metal with rivets),
bone (off-white).

## 4. Project Layout

```
src/main.c        window, main loop, command-line flags (--wing N, --autotest)
src/config.h      ALL tunables + per-wing difficulty table
src/game.h/.c     state machine, wing flow, checkpoints, treasures, victory/escape, save progress
src/world.h/.c    load wing text file → grid, validation, chunked block meshes, collision, line of sight, chests, exit
src/textures.h/.c generates the 16x16 block texture atlas in code
src/character.h/.c blocky humanoid models (box parts) + walk/swing/idle/float animations
src/player.h/.c   movement, third-person camera + camera collision, sword, aim assist, dash, hearts
src/enemy.h/.c    skeleton, ghost, witch, Witch Queen, hex bolts, enemy separation
src/render.h/.c   fog/light shader setup, per-frame uniforms, draw world/entities/transparent pass
src/audio.h/.c    load sounds/music if present, play helpers, ambience loop
src/ui.h/.c       menu, HUD, treasure list, prompts, chest progress ring, banners, pause, inventory, death, victory
assets/wings/wing1.txt ... wing5.txt
assets/shaders/world.vs, world.fs
```
Use `raymath.h` for vector/matrix math (it ships with raylib). Keep functions small and files readable — this is a
college project and the student must be able to explain the code. Add a short comment at the top of each file
explaining what it does.

## 5. Wing Map Format (`assets/wings/wingN.txt`)

Plain text. First line: the wing name. Then the grid, top row = north (−Z), left column = west (−X).
Each character is one 1×1 floor cell. All grid rows must have equal length. Max size 56 × 40.

Legend:
| Char | Meaning | Solid? |
|------|---------|--------|
| `#` | stone brick wall (full height) | yes |
| `W` | dark wood panel wall | yes |
| `B` | bookshelf wall | yes |
| `P` | obsidian pillar | yes |
| `T` | stone wall with a torch on every face that touches floor | yes |
| `.` | stone floor | no |
| `=` | red carpet floor | no |
| `,` | wood plank floor | no |
| `x` | bone pile decoration on stone floor (small cubes, not solid) | no |
| `@` | player start | no |
| `C` | treasure chest (0.9-block box, solid) | yes |
| `E` | exit door (solid iron door; opens when wing is complete) | until open |
| `s` `g` `w` `Q` | skeleton, ghost, witch, Witch Queen spawn | no |

Cells with `@ C E s g w Q x` get the floor type of their nearest floor neighbor (fallback: stone).

**Map design rules** (you design all 5 maps):
- Outer border fully solid. Rooms connected by doorways 2 cells wide and corridors 2–3 cells wide.
- The number of rooms that contain a `C` equals the chest count in the wing table; exactly one `C` per room.
  The start area/hallways have no chest.
- Rooms roughly 7×7 to 12×12. Put torches (`T`) at room walls; fewer torches in later wings.
- Use `P` pillars in large rooms (ballroom, throne room) for cover from witch bolts.
- Wing 1 must be small and simple (3 rooms around an entrance hall), quick to complete (~3–5 minutes).
- The exit `E` sits in a wall, reachable from floor on one side.

**Loader validation:** on load, check row lengths, closed border, exactly one `@`, at least one `E`,
chest count matches config, all floor reachable from `@` (flood fill treating `C` and closed `E` as solid; chests
must be adjacent to reachable floor). Print clear errors with line/column to stdout. In `--autotest`, any
validation error → exit code 1.

## 6. Rendering Details

**World mesh:**
- Build meshes per 16×16-cell chunk (each chunk one `Mesh`, non-indexed triangles; UploadMesh then
  LoadModelFromMesh or keep Mesh + Material and use DrawMesh). Only emit faces that are visible
  (wall faces touching a floor cell, floor tops, ceiling bottoms). Fill vertices, texcoords (atlas UVs inset by half a
  texel to avoid bleeding), normals, and colors.
- **Baked light in vertex colors:** per-face shade (top 1.0, N/S 0.8, E/W 0.65, bottom 0.5) multiplied by torch light:
  for each vertex, sum torch contributions within `TORCH_RADIUS` using smooth falloff, warm orange color.
  Store torch light in the vertex color RGB. Ambient is added in the shader (so torches can flicker globally).
- Rebuild only the affected chunk when a door opens (or draw the door as a separate entity — simpler).

**Shader (`assets/shaders/world.vs/.fs`, GLSL 330):**
- Uses raylib default attribute/uniform names: `vertexPosition, vertexTexCoord, vertexNormal, vertexColor`,
  `mvp, matModel, texture0, colDiffuse`. Set `shader.locs[SHADER_LOC_MATRIX_MODEL]` and `shader.locs[SHADER_LOC_VECTOR_VIEW]`.
- Fragment color = `texel * colDiffuse * (ambient + vertexColor.rgb * flicker + playerLight)`.
  `playerLight` = warm light centered at the player (radius from wing table, smooth falloff).
- Exponential squared fog toward near-black `fogColor` (≈ 0.02, 0.02, 0.035) using distance to camera; `fogDensity`
  from wing table.
- Uniforms: `viewPos, fogColor, fogDensity, ambient, flicker, lightPos, lightRadius, lightColor`.
- `flicker` = 0.88 + small layered sines of time (updated per frame).
- If shader files fail to load, the game must still run with raylib's default shader.

**Characters:** a single cube mesh (`GenMeshCube(1,1,1)`) drawn with `DrawMesh` per body part, each with its own
transform matrix (scale × rotation × translation, limb rotations around the joint) and a material using the world shader
(so fog/light apply). Each part uses a small generated texture or solid tint. Vertex colors for the cube must be white
(set colors or ensure shader treats missing colors as white — verify `vertexColor` behavior when the mesh has no colors
and handle it, e.g., generate a white color array).
- **Knight (player):** steel helmet with dark visor slit, dark-steel body with a crimson tabard stripe, arms, legs,
  a sword (thin long box + crossguard) in the right hand. Height ≈ 1.8.
- **Skeleton:** bone-white, thin limbs, dark eye sockets on head, arms stretched slightly forward. Height ≈ 1.8.
- **Ghost:** pale blue-white, no legs, body tapering (stack of shrinking boxes), arms forward, floats and bobs
  (0.25–0.45 above floor), translucent (alpha ≈ 0.45), dark hollow eyes.
- **Witch:** dark purple robe (wide box to the floor), green-tinted face, tall pointed hat made of 3–4 stacked
  shrinking boxes with a brim, holds a glowing purple orb.
- **Witch Queen:** scale 1.8× witch, black-and-crimson robe, a crown of small gold boxes, two orbiting glowing orbs.
- Animations: walk = arms/legs swing (sin of distance walked), idle = slight breathing bob, sword swing = right arm
  rotates through a 120° arc over the active time, wind-up = enemy tinted red + slight lean back.
- Hit flash: enemy tinted white for 0.12 s when damaged.

**Draw order:** opaque world chunks → chests, doors, decorations, torches (small emissive flame cubes) → opaque
characters → hex bolts (emissive) → transparent pass (`BeginBlendMode(BLEND_ALPHA)`, depth write disabled via
`rlDisableDepthMask`, ghosts sorted back-to-front) → `EndMode3D` → 2D UI.

## 7. Camera & Movement

- Third-person orbit camera. `DisableCursor()` while playing; `EnableCursor()` in menus/pause/inventory/death.
- Mouse X → yaw, mouse Y → pitch (clamp −55° to +15°). Sensitivity in config. Distance 3.6, target = player head
  (y ≈ 1.5), slightly over the right shoulder (offset 0.4).
- **Camera collision:** step from the target toward the desired camera position in 0.05 increments; stop just before
  entering a solid cell or going above y = 3.8 / below y = 0.2. Smooth the resulting distance so it does not pop.
- Movement relative to camera yaw. Character body turns smoothly toward movement direction (and snaps toward the
  aim-assist target when swinging).
- Collision on the XZ plane: player is a 0.6×0.6 box, axis-separated slide against solid cells (same idea as the
  old `Map_Move`). Enemies use the same function with their own size (ghosts ignore walls but stay inside the map border).
- No jumping. One floor level per wing.
- Camera shake: small offset on hits, chest open, door open, ghost scare.

## 8. Gameplay Systems

**Sword:** active time 0.25 s, cooldown 0.40 s, range 2.0 units from player center, 130° arc in front, 1 damage,
each enemy hit at most once per swing, knockback 4 units/s decaying. The sword also destroys hex bolts it touches.

**Enemy AI (port the ideas from `legacy_2d/src/enemy.c`):** idle enemies sweep their gaze around a base angle; they
notice the player via sight range + view cone + line of sight (grid raycast), or hearing range regardless of cone.
Once alerted they stay alerted. Simple steering toward the player; slide along walls; enemy–enemy separation.

| Enemy | HP | Speed | Behavior |
|-------|----|-------|----------|
| Skeleton | 2 | 2.2 | walk to player; melee wind-up 0.5 s when within 1.4 |
| Ghost | 2 | 1.6 | floats through walls; can only be damaged while **visible** (within player light radius or near a torch); otherwise drawn very faint and invulnerable. On first noticing the player within 7 units: scare sound + camera shake. Touch attack with wind-up. |
| Witch | 3 | 2.0 | keeps 5–8 units away, strafes; every 2.2 s with LOS fires a hex bolt (glowing purple spinning cube, speed 5, 1 heart). Bolts die on walls. |
| Witch Queen | 20 | 1.8 | chases; melee wind-up 0.6 s; every 3.5 s fires a ring of 8 bolts; at ≤50% HP summons 2 ghosts once; boss HP bar at bottom of screen with her name. |

Per-wing multipliers in config scale enemy speed, sight range and witch fire rate.

**Chests:** prompt "[E] Open chest" when within 1.6 units and facing roughly toward it. Holding E fills a progress
ring over 1.5 s; releasing E, taking a hit, or moving away cancels. On open: lid rotates open, gold glow, treasure
banner ("You found the Silver Chalice"), +1 heart (max 5), checkpoint set, chest counter updates. When the last chest
in the wing is opened: banner "The way forward is open...", door sound, exit door slides down into the floor.

**Wing exit:** walking into the opened exit → fade to black → next wing (title banner "Wing II — The Grand Ballroom").
Wing 5 exit is only open when all chests are opened **and** the Queen is dead → victory screen.

**Save progress:** write `save.txt` (highest unlocked wing). Menu shows "Continue (Wing N)" if a save exists.

## 9. Audio (all optional, fail silently)

`InitAudioDevice()` at startup. Roles → pick best matching files from the downloaded packs:
sword swing (whoosh/knife), sword hit (impact), player hurt (impact/punch), chest opening (creak/latch/book/door),
treasure found (metal/coin/latch), door open (door), footsteps (Kenney `footstep0X.ogg`, play while walking every ~0.4 s,
quiet), ghost scare (`scaryhighpitchedghost.ogg`), ghost moan (ghost voices, occasional when a ghost is near),
witch bolt (any magical/whoosh), ambience loop (`dungeon_ambient_1.ogg` as looping Music stream, low volume).
Use `SetSoundPitch` with small random variation for repeated sounds. Master volume in config.

## 10. UI

Fonts: Pirata One for titles, Crimson Text for body (search `assets/fonts/` recursively for these ttf names; fallback
to default font). Palette: bone white (226,220,200), blood red (150,18,28), candle gold (232,184,88), near-black.

- **Main menu:** "BLACKTHORN MANOR" big title in Pirata One, subtitle "Collect the treasures. Survive the night.",
  options New Game / Continue / Quit (W/S or arrows + Enter, mouse click also works). Background: the wing 1 manor
  rendered in 3D with the camera slowly orbiting near the entrance, fogged (fallback: dark gradient).
- **HUD:** hearts top-left (pixel-style heart shapes drawn with rectangles), dash ready bar under them; wing name top
  center; treasure checklist top-right ("Treasures 1/3" + names, found ones in gold with a check, unknown ones as
  "??? (not yet found)"); interaction prompt + chest progress ring at screen center; small crosshair dot; vignette;
  red hurt flash; boss bar when the Queen is alerted.
- **Banners:** wing title on entering, treasure found, way open. Fade in/out.
- **Pause:** Resume / Restart wing / Quit to menu. **Inventory:** all treasures across all wings, hearts, enemies
  slain, time played. **Death:** "YOU HAVE FALLEN" + Enter to rise again. **Victory:** "YOU ESCAPED BLACKTHORN MANOR",
  treasures collected, enemies slain, total time, Enter for menu.
- Controls hint shown in the menu and during the first 20 seconds of wing 1.

## 11. config.h

Put every tunable here with a comment: window 1280×720, title "Blackthorn Manor", FPS 60, mouse sensitivity,
camera distance/offsets, player speed 4.5, dash speed/time/cooldown (12 / 0.15 / 0.8), hearts 5, invuln 1.0,
sword values, chest hold time, torch radius 7, all enemy stats, and a **wing table**:

```c
typedef struct {
    const char *file;          /* "assets/wings/wing1.txt" */
    int   chests;
    float enemySpeedMul;       /* 1.0, 1.05, 1.1, 1.2, 1.25 */
    float sightMul;            /* 0.8, 0.9, 1.0, 1.1, 1.2  */
    float witchFireMul;        /* 1.0 ... 1.4              */
    float fogDensity;          /* 0.06, 0.08, 0.10, 0.13, 0.16 */
    float playerLightRadius;   /* 7.0, 6.5, 6.0, 5.0, 4.5  */
    float ambient;             /* 0.20, 0.17, 0.14, 0.10, 0.07 */
} WingConfig;
```

## 12. Testing — you cannot see the window, so build tools to see it

- `BlackthornManor.exe --wing N` starts directly in wing N (skips menu).
- `BlackthornManor.exe --autotest`: for each wing 1–5: load + validate, run 90 frames with fixed dt (no mouse input,
  camera behind the player), save a screenshot with `LoadImageFromScreen()` + `ExportImage()` to `shots/wingN.png`
  (do NOT use `TakeScreenshot`, it may strip folder paths). Also save `shots/menu.png` and one close-up of each enemy
  type standing in front of the player (`shots/enemies.png`, spawn them temporarily for the shot). Print a summary
  (cells, chunks, vertices, chests, enemies, torches per wing) and exit 0 (exit 1 on any error).
- After every phase: build with zero warnings, run `--autotest`, **open the screenshots and look at them**. Check:
  scene is not black/blank, fog and torchlight are visible but the player and nearby floor are readable, characters look
  like the descriptions, no z-fighting/texture bleeding, HUD readable. Fix problems before moving on.
- Tell the user at the end of each phase what to try (e.g., "run `make run` and walk to a chest").

## 13. Build Phases (commit after each)

1. **Phase 1 – World & character:** config, textures, world loader + validation + chunk meshes (flat lighting OK),
   wing1.txt, knight model with walk animation, third-person camera, movement (collision may be basic).
   *Done when:* screenshot shows the knight standing in a blocky stone hall.
2. **Phase 2 – Atmosphere & collision:** full XZ collision, camera collision, world shader with fog + player light +
   ambient, baked torch light + flicker, torch flame cubes, ceiling.
   *Done when:* the hall looks dark and gothic with warm torch pools, and the camera never clips through walls.
3. **Phase 3 – Playable core:** chests + hold-E + treasures + banners + HUD, exit door, wing completion, skeletons,
   sword + aim assist + wind-ups, hearts, death + checkpoint respawn, pause.
   *Done when:* wing 1 can be fully played start to finish. **This is the minimum submittable game — make it solid.**
4. **Phase 4 – Horror:** ghosts (transparent pass, visibility rule, scare), witches + hex bolts, audio, camera shake,
   footsteps, ambience.
5. **Phase 5 – Full manor:** wings 2–5 maps, per-wing difficulty, Witch Queen + boss bar + summons, escape ending,
   victory screen, save/continue.
6. **Phase 6 – Polish & ship:** 3D menu background, inventory screen, controls hint, balance pass (wing 1 should be
   beatable by a non-gamer on the first try), `make release` works, update `README.md` (story, controls, build steps,
   file overview, debug keys) and `CREDITS.md` (fill the table with every asset actually used: Pirata One & Crimson Text
   — SIL Open Font License, Google Fonts; Kenney RPG Audio & Impact Sounds — CC0, kenney.nl; OpenGameArt ghost voices
   by qubodup, Scary High-pitched Ghost by Fupi, Loopable Dungeon Ambience by JaggedStone — CC0, opengameart.org;
   note that all textures and models are generated in code).

If time is short, cut in this order: menu 3D background → save/continue → Queen's ghost summon → wing 4/5 size
(make them smaller). Never cut Phase 3 quality.

## 14. Code Rules

- C99, no external libraries besides raylib (raymath/rlgl are part of raylib). No C++.
- No global mutable state scattered around: one `Game` struct passed by pointer; module-private statics only for
  loaded GPU resources (textures, shaders, sounds).
- Unload every GPU/audio resource on shutdown.
- Fixed array sizes with `#define` limits in config.h (e.g., MAX_ENEMIES 64, MAX_BOLTS 128, MAX_CHESTS 8, MAX_TORCHES 128).
- Frame dt clamped to 0.05.
- Keep `NOTES.md` with decisions you made and anything the user should know.