// Walk-around gathering, in the spirit of Stardew Valley: trees to chop, bushes to
// search, sparkling flowers and debris that come back each day, and a kiln minigame.
// Walking is free; every action costs a few minutes of daylight.
#include "game.h"
#include "art.h"

#define T_PLAYER 512                   // 6 frames of 16x32 (8 tiles each)
#define T_NATURE 560
// Offsets into the nature tiles, in make_art.py's nature_words order.
enum { NT_OAK = 0, NT_BIG = 16, NT_STUMP = 80, NT_BUSH = 84, NT_BARE = 88, NT_KILN = 92, NT_TWIG = 108,
       NT_STONE = 112, NT_WEEDS = 116, NT_AXE = 120, NT_FLOWER = 124, NT_SPARK = 128 };

u8 gear[GEAR_COUNT];
int bag_used;                          // items picked up today (the bag holds bag_size())

int bag_size(void) { return 10 + 10 * gear[GEAR_BAG]; }   // 20, 30, 40

enum { N_OAK, N_BIG, N_STUMP, N_BIGSTUMP, N_BUSH, N_BARE, N_KILN, N_TWIG, N_STONE, N_WEEDS, N_FLOWER, N_GONE };
typedef struct { u8 type, x, y, hp, color; } Node;   // x, y = where it meets the ground

#define MAXNODES 32
static Node nodes[MAXNODES];
static int nnodes, nodes_day = -1, kiln_used;

// The flowers' colors, and the stars they turn into.
static const u8 flower_hue[6] = { H_RED, H_BLUE, H_YELLOW, H_VIOLET, H_PINK, H_ORANGE };

static int px, py, face, step_t, moving;   // player feet; face: 0 down, 1 up, 2 left, 3 right
static int swing, swing_node;               // axe swing frames left, and its target
static char note[64];                       // a short message at the bottom
static int note_t;
static char pops[4][24];                    // "+2 SHELLS" popping up over the spot
static s16 pop_x[4], pop_y[4], pop_t[4];
enum { F_WALK, F_KILN };
static int fstate;
static int heat, zone, zone_v, score, kiln_t;   // the kiln minigame

static void add_node(int type, int x, int y, int hp) {
    if (nnodes >= MAXNODES) return;
    Node* n = &nodes[nnodes++];
    n->type = type; n->x = x; n->y = y; n->hp = hp; n->color = 0;
}

static int free_spot(int x, int y) {
    if (x > 100 && x < 140 && y > 84) return 0;            // keep the path clear
    if (y > 82 && y < 102 && x > 86 && x < 154) return 0;
    for (int i = 0; i < nnodes; i++) {
        int dx = nodes[i].x - x, dy = nodes[i].y - y;
        if (dx * dx + dy * dy < 22 * 22) return 0;
    }
    return 1;
}

// Trees, bushes, and the kiln stay put; forage and debris are scattered fresh each day.
static void make_day(void) {
    nnodes = 0;
    static const u8 oaks[][2] = { { 50, 72 }, { 84, 56 }, { 172, 66 }, { 198, 108 }, { 56, 132 }, { 166, 134 } };
    for (unsigned i = 0; i < sizeof oaks / 2; i++) add_node(N_OAK, oaks[i][0], oaks[i][1], 3);
    add_node(N_BIG, 136, 60, 8);
    add_node(N_BUSH, 30, 100, 0);
    add_node(N_BUSH, 206, 82, 0);
    add_node(N_BUSH, 96, 120, 0);
    add_node(N_KILN, 196, 146, 0);
    int want = 6 + rnd() % 3, flowers = 4 + rnd() % 3;
    for (int tries = 0; tries < 200 && (want > 0 || flowers > 0); tries++) {
        int x = 28 + rnd() % 186, y = 52 + rnd() % 100;
        if (!free_spot(x, y)) continue;
        if (flowers > 0) {
            add_node(N_FLOWER, x, y, 0);
            nodes[nnodes - 1].color = rnd() % 6;
            flowers--;
        } else {
            int r = rnd() % 3;
            add_node(r == 0 ? N_TWIG : r == 1 ? N_STONE : N_WEEDS, x, y, 0);
            want--;
        }
    }
    kiln_used = 0;
}

