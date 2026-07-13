/*
 * Jill of the Jungle -> Sega Master System conversion scaffold
 * devkitSMS / SDCC / SMSlib
 *
 * This is a compileable WIP port engine.  It consumes converted data generated
 * from the original Jill shareware files: 16x16 map tiles, collision flags,
 * player spawn points and checkpoint transitions.
 */

#include "SMSlib.h"
#include "game_data.h"

#define MAP_W 128
#define MAP_H 64
#define SCREEN_TW 32
#define SCREEN_TH 28

#define VT_PLAYER 0u
#define VT_ITEMS  8u
#define VT_ENEMY  28u
#define BG_BASE   64u
#define PLAYER_BASE ((unsigned char)VT_PLAYER)
#define ITEM_BASE   ((unsigned char)VT_ITEMS)
#define MAX_ITEMS 96
#define MAX_DOORS 32
#define MAX_OVR   32
#define MAX_ENEMIES 64

#define WALK_ACC 96
#define WALK_MAX 512
#define AIR_ACC  56
#define GRAV     52
#define JUMP_V  (-1120)
#define FALL_MAX 1280

static const LevelDesc *ld;
/* Exported (non-static) so the automated playtest harness can inspect and
   drive the game via the SDCC .noi symbol table. */
unsigned char cur_level;
int px, py;
int vx, vy;
static unsigned int cam_x, cam_y;
static int sx_acc, sy_acc;
unsigned char on_ground;
unsigned char climbing;      /* attached to a vine */
unsigned char in_water;
static unsigned char face_right;
static unsigned char cam_tx, cam_ty;     /* camera in 8x8 subtiles */
static unsigned char old_cam_tx, old_cam_ty;
static unsigned char pframe, cur_pframe, anim_t;
static unsigned int pend_col, pend_row;
static unsigned int prev_keys;
static unsigned char trigger_cool;
static unsigned char trans_latch;   /* 1 while player stands in any transition zone */
static unsigned char redraw_needed;

unsigned char n_items;
unsigned int item_x[MAX_ITEMS], item_y[MAX_ITEMS];
unsigned char item_type[MAX_ITEMS], item_param[MAX_ITEMS];
unsigned char item_taken[MAX_ITEMS];
unsigned char has_crystal, has_red_key, has_weapon;
unsigned char player_health, lives, gems;
unsigned char items_collected, enemies_killed;
static unsigned char hurt_t;
static unsigned char b_active, b_right;
static int b_x, b_y;
static unsigned int return_x, return_y;
static unsigned char return_valid;

unsigned char n_enemies;
static int en_x[MAX_ENEMIES], en_y[MAX_ENEMIES];
static signed int en_vx[MAX_ENEMIES], en_vy[MAX_ENEMIES];
static unsigned char en_type[MAX_ENEMIES], en_phase[MAX_ENEMIES];
unsigned char en_alive[MAX_ENEMIES];

static unsigned char n_doors;
static unsigned int door_x[MAX_DOORS], door_y[MAX_DOORS];
static unsigned char door_w[MAX_DOORS], door_h[MAX_DOORS], door_open[MAX_DOORS];

static unsigned char n_ovr;
static unsigned char ovr_x[MAX_OVR], ovr_y[MAX_OVR], ovr_mt[MAX_OVR];

static unsigned int row_off[MAP_H];

/* ------------------------------------------------------------------ */
/* Minimal PSG sound effects (channel 0 tone + channel 3 noise).       */
__sfr __at 0x7F PSGPort;

#define SFX_NONE   0
#define SFX_JUMP   1
#define SFX_PICKUP 2
#define SFX_HURT   3
#define SFX_THROW  4
#define SFX_DOOR   5

static unsigned char sfx_kind, sfx_t;

static void sfx_play(unsigned char kind) {
    sfx_kind = kind;
    sfx_t = 12;
}

static void psg_tone0(unsigned int f, unsigned char att) {
    PSGPort = (unsigned char)(0x80 | (f & 0x0F));
    PSGPort = (unsigned char)((f >> 4) & 0x3F);
    PSGPort = (unsigned char)(0x90 | (att & 0x0F));
}

