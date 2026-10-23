// Fireworks show prototype: a night sky over a small town. Launch shells and watch.
// Mode 4 bitmap (two pages) for particles, blended additively over a per-line sky
// gradient (HBlank interrupt). Skyline, stars, moon, and text are sprites.
#include "fw.h"
#include "tables.h"

#define REG16(a) (*(vu16*)(a))
#define REG32(a) (*(vu32*)(a))
#define REG_DISPCNT   REG16(0x04000000)
#define REG_DISPSTAT  REG16(0x04000004)
#define REG_VCOUNT    REG16(0x04000006)
#define REG_BG2PA     REG16(0x04000020)
#define REG_BG2PB     REG16(0x04000022)
#define REG_BG2PC     REG16(0x04000024)
#define REG_BG2PD     REG16(0x04000026)
#define REG_BG2X      REG32(0x04000028)
#define REG_BG2Y      REG32(0x0400002C)
#define REG_BLDCNT    REG16(0x04000050)
#define REG_BLDALPHA  REG16(0x04000052)
#define REG_DMA3SAD   REG32(0x040000D4)
#define REG_DMA3DAD   REG32(0x040000D8)
#define REG_DMA3CNT   REG32(0x040000DC)
#define REG_TM2D      REG16(0x04000108)
#define REG_TM2CNT    REG16(0x0400010A)
#define REG_KEYINPUT  REG16(0x04000130)
#define REG_IE        REG16(0x04000200)
#define REG_IF        REG16(0x04000202)
#define REG_WAITCNT   REG16(0x04000204)
#define REG_IME       REG16(0x04000208)
#define IRQ_VECTOR    REG32(0x03007FFC)

#define BG_PAL   ((vu16*)0x05000000)
#define OBJ_PAL  ((vu16*)0x05000200)
#define OAM      ((vu16*)0x07000000)
#define PAGE0    ((u8*)0x06000000)
#define PAGE1    ((u8*)0x0600A000)
#define OBJ_TILES ((vu16*)0x06010000)   // mode 4: only tiles 512+ are free

enum { KEY_A = 1, KEY_B = 2, KEY_SELECT = 4, KEY_START = 8, KEY_RIGHT = 16, KEY_LEFT = 32,
       KEY_UP = 64, KEY_DOWN = 128, KEY_R = 256, KEY_L = 512 };

#define T_SKY   512   // 4 sprites of 64x64 (64 tiles each)
#define T_FONT  768
#define T_STAR  808
#define T_MOON  809   // 16x16, 4 tiles

#define RGB(r, g, b) ((r) | ((g) << 5) | ((b) << 10))

volatile u32 frame;           // game loops; the test harness steps by this
u32 cpu_ticks;                // last frame's work, in 1/64 cycles (4389 = one full frame)
u32 clear_ticks, step_ticks;  // parts of it: clearing the page, particles
static u16 oam[128 * 4];
static int noam;
static u16 base_grad[160];
static int flash, flash_hue;
static u8 hue_rgb[15][3] = {
    {31, 3, 3}, {31, 12, 2}, {31, 21, 5}, {31, 29, 8}, {16, 31, 4}, {3, 27, 8}, {4, 26, 31},
    {6, 10, 31}, {17, 6, 31}, {31, 9, 21}, {26, 26, 28}, {22, 24, 31}, {24, 14, 3}, {28, 4, 28},
    {4, 20, 18},
};

enum { P_PEONY, P_CHRYS, P_WILLOW, P_PALM, P_RING, P_CROSS, P_HEART, P_CRACKLE, P_GLITTER, P_COUNT };
static const char* pat_names[P_COUNT] = {
    "PEONY", "CHRYSANTHEMUM", "WILLOW", "PALM", "RING", "CROSSETTE", "HEART", "CRACKLE", "GLITTER",
};
#define NCOLORS 12
static const char* hue_names[NCOLORS] = {
    "RED", "ORANGE", "GOLD", "YELLOW", "LIME", "GREEN", "CYAN", "BLUE", "VIOLET", "PINK",
    "WHITE", "SILVER",
};

int cur_pat, cur_hue;
int show_on, show_count, show_wait;
static int hud_on = 1;
static int msg_timer;
static const char* msg;

// --- small helpers -------------------------------------------------------------

static void wait_vblank(void) {
    while (REG_VCOUNT >= 160);
    while (REG_VCOUNT < 160);
}

static int isqrt(int v) {
    int r = 0;
    for (int b = 1 << 14; b; b >>= 1)
        if ((r + b) * (r + b) <= v) r += b;
    return r;
}

