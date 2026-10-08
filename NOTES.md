# The Ashen Monastery — Build Notes

## Master spec status (CLAUDE.md, section 16) — checked 2026-10-04

The combined master spec replaced CLAUDE.md. Old specs moved to `docs/old_specs/`
(`CLAUDE_blackthorn_manor.md`, `UPGRADE_ASHEN.md`; there was no `ENVIRONMENT_PS1.md`).
Mood photos are in `reference/` (used for palette/lighting/room types only; the castle is original).

| # | Item | Status at the start | Now |
|---|------|---------------------|-----|
| 1 | Story, names, texts | mostly done | done |
| 2 | Red Lightning wand | done | done |
| 3 | PS1 pipeline, moonlit grade | old red/black grade | done |
| 4 | `--tour` screenshots | not started | done |
| 5 | Materials, world-space UVs | not started | done |
| 6 | Architecture from the grid | not started | done |
| 7 | Props, room themes | not started | done |
| 8 | Windows, moonlight, weather | partly | done |
| 9 | Per-pixel lighting, palette, mood | not started | done (tag `environment-done`) |
| 10 | Minimap + full map | not started | done |
| 11 | Enemy restyle, model slots | partly | done |
| 12 | Title screen + menus | not started | done (tag `title-done`) |
| 13 | Relics, serpent, immortal boss | not started | done (tag `relics-done`) |
| 14 | Sanctum ending | mostly done | done |
| 15 | Wrap up | - | done (tag `final`) |

Nothing from the cut list (section 17) had to be cut.

**For the student:** write your name into "Created by: ___" - it appears in `src/ui.c` (final
screen) and `src/title.c` (credits). The paths in CLAUDE.md section 1 (`C:/Users/NAT/...`) are
different on this PC; see below.

Environment: the spec's `C:/Users/NAT/...` paths don't exist on this PC; the real paths are
`C:/Users/HP GM/Downloads/w64devkit/bin` and `C:/Users/HP GM/Downloads/raylib-6.0_win64_mingw-w64`
(the Makefile default). Screenshots are exported from the virtual-screen render texture instead of
`LoadImageFromScreen` (the window is scaled to fit this 1024x768 monitor; the texture is always 1280x720).

Progress log (newest last) is in the "Master spec" sections at the end of this file.

---

# History: Blackthorn Manor build notes

Decisions made while building, and things worth knowing.

## Environment (this machine)
- The paths in CLAUDE.md (`C:/Users/NAT/...`) don't exist here. Actual locations:
  - w64devkit: `C:/Users/HP GM/Downloads/w64devkit/bin` (already on PATH)
  - raylib 6.0: `C:/Users/HP GM/Downloads/raylib-6.0_win64_mingw-w64`
- That path contains a space, so the Makefile quotes `-I"$(RAYLIB_PATH)/include"` / `-L"..."`.
  The Makefile default `RAYLIB_PATH` points at the real location, so plain `make` works.
  `.vscode/*.json` were updated to the same paths.
- `make autotest` = build + run `--autotest`.

## Virtual screen (`src/screen.c`)
- This display is **1024×768**, so a 1280×720 window doesn't fit (the right side was cut off).
- The game renders into a fixed 1280×720 render texture, which is scaled (letterboxed) into a
  resizable window sized to fit the monitor. All game and UI code uses 1280×720 coordinates;
  mouse coordinates are remapped with `SetMouseOffset/SetMouseScale`.
- Autotest screenshots are exported from that render texture (`LoadImageFromTexture` + flip +
  `ExportImage`) instead of `LoadImageFromScreen`, so they are always the full 1280×720 frame.

## Assets
- Archives were unzipped in place: `assets/audio/kenney_impact/`, `assets/audio/kenney_rpg/`,
  `assets/audio/qubodup-GhostMoans/`, `assets/fonts/Pirata_One/`, `assets/fonts/Crimson_Text/`.
  The `.zip` files are git-ignored; the extracted files are committed.
- The scary ghost sound is `scaryhighpitchedghost.wav` (not `.ogg`); raylib loads both.

## Git
- The original 2D game was already committed (`680493d`); it is tagged `legacy-2d` and its
  source is copied into `legacy_2d/` (not compiled).
- The old `build.bat` referenced files that no longer exist; it lives in `legacy_2d/`.
- `build/`, `*.exe`, `save.txt` and `shots/*.png` are git-ignored.

## Phase 1 decisions
- Extra modules beyond the CLAUDE.md layout: `screen.c` (virtual screen, see above) and
  `meshgen.c` (CPU mesh builder shared by world chunks, the door and the character cube).
