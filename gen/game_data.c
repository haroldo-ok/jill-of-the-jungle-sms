#include "game_data.h"
const unsigned char pal_spr10[16]={0,6,22,27,31,21,8,4,63,53,23,42,17,29,55,18};
const unsigned char pal_bg10[16]={21,32,0,42,6,62,36,22,63,8,16,1,61,27,56,12};
const unsigned char pal_spr9[16]={0,6,22,27,31,21,8,4,63,54,57,42,17,18,3,2};
const unsigned char pal_bg9[16]={29,63,1,0,37,23,16,21,18,8,54,27,17,19,55,31};
const unsigned char pal_spr8[16]={0,6,22,27,31,21,8,4,63,3,54,18,12,42,17,2};
const unsigned char pal_bg8[16]={61,23,0,16,21,42,22,8,6,63,12,27,18,32,17,52};
const unsigned char pal_spr7[16]={0,6,22,27,31,21,8,4,63,42,54,23,45,17,3,18};
const unsigned char pal_bg7[16]={29,61,63,0,33,22,16,54,19,55,34,6,8,53,18,27};
const unsigned char pal_spr6[16]={0,6,22,27,31,21,8,4,63,53,42,23,60,55,17,41};
const unsigned char pal_bg6[16]={29,21,0,11,16,42,27,8,63,6,22,32,4,23,52,12};
const unsigned char pal_spr5[16]={0,6,22,27,31,21,8,4,63,53,42,23,56,45,17,49};
const unsigned char pal_bg5[16]={61,23,16,0,52,63,32,36,21,18,56,42,19,1,3,2};
const unsigned char pal_spr4[16]={0,6,22,27,31,21,8,4,63,32,23,42,17,53,48,29};
const unsigned char pal_bg4[16]={63,16,21,22,42,8,23,18,6,27,0,17,19,12,62,31};
const unsigned char pal_spr3[16]={0,6,22,27,31,21,8,4,63,52,42,19,56,18,49,55};
const unsigned char pal_bg3[16]={15,61,23,55,0,6,21,42,1,63,8,62,12,27,4,22};
const unsigned char pal_spr2[16]={0,6,22,27,31,21,8,4,63,23,17,42,54,45,18,1};
const unsigned char pal_bg2[16]={5,0,22,8,42,21,6,12,27,63,32,26,4,10,18,29};
const unsigned char pal_spr1[16]={0,6,22,27,31,21,8,4,63,53,23,18,12,42,45,17};
const unsigned char pal_bg1[16]={61,23,18,21,16,42,22,0,8,63,6,56,36,27,12,62};
const unsigned char pal_spr0[16]={0,6,22,27,31,21,8,4,63,54,23,42,45,18,3,57};
const unsigned char pal_bg0[16]={55,63,21,36,22,0,8,42,6,56,62,12,27,19,1,61};

