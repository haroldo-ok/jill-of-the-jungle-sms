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
#define MAX_OVR   80
#define MAX_ENEMIES 64

#define WALK_ACC 96
#define WALK_MAX 512
#define AIR_ACC  56
#define GRAV     48
/* JUMP_V tuned so a full held jump apexes at ~3.5 metatiles (56 px), matching
   the reach of the original DOS game where Jill clears roughly 4 tiles.  Apex
   height h = JUMP_V^2 / (2*GRAV) in 8.8 units.  The short-hop cap (JUMP_CUT)
   gives the variable-height jump: releasing early clamps upward velocity. */
#define JUMP_V   (-1340)
#define JUMP_CUT (-420)
#define FALL_MAX 1280
#define JUMP_BUF_FRAMES 6
#define COYOTE_FRAMES   5

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
static unsigned char jump_buf;   /* frames a jump press stays buffered */
static unsigned char coyote;     /* frames of grace after leaving a ledge */
static unsigned char jumping;    /* in the rising, button-held phase */

/* ------------------------------------------------------------------ */
/* Player transformations.  In the original, Jill can morph into other  */
/* creatures with different movement abilities.  We implement three     */
/* forms plus her normal state; a morph is granted by a creature-potion */
/* pickup and lasts for a timed duration.                               */
#define FORM_JILL 0
#define FORM_BIRD 1   /* flap to fly freely, ignores gravity while flapping */
#define FORM_FROG 2   /* very high jump, poor air control                   */
#define FORM_FISH 3   /* free swimming in and out of water                  */
unsigned char form;             /* current FORM_* */
unsigned int  form_timer;       /* frames remaining before reverting */
#define FORM_DURATION 900       /* ~15s at the 60-iter/s loop pacing */
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
int en_x[MAX_ENEMIES], en_y[MAX_ENEMIES];
signed int en_vx[MAX_ENEMIES], en_vy[MAX_ENEMIES];
unsigned char en_type[MAX_ENEMIES], en_phase[MAX_ENEMIES];
unsigned char en_alive[MAX_ENEMIES];

/* Enemy behavior classes (see enemy_behavior() for the type->class mapping). */
#define EB_WALKER  0
#define EB_HOPPER  1
#define EB_FLYER   2
#define EB_CHASER  3
#define EB_SHOOTER 4
#define EB_STATIC  5
static unsigned char enemy_behavior(unsigned char typ);

/* Enemy projectiles (shared pool, distinct from Jill's knife). */
#define MAX_EPROJ 4
static int  ep_x[MAX_EPROJ], ep_y[MAX_EPROJ];
static int  ep_vx[MAX_EPROJ], ep_vy[MAX_EPROJ];
unsigned char ep_active[MAX_EPROJ];

unsigned char n_doors;
unsigned int door_x[MAX_DOORS], door_y[MAX_DOORS];
unsigned char door_w[MAX_DOORS], door_h[MAX_DOORS], door_open[MAX_DOORS];

unsigned char n_ovr;
unsigned char ovr_x[MAX_OVR], ovr_y[MAX_OVR], ovr_mt[MAX_OVR];

static unsigned int row_off[MAP_H];

/* ------------------------------------------------------------------ */
/* HUD font: 8x8 1bpp glyphs, expanded to 4bpp at load time.           */
/* Tiles 0-47 hold Jill/items/enemies and BG starts at 64, so 48..63   */
/* are free for the status display.                                     */
#define HUD_BASE   48u
#define GLYPH_HEART 10u
#define GLYPH_LIFE  11u
#define GLYPH_GEM   12u
#define N_GLYPHS    13u

static const unsigned char hud_glyphs[N_GLYPHS][8] = {
    {0x3C,0x66,0x6E,0x7E,0x76,0x66,0x3C,0x00}, /* 0 */
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, /* 1 */
    {0x3C,0x66,0x06,0x0C,0x30,0x60,0x7E,0x00}, /* 2 */
    {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00}, /* 3 */
    {0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0x00}, /* 4 */
    {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}, /* 5 */
    {0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00}, /* 6 */
    {0x7E,0x66,0x0C,0x18,0x18,0x18,0x18,0x00}, /* 7 */
    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}, /* 8 */
    {0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00}, /* 9 */
    {0x00,0x66,0xFF,0xFF,0xFF,0x7E,0x3C,0x18}, /* heart  */
    {0x18,0x18,0x3C,0x7E,0x18,0x24,0x66,0x00}, /* life   */
    {0x18,0x3C,0x7E,0xFF,0x7E,0x3C,0x18,0x00}, /* gem    */
};
/* Colour index per glyph: digits white, heart red, life yellow, gem cyan. */
static const unsigned char hud_colors[N_GLYPHS] = {
    15,15,15,15,15,15,15,15,15,15, 12, 14, 11
};

