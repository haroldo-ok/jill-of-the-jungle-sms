/* ------------------------------------------------------------------------
 * Game flow: level loading, frame loop, transitions
 * ---------------------------------------------------------------------- */

u8 snap_health, snap_keys, snap_gems, snap_knives, snap_hijump;
u16 snap_slo, snap_shi;
u8 game_over_req, end_req;
u8 turtle, turtle_latch;
u16 held_prev;           /* turtle mode: half-speed game */
u8 mac_done, mac_i, mac_sub;
u16 mac_t, mac_mask, mac_prev;

static void take_snapshot(void) {
    snap_health = health; snap_keys = keys; snap_gems = gems; snap_knives = knives; snap_hijump = hijump;
    snap_slo = score_lo; snap_shi = score_hi;
}
static void restore_snapshot(void) {
    health = snap_health; keys = snap_keys; gems = snap_gems; knives = snap_knives; hijump = snap_hijump;
    score_lo = snap_slo; score_hi = snap_shi;
}

static void new_game_state(void) {
    health = HEALTH_MAX; keys = 0; gems = 0; knives = 0; hijump = 0; blades = 0;
    score_lo = 0; score_hi = 0;
    hub_valid = 0;
    msg_shown = 0;
    take_snapshot();
}

static void level_gfx_setup(void) {
    BANK(ld->bg_bank);
    SMS_loadTiles(ld->bgtiles, ld->bg_start, ld->n_bg * 32u);
    BANK(ld->core_bank);
    SMS_VRAMmemset(BLANK_TILE * 32u, 0, 32);
    fc_reset();
}

/* place the player and load everything; display is left OFF */
static void level_load(u8 idx, u8 entry, u8 use_ret) {
    u8 i, mus;
    SMS_displayOff();
    SMS_initSprites(); hud_spr_ok = 0;
    SMS_copySpritestoSAT();
    ld = &level_descs[idx];
    cur_map = ld->map; cur_mtflags = ld->mtflags; cur_sframes = ld->sframes;
    cur_level = idx;
    cur_entry = entry;
    pal_bg = ld->bg_pal;
    pal_spr = ld->spr_pal;
    pal_set(0);
    BANK(ld->core_bank);
    level_gfx_setup();
    n_ov = 0; rf_n = 0;
    level_objects_init();
    if (idx == 0) hub_restore();
    for (i = 0; i < PRN; i++) { if ((pr_t[i] == PR_KNIFE || pr_t[i] == PR_KNIFEB) && knives < 9 && !dead_t) knives++; pr_t[i] = 0; }
    for (i = 0; i < EFN; i++) ef_t[i] = 0;
    hud_bg = 0;
    hud_c[0] = ld->hud_w; hud_c[1] = ld->hud_y; hud_c[2] = ld->hud_r; hud_c[3] = ld->hud_g;
    hud_dirty = 1;
    /* player placement */
    set_form(FORM_HUMAN);
    px = (int)ld->spawn_x; py = (int)ld->spawn_y + 8;
    mus = ld->music;
    if (use_ret) { px = ret_px; py = ret_py; }
    else if (entry) {
        for (i = 0; i < num_obj; i++)
            if (ot[i] == T_CHECK && otag[i] == entry && (otgt[i] == 0xFFFF || otgt[i] >= 0x100)) {
                px = ox[i]; py = oy[i] - 8;
                if (otgt[i] >= 0x100 && otgt[i] < 0x200) mus = (u8)(otgt[i] - 0x100 + 1);
                break;
            }
    }
    vx = vy = 0; fxr = fyr = 0;
    on_ground = 0; climbing = 0; in_water = 0; face_right = 1;
    hurt_t = 0; dead_t = 0; blade_t = 0; fire_cd = 0; crouch = 0;
    cp_lock = 1; req_level = 0xFF; level_end_req = 0; macro_req = 0;
    mac_on = mac_done = 0; slides_req = 0;
    if (ld->nmac) macro_req = 1;           /* scripted ending starts with the level */
    frame_ctr = 0xFF;                       /* the culling sequence starts on the first frame */
    camera_snap();
    near_n = 0;                             /* active / draw lists valid from the first frame */
    cull_build((u8)((cam_x >> 4) - 6), (u8)((cam_y >> 4) - 6));
    draw_filter((u8)((cam_x >> 4) - 5), (u8)((cam_y >> 4) - 4));
    nt_tx = (u8)(cam_x >> 3); nt_ty = (u8)(cam_y >> 3);
    draw_all();
    music_play(mus);
}

