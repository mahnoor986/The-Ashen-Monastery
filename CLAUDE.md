# THE ASHEN MONASTERY — Master Build Spec (CLAUDE.md)

**This is the single source of truth for the project.** It replaces and combines the old `CLAUDE.md`
(Blackthorn Manor), `UPGRADE.md` / `UPGRADE_ASHEN.md` (The Ashen Monastery) and `ENVIRONMENT_PS1.md`
(PS1 gothic environment, title screen, relics). If those files still exist, move them to `docs/old_specs/`
and do not follow them anymore. Where the old files disagree with this one, **this file wins**.

You are Claude Code working in the user's VS Code project. The game is a **3D, third-person, retro PS1-style gothic
horror game** in **C99 + raylib 6.0**. Do not ask the user questions unless you are truly blocked; make reasonable
choices that fit this document and log them in `NOTES.md`.

The user is a student with a college deadline (about 2 days) and is NOT an experienced gamer. Priorities, in order:
1. It always compiles and runs. Never leave the build broken.
2. It is easy and fun to play for a non-gamer (forgiving, readable, clear goals, a map to guide them).
3. It looks like an old PlayStation-era gothic horror game: real low-poly architecture, real textures, low-res
   wobbly rendering, moonlight and candlelight in fog. **No block/voxel look anywhere.**
4. Extra content only after 1–3 are solid.

---

## 0. Project history and how to continue

- `git tag submittable`: the first working 3D version (Blackthorn Manor, block style). Always available as a fallback.
- Then the game was upgraded to **The Ashen Monastery** (story, wand, PS1 post-process, Sanctum ending).
- Then the **environment overhaul** started (PS1 gothic castle, minimap, title screen, relics).

**Before doing anything:** read `git log --oneline` and `git tag`, skim the code, and determine which items in the
**Work Checklist (section 16)** are already done. Write the current status into `NOTES.md`. Then continue with the
first unfinished item, in order. If something already exists but does not match this spec, update it.

## 1. Environment (Windows)

- Compiler: w64devkit at `C:/Users/NAT/Documents/w64devkit/bin` (gcc, make, gdb).
- raylib 6.0 at `C:/Users/NAT/Documents/raylib-6.0_win64_mingw-w64` with `include/raylib.h`, `include/raymath.h`,
  `include/rlgl.h`, `lib/libraylib.a`.
- Before running make in your shell, put w64devkit first on PATH:
  - Git Bash: `export PATH="/c/Users/NAT/Documents/w64devkit/bin:$PATH"`
  - PowerShell: `$env:PATH = "C:\Users\NAT\Documents\w64devkit\bin;" + $env:PATH`
- Build: `make RAYLIB_PATH=C:/Users/NAT/Documents/raylib-6.0_win64_mingw-w64`
- The `Makefile` compiles every `src/*.c` automatically. `TARGET := AshenMonastery.exe`; `.vscode/launch.json`
  `program` must match. Keep `-std=c99 -Wall -Wextra`. The build must have **zero warnings**.
- **raylib 6.0 API check:** before using any raylib/rlgl/raymath function you are not 100% sure about, `grep` its
  signature in the include folder. Do not guess signatures.
- At startup call `ChangeDirectory(GetApplicationDirectory())` so relative asset paths work when the exe is double-clicked.
- raylib may be built without JPG support: prefer PNG for all images, and test-load every image file.

## 2. Working rules

- Work through the checklist in order. After **every** task: zero-warning build, run `--autotest` and `--tour`
  (section 15), **open and look at every screenshot**, fix what looks wrong, then `git commit`.
- Tags: `environment-done` after the environment tasks, `title-done` after the title screen, `relics-done` after
  relics + serpent, `final` at the very end.
- If a task runs long or gets risky, revert to the last commit and follow the cut order (section 17).
- Do not rewrite systems that already work (chests, checkpoints, AI, collision, autotest). Change how things look,
  extend them, and rename them.
- **Originality:** every character, name, creature, relic and building must be original. Do not copy characters,
  names, props, locations or designs from existing games, films or books.
- **Reference images:** the user put mood photos in `reference/` (castle interiors and exteriors). View them before
  building architecture, props, lighting and the title screen. Use them as **mood references only** (palette,
  lighting, materials, kinds of rooms, furniture, atmosphere). Build an **original** castle; do not copy a specific
  real building, film set or silhouette. Aim for the *feeling* of the photos in PS1 style, not photorealism.
  What the references show (use this even if an image fails to load):
  - a long **cloister corridor**: tall pointed windows down one side, ribbed vaulted ceiling, **hanging iron lanterns**,
    wall lanterns, framed paintings, wooden benches, polished dark flagstones reflecting light;
  - a round **tower study**: tall bookshelves curving around the walls, **spiral iron stairs** to a gallery, big
    pointed arches, a carved desk in the middle, an iron ring chandelier with candles, brass instruments;
  - a **library hall**: honey-colored stone, tall stained windows high up, long wooden tables with many candles,
    bookcases with arched tops;
  - a stone **alcove** with carved columns, pointed arches and an ornate tall standing mirror;
  - a **dormitory**: four-poster beds with heavy crimson curtains, a huge arched window with the moon, fireplace, rugs, trunks;
  - an **alchemy lab**: dark stone walls, shelves full of glass jars and bottles, cauldrons, a workbench, stools,
    a blue-lit window in an arched recess;
  - **exterior**: a dark castle with many tall conical spires on a snowy cliff under a stormy sky; at night, colored
    light (warm orange and cold teal) washing across its walls.
  - Overall palette: warm amber candlelight against cold blue moonlight, dark stone, crimson and gold accents.