static void sfx_update(void) {
    unsigned int f;
    if (!sfx_t) return;
    sfx_t--;
    if (!sfx_t) { PSGPort = 0x9F; PSGPort = 0xFF; return; }  /* silence */
    switch (sfx_kind) {
    case SFX_JUMP:   f = 200u + (unsigned int)sfx_t * 30u;  psg_tone0(f, 4); break;
    case SFX_PICKUP: f = 620u - (unsigned int)sfx_t * 40u;  psg_tone0(f, 3); break;
    case SFX_HURT:   PSGPort = (unsigned char)(0xE0 | 0x06);            /* noise */
                     PSGPort = (unsigned char)(0xF0 | (12 - sfx_t < 12 ? (unsigned char)(sfx_t) : 0)); break;
    case SFX_THROW:  f = 150u + (unsigned int)sfx_t * 12u;  psg_tone0(f, 6); break;
    case SFX_DOOR:   f = 900u - (unsigned int)sfx_t * 55u;  psg_tone0(f, 4); break;
    default: break;
    }
}
/* ------------------------------------------------------------------ */

static unsigned char clamp_u8_int(int v, int lo, int hi) {
    if (v < lo) return (unsigned char)lo;
    if (v > hi) return (unsigned char)hi;
    return (unsigned char)v;
}

static unsigned char mt_at_cell(unsigned char mx, unsigned char my) {
    unsigned char i;
    if (mx >= MAP_W || my >= MAP_H) return 0;
    for (i = 0; i < n_ovr; i++)
        if (ovr_x[i] == mx && ovr_y[i] == my) return ovr_mt[i];
    return ld->map[row_off[my] + mx];
}

static unsigned char raw_mt_at_cell(unsigned char mx, unsigned char my) {
    if (mx >= MAP_W || my >= MAP_H) return 0;
    return ld->map[row_off[my] + mx];
}

static unsigned char flag_at_px(int x, int y) {
    unsigned char mx, my, mt;
    if (x < 0 || y < 0) return JF_SOLID;
    mx = (unsigned char)((unsigned int)x >> 4);
    my = (unsigned char)((unsigned int)y >> 4);
    if (mx >= MAP_W) return JF_SOLID;
    if (my >= MAP_H) return 0;
    mt = mt_at_cell(mx, my);
    return ld->flags[mt];
}

static unsigned int tile_at_subcell(unsigned int wc, unsigned int wr) {
    unsigned char mx, my, mt;
    if (wc >= (MAP_W * 2u) || wr >= (MAP_H * 2u)) return 0;
    mx = (unsigned char)(wc >> 1);
    my = (unsigned char)(wr >> 1);
    mt = mt_at_cell(mx, my);
    return ld->mtdef[((unsigned int)mt << 2) + ((wr & 1u) << 1) + (wc & 1u)] + BG_BASE;
}

static void draw_col_now(unsigned int wc) {
    unsigned char r;
    SMS_mapROMBank(ld->data_bank);
    for (r = 0; r < 28; r++) {
        unsigned int wr = (unsigned int)cam_ty + r;
        SMS_setTileatXY((unsigned char)(wc & 31u), (unsigned char)(wr % 28u), tile_at_subcell(wc, wr));
    }
}

static void draw_row_now(unsigned int wr) {
    unsigned char c;
    SMS_mapROMBank(ld->data_bank);
    for (c = 0; c < 32; c++) {
        unsigned int wc = (unsigned int)cam_tx + 1u + c;
        SMS_setTileatXY((unsigned char)(wc & 31u), (unsigned char)(wr % 28u), tile_at_subcell(wc, wr));
    }
}

static void draw_screen(void) {
    unsigned char c;
    /* CK1-style ring buffer: the VDP scroll registers move pixel-by-pixel;
       the name table contains world columns cam_tx+1..cam_tx+32, wrapped by
       hardware.  The left column is blanked to hide the wrap seam. */
    SMS_mapROMBank(ld->data_bank);
    for (c = 0; c < 32; c++) draw_col_now((unsigned int)cam_tx + 1u + c);
    pend_col = pend_row = 0xFFFFu;
}

static void draw_pending_strips(void) {
    if (pend_col != 0xFFFFu) { draw_col_now(pend_col); pend_col = 0xFFFFu; }
    if (pend_row != 0xFFFFu) { draw_row_now(pend_row); pend_row = 0xFFFFu; }
}

