// Walk-around gathering, in the spirit of Stardew Valley. The forest: trees to chop,
// bushes to search, sparkling flowers and debris that come back each day, and the kiln.
// The mine: floors of rocks to break, one hiding the ladder down, with richer ore deeper.
// The quarry: a stepped stone pit with white stone, silver, and gold, and old crates.
// The coast: terraces of rock full of tide pools; shells, sea glass, coral, and amber wash
// up on the rocks, and driftwood floats past the rocky point.
// Walking is free; every action costs a few minutes of daylight.
#include "game.h"
#include "art.h"

#define T_PLAYER 512                   // 6 frames of 16x32 (8 tiles each)
#define T_NATURE 560
// Offsets into the nature tiles, in make_art.py's nature_words order.
enum { NT_OAK = 0, NT_BIG = 16, NT_STUMP = 80, NT_BUSH = 84, NT_BARE = 88, NT_KILN = 92, NT_TWIG = 108,
       NT_STONE = 112, NT_WEEDS = 116, NT_AXE = 120, NT_FLOWER = 124, NT_SPARK = 128 };
// In the mine the same tile slots hold the mine sprites instead.
enum { MT_ROCK = 0, MT_HARD = 4, MT_ORE = 8, MT_HOLE = 12, MT_PICK = 16, MT_SPARK = 20 };
enum { BT_SCALLOP = 0, BT_GLASS = 4, BT_CORAL = 8, BT_CRYSTAL = 12, BT_DRIFT = 16, BT_SPARK = 20 };
#define QT_CRATE 21                    // in the quarry, after the mine sprites

u8 gear[GEAR_COUNT];
int bag_used;                          // items picked up today (the bag holds bag_size())

int bag_size(void) { return 10 + 10 * gear[GEAR_BAG]; }   // 20, 30, 40

enum { N_OAK, N_BIG, N_STUMP, N_BIGSTUMP, N_BUSH, N_BARE, N_KILN, N_TWIG, N_STONE, N_WEEDS, N_FLOWER, N_GONE,
       N_ROCK, N_HARD, N_ORE, N_HOLE, N_UP, N_FORAGE, N_DRIFT, N_POOL, N_DOCK, N_QSTONE, N_CRATE, N_CRATE_OPEN };
typedef struct { u8 type, x, y, hp, color; } Node;   // x, y = where it meets the ground

#define MAXNODES 32
static Node nodes[MAXNODES];
static int nnodes;
static Node forest[MAXNODES];          // the forest keeps its state all day
static int nforest, forest_day = -1, kiln_used, geodes_day = -1;
static Node beach[MAXNODES];           // so does the beach
static int nbeach, beach_day = -1, hook_day = -1;
static Node quarry[MAXNODES];          // and the quarry
static int nquarry, quarry_day = -1, climb_day = -1;
enum { Q_WHITE, Q_SILVER, Q_GOLD };    // quarry stone kinds (palette banks 2, 3, 4)
static const u8 qstone_hue[3] = { H_WHITE, H_SILVER, H_GOLD };

// Beach finds by palette bank: pink, white, cyan, green, blue, red, orange, gold, yellow.
static const u8 find_hue[9] = { H_PINK, H_WHITE, H_CYAN, H_GREEN, H_BLUE, H_RED, H_ORANGE, H_GOLD, H_YELLOW };
static const char* const find_name[9] = { "SCALLOP", "WHITE SHELL", "SEA GLASS", "SEA GLASS", "SEA GLASS", "CORAL", "CORAL", "AMBER", "SALT CRYSTAL" };
static int find_tile(int c) { return c < 2 ? BT_SCALLOP : c < 5 ? BT_GLASS : c < 7 ? BT_CORAL : BT_CRYSTAL; }

int field_area;                        // FIELD_FOREST, FIELD_MINE, FIELD_GEODES
int mine_floor, mine_deepest;          // the floor you are on; the deepest you have reached
static int hole_rock;                  // the rock hiding the ladder down

// Ore colors (palette banks 2..8): sulfur, red, blue, green, cyan, violet, silver.
enum { ORE_SULFUR, ORE_RED, ORE_BLUE, ORE_GREEN, ORE_CYAN, ORE_VIOLET, ORE_SILVER };
static const u8 ore_hue[7] = { 0, H_RED, H_BLUE, H_GREEN, H_CYAN, H_VIOLET, H_SILVER };

// The flowers' colors, and the stars they turn into.
static const u8 flower_hue[6] = { H_RED, H_BLUE, H_YELLOW, H_VIOLET, H_PINK, H_ORANGE };

static int px, py, face, step_t, moving;   // player feet; face: 0 down, 1 up, 2 left, 3 right
static int swing, swing_node;               // axe swing frames left, and its target
static char note[96];                       // a short message at the bottom (2 lines)
static int note_t;
static char pops[4][24];                    // "+2 SHELLS" popping up over the spot
static s16 pop_x[4], pop_y[4], pop_t[4];
enum { F_WALK, F_KILN, F_GEODE, F_HOOK, F_CLIMB };
static int fstate;
static int heat, zone, zone_v, score, kiln_t;   // the kiln minigame
static int geode_n, geode_pos, geode_v, geode_wait;   // the geode minigame
static int hook_casts, hook_t, hook_got, hook_bottle, hook_caught; // the driftwood minigame
static int climb_h, climb_miss, climb_key, climb_t, climb_left, climb_flash;   // the climb
static s16 log_x[3], log_v[3];
static u8 log_kind[3];                                 // 0 none, 1 log, 2 bottle
static char geode_res[3][24];

