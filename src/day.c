// The day: walk the town map from 10 AM to 6 PM, gathering materials and buying
// supplies for tonight. Every trip and every job costs time; each spot works once a day,
// and some spots need a tool from the market first.
#include "game.h"
#include "art.h"

#define T_MAPSPR 512                   // arrow, player (16x16 each, 4 tiles)
#define DAY_END 480                    // minutes after 10:00 AM: 6:00 PM
#define TRAVEL 25                      // minutes to walk anywhere

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
enum { K_OPEN, K_UPGRADE, K_EXPLORE, K_MINE, K_GEODES, K_BEACH,
       K_QUARRY, K_CLIMBMG, K_BUY_POWDER, K_BUY_SHELLS, K_BUY_STAR, K_BUY_PICK, K_BUY_NET, K_BUY_ROPE, K_BUY_AXE, K_BUY_BAG, K_BUY_LANTERN };
typedef struct { u8 area, kind, minutes, needs, price; const char* name; const char* desc; } Spot;
static const Spot spots[] = {
    { A_HOME, K_OPEN, 0, 0, 0, "OPEN THE SHOP", "END THE DAY AND OPEN FOR THE NIGHT." },
    { A_HOME, K_UPGRADE, 0, 0, 0, "IMPROVE THE SHOP", "SPEND COINS ON UPGRADES FOR THE SHOP." },
    { A_FOREST, K_EXPLORE, 0, 0, 0, "EXPLORE", "WALK THE WOODS: CHOP TREES, PICK FLOWERS, SEARCH BUSHES, BURN CHARCOAL." },
    { A_MINE, K_MINE, 0, 0, 0, "ENTER THE MINE", "BREAK ROCKS FOR ORE. ONE ROCK ON EACH FLOOR HIDES THE LADDER DOWN." },
    { A_MINE, K_GEODES, 30, 0, 0, "CRACK GEODES", "THREE GEODES A DAY. STOP THE POINTER ON THE CRACK." },
    { A_BEACH, K_BEACH, 0, 0, 0, "EXPLORE", "ROCKS FULL OF TIDE POOLS: SHELLS, SEA GLASS, CORAL, AMBER. A NET REACHES INTO THE POOLS." },
    { A_QUARRY, K_QUARRY, 0, TOOL_ROPE, 0, "EXPLORE", "A STEPPED STONE PIT: WHITE STONE, SILVER, GOLD, OLD CRATES. NEEDS A ROPE." },
    { A_QUARRY, K_CLIMBMG, 30, TOOL_ROPE, 0, "CLIMB THE CLIFF", "PRESS THE HOLDS AS THEY LIGHT UP. THE HIGHER, THE BETTER. NEEDS A ROPE." },
    { A_MARKET, K_BUY_POWDER, 10, 0, 12, "5 POWDER", "FIVE SCOOPS OF BLACK POWDER." },
    { A_MARKET, K_BUY_SHELLS, 10, 0, 15, "2 SHELLS", "TWO PAPER SHELLS, READY TO FILL." },
    { A_MARKET, K_BUY_STAR, 10, 0, 8, "1 STAR", "ONE COLORED STAR. PICK THE COLOR." },
    { A_MARKET, K_BUY_PICK, 10, 0, 0, "PICKAXE", "BREAKS ROCKS IN THE MINE. BETTER ONES BREAK HARD ROCK FASTER." },
    { A_MARKET, K_BUY_LANTERN, 10, 0, 0, "LANTERN", "LIGHTS THE MINE BELOW FLOOR 4." },
    { A_MARKET, K_BUY_NET, 10, 0, 0, "NET", "REACHES INTO THE BEACH'S TIDE POOLS." },
    { A_MARKET, K_BUY_ROPE, 10, 0, 0, "ROPE", "OPENS THE QUARRY." },
    { A_MARKET, K_BUY_AXE, 10, 0, 0, "AXE", "A SHARPER AXE: FEWER SWINGS, AND IT FELLS THE OLD TREE." },
    { A_MARKET, K_BUY_BAG, 10, 0, 0, "BAG", "A BIGGER BAG: CARRY MORE HOME EACH DAY." },
};
#define NSPOTS (int)(sizeof spots / sizeof spots[0])

