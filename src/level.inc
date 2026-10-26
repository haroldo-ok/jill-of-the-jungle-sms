/* ------------------------------------------------------------------------
 * Level data access, name-table scrolling, level loading
 * ---------------------------------------------------------------------- */

#define BLANK_TILE 447
#define MAX_OV 64

u16 ov_cell[MAX_OV];
u8 ov_mt[MAX_OV];
u8 n_ov;

u8 nt_tx, nt_ty;                 /* camera tile position the name table represents */
u8 colq_n, rowq_n;
u16 colq[3], rowq[3];
u16 colbuf[3][28];
u16 rowbuf[3][32];
u8 need_full;

/* hub (MAP) persistence ------------------------------------------------------ */
#define HUB_MAXO 72
u8 hub_valid;
u8 hub_ot[HUB_MAXO], hub_ost[HUB_MAXO], hub_oct[HUB_MAXO];
u16 hub_ov_cell[MAX_OV];
u8 hub_ov_mt[MAX_OV], hub_n_ov;

u8 cur_entry;                    /* entrance id used to enter the current level */
int ret_px, ret_py;              /* where Jill re-appears on the MAP */

/* streamed text tiles (levels with more text tiles than fit in VRAM): a tile is uploaded
 * on demand into the slot the converter assigned to it */
#define PT_MAXS 280
#define PT_Q 24
u16 pt_cur[PT_MAXS];             /* id + 1 of the tile resident in each slot, 0 = none */
u16 ptq[PT_Q];
u8 ptq_n, pt_direct;

static void pt_load(u16 id) {
    u8 b = 0, core = ld->core_bank;
    u16 vt = ld->pt_vram + ld->pt_slot[id];
    while (id >= 480) { id -= 480; b++; }
    BANK(ld->pt_bank + b);
    SMS_loadTiles(ld->pt_tiles + ((u16)id << 5), vt, 32);
    BANK(core);
}

static void pt_flush(void) {
    u8 i;
    for (i = 0; i < ptq_n; i++) pt_load(ptq[i]);
    ptq_n = 0;
}

static u16 patch_word(u16 w) {
    u16 id = w & 0x1FFF, slot = ld->pt_slot[id];
    if (pt_cur[slot] != id + 1) {
        pt_cur[slot] = id + 1;
        if (pt_direct) pt_load(id);
        else if (ptq_n < PT_Q) ptq[ptq_n++] = id;
        else need_full = 1;
    }
    return (ld->pt_vram + slot) | ((w & 0x4000) >> 5) | ((w & 0x2000) >> 3);
}

static u8 mt_at(u8 mx, u8 my) {
    u16 idx = ((u16)my << 7) | mx;
    if (n_ov) {
        const u16 *c = ov_cell;
        u8 i = n_ov;
        do { if (*c == idx) return ov_mt[(u8)(c - ov_cell)]; c++; } while (--i);
    }
    return ld->map[idx];
}

const u8 *cur_map, *cur_mtflags;          /* = ld->map / ld->mtflags (core bank) */

/* collision flags at world pixel (x, y): solid outside the map sides/top, empty below it.
   Z80 version of: ld->mtflags[mt_at(x >> 4, y >> 4)]  (x in HL, y in DE, result in A) */
static u8 flag_at(int x, int y) __naked {
    (void)x; (void)y;
    __asm
        bit  7, h
        jr   nz, 8$              ; x < 0
        ld   a, h
        cp   #8
        jr   nc, 8$              ; x >= 2048
        bit  7, d
        jr   nz, 8$              ; y < 0
        ld   a, d
        cp   #4
        jr   nc, 9$              ; y >= 1024
        ld   a, l                ; C = x >> 4
        rrca
        rrca
        rrca
        rrca
        and  #0x0F
        ld   c, a
        ld   a, h
        add  a, a
        add  a, a
        add  a, a
        add  a, a
        or   c
        ld   c, a
        ld   a, e                ; B = y >> 4
        rrca
        rrca
        rrca
        rrca
        and  #0x0F
        ld   b, a
        ld   a, d
        add  a, a
        add  a, a
        add  a, a
        add  a, a
        or   b
        ld   b, a
        ld   a, (_n_ov)
        or   a
        jr   z, 1$
        ld   a, c
        ld   l, b
        call _mt_at
        jr   2$
1$:     ld   a, b                ; HL = my << 7 | mx
        srl  a
        ld   h, a
        ld   a, #0
        rra
        or   c
        ld   l, a
        ld   de, (_cur_map)
        add  hl, de
        ld   a, (hl)
2$:     ld   l, a
        ld   h, #0
        ld   de, (_cur_mtflags)
        add  hl, de
        ld   a, (hl)
        ret
8$:     ld   a, #1               ; MF_SOLID
        ret
9$:     xor  a
        ret
    __endasm;
}

