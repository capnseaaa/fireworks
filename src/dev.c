// Dev mode, for testing: L+R+B (almost anywhere) opens a menu that gives materials and
// coins, unlocks everything, and jumps straight to a day or a night.
#include "game.h"

static int dev_from, dev_cursor, dev_note_t;
static const char* dev_note;

enum { D_COINS, D_MATS, D_PATTERNS, D_ALL, D_REP, D_DAY, D_NIGHT, D_LATE, D_MINE9, D_TITLE, D_BACK, D_COUNT };
static const char* const dev_items[D_COUNT] = {
    "+1000 COINS", "MAX MATERIALS", "ALL PATTERNS", "ALL UPGRADES + GEAR", "+10 REP",
    "GO TO A NEW DAY", "GO TO TONIGHT'S SHOP", "DAY CLOCK TO 5:50 PM", "MINE: REACHED FLOOR 9",
    "BACK TO THE TITLE", "CLOSE",
};

// Called from the main loop: L+R held, B pressed.
int dev_wanted(void) {
    return scene != SC_DEV && scene != SC_SHOW && (keys & (KEY_L | KEY_R)) == (KEY_L | KEY_R) && (hit & KEY_B);
}

void dev_open(void) {
    dev_from = scene;
    set_scene(SC_DEV);
}

void dev_enter(void) {
    blank_once();
    ui_palette();
    REG_BLDCNT = 0;
    REG_DISPCNT &= ~(1 << 13);
    dev_cursor = 0;
    dev_note_t = 0;
}

static void done(const char* s) { dev_note = s; dev_note_t = 90; }

// Leaves to where you came from. A day or the forest comes back mid-day; a night starts over.
static void go_back(void) {
    if (dev_from == SC_DAY || dev_from == SC_FIELD) day_return();
    else if (dev_from == SC_SHOP) set_scene(SC_SHOP);
    else set_scene(dev_from == SC_REVIEW ? SC_DAY : dev_from);
}

void dev_update(u8* back) {
    if (hit & KEY_UP) dev_cursor = (dev_cursor + D_COUNT - 1) % D_COUNT;
    if (hit & KEY_DOWN) dev_cursor = (dev_cursor + 1) % D_COUNT;
    if (hit & KEY_B) { go_back(); return; }
    if (hit & KEY_A) {
        switch (dev_cursor) {
        case D_COINS: coins += 1000; done("COINS ADDED."); break;
        case D_MATS:
            powder = shells = 99;
            for (int i = 0; i < NCOLORS; i++) stock[i] = 20;
            bag_used = 0;
            done("99 POWDER, 99 SHELLS, 20 OF EVERY STAR.");
            break;
        case D_PATTERNS: known = (1 << P_COUNT) - 1; done("ALL 9 PATTERNS KNOWN."); break;
        case D_ALL:
            for (int i = 0; i < UPG_COUNT; i++) upg[i] = upgrades[i].max;
            gear[GEAR_AXE] = gear[GEAR_BAG] = gear[GEAR_PICK] = 3;
            gear[GEAR_LANTERN] = 1;
            tools = TOOL_PICK | TOOL_NET | TOOL_ROPE;
            done("EVERY UPGRADE, TOOL, AND TOP GEAR.");
            break;
        case D_REP: rep += 10; done("REP +10."); break;
        case D_DAY: set_scene(SC_DAY); return;
        case D_NIGHT: set_scene(SC_SHOP); return;
        case D_LATE:
            if (dev_from == SC_DAY || dev_from == SC_FIELD) { day_min = DAY_END_MIN - 10; day_return(); return; }
            done("ONLY WORKS FROM THE DAY.");
            break;
        case D_MINE9: mine_deepest = 9; done("THE MINE REMEMBERS FLOOR 9."); break;
        case D_TITLE: set_scene(SC_TITLE); return;
        case D_BACK: go_back(); return;
        }
    }
    if (dev_note_t) dev_note_t--;

    fill_rect(back, 0, 0, 240, 160, UI_FILL);
    text(back, 8, 4, "DEV MODE", UI_YELLOW);
    char buf[48], *p = cat(buf, "NIGHT ");
    p = cat_num(p, night);
    p = cat(p, "  COINS ");
    p = cat_num(p, coins);
    p = cat(p, "  REP ");
    cat_num(p, rep);
    text(back, 8, 14, buf, UI_GRAY);
    for (int i = 0; i < D_COUNT; i++) {
        text(back, 22, 28 + i * 10, dev_items[i], i == dev_cursor ? UI_YELLOW : UI_WHITE);
        if (i == dev_cursor) text(back, 12, 28 + i * 10, ">", UI_YELLOW);
    }
    text(back, 8, 150, dev_note_t ? dev_note : "A: DO IT   B: CLOSE", dev_note_t ? UI_GREEN : UI_GRAY);
}
