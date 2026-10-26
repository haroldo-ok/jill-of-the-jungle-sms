#!/usr/bin/env python3
"""Convert the original Jill of the Jungle (Episode 1) data to Master System data.

Reads (from $JILL_DATA): JILL.EXE (VGA palette), JILL.DMA, JILL1.SHA, *.JN1,
JILL1.VCL, *.DDT, *.MAC.  Writes C sources / headers into gen/.

All graphics are derived from the original *VGA* palette: every level gets its
own optimised 16-colour background palette and 16-colour sprite palette
chosen from the 64 SMS colours (see smsgfx.choose_palette).
"""
from __future__ import annotations

import os
import struct
import sys
from collections import Counter, OrderedDict
from pathlib import Path
from typing import Dict, List, Sequence, Tuple

sys.path.insert(0, str(Path(__file__).resolve().parent))
import jillfmt as F
import smsgfx as G

ROOT = Path(os.environ.get('JILL_DATA', '/home/user/jill-source/ep1'))
OUT = Path(__file__).resolve().parents[1] / 'gen'

import convert_music as _CM
SONG_NAMES = _CM.SONGS
MACROS = ['end1.mac']
# text object colour (object xd) -> SMS colour (r | g<<2 | b<<4)
TEXT_COL = {1: 0x0F, 2: 0x1D, 3: 0x3D, 4: 0x17, 5: 0x37, 6: 0x06, 7: 0x3F}
LEVELS = ['MAP', '1', '2', '3', '4', '6', '9', '50', 'D0', 'D1', 'D2']
ARROW_FIX = [
    ('USE THE ARROW KEYS TO', 'USE THE D-PAD TO'),
    ('2MOVE BY USING THE ARROW\r\n2KEYS OR JOYSTICK,  JUMP BY\r\n2PRESSING <SHIFT>,  AND USE\r\n2WEAPONS BY PRESSING <ALT>.',
     '2MOVE BY USING THE D-PAD.\r\n2JUMP BY PRESSING BUTTON 1,\r\n2AND USE WEAPONS BY\r\n2PRESSING BUTTON 2.'),
    ('2YOU CAN PRESS "S" TO SAVE\r\n2THE GAME AT ANY TIME.  THEN\r\n2IF YOU DIE LATER,  YOU CAN',
     '2YOU CAN SAVE THE GAME FROM\r\n2THE PAUSE MENU AT ANY TIME.\r\n2THEN IF YOU DIE LATER,  YOU CAN'),
    ('You can press F1 at any\r\ntime to get help.', 'You can press PAUSE at any\r\ntime to get help.'),
    ('(Press "S" to save)', '(PAUSE menu: Save game)'),
    ('by pressing "S".', 'from the PAUSE menu.'),
    ('ALT or the fire button on\r\nyour joystick.', 'button 2.'),
    ('down to half-speed. Just\r\n2press "T".', 'down to half-speed. Turn\r\n2it on in the PAUSE menu.'),
]
LEVEL_FILE = {'D0': '0.DEM', 'D1': '1.DEM', 'D2': '2.DEM'}      # shareware demo worlds


def level_path(n):
    return ROOT / LEVEL_FILE.get(n, f'{n}.JN1')

# ---- metatile flag bits (runtime) -----------------------------------------
MF_SOLID, MF_PLAT, MF_VINE, MF_WATER, MF_HURT, MF_KILL, MF_MARK = 1, 2, 4, 8, 0x10, 0x20, 0x40

# ---- VRAM plan (tile slots) ------------------------------------------------
T_JILL = 0           # 8 tiles, streamed
T_HUD = 8            # 12 tiles: HUD drawn as six 8x16 sprites
HUD_TILES = 12
T_SPR0 = 20          # dynamic sprite-frame cache (pool) starts here
T_LIMIT = 448        # 0x3800 is the name table
POOL_MAX = 148       # tiles of the dynamic sprite-frame cache (multiple of 4)

# ---- sprite strips: name -> (anchor, box_w, box_h, [(set, frame), ...]) ----
S = OrderedDict()


def strip(name, anchor, bw, bh, frames):
    S[name] = (anchor, bw, bh, frames)


strip('APPLE', 'c', 12, 12, [(9, i) for i in range(4)])
strip('KEY', 'c', 16, 8, [(14, i) for i in (6, 7, 8, 9)])
strip('CRYSTAL', 'c', 16, 16, [(14, 10)])
strip('KNIFEI', 'c', 10, 10, [(13, 0)])
strip('TOK0', 'c', 16, 16, [(14, 38)])
strip('TOK4', 'c', 16, 16, [(14, 14)])
strip('TOK5', 'c', 16, 16, [(14, 15)])
strip('TOK7', 'c', 16, 16, [(14, 20)])
strip('TOK8', 'c', 16, 16, [(45, 0)])
strip('TOK9', 'c', 16, 16, [(14, 36)])
strip('KNIFE', 'c', 10, 10, [(13, i) for i in range(5)])
strip('BLADE', 'c', 16, 16, [(45, i) for i in range(4)])
strip('FIREBALL', 'c', 16, 16, [(26, i) for i in range(4)])
strip('FISHSHOT', 'c', 16, 6, [(51, 21), (51, 22)])
strip('ANT', 'f', 32, 16, [(10, i) for i in range(10)])
strip('PHOENIX', 'c', 16, 16, [(11, i) for i in range(12)])
strip('FROG', 'f', 14, 10, [(63, i) for i in range(6)])
strip('SLUG', 'f', 24, 24, [(52, i) for i in range(7)])
strip('CRAB', 'f', 16, 16, [(38, i) for i in range(4)])
strip('GATOR', 'f', 64, 8, [(39, i) for i in range(16)])
strip('SNAKE', 'f', 16, 16, [(15, i) for i in (9, 10, 11, 0, 1, 2, 3, 4, 5, 6, 7, 8)])
strip('PAC', 'c', 16, 16, [(50, i) for i in range(4)])
strip('JELLY', 'c', 16, 24, [(51, i) for i in range(9, 15)])
strip('EEL', 'c', 28, 16, [(51, i) for i in range(15, 21)])
strip('SPARK', 'c', 16, 8, [(47, i) for i in range(3, 8)])
strip('EYE', 'c', 16, 12, [(62, i) for i in range(5)])
strip('BOULDER', 'c', 16, 16, [(14, i) for i in range(24, 28)])
strip('STALAG', 'c', 16, 16, [(14, i) for i in range(28, 33)])
strip('ROCK', 'f', 16, 16, [(14, 34)])
strip('SKULL', 'c', 22, 26, [(47, i) for i in range(3)])
strip('KNIGHT', 'f', 32, 32, [(36, i) for i in range(5)])
strip('MEGA', 'f', 20, 24, [(33, i) for i in range(5)])
strip('EPIC', 'f', 32, 16, [(40, i) for i in range(8)])
strip('FLAG', 'f', 36, 16, [(5, i) for i in range(4)])
strip('ELEV', 'c', 16, 16, [(14, 44)])
strip('POLE', 'c', 16, 16, [(41, 22)])        # ELEVMID shaft piece, drawn under a moving lift
strip('SWITCH', 'c', 16, 16, [(60, 0), (60, 1)])
strip('NEST', 'c', 16, 16, [(37, i) for i in range(4)])
strip('BEES', 'c', 16, 16, [(37, i) for i in range(4, 10)])
strip('MIST', 'c', 16, 16, [(55, i) for i in range(4)])
strip('BUBBLE', 'c', 8, 8, [(51, i) for i in (6, 7, 8)])
strip('STAR', 'c', 8, 8, [(46, i) for i in range(15)])
strip('FROGP', 'f', 16, 8, [(22, i) for i in (0, 1, 3, 4)])
strip('FISHP', 'c', 24, 16, [(51, i) for i in range(6)])

