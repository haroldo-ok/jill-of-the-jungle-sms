/* ------------------------------------------------------------------------
 * Player: collision, physics, weapons, forms
 * ---------------------------------------------------------------------- */

#define K_UP    PORT_A_KEY_UP
#define K_DOWN  PORT_A_KEY_DOWN
#define K_LEFT  PORT_A_KEY_LEFT
#define K_RIGHT PORT_A_KEY_RIGHT
#define K_JUMP  PORT_A_KEY_1
#define K_FIRE  PORT_A_KEY_2

#define WALK_ACC 96
#define WALK_MAX 512
#define AIR_ACC  56
#define GRAV     52
#define JUMP_V   (-1232)
#define FALL_MAX 1280
#define HEALTH_MAX 6

u8 pw = 16, ph = 24;
u8 fxr, fyr;
u8 hurt_t, dead_t, form_t, blade_t, fire_cd, anim_t, crouch;
u8 jump_held;
u8 flop_t;
u8 pose_cache;

/* object indices of lifts (platforms the player can ride) */
#define MAX_ELEV 10
u8 elev_n, elev_i[MAX_ELEV];

/* projectiles ------------------------------------------------------------- */
#define PRN 5
#define PR_KNIFE 1
#define PR_FIRE  2
#define PR_FISH  3
#define PR_KNIFEB 4      /* dagger on its way back to Jill */
u8 pr_t[PRN], pr_l[PRN], pr_fr[PRN];
int pr_x[PRN], pr_y[PRN];
s8 pr_vx[PRN], pr_vy[PRN];

/* effects (sparkles) -------------------------------------------------------- */
#define EFN 6
u8 ef_t[EFN];
int ef_x[EFN], ef_y[EFN];

static void fx_spawn(int x, int y) {
    u8 i;
    for (i = 0; i < EFN; i++) if (!ef_t[i]) { ef_t[i] = 18; ef_x[i] = x; ef_y[i] = y; return; }
}

#define HUD() (hud_dirty = 1)

static void score_add(u16 v) {
    score_lo += v;
    while (score_lo >= 10000) { score_lo -= 10000; score_hi++; }
    HUD();
}

/* ---- collision ---------------------------------------------------------- */
static u8 col_flags(int x) {          /* OR of tile flags on column x, over the box height */
    int y = py, e = py + ph - 1;
    u8 f = 0;
    for (;;) {
        f |= flag_at(x, y);
        if (y >= e) break;
        y += 15;
        if (y > e) y = e;
    }
    return f;
}
static u8 row_flags(int y) {          /* OR of tile flags on row y, over the box width */
    int x = px, e = px + pw - 1;
    u8 f = 0;
    for (;;) {
        f |= flag_at(x, y);
        if (x >= e) break;
        x += 15;
        if (x > e) x = e;
    }
    return f;
}
static u8 elev_below(void) {          /* a lift supports the player? returns index+1 */
    u8 k, e;
    for (k = 0; k < elev_n; k++) {
        e = elev_i[k];
        if (ot[e] != T_ELEV) continue;
        if (py + ph == oy[e] + 16 && px + pw > ox[e] + 2 && px < ox[e] + 14) return e + 1;
    }
    return 0;
}
static u8 ground_below(void) {
    u8 f = row_flags(py + ph);
    if (f & MF_SOLID) return 1;
    if ((f & MF_PLAT) && (((py + ph) & 15) == 0)) return 1;
    return elev_below() != 0;
}

static void move_x(int dx) {
    while (dx > 0) { if (col_flags(px + pw) & MF_SOLID) { vx = 0; return; } px++; dx--; }
    while (dx < 0) { if (col_flags(px - 1) & MF_SOLID) { vx = 0; return; } px--; dx++; }
}
static void move_y(int dy) {
    while (dy > 0) {
        if (ground_below()) { vy = 0; return; }
        py++; dy--;
    }
    while (dy < 0) {
        if (row_flags(py - 1) & MF_SOLID) { vy = 0; return; }
        py--; dy++;
    }
}
/* apply 8.8 velocities with sub-pixel accumulators */
static void apply_vel(void) {
    int dx, dy;
    u16 t;
    dx = vx >> 8;
    t = (u16)fxr + (u8)vx;
    if (t > 255) dx++;
    fxr = (u8)t;
    dy = vy >> 8;
    t = (u16)fyr + (u8)vy;
    if (t > 255) dy++;
    fyr = (u8)t;
    move_x(dx);
    move_y(dy);
}

