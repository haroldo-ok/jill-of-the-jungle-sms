/* ------------------------------------------------------------------------
 * Level objects: initialisation, behaviour, drawing, hub persistence
 * ---------------------------------------------------------------------- */

static const u8 t_spr[67] = {
    [T_APPLE] = SPR_APPLE, [T_KNIFEI] = SPR_KNIFEI, [T_KEY] = SPR_KEY, [T_CRYSTAL] = SPR_CRYSTAL,
    [T_SLUG] = SPR_SLUG, [T_FROG] = SPR_FROG, [T_ANT] = SPR_ANT, [T_PHOENIX] = SPR_PHOENIX,
    [T_BOULDER] = SPR_BOULDER, [T_STALAG] = SPR_STALAG, [T_SNAKE] = SPR_SNAKE, [T_ROCK] = SPR_ROCK,
    [T_MEGA] = SPR_MEGA, [T_KNIGHT] = SPR_KNIGHT, [T_NEST] = SPR_NEST, [T_CRAB] = SPR_CRAB,
    [T_GATOR] = SPR_GATOR, [T_EPIC] = SPR_EPIC, [T_SKULL] = SPR_SKULL, [T_PAC] = SPR_PAC,
    [T_JELLY] = SPR_JELLY, [T_EEL] = SPR_EEL, [T_ELEV] = SPR_ELEV, [T_SWITCH] = SPR_SWITCH,
    [T_EYE] = SPR_EYE, [T_SPARK] = SPR_SPARK, [T_FLAG] = SPR_FLAG, [T_MIST] = SPR_MIST
};
static const u8 t_anim[67] = {
    [T_APPLE] = ANIM_APPLE + 1, [T_KEY] = ANIM_KEY + 1, [T_SLUG] = ANIM_SLUG + 1, [T_FROG] = ANIM_FROG + 1,
    [T_ANT] = ANIM_ANT + 1, [T_PHOENIX] = ANIM_PHOENIX + 1, [T_BOULDER] = ANIM_BOULDER + 1, [T_SNAKE] = ANIM_SNAKE + 1,
    [T_KNIGHT] = ANIM_KNIGHT + 1, [T_NEST] = ANIM_NEST + 1, [T_CRAB] = ANIM_CRAB + 1, [T_GATOR] = ANIM_GATOR + 1,
    [T_EPIC] = ANIM_EPIC + 1, [T_SKULL] = ANIM_SKULL + 1, [T_PAC] = ANIM_PAC + 1, [T_JELLY] = ANIM_JELLY + 1,
    [T_EEL] = ANIM_EEL + 1, [T_SPARK] = ANIM_SPARK + 1, [T_FLAG] = ANIM_FLAG + 1, [T_MIST] = ANIM_MIST + 1
};

static const s8 bob_tab[16] = { 0, 2, 4, 5, 6, 5, 4, 2, 0, -2, -4, -5, -6, -5, -4, -2 };
static const s8 bob3_tab[16] = { 0, 0, 1, 1, 2, 1, 1, 0, 0, 0, -1, -1, -2, -1, -1, 0 };   /* bob_tab / 3 */

u8 msg_shown;          /* bitmask of one-shot hint messages already displayed */
u8 cp_hit;
u8 cp_lock;
u8 req_level = 0xFF, req_entry;     /* pending level change */
u8 req_music;
u8 want_text;                       /* hint text index to display (0 = none) */
u8 level_end_req;                   /* 1 = leave through '!' */
u8 slides_req;
u8 macro_req;                       /* ending macro requested */