static void load_hud_tiles(void) {
    unsigned char g, row, plane, c;
    unsigned char tile[32];
    for (g = 0; g < N_GLYPHS; g++) {
        c = hud_colors[g];
        for (row = 0; row < 8; row++) {
            unsigned char bits = hud_glyphs[g][row];
            for (plane = 0; plane < 4; plane++)
                tile[row * 4 + plane] = (c & (1 << plane)) ? bits : 0;
        }
        SMS_loadTiles(tile, (unsigned int)(HUD_BASE + g), 32);
    }
}

/* ------------------------------------------------------------------ */
/* PSG: channel 0 + noise are reserved for sound effects, channels 1-2  */
/* for music, so an effect never silences the tune.                     */
__sfr __at 0x7F PSGPort;

/* Background music: ALL ORIGINAL compositions written for this port, in the    */
/* general upbeat jungle-platformer idiom.  Not a transcription of the original */
/* game's soundtrack.  Melody plays on PSG channel 1, bass on channel 2;        */
/* channel 0 and noise stay free for sound effects.                            */
/* Tone period = 3579545 / (32 * Hz).                                          */
#define P_REST 0
#define P_C3   851
#define P_D3   758
#define P_E3   678
#define P_F3   639
#define P_G3   570
#define P_A3   508
#define P_B3   451
#define P_C4   427
#define P_D4   380
#define P_E4   339
#define P_F4   320
#define P_G4   285
#define P_A4   254
#define P_B4   226
#define P_C5   214
#define P_D5   190
#define P_E5   169
#define P_F5   160
#define P_G5   143

#define TRACK_LEN 32   /* steps per track (melody and bass are parallel) */
#define N_TRACKS  4
#define MUS_TICKS 9    /* game-loop iterations per step (tempo) */

/* Track 0 - Overworld map: bright, marching, major key. */
static const unsigned int trk0_mel[TRACK_LEN] = {
    P_E4, P_G4, P_A4, P_G4,  P_E4, P_D4, P_E4, P_REST,
    P_G4, P_A4, P_C5, P_A4,  P_G4, P_E4, P_D4, P_REST,
    P_E5, P_D5, P_C5, P_A4,  P_G4, P_A4, P_C5, P_REST,
    P_A4, P_G4, P_E4, P_D4,  P_C4, P_D4, P_E4, P_REST,
};
static const unsigned int trk0_bass[TRACK_LEN] = {
    P_A3, 0, P_A3, 0,  P_E3, 0, P_E3, 0,
    P_F3, 0, P_F3, 0,  P_G3, 0, P_G3, 0,
    P_A3, 0, P_A3, 0,  P_F3, 0, P_F3, 0,
    P_C4, 0, P_G3, 0,  P_A3, 0, P_A3, 0,
};

/* Track 1 - Jungle/forest levels: busier, syncopated, a touch of swing. */
static const unsigned int trk1_mel[TRACK_LEN] = {
    P_A4, P_REST, P_C5, P_B4,  P_A4, P_G4, P_A4, P_REST,
    P_E4, P_G4, P_A4, P_REST,  P_C5, P_B4, P_A4, P_G4,
    P_A4, P_REST, P_C5, P_D5,  P_E5, P_D5, P_C5, P_B4,
    P_A4, P_G4, P_E4, P_G4,    P_A4, P_REST, P_A4, P_REST,
};
static const unsigned int trk1_bass[TRACK_LEN] = {
    P_A3, 0, 0, P_A3,  P_A3, 0, P_E3, 0,
    P_C3, 0, 0, P_C3,  P_G3, 0, P_G3, 0,
    P_F3, 0, 0, P_F3,  P_C3, 0, P_C3, 0,
    P_A3, 0, P_E3, 0,  P_A3, 0, P_A3, 0,
};

/* Track 2 - Caves/underground: darker, minor, sparse and moody. */
static const unsigned int trk2_mel[TRACK_LEN] = {
    P_A4, P_REST, P_REST, P_C5,  P_B4, P_REST, P_A4, P_REST,
    P_E4, P_REST, P_G4, P_REST,  P_A4, P_REST, P_REST, P_REST,
    P_C5, P_REST, P_B4, P_A4,    P_G4, P_REST, P_E4, P_REST,
    P_F4, P_REST, P_E4, P_D4,    P_E4, P_REST, P_REST, P_REST,
};
static const unsigned int trk2_bass[TRACK_LEN] = {
    P_A3, 0, 0, 0,  P_A3, 0, 0, 0,
    P_E3, 0, 0, 0,  P_E3, 0, 0, 0,
    P_F3, 0, 0, 0,  P_C3, 0, 0, 0,
    P_D3, 0, 0, 0,  P_E3, 0, 0, 0,
};