static void leave_mine(void);
static void geode_start(void);
static void hook_start(void);
static void climb_start(void);

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

// A fresh mine floor: rocks everywhere, some ore (richer deeper), one rock hides the ladder.
static void make_floor(int f) {
    nnodes = 0;
    add_node(N_UP, 120, 52, 0);
    static const u8 pools[3][7] = {
        { ORE_SULFUR, ORE_SULFUR, ORE_RED, ORE_BLUE, ORE_SULFUR, ORE_RED, ORE_BLUE },
        { ORE_SULFUR, ORE_RED, ORE_BLUE, ORE_GREEN, ORE_CYAN, ORE_GREEN, ORE_CYAN },
        { ORE_RED, ORE_BLUE, ORE_GREEN, ORE_CYAN, ORE_VIOLET, ORE_SILVER, ORE_VIOLET },
    };
    const u8* pool = pools[f <= 3 ? 0 : f <= 6 ? 1 : 2];
    int rocks = 11 + rnd() % 5, ores = 2 + f / 3 + rnd() % 2;
    for (int tries = 0; tries < 300 && (rocks > 0 || ores > 0); tries++) {
        int x = 30 + rnd() % 180, y = 66 + rnd() % 84;
        if (x > 104 && x < 136 && y < 76) continue;         // room around the ladder up
        int ok = 1;
        for (int i = 0; i < nnodes && ok; i++) {
            int dx = nodes[i].x - x, dy = nodes[i].y - y;
            if (dx * dx + dy * dy < 19 * 19) ok = 0;
        }
        if (!ok) continue;
        if (ores > 0) {
            add_node(N_ORE, x, y, 3);
            nodes[nnodes - 1].color = pool[rnd() % 7];
            ores--;
        } else {
            int hard = f >= 4 && rnd() % 4 == 0;
            add_node(hard ? N_HARD : N_ROCK, x, y, hard ? 4 : 2);
            rocks--;
        }
    }
    // The ladder hides under a plain rock (none on the deepest floor).
    hole_rock = -1;
    if (f < MINE_FLOORS)
        for (int k = 0; k < 40 && hole_rock < 0; k++) {
            int i = 1 + rnd() % (nnodes - 1);
            if (nodes[i].type == N_ROCK) hole_rock = i;
        }
}

// The coast: the pools and the rocky point stay put; finds wash up fresh each day.
static void make_beach(void) {
    nnodes = 0;
    for (int i = 0; i < BEACH_POOLS; i++) add_node(N_POOL, beach_pools[i * 2], beach_pools[i * 2 + 1], 1);
    add_node(N_DOCK, HOOK_SPOT_X, HOOK_SPOT_Y, 0);            // the rocky point
    int finds = 8 + rnd() % 3, wood = 2 + rnd() % 2;
    for (int tries = 0; tries < 400 && (finds > 0 || wood > 0); tries++) {
        int x = 8 + rnd() % 224, y = 44 + rnd() % 100;
        if (x < 40 && y < 48) continue;                        // the stair
        if (!beach_walk[(y >> 3) * 30 + (x >> 3)] || !beach_walk[((y - 6) >> 3) * 30 + (x >> 3)]) continue;
        int ok = 1;
        for (int i = 0; i < nnodes && ok; i++) {
            int dx = nodes[i].x - x, dy = nodes[i].y - y;
            if (dx * dx + dy * dy < 20 * 20) ok = 0;
        }
        if (!ok) continue;
        if (finds > 0) { add_node(N_FORAGE, x, y, 0); nodes[nnodes - 1].color = rnd() % 9; finds--; }
        else { add_node(N_DRIFT, x, y, 0); wood--; }
    }
}

// The quarry: stone, silver (deeper down), gold flecks, plain boulders, and old crates on the rim.
static void make_quarry(void) {
    nnodes = 0;
    int stones = 12 + rnd() % 3, crates = 3;
    for (int tries = 0; tries < 300 && (stones > 0 || crates > 0); tries++) {
        int k = rnd() % QUARRY_SPOTS;
        int x = quarry_spots[k * 3], y = quarry_spots[k * 3 + 1], lvl = quarry_spots[k * 3 + 2];
        int ok = 1;
        for (int i = 0; i < nnodes && ok; i++) {
            int dx = nodes[i].x - x, dy = nodes[i].y - y;
            if (dx * dx + dy * dy < 18 * 18) ok = 0;
        }
        if (!ok) continue;
        if (crates > 0 && lvl == 3) { add_node(N_CRATE, x, y, 1); crates--; continue; }
        if (stones <= 0 || lvl == 3) continue;
        int r = rnd() % 10;
        if (lvl <= 1 && r < 3) { add_node(N_QSTONE, x, y, 4); nodes[nnodes - 1].color = Q_SILVER; }
        else if (r < 6) { add_node(N_QSTONE, x, y, 3); nodes[nnodes - 1].color = Q_WHITE; }
        else if (r < 8) { add_node(N_QSTONE, x, y, 3); nodes[nnodes - 1].color = Q_GOLD; }
        else add_node(N_ROCK, x, y, 2);
        stones--;
    }
}