enum { DS_INTRO, DS_MAP, DS_AREA, DS_COLOR, DS_WORK, DS_RESULT, DS_DUSK, DS_UPGRADE };

int day_st;                            // state (non-static so test scripts can read it)
int day_min;                           // minutes since 10:00 AM
static int st_t, here, sel, cursor, bag_open;
static int work_spot, work_from, work_price;
static u32 used;                       // spots done today (bit per spot)
static char msg[160];
// The morning's pages: the day's note, then maybe a festival, a visit, or a letter.
static const char* page_label[4];
static char page_text[4][160];
static int npages, page_i;

static void add_page(const char* label, const char* text) {
    if (npages >= 4) return;
    page_label[npages] = label;
    page_text[npages][0] = 0;
    cat(page_text[npages++], text);
}
static u32 page_sig[2];
static int menu_x0 = 240, menu_y1;     // the open menu box (the player marker hides under it)
static int upg_result;                 // the result came from buying an upgrade

// Why an upgrade cannot be bought now, or 0.
static const char* upg_blocked(int k) {
    const Upgrade* u = &upgrades[k];
    if (upg[k] >= u->max) return "FULLY UPGRADED";
    if (rep_tier() < u->need[upg[k]]) return u->need[upg[k]] == 1 ? "NEEDS LOCAL FAVORITE" : "NEEDS TOWN TREASURE";
    if (coins < u->price[upg[k]]) return "NOT ENOUGH COINS";
    return 0;
}

static int menu_n, menu_spot[10];
static int area_menu(void) {           // the spots at the current area
    menu_n = 0;
    for (int i = 0; i < NSPOTS; i++)
        if (spots[i].area == here) menu_spot[menu_n++] = i;
    menu_spot[menu_n++] = -1;          // LEAVE
    return menu_n;
}

static int is_tool(int k) { return k == K_BUY_NET || k == K_BUY_ROPE; }
static int tool_bit(int k) { return k == K_BUY_NET ? TOOL_NET : TOOL_ROPE; }

// Gear prices go up with each tier; 0 once you have the best.
// Gear costs coins plus something gathered. mat: a star color 0..11, MAT_SHELLS, MAT_POWDER,
// or -1 for none. Returns 0 coins (and no material) once you have the best.
enum { MAT_SHELLS = 12, MAT_POWDER = 13 };
typedef struct { u16 coins; s8 mat; u8 n; } Cost;

static Cost gear_cost(int i) {
    Cost c = { spots[i].price, -1, 0 };
    switch (spots[i].kind) {
    case K_BUY_AXE:
        if (gear[GEAR_AXE] == 1) c = (Cost){ 55, MAT_SHELLS, 4 };
        else if (gear[GEAR_AXE] == 2) c = (Cost){ 140, H_SILVER, 2 };
        else c = (Cost){ 0, -1, 0 };
        break;
    case K_BUY_BAG:
        if (gear[GEAR_BAG] == 1) c = (Cost){ 50, H_GREEN, 2 };
        else if (gear[GEAR_BAG] == 2) c = (Cost){ 125, H_VIOLET, 2 };
        else c = (Cost){ 0, -1, 0 };
        break;
    case K_BUY_PICK:
        if (gear[GEAR_PICK] == 0) c = (Cost){ 35, -1, 0 };
        else if (gear[GEAR_PICK] == 1) c = (Cost){ 85, H_BLUE, 3 };
        else if (gear[GEAR_PICK] == 2) c = (Cost){ 180, H_SILVER, 2 };
        else c = (Cost){ 0, -1, 0 };
        break;
    case K_BUY_LANTERN: c = gear[GEAR_LANTERN] ? (Cost){ 0, -1, 0 } : (Cost){ 70, H_YELLOW, 2 }; break;
    case K_BUY_NET: c = (Cost){ 30, H_GREEN, 3 }; break;
    case K_BUY_ROPE: c = (Cost){ 40, MAT_SHELLS, 3 }; break;
    }
    return c;
}

