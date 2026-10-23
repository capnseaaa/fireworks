// The day: walk the town map from 8 AM to 6 PM, gathering materials and buying
// supplies for tonight. Every trip and every job costs time; each spot works once a day,
// and some spots need a tool from the market first.
#include "game.h"
#include "art.h"

#define T_MAPSPR 512                   // arrow, player (16x16 each, 4 tiles)
#define DAY_END 600                    // minutes after 8:00 AM: 6:00 PM
#define TRAVEL 20                      // minutes to walk anywhere

int tools;                             // TOOL_* bits

enum { A_HOME, A_MARKET, A_FOREST, A_QUARRY, A_MINE, A_BEACH, A_COUNT };

typedef struct { const char* name; const char* desc; u8 x, y, lx, ly; } Area;   // lx, ly: label
static const Area areas[A_COUNT] = {
    { "HOME", "YOUR LITTLE FIREWORK SHOP. OPEN IT WHEN YOU'RE READY FOR THE NIGHT.", 124, 70, 124, 81 },
    { "MARKET", "STALLS SELLING POWDER, SHELLS, COLORED STARS, AND TOOLS.", 50, 88, 50, 104 },
    { "FOREST", "WOOD FOR SHELLS, CHARCOAL FOR POWDER. THINGS GROW IN THE GROVE.", 34, 42, 30, 70 },
    { "QUARRY", "AN OLD STONE PIT. RARE WHITE AND SILVER STONE, IF YOU CAN CLIMB DOWN.", 96, 30, 96, 45 },
    { "MINE", "SULFUR NEAR THE ENTRANCE. COLORFUL ORE DEEP INSIDE.", 204, 32, 226, 56 },
    { "BEACH", "SALT AND SEA GLASS IN THE SAND. DRIFTWOOD. TIDE POOLS.", 206, 100, 214, 88 },
};

// What you can do at each spot. kind picks the result in do_spot().
enum { K_OPEN, K_WOOD, K_CHARCOAL, K_GROVE, K_SULFUR, K_TUNNEL, K_SAND, K_DRIFT, K_POOLS,
       K_CLIMB, K_RUINS, K_BUY_POWDER, K_BUY_SHELLS, K_BUY_STAR, K_BUY_PICK, K_BUY_NET, K_BUY_ROPE };
typedef struct { u8 area, kind, minutes, needs, price; const char* name; const char* desc; } Spot;
static const Spot spots[] = {
    { A_HOME, K_OPEN, 0, 0, 0, "OPEN THE SHOP", "END THE DAY AND OPEN FOR THE NIGHT." },
    { A_FOREST, K_WOOD, 60, 0, 0, "CHOP WOOD", "CARVE WOODEN SHELLS. ABOUT 2 SHELLS." },
    { A_FOREST, K_CHARCOAL, 60, 0, 0, "BURN CHARCOAL", "CHARCOAL FOR POWDER. ABOUT 3 POWDER." },
    { A_FOREST, K_GROVE, 40, 0, 0, "SEARCH THE GROVE", "GREEN THINGS GROW HERE. MAYBE MORE." },
    { A_MINE, K_SULFUR, 60, 0, 0, "DIG SULFUR", "SULFUR FOR POWDER. SOMETIMES RED ORE." },
    { A_MINE, K_TUNNEL, 90, TOOL_PICK, 0, "DEEP TUNNEL", "BLUE, CYAN, AND VIOLET ORE. NEEDS A PICKAXE." },
    { A_BEACH, K_SAND, 45, 0, 0, "COMB THE SAND", "SALT AND SEA GLASS: YELLOW, ORANGE, GOLD." },
    { A_BEACH, K_DRIFT, 45, 0, 0, "DRIFTWOOD", "WASHED-UP WOOD FOR SHELLS." },
    { A_BEACH, K_POOLS, 60, TOOL_NET, 0, "TIDE POOLS", "PINK AND CYAN SHELLS, BOTTLES. NEEDS A NET." },
    { A_QUARRY, K_CLIMB, 90, TOOL_ROPE, 0, "CLIMB DOWN", "WHITE AND SILVER STONE. NEEDS A ROPE." },
    { A_QUARRY, K_RUINS, 60, TOOL_ROPE, 0, "OLD WORKSHOP", "A RUINED FIREWORK WORKSHOP. NEEDS A ROPE." },
    { A_MARKET, K_BUY_POWDER, 10, 0, 12, "3 POWDER", "THREE SCOOPS OF BLACK POWDER." },
    { A_MARKET, K_BUY_SHELLS, 10, 0, 15, "2 SHELLS", "TWO PAPER SHELLS, READY TO FILL." },
    { A_MARKET, K_BUY_STAR, 10, 0, 8, "1 STAR", "ONE COLORED STAR. PICK THE COLOR." },
    { A_MARKET, K_BUY_PICK, 10, 0, 60, "PICKAXE", "OPENS THE MINE'S DEEP TUNNEL." },
    { A_MARKET, K_BUY_NET, 10, 0, 50, "NET", "OPENS THE BEACH'S TIDE POOLS." },
    { A_MARKET, K_BUY_ROPE, 10, 0, 80, "ROPE", "OPENS THE QUARRY." },
};
#define NSPOTS (int)(sizeof spots / sizeof spots[0])

