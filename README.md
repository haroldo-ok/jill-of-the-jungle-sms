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
  walls; contact damage with knockback and invincibility frames.
* **Enemy AI** — six behaviour classes mapped from the original object types:
  walkers that patrol and reverse at walls *and* ledge edges (so they don't
  walk off platforms), frog-like hoppers that leap toward Jill, flyers that
  drift and bob, homing chasers that track her position, and shooters that fire
  aimed projectiles.  Off-screen enemies are frozen for performance.
* **Transformations** — creature potions morph Jill into a bird (free flapping
  flight), a frog (a high fixed-arc leap), or a fish (free vertical swimming),
  for a timed duration; she reverts on expiry, death, or level change.
* **Doors** — jewel/key doors open on contact when the matching item is held.
  A door is taller than one metatile, so opening it clears every metatile row it
  spans (not just the top) and updates collision for the whole opening, letting
  the player walk through.
* **Progression** — checkpoint-based level transitions (edge-triggered, so
  arriving inside a zone doesn't bounce you straight back), a map/level return
  system, health, 3 lives, respawn, and game over (resets progress and returns
  to the overworld map).
* **HUD** — a live status display (health heart, lives, gem count) rendered from
  a small 8x8 sprite font in the otherwise-unused sprite tiles 48-63; the digits
  update directly from game state.
* **Audio** — PSG sound effects for jump, pickup, throw, hurt and doors on
  channel 0 + noise, plus original background music on channels 1 (melody) and 2
  (bass): four distinct tracks (overworld, jungle, caves, water) selected per
  level, and short event stingers for key pickups, level entry and death.  All
  music is original composition in the game's general idiom, not a transcription
  of the original soundtrack (which is separately copyrighted).  Effects, music
  and stingers share the chip without cutting each other out.

## Performance

Two optimisations were ported from the author's CK1-SMS engine.  The buffered
VDP blitter (streaming a run of tiles after setting the write address once) is
in place, though profiling showed it isn't the bottleneck here — the per-frame
cost is dominated by game logic and sprite setup, not tile writes.  The sprite
fast-path (writing SMSlib's sprite tables directly for fully on-screen sprites,
skipping per-sprite clipping arithmetic) is the measurable win: it cut the fixed
per-frame cost from ~66,300 to ~61,000 Z80 cycles (about 8%).  The test harness
can profile this directly via a marker port (`make playtest` reports it).

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

`tests/playtest.txt` drives 22 end-to-end scenarios covering boot, walking,
jumping, item pickup, level transitions (including the re-entry edge case),
vine climbing, water physics, throwing the knife to kill an enemy, contact
damage, death/respawn, game over, the HUD/audio, the retuned jump height, all three transformations, and each enemy behaviour class (walker reversal, shooter projectiles, contact damage).  Later scenarios use a harness `reset` so each starts from a clean boot rather than depending on prior state; the HUD/audio (it counts on-screen sprites
and inspects the PSG's per-channel volume registers to confirm the melody, bass
and sound effects are actually sounding). It also renders frames and fails if the
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

## Hardware compatibility

The ROM is tested to boot and run on accuracy-focused emulators (e.g. Emulicious)
as well as lenient ones.  One earlier hazard is worth recording: an automated
profiling hook wrote to I/O port 0x3E, which on the Master System is the memory-
control register, not a spare port.  Lenient emulators ignored it, but an
accurate one faithfully reconfigured the hardware and crashed at boot.  The
profiling marker now lives in RAM instead (harmless on real hardware), and the
only I/O ports the game touches are the PSG (0x7F) and the VDP (0xBE/0xBF).

## Known limitations

* Lifts, moving platforms, and in-game text/message boxes are not implemented.
* Enemy AI groups the original's many object scripts into six behaviour classes
  rather than reimplementing each one exactly.
* The background music is an original composition in the spirit of the game, not
  a transcription of the original soundtrack (which is separately copyrighted).
