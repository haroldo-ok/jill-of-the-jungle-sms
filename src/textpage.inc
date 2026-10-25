/* ------------------------------------------------------------------------
 * Text pages: scrollable, per-line coloured pages made from the original
 * 8x8 font (ASCII 32..127 expanded to three 2-plane variants = 288 tiles).
 * A line may start with a digit selecting its colour:
 *   1 yellow  2 light green  3 light cyan  4 light red  7 white  other: grey
 * ---------------------------------------------------------------------- */

#define TXT_PAL_SPR 0x0800

static const u8 txt_pal0[4] = { 0x00, 0x0F, 0x1D, 0x3D };   /* black, yellow, lt green, lt cyan */
static const u8 txt_pal1[4] = { 0x00, 0x17, 0x3F, 0x2A };   /* black, lt red, white, grey      */

static const u8 *tp_p0;      /* text start (in TEXT_BANK) */
static u8 tp_nl;             /* number of lines */
static u8 tp_menu_row, tp_menu_n, tp_sel;   /* menu mode: first item line, item count, current item */

static void tp_font(void) {
    u8 c, r, v;
    u8 buf[32];
    BANK(FONT_BANK);
    for (v = 0; v < 3; v++) {
        for (c = 0; c < 96; c++) {
            for (r = 0; r < 8; r++) {
                u8 b = font1bpp[(u16)c * 8 + r];
                u8 *o = buf + (r << 2);
                o[0] = (v != 1) ? b : 0;
                o[1] = (v != 0) ? b : 0;
                o[2] = 0; o[3] = 0;
            }
            SMS_loadTiles(buf, (u16)v * 96 + c, 32);
        }
    }
}

#define tp_lofs ((u16 *)hud_tiles)       /* line start offsets (HUD tile buffer is idle here) */

static void tp_index(void) {
    const u8 *s = tp_p0;
    tp_nl = 0;
    for (;;) {
        if (*s == 0 || *s == 0x1A || tp_nl >= 96) break;
        tp_lofs[tp_nl++] = (u16)(s - tp_p0);
        while (*s != '\n') { if (*s == 0 || *s == 0x1A) return; s++; }
        s++;
    }
}

static const u8 *tp_line(u8 n) {
    return n < tp_nl ? tp_p0 + tp_lofs[n] : 0;
}

static void tp_draw(u8 n) {
    const u8 *s = tp_line(n);
    u16 row = 0x7800 + ((u16)(n % 28) << 6);
    u8 x = 0, v = 2, pal = 1, c;
    if (s) {
        c = *s;
        if (c >= '1' && c <= '9') {
            s++;
            switch (c) {
            case '1': v = 0; pal = 0; break;
            case '2': v = 1; pal = 0; break;
            case '3': v = 2; pal = 0; break;
            case '4': v = 0; pal = 1; break;
            case '7': v = 1; pal = 1; break;
            default:  v = 2; pal = 1; break;
            }
        }
    }
    SMS_setAddr(row);
    SMS_setTile(0);                       /* column 0 is hidden by LEFTCOLBLANK */
    for (x = 1; x < 32; x++) {
        u16 t = 0;
        if (s && *s && *s != '\n' && *s != 0x1A) {
            c = *s++;
            if (c >= 32 && c < 128) t = (u16)v * 96 + (c - 32);
            if (pal) t |= TXT_PAL_SPR;
        }
        SMS_setTile(t);
    }
}

/* menu cursor: '>' in column 1 of the selected item line */
static void tp_cursor(void) {
    u8 i;
    for (i = 0; i < tp_menu_n; i++) {
        SMS_setAddr(0x7800 + ((u16)(tp_menu_row + i) << 6) + 2);
        SMS_setTile(i == tp_sel ? (u16)('>' - 32) : 0);
    }
}

/* show text page `idx`; returns the button that closed it (K_JUMP/K_FIRE) or 0 on timeout (frames; 0 = none) */
static u8 text_page(u8 idx, u16 timeout) {
    u8 i, tl = 0, rep = 0, res = 0, mx;
    u16 t = 0;
    u16 off;
    SMS_displayOff();
    SMS_initSprites();
    SMS_copySpritestoSAT();
    BANK(TEXT_BANK);
    off = game_text_off[idx];
    if (off == 0xFFFF) return K_JUMP;
    tp_p0 = game_texts + off;
    tp_index();
    SMS_VRAMmemset(0x3800, 0, 0x700);
    tp_font();
    BANK(TEXT_BANK);
    for (i = 0; i < 4; i++) {
        SMS_setBGPaletteColor(i, txt_pal0[i]);
        SMS_setSpritePaletteColor(i, txt_pal1[i]);
    }
    for (i = 4; i < 16; i++) { SMS_setBGPaletteColor(i, 0); SMS_setSpritePaletteColor(i, 0); }
    SMS_setBGScrollX(0); SMS_setBGScrollY(0);
    for (i = 0; i < 28; i++) tp_draw(i);
    mx = tp_nl > 24 ? tp_nl - 24 : 0;
    if (tp_menu_n) tp_cursor();
    SMS_displayOn();
    for (;;) {
        SMS_waitForVBlank();
        held = SMS_getKeysHeld();
        pressed = SMS_getKeysPressed();
        if (pressed & (K_JUMP | K_FIRE)) { res = (pressed & K_JUMP) ? K_JUMP : K_FIRE; break; }
        if (timeout && ++t >= timeout) break;
        if (tp_menu_n) {
            if (pressed & (K_UP | K_DOWN)) {
                if (pressed & K_DOWN) tp_sel = (u8)(tp_sel + 1 >= tp_menu_n ? 0 : tp_sel + 1);
                else tp_sel = (u8)(tp_sel ? tp_sel - 1 : tp_menu_n - 1);
                tp_cursor();
                t = 0;
            }
            continue;
        }
        if (rep) rep--;
        else if ((held & K_DOWN) && tl < mx) {
            tl++; rep = 3;
            SMS_setBGScrollY((u8)((tl * 8u) % 224));
            tp_draw(tl + 27);
        } else if ((held & K_UP) && tl) {
            tl--; rep = 3;
            SMS_setBGScrollY((u8)((tl * 8u) % 224));
            tp_draw(tl);
        }
        BANK(TEXT_BANK);
    }
    return res;
}