static void update_camera(void) {
    int nx = px + 8 - 128;
    int ny = py + 12 - 96;
    old_cam_tx = cam_tx;
    old_cam_ty = cam_ty;
    if (nx < 0) nx = 0;
    if (ny < 0) ny = 0;
    if (nx > (MAP_W * 16) - 256) nx = (MAP_W * 16) - 256;
    if (ny > (MAP_H * 16) - 192) ny = (MAP_H * 16) - 192;
    cam_x = (unsigned int)nx;
    cam_y = (unsigned int)ny;
    cam_tx = (unsigned char)(cam_x >> 3);
    cam_ty = (unsigned char)(cam_y >> 3);
    if (cam_tx != old_cam_tx) {
        if (cam_tx == (unsigned char)(old_cam_tx + 1)) pend_col = (unsigned int)cam_tx + 32u;
        else if ((unsigned char)(cam_tx + 1) == old_cam_tx) pend_col = (unsigned int)cam_tx + 1u;
        else redraw_needed = 1;
    }
    if (cam_ty != old_cam_ty) {
        if (cam_ty == (unsigned char)(old_cam_ty + 1)) pend_row = (unsigned int)cam_ty + 24u;
        else if ((unsigned char)(cam_ty + 1) == old_cam_ty) pend_row = (unsigned int)cam_ty;
        else redraw_needed = 1;
    }
}

static void apply_hw_scroll(void) {
    SMS_setBGScrollX((unsigned char)(0 - (unsigned char)cam_x));
    SMS_setBGScrollY((unsigned char)(cam_y % 224u));
}

static unsigned char solid_x_right(int nx) {
    return (flag_at_px(nx + 13, py + 3) | flag_at_px(nx + 13, py + 12) |
            flag_at_px(nx + 13, py + 22)) & JF_SOLID;
}
static unsigned char solid_x_left(int nx) {
    return (flag_at_px(nx + 2, py + 3) | flag_at_px(nx + 2, py + 12) |
            flag_at_px(nx + 2, py + 22)) & JF_SOLID;
}

static void move_x(void) {
    int dx;
    sx_acc += vx;
    dx = sx_acc >> 8;
    sx_acc -= dx << 8;
    if (dx > 0) {
        if (solid_x_right(px + dx)) {
            px = (((px + dx + 13) & ~15) - 14);
            vx = 0; sx_acc = 0;
        } else px += dx;
    } else if (dx < 0) {
        if (solid_x_left(px + dx)) {
            px = ((((px + dx + 2) >> 4) + 1) << 4) - 2;
            vx = 0; sx_acc = 0;
        } else px += dx;
    }
    if (px < 0) px = 0;
    if (px > (MAP_W * 16) - 16) px = (MAP_W * 16) - 16;
}

static void move_y(void) {
    int dy;
    sy_acc += vy;
    dy = sy_acc >> 8;
    sy_acc -= dy << 8;
    if (dy > 0) {
        int old_feet = py + 23;
        int new_feet = py + 23 + dy;
        unsigned char f = flag_at_px(px + 4, new_feet) | flag_at_px(px + 11, new_feet);
        unsigned char crossed = ((unsigned int)old_feet >> 4) < ((unsigned int)new_feet >> 4);
        if ((f & JF_SOLID) || ((f & JF_PLAT) && crossed)) {
            py = (new_feet & ~15) - 24;
            vy = 0; sy_acc = 0; on_ground = 1;
        } else { py += dy; on_ground = 0; }
    } else if (dy < 0) {
        int new_head = py + 2 + dy;
        unsigned char f = flag_at_px(px + 4, new_head) | flag_at_px(px + 11, new_head);
        if ((f & JF_SOLID) && !(f & JF_PLAT)) {
            py = ((((new_head >> 4) + 1) << 4) - 2);
            vy = 0; sy_acc = 0;
        } else py += dy;
        on_ground = 0;
    } else {
        unsigned char f = flag_at_px(px + 4, py + 24) | flag_at_px(px + 11, py + 24);
        on_ground = (f & (JF_SOLID | JF_PLAT)) ? 1 : 0;
    }
    if (py > (MAP_H * 16) + 32) {
        px = (int)ld->spawn_x; py = (int)ld->spawn_y - 24;
        vx = vy = sx_acc = sy_acc = 0;
    }
}

