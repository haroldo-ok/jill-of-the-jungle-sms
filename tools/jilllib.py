#!/usr/bin/env python3
"""Jill of the Jungle data readers and SMS conversion helpers.

References:
  * https://moddingwiki.shikadi.net/wiki/Jill_of_the_Jungle_Map_Format
  * https://moddingwiki.shikadi.net/wiki/SHA_Format
"""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import struct
from typing import Dict, List, Optional, Tuple

MAP_W = 128
MAP_H = 64
MAP_CELLS = MAP_W * MAP_H
OBJ_SIZE = 31
SAVE_SIZE_JILL = 70

F_PLAYERTHRU = 0x0001
F_STAIR      = 0x0002
F_VINE       = 0x0004
F_MSGTOUCH   = 0x0008
F_MSGDRAW    = 0x0010
F_MSGUPDATE  = 0x0020
F_INSIDE     = 0x0040
F_FRONT      = 0x0080
F_TRIGGER    = 0x0100
F_BACK       = 0x0200
F_ALWAYS     = 0x0400
F_KILLABLE   = 0x0800
F_FIREBALL   = 0x1000
F_WATER      = 0x2000
F_WEAPON     = 0x4000

SMS_F_SOLID  = 0x01
SMS_F_PLAT   = 0x02
SMS_F_VINE   = 0x04
SMS_F_WATER  = 0x08
SMS_F_TOUCH  = 0x10
SMS_F_DEADLY = 0x20

@dataclass
class DMAEntry:
    code: int
    tile: int
    tileset: int
    flags: int
    name: str

@dataclass
class Shape:
    w: int
    h: int
    type: int
    pixels: bytes          # already mapped to 0..15-ish colour indices for SMS
    raw_pixels: bytes

@dataclass
class ShapeSet:
    index: int
    bits: int
    flags: int
    shapes: List[Shape]

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
    codes: List[int]       # row-major, high map status bits stripped
    objects: List[JNObject]
    strings: List[str]
    level_no: int
    health: int


def _u16(data: bytes, off: int) -> int:
    return struct.unpack_from('<H', data, off)[0]


def _s16(data: bytes, off: int) -> int:
    return struct.unpack_from('<h', data, off)[0]


def parse_dma(path: Path) -> Dict[int, DMAEntry]:
    data = path.read_bytes()
    off = 0
    out: Dict[int, DMAEntry] = {}
    while off + 7 <= len(data):
        code, tile, tileset, flags, nlen = struct.unpack_from('<HBBHB', data, off)
        off += 7
        if off + nlen > len(data):
            raise ValueError(f"truncated DMA name at 0x{off:x}")
        name = data[off:off+nlen].decode('latin1', 'replace')
        off += nlen
        out[code] = DMAEntry(code, tile, tileset & 0x3F, flags, name)
    if off != len(data):
        raise ValueError(f"DMA parse ended at {off}, file size is {len(data)}")
    return out


def load_vga_palette(exe: Path) -> List[Tuple[int, int, int]]:
    data = exe.read_bytes()
    # Known Episode 1 offsets from ModdingWiki; pick by size/CRC-era fallback.
    candidate_offsets = [0x24A64, 0x1FC04, 0x1FC34, 0x1FD54, 0x1F884, 0x1ED84]
    for off in candidate_offsets:
        if off + 768 <= len(data):
            pal = [(data[off+i*3], data[off+i*3+1], data[off+i*3+2]) for i in range(256)]
            # A crude sanity check: classic VGA palette data is 6-bit.
            if all(0 <= c < 64 for rgb in pal for c in rgb):
                return pal
    # Last resort: grayscale.
    return [(i >> 2, i >> 2, i >> 2) for i in range(256)]


