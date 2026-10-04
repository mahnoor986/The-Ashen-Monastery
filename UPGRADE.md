# UPGRADE: Blackthorn Manor → THE ASHEN MONASTERY

This file **overrides `CLAUDE.md` wherever they conflict**. Everything in `CLAUDE.md` that this file does not
change stays as it is (build environment, code rules, autotest, wing/chest/checkpoint systems, hearts, aim assist).

**Time limit: a few hours.** The current game works and is the student's safety net. Rules:
- First: `git add -A && git commit -m "Blackthorn Manor - working version"` and `git tag submittable`.
- Work in the task order below. Build with zero warnings, run `--autotest`, look at the screenshots, and
  `git commit` after EVERY task. Never leave the build broken.
- Do not rewrite systems that already work. Change how things look and are named, not how the game works.
- If a task runs long or gets risky, stop it, revert to the last commit, and move to the next task.
- Do not ask the user questions unless blocked. Log decisions in `NOTES.md`.
- Everything must stay original: no characters, names or designs from existing games, films or books.

---

## The story

**Title:** THE ASHEN MONASTERY — subtitle: "Climb. Break the bells. Bring them home."

High on a burning mountain stands the Ashen Monastery, a school for young mages. Ten nights ago its Abbot made a pact
with fire and became **the Red Abbot**. He cast **five cursed bells** from the ashes of the dead. Each time they toll,
the monastery burns and the monks forget who they were. Master **Oren** and the other apprentices are trapped in the
**Sanctum** at the very top. You are **Kael**, the youngest apprentice, armed with a wand. Climb the monastery and
break all five bells.

**How the existing systems map to the story:**
- Each wing holds one Cursed Bell. The bell is bound by **Ward Seals**, one in a reliquary chest in every room
  (the existing chests). Opening all chests in a wing shatters that wing's bell → the existing "wing complete → exit
  opens" logic. Show the banner "THE FIRST BELL SHATTERS" (SECOND, THIRD, FOURTH, FIFTH) with a red screen flash,
  strong camera shake and a bell-crack sound.
- Wing 5 additionally requires the Red Abbot dead (existing boss logic).
- After wing 5, instead of the old victory screen, load the new **Sanctum** ending scene (task 6).

**Wings** (keep the existing map files and layouts; only change the name on the first line of each file):
1. The Ash Gate
2. The Hall of Prayer
3. The Scriptorium
4. The Ossuary
5. The Bell Tower

**Ward Seal names** (replace the old treasure names, same counts 3/4/4/5/5):
- Wing 1: Seal of Cinders, Seal of the Iron Key, Seal of the Pilgrim
- Wing 2: Seal of Hymns, Seal of the Kneeling Saint, Seal of Candlewax, Seal of Silence
- Wing 3: Seal of Ink, Seal of the Burned Page, Seal of the Quill, Seal of Forgotten Names
- Wing 4: Seal of Bone, Seal of the Nameless, Seal of Marrow, Seal of the Last Rite, Seal of Dust
- Wing 5: Seal of Embers, Seal of the Rope, Seal of the Toll, Seal of the Abbot's Ring, Seal of the Red Flame

Banner when a chest opens: "Ward Seal found: Seal of Cinders". HUD list title: "Ward Seals 1/3".

**Enemies** (rename + restyle; keep the existing AI behaviors and stats):

| Old | New | Look |
|-----|-----|------|
| Skeleton | **Ashen Monk** | charred black-grey robe with glowing orange ember cracks, hood, glowing red eyes |
| Ghost | **Choir Wraith** | pale grey-white, translucent, hooded, mouth open (dark box), floats; moan sound is its "singing" |
| Witch | **Ember Priest** | dark crimson robe, tall hood, holds a burning censer; throws **fireballs** (orange-red) instead of purple hexes |
| Witch Queen | **The Red Abbot** | 1.8× size, crimson-and-black robes, tall iron mitre shaped like a bell, burning eyes, two orbiting fireballs; same ring-of-bolts and summon attacks (summons Choir Wraiths). Boss bar name: "THE RED ABBOT" |

Rename the code identifiers too if it is quick and safe (enum names, comments); otherwise only rename what the player sees.

---

## Task 1 — PS1 horror look (red & black) (~45 min)

1. Render the 3D scene into a low-resolution `RenderTexture2D` of **640×360** (constants in config.h), texture filter
   `TEXTURE_FILTER_POINT`. Then draw it scaled up to the full 1280×720 window through a new post-process shader.
   Draw all UI afterward at full resolution so text stays crisp.
