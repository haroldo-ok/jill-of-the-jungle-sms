#!/usr/bin/env python3
"""Post-process the SDCC assembly of main.c: assign every function to the resident area
(_CODE, ROM slot 0) or to one of the slot-1 overlay areas (_OVLA / _OVLB).

SDCC's `codeseg` pragma works per translation unit, so the single-unit game is split here,
at the assembly level, by source file:

  resident : audio / video / level + a few shared helpers (+ everything they call)
  _OVLA    : player, flow, textpage, objdraw, main.c   (game session, drawing, UI)
  _OVLB    : objects.inc                               (object behaviour)

Calls between areas are only allowed through the tiny wrappers listed in ALLOWED.
Usage: split_overlays.py in.asm out.asm
"""
import re
import sys
from collections import defaultdict

FILE_AREA = {'audio': 'R', 'video': 'R', 'level': 'R', 'common': 'R',
             'player': 'A', 'flow': 'A', 'textpage': 'A', 'objdraw': 'A', 'main': 'A',
             'objects': 'B', 'fastcull': 'R'}
# helpers shared by both overlays live in the resident area
RESIDENT_SEEDS = {'main', 'obj_update_all_t', 'fx_spawn', 'score_add', 'row_flags', 'set_form',
                  'player_heal', 'player_hurt', 'touch', 'is_near', 'kill_obj', 'killable_dmg'}
ALLOWED = {('main', 'run_session'), ('obj_update_all_t', 'obj_update_near')}
AREA_DIRECTIVE = {'R': '\t.area _CODE', 'A': '\t.area _OVLA', 'B': '\t.area _OVLB'}


def main(src, dst):
    lines = open(src).read().split('\n')
    area_idx = [i for i, l in enumerate(lines) if l.strip() == '.area _CODE']
    start, end = area_idx[0] + 1, area_idx[1]
    hdr = [i for i in range(start, end) if lines[i].startswith('; Function ')]
    starts = []
    for h in hdr:
        s = h
        while s - 1 >= start and lines[s - 1].startswith(';'):
            s -= 1
        starts.append(s)
    # block boundaries: function starts and data labels
    data_label = re.compile(r'^(_\w+)::?$')
    cuts = set(starts)
    for i in range(start, end):
        m = data_label.match(lines[i])
        if m and i + 1 < end and re.match(r'\t\.(db|dw|ascii|ds|byte|word)\b', lines[i + 1]):
            cuts.add(i)
    cuts = sorted(cuts | {start})
    blocks = []                         # (kind, name, file, lines)
    for k, c in enumerate(cuts):
        e = cuts[k + 1] if k + 1 < len(cuts) else end
        body = lines[c:e]
        if c in starts:
            idx = starts.index(c)
            name = lines[hdr[idx]].split()[2]
            fm = re.search(r';src/(\w+)\.(?:inc|c):', '\n'.join(body))
            blocks.append(['F', name, fm.group(1) if fm else 'main', body])
        else:
            blocks.append(['D', None, None, body])
    # call graph
    names = {b[1] for b in blocks if b[0] == 'F'}
    calls = defaultdict(set)
    for kind, name, f, body in blocks:
        if kind != 'F':
            continue
        txt = '\n'.join(body)
        for m in re.finditer(r'\b(?:call|jp|ld\s+\w+,)\s*(?:[A-Za-z]+,\s*)?#?_(\w+)', txt):
            if m.group(1) in names and m.group(1) != name:
                calls[name].add(m.group(1))
    area = {}
    for kind, name, f, body in blocks:
        if kind == 'F':
            area[name] = 'R' if name in RESIDENT_SEEDS else FILE_AREA.get(f, 'A')
    changed = True
    while changed:                      # everything resident code calls must be resident too
        changed = False
        for n in [n for n in area if area[n] == 'R']:
            for c in calls[n]:
                if (n, c) in ALLOWED:
                    continue
                if area[c] != 'R':
                    area[c] = 'R'
                    changed = True
                    print(f'  note: {c} made resident (called from {n})', file=sys.stderr)
    bad = []
    for n, cs in calls.items():
        for c in cs:
            if area[n] != area[c] and area[c] != 'R' and (n, c) not in ALLOWED:
                bad.append((n, area[n], c, area[c]))
    if bad:
        for b in bad:
            print('error: cross-overlay call %s(%s) -> %s(%s)' % b, file=sys.stderr)
        sys.exit(1)
    out = lines[:start]
    cur = 'R'
    out.append(AREA_DIRECTIVE['R'])
    sizes = defaultdict(int)
    for kind, name, f, body in blocks:
        a = area[name] if kind == 'F' else 'R'
        if a != cur:
            out.append(AREA_DIRECTIVE[a])
            cur = a
        out.extend(body)
        sizes[a] += len(body)
    if cur != 'R':
        out.append(AREA_DIRECTIVE['R'])
    out.extend(lines[end:])
    open(dst, 'w').write('\n'.join(out))
    cnt = defaultdict(int)
    for n, a in area.items():
        cnt[a] += 1
    print('split: ' + ', '.join('%s=%d functions' % (k, v) for k, v in sorted(cnt.items())), file=sys.stderr)


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
