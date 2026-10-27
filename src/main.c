// Starbloom: a cozy firework shop. Boot, the main loop, sprites, and scene switching.
// Every scene draws a whole mode 4 page each frame; the pages flip at VBlank.
#include "game.h"

volatile u32 frame;           // game loops; the test harness steps by this
u16 keys, hit;
int scene = -1;
static int next_scene = SC_TITLE;
static int blank_frames;
static u16 oam[128 * 4];
static int noam;

void sprite(int x, int y, int tile, int shape, int size, int pal) {
    if (noam >= 128) return;
    u16* o = &oam[noam++ * 4];
    o[0] = (y & 255) | (shape << 14);
    o[1] = (x & 511) | (size << 14);
    o[2] = tile | (pal << 12);
}

void sprite_flip_last(void) { if (noam) oam[(noam - 1) * 4 + 1] |= 1 << 12; }

void set_scene(int s) { next_scene = s; }

// Scene loads change palettes and sprite tiles; keep the screen dark until the new
// scene has drawn its first page.
void blank_once(void) {
    REG_DISPCNT |= 0x80;
    blank_frames = 1;
}

static void wait_vblank(void) {
    while (REG_VCOUNT >= 160);
    while (REG_VCOUNT < 160);
}

extern int field_area;

// Each scene's song. The intro carries on the title's; the dev menu keeps whatever plays.
static void scene_music(int s) {
    switch (s) {
    case SC_TITLE: case SC_INTRO: music(SONG_TITLE); break;
    case SC_FREE: case SC_SHOW: music(SONG_SHOW); break;
    case SC_SHOP: music(SONG_SHOP); break;
    case SC_REVIEW: music(SONG_REVIEW); break;
    case SC_DAY: music(SONG_DAY); break;
    case SC_FIELD:                  // the day's song, with the sounds of the place over it
        music(field_area == FIELD_MINE || field_area == FIELD_GEODES ? SONG_MINE
              : field_area == FIELD_FOREST ? SONG_FOREST
              : field_area == FIELD_BEACH ? SONG_BEACH : SONG_QUARRY);
        break;
    }
}

static void enter(int s) {
    int from = scene;
    scene = s;
    REG_BG2X = 0;                  // no leftover screen shake
    if (s == SC_SHOP) shop_enter();
    else if (s == SC_DAY) day_enter();
    else if (s == SC_FIELD) field_enter();
    else if (s == SC_DEV) dev_enter();
    else if (s == SC_TITLE || s == SC_FREE || s == SC_SHOW) show_enter();
    else if (s == SC_INTRO) { show_enter(); intro_begin(); }
    else if (s == SC_REVIEW) {
        if (from == SC_SHOP) show_enter();   // a night with no show
        review_begin();
    }
    scene_music(s);
}

int main(void) {
    REG_DISPCNT = 0x80;            // forced blank while setting up
    REG_WAITCNT = 0x4317;          // faster ROM access + prefetch
    sky_init();
    ui_init();
    clear_page(PAGE0);
    clear_page(PAGE1);
    REG_BG2PA = 256; REG_BG2PB = 0; REG_BG2PC = 0; REG_BG2PD = 256;
    REG_BG2X = 0; REG_BG2Y = 0;
    IRQ_VECTOR = (u32)irq_handler;
    sound_init();
    REG_DISPSTAT = (1 << 3) | (1 << 4);   // VBlank (sound) and HBlank (sky gradient) interrupts
    REG_IE = 3;
    REG_IF = 0xFFFF;
    REG_IME = 1;
    new_game();

    int page = 0;
    u16 prev = 0;
    REG_DISPCNT = 4 | (1 << 10) | (1 << 12) | (1 << 6) | 0x80;
    for (;;) {
        wait_vblank();
        // Show the page drawn last frame; draw the other one next.
        page ^= 1;
        REG_DISPCNT = (REG_DISPCNT & ~0x10) | (page ? 0 : 0x10);
        if (blank_frames && --blank_frames == 0) REG_DISPCNT &= ~0x80;
        u8* back = page ? PAGE1 : PAGE0;
        for (int i = 0; i < 128 * 4; i++) OAM[i] = oam[i];
        if (scene != SC_SHOP && scene != SC_DAY && scene != SC_FIELD && scene != SC_DEV) show_vblank();   // the sky's light and shake

        REG_TM2CNT = 0; REG_TM2D = 0; REG_TM2CNT = 0x81;   // time this frame's work
        frame++;
        sound_frame();
        keys = ~REG_KEYINPUT & 0x3FF;
        hit = keys & ~prev;
        prev = keys;
        if (next_scene != scene) {
            enter(next_scene);
            hit = 0;               // a press that switched scenes does not act twice
        }

        noam = 0;
        if (dev_wanted()) { dev_open(); hit = 0; }   // L+R+B: the dev menu
        switch (scene) {
        case SC_TITLE: title_update(back); break;
        case SC_FREE: free_update(back); break;
        case SC_SHOP: shop_update(back); break;
        case SC_SHOW: nightshow_update(back); break;
        case SC_REVIEW: review_update(back); break;
        case SC_DAY: day_update(back); break;
        case SC_FIELD: field_update(back); break;
        case SC_DEV: dev_update(back); break;
        case SC_INTRO: intro_update(back); break;
        }
        for (int i = noam; i < 128; i++) { oam[i * 4] = 0x200; oam[i * 4 + 1] = 0; oam[i * 4 + 2] = 0; }
        cpu_ticks = REG_TM2D;
    }
}
