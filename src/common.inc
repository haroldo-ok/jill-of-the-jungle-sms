/* ------------------------------------------------------------------------
 * Jill of the Jungle - Sega Master System port
 * common definitions (included from main.c: single translation unit)
 * ---------------------------------------------------------------------- */

#include "SMSlib.h"
#include "game_data.h"

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned int u16;
typedef signed int s16;
typedef unsigned long u32;

__sfr __at 0xBF VDPCtl;
__sfr __at 0x7F PSGPort;

#define T_JILL 0
#define MAP_W 128
#define MAP_H 64
#define MAXO  128
#define T_NONE 0

/* object types (original JN1 type ids) */
#define T_PLAYER   0
#define T_APPLE    1
#define T_KNIFEI   2
#define T_SLUG     11
#define T_CHECK    12
#define T_KEY      14
#define T_PAD      15
#define T_MIST     19
#define T_TEXT     20
#define T_TEXT2    21
#define T_FROG     22
#define T_DOOR     24
#define T_FALLDOOR 25
#define T_BRIDGER  26
#define T_TOKEN    28
#define T_ANT      29
#define T_PHOENIX  30
#define T_SWITCH   32
#define T_CRYSTAL  33
#define T_BOULDER  35
#define T_STALAG   38
#define T_SNAKE    39
#define T_ROCK     40
#define T_MEGA     42
#define T_KNIGHT   44
#define T_NEST     45
#define T_CRAB     47
#define T_GATOR    48
#define T_EPIC     49
#define T_SKULL    51
#define T_PAC      53
#define T_JELLY    59
#define T_EEL      60
#define T_ELEV     61
#define T_EYE      64
#define T_SPARK    65
#define T_FLAG     66
#define T_BEES     100     /* runtime only: swarm released by a nest */
#define T_DEAD     101     /* runtime only: dying monster (puff animation) */

/* object record layout in ROM (u16 words) */
#define OR_TYP 0
#define OR_X   1
#define OR_Y   2
#define OR_W   3
#define OR_H   4
#define OR_XD  5
#define OR_YD  6
#define OR_ST  7
#define OR_CNT 8
#define OR_AUX 9
#define OR_STR 10
#define OR_TGT 11
#define OR_SIZE 12

/* player forms */
#define FORM_HUMAN   0
#define FORM_FROG    1
#define FORM_PHOENIX 2
#define FORM_FISH    3

/* sound events -> original digitised effect numbers */
#define SND_JUMP    SFX_18
#define SND_LAND    SFX_23
#define SND_APPLE   SFX_15
#define SND_KEY     SFX_04
#define SND_GEM     SFX_33
#define SND_HURT    SFX_19
#define SND_DIE     SFX_12
#define SND_THROW   SFX_28
#define SND_HIT     SFX_35
#define SND_DOOR    SFX_08
#define SND_SWITCH  SFX_39
#define SND_MORPH   SFX_41
#define SND_DONE    SFX_06
#define SND_SPLASH  SFX_25
#define SND_BOULDER SFX_05
#define SND_THUD    SFX_02
#define SND_ZAP     SFX_10
#define SND_FIRE    SFX_03
#define SND_TOKEN   SFX_01
#define SND_BOUNCE  SFX_11
#define SND_FINAL   SFX_48
#define SND_MENU    SFX_16
#define SND_EAT     SFX_15

/* ---- ROM banking ------------------------------------------------------ */
static u8 cur_bank;                      /* ROM slot 2 (data banks) */

/* ROM slot 1 holds switchable code overlays (see tools/split_overlays.py) */
#define OVL_A 30
#define OVL_B 31
u8 cur_ovl = 1;
#define OVL_SET(b) do { cur_ovl = (b); *(volatile u8 *)0xFFFE = cur_ovl; } while (0)
#define BANK(b) do { u8 _b = (b); if (cur_bank != _b) { cur_bank = _b; SMS_mapROMBank(_b); } } while (0)

/* ---- global game state (non-static ones are watched by the playtests) --- */
const LevelDesc *ld;
u8 cur_level;
u8 mac_on;                     /* scripted input (ending macro) is driving the player */
int px, py;                 /* player physics box: top-left, 16 wide x 24 tall */
int vx, vy;                 /* 8.8 fixed */
unsigned int cam_x, cam_y;
u8 on_ground, climbing, in_water;
u8 face_right;
u8 health, form;
u8 keys, gems, knives, blades, hijump;
u16 score_lo;               /* score = score_hi * 10000 + score_lo */
u16 score_hi;
u8 lives_used;
u8 game_flags;
u8 num_obj;
u8 frame_ctr;
u16 pressed, held;

/* runtime objects ---------------------------------------------------------- */
u8  ot[MAXO];
u8  near_f[MAXO];            /* per-frame: object is inside the update/draw window */
int ox[MAXO], oy[MAXO];
u8  ow[MAXO], oh[MAXO];
s8  ovx[MAXO], ovy[MAXO];
u8  ost[MAXO], oct[MAXO], ofr[MAXO];
u16 otag[MAXO];
int oxd[MAXO], oyd[MAXO];
u16 oaux[MAXO], otgt[MAXO];
u8  ohp[MAXO];

static u8 rnd_s = 0x5B;
static u8 rnd(void) {
    rnd_s ^= (u8)(rnd_s << 7);
    rnd_s ^= (u8)(rnd_s >> 5);
    rnd_s ^= (u8)(rnd_s << 3);
    return rnd_s;
}

#define ABS(a) ((a) < 0 ? -(a) : (a))