# object type -> strips needed in that level
TYPE_STRIPS = {
    1: ['APPLE'], 2: ['KNIFEI'], 14: ['KEY'], 33: ['CRYSTAL'], 11: ['SLUG'], 22: ['FROG'],
    29: ['ANT'], 30: ['PHOENIX'], 35: ['BOULDER'], 38: ['STALAG'], 39: ['SNAKE'], 40: ['ROCK'],
    42: ['MEGA'], 44: ['KNIGHT'], 45: ['NEST', 'BEES'], 47: ['CRAB'], 48: ['GATOR'], 49: ['EPIC'],
    51: ['SKULL', 'SPARK'], 53: ['PAC'], 59: ['JELLY'], 60: ['EEL'], 61: ['ELEV', 'POLE'], 32: ['SWITCH'],
    64: ['EYE'], 65: ['SPARK'], 66: ['FLAG'], 19: ['MIST'],
}
TOKEN_STRIP = {0: 'TOK0', 4: 'TOK4', 5: 'TOK5', 7: 'TOK7', 8: 'TOK8', 9: 'TOK9'}
ALWAYS_STRIPS = ['KNIFE', 'STAR']

# animation sequences: name -> (count, left frames, right frames)  (indices inside the strip)
ANIMS = OrderedDict([
    ('APPLE', (4, [0, 1, 2, 3], [0, 1, 2, 3])),
    ('KEY', (4, [0, 1, 2, 3], [0, 1, 2, 3])),
    ('ANT', (4, [0, 1, 2, 3], [5, 6, 7, 8])),
    ('PHOENIX', (4, [0, 1, 2, 3], [4, 5, 6, 7])),
    ('FROG', (3, [0, 1, 2], [3, 4, 5])),
    ('SLUG', (4, [0, 1, 2, 1], [6, 5, 4, 5])),
    ('CRAB', (4, [0, 1, 2, 3], [0, 1, 2, 3])),
    ('GATOR', (4, [12, 13, 14, 15], [4, 5, 6, 7])),
    ('SNAKE', (3, [0, 1, 2], [3, 4, 5])),
    ('PAC', (4, [0, 1, 2, 3], [0, 1, 2, 3])),
    ('JELLY', (6, [0, 1, 2, 3, 4, 5], [0, 1, 2, 3, 4, 5])),
    ('EEL', (3, [0, 1, 2], [3, 4, 5])),
    ('SPARK', (5, [0, 1, 2, 3, 4], [0, 1, 2, 3, 4])),
    ('BOULDER', (4, [0, 1, 2, 3], [3, 2, 1, 0])),
    ('SKULL', (3, [0, 1, 2], [0, 1, 2])),
    ('KNIGHT', (2, [0, 1], [0, 1])),
    ('EPIC', (8, [0, 1, 2, 3, 4, 5, 6, 7], [0, 1, 2, 3, 4, 5, 6, 7])),
    ('FLAG', (4, [0, 1, 2, 3], [0, 1, 2, 3])),
    ('BEES', (3, [0, 1, 2], [3, 4, 5])),
    ('NEST', (4, [0, 1, 2, 3], [0, 1, 2, 3])),
    ('MIST', (4, [0, 1, 2, 3], [0, 1, 2, 3])),
    ('FISHP', (3, [0, 1, 2], [3, 4, 5])),
    ('FROGP', (2, [2, 3], [0, 1])),
    ('BLADE', (4, [0, 1, 2, 3], [3, 2, 1, 0])),
    ('FIREBALL', (4, [0, 1, 2, 3], [0, 1, 2, 3])),
    ('KNIFE', (4, [1, 2, 3, 4], [1, 2, 3, 4])),
    ('STAR', (3, [0, 1, 2], [0, 1, 2])),
    ('MEGA', (1, [0], [0])),
])

# Jill: which frames of set 8 are real 16x32 frames
JILL_SET = 8


def hexpal(sel):
    return [((s & 3) | (((s >> 2) & 3) << 2) | (((s >> 4) & 3) << 4)) for s in sel]


class BankWriter:
    """Packs byte blobs into 16 KB banks (banks 2.. are data banks)."""

    def __init__(self, first=2):
        self.banks: List[List[Tuple[str, bytes]]] = []
        self.sizes: List[int] = []
        self.first = first
        self.where: Dict[str, int] = {}

    def add(self, name: str, data: bytes, bank: int | None = None, kind='u8') -> int:
        if len(data) > 16000:
            raise ValueError(f'{name} too big for one bank ({len(data)})')
        if bank is None:
            for i, sz in enumerate(self.sizes):
                if sz + len(data) <= 16000:
                    bank = i
                    break
            else:
                self.banks.append([])
                self.sizes.append(0)
                bank = len(self.banks) - 1
        while bank >= len(self.banks):
            self.banks.append([])
            self.sizes.append(0)
        self.banks[bank].append((name, data, kind))
        self.sizes[bank] += len(data)
        self.where[name] = bank + self.first
        return bank + self.first

    def add_c(self, name: str, text: str, size: int, bank: int | None = None) -> int:
        if bank is None:
            for i, sz in enumerate(self.sizes):
                if sz + size <= 16000:
                    bank = i
                    break
            else:
                self.banks.append([])
                self.sizes.append(0)
                bank = len(self.banks) - 1
        while bank >= len(self.banks):
            self.banks.append([])
            self.sizes.append(0)
        self.banks[bank].append((name, text, 'c'))
        self.sizes[bank] += size
        self.where[name] = bank + self.first
        return bank + self.first

    def find(self, size: int) -> int:
        """index of a bank with at least `size` bytes free (best fit), or a new one"""
        best = None
        for i, sz in enumerate(self.sizes):
            if sz + size <= 16000 and (best is None or sz > self.sizes[best]):
                best = i
        return best if best is not None else self.new_bank()

    def new_bank(self) -> int:
        self.banks.append([])
        self.sizes.append(0)
        return len(self.banks) - 1


