#!/usr/bin/env python3
"""Extract test-relevant coordinates from the generated gen/bank*.c data:
transitions, items, and cells whose collision flags include vine/water/deadly.
Used to author the automated playtest scripts with real map coordinates."""
import re, sys, glob

MAP_W, MAP_H = 128, 64
JF = dict(SOLID=1, PLAT=2, VINE=4, WATER=8, TOUCH=16, DEADLY=32)

def load_arrays():
    arrays = {}
    for path in glob.glob('gen/bank*.c'):
        text = open(path).read()
        for m in re.finditer(r'\w+ (?:const )?(?:unsigned )?(?:char|int) (\w+)\[\] = \{([^;]*)\};',
                             text, re.S):
            name, body = m.group(1), m.group(2)
            vals = [int(v, 16) if v.lower().startswith('0x') else int(v, 10)
                    for v in re.findall(r'0x[0-9a-fA-F]+|-?\d+', body)]
            arrays[name] = vals
    return arrays

def main():
    lvl = sys.argv[1] if len(sys.argv) > 1 else 'lvl0_map'
    a = load_arrays()
    trans = a.get(f'{lvl}_trans', [])
    items = a.get(f'{lvl}_items', [])
    flags = a.get(f'{lvl}_flags', [])
    grid  = a.get(f'{lvl}_map', [])
    print(f'== {lvl} ==')
    print('transitions (x y w h target sx sy):')
    for i in range(0, len(trans), 7):
        print('  ', trans[i:i+7])
    print('items (x y type param):')
    for i in range(0, len(items), 4):
        print('  ', items[i:i+4])
    enemies = a.get(f'{lvl}_enemies', [])
    print('enemies (x y type xd):')
    for i in range(0, min(len(enemies), 40), 4):
        print('  ', enemies[i:i+4])
    for fname, bit in (('VINE', 4), ('WATER', 8), ('DEADLY', 32), ('PLAT', 2)):
        cells = []
        for idx, mt in enumerate(grid):
            if mt < len(flags) and (flags[mt] & bit):
                cells.append((idx % MAP_W, idx // MAP_W))
            if len(cells) >= 6:
                break
        print(f'{fname} cells (mx,my -> px,py):',
              [(c[0], c[1], c[0]*16, c[1]*16) for c in cells])

if __name__ == '__main__':
    main()