/* ---- helpers ---------------------------------------------------------------- */
static u8 touch(u8 i, s8 ins) {
    if ((u8)(ocol[i] - pcol + 5) > 9 || (u8)(orow[i] - prow + 5) > 9) return 0;   /* far (cells) */
    return ox[i] + ins < px + pw && ox[i] + ow[i] - ins > px &&
           oy[i] + oh[i] - ins > py && oy[i] + ins < py + ph;
}
static u8 is_near(u8 i) {
    return (u16)(ox[i] - (int)cam_x + 96) < 448 && (u16)(oy[i] - (int)cam_y + 96) < 384;
}
static u8 spd_of(u8 i) {
    int a = oxd[i] < 0 ? -oxd[i] : oxd[i];
    u8 s;
    if (a == 0) a = 2;
    s = (u8)(a >> 1);
    if ((a & 1) && (frame_ctr & 1)) s++;
    return s;
}
static void near_add(u8 i) {          /* a freshly spawned object joins the active list at once */
    u8 k;
    for (k = 0; k < near_n; k++) if (near_l[k] == i) return;
    if (near_n < MAXO) near_l[near_n++] = i;
}
static u8 alloc_obj(void) {
    u8 i;
    for (i = 0; i < num_obj; i++) if (ot[i] == T_NONE && ofr[i] != 0xFE) { near_add(i); return i; }
    if (num_obj < MAXO) { near_add(num_obj); return num_obj++; }
    return 0xFF;
}

static void kill_obj(u8 i) {
    fx_spawn(ox[i] + (ow[i] >> 1) - 4, oy[i] + (oh[i] >> 1) - 4);
    ot[i] = T_NONE;
    ofr[i] = 0;
}

/* returns 1 if the projectile is consumed */
static u8 killable_dmg(u8 j, u8 dmg) {
    u8 t = ot[j];
    switch (t) {
    case T_ANT: case T_SLUG: case T_FROG: case T_PHOENIX: case T_CRAB: case T_SNAKE:
    case T_BEES: case T_JELLY: case T_EEL: case T_GATOR: case T_PAC:
        if (ohp[j] > dmg) { ohp[j] -= dmg; fx_spawn(ox[j] + (ow[j] >> 1) - 4, oy[j] + (oh[j] >> 1) - 4); }
        else { kill_obj(j); score_add(t == T_BEES ? 50 : 200); }
        return 1;
    case T_NEST:
        if (ost[j] == 0) ost[j] = 1;
        return 1;
    case T_KNIGHT: case T_BOULDER: case T_ROCK: case T_SKULL:
        fx_spawn(ox[j] + (ow[j] >> 1) - 4, oy[j] + (oh[j] >> 1) - 4);
        return 1;
    default:
        return 0;
    }
}

/* ---- doors / bridges ---------------------------------------------------------- */
static void door_open(u8 j) {
    u8 n = (u8)((oh[j] + 15) >> 4), k, mx = (u8)(ox[j] >> 4), my = (u8)(oy[j] >> 4);
    if (ost[j]) return;
    ost[j] = 1;
    if (ot[j] == T_FALLDOOR) n = 1;
    for (k = 0; k < n; k++) set_cell(mx, (u8)(my + k), (u8)(oaux[j] >> (k ? 8 : 0)));
    sfx_play(SND_DOOR);
}
static void bridger_toggle(u8 j) {
    u8 mx = (u8)(ox[j] >> 4), my = (u8)(oy[j] >> 4);
    ost[j] ^= 1;
    set_cell(mx, my, ost[j] ? (u8)oaux[j] : ld->map[((u16)my << 7) | mx]);
    sfx_play(SND_SWITCH);
}
static void trigger_tag(u16 tag, u8 from) {
    u8 j;
    for (j = 0; j < num_obj; j++) {
        if (j == from || otag[j] != tag) continue;
        switch (ot[j]) {
        case T_DOOR: case T_FALLDOOR: door_open(j); break;
        case T_STALAG: if (!ost[j]) ost[j] = 1; break;
        case T_BRIDGER: bridger_toggle(j); break;
        }
    }
}
static void switch_tag(u16 tag, u8 from) {
    u8 j;
    for (j = 0; j < num_obj; j++) {
        if (j == from || otag[j] != tag) continue;
        switch (ot[j]) {
        case T_BRIDGER: bridger_toggle(j); break;
        case T_KNIGHT: ost[j] ^= 1; break;
        }
    }
}

