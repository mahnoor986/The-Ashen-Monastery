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