static int price_of(int i) { return gear_cost(i).coins; }
static Cost work_cost;                 // what the purchase in progress takes

static int mat_have(int m) { return m == MAT_SHELLS ? shells : m == MAT_POWDER ? powder : m >= 0 ? stock[m] : 0; }

static void mat_take(int m, int n) {
    if (m == MAT_SHELLS) shells -= n;
    else if (m == MAT_POWDER) powder -= n;
    else if (m >= 0) stock[m] -= n;
}

static char* mat_name(char* p, int m, int n) {
    p = cat_num(p, n);
    p = cat(p, " ");
    if (m == MAT_SHELLS) return cat(p, "SHELLS");
    if (m == MAT_POWDER) return cat(p, "POWDER");
    p = cat(p, hue_names[m]);
    return cat(p, n == 1 ? " STAR" : " STARS");
}

static int is_buy(int i) { return spots[i].kind >= K_BUY_POWDER; }

static char* spot_name(char* p, int i) {
    static const char* const tiers[4] = { "", "", " II", " III" };
    int k = spots[i].kind;
    p = cat(p, spots[i].name);
    if (k == K_BUY_AXE) p = cat(p, tiers[gear[GEAR_AXE] < 3 ? gear[GEAR_AXE] + 1 : 3]);
    if (k == K_BUY_BAG) p = cat(p, tiers[gear[GEAR_BAG] < 3 ? gear[GEAR_BAG] + 1 : 3]);
    if (k == K_BUY_PICK) p = cat(p, tiers[gear[GEAR_PICK] < 3 ? gear[GEAR_PICK] + 1 : 3]);
    return p;
}

// Why a spot cannot be used right now, or 0.
static const char* blocked(int i) {
    const Spot* s = &spots[i];
    if (s->kind == K_OPEN || s->kind == K_UPGRADE) return 0;
    if (s->kind == K_EXPLORE) return day_min >= DAY_END ? "NOT ENOUGH DAYLIGHT" : 0;
    if (s->kind == K_MINE || s->kind == K_BEACH) return day_min >= DAY_END ? "NOT ENOUGH DAYLIGHT" : 0;
    if ((s->kind == K_BUY_AXE || s->kind == K_BUY_BAG || s->kind == K_BUY_PICK || s->kind == K_BUY_LANTERN) && !price_of(i))
        return "YOU HAVE THE BEST ONE";
    if (s->kind == K_GEODES && geodes_done()) return "DONE FOR TODAY";
    if (s->kind == K_CLIMBMG && climb_done()) return "DONE FOR TODAY";
    if (s->kind == K_QUARRY && (tools & TOOL_ROPE)) return day_min >= DAY_END ? "NOT ENOUGH DAYLIGHT" : 0;
    if (s->needs && !(tools & s->needs))
        return s->needs == TOOL_NET ? "NEEDS A NET" : "NEEDS A ROPE";
    if (is_tool(s->kind) && (tools & tool_bit(s->kind))) return "YOU HAVE ONE";
    if (is_buy(i) && coins < price_of(i)) return "NOT ENOUGH COINS";
    if (is_buy(i)) {
        Cost c = gear_cost(i);
        if (c.mat >= 0 && mat_have(c.mat) < c.n) return "NOT ENOUGH MATERIALS";
    }
    if (!is_buy(i) && (used & (1u << i))) return "DONE FOR TODAY";
    if (day_min + s->minutes > DAY_END) return "NOT ENOUGH DAYLIGHT";
    return 0;
}

