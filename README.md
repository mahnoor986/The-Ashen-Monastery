# Gothic Dungeon

Top-down gothic dungeon crawler in C (C99) + raylib. Placeholder shapes, real-time melee combat,
torch lighting, 3 hand-made levels, 4 enemy types and a boss.

## Setup (Windows)
1. Install raylib with its Windows installer (gives you `C:\raylib\raylib` and `C:\raylib\w64devkit`),
   or use your own w64devkit + raylib.
2. Open this folder in VS Code (C/C++ extension installed).
3. If your paths differ from `C:\w64devkit` / `C:\raylib\raylib`, edit them in:
   `.vscode/tasks.json`, `.vscode/launch.json`, `.vscode/c_cpp_properties.json`
   (or run `make RAYLIB_PATH=D:/your/raylib`).
4. **Ctrl+Shift+B** builds -> `game.exe`. **F5** builds + debugs with gdb.
   Terminal: `make run`.  No make? run `build.bat`.

## Controls
| Key | Action |
|-----|--------|
| WASD / Arrows | move |
| Space / J / Left click | sword swing (arc in front of you) |
| Left Shift | dash (brief invulnerability) |
| I / Tab | inventory screen |
| Esc / P | pause |
| F1 | debug view (hitboxes, sight cones, tile grid) |
| F2 | toggle lighting |

Keys (gold) open locked doors by touching them. Stairs (`>`) go deeper. Kill the boss in level 3 to win.

## Layout
```
src/config.h    every tunable (speeds, HP, ranges, palette, lighting)
src/levels.h    hand-made level maps as text (edit them!)
src/draw.c      DrawEntity(type, pos, facing) - ALL placeholder art lives here
src/map.c       tiles, collision, line of sight, tile rendering
src/player.c    movement, dash, attack
src/enemy.c     4 enemy AIs + boss + projectiles
src/lighting.c  darkness + flickering torch light (render texture)
src/ui.c        menu, HUD, pause, inventory, game over, victory
src/game.c      state machine, level flow, combat resolution, camera
```

## Swapping in real art later
Only `draw.c` (`DrawEntityFx`, `Map_Draw` tile section in `map.c`) needs to change - load a texture
and `DrawTexturePro` instead of shapes. Gameplay code never draws shapes directly.
Drop gothic fonts at `assets/fonts/title.ttf` and `assets/fonts/body.ttf` and the UI uses them automatically.

## Next steps (suggested)
Tune combat feel in `config.h` -> add audio -> shaders (vignette/grain/chromatic aberration) ->
save/load -> Tiled level editor. Keep `CREDITS.md` updated with every asset.
