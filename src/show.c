// Fireworks scenes: title, free play, the night's show, and review time.
// Mode 4 bitmap (two pages) for particles, blended additively over a per-line sky
// gradient (HBlank interrupt). Skyline, stars, and moon are sprites.
#include "game.h"
#include "tables.h"
#include "art.h"

#define T_SKY   512   // 4 sprites of 64x64 (64 tiles each)
#define T_FONT  768   // 3x5 sprite font (free play HUD)
#define T_STAR  808
#define T_MOON  809   // 16x16, 4 tiles
#define T_LOGO  848   // the Starbloom logo: 4 sprites of 64x32, then 8 of 32x8 (160 tiles), OBJ palette bank 5
#define LOGO_Y  50                    // the logo sits in the middle of the sky (Caleb's pick)

u32 cpu_ticks;                // last frame's work, in 1/64 cycles (4389 = one full frame)
static u16 base_grad[160];
static int flash, flash_hue;

const char* const pat_names[P_COUNT] = {
    "PEONY", "CHRYSANTHEMUM", "WILLOW", "PALM", "RING", "CROSSETTE", "HEART", "CRACKLE", "GLITTER",
};
const char* const pat_plural[P_COUNT] = {
    "PEONIES", "CHRYSANTHEMUMS", "WILLOWS", "PALMS", "RINGS", "CROSSETTES", "HEARTS", "CRACKLES", "GLITTER",
};
const char* const hue_names[NCOLORS] = {
    "RED", "ORANGE", "GOLD", "YELLOW", "LIME", "GREEN", "CYAN", "BLUE", "VIOLET", "PINK", "WHITE", "SILVER",
};
const char* const size_names[3] = { "SMALL", "MEDIUM", "LARGE" };
const u8 hue_rgb[15][3] = {
    {31, 3, 3}, {31, 12, 2}, {31, 21, 5}, {31, 29, 8}, {16, 31, 4}, {3, 27, 8}, {4, 26, 31},
    {6, 10, 31}, {17, 6, 31}, {31, 9, 21}, {26, 26, 28}, {22, 24, 31}, {24, 14, 3}, {28, 4, 28},
    {4, 20, 18},
};

// --- sky and palettes ----------------------------------------------------------

static int isqrt(int v) {
    int r = 0;
    for (int b = 1 << 14; b; b >>= 1)
        if ((r + b) * (r + b) <= v) r += b;
    return r;
}

// Sky: deep navy at the top, dusky violet at the horizon. Colors are worked out in
// quarter steps and dithered across lines, so the 5-bit bands do not show.
void sky_init(void) {
    static const u8 dither[4] = { 0, 2, 1, 3 };
    for (int y = 0; y < 160; y++) {
        int r, g, b, d = dither[y & 3];
        if (y < 100) { r = 4 + 16 * y / 100; g = 4 + 8 * y / 100; b = 20 + 24 * y / 100; }
        else { int t = y - 100; r = 20 + 16 * t / 60; g = 12 + 8 * t / 60; b = 44 + 4 * t / 60; }
        base_grad[y] = RGB((r + d) >> 2, (g + d) >> 2, (b + d) >> 2);
        grad[y] = base_grad[y];
    }
}

void sky_reset(void) {
    for (int y = 0; y < 160; y++) grad[y] = base_grad[y];
}