static void dma_fill32(void* dst, u32 words) {
    static volatile u32 zero;
    zero = 0;
    REG_DMA3SAD = (u32)&zero;
    REG_DMA3DAD = (u32)dst;
    REG_DMA3CNT = words | (2u << 23) | (1u << 26) | (1u << 31);   // src fixed, 32-bit
}

// --- palettes ------------------------------------------------------------------

static void make_palettes(void) {
    for (int h = 0; h < 15; h++)
        for (int l = 0; l < 16; l++) {
            int c[3];
            for (int k = 0; k < 3; k++) {
                int v = hue_rgb[h][k] * (l + 1) >> 4;
                if (l > 10) v += (31 - v) * (l - 10) / 6;   // hot core whitens
                c[k] = v;
            }
            BG_PAL[16 + h * 16 + l] = RGB(c[0], c[1], c[2]);
        }
    // Sky: deep navy at the top, dusky violet at the horizon. Colors are worked out in
    // quarter steps and dithered across lines, so the 5-bit bands do not show.
    static const u8 dither[4] = { 0, 2, 1, 3 };
    for (int y = 0; y < 160; y++) {
        int r, g, b, d = dither[y & 3];
        if (y < 100) { r = 4 + 16 * y / 100; g = 4 + 8 * y / 100; b = 20 + 24 * y / 100; }
        else { int t = y - 100; r = 20 + 16 * t / 60; g = 12 + 8 * t / 60; b = 44 + 4 * t / 60; }
        base_grad[y] = RGB((r + d) >> 2, (g + d) >> 2, (b + d) >> 2);
    }
    OBJ_PAL[16 + 1] = RGB(31, 31, 31);   // font
    OBJ_PAL[16 + 2] = RGB(2, 2, 6);
}

static int clamp31(int v) { return v > 31 ? 31 : v; }

static u16 tint(u16 c, int div) {
    int r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
    const u8* h = hue_rgb[flash_hue];
    r = clamp31(r + h[0] * flash / div);
    g = clamp31(g + h[1] * flash / div);
    b = clamp31(b + h[2] * flash / div);
    return RGB(r, g, b);
}

// Sky and skyline light up with each burst, in the burst's color.
static void update_light(void) {
    static int last = -1;
    if (flash == 0 && last == 0) return;
    last = flash;
    for (int y = 0; y < 160; y++) grad[y] = flash ? tint(base_grad[y], 160 + (159 - y)) : base_grad[y];
    static const u16 sky_cols[6] = { 0, RGB(5, 4, 10), RGB(2, 2, 5), RGB(31, 24, 10), RGB(14, 10, 6), RGB(1, 1, 3) };
    for (int i = 1; i < 6; i++) OBJ_PAL[i] = flash && i != 3 ? tint(sky_cols[i], i == 1 ? 110 : 200) : sky_cols[i];
}

// --- sprites -------------------------------------------------------------------

static void put_px(int tile, int w8, int x, int y, int c) {
    // 4bpp pixel inside a sprite whose tiles run in 1D order (w8 = width in tiles).
    int t = tile + (y >> 3) * w8 + (x >> 3);
    vu16* p = OBJ_TILES + t * 16 + (y & 7) * 2 + ((x & 7) >> 2);
    int sh = (x & 3) * 4;
    *p = (*p & ~(15 << sh)) | (c << sh);
}

static u32 hash(u32 a) { a ^= a >> 16; a *= 0x45d9f3b; a ^= a >> 16; return a; }

static void make_skyline(void) {
    static u8 far_h[256], near_h[256];
    u32 seed = 7;
    for (int x = 0; x < 256;) {
        seed = hash(seed + x);
        int w = 10 + seed % 16, h = 18 + (seed >> 8) % 26;
        for (int i = 0; i < w && x < 256; i++, x++) far_h[x] = h;
    }
    for (int x = 0; x < 256;) {
        seed = hash(seed + x * 3);
        int w = 8 + seed % 14, h = 8 + (seed >> 8) % 22;
        if ((seed >> 16) % 5 == 0) h = 4;   // gaps between houses
        for (int i = 0; i < w && x < 256; i++, x++) near_h[x] = h;
    }
    for (int i = 0; i < 4 * 64 * 16; i++) OBJ_TILES[T_SKY * 16 + i] = 0;
    for (int x = 0; x < 256; x++)
        for (int y = 0; y < 64; y++) {
            int h = 64 - y, c = 0;
            if (h <= 3) c = 5;
            else if (h <= near_h[x]) {
                c = 2;
                if (x % 3 == 1 && h % 4 == 2 && h < near_h[x] - 1) {
                    u32 r = hash(x / 3 * 977 + h / 4);
                    c = r % 4 == 0 ? 3 : (r % 4 == 1 ? 4 : 2);
                }
            } else if (h <= far_h[x]) {
                c = 1;
                if (x % 4 == 2 && h % 5 == 2 && hash(x * 31 + h) % 7 == 0) c = 4;
            }
            if (c) put_px(T_SKY + (x >> 6) * 64, 8, x & 63, y, c);
        }
}