static unsigned char item_sprite_index(unsigned char typ) {
    if (typ == 1) return 0;      /* apple/fruit */
    if (typ == 2) return 1;      /* knife */
    if (typ == 14) return 2;     /* red key */
    if (typ == 33) return 3;     /* crystal/gem */
    return 4;                    /* generic token/morph */
}

static unsigned char enemy_sprite_index(unsigned char typ) {
    if (typ == 30 || typ == 43 || typ == 45 || typ == 46) return 1;
    if (typ == 22) return 2;
    if (typ == 35 || typ == 38 || typ == 41 || typ == 65) return 3;
    if (typ == 7 || typ == 11 || typ == 17 || typ == 48 || typ == 53 || typ == 59 || typ == 60) return 4;
    return 0;
}

static void draw_16x16(int sx, int sy, unsigned char base) {
    SMS_addSpriteClipping(sx,     sy,     base);
    SMS_addSpriteClipping(sx + 8, sy,     (unsigned char)(base + 1));
    SMS_addSpriteClipping(sx,     sy + 8, (unsigned char)(base + 2));
    SMS_addSpriteClipping(sx + 8, sy + 8, (unsigned char)(base + 3));
}

static void build_sprites(void) {
    unsigned char i;
    int sx = px - (int)cam_x;
    /* The physics box is 16x24, but Jill art is 16x32.  Draw the extra 8 px
       above the box so her feet stay aligned with the ground. */
    int sy = py - 8 - (int)cam_y;
    SMS_initSprites();
    if (hurt_t && (hurt_t & 2)) goto draw_world_sprites;
    SMS_addSpriteClipping(sx,     sy,      (unsigned char)(PLAYER_BASE));
    SMS_addSpriteClipping(sx + 8, sy,      (unsigned char)(PLAYER_BASE + 1));
    SMS_addSpriteClipping(sx,     sy + 8,  (unsigned char)(PLAYER_BASE + 2));
    SMS_addSpriteClipping(sx + 8, sy + 8,  (unsigned char)(PLAYER_BASE + 3));
    SMS_addSpriteClipping(sx,     sy + 16, (unsigned char)(PLAYER_BASE + 4));
    SMS_addSpriteClipping(sx + 8, sy + 16, (unsigned char)(PLAYER_BASE + 5));
    SMS_addSpriteClipping(sx,     sy + 24, (unsigned char)(PLAYER_BASE + 6));
    SMS_addSpriteClipping(sx + 8, sy + 24, (unsigned char)(PLAYER_BASE + 7));

draw_world_sprites:
    for (i = 0; i < n_items; i++) {
        unsigned char si, base;
        int ix, iy;
        if (item_taken[i]) continue;
        ix = (int)item_x[i] - (int)cam_x;
        iy = (int)item_y[i] - (int)cam_y;
        if (ix < -16 || ix > 255 || iy < -16 || iy > 191) continue;
        si = item_sprite_index(item_type[i]);
        base = (unsigned char)(ITEM_BASE + si * 4);
        draw_16x16(ix, iy, base);
    }
    for (i = 0; i < n_enemies; i++) {
        int ex, ey;
        unsigned char si;
        if (!en_alive[i]) continue;
        ex = en_x[i] - (int)cam_x; ey = en_y[i] - (int)cam_y;
        if (ex < -24 || ex > 263 || ey < -24 || ey > 199) continue;
        si = enemy_sprite_index(en_type[i]);
        draw_16x16(ex, ey, (unsigned char)(VT_ENEMY + si * 4));
    }
    if (b_active) {
        /* Reuse the knife sprite as the first weapon projectile. */
        draw_16x16(b_x - (int)cam_x, b_y - (int)cam_y, (unsigned char)(ITEM_BASE + 4));
    }
}