const unsigned char jill_slot[JILL_N_FRAMES] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,255,255,22,23,24,255,255,255,255,255,25,26,27,255,28,29,255,255,30,31,32,255,33,34,255,255,35,36,37,38,39,255,255,255,40,41,42,255,43,44,255,255,45,46,47,48,49,255,255,255,255,255,255};
const unsigned char jill_pal[16] = {0,6,22,27,31,21,8,4,63,0,0,0,0,0,0,0};
const Anim anims[] = {
  {4,{0,1,2,3,0,0,0,0},{0,1,2,3,0,0,0,0}},
  {4,{0,1,2,3,0,0,0,0},{0,1,2,3,0,0,0,0}},
  {4,{0,1,2,3,0,0,0,0},{5,6,7,8,0,0,0,0}},
  {4,{0,1,2,3,0,0,0,0},{4,5,6,7,0,0,0,0}},
  {3,{0,1,2,0,0,0,0,0},{3,4,5,0,0,0,0,0}},
  {4,{0,1,2,1,0,0,0,0},{6,5,4,5,0,0,0,0}},
  {4,{0,1,2,3,0,0,0,0},{0,1,2,3,0,0,0,0}},
  {4,{12,13,14,15,0,0,0,0},{4,5,6,7,0,0,0,0}},
  {3,{0,1,2,0,0,0,0,0},{3,4,5,0,0,0,0,0}},
  {4,{0,1,2,3,0,0,0,0},{0,1,2,3,0,0,0,0}},
  {6,{0,1,2,3,4,5,0,0},{0,1,2,3,4,5,0,0}},
  {3,{0,1,2,0,0,0,0,0},{3,4,5,0,0,0,0,0}},
  {5,{0,1,2,3,4,0,0,0},{0,1,2,3,4,0,0,0}},
  {4,{0,1,2,3,0,0,0,0},{3,2,1,0,0,0,0,0}},
  {3,{0,1,2,0,0,0,0,0},{0,1,2,0,0,0,0,0}},
  {2,{0,1,0,0,0,0,0,0},{0,1,0,0,0,0,0,0}},
  {8,{0,1,2,3,4,5,6,7},{0,1,2,3,4,5,6,7}},
  {4,{0,1,2,3,0,0,0,0},{0,1,2,3,0,0,0,0}},
  {3,{0,1,2,0,0,0,0,0},{3,4,5,0,0,0,0,0}},
  {4,{0,1,2,3,0,0,0,0},{0,1,2,3,0,0,0,0}},
  {4,{0,1,2,3,0,0,0,0},{0,1,2,3,0,0,0,0}},
  {3,{0,1,2,0,0,0,0,0},{3,4,5,0,0,0,0,0}},
  {2,{2,3,0,0,0,0,0,0},{0,1,0,0,0,0,0,0}},
  {4,{0,1,2,3,0,0,0,0},{3,2,1,0,0,0,0,0}},
  {4,{0,1,2,3,0,0,0,0},{0,1,2,3,0,0,0,0}},
  {4,{1,2,3,4,0,0,0,0},{1,2,3,4,0,0,0,0}},
  {3,{0,1,2,0,0,0,0,0},{0,1,2,0,0,0,0,0}},
  {1,{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
};
const LevelDesc level_descs[JILL_N_LEVELS] = {
  {3,2,3,8,4,14,5,148,168,2,160,62,lvl0_bgtiles,lvl0_mtflags,lvl0_map,lvl0_strings,lvl0_mtdef,lvl0_objs,lvl0_stroff,lvl0_spkey,lvl0_spoff,lvl0_sptile,8,0,lvl0_mac,0,0,0,328,34,lvl0_sframes,pal_bg0,pal_spr0,96,40,"MAP"},
  {5,5,4,8,4,10,5,148,168,9,153,121,lvl1_bgtiles,lvl1_mtflags,lvl1_map,lvl1_strings,lvl1_mtdef,lvl1_objs,lvl1_stroff,lvl1_spkey,lvl1_spoff,lvl1_sptile,4,0,lvl1_mac,0,0,0,321,255,lvl1_sframes,pal_bg1,pal_spr1,112,160,"1"},
  {7,7,8,8,4,9,5,148,168,7,263,86,lvl2_bgtiles,lvl2_mtflags,lvl2_map,lvl2_strings,lvl2_mtdef,lvl2_objs,lvl2_stroff,lvl2_spkey,lvl2_spoff,lvl2_sptile,0,0,lvl2_mac,0,0,0,431,255,lvl2_sframes,pal_bg2,pal_spr2,64,928,"2"},
  {10,4,10,8,4,11,5,148,168,5,124,116,lvl3_bgtiles,lvl3_mtflags,lvl3_map,lvl3_strings,lvl3_mtdef,lvl3_objs,lvl3_stroff,lvl3_spkey,lvl3_spoff,lvl3_sptile,4,0,lvl3_mac,0,0,0,292,14,lvl3_sframes,pal_bg3,pal_spr3,1936,736,"3"},
  {12,7,12,8,4,10,5,148,168,1,95,71,lvl4_bgtiles,lvl4_mtflags,lvl4_map,lvl4_strings,lvl4_mtdef,lvl4_objs,lvl4_stroff,lvl4_spkey,lvl4_spoff,lvl4_sptile,6,0,lvl4_mac,0,0,0,263,255,lvl4_sframes,pal_bg4,pal_spr4,1056,928,"4"},
  {14,14,13,8,4,11,5,148,168,2,111,119,lvl5_bgtiles,lvl5_mtflags,lvl5_map,lvl5_strings,lvl5_mtdef,lvl5_objs,lvl5_stroff,lvl5_spkey,lvl5_spoff,lvl5_sptile,4,0,lvl5_mac,0,0,0,279,28,lvl5_sframes,pal_bg5,pal_spr5,704,496,"6"},
  {16,15,8,8,4,11,5,148,168,4,229,109,lvl6_bgtiles,lvl6_mtflags,lvl6_map,lvl6_strings,lvl6_mtdef,lvl6_objs,lvl6_stroff,lvl6_spkey,lvl6_spoff,lvl6_sptile,2,0,lvl6_mac,0,0,0,397,55,lvl6_sframes,pal_bg6,pal_spr6,32,224,"9"},
  {18,15,9,8,4,14,5,88,108,6,77,20,lvl7_bgtiles,lvl7_mtflags,lvl7_map,lvl7_strings,lvl7_mtdef,lvl7_objs,lvl7_stroff,lvl7_spkey,lvl7_spoff,lvl7_sptile,65,138,lvl7_mac,lvl7_ptslot,lvl7_pt0,19,185,255,lvl7_sframes,pal_bg7,pal_spr7,96,912,"50"},
  {21,20,21,8,4,9,5,148,168,8,127,72,lvl8_bgtiles,lvl8_mtflags,lvl8_map,lvl8_strings,lvl8_mtdef,lvl8_objs,lvl8_stroff,lvl8_spkey,lvl8_spoff,lvl8_sptile,4,0,lvl8_mac,0,0,0,295,255,lvl8_sframes,pal_bg8,pal_spr8,928,928,"D0"},
  {23,22,23,8,4,14,5,148,168,1,114,76,lvl9_bgtiles,lvl9_mtflags,lvl9_map,lvl9_strings,lvl9_mtdef,lvl9_objs,lvl9_stroff,lvl9_spkey,lvl9_spoff,lvl9_sptile,7,0,lvl9_mac,0,0,0,282,20,lvl9_sframes,pal_bg9,pal_spr9,1600,944,"D1"},
  {24,5,23,8,4,10,5,148,168,8,66,34,lvl10_bgtiles,lvl10_mtflags,lvl10_map,lvl10_strings,lvl10_mtdef,lvl10_objs,lvl10_stroff,lvl10_spkey,lvl10_spoff,lvl10_sptile,0,0,lvl10_mac,0,0,0,234,255,lvl10_sframes,pal_bg10,pal_spr10,192,912,"D2"},
};
const BlobRef song_table[N_SONGS + 1] = {
  {4,song_dan},
  {24,song_funky},
  {12,song_it},
  {14,song_jupiter},
  {25,song_ominous},
  {25,song_seven},
  {26,song_spiders},
  {26,song_stealth},
  {18,song_zeppelin},
  {0,0}};
const BlobRef sfx_table[N_SFX + 1] = {
  {2,sfx_01},
  {2,sfx_02},
  {2,sfx_03},
  {2,sfx_04},
  {2,sfx_05},
  {2,sfx_06},
  {2,sfx_08},
  {2,sfx_10},
  {2,sfx_11},
  {2,sfx_12},
  {2,sfx_15},
  {2,sfx_16},
  {2,sfx_18},
  {2,sfx_19},
  {2,sfx_23},
  {2,sfx_24},
  {2,sfx_25},
  {2,sfx_28},
  {2,sfx_33},
  {2,sfx_35},
  {2,sfx_39},
  {2,sfx_41},
  {2,sfx_48},
  {0,0}};