enum { DS_INTRO, DS_MAP, DS_AREA, DS_COLOR, DS_WORK, DS_RESULT, DS_DUSK };

int day_st;                            // state (non-static so test scripts can read it)
int day_min;                           // minutes since 8:00 AM
static int st_t, here, sel, cursor, bag_open;
static int work_spot, work_from;
static u32 used;                       // spots done today (bit per spot)
static char msg[160];
static u32 page_sig[2];

static int menu_n, menu_spot[10];
static int area_menu(void) {           // the spots at the current area
    menu_n = 0;
    for (int i = 0; i < NSPOTS; i++)
        if (spots[i].area == here) menu_spot[menu_n++] = i;
    menu_spot[menu_n++] = -1;          // LEAVE
    return menu_n;
}

static int is_tool(int k) { return k == K_BUY_PICK || k == K_BUY_NET || k == K_BUY_ROPE; }
static int tool_bit(int k) { return k == K_BUY_PICK ? TOOL_PICK : k == K_BUY_NET ? TOOL_NET : TOOL_ROPE; }

// Why a spot cannot be used right now, or 0.
static const char* blocked(int i) {
    const Spot* s = &spots[i];
    if (s->kind == K_OPEN) return 0;
    if (s->needs && !(tools & s->needs))
        return s->needs == TOOL_PICK ? "NEEDS A PICKAXE" : s->needs == TOOL_NET ? "NEEDS A NET" : "NEEDS A ROPE";
    if (is_tool(s->kind) && (tools & tool_bit(s->kind))) return "YOU HAVE ONE";
    if (s->price && coins < s->price) return "NOT ENOUGH COINS";
    if (!s->price && (used & (1u << i))) return "DONE FOR TODAY";
    if (day_min + s->minutes > DAY_END) return "NOT ENOUGH DAYLIGHT";
    return 0;
}

static char* time_str(char* p, int m) {
    int h = 8 + m / 60, mm = m % 60;
    int pm = h >= 12;
    int hh = h % 12;
    if (hh == 0) hh = 12;
    p = cat_num(p, hh);
    *p++ = ':';
    *p++ = '0' + mm / 10;
    *p++ = '0' + mm % 10;
    return cat(p, pm ? " PM" : " AM");
}

// --- results ---------------------------------------------------------------------

static char* add_stars(char* p, const u8* pool, int npool, int n) {
    for (int i = 0; i < n; i++) {
        int h = pool[rnd() % npool];
        stock[h]++;
        if (i) p = cat(p, ", ");
        p = cat(p, "1 ");
        p = cat(p, hue_names[h]);
    }
    return cat(p, n == 1 ? " STAR" : " STARS");
}

static char* maybe_scroll(char* p, int chance) {
    if ((int)(rnd() % 100) >= chance) return p;
    int pat = unlock_pattern();
    if (pat < 0) return p;
    p = cat(p, "\nAND A SCROLL! YOU CAN NOW MAKE ");
    p = cat(p, pat_plural[pat]);
    return cat(p, "!");
}

