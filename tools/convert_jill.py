#!/usr/bin/env python3
from __future__ import annotations

import os
import re
from pathlib import Path
from typing import Dict, List, Tuple

from jilllib import (
    MAP_W, MAP_H, DMAEntry, JNMap, ShapeSet,
    parse_dma, parse_sha, parse_jn, load_vga_palette,
    shape_to_sms_tiles, pixels_to_sms_tiles, dma_flags_to_sms,
    c_array_u8, c_array_u16,
)

ROOT = Path(os.environ.get('JILL_DATA', '/home/user/jill-source/ep1'))
OUT = Path(__file__).resolve().parents[1] / 'gen'


def ident(s: str) -> str:
    return re.sub(r'[^a-zA-Z0-9_]', '_', s).lower()


def sms_color(rgb6: Tuple[int, int, int]) -> int:
    r, g, b = rgb6
    return ((r >> 4) & 3) | (((g >> 4) & 3) << 2) | (((b >> 4) & 3) << 4)

# The converter maps SHA colour-map entries to EGA indices.  Therefore the SMS
# palette must be an EGA-like palette, not the first 16 entries of Jill's VGA
# palette.  Using the VGA entries was the reason the player sprite looked like
# purple/white garbage while the bitplanes themselves were valid.
EGA_SMS = bytes([
    0x00, 0x20, 0x08, 0x28, 0x02, 0x22, 0x06, 0x2A,
    0x15, 0x35, 0x1D, 0x3D, 0x17, 0x37, 0x1F, 0x3F,
])


def choose_maps(root: Path) -> List[Path]:
    paths = {p.name.upper(): p for p in root.glob('*.JN1')}
    order = []
    for n in ['MAP.JN1', '1.JN1', '2.JN1', '3.JN1', '4.JN1', '6.JN1', '9.JN1', '50.JN1', 'INTRO.JN1']:
        if n in paths:
            order.append(paths[n])
    for p in sorted(root.glob('*.JN1')):
        if p not in order:
            order.append(p)
    return order


def target_from_string(s: str) -> str:
    t = s.strip().lower()
    if not t:
        return ''
    if t[0] in '*#&':
        return ''
    if t == '!':
        return 'map.jn1'
    if t.endswith('.jn1'):
        return t
    return ''


def get_shape(shapesets: Dict[int, ShapeSet], ent: DMAEntry):
    ss = shapesets.get(ent.tileset)
    if ss is None or ent.tile >= len(ss.shapes):
        raise KeyError(f"DMA code {ent.code} points to missing shape set={ent.tileset} tile={ent.tile}")
    return ss.shapes[ent.tile]


def convert_level(jn: JNMap, dma: Dict[int, DMAEntry], shapesets: Dict[int, ShapeSet]):
    used_codes = sorted(set(jn.codes))
    code_to_mt: Dict[int, int] = {}
    tile_blob = bytearray()
    mtdef: List[int] = []
    mtflags: List[int] = []
    for code in used_codes:
        if code not in dma:
            raise KeyError(f"{jn.path.name}: map code {code} has no JILL.DMA entry")
        ent = dma[code]
        shape = get_shape(shapesets, ent)
        if shape.w > 16 or shape.h > 16:
            raise ValueError(f"{jn.path.name}: unsupported BG shape {ent.name} {shape.w}x{shape.h}")
        mt = len(code_to_mt)
        code_to_mt[code] = mt
        tile_blob.extend(shape_to_sms_tiles(shape))
        base = mt * 4
        mtdef.extend([base, base + 1, base + 2, base + 3])
        mtflags.append(dma_flags_to_sms(ent.flags))
    map_bytes = bytes(code_to_mt[c] for c in jn.codes)
    player = next((o for o in jn.objects if o.typ == 0), None)
    spawn_x = player.x if player else 32
    spawn_y = player.y if player else 32
    return bytes(tile_blob), mtdef, bytes(mtflags), map_bytes, spawn_x, spawn_y