// 3x5 glyphs, one octal digit per row.
static const u16 font[40] = {
    075557, 026227, 071747, 071717, 055711, 074717, 074757, 071111, 075757, 075717,
    025755, 065656, 034443, 065556, 074647, 074644, 034553, 055755, 072227, 011152,
    055655, 044447, 057755, 065555, 025552, 065644, 025563, 065655, 034216, 072222,
    055557, 055552, 055775, 055255, 055222, 071247, 002020, 051245, 000700, 011244,
};

static void make_font(void) {
    for (int i = 0; i < 40 * 16; i++) OBJ_TILES[T_FONT * 16 + i] = 0;
    for (int g = 0; g < 40; g++)
        for (int pass = 0; pass < 2; pass++)
            for (int r = 0; r < 5; r++)
                for (int c = 0; c < 3; c++)
                    if ((font[g] >> ((4 - r) * 3 + (2 - c))) & 1)
                        put_px(T_FONT + g, 1, c + (pass ? 0 : 1), r + (pass ? 0 : 1), pass ? 1 : 2);
    for (int i = 0; i < 16; i++) OBJ_TILES[T_STAR * 16 + i] = 0;
    put_px(T_STAR, 1, 3, 3, 1);
    // Crescent moon: a disc minus a shifted disc.
    for (int i = 0; i < 4 * 16; i++) OBJ_TILES[T_MOON * 16 + i] = 0;
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) {
            int dx = x - 8, dy = y - 8, ex = x - 11, ey = y - 6;
            if (dx * dx + dy * dy <= 36 && ex * ex + ey * ey > 30) put_px(T_MOON, 2, x, y, 1);
        }
    OBJ_PAL[48 + 1] = RGB(30, 28, 20);
}

static void sprite(int x, int y, int tile, int shape_size, int pal) {
    if (noam >= 128) return;
    u16* o = &oam[noam++ * 4];
    o[0] = (y & 255);
    o[1] = (x & 511) | (shape_size << 14);
    o[2] = tile | (pal << 12);
}

static int glyph(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'Z') return ch - 'A' + 10;
    if (ch == ':') return 36;
    if (ch == '%') return 37;
    if (ch == '-') return 38;
    if (ch == '/') return 39;
    return -1;
}

static void text(int x, int y, const char* s) {
    for (; *s; s++, x += 4) {
        int g = glyph(*s);
        if (g >= 0) sprite(x, y, T_FONT + g, 0, 1);
    }
}

static int text_w(const char* s) { int n = 0; while (s[n]) n++; return n * 4; }

static char* utoa(char* p, u32 v) {
    char tmp[10]; int n = 0;
    do { tmp[n++] = '0' + v % 10; v /= 10; } while (v);
    while (n) *p++ = tmp[--n];
    *p = 0;
    return p;
}

static u8 star_xy[24][2];

static void make_stars(void) {
    for (int i = 0; i < 24; i++) {
        u32 r = hash(i * 7919 + 13);
        star_xy[i][0] = r % 236;
        star_xy[i][1] = 4 + (r >> 8) % 80;
    }
}

static void build_oam(void) {
    noam = 0;
    if (hud_on) {
        char buf[40], *p;
        if (show_on) text(4, 4, "SHOW TIME");
        else {
            text(4, 4, pat_names[cur_pat]);
            text(4, 11, hue_names[cur_hue]);
        }
        p = utoa(buf, nparts); *p++ = ' '; *p++ = 'P'; *p = 0;
        text(236 - text_w(buf), 4, buf);
        p = buf; *p++ = 'C'; *p++ = 'P'; *p++ = 'U'; *p++ = ' ';
        p = utoa(p, cpu_ticks * 100 / 4389); *p++ = '%'; *p = 0;
        text(236 - text_w(buf), 11, buf);
        if (msg_timer) text((240 - text_w(msg)) / 2, 40, msg);
        else if (!show_on) text(4, 20, "A FIRE  B VOLLEY  L/R SHELL  UP/DN COLOR  START SHOW");
    }
    sprite(204, 34, T_MOON, 1, 3);
    for (int i = 0; i < 24; i++) sprite(star_xy[i][0], star_xy[i][1], T_STAR, 0, 2);
    for (int k = 0; k < 4; k++) sprite(k * 64, 96, T_SKY + k * 64, 3, 0);
    for (int i = noam; i < 128; i++) { oam[i * 4] = 0x200; oam[i * 4 + 1] = 0; oam[i * 4 + 2] = 0; }
}