static void set_form(u8 f) {
    int bottom = py + ph;
    u8 cx = (u8)(px + (pw >> 1));
    (void)cx;
    form = f;
    form_t = 0; flop_t = 0;
    switch (f) {
    case FORM_FROG:    pw = 14; ph = 10; break;
    case FORM_PHOENIX: pw = 16; ph = 16; break;
    case FORM_FISH:    pw = 24; ph = 12; break;
    default:           pw = 16; ph = 24; break;
    }
    py = bottom - ph;
    vx = vy = 0;
    HUD();
}

static void player_die(void) {
    if (dead_t) return;
    dead_t = 1;
    health = 0;
    vx = vy = 0;
    set_form(FORM_HUMAN);
    sfx_play(SND_DIE);
    HUD();
}

static void player_hurt(u8 dmg) {
    if (hurt_t || dead_t) return;
    sfx_play(SND_HURT);
    hurt_t = 64;
    vy = -420;
    vx = face_right ? -300 : 300;
    if (form != FORM_HUMAN) { set_form(FORM_HUMAN); vy = -420; vx = face_right ? -300 : 300; return; }
    if (health <= dmg) { player_die(); return; }
    health -= dmg;
    HUD();
}

static void player_heal(void) {
    if (health < HEALTH_MAX) { health++; HUD(); }
}

/* ---- projectiles --------------------------------------------------------- */
static void shoot(u8 kind, int x, int y, s8 vxx, s8 vyy) {
    u8 i;
    for (i = 0; i < PRN; i++) if (!pr_t[i]) {
        pr_t[i] = kind; pr_x[i] = x; pr_y[i] = y; pr_vx[i] = vxx; pr_vy[i] = vyy;
        pr_l[i] = kind == PR_KNIFE ? 36 : 60; pr_fr[i] = 0;
        return;
    }
}

static void kill_obj(u8 i);
static u8 killable_dmg(u8 i, u8 dmg);

/* The dagger is a boomerang: it flies out (steerable by momentum), turns back when its
   range runs out or it hits a wall/monster, and returns to Jill.  Only if it falls into a
   hazard (lava, spikes...) is it lost. */
static void proj_update(void) {
    u8 i, j, f, kn;
    int dx, dy, tx, ty;
    for (i = 0; i < PRN; i++) {
        if (!pr_t[i]) continue;
        kn = pr_t[i] == PR_KNIFE || pr_t[i] == PR_KNIFEB;
        if (pr_t[i] == PR_KNIFE) {
            if ((frame_ctr & 3) == 0) {
                if ((held & K_UP) && pr_vy[i] > -3) pr_vy[i]--;
                if ((held & K_DOWN) && pr_vy[i] < 3) pr_vy[i]++;
            }
            if ((frame_ctr & 7) == 0) {
                if ((held & K_RIGHT) && pr_vx[i] < 6) pr_vx[i]++;
                if ((held & K_LEFT) && pr_vx[i] > -6) pr_vx[i]--;
            }
            if (pr_vx[i] == 0) pr_vx[i] = (held & K_LEFT) ? -1 : 1;
            pr_x[i] += pr_vx[i];
            pr_y[i] += pr_vy[i];
        } else if (pr_t[i] == PR_KNIFEB) {
            tx = px + (pw >> 1) - 4; ty = py + (ph >> 1) - 4;
            dx = tx - pr_x[i]; dy = ty - pr_y[i];
            if (dx > -10 && dx < 10 && dy > -14 && dy < 14) {      /* caught */
                pr_t[i] = 0;
                if (knives < 9) knives++;
                HUD(); sfx_play(SND_KEY);
                continue;
            }
            pr_x[i] += dx > 5 ? 5 : dx < -5 ? -5 : dx;
            pr_y[i] += dy > 4 ? 4 : dy < -4 ? -4 : dy;
        } else {
            pr_x[i] += pr_vx[i];
            pr_y[i] += pr_vy[i];
        }
        pr_fr[i]++;
        if (pr_t[i] == PR_KNIFEB) { if (pr_fr[i] > 250) pr_fr[i] = 250; }
        else if (--pr_l[i] == 0) {
            if (kn) { pr_t[i] = PR_KNIFEB; continue; }
            pr_t[i] = 0; continue;
        }
        f = flag_at(pr_x[i] + 4, pr_y[i] + 4);
        if (kn && (f & (MF_HURT | MF_KILL))) {                    /* lost in the lava */
            fx_spawn(pr_x[i], pr_y[i]); pr_t[i] = 0; sfx_play(SND_HURT);
            continue;
        }
        if (f & MF_SOLID) {
            if (pr_t[i] == PR_KNIFE) { fx_spawn(pr_x[i], pr_y[i]); pr_x[i] -= pr_vx[i]; pr_y[i] -= pr_vy[i]; pr_t[i] = PR_KNIFEB; }
            else if (!kn) { fx_spawn(pr_x[i], pr_y[i]); pr_t[i] = 0; continue; }
        }
        if (pr_t[i] == PR_KNIFEB && (frame_ctr & 3)) continue;
        for (f = 0; f < near_n; f++) {
            j = near_l[f];
            if (ot[j] == T_NONE) continue;
            if (pr_x[i] + 8 > ox[j] && pr_x[i] < ox[j] + ow[j] && pr_y[i] + 8 > oy[j] && pr_y[i] < oy[j] + oh[j]) {
                if (killable_dmg(j, 1)) {
                    sfx_play(SND_HIT);
                    if (kn) pr_t[i] = PR_KNIFEB; else pr_t[i] = 0;
                    break;
                }
            }
        }
    }
}