static void load_level_at(unsigned char lvl, unsigned int sx, unsigned int sy) {
    unsigned char i;
    cur_level = lvl;
    ld = &level_descs[lvl];
    SMS_displayOff();
    SMS_initSprites();
    SMS_copySpritestoSAT();
    SMS_VRAMmemsetW(0, 0x0000, 16384);
    SMS_setBGScrollX(0);
    SMS_setBGScrollY(0);

    for (i = 0; i < MAP_H; i++) row_off[i] = ((unsigned int)i) * MAP_W;

    SMS_mapROMBank(ld->tile_bank);
    SMS_loadTiles(ld->tiles, BG_BASE, ld->tiles_size);
    SMS_mapROMBank(JILL_BANK_SPRITES);
    SMS_loadTiles(spr_jill, VT_PLAYER, 8 * 32);
    SMS_loadTiles(spr_items, VT_ITEMS, 20 * 32);
    SMS_loadTiles(spr_enemies, VT_ENEMY, 20 * 32);

    SMS_mapROMBank(ld->data_bank);
    n_ovr = 0;
    n_items = ld->nitems;
    if (n_items > MAX_ITEMS) n_items = MAX_ITEMS;
    for (i = 0; i < n_items; i++) {
        const unsigned int *it = ld->items + ((unsigned int)i << 2);
        item_x[i] = it[0]; item_y[i] = it[1];
        item_type[i] = (unsigned char)it[2]; item_param[i] = (unsigned char)it[3]; item_taken[i] = 0;
    }
    n_doors = ld->ndoors;
    if (n_doors > MAX_DOORS) n_doors = MAX_DOORS;
    for (i = 0; i < n_doors; i++) {
        const unsigned int *dr = ld->doors + (unsigned int)i * 7u;
        door_x[i] = dr[0]; door_y[i] = dr[1];
        door_w[i] = (unsigned char)dr[2]; door_h[i] = (unsigned char)dr[3];
        door_open[i] = 0;
    }
    n_enemies = ld->nenemies;
    if (n_enemies > MAX_ENEMIES) n_enemies = MAX_ENEMIES;
    for (i = 0; i < n_enemies; i++) {
        const unsigned int *en = ld->enemies + ((unsigned int)i << 2);
        en_x[i] = (int)en[0]; en_y[i] = (int)en[1]; en_type[i] = (unsigned char)en[2];
        en_vx[i] = (i & 1) ? 128 : -128; en_vy[i] = 0; en_alive[i] = 1; en_phase[i] = (unsigned char)(i * 17);
        if (en_type[i] == 30 || en_type[i] == 43 || en_type[i] == 45 || en_type[i] == 46) en_vx[i] = (i & 1) ? 96 : -96;
    }

    if (sx == 0 && sy == 0) { sx = ld->spawn_x; sy = ld->spawn_y; }
    px = (int)sx;
    py = (int)sy - 24;
    if (py < 0) py = 0;
    vx = vy = sx_acc = sy_acc = 0;
    face_right = 1;
    on_ground = 0;
    cam_x = cam_y = 0;
    cam_tx = cam_ty = 0;
    pend_col = pend_row = 0xFFFFu;
    pframe = cur_pframe = anim_t = 0;
    b_active = 0;
    climbing = 0;
    in_water = 0;
    redraw_needed = 1;
    update_camera();
    draw_screen();
    apply_hw_scroll();
    build_sprites();
    SMS_copySpritestoSAT();
    trigger_cool = 30;
    trans_latch = 1;   /* assume arrival may be inside a zone; require exit first */
    SMS_displayOn();
}

static void check_transitions(void) {
    unsigned char i;
    unsigned char inside = 0;
    const unsigned int *t;
    if (trigger_cool) { trigger_cool--; return; }
    SMS_mapROMBank(ld->data_bank);
    t = ld->trans;
    for (i = 0; i < ld->ntrans; i++) {
        unsigned int x = t[0], y = t[1], w = t[2], h = t[3];
        unsigned char target = (unsigned char)t[4];
        unsigned int sx = t[5], sy = t[6];
        t += 7;
        if ((unsigned int)(px + 13) >= x && (unsigned int)(px + 2) <= x + w &&
            (unsigned int)(py + 23) >= y && (unsigned int)(py + 2) <= y + h) {
            inside = 1;
            if (trans_latch) break;   /* still standing where we arrived */
            if (cur_level == 0 && target != 0) {
                return_x = (unsigned int)px; return_y = (unsigned int)py; return_valid = 1;
            } else if (target == 0 && return_valid) {
                sx = return_x; sy = return_y + 24;
            }
            load_level_at(target, sx, sy);
            return;
        }
    }
    trans_latch = inside;
}