// Each kind's solid footprint (half width, height above the ground point).
static void footprint(const Node* n, int* hw, int* hh) {
    switch (n->type) {
    case N_OAK: case N_STUMP: *hw = 6; *hh = 6; break;
    case N_BIG: case N_BIGSTUMP: *hw = 12; *hh = 8; break;
    case N_HOLE: *hw = 0; *hh = 0; break;               // walk over the hole
    case N_POOL: *hw = 12; *hh = 14; break;
    case N_CRATE: case N_CRATE_OPEN: *hw = 6; *hh = 6; break;
    case N_DOCK: *hw = 0; *hh = 0; break;
    case N_UP: *hw = 8; *hh = 6; break;
    case N_KILN: *hw = 12; *hh = 8; break;
    case N_GONE: *hw = 0; *hh = 0; break;
    default: *hw = 6; *hh = 5; break;
    }
}

static int blocked(int x, int y) {
    if (field_area == FIELD_MINE) {
        if (x < 24 || x > 216 || y < 58 || y > 152) return 1;
    } else if (field_area == FIELD_QUARRY) {
        if (x < 4 || x > 235 || y < 14 || y > 156) return 1;
        for (int dx = -3; dx <= 3; dx += 3) {          // walls, the pond, the shed (the picture's mask)
            int i = y * 240 + x + dx;
            if (!((quarry_mask[i >> 3] >> (i & 7)) & 1)) return 1;
        }
    } else if (field_area == FIELD_BEACH) {
        if (x < 4 || x > 235 || y < 16 || y > 155) return 1;
        // The picture's walk mask: cliff faces, level edges, the sea, and pools are walls.
        for (int dx = -3; dx <= 3; dx += 3) {
            int xx = x + dx, i = y * 240 + xx;
            if (!((beach_mask[i >> 3] >> (i & 7)) & 1)) return 1;
        }
    } else {
        if (x < 22 || x > 218 || y < 44) return 1;
        if (y > 154 && (x < 106 || x > 134)) return 1;
    }
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
    unsigned k = 0;
    while (s[k] && k < sizeof note - 1) { note[k] = s[k]; k++; }   // never past the buffer
    note[k] = 0;
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
        if (!gear[GEAR_PICK]) { say_note("I NEED A PICKAXE FOR THAT. THE MARKET SELLS THEM."); return; }
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
    case N_ROCK:
    case N_HARD:
    case N_ORE:
    case N_QSTONE:
        if (!gear[GEAR_PICK]) { say_note("I NEED A PICKAXE. THE MARKET SELLS THEM."); return; }
        if (n->type == N_HARD && gear[GEAR_PICK] < 2) { say_note("THIS ROCK IS TOO HARD. I NEED A BETTER PICKAXE."); return; }
        if (n->type == N_QSTONE && n->color == Q_SILVER && gear[GEAR_PICK] < 2) { say_note("SILVER IS TOUGH. I NEED A BETTER PICKAXE."); return; }
        swing = 14;
        swing_node = i;
        spend(5);
        break;
    case N_UP:
        leave_mine();
        break;
    case N_FORAGE:
        if (bag_full()) return;
        spend(5);
        n->type = N_GONE;
        if (take(1)) {
            stock[find_hue[n->color]]++;
            char buf[24], *p = cat(buf, "+1 ");
            cat(p, hue_names[find_hue[n->color]]);
            pop(x, y + 10, buf);
            say_note(find_name[n->color]);
        }
        break;
    case N_DRIFT:
        if (bag_full()) return;
        spend(5);
        n->type = N_GONE;
        { int got = take(1 + (rnd() & 1)); shells += got; pop_items(x, y + 10, got, got == 1 ? "SHELL" : "SHELLS"); }
        break;
    case N_POOL:
        if (!(tools & TOOL_NET)) { say_note("THINGS GLINT IN THE POOL. A NET WOULD REACH THEM."); return; }
        if (!n->hp) { say_note("THIS POOL IS EMPTY UNTIL THE NEXT TIDE."); return; }
        if (bag_full()) return;
        spend(10);
        n->hp = 0;
        {
            static const u8 pool_hues[3] = { H_PINK, H_CYAN, H_BLUE };
            int h = pool_hues[rnd() % 3];
            int got = take(1 + (rnd() & 1));
            stock[h] += got;
            char buf[24], *p = cat(buf, "+");
            p = cat_num(p, got);
            p = cat(p, " ");
            cat(p, hue_names[h]);
            pop(x, y - 10, buf);
            if (rnd() % 100 < 25) {
                int pat = unlock_pattern();
                if (pat >= 0) {
                    char m[80], *q = cat(m, "A BOTTLE WITH A SCROLL! NOW I CAN MAKE ");
                    q = cat(q, pat_plural[pat]);
                    cat(q, ".");
                    say_note(m);
                }
            }
        }
        break;
    case N_DOCK:
        if (hook_day == night) { say_note("ENOUGH FISHING FOR DRIFTWOOD TODAY."); return; }
        hook_start();
        break;
    case N_CRATE:
        spend(10);
        n->type = N_CRATE_OPEN;
        if (rnd() % 100 < 30) {
            int pat = unlock_pattern();
            if (pat >= 0) {
                char m[80], *q = cat(m, "AN OLD WORKSHOP SCROLL! NOW I CAN MAKE ");
                q = cat(q, pat_plural[pat]);
                cat(q, ".");
                say_note(m);
                break;
            }
        }
        if (take(1)) {
            static const u8 odd[3] = { H_PINK, H_VIOLET, H_SILVER };
            star_pop(x, y + 6, odd[rnd() % 3]);
        }
        break;
    case N_CRATE_OPEN:
        say_note("JUST SAWDUST LEFT IN THIS ONE.");
        break;
    case N_BARE:
        say_note("NOTHING LEFT ON THIS BUSH TODAY.");
        break;
    }
}