static void screen_on(void) {
    apply_scroll();
    SMS_displayOn();
}

static void do_level(u8 idx, u8 entry, u8 use_ret) {
    fade_to(3, 0);
    SMS_displayOff();
    if (cur_level == 0 && idx != 0) hub_save();
    level_load(idx, entry, use_ret);
    screen_on();
    fade_to(0, 3);
}

/* build + push one video frame */
static void present(void) {
    stream_build();
    SMS_waitForVBlank();
    SMS_copySpritestoSAT();
    apply_scroll();
    stream_write();
    apply_refresh();
    pt_flush();
    {
        u8 bud = 8;
        if (jill_want != jill_cur && jill_want != 0xFF) { jill_upload(); bud = 4; }
        fc_upload(bud);
        if (hud_dirty == 2) { SMS_loadTiles(hud_tiles, T_HUD, HUD_TILES * 32); hud_dirty = 0; }
    }
}

static u8 jill_pose(void) {
    u8 f, a;
    if (dead_t) {
        if (dead_t < 40) return 48 + (dead_t / 10);
        return 72 + ((dead_t >> 4) % 3 == 0 ? 0 : 1);
    }
    if (hurt_t > 50) return face_right ? 61 : 60;
    if (climbing) { a = (anim_t >> 3) & 3; return 24 + (a == 3 ? 1 : a); }
    if (in_water) return 56 + ((anim_t >> 3) % 3);
    if (!on_ground) {
        if (vy < -200) f = 0; else if (vy < 200) f = 1; else f = 2;
        return (face_right ? 32 : 40) + f;
    }
    if (crouch) return face_right ? 37 : 45;
    if (vx > 40 || vx < -40) return (face_right ? JILL_RUN_R : JILL_RUN_L) + ((anim_t >> 2) & 3);
    return 16 + ((frame_ctr >> 5) & 3 ? 0 : 0);
}

static void draw_player(void) {
    int sx = px - (int)cam_x, sy = py - (int)cam_y;
    u8 f;
    if (hurt_t && !dead_t && (hurt_t & 2) && hurt_t < 52) return;      /* blink while invulnerable */
    switch (form) {
    case FORM_FROG:
        f = (vy < -100 || vy > 100) ? 1 : 0;
        draw_spr(SPR_FROGP + (face_right ? 0 : 2) + f, sx - 1, sy - 0);
        break;
    case FORM_PHOENIX:
        f = (anim_t >> 2) & 3;
        draw_spr(SPR_PHOENIX + (face_right ? 4 : 0) + f, sx, sy);
        break;
    case FORM_FISH:
        f = (anim_t >> 3) % 3;
        draw_spr(SPR_FISHP + (face_right ? 3 : 0) + f, sx, sy - 2);
        break;
    default:
        jill_want = jill_pose();
        draw_jill(sx, sy - 8);
        break;
    }
}