static void twinkle(void) {
    int t = (frame >> 2) & 31;
    int v = 12 + (t < 16 ? t : 31 - t) / 3;
    OBJ_PAL[32 + 1] = RGB(v, v, v + 2 > 31 ? 31 : v + 2);
}

// --- shells --------------------------------------------------------------------

static void add(int x, int y, int vx, int vy, int life, int hue, int kind) {
    if (nparts >= MAXP) return;
    Part* s = &parts[nparts++];
    s->x = x; s->y = y; s->vx = vx; s->vy = vy;
    s->life = life > 255 ? 255 : life; s->hue = hue; s->kind = kind; s->aux = 0;
}

static void sphere(int x, int y, int n, int speed, int kind, int hue, int life, int lifemask) {
    for (int i = 0; i < n; i++) {
        u32 r = rnd();
        int z = (int)(r & 511) - 256;
        int s = speed * sqrt_tab[z < 0 ? -z : z] >> 8;
        s += s * ((int)((r >> 17) & 31) - 16) >> 8;
        int a = (r >> 9) & 255;
        add(x, y, s * sin_tab[(a + 64) & 255] >> 8, s * sin_tab[a] >> 8, life + ((r >> 22) & lifemask), hue, kind);
    }
}

static void explode(const Burst* b) {
    int x = b->x, y = b->y, hue = b->hue;
    int pat = b->aux & 15, size = b->aux >> 4;
    int S = 300 + size * 80;
    int N = 60 + size * 45;
    u32 r = rnd();
    switch (pat) {
    case P_PEONY:
        sphere(x, y, N, S, K_PEONY, hue, 50, 15);
        if (r & 1) sphere(x, y, N / 3, S * 2 / 5, K_PEONY, (hue + 3 + (r >> 1) % 6) % NCOLORS, 45, 15);
        break;
    case P_CHRYS:
        sphere(x, y, N, S, K_CHRYS, hue, 60, 15);
        break;
    case P_WILLOW:
        sphere(x, y, N * 4 / 5, S * 9 / 10, K_WILLOW, H_WILLOW, 150, 31);
        break;
    case P_PALM: {
        int arms = 6 + (r & 1), rot = (r >> 1) & 255;
        for (int a = 0; a < arms; a++) {
            int ang = (rot + a * 256 / arms) & 255;
            for (int j = 0; j < 7; j++) {
                int s = S * (55 + j * 7) / 100;
                add(x, y, s * sin_tab[(ang + 64) & 255] >> 8, s * sin_tab[ang] >> 8, 70 + j * 3, hue, K_PALM);
            }
        }
        sphere(x, y, 12, S / 4, K_PEONY, H_GOLD, 25, 7);
        break;
    }
    case P_RING: {
        int n = 40 + size * 12;
        int tilt = 64 + (r & 191);   // 0.25..1.0
        int rot = (r >> 8) & 255;
        for (int i = 0; i < n; i++) {
            int a = i * 256 / n;
            int cx = S * sin_tab[(a + 64) & 255] >> 8, cy = (S * sin_tab[a] >> 8) * tilt >> 8;
            // Rotate the flattened ring so it is not always level.
            int c = sin_tab[(rot + 64) & 255], s = sin_tab[rot];
            add(x, y, (cx * c - cy * s) >> 8, (cx * s + cy * c) >> 8, 52, hue, K_PEONY);
        }
        break;
    }
    case P_CROSS:
        sphere(x, y, 10 + size * 3, S * 4 / 5, K_CROSS, hue, 34, 7);
        break;
    case P_HEART:
        for (int i = 0; i < 48; i++)
            add(x, y, heart_tab[i * 2] * S / 300, heart_tab[i * 2 + 1] * S / 300, 58, hue, K_PEONY);
        break;
    case P_CRACKLE:
        sphere(x, y, N * 3 / 4, S, K_CRACKLE, hue, 35, 31);
        break;
    case P_GLITTER:
        sphere(x, y, N, S * 4 / 5, K_GLITTER, hue, 70, 31);
        break;
    }
    flash = 14 + size * 4;
    flash_hue = pat == P_WILLOW ? H_WILLOW : hue;
}