static void do_spot(int i) {
    static const u8 greens[] = { H_GREEN, H_LIME }, reds[] = { H_RED }, deep[] = { H_BLUE, H_CYAN, H_VIOLET, H_GREEN };
    static const u8 warm[] = { H_YELLOW, H_ORANGE, H_GOLD }, pools[] = { H_PINK, H_CYAN, H_BLUE };
    static const u8 pale[] = { H_WHITE, H_SILVER }, odd[] = { H_PINK, H_VIOLET, H_SILVER };
    char* p = cat(msg, "YOU GOT ");
    int n;
    switch (spots[i].kind) {
    case K_WOOD:
        n = 2 + (rnd() % 3 == 0);
        shells += n;
        p = cat_num(p, n);
        cat(p, " SHELLS.");
        break;
    case K_CHARCOAL:
        n = 3 + (rnd() & 1);
        powder += n;
        p = cat_num(p, n);
        cat(p, " POWDER.");
        break;
    case K_GROVE:
        p = add_stars(p, greens, 2, 1 + (rnd() & 1));
        p = cat(p, ".");
        maybe_scroll(p, 15);
        break;
    case K_SULFUR:
        n = 3;
        powder += n;
        p = cat(p, "3 POWDER");
        if (rnd() % 100 < 40) { p = cat(p, " AND "); p = add_stars(p, reds, 1, 1); }
        cat(p, ".");
        break;
    case K_TUNNEL:
        p = add_stars(p, deep, 4, 2 + (rnd() & 1));
        p = cat(p, ".");
        maybe_scroll(p, 10);
        break;
    case K_SAND:
        p = add_stars(p, warm, 3, 2);
        cat(p, ".");
        break;
    case K_DRIFT:
        n = 1 + (rnd() & 1);
        shells += n;
        p = cat_num(p, n);
        cat(p, n == 1 ? " SHELL." : " SHELLS.");
        break;
    case K_POOLS:
        p = add_stars(p, pools, 3, 1 + (rnd() & 1));
        p = cat(p, ".");
        maybe_scroll(p, 25);
        break;
    case K_CLIMB:
        p = add_stars(p, pale, 2, 2);
        p = cat(p, ".");
        maybe_scroll(p, 20);
        break;
    case K_RUINS:
        if (rnd() % 100 < 40 && unlock_count() < P_COUNT) { cat(msg, "NOTHING BUT DUST..."); maybe_scroll(msg + 18, 100); }
        else { p = add_stars(p, odd, 3, 1); cat(p, "."); }
        break;
    case K_BUY_POWDER: powder += 3; cat(msg, "YOU BOUGHT 3 POWDER."); break;
    case K_BUY_SHELLS: shells += 2; cat(msg, "YOU BOUGHT 2 SHELLS."); break;
    case K_BUY_PICK: tools |= TOOL_PICK; cat(msg, "YOU BOUGHT A PICKAXE! THE MINE'S DEEP TUNNEL IS OPEN."); break;
    case K_BUY_NET: tools |= TOOL_NET; cat(msg, "YOU BOUGHT A NET! THE TIDE POOLS ARE OPEN."); break;
    case K_BUY_ROPE: tools |= TOOL_ROPE; cat(msg, "YOU BOUGHT A ROPE! THE QUARRY IS OPEN."); break;
    }
}

// --- scene -------------------------------------------------------------------------

static void go(int s) { day_st = s; st_t = 0; }

static void dusk(void) {
    msg[0] = 0;
    cat(msg, "THE SUN IS SETTING. TIME TO HEAD HOME AND OPEN THE SHOP!");
    here = A_HOME;
    go(DS_DUSK);
}

