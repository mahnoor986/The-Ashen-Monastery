# The Ashen Monastery

*Climb. Break the bells. Bring them home.*

A blocky, third-person PS1-style horror game in **C99 + raylib 6.0**.

> High on a burning mountain stands the Ashen Monastery, a school for young mages. Ten nights ago
> its Abbot made a pact with fire and became **the Red Abbot**. He cast **five cursed bells** from
> the ashes of the dead. Each time they toll, the monastery burns and the monks forget who they were.
> Master **Oren** and the other apprentices are trapped in the **Sanctum** at the very top.
> You are **Kael**, the youngest apprentice, armed with a wand. Climb the monastery and break all five bells.

Each of the five wings holds one cursed bell, bound by **Ward Seals** hidden in reliquaries, one in every
room. Open every reliquary in a wing (hold **E**) and the bell shatters: the iron door to the next wing
sinks into the floor. In the Bell Tower the last bell only breaks once every seal is found **and** the
Red Abbot is destroyed. Then climb to the Sanctum, where your friends are waiting.

The game is forgiving: you have 5 hearts and every hit costs one, opening a reliquary heals a heart and
saves a checkpoint, and if the fire takes you, you rise from the ashes at your last reliquary with your
seals kept. Enemies always glow red before they strike, and Red Lightning aims itself at the nearest
enemy you can see.

## Controls

| Key | Action |
|-----|--------|
| W A S D | move (relative to the camera) |
| Mouse (or arrow keys) | look around |
| **Left click** | **Red Lightning** (aims itself at the nearest visible enemy, range 12) |
| Shift | dash: a quick dodge, nothing can hurt you while dashing |
| E (hold) | open a reliquary / speak (in the Sanctum) |
| I or Tab | Ward Seals of all wings, hearts, enemies defeated, time |
| Esc or P | pause (Resume / Restart wing / Quit to menu) |
| Enter | confirm, skip the intro, advance dialogue, rise again after falling |

**Tips:** Choir Wraiths can only be hurt in the light (near you or near a torch). Ember Priests keep
their distance and throw fireballs: hide behind pillars and bookshelves, or shoot the fireballs down
with your lightning.

### Enemies

| Enemy | |
|-------|--|
| Ashen Monk | charred hooded monk with ember cracks, lurches toward you; 2 hits |
| Choir Wraith | pale hooded singer that floats through walls; only hurt in the light; 2 hits |
| Ember Priest | crimson priest with a burning censer, throws fireballs; 3 hits |
| The Red Abbot | the boss: bell-shaped iron mitre, rings of fireballs, summons wraiths; 20 hits |

### Debug keys

| Key | Action |
|-----|--------|
| F1 | debug overlay (collision boxes, enemy sight circles and view cones, reliquary reach) |
| F2 | post-process effect on/off (red PS1 look) |
| F3 | god mode (can't be hurt) |
| F4 | break the current wing's bell instantly |
| F6 | show FPS |

## The wings

1. **The Ash Gate**: 3 seals, the easy start.
2. **The Hall of Prayer**: 4 seals, a pillared hall with a crimson carpet.
3. **The Scriptorium**: 4 seals, bookshelf mazes and Ember Priests.
4. **The Ossuary**: 5 seals, bones and very little light.
5. **The Bell Tower**: 5 seals and the Red Abbot, almost pitch black.
6. **The Sanctum**: the ending.

## Building (Windows)

You need **w64devkit** (gcc + make) and **raylib 6.0** for MinGW-w64.

```
make                 # debug build -> AshenMonastery.exe
make run             # build and run
make autotest        # build and run the self-test (screenshots in shots/)
make release         # optimized build without a console window
make clean
```

The Makefile expects raylib at `C:/Users/HP GM/Downloads/raylib-6.0_win64_mingw-w64`. If yours is
elsewhere: `make RAYLIB_PATH="D:/path/to/raylib-6.0_win64_mingw-w64"`. No make? Run `build.bat`
(edit the two paths at its top). In VS Code, **Ctrl+Shift+B** builds and **F5** starts the debugger.

Command-line options:

- `AshenMonastery.exe --wing 3` starts directly in wing 3, skipping the menu.
- `AshenMonastery.exe --autotest` validates and renders every wing, plays wing 1 with scripted input
  (reliquaries, door, lightning, death and checkpoint), fights the Red Abbot, plays the Sanctum ending,
  saves screenshots to `shots/`, prints a summary and exits with 0 (or 1 if anything failed).

Progress is saved in `save.txt` (the highest wing reached); the menu then offers **Continue**.

## Files

```
src/main.c        window, main loop, command-line flags (--wing N, --autotest)
src/config.h      every tunable number, the per-wing difficulty table, Ward Seal names
src/game.c/.h     state machine, wing flow, reliquaries, Red Lightning, checkpoints, saving,
                  intro, the Sanctum ending, autotest
src/world.c/.h    loads a wing text file, validates it, builds chunked block meshes with baked
                  torch light, collision and line of sight
src/textures.c/.h block texture atlas: Poly Haven photos where present, generated tiles otherwise
src/meshgen.c/.h  helper that builds block meshes (faces, boxes) on the CPU
src/character.c/.h Kael, Ashen Monk, Choir Wraith, Ember Priest, Red Abbot, Sanctum NPCs, fireballs
src/player.c/.h   movement, dash, wand timing, hearts, third-person camera with wall collision
src/enemy.c/.h    enemy AI (sight, hearing, wind-ups, twitching), fireballs, separation
src/render.c/.h   world shader (fog, torchlight, player light, lightning flash), reliquaries, doors, flames
src/post.c/.h     640x360 scene + post-process (red/black or gold grade, grain, dither, vignette)
src/atmos.c/.h    falling ash and rising embers
src/audio.c/.h    sound effects + ambience; the lightning crack and the bells are synthesised
src/ui.c/.h       fonts, menu, intro, HUD, banners, dialogue, pause, inventory, death, final screen
src/input.c/.h    keyboard + mouse -> one Input struct per frame (the autotest fakes it)
src/screen.c/.h   fixed 1280x720 virtual screen, scaled to fit any window/monitor
assets/wings/     wing1.txt ... wing5.txt, sanctum.txt (format: see CLAUDE.md section 5)
assets/shaders/   world.vs / world.fs, post.fs
assets/textures/  Poly Haven photo textures (optional)
assets/fonts/     Pirata One, Crimson Text
assets/audio/     Kenney sound packs, OpenGameArt ghost sounds and ambience
legacy_2d/        the original 2D version of this project (not compiled)
```

Map characters: `#` stone wall, `W` wood wall, `B` bookshelf, `P` pillar, `T` wall with torches,
`.` stone floor, `=` crimson carpet, `,` wood floor, `x` bone pile, `@` start, `C` reliquary,
`E` exit door, `s g w Q` Ashen Monk, Choir Wraith, Ember Priest, Red Abbot, `O a` Master Oren and
the apprentices/monks (Sanctum).

See `CREDITS.md` for assets, `NOTES.md` for design decisions, `UPGRADE.md` for the Ashen Monastery spec.
The previous version of this game (Blackthorn Manor) is tagged `submittable` in git.