- The wing table values live in `config.h` as the `WING_TABLE` macro; `game.c` creates the
  array from it (a `static` array in a header would trigger unused-variable warnings).
- Fonts are found by file name anywhere under `assets/fonts/` (`LoadDirectoryFilesEx`, recursive).
- Characters use one cube mesh built with the atlas's plain white tile, tinted per part, so
  everything (world + characters) shares a single texture.
- The exit door is drawn as a separate slab (4 stacked iron blocks) per `E` cell, not part of
  the chunk mesh; it will slide down into the floor when the wing is complete (`doorSlide`).
- Torch flames are plain `DrawCube`s for now (phase 2 makes them part of the lighting).
- `--autotest` skips wings whose file doesn't exist yet (wings 2-5 arrive in phase 5) and
  also saves `shots/knight.png` (knight walking + mid-swing close-up).
- Arrow keys also turn the camera (useful on a laptop touchpad). Esc frees the mouse,
  clicking the window captures it again (Esc becomes Pause in phase 3).
- Wing maps are hand-designed; I laid them out with a throwaway Python script (rectangles),
  but the `.txt` files are the source of truth and can be edited by hand.

## Phase 2 decisions
- Vertex color = baked torch light (rgb) + face shade (alpha); the shader adds ambient and
  the player light, multiplies by shade, then fogs. If the shader files fail to load, meshes
  are baked with a plain look for raylib's default shader instead (`MB_SetShaderEncoding`).
- Torch light uses a cubic falloff (`TORCH_INTENSITY` 1.7) so torches make distinct pools.
  It is blocked by walls (grid ray march), so light doesn't leak into neighbouring rooms.
- Characters and doors have no baked light; the shader uses `entityLight` = the torch light
  sampled at their position (`isEntity` uniform).
- Torch flames are drawn unlit with raylib's default shader, so they glow through the fog.

## Phase 3 + 4 decisions (built together; they share the same files)
- `input.c`: the game reads one `Input` struct per frame instead of the keyboard directly, so
  `--autotest` can play wing 1 with scripted input: it opens all 3 chests, checks the door
  opens and you can walk through it, kills a skeleton with two aim-assisted swings, dies to a
  telegraphed attack and checks you rise again at the chest checkpoint.
- Enemies never touch the player directly; they report hits/scares/bolts in `EnemyEvents`
  and `game.c` applies damage, sound and camera shake.
- A sword hit staggers skeletons/ghosts/witches (cancels their wind-up). The Queen doesn't flinch.
- Ghosts in the dark take no damage; the first time your blade passes through one, a banner
  explains "Ghosts can only be hurt in the light".
- Chests face the open floor; the chest prompt accepts facing with either the body or the camera.
- Every wing starts with full hearts. Restart wing (pause menu) reloads the wing from scratch.
- Hex bolts are drawn as glowing cubes; arrow keys also turn the camera.
- The virtual-screen texture is copied to the window with blending off (translucent UI would
  otherwise leave the picture see-through), and screenshots drop the alpha channel.
- Extra screenshots: `shots/enemies.png`, `hud.png`, `chest_open.png`, `death.png`, `victory.png`.
- Audio roles -> files (all optional): swing `drawKnife1-3`, hit `impactPlate_medium_*`,
  hurt `impactPunch_heavy_*`, chest `creak1-3`, treasure `handleCoins*`, door `doorOpen_*`,
  footsteps `footstep00-09`, scare `scaryhighpitchedghost.wav`, moan `qubodup-GhostMoan01-05.wav`,
  bolt `knifeSlice*` (pitched up), bolt shatter `impactGlass_light_*`, enemy death
  `impactWood_heavy_*`, dash `cloth1-3`, menu `metalClick`, ambience `dungeon_ambient_1.ogg`.

## Phase 5 decisions
- Wings 2-5 were laid out with a throwaway Python script (rooms carved from stone, walls take
  each room's material, torches spaced further apart in later wings: every 3-4 cells in wing 2,
  every 8 in wing 5). The `.txt` files are the source of truth and can be edited by hand.
  - Wing 2 Ballroom: foyer -> big pillared ballroom (wood floor, carpet cross) -> parlor,
    dining hall, gallery with the exit. Wood-panel walls.
  - Wing 3 Library: bookshelf walls, bookshelf islands in the stacks and archive (cover from bolts).
  - Wing 4 Bone Chapel: nave with pillars, ossuary, crypt, apse, charnel house; bone piles.
  - Wing 5 Throne Room: one huge obsidian-walled hall with pillars, four side chambers, the
    Queen waits on the carpet near the gate.