static void check_items(void) {
    unsigned char i;
    for (i = 0; i < n_items; i++) {
        int ix, iy;
        if (item_taken[i]) continue;
        ix = (int)item_x[i]; iy = (int)item_y[i];
        if (px + 13 >= ix && px + 2 <= ix + 15 &&
            py + 23 >= iy && py + 2 <= iy + 15) {
            item_taken[i] = 1;
            items_collected++;
            sfx_play(SFX_PICKUP);
            if (item_type[i] == 1) {                 /* food: restore health */
                if (player_health < 8) player_health += 2;
                if (player_health > 8) player_health = 8;
            }
            if (item_type[i] == 33) { has_crystal = 1; gems++; }
            if (item_type[i] == 14) has_red_key = 1;
            if (item_type[i] == 2)  has_weapon = 1;
            if (item_type[i] == 28 && (item_param[i] == 2 || item_param[i] == 8)) has_weapon = 1;
        }
    }
}

static void open_door(unsigned char i) {
    unsigned char mx, my, repl;
    if (door_open[i]) return;
    door_open[i] = 1;
    sfx_play(SFX_DOOR);
    mx = (unsigned char)(door_x[i] >> 4);
    my = (unsigned char)(door_y[i] >> 4);
    /* Map jewel doors are represented as one solid MAPDOOR metatile.  Stamp
       the neighbouring background tile over it and make collision use that
       replacement. */
    repl = raw_mt_at_cell((unsigned char)(mx + 1), my);
    if (n_ovr < MAX_OVR) {
        ovr_x[n_ovr] = mx; ovr_y[n_ovr] = my; ovr_mt[n_ovr] = repl; n_ovr++;
    }
    redraw_needed = 1;
}

static void check_doors(void) {
    unsigned char i;
    if (!has_crystal && !has_red_key) return;
    for (i = 0; i < n_doors; i++) {
        int dx, dy;
        if (door_open[i]) continue;
        dx = (int)door_x[i]; dy = (int)door_y[i];
        if (px + 18 >= dx && px - 4 <= dx + door_w[i] &&
            py + 24 >= dy && py <= dy + door_h[i]) {
            open_door(i);
        }
    }
}

static void load_level_at(unsigned char lvl, unsigned int sx, unsigned int sy);

static void respawn_player(void) {
    if (lives) lives--;
    if (!lives) {
        /* Game over: reset progress and restart on the overworld map. */
        lives = 3;
        gems = 0;
        has_crystal = has_red_key = has_weapon = 0;
        items_collected = 0;
        player_health = 8; hurt_t = 90;
        load_level_at(0, 0, 0);
        return;
    }
    px = (int)ld->spawn_x; py = (int)ld->spawn_y - 24;
    vx = vy = sx_acc = sy_acc = 0;
    climbing = 0;
    player_health = 8; hurt_t = 90;
    cam_x = cam_y = 0; cam_tx = cam_ty = 0;
    update_camera();
    redraw_needed = 1;
}

static void player_hurt(int source_x) {
    if (hurt_t) return;
    if (player_health) player_health--;
    if (!player_health) { respawn_player(); return; }
    sfx_play(SFX_HURT);
    hurt_t = 90;
    climbing = 0;
    vx = (px < source_x) ? -760 : 760;
    vy = -520;
    on_ground = 0;
}

static unsigned char enemy_flying(unsigned char typ) {
    return (typ == 30 || typ == 43 || typ == 45 || typ == 46 || typ == 59 || typ == 60) ? 1 : 0;
}