// Each kind's solid footprint (half width, height above the ground point).
static void footprint(const Node* n, int* hw, int* hh) {
    switch (n->type) {
    case N_OAK: case N_STUMP: *hw = 6; *hh = 6; break;
    case N_BIG: case N_BIGSTUMP: *hw = 12; *hh = 8; break;
    case N_KILN: *hw = 12; *hh = 8; break;
    case N_GONE: *hw = 0; *hh = 0; break;
    default: *hw = 6; *hh = 5; break;
    }
}

static int blocked(int x, int y) {
    if (x < 22 || x > 218 || y < 44) return 1;
    if (y > 154 && (x < 106 || x > 134)) return 1;
    for (int i = 0; i < nnodes; i++) {
        int hw, hh;
        footprint(&nodes[i], &hw, &hh);
        if (!hw) continue;
        if (x > nodes[i].x - hw - 4 && x < nodes[i].x + hw + 4 && y > nodes[i].y - hh && y < nodes[i].y + 3) return 1;
    }
    return 0;
}

// The node just in front of the player, or -1.
static int facing_node(void) {
    static const s8 dirs[4][2] = { { 0, 1 }, { 0, -1 }, { -1, 0 }, { 1, 0 } };
    int best = -1, bd = 1 << 30;
    for (int k = 4; k <= 16; k += 4) {
        int tx = px + dirs[face][0] * k, ty = py + dirs[face][1] * k - 2;
        for (int i = 0; i < nnodes; i++) {
            const Node* n = &nodes[i];
            if (n->type == N_GONE) continue;
            int hw, hh;
            footprint(n, &hw, &hh);
            if (tx > n->x - hw - 6 && tx < n->x + hw + 6 && ty > n->y - hh - 6 && ty < n->y + 4) {
                int d = (tx - n->x) * (tx - n->x) + (ty - n->y) * (ty - n->y);
                if (d < bd) { bd = d; best = i; }
            }
        }
        if (best >= 0) return best;
    }
    return -1;
}

static void say_note(const char* s) {
    note[0] = 0;
    cat(note, s);
    note_t = 120;
}

static void pop(int x, int y, const char* s) {
    int k = 0;
    for (int i = 1; i < 4; i++) if (pop_t[i] < pop_t[k]) k = i;
    pops[k][0] = 0;
    cat(pops[k], s);
    pop_x[k] = x; pop_y[k] = y; pop_t[k] = 60;
}

// Takes n items into the bag; returns how many fit.
static int take(int n) {
    int room = bag_size() - bag_used;
    if (n > room) n = room;
    if (n < 0) n = 0;
    bag_used += n;
    return n;
}

static void spend(int minutes) { day_min += minutes; }

static void pop_items(int x, int y, int n, const char* what) {
    char buf[24], *p = cat(buf, "+");
    p = cat_num(p, n);
    p = cat(p, " ");
    cat(p, what);
    pop(x, y, buf);
}

static void star_pop(int x, int y, int hue) {
    stock[hue]++;
    char buf[24], *p = cat(buf, "+1 ");
    p = cat(p, hue_names[hue]);
    cat(p, " STAR");
    pop(x, y, buf);
}

static int bag_full(void) {
    if (bag_used < bag_size()) return 0;
    say_note("MY BAG IS FULL. TIME TO HEAD HOME.");
    return 1;
}