/* ---- main player update --------------------------------------------------- */
static u8 vine_here(void) {
    return (flag_at(px + (pw >> 1), py + (ph >> 1)) & MF_VINE) ||
           (flag_at(px + (pw >> 1), py + ph - 2) & MF_VINE);
}

static void player_fire(void) {
    if (pressed & K_FIRE) {
        if (form == FORM_HUMAN && knives && !crouch) {
            s8 dy = 0;
            if (held & K_UP) dy = -1; else if (held & K_DOWN) dy = 1;
            shoot(PR_KNIFE, px + (face_right ? 14 : -6), py + 6, face_right ? 4 : -4, dy);
            knives--; HUD();
            sfx_play(SND_THROW);
        } else if (form == FORM_PHOENIX && !fire_cd) {
            shoot(PR_FIRE, px + (face_right ? 12 : -12), py, face_right ? 5 : -5, 0);
            fire_cd = 14; sfx_play(SND_FIRE);
        } else if (form == FORM_FISH && !fire_cd) {
            shoot(PR_FISH, px + (face_right ? 16 : -4), py + 2, face_right ? 5 : -5, 0);
            fire_cd = 12; sfx_play(SND_THROW);
        }
    }
    if (fire_cd) fire_cd--;
}

static void player_walk(void) {
    int maxv = WALK_MAX, acc;
    u8 stun = hurt_t > 52;
    u8 ctl = !stun;
    u8 ground;
    in_water = (flag_at(px + (pw >> 1), py + (ph >> 1)) & MF_WATER) != 0;
    if (form == FORM_FROG) maxv = 330;
    if (in_water) maxv = 300;
    ground = ground_below();
    if (ground && vy >= 0) { if (!on_ground) { if (vy > 700) sfx_play(SND_LAND); } on_ground = 1; vy = 0; }
    else on_ground = 0;

    if (climbing) {
        vx = vy = 0;
        if (!vine_here()) {
            climbing = 0;
            if (held & K_UP) vy = -380;            /* hop off the top of the vine */
        } else if (ctl) {
            if (held & K_UP) { if (!(row_flags(py - 1) & MF_SOLID)) py--; }
            else if (held & K_DOWN) { if (!ground_below()) py++; else climbing = 0; }
            if (held & K_LEFT)  { face_right = 0; if (!(col_flags(px - 1) & MF_SOLID) && (frame_ctr & 1)) px--; }
            if (held & K_RIGHT) { face_right = 1; if (!(col_flags(px + pw) & MF_SOLID) && (frame_ctr & 1)) px++; }
            if (pressed & K_JUMP) { climbing = 0; vy = JUMP_V / 2; vx = (held & K_LEFT) ? -300 : (held & K_RIGHT) ? 300 : 0; jump_held = 1; sfx_play(SND_JUMP); }
            if (climbing) { anim_t++; return; }
        }
    } else if (ctl && (held & (K_UP | K_DOWN)) && form == FORM_HUMAN && vine_here() && !(held & K_JUMP)) {
        climbing = 1; vx = vy = 0;
        return;
    }

    crouch = 0;
    if (ctl) {
        acc = ground ? WALK_ACC : AIR_ACC;
        if (in_water) acc = 40;
        if (ground && (held & K_DOWN) && form == FORM_HUMAN) { crouch = 1; }
        if (!crouch && (held & K_LEFT)) {
            face_right = 0;
            vx -= acc; if (vx < -maxv) vx = -maxv;
        } else if (!crouch && (held & K_RIGHT)) {
            face_right = 1;
            vx += acc; if (vx > maxv) vx = maxv;
        } else {
            int fr = ground ? 80 : 24;
            if (vx > 0) { vx -= fr; if (vx < 0) vx = 0; }
            else if (vx < 0) { vx += fr; if (vx > 0) vx = 0; }
        }
        if (in_water) {
            if (held & K_JUMP) { vy -= 90; if (vy < -380) vy = -380; }
            else if (held & K_DOWN) { vy += 60; if (vy > 380) vy = 380; }
            else { vy += 14; if (vy > 160) vy = 160; }
            if ((pressed & K_JUMP) && on_ground) { vy = -700; sfx_play(SND_SPLASH); }
        } else {
            if ((pressed & K_JUMP) && ground) {
                vy = JUMP_V;
                if (form == FORM_FROG) vy = JUMP_V + JUMP_V / 4;
                if (hijump) vy = vy + vy / 3;
                on_ground = 0; jump_held = 1;
                sfx_play(SND_JUMP);
            }
            if (!(held & K_JUMP)) jump_held = 0;
            vy += GRAV;
            if (vy > FALL_MAX) vy = FALL_MAX;
        }
    } else {
        if (!in_water) { vy += GRAV; if (vy > FALL_MAX) vy = FALL_MAX; }
    }
    if (ground && vy >= 0 && !(pressed & K_JUMP)) vy = 0;
    apply_vel();
    if (vx != 0 || !on_ground) anim_t++;
}

