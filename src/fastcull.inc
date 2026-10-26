/* ------------------------------------------------------------------------
 * Hand-written Z80 for the per-frame object loops (SDCC's code for these
 * byte loops was several hundred cycles per object).
 *
 *   cells_refresh()       ocol/orow = ox/oy >> 4 for every object on the active list
 *   cull_build(c0, r0)    active list = objects within 28x24 cells of (c0, r0), plus all lifts
 *   draw_filter(c0, r0)   draw list = active objects within 25x18 cells of (c0, r0), lifts excluded
 * ---------------------------------------------------------------------- */

u8 draw_l[MAXO], draw_n;

static void cells_refresh(void) __naked {
    __asm
        push ix
        ld   a, (_near_n)
        or   a
        jr   z, 9$
        ld   b, a
        ld   ix, #_near_l
1$:     ld   e, 0 (ix)           ; i
        ld   d, #0
        ld   hl, #_ox
        add  hl, de
        add  hl, de              ; &ox[i]
        ld   a, (hl)
        inc  hl
        ld   h, (hl)
        ld   l, a                ; ox[i]
        add  hl, hl
        add  hl, hl
        add  hl, hl
        add  hl, hl              ; H = (ox >> 4) & 0xFF (ox < 4096)
        ld   a, h
        ld   hl, #_ocol
        add  hl, de
        ld   (hl), a
        ld   hl, #_oy
        add  hl, de
        add  hl, de
        ld   a, (hl)
        inc  hl
        ld   h, (hl)
        ld   l, a
        add  hl, hl
        add  hl, hl
        add  hl, hl
        add  hl, hl
        ld   a, h
        ld   hl, #_orow
        add  hl, de
        ld   (hl), a
        inc  ix
        djnz 1$
9$:     pop  ix
        ret
    __endasm;
}

/* A = c0, L = r0 (sdcccall 1) */
static void cull_build(u8 c0, u8 r0) __naked {
    (void)c0; (void)r0;
    __asm
        push ix
        push iy
        ld   c, a                ; C = c0
        ld   b, l                ; B = r0
        ld   hl, #_ot
        ld   ix, #_ocol
        ld   iy, #_orow
        ld   de, #_near_l
        ld   a, (_num_obj)
        or   a
        jr   z, 9$
        exx
        ld   b, a                ; alt B = count
        ld   c, #0               ; alt C = index
        exx
1$:     ld   a, (hl)
        or   a
        jr   z, 5$               ; T_NONE
        cp   #61                 ; T_ELEV: always active
        jr   z, 4$
        ld   a, 0 (ix)
        sub  c
        cp   #28
        jr   nc, 5$
        ld   a, 0 (iy)
        sub  b
        cp   #24
        jr   nc, 5$
4$:     exx
        ld   a, c
        exx
        ld   (de), a
        inc  de
5$:     inc  hl
        inc  ix
        inc  iy
        exx
        inc  c
        dec  b
        exx
        jr   nz, 1$
9$:     ld   hl, #_near_l
        ex   de, hl
        or   a
        sbc  hl, de
        ld   a, l
        ld   (_near_n), a
        pop  iy
        pop  ix
        ret
    __endasm;
}

/* A = c0, L = r0.  Lifts in the window come first (draw_ne of them): they are drawn before Jill. */
u8 draw_ne;
static void draw_filter(u8 c0, u8 r0) __naked {
    (void)c0; (void)r0;
    __asm
        push ix
        push iy
        ld   c, a                ; C = c0
        ld   b, l                ; B = r0
        ld   iy, #_draw_l
        exx
        ld   c, #1               ; alt C = pass (1: lifts only, 0: everything else)
        exx
0$:     ld   ix, #_near_l
        ld   a, (_near_n)
        or   a
        jr   z, 6$
        exx
        ld   b, a                ; alt B = count
        exx
1$:     ld   e, 0 (ix)
        ld   d, #0
        ld   hl, #_ot
        add  hl, de
        ld   a, (hl)
        or   a
        jr   z, 5$
        sub  #61                 ; Z: a lift
        exx
        jr   z, 2$
        dec  c
        inc  c                   ; Z: pass 0
        exx
        jr   nz, 5$              ; not a lift, lifts pass: skip
        jr   3$
2$:     dec  c
        inc  c
        exx
        jr   z, 5$               ; a lift, pass 0: skip
3$:     ld   hl, #_ocol
        add  hl, de
        ld   a, (hl)
        sub  c
        cp   #25
        jr   nc, 5$
        ld   hl, #_orow
        add  hl, de
        ld   a, (hl)
        sub  b
        cp   #18
        jr   nc, 5$
        ld   0 (iy), e
        inc  iy
5$:     inc  ix
        exx
        dec  b
        exx
        jr   nz, 1$
6$:     exx
        ld   a, c
        dec  c
        exx
        or   a
        jr   z, 9$               ; both passes done
        push iy                  ; end of pass 1: number of lifts
        pop  hl
        ld   de, #_draw_l
        or   a
        sbc  hl, de
        ld   a, l
        ld   (_draw_ne), a
        jr   0$
9$:     push iy
        pop  hl
        ld   de, #_draw_l
        or   a
        sbc  hl, de
        ld   a, l
        ld   (_draw_n), a
        pop  iy
        pop  ix
        ret
    __endasm;
}