- The Queen gets a faint purple self-glow so she is readable in the near-black throne room.
- `--autotest` also fights the Queen (summon at half health, gate shut until she dies, escape
  -> victory) and saves `shots/boss.png`. It prints ms/frame per wing (about 16.7 = 60 fps,
  vsync-limited, on this Intel HD 510).
- Save: `save.txt` holds the highest wing reached (1-5). The menu shows "Continue (Wing N)".
  New Game always starts at wing 1 (the save keeps the highest wing reached).

## Phase 6 decisions
- Menu background: wing 1 rendered in 3D with the camera slowly orbiting inside the entrance hall.
- Controls hint: bottom of the menu, and during the first 20 s of wing 1.
- Balance: `--autotest` runs a "button masher" check (only left click, two wing-1 skeletons
  at once); it wins with 4 of 5 hearts left. Hits stagger normal enemies, every attack is
  telegraphed for 0.5 s, chests heal, and every wing starts with full hearts.
- `make release` needed `"$(MAKE)"` quoted (make itself lives in a path with a space).
- `build.bat` was rewritten for this project (collects `src\*.c`, paths at the top).
- The save is written only when a run starts or a new wing is reached (not by the menu's
  background). Starting with `--wing N` also counts as reaching wing N.
- Not done / ideas: no mouse-sensitivity option in a menu (edit `MOUSE_SENSITIVITY` in
  config.h), no per-wing music, ghosts only drawn faintly (not hidden) in the dark.

# The Ashen Monastery upgrade (UPGRADE.md)
- The spec named `UPGRADE_ASHEN.md`; the file in the repo is `UPGRADE.md` (same content).
- The working Blackthorn Manor is tagged `submittable`.

## Task 1 - PS1 look
- `post.c`: the 3D scene renders into a 640x360 render texture (point filtering), then is drawn
  up-scaled through `assets/shaders/post.fs` (red/black grade, crushed blacks, chromatic offset,
  grain, vignette, red pulse, 5-bit colour + 4x4 ordered dither). UI is drawn after, crisp.
- Because raylib texture modes can't nest, `Game_Draw` now does the whole frame itself:
  low-res scene -> `Screen_Begin` -> post -> UI -> `Screen_End`.
- PS1 vertex wobble in `world.vs` (snap to a 320x180 grid) is on; it looked fine in the shots.
- F2 now toggles the post-process; the FPS counter moved to F6.
- Pale tones (bone, wraiths) keep some pallor in the grade so they read against the red.

## Task 2 - Red Lightning wand
- The sword is gone: Kael holds a dark-wood wand with a glowing red tip. Left click casts
  (cooldown 0.45 s, range 12, 1 damage). Aim assist picks the nearest enemy that can be hurt
  AND is in line of sight; otherwise the bolt follows the camera/crosshair.
- The bolt is a ray marched from the wand tip in 0.1 steps: first wall or enemy stops it,
  fireballs it passes through are destroyed. A Choir Wraith in the dark lets it pass through.
- Visual: 10 jagged segments re-randomised every frame + 3 forks, additive red glow + white core
  (`DrawCylinderEx`). A second point light (`flashPos/Color/Radius` in world.fs) flashes red.
- The crack sound is synthesised in `audio.c` (`MakeCrack`: noise burst + falling whine + thump).
- `shots/lightning.png` shows a cast. The masher balance test now ends with 5/5 hearts.

## Task 3 - Real textures
- Downloaded six 1k PNG diffuse maps from the Poly Haven API into `assets/textures/` (CC0, see
  CREDITS.md). Each is optional: missing/unloadable -> the generated tile stays.
- Atlas: generated tiles are still painted at 16 px, the atlas is nearest-scaled to 64 px slots
  (`ATLAS_TILE`), then photos are pasted in. UVs use `ATLAS_TILE`, half-texel inset kept.
- Photos are shrunk with bicubic `ImageResize` (nearest from 1024 -> 64 px turns into noise);
  the GPU still samples with point filtering, so the result keeps the PS1 look. Darkened by 35.
- Carpet is now deep crimson; bookshelf, carpet, bone, chest, plank floor stay generated.

