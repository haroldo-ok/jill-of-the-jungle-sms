/* ------------------------------------------------------------------------
 * Video: palettes/fades, dynamic sprite-frame cache, Jill frame streaming,
 * sprite-based HUD, vblank work.
 * ---------------------------------------------------------------------- */

#define NCELL 37                 /* pool = 148 tiles = 37 cells of 4 tiles */
#define UPQ 12

static const u8 *pal_bg, *pal_spr;
u8 fc_state[SPR_COUNT];   /* 0xFF = absent, else cell | 0x80 while upload pending */
u8 fc_age[SPR_COUNT];
u8 cell_own[NCELL];       /* owning frame id + 1, 0 = free */
u8 upq[UPQ], upq_n;
u8 fctr;                  /* frame counter used for LRU */
u8 jill_cur = 0xFF, jill_want = 0xFF;
u8 hud_dirty;
u8 hud_tiles[HUD_TILES * 32];
u8 sprites_dropped;

/* ---- palette fading ---------------------------------------------------- */
static u8 dim_col(u8 c, u8 lvl) {          /* lvl 3 = full colour, 0 = black */
    u8 r = c & 3, g = (c >> 2) & 3, b = (c >> 4) & 3, d = 3 - lvl;
    r = r > d ? r - d : 0;
    g = g > d ? g - d : 0;
    b = b > d ? b - d : 0;
    return (u8)(r | (g << 2) | (b << 4));
}
static void pal_set(u8 lvl) {
    u8 i;
    for (i = 0; i < 16; i++) {
        SMS_setBGPaletteColor(i, dim_col(pal_bg[i], lvl));
        SMS_setSpritePaletteColor(i, dim_col(pal_spr[i], lvl));
    }
}
static void fade_to(u8 lvl_from, u8 lvl_to) {
    s8 step = lvl_to > lvl_from ? 1 : -1;
    u8 l = lvl_from;
    for (;;) {
        pal_set(l);
        SMS_waitForVBlank(); SMS_waitForVBlank(); SMS_waitForVBlank();
        if (l == lvl_to) break;
        l = (u8)(l + step);
    }
}

/* ---- sprite frame cache -------------------------------------------------- */
static void fc_reset(void) {
    u8 i;
    for (i = 0; i < SPR_COUNT; i++) { fc_state[i] = 0xFF; fc_age[i] = 0; }
    for (i = 0; i < NCELL; i++) cell_own[i] = 0;
    upq_n = 0;
    jill_cur = 0xFF;
}

/* ensure frame is present; returns tile index of its first tile, or 0xFFFF if not yet uploaded */
static u16 fc_miss(u8 fid);
static u16 fc_get(u8 fid) {                 /* hit path kept small (no big stack frame) */
    u8 st = fc_state[fid];
    if (st == 0xFF) return fc_miss(fid);
    fc_age[fid] = fctr;
    if (st & 0x80) return 0xFFFF;
    return T_SPR0 + ((u16)st << 2);
}
static u16 fc_miss(u8 fid) {
    const SFrame *sf;
    u8 n, best, bestcost, start, k, cost, f;
    u8 ncells;
    sf = &ld->sframes[fid];
    if (!sf->p) return 0xFFFF;
    n = (u8)((sf->cb >> 4) * (sf->cb & 15) * 2);          /* tiles */
    ncells = (u8)((n + 3) >> 2);
    best = 0xFF; bestcost = 255;
    for (start = 0; start + ncells <= NCELL; start++) {
        cost = 0;
        for (k = 0; k < ncells; k++) {
            f = cell_own[start + k];
            if (f) {
                if (fc_age[f - 1] == fctr) { cost = 200; break; }   /* in use this frame */
                cost++;
            }
        }
        if (cost < bestcost) { bestcost = cost; best = start; if (!cost) break; }
    }
    if (best == 0xFF || bestcost >= 200 || upq_n >= UPQ) return 0xFFFF;
    for (k = 0; k < ncells; k++) {                          /* evict */
        f = cell_own[best + k];
        if (f) {
            u8 c;
            for (c = 0; c < NCELL; c++) if (cell_own[c] == f) cell_own[c] = 0;
            fc_state[f - 1] = 0xFF;
        }
    }
    for (k = 0; k < ncells; k++) cell_own[best + k] = (u8)(fid + 1);
    fc_state[fid] = best | 0x80;
    fc_age[fid] = fctr;
    upq[upq_n++] = fid;
    return 0xFFFF;
}

