/* ------------------------------------------------------------------------
 * Object drawing and initialisation
 * ---------------------------------------------------------------------- */

static u8 anim_frame(u8 a, u8 i, u8 right) {
    const Anim *an = &anims[a];
    u8 k = ofr[i] >> 3, n = an->n;
    while (k >= n) k -= n;
    return right ? an->r[k] : an->l[k];
}

int g_sx, g_sy;                    /* screen position of the object being drawn */

/* draws object i at (g_sx, g_sy) (set and range-checked by the caller) */
static const u8 dcls[102] = { [T_TOKEN] = 1, [T_BEES] = 2, [T_GATOR] = 3, [T_SNAKE] = 4, [T_KNIGHT] = 5, [T_MEGA] = 6, [T_SWITCH] = 7, [T_NEST] = 8, [T_STALAG] = 9, [T_EYE] = 10, [T_MIST] = 11, [T_DOOR] = 12, [T_FALLDOOR] = 12, [T_BRIDGER] = 12, [T_PAD] = 12, [T_CHECK] = 12, [T_ELEV] = 13 };   /* type -> dense case number (jump table) */
static void draw_obj(u8 i) {
    u8 t = ot[i], a, f, k, right;
    int sx = g_sx, sy = g_sy;
    if (t == T_NONE) return;
    right = ovx[i] > 0;
    switch (t < 102 ? dcls[t] : 0) {
    case 1:   /* T_TOKEN */
        switch ((u8)otag[i]) {
        case 4: f = SPR_TOK4; break;
        case 5: f = SPR_TOK5; break;
        case 7: f = SPR_TOK7; break;
        case 8: f = SPR_TOK8; break;
        case 9: f = SPR_TOK9; break;
        default: f = SPR_TOK0; break;
        }
        draw_spr(f, sx, sy + bob3_tab[(ofr[i] >> 2) & 15]);
        return;
    case 2:   /* T_BEES */
        draw_spr(SPR_BEES + anim_frame(ANIM_BEES, i, right), sx, sy);
        return;
    case 3:   /* T_GATOR */
        f = anim_frame(ANIM_GATOR, i, right);
        if (right) { draw_spr(SPR_GATOR + f - 4, sx, sy); draw_spr(SPR_GATOR + f, sx + 32, sy); }
        else       { draw_spr(SPR_GATOR + f, sx, sy); draw_spr(SPR_GATOR + f - 4, sx + 32, sy); }
        return;
    case 4:   /* T_SNAKE */
        f = anim_frame(ANIM_SNAKE, i, right);
        if (right) draw_spr(SPR_SNAKE + f, sx + 40, sy);
        else       draw_spr(SPR_SNAKE + f, sx, sy);
        {   /* body segments */
            u8 fr = ofr[i], ph = (u8)(fr >> 1);
            int bx = right ? sx : sx + 16, by = sy + 12;
            a = (u8)(fr >> 3);
            while (a >= 6) a -= 6;
            for (k = 0; k < 5; k++) {
                draw_spr(SPR_SNAKE + 6 + a, bx, by - (bob_tab[ph & 15] >> 2));
                bx += 8; ph += 2;
                if (++a == 6) a = 0;
            }
        }
        return;
    case 5:   /* T_KNIGHT */
        if (ost[i] && touch(i, -10)) f = 2 + ((ofr[i] >> 2) % 3);
        else f = ost[i] ? ((ofr[i] >> 4) & 1) : 1;
        draw_spr(SPR_KNIGHT + f, sx, sy);
        return;
    case 6:   /* T_MEGA */
        f = (u8)oxd[i];
        if (f > 4) f = 0;
        draw_spr(SPR_MEGA + f, sx, sy);
        return;
    case 7:   /* T_SWITCH */
        draw_spr(SPR_SWITCH + (ost[i] & 1), sx, sy);
        return;
    case 8:   /* T_NEST */
        f = ost[i] ? 1 + ((ofr[i] >> 3) % 3) : 0;
        draw_spr(SPR_NEST + f, sx, sy);
        return;
    case 9:   /* T_STALAG */
        f = 0;
        if (ost[i] == 2) { f = 1 + ((24 - oct[i]) >> 3); if (f > 4) f = 4; }
        draw_spr(SPR_STALAG + f, sx, sy);
        return;
    case 10:   /* T_EYE */
        draw_spr(SPR_EYE + (ost[i] ? 1 : 0), sx, sy);
        return;
    case 11:   /* T_MIST */
        draw_spr(SPR_MIST + anim_frame(ANIM_MIST, i, 0), sx, sy);
        return;
    case 12:   /* T_DOOR T_FALLDOOR T_BRIDGER T_PAD T_CHECK */
        return;
    case 13:   /* T_ELEV */
        if (ovy[i]) draw_spr(SPR_POLE, sx, sy + 16);     /* shaft piece follows a moving lift */
        draw_spr(SPR_ELEV, sx, sy);
        return;
    }
    if (t < 67 && (t_spr[t] || t == T_APPLE)) {
        a = t_anim[t];
        f = t_spr[t];
        if (a) f += anim_frame(a - 1, i, right);
        draw_spr(f, sx, sy);
    }
}