static void act(int i) {
    Node* n = &nodes[i];
    int x = n->x, y = n->y - 20;
    switch (n->type) {
    case N_OAK:
    case N_BIG:
        if (n->type == N_BIG && gear[GEAR_AXE] < 2) { say_note("THIS OLD TREE NEEDS A STRONGER AXE."); return; }
        if (bag_full()) return;
        swing = 14;
        swing_node = i;
        spend(5);
        break;
    case N_BUSH:
        if (bag_full()) return;
        spend(10);
        n->type = N_BARE;
        if (take(1)) star_pop(x, y, (rnd() & 1) ? H_GREEN : H_LIME);
        break;
    case N_FLOWER:
        if (bag_full()) return;
        spend(5);
        n->type = N_GONE;
        if (take(1)) star_pop(x, y + 10, flower_hue[n->color]);
        break;
    case N_TWIG:
        spend(5);
        n->type = N_GONE;
        if ((rnd() & 1) && take(1)) { shells++; pop(x, y + 10, "+1 SHELL"); }
        else pop(x, y + 10, "JUST A TWIG");
        break;
    case N_WEEDS:
        spend(5);
        n->type = N_GONE;
        if (rnd() % 4 == 0 && take(1)) star_pop(x, y + 10, H_GREEN);
        else pop(x, y + 10, "CLEARED");
        break;
    case N_STONE:
        if (!(tools & TOOL_PICK)) { say_note("I NEED A PICKAXE FOR THAT. THE MARKET SELLS THEM."); return; }
        spend(5);
        n->type = N_GONE;
        if ((rnd() & 1) && take(1)) { powder++; pop(x, y + 10, "+1 POWDER"); }
        else pop(x, y + 10, "JUST STONE");
        break;
    case N_KILN:
        if (kiln_used) { say_note("THE KILN NEEDS TO COOL DOWN. TOMORROW."); return; }
        fstate = F_KILN;
        heat = 40; zone = 50; zone_v = 1; score = 0; kiln_t = 0;
        break;
    case N_STUMP:
    case N_BIGSTUMP:
        say_note("JUST A STUMP NOW. IT'LL GROW BACK.");
        break;
    case N_BARE:
        say_note("NOTHING LEFT ON THIS BUSH TODAY.");
        break;
    }
}

// A swing lands: chip the tree, and fell it when it runs out.
static void land_swing(void) {
    Node* n = &nodes[swing_node];
    int dmg = gear[GEAR_AXE];
    n->hp = n->hp > dmg ? n->hp - dmg : 0;
    if (n->hp) return;
    if (n->type == N_OAK) {
        n->type = N_STUMP;
        int got = take(2 + (gear[GEAR_AXE] >= 3));
        shells += got;
        pop_items(n->x, n->y - 30, got, got == 1 ? "SHELL" : "SHELLS");
    } else {
        n->type = N_BIGSTUMP;
        int got = take(5);
        shells += got;
        pop_items(n->x, n->y - 50, got, "SHELLS");
        if (rnd() % 100 < 35) {
            int pat = unlock_pattern();
            if (pat >= 0) {
                char buf[64], *p = cat(buf, "A SCROLL IN THE HOLLOW! NOW I CAN MAKE ");
                p = cat(p, pat_plural[pat]);
                cat(p, ".");
                say_note(buf);
            }
        }
    }
}

// --- the kiln: hold A to fan the fire, keep it inside the moving band ---------------

static void kiln_step(void) {
    kiln_t++;
    if (keys & KEY_A) heat += 2; else heat -= 1;
    if (heat < 0) heat = 0;
    if (heat > 100) heat = 100;
    zone += zone_v;                                   // the sweet spot drifts
    if (zone < 22 || zone > 78 || (rnd() % 90) == 0) zone_v = -zone_v;
    if (heat > zone - 10 && heat < zone + 10) score++;
    if (kiln_t >= 480) {                               // eight seconds
        int got = take(1 + score * 4 / 480 + (score > 400));
        powder += got;
        spend(30);
        kiln_used = 1;
        fstate = F_WALK;
        char buf[48], *p = cat(buf, score > 360 ? "PERFECT CHARCOAL! +" : score > 200 ? "GOOD CHARCOAL. +" : "A BIT SMOKY... +");
        p = cat_num(p, got);
        cat(p, " POWDER");
        say_note(buf);
    }
}

