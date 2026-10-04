The Ashen Monastery - model slots
=================================

Every character and relic is built in code. If you put a .glb file with one of the names below
into this folder, the game loads it instead (LoadModel + LoadModelAnimations) the next time it
starts. Missing files are simply ignored.

  player.glb          Kael, the apprentice                  scaled to 1.75 units tall
  monk.glb            Ashen Monk                            2.0
  wraith.glb          Choir Wraith (drawn semi-transparent) 2.0
  priest.glb          Ember Priest                          2.1
  abbot.glb           The Red Abbot (boss)                  3.6
  serpent.glb         Ember Serpent                         1.2
  oren.glb            Master Oren                           1.8
  apprentice.glb      Ilsa, Tobin, Mira and the freed monks 1.7
  relic_grimoire.glb  The Ashbound Grimoire                 0.5
  relic_ring.glb      The Ember Ring                        0.5
  relic_locket.glb    The Moonsilver Locket                 0.5
  relic_chalice.glb   The Chalice of Cinders                0.5
  relic_crown.glb     The Thorned Crown                     0.5

Rules
- The model is scaled so its bounding box has the height above, feet on the floor (y = 0).
- It should face +Z. If it faces another way, change MODEL_YAW_OFFSET in src/config.h.
- Animations are matched by name (case does not matter, the name only has to contain the word):
    idle | walk or run | attack or cast | hit | death
  Missing animations fall back to idle (or to the first animation).
- The model is lit by the game's world shader (fog, candles, moonlight).