static void update_enemies(void) {
    unsigned char i;
    for (i = 0; i < n_enemies; i++) {
        int nx, ny;
        if (!en_alive[i]) continue;
        if (en_x[i] < (int)cam_x - 64 || en_x[i] > (int)cam_x + 320 || en_y[i] < (int)cam_y - 64 || en_y[i] > (int)cam_y + 256) continue;
        en_phase[i]++;
        if (enemy_flying(en_type[i])) {
            en_x[i] += (en_vx[i] >> 7);
            if (en_x[i] < 0 || en_x[i] > (MAP_W * 16) - 16) en_vx[i] = -en_vx[i];
            if ((en_phase[i] & 31) == 0) en_vy[i] = -en_vy[i];
            en_y[i] += (en_vy[i] >= 0) ? 1 : -1;
        } else if (en_type[i] == 35 || en_type[i] == 38 || en_type[i] == 41 || en_type[i] == 65) {
            /* static/moving traps: keep simple for now */
        } else {
            if (en_type[i] == 22 && (en_phase[i] & 63) == 0) en_vy[i] = -650;
            en_vy[i] += 32; if (en_vy[i] > 768) en_vy[i] = 768;
            nx = en_x[i] + (en_vx[i] >> 7);
            if ((flag_at_px(nx + 2, en_y[i] + 8) | flag_at_px(nx + 13, en_y[i] + 8)) & JF_SOLID) en_vx[i] = -en_vx[i];
            else en_x[i] = nx;
            ny = en_y[i] + (en_vy[i] >> 8);
            if (en_vy[i] > 0 && ((flag_at_px(en_x[i] + 4, ny + 15) | flag_at_px(en_x[i] + 11, ny + 15)) & (JF_SOLID | JF_PLAT))) {
                en_y[i] = ((ny + 15) & ~15) - 16; en_vy[i] = 0;
            } else en_y[i] = ny;
        }
        if (px + 13 >= en_x[i] && px + 2 <= en_x[i] + 15 && py + 23 >= en_y[i] && py + 2 <= en_y[i] + 15) player_hurt(en_x[i]);
        if (b_active && b_x + 15 >= en_x[i] && b_x <= en_x[i] + 15 && b_y + 15 >= en_y[i] && b_y <= en_y[i] + 15) {
            en_alive[i] = 0; b_active = 0;
            enemies_killed++;
            sfx_play(SFX_HURT);
        }
    }
}

static void spawn_weapon(void) {
    if (!has_weapon || b_active) return;
    b_active = 1;
    sfx_play(SFX_THROW);
    b_right = face_right;
    b_x = px + (face_right ? 14 : -14);
    b_y = py + 8;
}

static void update_weapon(void) {
    if (!b_active) return;
    b_x += b_right ? 5 : -5;
    if (b_x < (int)cam_x - 32 || b_x > (int)cam_x + 288) { b_active = 0; return; }
    if (flag_at_px(b_x + 8, b_y + 8) & JF_SOLID) b_active = 0;
}

static void choose_player_frame(void) {
    unsigned char f;
    if (!on_ground) {
        f = face_right ? 13 : 5;
    } else if (vx > 64 || vx < -64) {
        static const unsigned char walk_l[6] = {0,1,3,4,5,7};
        static const unsigned char walk_r[6] = {8,9,11,12,13,15};
        anim_t++;
        f = face_right ? walk_r[(anim_t >> 2) % 6] : walk_l[(anim_t >> 2) % 6];
    } else {
        f = face_right ? 8 : 0;
    }
    if (f >= JILL_N_PLAYER_FRAMES) f = 0;
    pframe = f;
}

static void upload_player_frame(void) {
    if (pframe == cur_pframe) return;
    SMS_mapROMBank(JILL_BANK_SPRITES);
    SMS_loadTiles(spr_jill + ((unsigned int)pframe * 256u), VT_PLAYER, 8 * 32);
    cur_pframe = pframe;
}

static unsigned char flags_at_body(void) {
    return flag_at_px(px + 8, py + 8) | flag_at_px(px + 8, py + 18);
}

static void check_deadly(void) {
    unsigned char f = flag_at_px(px + 4, py + 6)  | flag_at_px(px + 11, py + 6) |
                      flag_at_px(px + 4, py + 22) | flag_at_px(px + 11, py + 22);
    if (f & JF_DEADLY) player_hurt(px - 8);
}