2. New `assets/shaders/post.fs` (GLSL 330, fragment-only; raylib supplies the default vertex shader):
   - Red/black color grade: compute luminance; map dark tones toward black, mid tones toward deep blood red
     (≈ 0.55, 0.05, 0.04), and keep bright warm tones (fire, torches) orange-yellow. Mix with the original color by
     `gradeStrength` (config, default 0.75) so the scene stays readable.
   - Crushed blacks and slightly raised contrast.
   - Film grain (hash noise animated by a `time` uniform, strength ~0.06).
   - Vignette (dark edges).
   - Subtle chromatic offset (sample R and B channels about 1 low-res pixel apart).
   - Optional PS1 color reduction: quantize to 5 bits per channel with a 4×4 ordered dither (config flag, default on).
3. Optional PS1 vertex wobble in `world.vs`: snap clip-space positions to a coarse grid (config flag, default ON but
   subtle; turn it off if it looks bad in screenshots).
4. Change fog color to very dark red-black (≈ 0.05, 0.01, 0.01) and tint ambient light slightly red.
5. **F2** toggles the post-process effect on/off (show the debug key in README).
6. If `post.fs` fails to load, draw the low-res render texture scaled up without the shader.

## Task 2 — Wand with red lightning (replaces the sword) (~45 min)

- Player model: remove the sword; the right hand holds a **wand** (thin dark wood box ≈ 0.35 long) with a small
  glowing red tip cube (drawn unlit/emissive). Idle: wand held forward-down. Cast: the right arm snaps forward
  pointing at the target for 0.2 s.
- Left click casts **Red Lightning**: cooldown 0.45 s, range 12 units, damage 1 (same as the old sword, so balance holds).
  - Aim: keep aim assist. On cast, if a living, damageable enemy is within 12 units **and visible** (line of sight),
    snap the character to face the nearest one and target its chest height. Otherwise fire along the camera's forward
    direction.
  - Hit test: march along the ray; stop at the first solid cell or the first enemy whose sphere it touches. The beam
    destroys any fireball it passes through.
  - Visual (lasts 0.15 s): a jagged bolt from the wand tip to the hit point. Build it from 8–12 segments with random
    sideways offsets that are **re-randomized every frame** (so it crackles). Draw each segment twice with additive
    blending: a thin bright core (≈ RGB 255,190,190) and a wider red glow (≈ 220,20,30 with alpha). Use thin
    `DrawCylinderEx` segments or camera-facing quads. Add 2–3 short forked branches.
  - Light flash: add a second point light uniform to the world shader (`flashPos, flashColor, flashRadius`).
    During the beam, put a strong red light (radius ~6) at the hit point so the walls flash red.
  - Small spark burst of red particles at the hit point; camera shake 0.15.
  - Sound: a sharp crack. Use the best matching existing file (impact/metal) with high pitch, or generate a short
    noise-burst crack procedurally with `LoadSoundFromWave` if nothing fits.
- Keep the Choir Wraith visibility rule (only damageable when lit). The beam's own flash does **not** count as light.
- Update the HUD crosshair to a small red dot and the controls text: "Left click: Red Lightning".

## Task 3 — Real textures (~45 min)

- Download free **CC0** textures from **Poly Haven** into `assets/textures/`. Try the public API first
  (`https://api.polyhaven.com/assets?t=textures` to list, `https://api.polyhaven.com/files/<id>` for file URLs).
  Pick diffuse maps at 1k resolution, **prefer PNG** (raylib may be built without JPG support; test-load each file and
  fall back to PNG if a JPG fails).
- Choose by name/category:
  - `wall.png`: dark castle/medieval stone brick
  - `wood_wall.png`: dark wood planks
  - `floor.png`: worn stone floor or cobblestone
  - `pillar.png`: dark marble or rough rock
  - `door.png`: rusty metal
  - `ceiling.png`: dark wood or rough stone
- If the API or download fails, print clear instructions for the user: open https://polyhaven.com/textures, search
  the keyword, download the 1k PNG diffuse, save it under the given file name. Then continue without it.
- In the atlas builder: for each atlas slot, if the matching file exists, load it, resize it to the atlas tile size
  (raise `ATLAS_TILE` to 64 px, nearest-neighbor resize for the PS1 look), darken it slightly, and paste it into the slot
  with `ImageDraw`. Otherwise keep the existing generated texture. The mesh code and UVs must keep working: make sure
  UV math uses the tile-size constant, keep the half-texel inset.
- Keep generated textures for bookshelf, carpet (make it deep crimson) and bone.
- Add Poly Haven (CC0) entries to `CREDITS.md` with the exact asset names used.

## Task 4 — Scarier enemies + burning atmosphere (~30 min)

- Enemy bodies: about 15% taller and 15% thinner than now, darker colors per the enemy table above.
- **Glowing red eyes** on all enemies: two tiny emissive cubes drawn unlit (not affected by fog darkness), so eyes
  are the first thing visible in the dark.
