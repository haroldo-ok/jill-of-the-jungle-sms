#!/usr/bin/env python3
"""Static full-playability gate for the current WIP Jill SMS port.

This deliberately fails until the runtime systems needed for complete Jill
playability exist.  It is useful in CI because it prevents a smoke-testable ROM
from being mislabeled as a complete port.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
main_c = (ROOT / 'src' / 'main.c').read_text()
report = (ROOT / 'gen' / 'conversion_report.txt').read_text() if (ROOT / 'gen' / 'conversion_report.txt').exists() else ''
required_systems = {
    'collectibles/inventory': ['iType 1', 'APPLE', 'TOKEN', 'KEY'],
    'doors/locks': ['DOOR', 'iType 24'],
    'switches/bridges/lifts': ['SWITCH', 'BRIDGER', 'ELEV'],
    'enemies/hazards': ['BIGANT', 'PHOENIX', 'BOULDER'],
    'health/damage': ['health', 'damage'],
    'music/sfx': ['PSG', 'CMF', 'DDT'],
    'morph forms/water': ['frog', 'bird', 'fish', 'water'],
}
missing = []
for name, tokens in required_systems.items():
    if not any(tok in main_c for tok in tokens):
        missing.append(name)
if 'MAP.JN1' not in report or '1.JN1' not in report:
    missing.append('converted episode maps')
if missing:
    print('FAIL: ROM is not yet fully playable. Missing runtime systems:')
    for m in missing:
        print(' - ' + m)
    print('\nSmoke conversion/build may still pass; this gate is the honest full-playability check.')
    sys.exit(1)
print('PASS: full-playability static gate')