/* ---- movement primitives ------------------------------------------------------ */
static u8 blocked_front(u8 i, u8 d_right, u8 ledge) {
    int xf = d_right ? ox[i] + ow[i] : ox[i] - 1;
    int ym = oy[i] + (oh[i] >> 1);
    u8 f = flag_at(xf, ym) | flag_at(xf, oy[i] + oh[i] - 1);
    if (f & (MF_SOLID | MF_MARK)) return 1;
    if (flag_at(xf, ym) & MF_WATER && ot[i] != T_GATOR && ot[i] != T_EEL && ot[i] != T_JELLY) return 1;
    if (ledge) {
        u8 g = flag_at(xf, oy[i] + oh[i]);
        if (!(g & (MF_SOLID | MF_PLAT))) return 1;
    }
    return 0;
}
static void patrol(u8 i, u8 ledge) {
    u8 n = spd_of(i);
    u8 right = ovx[i] > 0;
    while (n--) {
        if (blocked_front(i, right, ledge)) { right = !right; ovx[i] = right ? 1 : -1; break; }
        ox[i] += right ? 1 : -1;
    }
}
static u8 supported(u8 i) {
    u8 f = flag_at(ox[i] + (ow[i] >> 1), oy[i] + oh[i]);
    return (f & (MF_SOLID | MF_PLAT)) != 0;
}
static void fall(u8 i) {
    if (!supported(i)) {
        if (ovy[i] < 4 && (frame_ctr & 1)) ovy[i]++;
        oy[i] += ovy[i];
        if (supported(i)) { oy[i] &= ~15; oy[i] += 16 - oh[i]; ovy[i] = 0; }
    } else ovy[i] = 0;
}

static void tick_anim(u8 i) { ofr[i]++; }

/* ---- lifts (Jill's ELEV object, as in the Epic engine's msg_elev) ----------------------
 * A lift sits on a shaft of ELEVMID tiles.  Standing on it, UP raises it one cell (the cell it
 * leaves becomes shaft), DOWN lowers it one cell (the shaft cell below takes the background of
 * the lift's cell).  Lifts whose counter is not -1 sink back down when nobody rides them.
 * ovy = direction while moving, oaux = pixels left of the current cell step,
 * oct = current cell row (persisted in the hub), ost = ride timer | 0x80 while carrying Jill. */
#define ELEV_SPD 4
static s8 elev_dir;
static u8 elev_rider(u8 i) {
    return !dead_t && py + ph == oy[i] + 16 && px + pw > ox[i] + 2 && px < ox[i] + 14;
}
static void elev_start(u8 i, s8 dir, u8 carry) {
    u8 xc = (u8)(ox[i] >> 4), yc = (u8)(oy[i] >> 4);
    if (dir > 0) set_cell(xc, yc + 1, mt_at(xc, yc));      /* shaft cell below -> background */
    ovy[i] = dir; oaux[i] = 16;
    ost[i] = carry ? 0x86 : 0;
    if (dir != elev_dir) { elev_dir = dir; sfx_play(SND_SWITCH); }    /* click on direction change */
}
static void elev_update(u8 i) {
    u8 xc = (u8)(ox[i] >> 4), yc = (u8)(oy[i] >> 4);
    if (ovy[i]) {                                           /* moving one cell */
        s8 d = ovy[i] < 0 ? -ELEV_SPD : ELEV_SPD;
        oy[i] += d;
        if ((ost[i] & 0x80) && px + pw > ox[i] - 2 && px < ox[i] + 18) { py += d; vy = 0; }
        oaux[i] -= ELEV_SPD;
        if (!oaux[i]) {
            if (ovy[i] < 0) set_cell(xc, (u8)((oy[i] >> 4) + 1), ld->elev_mt);   /* left behind: shaft */
            ovy[i] = 0;
            oct[i] = (u8)(oy[i] >> 4);
            ost[i] &= 0x7F;
        }
        return;
    }
    if (elev_rider(i)) {
        ost[i] = 6;
        if (!(msg_shown & 16)) { msg_shown |= 16; want_text = 22; return; }
        if (!(held & (K_UP | K_DOWN))) elev_dir = 0;
        if (held & K_UP) {
            if (yc && !(flag_at(ox[i] + 8, oy[i] - 1) & MF_SOLID) &&
                !((row_flags(py - 1) | row_flags(py - 8) | row_flags(py - 16)) & MF_SOLID))
                elev_start(i, -1, 1);
        } else if (held & K_DOWN) {
            if (mt_at(xc, yc + 1) == ld->elev_mt) elev_start(i, 1, 1);
        }
    } else {
        if (ost[i]) ost[i]--;
        else if (otag[i] != 0xFFFF && mt_at(xc, yc + 1) == ld->elev_mt) elev_start(i, 1, 0);
    }
}