---

## 3. The story

**Title:** THE ASHEN MONASTERY — subtitle: "Climb. Break the bells. Bring them home."

High on a storm-swept mountain stands the Ashen Monastery, a gothic castle and school for young mages. Ten nights
ago its Abbot made a pact with fire and became **the Red Abbot**. He cast **five cursed bells** from the ashes of the
dead; each time they toll, the monks forget who they were. To make himself immortal he bound his life into **five
Soul Relics** and his **serpent familiar**. Master **Oren** and the other apprentices are trapped in the **Sanctum**
at the summit. You are **Kael**, the youngest apprentice, armed with a wand. Climb the monastery, destroy the relics,
break the bells, free the serpent's cage and end the Red Abbot.

**How the story maps to the game systems:**
- Each wing holds one Cursed Bell and one Soul Relic. Every room has a reliquary chest. Most chests hold a
  **Ward Seal** (+1 heart, checkpoint). The **last chest opened in a wing** holds that wing's **Soul Relic**.
  Destroying the relic shatters the wing's bell and opens the wing's exit (section 9).
- Wing 5 also has the **caged serpent** and the **Red Abbot** (section 9).
- After wing 5, the **Sanctum** ending scene plays (section 12).

**Wings** (difficulty increases):

| # | Name | Chests | Enemies (guideline) | Room themes (section 7) |
|---|------|--------|---------------------|--------------------------|
| 1 | The Ash Gate | 3 | 4 Ashen Monks, 1 Choir Wraith | entrance + cloister corridors, tutorial-easy |
| 2 | The Hall of Prayer | 4 | 6 Monks, 2 Wraiths, 1 Ember Priest | great hall + dormitories |
| 3 | The Scriptorium | 4 | 4 Monks, 2 Wraiths, 4 Priests | library + tower study |
| 4 | The Ossuary | 5 | 7 Monks, 4 Wraiths, 3 Priests | alchemy vaults + crypt |
| 5 | The Bell Tower | 5 | Red Abbot + serpent + 3 Monks, 2 Priests, 2 Wraiths | bell tower, cage dais |

No enemies in the starting room of any wing.

**Ward Seal names** (HUD list title "Ward Seals 1/3"; banner "Ward Seal found: Seal of Cinders"):
- Wing 1: Seal of Cinders, Seal of the Iron Key, Seal of the Pilgrim
- Wing 2: Seal of Hymns, Seal of the Kneeling Saint, Seal of Candlewax, Seal of Silence
- Wing 3: Seal of Ink, Seal of the Burned Page, Seal of the Quill, Seal of Forgotten Names
- Wing 4: Seal of Bone, Seal of the Nameless, Seal of Marrow, Seal of the Last Rite, Seal of Dust
- Wing 5: Seal of Embers, Seal of the Rope, Seal of the Toll, Seal of the Abbot's Ring, Seal of the Red Flame

The chest that turns out to be the last one in its wing shows its relic instead of a seal; its seal counts as found too
(the HUD seal counter always reaches the full number).

---

## 4. Core rules (forgiving — the user is not a gamer)

- Player has **5 hearts**. Every hit costs exactly 1 heart. 1.0 s invulnerability after a hit (character blinks).
- Opening a chest restores 1 heart and sets a **checkpoint** at that chest.
- On death: "THE FIRE TAKES YOU" / "[ ENTER ] Rise from the ashes" → respawn at the last checkpoint (or wing start)
  with full hearts. Opened chests, destroyed relics, the broken cage, the dead serpent and killed enemies stay that way.
  Surviving enemies return to their spawn points. A living boss returns to full HP.
- **Aim assist:** on every cast, if a valid target (enemy, relic, cage) is within 12 units and in line of sight, the
  character turns to the nearest one and the spell aims at it. Floating relics have priority while they exist.
- **Telegraphed attacks:** every enemy attack has a 0.5 s wind-up (enemy glows red and stops); the hit only lands if
  the player is still in range when the wind-up ends.
- Enemies are slower than the player (per-wing multipliers in config).
- Dash (Shift): short burst with invulnerability, 0.8 s cooldown.

**Controls:** WASD move (relative to camera) · Mouse look · **Left click = Red Lightning** · Shift = dash ·
E (hold) = open chest / talk · I / Tab = inventory · **M = full-screen map** · Esc / P = pause · Enter = confirm.
Debug keys (keep in final build, list in README): F1 debug overlay, **F2 toggle post-process**, F3 god mode,
F4 complete current wing instantly.

---

## 5. Player & the wand

- Third-person player character **Kael** holding a **wand** in the right hand (thin dark wood, ≈ 0.35 long, small
  glowing red tip drawn unlit). Idle: wand held forward-down. Cast: the right arm snaps forward toward the target for 0.2 s.
- **Red Lightning** (left click): cooldown 0.45 s, range 12 units, damage 1.
  - Aim per section 4; otherwise fire along the camera's forward direction.
  - Hit test: march along the ray; stop at the first solid cell, prop collider or target sphere. The beam destroys any
    fireball it passes through.
  - Visual (0.15 s): a jagged bolt from the wand tip to the hit point, 8–12 segments with random sideways offsets
    **re-randomized every frame** (crackle), 2–3 short forks. Each segment drawn twice with additive blending: thin
    bright core (≈ 255,190,190) and a wider red glow (≈ 220,20,30 with alpha). Thin `DrawCylinderEx` segments or
    camera-facing quads.
  - Flash light: world-shader uniforms `flashPos, flashColor, flashRadius`; strong red light (radius ~6) at the hit
    point during the beam so the walls flash red. Red spark burst at the hit point; camera shake 0.15.
  - Sound: sharp crack (best matching impact/metal file at high pitch, or a procedural noise-burst crack via
    `LoadSoundFromWave`).
