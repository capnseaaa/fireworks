// Game-wide state and the functions the scenes share.
#pragma once
#include "fw.h"
#include "gba.h"

enum { P_PEONY, P_CHRYS, P_WILLOW, P_PALM, P_RING, P_CROSS, P_HEART, P_CRACKLE, P_GLITTER, P_COUNT };
#define NCOLORS 12
enum { S_SMALL, S_MEDIUM, S_LARGE };
extern const char* const pat_names[P_COUNT];
extern const char* const pat_plural[P_COUNT];
extern const char* const hue_names[NCOLORS];
extern const u8 hue_rgb[15][3];
extern const char* const size_names[3];
extern const u8 size_time[3], pat_time[P_COUNT], size_powder[3], size_pay[3];

// --- main.c: frame, input, sprites, scenes ---
extern volatile u32 frame;
extern u16 keys, hit;            // held, newly pressed
enum { SC_TITLE, SC_FREE, SC_SHOP, SC_SHOW, SC_REVIEW };
extern int scene;
void set_scene(int s);            // switches at the start of the next frame
void blank_once(void);            // hide the screen until the next drawn frame (scene loads)
// shape: 0 square, 1 wide, 2 tall
void sprite(int x, int y, int tile, int shape, int size, int pal);

// --- ui.c: drawing into a mode 4 page (palette 1..15) ---
enum { UI_WHITE = 1, UI_SHADOW, UI_FILL, UI_GOLD, UI_GOLD_DK, UI_GRAY, UI_RED, UI_GREEN, UI_PINK,
       UI_YELLOW, UI_SEL, UI_DK_GREEN, UI_DK_RED, UI_INK, UI_DK_GOLD };   // dark ones: text on bubbles
void ui_init(void);
void ui_palette(void);
void text(u8* page, int x, int y, const char* s, int color);
void text_sh(u8* page, int x, int y, const char* s, int color, int shadow);   // shadow 0 = none
void text_big(u8* page, int x, int y, const char* s, int color);   // 2x size
int text_w(const char* s);        // width in pixels at 1x
const u8* glyph_rows(char c);     // 7 rows of 5 bits, or 0
void box(u8* page, int x, int y, int w, int h);
void frame_rect(u8* page, int x, int y, int w, int h, int t, int c);
int wrap(const char* s, int maxchars, char out[][41], int maxlines);
char* cat(char* d, const char* s);
char* cat_num(char* d, int v);

// --- render.c (ARM, IWRAM) ---
__attribute__((long_call)) void copy_page(u8* dst, const u8* src);
__attribute__((long_call)) void fill_rect(u8* page, int x, int y, int w, int h, int c);
__attribute__((long_call)) void draw_glyph(u8* page, int x, int y, const u8* rows, int c, int shadow);

// --- show.c: fireworks scenes ---
void sky_init(void);
void sky_reset(void);              // plain night sky, no burst light
void show_enter(void);
void show_vblank(void);
void show_sprites(void);
void free_update(u8* back);
void title_update(u8* back);
void nightshow_update(u8* back);
void review_begin(void);
void review_update(u8* back);
extern int last_stars;
extern u32 cpu_ticks;

// --- shop.c ---
void shop_enter(void);
void shop_update(u8* back);

// --- game state (shop.c) ---
// A firework in tonight's crate, with what its customer asked for and secretly likes.
typedef struct {
    u8 pat, hue, size, cust;      // cust = index into cust_names
    u16 want_pats;                // acceptable patterns (0 = any)
    s8 want_hue, want_size;       // -1 = any
    u8 like_type, like_val;       // hidden like: 0 none, 1 color, 2 pattern, 3 size
} Shell;
#define MAXCRATE 14
extern Shell crate[MAXCRATE];
extern int ncrate;
extern int night, coins, rep, powder, shells;
extern int stock[NCOLORS];
extern u16 known;                 // bit per pattern the player can make
extern int night_made, night_happy, night_coins, night_rep;
extern const char* const cust_names[];
void new_game(void);
void next_night(void);
enum { T_LOVE, T_HAPPY, T_SURPRISE, T_CLOSE, T_NOPE, T_REFUND };
int judge_shell(const Shell* s, char* comment, char* gift);