## Task 4 - Scarier enemies + burning atmosphere
- Enemies renamed in code and on screen: Skeleton -> Ashen Monk (`EN_MONK`), Ghost -> Choir
  Wraith (`EN_WRAITH`), Witch -> Ember Priest (`EN_PRIEST`), Witch Queen -> The Red Abbot
  (`EN_ABBOT`); config prefixes too (`MONK_`, `WRAITH_`, `PRIEST_`, `ABBOT_`). AI and stats unchanged.
- New box models: charred hooded monks with ember-crack cloth (new generated atlas tile
  `TILE_EMBER`), hooded singing wraiths, crimson priests with tall hoods and a burning censer,
  the Abbot with a bell-shaped iron mitre and two orbiting fireballs. Priest/Abbot "bolts" are
  now orange fireballs. All enemies are scaled 15% taller and 15% thinner.
- Eyes are emissive mode 2 (unlit AND no fog) so they are the first thing seen in the dark.
- Enemies get a minimum light (`ENEMY_MIN_LIGHT` 0.55) so their silhouettes stay readable for a
  beginner; the first darker version made monks invisible except for their eyes.
- Twitching heads (random 0.6-2.5 s, 0.08-0.15 s snaps); Ashen Monks lurch (stutter-step speed).
- `atmos.c`: 300 ash flakes wrap around the camera; 60 additive embers rise from nearby torches.
- Bell tolls every 25-40 s: synthesised bell (`MakeBell`: 110/220/277/330/440 Hz partials, 4 s),
  red screen pulse, torches flare. A shorter cracked bell (`SND_BELL_BREAK`) is ready for Task 5.

## Task 5 - Story texts
- Wing files keep their layouts; only the first line changed (The Ash Gate ... The Bell Tower).
- Ward Seal names replace the treasures (same counts). Banner "Ward Seal found: ...", HUD
  "Ward Seals 1/3", chest prompt "[E] Hold to open reliquary".
- Completing a wing: "THE FIRST BELL SHATTERS" banner, full red pulse, max camera shake and the
  generated cracked-bell sound. Wing banners read "THE FIRST BELL - The Ash Gate".
- New Game shows `STATE_INTRO` (4 lines on black, Enter/click/Esc skips), then wing 1.
  Continue skips the intro.
- Death screen: "THE FIRE TAKES YOU" / "[ ENTER ] Rise from the ashes". Menu is red and black.

## Task 6 - The Sanctum ending
- `assets/wings/sanctum.txt`: 9x22 candle-lit hall, pillars on both sides, crimson carpet, `O` =
  Master Oren, `a` = apprentices/monks (the first three `a` in reading order become Ilsa, Tobin,
  Mira; the rest are freed monks facing the carpet). `World_Load(..., needExit)`: the Sanctum is
  loaded with 0 chests and no exit required.
- After the fifth bell's door: `Game_LoadSanctum` instead of the old victory screen. Warm mode:
  amber fog + golden ambient (`Render_SetWarm`), post grade mode 1 (gold), no ash, no tolls,
  slower/quieter ambience (`Audio_SetCalm`), no wand.
- Friends speak in a text box within 2 units; at Oren "[E] Speak with Master Oren" opens his
  4-line dialogue (side camera framing both), then fade to black -> "THE BELLS ARE SILENT" with
  stats and a short credits list ("Created by: ___" for the student to fill in).
- Autotest: `SanctumTest` (friend line, Oren prompt, dialogue, final screen) + `shots/sanctum.png`,
  `dialogue.png`, `victory.png`.

## Task 7 - Wrap up
- Executable is now `AshenMonastery.exe` (Makefile, `.vscode/launch.json`, `build.bat`).
- README, CREDITS and the top of CLAUDE.md describe The Ashen Monastery.
- Nothing from the cut list had to be cut: vertex wobble, embers, friend lines, real textures and
  bell tolls are all in.
- Kael still uses the knight's body (helmet + tabard) with the wand; the spec only asked to swap
  the sword for the wand.

# Master spec progress

## Item 1 - Story texts
- Intro lines 2 and 4 now mention the relics and the caged serpent; chest prompt "[E] Hold to open
  chest"; pause "Quit to title"; autotest labels use the Ashen names.

## Item 3 - Moonlit gothic grade
- `post.fs` grade mode 0 is now a split-tone: cool blue-indigo shadows, warm amber highlights,
  saturation x0.82, a gentle S-curve; vignette fades toward deep indigo instead of black.
  Grain 0.04. Mode 1 (Sanctum) stays warm gold.
- Fog/background are deep indigo (0.025, 0.03, 0.06), ambient is cold moonlight blue, torches are
  amber (1.0, 0.68, 0.35). Red is kept for danger/magic: bell tolls no longer pulse the screen red
  (only the lights flare); a bell shattering still flashes red.