/* Track 3 - Water levels: flowing, gentle, lilting triplet feel. */
static const unsigned int trk3_mel[TRACK_LEN] = {
    P_C5, P_D5, P_E5, P_REST,  P_D5, P_C5, P_A4, P_REST,
    P_G4, P_A4, P_C5, P_REST,  P_A4, P_G4, P_E4, P_REST,
    P_C5, P_D5, P_E5, P_G5,    P_E5, P_D5, P_C5, P_REST,
    P_A4, P_C5, P_A4, P_G4,    P_E4, P_G4, P_A4, P_REST,
};
static const unsigned int trk3_bass[TRACK_LEN] = {
    P_C3, 0, P_G3, 0,  P_C3, 0, P_G3, 0,
    P_C3, 0, P_E3, 0,  P_C3, 0, P_E3, 0,
    P_F3, 0, P_C4, 0,  P_F3, 0, P_C4, 0,
    P_A3, 0, P_E3, 0,  P_A3, 0, P_A3, 0,
};

static const unsigned int *const track_mel[N_TRACKS]  = { trk0_mel,  trk1_mel,  trk2_mel,  trk3_mel  };
static const unsigned int *const track_bass[N_TRACKS] = { trk0_bass, trk1_bass, trk2_bass, trk3_bass };

/* Map each level to a track (levels beyond the table default to jungle). */
static unsigned char track_for_level(unsigned char lvl) {
    switch (lvl) {
    case 0:            return 0;   /* overworld map    */
    case 1: case 2:    return 1;   /* jungle           */
    case 3: case 4:    return 2;   /* caves            */
    case 5: case 6:    return 3;   /* water            */
    default:           return 1;
    }
}

unsigned char music_on = 1;
unsigned char music_track;      /* which track is currently playing */
unsigned char music_step;
static unsigned char music_t;

/* Event stingers: short original flourishes that briefly take over the melody
   channel (channel 1) without disturbing the bass or sound effects. */
#define STINGER_NONE  0
#define STINGER_KEY   1   /* picked up a key / crystal */
#define STINGER_CLEAR 2   /* completed / entered a new level */
#define STINGER_DEATH 3
static const unsigned int sting_key[4]   = { P_C5, P_E5, P_G5, P_REST };
static const unsigned int sting_clear[4] = { P_C5, P_G4, P_C5, P_E5 };
static const unsigned int sting_death[4] = { P_A4, P_F4, P_D4, P_C4 };
unsigned char stinger_kind, stinger_step, stinger_t;

static void psg_tone(unsigned char ch, unsigned int f, unsigned char att) {
    unsigned char latch = (unsigned char)(0x80 | (ch << 5));
    PSGPort = (unsigned char)(latch | (f & 0x0F));
    PSGPort = (unsigned char)((f >> 4) & 0x3F);
    PSGPort = (unsigned char)(latch | 0x10 | (att & 0x0F));
}

static void music_silence(void) {
    PSGPort = 0xBF;   /* ch1 off */
    PSGPort = 0xDF;   /* ch2 off */
}

static void music_set_track(unsigned char lvl) {
    unsigned char t = track_for_level(lvl);
    if (t != music_track) { music_track = t; music_step = 0; music_t = 0; }
}

static void stinger_play(unsigned char kind) {
    stinger_kind = kind;
    stinger_step = 0;
    stinger_t = 0;
}

static void music_update(void) {
    unsigned int m, b;
    if (!music_on) return;

    /* A stinger briefly overrides the melody channel; the bass keeps playing. */
    if (stinger_kind) {
        if (stinger_t) { stinger_t--; }
        else {
            const unsigned int *s = (stinger_kind == STINGER_KEY)   ? sting_key :
                                    (stinger_kind == STINGER_CLEAR) ? sting_clear :
                                                                      sting_death;
            if (stinger_step < 4) {
                if (s[stinger_step]) psg_tone(1, s[stinger_step], 4);
                stinger_step++;
                stinger_t = 5;
            } else {
                stinger_kind = STINGER_NONE;   /* hand the melody back to the track */
            }
        }
    }

    if (music_t) { music_t--; return; }
    music_t = MUS_TICKS;
    m = track_mel[music_track][music_step];
    b = track_bass[music_track][music_step];
    /* Don't touch the melody channel while a stinger owns it. */
    if (!stinger_kind) { if (m) psg_tone(1, m, 6); else PSGPort = 0xBF; }
    if (b) psg_tone(2, b, 9);
    music_step++;
    if (music_step >= TRACK_LEN) music_step = 0;
}

/* ------------------------------------------------------------------ */
/* Minimal PSG sound effects (channel 0 tone + channel 3 noise).       */

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
    /* The overlay list is almost always empty (it only holds opened-door
       stamps), so guarding the scan removes it from the per-tile hot path. */
    if (n_ovr) {
        for (i = 0; i < n_ovr; i++)
            if (ovr_x[i] == mx && ovr_y[i] == my) return ovr_mt[i];
    }
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