static void draw_blades(void) {
    static const s8 ox_t[8] = { 22, 16, 0, -16, -22, -16, 0, 16 };
    static const s8 oy_t[8] = { 0, 12, 18, 12, 0, -12, -18, -12 };
    u8 k, ph8, j;
    int cx, cy, bx, by;
    if (!blade_t) return;
    cx = px + (pw >> 1) - 8 - (int)cam_x; cy = py + (ph >> 1) - 8 - (int)cam_y;
    for (k = 0; k < 2; k++) {
        ph8 = (u8)(((frame_ctr >> 1) + (k << 2)) & 7);
        bx = cx + ox_t[ph8]; by = cy + oy_t[ph8];
        draw_spr(SPR_BLADE + ((frame_ctr >> 1) & 3), bx, by);
        bx += (int)cam_x + 8; by += (int)cam_y + 8;
        for (j = 0; j < num_obj; j++) {
            if (ot[j] == T_NONE) continue;
            if (bx > ox[j] - 4 && bx < ox[j] + ow[j] + 4 && by > oy[j] - 4 && by < oy[j] + oh[j] + 4) killable_dmg(j, 2);
        }
    }
}

static void draw_misc(void) {
    u8 i;
    for (i = 0; i < PRN; i++) {
        if (!pr_t[i]) continue;
        switch (pr_t[i]) {
        case PR_KNIFE: case PR_KNIFEB:
            draw_spr(SPR_KNIFE + (((pr_fr[i] >> 1) & 3) + 1), pr_x[i] - (int)cam_x - 1, pr_y[i] - (int)cam_y - 1);
            break;
        case PR_FIRE:
            draw_spr(SPR_FIREBALL + ((pr_fr[i] >> 1) & 3), pr_x[i] - (int)cam_x - 4, pr_y[i] - (int)cam_y);
            break;
        default:
            draw_spr(SPR_FISHSHOT + ((pr_fr[i] >> 2) & 1) + 0, pr_x[i] - (int)cam_x, pr_y[i] - (int)cam_y);
            break;
        }
    }
    for (i = 0; i < EFN; i++) {
        if (!ef_t[i]) continue;
        draw_spr(SPR_STAR + ((18 - ef_t[i]) / 6), ef_x[i] - (int)cam_x, ef_y[i] - (int)cam_y);
        ef_t[i]--;
    }
}

/* one game frame (logic + sprite list); returns nothing, present() pushes it */
static void game_logic(void) {
    fctr++;
    frame_ctr++;
    if (SMS_queryPauseRequested()) { SMS_resetPauseRequest(); want_text = 21; }
    held = SMS_getKeysHeld();
    pressed = (held & ~held_prev) | SMS_getKeysPressed() | turtle_latch;   /* edges survive slow frames */
    held_prev = held;
    turtle_latch = 0;
    if (macro_req) {
        macro_req = 0;
        if (!mac_on && !mac_done && ld->nmac) { mac_on = 1; mac_t = mac_i = mac_sub = 0; mac_mask = mac_prev = 0; }
    }
    if (mac_on) {                       /* scripted input (ending): 2 macro ticks per 3 frames */
        mac_sub += 2;
        if (mac_sub >= 3) { mac_sub -= 3; mac_t++; }
        while (mac_i < ld->nmac && ld->mac[mac_i << 1] <= mac_t) { mac_mask = ld->mac[(mac_i << 1) + 1]; mac_i++; }
        held = mac_mask;
        pressed = mac_mask & ~mac_prev;
        mac_prev = mac_mask;
        if (mac_i >= ld->nmac) { mac_on = 0; mac_done = 1; slides_req = 1; }
    }
    if (dead_t) {
        dead_t++;
        if (dead_t > 150) game_over_req = 1;
    } else {
        player_update();
    }
    if (!dead_t) anim_t = anim_t;
    proj_update();
    cp_hit = 0;
    switch (frame_ctr & 7) {  /* culling on coarse cell positions, spread over 8 frames (wide margins) */
    case 0: cells_refresh(); break;       /* only active objects move */
    case 1: cull_build((u8)((cam_x >> 4) - 6), (u8)((cam_y >> 4) - 6)); break;
    case 2: case 6: cells_refresh(); draw_filter((u8)((cam_x >> 4) - 5), (u8)((cam_y >> 4) - 4)); break;
    }
    obj_update_all_t();
    if (!cp_hit) cp_lock = 0;
    camera_follow();
    if (hud_dirty == 1) { hud_render(); hud_dirty = 2; }

    if (hud_spr_ok) SpriteNextFree = 6;    /* the HUD entries 0..5 are still in the table */
    else { SMS_initSprites(); hud_draw(); hud_spr_ok = 1; }
    draw_objs(0, draw_ne);                     /* lift controls show in front of Jill's feet */
    draw_player();
    draw_blades();
    draw_objs(draw_ne, draw_n - draw_ne);
    draw_misc();
}