/* ---- per-object update ----------------------------------------------------------- */
static void collect(u8 i) {
    u8 c;
    switch (ot[i]) {
    case T_APPLE: if (health >= HEALTH_MAX) { score_add(100); } else player_heal(); score_add(100); sfx_play(SND_APPLE); break;
    case T_KNIFEI: if (knives < 9) knives++; HUD(); score_add(50); sfx_play(SND_KEY); break;
    case T_KEY: keys++; HUD(); score_add(200); sfx_play(SND_KEY); break;
    case T_CRYSTAL: gems++; HUD(); score_add(500); sfx_play(SND_GEM); break;
    case T_TOKEN:
        c = (u8)otag[i];
        sfx_play(SND_TOKEN);
        switch (c) {
        case 4: set_form(FORM_FROG); break;
        case 5: set_form(FORM_PHOENIX); break;
        case 7: set_form(FORM_FISH); break;
        case 8: blade_t = 250; HUD(); break;
        case 9: hijump = 1; HUD(); break;
        default: score_add(1000); break;
        }
        if (c == 7 && !(msg_shown & 1)) { msg_shown |= 1; want_text = 7; }
        if (c == 5 && !(msg_shown & 2)) { msg_shown |= 2; want_text = 8; }
        break;
    }
    fx_spawn(ox[i] + (ow[i] >> 1) - 4, oy[i] + (oh[i] >> 1) - 4);
    ot[i] = T_NONE;
}