- Falling ash only in wing 5 (the Bell Tower); embers everywhere.

## Item 4 - `--tour`
- `--tour` saves `shots/tour_wN_1..4.png` for wings 1-5 and the Sanctum (N = 6): start view
  without HUD, the largest room facing its longest wall, the longest straight corridor, and the
  start view with the HUD. The first frame of each shot is not timed; average FPS is printed.
- `world.c` now classifies cells: a floor cell inside any fully open 4x4 square is a room cell,
  every other open cell is a corridor cell; connected room cells form `Room`s (bounding box +
  cell count). The architecture will use this for wall heights.
- New flags: `--sanctum` (or `--wing 6`) starts in the Sanctum.

## Item 5 - Materials with world-space UVs
- `textures.c` now has world materials (`MaterialId`): 8 photo materials from `assets/textures/`
  (Poly Haven CC0, see CREDITS.md) and 10 generated ones (carpet, gold, stained glass, banner with
  an original key + crescent emblem, book spines, bone, cobweb, flame, painting, crimson cloth).
  Every texture is resized to 128x128, point filtered, repeat wrap, no mipmaps.
- Photo textures are stored pre-shrunk to 256x256 PNG (the 1k originals were 5-6 MB each);
  at load they are resized to 128, desaturated and tinted per material (stone a little
  green-grey). A missing file prints download instructions and uses a generated texture.
- `meshgen.c`: `MB_Quad` (free quad) + `MB_WorldUV` (box-projected world-space UVs, one repeat
  per 2 units). `architecture.c` (new) builds one mesh per (16x16 chunk, material); faces are
  split into ~1x1 quads. The old atlas stays only for characters, chests and the door.
- Cobwebs (alpha) are drawn in a transparent pass without depth writes.

## Item 6 - Architecture from the grid
- `architecture.c` builds everything from the grid: rooms 6.0 tall, corridors 3.5; lintel walls
  where corridors open into rooms; pointed two-centred arches (radius 0.75 x span, 7 segments per
  side, spring >= 2.2, stone infill both sides, trim band + jambs on the room side; openings wider
  than 4 get a straight lintel); plinth (0.4) + cornice (0.3) on every wall; pilasters every 4
  cells on long room walls (vaulted rooms: where the bays meet); 'P' = 8-sided columns with base
  and capital; trim posts on outer wall corners; flat rooms get beams every 2 units, rooms of 8x8+
  get groin vaults (max of two pointed barrel profiles per ~4x4 bay, springing at 4.3) with
  diagonal + transverse ribs; corridors get a beam every 3 units; carpets have a gold edge.