/* draw-list entries [A, A+L): screen position, exact range check, draw (Z80; see fastcull.inc) */
static void draw_objs(u8 from, u8 n) __naked {
    (void)from; (void)n;
    __asm
        ld   b, l
        inc  l
        dec  l
        ret  z
        ld   e, a
        ld   d, #0
        ld   hl, #_draw_l
        add  hl, de
1$:     push bc
        push hl
        ld   e, (hl)
        ld   d, #0
        ld   hl, #_ox
        add  hl, de
        add  hl, de
        ld   c, (hl)
        inc  hl
        ld   b, (hl)
        push de
        ld   de, (_cam_x)
        ld   h, b
        ld   l, c
        or   a
        sbc  hl, de              ; sx = ox - cam_x
        ld   (_g_sx), hl
        ld   de, #72
        add  hl, de
        ld   de, #329
        or   a
        sbc  hl, de              ; -72 <= sx <= 256 ?
        pop  de
        jp   nc, 8$
        ld   hl, #_oy
        add  hl, de
        add  hl, de
        ld   c, (hl)
        inc  hl
        ld   b, (hl)
        push de
        ld   de, (_cam_y)
        ld   h, b
        ld   l, c
        or   a
        sbc  hl, de              ; sy = oy - cam_y
        ld   (_g_sy), hl
        ld   de, #48
        add  hl, de
        ld   de, #249
        or   a
        sbc  hl, de              ; -48 <= sy <= 200 ?
        pop  de
        jp   nc, 8$
        ; plain objects (no special drawing case): one animated frame, drawn right here
        ld   hl, #_ot
        add  hl, de
        ld   a, (hl)
        ld   c, a                ; C = type
        cp   #67
        jr   nc, 30$
        ld   hl, #_dcls
        add  a, l
        ld   l, a
        adc  a, h
        sub  l
        ld   h, a
        ld   a, (hl)
        or   a
        jr   nz, 30$             ; special case: the C code draws it
        ld   hl, #_t_spr
        ld   a, c
        add  a, l
        ld   l, a
        adc  a, h
        sub  l
        ld   h, a
        ld   b, (hl)             ; B = base frame
        ld   a, b
        or   a
        jr   nz, 31$
        ld   a, c
        cp   #1                  ; the apple base frame is 0
        jr   nz, 8$
31$:    ld   hl, #_t_anim
        ld   a, c
        add  a, l
        ld   l, a
        adc  a, h
        sub  l
        ld   h, a
        ld   a, (hl)
        or   a
        jr   z, 33$
        push bc
        dec  a                   ; HL = anims + (a - 1) * 17
        ld   l, a
        ld   h, #0
        ld   c, l
        ld   b, h
        add  hl, hl
        add  hl, hl
        add  hl, hl
        add  hl, hl
        add  hl, bc
        ld   bc, #_anims
        add  hl, bc
        push hl
        ld   hl, #_ofr
        add  hl, de
        ld   a, (hl)
        rrca
        rrca
        rrca
        and  #0x1F               ; k = ofr >> 3
        pop  hl
        ld   c, (hl)             ; n
        inc  hl                  ; l[]
32$:    cp   c
        jr   c, 34$
        sub  c
        jr   32$
34$:    ld   c, a
        push hl
        ld   hl, #_ovx
        add  hl, de
        ld   a, (hl)
        pop  hl
        dec  a
        bit  7, a
        jr   nz, 35$             ; facing left: l[]
        ld   a, #8               ; facing right: r[]
        add  a, l
        ld   l, a
        adc  a, h
        sub  l
        ld   h, a
35$:    ld   a, c
        add  a, l
        ld   l, a
        adc  a, h
        sub  l
        ld   h, a
        ld   a, (hl)
        pop  bc
        add  a, b
        ld   b, a
33$:    ld   hl, (_g_sy)
        push hl
        ld   de, (_g_sx)
        ld   a, b
        call _draw_spr           ; pops y itself
        jr   8$
30$:    ld   a, e
        call _draw_obj
8$:     pop  hl
        inc  hl
        pop  bc
        dec  b
        jp   nz, 1$
        ret
    __endasm;
}