- The Choir Wraith visibility rule stays (only damageable when lit); the beam's own flash does **not** count as light.
- HUD crosshair: small red dot.

---

## 6. Enemies

Keep the existing AI (ported from the original 2D game): idle enemies sweep their gaze around a base angle; they notice
the player via sight range + view cone + line of sight (grid raycast), or hearing range regardless of cone; once
alerted they stay alerted; steer toward the player, slide along walls, enemy–enemy separation.

| Enemy | HP | Speed | Behavior | Look |
|-------|----|-------|----------|------|
| **Ashen Monk** | 2 | 2.2 | walks to the player with stutter-steps; melee wind-up 0.5 s within 1.4 | charred black-grey hooded robe with glowing orange ember cracks, arms hanging low |
| **Choir Wraith** | 2 | 1.6 | floats through walls (stays inside the map border); damageable only while **visible** (inside the player light radius or near a light source), otherwise very faint and invulnerable; on first noticing the player within 7 units: scare sound + camera shake; touch attack with wind-up; occasional moan ("singing") when near | pale grey-white, translucent (alpha ≈ 0.45), hooded, open dark mouth, no legs, bobbing |
| **Ember Priest** | 3 | 2.0 | keeps 5–8 units away and strafes; every 2.2 s with line of sight throws a **fireball** (orange-red glowing, speed 5, 1 heart); fireballs die on walls | dark crimson robe, tall hood, burning censer |
| **The Red Abbot** (boss, wing 5) | 20 | 1.8 | immortal until the serpent dies (section 9); then chases, melee wind-up 0.6 s, ring of 8 fireballs every 3.5 s, summons 2 Choir Wraiths once at ≤ 50% HP; boss bar "THE RED ABBOT" | 1.8× size, crimson-and-black robes, tall iron bell-shaped mitre, burning eyes, two orbiting fireballs |
| **Ember Serpent** (wing 5) | 8 | fast | see section 9 | long body of black scales with glowing orange cracks, red eyes |

All enemies: about 15% taller and thinner than normal proportions, **glowing red eyes** drawn unlit (first thing you
see in the dark), **twitching** (at random intervals of 0.6–2.5 s the head snaps to a random angle for 0.08–0.15 s),
white hit flash for 0.12 s. Per-wing multipliers scale speed, sight range and priest fire rate.

**Character visuals right now:** characters are code-built. Until the user provides models, keep them code-built but
make them **non-blocky** where cheap (cone robes flaring to the floor, sphere heads, cylinder limbs, half-sphere hoods).
**Model slots:** for each role (player, each enemy type, the boss, the serpent, each Sanctum NPC) and each relic
(`relic_grimoire`, `relic_ring`, `relic_locket`, `relic_chalice`, `relic_crown`), if `assets/models/<role>.glb`
exists, load it with `LoadModel` + `LoadModelAnimations` instead of the code-built version. Map animations by
keyword (case-insensitive contains): idle, walk/run, attack/cast, hit, death; missing ones fall back to idle without
crashing. Scale each model to the role height from config via its bounding box, feet on the floor, per-role yaw offset
in config. Models use the world shader. Write `assets/models/README.txt` listing the expected file names.

---

## 7. The world: a PS1 gothic castle (no blocks)

### 7.1 Wing map format (`assets/wings/wingN.txt`, `assets/wings/sanctum.txt`)

Plain text. First line: the wing name. Then the grid, top row = north (−Z), left column = west (−X). Each character is
one 1×1 floor cell. All rows equal length. Max size 56 × 40. The grid drives collision, AI and the minimap; the
**rendering builds real architecture from it** (7.3).

| Char | Meaning | Solid? |
|------|---------|--------|
| `#` | stone wall | yes |
| `W` | dark wood-paneled wall | yes |
| `B` | bookshelf wall | yes |
| `P` | round pillar (collision = full cell) | yes |
| `T` | stone wall with a wall sconce on every face that touches floor | yes |
| `.` | stone floor | no |
| `=` | crimson carpet runner | no |
| `,` | wooden floorboards | no |
| `x` | bone pile decoration (not solid) | no |
| `@` | player start | no |
| `C` | reliquary chest (solid) | yes |
| `E` | exit door (solid until opened) | until open |
| `s` `g` `w` `Q` | Ashen Monk, Choir Wraith, Ember Priest, Red Abbot spawn | no |
| `O` `a` | Sanctum only: Master Oren, apprentice/monk NPC | no |

Cells with `@ C E s g w Q x O a` get the floor type of their nearest floor neighbor (fallback: stone).

**Map design rules:** outer border fully solid; rooms connected by doorways 2 wide and corridors 2–3 wide; exactly one
`C` per room and the number of chest rooms equals the wing's chest count; start area and corridors have no chest; rooms
about 7×7 to 12×12; `P` pillars in large rooms for cover; wing 1 small and simple (3 rooms around an entrance hall,
3–5 minutes to complete); the exit `E` sits in a wall reachable from one side; wing 5's bell tower room is at least
12×12 with pillars, the cage dais and the boss start far apart in it.

