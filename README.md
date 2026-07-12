# Jill of the Jungle — Sega Master System devkitSMS port (work-in-progress)

This workspace contains a compileable Sega Master System/devkitSMS conversion
scaffold for the Jill of the Jungle shareware data files.  It does **not**
vendor Epic's original copyrighted assets; `make assets` extracts data from a
local legally obtained copy (the archive downloaded in `../jill-source/ep1` in
this workspace).

Current status:

* Parses Jill `.JN1` maps, `JILL.DMA`, `JILL1.SHA`, and the VGA palette from
  `JILL.EXE`.
* Converts all Episode 1 maps present in the shareware archive to SMS 4bpp
  background tiles, metatile maps, collision flags, spawn points and checkpoint
  transitions.
* Builds a real SMS ROM (`jill.sms`) with devkitSMS/SMSlib/SDCC.
* Implements a minimal side-view engine: scrolling map renderer, Jill player
  movement, gravity/jump/collision, and checkpoint-based level transitions.

Known gaps before this can honestly be called “fully playable”:

* Object/enemy behaviours, switches, doors, keys, morph forms, lifts, water,
  collectibles, score/health and audio are not yet implemented.
* Sprite art mapping is provisional (Jill frame extraction only; enemies are
  not animated or active).
* Rendering uses a simple full-screen redraw on coarse camera movement rather
  than the optimized ring-buffer streaming used by `ck1-sms`.

## Build

```sh
# SDCC must be on PATH. If needed, see tools/get_sdcc_4_2.sh.
make assets test jill.sms
```

To use a different Jill data directory:

```sh
JILL_DATA=/path/to/JILL1_FILES make assets
```

## Tests

`make test` runs converter/data validation.  `tools/playtest_static.py` is a
static playability-gate placeholder: it verifies that maps, transitions, spawns
and collision data were produced and reports the still-missing runtime systems.
