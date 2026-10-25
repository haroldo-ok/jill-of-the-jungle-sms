#!/usr/bin/env python3
"""CMF (.DDT) -> SMS PSG stream converter.

Stream format (60 Hz frames), bytes:
  0x80|ch<<4|att  lo hi   ch 0..2: set tone period (10 bit, lo/hi bytes) and attenuation (0 loud .. 15 off)
  0xC0|ch<<4|att          ch 0..2: attenuation only
  0xB0|ctl  b             noise hit: ctl bit2 = white, bits0-1 = rate; b = start_att<<4 | frames_per_step
  0xBF                    noise off
  0xF0+n  (n=0..12)       wait n+1 frames
  0xFD n                  wait n frames
  0xFF                    loop to start
"""
from __future__ import annotations
import math
import struct
from pathlib import Path
from typing import List

SONGS = ['dan', 'funky', 'it', 'jupiter', 'ominous', 'seven', 'spiders', 'stealth', 'zeppelin']
PSG_CLK = 3579545
FPS = 60

# drum channel -> (noise ctl, start att, frames/step)
DRUMS = {11: (0x06, 0, 2), 12: (0x05, 0, 2), 13: (0x06, 1, 3), 14: (0x04, 2, 4), 15: (0x04, 3, 1)}


def parse_cmf(path: Path):
    d = path.read_bytes()
    assert d[:4] == b'CTMF'
    ver, ioff, moff, tpq, tps = struct.unpack_from('<HHHHH', d, 4)
    p, t, ev, run = moff, 0, [], 0

    def vlq():
        nonlocal p
        v = 0
        while True:
            b = d[p]
            p += 1
            v = (v << 7) | (b & 0x7F)
            if not b & 0x80:
                return v
    while p < len(d):
        t += vlq()
        b = d[p]
        if b == 0xFF:
            ty, ln = d[p + 1], d[p + 2]
            p += 3 + ln
            if ty == 0x2F:
                ev.append((t, 'end', 0, 0, 0))
                break
            continue
        if b == 0xF0:
            p += 1
            while d[p] != 0xF7:
                p += 1
            p += 1
            continue
        if b & 0x80:
            run = b
            p += 1
        k, ch = run & 0xF0, run & 15
        if k in (0xC0, 0xD0):
            ev.append((t, k, ch, d[p], 0))
            p += 1
        else:
            ev.append((t, k, ch, d[p], d[p + 1]))
            p += 2
    return tps, ev


def period_of(note: float) -> int:
    f = 440.0 * 2 ** ((note - 69) / 12)
    return max(1, min(1023, int(round(PSG_CLK / (32 * f)))))


def convert(path: Path) -> bytes:
    tps, ev = parse_cmf(path)
    end_tick = ev[-1][0]
    nframes = int(math.ceil(end_tick * FPS / tps))
    chvol = [100] * 16
    active = [dict() for _ in range(16)]       # ch -> {note: vel}
    order = [[] for _ in range(16)]            # ch -> note stack (latest last)
    hold = [None] * 16
    ei = 0
    out = bytearray()
    state_tone = [None, None, None]            # (period, att)
    wait = 0

    def flush_wait():
        nonlocal wait
        while wait > 0:
            if wait <= 13:
                out.append(0xF0 + wait - 1)
                wait = 0
            else:
                n = min(wait, 255)
                out.extend([0xFD, n])
                wait -= n

    drum_hits: List[int] = []
    for f in range(nframes):
        limit = (f + 1) * tps / FPS
        drum_hits.clear()
        started: List[tuple] = []
        while ei < len(ev) and ev[ei][0] < limit:
            t, k, ch, a, b = ev[ei]
            ei += 1
            if k == 0x90 and b > 0:
                if ch >= 11:
                    drum_hits.append(ch)
                else:
                    active[ch][a] = b
                    if a in order[ch]:
                        order[ch].remove(a)
                    order[ch].append(a)
                    started.append((ch, a))
            elif k == 0x80 or (k == 0x90 and b == 0):
                if ch < 11 and a in active[ch]:
                    if (ch, a) in started:
                        hold[ch] = (a, active[ch][a])      # shorter than one frame: keep one frame
                    del active[ch][a]
                    if a in order[ch]:
                        order[ch].remove(a)
            elif k == 0xB0 and a == 7:
                chvol[ch] = b
        # candidate (note, score) per melodic channel: latest note of each channel
        cands = []
        for ch in range(11):
            if order[ch]:
                n = order[ch][-1]
                cands.append((n, active[ch][n] * chvol[ch] / 127.0, ch))
            elif hold[ch]:
                n, v = hold[ch]
                cands.append((n, v * chvol[ch] / 127.0, ch))
            hold[ch] = None
        voices = [None, None, None]
        if cands:
            cands.sort(key=lambda c: c[0])
            lead = cands[-1]
            rest = cands[:-1]
            voices[0] = lead
            if rest:
                bass = rest[0]
                voices[2] = bass
                mid = [c for c in rest[1:]]
                if mid:
                    voices[1] = max(mid, key=lambda c: c[1])
                elif len(rest) == 1:
                    pass
        for vi, v in enumerate(voices):
            if v is None:
                new = None
            else:
                note, score, ch = v
                while note < 45:
                    note += 12
                while note > 100:
                    note -= 12
                att = 15 - int(round(min(1.0, score / 100.0) * 13)) if score > 0 else 15
                att = max(0, min(15, att + (1 if vi else 0)))
                new = (period_of(note), att)
            old = state_tone[vi]
            if new == old:
                continue
            flush_wait()
            if new is None:
                out.append(0xC0 | (vi << 4) | 15)
            elif old is None or old[0] != new[0]:
                out += bytes([0x80 | (vi << 4) | new[1], new[0] & 0xFF, new[0] >> 8])
            else:
                out.append(0xC0 | (vi << 4) | new[1])
            state_tone[vi] = new
        if drum_hits:
            flush_wait()
            ch = sorted(drum_hits, key=lambda c: (c != 11, c))[0]
            ctl, att0, rate = DRUMS[ch]
            out += bytes([0xB0 | ctl, (att0 << 4) | rate])
        wait += 1
    flush_wait()
    out.append(0xFF)
    return bytes(out)


def convert_all(root: Path, bw):
    rows, names = [], []
    for nm in SONGS:
        blob = convert(root / f'{nm.upper()}.DDT')
        sym = f'song_{nm}'
        bank = bw.add(sym, blob, None)
        rows.append((sym, bank, len(blob)))
        names.append(nm)
    return rows, names


if __name__ == '__main__':
    import sys
    root = Path(sys.argv[1])
    for nm in SONGS:
        b = convert(root / f'{nm.upper()}.DDT')
        print(nm, len(b))