/* ---- initialisation from the ROM records ------------------------------------- */
static void level_objects_init(void) {
    const u16 *r;
    u16 n = ld->nobjs, q;
    u8 k = 0, t;
    BANK(ld->obj_bank);
    r = ld->objs;
    elev_n = 0;
    for (q = 0; q < n && k < MAXO; q++, r += OR_SIZE) {
        t = (u8)r[OR_TYP];
        if (t == T_TEXT || t == T_TEXT2 || t > 66) continue;
        ot[k] = t;
        ox[k] = (int)r[OR_X]; oy[k] = (int)r[OR_Y];
        ow[k] = (u8)r[OR_W]; oh[k] = (u8)r[OR_H];
        oxd[k] = (int)r[OR_XD]; oyd[k] = (int)r[OR_YD];
        ost[k] = (u8)r[OR_ST]; otag[k] = r[OR_CNT]; oaux[k] = r[OR_AUX]; otgt[k] = r[OR_TGT];
        oct[k] = 0; ofr[k] = (u8)(k * 7); ovx[k] = 0; ovy[k] = 0; ohp[k] = 0;
        ovx[k] = oxd[k] > 0 ? 1 : -1;
        switch (t) {
        case T_ANT: case T_FROG: case T_PHOENIX: ohp[k] = 1; break;
        case T_SLUG: case T_CRAB: case T_SNAKE: case T_JELLY: case T_EEL: case T_PAC: ohp[k] = 2; break;
        case T_GATOR: ohp[k] = 4; break;
        case T_ELEV:
            ovy[k] = 0; ost[k] = 0; oaux[k] = 0; oct[k] = (u8)(oy[k] >> 4);
            if (elev_n < MAX_ELEV) elev_i[elev_n++] = k;
            break;
        case T_SPARK:
            ovy[k] = -1; ost[k] = 0; break;
        }
        if (t == T_PHOENIX || t == T_JELLY) oaux[k] = (u16)oy[k];
        if (t == T_FROG) { oct[k] = (u8)(k & 31); ost[k] = 0; }
        if (t == T_DOOR || t == T_FALLDOOR || t == T_BRIDGER || t == T_PAD) ost[k] = 0;
        if (t == T_EYE || t == T_STALAG || t == T_ROCK || t == T_BOULDER || t == T_NEST) ost[k] = 0;
        if (t == T_PAC) { ovx[k] = 0; ovy[k] = 0; }
        k++;
    }
    num_obj = k;
    for (k = 0; k < num_obj; k++) { ocol[k] = (u8)((u16)ox[k] >> 4); orow[k] = (u8)((u16)oy[k] >> 4); }
    BANK(ld->core_bank);
}

/* ---- hub persistence --------------------------------------------------------- */
static void hub_save(void) {
    u8 i;
    for (i = 0; i < num_obj && i < HUB_MAXO; i++) { hub_ot[i] = ot[i]; hub_ost[i] = ost[i]; hub_oct[i] = oct[i]; }
    for (i = 0; i < n_ov; i++) { hub_ov_cell[i] = ov_cell[i]; hub_ov_mt[i] = ov_mt[i]; }
    hub_n_ov = n_ov;
    hub_valid = 1;
}
static void hub_restore(void) {
    u8 i;
    if (!hub_valid) return;
    for (i = 0; i < num_obj && i < HUB_MAXO; i++) {
        if (ot[i] != T_ELEV) ot[i] = hub_ot[i];
        ost[i] = hub_ost[i]; oct[i] = hub_oct[i];
        if (ot[i] == T_ELEV) { oy[i] = (int)oct[i] << 4; ost[i] = 0; }
    }
    for (i = 0; i < hub_n_ov; i++) { ov_cell[i] = hub_ov_cell[i]; ov_mt[i] = hub_ov_mt[i]; }
    n_ov = hub_n_ov;
}
