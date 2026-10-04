# The Ashen Monastery

*Climb. Break the bells. Bring them home.*

A third-person, retro PS1-style gothic horror game in **C99 + raylib 6.0**: low-poly gothic
architecture with real textures, low-resolution wobbly rendering, moonlight and candlelight in fog.

> High on a storm-swept mountain stands the Ashen Monastery, a gothic castle and school for young
> mages. Ten nights ago its Abbot made a pact with fire and became **the Red Abbot**. He cast **five
> cursed bells** from the ashes of the dead; each time they toll, the monks forget who they were. To
> make himself immortal he bound his life into **five Soul Relics** and his **serpent familiar**.
> Master **Oren** and the other apprentices are trapped in the **Sanctum** at the summit. You are
> **Kael**, the youngest apprentice, armed with a wand. Climb the monastery, destroy the relics,
> break the bells, free the serpent's cage and end the Red Abbot.

## How to play

- Every room has a **chest** (hold **E**). Most hold a **Ward Seal**: +1 heart and a checkpoint.
- The **last chest** in a wing releases that wing's **Soul Relic**. Strike it three times with
  **Red Lightning**: it shatters, the wing's cursed bell breaks and the exit doors swing open.
- In **the Bell Tower** the Ember Serpent lies in a sealed iron cage. Once all five relics are
  destroyed the seal breaks: smash the cage (6 hits), defeat the serpent (dodge sideways when it
  rears up and glows red), and the Red Abbot loses his shield. Defeat him and climb to the Sanctum.
- It is forgiving: 5 hearts, every hit costs exactly one, every enemy attack glows red for half a
  second before it lands, Red Lightning aims itself, and if the fire takes you, you rise again at
  your last chest with full hearts (opened chests and destroyed relics stay that way).
- The **minimap** (bottom right) always shows the chests (gold), the exit and you. **M** opens
  the full map.

## Controls

| Key | Action |
|-----|--------|
| W A S D | move (relative to the camera) |
| Mouse (or arrow keys) | look around |
| **Left click** | **Red Lightning** (aims itself at the nearest relic / enemy you can see, range 12) |
| Shift | dash: a quick dodge, nothing can hurt you while dashing |
| E (hold) | open a chest / talk (in the Sanctum) |
| M | full-screen map |
| I or Tab | inventory: seals and relics of all wings, hearts, enemies defeated, time |
| Esc or P | pause (Resume / Restart wing / Quit to title) |
| Enter | confirm, skip the intro, advance dialogue, rise again after falling |

### Debug keys (kept in the final build)