static char* time_str(char* p, int m) {
    int h = 10 + m / 60, mm = m % 60;
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

static void do_spot(int i) {
    switch (spots[i].kind) {
    case K_BUY_POWDER: powder += 5; cat(msg, "YOU BOUGHT 5 POWDER."); break;
    case K_BUY_SHELLS: shells += 2; cat(msg, "YOU BOUGHT 2 SHELLS."); break;
    case K_BUY_PICK: gear[GEAR_PICK]++; cat(msg, gear[GEAR_PICK] == 1 ? "A PICKAXE! TIME TO TRY THE MINE." : "A BETTER PICKAXE! ROCKS BREAK FASTER."); break;
    case K_BUY_LANTERN: gear[GEAR_LANTERN] = 1; cat(msg, "A LANTERN! NOW I CAN GO DEEPER THAN FLOOR 4."); break;
    case K_BUY_NET: tools |= TOOL_NET; cat(msg, "YOU BOUGHT A NET! THE TIDE POOLS ARE OPEN."); break;
    case K_BUY_ROPE: tools |= TOOL_ROPE; cat(msg, "YOU BOUGHT A ROPE! THE QUARRY IS OPEN."); break;
    case K_BUY_AXE: gear[GEAR_AXE]++; cat(msg, "A SHARPER AXE! TREES COME DOWN FASTER."); break;
    case K_BUY_BAG: gear[GEAR_BAG]++; cat(msg, "A BIGGER BAG! I CAN CARRY MORE EACH DAY."); break;
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

static int resuming;                   // back from walking around an area, mid-day

void day_return(void) {
    resuming = 1;
    set_scene(SC_DAY);
}

char* day_clock(char* p, int m) { return time_str(p, m); }

void day_enter(void) {
    blank_once();
    for (int i = 0; i < MAP_PAL_N; i++) BG_PAL[16 + i] = map_pal[i];
    ui_palette();
    REG_BLDCNT = 0;
    REG_DISPCNT &= ~(1 << 13);         // no window left over from the forest
    vu32* t = (vu32*)OBJ_TILES;
    for (int i = 0; i < 2 * 4 * 8; i++) t[T_MAPSPR * 8 + i] = map_tiles[i];
    OBJ_PAL[1] = RGB(4, 3, 6);
    OBJ_PAL[2] = RGB(31, 25, 6);
    OBJ_PAL[3] = RGB(31, 31, 31);
    OBJ_PAL[4] = RGB(31, 24, 18);
    OBJ_PAL[5] = RGB(8, 14, 28);
    page_sig[0] = page_sig[1] = 0;
    if (resuming) {
        resuming = 0;
        if (day_min >= DAY_END) { day_min = DAY_END; dusk(); return; }
        area_menu();
        cursor = 0;
        go(DS_AREA);
        return;
    }
    save_game(SAVE_DAY);
    bag_used = 0;
    day_min = 0;
    here = A_HOME;
    sel = A_HOME;
    used = 0;
    bag_open = 0;
    page_sig[0] = page_sig[1] = 0;
    char* p = cat(msg, "DAY ");
    p = cat_num(p, night);
    p = cat(p, ". TIME TO GATHER SUPPLIES FOR TONIGHT.");
    if (night == 2) cat(p, " EVERY WALK TAKES TIME, AND THE SUN SETS AT 6 PM. (D-PAD TO PICK A PLACE, A TO GO.)");
    npages = page_i = 0;
    add_page(0, msg);
    int fest = festival_night(night);
    if (fest) {
        p = cat(msg, "TONIGHT IS PORT WILLOW'S ");
        p = cat(p, festival_names[fest]);
        cat(p, "! THE WHOLE TOWN WILL BE OUT. EXPECT A CROWD, AND BIGGER TIPS.");
        add_page(0, msg);
    }
    // The gift comes after the morning's save, so reloading this day gives it again
    // (once) rather than losing it.
    int ev = day_event(night);
    if (ev == EV_VISIT) {
        add_page("GRANDMA", visit_text(night));
        visit_gift(night, msg);
        add_page(0, msg);
    } else if (ev == EV_LETTER) add_page("A LETTER FROM GRANDMA", letter_text(night));
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
    if (hit & KEY_START) { bag_open ^= 1; sfx(bag_open ? SFX_CLOTH : SFX_BACK); }
    if (bag_open) { if (a || b) { bag_open = 0; sfx(SFX_BACK); } return; }
    // Menu sounds: every state moves a cursor with the d-pad.
    if (hit & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT)) sfx(SFX_MOVE);
    if (b && day_st != DS_INTRO && day_st != DS_RESULT && day_st != DS_DUSK) sfx(SFX_BACK);
    switch (day_st) {
    case DS_INTRO:
        if (a) {
            if (++page_i < npages) sfx_ex(SFX_PAGE, 40, 256);
            else go(DS_MAP);
        }
        break;
    case DS_MAP:
        if (hit & KEY_LEFT) sel = area_toward(-1, 0);
        if (hit & KEY_RIGHT) sel = area_toward(1, 0);
        if (hit & KEY_UP) sel = area_toward(0, -1);
        if (hit & KEY_DOWN) sel = area_toward(0, 1);
        if (a) {
            sfx(SFX_SELECT);
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
        if (i < 0) { sfx(SFX_BACK); go(DS_MAP); break; }
        if (blocked(i)) { sfx(SFX_NOPE); break; }
        sfx(spots[i].kind == K_OPEN ? SFX_DOOR : SFX_SELECT);
        if (spots[i].kind == K_OPEN) { set_scene(SC_SHOP); break; }
        if (spots[i].kind == K_UPGRADE) { go(DS_UPGRADE); cursor = 0; break; }
        if (spots[i].kind == K_EXPLORE) { field_go(FIELD_FOREST); break; }
        if (spots[i].kind == K_MINE) { field_go(FIELD_MINE); break; }
        if (spots[i].kind == K_BEACH) { field_go(FIELD_BEACH); break; }
        if (spots[i].kind == K_QUARRY) { field_go(FIELD_QUARRY); break; }
        if (spots[i].kind == K_CLIMBMG) { field_go(FIELD_CLIMB); break; }
        if (spots[i].kind == K_GEODES) { field_go(FIELD_GEODES); break; }
        if (spots[i].kind == K_BUY_STAR) { go(DS_COLOR); cursor = 0; break; }
        work_spot = i;
        work_from = day_min;
        work_price = price_of(i);
        work_cost = gear_cost(i);
        go(DS_WORK);
        break;
    }
    case DS_COLOR:
        if (hit & KEY_UP) cursor = (cursor + NCOLORS - 1) % NCOLORS;
        if (hit & KEY_DOWN) cursor = (cursor + 1) % NCOLORS;
        if (b) { area_menu(); cursor = 0; go(DS_AREA); break; }
        if (a && coins < 8) sfx(SFX_NOPE);
        if (a && coins >= 8) {
            sfx(SFX_COINS);
            coins -= 8;
            stock[cursor]++;
            day_min += 2;
            if (day_min > DAY_END) day_min = DAY_END;
        }
        break;
    case DS_UPGRADE:
        if (hit & KEY_UP) cursor = (cursor + UPG_COUNT) % (UPG_COUNT + 1);
        if (hit & KEY_DOWN) cursor = (cursor + 1) % (UPG_COUNT + 1);
        if (b || (a && cursor == UPG_COUNT)) { area_menu(); cursor = 0; go(DS_AREA); break; }
        if (a && cursor < UPG_COUNT && upg_blocked(cursor)) sfx(SFX_NOPE);
        if (a && !upg_blocked(cursor)) {
            sfx(SFX_LEVELUP);
            const Upgrade* u = &upgrades[cursor];
            coins -= u->price[upg[cursor]];
            upg[cursor]++;
            msg[0] = 0;
            char* p = cat(msg, "NEW ");
            p = cat(p, u->name);
            p = cat(p, "! ");
            cat(p, u->desc);
            upg_result = 1;
            int keep = cursor;
            go(DS_RESULT);
            cursor = keep;
        }
        break;
    case DS_WORK: {
        const Spot* s = &spots[work_spot];
        int len = is_buy(work_spot) ? 20 : 60;
        day_min = work_from + s->minutes * st_t / len;
        if (st_t >= len) {
            day_min = work_from + s->minutes;
            if (is_buy(work_spot)) {
                coins -= work_price;
                if (work_cost.mat >= 0) mat_take(work_cost.mat, work_cost.n);
                sfx(s->kind >= K_BUY_PICK ? SFX_LEVELUP : SFX_COINS);   // new gear is a big moment
            }
            else used |= 1u << work_spot;
            msg[0] = 0;
            do_spot(work_spot);
            go(DS_RESULT);
        }
        break;
    }
    case DS_RESULT:
        if (a) {
            sfx(SFX_SELECT);
            if (day_min >= DAY_END - 15) dusk();
            else if (upg_result) {
                upg_result = 0;
                go(DS_UPGRADE);
            } else {
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
    menu_x0 = x; menu_y1 = y + h;
    box(back, x, y, w, h);
    for (int i = 0; i < n; i++) {
        text(back, x + 13, y + 6 + i * 9, items[i], !ok[i] ? UI_GRAY : i == cur ? UI_YELLOW : UI_WHITE);
        if (i == cur) text(back, x + 5, y + 6 + i * 9, ">", UI_YELLOW);
    }
}

static void draw_bag(u8* back) {
    box(back, 12, 16, 216, 118);
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
    if (!tools && !gear[GEAR_PICK] && !gear[GEAR_LANTERN]) p = cat(p, "NONE");
    if (gear[GEAR_PICK]) { p = cat(p, "PICK "); p = cat_num(p, gear[GEAR_PICK]); p = cat(p, " "); }
    if (gear[GEAR_LANTERN]) p = cat(p, "LANTERN ");
    if (tools & TOOL_NET) p = cat(p, "NET ");
    if (tools & TOOL_ROPE) p = cat(p, "ROPE");
    text(back, 22, 84, buf, UI_WHITE);
    p = cat(buf, "PATTERNS: ");
    p = cat_num(p, unlock_count());
    cat(p, " OF 9");
    text(back, 22, 94, buf, UI_WHITE);
    p = cat(buf, "REP ");
    p = cat_num(p, rep);
    p = cat(p, "  ");
    cat(p, rep_titles[rep_tier()]);
    text(back, 22, 104, buf, UI_YELLOW);
    p = cat(buf, "BENCH ");
    p = cat_num(p, upg[UPG_BENCH]);
    p = cat(p, " HOURS ");
    p = cat_num(p, upg[UPG_HOURS]);
    p = cat(p, " SIGN ");
    p = cat_num(p, upg[UPG_SIGN]);
    p = cat(p, " SHELF ");
    cat_num(p, upg[UPG_SHELF]);
    text(back, 22, 114, buf, UI_WHITE);
}

static void draw_page(u8* back) {
    menu_x0 = 240;
    menu_y1 = 0;
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
        if (page_label[page_i]) {
            text(back, 8, 121, page_label[page_i], UI_YELLOW);
            draw_lines(back, page_text[page_i], 130, 3, UI_WHITE);
        } else draw_lines(back, page_text[page_i], 121, 4, UI_WHITE);
        if (page_i + 1 < npages && (frame & 16)) fill_rect(back, 226, 152, 5, 3, UI_YELLOW);
        break;
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
            p = spot_name(labels[k], i);
            if (price_of(i)) { p = cat(p, " "); p = cat_num(p, price_of(i)); cat(p, "C"); }
            items[k] = labels[k];
            ok[k] = !blocked(i);
        }
        menu_box(back, menu_n, items, ok, cursor);
        int i = menu_spot[cursor];
        text(back, 8, 121, areas[here].name, UI_YELLOW);
        if (i < 0) draw_lines(back, "BACK TO THE MAP.", 130, 2, UI_WHITE);
        else {
            const char* why = blocked(i);
            Cost c = is_buy(i) ? gear_cost(i) : (Cost){ 0, -1, 0 };
            if (c.mat >= 0) {
                // Gear: one line of description, then what it takes and what you have.
                char d[1][41];
                if (wrap(spots[i].desc, 37, d, 1)) text(back, 8, 130, d[0], UI_WHITE);
                p = cat(buf, "NEEDS ");
                p = mat_name(p, c.mat, c.n);
                p = cat(p, " (HAVE ");
                p = cat_num(p, mat_have(c.mat));
                cat(p, ")");
                text(back, 8, 139, buf, mat_have(c.mat) >= c.n ? UI_GREEN : UI_RED);
            } else draw_lines(back, spots[i].desc, 130, 2, UI_WHITE);
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
    case DS_UPGRADE: {
        const char* items[UPG_COUNT + 1];
        u8 ok[UPG_COUNT + 1];
        char labels[UPG_COUNT][24];
        for (int k = 0; k < UPG_COUNT; k++) {
            p = cat(labels[k], upgrades[k].name);
            p = cat(p, " ");
            p = cat_num(p, upg[k]);
            p = cat(p, "/");
            cat_num(p, upgrades[k].max);
            items[k] = labels[k];
            ok[k] = !upg_blocked(k);
        }
        items[UPG_COUNT] = "BACK";
        ok[UPG_COUNT] = 1;
        menu_box(back, UPG_COUNT + 1, items, ok, cursor);
        text(back, 8, 121, "IMPROVE THE SHOP", UI_YELLOW);
        if (cursor == UPG_COUNT) draw_lines(back, "BACK TO HOME.", 130, 2, UI_WHITE);
        else {
            const Upgrade* u = &upgrades[cursor];
            draw_lines(back, u->desc, 130, 2, UI_WHITE);
            const char* why = upg_blocked(cursor);
            p = buf;
            if (upg[cursor] < u->max) { p = cat_num(p, u->price[upg[cursor]]); p = cat(p, " COINS"); }
            if (why && upg[cursor] < u->max) p = cat(p, "  ");
            if (why) cat(p, why);
            text(back, 8, 148, buf, why ? UI_RED : UI_GREEN);
        }
        break;
    }
    case DS_WORK: {
        const Spot* s = &spots[work_spot];
        text(back, 8, 121, s->name, UI_YELLOW);
        text(back, 8, 130, is_buy(work_spot) ? "BUYING..." : "WORKING...", UI_WHITE);
        int len = is_buy(work_spot) ? 20 : 60;
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
                   stock[0] + stock[1] * 3 + stock[cursor % NCOLORS] * 7,
                   day_st == DS_INTRO ? page_i * 2 + (page_i + 1 < npages && (frame & 16) ? 1 : 0) : 0 };
    for (unsigned i = 0; i < sizeof vals / sizeof vals[0]; i++) sig = (sig ^ (u32)vals[i]) * 16777619u;
    for (int i = 0; msg[i]; i += 3) sig = (sig ^ (u8)msg[i]) * 16777619u;   // the words themselves
    int pi = back == PAGE1;
    if (page_sig[pi] != sig) { page_sig[pi] = sig; draw_page(back); }
    // The player stands at the current area; a bouncing arrow marks the selection.
    if (day_st == DS_MAP && !bag_open)
        sprite(areas[sel].x - 8, (areas[sel].y < 40 ? 12 : areas[sel].y - 28) + ((frame >> 3) & 1), T_MAPSPR, 0, 1, 0);
    int px = areas[here].x - 8, py = areas[here].y - 8;
    int covered = px + 16 > menu_x0 && py < menu_y1;
    if (!bag_open && !covered) sprite(px, py, T_MAPSPR + 4, 0, 1, 0);
}
