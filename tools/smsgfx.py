#!/usr/bin/env python3
"""Colour quantisation and tile packing helpers for the Jill SMS port.

Everything here works from the original 256-colour VGA palette (6-bit RGB
read from JILL.EXE) and reduces it to the Master System's 64 colours
(2 bits per channel), choosing the best 16 per palette.
"""
from __future__ import annotations

import itertools
import math
from typing import Dict, Iterable, List, Sequence, Tuple

# SMS channel level n (0..3) is displayed as n*85 on an 8-bit scale.
SMS_RGB = [((c & 3) * 85, ((c >> 2) & 3) * 85, ((c >> 4) & 3) * 85) for c in range(64)]


def vga_to_rgb8(c6: Tuple[int, int, int]) -> Tuple[int, int, int]:
    return tuple(min(255, v * 255 // 63) for v in c6)


def _lab(rgb: Tuple[int, int, int]) -> Tuple[float, float, float]:
    def lin(v: float) -> float:
        v /= 255.0
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
    r, g, b = (lin(v) for v in rgb)
    x = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047
    y = 0.2126 * r + 0.7152 * g + 0.0722 * b
    z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883

    def f(t: float) -> float:
        return t ** (1 / 3) if t > 0.008856 else 7.787 * t + 16 / 116
    fx, fy, fz = f(x), f(y), f(z)
    return (116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz))


_SMS_LAB = [_lab(c) for c in SMS_RGB]
_lab_cache: Dict[Tuple[int, int, int], Tuple[float, float, float]] = {}


def lab_of(rgb: Tuple[int, int, int]):
    v = _lab_cache.get(rgb)
    if v is None:
        v = _lab_cache[rgb] = _lab(rgb)
    return v


def _d2(a, b) -> float:
    # Luma is weighted up: the eye notices brightness errors most on 16 colours.
    return 1.6 * (a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 + (a[2] - b[2]) ** 2


def choose_palette(weights: Dict[Tuple[int, int, int], float], n: int,
                   forced: Sequence[int] = ()) -> List[int]:
    """Pick `n` SMS colours (0..63) minimising weighted error against the
    weighted VGA colours.  `forced` colours are always included."""
    items = [(lab_of(vga_to_rgb8(c)), w) for c, w in weights.items() if w > 0]
    if not items:
        return list(forced) + [0] * (n - len(forced))
    # error table: cost[i][s] = w * dist(item i, sms colour s)
    cost = [[w * _d2(l, _SMS_LAB[s]) for s in range(64)] for l, w in items]
    chosen: List[int] = list(forced)

    def total(sel: Sequence[int]) -> float:
        return sum(min(row[s] for s in sel) for row in cost)

    while len(chosen) < n:
        best, bs = None, None
        for s in range(64):
            if s in chosen:
                continue
            t = total(chosen + [s]) if chosen else sum(row[s] for row in cost)
            if best is None or t < best:
                best, bs = t, s
        chosen.append(bs)
    # local search: swap one colour at a time while it helps
    improved = True
    cur = total(chosen)
    while improved:
        improved = False
        for i in range(len(forced), n):
            for s in range(64):
                if s in chosen:
                    continue
                trial = chosen[:i] + [s] + chosen[i + 1:]
                t = total(trial)
                if t + 1e-9 < cur:
                    chosen, cur, improved = trial, t, True
    return chosen


def nearest_index(vga6: Tuple[int, int, int], palette: Sequence[int], skip0: bool = False) -> int:
    lab = lab_of(vga_to_rgb8(vga6))
    best, bi = None, 0
    for i, s in enumerate(palette):
        if skip0 and i == 0:
            continue
        d = _d2(lab, _SMS_LAB[s])
        if best is None or d < best:
            best, bi = d, i
    return bi


class Remap:
    """Memoised VGA index -> palette slot mapper."""

    def __init__(self, vga_pal: Sequence[Tuple[int, int, int]], sms_pal: Sequence[int], skip0: bool = False):
        self.vga = vga_pal
        self.sms = list(sms_pal)
        self.skip0 = skip0
        self.cache: Dict[int, int] = {}

    def __call__(self, vga_index: int) -> int:
        v = self.cache.get(vga_index)
        if v is None:
            v = self.cache[vga_index] = nearest_index(self.vga[vga_index], self.sms, self.skip0)
        return v


def planar_tile(px: Sequence[int]) -> bytes:
    """8x8 array of 0..15 -> 32-byte SMS planar tile."""
    out = bytearray()
    for y in range(8):
        b = [0, 0, 0, 0]
        for x in range(8):
            c = px[y * 8 + x]
            bit = 0x80 >> x
            for p in range(4):
                if c & (1 << p):
                    b[p] |= bit
        out.extend(b)
    return bytes(out)


def flip_h(px: Sequence[int]) -> List[int]:
    return [px[y * 8 + 7 - x] for y in range(8) for x in range(8)]


def flip_v(px: Sequence[int]) -> List[int]:
    return [px[(7 - y) * 8 + x] for y in range(8) for x in range(8)]


class TilePool:
    """Deduplicating tile store (with H/V flip matching) for background tiles."""

    TILE_FLIP_X = 0x0200
    TILE_FLIP_Y = 0x0400

    def __init__(self):
        self.tiles: List[bytes] = []
        self.index: Dict[bytes, int] = {}

    def add(self, px: Sequence[int], allow_flip: bool = True) -> int:
        """Returns tile index with flip flags OR'd in (name-table word bits)."""
        px = list(px)
        keys = [(px, 0)]
        if allow_flip:
            hx = flip_h(px)
            vy = flip_v(px)
            keys += [(hx, self.TILE_FLIP_X), (vy, self.TILE_FLIP_Y), (flip_v(hx), self.TILE_FLIP_X | self.TILE_FLIP_Y)]
        for p, flags in keys:
            t = planar_tile(p)
            if t in self.index:
                return self.index[t] | flags
        t = planar_tile(px)
        idx = len(self.tiles)
        self.tiles.append(t)
        self.index[t] = idx
        return idx

    def add_unique(self, px: Sequence[int]) -> int:
        """Always allocate a private tile (used for animated tiles)."""
        t = planar_tile(px)
        idx = len(self.tiles)
        self.tiles.append(t)
        return idx

    def blob(self) -> bytes:
        return b''.join(self.tiles)