def parse_sha(path: Path) -> Dict[int, ShapeSet]:
    data = path.read_bytes()
    offsets = struct.unpack_from('<128I', data, 0)
    sizes = struct.unpack_from('<128H', data, 512)
    sets: Dict[int, ShapeSet] = {}
    for si, (off, size) in enumerate(zip(offsets, sizes)):
        if not off and not size:
            continue
        if off <= 0 or off + size > len(data):
            raise ValueError(f"bad SHA set {si}: off={off} size={size}")
        p = off
        num_shapes = data[p]
        # numRots, lenCGA, lenEGA, lenVGA are not needed for raw VGA/Jill data.
        bits = data[p + 9]
        flags = _u16(data, p + 10)
        p += 12
        cmap: Optional[bytes] = None
        if not (flags & 0x0001) and bits != 8:
            cmap_len = (1 << bits) * 4
            cmap = data[p:p+cmap_len]
            p += cmap_len
        shapes: List[Shape] = []
        for ti in range(num_shapes):
            if p + 3 > off + size:
                raise ValueError(f"truncated SHA tile header set={si} tile={ti}")
            w, h, typ = data[p], data[p+1], data[p+2]
            p += 3
            if typ == 0:
                raw_len = w * h
                raw = data[p:p+raw_len]
                p += raw_len
            elif typ == 1:
                stride = ((w * bits) + 7) // 8
                packed = data[p:p+stride*h]
                p += stride * h
                # Unpack low-depth PLAIN just in case; Jill itself normally uses BYTE.
                vals = []
                mask = (1 << bits) - 1
                for y in range(h):
                    bitpos = 0
                    row = packed[y*stride:(y+1)*stride]
                    for x in range(w):
                        byte = row[bitpos >> 3]
                        shift = 8 - bits - (bitpos & 7)
                        vals.append((byte >> shift) & mask)
                        bitpos += bits
                raw = bytes(vals)
            else:
                raise ValueError(f"unsupported SHA tile type {typ} set={si} tile={ti}")
            if cmap is not None:
                mapped = bytes((cmap[min(v, (len(cmap)//4)-1)*4 + 1] & 0x0F) for v in raw)  # EGA entry
            else:
                mapped = bytes((v & 0x0F) for v in raw)
            shapes.append(Shape(w, h, typ, mapped, raw))
        sets[si] = ShapeSet(si, bits, flags, shapes)
    return sets


def parse_jn(path: Path) -> JNMap:
    data = path.read_bytes()
    if len(data) < MAP_CELLS * 2 + 2:
        raise ValueError(f"{path} too small for a Jill map")
    # Original storage is column-major: ((x * 64) + y) * 2.
    codes: List[int] = [0] * MAP_CELLS
    for x in range(MAP_W):
        for y in range(MAP_H):
            codes[y * MAP_W + x] = _u16(data, ((x * MAP_H) + y) * 2) & 0x3FFF
    off = MAP_CELLS * 2
    nobj = _u16(data, off)
    off += 2
    objects: List[JNObject] = []
    ptr_objects: List[int] = []
    for i in range(nobj):
        if off + OBJ_SIZE > len(data):
            raise ValueError(f"{path}: truncated object {i}")
        typ = data[off]
        obj = JNObject(
            i=i, typ=typ,
            x=_u16(data, off+1), y=_u16(data, off+3),
            xd=_s16(data, off+5), yd=_s16(data, off+7),
            w=_u16(data, off+9), h=_u16(data, off+11),
            state=_s16(data, off+13), substate=_s16(data, off+15),
            statecount=_s16(data, off+17), counter=_s16(data, off+19),
            flags=_u16(data, off+21), pointer=struct.unpack_from('<I', data, off+23)[0],
            info1=_s16(data, off+27), zaphold=_s16(data, off+29),
        )
        objects.append(obj)
        if obj.pointer:
            ptr_objects.append(i)
        off += OBJ_SIZE
    if off + SAVE_SIZE_JILL > len(data):
        raise ValueError(f"{path}: no Jill save-data block")
    level_no = _s16(data, off)
    health = _s16(data, off+2)
    off += SAVE_SIZE_JILL
    strings: List[str] = []
    while off + 2 <= len(data):
        ln = _u16(data, off)
        off += 2
        if ln > 4096 or off + ln > len(data):
            break
        s = data[off:off+ln].decode('latin1', 'replace')
        off += ln
        if off < len(data) and data[off] == 0:
            off += 1
        strings.append(s)
    for oi, s in zip(ptr_objects, strings):
        objects[oi].string = s
    return JNMap(path=path, codes=codes, objects=objects, strings=strings,
                 level_no=level_no, health=health)


def shape_to_sms_tiles(shape: Shape, crop_h: int = 16) -> bytes:
    """Convert a 16x16 (or smaller) shape to four SMS 8x8 4bpp tiles."""
    out = bytearray()
    for ty in (0, 8):
        for tx in (0, 8):
            for y in range(8):
                yy = ty + y
                b = [0, 0, 0, 0]
                for x in range(8):
                    xx = tx + x
                    c = 0
                    if xx < shape.w and yy < shape.h and yy < crop_h:
                        c = shape.pixels[yy * shape.w + xx] & 0x0F
                    bit = 0x80 >> x
                    if c & 1: b[0] |= bit
                    if c & 2: b[1] |= bit
                    if c & 4: b[2] |= bit
                    if c & 8: b[3] |= bit
                out.extend(b)
    return bytes(out)


def pixels_to_sms_tiles(pixels: bytes, w: int, h: int, tile_w: int, tile_h: int) -> bytes:
    out = bytearray()
    for ty in range(tile_h):
        for tx in range(tile_w):
            for y in range(8):
                yy = ty*8 + y
                b = [0, 0, 0, 0]
                for x in range(8):
                    xx = tx*8 + x
                    c = 0
                    if xx < w and yy < h:
                        c = pixels[yy*w + xx] & 0x0F
                    bit = 0x80 >> x
                    if c & 1: b[0] |= bit
                    if c & 2: b[1] |= bit
                    if c & 4: b[2] |= bit
                    if c & 8: b[3] |= bit
                out.extend(b)
    return bytes(out)


def dma_flags_to_sms(flags: int) -> int:
    out = 0
    # In Jill, zero flags means default solid. F_PLAYERTHRU means background/pass-through.
    if not (flags & F_PLAYERTHRU):
        out |= SMS_F_SOLID
    if flags & F_STAIR:
        out |= SMS_F_PLAT
    if flags & F_VINE:
        out |= SMS_F_VINE
    if flags & F_WATER:
        out |= SMS_F_WATER
    if flags & (F_MSGTOUCH | F_TRIGGER):
        out |= SMS_F_TOUCH
    # A conservative approximation for traps/hazard blocks used by early WIP engine.
    if flags & F_FIREBALL:
        out |= SMS_F_DEADLY
    return out


def c_array_u8(name: str, data: bytes, columns: int = 16) -> str:
    lines = [f"const unsigned char {name}[] = {{"]
    for i in range(0, len(data), columns):
        chunk = ','.join(f"0x{b:02X}" for b in data[i:i+columns])
        lines.append(f"  {chunk},")
    lines.append("};")
    return '\n'.join(lines)


def c_array_u16(name: str, vals: List[int], columns: int = 12) -> str:
    if not vals:
        vals = [0]
    lines = [f"const unsigned int {name}[] = {{"]
    for i in range(0, len(vals), columns):
        chunk = ','.join(str(v & 0xFFFF) for v in vals[i:i+columns])
        lines.append(f"  {chunk},")
    lines.append("};")
    return '\n'.join(lines)
