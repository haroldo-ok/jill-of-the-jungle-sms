#include "game_data.h"
const unsigned char bg_palette[] = {
  0x00,0x20,0x08,0x28,0x02,0x22,0x06,0x2A,0x15,0x35,0x1D,0x3D,0x17,0x37,0x1F,0x3F,
};
const unsigned char spr_palette[] = {
  0x00,0x20,0x08,0x28,0x02,0x22,0x06,0x2A,0x15,0x35,0x1D,0x3D,0x17,0x37,0x1F,0x3F,
};
const LevelDesc level_descs[JILL_N_LEVELS] = {
  {3,4,lvl0_map_tiles,6528,lvl0_map_mtdef,lvl0_map_flags,lvl0_map_map,lvl0_map_trans,13,lvl0_map_items,11,lvl0_map_doors,9,lvl0_map_enemies,0,96,40,"MAP.JN1"},
  {5,6,lvl1_1_tiles,7168,lvl1_1_mtdef,lvl1_1_flags,lvl1_1_map,lvl1_1_trans,2,lvl1_1_items,52,lvl1_1_doors,1,lvl1_1_enemies,41,112,160,"1.JN1"},
  {7,8,lvl2_2_tiles,11136,lvl2_2_mtdef,lvl2_2_flags,lvl2_2_map,lvl2_2_trans,2,lvl2_2_items,29,lvl2_2_doors,2,lvl2_2_enemies,37,64,928,"2.JN1"},
  {9,10,lvl3_3_tiles,4224,lvl3_3_mtdef,lvl3_3_flags,lvl3_3_map,lvl3_3_trans,2,lvl3_3_items,56,lvl3_3_doors,4,lvl3_3_enemies,28,1936,736,"3.JN1"},
  {11,12,lvl4_4_tiles,3456,lvl4_4_mtdef,lvl4_4_flags,lvl4_4_map,lvl4_4_trans,2,lvl4_4_items,20,lvl4_4_doors,6,lvl4_4_enemies,4,1056,928,"4.JN1"},
  {13,14,lvl5_6_tiles,3840,lvl5_6_mtdef,lvl5_6_flags,lvl5_6_map,lvl5_6_trans,3,lvl5_6_items,34,lvl5_6_doors,10,lvl5_6_enemies,25,704,496,"6.JN1"},
  {15,16,lvl6_9_tiles,9856,lvl6_9_mtdef,lvl6_9_flags,lvl6_9_map,lvl6_9_trans,2,lvl6_9_items,42,lvl6_9_doors,9,lvl6_9_enemies,31,32,224,"9.JN1"},
  {17,18,lvl7_50_tiles,2944,lvl7_50_mtdef,lvl7_50_flags,lvl7_50_map,lvl7_50_trans,0,lvl7_50_items,3,lvl7_50_doors,0,lvl7_50_enemies,0,96,912,"50.JN1"},
  {19,20,lvl8_intro_tiles,6784,lvl8_intro_mtdef,lvl8_intro_flags,lvl8_intro_map,lvl8_intro_trans,0,lvl8_intro_items,13,lvl8_intro_doors,0,lvl8_intro_enemies,0,1960,944,"INTRO.JN1"},
};