static void restart_level(void) {
    u8 lv = cur_level, en = cur_entry;
    fade_to(3, 0);
    restore_snapshot();
    if (health == 0) health = HEALTH_MAX;
    SMS_displayOff();
    level_load(lv, en, 0);
    screen_on();
    fade_to(0, 3);
    game_over_req = 0;
}

/* ---- battery save (cartridge SRAM): the world-map state + stats ---------------- */
#define SAVE_SIZE (8 + HUB_MAXO * 3 + MAX_OV * 3 + 2)

static u8 save_valid(void) {
    u8 ok = 0;
    u16 i, sum = 0, got;
    __asm__("di"); SMS_enableSRAM();      /* the audio IRQ reads ROM through slot 2 */
    if (SMS_SRAM[0] == 'J' && SMS_SRAM[1] == 'L' && SMS_SRAM[2] == 2) {
        for (i = 0; i < SAVE_SIZE - 2; i++) sum += SMS_SRAM[i];
        got = (u16)(SMS_SRAM[SAVE_SIZE - 2] | ((u16)SMS_SRAM[SAVE_SIZE - 1] << 8));
        ok = (got == sum);
    }
    SMS_disableSRAM(); __asm__("ei");
    return ok;
}

static void save_game(void) {
    u16 i, o = 3, sum = 0;
    u8 *d;
    if (cur_level == 0) { hub_save(); ret_px = px; ret_py = py; }
    __asm__("di"); SMS_enableSRAM();      /* the audio IRQ reads ROM through slot 2 */
    d = SMS_SRAM;
    d[0] = 'J'; d[1] = 'L'; d[2] = 2;
    d[o++] = (u8)health; d[o++] = keys; d[o++] = gems; d[o++] = knives; d[o++] = hijump; d[o++] = msg_shown;
    d[o++] = (u8)score_lo; d[o++] = (u8)(score_lo >> 8); d[o++] = (u8)score_hi; d[o++] = (u8)(score_hi >> 8);
    d[o++] = (u8)ret_px; d[o++] = (u8)((u16)ret_px >> 8); d[o++] = (u8)ret_py; d[o++] = (u8)((u16)ret_py >> 8);
    d[o++] = hub_n_ov;
    for (i = 0; i < HUB_MAXO; i++) { d[o++] = hub_ot[i]; d[o++] = hub_ost[i]; d[o++] = hub_oct[i]; }
    for (i = 0; i < MAX_OV; i++) { d[o++] = (u8)hub_ov_cell[i]; d[o++] = (u8)(hub_ov_cell[i] >> 8); d[o++] = hub_ov_mt[i]; }
    for (i = o; i < SAVE_SIZE - 2; i++) d[i] = 0;
    for (i = 0; i < SAVE_SIZE - 2; i++) sum += d[i];
    d[SAVE_SIZE - 2] = (u8)sum; d[SAVE_SIZE - 1] = (u8)(sum >> 8);
    SMS_disableSRAM(); __asm__("ei");
}