- Room detection ignores "islands" (small wall groups not touching the outer walls, e.g. the
  Scriptorium's bookcases), so rooms extend around them; 'B' islands render as free-standing
  2.6-tall bookcases with a top board.
- Exit doorways are short corridor passages; their opening gets a pointed arch and two door
  leaves (wood + iron straps + ring handles) that swing inward when the wing's exit opens
  (`doorSlide` now means swing amount). `World.doorways[]` records where they are.
- Reliquary chests are rebuilt low-poly: wooden body on iron feet, iron corner bands, lock plate,
  curved 6-segment lid with iron bands, hinged at the back.
- Camera: stays 0.45 below the local ceiling (`World_CeilingAt`); the lightning ray stops there too.
- Dev flag `--bright` (flat bright ambient) to inspect geometry in screenshots; `shots/door.png`.

## Item 7 - Props and room themes
- `props.c` (new) + `geo.h` (the builder API shared with architecture.c). `Props_Place` runs
  before the meshes are baked: it chooses props deterministically per wing (`World.theme`),
  registers flames (`World.flames`), light sources (`World.torches`, now with colour + radius)
  and collision boxes (`World.colliders`, used by `World_BoxBlocked`, the camera and enemies).
- Rules: wall props stand in "slots" (room cell + plain wall side); centre props need free cells.
  Kept free: the start, chests (+1 ring), spawns, NPCs, exits (+2), carpets, bone piles and a
  2-cell path in front of every opening. Each solid prop re-runs the flood fill from the start;
  if any cell, chest or exit would become unreachable, the prop is removed again.
- Everywhere: candelabras (3 candles, light), banners with the key + crescent emblem, framed
  paintings, rubble, cobwebs in upper room corners. Corridors: hanging iron lanterns about every
  4 cells (light), wall lanterns, wooden benches.
- Wing themes: 1 suits of armor on plinths, stone benches, a portcullis behind the start;
  2 dining tables with benches and candles + ring chandeliers in the great hall, dormitories with
  four-poster beds (crimson curtains), trunks, a stone fireplace with fire (light) and a rug;
  3 bookcases, reading tables with candles, the tower study (desk, brass globe, spiral stair in a
  corner, gallery ledge with railing high on the walls); 4 alchemy rooms (jar shelves with glowing
  glass, cauldrons with a green glow light, workbenches, stools) alternating with crypts (stone
  coffins, skull niches, floor candles) and one tall standing mirror (gold frame, pointed top,
  dark glass with a sheen); 5 the great bell hanging high in the biggest room, ropes, broken pews,
  rubble; Sanctum: chandeliers, candelabras, candles.
- Two more generated materials: `MAT_POTION` (emissive green) and `MAT_MIRROR`.

## Item 8 - Windows, moonlight, weather
- Windows (`FindWindows` in architecture.c, before props): on outside walls (only solid cells
  between the wall and the map border) of rooms and corridors, about every 5 cells, never at the
  end of a wall run. Each is a pointed arch with two pointed lancets of generated stained glass
  (emissive), a mullion, a stone sill, trim jambs + arch band and a small diamond light in the
  tracery; each adds a cold moonlight source (0.55, 0.65, 0.9). Props and pilasters avoid them.
  The Scriptorium has few windows because its walls are bookshelves.
- Moonlight shafts (`Render_DrawWindowShafts`): an additive pale-blue prism from each window to
  the floor (falls 0.7 sideways per unit of drop), a soft patch on the floor and 6 drifting dust
  motes per window. Transparent pass, both sides, no depth writes.
- Lightning: every 20-45 s (wing 5: x0.45), two flashes within 0.4 s (`LightningFlash`): ambient
  jumps, the glass blazes (`emissiveBoost` uniform), the shafts flare white; thunder 1.3 s later
  (`SND_THUNDER`, synthesised: filtered noise crack + rolling rumble). `LIGHTNING_ENABLED` in config.
- Bell tolls get rarer as bells break (interval x 5 / bells left) and stop after the fifth.
- Embers now rise only from real fires (sconce torches, fireplaces), not candles or windows.
- New screenshots: `shots/window.png` (lightning) and `shots/window_dark.png`.

## Item 9 - Per-pixel lighting, palette, per-wing mood
- `world.vs/fs` rewritten: per-pixel Lambert (slightly wrapped) from up to 16 world lights
  (`lightsPos/Color/Radius[]`, falloff (1 - (d/r)^2)^2), the player's soft light (between Kael and
  the camera, a little above), the red lightning flash, a cold ambient with a small sky/ground
  difference, exp^2 fog in the wing's colour; floors get a faint Blinn highlight (polished
  flagstones). Texture coordinates are `noperspective` (PS1 affine mapping); vertex snapping stays.
- Light selection (render.c): every frame the 16 nearest lights within 22 units that the camera
  or Kael can see (grid line of sight; anything within 3 units counts) go into shader slots; each
  slot fades its light in/out over 1/4 s so lights never pop (teleports snap). Candles, torches and
  fires flicker individually and flare when a bell tolls; moonlit windows are steady and flood
  white-blue during lightning. Up to 4 moving lights per frame (`Render_AddDynamicLight`): the Red
  Abbot carries a red glow.
- Nothing is baked into vertices any more (`BAKE_LIGHT 0` in architecture.c, kept for a
  shader-less fallback). Characters are lit per pixel too, plus a little extra ambient.
- Flames are additive camera-facing sprites (generated flame texture) with a soft halo.
- Per-wing mood in `WING_TABLE` (fog colour, ambient tint, candle brightness): 1 cold moonlit
  blue with many candles; 2 warm gold; 3 amber with green-tinted shadows; 4 sickly green-cyan,
  dense fog, dimmer candles; 5 stormy blue with frequent lightning. The Sanctum is warm gold.
  The background clear colour is the wing's fog colour.
- Performance: `--tour` now runs without vsync and measures ~130 FPS average on this Intel HD 510
  (16 lights, 640x360), so MAX_LIGHTS stays 16.

## Item 10 - Minimap and full map
- `minimap.c` (new): at load the wing is painted into a texture (8 px per cell: dark stone walls,
  lighter wall edges, grey-blue room floors, slightly darker corridors, dark red carpets, brown
  floorboards). The corner minimap (230x170, rounded dark panel, thin old-gold border, ~85%
  opacity, north up, ~24 cells across) shows a window of it around Kael, with the wing name and
  "Ward Seals x/y - M: map" below.
