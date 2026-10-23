// Fireworks: a cozy firework shop. Boot, the main loop, sprites, and scene switching.
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

static void enter(int s) {
    int from = scene;
    scene = s;
    REG_BG2X = 0;                  // no leftover screen shake
    if (s == SC_SHOP) shop_enter();
    else if (s == SC_DAY) day_enter();
    else if (s == SC_TITLE || s == SC_FREE || s == SC_SHOW) show_enter();
    else if (s == SC_REVIEW) {
        if (from == SC_SHOP) show_enter();   // a night with no show
        review_begin();
    }
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
    REG_DISPSTAT = 1 << 4;         // HBlank interrupt (sky gradient)
    REG_IE = 2;
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
        if (scene != SC_SHOP) show_vblank();

        REG_TM2CNT = 0; REG_TM2D = 0; REG_TM2CNT = 0x81;   // time this frame's work
        frame++;
        keys = ~REG_KEYINPUT & 0x3FF;
        hit = keys & ~prev;
        prev = keys;
        if (next_scene != scene) {
            enter(next_scene);
            hit = 0;               // a press that switched scenes does not act twice
        }

        noam = 0;
        switch (scene) {
        case SC_TITLE: title_update(back); break;
        case SC_FREE: free_update(back); break;
        case SC_SHOP: shop_update(back); break;
        case SC_SHOW: nightshow_update(back); break;
        case SC_REVIEW: review_update(back); break;
        case SC_DAY: day_update(back); break;
        }
        for (int i = noam; i < 128; i++) { oam[i * 4] = 0x200; oam[i * 4 + 1] = 0; oam[i * 4 + 2] = 0; }
        cpu_ticks = REG_TM2D;
    }
}