/* restores the saved world; the caller then loads level 0 at ret_px/ret_py */
static void load_game(void) {
    u16 i, o = 3;
    u8 *d;
    new_game_state();
    __asm__("di"); SMS_enableSRAM();      /* the audio IRQ reads ROM through slot 2 */
    d = SMS_SRAM;
    health = d[o++]; keys = d[o++]; gems = d[o++]; knives = d[o++]; hijump = d[o++]; msg_shown = d[o++];
    score_lo = (u16)(d[o] | ((u16)d[o + 1] << 8)); o += 2;
    score_hi = (u16)(d[o] | ((u16)d[o + 1] << 8)); o += 2;
    ret_px = (int)(d[o] | ((u16)d[o + 1] << 8)); o += 2;
    ret_py = (int)(d[o] | ((u16)d[o + 1] << 8)); o += 2;
    hub_n_ov = d[o++];
    for (i = 0; i < HUB_MAXO; i++) { hub_ot[i] = d[o++]; hub_ost[i] = d[o++]; hub_oct[i] = d[o++]; }
    for (i = 0; i < MAX_OV; i++) { hub_ov_cell[i] = (u16)(d[o] | ((u16)d[o + 1] << 8)); hub_ov_mt[i] = d[o + 2]; o += 3; }
    SMS_disableSRAM(); __asm__("ei");
    hub_valid = 1;
    take_snapshot();
}

/* hint / help page over the running level, then restore the level video state */
static void show_hint(u8 idx) {
    fade_to(3, 0);
    if (idx == 21) { tp_menu_row = 3; tp_menu_n = 5; tp_sel = 0; }
    text_page(idx, 0);
    if (idx == 21) {                      /* pause menu */
        tp_menu_n = 0;
        switch (tp_sel) {
        case 1: save_game(); sfx_play(SND_DONE); break;
        case 2: turtle = 1; break;
        case 3: turtle = 0; break;
        case 4: text_page(1, 0); break;
        }
    }
    SMS_displayOff();
    BANK(ld->core_bank);
    level_gfx_setup();
    pal_set(0);
    nt_tx = (u8)(cam_x >> 3); nt_ty = (u8)(cam_y >> 3);
    draw_all();
    apply_scroll();
    hud_dirty = 1;
    SMS_displayOn();
    fade_to(0, 3);
}

/* ending: the four story slides of level 50 (map regions with baked text) */
static void ending_slides(void) {
    static const u16 sx0[4] = { 40, 416, 680, 1056 };
    static const u16 sx1[4] = { 40, 416, 712, 1056 };
    u8 i;
    u16 t;
    for (i = 0; i < 4; i++) {
        fade_to(3, 0);
        SMS_displayOff();
        SMS_initSprites(); SMS_copySpritestoSAT(); hud_spr_ok = 0;
        cam_x = sx0[i]; cam_y = 0;
        nt_tx = (u8)(cam_x >> 3); nt_ty = 0;
        draw_all();
        apply_scroll();
        SMS_displayOn();
        fade_to(0, 3);
        for (t = 0; t < 1500; t++) {
            SMS_waitForVBlank();
            if (cam_x < sx1[i] && !(t & 3) && t > 90) {
                cam_x++;
                camera_track();
                stream_build();
                apply_scroll();
                stream_write();
                pt_flush();
            }
            if (SMS_getKeysPressed() & (K_JUMP | K_FIRE)) break;
        }
    }
    fade_to(3, 0);
    end_req = 1;
}

static void handle_requests(void) {
    if (slides_req) { slides_req = 0; ending_slides(); return; }
    if (want_text) { u8 w = want_text; want_text = 0; if (!mac_on) show_hint(w); }
    if (game_over_req) { restart_level(); return; }
    if (need_full) {
        SMS_waitForVBlank();
        SMS_displayOff();
        nt_tx = (u8)(cam_x >> 3); nt_ty = (u8)(cam_y >> 3);
        draw_all();
        apply_scroll();
        SMS_displayOn();
    }
    if (req_level != 0xFF) {
        u8 lv = req_level, en = req_entry;
        req_level = 0xFF;
        take_snapshot();
        do_level(lv, en, 0);
        return;
    }
    if (level_end_req) {
        level_end_req = 0;
        sfx_play(SND_DONE);
        if (cur_level >= 8) {              /* demo worlds end at their exit flag: back to the title */
            fade_to(3, 0);
            end_req = 1;
            return;
        }
        take_snapshot();
        do_level(0, 0, 1);
    }
}
