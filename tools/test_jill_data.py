#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
report = ROOT / 'gen' / 'conversion_report.txt'
if not report.exists():
    print('missing gen/conversion_report.txt; run make assets first', file=sys.stderr)
    sys.exit(1)
lines = [ln for ln in report.read_text().splitlines() if ln.strip()]
assert lines, 'no converted levels reported'
assert any('MAP.JN1' in ln for ln in lines), 'MAP.JN1 was not converted'
assert any('1.JN1' in ln for ln in lines), '1.JN1 was not converted'
for ln in lines:
    m_mt = re.search(r'mt=\s*(\d+)', ln)
    m_tb = re.search(r'tiles_bytes=\s*(\d+)', ln)
    m_tr = re.search(r'trans=\s*(\d+)', ln)
    assert m_mt and int(m_mt.group(1)) <= 128, f'metatile index no longer fits u8 map: {ln}'
    assert m_tb and int(m_tb.group(1)) <= 16 * 1024, f'tile bank too large: {ln}'
    assert m_tr is not None, f'transition count missing: {ln}'
print(f'PASS: converted {len(lines)} Jill maps; metatile counts and bank sizes fit current SMS layout')