static void launch(int x, int pat, int hue, int size) {
    if (nparts >= MAXP) return;
    int apex = 22 + (int)(rnd() % 50);         // burst height (screen y)
    int h = 152 - apex;
    int v0 = isqrt(2048 * h);                  // reaches the apex with gravity 4 (8.8)
    add(x << 6, 152 << 6, (int)(rnd() & 31) - 16, -v0, 255, hue, K_ROCKET);
    parts[nparts - 1].aux = pat | (size << 4);
}

static void launch_random(void) {
    int pat = rnd() % P_COUNT;
    int hue = rnd() % NCOLORS;
    if (pat == P_GLITTER) hue = (rnd() & 1) ? H_SILVER : H_GOLD;
    if (pat == P_CRACKLE) hue = (rnd() & 1) ? H_GOLD : H_WHITE;
    launch(30 + rnd() % 180, pat, hue, rnd() % 3);
}

// The auto show: a calm opening, a busier middle, a finale, then quiet.
static void run_show(void) {
    if (--show_wait > 0) return;
    show_count++;
    if (show_count <= 30) {
        launch_random();
        if ((rnd() & 7) == 0) launch_random();
        show_wait = 30 + rnd() % 50;
    } else if (show_count <= 55) {
        launch_random();
        show_wait = 6 + rnd() % 10;
    } else if (nparts) {
        show_wait = 1;              // let the finale fade out first
    } else {
        show_on = 0;
        msg = "THAT WAS THE SHOW";
        msg_timer = 180;
    }
}

// --- main ----------------------------------------------------------------------

int main(void) {
    REG_DISPCNT = 0x80;            // forced blank while setting up
    REG_WAITCNT = 0x4317;          // faster ROM access + prefetch
    make_palettes();
    make_skyline();
    make_font();
    make_stars();
    for (int y = 0; y < 160; y++) grad[y] = base_grad[y];
    update_light();
    dma_fill32(PAGE0, 0xA000 / 4 * 2);
    REG_BG2PA = 256; REG_BG2PB = 0; REG_BG2PC = 0; REG_BG2PD = 256;
    REG_BG2X = 0; REG_BG2Y = 0;
    // BG2 adds onto the sky backdrop: 1st target BG2, 2nd target backdrop, both at full weight.
    REG_BLDCNT = (1 << 2) | (1 << 6) | (1 << 13);
    REG_BLDALPHA = 16 | (16 << 8);
    cur_hue = H_RED;
    IRQ_VECTOR = (u32)irq_handler;
    REG_DISPSTAT = 1 << 4;         // HBlank interrupt
    REG_IE = 2;
    REG_IF = 0xFFFF;
    REG_IME = 1;

    int page = 0;
    u16 prev = 0;
    REG_DISPCNT = 4 | (1 << 10) | (1 << 12) | (1 << 6);
    for (;;) {
        wait_vblank();
        // Show the page drawn last frame; draw the other one next.
        page ^= 1;
        REG_DISPCNT = (REG_DISPCNT & ~0x10) | (page ? 0 : 0x10);
        u8* back = page ? PAGE1 : PAGE0;
        for (int i = 0; i < 128 * 4; i++) OAM[i] = oam[i];
        update_light();
        twinkle();

        REG_TM2CNT = 0; REG_TM2D = 0; REG_TM2CNT = 0x81;   // time this frame's work
        frame++;

        u16 keys = ~REG_KEYINPUT & 0x3FF;
        u16 hit = keys & ~prev;
        prev = keys;
        if (hit & KEY_SELECT) hud_on ^= 1;
        if (hit & KEY_START) {
            show_on ^= 1;
            show_count = 0; show_wait = 1;
            msg_timer = 0;
        }
        if (!show_on) {
            if (hit & KEY_R) cur_pat = (cur_pat + 1) % P_COUNT;
            if (hit & KEY_L) cur_pat = (cur_pat + P_COUNT - 1) % P_COUNT;
            if (hit & KEY_UP) cur_hue = (cur_hue + 1) % NCOLORS;
            if (hit & KEY_DOWN) cur_hue = (cur_hue + NCOLORS - 1) % NCOLORS;
            if (hit & KEY_A) launch(40 + rnd() % 160, cur_pat, cur_hue, 1 + (rnd() & 1));
            if (hit & KEY_B) for (int i = 0; i < 5; i++) launch_random();
        } else run_show();
        if (msg_timer) msg_timer--;

        u32 t0 = REG_TM2D;
        clear_page(back);
        u32 t1 = REG_TM2D;
        step_draw(back);
        u32 t2 = REG_TM2D;
        clear_ticks = t1 - t0; step_ticks = t2 - t1;
        for (int i = 0; i < nbursts; i++) explode(&bursts[i]);
        nbursts = 0;
        if (flash) flash--;
        build_oam();
        cpu_ticks = REG_TM2D;
    }
}
