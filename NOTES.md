# Blackthorn Manor — Build Notes

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