**Loader validation:** row lengths, closed border, exactly one `@`, at least one `E` (not required in `sanctum.txt`),
chest count matches config (0 for the Sanctum), all floor reachable from `@` (flood fill treating `C` and closed `E` as
solid; chests adjacent to reachable floor). Clear errors with line/column on stdout. In `--autotest` any error → exit 1.

### 7.2 Materials

Separate **repeating textures per material** (`TEXTURE_WRAP_REPEAT`, `TEXTURE_FILTER_POINT`, no mipmaps, resized to
**128×128** nearest-neighbor for the PS1 look) with **world-space UVs** (u,v from world position × material scale, about
one repeat per 2 world units) so textures flow continuously across cells. World meshes: **one Mesh per (chunk, material)**
with positions, normals and UVs; 16×16-cell chunks.

Materials: `wall_stone` (large rough ashlar blocks), `trim_stone` (smooth pale limestone), `floor_stone` (big worn
flagstones), `carpet` (deep crimson with gold border), `wood_dark` (beams, doors, furniture, floorboards), `ceiling`
(dark plaster or stone), `pillar` (smooth dark stone), `metal` (dark iron), `glass` (generated stained glass),
`banner` (generated cloth with an original emblem: a key and a crescent moon), `books` (generated spines), `bone`,
`cobweb` (generated, alpha).