| Key | Action |
|-----|--------|
| F1 | debug overlay (collision boxes, enemy sight circles and view cones, chest reach) |
| F2 | PS1 post-process on/off |
| F3 | god mode (can't be hurt) |
| F4 | complete the current wing instantly (chests, relic, cage, serpent, Abbot) |
| F6 | show FPS |

### Enemies

| Enemy | |
|-------|--|
| Ashen Monk | charred hooded monk with glowing ember cracks; lurches toward you; 2 hits |
| Choir Wraith | pale singing ghost that floats through walls; can only be hurt in the light; 2 hits |
| Ember Priest | crimson robe and burning censer; keeps its distance and throws fireballs (shoot them down); 3 hits |
| Ember Serpent | the Abbot's familiar in the Bell Tower; circles you and lunges; 8 hits |
| The Red Abbot | the boss: bell-shaped iron mitre, rings of fireballs, summons wraiths; immortal until the serpent dies; 20 hits |

## The wings

1. **The Ash Gate** - entrance halls and cloisters, 3 chests, the Ashbound Grimoire.
2. **The Hall of Prayer** - a great dining hall and dormitories, 4 chests, the Ember Ring.
3. **The Scriptorium** - bookcases and a tower study, 4 chests, the Moonsilver Locket.
4. **The Ossuary** - alchemy vaults and crypts, 5 chests, the Chalice of Cinders.
5. **The Bell Tower** - the great bell, the caged serpent and the Red Abbot, 5 chests, the Thorned Crown.
6. **The Sanctum** - the ending.

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
elsewhere: `make RAYLIB_PATH="D:/path/to/raylib-6.0_win64_mingw-w64"`. Put w64devkit's `bin` first on
PATH. No make? Run `build.bat` (edit the two paths at its top). In VS Code, **Ctrl+Shift+B** builds
and **F5** starts the debugger.

Command-line options:

- `AshenMonastery.exe --wing 3` starts directly in wing 3 (`--wing 6` or `--sanctum`: the Sanctum).
- `AshenMonastery.exe --autotest` loads and validates every wing, renders them, plays wing 1 and
  the whole Bell Tower with scripted input, fights everything, plays the Sanctum ending, saves
  screenshots to `shots/`, prints a summary and exits with 0 (or 1 if anything failed).
- `AshenMonastery.exe --tour` saves 4 screenshots per wing (`shots/tour_wN_K.png`: start, largest
  room, a corridor, start with the HUD) and prints the average FPS (measured without vsync).
- `--bright` (developer aid) lights everything flatly to inspect geometry in screenshots.

Progress is saved in `save.txt` (the highest wing reached); the title menu then offers **Continue**.

## Files

```
src/main.c          window, main loop, command-line flags
src/config.h        every tunable number, the per-wing table (difficulty + mood), names
src/game.c/.h       state machine, wing flow, chests, Red Lightning + aim assist, checkpoints,
                    saving, the Sanctum, the autotest and --tour
src/world.c/.h      wing loader + validation, rooms/corridors/islands, collision, line of sight
src/architecture.c  grid -> walls, arches, plinths, cornices, pilasters, columns, vaults, beams,
                    floors, windows, sconces, exit doorways (one mesh per chunk + material)
src/geo.h           the static-geometry builder shared by architecture.c, props.c, title.c, relics.c
src/props.c         prop placement (wall slots, kept-free lanes, reachability) + all prop models
src/textures.c/.h   world materials (Poly Haven photos or generated) + the small character atlas
src/character.c/.h  code-built characters (robes, spheres, cylinders), animations, .glb model slots
src/player.c/.h     movement, dash, wand timing, hearts, third-person camera with collision
src/enemy.c/.h      enemy AI (sight, hearing, wind-ups, twitching), fireballs, separation
src/relics.c/.h     Soul Relics, the cage, the Ember Serpent, the Abbot's shield
src/render.c/.h     world shader, light selection (16 per frame), doors, chests, flames, moonlight shafts
src/post.c/.h       640x360 scene target + post-process (moonlit grade, grain, dither, vignette)
src/minimap.c/.h    corner minimap + full-screen map
src/title.c/.h      the 3D title castle + title menus (controls, credits)
src/atmos.c/.h      falling ash (Bell Tower) and rising embers
src/audio.c/.h      sounds + ambience; crack, bells, thunder, wind, whoosh, drone and hiss are synthesised
src/ui.c/.h         fonts, HUD, banners, pause, inventory, death, dialogue, final screen
src/input.c/.h      keyboard + mouse -> one Input struct per frame (the autotest fakes it)
src/screen.c/.h     fixed 1280x720 virtual screen, scaled to fit any window/monitor
assets/wings/       wing1.txt ... wing5.txt, sanctum.txt (format: CLAUDE.md section 7.1)
assets/shaders/     world.vs / world.fs (per-pixel lights, fog, PS1 wobble), post.fs
assets/textures/    Poly Haven photo textures (optional; generated fallbacks)
assets/models/      optional .glb replacements (see README.txt there)
assets/fonts/       Pirata One, Crimson Text
assets/audio/       Kenney sound packs, OpenGameArt ghost sounds and ambience
reference/          mood photos used as a palette/lighting reference (not shipped in the game)
docs/old_specs/     earlier design documents
legacy_2d/          the original 2D version of this project (not compiled)
```

See `CREDITS.md` for assets, `NOTES.md` for design decisions and status, `CLAUDE.md` for the spec.
Earlier versions are tagged in git: `submittable` (Blackthorn Manor), `environment-done`,
`title-done`, `relics-done`, `final`.
