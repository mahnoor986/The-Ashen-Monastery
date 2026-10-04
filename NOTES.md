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
