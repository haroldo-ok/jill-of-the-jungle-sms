# smstest

A minimal headless Sega Master System emulator (Z80 core by superzazu, MIT)
used to automatically playtest `jill.sms` in CI, with no GUI and no human.

It emulates the Z80, the Sega mapper, work RAM, the VDP (mode 4: VRAM/CRAM,
frame + line IRQs, vcounter, and a full background/sprite rasteriser so tests
can take screenshots) and the joypad ports.  PSG writes are accepted.

Tests are written as small scripts (see `../../tests/playtest.txt`).  Because
devkitSMS/SDCC emits a NoICE symbol file (`jill.noi`), the script can refer to
game variables *by name* — so a test can hold a button, wait until `on_ground`
becomes 1, and assert that `gems` incremented.

Commands:

    symbols <file.noi>                 load symbol names
    hold <btn[+btn...]|none>           set held buttons (up down left right 1 2)
    tap <btns>                         press for 2 frames then release
    run <frames>                       advance emulation
    waituntil <sym> <op> <val> <max> [w1|w2|s16]
    expect <sym|0xADDR> <op> <val> [w1|w2|s16]
    poke <sym|0xADDR> <val> [w1]       teleport/force state to set up a scenario
    print <sym> [w1|w2|s16]
    screenshot <file.ppm>
    expectcolors <n>                   fail if the frame has < n distinct colours
    expectvdp display <0|1>
    echo <text>

Exit status is non-zero if any expectation failed, so `make playtest` works as
a CI gate.