// A swing lands: chip the tree or rock, and fell or break it when it runs out.
static void land_swing(void) {
    Node* n = &nodes[swing_node];
    int dmg = field_area == FIELD_MINE ? gear[GEAR_PICK] : gear[GEAR_AXE];
    n->hp = n->hp > dmg ? n->hp - dmg : 0;
    if (n->hp) return;
    if (n->type == N_ROCK || n->type == N_HARD) {
        if (swing_node == hole_rock) {
            n->type = N_HOLE;
            pop(n->x, n->y - 20, "A LADDER!");
            return;
        }
        n->type = N_GONE;
        if (rnd() % 100 < 35 && take(1)) { powder++; pop(n->x, n->y - 20, "+1 POWDER"); }
        return;
    }
    if (n->type == N_QSTONE) {
        n->type = N_GONE;
        int h = qstone_hue[n->color];
        int got = take(n->color == Q_SILVER ? 1 + (gear[GEAR_PICK] >= 3) : 1 + (rnd() & 1));
        stock[h] += got;
        char buf[24], *p = cat(buf, "+");
        p = cat_num(p, got);
        p = cat(p, " ");
        cat(p, hue_names[h]);
        pop(n->x, n->y - 20, buf);
        return;
    }
    if (n->type == N_ORE) {
        n->type = N_GONE;
        if (n->color == ORE_SULFUR) {
            int got = take(3);
            powder += got;
            pop_items(n->x, n->y - 20, got, "POWDER");
        } else {
            int got = take(1 + (gear[GEAR_PICK] >= 3));
            stock[ore_hue[n->color]] += got;
            char buf[24], *p = cat(buf, "+");
            p = cat_num(p, got);
            p = cat(p, " ");
            cat(p, hue_names[ore_hue[n->color]]);
            pop(n->x, n->y - 20, buf);
        }
        return;
    }
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
        int got = take(2 + score * 4 / 480 + (score > 400));
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

// --- the geodes: stop the swinging pointer on the crack ------------------------------

static void geode_start(void) {
    fstate = F_GEODE;
    geode_n = 0; geode_pos = 0; geode_v = 2; geode_wait = 0;
}

static void geode_step(void) {
    if (geode_wait) {
        if (--geode_wait == 0 && ++geode_n < 3) { geode_pos = 0; geode_v = 2 + geode_n; }
        if (geode_n >= 3 && geode_wait == 0) {
            spend(30);
            geodes_day = night;
            say_note("THAT'S ALL THE GEODES FOR TODAY.");
            fstate = F_WALK;
        }
        return;
    }
    geode_pos += geode_v;
    if (geode_pos <= 0 || geode_pos >= 100) geode_v = -geode_v;
    if (hit & KEY_A) {
        int d = geode_pos > 50 ? geode_pos - 50 : 50 - geode_pos;
        char* p = geode_res[geode_n];
        static const u8 rare[3] = { H_VIOLET, H_SILVER, H_CYAN }, common[3] = { H_RED, H_BLUE, H_GREEN };
        if (d < 5 && take(1)) {
            int h = rare[rnd() % 3];
            stock[h]++;
            p = cat(p, "PERFECT! 1 ");
            cat(p, hue_names[h]);
        } else if (d < 18 && take(1)) {
            int h = common[rnd() % 3];
            stock[h]++;
            p = cat(p, "GOOD. 1 ");
            cat(p, hue_names[h]);
        } else {
            if (take(1)) powder++;
            cat(p, "IT CRUMBLED. 1 POWDER");
        }
        geode_wait = 50;
    }
}

static void draw_geode(u8* back) {
    box(back, 16, 24, 208, 100);
    text(back, 26, 30, "CRACK THE GEODES", UI_YELLOW);
    text(back, 26, 40, "PRESS A WHEN THE POINTER IS ON", UI_WHITE);
    text(back, 26, 49, "THE CRACK. DEAD CENTER IS RAREST.", UI_WHITE);
    int bx = 30, by = 66, bw = 180;
    fill_rect(back, bx, by, bw, 10, UI_SHADOW);
    fill_rect(back, bx + 32 * bw / 100, by, 36 * bw / 100, 10, UI_DK_GREEN);
    fill_rect(back, bx + 45 * bw / 100, by, 10 * bw / 100, 10, UI_YELLOW);
    fill_rect(back, bx + geode_pos * bw / 100 - 1, by - 3, 3, 16, UI_WHITE);
    for (int k = 0; k < 3; k++) {
        char buf[48], *p = cat(buf, "GEODE ");
        p = cat_num(p, k + 1);
        p = cat(p, ": ");
        cat(p, k < geode_n || (k == geode_n && geode_wait) ? geode_res[k] : "...");
        text(back, 26, 86 + 10 * k, buf, k < geode_n || (k == geode_n && geode_wait) ? UI_GREEN : UI_GRAY);
    }
}

// --- driftwood: drop the hook as the logs float past ---------------------------------

#define HOOK_X 120
#define HOOK_CASTS 6

static void hook_start(void) {
    fstate = F_HOOK;
    hook_casts = HOOK_CASTS; hook_t = 0; hook_got = 0; hook_bottle = 0;
    for (int k = 0; k < 3; k++) { log_kind[k] = 0; log_x[k] = -40 - k * 70; }
}

static void hook_step(void) {
    for (int k = 0; k < 3; k++) {                         // logs drift by, left to right
        if (!log_kind[k]) {
            if (rnd() % 30 == 0) { log_kind[k] = rnd() % 8 == 0 ? 2 : 1; log_x[k] = 28; log_v[k] = 1 + rnd() % 2; }
            continue;
        }
        log_x[k] += log_v[k];
        if (log_x[k] > 212) log_kind[k] = 0;
    }
    if (hook_t) {
        // While the hook is in the water, it snags the first log or bottle it touches.
        if (!hook_caught && hook_t <= 16 && hook_t >= 4)
            for (int k = 0; k < 3; k++)
                if (log_kind[k] && HOOK_X >= log_x[k] - 3 && HOOK_X <= log_x[k] + 18) {
                    if (log_kind[k] == 2) hook_bottle = 1;
                    else hook_got++;
                    log_kind[k] = 0;
                    hook_caught = 1;
                    break;
                }
        if (--hook_t == 0 && hook_casts == 0) {           // the last cast is back
            int got = take(hook_got);
            shells += got;
            spend(30);
            hook_day = night;
            fstate = F_WALK;
            char buf[80], *p = cat(buf, "CAUGHT ");
            p = cat_num(p, got);
            p = cat(p, got == 1 ? " PIECE OF DRIFTWOOD: +" : " PIECES OF DRIFTWOOD: +");
            p = cat_num(p, got);
            p = cat(p, got == 1 ? " SHELL." : " SHELLS.");
            if (hook_bottle) {
                int pat = unlock_pattern();
                if (pat >= 0) { p = cat(p, " AND A BOTTLE: "); p = cat(p, pat_plural[pat]); cat(p, "!"); }
            }
            say_note(buf);
        }
        return;
    }
    if ((hit & KEY_A) && hook_casts) {                    // drop the hook
        hook_casts--;
        hook_t = 20;
        hook_caught = 0;
    }
}

static void draw_hook(u8* back) {
    box(back, 16, 20, 208, 104);
    text(back, 26, 26, "CATCH DRIFTWOOD", UI_YELLOW);
    text(back, 26, 36, "PRESS A TO DROP THE HOOK ON A LOG.", UI_WHITE);
    char buf[32], *p = cat(buf, "CASTS LEFT ");
    p = cat_num(p, hook_casts);
    p = cat(p, "   CAUGHT ");
    cat_num(p, hook_got + hook_bottle);
    text(back, 26, 46, buf, UI_WHITE);
    fill_rect(back, 24, 74, 192, 40, UI_INK);             // the water
    for (int x = 24; x < 216; x += 12) fill_rect(back, x + ((frame >> 3) % 12), 80 + (x / 12 % 3) * 10, 4, 1, UI_GRAY);
    for (int k = 0; k < 3; k++) {
        if (!log_kind[k]) continue;
        if (log_kind[k] == 1) { fill_rect(back, log_x[k], 90, 16, 6, UI_GOLD_DK); fill_rect(back, log_x[k], 90, 16, 2, UI_GOLD); }
        else { fill_rect(back, log_x[k] + 4, 86, 6, 10, UI_GREEN); fill_rect(back, log_x[k] + 6, 83, 2, 3, UI_GOLD_DK); }
    }
    int depth = hook_t ? (hook_t > 10 ? 20 - hook_t : hook_t) * 3 : 0;   // down and back up
    fill_rect(back, HOOK_X, 60, 1, 18 + depth, UI_WHITE);
    fill_rect(back, HOOK_X - 2, 76 + depth, 5, 3, UI_YELLOW);
}

// --- the climb: press the handhold that lights up, before it fades ------------------

#define CLIMB_TOP 10
static const u16 climb_keys[3] = { KEY_L, KEY_R, KEY_UP };
static const char* const climb_names[3] = { "L", "R", "UP" };

static void climb_next(void) {
    climb_key = rnd() % 3;
    climb_left = 70 - climb_h * 4;                         // less time the higher you get
}

static void climb_start(void) {
    fstate = F_CLIMB;
    climb_h = 0; climb_miss = 0; climb_t = 0; climb_flash = 0;
    climb_next();
}

static void climb_end(void) {
    char buf[96], *p = buf;
    int got = 0;
    if (climb_h >= 4 && take(1)) { stock[H_WHITE]++; got++; }
    if (climb_h >= 7 && take(1)) { stock[H_SILVER]++; got++; }
    if (climb_h >= CLIMB_TOP && take(1)) { stock[H_GOLD]++; got++; }
    if (climb_h >= CLIMB_TOP) p = cat(p, "MADE IT TO THE TOP! ");
    else if (climb_h >= 4) p = cat(p, "A GOOD CLIMB. ");
    else p = cat(p, "I SLIPPED BACK DOWN. ");
    if (got) {
        p = cat(p, "FOUND ");
        p = cat_num(p, got);
        p = cat(p, got == 1 ? " STAR IN THE CRACKS." : " STARS IN THE CRACKS.");
    }
    if (climb_h >= CLIMB_TOP && rnd() % 100 < 35) {
        int pat = unlock_pattern();
        if (pat >= 0) { p = cat(p, " AN EAGLE'S NEST HELD A SCROLL: "); p = cat(p, pat_plural[pat]); cat(p, "!"); }
    }
    spend(30);
    climb_day = night;
    fstate = F_WALK;
    say_note(buf);
}

static void climb_step(void) {
    climb_t++;
    if (climb_flash) climb_flash--;
    u16 press = hit & (KEY_L | KEY_R | KEY_UP);
    if (press) {
        if (press == climb_keys[climb_key]) { climb_h++; climb_flash = 8; }
        else { climb_miss++; if (climb_h) climb_h--; }
        if (climb_h >= CLIMB_TOP || climb_miss >= 3) { climb_end(); return; }
        climb_next();
    } else if (--climb_left <= 0) {                        // too slow: slip a hold
        climb_miss++;
        if (climb_h) climb_h--;
        if (climb_miss >= 3) { climb_end(); return; }
        climb_next();
    }
}

static void draw_climb(u8* back) {
    box(back, 16, 20, 208, 104);
    text(back, 26, 26, "CLIMB THE CLIFF", UI_YELLOW);
    text(back, 26, 36, "PRESS THE HOLD SHOWN.", UI_WHITE);
    char buf[32], *p = cat(buf, "SLIPS ");
    p = cat_num(p, climb_miss);
    cat(p, " OF 3");
    text(back, 26, 46, buf, climb_miss ? UI_RED : UI_WHITE);
    // The cliff, with the climber's height and the next three prizes.
    fill_rect(back, 180, 30, 24, 88, UI_GOLD_DK);
    for (int k = 0; k < 8; k++) fill_rect(back, 182 + (k * 7) % 18, 34 + k * 10, 4, 2, UI_SHADOW);
    int cy = 112 - climb_h * 8;
    fill_rect(back, 188, cy - 6, 8, 8, climb_flash ? UI_YELLOW : UI_RED);
    fill_rect(back, 176, 112 - 4 * 8, 4, 2, UI_WHITE);
    fill_rect(back, 176, 112 - 7 * 8, 4, 2, UI_GRAY);
    fill_rect(back, 176, 112 - 10 * 8, 4, 2, UI_YELLOW);
    // The hold to press, big, with its time running out.
    text_big(back, 70 - text_w(climb_names[climb_key]), 66, climb_names[climb_key], UI_YELLOW);
    int full = 70 - climb_h * 4;
    fill_rect(back, 30, 100, 120, 5, UI_SHADOW);
    fill_rect(back, 30, 100, 120 * climb_left / (full > 0 ? full : 1), 5, UI_GREEN);
}

// --- scene ---------------------------------------------------------------------------

// Deeper floors get darker; the BG palette is the map's, dimmed.
static void mine_light(void) {
    int pct = 100 - (mine_floor - 1) * 5;
    for (int i = 0; i < MAP_PAL_N; i++) {
        u16 c = map_pal[i];
        int r = (c & 31) * pct / 100, g = ((c >> 5) & 31) * pct / 100, b = ((c >> 10) & 31) * pct / 100;
        BG_PAL[16 + i] = RGB(r, g, b);
    }
}

static void floor_note(void) {
    char buf[96], *p = cat(buf, "MINE FLOOR ");
    p = cat_num(p, mine_floor);
    cat(p, mine_floor == 1 ? ". (A: MINE ROCKS. THE LADDER UP LEAVES.)" : ".");
    say_note(buf);
}

int geodes_done(void) { return geodes_day == night; }
int climb_done(void) { return climb_day == night; }

void field_go(int area) {
    field_area = area;
    mine_floor = 1;
    set_scene(SC_FIELD);
}

void field_enter(void) {
    blank_once();
    for (int i = 0; i < MAP_PAL_N; i++) BG_PAL[16 + i] = map_pal[i];
    ui_palette();
    REG_BLDCNT = 0;
    REG_DISPCNT &= ~(1 << 13);
    vu32* t = (vu32*)OBJ_TILES;
    for (int i = 0; i < 6 * 8 * 8; i++) t[T_PLAYER * 8 + i] = player_tiles[i];
    for (unsigned i = 0; i < sizeof player_pal / 2; i++) OBJ_PAL[i] = player_pal[i];
    for (unsigned i = 0; i < sizeof nature_pal / 2; i++) OBJ_PAL[16 + i] = nature_pal[i];
    swing = 0;
    note_t = 0;
    fstate = F_WALK;
    for (int i = 0; i < 4; i++) pop_t[i] = 0;
    if (field_area == FIELD_FOREST) {
        for (int i = 0; i < (NT_SPARK + 1) * 8; i++) t[T_NATURE * 8 + i] = nature_tiles[i];
        for (int i = 0; i < 6 * 16; i++) OBJ_PAL[32 + i] = flower_pals[i];
        if (forest_day != night) { make_day(); forest_day = night; nforest = nnodes; for (int i = 0; i < nnodes; i++) forest[i] = nodes[i]; }
        nnodes = nforest;
        for (int i = 0; i < nnodes; i++) nodes[i] = forest[i];
        px = 120; py = 150; face = 1;
        say_note("THE FOREST. (A: CHOP, PICK, SEARCH. WALK DOWN THE PATH TO LEAVE.)");
    } else if (field_area == FIELD_QUARRY || field_area == FIELD_CLIMB) {
        for (int i = 0; i < (MT_SPARK + 1) * 8; i++) t[T_NATURE * 8 + i] = mine_tiles[i];
        for (int i = 0; i < 4 * 8; i++) t[(T_NATURE + QT_CRATE) * 8 + i] = crate_tiles[i];
        for (int i = 0; i < 16; i++) {                       // stone banks: white, silver, gold
            OBJ_PAL[32 + i] = beach_pals[1 * 16 + i];
            OBJ_PAL[48 + i] = ore_pals[6 * 16 + i];
            OBJ_PAL[64 + i] = beach_pals[7 * 16 + i];
        }
        if (field_area == FIELD_CLIMB) { nnodes = 0; px = 211; py = 40; face = 0; climb_start(); return; }
        if (quarry_day != night) { make_quarry(); quarry_day = night; nquarry = nnodes; for (int i = 0; i < nnodes; i++) quarry[i] = nodes[i]; }
        nnodes = nquarry;
        for (int i = 0; i < nnodes; i++) nodes[i] = quarry[i];
        hole_rock = -1;
        px = 211; py = 40; face = 0;
        say_note("THE QUARRY. (A: BREAK STONE, SEARCH CRATES. THE ROPE LADDER LEADS OUT.)");
    } else if (field_area == FIELD_BEACH) {
        for (int i = 0; i < BT_SPARK * 8; i++) t[T_NATURE * 8 + i] = beach_tiles[i];
        for (int i = 0; i < 8; i++) t[(T_NATURE + BT_SPARK) * 8 + i] = nature_tiles[NT_SPARK * 8 + i];
        for (int i = 0; i < 9 * 16; i++) OBJ_PAL[32 + i] = beach_pals[i];
        if (beach_day != night) { make_beach(); beach_day = night; nbeach = nnodes; for (int i = 0; i < nnodes; i++) beach[i] = nodes[i]; }
        nnodes = nbeach;
        for (int i = 0; i < nnodes; i++) nodes[i] = beach[i];
        px = 20; py = 40; face = 0;
        say_note("THE TIDE POOLS. (A: PICK UP FINDS, DIP INTO POOLS WITH A NET. FISH FROM THE ROCKY POINT.)");
    } else {
        for (int i = 0; i < (MT_SPARK + 1) * 8; i++) t[T_NATURE * 8 + i] = mine_tiles[i];
        for (int i = 0; i < 7 * 16; i++) OBJ_PAL[32 + i] = ore_pals[i];
        if (field_area == FIELD_GEODES) { nnodes = 0; px = 120; py = 140; face = 1; geode_start(); return; }
        mine_light();
        make_floor(mine_floor);
        px = 120; py = 70; face = 0;
        floor_note();
    }
}

static void leave(void) {
    if (field_area == FIELD_FOREST) { nforest = nnodes; for (int i = 0; i < nnodes; i++) forest[i] = nodes[i]; }
    if (field_area == FIELD_BEACH) { nbeach = nnodes; for (int i = 0; i < nnodes; i++) beach[i] = nodes[i]; }
    if (field_area == FIELD_QUARRY) { nquarry = nnodes; for (int i = 0; i < nnodes; i++) quarry[i] = nodes[i]; }
    day_return();
}

static void leave_mine(void) { leave(); }

// Standing on the open ladder hole: down to the next floor.
static void descend(void) {
    if (mine_floor + 1 >= MINE_DARK && !gear[GEAR_LANTERN]) {
        if (!note_t) say_note("IT'S PITCH BLACK DOWN THERE. I NEED A LANTERN.");
        return;
    }
    mine_floor++;
    if (mine_floor > mine_deepest) mine_deepest = mine_floor;
    spend(10);
    mine_light();
    make_floor(mine_floor);
    px = 120; py = 70; face = 0;
    for (int i = 0; i < 4; i++) pop_t[i] = 0;
    floor_note();
}

void field_update(u8* back) {
    if (fstate == F_KILN) kiln_step();
    else if (fstate == F_GEODE) geode_step();
    else if (fstate == F_HOOK) hook_step();
    else if (fstate == F_CLIMB) climb_step();
    else if (field_area == FIELD_GEODES || field_area == FIELD_CLIMB) {
        if (hit & (KEY_A | KEY_B)) { leave(); return; }
    } else if (swing) {
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
        if (field_area == FIELD_BEACH && py < 20 && px < 34) { leave(); return; }   // up the stair
        if (field_area == FIELD_QUARRY && py < 22 && px > 200) { leave(); return; }  // up the rope ladder
        if (field_area == FIELD_MINE)                    // step onto an open ladder hole
            for (int i = 0; i < nnodes; i++)
                if (nodes[i].type == N_HOLE && px > nodes[i].x - 6 && px < nodes[i].x + 6 && py > nodes[i].y - 8 && py < nodes[i].y + 2)
                    descend();
        if (hit & KEY_A) {
            int i = facing_node();
            if (i >= 0) act(i);
        }
        if (hit & KEY_B && field_area != FIELD_MINE) { leave(); return; }
    }
    if (day_min >= DAY_END_MIN) { day_min = DAY_END_MIN; leave(); return; }
    if (note_t) note_t--;

    copy_page(back, field_area == FIELD_FOREST ? forest_bg : field_area == FIELD_BEACH ? beach_bg
                    : field_area == FIELD_QUARRY || field_area == FIELD_CLIMB ? quarry_bg : mine_bg);
    // Status bar.
    char buf[48], *p = day_clock(buf, day_min);
    text(back, 3, 2, buf, UI_YELLOW);
    if (field_area == FIELD_FOREST) text(back, 100, 2, "FOREST", UI_WHITE);
    else if (field_area == FIELD_BEACH) text(back, 85, 2, "TIDE POOLS", UI_WHITE);
    else if (field_area == FIELD_QUARRY || field_area == FIELD_CLIMB) text(back, 100, 2, "QUARRY", UI_WHITE);
    else {
        p = cat(buf, "MINE ");
        cat_num(p, mine_floor);
        text(back, 100, 2, field_area == FIELD_GEODES ? "MINE" : buf, UI_WHITE);
    }
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
    else if (fstate == F_GEODE) { draw_geode(back); wx0 = 16; wy0 = 24; wx1 = 224; wy1 = 124; }
    else if (fstate == F_HOOK) { draw_hook(back); wx0 = 16; wy0 = 20; wx1 = 224; wy1 = 124; }
    else if (fstate == F_CLIMB) { draw_climb(back); wx0 = 16; wy0 = 20; wx1 = 224; wy1 = 124; }
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
        sprite(ax, ay, T_NATURE + (field_area == FIELD_FOREST ? NT_AXE : MT_PICK), 0, 1, 1);
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
        if (field_area == FIELD_BEACH) {
            switch (nd->type) {
            case N_FORAGE:
                if ((frame + i * 13) % 90 < 12) sprite(nd->x + 2, nd->y - 16, T_NATURE + BT_SPARK, 0, 0, 1);
                sprite(nd->x - 8, nd->y - 12, T_NATURE + find_tile(nd->color), 0, 1, 2 + nd->color);
                break;
            case N_DRIFT: sprite(nd->x - 8, nd->y - 11, T_NATURE + BT_DRIFT, 0, 1, 1); break;
            case N_POOL:                                   // the pool itself is in the picture
                if (nd->hp && (frame + i * 29) % 70 < 14) sprite(nd->x - 2, nd->y - 12, T_NATURE + BT_SPARK, 0, 0, 1);
                break;
            case N_DOCK:
                if (hook_day != night && (frame & 32)) sprite(nd->x - 3, nd->y - 4, T_NATURE + BT_SPARK, 0, 0, 1);
                break;
            }
            continue;
        }
        if (field_area == FIELD_QUARRY) {
            switch (nd->type) {
            case N_ROCK: sprite(nd->x - 8, nd->y - 12, T_NATURE + MT_ROCK, 0, 1, 1); break;
            case N_QSTONE:
                if ((frame + i * 17) % 100 < 10) sprite(nd->x + 2, nd->y - 16, T_NATURE + MT_SPARK, 0, 0, 1);
                sprite(nd->x - 8, nd->y - 12, T_NATURE + MT_ORE, 0, 1, 2 + nd->color);
                break;
            case N_CRATE: case N_CRATE_OPEN: sprite(nd->x - 8, nd->y - 12, T_NATURE + QT_CRATE, 0, 1, 1); break;
            }
            continue;
        }
        if (field_area != FIELD_FOREST) {
            switch (nd->type) {
            case N_ROCK: sprite(nd->x - 8, nd->y - 12, T_NATURE + MT_ROCK, 0, 1, 1); break;
            case N_HARD: sprite(nd->x - 8, nd->y - 12, T_NATURE + MT_HARD, 0, 1, 1); break;
            case N_ORE:
                if ((frame + i * 17) % 100 < 10) sprite(nd->x + 2, nd->y - 16, T_NATURE + MT_SPARK, 0, 0, 1);
                sprite(nd->x - 8, nd->y - 12, T_NATURE + MT_ORE, 0, 1, 2 + nd->color);
                break;
            case N_HOLE: sprite(nd->x - 8, nd->y - 12, T_NATURE + MT_HOLE, 0, 1, 1); break;
            }
            continue;
        }
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
