/* ------------------------------------------------------------------------
 * Object drawing and initialisation
 * ---------------------------------------------------------------------- */

static u8 anim_frame(u8 a, u8 i, u8 right) {
    const Anim *an = &anims[a];
    u8 k = ofr[i] >> 3, n = an->n;
    while (k >= n) k -= n;
    return right ? an->r[k] : an->l[k];
}

static void draw_obj(u8 i) {
    u8 t = ot[i], a, f, k, right;
    int sx, sy;
    if (t == T_NONE) return;
    sx = ox[i] - (int)cam_x;
    sy = oy[i] - (int)cam_y;
    if (sx < -72 || sx > 256 || sy < -48 || sy > 200) return;
    right = ovx[i] > 0;
    switch (t) {
    case T_TOKEN:
        switch ((u8)otag[i]) {
        case 4: f = SPR_TOK4; break;
        case 5: f = SPR_TOK5; break;
        case 7: f = SPR_TOK7; break;
        case 8: f = SPR_TOK8; break;
        case 9: f = SPR_TOK9; break;
        default: f = SPR_TOK0; break;
        }
        draw_spr(f, sx, sy + bob_tab[(ofr[i] >> 2) & 15] / 3);
        return;
    case T_BEES:
        draw_spr(SPR_BEES + anim_frame(ANIM_BEES, i, right), sx, sy);
        return;
    case T_GATOR:
        f = anim_frame(ANIM_GATOR, i, right);
        if (right) { draw_spr(SPR_GATOR + f - 4, sx, sy); draw_spr(SPR_GATOR + f, sx + 32, sy); }
        else       { draw_spr(SPR_GATOR + f, sx, sy); draw_spr(SPR_GATOR + f - 4, sx + 32, sy); }
        return;
    case T_SNAKE:
        f = anim_frame(ANIM_SNAKE, i, right);
        if (right) draw_spr(SPR_SNAKE + f, sx + 40, sy);
        else       draw_spr(SPR_SNAKE + f, sx, sy);
        for (k = 0; k < 5; k++) {
            a = (u8)((ofr[i] >> 3) + k);
            while (a >= 6) a -= 6;
            draw_spr(SPR_SNAKE + 6 + a, sx + (right ? (k << 3) : 16 + (k << 3)), sy + 12 - (bob_tab[(u8)((ofr[i] >> 1) + (k << 1)) & 15] >> 2));
        }
        return;
    case T_KNIGHT:
        if (ost[i] && touch(i, -10)) f = 2 + ((ofr[i] >> 2) % 3);
        else f = ost[i] ? ((ofr[i] >> 4) & 1) : 1;
        draw_spr(SPR_KNIGHT + f, sx, sy);
        return;
    case T_MEGA:
        f = (u8)oxd[i];
        if (f > 4) f = 0;
        draw_spr(SPR_MEGA + f, sx, sy);
        return;
    case T_SWITCH:
        draw_spr(SPR_SWITCH + (ost[i] & 1), sx, sy);
        return;
    case T_NEST:
        f = ost[i] ? 1 + ((ofr[i] >> 3) % 3) : 0;
        draw_spr(SPR_NEST + f, sx, sy);
        return;
    case T_STALAG:
        f = 0;
        if (ost[i] == 2) { f = 1 + ((24 - oct[i]) >> 3); if (f > 4) f = 4; }
        draw_spr(SPR_STALAG + f, sx, sy);
        return;
    case T_EYE:
        draw_spr(SPR_EYE + (ost[i] ? 1 : 0), sx, sy);
        return;
    case T_MIST:
        draw_spr(SPR_MIST + anim_frame(ANIM_MIST, i, 0), sx, sy);
        return;
    case T_DOOR: case T_FALLDOOR: case T_BRIDGER: case T_PAD: case T_CHECK:
        return;
    }
    if (t < 67 && (t_spr[t] || t == T_APPLE)) {
        a = t_anim[t];
        f = t_spr[t];
        if (a) f += anim_frame(a - 1, i, right);
        draw_spr(f, sx, sy);
    }
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
            ovy[k] = (ost[k] == 0xFF) ? -1 : 1; ost[k] = 0;
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
    }
    for (i = 0; i < hub_n_ov; i++) { ov_cell[i] = hub_ov_cell[i]; ov_mt[i] = hub_ov_mt[i]; }
    n_ov = hub_n_ov;
}