static u16 tile_at(u16 wc, u16 wr) {
    u8 mt;
    if (wc >= 256 || wr >= 128) return BLANK_TILE;
    if (ld->nspan) {                                   /* baked text tiles */
        u16 key = (wr << 8) | wc, k;
        u8 lo = 0, hi = ld->nspan, mid;
        while (lo < hi) {
            mid = (lo + hi) >> 1;
            if (ld->sp_key[mid] <= key) lo = mid + 1; else hi = mid;
        }
        if (lo) {
            lo--;
            k = key - ld->sp_key[lo];
            if (k < ld->sp_off[lo + 1] - ld->sp_off[lo]) {
                k = ld->sp_tile[ld->sp_off[lo] + k];
                return (k & 0x8000) ? patch_word(k) : k;
            }
        }
    }
    mt = mt_at((u8)(wc >> 1), (u8)(wr >> 1));
    return ld->mtdef[((u16)mt << 2) + ((wr & 1) << 1) + (wc & 1)];
}

/* rows of 8x8 tiles that contain baked text spans (those take the slow per-tile path) */
u8 span_row[16];
static void span_rows_init(void) {
    u8 i;
    for (i = 0; i < 16; i++) span_row[i] = 0;
    for (i = 0; i < ld->nspan; i++) {
        u8 r = (u8)(ld->sp_key[i] >> 8);
        if (r < 128) span_row[r >> 3] |= (u8)(1 << (r & 7));
    }
}
#define SPAN_ROW(wr) (span_row[(u8)(wr) >> 3] & (u8)(1 << ((u8)(wr) & 7)))

/* 32 tiles of world row wr from world column wc, into the ring buffer buf[col & 31] */
static void fill_row(u16 *buf, u16 wr, u16 wc) {
    u8 c, idx = (u8)(wc & 31), mt = 0, my, sub;
    const u16 *def = ld->mtdef;
    const u8 *row;
    if (wr >= 128 || SPAN_ROW(wr)) {
        for (c = 0; c < 32; c++, wc++) { buf[idx] = tile_at(wc, wr); idx = (idx + 1) & 31; }
        return;
    }
    my = (u8)(wr >> 1);
    sub = (u8)((wr & 1) << 1);
    row = ld->map + ((u16)my << 7);
    for (c = 0; c < 32; c++, wc++) {
        u16 t = BLANK_TILE;
        if (wc < 256) {
            if (!c || !(wc & 1)) mt = n_ov ? mt_at((u8)(wc >> 1), my) : row[(u8)(wc >> 1)];
            t = def[((u16)mt << 2) + sub + (u8)(wc & 1)];
        }
        buf[idx] = t;
        idx = (idx + 1) & 31;
    }
}
/* 28 tiles of world column wc from world row wr, into the ring buffer buf[row % 28] */
static void fill_col(u16 *buf, u16 wc, u16 wr) {
    u8 r, idx, mt = 0, mx, sub = (u8)(wc & 1);
    const u16 *def = ld->mtdef;
    const u8 *map = ld->map;
    idx = (u8)(wr % 28);
    mx = (u8)(wc >> 1);
    for (r = 0; r < 28; r++, wr++) {
        u16 t = BLANK_TILE;
        if (wc < 256 && wr < 128) {
            if (SPAN_ROW(wr)) t = tile_at(wc, wr);
            else {
                if (!r || !(wr & 1)) mt = n_ov ? mt_at(mx, (u8)(wr >> 1)) : map[((wr >> 1) << 7) + mx];
                t = def[((u16)mt << 2) + ((u8)(wr & 1) << 1) + sub];
            }
        }
        buf[idx] = t;
        if (++idx == 28) idx = 0;
    }
}

static void build_col(u8 k, u16 wc) {
    fill_col(colbuf[k], wc, nt_ty);
}
static void build_row(u8 k, u16 wr) {
    fill_row(rowbuf[k], wr, (u16)nt_tx + 1);
}

static void stream_build(void) {
    u8 k;
    for (k = 0; k < colq_n; k++) build_col(k, colq[k]);
    for (k = 0; k < rowq_n; k++) build_row(k, rowq[k]);
}

static void stream_write(void) {
    u8 k, r;
    for (k = 0; k < colq_n; k++) {
        u8 col = (u8)(colq[k] & 31);
        for (r = 0; r < 28; r++) SMS_setTileatXY(col, r, colbuf[k][r]);
    }
    for (k = 0; k < rowq_n; k++)
        SMS_VRAMmemcpy(0x3800 + ((u16)(rowq[k] % 28) << 6), rowbuf[k], 64);
    colq_n = rowq_n = 0;
}

