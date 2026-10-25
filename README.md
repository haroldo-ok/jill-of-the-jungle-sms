# Jill of the Jungle — Sega Master System port (devkitSMS)

A complete Sega Master System conversion of Epic MegaGames' *Jill of the Jungle*
(Episode 1, shareware release), built with devkitSMS / SMSlib / SDCC.

This repository contains **no copyrighted Epic assets**. `make assets` converts
the original data files of a local, legally obtained copy of the shareware
release into the `gen/` sources (graphics, levels, text, music, sound effects).
Nothing is a placeholder: every level, sprite, text page, tune and effect that
the shareware data contains is converted.

## Build

```sh
# SDCC 4.x and devkitSMS (SMSlib.lib, crt0_sms.rel) must be available; python3 + Pillow
JILL_DATA=/path/to/JILL1_FILES make assets    # original data -> gen/
make                                           # -> jill.sms (512 KB, Sega mapper)
make playtest                                  # headless emulator regression run
```

## What is converted

* **Graphics** — tiles/sprites come from `JILL.DMA` / `JILL1.SHA` and are colour-mapped
  from the original **VGA palette** (taken from `JILL.EXE`), not the EGA one. Each level gets
  its own optimised 16-colour background palette and sprite palette; fades use them.
* **Levels** — the map hub, levels 1, 2, 3, 4, 6, 9 and the ending level (50), plus the three
  shareware **demo worlds** (`0.DEM`, `1.DEM`, `2.DEM`), all with their original object lists,
  switches/doors/lifts/bridges, enemies, hazards, hints, checkpoints and per-level music.
* **Text** — in-level signs are baked into the background with the original 6x6 / 8x8 fonts;
  help, hints and the shareware information pages (re-flowed to 30 columns) are scrollable text pages.
  The commercial order-form / BBS pages of the original intro are intentionally not carried over.
* **Audio** — the original `.DDT` songs and sound effects are converted for the PSG.
* **Ending** — the scripted macro walk (`END1.MAC`) and the four story slides.
* **Game features** — morph forms (frog, phoenix, fish), knife and fish-bullets, lifts, switches,
  keys and gem gates, water, vines, health, battery **save/load** (Pause menu → *Save game*,
  title → *Load saved game*), **turtle mode** (half speed, Pause menu), unlimited lives with
  restart-at-level-start like the original.

Controls: D-pad = move/climb/crouch, button 1 = jump, button 2 = throw knife / shoot,
Pause = menu (resume / save / turtle mode / help).

Enemy and object behaviours were reimplemented from the original data and observed behaviour;
a few of the less common ones (e.g. eye, knight, gator patterns) are approximations. Two object
types that occur only in the Volume II demo (`1.DEM`: types 9 and 52) have no graphics in the
supplied data and are ignored.

## Architecture notes

* One translation unit (`src/main.c`) compiled by SDCC. The ROM is 512 KB: slot 0 holds the resident
  code, slot 1 is switched between two **code overlays** (banks 30/31 — game session/drawing/UI vs.
  object behaviour), slot 2 holds the data banks (2..26). `tools/split_overlays.py` distributes the
  compiled assembly over the three code areas (SDCC's `#pragma codeseg` is per translation unit),
  and `tools/ihx2rom.py` packs the linker output into the final ROM and fixes the header checksum.
* Name table is a 32x28 ring buffer streamed per column/row; levels with large baked text
  (the ending) stream text tiles on demand.
* Sprites are 8x16; frames are cached in a VRAM pool and uploaded on demand.

## Automated playtesting

`tools/smstest/` is a small headless Master System emulator (Z80, Sega mapper with SRAM, VDP,
pads, pause button) that boots the real ROM, presses buttons and asserts on game variables by
name via the SDCC symbol file; it also has debugging aids (`bp`, `spwatch`, `rwatch`, `pc`).

```sh
make playtest     # tests/playtest.txt: title, hub, pause/save, every level, demos, ending
```

`make debug` builds `jilldbg.sms` with all symbols visible.

## Development log (what was done)

1. **Conversion pipeline** (`tools/convert_jill.py` and helpers): levels, VGA-palette graphics,
   sprites, fonts, text, music and sound effects are converted from the original data files into
   `gen/` sources; nothing is a placeholder. Per-level palettes and tile budgets, streamed text
   tiles for the ending level, baked in-level signs.
2. **Engine** (`src/*.inc`, single translation unit): ring-buffer scrolling, sprite frame cache,
   player physics (walk, jump, climb, swim, morph forms), all object types (enemies, switches,
   doors, lifts, bridges, pads, tokens), HUD, hints, pause menu.
3. **Memory layout**: 512 KB ROM; resident code in slot 0, two code overlays in banks 30/31
   (`tools/split_overlays.py` splits SDCC assembly), data banks 2..26, own packer
   `tools/ihx2rom.py`.
4. **Ending**: scripted macro walk plus the four story slides.
5. **Demo worlds and title menu**: `0.DEM`, `1.DEM`, `2.DEM` as extra levels; menu with Start /
   Load saved game / How to play / three demos.
6. **Save / load (SRAM), turtle mode, pause menu**; PC-specific help text rewritten for SMS controls.
7. **Bugs found with the headless emulator and fixed**:
   * u8 overflow in `camera_track` that corrupted RAM after large camera jumps (random reboots);
   * jump felt dead: variable-height cut removed (original has fixed jumps), button edges are
     detected from the held state so they survive slow frames, object culling made cheaper;
   * the dagger is now a boomerang that returns to Jill and is lost only in hazards (no ammo);
   * music/SFX now run from the frame interrupt (`SMS_setFrameInterruptHandler`); SDCC's
     `__critical` clobbers the first argument, so interrupts are disabled with explicit `di`/`ei`;
   * HUD redraw was ~0.5 s per change (every throw/catch/pickup); now ~20x faster;
   * returning from a level put Jill on the entrance, re-entering it (return position fixed);
   * knight attack pose (`touch()` takes a signed inset).
8. **Tooling**: `tools/smstest` emulator gained SRAM, pause button, `pc`, `bp`, `spwatch`, `rwatch`,
   `vregs`, PSG write counting/logging and a cycle profiler (`profstart` / `profdump`).

## Known limitations

* Not tested on real hardware; the game logic runs at ~30 fps and drops lower while the camera
  scrolls (tile streaming is the main cost).
* Object types 9 and 52 (Volume II demo only) have no graphics in the supplied data and are ignored.
* Some enemy behaviours (eye, knight, gator) are approximations; the spinning blade token lasts a
  fixed time (the original behaviour is unverified).
* The commercial order-form / BBS pages of the original intro are intentionally not included.
