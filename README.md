# Jill of the Jungle — Sega Master System port (devkitSMS)

A Sega Master System conversion of Epic MegaGames' *Jill of the Jungle*
(Episode 1), built with devkitSMS / SMSlib / SDCC.

This repository contains **no copyrighted Epic assets**. `make assets` extracts
maps, tiles, sprites and the palette from a local, legally obtained copy of the
shareware release and generates the `gen/` data.

## Build

```sh
# SDCC must be on PATH (Debian/Ubuntu: apt install sdcc; or see tools/get_sdcc_4_2.sh)
make assets     # convert the original data files -> gen/
make            # -> jill.sms (336 KB, Sega mapper, 21 banks)
make playtest   # automated playability gate (see below)
```

`JILL_DATA=/path/to/JILL1_FILES make assets` selects a different data directory.

## What the engine implements

* **Data conversion** — `.JN1` maps, `JILL.DMA` metatiles/collision flags,
  `JILL1.SHA` shapes and the VGA palette from `JILL.EXE` are converted to SMS
  4bpp tiles, 16x16 metatiles, collision flags, spawn points, item/enemy/door
  object lists and inter-level transitions. All 9 Episode 1 maps are converted
  and banked across the ROM.
* **Renderer** — hardware-scrolled background with ring-buffer column/row
  streaming (only the newly exposed strip is redrawn as the camera moves), plus
  sprite compositing for Jill, items, enemies and projectiles.
* **Player** — acceleration/friction, gravity, variable-height jump, 16x24
  collision box, walk/jump animation with per-frame sprite tile streaming.
* **Terrain interaction** — solid blocks, one-way platforms (`JF_PLAT`),
  **vine climbing** (`JF_VINE`: attach, climb in 4 directions, jump off),
  **water** (`JF_WATER`: buoyancy, slow sinking, swim strokes, halved movement
  speed) and **deadly tiles** (`JF_DEADLY`).
* **Items** — pickups drive real state: food restores health, plus gems, the red
  key, and the knife (which enables the weapon).
* **Combat** — throwable knife projectile that kills enemies and is absorbed by
  walls; enemies with walking, flying and static-trap behaviours; contact
  damage with knockback and invincibility frames.
* **Doors** — jewel/key doors open on contact when the matching item is held,
  stamping the neighbouring background metatile over the door and updating
  collision.
* **Progression** — checkpoint-based level transitions (edge-triggered, so
  arriving inside a zone doesn't bounce you straight back), a map/level return
  system, health, 3 lives, respawn, and game over (resets progress and returns
  to the overworld map).
* **Audio** — PSG sound effects for jump, pickup, throw, hurt and doors.

## Automated playtesting

The ROM is playtested **by machine, not by hand**. `tools/smstest/` is a small
headless Master System emulator (Z80 + Sega mapper + VDP + pads) that boots the
real `jill.sms`, presses buttons, and asserts on live game state. Because SDCC
emits a NoICE symbol file, tests refer to engine variables *by name*:

```
hold up
waituntil climbing == 1 300 w1
run 200
expect py < 90 s16          # she actually moved up the vine
```

`tests/playtest.txt` drives 14 end-to-end scenarios covering boot, walking,
jumping, item pickup, level transitions (including the re-entry edge case),
vine climbing, water physics, throwing the knife to kill an enemy, contact
damage, death/respawn, and game over. It also renders frames and fails if the
screen comes back blank — a guard that caught two real cases where screenshots
were being taken while the VDP was still off mid level-load.

```sh
make playtest     # exit status != 0 if any assertion fails -> usable as a CI gate
```

Screenshots land in `shots/`. `tools/smstest/test_signedcmp` is a unit test for
the emulator's own Z80 signed-comparison flag handling, so a failing game test
can be trusted to mean a game bug rather than an emulator bug.

`make test` additionally runs the converter/data validation in
`tools/test_jill_data.py`, and `tools/extract_test_coords.py` dumps real map
coordinates (transitions, items, enemies, vine/water/deadly cells) for authoring
new test scenarios.

## Known limitations

* Morph forms (bird/fish/frog), lifts and moving platforms, in-game text/message
  boxes and the score/HUD are not implemented.
* Enemy AI is a behavioural approximation grouped by object type rather than a
  faithful reimplementation of each original object's script.
* Background music is not implemented (sound effects only).
