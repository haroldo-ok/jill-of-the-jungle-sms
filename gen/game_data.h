#ifndef JILL_GAME_DATA_H
#define JILL_GAME_DATA_H

#include "SMSlib.h"

#define JILL_N_LEVELS 11
#define SPR_COUNT 205
#define JILL_N_FRAMES 75
#define T_SPR0 20
#define T_HUD 8
#define HUD_TILES 12

#define MF_SOLID 0x01
#define MF_PLAT  0x02
#define MF_VINE  0x04
#define MF_WATER 0x08
#define MF_HURT  0x10
#define MF_KILL  0x20
#define MF_MARK  0x40

#define SPR_APPLE 0
#define SPR_APPLE_N 4
#define SPR_KEY 4
#define SPR_KEY_N 4
#define SPR_CRYSTAL 8
#define SPR_CRYSTAL_N 1
#define SPR_KNIFEI 9
#define SPR_KNIFEI_N 1
#define SPR_TOK0 10
#define SPR_TOK0_N 1
#define SPR_TOK4 11
#define SPR_TOK4_N 1
#define SPR_TOK5 12
#define SPR_TOK5_N 1
#define SPR_TOK7 13
#define SPR_TOK7_N 1
#define SPR_TOK8 14
#define SPR_TOK8_N 1
#define SPR_TOK9 15
#define SPR_TOK9_N 1
#define SPR_KNIFE 16
#define SPR_KNIFE_N 5
#define SPR_BLADE 21
#define SPR_BLADE_N 4
#define SPR_FIREBALL 25
#define SPR_FIREBALL_N 4
#define SPR_FISHSHOT 29
#define SPR_FISHSHOT_N 2
#define SPR_ANT 31
#define SPR_ANT_N 10
#define SPR_PHOENIX 41
#define SPR_PHOENIX_N 12
#define SPR_FROG 53
#define SPR_FROG_N 6
#define SPR_SLUG 59
#define SPR_SLUG_N 7
#define SPR_CRAB 66
#define SPR_CRAB_N 4
#define SPR_GATOR 70
#define SPR_GATOR_N 16
#define SPR_SNAKE 86
#define SPR_SNAKE_N 12
#define SPR_PAC 98
#define SPR_PAC_N 4
#define SPR_JELLY 102
#define SPR_JELLY_N 6
#define SPR_EEL 108
#define SPR_EEL_N 6
#define SPR_SPARK 114
#define SPR_SPARK_N 5
#define SPR_EYE 119
#define SPR_EYE_N 5
#define SPR_BOULDER 124
#define SPR_BOULDER_N 4
#define SPR_STALAG 128
#define SPR_STALAG_N 5
#define SPR_ROCK 133
#define SPR_ROCK_N 1
#define SPR_SKULL 134
#define SPR_SKULL_N 3
#define SPR_KNIGHT 137
#define SPR_KNIGHT_N 5
#define SPR_MEGA 142
#define SPR_MEGA_N 5
#define SPR_EPIC 147
#define SPR_EPIC_N 8
#define SPR_FLAG 155
#define SPR_FLAG_N 4
#define SPR_ELEV 159
#define SPR_ELEV_N 1
#define SPR_POLE 160
#define SPR_POLE_N 1
#define SPR_SWITCH 161
#define SPR_SWITCH_N 2
#define SPR_NEST 163
#define SPR_NEST_N 4
#define SPR_BEES 167
#define SPR_BEES_N 6
#define SPR_MIST 173
#define SPR_MIST_N 4
#define SPR_BUBBLE 177
#define SPR_BUBBLE_N 3
#define SPR_STAR 180
#define SPR_STAR_N 15
#define SPR_FROGP 195
#define SPR_FROGP_N 4
#define SPR_FISHP 199
#define SPR_FISHP_N 6