- Exploration: every 0.15 s, open cells within 7 units that Kael can see (grid line of sight)
  become discovered, and walls next to discovered floor. Undiscovered cells are drawn dark.
  `Game.discovered` survives death/respawn and is cleared when a wing is (re)loaded.
- Always shown: unopened chests (gold, even undiscovered), opened chests (grey check), the exit
  (red locked / green open), Kael (white arrow, facing), the Red Abbot as a pulsing red dot once
  alerted, and the serpent cage (fields ready for item 13).
- M opens `STATE_MAP`: the whole wing scaled to fit, the same icons, a legend; M/Esc/Enter close.
  Controls hint lists "M map". `shots/map.png` in the autotest.
- Lighting balance: candle/torch/fire light x0.72 (`FLAME_LIGHT_SCALE`), dimmer fireplace and wing 2
  candles, less extra light on characters (the dormitory was over-exposed).

## Item 11 - Enemy restyle and model slots
- `character.c` rewritten around unit low-poly primitives built as lathes (no more boxes for
  bodies): flared robe, cylinder, cone, sphere (+ ember-crack versions). Robes flare to the floor,
  heads are spheres, limbs are cylinders/sleeves, hoods are rounded cowls with a shadowed face.
  - Kael is now a young apprentice mage: dark indigo robe, crimson stole, belt, hood down on the
    shoulders, dark hair; the wand is unchanged (dark wood, gold band, glowing red tip).
  - Ashen Monk: charred ember-crack robe and sleeves, peaked hood, claw hands. Choir Wraith: robe
    tapering into a downward wisp, open singing mouth. Ember Priest: crimson robe, tall cone hood,
    censer on a chain. Red Abbot: 1.8x, crimson/black, iron bell mitre with lip and knob, two
    orbiting fireballs. Sanctum NPCs: robes, hair, Oren's long cone beard and glowing staff.
  - Red emissive eyes (unlit, no fog), 15% taller/thinner enemies, twitching heads: kept.
- Model slots: `Model_Slot(name, height, yawOffset)` loads `assets/models/<name>.glb` once
  (LoadModel + LoadModelAnimations), scales it to the role height by its bounding box, feet on the
  floor, assigns the world shader; animations are matched by keyword (idle, walk/run,
  attack/cast, hit, death; missing -> idle). Every code-built character checks its slot first.
  `assets/models/README.txt` lists the file names (relic slots are used by item 13).
- HUD: the minimap hides during Master Oren's dialogue; the controls hint and friends' text box
  moved left so they never cover the minimap.

## Item 12 - Title screen and menus
- `title.c` (new) builds the title scene once with the same geometry builder as the wings:
  a jagged cliff (lathe rings with noise; snow on the gentler upper slopes, dark slate crags,
  two-sided faces), and an original castle-monastery on top: eight round towers (8-12 sides, pale
  stone, corbelled rings, dark conical spires with iron needle tips) of different heights, curtain
  walls with merlons, a long hall with a steep gable roof, buttresses and tall pointed windows,
  and a stained-glass end window. Many lit windows: warm steady, warm flickering, a few cold blue.
- Sky: gradient, a generated pale moon, 14 drifting cloud billboards (Perlin noise texture);
  falling snow in a box in front of the camera; mist banks at the cliff's foot; 7 crows (two
  flapping wing triangles each) circling the spires; lightning every 6-12 s (two flashes, a bolt,
  a big white-blue light, thunder 1 s later); a warm orange and a cold teal light sweep around the
  castle in opposite directions; a soft moonlight from above. The camera drifts in a 70 s loop
  (low and looking up -> higher, orbiting a few degrees, slight sway). Same 640x360 PS1 pipeline.
- Flow (`Game.menuPhase`): black -> the scene fades in with a distant bell -> "THE ASHEN
  MONASTERY" letter by letter with a soft glow (`UI_TextReveal`) -> subtitle -> "Press any key"
  pulses. A key slides the menu in from the left over a dark gradient: New Game, Continue (if
  saved), Controls, Credits, Quit. Hover: bigger, candle gold, flickering flame icon, soft tick.
  Choosing: synthesised whoosh + lightning flash + fade to black, then the intro / the saved wing.
  Controls: two columns of key caps. Credits: slow scroll ("Created by: ___" + all assets).