Real textures: free **CC0** textures from **Poly Haven** in `assets/textures/` (try the public API
`https://api.polyhaven.com/assets?t=textures` and `https://api.polyhaven.com/files/<id>`, 1k diffuse, PNG preferred).
If a download fails, print instructions for the user (open https://polyhaven.com/textures, search the keyword,
download the 1k PNG diffuse, save under the material's file name) and use the generated texture. List every Poly Haven
asset used in `CREDITS.md`.

### 7.3 Architecture from the grid

Classify floor cells: **room cells** (open area at least 4×4) and **corridor cells** (everything else). Then build
(low-poly, PS1-appropriate polygon counts):
1. **Wall heights:** rooms 6.0, corridors 3.5 (config). Where a corridor opens into a room, the room wall continues
   above the opening from 3.5 to 6.0 (lintel wall).
2. **Pointed gothic arches** at every corridor↔room opening: `trim_stone` arch trim following a two-centered pointed
   arch, 6–8 segments per side, protruding 0.1 on the room side.
3. **Wall layering:** stone **plinth** at the base (0.4 high, 0.08 deep, `trim_stone`), main face (`wall_stone`),
   **cornice** at the top (0.3 high, 0.15 deep).
4. Wall faces **subdivided into ~1×1 quads** (needed for affine texturing and vertex wobble).
5. **Pilasters:** 8-sided half-columns with base and capital every 4 cells along long room walls (skip windows, doors, sconces).
6. **Round pillars:** `P` cells become 8-sided columns (radius 0.35) with square base and capital.
7. **Corners:** thin vertical trim on outer room corners.
8. **Ceilings:** rooms get dark wooden beams every 2 units; rooms 8×8 or larger get **ribbed vaults** instead (two
   diagonal pointed-arch ribs per 4×4 bay). Corridors: flat ceiling with a beam every 3 units.
9. **Floors:** `floor_stone`; `=` carpet runner quads raised 0.01; `,` wooden floorboards.
10. **Exit doors:** tall pointed-arch double door (`wood_dark` with `metal` straps and ring handles); both leaves swing
    inward when the wing's exit opens.
11. **Chests:** low-poly reliquary chest (curved lid of 4–6 segments, metal corner bands, small legs); lid rotates on
    its back edge.
12. **Sconces** (`T`): iron bracket with a torch head and flickering emissive flame quads.

### 7.4 Props and room themes

Deterministic seeded placement per wing. Props only **against walls** in room cells; never in doorway lanes or a
2-cell path in front of an opening; never on chests, spawns or the start. Re-run the reachability check after placing
and remove any prop that breaks it. Solid props add small AABBs to collision. All built in code from low-poly
primitives (6–8-sided cylinders, cones, boxes) with the materials.

- Everywhere: tall **candelabras** (3 candles, light source), **cobwebs** in upper corners, small **rubble**,
  **banners/tapestries** between pilasters, **framed paintings** (dark frames, dim generated images), small **rugs**.
- **Corridors** (all wings): **hanging iron lanterns** on chains every 4 cells (light sources), wall lanterns between
  windows, wooden **benches** under windows. `floor_stone` gets a faint specular highlight so lights reflect like
  polished stone.
- Per wing:
  1. Ash Gate: **suits of armor** on plinths, an iron **portcullis** above the entrance, stone benches, many lanterns
     (brightest, easiest wing).
  2. Hall of Prayer: in the biggest room long **dining tables** with benches and many candles (3-wide aisle free) and an
     iron ring **chandelier**; smaller rooms are dormitories with **four-poster beds** (crimson curtains), trunks, a stone
     **fireplace** with animated fire (light source).
  3. Scriptorium: arched-top **bookshelves** against all room walls (2.5 tall), long reading tables with candles; the
     largest room is a **tower study**: curving shelves, a decorative **spiral stair** (non-walkable, solid cylinder
     collision) up to a gallery ring, carved central desk, brass globe and instruments.
  4. Ossuary: alchemy rooms with **shelves of glass jars and bottles** (colored emissive tint), iron **cauldrons** with a
     faint green glow, workbenches and stools; crypt rooms with **stone coffins**, **bone piles**, **skull niches**,
     dripping candles; one stone **alcove** with an ornate tall **standing mirror** (gold frame, pointed top, dark glass
     with a faint blurry reflection or dark animated noise if a reflection is too costly).
  5. Bell Tower: **broken pews**, a huge **bell** hanging high in the biggest room (lathe mesh, metal), ropes, rubble,
     cracked walls, the **serpent cage dais** (section 9).

### 7.5 Windows, moonlight and weather

- **Pointed-arch windows** every 4–6 cells on room walls that face the map border: `trim_stone` frame, stone mullions
  (2 lights), emissive generated **stained glass** (deep blue, violet, a little crimson/gold, lead lines, brightest in
  the middle). Each window is a **cold blue light** source.
- **Light shafts:** translucent additive pale-blue wedges from each window onto the floor (transparent pass), with
  slowly drifting **dust motes**.
- **Lightning:** every 20–45 s all window lights and shafts flash white-blue twice within 0.4 s; thunder 1–2 s later
  (existing rumble or procedural filtered noise). Config to disable.
- **Bell tolls:** every 25–40 s a distant bell tolls (synthesize if no file: decaying sine partials ≈ 110, 220, 277,
  330, 440 Hz, 4 s). On each toll the lights flare briefly. Fewer tolls as bells are broken (none after all five).
- **Embers:** ~60 tiny orange additive particles rising near fires and sconces. **Falling ash** only in wing 5.

### 7.6 Lighting, palette and the PS1 pipeline

**Per-pixel lighting** in `assets/shaders/world.vs/.fs` (GLSL 330, raylib default attribute/uniform names
`vertexPosition, vertexTexCoord, vertexNormal, vertexColor, mvp, matModel, texture0, colDiffuse`; set
`shader.locs[SHADER_LOC_MATRIX_MODEL]` and `[SHADER_LOC_VECTOR_VIEW]`):
- Light arrays, `MAX_LIGHTS 16` (position, color, radius, flicker phase); each frame pick the 16 lights nearest the
  camera from sconces, candelabras, lanterns, chandeliers, fireplaces and windows. Plus a player light and the spell
  flash light as separate uniforms. Lambert diffuse, smooth radius falloff, per-light candle flicker.
- Exponential-squared fog by distance to the camera with per-wing density and tint.
- **PS1 touches:** affine texture mapping (`noperspective` texcoords), subtle vertex snapping (config, default on, low).
- If shaders fail to load, run with raylib's default shader.

**Palette — "moonlit gothic":**
- Shadows deep blue-indigo, never pure black (fog ≈ 0.025, 0.03, 0.06).
- Moonlight cold pale blue (≈ 0.55, 0.65, 0.9). Candle/torch light warm amber (≈ 1.0, 0.68, 0.35).
- Stone desaturated grey with a slight green-grey tint. Accents: deep crimson (banners, carpets) and old gold.
- **Red is reserved for danger and magic:** enemy eyes, the Red Lightning, getting hurt (red vignette pulse), relics,
  fireballs, the boss.

**Post-process:** render the 3D scene into a **640×360** `RenderTexture2D` (point filter), draw it scaled to
1280×720 through `assets/shaders/post.fs` (fragment-only): gentle **split-tone grade** (cool blue shadows, warm amber
highlights), slightly lowered saturation, mild contrast, film grain ≈ 0.04, vignette, subtle chromatic offset
(≈ 1 low-res pixel), 5-bit-per-channel quantization with 4×4 ordered dither (config flag). `gradeMode` uniform:
0 = moonlit gothic, 1 = warm gold (Sanctum ending). **F2** toggles the post-process. If `post.fs` fails, draw the
low-res texture scaled up without it. All UI is drawn afterward at full resolution.

**Per-wing mood** (wing config table): 1 cold moonlit blue with many candles (easiest to see); 2 warm gold candlelight
and crimson banners; 3 amber reading lamps with green-tinted shadows; 4 sickly pale green-cyan, few lights, dense fog;
5 stormy blue, frequent lightning, red light around the boss and the cage.

---

## 8. Camera & movement

- Third-person orbit camera. `DisableCursor()` while playing; `EnableCursor()` in menus, pause, inventory, map,
  dialogue and death.
- Mouse X → yaw, mouse Y → pitch (clamp −55° to +15°), sensitivity in config. Distance 3.6, target at the player's head
  (y ≈ 1.5), slightly over the right shoulder (offset 0.4).
- **Camera collision:** step from the target toward the desired position in 0.05 increments; stop before entering a
  solid cell or prop collider, or going above the local ceiling − 0.2 / below 0.2. Smooth the distance so it does not pop.
- Movement relative to camera yaw; the body turns smoothly toward movement and snaps to the aim target when casting.
- XZ collision: player 0.6×0.6 box, axis-separated slide against solid cells and prop AABBs. Enemies use the same
  function with their own size. No jumping; one floor level per wing.
- Camera shake on hits, chest/relic/cage events, door opening, Wraith scares, lightning strikes.

---

## 9. Chests, relics, the caged serpent and wing exits

**Chests:** prompt "[E] Open chest" within 1.6 units when roughly facing it. Hold E 1.5 s (progress ring); releasing,
taking a hit or moving away cancels. On open: lid opens, gold glow, +1 heart (max 5), checkpoint set.
- Not the last chest in the wing → banner "Ward Seal found: <name>".
- **The last chest in the wing → the wing's Soul Relic** (below).

**Soul Relics** (one per wing, in this order; original designs, low-poly in code or from `assets/models/relic_*.glb`):

| Wing | Relic | Design |
|------|-------|--------|
| 1 | **The Ashbound Grimoire** | thick black leather book, iron corner caps, heavy iron clasp, red wax seal; dark smoke wisps rising from its pages |
| 2 | **The Ember Ring** | heavy dark-gold band with a large faceted **red** stone that pulses like a heartbeat |
| 3 | **The Moonsilver Locket** | oval silver locket on a fine chain, a crescent moon engraved on the front, cold blue glow leaking from its seam |
| 4 | **The Chalice of Cinders** | tall dark-gold chalice on a hexagonal foot, glowing embers swirling inside the cup |
| 5 | **The Thorned Crown** | black iron crown of twisted thorns with one blood-red gem at the front, faint red sparks crawling over it |

**Relic moment:**
1. The lid bursts open; the relic **rises** and floats at chest height, slowly rotating, glowing, with a low droning
   whisper and the nearby light dimming. The player can move.
2. Banner "SOUL RELIC FOUND: <name>" and prompt "Strike it with your wand!"
3. It takes **3 Red Lightning hits**; each hit shakes it, adds emissive red cracks and plays a scream. Relics never
   hurt the player.
4. Third hit: it **shatters** into spinning shards with a red-white burst, strong camera shake, a fading scream, then the
   deep **bell-crack** sound, a red screen flash and the banner "THE FIRST BELL SHATTERS" (SECOND … FIFTH). The wing's
   exit doors swing open (wing 5: see below).
5. HUD: a row of 5 relic icons plus 1 serpent icon under the hearts (dark silhouettes; destroyed ones cracked with a
   gold outline).

**The caged serpent (wing 5):**
- In the bell tower room: a raised stone dais with a tall round **iron cage** (8 vertical bars, domed top, chains up to
  the ceiling). Inside lies a coiled **Ember Serpent** (chain of 20–30 low-poly segments along a spline with sine-wave
  motion, flicking tongue). It raises its head and hisses when the player comes near.
- The cage is **sealed** (glowing red runes; spells spark off) until all five relics are destroyed. When the fifth
  relic shatters: banner "THE CAGE SEAL IS BROKEN", the runes fade.
- **Break the cage:** 6 wand hits; each bends/cracks bars with sparks and a metal clang while the serpent thrashes. On
  the sixth hit the bars burst outward and the serpent is free.
- **Serpent fight:** HP 8; slithers fast in curves and keeps distance; every 2.5 s a telegraphed **lunge** (rears up and
  glows red 0.6 s, then strikes 4 units forward; 1 heart; dodge sideways or dash). Easy to hit while rearing. On death it
  writhes and bursts into embers; its HUD icon cracks.
- **The Red Abbot** waits in the same room far from the cage, **not alerted** and protected by a visible red shield
  shell (spells spark off, boss bar reads "IMMORTAL") until the serpent dies. Then banner "THE RED ABBOT IS MORTAL",
  the shield breaks and the boss fight starts.
- **Wing 5 exit** opens only when all chests are opened, all relics destroyed, the serpent dead and the Abbot dead →
  walk through → the Sanctum.

**Wing exit:** walking through the opened exit → fade to black → next wing with the banner "THE SECOND BELL — The Hall
of Prayer", etc.

**Save progress:** `save.txt` stores the highest unlocked wing. The title menu shows "Continue" if a save exists.

---

## 10. Minimap and full map

- **Corner minimap** bottom-right, ~230×170 px, rounded dark panel with a thin old-gold border, 85% opacity, north-up,
  centered on the player, about 24×18 cells visible. Draw from the grid: walls dark stone, floor lighter grey-blue,
  carpets darker red, rooms vs corridors slightly different shades; render the full wing once into a `RenderTexture`
  at load and scroll a window of it.
- **Exploration reveal:** cells within 7 units of the player with line of sight become discovered; undiscovered cells
  are hidden. Saved with the checkpoint.
- **Always shown** (it is a guide): unopened chests as gold icons (even undiscovered), opened chests as grey checkmarks,
  the exit as a door icon (red locked / green open), the player as a white arrow in the facing direction, in wing 5 the
  cage icon. Enemies are not shown, except the boss as a pulsing red dot once alerted. Label under it: wing name and
  "Ward Seals 1/3".
- **M** opens a full-screen map (game paused): the whole wing, discovered parts, all icons, a legend. M or Esc closes.

---

## 11. Title screen and menus

**The scene** (3D, through the same PS1 pipeline; look at the exterior references; original castle design):
- A tall jagged **cliff** (noise-displaced cone/cylinder mesh, dark rock, snow where normals point up).
- The **monastery-castle** on top: a cluster of round towers (8–12 sides) of different heights with tall **conical
  spires** (some with needle tips), connecting walls with battlements, a long hall with pointed windows, buttresses.
  Many **lit windows** (warm amber emissive quads, a few flickering, a few cold blue).
- **Sky:** dark stormy gradient dome, layered scrolling generated cloud quads, a pale **moon** half-hidden by clouds.
- **Weather:** slow falling snow, low mist sheets drifting around the cliff base.
- **Lightning** every 6–12 s: the castle flashes cold white-blue twice for 0.2 s, thunder 1 s later.
- **Colored light wash:** two big soft lights, warm orange and cold teal, slowly sweeping across the walls in opposite
  directions.
- **Crows:** 5–8 small black birds (two flapping wing triangles each) circling the spires.
- **Camera:** slow cinematic drift, starts low looking up, slowly rises and orbits a few degrees, loops smoothly,
  slight handheld sway.

**The menu:**
- On launch: black → the scene fades in with a distant bell toll → the title "THE ASHEN MONASTERY" fades in letter by
  letter in the title font with a soft glow, then the subtitle → "Press any key" pulses.
- After a key: the menu slides in on the left over a soft dark gradient panel: **New Game, Continue (if a save exists),
  Controls, Credits, Quit**. Keyboard (W/S, arrows, Enter) and mouse (hover + click).
- Hovered item grows slightly, turns candle-gold, a small flickering flame icon appears, soft tick sound. Selecting
  plays a deep whoosh and a lightning flash and fades to black.
- **Controls** screen: two-column list of all keys with key-cap boxes. **Credits:** slow scroll (student name
  placeholder "Created by: ___", all assets from CREDITS.md). Esc returns.
- Ambience: wind + the ambience track at low volume.

**Intro** (after New Game, before wing 1; white text fading in on black, Enter/click skips), one line at a time:
1. "Ten nights ago, the Red Abbot cast five bells from the ashes of the dead."
2. "He hid his life in five cursed relics, and in the serpent he keeps caged in the tower."
3. "Master Oren and the other apprentices are trapped in the Sanctum at the summit."
4. "Climb, Kael. Destroy the relics. Break the bells."

---

## 12. The Sanctum ending

- Map `assets/wings/sanctum.txt`: a long candle-lit hall about 9 wide × 22 deep, pillars along both sides, crimson
  carpet down the middle, player start at the south end, `O` for Master Oren at the north end, `a` for NPCs. Use the
  full architecture system plus chandeliers and many candles.
- Lighting: **warm gold**, the first warm scene in the game (dark amber fog, higher ambient, `gradeMode = 1`). No ash,
  no bell tolls, no lightning. Gentle music: the ambience at lower pitch/volume, or silence.
- NPCs (non-hostile, no red eyes, normal proportions, face the player, idle breathing; model slots apply):
  - **Master Oren**: grey robe, long white beard, tall staff with a gold glowing tip.
  - **Ilsa** (dark blue robe), **Tobin** (brown robe), **Mira** (green robe) near him.
  - 4–6 freed monks in light grey robes lining the carpet.
- Friends speak one line when the player comes within 2 units (text box at the bottom):
  - Ilsa: "You're covered in ash. Welcome back."
  - Tobin: "Five bells. I counted every one."
  - Mira: "Was he... was the Abbot still human, at the end?"
- At Master Oren: prompt "[E] Speak with Master Oren"; dialogue box, Enter/click advances:
  1. "Kael... the fifth bell has fallen. I heard it break from here."
  2. "We thought the fire would take us all. Ilsa kept the candles lit. Tobin counted every toll."
  3. "The Abbot rang those bells to keep the mountain burning. You walked through his fire, and you came back."
  4. "Rest now. Tomorrow, you are no longer an apprentice."
- Fade to black → final screen "THE BELLS ARE SILENT": Ward Seals found, relics destroyed, enemies defeated, total time,
  then short credits ("Created by: ___" + asset credits). Enter returns to the title.

---

## 13. HUD and other UI

Fonts: Pirata One (titles) and Crimson Text (body), searched recursively in `assets/fonts/`; fall back to the default
font. UI palette: bone white (226,220,200), candle gold (232,184,88), deep crimson (150,18,28), near-black panels.

- **HUD:** hearts top-left (pixel-style hearts), dash-ready bar, relic + serpent icon row; wing name top center;
  Ward Seal checklist top-right (found ones gold with a check, unknown "???"); interaction prompts and the chest progress
  ring at screen center; red crosshair dot; vignette; red hurt pulse; minimap bottom-right; boss bar at the bottom when
  the Abbot is awake ("IMMORTAL" while shielded).
- **Banners:** wing title, seal found, relic found, bell shatters, cage seal broken, Abbot mortal. Fade in/out.
- **Pause:** Resume / Restart wing / Quit to title. **Inventory:** all seals and relics across wings, hearts, enemies
  defeated, time played. **Death:** "THE FIRE TAKES YOU" / "[ ENTER ] Rise from the ashes".
- Controls hint during the first 20 seconds of wing 1.

---

## 14. Audio (all optional, fail silently)

`InitAudioDevice()` at startup. The user downloaded (possibly still zipped or nested; unzip if needed): Kenney
"RPG Audio" and "Impact Sounds", OpenGameArt ghost moaning voices, `scaryhighpitchedghost.ogg`,
`dungeon_ambient_1.ogg`. Pick the best file per role by name; synthesize with `LoadSoundFromWave` where noted.
Roles: wand crack, spell hit, player hurt, chest opening (creak/latch), seal found (metal/coin), door opening, footsteps
(`footstep0X.ogg`, every ~0.4 s while walking, quiet), Wraith scare, Wraith moan, fireball whoosh, relic whisper drone,
relic scream, relic shatter, bell crack (synth if missing), bell toll (synth), thunder (synth if missing), cage clang,
serpent hiss, menu tick and whoosh, ambience loop (Music stream, low volume). Small random pitch variation on repeated
sounds. Master volume in config.

---

## 15. Testing — you cannot see the window, so build tools to see it

- `AshenMonastery.exe --wing N` starts directly in wing N (N = 6 or `--sanctum` for the ending hall).
- `--autotest`: for each wing 1–5 and the Sanctum: load + validate, run 90 frames with fixed dt, save
  `shots/wingN.png` and `shots/sanctum.png` with `LoadImageFromScreen()` + `ExportImage()` (**not** `TakeScreenshot`,
  it may strip folder paths). Also `shots/title.png` (after the title has fully appeared), `shots/enemies.png` (each
  enemy type in front of the player), `shots/relics.png` (all five relics floating in a row) and `shots/cage.png`.
  Print a summary (cells, chunks, vertices, chests, enemies, lights, props per wing) and exit 0 (1 on any error).
- `--tour`: for each wing, 4 screenshots `shots/tour_wN_K.png`: (1) third-person view at the start, (2) the center of
  the largest room looking at a wall with a window (or the longest wall), (3) a corridor looking along it, (4) view 1
  with the minimap visible. Print average FPS. Exit 0.
- After every task, open the screenshots and check: no visible block grid; architecture, props and windows present;
  player and nearby floor readable while dark corners stay dark; colors moonlit gothic (blue shadows, amber candles,
  crimson/gold accents), not a heavy red filter; HUD and minimap readable; no z-fighting or texture seams. Fix before
  moving on. If average FPS is under 60, reduce `MAX_LIGHTS` to 8 and prop density.
- At the end of each task tell the user in one line what to try (e.g. "run `make run` and open the first chest").

---

## 16. Work checklist (in order — verify what's done first, see section 0)

1. Story, names and texts (sections 3, 6, 11 intro, 13) — Ashen Monastery names everywhere the player sees them.
2. Red Lightning wand (section 5).
3. PS1 pipeline with the **moonlit gothic** grade (7.6) — replace any old red/black grade.
4. `--tour` screenshots (section 15).
5. Materials with world-space UVs and Poly Haven textures (7.2).
6. Architecture from the grid (7.3).
7. Props and room themes (7.4).
8. Windows, moonlight, light shafts, lightning, bell tolls, embers (7.5).
9. Per-pixel lighting, palette, per-wing mood (7.6) → tag `environment-done`.
10. Minimap and full map (section 10).
11. Enemy restyle: non-blocky shapes, red eyes, twitching; model slots (section 6).
12. Title screen and menus (section 11) → tag `title-done`.
13. Soul Relics, caged serpent, immortal-until-serpent boss (section 9) → tag `relics-done`.
14. Sanctum ending (section 12).
15. Wrap up: `make release` works; `README.md` (story, controls, build steps, file overview, debug keys);
    `CREDITS.md` (fonts — SIL OFL, Google Fonts; Kenney RPG Audio & Impact Sounds — CC0; OpenGameArt ghost voices by
    qubodup, Scary High-pitched Ghost by Fupi, Loopable Dungeon Ambience by JaggedStone — CC0; Poly Haven textures —
    CC0, by exact asset name; everything else generated in code); final `--autotest` passes → tag `final`.

## 17. Cut order if time runs short

Ribbed vaults (beams everywhere) → title crows and colored light wash → dust motes → in-game lightning → the standing
mirror and spiral stair → other wing-specific props (keep candelabras, lanterns, banners, cobwebs, bookshelves) →
light shafts → falling ash and embers → friend lines in the Sanctum.
**Never cut:** world-space materials, wall heights + arches + trims, windows, the moonlit palette and per-pixel
lighting, the PS1 pipeline, the Red Lightning wand, the minimap, the title castle scene, the five relics and the caged
serpent, the Sanctum ending with Master Oren's dialogue.

---

## 18. Project layout and code rules

```
src/main.c          window, main loop, flags (--wing N, --sanctum, --autotest, --tour)
src/config.h        ALL tunables + per-wing table
src/game.h/.c       state machine, wing flow, checkpoints, seals, relics, serpent/boss gating, save
src/world.h/.c      wing loader + validation, grid collision, line of sight, chests, exits
src/architecture.c  grid → walls, arches, trims, pillars, ceilings, floors, windows (meshes per chunk/material)
src/props.c         prop placement + low-poly prop meshes + prop colliders
src/textures.h/.c   material loading + generated fallback textures
src/character.h/.c  code-built characters + animations + model slots
src/player.h/.c     movement, camera + camera collision, Red Lightning, aim assist, dash, hearts
src/enemy.h/.c      Ashen Monk, Choir Wraith, Ember Priest, Red Abbot, Ember Serpent, fireballs
src/relics.h/.c     Soul Relics, relic moment, cage
src/render.h/.c     shaders, light selection, PS1 pipeline, draw passes, particles
src/minimap.h/.c    minimap + full-screen map
src/title.h/.c      title castle scene + menus + intro
src/audio.h/.c      sounds, synthesized sounds, ambience
src/ui.h/.c         HUD, banners, pause, inventory, death, dialogue, Sanctum final screen
assets/wings/wing1.txt ... wing5.txt, sanctum.txt
assets/shaders/world.vs, world.fs, post.fs
assets/textures/  assets/models/  assets/fonts/  assets/audio/  reference/  shots/
```
Split files differently if the existing code is already organized another way; keep it readable.

**Code rules:**
- C99, no external libraries besides raylib (raymath/rlgl are part of raylib). No C++.
- Keep functions small and readable — the student must be able to explain the code. Short comment at the top of each
  file explaining what it does.
- One `Game` struct passed by pointer; module-private statics only for loaded GPU/audio resources.
- Unload every GPU/audio resource on shutdown.
- Fixed array sizes with `#define` limits in config.h (e.g. MAX_ENEMIES 64, MAX_BOLTS 128, MAX_CHESTS 8,
  MAX_LIGHTS 16, MAX_PROPS 512).
- Frame dt clamped to 0.05.
- Keep `NOTES.md` with status, decisions, and anything the user should know.