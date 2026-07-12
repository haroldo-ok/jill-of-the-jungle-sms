#ifndef JILL_GAME_DATA_H
#define JILL_GAME_DATA_H

#include "SMSlib.h"

#define JILL_N_LEVELS 9
#define JILL_BANK_SPRITES 2
#define JILL_N_PLAYER_FRAMES 50

#define JF_SOLID  0x01
#define JF_PLAT   0x02
#define JF_VINE   0x04
#define JF_WATER  0x08
#define JF_TOUCH  0x10
#define JF_DEADLY 0x20

typedef struct {
  unsigned char tile_bank;
  unsigned char data_bank;
  const unsigned char *tiles;
  unsigned int tiles_size;
  const unsigned int *mtdef;
  const unsigned char *flags;
  const unsigned char *map;
  const unsigned int *trans;
  unsigned char ntrans;
  const unsigned int *items;
  unsigned char nitems;
  const unsigned int *doors;
  unsigned char ndoors;
  const unsigned int *enemies;
  unsigned char nenemies;
  unsigned int spawn_x;
  unsigned int spawn_y;
  const char *name;
} LevelDesc;

extern const unsigned char bg_palette[16];
extern const unsigned char spr_palette[16];
extern const LevelDesc level_descs[JILL_N_LEVELS];
extern const unsigned char spr_jill[];
extern const unsigned char spr_items[];
extern const unsigned char spr_enemies[];
extern const unsigned char lvl0_map_tiles[];
extern const unsigned int lvl0_map_mtdef[];
extern const unsigned char lvl0_map_flags[];
extern const unsigned char lvl0_map_map[];
extern const unsigned int lvl0_map_trans[];
extern const unsigned int lvl0_map_items[];
extern const unsigned int lvl0_map_doors[];
extern const unsigned int lvl0_map_enemies[];
extern const unsigned char lvl1_1_tiles[];
extern const unsigned int lvl1_1_mtdef[];
extern const unsigned char lvl1_1_flags[];
extern const unsigned char lvl1_1_map[];
extern const unsigned int lvl1_1_trans[];
extern const unsigned int lvl1_1_items[];
extern const unsigned int lvl1_1_doors[];
extern const unsigned int lvl1_1_enemies[];
extern const unsigned char lvl2_2_tiles[];
extern const unsigned int lvl2_2_mtdef[];
extern const unsigned char lvl2_2_flags[];
extern const unsigned char lvl2_2_map[];
extern const unsigned int lvl2_2_trans[];
extern const unsigned int lvl2_2_items[];
extern const unsigned int lvl2_2_doors[];
extern const unsigned int lvl2_2_enemies[];
extern const unsigned char lvl3_3_tiles[];
extern const unsigned int lvl3_3_mtdef[];
extern const unsigned char lvl3_3_flags[];
extern const unsigned char lvl3_3_map[];
extern const unsigned int lvl3_3_trans[];
extern const unsigned int lvl3_3_items[];
extern const unsigned int lvl3_3_doors[];
extern const unsigned int lvl3_3_enemies[];
extern const unsigned char lvl4_4_tiles[];
extern const unsigned int lvl4_4_mtdef[];
extern const unsigned char lvl4_4_flags[];
extern const unsigned char lvl4_4_map[];
extern const unsigned int lvl4_4_trans[];
extern const unsigned int lvl4_4_items[];
extern const unsigned int lvl4_4_doors[];
extern const unsigned int lvl4_4_enemies[];
extern const unsigned char lvl5_6_tiles[];
extern const unsigned int lvl5_6_mtdef[];
extern const unsigned char lvl5_6_flags[];
extern const unsigned char lvl5_6_map[];
extern const unsigned int lvl5_6_trans[];
extern const unsigned int lvl5_6_items[];
extern const unsigned int lvl5_6_doors[];
extern const unsigned int lvl5_6_enemies[];
extern const unsigned char lvl6_9_tiles[];
extern const unsigned int lvl6_9_mtdef[];
extern const unsigned char lvl6_9_flags[];
extern const unsigned char lvl6_9_map[];
extern const unsigned int lvl6_9_trans[];
extern const unsigned int lvl6_9_items[];
extern const unsigned int lvl6_9_doors[];
extern const unsigned int lvl6_9_enemies[];
extern const unsigned char lvl7_50_tiles[];
extern const unsigned int lvl7_50_mtdef[];
extern const unsigned char lvl7_50_flags[];
extern const unsigned char lvl7_50_map[];
extern const unsigned int lvl7_50_trans[];
extern const unsigned int lvl7_50_items[];
extern const unsigned int lvl7_50_doors[];
extern const unsigned int lvl7_50_enemies[];
extern const unsigned char lvl8_intro_tiles[];
extern const unsigned int lvl8_intro_mtdef[];
extern const unsigned char lvl8_intro_flags[];
extern const unsigned char lvl8_intro_map[];
extern const unsigned int lvl8_intro_trans[];
extern const unsigned int lvl8_intro_items[];
extern const unsigned int lvl8_intro_doors[];
extern const unsigned int lvl8_intro_enemies[];

#endif