static void draw_kiln(u8* back) {
    box(back, 20, 30, 200, 80);
    text(back, 30, 36, "THE KILN", UI_YELLOW);
    text(back, 30, 46, "HOLD A TO FAN THE FIRE. KEEP", UI_WHITE);
    text(back, 30, 55, "THE HEAT IN THE GREEN.", UI_WHITE);
    int bx = 30, by = 72, bw = 180;
    fill_rect(back, bx, by, bw, 10, UI_SHADOW);
    fill_rect(back, bx + (zone - 10) * bw / 100, by, 20 * bw / 100, 10, UI_DK_GREEN);
    fill_rect(back, bx + heat * bw / 100 - 1, by - 3, 3, 16, UI_YELLOW);
    fill_rect(back, bx, by + 18, bw, 4, UI_SHADOW);
    fill_rect(back, bx, by + 18, bw * kiln_t / 480, 4, UI_RED);
}

// --- scene ---------------------------------------------------------------------------

void field_enter(void) {
    blank_once();
    for (int i = 0; i < MAP_PAL_N; i++) BG_PAL[16 + i] = map_pal[i];
    ui_palette();
    REG_BLDCNT = 0;
    REG_DISPCNT &= ~(1 << 13);
    vu32* t = (vu32*)OBJ_TILES;
    for (int i = 0; i < 6 * 8 * 8; i++) t[T_PLAYER * 8 + i] = player_tiles[i];
    for (int i = 0; i < (NT_SPARK + 1) * 8; i++) t[T_NATURE * 8 + i] = nature_tiles[i];
    for (unsigned i = 0; i < sizeof player_pal / 2; i++) OBJ_PAL[i] = player_pal[i];
    for (unsigned i = 0; i < sizeof nature_pal / 2; i++) OBJ_PAL[16 + i] = nature_pal[i];
    for (int i = 0; i < 6 * 16; i++) OBJ_PAL[32 + i] = flower_pals[i];
    if (nodes_day != night) { make_day(); nodes_day = night; }
    px = 120; py = 150; face = 1;
    swing = 0;
    note_t = 0;
    fstate = F_WALK;
    for (int i = 0; i < 4; i++) pop_t[i] = 0;
    say_note("THE FOREST. (A: CHOP, PICK, SEARCH. WALK DOWN THE PATH TO LEAVE.)");
}

static void leave(void) {
    day_return();
}

