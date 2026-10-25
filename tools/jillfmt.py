#!/usr/bin/env python3
"""Readers for the original Jill of the Jungle data files (Epic MegaGames).

Formats: JILL.DMA (tile table), JILL1.SHA (shape sets), *.JN1/*.DEM (maps),
JILL1.VCL (sounds + text), *.DDT (CMF music), *.MAC (input macros) and the
6-bit VGA palette stored in JILL.EXE.  See ModdingWiki for the layouts.
"""
from __future__ import annotations

import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple

MAP_W, MAP_H = 128, 64
OBJ_SIZE = 31
SAVE_SIZE = 70
PALETTE_OFFSET = 150116  # in JILL.EXE (verified: index 0 black, 1-15 = EGA set)

# DMA / object flag bits
F_PLAYERTHRU, F_STAIR, F_VINE, F_MSGTOUCH, F_MSGDRAW, F_MSGUPDATE = 1, 2, 4, 8, 16, 32
F_INSIDE, F_FRONT, F_TRIGGER, F_BACK, F_ALWAYS, F_KILLABLE = 0x40, 0x80, 0x100, 0x200, 0x400, 0x800
F_FIREBALL, F_WATER, F_WEAPON = 0x1000, 0x2000, 0x4000


def load_palette(exe: Path) -> List[Tuple[int, int, int]]:
    d = exe.read_bytes()
    pal = [tuple(d[PALETTE_OFFSET + i * 3: PALETTE_OFFSET + i * 3 + 3]) for i in range(256)]
    if pal[0] != (0, 0, 0) or max(max(p) for p in pal) > 63 or pal[4] != (42, 0, 0):
        raise ValueError("JILL.EXE palette not found at the expected offset (different version?)")
    return pal


@dataclass
class DMAEntry:
    code: int
    tile: int
    tileset: int
    flags: int
    name: str


def parse_dma(path: Path) -> Dict[int, DMAEntry]:
    d = path.read_bytes()
    off, out = 0, {}
    while off + 7 <= len(d):
        code, tile, ts, flags, nl = struct.unpack_from('<HBBHB', d, off)
        off += 7
        out[code] = DMAEntry(code, tile, ts & 0x3F, flags, d[off:off + nl].decode('latin1'))
        off += nl
    if off != len(d):
        raise ValueError("bad DMA file")
    return out


@dataclass
class Shape:
    w: int
    h: int
    raw: bytes          # file pixel values
    vga: bytes          # palette index after the colour map (== raw for fonts / 8-bit)
    opaque0: bool       # True when raw value 0 is a real colour (fonts, 8-bit sets)


@dataclass
class ShapeSet:
    index: int
    bits: int
    flags: int
    shapes: List[Shape]


def parse_sha(path: Path) -> Dict[int, ShapeSet]:
    d = path.read_bytes()
    offs = struct.unpack_from('<128I', d, 0)
    sizes = struct.unpack_from('<128H', d, 512)
    sets: Dict[int, ShapeSet] = {}
    for si, (off, size) in enumerate(zip(offs, sizes)):
        if not off and not size:
            continue
        n = d[off]
        bits = d[off + 9]
        flags = struct.unpack_from('<H', d, off + 10)[0]
        p = off + 12
        cmap = None
        if not (flags & 1) and bits != 8:
            cmap = d[p:p + (1 << bits) * 4]
            p += (1 << bits) * 4
        shapes = []
        for _ in range(n):
            w, h, typ = d[p], d[p + 1], d[p + 2]
            p += 3
            if typ != 0:
                raise ValueError("unsupported SHA tile type")
            raw = d[p:p + w * h]
            p += w * h
            if cmap is not None:
                m = (1 << bits) - 1
                vga = bytes(cmap[(v & m) * 4 + 2] for v in raw)
            else:
                vga = bytes(raw)
            shapes.append(Shape(w, h, bytes(raw), vga, cmap is None))
        sets[si] = ShapeSet(si, bits, flags, shapes)
    return sets


@dataclass
class JNObject:
    i: int
    typ: int
    x: int
    y: int
    xd: int
    yd: int
    w: int
    h: int
    state: int
    substate: int
    statecount: int
    counter: int
    flags: int
    pointer: int
    info1: int
    zaphold: int
    string: str = ""


@dataclass
class JNMap:
    path: Path
    codes: List[int]
    objects: List[JNObject]
    strings: List[str]
    level_no: int
    health: int


def parse_jn(path: Path) -> JNMap:
    d = path.read_bytes()
    n = MAP_W * MAP_H
    codes = [0] * n
    for x in range(MAP_W):
        for y in range(MAP_H):
            codes[y * MAP_W + x] = struct.unpack_from('<H', d, (x * MAP_H + y) * 2)[0] & 0x3FFF
    off = n * 2
    nobj = struct.unpack_from('<H', d, off)[0]
    off += 2
    objs, ptrs = [], []
    for i in range(nobj):
        t = d[off]
        v = struct.unpack_from('<HHhhHHhhhhHIhh', d, off + 1)
        o = JNObject(i, t, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10], v[11], v[12], v[13])
        objs.append(o)
        if o.pointer:
            ptrs.append(i)
        off += OBJ_SIZE
    level_no, health = struct.unpack_from('<hh', d, off)
    off += SAVE_SIZE
    strings = []
    while off + 2 <= len(d):
        ln = struct.unpack_from('<H', d, off)[0]
        off += 2
        if ln > 4096 or off + ln > len(d):
            break
        strings.append(d[off:off + ln].decode('latin1'))
        off += ln
        if off < len(d) and d[off] == 0:
            off += 1
    for oi, s in zip(ptrs, strings):
        objs[oi].string = s
    return JNMap(path, codes, objs, strings, level_no, health)


@dataclass
class VCL:
    sounds: Dict[int, Tuple[bytes, int]]   # index -> (unsigned 8-bit pcm, rate)
    texts: Dict[int, str]


def parse_vcl(path: Path) -> VCL:
    d = path.read_bytes()
    so = struct.unpack_from('<50I', d, 0)
    sl = struct.unpack_from('<50H', d, 200)
    sf = struct.unpack_from('<50H', d, 300)
    to = struct.unpack_from('<40I', d, 400)
    tl = struct.unpack_from('<40H', d, 560)
    snd = {i: (d[so[i]:so[i] + sl[i]], sf[i]) for i in range(50) if sl[i]}
    txt = {i: d[to[i]:to[i] + tl[i]].decode('latin1') for i in range(40) if tl[i]}
    return VCL(snd, txt)


def parse_mac(path: Path) -> List[Tuple[int, Dict[str, int]]]:
    """Returns [(frame, {input: value})] — x/y are -1/0/1, buttons 0/1."""
    d = path.read_bytes()
    p, frame, ev = 0, 0, []
    while p < len(d):
        fl = d[p]
        p += 1
        vals = {}
        for bit, name in ((1, 'x'), (2, 'y'), (4, 'b1'), (8, 'b2'), (16, 'key')):
            if fl & bit:
                v = d[p]
                p += 1
                if name in ('x', 'y') and v == 255:
                    v = -1
                vals[name] = v
        ev.append((frame, vals))
        if p >= len(d):
            break
        t = d[p]
        p += 1
        if t & 0x80:
            t = (t & 0x7F) | (d[p] << 7)
            p += 1
        frame = t
    return ev
