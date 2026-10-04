# The Ashen Monastery — Build Notes

## Master spec status (CLAUDE.md, section 16) — checked 2026-10-04

The combined master spec replaced CLAUDE.md. Old specs moved to `docs/old_specs/`
(`CLAUDE_blackthorn_manor.md`, `UPGRADE_ASHEN.md`; there was no `ENVIRONMENT_PS1.md`).
Mood photos are in `reference/` (used for palette/lighting/room types only; the castle is original).

| # | Item | Status at the start |
|---|------|---------------------|
| 1 | Story, names, texts | mostly done (Ashen names, seals, death screen, Sanctum); intro lines 2/4 and a few prompts differ |
| 2 | Red Lightning wand | done (aim assist, ray march, crackling bolt + forks, red flash light, synth crack) |
| 3 | PS1 pipeline, moonlit grade | partly: 640x360 + post.fs exist, but the grade is the old red/black one |
| 4 | `--tour` screenshots | not started |
| 5 | Materials, world-space UVs | not started (one 4x4 atlas, per-face UVs) |
| 6 | Architecture from the grid | not started (block walls) |
| 7 | Props, room themes | not started |
| 8 | Windows, moonlight, weather | partly (bell tolls, embers, ash exist) |
| 9 | Per-pixel lighting, palette, mood | not started (baked vertex torch light) |
| 10 | Minimap + full map | not started |
| 11 | Enemy restyle, model slots | partly (red eyes, twitching, tall/thin; still boxes) |
| 12 | Title screen + menus | not started (menu over wing 1) |
| 13 | Relics, serpent, immortal boss | not started |
| 14 | Sanctum ending | mostly done (needs the new architecture/props) |
| 15 | Wrap up | — |

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