- Sounds: synthesised wind loop (title only) and whoosh (`MakeNoise`); `Audio_Loop/Audio_Stop`.
- Autotest: `shots/title.png`, `menu.png`, `controls.png`, `credits.png`; checks a key opens the menu.

## Item 13 - Soul Relics, the caged serpent, the immortal Abbot
- `relics.c` (new). The last chest opened in a wing releases that wing's Soul Relic instead of a
  seal banner (its seal still counts): it rises to chest height, floats and turns, glows red
  (moving light), whispers (`SND_DRONE`, synthesised) and the nearby candles dim. Banner "SOUL
  RELIC FOUND: <name>" / "Strike it with your wand!". Aim assist always prefers a floating relic.
  3 hits: each shakes it, adds glowing red cracks and makes it scream; the third shatters it
  (red-white burst, shards, strong shake, cracked bell, red flash, "THE FIRST BELL SHATTERS").
  Wings 1-4: the exit opens. Relics are code-built (Grimoire: black leather, iron caps, clasp,
  red seal; Ring: dark gold band + faceted red stone that pulses; Locket: oval silver, crescent,
  blue glowing seam, chain; Chalice: hexagonal foot, glowing embers; Crown: black iron thorns +
  red gem) or loaded from `assets/models/relic_*.glb`. Each gives off wisps/embers/sparks.
- Bell Tower: `props.c` places a 3x3 cage spot in the bell tower room as far as possible from the
  Abbot (the great bell moves aside if needed). A stone dais with glowing red runes, 8 iron bars,
  a domed top and chains to the ceiling; the bars are solid (a collider) until broken. Sealed
  while any relic remains (sparks + a hint); then 6 hits bend/clang the bars, the 6th bursts them
  outward. The Ember Serpent (26 rope-following segments, ember-scale texture, red eyes, flicking
  tongue) coils inside, raising its head and hissing (`SND_HISS`) when Kael comes near.
- Serpent fight: HP 8, circles Kael at ~6.5 units in curves; every 2.5 s it rears up glowing red
  for 0.6 s (the telegraph) and then lunges 4 units forward (1 heart if within 1.1). On death it
  bursts into embers -> "THE RED ABBOT IS MORTAL", his shield breaks and he attacks.
- The Abbot is `immortal` until then: he waits (does not notice the player), a translucent red
  shield shell surrounds him, spells spark off it (with a hint), and the boss bar reads IMMORTAL
  when Kael is within 18 units. The wing 5 exit opens only after every chest, the relic, the
  serpent and the Abbot. Respawn: a living freed serpent returns to the dais at full HP; a living
  Abbot returns to full HP.
- HUD: a row of 5 relic silhouettes + 1 serpent under the hearts (dark; destroyed = cracked with a
  gold outline). Inventory lists each wing's relic; the final screen counts relics destroyed.
- Particles have their own gravity now (smoke and embers rise).
- Autotest: wing 1 relic before the exit; the whole Bell Tower sequence (sealed cage ignores hits
  -> crown -> cage -> serpent -> mortal Abbot -> summon -> exit -> Sanctum); `shots/relics.png`
  and `shots/cage.png` from a free camera.

## Item 14 - The Sanctum ending
- Already in place from the Ashen upgrade and now on the new systems: the hall is built by the
  architecture (pillars as columns, ribbed vault, carpet with gold edges) with the Sanctum theme
  (three ring chandeliers, candelabras, floor candles, banners), warm gold mood (fog, ambient,
  grade mode 1), no ash, tolls or lightning, calm ambience. Master Oren (grey robe, cone beard,
  glowing staff), Ilsa / Tobin / Mira (blue / brown / green) and six freed monks in light grey
  (a little darker now, they glowed too white). Friends' lines within 2 units, Oren's 4-line
  dialogue, fade to "THE BELLS ARE SILENT" with seals, relics, enemies, time and short credits;
  Enter returns to the title.

## Item 15 - Wrap up
- `make release` builds without warnings and creates `dist/AshenMonastery_Windows.zip` with the
  executable, complete `assets/` folder, README, credits and quick-start instructions. The
  tag-triggered GitHub Actions workflow builds and attaches that same ZIP to each `v*` release;
  debug and release both pass `--autotest` (35 checks). README rewritten (story, how to play,
  controls, debug keys, wings, build steps, flags, file overview); CREDITS lists every Poly Haven
  asset by name, the fonts, the Kenney / OpenGameArt sounds and everything made in code.
- The old `final` tag (Ashen Monastery before this spec) was moved to this version; the earlier
  milestones stay reachable through `submittable`, `environment-done`, `title-done`, `relics-done`.