/* full redraw of the name table (display should be off) */
static void draw_all(void) {
    u8 r, c;
    u16 wr, wc, q;
    for (q = 0; q < PT_MAXS; q++) pt_cur[q] = 0;
    ptq_n = 0; pt_direct = 1;
    span_rows_init();
    wc = (u16)nt_tx + 1;
    c = (u8)(nt_ty % 28);
    for (r = 0; r < 28; r++) {
        wr = (u16)nt_ty + r;
        fill_row(rowbuf[0], wr, wc);
        SMS_VRAMmemcpy(0x3800 + ((u16)c << 6), rowbuf[0], 64);
        if (++c == 28) c = 0;
    }
    colq_n = rowq_n = 0;
    need_full = 0;
    pt_direct = 0;
}

/* re-render one 16x16 cell if it's inside the name-table window (call in vblank or display off) */
static void refresh_cell(u8 mx, u8 my) {
    u16 wc = (u16)mx << 1, wr = (u16)my << 1;
    u8 i, j;
    for (j = 0; j < 2; j++)
        for (i = 0; i < 2; i++) {
            u16 c = wc + i, r = wr + j;
            if (c > nt_tx && c <= (u16)nt_tx + 32 && r >= nt_ty && r < (u16)nt_ty + 28)
                SMS_setTileatXY((u8)(c & 31), (u8)(r % 28), tile_at(c, r));
        }
}

/* pending cell refreshes (applied in vblank) */
#define MAX_RF 6
u8 rf_x[MAX_RF], rf_y[MAX_RF], rf_n;
static void set_cell(u8 mx, u8 my, u8 mt) {
    u16 idx = ((u16)my << 7) | mx;
    u8 i;
    for (i = 0; i < n_ov; i++) if (ov_cell[i] == idx) break;
    if (i == n_ov) {
        if (n_ov >= MAX_OV) return;
        n_ov++;
    }
    ov_cell[i] = idx;
    ov_mt[i] = mt;
    if (rf_n < MAX_RF) { rf_x[rf_n] = mx; rf_y[rf_n] = my; rf_n++; } else need_full = 1;
}
static void apply_refresh(void) {
    u8 i;
    for (i = 0; i < rf_n; i++) refresh_cell(rf_x[i], rf_y[i]);
    rf_n = 0;
}

/* make the name table follow cam_x/cam_y (queues the edge columns/rows) */
static void camera_track(void) {
    u8 ntx, nty;
    int dx, dy;
    if (need_full) return;
    ntx = (u8)(cam_x >> 3);
    nty = (u8)(cam_y >> 3);
    dx = (int)ntx - (int)nt_tx; if (dx < 0) dx = -dx;
    dy = (int)nty - (int)nt_ty; if (dy < 0) dy = -dy;
    if (dx + dy + (int)colq_n + (int)rowq_n > 3) { need_full = 1; return; }
    while (nt_tx < ntx) { nt_tx++; colq[colq_n++] = (u16)nt_tx + 32; }
    while (nt_tx > ntx) { nt_tx--; colq[colq_n++] = (u16)nt_tx + 1; }
    while (nt_ty < nty) { nt_ty++; rowq[rowq_n++] = (u16)nt_ty + 27; }
    while (nt_ty > nty) { nt_ty--; rowq[rowq_n++] = (u16)nt_ty; }
}

static void camera_follow(void) {
    int sx = px - (int)cam_x;
    int sy = py - (int)cam_y;
    int nx = (int)cam_x, ny = (int)cam_y;
    if (sx < 88) nx = px - 88; else if (sx > 136) nx = px - 136;
    if (sy < 72) ny = py - 72; else if (sy > 112) ny = py - 112;
    if (nx < 0) nx = 0;
    if (nx > MAP_W * 16 - 256) nx = MAP_W * 16 - 256;
    if (ny < 0) ny = 0;
    if (ny > MAP_H * 16 - 192) ny = MAP_H * 16 - 192;
    cam_x = (unsigned int)nx;
    cam_y = (unsigned int)ny;
    camera_track();
}

static void camera_snap(void) {
    int nx = px - 112, ny = py - 88;
    if (nx < 0) nx = 0;
    if (nx > MAP_W * 16 - 256) nx = MAP_W * 16 - 256;
    if (ny < 0) ny = 0;
    if (ny > MAP_H * 16 - 192) ny = MAP_H * 16 - 192;
    cam_x = (unsigned int)nx;
    cam_y = (unsigned int)ny;
    nt_tx = (u8)(cam_x >> 3);
    nt_ty = (u8)(cam_y >> 3);
    colq_n = rowq_n = 0;
    need_full = 1;
}

/* ---- scroll registers ------------------------------------------------------- */
static void apply_scroll(void) {
    SMS_setBGScrollX((u8)(0 - cam_x));
    {   u16 v = cam_y;                   /* cam_y % 224 without a division */
        while (v >= 224) v -= 224;
        SMS_setBGScrollY((u8)v);
    }
}
