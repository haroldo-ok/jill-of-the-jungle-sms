/* ------------------------------------------------------------------------
 * PSG music (converted from the original CMF files) and sound effects
 * (converted from the original digitised VCL samples).
 *
 * Music uses tone 0..2 and the noise channel.  A sound effect temporarily
 * takes over tone 2 and the noise channel.
 * ---------------------------------------------------------------------- */

u8 mus_idx, mus_bank, mus_wait;
static const u8 *mus_ptr, *mus_start;
u16 t_per[3];
u8 t_att[3];
u8 n_ctl, n_att, n_rate, n_cnt, n_on, n_hit;

u8 sfx_on, sfx_bank;
static const u8 *sfx_ptr;
u16 sfx_per;
u8 sfx_att, sfx_noise, sfx_ctl;

u16 w_per[3];
u8 w_att[4];
u8 w_nctl;
u8 audio_enabled = 1;

static void psg_att(u8 ch, u8 att) {
    if (w_att[ch] == att) return;
    w_att[ch] = att;
    PSGPort = (u8)(0x90 | (ch << 5) | att);
}
/* constant-channel versions for the interrupt-time tick (no call, constant addresses) */
#define PSG_ATT(ch, att) do { u8 _a = (att); if (w_att[ch] != _a) { w_att[ch] = _a; PSGPort = (u8)(0x90 | ((ch) << 5) | _a); } } while (0)
#define PSG_PER(ch, per) do { u16 _p = (per); if (w_per[ch] != _p) { w_per[ch] = _p; \
        PSGPort = (u8)(0x80 | ((ch) << 5) | ((u8)_p & 15)); PSGPort = (u8)(_p >> 4); } } while (0)
static void psg_noise(u8 ctl) {
    PSGPort = (u8)(0xE0 | ctl);
    w_nctl = ctl;
}

static void audio_reset(void) {
    u8 i;
    for (i = 0; i < 4; i++) { w_att[i] = 0xFF; }
    for (i = 0; i < 3; i++) { w_per[i] = 0xFFFF; t_att[i] = 15; t_per[i] = 0; }
    for (i = 0; i < 4; i++) psg_att(i, 15);
    mus_ptr = 0; mus_idx = 0; n_on = 0; sfx_on = 0;
}

static void music_play_i(u8 idx) {
    u8 i;
    if (idx == mus_idx) return;
    mus_idx = idx;
    n_on = 0;
    for (i = 0; i < 3; i++) t_att[i] = 15;
    if (!idx || !audio_enabled) { mus_ptr = 0; return; }
    mus_bank = song_table[idx - 1].bank;
    mus_ptr = mus_start = song_table[idx - 1].data;
    mus_wait = 0;
}

static void sfx_play_i(u8 id) {
    if (!audio_enabled) return;
    sfx_bank = sfx_table[id].bank;
    sfx_ptr = sfx_table[id].data;
    sfx_on = 1;
}

/* the tick runs from the interrupt: mutate its state with interrupts off
   (SDCC's __critical clobbers the register argument, so DI/EI by hand) */
static void music_play(u8 idx) { __asm__("di"); music_play_i(idx); __asm__("ei"); }
static void sfx_play(u8 id) { __asm__("di"); sfx_play_i(id); __asm__("ei"); }


static void music_step(void) {
    const u8 *p;
    u8 b, ch;
    if (mus_wait) { mus_wait--; return; }
    BANK(mus_bank);
    p = mus_ptr;
    for (;;) {
        b = *p++;
        if (b >= 0xF0) {
            if (b == 0xFF) { p = mus_start; continue; }
            if (b == 0xFD) { mus_wait = (u8)(*p++ - 1); break; }
            mus_wait = (u8)(b - 0xF0);
            break;
        } else if (b >= 0xC0) {
            t_att[(b >> 4) & 3] = b & 15;
        } else if (b >= 0xB0) {
            if (b == 0xBF) { n_on = 0; }
            else {
                n_ctl = b & 7;
                n_att = (u8)(*p >> 4);
                n_rate = (u8)(*p & 15);
                p++;
                n_cnt = 0; n_on = 1; n_hit = 1;
            }
        } else {
            ch = (b >> 4) & 3;
            t_att[ch] = b & 15;
            t_per[ch] = (u16)p[0] | ((u16)p[1] << 8);
            p += 2;
        }
    }
    mus_ptr = p;
}

static void sfx_step(void) {
    u8 h;
    BANK(sfx_bank);
    h = *sfx_ptr++;
    if (h == 0xFF) {
        sfx_on = 0;
        w_att[2] = 0xFF; w_att[3] = 0xFF; w_per[2] = 0xFFFF;   /* force music to re-take the voices */
        return;
    }
    sfx_att = h >> 4;
    if (h & 8) { sfx_noise = 1; sfx_ctl = h & 7; }
    else { sfx_noise = 0; sfx_per = ((u16)(h & 7) << 8) | *sfx_ptr++; }
}

/* runs from the frame (vblank) interrupt, so tempo is independent of the game's frame rate */
static void audio_tick(void) {
    u8 sb = cur_bank;
    u8 a;
    if (!audio_enabled) return;
    if (mus_ptr) music_step();
    if (sfx_on) sfx_step();
    PSG_PER(0, t_per[0]); PSG_ATT(0, t_att[0]);
    PSG_PER(1, t_per[1]); PSG_ATT(1, t_att[1]);
    if (sfx_on) {
        if (sfx_noise) {
            PSG_ATT(2, 15);
            if (w_nctl != sfx_ctl || w_att[3] == 15) psg_noise(sfx_ctl);
            PSG_ATT(3, sfx_att);
        } else {
            PSG_ATT(3, 15);
            PSG_PER(2, sfx_per); PSG_ATT(2, sfx_att);
        }
    } else {
        PSG_PER(2, t_per[2]); PSG_ATT(2, t_att[2]);
        if (n_on) {
            if (n_hit) { psg_noise(n_ctl); n_hit = 0; }
            a = n_att;
            PSG_ATT(3, a);
            if (++n_cnt >= n_rate) { n_cnt = 0; if (++n_att >= 15) { n_on = 0; } }
        } else PSG_ATT(3, 15);
    }
    BANK(sb);
}