/* ------------------------------------------------------------------ */
/* Buffered background streaming, ported from the CK1-SMS engine.       */
/* Instead of one SMS_setTileatXY per tile (which re-issues the VRAM     */
/* address every call), we build a run of tiles in a little-endian int   */
/* buffer and commit it with SMSlib's timing-safe tilemap loaders.       */
/* This is the hot path during scrolling.                                */
/* Profiling marker in RAM: the test harness watches writes to this byte to time
   one game-loop iteration.  Unlike a port write, storing to RAM has no effect on
   real hardware, so this is safe to leave in the shipping ROM.  (An earlier
   version wrote to I/O port 0x3E, which is the SMS memory-control register --
   harmless on lenient emulators but a hard crash on accurate ones.) */
volatile unsigned char prof_marker;

static unsigned int tile_at_subcell(unsigned int wc, unsigned int wr) {
    unsigned char mx, my, mt;
    if (wc >= (MAP_W * 2u) || wr >= (MAP_H * 2u)) return 0;
    mx = (unsigned char)(wc >> 1);
    my = (unsigned char)(wr >> 1);
    mt = mt_at_cell(mx, my);
    return ld->mtdef[((unsigned int)mt << 2) + ((wr & 1u) << 1) + (wc & 1u)] + BG_BASE;
}

/* Column: 28 tiles down one name-table column.  Uses SMSlib's timing-safe
   per-tile SMS_setTileatXY, writing each tile to exactly the (x,y) the original
   raw-port version targeted.  The visible 28-row window is positioned by the
   Y-scroll register inside the 32-row name table, so the destination row wraps
   at 28 -- we compute it per tile exactly as the original's `wr % 28` did. */
static void draw_col_now(unsigned int wc) {
    unsigned char r;
    unsigned char ntx = (unsigned char)(wc & 31u);
    SMS_mapROMBank(ld->data_bank);
    for (r = 0; r < 28; r++) {
        unsigned int wr = (unsigned int)cam_ty + r;
        unsigned char nty = (unsigned char)(wr % 28u);
        SMS_setTileatXY(ntx, nty, tile_at_subcell(wc, wr));
    }
}

/* Row: 32 tiles across one name-table row.  Uses SMSlib's timing-safe per-tile
   SMS_setTileatXY.  The name table holds world columns cam_tx+1..cam_tx+32
   wrapped into 32 columns, so each tile's destination column is
   (cam_tx + 1 + c) & 31, exactly as the original raw-port version computed. */