void day_enter(void) {
    blank_once();
    for (int i = 0; i < MAP_PAL_N; i++) BG_PAL[16 + i] = map_pal[i];
    ui_palette();
    REG_BLDCNT = 0;
    REG_DISPCNT &= ~(1 << 13);
    vu32* t = (vu32*)OBJ_TILES;
    for (int i = 0; i < 2 * 4 * 8; i++) t[T_MAPSPR * 8 + i] = map_tiles[i];
    OBJ_PAL[1] = RGB(4, 3, 6);
    OBJ_PAL[2] = RGB(31, 25, 6);
    OBJ_PAL[3] = RGB(31, 31, 31);
    OBJ_PAL[4] = RGB(31, 24, 18);
    OBJ_PAL[5] = RGB(8, 14, 28);
    day_min = 0;
    here = A_HOME;
    sel = A_HOME;
    used = 0;
    bag_open = 0;
    page_sig[0] = page_sig[1] = 0;
    char* p = cat(msg, "DAY ");
    p = cat_num(p, night);
    p = cat(p, ". GATHER MATERIALS FOR TONIGHT!");
    if (night == 2) cat(p, "\nEVERY TRIP TAKES TIME. THE SUN SETS AT 6:00 PM.");
    go(DS_INTRO);
}

// Picks the area in the pressed direction, nearest first.
static int area_toward(int dx, int dy) {
    int best = sel, bd = 1 << 30;
    for (int i = 0; i < A_COUNT; i++) {
        if (i == sel) continue;
        int vx = areas[i].x - areas[sel].x, vy = areas[i].y - areas[sel].y;
        int along = vx * dx + vy * dy;
        if (along <= 0) continue;
        int side = vx * dy - vy * dx;
        if (side < 0) side = -side;
        int d = along + side * 3;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

static void step(void) {
    st_t++;
    int a = hit & KEY_A, b = hit & KEY_B;
    if (hit & KEY_START) bag_open ^= 1;
    if (bag_open) { if (a || b) bag_open = 0; return; }
    switch (day_st) {
    case DS_INTRO:
        if (a) go(DS_MAP);
        break;
    case DS_MAP:
        if (hit & KEY_LEFT) sel = area_toward(-1, 0);
        if (hit & KEY_RIGHT) sel = area_toward(1, 0);
        if (hit & KEY_UP) sel = area_toward(0, -1);
        if (hit & KEY_DOWN) sel = area_toward(0, 1);
        if (a) {
            // Walking always works; if it takes you past sunset, the day ends.
            day_min += sel == here ? 0 : TRAVEL;
            here = sel;
            if (day_min >= DAY_END) { day_min = DAY_END; dusk(); break; }
            area_menu();
            cursor = 0;
            go(DS_AREA);
        }
        break;
    case DS_AREA: {
        area_menu();
        if (hit & KEY_UP) cursor = (cursor + menu_n - 1) % menu_n;
        if (hit & KEY_DOWN) cursor = (cursor + 1) % menu_n;
        if (b) { go(DS_MAP); break; }
        if (!a) break;
        int i = menu_spot[cursor];
        if (i < 0) { go(DS_MAP); break; }
        if (blocked(i)) break;
        if (spots[i].kind == K_OPEN) { set_scene(SC_SHOP); break; }
        if (spots[i].kind == K_BUY_STAR) { go(DS_COLOR); cursor = 0; break; }
        work_spot = i;
        work_from = day_min;
        go(DS_WORK);
        break;
    }
    case DS_COLOR:
        if (hit & KEY_UP) cursor = (cursor + NCOLORS - 1) % NCOLORS;
        if (hit & KEY_DOWN) cursor = (cursor + 1) % NCOLORS;
        if (b) { area_menu(); cursor = 0; go(DS_AREA); break; }
        if (a && coins >= 8) {
            coins -= 8;
            stock[cursor]++;
            day_min += 2;
            if (day_min > DAY_END) day_min = DAY_END;
        }
        break;
    case DS_WORK: {
        const Spot* s = &spots[work_spot];
        int len = s->price ? 20 : 60;
        day_min = work_from + s->minutes * st_t / len;
        if (st_t >= len) {
            day_min = work_from + s->minutes;
            if (s->price) coins -= s->price;
            else used |= 1u << work_spot;
            msg[0] = 0;
            do_spot(work_spot);
            go(DS_RESULT);
        }
        break;
    }
    case DS_RESULT:
        if (a) {
            if (day_min >= DAY_END - 15) dusk();
            else {
                int keep = cursor;
                area_menu();
                cursor = keep < menu_n ? keep : 0;
                go(DS_AREA);
            }
        }
        break;
    case DS_DUSK:
        if (a) set_scene(SC_SHOP);
        break;
    }
}

// --- drawing -------------------------------------------------------------------------

static void draw_lines(u8* back, const char* s, int y, int maxlines, int color) {
    char lines[5][41];
    int n = wrap(s, 37, lines, maxlines);
    for (int i = 0; i < n; i++) text(back, 8, y + 9 * i, lines[i], color);
}

static void menu_box(u8* back, int n, const char* const* items, const u8* ok, int cur) {
    int w = 0;
    for (int i = 0; i < n; i++) if (text_w(items[i]) > w) w = text_w(items[i]);
    w += 22;
    int h = n * 9 + 10, x = 236 - w, y = 14;
    box(back, x, y, w, h);
    for (int i = 0; i < n; i++) {
        text(back, x + 13, y + 6 + i * 9, items[i], !ok[i] ? UI_GRAY : i == cur ? UI_YELLOW : UI_WHITE);
        if (i == cur) text(back, x + 5, y + 6 + i * 9, ">", UI_YELLOW);
    }
}

static void draw_bag(u8* back) {
    box(back, 12, 16, 216, 98);
    text(back, 22, 22, "BAG", UI_YELLOW);
    char buf[48], *p = cat(buf, "POWDER ");
    p = cat_num(p, powder);
    p = cat(p, "   SHELLS ");
    cat_num(p, shells);
    text(back, 22, 32, buf, UI_WHITE);
    for (int h = 0; h < NCOLORS; h++) {
        int x = 22 + (h % 3) * 66, y = 44 + (h / 3) * 9;
        fill_rect(back, x, y + 1, 4, 5, 16 + h);
        p = cat(buf, hue_names[h]);
        p = cat(p, " ");
        cat_num(p, stock[h]);
        text(back, x + 7, y, buf, stock[h] ? UI_WHITE : UI_GRAY);
    }
    p = cat(buf, "TOOLS: ");
    if (!tools) p = cat(p, "NONE");
    if (tools & TOOL_PICK) p = cat(p, "PICKAXE ");
    if (tools & TOOL_NET) p = cat(p, "NET ");
    if (tools & TOOL_ROPE) p = cat(p, "ROPE");
    text(back, 22, 84, buf, UI_WHITE);
    p = cat(buf, "PATTERNS: ");
    p = cat_num(p, unlock_count());
    cat(p, " OF 9");
    text(back, 22, 94, buf, UI_WHITE);
}

static void draw_page(u8* back) {
    copy_page(back, map_bg);
    char buf[64], *p;
    time_str(buf, day_min);
    text(back, 3, 2, buf, UI_YELLOW);
    p = cat(buf, "DAY ");
    cat_num(p, night);
    text(back, 96, 2, buf, UI_WHITE);
    p = cat(buf, "COINS ");
    cat_num(p, coins);
    text(back, 237 - text_w(buf), 2, buf, UI_WHITE);

    // Area names on the map, the selected one in yellow.
    for (int i = 0; i < A_COUNT; i++) {
        int x = areas[i].lx - text_w(areas[i].name) / 2;
        if (x + text_w(areas[i].name) > 236) x = 236 - text_w(areas[i].name);
        text(back, x, areas[i].ly, areas[i].name, i == sel ? UI_YELLOW : UI_WHITE);
    }

    box(back, 0, 116, 240, 44);
    switch (day_st) {
    case DS_INTRO:
    case DS_RESULT:
    case DS_DUSK:
        draw_lines(back, msg, 121, 4, UI_WHITE);
        break;
    case DS_MAP: {
        const Area* ar = &areas[sel];
        text(back, 8, 121, ar->name, UI_YELLOW);
        p = buf;
        if (sel == here) cat(p, "YOU ARE HERE");
        else { p = cat_num(p, TRAVEL); cat(p, " MIN WALK"); }
        text(back, 232 - text_w(buf), 121, buf, sel == here ? UI_GREEN : UI_WHITE);
        draw_lines(back, ar->desc, 130, 2, UI_WHITE);
        text(back, 8, 148, "START: BAG", UI_GRAY);
        break;
    }
    case DS_AREA: {
        const char* items[10];
        u8 ok[10];
        char labels[10][24];
        for (int k = 0; k < menu_n; k++) {
            int i = menu_spot[k];
            if (i < 0) { items[k] = "LEAVE"; ok[k] = 1; continue; }
            p = cat(labels[k], spots[i].name);
            if (spots[i].price) { p = cat(p, " "); p = cat_num(p, spots[i].price); cat(p, "C"); }
            items[k] = labels[k];
            ok[k] = !blocked(i);
        }
        menu_box(back, menu_n, items, ok, cursor);
        int i = menu_spot[cursor];
        text(back, 8, 121, areas[here].name, UI_YELLOW);
        if (i < 0) draw_lines(back, "BACK TO THE MAP.", 130, 2, UI_WHITE);
        else {
            draw_lines(back, spots[i].desc, 130, 2, UI_WHITE);
            const char* why = blocked(i);
            p = buf;
            if (spots[i].minutes && spots[i].kind != K_OPEN) { p = cat_num(p, spots[i].minutes); p = cat(p, " MIN"); }
            if (why) { if (p != buf) p = cat(p, "  "); cat(p, why); }
            text(back, 8, 148, buf, why ? UI_RED : UI_GREEN);
        }
        break;
    }
    case DS_COLOR: {
        const char* items[NCOLORS];
        u8 ok[NCOLORS];
        char labels[NCOLORS][16];
        for (int h = 0; h < NCOLORS; h++) {
            p = cat(labels[h], hue_names[h]);
            while (p < labels[h] + 8) *p++ = ' ';
            cat_num(p, stock[h]);
            items[h] = labels[h];
            ok[h] = coins >= 8;
        }
        // Show 9 rows at a time around the cursor.
        int top = cursor >= 9 ? cursor - 8 : 0;
        menu_box(back, 9, items + top, ok + top, cursor - top);
        text(back, 8, 121, "MARKET", UI_YELLOW);
        draw_lines(back, "PICK A COLOR. 8 COINS EACH. B WHEN DONE.", 130, 2, UI_WHITE);
        break;
    }
    case DS_WORK: {
        const Spot* s = &spots[work_spot];
        text(back, 8, 121, s->name, UI_YELLOW);
        text(back, 8, 130, s->price ? "BUYING..." : "WORKING...", UI_WHITE);
        int len = s->price ? 20 : 60;
        fill_rect(back, 8, 146, 224, 6, UI_SHADOW);
        fill_rect(back, 8, 146, 224 * st_t / len, 6, UI_YELLOW);
        break;
    }
    }
    if (bag_open) draw_bag(back);
}

void day_update(u8* back) {
    step();
    // Redraw a page only when what it shows changed.
    u32 sig = 2166136261u;
    int vals[] = { day_st, sel, here, cursor, day_min, coins, bag_open, day_st == DS_WORK ? st_t : 0, powder, shells,
                   stock[0] + stock[1] * 3 + stock[cursor % NCOLORS] * 7 };
    for (unsigned i = 0; i < sizeof vals / sizeof vals[0]; i++) sig = (sig ^ (u32)vals[i]) * 16777619u;
    int pi = back == PAGE1;
    if (page_sig[pi] != sig) { page_sig[pi] = sig; draw_page(back); }
    // The player stands at the current area; a bouncing arrow marks the selection.
    if (day_st == DS_MAP && !bag_open)
        sprite(areas[sel].x - 8, (areas[sel].y < 40 ? 12 : areas[sel].y - 28) + ((frame >> 3) & 1), T_MAPSPR, 0, 1, 0);
    if (!bag_open) sprite(areas[here].x - 8, areas[here].y - 8, T_MAPSPR + 4, 0, 1, 0);
}