void field_update(u8* back) {
    if (fstate == F_KILN) kiln_step();
    else if (swing) {
        if (--swing == 4) land_swing();
    } else {
        int dx = 0, dy = 0;
        if (keys & KEY_LEFT) { dx = -1; face = 2; }
        else if (keys & KEY_RIGHT) { dx = 1; face = 3; }
        if (keys & KEY_UP) { dy = -1; if (!dx) face = 1; }
        else if (keys & KEY_DOWN) { dy = 1; if (!dx) face = 0; }
        moving = dx || dy;
        int sp = 1 + (frame & 1);                        // 1.5 px a frame
        for (int k = 0; k < sp; k++) {
            if (dx && !blocked(px + dx, py)) px += dx;
            if (dy && !blocked(px, py + dy)) py += dy;
        }
        if (moving) step_t++;
        if (py > 156) { leave(); return; }
        if (hit & KEY_A) {
            int i = facing_node();
            if (i >= 0) act(i);
        }
        if (hit & KEY_B) { leave(); return; }
    }
    if (day_min >= DAY_END_MIN) { day_min = DAY_END_MIN; leave(); return; }
    if (note_t) note_t--;

    copy_page(back, forest_bg);
    // Status bar.
    char buf[48], *p = day_clock(buf, day_min);
    text(back, 3, 2, buf, UI_YELLOW);
    text(back, 100, 2, "FOREST", UI_WHITE);
    p = cat(buf, "BAG ");
    p = cat_num(p, bag_used);
    p = cat(p, "/");
    cat_num(p, bag_size());
    text(back, 237 - text_w(buf), 2, buf, bag_used >= bag_size() ? UI_RED : UI_WHITE);
    for (int k = 0; k < 4; k++)
        if (pop_t[k]) {
            pop_t[k]--;
            int y = pop_y[k] - (60 - pop_t[k]) / 3;
            text(back, pop_x[k] - text_w(pops[k]) / 2, y, pops[k], UI_WHITE);
        }
    // Window 0 keeps sprites out of whichever box is open.
    int wx0 = 0, wy0 = 0, wx1 = 0, wy1 = 0;
    if (fstate == F_KILN) { draw_kiln(back); wx0 = 20; wy0 = 30; wx1 = 220; wy1 = 110; }
    else if (note_t) {
        char lines[2][41];
        int n = wrap(note, 37, lines, 2);
        box(back, 0, 136, 240, 24);
        for (int i = 0; i < n; i++) text(back, 8, 141 + 9 * i - (n == 1 ? -4 : 0), lines[i], UI_WHITE);
        wx0 = 0; wy0 = 136; wx1 = 240; wy1 = 160;
    }
    if (wx1) {
        REG_WIN0H = (wx0 << 8) | wx1;
        REG_WIN0V = (wy0 << 8) | wy1;
        REG_WININ = 1 << 2;            // inside: the bitmap only
        REG_WINOUT = (1 << 2) | (1 << 4);
        REG_DISPCNT |= 1 << 13;
    } else REG_DISPCNT &= ~(1 << 13);

    // Sprites, sorted so whatever stands lower on screen draws in front.
    int order[MAXNODES + 1], n = 0;
    for (int i = 0; i < nnodes; i++) if (nodes[i].type != N_GONE) order[n++] = i;
    order[n++] = -1;                                   // the player
    for (int a = 1; a < n; a++) {                      // insertion sort by ground y, front first
        int v = order[a], vy = v < 0 ? py : nodes[v].y, b = a - 1;
        while (b >= 0 && (order[b] < 0 ? py : nodes[order[b]].y) < vy) { order[b + 1] = order[b]; b--; }
        order[b + 1] = v;
    }
    if (swing) {                                       // the axe, in front of the player
        int ax = px + (face == 2 ? -14 : face == 3 ? 2 : -2), ay = py - 22 + (swing > 7 ? -4 : 2);
        sprite(ax, ay, T_NATURE + NT_AXE, 0, 1, 1);
    }
    for (int k = 0; k < n; k++) {
        int i = order[k];
        if (i < 0) {
            static const u8 base[4] = { 0, 2, 4, 4 };
            int fr = base[face] + (moving && ((step_t >> 3) & 1));
            int flip = face == 3 ? (1 << 12) : 0;
            sprite(px - 8, py - 31, T_PLAYER + fr * 8, 2, 2, 0);
            if (flip) sprite_flip_last();
            continue;
        }
        const Node* nd = &nodes[i];
        switch (nd->type) {
        case N_OAK: sprite(nd->x - 16, nd->y - 31, T_NATURE + NT_OAK, 0, 2, 1); break;
        case N_BIG: sprite(nd->x - 32, nd->y - 62, T_NATURE + NT_BIG, 0, 3, 1); break;
        case N_STUMP: sprite(nd->x - 8, nd->y - 15, T_NATURE + NT_STUMP, 0, 1, 1); break;
        case N_BIGSTUMP: sprite(nd->x - 8, nd->y - 15, T_NATURE + NT_STUMP, 0, 1, 1); break;
        case N_BUSH: sprite(nd->x - 8, nd->y - 15, T_NATURE + NT_BUSH, 0, 1, 1); break;
        case N_BARE: sprite(nd->x - 8, nd->y - 15, T_NATURE + NT_BARE, 0, 1, 1); break;
        case N_KILN: sprite(nd->x - 16, nd->y - 31, T_NATURE + NT_KILN, 0, 2, 1); break;
        case N_TWIG: sprite(nd->x - 8, nd->y - 13, T_NATURE + NT_TWIG, 0, 1, 1); break;
        case N_STONE: sprite(nd->x - 8, nd->y - 12, T_NATURE + NT_STONE, 0, 1, 1); break;
        case N_WEEDS: sprite(nd->x - 8, nd->y - 13, T_NATURE + NT_WEEDS, 0, 1, 1); break;
        case N_FLOWER:
            if ((frame + i * 13) % 90 < 12) sprite(nd->x + 2, nd->y - 18, T_NATURE + NT_SPARK, 0, 0, 1);
            sprite(nd->x - 8, nd->y - 14, T_NATURE + NT_FLOWER, 0, 1, 2 + nd->color);
            break;
        }
    }
}