static void draw_row_now(unsigned int wr) {
    unsigned char c;
    unsigned char nty = (unsigned char)(wr % 28u);
    SMS_mapROMBank(ld->data_bank);
    for (c = 0; c < 32; c++) {
        unsigned int wc = (unsigned int)cam_tx + 1u + c;
        unsigned char ntx = (unsigned char)(wc & 31u);
        SMS_setTileatXY(ntx, nty, tile_at_subcell(wc, wr));
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

/* Fast sprite path, ported from CK1-SMS: for the common fully-on-screen case
   we write SMSlib's sprite tables directly, skipping the per-sprite clipping
   arithmetic in SMS_addSpriteClipping.  Off-screen or partially-clipped cases
   fall back to the clipping version. */
extern unsigned char SpriteTableY[64];
extern unsigned char SpriteTableXN[128];
extern unsigned char SpriteNextFree;

static void draw_16x16(int sx, int sy, unsigned char base) {
    if ((unsigned int)sx <= 240 && (unsigned int)sy <= 176) {
        unsigned char x = (unsigned char)sx, y1 = (unsigned char)(sy - 1);
        unsigned char n = SpriteNextFree;
        unsigned char *ty, *xn;
        if (n > 60) return;
        ty = SpriteTableY + n;
        xn = SpriteTableXN + (unsigned char)(n << 1);
        ty[0] = y1;     ty[1] = y1;
        ty[2] = y1 + 8; ty[3] = y1 + 8;
        xn[0] = x;      xn[1] = base;
        xn[2] = x + 8;  xn[3] = base + 1;
        xn[4] = x;      xn[5] = base + 2;
        xn[6] = x + 8;  xn[7] = base + 3;
        SpriteNextFree = n + 4;
        return;
    }
    SMS_addSpriteClipping(sx,     sy,     base);
    SMS_addSpriteClipping(sx + 8, sy,     (unsigned char)(base + 1));
    SMS_addSpriteClipping(sx,     sy + 8, (unsigned char)(base + 2));
    SMS_addSpriteClipping(sx + 8, sy + 8, (unsigned char)(base + 3));
}

static void draw_16x32(int sx, int sy, unsigned char base) {
    if ((unsigned int)sx <= 240 && (unsigned int)sy <= 160) {
        unsigned char x = (unsigned char)sx, y1 = (unsigned char)(sy - 1);
        unsigned char n = SpriteNextFree;
        unsigned char *ty, *xn;
        if (n > 56) return;
        ty = SpriteTableY + n;
        xn = SpriteTableXN + (unsigned char)(n << 1);
        ty[0] = y1;      ty[1] = y1;
        ty[2] = y1 + 8;  ty[3] = y1 + 8;
        ty[4] = y1 + 16; ty[5] = y1 + 16;
        ty[6] = y1 + 24; ty[7] = y1 + 24;
        xn[0] = x;      xn[1]  = base;
        xn[2] = x + 8;  xn[3]  = base + 1;
        xn[4] = x;      xn[5]  = base + 2;
        xn[6] = x + 8;  xn[7]  = base + 3;
        xn[8] = x;      xn[9]  = base + 4;
        xn[10] = x + 8; xn[11] = base + 5;
        xn[12] = x;     xn[13] = base + 6;
        xn[14] = x + 8; xn[15] = base + 7;
        SpriteNextFree = n + 8;
        return;
    }
    SMS_addSpriteClipping(sx,     sy,      base);
    SMS_addSpriteClipping(sx + 8, sy,      (unsigned char)(base + 1));
    SMS_addSpriteClipping(sx,     sy + 8,  (unsigned char)(base + 2));
    SMS_addSpriteClipping(sx + 8, sy + 8,  (unsigned char)(base + 3));
    SMS_addSpriteClipping(sx,     sy + 16, (unsigned char)(base + 4));
    SMS_addSpriteClipping(sx + 8, sy + 16, (unsigned char)(base + 5));
    SMS_addSpriteClipping(sx,     sy + 24, (unsigned char)(base + 6));
    SMS_addSpriteClipping(sx + 8, sy + 24, (unsigned char)(base + 7));
}

static void draw_hud(void) {
    /* Added before any world sprites: on the SMS the lowest sprite index wins,
       so the status display always stays on top of Jill and the enemies. */
    unsigned char h = player_health;
    unsigned char l = lives;
    unsigned char g = gems;
    if (h > 9) h = 9;
    if (l > 9) l = 9;
    SMS_addSpriteClipping(8,   4, (unsigned char)(HUD_BASE + GLYPH_HEART));
    SMS_addSpriteClipping(18,  4, (unsigned char)(HUD_BASE + h));
    SMS_addSpriteClipping(44,  4, (unsigned char)(HUD_BASE + GLYPH_LIFE));
    SMS_addSpriteClipping(54,  4, (unsigned char)(HUD_BASE + l));
    SMS_addSpriteClipping(80,  4, (unsigned char)(HUD_BASE + GLYPH_GEM));
    if (g > 99) g = 99;
    SMS_addSpriteClipping(90,  4, (unsigned char)(HUD_BASE + (g / 10)));
    SMS_addSpriteClipping(100, 4, (unsigned char)(HUD_BASE + (g % 10)));
}

static void build_sprites(void) {
    unsigned char i;
    int sx = px - (int)cam_x;
    /* The physics box is 16x24, but Jill art is 16x32.  Draw the extra 8 px
       above the box so her feet stay aligned with the ground. */
    int sy = py - 8 - (int)cam_y;
    SMS_initSprites();
    draw_hud();
    if (hurt_t && (hurt_t & 2)) goto draw_world_sprites;
    if (form == FORM_JILL) {
        draw_16x32(sx, sy, (unsigned char)PLAYER_BASE);
    } else {
        /* Morphed: draw the creature sprite (16x16) centred on Jill's box.
           Bird = enemy sprite 1, frog = 2, fish reuses the frog shape. */
        unsigned char ci = (form == FORM_BIRD) ? 1 : 2;
        draw_16x16(sx, sy + 8, (unsigned char)(VT_ENEMY + ci * 4));
    }

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
    /* Enemy projectiles reuse the gem sprite (a small bright blob). */
    for (i = 0; i < MAX_EPROJ; i++) {
        int ex, ey;
        if (!ep_active[i]) continue;
        ex = ep_x[i] - (int)cam_x; ey = ep_y[i] - (int)cam_y;
        if (ex < -16 || ex > 255 || ey < -16 || ey > 191) continue;
        draw_16x16(ex, ey, (unsigned char)(ITEM_BASE + 3 * 4));
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
    load_hud_tiles();

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
        unsigned char beh;
        en_x[i] = (int)en[0]; en_y[i] = (int)en[1]; en_type[i] = (unsigned char)en[2];
        en_vy[i] = 0; en_alive[i] = 1; en_phase[i] = (unsigned char)(i * 17);
        beh = enemy_behavior(en_type[i]);
        /* Flyers drift slower; walkers/hoppers patrol at a walking pace. */
        if (beh == EB_FLYER)      en_vx[i] = (i & 1) ? 96 : -96;
        else if (beh == EB_STATIC || beh == EB_SHOOTER || beh == EB_CHASER) en_vx[i] = 0;
        else                      en_vx[i] = (i & 1) ? 128 : -128;
    }
    for (i = 0; i < MAX_EPROJ; i++) ep_active[i] = 0;

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
    form = FORM_JILL; form_timer = 0;
    redraw_needed = 1;
    update_camera();
    draw_screen();
    apply_hw_scroll();
    build_sprites();
    SMS_copySpritestoSAT();
    trigger_cool = 30;
    trans_latch = 1;   /* assume arrival may be inside a zone; require exit first */
    SMS_displayOn();
    music_set_track(lvl);
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
            stinger_play(STINGER_CLEAR);
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
            if (item_type[i] == 33) { has_crystal = 1; gems++; stinger_play(STINGER_KEY); }
            if (item_type[i] == 14) { has_red_key = 1; stinger_play(STINGER_KEY); }
            if (item_type[i] == 2)  has_weapon = 1;
            if (item_type[i] == 28 && (item_param[i] == 2 || item_param[i] == 8)) has_weapon = 1;
            /* Creature potions (object type 28) grant a timed morph.  The
               param selects which form; unknown params heal instead. */
            if (item_type[i] == 28) {
                switch (item_param[i]) {
                case 3:  case 11: form = FORM_BIRD; form_timer = FORM_DURATION; break;
                case 4:  case 12: form = FORM_FROG; form_timer = FORM_DURATION; break;
                case 5:  case 13: form = FORM_FISH; form_timer = FORM_DURATION; break;
                default: break;
                }
            }
        }
    }
}

