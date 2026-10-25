/*
 * Jill of the Jungle -> Sega Master System
 * devkitSMS / SDCC / SMSlib.  Single translation unit.
 */
#ifdef DEBUGSYMS
#define static
#endif
#include "common.inc"
#include "audio.inc"
#include "video.inc"
#include "level.inc"
#include "player.inc"
#include "objects.inc"
#include "objdraw.inc"

#define JILL_RUN_R 8
#define JILL_RUN_L 0

#include "textpage.inc"
#include "flow.inc"

static void game_loop(void) {
    static u8 tphase;
    end_req = 0;
    while (!end_req) {
        if (turtle && (tphase ^= 1)) {          /* turtle mode: every second frame is skipped */
            SMS_waitForVBlank();
            turtle_latch |= SMS_getKeysPressed();
            continue;
        }
        game_logic();
        present();
        handle_requests();
    }
}

/* title menu: start / load / help / three demo worlds; idle cycles through the news bulletins */
#define LV_DEMO0 8
#define TITLE_ITEMS 6
static u8 start_level, start_load;

static void title_screen(void) {
    static const u8 attract[9] = { 16, 17, 18, 19, 10, 11, 12, 13, 14 };
    u8 k, n = 0;
    music_play(0);
    tp_menu_row = 10; tp_menu_n = TITLE_ITEMS; tp_sel = 0;
    start_load = 0;
    for (;;) {
        k = text_page(0, 600);
        if (k) {
            if (tp_sel == 0) { start_level = 0; break; }
            if (tp_sel == 1) {
                if (save_valid()) { start_level = 0; start_load = 1; break; }
                sfx_play(SND_HURT);
                continue;
            }
            if (tp_sel == 2) { tp_menu_n = 0; text_page(1, 0); tp_menu_n = TITLE_ITEMS; continue; }
            start_level = (u8)(LV_DEMO0 + tp_sel - 3);
            break;
        }
        tp_menu_n = 0;
        text_page(attract[n], 700);
        tp_menu_n = TITLE_ITEMS;
        if (++n > 8) n = 0;
    }
    tp_menu_n = 0;
}

static void run_session(void) {
    for (;;) {
        title_screen();
        turtle = 0; turtle_latch = 0;
        if (start_load) { load_game(); cur_level = 0; level_load(0, 0, 1); }
        else { new_game_state(); cur_level = start_level; level_load(start_level, 0, 0); }
        screen_on();
        fade_to(0, 3);
        game_loop();
    }
}

void main(void) {
    SMS_init();
    SMS_setSpriteMode(SPRITEMODE_TALL);
    SMS_useFirstHalfTilesforSprites(1);
    SMS_VDPturnOnFeature(VDPFEATURE_LEFTCOLBLANK);
    SMS_setBackdropColor(0);
    audio_reset();
    SMS_setFrameInterruptHandler(audio_tick);
    OVL_SET(OVL_A);
    run_session();
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
SMS_EMBED_SDSC_HEADER_AUTO_DATE(1, 0, "Haroldo-OK", "Jill of the Jungle SMS",
    "Port of Epic MegaGames' Jill of the Jungle (Episode 1) to the Sega Master System");