static void fc_upload(u8 budget) {
    u8 sb = cur_bank, i = 0, n, fid, bk;
    const SFrame *sf;
    const u8 *src;
    while (i < upq_n && budget) {
        fid = upq[i];
        if (fc_state[fid] == 0xFF || !(fc_state[fid] & 0x80)) { i++; continue; }     /* evicted meanwhile */
        BANK(ld->core_bank);
        sf = &ld->sframes[fid];
        n = (u8)((sf->cb >> 4) * (sf->cb & 15) * 2);
        if (n > budget && budget < 8) break;
        bk = sf->bank; src = sf->p;
        BANK(bk);
        SMS_loadTiles(src, T_SPR0 + ((u16)(fc_state[fid] & 0x7F) << 2), (u16)n * 32);
        fc_state[fid] &= 0x7F;
        budget = n >= budget ? 0 : budget - n;
        i++;
    }
    if (i) {
        for (n = i; n < upq_n; n++) upq[n - i] = upq[n];
        upq_n -= i;
    }
    BANK(sb);
}

/* ---- fast sprite emitter: writes straight into SMSlib's sprite table ------------------
 * se_cols x se_bands 8x16 sprites from tile se_t (step 2), top-left (se_x, se_y).
 * Same clipping as SMS_addSpriteClipping with the default window: x 0..255, y -15..191. */
int se_x, se_y;
u8 se_t, se_cols, se_bands;
extern unsigned char SpriteTableY[], SpriteTableXN[], SpriteNextFree;

u8 se_cx, se_cl, se_room;
u8 hud_spr_ok;                      /* HUD sprites (table entries 0..5) already written */
static void spr_emit(void) __naked {
    __asm
        ld   a, (_se_cols)
        or   a
        ret  z
        push ix
        push iy
        ld   (_se_cl), a         ; columns left
        ld   a, (_SpriteNextFree)
        ld   c, a
        ld   a, #64
        sub  c
        jr   nc, 0$
        xor  a
0$:     ld   (_se_room), a       ; free table entries
        ld   b, #0
        ld   iy, #_SpriteTableY
        add  iy, bc
        ld   ix, #_SpriteTableXN
        add  ix, bc
        add  ix, bc
        ld   a, (_se_t)
        ld   c, a                ; C = tile
        ld   hl, (_se_x)
1$:     push hl                  ; column: x in HL
        ld   a, h
        or   a
        jr   nz, 7$              ; x < 0 or x > 255: skip the column (tiles still advance)
        ld   a, l
        ld   (_se_cx), a
        ld   a, (_se_bands)
        ld   b, a
        ld   de, (_se_y)
2$:     ld   hl, #15             ; band: y in DE
        add  hl, de
        ld   a, h
        or   a
        jr   nz, 5$              ; y < -15 or y > 240
        ld   a, l
        cp   #207
        jr   nc, 5$              ; y > 191
        ld   a, (_se_room)
        or   a
        jr   z, 6$
        dec  a
        ld   (_se_room), a
        ld   a, e
        dec  a
        ld   0 (iy), a           ; Y = y - 1
        inc  iy
        ld   a, (_se_cx)
        ld   0 (ix), a
        ld   1 (ix), c
        inc  ix
        inc  ix
5$:     inc  c
        inc  c
        ld   hl, #16
        add  hl, de
        ex   de, hl
        djnz 2$
        jr   8$
6$:     ld   hl, #_sprites_dropped
        inc  (hl)
        jr   5$
7$:     ld   a, (_se_bands)
        add  a, a
        add  a, c
        ld   c, a
8$:     pop  hl
        ld   de, #8
        add  hl, de
        ld   a, (_se_cl)
        dec  a
        ld   (_se_cl), a
        jr   nz, 1$
        push iy                  ; SpriteNextFree = entries used
        pop  hl
        ld   de, #_SpriteTableY
        or   a
        sbc  hl, de
        ld   a, l
        ld   (_SpriteNextFree), a
        pop  iy
        pop  ix
        ret
    __endasm;
}


