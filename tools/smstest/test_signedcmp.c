/* Verify the SDCC signed-compare idiom on the harness Z80 core:
 *   ld a,lo1 / sub c / ld a,hi1 / sbc b / jp PO,$+5 / xor 0x80 / jp M,fail
 * Result byte written to 0xC000: 1 = compare says "less" (jump M taken),
 * 0 = "greater or equal".
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "z80.h"

static uint8_t mem[0x10000];
static uint8_t rd(void* u, uint16_t a){ (void)u; return mem[a]; }
static void wr(void* u, uint16_t a, uint8_t v){ (void)u; mem[a]=v; }
static uint8_t pin(z80* z, uint8_t p){ (void)z;(void)p; return 0xFF; }
static void pout(z80* z, uint8_t p, uint8_t v){ (void)z;(void)p;(void)v; }

static int test_ge(int lhs, int rhs) {
    /* returns 1 if the emulated sequence decides lhs < rhs (signed) */
    uint8_t prog[] = {
        0x01, (uint8_t)(rhs & 0xFF), (uint8_t)((rhs >> 8) & 0xFF), /* ld bc,rhs */
        0x3E, (uint8_t)(lhs & 0xFF),   /* ld a,lo(lhs) */
        0x91,                          /* sub c */
        0x3E, (uint8_t)((lhs>>8)&0xFF),/* ld a,hi(lhs) */
        0x98,                          /* sbc a,b */
        0xE2, 0x0E, 0x00,              /* jp PO,$000E */
        0xEE, 0x80,                    /* xor 0x80 */
        /* 000E: */
        0xFA, 0x15, 0x00,              /* jp M,$0015 */
        0x3E, 0x00,                    /* ld a,0  (lhs >= rhs) */
        0xC3, 0x17, 0x00,              /* jp $0017 */
        /* 0015: */
        0x3E, 0x01,                    /* ld a,1  (lhs < rhs) */
        /* 0017: */
        0x32, 0x00, 0xC0,              /* ld (0xC000),a */
        0x76                           /* halt */
    };
    memset(mem, 0, sizeof mem);
    memcpy(mem, prog, sizeof prog);
    z80 cpu; z80_init(&cpu);
    cpu.read_byte = rd; cpu.write_byte = wr;
    cpu.port_in = pin; cpu.port_out = pout;
    for (int i = 0; i < 200 && !cpu.halted; i++) z80_step(&cpu);
    return mem[0xC000];
}

int main(void) {
    struct { int a, b; } cases[] = {
        {77, 64}, {64, 77}, {66, 79}, {153, 144}, {132, 159},
        {0, 0}, {-5, 3}, {3, -5}, {-300, -200}, {32767, -32768},
        {130+23, 144}, {130+2, 144+15}
    };
    int bad = 0;
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        int expect = cases[i].a < cases[i].b;
        int got = test_ge(cases[i].a, cases[i].b);
        printf("%6d < %6d : expect %d got %d %s\n",
               cases[i].a, cases[i].b, expect, got, expect == got ? "" : "<-- BUG");
        if (expect != got) bad++;
    }
    return bad ? 1 : 0;
}