static void update_player(unsigned int keys, unsigned int pressed) {
    unsigned char body = flags_at_body();
    in_water = (body & JF_WATER) ? 1 : 0;

    if (hurt_t) hurt_t--;

    /* --- vine climbing -------------------------------------------- */
    if (climbing) {
        if (!(body & JF_VINE)) {
            climbing = 0;                    /* climbed off the vine */
        } else {
            vx = 0; vy = 0;
            if (keys & PORT_A_KEY_UP)    py -= 1;
            if (keys & PORT_A_KEY_DOWN)  py += 1;
            if (keys & PORT_A_KEY_LEFT)  { px -= 1; face_right = 0; }
            if (keys & PORT_A_KEY_RIGHT) { px += 1; face_right = 1; }
            if (pressed & PORT_A_KEY_1) {    /* jump off the vine */
                climbing = 0;
                vy = JUMP_V;
                sfx_play(SFX_JUMP);
            } else {
                /* landing on solid ground also detaches */
                unsigned char g = flag_at_px(px + 4, py + 24) | flag_at_px(px + 11, py + 24);
                if ((g & (JF_SOLID | JF_PLAT)) && (keys & PORT_A_KEY_DOWN)) climbing = 0;
                if (pressed & PORT_A_KEY_2) spawn_weapon();
                choose_player_frame();
                check_deadly();
                return;
            }
        }
    }
    if (!climbing && (body & JF_VINE) &&
        (keys & (PORT_A_KEY_UP | PORT_A_KEY_DOWN))) {
        climbing = 1;
        vx = vy = sx_acc = sy_acc = 0;
        on_ground = 0;
    }

    /* --- horizontal ------------------------------------------------ */
    if (keys & PORT_A_KEY_LEFT) {
        vx -= on_ground ? WALK_ACC : AIR_ACC;
        if (vx < -WALK_MAX) vx = -WALK_MAX;
        if (in_water && vx < -(WALK_MAX >> 1)) vx = -(WALK_MAX >> 1);
        face_right = 0;
    } else if (keys & PORT_A_KEY_RIGHT) {
        vx += on_ground ? WALK_ACC : AIR_ACC;
        if (vx > WALK_MAX) vx = WALK_MAX;
        if (in_water && vx > (WALK_MAX >> 1)) vx = (WALK_MAX >> 1);
        face_right = 1;
    } else {
        if (on_ground) vx = 0;
        else vx = (vx * 3) >> 2;
    }

    /* --- vertical: jump / swim ------------------------------------- */
    if (in_water) {
        if (pressed & PORT_A_KEY_1) {        /* swim stroke */
            vy = -640;
            sfx_play(SFX_JUMP);
        }
        vy += GRAV >> 2;                      /* buoyant gravity */
        if (vy > 384) vy = 384;               /* slow sinking */
    } else {
        if ((pressed & PORT_A_KEY_1) && on_ground) {
            vy = JUMP_V;
            on_ground = 0;
            sfx_play(SFX_JUMP);
        }
        if (!(keys & PORT_A_KEY_1) && vy < -360) vy = -360;
        if (!on_ground) {
            vy += GRAV;
            if (vy > FALL_MAX) vy = FALL_MAX;
        }
    }
    if (pressed & PORT_A_KEY_2) spawn_weapon();
    move_x();
    move_y();
    check_deadly();
    choose_player_frame();
}

void main(void) {
    unsigned int keys, pressed;
    SMS_setSpriteMode(SPRITEMODE_NORMAL);
    SMS_useFirstHalfTilesforSprites(1);
    SMS_VDPturnOnFeature(VDPFEATURE_LEFTCOLBLANK);
    SMS_loadBGPalette(bg_palette);
    SMS_loadSpritePalette(spr_palette);
    SMS_setBackdropColor(0);
    player_health = 8; hurt_t = 0;
    lives = 3;
    gems = 0;
    items_collected = enemies_killed = 0;

    load_level_at(0, 0, 0);
    prev_keys = SMS_getKeysStatus();

    for (;;) {
        SMS_waitForVBlank();
        UNSAFE_SMS_copySpritestoSAT();
        apply_hw_scroll();
        sfx_update();
        keys = SMS_getKeysStatus();
        pressed = keys & ~prev_keys;
        prev_keys = keys;

        update_player(keys, pressed);
        update_weapon();
        update_enemies();
        check_items();
        check_doors();
        check_transitions();
        update_camera();
        if (redraw_needed) {
            draw_screen();
            redraw_needed = 0;
        } else {
            draw_pending_strips();
        }
        upload_player_frame();
        SMS_mapROMBank(ld->data_bank);
        build_sprites();
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
SMS_EMBED_SDSC_HEADER_AUTO_DATE(0, 1,
    "jill-sms",
    "Jill SMS WIP",
    "Compileable Sega Master System conversion scaffold for Jill of the Jungle");