static const u8 ucls[102] = { [T_APPLE] = 1, [T_KNIFEI] = 1, [T_KEY] = 1, [T_CRYSTAL] = 1, [T_TOKEN] = 1, [T_ANT] = 2, [T_SLUG] = 2, [T_CRAB] = 2, [T_SNAKE] = 2, [T_GATOR] = 2, [T_PHOENIX] = 3, [T_FROG] = 4, [T_PAC] = 5, [T_JELLY] = 6, [T_EEL] = 7, [T_SPARK] = 8, [T_SKULL] = 9, [T_EYE] = 10, [T_KNIGHT] = 11, [T_NEST] = 12, [T_BEES] = 13, [T_STALAG] = 14, [T_ROCK] = 15, [T_BOULDER] = 16, [T_ELEV] = 17, [T_SWITCH] = 18, [T_PAD] = 19, [T_DOOR] = 20, [T_CHECK] = 21, [T_MIST] = 22, [T_EPIC] = 23, [T_FLAG] = 23 };   /* type -> dense case number (jump table) */
static void obj_update(u8 i) {
    u8 t = ot[i], j, k;
    switch (t < 102 ? ucls[t] : 0) {
    case 1:   /* T_APPLE T_KNIFEI T_KEY T_CRYSTAL T_TOKEN */
        tick_anim(i);
        if (!dead_t && touch(i, 1)) collect(i);
        break;

    /* ---------- ground walkers ---------- */
    case 2:   /* T_ANT T_SLUG T_CRAB T_SNAKE T_GATOR */
        tick_anim(i);
        if (oxd[i] || t == T_ANT || t == T_SLUG || t == T_SNAKE || t == T_GATOR) {
            fall(i);
            patrol(i, t != T_GATOR);
        }
        if (touch(i, t == T_SNAKE || t == T_GATOR ? 4 : 3)) player_hurt(1);
        break;
    case 3:   /* T_PHOENIX */
        tick_anim(i);
        { u8 n = spd_of(i), right = ovx[i] > 0;
          while (n--) {
            if (blocked_front(i, right, 0)) { right = !right; ovx[i] = right ? 1 : -1; break; }
            ox[i] += right ? 1 : -1;
          }
          oy[i] = (int)oaux[i] + bob_tab[(ofr[i] >> 2) & 15] * 2;
        }
        if (touch(i, 3)) player_hurt(1);
        break;
    case 4:   /* T_FROG */
        tick_anim(i);
        if (ost[i] == 0) {                                   /* sitting */
            if (oct[i]) oct[i]--;
            else if (is_near(i)) {
                int dx = px - ox[i], dy = py - oy[i];
                if (dx > -88 && dx < 88 && dy > -56 && dy < 56) {
                    ovx[i] = dx < 0 ? -1 : 1; ovy[i] = -4; ost[i] = 1;
                }
            }
        } else {                                              /* airborne */
            if (blocked_front(i, ovx[i] > 0, 0)) ovx[i] = -ovx[i];
            ox[i] += ovx[i];
            if ((frame_ctr & 3) == 0) ovy[i]++;
            oy[i] += ovy[i];
            if (ovy[i] > 0 && supported(i)) { oy[i] = (oy[i] & ~15) + 16 - oh[i]; ost[i] = 0; oct[i] = 50; }
            else if (ovy[i] < 0 && (flag_at(ox[i] + 7, oy[i]) & MF_SOLID)) ovy[i] = 0;
        }
        if (touch(i, 2)) player_hurt(1);
        break;
    case 5:   /* T_PAC */
        tick_anim(i);
        { u8 n = spd_of(i);
          if (!ovx[i] && !ovy[i]) { if (otag[i] == 2) ovy[i] = -1; else ovx[i] = oxd[i] < 0 ? -1 : 1; }
          while (n--) {
            if (ovx[i]) {
                if (blocked_front(i, ovx[i] > 0, 0)) { ovx[i] = -ovx[i]; if (otag[i] == 2 && oxd[i]) { ovy[i] = (rnd() & 1) ? 1 : -1; } }
                else ox[i] += ovx[i];
            }
            if (ovy[i]) {
                int yy = ovy[i] > 0 ? oy[i] + oh[i] : oy[i] - 1;
                if (flag_at(ox[i] + 8, yy) & (MF_SOLID | MF_MARK)) { ovy[i] = -ovy[i]; if (!oxd[i]) {} else ovx[i] = (rnd() & 1) ? 1 : -1; }
                else oy[i] += ovy[i];
            }
          }
        }
        if (touch(i, 3)) player_hurt(1);
        break;
    case 6:   /* T_JELLY */
        tick_anim(i);
        oy[i] = (int)oaux[i] + bob_tab[(ofr[i] >> 2) & 15] * 3;
        if (touch(i, 3)) player_hurt(1);
        break;
    case 7:   /* T_EEL */
        tick_anim(i);
        { u8 right = ovx[i] > 0;
          if (blocked_front(i, right, 0) || !(flag_at(right ? ox[i] + ow[i] + 1 : ox[i] - 2, oy[i] + 8) & MF_WATER)) { right = !right; ovx[i] = right ? 1 : -1; }
          else ox[i] += right ? 1 : -1;
          if (frame_ctr & 1) ox[i] += right ? 1 : -1;
        }
        if (touch(i, 4)) player_hurt(1);
        break;
    case 8:   /* T_SPARK */
        tick_anim(i);
        { u8 n = (u8)((oyd[i] ? oyd[i] : 2) >> 1);
          while (n--) {
            int yy = ovy[i] > 0 ? oy[i] + oh[i] : oy[i] - 1;
            if (flag_at(ox[i] + 8, yy) & (MF_SOLID | MF_MARK)) ovy[i] = -ovy[i];
            else oy[i] += ovy[i];
          }
        }
        if (touch(i, 3)) player_hurt(1);
        break;
    case 9:   /* T_SKULL */
        tick_anim(i);
        if (touch(i, 4)) player_hurt(1);
        break;
    case 10:   /* T_EYE */ /* spring */
        if (ost[i]) ost[i]--;
        else if (!dead_t && vy > 0 && px + pw > ox[i] + 2 && px < ox[i] + ow[i] - 2 &&
                 py + ph >= oy[i] - 2 && py + ph <= oy[i] + 8) {
            vy = -1500; on_ground = 0; climbing = 0; ost[i] = 8; sfx_play(SND_BOUNCE);
            py = oy[i] - ph;
        }
        break;
    case 11:   /* T_KNIGHT */
        tick_anim(i);
        if (ost[i]) {
            if (oxd[i]) {
                u8 n = (u8)((oxd[i] < 0 ? -oxd[i] : oxd[i]) / 3);
                u8 right = ovx[i] > 0;
                if (!n) n = 1;
                while (n--) {
                    if (blocked_front(i, right, 1)) { right = !right; ovx[i] = right ? 1 : -1; break; }
                    ox[i] += right ? 1 : -1;
                }
            }
            if (touch(i, 6)) player_hurt(2);
        }
        break;
    case 12:   /* T_NEST */
        tick_anim(i);
        if (ost[i] == 0) {
            int dx = px - ox[i], dy = py - oy[i];
            if (dx > -40 && dx < 40 && dy > 0 && dy < 96) ost[i] = 1;
        } else if (ost[i] == 1) {
            for (k = 0; k < 3; k++) {
                j = alloc_obj();
                if (j == 0xFF) break;
                ot[j] = T_BEES; ox[j] = ox[i] + k * 6; oy[j] = oy[i] + 8; ow[j] = 12; oh[j] = 12;
                ovx[j] = 0; ovy[j] = 0; ost[j] = 0; oct[j] = 255; ofr[j] = k * 5; ohp[j] = 1; otag[j] = 0xFFFF;
                oxd[j] = 2; oyd[j] = 0;
            }
            ost[i] = 2; sfx_play(SND_ZAP);
        }
        break;
    case 13:   /* T_BEES */
        tick_anim(i);
        if (oct[i]) oct[i]--; else { kill_obj(i); break; }
        if ((frame_ctr & 1) == 0) {
            if (ox[i] + 6 < px + (pw >> 1)) ox[i]++; else if (ox[i] + 6 > px + (pw >> 1)) ox[i]--;
            if (oy[i] + 6 < py + (ph >> 1)) oy[i]++; else if (oy[i] + 6 > py + (ph >> 1)) oy[i]--;
            if (rnd() < 40) ox[i] += (rnd() & 1) ? 2 : -2;
            if (rnd() < 40) oy[i] += (rnd() & 1) ? 2 : -2;
        }
        ovx[i] = (px > ox[i]) ? 1 : -1;
        if (touch(i, 3)) { player_hurt(1); }
        break;

    /* ---------- falling hazards ---------- */
    case 14:   /* T_STALAG */
        if (ost[i] == 1) {
            if (ovy[i] < 6) ovy[i]++;
            oy[i] += ovy[i];
            if (touch(i, 3)) player_hurt(2);
            if (flag_at(ox[i] + 8, oy[i] + oh[i]) & MF_SOLID) { ost[i] = 2; oct[i] = 24; oy[i] = (oy[i] + 15) & ~15; sfx_play(SND_THUD); }
        } else if (ost[i] == 2) {
            if (--oct[i] == 0) kill_obj(i);
        }
        break;
    case 15:   /* T_ROCK */
        if (ost[i] == 0) {
            int dx = px - ox[i], dy = py - oy[i];
            if (is_near(i) && dx > -20 && dx < 20 && dy > 8 && dy < 140) { ost[i] = 1; ovy[i] = 0; }
        } else if (ost[i] == 1) {
            if (ovy[i] < 6) ovy[i]++;
            oy[i] += ovy[i];
            if (touch(i, 3)) player_hurt(2);
            if (flag_at(ox[i] + 8, oy[i] + oh[i]) & (MF_SOLID | MF_PLAT)) { ost[i] = 2; oct[i] = 30; oy[i] = (oy[i] + 15) & ~15; sfx_play(SND_THUD); }
        } else if (--oct[i] == 0) kill_obj(i);
        break;
    case 16:   /* T_BOULDER */
        if (ost[i] == 0) {
            int dx = px - ox[i], dy = py - oy[i];
            if (is_near(i) && dx > -96 && dx < 96 && dy > -48 && dy < 48) { ost[i] = 1; ovx[i] = dx < 0 ? -1 : 1; sfx_play(SND_BOULDER); }
        } else {
            tick_anim(i);
            if (!supported(i)) { if (ovy[i] < 5) ovy[i]++; oy[i] += ovy[i]; if (supported(i)) { oy[i] = (oy[i] & ~15) + 16 - oh[i]; ovy[i] = 0; } }
            else {
                u8 n = 2;
                while (n--) {
                    if (blocked_front(i, ovx[i] > 0, 0)) { kill_obj(i); sfx_play(SND_THUD); break; }
                    ox[i] += ovx[i];
                }
            }
            if (ot[i] == T_BOULDER && touch(i, 3)) player_hurt(2);
        }
        break;

    /* ---------- machinery ---------- */
    case 17:   /* T_ELEV */ /* idle, nobody near, not a self-returning lift: nothing to do (cheap u8 test first) */
        elev_update(i);          /* idle lifts are filtered out in obj_loop */
        break;
    case 18:   /* T_SWITCH */
        if (!dead_t && (pressed & (K_UP | K_DOWN)) && px + pw > ox[i] && px < ox[i] + 16 &&
            py < oy[i] + 24 && py + 8 > oy[i]) {
            ost[i] ^= 1;
            switch_tag(otag[i], i);
            if (!(msg_shown & 4)) { msg_shown |= 4; want_text = 6; }
        }
        break;
    case 19:   /* T_PAD */
        if (touch(i, 2)) {
            if (!ost[i]) { ost[i] = 1; trigger_tag(otag[i], i); }
        } else ost[i] = 0;
        break;
    case 20:   /* T_DOOR */
        if (!ost[i] && !dead_t && px + pw > ox[i] - 2 && px < ox[i] + 18 && py + ph > oy[i] && py < oy[i] + oh[i]) {
            if (oxd[i] == 1) { if (keys) { keys--; HUD(); door_open(i); } }
            else if (oyd[i] == 1) {
                if (gems) { gems--; HUD(); door_open(i); }
                else if (!(msg_shown & 8)) { msg_shown |= 8; want_text = 4; }
            }
        }
        break;
    case 21:   /* T_CHECK */
        if (touch(i, 0) && !dead_t) {
            u16 tg = otgt[i];
            if (tg == 0xFFFF && otag[i] == 52) slides_req = 1;
            else if (tg < JILL_N_LEVELS) {
                if (!cp_lock) { req_level = (u8)tg; req_entry = (u8)otag[i]; if (oxd[i] || oyd[i]) { ret_px = oxd[i]; ret_py = oyd[i] + 8; } else { ret_px = ox[i]; ret_py = oy[i] - 8; } }
            } else if (tg == 0xFE) {
                if (!cp_lock) level_end_req = 1;
            } else if (tg >= 0x300 && tg < 0x400) {
                macro_req = (u8)(tg - 0x300 + 1);
            } else if (tg >= 0x100 && tg < 0x200) {
                if (!ost[i]) { ost[i] = 1; music_play((u8)(tg - 0x100 + 1)); }
            }
            cp_hit = 1;
        } else if (otgt[i] >= 0x100 && otgt[i] < 0x200) ost[i] = 0;
        break;
    case 22:   /* T_MIST */
        tick_anim(i);
        { int dx = oxd[i];
          if ((frame_ctr & 3) == 0) ox[i] += dx < 0 ? -1 : (dx > 0 ? 1 : 0);
        }
        break;
    case 23:   /* T_EPIC T_FLAG */
        tick_anim(i);
        break;
    }
}

