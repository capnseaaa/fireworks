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
enum { SC_TITLE, SC_FREE, SC_SHOP, SC_SHOW, SC_REVIEW, SC_DAY, SC_INTRO, SC_FIELD, SC_DEV };
extern int scene;
void set_scene(int s);            // switches at the start of the next frame
void blank_once(void);            // hide the screen until the next drawn frame (scene loads)
// shape: 0 square, 1 wide, 2 tall
void sprite(int x, int y, int tile, int shape, int size, int pal);
void sprite_flip_last(void);     // mirror the sprite just added

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
void text_typed(u8* page, int x, int y, char lines[][41], int n, int shown, int color);
char* cat(char* d, const char* s);
char* cat_num(char* d, int v);

// --- sound.c: sound effects (audio.h names them) and music ---
#include "audio.h"
void sound_init(void);
void sound_frame(void);           // once a game loop: starts delayed sounds
void sfx(int id);
void sfx_ex(int id, int vol, int pitch);          // vol 0..64, pitch 256 = normal
void sfx_later(int id, int vol, int pitch, int frames);
void sfx_stop_all(void);
void music(int song);             // keeps playing if it already is; -1 = silence
void music_toggle(void);
void settings_load(void);         // OPTIONS, kept in save memory apart from the game
void settings_save(void);
extern int music_off, sfx_off;

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
void intro_begin(void);
void intro_update(u8* back);
void review_update(u8* back);
extern int last_stars;
extern u32 cpu_ticks;

// --- day.c: the town map ---
enum { TOOL_PICK = 1, TOOL_NET = 2, TOOL_ROPE = 4 };
extern int tools;
void day_enter(void);
void day_update(u8* back);
void day_return(void);            // back to the map from walking an area, same day
char* day_clock(char* p, int minutes);
extern int day_min;
#define DAY_END_MIN 480           // 6 PM (the day starts at 10 AM)

// --- field.c: walking an area and gathering ---
enum { GEAR_AXE, GEAR_BAG, GEAR_PICK, GEAR_LANTERN, GEAR_COUNT };   // tiers; pickaxe/lantern 0 = none
extern u8 gear[GEAR_COUNT];       // tier 1..3
extern int bag_used;              // items picked up today
int bag_size(void);
enum { FIELD_FOREST, FIELD_MINE, FIELD_GEODES, FIELD_BEACH, FIELD_QUARRY, FIELD_CLIMB };
#define MINE_FLOORS 10
#define MINE_DARK 5                // floors from here down need a lantern
extern int mine_floor, mine_deepest;
void field_go(int area);
int geodes_done(void);
int climb_done(void);
void field_enter(void);
void field_update(u8* back);

// --- events.c: Grandma's visits and letters, festival nights ---
enum { EV_NONE, EV_VISIT, EV_LETTER };
int day_event(int day);           // what the morning brings
const char* visit_text(int day);
const char* letter_text(int day);
void visit_gift(int day, char* out);   // gives it, and says what it was
int festival_night(int night);    // 0, or which festival (festival_names)
extern const char* const festival_names[5];

// --- dev.c: L+R+B test menu ---
int dev_wanted(void);
void dev_open(void);
void dev_enter(void);
void dev_update(u8* back);

// --- shop.c ---
void shop_enter(void);
void shop_update(u8* back);

// --- game state (shop.c) ---
// A firework in tonight's crate, with what its customer asked for and secretly likes.
typedef struct {
    u8 pat, hue, size, cust;      // cust = index into cust_names
    s8 hue2;                      // the second color, -1 = none
    u16 want_pats;                // acceptable patterns (0 = any)
    s8 want_hue, want_size;       // -1 = any
    s8 want_hue2;                 // a second color they asked for, -1 = none
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
// Shop upgrades, bought at home during the day.
enum { UPG_BENCH, UPG_HOURS, UPG_SIGN, UPG_SHELF, UPG_COUNT };
typedef struct { const char* name; const char* desc; u8 max; u16 price[3]; u8 need[3]; } Upgrade;
extern const Upgrade upgrades[UPG_COUNT];
extern u8 upg[UPG_COUNT];        // level of each
int rep_tier(void);               // 0..3, from reputation
extern const char* const rep_titles[4];
int close_min(void);              // closing time, minutes after 8 PM
int unlock_pattern(void);         // teaches a random unknown pattern; -1 if none left
int unlock_count(void);           // patterns known
enum { SAVE_NIGHT, SAVE_DAY };
void save_game(int phase);
int has_save(void);
int load_game(void);              // the phase to resume, or -1
enum { T_LOVE, T_HAPPY, T_SURPRISE, T_CLOSE, T_NOPE, T_REFUND };
int judge_shell(const Shell* s, char* comment, char* gift);