def c_bytes(name: str, data: bytes, kind: str = 'u8') -> str:
    if kind == 'c':
        return data
    if kind == 'u16':
        vals = struct.unpack('<%dH' % (len(data) // 2), data) if data else (0,)
        lines = [f'const unsigned int {name}[] = {{']
        for i in range(0, len(vals), 12):
            lines.append('  ' + ','.join(str(v) for v in vals[i:i + 12]) + ',')
        lines.append('};')
        return '\n'.join(lines)
    lines = [f'const unsigned char {name}[] = {{']
    data = data or b'\0'
    for i in range(0, len(data), 24):
        lines.append('  ' + ','.join('0x%02X' % b for b in data[i:i + 24]) + ',')
    lines.append('};')
    return '\n'.join(lines)


# ---------------------------------------------------------------------------
# tile helpers
# ---------------------------------------------------------------------------

def shape_grid(shape: F.Shape, remap, w=16, h=16, transparent_zero=False, pad=0):
    grid = [[pad] * w for _ in range(h)]
    for y in range(min(h, shape.h)):
        for x in range(min(w, shape.w)):
            i = y * shape.w + x
            if transparent_zero and shape.raw[i] == 0 and not shape.opaque0:
                grid[y][x] = 0
            else:
                grid[y][x] = remap(shape.vga[i])
    return grid


def sprite_blob(shape: F.Shape, remap) -> Tuple[bytes, int, int]:
    """Tall-sprite (8x16) layout: per 8px column, per 16px band: 2 tiles."""
    cols = (shape.w + 7) // 8
    bands = (shape.h + 15) // 16
    grid = shape_grid(shape, remap, cols * 8, bands * 16, transparent_zero=True)
    out = bytearray()
    for c in range(cols):
        for b in range(bands):
            for half in range(2):
                px = [grid[b * 16 + half * 8 + y][c * 8 + x] for y in range(8) for x in range(8)]
                out += G.planar_tile(px)
    return bytes(out), cols, bands


def gather_weights(shapes: Sequence[F.Shape], pal, weight=1.0, transparent_zero=True, acc=None):
    acc = acc if acc is not None else Counter()
    for s in shapes:
        for i, v in enumerate(s.vga):
            if transparent_zero and s.raw[i] == 0 and not s.opaque0:
                continue
            acc[pal[v]] += weight
    return acc


# ---------------------------------------------------------------------------
class LevelBuild:
    pass


def classify_tile(e: F.DMAEntry) -> int:
    f = e.flags
    fl = 0
    if not (f & F.F_PLAYERTHRU):
        fl |= MF_SOLID
    if f & F.F_STAIR:
        fl |= MF_PLAT
        fl &= ~MF_SOLID
    if f & F.F_VINE:
        fl |= MF_VINE
    if f & F.F_WATER:
        fl |= MF_WATER
    n = e.name
    if n.startswith('LAVA') and n != 'LAVAB':
        fl |= MF_KILL
    elif (f & F.F_MSGTOUCH) and not (f & F.F_WATER) and (n.startswith('SPIKE') or n.startswith('WTHORN') or
                                                         n.startswith('STALAC') or n.startswith('STALAG')):
        fl |= MF_HURT
    return fl


def find_markers(m: F.JNMap, dma) -> set:
    """Isolated 'CASTLE' / arrow blocks floating in open space are monster
    turn-around markers (invisible and non-solid for the player)."""
    MARK_NAMES = {'CASTLE', 'CASAR', 'CASAL', 'CASAU', 'CASAD', 'GGG'}
    cand = {(x, y) for y in range(F.MAP_H) for x in range(F.MAP_W)
            if dma[m.codes[y * F.MAP_W + x]].name in MARK_NAMES}
    seen, marks = set(), set()
    for c in sorted(cand):
        if c in seen:
            continue
        comp, stack = [], [c]
        seen.add(c)
        while stack:
            x, y = stack.pop()
            comp.append((x, y))
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                n = (x + dx, y + dy)
                if n in cand and n not in seen:
                    seen.add(n)
                    stack.append(n)
        if len(comp) > 6:
            continue
        ok = True
        for x, y in comp:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if (nx, ny) in comp or not (0 <= nx < F.MAP_W and 0 <= ny < F.MAP_H):
                    continue
                e = dma[m.codes[ny * F.MAP_W + nx]]
                if not (e.flags & F.F_PLAYERTHRU):
                    ok = False
        if ok:
            marks.update(comp)
    return marks


def passable_neighbour(m, dma, x, y):
    best = None
    for dx, dy in ((1, 0), (-1, 0), (0, -1), (0, 1), (1, 1), (-1, -1), (1, -1), (-1, 1)):
        nx, ny = x + dx, y + dy
        if 0 <= nx < F.MAP_W and 0 <= ny < F.MAP_H:
            c = m.codes[ny * F.MAP_W + nx]
            e = dma[c]
            if (e.flags & F.F_PLAYERTHRU) and not (e.flags & F.F_VINE) and not (e.flags & F.F_WATER) \
                    and not (e.flags & F.F_STAIR):
                return c
    return 0


def main():
    OUT.mkdir(exist_ok=True)
    pal = F.load_palette(ROOT / 'JILL.EXE')
    dma = F.parse_dma(ROOT / 'JILL.DMA')
    sha = F.parse_sha(ROOT / 'JILL1.SHA')
    vcl = F.parse_vcl(ROOT / 'JILL1.VCL')
    maps = [F.parse_jn(level_path(n)) for n in LEVELS]
    lvl_id = {f'{n}.jn1'.lower(): i for i, n in enumerate(LEVELS)}

    # ---- inventory of strips (global ids) ----------------------------------
    strip_names = list(S.keys())
    strip_base: Dict[str, int] = {}
    sid = 0
    for n in strip_names:
        strip_base[n] = sid
        sid += len(S[n][3])
    n_spr_ids = sid

    bw = BankWriter(first=2)
    levels_c: List[str] = []
    stats: List[str] = []
    jill_shapes = sha[JILL_SET].shapes
    jill_frames = [i for i, s in enumerate(jill_shapes) if s.w == 16 and s.h == 32]

    # Jill's own 8 colours are shared by every level (sprite palette slots 1..8)
    jw = gather_weights([jill_shapes[i] for i in jill_frames], pal, 1.0, acc=Counter())
    JILL_COLS = G.choose_palette(jw, 8, forced=[0x00])[1:] if False else G.choose_palette(jw, 8)
    jill_rm = G.Remap(pal, [0x00] + JILL_COLS, skip0=True)
    jill_blob = bytearray()
    jill_slot = [0xFF] * len(jill_shapes)
    for n_, fi in enumerate(jill_frames):
        blob, cols, bands = sprite_blob(jill_shapes[fi], jill_rm)
        jill_slot[fi] = n_
        jill_blob += blob
    jill_bank = bw.add('jill_gfx', bytes(jill_blob), None)

    # fonts ---------------------------------------------------------------
    font = sha[1].shapes
    font1bpp = bytearray()
    for ch in range(32, 128):
        s = font[ch]
        for y in range(8):
            b = 0
            for x in range(8):
                if s.raw[y * 8 + x] in (1, 2):
                    b |= 0x80 >> x
            font1bpp.append(b)

    desc_rows = []
    for li, (name, m) in enumerate(zip(LEVELS, maps)):
        L = LevelBuild()
        marks = find_markers(m, dma)
        # -- objects pre-pass: alt tiles for doors/bridgers/falldoors ----------
        used_codes = set(m.codes)
        alt_code: Dict[int, int] = {}
        for o in m.objects:
            cx, cy = o.x // 16, o.y // 16
            if o.typ == 24:               # door: 1 or 2 cells high
                for dy in range(0, (o.h + 15) // 16):
                    alt_code[(cy + dy) * 128 + cx] = passable_neighbour(m, dma, cx, cy + dy)
            elif o.typ == 25:
                alt_code[cy * 128 + cx] = passable_neighbour(m, dma, cx, cy)
            elif o.typ == 26:
                here = m.codes[cy * 128 + cx]
                e = dma[here]
                if e.flags & F.F_PLAYERTHRU and not (e.flags & F.F_STAIR):
                    # starts open: closing creates a bridge (horizontal) or wall
                    alt_code[cy * 128 + cx] = 432 if o.xd else 140
                else:
                    alt_code[cy * 128 + cx] = passable_neighbour(m, dma, cx, cy)
        # marker cells -> invisible twin of neighbour tile
        cell_code = list(m.codes)
        mark_cells = {}
        for (x, y) in marks:
            r = passable_neighbour(m, dma, x, y)
            mark_cells[y * 128 + x] = r
        # metatile table: code (+mark twin flag)
        mt_keys: List[Tuple[int, int]] = []     # (code, marker)
        mt_index: Dict[Tuple[int, int], int] = {}

        def mt_of(code: int, marker: int = 0) -> int:
            k = (code, marker)
            if k not in mt_index:
                mt_index[k] = len(mt_keys)
                mt_keys.append(k)
            return mt_index[k]

        map_bytes = bytearray()
        for i, c in enumerate(cell_code):
            if i in mark_cells:
                map_bytes.append(mt_of(mark_cells[i], 1))
            else:
                map_bytes.append(mt_of(c, 0))
        # alt metatiles
        obj_alt: Dict[int, int] = {}
        for k, c in alt_code.items():
            obj_alt[k] = mt_of(c, 0)
        # lift shaft metatile (ELEVMID, code 138) - lifts build / remove it as they move
        L.elev_mt = mt_of(138, 0) if any(o.typ == 61 for o in m.objects) else 255
        # Resolve mt ids for BRIDGER 'other state'
        if len(mt_keys) > 255:
            raise ValueError('too many metatiles')

        # -- colours for BG ---------------------------------------------------
        w = Counter()
        for (code, marker) in mt_keys:
            e = dma[code]
            s = sha[e.tileset].shapes[e.tile]
            cnt = 1 + sum(1 for c in m.codes if c == code) * 1.0
            for v in s.vga:
                w[pal[v]] += cnt
        forced_bg: List[int] = []
        text_objs = [o for o in m.objects if o.typ in (20, 21)]
        for xdv in sorted({o.xd for o in text_objs}):
            c_ = TEXT_COL.get(xdv, TEXT_COL[7])
            if c_ not in forced_bg:
                forced_bg.append(c_)
        bg_sel = G.choose_palette(w, 16, forced=forced_bg)
        bg_rm = G.Remap(pal, bg_sel)

        # -- strips needed ----------------------------------------------------
        need: List[str] = list(ALWAYS_STRIPS)
        toks = set()
        for o in m.objects:
            for sname in TYPE_STRIPS.get(o.typ, []):
                if sname not in need:
                    need.append(sname)
            if o.typ == 28:
                toks.add(o.counter)
        for t in sorted(toks):
            if t in TOKEN_STRIP and TOKEN_STRIP[t] not in need:
                need.append(TOKEN_STRIP[t])
        # morph forms are needed when the matching token exists (or blade)
        if 4 in toks:
            need.append('FROGP')
        if 5 in toks:
            need.append('PHOENIX') if 'PHOENIX' not in need else None
            need.append('FIREBALL')
        if 7 in toks:
            need += ['FISHP', 'FISHSHOT', 'BUBBLE']
        if 8 in toks:
            need.append('BLADE')
        if 9 in toks or True:
            pass
        need = list(OrderedDict.fromkeys(need))

        # -- sprite palette ---------------------------------------------------
        sw = Counter()
        for sn in need:
            _, _, _, frs = S[sn]
            gather_weights([sha[a].shapes[b] for a, b in frs], pal, 1.0, acc=sw)
        spr_sel = G.choose_palette(sw, 16, forced=[0x00] + JILL_COLS)
        spr_rm = G.Remap(pal, spr_sel, skip0=True)
        L.hud = [G.nearest_index(c, spr_sel, skip0=True) for c in ((63, 63, 63), (63, 63, 0), (63, 8, 8), (30, 30, 30))]

        # -- BG tiles ---------------------------------------------------------
        pool = G.TilePool()
        mtdef_words: List[int] = []     # tile index relative to pool (flip bits included)
        mtflags = bytearray()
        mt_grids = []
        for (code, marker) in mt_keys:
            e = dma[code]
            s = sha[e.tileset].shapes[e.tile]
            grid = shape_grid(s, bg_rm, 16, 16, pad=0)
            mt_grids.append(grid)
            for ty in (0, 8):
                for tx in (0, 8):
                    px = [grid[ty + y][tx + x] for y in range(8) for x in range(8)]
                    mtdef_words.append(pool.add(px))
            fl = classify_tile(e)
            if marker:
                fl = MF_MARK
            mtflags.append(fl)

        # -- text objects baked into the background as patched 8x8 tiles ------------
        ink = {}
        for o in text_objs:
            font = sha[2 if o.typ == 20 else 1].shapes
            col = bg_rm.sms.index(TEXT_COL.get(o.xd, TEXT_COL[7]))
            x0 = o.x
            for ch in o.string:
                g = font[ord(ch) & 0x7F]
                for gy in range(g.h):
                    for gx in range(g.w):
                        if g.raw[gy * g.w + gx] in (1, 2):
                            ink[(x0 + gx, o.y + gy)] = col
                x0 += g.w
        cells: Dict[Tuple[int, int], List[int]] = {}
        for (x_, y_), col in ink.items():
            k = (x_ >> 3, y_ >> 3)
            if k not in cells:
                mtid = map_bytes[(k[1] >> 1) * 128 + (k[0] >> 1)]
                gr = mt_grids[mtid]
                ox_, oy_ = (k[0] & 1) * 8, (k[1] & 1) * 8
                cells[k] = [gr[oy_ + yy][ox_ + xx] for yy in range(8) for xx in range(8)]
            cells[k][(y_ & 7) * 8 + (x_ & 7)] = col
        # Dedup the text tiles; small sets go into the static pool, large ones are streamed
        # into a ring of VRAM "slots" chosen so that tiles that can share a name-table window
        # never share a slot.
        tmp = G.TilePool()

        def add2(px):                      # -> (index, flip flags); index may exceed 511
            px = list(px)
            hx, vy = G.flip_h(px), G.flip_v(px)
            for q_, fl in ((px, 0), (hx, 0x200), (vy, 0x400), (G.flip_v(hx), 0x600)):
                t_ = G.planar_tile(q_)
                if t_ in tmp.index:
                    return tmp.index[t_], fl
            t_ = G.planar_tile(px)
            tmp.index[t_] = len(tmp.tiles)
            tmp.tiles.append(t_)
            return len(tmp.tiles) - 1, 0
        cword = {k: add2(px) for k, px in cells.items()}
        stream = len(pool.tiles) + len(tmp.tiles) > 280
        pt_slot: List[int] = []
        n_slots = 0
        spans: List[Tuple[int, List[int]]] = []     # (key start, tile words)
        if stream:
            pos: Dict[int, List[Tuple[int, int]]] = {}
            for (tx_, ty_), wd in cword.items():
                pos.setdefault(wd[0], []).append((tx_, ty_))
            order = sorted(pos, key=lambda i: min(p_[0] for p_ in pos[i]))
            slot_of: Dict[int, int] = {}
            users: List[List[int]] = []          # slot -> tile ids
            for i in order:
                bad = set()
                for j, sl in slot_of.items():
                    if any(abs(a_[0] - b_[0]) <= 31 and abs(a_[1] - b_[1]) <= 27
                           for a_ in pos[i] for b_ in pos[j]):
                        bad.add(sl)
                sl = 0
                while sl in bad:
                    sl += 1
                slot_of[i] = sl
                if sl >= len(users):
                    users.append([])
                users[sl].append(i)
            n_slots = len(users)
            pt_slot = [slot_of.get(i, 0) for i in range(len(tmp.tiles))]
        for (tx_, ty_) in sorted(cells, key=lambda k: (k[1], k[0])):
            wd = cword[(tx_, ty_)]
            if stream:
                tw = 0x8000 | (0x4000 if wd[1] & 0x200 else 0) | (0x2000 if wd[1] & 0x400 else 0) | wd[0]
            else:
                tw = pool.add(cells[(tx_, ty_)])
            key = ty_ * 256 + tx_
            if spans and spans[-1][0] + len(spans[-1][1]) == key and tx_ != 0:
                spans[-1][1].append(tw)
            else:
                spans.append((key, [tw]))
        L.n_patch = len(cells)
        L.stream = stream
        L.n_slots = n_slots
        L.pt_slot = pt_slot
        L.pt_tiles = tmp.tiles if stream else []
        print(f'  {name}: text tiles {len(cells)} unique {len(tmp.tiles)} stream={stream} slots={n_slots}')

        # -- sprite frames (streamed into a VRAM cache at run time) -------------
        chunks: List[bytearray] = [bytearray()]
        frame_info = [None] * n_spr_ids       # (chunk, offset, xo, yo, cb)
        for sn in need:
            anchor, bwid, bhgt, frs = S[sn]
            for k, (a_, b_) in enumerate(frs):
                shp = sha[a_].shapes[b_]
                blob, cols, bands = sprite_blob(shp, spr_rm)
                if anchor == 'f':
                    xo, yo = (bwid - shp.w) // 2, bhgt - shp.h
                else:
                    xo, yo = (bwid - shp.w) // 2, (bhgt - shp.h) // 2
                if len(chunks[-1]) + len(blob) > 15000:
                    chunks.append(bytearray())
                frame_info[strip_base[sn] + k] = (len(chunks) - 1, len(chunks[-1]), xo & 0xFF, yo & 0xFF,
                                                  (cols << 4) | bands)
                chunks[-1] += blob
        n_spr_tiles = sum(len(c) for c in chunks) // 32
        # -- VRAM layout ---------------------------------------------------------
        n_bg = len(pool.tiles)
        total_free = T_LIMIT - T_SPR0
        if n_bg + L.n_slots > total_free - 24:
            raise ValueError(f'{name}: BG tiles {n_bg}+{L.n_slots} leave no room for sprites ({total_free})')
        pool_tiles = min(POOL_MAX, total_free - n_bg - L.n_slots) // 4 * 4
        bg_start = T_SPR0 + pool_tiles
        n1, n2 = n_bg, 0
        sdesc = frame_info

        def final_tile(widx: int) -> int:
            return (bg_start + (widx & 0x1FF)) | (widx & 0x0600)
        mtdef_final = [final_tile(wd) for wd in mtdef_words]
        sp_tiles = [wd if stream else final_tile(wd) for _, t in spans for wd in t]
        sp_key = [k_ for k_, _ in spans]
        sp_off = [0]
        for _, t in spans:
            sp_off.append(sp_off[-1] + len(t))
        L.n_span = len(spans)

        # -- objects table ------------------------------------------------------
        objs = []
        strings: List[str] = []
        for o in m.objects:
            if o.typ in (0, 20, 21):
                continue
            aux = 0
            cx, cy = o.x // 16, o.y // 16
            if o.typ in (24, 25, 26):
                aux = obj_alt.get(cy * 128 + cx, 0)
                if o.typ == 24 and o.h > 16:
                    aux |= (obj_alt.get((cy + 1) * 128 + cx, 0)) << 8
            sidx = 0xFFFF
            if o.typ == 12:
                sidx = len(strings)
                strings.append(o.string)
            tgt = 0xFFFF
            if o.typ == 12:
                t = o.string.strip().lower()
                if t.endswith('.jn1') and t in lvl_id:
                    tgt = lvl_id[t]
                elif t == '!':
                    tgt = 0xFE
                elif t.startswith('*'):
                    nm_ = t[1:].replace('.ddt', '')
                    tgt = 0x100 + SONG_NAMES.index(nm_)
                elif t.startswith('&'):
                    tgt = 0x300 + MACROS.index(t[1:])
                elif t.endswith('.dem'):
                    tgt = 0x200 + int(t[:-4])
            objs.append((o.typ, o.x, o.y, o.w, o.h, o.xd & 0xFFFF, o.yd & 0xFFFF, o.state & 0xFFFF,
                         o.counter & 0xFFFF, aux, sidx, tgt))
        player = [o for o in m.objects if o.typ == 0]
        spawn = (player[0].x, player[0].y) if player else (32, 32)
        spawns2 = (player[1].x, player[1].y) if len(player) > 1 else spawn

        L.name, L.mtdef, L.mtflags, L.map = name, mtdef_final, bytes(mtflags), bytes(map_bytes)
        L.objs, L.strings, L.sdesc = objs, strings, sdesc
        L.bg_pal, L.spr_pal = hexpal(bg_sel), hexpal(spr_sel)
        L.pool, L.n1, L.bg_start = pool, n1, bg_start
        L.spawn, L.spawn2 = spawn, spawns2
        L.n_mt = len(mt_keys)

        # text pack: strings concatenated with NULs
        spool = bytearray()
        sidx_off = []
        for s in strings:
            sidx_off.append(len(spool))
            spool += s.encode('latin1', 'replace') + b'\0'

        # write banks for this level
        pool_blob = pool.blob()
        L.pool_tiles, L.n2 = pool_tiles, n2
        cb = bw.new_bank()
        bw.add(f'lvl{li}_mtdef', struct.pack('<%dH' % len(mtdef_final), *mtdef_final), cb, 'u16')
        bw.add(f'lvl{li}_mtflags', L.mtflags, cb)
        bw.add(f'lvl{li}_map', L.map, cb)
        bw.add(f'lvl{li}_spkey', struct.pack('<%dH' % max(1, len(sp_key)), *(sp_key or [0])), cb, 'u16')
        bw.add(f'lvl{li}_spoff', struct.pack('<%dH' % len(sp_off), *sp_off), cb, 'u16')
        bw.add(f'lvl{li}_sptile', struct.pack('<%dH' % max(1, len(sp_tiles)), *(sp_tiles or [0])), cb, 'u16')
        mac_words: List[int] = []
        for o in m.objects:
            if o.typ == 12 and o.string.startswith('&'):
                held_ = {'x': 0, 'y': 0, 'b1': 0, 'b2': 0}
                for tick_, ch_ in F.parse_mac(ROOT / o.string[1:].upper()):
                    held_.update({k_: v_ for k_, v_ in ch_.items() if k_ in held_})
                    mask = ((1 if held_['y'] < 0 else 0) | (2 if held_['y'] > 0 else 0) |
                            (4 if held_['x'] < 0 else 0) | (8 if held_['x'] > 0 else 0) |
                            (0x10 if held_['b1'] else 0) | (0x20 if held_['b2'] else 0))
                    mac_words += [tick_, mask]
                break
        L.n_mac = len(mac_words) // 2
        bw.add(f'lvl{li}_mac', struct.pack('<%dH' % max(2, len(mac_words)), *(mac_words or [0, 0])), cb, 'u16')
        L.pt_bank = 0
        if stream:
            bw.add(f'lvl{li}_ptslot', struct.pack('<%dH' % len(pt_slot), *pt_slot), cb, 'u16')
            for ci in range(0, len(L.pt_tiles), 480):
                nb_ = bw.new_bank()
                bw.add(f'lvl{li}_pt{ci // 480}', b''.join(L.pt_tiles[ci:ci + 480]), nb_)
                if ci == 0:
                    L.pt_bank = nb_ + bw.first
        sf_res = 7 * n_spr_ids
        bw.sizes[cb] += sf_res          # reserve room for the frame table
        flat = []
        for r in objs:
            flat.extend(r)
        ob = bw.find(len(flat) * 2 + len(spool) + 2 * len(sidx_off) + len(jill_slot) + 7 * n_spr_ids + 64)
        bw.add(f'lvl{li}_objs', struct.pack('<%dH' % len(flat), *flat) if flat else struct.pack('<H', 0), ob, 'u16')
        bw.add(f'lvl{li}_strings', bytes(spool) or b'\0', ob)
        bw.add(f'lvl{li}_stroff', struct.pack('<%dH' % len(sidx_off), *sidx_off) if sidx_off else b'\0\0', ob, 'u16')
        bw.add(f'lvl{li}_bgtiles', pool_blob, bw.find(len(pool_blob)))
        for ci, ch in enumerate(chunks):
            bw.add(f'lvl{li}_sfc{ci}', bytes(ch) or b'\0', bw.find(len(ch)))
        L.nchunks = len(chunks)
        rows_ = []
        for fi_ in sdesc:
            if fi_ is None:
                rows_.append('{0,0,0,0,0}')
            else:
                ci, off, xo, yo, cbv = fi_
                rows_.append(f'{{lvl{li}_sfc{ci}+{off},{bw.where[f"lvl{li}_sfc{ci}"]},{xo},{yo},{cbv}}}')
        txt = f'const SFrame lvl{li}_sframes[] = {{\n  ' + ',\n  '.join(rows_) + '\n};'
        bw.sizes[cb] -= sf_res
        bw.add_c(f'lvl{li}_sframes', txt, 7 * len(rows_), cb)
        gfx_bank = bw.where[f'lvl{li}_bgtiles']
        jb = jill_bank
        db = cb + bw.first
        stats.append(f'{li:02d} {name:6s} mt={L.n_mt:3d} bgtiles={n_bg:3d} sprtiles={n_spr_tiles:3d} '
                     f'objs={len(objs):3d} mark={len(marks)} strips={len(need)} '
                     f'pool={pool_tiles} vram_bg=[{bg_start}..{bg_start + n1 - 1}]+{n2}')
        desc_rows.append((li, name, gfx_bank, jb, db, L, n1, n2, bg_start, 0, n_spr_tiles, len(pool_blob)))

    # ---- music & sfx (separate modules) ------------------------------------
    music_rows, sfx_rows, music_names = [], [], []
    try:
        import convert_music as CM
        import convert_sfx as CS
        music_rows, music_names = CM.convert_all(ROOT, bw)
        sfx_rows = CS.convert_all(vcl, bw)
    except ImportError as ex:   # pragma: no cover
        print('music/sfx converters not available:', ex)

    # ---- global text (help pages) -------------------------------------------
    texts = bytearray()
    text_off = []
    EXTRA_TEXTS = {
        0: ('\n1      JILL OF THE JUNGLE\n\n3        by Epic MegaGames\n\n\n2       Sega Master System\n2            version\n\n\n'
            '7    Start the game\n7    Load saved game\n7    How to play\n7    Demo: Jill\n7    Demo: Volume II\n7    Demo: Volume III\n\n'
            '4  Up/Down: choose   Button: go\n\x1a'),
    }
    # The original INTRO.JN1 is a shareware attract world whose text sits on flat panels; its
    # credits / story / sequel info are re-typeset here as scrollable text pages.
    intro = F.parse_jn(ROOT / 'INTRO.JN1')
    PAGE_COL = {1: '1', 2: '2', 3: '3', 4: '4', 5: '4', 6: '7', 7: '7'}

    def intro_page(title, boxes):
        objs_ = [o for o in intro.objects if o.typ in (20, 21) and o.string.strip()
                 and any(b[0] <= o.x <= b[2] and b[1] <= o.y <= b[3] for b in boxes)]
        x0 = min(o.x for o in objs_)
        rows: Dict[int, list] = {}
        for o in sorted(objs_, key=lambda o: (o.y, o.x)):
            rows.setdefault(o.y, []).append(o)
        paras = []          # [col, indent, text, last_y, single]
        for y_ in sorted(rows):
            line, col = '', None
            for o in rows[y_]:
                cw = 6 if o.typ == 20 else 8
                pos = (o.x - x0) // cw
                if col is None:
                    col = PAGE_COL.get(o.xd, '7')
                if line and pos <= len(line):
                    pos = len(line) + 1
                line = line.ljust(pos) + o.string
            single = len(rows[y_]) == 1
            ind = min(len(line) - len(line.lstrip()), 4)
            body = line.strip()
            body = ' '.join(body.split()) if single else body
            if (paras and single and paras[-1][4] and y_ - paras[-1][3] < 12 and paras[-1][0] == col
                    and abs(paras[-1][1] - ind) <= 2 and paras[-1][6] >= 34 and body[:1] not in '*>' and not paras[-1][2].endswith(':')):
                paras[-1][2] += ' ' + body
                paras[-1][3] = y_
                paras[-1][6] = len(body)
            else:
                gap = bool(paras) and y_ - paras[-1][3] >= 12
                paras.append([col, ind, body, y_, single, gap, len(body)])
        out = ['1' + title, '']
        for pi, (col, ind, body, _, single, gap, _l) in enumerate(paras):
            if pi and gap:
                out.append('')
            first = True
            hang = ind + (2 if body[:1] in '*>' else 0)
            while len(body) + (ind if first else hang) > 30:
                w_ = 30 - (ind if first else hang)
                cut = body.rfind(' ', 0, w_ + 1)
                if cut <= 0:
                    cut = w_
                out.append(col + ' ' * (ind if first else hang) + body[:cut].rstrip())
                body = body[cut:].lstrip()
                first = False
            out.append(col + ' ' * (ind if first else hang) + body)
        return '\n'.join(out) + '\n\x1a'

    EXTRA_TEXTS[16] = intro_page('Jill of the Jungle', [(240, 104, 560, 200), (600, 40, 880, 60)])
    EXTRA_TEXTS[17] = intro_page('The Story', [(600, 100, 880, 216)])
    EXTRA_TEXTS[18] = intro_page('The Jill Series', [(32, 580, 302, 731)])
    EXTRA_TEXTS[19] = intro_page('Thank You', [(592, 296, 886, 407)])
    EXTRA_TEXTS[21] = '\n1      PAUSED\n\n7    Resume\n7    Save game\n7    Turtle mode: on\n7    Turtle mode: off\n7    How to play\n\x1a'
    EXTRA_TEXTS[22] = '1Elevator\n\nPress UP/DOWN to use\nthe elevator.\n\x1a'      # JILL.EXE: _msg_elev
    vcl.texts.update(EXTRA_TEXTS)
    # PC-specific wording -> SMS controls
    for k_, v_ in list(vcl.texts.items()):
        for a_, b_ in ARROW_FIX:
            v_ = v_.replace(a_, b_)
        vcl.texts[k_] = v_
    for i in range(40):
        t = vcl.texts.get(i)
        text_off.append(len(texts) if t else 0xFFFF)
        if t:
            texts += t.replace('\r', '').encode('latin1', 'replace') + b'\0'
    tb = bw.add('game_texts', bytes(texts), None)
    bw.add('game_text_off', struct.pack('<40H', *text_off), tb - bw.first, 'u16')

    # font / hud in a fixed bank (also used by the title/menu code)
    fb = bw.add('font1bpp', bytes(font1bpp), None)

    # ---- emit bank sources ----------------------------------------------------
    for bi, items in enumerate(bw.banks):
        bank = bi + bw.first
        src = ['#include "game_data.h"']
        for nm, data, kind in items:
            src.append(c_bytes(nm, data, kind))
        (OUT / f'bank{bank}.c').write_text('\n'.join(src) + '\n')
    nb = len(bw.banks)
    if bw.first + nb > 30:
        raise SystemExit(f'data needs banks up to {bw.first + nb - 1}; only 2..29 are free')
    banks = list(range(bw.first, bw.first + nb))
    (OUT / 'banks.mk').write_text(
        'BANKS := ' + ' '.join(map(str, banks)) + '\nBANKFLAGS := ' +
        ' '.join(f'-Wl-b_BANK{b}=0x{b:X}8000' for b in banks) + '\n')

    # ---- header & descriptor table ------------------------------------------
    h = ['#ifndef JILL_GAME_DATA_H', '#define JILL_GAME_DATA_H', '', '#include "SMSlib.h"', '',
         f'#define JILL_N_LEVELS {len(LEVELS)}', f'#define SPR_COUNT {n_spr_ids}',
         f'#define JILL_N_FRAMES {len(jill_shapes)}',
         f'#define T_SPR0 {T_SPR0}', f'#define T_HUD {T_HUD}', f'#define HUD_TILES {HUD_TILES}', '',
         '#define MF_SOLID 0x01', '#define MF_PLAT  0x02', '#define MF_VINE  0x04', '#define MF_WATER 0x08',
         '#define MF_HURT  0x10', '#define MF_KILL  0x20', '#define MF_MARK  0x40', '']
    for n in strip_names:
        h.append(f'#define SPR_{n} {strip_base[n]}')
        h.append(f'#define SPR_{n}_N {len(S[n][3])}')
    h.append('')
    h.append('typedef struct { const unsigned char *p; unsigned char bank, xo, yo, cb; } SFrame;')
    h.append('typedef struct {\n  unsigned char core_bank, obj_bank, bg_bank, hud_w, hud_y, hud_r, hud_g;\n'
             '  unsigned char pool_tiles, bg_start, music;\n'
             '  unsigned int n_bg, nobjs;\n'
             '  const unsigned char *bgtiles, *mtflags, *map, *strings;\n'
             '  const unsigned int *mtdef, *objs, *stroff, *sp_key, *sp_off, *sp_tile;\n  unsigned char nspan, nmac;\n  const unsigned int *mac;\n  const unsigned int *pt_slot; const unsigned char *pt_tiles;\n  unsigned char pt_bank;\n  unsigned int pt_vram;\n  unsigned char elev_mt;\n'
             '  const SFrame *sframes;\n'
             '  const unsigned char *bg_pal, *spr_pal;\n'
             '  unsigned int spawn_x, spawn_y;\n'
             '  const char *name;\n} LevelDesc;')
    h.append('extern const LevelDesc level_descs[JILL_N_LEVELS];')
    h.append('extern const unsigned char font1bpp[]; /* in bank %d */' % fb)
    h.append(f'#define FONT_BANK {fb}')
    h.append(f'#define JILL_BANK {jill_bank}')
    h.append('extern const unsigned char jill_gfx[];')
    h.append('extern const unsigned char jill_slot[JILL_N_FRAMES];')
    h.append('typedef struct { unsigned char n; unsigned char l[8]; unsigned char r[8]; } Anim;')
    h.append('extern const Anim anims[];')
    for ai, an in enumerate(ANIMS):
        h.append(f'#define ANIM_{an} {ai}')
    h.append('extern const unsigned char jill_pal[16];')
    h.append(f'#define TEXT_BANK {tb}')
    h.append('extern const unsigned char game_texts[]; extern const unsigned int game_text_off[];')
    h.append(f'#define N_SONGS {len(music_rows)}')
    h.append(f'#define N_SFX {len(sfx_rows)}')
    for i, (nm, bank, _) in enumerate(sfx_rows):
        h.append(f'#define {nm.upper()} {i}')
    for i, nm in enumerate(music_names):
        h.append(f'#define SONG_{nm.upper()} {i + 1}')
    h.append('typedef struct { unsigned char bank; const unsigned char *data; } BlobRef;')
    h.append('extern const BlobRef song_table[N_SONGS + 1];')
    h.append('extern const BlobRef sfx_table[N_SFX + 1];')
    for nm, bank, _ in music_rows + sfx_rows:
        h.append(f'extern const unsigned char {nm}[];')
    for (li, name, gb, jb, db, L, n1, n2, bs, r2, nst, pl) in desc_rows:
        for k in ('bgtiles', 'mtflags', 'map', 'strings'):
            h.append(f'extern const unsigned char lvl{li}_{k}[];')
        h.append(f'extern const SFrame lvl{li}_sframes[];')
        for ci in range(L.nchunks):
            h.append(f'extern const unsigned char lvl{li}_sfc{ci}[];')
        for k in ('mtdef', 'objs', 'stroff', 'spkey', 'spoff', 'sptile', 'mac'):
            h.append(f'extern const unsigned int lvl{li}_{k}[];')
        if L.stream:
            h.append(f'extern const unsigned int lvl{li}_ptslot[]; extern const unsigned char lvl{li}_pt0[];')
    h.append('')
    h.append('#endif')
    (OUT / 'game_data.h').write_text('\n'.join(h) + '\n')

    song_for_level = {}
    for i, mnm in enumerate(music_names):
        song_for_level[mnm] = i
    gd = ['#include "game_data.h"', '']
    for nm, data, kind in []:
        pass
    rows = []
    for (li, name, gb, jb, db, L, n1, n2, bs, r2, nst, pl) in desc_rows:
        first_music = 0
        # level default music = first '*' checkpoint string
        m = maps[li]
        for o in m.objects:
            if o.typ == 12 and o.string.startswith('*'):
                first_music = song_for_level.get(o.string[1:].lower().replace('.ddt', ''), 0) + 1
                break
        n_bg = len(L.pool.tiles)
        rows.append(
            f'  {{{db},{bw.where[f"lvl{li}_objs"]},{gb},{L.hud[0]},{L.hud[1]},{L.hud[2]},{L.hud[3]},{L.pool_tiles},{bs},{first_music},{n_bg},{len(L.objs)},'
            f'lvl{li}_bgtiles,lvl{li}_mtflags,lvl{li}_map,lvl{li}_strings,'
            f'lvl{li}_mtdef,lvl{li}_objs,lvl{li}_stroff,lvl{li}_spkey,lvl{li}_spoff,lvl{li}_sptile,{L.n_span},{L.n_mac},lvl{li}_mac,'
            f'{"lvl%d_ptslot" % li if L.stream else "0"},{"lvl%d_pt0" % li if L.stream else "0"},{L.pt_bank},{bs + n_bg},{L.elev_mt},'
            f'lvl{li}_sframes,'
            f'pal_bg{li},pal_spr{li},{L.spawn[0]},{L.spawn[1]},"{name}"}},')
        gd.insert(1, f'const unsigned char pal_bg{li}[16]={{' + ','.join(str(v) for v in L.bg_pal) + '};')
        gd.insert(1, f'const unsigned char pal_spr{li}[16]={{' + ','.join(str(v) for v in L.spr_pal) + '};')
    gd.append('const unsigned char jill_slot[JILL_N_FRAMES] = {' + ','.join(str(v) for v in jill_slot) + '};')
    gd.append('const unsigned char jill_pal[16] = {' + ','.join(str(v) for v in hexpal([0x00] + JILL_COLS + [0] * 7)) + '};')
    gd.append('const Anim anims[] = {')
    for an, (n_, l_, r_) in ANIMS.items():
        l8 = (l_ + [0] * 8)[:8]
        r8 = (r_ + [0] * 8)[:8]
        gd.append('  {%d,{%s},{%s}},' % (n_, ','.join(map(str, l8)), ','.join(map(str, r8))))
    gd.append('};')
    gd.append('const LevelDesc level_descs[JILL_N_LEVELS] = {')
    gd.extend(rows)
    gd.append('};')
    gd.append('const BlobRef song_table[N_SONGS + 1] = {')
    for nm, bank, _ in music_rows:
        gd.append(f'  {{{bank},{nm}}},')
    gd.append('  {0,0}};')
    gd.append('const BlobRef sfx_table[N_SFX + 1] = {')
    for nm, bank, _ in sfx_rows:
        gd.append(f'  {{{bank},{nm}}},')
    gd.append('  {0,0}};')
    (OUT / 'game_data.c').write_text('\n'.join(gd) + '\n')
    (OUT / 'conversion_report.txt').write_text('\n'.join(stats) + '\n')
    print('\n'.join(stats))
    print(f'{nb} data banks: {bw.first}..{bw.first + nb - 1}; sprite ids {n_spr_ids}')


if __name__ == '__main__':
    main()