/* draw a cached frame with its top-left at screen position (x,y) */
const SFrame *cur_sframes;          /* = ld->sframes */
u8 ds_fid;
/* draw a cached frame with its top-left at screen position (x,y).
   Z80 (A = fid, DE = x, y on the stack, callee pops it) of:
     x += sf->xo; y += sf->yo; if (x < -32 || x > 255 || y < -32 || y > 191) return;
     t = fc_get(fid); if (t == 0xFFFF) return; emit sf->cb sprites from tile t */
static void draw_spr(u8 fid, int x, int y) __naked {
    (void)fid; (void)x; (void)y;
    __asm
        ld   (_ds_fid), a
        ld   (_se_x), de
        pop  hl                  ; return address
        pop  bc                  ; y
        push hl
        ld   (_se_y), bc
        ld   l, a                ; HL = cur_sframes + fid * 6
        ld   h, #0
        ld   e, l
        ld   d, h
        add  hl, hl
        add  hl, de
        add  hl, hl
        ld   de, (_cur_sframes)
        add  hl, de
        push hl                  ; sf
        inc  hl
        inc  hl
        inc  hl
        ld   a, (hl)             ; xo
        inc  hl
        ld   c, (hl)             ; yo
        ld   e, a
        rla
        sbc  a, a
        ld   d, a
        ld   hl, (_se_x)
        add  hl, de
        ld   (_se_x), hl
        ld   de, #32
        add  hl, de
        ld   de, #288
        or   a
        sbc  hl, de              ; -32 <= x <= 255 ?
        jr   nc, 9$
        ld   a, c
        ld   e, a
        rla
        sbc  a, a
        ld   d, a
        ld   hl, (_se_y)
        add  hl, de
        ld   (_se_y), hl
        ld   de, #32
        add  hl, de
        ld   de, #224
        or   a
        sbc  hl, de              ; -32 <= y <= 191 ?
        jr   nc, 9$
        ld   a, (_ds_fid)
        call _fc_get             ; DE = first tile or 0xFFFF
        ld   a, d
        and  e
        inc  a
        jr   z, 9$
        ld   a, e
        ld   (_se_t), a
        pop  hl
        ld   de, #5
        add  hl, de
        ld   a, (hl)             ; cb: cols << 4 | bands
        ld   c, a
        and  #0x0F
        ld   (_se_bands), a
        ld   a, c
        rrca
        rrca
        rrca
        rrca
        and  #0x0F
        ld   (_se_cols), a
        jp   _spr_emit
9$:     pop  hl
        ret
    __endasm;
}

/* ---- Jill: 8 tiles streamed from ROM ----------------------------------- */
static void jill_upload(void) {
    u8 sb = cur_bank, slot;
    if (jill_want == jill_cur || jill_want == 0xFF) return;
    slot = jill_slot[jill_want];
    if (slot == 0xFF) { jill_cur = jill_want; return; }
    BANK(JILL_BANK);
    SMS_loadTiles(jill_gfx + ((u16)slot << 8), T_JILL, 256);
    BANK(sb);
    jill_cur = jill_want;
}

static void draw_jill(int x, int y) {
    /* x,y = top-left of the 16x32 picture */
    if (x < -16 || x > 255 || y < -32 || y > 191) return;
    if (jill_cur == 0xFF) return;
    se_x = x; se_y = y; se_t = 0; se_cols = 2; se_bands = 2;   /* tiles 0,2 | 4,6 (column-major) */
    spr_emit();
}