def main() -> None:
    OUT.mkdir(exist_ok=True)
    dma = parse_dma(ROOT / 'JILL.DMA')
    shapesets = parse_sha(ROOT / 'JILL1.SHA')
    palette = load_vga_palette(ROOT / 'JILL.EXE')
    maps = [parse_jn(p) for p in choose_maps(ROOT)]
    filename_to_id = {m.path.name.lower(): i for i, m in enumerate(maps)}

    # Bank 2: player and item sprites.  Jill's native frames are 16x32; the
    # first WIP cropped them to 16x24 (a Keen leftover), which made the sprite
    # look broken in emulators.  Keep the full four-tile-tall frame.
    bank = 2
    bank_files: List[int] = [bank]
    player_frame_count = 1
    if 8 in shapesets and shapesets[8].shapes:
        # Convert every 16x32 Jill frame in the main player set.  The runtime
        # streams the selected frame into the fixed player VRAM slot.
        frames = [sh for sh in shapesets[8].shapes if sh.w == 16 and sh.h == 32]
        player_frame_count = len(frames)
        sprite_src = bytearray()
        for sh in frames:
            px = bytearray()
            for y in range(32):
                for x in range(16):
                    px.append(sh.pixels[y * sh.w + x])
            sprite_src.extend(pixels_to_sms_tiles(bytes(px), 16, 32, 2, 4))
        sprite_src = bytes(sprite_src)
    else:
        pix = bytearray(16*32)
        for y in range(32):
            for x in range(16):
                pix[y*16+x] = 10 if 1 <= x <= 14 and 1 <= y <= 30 else 0
        sprite_src = pixels_to_sms_tiles(bytes(pix), 16, 32, 2, 4)

    def sprite16_from_shape(set_id: int, shape_id: int) -> bytes:
        ss = shapesets[set_id]
        sh = ss.shapes[shape_id]
        pix = bytearray(16*16)
        ox = max(0, (16 - sh.w) // 2)
        oy = max(0, (16 - sh.h) // 2)
        for y in range(min(sh.h, 16)):
            for x in range(min(sh.w, 16)):
                pix[(oy+y)*16 + (ox+x)] = sh.pixels[y*sh.w + x]
        return pixels_to_sms_tiles(bytes(pix), 16, 16, 2, 2)

    item_blob = bytearray()
    item_blob.extend(sprite16_from_shape(9, 0))    # apple / bonus fruit
    item_blob.extend(sprite16_from_shape(13, 0))   # knife
    item_blob.extend(sprite16_from_shape(14, 5))   # red key
    item_blob.extend(sprite16_from_shape(14, 11))  # crystal/gem (red diamond pickup)
    item_blob.extend(sprite16_from_shape(14, 20))  # generic inventory token

    enemy_blob = bytearray()
    enemy_blob.extend(sprite16_from_shape(10, 0))   # ant / ground bug (cropped from 32x16)
    enemy_blob.extend(sprite16_from_shape(11, 0))   # phoenix / bird
    enemy_blob.extend(sprite16_from_shape(15, 0))   # frog
    enemy_blob.extend(sprite16_from_shape(14, 13))  # spike / small hazard
    enemy_blob.extend(sprite16_from_shape(12, 0))   # fire-ish tall enemy, cropped

    (OUT / f'bank{bank}.c').write_text(
        '#include "game_data.h"\n' +
        c_array_u8('spr_jill', sprite_src) + '\n' +
        c_array_u8('spr_items', bytes(item_blob)) + '\n' +
        c_array_u8('spr_enemies', bytes(enemy_blob)) + '\n'
    )
    sprite_bank = bank
    bank += 1

    externs: List[str] = ['extern const unsigned char spr_jill[];', 'extern const unsigned char spr_items[];', 'extern const unsigned char spr_enemies[];']
    desc_rows: List[str] = []
    bank_c_texts: Dict[int, List[str]] = {}
    stats: List[str] = []
    converted = []
    for i, m in enumerate(maps):
        stem = ident(m.path.stem)
        tiles, mtdef, mtflags, map_bytes, sx, sy = convert_level(m, dma, shapesets)
        transitions: List[int] = []
        items: List[int] = []
        doors: List[int] = []
        enemies: List[int] = []
        for o in m.objects:
            if o.typ == 12:
                tgt_name = target_from_string(o.string)
                if not tgt_name or tgt_name not in filename_to_id:
                    continue
                tid = filename_to_id[tgt_name]
                # If the checkpoint provides destination coords, use them; otherwise
                # fall back to the target map's own player object.
                dx = o.xd if o.xd > 0 else 0
                dy = o.yd if o.yd > 0 else 0
                if dx == 0 and dy == 0:
                    target_player = next((to for to in maps[tid].objects if to.typ == 0), None)
                    dx = target_player.x if target_player else 32
                    dy = target_player.y if target_player else 32
                w = o.w if o.w else 16
                h = o.h if o.h else 16
                transitions.extend([o.x, o.y, w, h, tid, dx, dy])
            elif o.typ in (1, 2, 14, 28, 33):
                # x, y, type, counter.  The runtime maps these to a small fixed
                # sprite set.  This immediately fixes missing object-layer gems
                # and fruit/keys in the first level.
                items.extend([o.x, o.y, o.typ, o.counter & 0xFFFF])
            elif o.typ == 24:
                doors.extend([o.x, o.y, o.w or 16, o.h or 24, o.xd & 0xFFFF, o.yd & 0xFFFF, o.counter & 0xFFFF])
            elif o.typ in (4, 7, 8, 9, 11, 17, 22, 29, 30, 35, 38, 39, 41, 43, 45, 46, 47, 48, 53, 59, 60, 65):
                enemies.extend([o.x, o.y, o.typ, o.xd & 0xFFFF])

        tile_bank = bank; bank_files.append(bank); bank += 1
        data_bank = bank; bank_files.append(bank); bank += 1
        tname = f'lvl{i}_{stem}_tiles'
        mtname = f'lvl{i}_{stem}_mtdef'
        flname = f'lvl{i}_{stem}_flags'
        mapname = f'lvl{i}_{stem}_map'
        trname = f'lvl{i}_{stem}_trans'
        itname = f'lvl{i}_{stem}_items'
        drname = f'lvl{i}_{stem}_doors'
        enname = f'lvl{i}_{stem}_enemies'
        externs.extend([
            f'extern const unsigned char {tname}[];',
            f'extern const unsigned int {mtname}[];',
            f'extern const unsigned char {flname}[];',
            f'extern const unsigned char {mapname}[];',
            f'extern const unsigned int {trname}[];',
            f'extern const unsigned int {itname}[];',
            f'extern const unsigned int {drname}[];',
            f'extern const unsigned int {enname}[];',
        ])
        bank_c_texts[tile_bank] = ['#include "game_data.h"', c_array_u8(tname, tiles)]
        bank_c_texts[data_bank] = [
            '#include "game_data.h"',
            c_array_u16(mtname, mtdef),
            c_array_u8(flname, mtflags),
            c_array_u8(mapname, map_bytes),
            c_array_u16(trname, transitions),
            c_array_u16(itname, items),
            c_array_u16(drname, doors),
            c_array_u16(enname, enemies),
        ]
        desc_rows.append(
            f'  {{{tile_bank},{data_bank},{tname},{len(tiles)},{mtname},{flname},{mapname},'
            f'{trname},{len(transitions)//7},{itname},{len(items)//4},{drname},{len(doors)//7},'
            f'{enname},{len(enemies)//4},{sx},{sy},"{m.path.name}"}},'
        )
        stats.append(f"{i:02d} {m.path.name:9s} mt={len(mtflags):3d} tiles_bytes={len(tiles):5d} trans={len(transitions)//7:2d} items={len(items)//4:3d} doors={len(doors)//7:2d} enemies={len(enemies)//4:3d} spawn=({sx},{sy}) strings={len(m.strings)}")
        converted.append((m, len(mtflags), len(tiles), len(transitions)//7))

    for bno, parts in bank_c_texts.items():
        (OUT / f'bank{bno}.c').write_text('\n'.join(parts) + '\n')

    # Header/fixed data. SHA pixels are now EGA-indexed, so both palettes use
    # a fixed EGA approximation.  The raw VGA palette is still parsed above for
    # future 256-colour quantisation work.
    _ = palette
    bgpal = list(EGA_SMS)
    sprpal = list(EGA_SMS)
    header = f'''#ifndef JILL_GAME_DATA_H\n#define JILL_GAME_DATA_H\n\n#include "SMSlib.h"\n\n#define JILL_N_LEVELS {len(maps)}\n#define JILL_BANK_SPRITES {sprite_bank}\n#define JILL_N_PLAYER_FRAMES {player_frame_count}\n\n#define JF_SOLID  0x01\n#define JF_PLAT   0x02\n#define JF_VINE   0x04\n#define JF_WATER  0x08\n#define JF_TOUCH  0x10\n#define JF_DEADLY 0x20\n\ntypedef struct {{\n  unsigned char tile_bank;\n  unsigned char data_bank;\n  const unsigned char *tiles;\n  unsigned int tiles_size;\n  const unsigned int *mtdef;\n  const unsigned char *flags;\n  const unsigned char *map;\n  const unsigned int *trans;\n  unsigned char ntrans;\n  const unsigned int *items;\n  unsigned char nitems;\n  const unsigned int *doors;\n  unsigned char ndoors;\n  const unsigned int *enemies;\n  unsigned char nenemies;\n  unsigned int spawn_x;\n  unsigned int spawn_y;\n  const char *name;\n}} LevelDesc;\n\nextern const unsigned char bg_palette[16];\nextern const unsigned char spr_palette[16];\nextern const LevelDesc level_descs[JILL_N_LEVELS];\n{chr(10).join(externs)}\n\n#endif\n'''
    (OUT / 'game_data.h').write_text(header)
    fixed = [
        '#include "game_data.h"',
        c_array_u8('bg_palette', bytes(bgpal), 16),
        c_array_u8('spr_palette', bytes(sprpal), 16),
        f'const LevelDesc level_descs[JILL_N_LEVELS] = {{',
        *desc_rows,
        '};',
    ]
    (OUT / 'game_data.c').write_text('\n'.join(fixed) + '\n')

    for bno in sorted(bank_files):
        if bno == sprite_bank:
            continue
        # Already written via bank_c_texts.
        pass
    bank_list = ' '.join(str(b) for b in sorted(bank_files))
    bank_flags = ' '.join(f'-Wl-b_BANK{b}=0x{b:X}8000' for b in sorted(bank_files))
    (OUT / 'banks.mk').write_text(f'BANKS := {bank_list}\nBANKFLAGS := {bank_flags}\n')
    (OUT / 'conversion_report.txt').write_text('\n'.join(stats) + '\n')
    print('\n'.join(stats))
    print(f"Generated {len(maps)} levels, banks {bank_list}")


if __name__ == '__main__':
    main()