typedef struct { const unsigned char *p; unsigned char bank, xo, yo, cb; } SFrame;
typedef struct {
  unsigned char core_bank, obj_bank, bg_bank, hud_w, hud_y, hud_r, hud_g;
  unsigned char pool_tiles, bg_start, music;
  unsigned int n_bg, nobjs;
  const unsigned char *bgtiles, *mtflags, *map, *strings;
  const unsigned int *mtdef, *objs, *stroff, *sp_key, *sp_off, *sp_tile;
  unsigned char nspan, nmac;
  const unsigned int *mac;
  const unsigned int *pt_slot; const unsigned char *pt_tiles;
  unsigned char pt_bank;
  unsigned int pt_vram;
  unsigned char elev_mt;
  const SFrame *sframes;
  const unsigned char *bg_pal, *spr_pal;
  unsigned int spawn_x, spawn_y;
  const char *name;
} LevelDesc;
extern const LevelDesc level_descs[JILL_N_LEVELS];
extern const unsigned char font1bpp[]; /* in bank 5 */
#define FONT_BANK 5
#define JILL_BANK 2
extern const unsigned char jill_gfx[];
extern const unsigned char jill_slot[JILL_N_FRAMES];
typedef struct { unsigned char n; unsigned char l[8]; unsigned char r[8]; } Anim;
extern const Anim anims[];
#define ANIM_APPLE 0
#define ANIM_KEY 1
#define ANIM_ANT 2
#define ANIM_PHOENIX 3
#define ANIM_FROG 4
#define ANIM_SLUG 5
#define ANIM_CRAB 6
#define ANIM_GATOR 7
#define ANIM_SNAKE 8
#define ANIM_PAC 9
#define ANIM_JELLY 10
#define ANIM_EEL 11
#define ANIM_SPARK 12
#define ANIM_BOULDER 13
#define ANIM_SKULL 14
#define ANIM_KNIGHT 15
#define ANIM_EPIC 16
#define ANIM_FLAG 17
#define ANIM_BEES 18
#define ANIM_NEST 19
#define ANIM_MIST 20
#define ANIM_FISHP 21
#define ANIM_FROGP 22
#define ANIM_BLADE 23
#define ANIM_FIREBALL 24
#define ANIM_KNIFE 25
#define ANIM_STAR 26
#define ANIM_MEGA 27
extern const unsigned char jill_pal[16];
#define TEXT_BANK 26
extern const unsigned char game_texts[]; extern const unsigned int game_text_off[];
#define N_SONGS 9
#define N_SFX 23
#define SFX_01 0
#define SFX_02 1
#define SFX_03 2
#define SFX_04 3
#define SFX_05 4
#define SFX_06 5
#define SFX_08 6
#define SFX_10 7
#define SFX_11 8
#define SFX_12 9
#define SFX_15 10
#define SFX_16 11
#define SFX_18 12
#define SFX_19 13
#define SFX_23 14
#define SFX_24 15
#define SFX_25 16
#define SFX_28 17
#define SFX_33 18
#define SFX_35 19
#define SFX_39 20
#define SFX_41 21
#define SFX_48 22
#define SONG_DAN 1
#define SONG_FUNKY 2
#define SONG_IT 3
#define SONG_JUPITER 4
#define SONG_OMINOUS 5
#define SONG_SEVEN 6
#define SONG_SPIDERS 7
#define SONG_STEALTH 8
#define SONG_ZEPPELIN 9
typedef struct { unsigned char bank; const unsigned char *data; } BlobRef;
extern const BlobRef song_table[N_SONGS + 1];
extern const BlobRef sfx_table[N_SFX + 1];
extern const unsigned char song_dan[];
extern const unsigned char song_funky[];
extern const unsigned char song_it[];
extern const unsigned char song_jupiter[];
extern const unsigned char song_ominous[];
extern const unsigned char song_seven[];
extern const unsigned char song_spiders[];
extern const unsigned char song_stealth[];
extern const unsigned char song_zeppelin[];
extern const unsigned char sfx_01[];
extern const unsigned char sfx_02[];
extern const unsigned char sfx_03[];
extern const unsigned char sfx_04[];
extern const unsigned char sfx_05[];
extern const unsigned char sfx_06[];
extern const unsigned char sfx_08[];
extern const unsigned char sfx_10[];
extern const unsigned char sfx_11[];
extern const unsigned char sfx_12[];
extern const unsigned char sfx_15[];
extern const unsigned char sfx_16[];
extern const unsigned char sfx_18[];
extern const unsigned char sfx_19[];
extern const unsigned char sfx_23[];
extern const unsigned char sfx_24[];
extern const unsigned char sfx_25[];
extern const unsigned char sfx_28[];
extern const unsigned char sfx_33[];
extern const unsigned char sfx_35[];
extern const unsigned char sfx_39[];
extern const unsigned char sfx_41[];
extern const unsigned char sfx_48[];
extern const unsigned char lvl0_bgtiles[];
extern const unsigned char lvl0_mtflags[];
extern const unsigned char lvl0_map[];
extern const unsigned char lvl0_strings[];
extern const SFrame lvl0_sframes[];
extern const unsigned char lvl0_sfc0[];
extern const unsigned int lvl0_mtdef[];
extern const unsigned int lvl0_objs[];
extern const unsigned int lvl0_stroff[];
extern const unsigned int lvl0_spkey[];
extern const unsigned int lvl0_spoff[];
extern const unsigned int lvl0_sptile[];
extern const unsigned int lvl0_mac[];
extern const unsigned char lvl1_bgtiles[];
extern const unsigned char lvl1_mtflags[];
extern const unsigned char lvl1_map[];
extern const unsigned char lvl1_strings[];
extern const SFrame lvl1_sframes[];
extern const unsigned char lvl1_sfc0[];
extern const unsigned char lvl1_sfc1[];
extern const unsigned int lvl1_mtdef[];
extern const unsigned int lvl1_objs[];
extern const unsigned int lvl1_stroff[];
extern const unsigned int lvl1_spkey[];
extern const unsigned int lvl1_spoff[];
extern const unsigned int lvl1_sptile[];
extern const unsigned int lvl1_mac[];
extern const unsigned char lvl2_bgtiles[];
extern const unsigned char lvl2_mtflags[];
extern const unsigned char lvl2_map[];
extern const unsigned char lvl2_strings[];
extern const SFrame lvl2_sframes[];
extern const unsigned char lvl2_sfc0[];
extern const unsigned int lvl2_mtdef[];
extern const unsigned int lvl2_objs[];
extern const unsigned int lvl2_stroff[];
extern const unsigned int lvl2_spkey[];
extern const unsigned int lvl2_spoff[];
extern const unsigned int lvl2_sptile[];
extern const unsigned int lvl2_mac[];
extern const unsigned char lvl3_bgtiles[];
extern const unsigned char lvl3_mtflags[];
extern const unsigned char lvl3_map[];
extern const unsigned char lvl3_strings[];
extern const SFrame lvl3_sframes[];
extern const unsigned char lvl3_sfc0[];
extern const unsigned int lvl3_mtdef[];
extern const unsigned int lvl3_objs[];
extern const unsigned int lvl3_stroff[];
extern const unsigned int lvl3_spkey[];
extern const unsigned int lvl3_spoff[];
extern const unsigned int lvl3_sptile[];
extern const unsigned int lvl3_mac[];
extern const unsigned char lvl4_bgtiles[];
extern const unsigned char lvl4_mtflags[];
extern const unsigned char lvl4_map[];
extern const unsigned char lvl4_strings[];
extern const SFrame lvl4_sframes[];
extern const unsigned char lvl4_sfc0[];
extern const unsigned int lvl4_mtdef[];
extern const unsigned int lvl4_objs[];
extern const unsigned int lvl4_stroff[];
extern const unsigned int lvl4_spkey[];
extern const unsigned int lvl4_spoff[];
extern const unsigned int lvl4_sptile[];
extern const unsigned int lvl4_mac[];
extern const unsigned char lvl5_bgtiles[];
extern const unsigned char lvl5_mtflags[];
extern const unsigned char lvl5_map[];
extern const unsigned char lvl5_strings[];
extern const SFrame lvl5_sframes[];
extern const unsigned char lvl5_sfc0[];
extern const unsigned int lvl5_mtdef[];
extern const unsigned int lvl5_objs[];
extern const unsigned int lvl5_stroff[];
extern const unsigned int lvl5_spkey[];
extern const unsigned int lvl5_spoff[];
extern const unsigned int lvl5_sptile[];
extern const unsigned int lvl5_mac[];
extern const unsigned char lvl6_bgtiles[];
extern const unsigned char lvl6_mtflags[];
extern const unsigned char lvl6_map[];
extern const unsigned char lvl6_strings[];
extern const SFrame lvl6_sframes[];
extern const unsigned char lvl6_sfc0[];
extern const unsigned char lvl6_sfc1[];
extern const unsigned int lvl6_mtdef[];
extern const unsigned int lvl6_objs[];
extern const unsigned int lvl6_stroff[];
extern const unsigned int lvl6_spkey[];
extern const unsigned int lvl6_spoff[];
extern const unsigned int lvl6_sptile[];
extern const unsigned int lvl6_mac[];
extern const unsigned char lvl7_bgtiles[];
extern const unsigned char lvl7_mtflags[];
extern const unsigned char lvl7_map[];
extern const unsigned char lvl7_strings[];
extern const SFrame lvl7_sframes[];
extern const unsigned char lvl7_sfc0[];
extern const unsigned int lvl7_mtdef[];
extern const unsigned int lvl7_objs[];
extern const unsigned int lvl7_stroff[];
extern const unsigned int lvl7_spkey[];
extern const unsigned int lvl7_spoff[];
extern const unsigned int lvl7_sptile[];
extern const unsigned int lvl7_mac[];
extern const unsigned int lvl7_ptslot[]; extern const unsigned char lvl7_pt0[];
extern const unsigned char lvl8_bgtiles[];
extern const unsigned char lvl8_mtflags[];
extern const unsigned char lvl8_map[];
extern const unsigned char lvl8_strings[];
extern const SFrame lvl8_sframes[];
extern const unsigned char lvl8_sfc0[];
extern const unsigned int lvl8_mtdef[];
extern const unsigned int lvl8_objs[];
extern const unsigned int lvl8_stroff[];
extern const unsigned int lvl8_spkey[];
extern const unsigned int lvl8_spoff[];
extern const unsigned int lvl8_sptile[];
extern const unsigned int lvl8_mac[];
extern const unsigned char lvl9_bgtiles[];
extern const unsigned char lvl9_mtflags[];
extern const unsigned char lvl9_map[];
extern const unsigned char lvl9_strings[];
extern const SFrame lvl9_sframes[];
extern const unsigned char lvl9_sfc0[];
extern const unsigned int lvl9_mtdef[];
extern const unsigned int lvl9_objs[];
extern const unsigned int lvl9_stroff[];
extern const unsigned int lvl9_spkey[];
extern const unsigned int lvl9_spoff[];
extern const unsigned int lvl9_sptile[];
extern const unsigned int lvl9_mac[];
extern const unsigned char lvl10_bgtiles[];
extern const unsigned char lvl10_mtflags[];
extern const unsigned char lvl10_map[];
extern const unsigned char lvl10_strings[];
extern const SFrame lvl10_sframes[];
extern const unsigned char lvl10_sfc0[];
extern const unsigned int lvl10_mtdef[];
extern const unsigned int lvl10_objs[];
extern const unsigned int lvl10_stroff[];
extern const unsigned int lvl10_spkey[];
extern const unsigned int lvl10_spoff[];
extern const unsigned int lvl10_sptile[];
extern const unsigned int lvl10_mac[];

#endif