/* ---- sprite HUD: six 8x16 sprites, text rendered into tiles ------------- */
static const u8 hglyph[19][5] = {
    {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
    {7,4,7,1,7},{7,4,7,5,7},{7,1,2,2,2},{7,5,7,5,7},{7,5,7,1,7},
    {5,7,7,2,0},   /* 10 heart */
    {6,6,2,3,2},   /* 11 key */
    {2,7,7,2,0},   /* 12 gem */
    {1,2,6,4,0},   /* 13 knife */
    {5,2,7,2,5},   /* 14 blade */
    {2,7,2,2,2},   /* 15 jump */
    {7,4,6,4,4},   /* 16 frog 'F' */
    {6,5,6,4,4},   /* 17 bird 'P' */
    {0,5,2,0,0}    /* 18 fish '~' */
};
u8 hud_bg, hud_c[4];

static void hud_pix(u8 x, u8 y, u8 col) {
    u8 *p = hud_tiles + (((u16)(x >> 3) * 2 + (y >> 3)) << 5) + ((y & 7) << 2);
    u8 m = (u8)(0x80 >> (x & 7));
    u8 i;
    for (i = 0; i < 4; i++) {
        if ((col >> i) & 1) p[i] |= m; else p[i] &= (u8)~m;
    }
}
static void hud_glyph(u8 x, u8 y, u8 g, u8 col) {
    /* 3-pixel rows never straddle a tile (x % 8 <= 5): update the bit-planes a row at a time */
    u8 r, yy, m, i, sh = (u8)(5 - (x & 7));
    u8 *p;
    for (r = 0; r < 5; r++) {
        m = (u8)(hglyph[g][r] << sh);
        if (!m) continue;
        yy = (u8)(y + r);
        p = hud_tiles + (((u16)(x >> 3) * 2 + (yy >> 3)) << 5) + ((yy & 7) << 2);
        for (i = 0; i < 4; i++) {
            if ((col >> i) & 1) p[i] |= m; else p[i] &= (u8)~m;
        }
    }
}
static void hud_num(u8 x, u8 y, u16 v, u8 digits, u8 col) {
    u8 i, d[6];
    for (i = 0; i < digits; i++) { d[digits - 1 - i] = (u8)(v % 10); v /= 10; }
    for (i = 0; i < digits; i++) hud_glyph(x + (i << 2), y, d[i], col);
}
static void hud_render(void) {
    u8 sb = cur_bank, iw = hud_c[0];
    u16 sc;
    {   /* solid background: every pixel row of every tile gets the same 4 plane bytes */
        u8 *q = hud_tiles, p0 = (hud_bg & 1) ? 0xFF : 0, p1 = (hud_bg & 2) ? 0xFF : 0,
           p2 = (hud_bg & 4) ? 0xFF : 0, p3 = (hud_bg & 8) ? 0xFF : 0;
        u16 n;
        for (n = 0; n < HUD_TILES * 8; n++) { *q++ = p0; *q++ = p1; *q++ = p2; *q++ = p3; }
    }
    sc = score_lo;
    hud_num(1, 1, score_hi, 2, iw);
    hud_num(9, 1, sc, 4, iw);
    hud_glyph(29, 1, 10, hud_c[2]);
    hud_num(33, 1, health, 1, iw);
    if (form) hud_glyph(41, 1, (u8)(15 + form), hud_c[1]);
    hud_glyph(1, 9, 11, hud_c[1]);  hud_num(5, 9, keys, 1, iw);
    hud_glyph(13, 9, 12, iw);       hud_num(17, 9, gems, 1, iw);
    hud_glyph(25, 9, 13, hud_c[3]); hud_num(29, 9, knives, 1, iw);
    if (blades) { hud_glyph(41, 9, 14, iw); }
    BANK(sb);
    SMS_loadTiles(hud_tiles, T_HUD, HUD_TILES * 32);
    hud_dirty = 0;
}
static void hud_draw(void) {
    se_x = 8; se_y = 0; se_t = T_HUD; se_cols = 6; se_bands = 1;
    spr_emit();
}