/* resident wrapper: run the object behaviour code from its overlay bank */
static void collect_try(u8 i) {
    if (!dead_t && touch(i, 1)) collect(i);
}
static const u8 is_item[102] = { [T_APPLE] = 1, [T_KNIFEI] = 1, [T_KEY] = 1, [T_CRYSTAL] = 1, [T_TOKEN] = 1 };

/* one behaviour step for every active object.  Collectibles (the most common objects) are
   handled inline: animate, and only test the pickup when Jill is within a couple of cells. */
static void obj_loop(void) __naked {
    __asm
        ld   a, (_near_n)
        or   a
        ret  z
        ld   b, a
        ld   hl, #_near_l
1$:     push bc
        push hl
        ld   e, (hl)
        ld   d, #0
        ld   hl, #_ot
        add  hl, de
        ld   a, (hl)
        or   a
        jp   z, 8$               ; empty slot
        cp   #61                 ; lift: skip idle lifts nobody is near
        jp   z, 20$
        cp   #102
        jp   nc, 4$
        ld   hl, #_is_item
        add  a, l
        ld   l, a
        adc  a, h
        sub  l
        ld   h, a
        ld   a, (hl)
        or   a
        jp   z, 4$
        ld   hl, #_ofr           ; animation tick
        add  hl, de
        inc  (hl)
        ld   a, (_pcol)
        ld   c, a
        ld   hl, #_ocol
        add  hl, de
        ld   a, (hl)
        sub  c
        add  a, #2
        cp   #4
        jp   nc, 8$
        ld   a, (_prow)
        ld   c, a
        ld   hl, #_orow
        add  hl, de
        ld   a, (hl)
        sub  c
        add  a, #2
        cp   #5
        jp   nc, 8$
        ld   a, e
        call _collect_try
        jp   8$
4$:     ld   a, e
        call _obj_update
        jp   8$
20$:    ld   hl, #_ovy
        add  hl, de
        ld   a, (hl)
        or   a
        jp   nz, 4$              ; moving
        ld   hl, #_ost
        add  hl, de
        ld   a, (hl)
        or   a
        jp   nz, 4$              ; ridden recently
        ld   a, (_pcol)
        ld   c, a
        ld   hl, #_ocol
        add  hl, de
        ld   a, (hl)
        sub  c
        inc  a
        cp   #3
        jr   nc, 21$
        ld   a, (_prow)
        ld   c, a
        ld   hl, #_orow
        add  hl, de
        ld   a, (hl)
        sub  c
        add  a, #2
        cp   #4
        jp   c, 4$               ; Jill is next to it
21$:    ld   hl, #_otag          ; self-returning lift (counter != -1): check every 8th frame
        add  hl, de
        add  hl, de
        ld   a, (hl)
        inc  hl
        and  (hl)
        inc  a
        jp   z, 8$               ; counter == 0xFFFF: stays put
        ld   a, (_frame_ctr)
        add  a, e
        and  #7
        jp   z, 4$
8$:     pop  hl
        inc  hl
        pop  bc
        dec  b
        jp   nz, 1$
        ret
    __endasm;
}
static void obj_update_near(void) {
    pcol = (u8)((u16)px >> 4); prow = (u8)((u16)py >> 4);
    obj_loop();
}
static void obj_update_all_t(void) {   /* resident: runs the object-behaviour overlay once per frame */
    u8 o = cur_ovl;
    OVL_SET(OVL_B);
    obj_update_near();
    OVL_SET(o);
}
