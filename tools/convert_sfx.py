#!/usr/bin/env python3
"""VCL digital sound effects (6 kHz 8-bit PCM) -> per-frame PSG effect streams.

Frame stream (60 Hz): 0xFF ends.  Otherwise header = att<<4 | noise<<3 | x
  tone  (noise=0): x = period>>8, followed by period&255
  noise (noise=1): x = noise control (bit2 white, bits0-1 rate), no extra byte
"""
from __future__ import annotations
import numpy as np
from pathlib import Path

PSG_CLK = 3579545
FPS = 60


def analyse(pcm: bytes, rate: int):
    a = np.frombuffer(pcm, dtype=np.uint8).astype(float) - 128.0
    a -= a.mean()
    spf = rate / FPS
    nfr = int(np.ceil(len(a) / spf))
    peak = max(1.0, np.abs(a).max())
    frames = []
    for f in range(nfr):
        seg = a[int(f * spf):int((f + 1) * spf)]
        win = a[max(0, int(f * spf) - 40):int((f + 1) * spf) + 40]       # wider window for pitch
        if len(seg) < 4:
            seg = np.pad(seg, (0, 4 - len(seg)))
        rms = float(np.sqrt(np.mean(seg ** 2)))
        amp = min(1.0, rms / (peak * 0.45))
        # autocorrelation pitch (60 Hz .. 2.5 kHz)
        w = win - win.mean()
        e = float(np.dot(w, w)) + 1e-9
        best, blag = 0.0, 0
        ac = np.correlate(w, w, 'full')[len(w) - 1:] / e
        neg = np.where(ac < 0)[0]
        if len(neg) and e > 1e-3:
            start = int(neg[0])
            hi = min(len(ac) - 1, int(rate / 60))
            if hi > start + 1:
                k = int(np.argmax(ac[start:hi]))
                best, blag = float(ac[start + k]), start + k
                if blag < 2:
                    best, blag = 0.0, 0
        zc = float(np.sum(np.abs(np.diff(np.sign(seg))) > 0)) / max(1, len(seg)) * rate / 2
        frames.append((amp, best, rate / blag if blag else 0.0, zc))
    return frames


def render(pcm: bytes, rate: int) -> bytes:
    fr = analyse(pcm, rate)
    out = bytearray()
    prev_f = 0.0
    for amp, per, f0, zc in fr:
        att = 15 - int(round(amp ** 0.7 * 15)) if amp > 0.03 else 15
        att = max(0, min(15, att))
        if att == 15:
            out.append(0xF8)           # silent noise frame
            continue
        if per > 0.35 and 110 <= f0 <= 4000:
            if prev_f and abs(f0 - prev_f) / prev_f > 0.4:   # reject octave jumps
                f0 = prev_f
            prev_f = f0
            n = max(1, min(1023, int(round(PSG_CLK / (32 * f0)))))
            out += bytes([(att << 4) | (n >> 8), n & 255])
        else:
            rate_bits = 0 if zc > 1500 else (1 if zc > 800 else 2)
            if per > 0.35 and f0 < 110:
                rate_bits = 2
            out.append((att << 4) | 8 | 4 | rate_bits)
    # trim trailing silence
    out.append(0xFF)
    return bytes(out)


def describe(pcm: bytes, rate: int):
    fr = analyse(pcm, rate)
    tonal = sum(1 for a, p, f, z in fr if p > 0.35 and 110 <= f <= 4000 and a > 0.03)
    fs = [f for a, p, f, z in fr if p > 0.35 and a > 0.1 and 110 <= f <= 4000]
    return len(fr), tonal, (int(fs[0]), int(fs[len(fs) // 2]), int(fs[-1])) if fs else None


def convert_all(vcl, bw):
    rows = []
    for idx in sorted(vcl.sounds):
        pcm, rate = vcl.sounds[idx]
        blob = render(pcm, rate)
        sym = f'sfx_{idx:02d}'
        bank = bw.add(sym, blob, None)
        rows.append((sym, bank, len(blob)))
    return rows


if __name__ == '__main__':
    import sys
    sys.path.insert(0, str(Path(__file__).parent))
    import jillfmt as F
    v = F.parse_vcl(Path(sys.argv[1]) / 'JILL1.VCL')
    for i in sorted(v.sounds):
        pcm, rate = v.sounds[i]
        print(i, describe(pcm, rate), len(render(pcm, rate)))