- **Twitching:** each enemy, at random intervals of 0.6–2.5 s, snaps its head to a random angle for 0.08–0.15 s,
  then back. Movement speed is modulated by noise (stutter-step) for Ashen Monks.
- Ashen Monks: arms hang lower; ember-crack texture (dark grey with scattered orange pixels).
- **Falling ash:** ~300 small grey flakes drifting down and sideways in a volume around the camera (wrap them around
  when they leave the volume). **Rising embers:** ~60 tiny orange glowing particles near torches. Draw both as
  camera-facing quads or small cubes; embers additive.
- **Bell tolls:** every 25–40 s a distant bell tolls (deep, low). If no bell sound exists among the assets, synthesize
  one with `LoadSoundFromWave` (sum of decaying sine partials, e.g. 110, 220, 277, 330, 440 Hz with different decay
  times, 4 s long). On each toll: the screen briefly pulses red and the torches flare.

## Task 5 — Story texts (~15 min)

- **Main menu:** title "THE ASHEN MONASTERY", subtitle, options New Game / Continue / Quit. Recolor the menu to red and black.
- **Intro** (after New Game, before wing 1; white text fading in on black, Enter/click skips), one line at a time:
  1. "Ten nights ago, the Red Abbot cast five bells from the ashes of the dead."
  2. "Each time they toll, the monastery burns, and the monks forget who they were."
  3. "Master Oren and the other apprentices are trapped in the Sanctum at the summit."
  4. "Climb, Kael. Break the bells."
- Wing banner: "THE FIRST BELL — The Ash Gate", etc.
- Death screen: "THE FIRE TAKES YOU" / "[ ENTER ] Rise from the ashes".

## Task 6 — The Sanctum ending (~45 min)

- New map `assets/wings/sanctum.txt` (not part of the 5-wing count, no chests, no enemies, validation must allow that):
  a long candle-lit hall about 9 wide × 22 deep, pillars along both sides, crimson carpet down the middle, player
  start at the south end. Add an `O` map char for Master Oren and `a` for apprentices/monks (NPC spawns).
- Lighting here is **warm gold**, the first warm scene in the game: fog color dark amber, higher ambient, post-process
  grade switched to a warm golden grade (add a `gradeMode` uniform: 0 = red horror, 1 = warm gold). No falling ash,
  no bell tolls. Gentle music: reuse the ambience at lower pitch/volume or silence.
- NPCs (box characters, non-hostile, no red eyes, normal proportions, stand and face the player, idle breathing):
  - **Master Oren** at the north end: grey robe, long white beard block, tall staff with a gold glowing tip.
  - Three apprentices near him: **Ilsa** (dark blue robe), **Tobin** (brown robe), **Mira** (green robe).
  - 4–6 freed monks in light grey robes lining the carpet, facing the center.
- Friends speak one line when the player walks within 2 units (text box at the bottom):
  - Ilsa: "You're covered in ash. Welcome back."
  - Tobin: "Five bells. I counted every one."
  - Mira: "Was he... was the Abbot still human, at the end?"
- Reaching Master Oren shows the prompt "[E] Speak with Master Oren"; dialogue box, Enter/click advances:
  1. "Kael... the fifth bell has fallen. I heard it break from here."
  2. "We thought the fire would take us all. Ilsa kept the candles lit. Tobin counted every toll."
  3. "The Abbot rang those bells to keep the mountain burning. You walked through his fire, and you came back."
  4. "Rest now. Tomorrow, you are no longer an apprentice."
- Then fade to black → final screen: "THE BELLS ARE SILENT", Ward Seals found, enemies defeated, total time, then a
  short credits list (the student's name placeholder "Created by: ___" for the user to fill in, plus the asset credits),
  Enter returns to the menu.
- Autotest: also render `shots/sanctum.png`.

## Task 7 — Wrap up (~15 min)

- `TARGET := AshenMonastery.exe` in the Makefile and update `.vscode/launch.json`.
- Update `README.md` (story, controls including Red Lightning and F2, how to build, file overview, debug keys) and
  `CREDITS.md` (fonts, Kenney audio, OpenGameArt sounds, Poly Haven textures; models, enemies, particles, bell and
  crack sounds generated in code where that is true).
- Update the story/title sections at the top of `CLAUDE.md` to say the game is now The Ashen Monastery and point to this file.
- `make release` must work. Final `--autotest` must pass. Commit and tag `final`.

## If time runs out — cut in this order

PS1 vertex wobble → rising embers → friend lines in the Sanctum → real textures (keep generated ones, recolored darker
and redder) → bell tolls. **Never cut:** the post-process look, the red lightning wand, the story renames, the Sanctum
ending with Master Oren's dialogue.