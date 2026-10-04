# Blackthorn Manor

A blocky, third-person gothic horror game in **C99 + raylib 6.0**.

> You stand at the iron gate of Blackthorn Manor. Its treasures are scattered through five
> cursed wings, guarded by the dead and the witches who serve the Witch Queen.
> Collect every treasure, defeat the Queen, and escape.

Every room holds one treasure chest. Open every chest in a wing (hold **E**) and the iron
exit door sinks into the floor; walk through it to reach the next wing. In the last wing the
manor gate only opens once every chest is open **and** the Witch Queen is destroyed.

The game is forgiving: you have 5 hearts and every hit costs one, opening a chest heals a heart
and saves a checkpoint, and if you fall you rise again at your last chest with your treasures
kept. Enemies always glow red before they strike, and swinging your sword automatically turns
you toward the nearest enemy.

## Controls

| Key | Action |
|-----|--------|
| W A S D | move (relative to the camera) |
| Mouse (or arrow keys) | look around |
| Left click | sword swing (aims itself at the nearest enemy) |
| Shift | dash: a quick dodge, you can't be hurt while dashing |
| E (hold) | open a chest |
| I or Tab | inventory: every treasure, hearts, enemies slain, time |
| Esc or P | pause (Resume / Restart wing / Quit to menu) |
| Enter | confirm in menus, rise again after falling |

**Tips:** ghosts can only be hurt when they are in the light (near you or near a torch).
Witches keep their distance and throw purple hex bolts; hide behind pillars and bookshelves,
or cut the bolts out of the air with your sword.

### Debug keys

| Key | Action |
|-----|--------|
| F1 | debug overlay (collision boxes, enemy sight circles and view cones, chest reach) |
| F2 | show FPS |
| F3 | god mode (can't be hurt) |
| F4 | complete the current wing instantly |

## The five wings

1. **The Gatehouse**: 3 chests, a short tutorial-like start with many torches.
2. **The Grand Ballroom**: 4 chests, a pillared ballroom with red carpet.
3. **The Moonlit Library**: 4 chests, bookshelf mazes and witches.
4. **The Bone Chapel**: 5 chests, bone piles and very little light.
5. **The Witch Queen's Throne Room**: 5 chests and the Witch Queen herself, almost pitch black.

## Building (Windows)

You need **w64devkit** (gcc + make) and **raylib 6.0** for MinGW-w64.

```
make                 # debug build -> BlackthornManor.exe
make run             # build and run
make autotest        # build and run the self-test (screenshots in shots/)
make release         # optimized build without a console window
make clean
```

The Makefile expects raylib at `C:/Users/HP GM/Downloads/raylib-6.0_win64_mingw-w64`. If yours
is elsewhere: `make RAYLIB_PATH="D:/path/to/raylib-6.0_win64_mingw-w64"`.
No make? Run `build.bat` (edit the two paths at its top). In VS Code, **Ctrl+Shift+B** builds
and **F5** starts the debugger.

Command-line options:

- `BlackthornManor.exe --wing 3` starts directly in wing 3, skipping the menu.
- `BlackthornManor.exe --autotest` loads and validates every wing, renders each one, plays wing 1
  with scripted input (chests, door, sword, death and checkpoint), fights the Witch Queen, saves
  screenshots to `shots/`, prints a summary and exits with 0 (or 1 if anything failed).

Progress is saved in `save.txt` (the highest wing reached); the menu then offers **Continue**.

## Files

```
src/main.c        window, main loop, command-line flags (--wing N, --autotest)
src/config.h      every tunable number + the per-wing difficulty table + treasure names
src/game.c/.h     state machine, wing flow, chests + treasures, combat, checkpoints, saving, autotest
src/world.c/.h    loads a wing text file, validates it, builds chunked block meshes with baked
                  torch light, collision and line of sight
src/textures.c/.h generates the 16x16 block texture atlas in code
src/meshgen.c/.h  helper that builds block meshes (faces, boxes) on the CPU
src/character.c/.h blocky knight, skeleton, ghost, witch and Witch Queen models + animations
src/player.c/.h   movement, dash, sword timing, hearts, third-person camera with wall collision
src/enemy.c/.h    enemy AI (sight, hearing, wind-ups), hex bolts, separation
src/render.c/.h   fog/light shader, chests, doors, torch flames
src/audio.c/.h    sound effects and the ambience loop (every file optional)
src/ui.c/.h       fonts, menu, HUD, banners, pause, inventory, death and victory screens
src/input.c/.h    keyboard + mouse -> one Input struct per frame (the autotest fakes it)
src/screen.c/.h   fixed 1280x720 virtual screen, scaled to fit any window/monitor
assets/wings/     wing1.txt ... wing5.txt (edit them: the format is in CLAUDE.md section 5)
assets/shaders/   world.vs / world.fs (fog, ambient, flickering torchlight, player light)
assets/fonts/     Pirata One, Crimson Text
assets/audio/     Kenney sound packs, OpenGameArt ghost sounds and ambience
legacy_2d/        the original 2D version of this project (not compiled)
```

Wing maps are plain text: `#` stone wall, `W` wood wall, `B` bookshelf, `P` obsidian pillar,
`T` wall with torches, `.` stone floor, `=` red carpet, `,` wood floor, `x` bone pile,
`@` player start, `C` chest, `E` exit door, `s g w Q` skeleton, ghost, witch, Witch Queen.
The game checks every map when it loads and prints clear errors (file:line:column).

All textures and 3D models are generated in code. See `CREDITS.md` for fonts and sounds, and
`NOTES.md` for design decisions.