static void player_fly(void) {         /* phoenix */
    u8 ctl = hurt_t <= 52;
    int m = 380;
    in_water = (flag_at(px + (pw >> 1), py + (ph >> 1)) & MF_WATER) != 0;
    if (in_water) { set_form(FORM_HUMAN); return; }
    on_ground = 0; climbing = 0;
    if (ctl) {
        if (held & K_LEFT)  { face_right = 0; vx -= 40; if (vx < -m) vx = -m; }
        else if (held & K_RIGHT) { face_right = 1; vx += 40; if (vx > m) vx = m; }
        else { if (vx > 0) vx -= 24; if (vx < 0) vx += 24; }
        if (held & K_UP) { vy -= 40; if (vy < -m) vy = -m; }
        else if (held & K_DOWN) { vy += 40; if (vy > m) vy = m; }
        else { if (vy > 0) vy -= 24; if (vy < 0) vy += 24; }
        if (pressed & K_JUMP) { vy = -m; }
    }
    apply_vel();
    anim_t++;
}

static void player_swim(void) {        /* fish */
    u8 ctl = hurt_t <= 52;
    int m = 300;
    in_water = (flag_at(px + (pw >> 1), py + (ph >> 1)) & MF_WATER) != 0;
    on_ground = 0; climbing = 0;
    if (in_water) {
        flop_t = 0;
        if (ctl) {
            if (held & K_LEFT)  { face_right = 0; vx -= 36; if (vx < -m) vx = -m; }
            else if (held & K_RIGHT) { face_right = 1; vx += 36; if (vx > m) vx = m; }
            else { if (vx > 0) vx -= 20; if (vx < 0) vx += 20; }
            if (held & (K_UP | K_JUMP)) { vy -= 36; if (vy < -m) vy = -m; }
            else if (held & K_DOWN) { vy += 36; if (vy > m) vy = m; }
            else { if (vy > 0) vy -= 20; if (vy < 0) vy += 20; }
        }
    } else {
        vy += GRAV; if (vy > FALL_MAX) vy = FALL_MAX;
        if (vx > 0) vx -= 20; if (vx < 0) vx += 20;
        if (++flop_t > 90) { set_form(FORM_HUMAN); return; }
    }
    apply_vel();
    anim_t++;
}

static void player_hazards(void) {
    u8 f;
    int cx = px + (pw >> 1);
    f = flag_at(cx, py + ph - 2) | flag_at(cx, py + (ph >> 1)) | flag_at(px + 2, py + ph - 2) | flag_at(px + pw - 3, py + ph - 2);
    if (f & MF_KILL) { player_die(); return; }
    if (f & MF_HURT) player_hurt(1);
    if (py > 1024) player_die();
}

static void player_update(void) {
    if (hurt_t) hurt_t--;
    if (blade_t) { blade_t--; if (!blade_t) HUD(); }
    switch (form) {
    case FORM_PHOENIX: player_fly(); break;
    case FORM_FISH:    player_swim(); break;
    default:           player_walk(); break;
    }
    player_fire();
    player_hazards();
    if (px < 0) px = 0;
}
