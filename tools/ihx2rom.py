#!/usr/bin/env python3
"""IHX -> Sega Master System ROM, for the bank layout used by this project.

Unlike devkitSMS' ihx2sms (which infers banks from the order of the data), the bank of every
record is taken from its Intel-HEX extended linear address:
  addr < 0x8000, ext 0        -> banks 0/1, the fixed first 32 KB
  0x8000..0xBFFF, ext = N     -> data bank N (mapped in ROM slot 2)
  0x4000..0x7FFF, ext = N>=2  -> code overlay bank N (mapped in ROM slot 1)
Also fills in the SDSC release date and the SEGA header checksum like ihx2sms does.
Usage: ihx2rom.py in.ihx out.sms
"""
import sys
import time

BANK = 0x4000


def main(src, dst):
    rom = bytearray(256 * BANK)
    size = 0
    ela = 0
    for line in open(src):
        line = line.strip()
        if not line.startswith(':'):
            continue
        n = int(line[1:3], 16)
        addr = int(line[3:7], 16)
        typ = int(line[7:9], 16)
        data = bytes.fromhex(line[9:9 + 2 * n])
        if typ == 4:
            ela = int.from_bytes(data, 'big')
        elif typ == 0:
            if ela == 0 and addr + n <= 0x8000:
                off = addr
            elif 0x8000 <= addr and addr + n <= 0xC000:
                off = ela * BANK + addr - 0x8000
            elif 0x4000 <= addr and addr + n <= 0x8000 and ela >= 2:
                off = ela * BANK + addr - 0x4000
            else:
                sys.exit('ihx2rom: record at ext %04X addr %04X (+%d) does not fit a bank' % (ela, addr, n))
            rom[off:off + n] = data
            size = max(size, off + n)
    size = (size + BANK - 1) // BANK * BANK
    full = 1
    while full < size:
        full *= 2
    size = max(full, 32 * 1024)
    if rom[0x7FE0:0x7FE4] == b'SDSC' and rom[0x7FE6:0x7FEA] == b'\0\0\0\0':
        t = time.localtime(int(__import__('os').environ.get('SOURCE_DATE_EPOCH', time.time())))
        bcd = lambda v: (v // 10) * 16 + v % 10
        rom[0x7FE6:0x7FEA] = bytes([bcd(t.tm_mday), bcd(t.tm_mon), bcd(t.tm_year % 100), bcd(t.tm_year // 100)])
    if rom[0x7FF0:0x7FF8] == b'TMR SEGA':
        cs = sum(rom[:0x7FF0])
        rom[0x7FFA] = cs & 0xFF
        rom[0x7FFB] = (cs >> 8) & 0xFF
    else:
        print('ihx2rom: warning: SEGA header not found')
    open(dst, 'wb').write(bytes(rom[:size]))
    print('ihx2rom: %d KB ROM (%d banks used)' % (size // 1024, (max(1, size) + BANK - 1) // BANK))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