static void open_door(unsigned char i) {
    unsigned char mx, my, my2, repl, row;
    if (door_open[i]) return;
    door_open[i] = 1;
    sfx_play(SFX_DOOR);
    mx  = (unsigned char)(door_x[i] >> 4);
    my  = (unsigned char)(door_y[i] >> 4);
    /* A door is door_h pixels tall (24 for the jewel/key doors), which spans
       more than one 16px metatile row.  Clear EVERY metatile the door covers,
       not just the top one -- otherwise the lower cell keeps its solid MAPDOOR
       collision and the player can see the top open but can't walk through. */
    my2 = (unsigned char)((door_y[i] + door_h[i] - 1) >> 4);
    /* Use a neighbouring background tile as the replacement fill. */
    repl = raw_mt_at_cell((unsigned char)(mx + 1), my);
    for (row = my; row <= my2; row++) {
        if (n_ovr < MAX_OVR) {
            ovr_x[n_ovr] = mx; ovr_y[n_ovr] = row; ovr_mt[n_ovr] = repl; n_ovr++;
        }
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
    stinger_play(STINGER_DEATH);
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
    form = FORM_JILL; form_timer = 0;
    vx = (px < source_x) ? -760 : 760;
    vy = -520;
    on_ground = 0;
}

static unsigned char enemy_flying(unsigned char typ) {
    return (typ == 30 || typ == 43 || typ == 45 || typ == 46 || typ == 59 || typ == 60) ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* Enemy behavior classes.  The original game has many distinct object  */
/* scripts; we group them into faithful movement archetypes (the EB_*   */
/* constants and this classifier's prototype are declared near the top).*/
static unsigned char enemy_behavior(unsigned char typ) {
    switch (typ) {
    case 30: case 43: case 45: case 46:            return EB_FLYER;
    case 59: case 60:                              return EB_CHASER;  /* flying homers */
    case 22:                                       return EB_HOPPER;
    case 35: case 38: case 41: case 65:            return EB_STATIC;  /* spikes/traps */
    case 7:  case 17: case 48: case 53:            return EB_SHOOTER;
    case 11:                                       return EB_SHOOTER;  /* phoenix */
    default:                                       return EB_WALKER;
    }
}

/* Enemy projectiles: pool declared near the top; spawn/update below. */
static void eproj_spawn(int x, int y, int vx8, int vy8) {
    unsigned char i;
    for (i = 0; i < MAX_EPROJ; i++) {
        if (!ep_active[i]) {
            ep_active[i] = 1;
            ep_x[i] = x; ep_y[i] = y; ep_vx[i] = vx8; ep_vy[i] = vy8;
            return;
        }
    }
}

static void update_eproj(void) {
    unsigned char i;
    for (i = 0; i < MAX_EPROJ; i++) {
        if (!ep_active[i]) continue;
        ep_x[i] += ep_vx[i] >> 6;
        ep_y[i] += ep_vy[i] >> 6;
        /* despawn off-map or on hitting solid terrain */
        if (ep_x[i] < 0 || ep_x[i] > (MAP_W * 16) || ep_y[i] < 0 || ep_y[i] > (MAP_H * 16) ||
            (flag_at_px(ep_x[i] + 4, ep_y[i] + 4) & JF_SOLID)) {
            ep_active[i] = 0; continue;
        }
        if (!hurt_t && px + 13 >= ep_x[i] && px + 2 <= ep_x[i] + 8 &&
            py + 23 >= ep_y[i] && py + 2 <= ep_y[i] + 8) {
            ep_active[i] = 0;
            player_hurt(ep_x[i]);
        }
    }
}

static unsigned char on_ground_enemy(unsigned char i) {
    return (flag_at_px(en_x[i] + 4, en_y[i] + 16) |
            flag_at_px(en_x[i] + 11, en_y[i] + 16)) & (JF_SOLID | JF_PLAT);
}

static void update_enemies(void) {
    unsigned char i;
    for (i = 0; i < n_enemies; i++) {
        int nx, ny;
        unsigned char beh;
        if (!en_alive[i]) continue;
        /* Off-screen enemies are frozen (both cheap and matches the original's
           behaviour of only animating what's near the player). */
        if (en_x[i] < (int)cam_x - 64 || en_x[i] > (int)cam_x + 320 ||
            en_y[i] < (int)cam_y - 64 || en_y[i] > (int)cam_y + 256) continue;
        en_phase[i]++;
        beh = enemy_behavior(en_type[i]);

        switch (beh) {
        case EB_FLYER: {
            /* Horizontal drift with a sine-like vertical bob (triangle wave). */
            nx = en_x[i] + (en_vx[i] >> 7);
            if (nx < 0 || nx > (MAP_W * 16) - 16 ||
                ((flag_at_px(nx + 2, en_y[i] + 8) | flag_at_px(nx + 13, en_y[i] + 8)) & JF_SOLID))
                en_vx[i] = -en_vx[i];
            else en_x[i] = nx;
            en_y[i] += ((en_phase[i] >> 3) & 1) ? 1 : -1;
            break;
        }
        case EB_CHASER: {
            /* Flying homer: eases toward Jill's position on both axes. */
            int tx = px, ty = py;
            if (en_x[i] < tx - 1) en_x[i] += 2; else if (en_x[i] > tx + 1) en_x[i] -= 2;
            if (en_y[i] < ty - 1) en_y[i] += 1; else if (en_y[i] > ty + 1) en_y[i] -= 1;
            break;
        }
        case EB_HOPPER: {
            /* Frog-like: leaps periodically, gravity brings it back down. */
            if (on_ground_enemy(i) && (en_phase[i] & 63) == 0) {
                en_vy[i] = -700;
                en_vx[i] = (px > en_x[i]) ? 110 : -110;   /* hop toward Jill */
            }
            en_vy[i] += 32; if (en_vy[i] > 768) en_vy[i] = 768;
            nx = en_x[i] + (en_vx[i] >> 7);
            if (((flag_at_px(nx + 2, en_y[i] + 8) | flag_at_px(nx + 13, en_y[i] + 8)) & JF_SOLID))
                en_vx[i] = 0;
            else en_x[i] = nx;
            ny = en_y[i] + (en_vy[i] >> 8);
            if (en_vy[i] > 0 &&
                ((flag_at_px(en_x[i] + 4, ny + 15) | flag_at_px(en_x[i] + 11, ny + 15)) & (JF_SOLID | JF_PLAT))) {
                en_y[i] = ((ny + 15) & ~15) - 16; en_vy[i] = 0; en_vx[i] = 0;
            } else en_y[i] = ny;
            break;
        }
        case EB_SHOOTER: {
            /* Mostly stationary; fires a projectile at Jill on a timer when she
               is roughly on-screen and facing-reachable. */
            en_vy[i] += 32; if (en_vy[i] > 768) en_vy[i] = 768;
            ny = en_y[i] + (en_vy[i] >> 8);
            if (en_vy[i] > 0 &&
                ((flag_at_px(en_x[i] + 4, ny + 15) | flag_at_px(en_x[i] + 11, ny + 15)) & (JF_SOLID | JF_PLAT))) {
                en_y[i] = ((ny + 15) & ~15) - 16; en_vy[i] = 0;
            } else en_y[i] = ny;
            if ((en_phase[i] & 63) == 0) {
                int dx = px - en_x[i];
                int dy = py - en_y[i];
                int vx8 = (dx >= 0) ? 480 : -480;
                int vy8 = (dy < -8) ? -220 : (dy > 8 ? 220 : 0);  /* aim up/down a bit */
                eproj_spawn(en_x[i] + 4, en_y[i] + 4, vx8, vy8);
            }
            break;
        }
        case EB_STATIC:
            /* Fixed hazard: no movement, contact damage handled below. */
            break;
        default: {  /* EB_WALKER */
            /* Patrols horizontally, reverses at a wall or the edge of a ledge. */
            unsigned char grounded = on_ground_enemy(i);
            en_vy[i] += 32; if (en_vy[i] > 768) en_vy[i] = 768;
            nx = en_x[i] + (en_vx[i] >> 7);
            {
                unsigned char wall = (flag_at_px(nx + 2, en_y[i] + 8) |
                                      flag_at_px(nx + 13, en_y[i] + 8)) & JF_SOLID;
                /* Ledge check: look a few px beyond the leading edge for floor.
                   Checking ahead of the *current* position (not the tentative
                   nx) means the turn happens before the enemy steps into empty
                   space and starts falling. */
                int lead = (en_vx[i] >= 0) ? (en_x[i] + 18) : (en_x[i] - 2);
                unsigned char ground_ahead =
                    (flag_at_px(lead, en_y[i] + 18)) & (JF_SOLID | JF_PLAT);
                if (wall || (grounded && !ground_ahead)) en_vx[i] = -en_vx[i];
                else en_x[i] = nx;
            }
            ny = en_y[i] + (en_vy[i] >> 8);
            if (en_vy[i] > 0 &&
                ((flag_at_px(en_x[i] + 4, ny + 15) | flag_at_px(en_x[i] + 11, ny + 15)) & (JF_SOLID | JF_PLAT))) {
                en_y[i] = ((ny + 15) & ~15) - 16; en_vy[i] = 0;
            } else en_y[i] = ny;
            break;
        }
        }

        /* contact damage */
        if (!hurt_t && px + 13 >= en_x[i] && px + 2 <= en_x[i] + 15 &&
            py + 23 >= en_y[i] && py + 2 <= en_y[i] + 15) player_hurt(en_x[i]);
        /* killed by Jill's knife */
        if (b_active && b_x + 15 >= en_x[i] && b_x <= en_x[i] + 15 &&
            b_y + 15 >= en_y[i] && b_y <= en_y[i] + 15) {
            en_alive[i] = 0; b_active = 0;
            enemies_killed++;
            sfx_play(SFX_HURT);
        }
    }
    update_eproj();
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
    /* Morph forms are timed; revert to Jill when the timer runs out. */
    if (form != FORM_JILL) {
        if (form_timer) form_timer--;
        else form = FORM_JILL;
    }
    /* Coyote time: full grace while standing, counting down once airborne. */
    if (on_ground) coyote = COYOTE_FRAMES;
    else if (coyote) coyote--;

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

    /* --- vertical: form-specific movement -------------------------- */
    if (form == FORM_BIRD) {
        /* Bird: each press flaps upward; gravity is gentle so held taps let
           Jill gain height and glide.  Ignores water entirely. */
        if (pressed & PORT_A_KEY_1) { vy = -600; sfx_play(SFX_JUMP); }
        vy += GRAV >> 2;
        if (vy > 420) vy = 420;
        on_ground = 0;
    } else if (form == FORM_FISH || in_water) {
        /* Fish (or anyone in water): buoyant free-swimming with up strokes. */
        if (pressed & PORT_A_KEY_1) {
            vy = (form == FORM_FISH) ? -560 : -640;
            sfx_play(SFX_JUMP);
        }
        if (keys & PORT_A_KEY_UP)   vy -= GRAV;      /* fish can steer vertically */
        if (keys & PORT_A_KEY_DOWN) vy += GRAV;
        vy += GRAV >> 2;
        if (vy > 384) vy = 384;
        if (vy < -420) vy = -420;
    } else {
        /* Jump buffering: a press within JUMP_BUF_FRAMES of landing still fires,
           and coyote time lets a jump register just after walking off a ledge. */
        int jv = (form == FORM_FROG) ? (JUMP_V * 3 / 2) : JUMP_V;  /* frog leaps high */
        if (pressed & PORT_A_KEY_1) jump_buf = JUMP_BUF_FRAMES;
        if (jump_buf && (on_ground || coyote)) {
            jump_buf = 0; coyote = 0;
            vy = jv;
            on_ground = 0;
            jumping = 1;
            sfx_play(SFX_JUMP);
        }
        if (jump_buf) jump_buf--;
        /* Variable height: releasing the button during the rise cuts the climb.
           The frog is committed to its leap (no early cut), giving a fixed arc. */
        if (jumping && form != FORM_FROG) {
            if (!(keys & PORT_A_KEY_1)) { if (vy < JUMP_CUT) vy = JUMP_CUT; jumping = 0; }
            else if (vy >= 0) jumping = 0;
        } else if (jumping && vy >= 0) jumping = 0;
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
    form = FORM_JILL; form_timer = 0;
    lives = 3;
    gems = 0;
    items_collected = enemies_killed = 0;

    load_level_at(0, 0, 0);
    prev_keys = SMS_getKeysStatus();

    for (;;) {
        SMS_waitForVBlank();
        prof_marker = 0xA0;   /* profiling: game-loop body start (RAM, safe) */
        SMS_copySpritestoSAT();
        apply_hw_scroll();
        sfx_update();
        music_update();
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
        prof_marker = 0xA1;   /* profiling: game-loop body end (RAM, safe) */
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
SMS_EMBED_SDSC_HEADER_AUTO_DATE(0, 1,
    "jill-sms",
    "Jill SMS WIP",
    "Compileable Sega Master System conversion scaffold for Jill of the Jungle");