static void firework_palette(void) {
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
    ui_palette();
    OBJ_PAL[16 + 1] = RGB(31, 31, 31);   // sprite font
    OBJ_PAL[16 + 2] = RGB(2, 2, 6);
    OBJ_PAL[48 + 1] = RGB(30, 28, 20);   // moon
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
static int light_last = -1;

static void update_light(void) {
    if (flash == 0 && light_last == 0) return;
    light_last = flash;
    for (int y = 0; y < 160; y++) grad[y] = flash ? tint(base_grad[y], 160 + (159 - y)) : base_grad[y];
    static const u16 sky_cols[6] = { 0, RGB(5, 4, 10), RGB(2, 2, 5), RGB(31, 24, 10), RGB(14, 10, 6), RGB(1, 1, 3) };
    for (int i = 1; i < 6; i++) OBJ_PAL[i] = flash && i != 3 ? tint(sky_cols[i], i == 1 ? 110 : 200) : sky_cols[i];
}

static void twinkle(void) {
    int t = (frame >> 2) & 31;
    int v = 12 + (t < 16 ? t : 31 - t) / 3;
    OBJ_PAL[32 + 1] = RGB(v, v, v + 2 > 31 ? 31 : v + 2);
}

int review_shake(void);
// Window 0 turns the glow off over the rating card. Set while drawing a page,
// applied when that page is shown.
static int win_on, win_x0, win_y0, win_x1, win_y1;

void show_vblank(void) {
    if (win_on) {
        REG_WIN0H = (win_x0 << 8) | win_x1;
        REG_WIN0V = (win_y0 << 8) | win_y1;
        REG_WININ = 0x1F;              // inside: all layers, no blending
        REG_WINOUT = 0x3F;             // outside: all layers, blending
        REG_DISPCNT |= 1 << 13;
    } else REG_DISPCNT &= ~(1 << 13);
    REG_BG2X = review_shake() << 8;   // review time shakes the screen when stars land
    update_light();
    twinkle();
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

static void make_small_font(void) {
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
}

static int small_glyph(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'Z') return ch - 'A' + 10;
    if (ch == ':') return 36;
    if (ch == '%') return 37;
    if (ch == '-') return 38;
    if (ch == '/') return 39;
    return -1;
}

static void small_text(int x, int y, const char* s) {
    for (; *s; s++, x += 4) {
        int g = small_glyph(*s);
        if (g >= 0) sprite(x, y, T_FONT + g, 0, 0, 1);
    }
}

static int small_w(const char* s) { int n = 0; while (s[n]) n++; return n * 4; }

static u8 star_xy[24][2];
// No stars or moon over these boxes (bubbles, cards); reset after each frame's sprites.
static s16 hide[12][4];
static int hide_n;

static void hide_box(int x, int y, int w, int h) {
    if (hide_n >= 12) return;
    hide[hide_n][0] = x; hide[hide_n][1] = y; hide[hide_n][2] = x + w; hide[hide_n][3] = y + h;
    hide_n++;
}

static int hidden(int x, int y, int w, int h) {
    for (int i = 0; i < hide_n; i++)
        if (x + w > hide[i][0] && x < hide[i][2] && y + h > hide[i][1] && y < hide[i][3]) return 1;
    return 0;
}

void show_sprites(void) {
    if (!hidden(204, 34, 16, 16)) sprite(204, 34, T_MOON, 0, 1, 3);
    for (int i = 0; i < 24; i++)
        if (!hidden(star_xy[i][0] + 3, star_xy[i][1] + 3, 1, 1)) sprite(star_xy[i][0], star_xy[i][1], T_STAR, 0, 0, 2);
    // In review time and the intro the town sits behind the bitmap, so boxes cover it.
    for (int k = 0; k < 4; k++) sprite(k * 64, 96, (T_SKY + k * 64) | (scene == SC_REVIEW || scene == SC_INTRO || scene == SC_TITLE ? 1 << 10 : 0), 0, 3, 0);
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

// A rocket's hue byte: low 4 bits the color; high 4 bits the second color + 1 (0 = none).
static void explode(const Burst* b) {
    int x = b->x, y = b->y, hue = b->hue & 15, hue2 = (b->hue >> 4) - 1;
    int h2 = hue2 >= 0 ? hue2 : hue;               // where a pattern splits its stars
    int pat = b->aux & 15, size = b->aux >> 4;
    int S = 300 + size * 80;
    int N = 60 + size * 45;
    u32 r = rnd();
    switch (pat) {
    case P_PEONY:                                   // a second color becomes the core
        sphere(x, y, N, S, K_PEONY, hue, 50, 15);
        if (hue2 >= 0) sphere(x, y, N / 3, S * 2 / 5, K_PEONY, hue2, 45, 15);
        else if (r & 1) sphere(x, y, N / 3, S * 2 / 5, K_PEONY, (hue + 3 + (r >> 1) % 6) % NCOLORS, 45, 15);
        break;
    case P_CHRYS:
        sphere(x, y, N / 2, S, K_CHRYS, hue, 60, 15);
        sphere(x, y, N - N / 2, S, K_CHRYS, h2, 60, 15);
        break;
    case P_WILLOW:
        sphere(x, y, N * 2 / 5, S * 9 / 10, K_WILLOW, hue == H_GOLD ? H_WILLOW : hue, 150, 31);
        sphere(x, y, N * 2 / 5, S * 9 / 10, K_WILLOW, h2 == H_GOLD ? H_WILLOW : h2, 150, 31);
        break;
    case P_PALM: {
        int arms = 6 + (r & 1), rot = (r >> 1) & 255;
        for (int a = 0; a < arms; a++) {
            int ang = (rot + a * 256 / arms) & 255;
            for (int j = 0; j < 7; j++) {
                int s = S * (55 + j * 7) / 100;
                add(x, y, s * sin_tab[(ang + 64) & 255] >> 8, s * sin_tab[ang] >> 8, 70 + j * 3, (a & 1) ? h2 : hue, K_PALM);
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
            add(x, y, (cx * c - cy * s) >> 8, (cx * s + cy * c) >> 8, 52, (i & 1) ? h2 : hue, K_PEONY);
        }
        break;
    }
    case P_CROSS:
        sphere(x, y, 5 + size * 2, S * 4 / 5, K_CROSS, hue, 34, 7);
        sphere(x, y, 5 + size, S * 4 / 5, K_CROSS, h2, 34, 7);
        break;
    case P_HEART:
        for (int i = 0; i < 48; i++)
            add(x, y, heart_tab[i * 2] * S / 300, heart_tab[i * 2 + 1] * S / 300, 58, (i & 1) ? h2 : hue, K_PEONY);
        break;
    case P_CRACKLE:
        sphere(x, y, N * 3 / 8, S, K_CRACKLE, hue, 35, 31);
        sphere(x, y, N * 3 / 8, S, K_CRACKLE, h2, 35, 31);
        break;
    case P_GLITTER:
        sphere(x, y, N / 2, S * 4 / 5, K_GLITTER, hue, 70, 31);
        sphere(x, y, N - N / 2, S * 4 / 5, K_GLITTER, h2, 70, 31);
        break;
    }
    flash = 14 + size * 4;
    flash_hue = pat == P_WILLOW && hue == H_GOLD ? H_WILLOW : hue;
}

// Bursts at screen y apex (the default picks one at random).
static void launch_at(int x, int pat, int hue, int size, int apex) {
    if (nparts >= MAXP) return;
    int h = 152 - apex;
    int v0 = isqrt(2048 * h);                  // reaches the apex with gravity 4 (8.8)
    add(x << 6, 152 << 6, (int)(rnd() & 31) - 16, -v0, 255, hue, K_ROCKET);
    parts[nparts - 1].aux = pat | (size << 4);
}

static void launch(int x, int pat, int hue, int size) {
    launch_at(x, pat, hue, size, 22 + (int)(rnd() % 50));
}

static void launch_random(void) {
    int pat = rnd() % P_COUNT;
    int hue = rnd() % NCOLORS;
    if (pat == P_GLITTER) hue = (rnd() & 1) ? H_SILVER : H_GOLD;
    if (pat == P_CRACKLE) hue = (rnd() & 1) ? H_GOLD : H_WHITE;
    launch(30 + rnd() % 180, pat, hue, rnd() % 3);
}

// Moves and draws all particles into the back page, then turns rockets into bursts.
static void sky_frame(u8* back) {
    clear_page(back);
    step_draw(back);
    for (int i = 0; i < nbursts; i++) explode(&bursts[i]);
    nbursts = 0;
    if (flash) flash--;
}

// --- scene entry ---------------------------------------------------------------

static int show_on, show_count, show_wait, hud_on = 1, msg_timer;
static const char* msg;
static int cur_pat, cur_hue = H_RED;
static int title_cursor, title_wait, title_saved, title_confirm;
static int ns_idx, ns_wait, ns_end;

void show_enter(void) {
    blank_once();
    firework_palette();
    make_skyline();
    make_small_font();
    vu32* lt = (vu32*)OBJ_TILES;
    for (int i = 0; i < 160 * 8; i++) lt[T_LOGO * 8 + i] = logo_tiles[i];
    for (int i = 0; i < 16; i++) OBJ_PAL[80 + i] = logo_pal[i];
    for (int i = 0; i < 24; i++) {
        u32 r = hash(i * 7919 + 13);
        star_xy[i][0] = r % 236;
        star_xy[i][1] = 4 + (r >> 8) % 80;
    }
    // BG2 adds onto the sky backdrop: 1st target BG2, 2nd target backdrop, both at full weight.
    REG_BLDCNT = (1 << 2) | (1 << 6) | (1 << 13);
    REG_BLDALPHA = 16 | (16 << 8);
    nparts = 0;
    nbursts = 0;
    flash = 0;
    win_on = 0;
    hide_n = 0;
    light_last = -1;
    show_on = 0;
    msg_timer = 0;
    title_wait = 1;                                  // the title opens with a firework
    title_saved = has_save();
    title_confirm = 0;
    title_cursor = title_saved ? 0 : 1;
    ns_idx = 0;
    ns_wait = 60;
    ns_end = 0;
}

static void text_center(u8* page, int y, const char* s, int color) {
    text(page, (240 - text_w(s)) / 2, y, s, color);
}

// --- title ---------------------------------------------------------------------

void title_update(u8* back) {
    // Fireworks bloom in the sky above the logo.
    if (--title_wait <= 0) {
        int pat = rnd() % P_COUNT, hue = rnd() % NCOLORS;
        launch_at(24 + rnd() % 192, pat, hue, rnd() % 2, 12 + rnd() % 20);
        if ((rnd() & 3) == 0) launch_at(24 + rnd() % 192, rnd() % P_COUNT, rnd() % NCOLORS, 0, 14 + rnd() % 16);
        title_wait = 35 + rnd() % 45;
    }
    static const char* const items[3] = { "CONTINUE", "NEW GAME", "FREE PLAY" };
    int first = title_saved ? 0 : 1;   // no CONTINUE without a save
    if (title_cursor < first) title_cursor = first;
    if (hit & KEY_UP) { title_cursor = title_cursor > first ? title_cursor - 1 : 2; title_confirm = 0; }
    if (hit & KEY_DOWN) { title_cursor = title_cursor < 2 ? title_cursor + 1 : first; title_confirm = 0; }
    if (hit & (KEY_A | KEY_START)) {
        if (title_cursor == 0) {
            int phase = load_game();
            if (phase >= 0) set_scene(phase == SAVE_DAY ? SC_DAY : SC_SHOP);
        } else if (title_cursor == 1) {
            if (title_saved && !title_confirm) title_confirm = 1;   // ask before replacing a save
            else { new_game(); set_scene(SC_INTRO); }
        } else set_scene(SC_FREE);
    }
    sky_frame(back);
    text_center(back, LOGO_Y + 46, "A COZY FIREWORK SHOP", UI_WHITE);
    // The logo, across the screen, with a few stars twinkling around it.
    static const u8 twinkles[4][2] = { { 14, 20 }, { 220, 24 }, { 36, 72 }, { 200, 70 } };
    for (int k = 0; k < 4; k++)
        if (((frame >> 4) + k * 3) % 5 < 2) sprite(twinkles[k][0], twinkles[k][1], T_STAR, 0, 0, 2);
    for (int k = 0; k < 4; k++) sprite(k * 64, LOGO_Y, T_LOGO + k * 32, 1, 3, 5);
    for (int k = 0; k < 8; k++) sprite(k * 32, LOGO_Y + 32, T_LOGO + 128 + k * 4, 1, 1, 5);
    hide_box(0, LOGO_Y - 4, 240, 48);               // no moon or stars over the logo
    for (int i = first; i < 3; i++) {
        int y = LOGO_Y + 62 + (i - first) * 11;
        text(back, 90, y, items[i], title_cursor == i ? UI_YELLOW : UI_WHITE);
        if (title_cursor == i) text(back, 80, y, ">", UI_YELLOW);
    }
    if (title_confirm) {
        text_center(back, 136, "START OVER? YOUR SAVE WILL BE", UI_WHITE);
        text_center(back, 146, "REPLACED. PRESS A AGAIN.", UI_WHITE);
    }
    show_sprites();
}

// --- free play (the v0.1 sandbox) ----------------------------------------------

static void run_auto_show(void) {
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

void free_update(u8* back) {
    if ((keys & KEY_SELECT) && (hit & KEY_START)) { set_scene(SC_TITLE); return; }
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
    } else run_auto_show();
    if (msg_timer) msg_timer--;
    sky_frame(back);

    if (hud_on) {
        char buf[40], *p;
        if (show_on) small_text(4, 4, "SHOW TIME");
        else {
            small_text(4, 4, pat_names[cur_pat]);
            small_text(4, 11, hue_names[cur_hue]);
        }
        p = cat_num(buf, nparts); cat(p, " P");
        small_text(236 - small_w(buf), 4, buf);
        p = cat(buf, "CPU ");
        p = cat_num(p, cpu_ticks * 100 / 4389); cat(p, "%");
        small_text(236 - small_w(buf), 11, buf);
        if (msg_timer) small_text((240 - small_w(msg)) / 2, 40, msg);
        else if (!show_on) {
            small_text(4, 20, "A FIRE  B VOLLEY  L/R SHELL  UP/DN COLOR");
            small_text(4, 27, "START SHOW  SELECT HIDE  SELECT+START MENU");
        }
    }
    show_sprites();
}

// --- the night's show: the crate goes up, in the order it was made --------------
// You do not see whose firework is whose; that comes out at review time.

void nightshow_update(u8* back) {
    if (ns_idx < ncrate) {
        if (--ns_wait <= 0) {
            const Shell* s = &crate[ns_idx];
            int n = ncrate;
            int x = n == 1 ? 120 : 40 + (ns_idx * 160 / (n - 1));
            x += (int)(rnd() % 21) - 10;
            launch(x, s->pat, s->hue | ((s->hue2 + 1) << 4), s->size);
            ns_idx++;
            ns_wait = s->pat == P_WILLOW ? 150 : 110;
        }
    } else if (nparts == 0 && ++ns_end > 40) {
        set_scene(SC_REVIEW);
    }
    if (hit & KEY_START) ns_idx = ncrate;   // skip the rest of the launches
    sky_frame(back);
    char head[24];
    char* p = cat(head, "NIGHT ");
    p = cat_num(p, night);
    cat(p, " - THE SHOW");
    text_center(back, 4, head, UI_YELLOW);
    show_sprites();
}

// --- review time: every customer's comment pops up, then the night's rating -----

static int rv_t, rv_phase, rv_stars, rv_ngifts;
static int rv_start[MAXCRATE];         // frame (of rv_t) each bubble pops; -1 = not yet
static u8 rv_tier[MAXCRATE];
static char rv_said[MAXCRATE][40];      // comments run up to 36 characters
static char rv_lines[MAXCRATE][3][41];  // rv_said wrapped once, not every frame
static u8 rv_nlines[MAXCRATE];
static char rv_gifts[MAXCRATE][41];
static int shake;                      // frames of screen shake left
static u32 rv_drawn[2];                // what each page last showed
int last_stars;                        // the night's rating, 0..5

#define BUBBLE_GAP 45                  // frames between bubbles
#define STAR_GAP 26                    // frames between stars
#define STAR_T0 34                     // first star lands after the card drops in
#define STAR_X(k) (120 - 44 + (k) * 22)

// Judges the whole crate (paying out as it goes) and works out the star rating.
void review_begin(void) {
    REG_BLDCNT = 0;                    // no glow while the bubbles are up: they stay crisp
    sky_reset();
    nparts = 0;
    nbursts = 0;
    flash = 0;
    rv_t = 0;
    rv_drawn[0] = rv_drawn[1] = 0;
    rv_phase = ncrate ? 0 : 1;
    rv_ngifts = 0;
    static const u8 points[6] = { 10, 8, 8, 6, 3, 0 };   // by tier, out of 10
    int sum = 0;
    int tier0 = rep_tier();
    for (int i = 0; i < ncrate; i++) {
        char gift[48];
        gift[0] = 0;
        rv_tier[i] = judge_shell(&crate[i], rv_said[i], gift);
        rv_start[i] = 30 + i * BUBBLE_GAP;
        rv_nlines[i] = wrap(rv_said[i], 12, rv_lines[i], 3);
        sum += points[rv_tier[i]];
        if (gift[0]) cat(rv_gifts[rv_ngifts++], gift);
    }
    rv_stars = ncrate ? (sum + ncrate) / (2 * ncrate) : 0;   // rounded, 0..5
    // A grand finale: ending on a large firework, the biggest of the night.
    if (ncrate >= 3 && crate[ncrate - 1].size == S_LARGE && rv_ngifts < MAXCRATE) {
        coins += 20; night_coins += 20;
        rep += 1; night_rep += 1;
        for (int i = rv_ngifts; i > 0; i--) { rv_gifts[i][0] = 0; cat(rv_gifts[i], rv_gifts[i - 1]); }
        rv_gifts[0][0] = 0;
        cat(rv_gifts[0], "GRAND FINALE! +20C +1 REP");
        rv_ngifts++;
    }
    if (rep_tier() > tier0 && rv_ngifts < MAXCRATE) {        // the town knows your name now
        for (int i = rv_ngifts; i > 0; i--) { rv_gifts[i][0] = 0; cat(rv_gifts[i], rv_gifts[i - 1]); }
        char* p = cat(rv_gifts[0], "NEW TITLE: ");          // shown first
        cat(p, rep_titles[rep_tier()]);
        rv_ngifts++;
    }
    last_stars = rv_stars;
    if (!ncrate) REG_BLDCNT = (1 << 2) | (1 << 6) | (1 << 13);
}

// A ring of bright sparks flying out from the edge of a w x h box centered on (x, y).
static void spark_ring(int x, int y, int w, int h, int n, int speed, int hue) {
    for (int i = 0; i < n; i++) {
        int a = (i * 256 / n + (rnd() & 15)) & 255;
        int c = sin_tab[(a + 64) & 255], sn = sin_tab[a];
        int s = speed + (int)(rnd() % (speed / 2 + 1));
        int ex = x + (c * w >> 9), ey = y + (sn * h >> 9);
        add(ex << 6, ey << 6, s * c >> 8, s * sn >> 8, 14 + (rnd() & 7), hue, K_GLINT);
    }
}

static void bubble_rect(int i, int* x, int* y, int* w, int* h) {
    *w = 78;
    *h = 14 + 9 * rv_nlines[i];
    *x = 1 + (i % 3) * 80;
    *y = 15 + (i / 3) * 44 + (i * 5) % 4;
}

// A small speech bubble in a 3-wide grid over the town, tail pointing at the rooftops.
// age = frames since it popped: happy ones spring in and overshoot, unhappy ones shake.
static void draw_bubble(u8* back, int i, int age) {
    static const u16 spring[9] = { 50, 130, 210, 285, 305, 285, 262, 250, 256 };
    const Shell* s = &crate[i];
    int t = rv_tier[i];
    int bad = t >= T_NOPE;
    int x, y, w, h;
    bubble_rect(i, &x, &y, &w, &h);
    int scale = 256, dx = 0;
    if (!bad) scale = age < 9 ? spring[age] : 256;
    else {
        scale = age < 3 ? 100 + age * 70 : 256;
        if (age >= 3 && age < 18) dx = ((age >> 1) & 1 ? 3 : -3) * (18 - age) / 15;
    }
    int cw = w * scale >> 8, ch = h * scale >> 8;
    int bx = x + (w - cw) / 2 + dx, by = y + (h - ch) / 2;
    frame_rect(back, bx, by, cw, ch, 1, UI_INK);
    fill_rect(back, bx + 1, by + 1, cw - 2, ch - 2, UI_WHITE);
    if (age < 5) return;               // the words land once it has popped
    x += dx;
    fill_rect(back, x, y, 1, 1, 0);    // round the corners
    fill_rect(back, x + w - 1, y, 1, 1, 0);
    fill_rect(back, x, y + h - 1, 1, 1, 0);
    fill_rect(back, x + w - 1, y + h - 1, 1, 1, 0);
    int tx = x + 12 + (i * 23) % (w - 30);
    for (int r = 0; r < 5; r++) {      // tail
        fill_rect(back, tx, y + h - 1 + r, 6 - r + 1, 1, UI_INK);
        if (5 - r > 0) fill_rect(back, tx + 1, y + h - 1 + r, 5 - r, 1, UI_WHITE);
    }
    text_sh(back, x + 4, y + 3, cust_names[s->cust], UI_DK_GOLD, 0);
    const char* mood = t <= T_SURPRISE ? "*" : t == T_CLOSE ? "" : t == T_NOPE ? "..." : "!";
    text_sh(back, x + w - 4 - text_w(mood), y + 3, mood, t <= T_CLOSE ? UI_DK_RED : UI_INK, 0);
    for (int k = 0; k < rv_nlines[i]; k++) text_sh(back, x + 4, y + 12 + 9 * k, rv_lines[i][k], UI_INK, 0);
}

// One glyph drawn with square pixels of the given size, centered on (cx, cy).
static void glyph_big(u8* back, int cx, int cy, char c, int px, int color) {
    const u8* g = glyph_rows(c);
    if (!g) return;
    int x0 = cx - 5 * px / 2, y0 = cy - 7 * px / 2;
    for (int r = 0; r < 7; r++)
        for (int b = 0; b < 5; b++)
            if (g[r] & (16 >> b)) fill_rect(back, x0 + b * px, y0 + r * px, px, px, color);
}

static int count_up(int v, int t) {    // a number that rolls up from 0 over 30 frames
    if (t <= 0) return 0;
    return t >= 30 ? v : v * t / 30;
}

static void draw_card(u8* back) {
    int t = rv_t;
    // The card drops in from above and settles with a small bounce.
    static const s8 drop[12] = { -126, -96, -68, -44, -24, -8, 3, 6, 4, 2, 1, 0 };
    int ng = rv_ngifts < 3 ? rv_ngifts : 3;
    int cx = 30, cy = 14 + (t < 12 ? drop[t] : 0), cw = 180, chh = 106 + 9 * ng;
    box(back, cx, cy, cw, chh);
    hide_box(cx, cy, cw, chh);
    win_on = 1;                        // the card stays solid while the sky glows
    win_x0 = cx; win_y0 = cy < 0 ? 0 : cy; win_x1 = cx + cw; win_y1 = cy + chh;
    char buf[48], *p = cat(buf, "NIGHT ");
    p = cat_num(p, night);
    cat(p, " RATING");
    text_center(back, cy + 7, buf, UI_YELLOW);

    // Stars: dim outlines first, then each earned star slams down, big to small.
    int sy = cy + 30;
    for (int k = 0; k < 5; k++) {
        int land = STAR_T0 + k * STAR_GAP, age = t - land;
        if (k < rv_stars && age >= -6) {
            int px = age < 0 ? 3 - age : 3;         // 9, 8, .. 4, then lands at 3
            glyph_big(back, STAR_X(k), sy, '#', px, UI_YELLOW);
            if (age >= 0 && age < 10) {                 // a ring of sparkles flies out as it lands
                int r = 8 + age * 2;
                for (int d = 0; d < 8; d++) {
                    int dx = sin_tab[(d * 32 + 64) & 255] * r >> 8, dy = sin_tab[d * 32] * r >> 8;
                    fill_rect(back, STAR_X(k) + dx, sy + dy, age < 6 ? 2 : 1, age < 6 ? 2 : 1, UI_YELLOW);
                }
            }
        } else if (t > 14) glyph_big(back, STAR_X(k), sy, '#', 3, UI_GRAY);
    }

    static const char* const verdicts[6] = {
        "A NIGHT TO FORGET...", "A NIGHT TO FORGET...", "A ROUGH NIGHT...", "A NICE NIGHT.",
        "A GREAT NIGHT!", "A MAGICAL NIGHT!",
    };
    int tv = STAR_T0 + rv_stars * STAR_GAP + 10;    // the verdict pops after the last star
    int y = cy + 50;
    if (t >= tv) {
        int hop = t - tv < 6 ? (6 - (t - tv)) : 0;   // springs up into place
        text_center(back, y - hop, ncrate ? verdicts[rv_stars] : "NO SHOW TONIGHT.", UI_WHITE);
    }
    // Then the numbers roll in, line by line.
    int tl = tv + 16;
    y += 14;
    if (t >= tl) {
        p = cat(buf, "HAPPY CUSTOMERS  ");
        p = cat_num(p, count_up(night_happy, t - tl));
        p = cat(p, " / ");
        cat_num(p, ncrate);
        text(back, cx + 12, y, buf, UI_WHITE);
    }
    y += 10;
    tl += 12;
    if (t >= tl) {
        p = cat(buf, "COINS  ");
        if (night_coins >= 0) p = cat(p, "+");
        p = cat_num(p, count_up(night_coins, t - tl));
        p = cat(p, "  (TOTAL ");
        p = cat_num(p, coins);
        cat(p, ")");
        text(back, cx + 12, y, buf, UI_WHITE);
    }
    y += 10;
    tl += 12;
    if (t >= tl) {
        p = cat(buf, "REP    ");
        if (night_rep >= 0) p = cat(p, "+");
        p = cat_num(p, count_up(night_rep, t - tl));
        p = cat(p, "  (TOTAL ");
        p = cat_num(p, rep);
        cat(p, ")");
        text(back, cx + 12, y, buf, night_rep < 0 ? UI_RED : UI_WHITE);
    }
    y += 12;
    for (int i = 0; i < rv_ngifts && i < 3; i++) {
        tl += 12;
        if (t >= tl) {
            char lines[1][41];
            if (wrap(rv_gifts[i], 26, lines, 1)) text(back, cx + 12, y, lines[0], UI_GREEN);
        }
        y += 9;
    }
    if (t > tl + 20 && (frame & 16)) text_center(back, y + 2, "A: NEXT DAY", UI_YELLOW);
}

static int card_done(void) {
    return rv_t > STAR_T0 + rv_stars * STAR_GAP + 10 + 16 + 24 + rv_ngifts * 12 + 20;
}

void review_update(u8* back) {
    rv_t++;
    if (rv_phase == 0) {
        int last = ncrate ? rv_start[ncrate - 1] : 0;
        if (hit & KEY_A) {
            if (rv_t < last) {
                // Hurry: the rest pop in quick succession.
                for (int i = 0, k = 0; i < ncrate; i++)
                    if (rv_start[i] > rv_t) rv_start[i] = rv_t + 1 + 6 * k++;
            } else if (rv_t > last + 10) {
                rv_phase = 1;
                rv_t = 0;
                nparts = 0;
                REG_BLDCNT = (1 << 2) | (1 << 6) | (1 << 13);   // glow back on for the celebration
            }
        }
        for (int i = 0; i < ncrate; i++)
            if (rv_t == rv_start[i]) {
                int x, y, w, h;
                bubble_rect(i, &x, &y, &w, &h);
                int bad = rv_tier[i] >= T_NOPE;
                spark_ring(x + w / 2, y + h / 2, w, h, bad ? 10 : 24, bad ? 450 : 700,
                           bad ? H_SILVER : rv_tier[i] <= T_SURPRISE ? H_PINK : H_YELLOW);
                if (bad) shake = 6;
            }
    } else {
        if (hit & KEY_A) {
            if (!card_done()) rv_t = 1000;          // skip to the end of the animation
            else { next_night(); set_scene(SC_DAY); return; }
        }
        for (int k = 0; k < rv_stars; k++)
            if (rv_t == STAR_T0 + k * STAR_GAP) {   // a star lands
                shake = 5;
                flash = 8 + k * 2;
                flash_hue = H_GOLD;
            }
        // Four or five stars: fireworks go up on both sides of the card.
        int tv = STAR_T0 + rv_stars * STAR_GAP + 10;
        if (rv_stars >= 4 && rv_t >= tv && rv_t < tv + 160 && (rv_t - tv) % 40 == 0) {
            int side = ((rv_t - tv) / 40) & 1;
            launch(side ? 214 : 26, rnd() % P_COUNT, rv_stars == 5 ? H_GOLD : rnd() % NCOLORS, 0);
        }
    }
    if (shake) shake--;

    // Skip redrawing a page that already shows this frame: nothing moving, no sparks.
    int moving = nparts > 0 || shake;
    if (rv_phase == 0)
        for (int i = 0; i < ncrate; i++) moving |= rv_t >= rv_start[i] && rv_t - rv_start[i] < 20;
    else moving |= !card_done();
    u32 sig = moving ? 0xFFFFFFFF - rv_t : (u32)(rv_phase * 4 + ((frame >> 4) & 1) + 1);
    int pi = back == PAGE1;
    if (rv_drawn[pi] == sig) { show_sprites(); return; }
    rv_drawn[pi] = sig;

    clear_page(back);
    hide_n = 0;
    win_on = 0;
    // Sparks and fireworks first; bubbles and the card sit on top of them.
    step_draw(back);
    for (int i = 0; i < nbursts; i++) explode(&bursts[i]);
    nbursts = 0;
    if (flash) flash--;
    if (rv_phase == 0) {
        text_center(back, 4, "REVIEW TIME", UI_YELLOW);
        for (int i = 0; i < ncrate; i++)
            if (rv_t >= rv_start[i]) {
                int x, y, w, h;
                bubble_rect(i, &x, &y, &w, &h);
                draw_bubble(back, i, rv_t - rv_start[i]);
                hide_box(x - 4, y, w + 8, h + 6);
            }
        if (ncrate && rv_t > rv_start[ncrate - 1] + 10 && (frame & 16))
            text_center(back, 150, "A: SEE THE RATING", UI_WHITE);
    } else draw_card(back);
    show_sprites();
}

int review_shake(void) {               // screen offset for this frame's shake
    return shake ? ((shake & 1) ? 2 : -2) : 0;
}

// --- intro: Grandma hands the shop to her grandson ---------------------------------

static const char* const intro_pages[] = {
    "THERE YOU ARE, SWEETHEART! COME HERE, LET ME GET A GOOD LOOK AT YOU.",
    "FORTY YEARS I'VE RUN THIS LITTLE FIREWORK SHOP. NOW IT'S YOURS. THE TOWN NEEDS ITS FIREWORKS.",
    "EACH NIGHT, YOU OPEN UP. FOLKS COME IN AND TELL YOU WHAT THEY'D LOVE TO SEE IN THE SKY.",
    "YOU BUILD EACH ONE AT THE COUNTER: A SIZE, A PATTERN, A COLOR. (D-PAD TO PICK, A TO CHOOSE, B TO GO BACK.)",
    "FANCY ONES TAKE LONGER, AND YOU CLOSE AT 1 AM. YOU CAN'T MAKE EVERYTHING, SO CHOOSE WISELY.",
    "AT CLOSING TIME, IT ALL GOES UP IN ONE BIG SHOW. AFTERWARD, EVERYONE TELLS YOU WHAT THEY THOUGHT.",
    "A LITTLE SECRET: SOME FOLKS LOVE THINGS THEY NEVER ASK FOR. KEEP YOUR EYES OPEN, DEAR.",
    "BY DAY, WALK AROUND TOWN FOR POWDER, SHELLS, AND COLORED STARS. (START OPENS YOUR BAG.)",
    "NOW GO FLIP THAT SIGN TO OPEN. I'LL BE WATCHING THE SKY. DON'T LET IT GO DARK, DEAR!",
};
#define INTRO_N (int)(sizeof intro_pages / sizeof intro_pages[0])
static int in_page, in_shown, in_wait;
static char in_lines[4][41];
static int in_n, in_len;

static void intro_page(int i) {
    in_page = i;
    in_shown = 0;
    in_n = wrap(intro_pages[i], 37, in_lines, 3);
    in_len = 0;
    while (intro_pages[i][in_len]) in_len++;
}

#define T_GRANDMA 816                  // one customer body (the bun), 32x64

void intro_begin(void) {
    intro_page(0);
    in_wait = 40;
    // Grandma: the bun-haired customer body in her own colors (OBJ palette bank 4).
    vu32* t = (vu32*)OBJ_TILES;
    for (int i = 0; i < 32 * 8; i++) t[T_GRANDMA * 8 + i] = cust_tiles[2 * 32 * 8 + i];
    static const u16 cols[13] = {
        0, RGB(5, 3, 5), RGB(30, 24, 19), RGB(25, 18, 13), RGB(27, 27, 28), RGB(20, 20, 22),
        RGB(17, 9, 22), RGB(12, 6, 16), RGB(4, 3, 6), RGB(31, 16, 17), RGB(17, 9, 22), RGB(12, 6, 16), RGB(31, 31, 31),
    };
    for (int i = 1; i < 13; i++) OBJ_PAL[64 + i] = cols[i];
}

void intro_update(u8* back) {
    if (in_shown < in_len) in_shown += 2;
    if (hit & KEY_START) { set_scene(SC_SHOP); return; }      // skip the intro
    if (hit & KEY_A) {
        if (in_shown < in_len) in_shown = in_len;
        else if (in_page + 1 < INTRO_N) intro_page(in_page + 1);
        else { set_scene(SC_SHOP); return; }
    }
    if (--in_wait <= 0) {                                      // a quiet firework now and then
        launch(40 + rnd() % 160, rnd() % 5, rnd() % NCOLORS, rnd() % 2);
        in_wait = 120 + rnd() % 100;
    }
    sky_frame(back);
    box(back, 0, 116, 240, 44);
    win_on = 1;                        // the box stays solid while the sky glows
    win_x0 = 0; win_y0 = 116; win_x1 = 240; win_y1 = 160;
    text(back, 8, 121, "GRANDMA", UI_YELLOW);
    text_typed(back, 8, 130, in_lines, in_n, in_shown, UI_WHITE);
    if (in_shown >= in_len && (frame & 16)) fill_rect(back, 226, 150, 5, 3, UI_YELLOW);
    sprite(10, 116 - 64, T_GRANDMA, 2, 3, 4);
    show_sprites();
}
