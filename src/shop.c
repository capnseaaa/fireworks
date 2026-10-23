// The night shop: customers come to the counter with requests, you build fireworks
// from quick menus, and each one costs time on the night clock. At closing time the
// crate goes up in the night's show.
#include "game.h"
#include "art.h"

#define T_CUST   512    // 4 styles x 32 tiles (32x64 sprites)
#define T_BUBBLE 640    // BUB_COUNT bubbles x 4 tiles (16x16)

#define CLOSE_MIN 300   // minutes after 8:00 PM: closes at 1:00 AM
#define ARRIVE_MIN 10   // time between customers
#define CUST_Y 34       // customer sprite top; its bottom row meets the counter

Shell crate[MAXCRATE];
int ncrate;
int night, coins, rep, powder, shells;
int stock[NCOLORS];
u16 known;
int night_made, night_happy, night_coins, night_rep;

const u8 size_time[3] = { 15, 25, 35 };
const u8 pat_time[P_COUNT] = { 0, 10, 15, 15, 10, 20, 25, 15, 15 };
const u8 size_powder[3] = { 1, 2, 3 };
const u8 size_pay[3] = { 8, 15, 25 };
static const u8 start_stock[NCOLORS] = { 3, 0, 3, 0, 0, 3, 0, 3, 0, 2, 2, 0 };
#define START_POWDER 12
#define START_SHELLS 8

const char* const cust_names[] = {
    "MABEL", "OTTO", "JUNE", "FINN", "ROSA", "ELI", "IVY", "TOBY", "NELL", "GUS",
    "PIP", "WREN", "ARLO", "HAZEL", "MILO", "CLEO", "BEA", "REMY", "SAGE", "OLIVE",
    "FELIX", "NORA", "TEO", "LUNA", "ABE", "DOT", "KAI", "MAE", "RUBY", "SAM",
};
#define NNAMES 30

void new_game(void) {
    night = 1;
    coins = 0;
    rep = 0;
    powder = START_POWDER;
    shells = START_SHELLS;
    for (int i = 0; i < NCOLORS; i++) stock[i] = start_stock[i];
    known = (1 << P_PEONY) | (1 << P_CHRYS) | (1 << P_WILLOW) | (1 << P_RING);
    ncrate = 0;
}

// No day phase yet: each new night tops materials back up to the starting amounts.
void next_night(void) {
    night++;
    for (int i = 0; i < NCOLORS; i++)
        if (stock[i] < start_stock[i]) stock[i] = start_stock[i];
    if (powder < START_POWDER) powder = START_POWDER;
    if (shells < START_SHELLS) shells = START_SHELLS;
}

// --- customers -------------------------------------------------------------------

typedef struct {
    u8 name, style, skin, hair, shirt, acc;
    u16 want_pats;            // acceptable patterns (0 = any)
    s8 want_hue, want_size;   // -1 = any
    u8 like_type, like_val;   // hidden like: 0 none, 1 color, 2 pattern, 3 size
    char body[96];            // the request without the greeting
    char line[120];
} Cust;

enum { LIKE_NONE, LIKE_COLOR, LIKE_PAT, LIKE_SIZE };

static Cust cu;
static u32 names_used;          // customers already seen tonight

static const char* const greets[] = { "HI!", "GOOD EVENING!", "HELLO!", "HEY THERE!", "EVENING!" };
static const char* const occasions[] = {
    "MY BIRTHDAY", "OUR ANNIVERSARY", "MY GRANDMA", "THE FESTIVAL", "MY BEST FRIEND", "MY DOG'S BIRTHDAY",
};
static const char* const hints[P_COUNT] = {
    "SOMETHING THAT BLOOMS LIKE A BIG FLOWER.", "SOMETHING THAT BLOOMS LIKE A BIG FLOWER.",
    "ONE THAT DRIPS DOWN LIKE A WILLOW TREE.", "ONE THAT LOOKS LIKE A PALM TREE.",
    "A PERFECT CIRCLE IN THE SKY.", "ONE THAT SPLITS INTO LITTLE CROSSES.",
    "A HEART, FOR SOMEONE SPECIAL.", "SOMETHING THAT CRACKLES AND POPS!",
    "SPARKLES THAT TWINKLE AND SHIMMER.",
};
static const char* const want_size_words[3] = { "LITTLE", "MEDIUM-SIZED", "BIG" };

static const u16 skins[4][2] = {
    { RGB(31, 25, 20), RGB(27, 19, 14) }, { RGB(28, 20, 14), RGB(23, 15, 10) },
    { RGB(22, 14, 9), RGB(17, 10, 6) }, { RGB(14, 9, 6), RGB(10, 6, 4) },
};
static const u16 hairs[7] = {
    RGB(6, 4, 4), RGB(14, 8, 4), RGB(28, 22, 10), RGB(24, 10, 4), RGB(22, 22, 24), RGB(10, 14, 28), RGB(28, 12, 20),
};
static const u16 shirts[8] = {
    RGB(8, 16, 28), RGB(28, 10, 10), RGB(10, 22, 12), RGB(28, 22, 8), RGB(20, 10, 26), RGB(6, 20, 22),
    RGB(26, 26, 26), RGB(28, 14, 20),
};

static u16 shade(u16 c) {
    int r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
    return RGB(r * 3 / 4, g * 3 / 4, b * 3 / 4);
}

static void cust_palette(void) {
    OBJ_PAL[1] = RGB(5, 3, 5);
    OBJ_PAL[2] = skins[cu.skin][0];
    OBJ_PAL[3] = skins[cu.skin][1];
    OBJ_PAL[4] = hairs[cu.hair];
    OBJ_PAL[5] = shade(hairs[cu.hair]);
    OBJ_PAL[6] = shirts[cu.shirt];
    OBJ_PAL[7] = shade(shirts[cu.shirt]);
    OBJ_PAL[8] = RGB(4, 3, 6);
    OBJ_PAL[9] = RGB(31, 17, 17);
    OBJ_PAL[10] = shirts[cu.acc];
    OBJ_PAL[11] = shade(shirts[cu.acc]);
    OBJ_PAL[12] = RGB(31, 31, 31);
}

static const char* article(const char* word) {
    char c = word[0];
    return (c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U') ? "AN " : "A ";
}

static int random_known_pattern(void) {
    for (;;) {
        int p = rnd() % P_COUNT;
        if (known & (1 << p)) return p;
    }
}

static void make_customer(void) {
    do cu.name = rnd() % NNAMES; while (names_used & (1u << cu.name));   // new faces each night
    names_used |= 1u << cu.name;
    cu.style = rnd() % 4;
    cu.skin = rnd() % 4;
    cu.hair = rnd() % 7;
    cu.shirt = rnd() % 8;
    do cu.acc = rnd() % 8; while (cu.acc == cu.shirt);
    cu.want_pats = 0;
    cu.want_hue = -1;
    cu.want_size = -1;

    int pat = random_known_pattern();
    int hue = rnd() % 11;                  // no one asks for silver by name
    int kind = rnd() % 100;
    char* p = cu.body;
    if (kind < 30) {                       // color + pattern
        p = cat(p, "COULD I GET ");
        p = cat(p, article(hue_names[hue]));
        p = cat(p, hue_names[hue]);
        p = cat(p, " ");
        p = cat(p, pat_names[pat]);
        cat(p, "?");
        cu.want_hue = hue;
        cu.want_pats = 1 << pat;
    } else if (kind < 45) {                // size + color + pattern, for an occasion
        int sz = rnd() % 3;
        p = cat(p, "I NEED ");
        p = cat(p, article(want_size_words[sz]));
        p = cat(p, want_size_words[sz]);
        p = cat(p, " ");
        p = cat(p, hue_names[hue]);
        p = cat(p, " ");
        p = cat(p, pat_names[pat]);
        p = cat(p, " FOR ");
        p = cat(p, occasions[rnd() % 6]);
        cat(p, ".");
        cu.want_hue = hue;
        cu.want_pats = 1 << pat;
        cu.want_size = sz;
    } else if (kind < 65) {                // color only
        if (rnd() & 1) { p = cat(p, "SOMETHING "); p = cat(p, hue_names[hue]); cat(p, ", PLEASE!"); }
        else { p = cat(p, "MY FAVORITE COLOR IS "); p = cat(p, hue_names[hue]); cat(p, "!"); }
        cu.want_hue = hue;
    } else if (kind < 85) {                // a pattern, described, maybe with a color
        p = cat(p, hints[pat]);
        cu.want_pats = 1 << pat;
        if (pat == P_PEONY || pat == P_CHRYS) cu.want_pats = (1 << P_PEONY) | (1 << P_CHRYS);
        if (rnd() & 1) {
            p = cat(p, " ");
            p = cat(p, hue_names[hue]);
            cat(p, ", IF YOU CAN!");
            cu.want_hue = hue;
        }
    } else {                               // just a size
        if (rnd() & 1) { cat(p, "SOMETHING BIG! SURPRISE ME!"); cu.want_size = S_LARGE; }
        else { cat(p, "JUST A LITTLE ONE. SURPRISE ME!"); cu.want_size = S_SMALL; }
    }
    p = cat(cu.line, greets[rnd() % 5]);
    p = cat(p, " ");
    cat(p, cu.body);

    cu.like_type = LIKE_NONE;
    if (rnd() % 100 < 60) {
        cu.like_type = 1 + rnd() % 3;
        cu.like_val = cu.like_type == LIKE_COLOR ? rnd() % NCOLORS
                    : cu.like_type == LIKE_PAT ? rnd() % P_COUNT : rnd() % 3;
    }
}

// --- judging a firework ----------------------------------------------------------

enum { R_MATCH, R_CLOSE, R_MISS, R_OPP };

// Colors 0..9 sit on a wheel (red .. pink, then back to red). White and silver are neutral.
static int hue_rel(int want, int got) {
    if (want == got) return R_MATCH;
    int wn = want >= 10, gn = got >= 10;
    if (wn && gn) return R_CLOSE;
    if (wn || gn) return R_MISS;
    int d = want > got ? want - got : got - want;
    if (d > 5) d = 10 - d;
    return d == 1 ? R_CLOSE : d <= 3 ? R_MISS : R_OPP;
}

static int sel_size, sel_pat, sel_hue;

// rel gets how close each part came: [0] color, [1] size, [2] pattern.
static int judge(const Shell* s, int* liked, int* rel) {
    int closes = 0, misses = 0, opps = 0;
    rel[0] = rel[1] = rel[2] = R_MATCH;
    if (s->want_hue >= 0) rel[0] = hue_rel(s->want_hue, s->hue);
    if (s->want_size >= 0) {
        int d = s->want_size > s->size ? s->want_size - s->size : s->size - s->want_size;
        rel[1] = d == 0 ? R_MATCH : d == 1 ? R_CLOSE : R_OPP;
    }
    if (s->want_pats) {
        const u16 flowers = (1 << P_PEONY) | (1 << P_CHRYS);
        if (s->want_pats & (1 << s->pat)) rel[2] = R_MATCH;
        else if ((s->want_pats & flowers) && ((1 << s->pat) & flowers)) rel[2] = R_CLOSE;
        else rel[2] = R_MISS;
    }
    for (int i = 0; i < 3; i++) {
        if (rel[i] == R_CLOSE) closes++;
        if (rel[i] == R_MISS) misses++;
        if (rel[i] == R_OPP) opps++;
    }
    *liked = (s->like_type == LIKE_COLOR && s->hue == s->like_val) ||
             (s->like_type == LIKE_PAT && s->pat == s->like_val) ||
             (s->like_type == LIKE_SIZE && s->size == s->like_val);
    if (*liked) return closes + misses + opps == 0 ? T_LOVE : T_SURPRISE;
    if (opps) return T_REFUND;
    if (misses) return T_NOPE;
    if (closes) return T_CLOSE;
    return T_HAPPY;
}

// Short enough for a review bubble (12 characters a line).
static char* like_words(char* p, const Shell* s) {
    static const char* const sizes[3] = { "LITTLE ONES", "MEDIUM ONES", "BIG ONES" };
    static const char* const pats[P_COUNT] = {
        "PEONIES", "MUMS", "WILLOWS", "PALMS", "RINGS", "CROSSETTES", "HEARTS", "CRACKLES", "GLITTER",
    };
    if (s->like_type == LIKE_COLOR) return cat(p, hue_names[s->like_val]);
    if (s->like_type == LIKE_PAT) return cat(p, pats[s->like_val]);
    return cat(p, sizes[s->like_val]);
}

static char* give_gift(char* p) {
    u32 r = rnd() % 100;
    u16 locked = ((1 << P_COUNT) - 1) & ~known;
    if (r < 30 && locked) {
        int pat;
        do pat = rnd() % P_COUNT; while (!(locked & (1 << pat)));
        known |= 1 << pat;
        p = cat(p, "NEW SCROLL: ");
        return cat(p, pat_plural[pat]);
    }
    if (r < 80) {
        int best = rnd() % NCOLORS;        // lean toward colors you are short on
        for (int i = 0; i < 2; i++) {
            int h = rnd() % NCOLORS;
            if (stock[h] < stock[best]) best = h;
        }
        stock[best] += 2;
        p = cat(p, "2 ");
        p = cat(p, hue_names[best]);
        return cat(p, " STARS");
    }
    coins += 15;
    night_coins += 15;
    return cat(p, "A 15 COIN TIP");
}

// --- review comments ---------------------------------------------------------------
// Fit a review bubble: 3 lines of 12 characters. Most name what the customer saw.

static const char* const pat_short[P_COUNT] = {
    "PEONY", "MUM", "WILLOW", "PALM", "RING", "CROSSETTE", "HEART", "CRACKLE", "GLITTER",
};

static const char* pick(const char* const* list, int n) { return list[rnd() % n]; }

// Fills in {G} got color, {W} wanted color, {P} got pattern, {Q} wanted pattern, {L} the like.
static char* fill(char* p, const char* t, const Shell* s) {
    const u16 flowers = (1 << P_PEONY) | (1 << P_CHRYS);
    for (; *t; t++) {
        if (*t != '{') { *p++ = *t; continue; }
        char k = t[1];
        t += 2;                                   // skip "X}"
        if (k == 'G') p = cat(p, hue_names[s->hue]);
        else if (k == 'W') p = cat(p, s->want_hue >= 0 ? hue_names[s->want_hue] : "IT");
        else if (k == 'P') p = cat(p, pat_short[s->pat]);
        else if (k == 'Q') {
            int q = 0;
            while (q < P_COUNT && !(s->want_pats & (1 << q))) q++;
            p = cat(p, (s->want_pats & flowers) == flowers ? "FLOWER" : q < P_COUNT ? pat_short[q] : "ONE");
        } else if (k == 'L') p = like_words(p, s);
    }
    *p = 0;
    return p;
}

static void comment_for(char* out, const Shell* s, int tier, const int* rel) {
    static const char* const love[] = {
        "I LOVE {L}! BEST NIGHT!", "{L}! MY FAVORITE! THANK YOU!", "HOW DID YOU KNOW I LOVE {L}?",
        "PERFECT! AND I LOVE {L}!", "WOW. JUST WOW. I LOVE {L}!", "MY HEART! {L}! THANK YOU!",
        "BEST {P} EVER! WOW!", "I'M CRYING! {L}!",
    };
    static const char* const surprise[] = {
        "NOT IT, BUT I LOVE {L}!", "WHO KNEW I LOVED {L}?", "A SURPRISE? I LOVE {L}!",
    };
    static const char* const happy[] = {
        "JUST WHAT I WANTED!", "PERFECT! THANK YOU!", "A LOVELY {G} {P}!", "IT WAS BEAUTIFUL!",
        "THE {P} WAS PERFECT!", "SO MUCH {G}! I LOVED IT.",
    };
    static const char* const happy_any[] = {      // asked for nothing in particular
        "WHAT A SHOW! THANK YOU!", "IT WAS BEAUTIFUL!", "A {G} {P}! LOVELY.",
    };
    static const char* const close_hue[] = { "{G}, NOT {W}... BUT OKAY.", "ALMOST {W}. GOOD ENOUGH." };
    static const char* const close_pat[] = { "NOT QUITE MY {Q}, BUT NICE.", "ALMOST A {Q}. NOT BAD!" };
    static const char* const meh[] = {             // neither for nor against
        "IT WAS FINE.", "NOT BAD, I GUESS.", "IT WAS OKAY!", "HMM. IT WAS ALRIGHT.", "PRETTY NICE.",
        "A DECENT {P}.", "IT WAS... A FIREWORK.", "SURE, IT WAS NICE.",
    };
    static const char* const nope_hue[] = { "I WANTED {W}, NOT {G}...", "WHERE WAS THE {W}?", "{G}? I ASKED FOR {W}..." };
    static const char* const nope_pat[] = { "WHERE WAS MY {Q}?", "THAT WAS NO {Q}...", "I WANTED A {Q}..." };
    static const char* const refund_hue[] = { "{G}?! I SAID {W}! REFUND!", "{W}! NOT {G}! REFUND!" };
    char* p = out;
    const char* t = 0;
    int big = s->want_size == S_LARGE;
    switch (tier) {
    case T_LOVE: t = pick(love, 8); break;
    case T_SURPRISE: t = pick(surprise, 3); break;
    case T_HAPPY:
        t = (s->want_hue < 0 && !s->want_pats) ? pick(happy_any, 3) : pick(happy, 6);
        if (s->want_size == S_LARGE && (rnd() & 1)) t = "SO BIG! JUST WHAT I WANTED!";
        break;
    case T_CLOSE:
        if (rnd() & 1) t = pick(meh, 8);
        else if (rel[0] == R_CLOSE) t = pick(close_hue, 2);
        else if (rel[2] == R_CLOSE) t = pick(close_pat, 2);
        else t = big ? "A BIT SMALL, BUT STILL NICE." : "A BIT BIG, BUT STILL NICE.";
        break;
    case T_NOPE:
        if (rel[0] == R_MISS) t = pick(nope_hue, 3);
        else if (rel[2] == R_MISS) t = pick(nope_pat, 3);
        else t = "HMM... THAT WASN'T IT.";
        break;
    case T_REFUND:
        if (rel[0] == R_OPP) t = pick(refund_hue, 2);
        else t = big ? "SO TINY! I WANT A REFUND." : "WAY TOO BIG! REFUND, PLEASE.";
        break;
    }
    fill(p, t, s);
}

// Review time: judges one firework from tonight's crate, pays out, and writes the
// customer's short comment and any gift (empty if none). Returns the tier.
int judge_shell(const Shell* s, char* comment, char* gift) {
    int liked, rel[3];
    int tier = judge(s, &liked, rel);
    int pay = size_pay[s->size] + pat_time[s->pat] / 2;
    int drep = 0;
    comment_for(comment, s, tier, rel);
    switch (tier) {
    case T_LOVE: pay = pay * 3 / 2; drep = 2; break;
    case T_SURPRISE: drep = 1; break;   // loved it without asking for it
    case T_HAPPY: drep = 1; break;
    case T_CLOSE: pay = pay * 4 / 5; break;
    case T_NOPE: pay = pay / 2; drep = -1; break;
    case T_REFUND: pay = 0; drep = -2; break;
    }
    coins += pay;
    night_coins += pay;
    if (rep + drep < 0) drep = -rep;
    rep += drep;
    night_rep += drep;
    if (tier <= T_CLOSE) night_happy++;
    gift[0] = 0;
    if (tier == T_LOVE || tier == T_SURPRISE) give_gift(cat(gift, "GIFT  "));
    return tier;
}

// --- dialog and menus --------------------------------------------------------------

enum { ST_OPEN, ST_ARRIVE, ST_ASK, ST_CHOICE, ST_SIZE, ST_PAT, ST_COLOR, ST_CONFIRM, ST_CRAFT,
       ST_THANKS, ST_DECLINE, ST_LEAVE, ST_CLOSED };

int shop_st;                   // state (non-static so test scripts can read it)
int shop_min;                  // minutes since 8:00 PM
static int st_t;
static int cx = 240;           // customer sprite x
static int show_cust, bubble;
static int cursor;
static int craft_from, craft_cost;
static u32 page_sig[2];        // what each page last showed (skip unchanged redraws)

static char dlg[200];
static int dlg_len, dlg_shown, dlg_who;   // dlg_who: 1 = the customer speaks

static void say(int who, const char* s) {
    char* p = dlg;
    while (*s && p < dlg + sizeof dlg - 1) *p++ = *s++;
    *p = 0;
    dlg_len = p - dlg;
    dlg_shown = 0;
    dlg_who = who;
}

static void go(int s) {
    shop_st = s;
    st_t = 0;
    cursor = 0;
}

#define MAXMENU 12
#define MENU_ROWS 9
static int menu_n;
static char menu_txt[MAXMENU][20];
static u8 menu_ok[MAXMENU], menu_val[MAXMENU];
static char info[48];

static void menu_add(const char* s, int ok, int val) {
    cat(menu_txt[menu_n], s);
    menu_ok[menu_n] = ok;
    menu_val[menu_n] = val;
    menu_n++;
}

static int time_left(void) { return CLOSE_MIN - shop_min; }

static char* time_str(char* p, int m) {
    int h = 8 + m / 60, mm = m % 60;
    int am = h >= 12;
    int hh = h % 12;
    if (hh == 0) hh = 12;
    p = cat_num(p, hh);
    *p++ = ':';
    *p++ = '0' + mm / 10;
    *p++ = '0' + mm % 10;
    return cat(p, am ? " AM" : " PM");
}

static int craft_minutes(void) { return size_time[sel_size] + pat_time[sel_pat]; }

// Fills the menu for the current state and the info line for the highlighted item.
static void build_menu(void) {
    menu_n = 0;
    for (int i = 0; i < MAXMENU; i++) menu_txt[i][0] = 0;
    info[0] = 0;
    char* p;
    switch (shop_st) {
    case ST_CHOICE:
        menu_add("MAKE IT", 1, 0);
        menu_add("SORRY, I CAN'T", 1, 1);
        p = cat(info, "TIME LEFT: ");
        p = cat_num(p, time_left());
        cat(p, " MIN");
        break;
    case ST_SIZE:
        for (int s = 0; s < 3; s++)
            menu_add(size_names[s], powder >= size_powder[s] && shells > 0 && size_time[s] <= time_left(), s);
        {
            int s = menu_val[cursor];
            if (shells == 0) cat(info, "NO SHELLS LEFT!");
            else if (powder < size_powder[s]) cat(info, "NOT ENOUGH POWDER");
            else if (size_time[s] > time_left()) cat(info, "NOT ENOUGH TIME");
            else {
                p = cat_num(info, size_time[s]);
                p = cat(p, " MIN, ");
                p = cat_num(p, size_powder[s]);
                p = cat(p, " POWDER (HAVE ");
                p = cat_num(p, powder);
                cat(p, ")");
            }
        }
        break;
    case ST_PAT:
        for (int k = 0; k < P_COUNT; k++)
            if (known & (1 << k))
                menu_add(pat_names[k], size_time[sel_size] + pat_time[k] <= time_left(), k);
        {
            int k = menu_val[cursor];
            int total = size_time[sel_size] + pat_time[k];
            if (total > time_left()) cat(info, "NOT ENOUGH TIME");
            else {
                p = cat(info, "+");
                p = cat_num(p, pat_time[k]);
                p = cat(p, " MIN (TOTAL ");
                p = cat_num(p, total);
                cat(p, " MIN)");
            }
        }
        break;
    case ST_COLOR:
        for (int h = 0; h < NCOLORS; h++) {
            char buf[20];
            p = cat(buf, hue_names[h]);
            while (p < buf + 8) *p++ = ' ';
            cat_num(p, stock[h]);
            menu_add(buf, stock[h] > 0, h);
        }
        {
            int h = menu_val[cursor];
            p = cat_num(info, stock[h]);
            p = cat(p, " ");
            p = cat(p, hue_names[h]);
            cat(p, stock[h] == 1 ? " STAR LEFT" : " STARS LEFT");
        }
        break;
    case ST_CONFIRM:
        menu_add("YES", 1, 0);
        menu_add("NO", 1, 1);
        p = cat_num(info, craft_minutes());
        p = cat(p, " MIN. DONE AT ");
        time_str(p, shop_min + craft_minutes());
        break;
    }
}

static int menu_update(void) {
    // Returns the chosen value, -1 for nothing yet, -2 for back.
    if (st_t == 1 && !menu_ok[cursor])            // start on something you can pick
        for (int i = 0; i < menu_n; i++)
            if (menu_ok[i]) { cursor = i; build_menu(); break; }
    if (hit & KEY_UP) cursor = (cursor + menu_n - 1) % menu_n;
    if (hit & KEY_DOWN) cursor = (cursor + 1) % menu_n;
    if (hit & KEY_B) return -2;
    if ((hit & KEY_A) && menu_ok[cursor]) return menu_val[cursor];
    return -1;
}

static char* craft_name(char* p) {
    p = cat(p, size_names[sel_size]);
    p = cat(p, " ");
    p = cat(p, hue_names[sel_hue]);
    p = cat(p, " ");
    return cat(p, pat_names[sel_pat]);
}

// --- the state machine ---------------------------------------------------------

static void next_customer(void) {
    if (time_left() < size_time[S_SMALL] || shells == 0 || powder == 0) {
        char buf[160], *p = buf;
        if (shells == 0) p = cat(p, "YOU'RE OUT OF SHELLS, SO YOU CLOSE UP EARLY.");
        else if (powder == 0) p = cat(p, "YOU'RE OUT OF POWDER, SO YOU CLOSE UP EARLY.");
        else { p = cat(p, "IT'S "); p = time_str(p, shop_min); p = cat(p, ". TIME TO CLOSE UP!"); }
        cat(p, ncrate ? "\nYOUR FIREWORKS ARE READY FOR THE SHOW." : "\nNO FIREWORKS TONIGHT...");
        say(0, buf);
        go(ST_CLOSED);
        return;
    }
    make_customer();
    cust_palette();
    cx = 244;
    show_cust = 1;
    bubble = -1;
    go(ST_ARRIVE);
}

void shop_enter(void) {
    blank_once();
    for (int i = 0; i < SHOP_PAL_N; i++) BG_PAL[16 + i] = shop_pal[i];
    ui_palette();
    REG_BLDCNT = 0;
    REG_DISPCNT &= ~(1 << 13);         // no window from the show's bubbles
    sky_reset();                       // the window shows the plain night sky
    vu32* t = (vu32*)OBJ_TILES;
    for (int i = 0; i < 32 * 4 * 8; i++) t[T_CUST * 8 + i] = cust_tiles[i];
    for (int i = 0; i < BUB_COUNT * 4 * 8; i++) t[T_BUBBLE * 8 + i] = bubble_tiles[i];
    OBJ_PAL[16 + 1] = RGB(4, 3, 6);    // bubbles
    OBJ_PAL[16 + 2] = RGB(31, 31, 30);
    OBJ_PAL[16 + 3] = RGB(30, 8, 10);
    OBJ_PAL[16 + 4] = RGB(30, 24, 6);
    OBJ_PAL[16 + 5] = RGB(12, 12, 14);

    shop_min = 0;
    ncrate = 0;
    page_sig[0] = page_sig[1] = 0;
    names_used = 0;
    night_made = night_happy = night_coins = night_rep = 0;
    show_cust = 0;
    bubble = -1;
    char buf[160], *p = cat(buf, "NIGHT ");
    p = cat_num(p, night);
    p = cat(p, ". THE SHOP IS OPEN!");
    if (night == 1) cat(p, "\nMAKE WHAT CUSTOMERS ASK FOR. BIGGER AND FANCIER FIREWORKS TAKE MORE TIME.");
    say(0, buf);
    go(ST_OPEN);
}

static void step(void) {
    int a = hit & KEY_A;
    // A first finishes the text that is still typing out.
    if (dlg_shown < dlg_len) {
        dlg_shown += 2;
        if (dlg_shown > dlg_len) dlg_shown = dlg_len;
        if (a) { dlg_shown = dlg_len; a = 0; hit &= ~KEY_A; }
    }
    int done = dlg_shown >= dlg_len;
    st_t++;
    int v;
    switch (shop_st) {
    case ST_OPEN:
        if (a && done) next_customer();
        break;
    case ST_ARRIVE:
        cx -= 3;
        if (cx <= 104) { cx = 104; say(1, cu.line); go(ST_ASK); }
        break;
    case ST_ASK:
        if (a && done) go(ST_CHOICE);
        break;
    case ST_CHOICE:
        build_menu();
        v = menu_update();
        if (v == 0) go(ST_SIZE);
        else if (v == 1) {
            say(1, "OH, THAT'S OKAY. MAYBE NEXT TIME!");
            shop_min += 5;
            go(ST_DECLINE);
        }
        break;
    case ST_SIZE:
        build_menu();
        v = menu_update();
        if (v == -2) go(ST_CHOICE);
        else if (v >= 0) { sel_size = v; go(ST_PAT); }
        break;
    case ST_PAT:
        build_menu();
        v = menu_update();
        if (v == -2) { go(ST_SIZE); cursor = sel_size; }
        else if (v >= 0) { sel_pat = v; go(ST_COLOR); }
        break;
    case ST_COLOR:
        build_menu();
        v = menu_update();
        if (v == -2) go(ST_PAT);
        else if (v >= 0) { sel_hue = v; go(ST_CONFIRM); }
        break;
    case ST_CONFIRM:
        build_menu();
        v = menu_update();
        if (v == -2 || v == 1) go(ST_COLOR);
        else if (v == 0) {
            powder -= size_powder[sel_size];
            shells--;
            stock[sel_hue]--;
            craft_from = shop_min;
            craft_cost = craft_minutes();
            char buf[64], *p = cat(buf, "MAKING ");
            p = cat(p, article(size_names[sel_size]));
            p = craft_name(p);
            cat(p, "...");
            say(0, buf);
            dlg_shown = dlg_len;
            bubble = BUB_SPARK;
            go(ST_CRAFT);
        }
        break;
    case ST_CRAFT:
        shop_min = craft_from + craft_cost * st_t / 72;
        if (st_t >= 72) {
            shop_min = craft_from + craft_cost;
            if (ncrate < MAXCRATE) {
                Shell* s = &crate[ncrate++];
                s->pat = sel_pat; s->hue = sel_hue; s->size = sel_size; s->cust = cu.name;
                s->want_pats = cu.want_pats; s->want_hue = cu.want_hue; s->want_size = cu.want_size;
                s->like_type = cu.like_type; s->like_val = cu.like_val;
            }
            // They take it with them; what they think of it comes after the show.
            static const char* const thanks[] = {
                "THANK YOU! SEE YOU AT THE SHOW!", "OOH, I CAN'T WAIT TO SEE IT GO UP!",
                "THANKS! I'LL BE WATCHING TONIGHT!", "WONDERFUL. SEE YOU AT THE SHOW!",
            };
            say(1, thanks[rnd() % 4]);
            bubble = -1;
            night_made++;
            go(ST_THANKS);
        }
        break;
    case ST_THANKS:
    case ST_DECLINE:
        if (a && done) { bubble = -1; go(ST_LEAVE); }
        break;
    case ST_LEAVE:
        cx -= 3;
        if (cx < -40) {
            show_cust = 0;
            shop_min += ARRIVE_MIN;
            if (shop_min > CLOSE_MIN) shop_min = CLOSE_MIN;
            next_customer();
        }
        break;
    case ST_CLOSED:
        if (a && done) set_scene(ncrate ? SC_SHOW : SC_REVIEW);
        break;
    }
}

// --- drawing -------------------------------------------------------------------

static void draw_page(u8* back);
static void draw_sprites(void);

static int is_menu(void) { return shop_st >= ST_CHOICE && shop_st <= ST_CONFIRM; }

static u32 mix(u32 h, int v) { return (h ^ (u32)v) * 16777619u; }

static void draw(u8* back) {
    // Most frames nothing changes; each page is redrawn only when what it shows changed.
    u32 sig = 2166136261u;
    int vals[] = { shop_st, dlg_shown, dlg_len, cursor, shop_min, ncrate, coins, rep, (int)(frame & 16), shop_st == ST_CRAFT ? st_t : 0, dlg_who, night };
    for (unsigned i = 0; i < sizeof vals / sizeof vals[0]; i++) sig = mix(sig, vals[i]);
    int pi = back == PAGE1;
    int same = page_sig[pi] == sig;
    page_sig[pi] = sig;
    if (!same) draw_page(back);
    draw_sprites();
}

static void draw_page(u8* back) {
    copy_page(back, shop_bg);
    char buf[48], *p;

    // Status bar.
    time_str(buf, shop_min);
    text(back, 3, 2, buf, UI_YELLOW);
    p = cat(buf, "NIGHT ");
    cat_num(p, night);
    text(back, 84, 2, buf, UI_WHITE);
    p = cat(buf, "COINS ");
    p = cat_num(p, coins);
    p = cat(p, " REP ");
    cat_num(p, rep);
    text(back, 237 - text_w(buf), 2, buf, UI_WHITE);

    // Tonight's crate: a tube on the counter for each firework made.
    for (int i = 0; i < ncrate; i++) {
        int x = 150 + i * 6;
        fill_rect(back, x, 89, 4, 9, 16 + crate[i].hue);
        fill_rect(back, x, 89, 4, 1, UI_WHITE);
        fill_rect(back, x + 1, 87, 1, 2, UI_SHADOW);
    }

    // Dialog box.
    box(back, 0, 116, 240, 44);
    char lines[4][41];
    int y = 121;
    if (is_menu()) {
        // While choosing, keep the request on screen and explain the highlighted item.
        text(back, 8, y, cust_names[cu.name], UI_YELLOW);
        int n = wrap(cu.body, 37, lines, 2);
        for (int i = 0; i < n; i++) text(back, 8, y + 9 * (i + 1), lines[i], UI_WHITE);
        text(back, 8, y + 27, info, menu_ok[cursor] ? UI_GREEN : UI_RED);
    } else {
        char shown[200];
        int k;
        for (k = 0; k < dlg_shown && dlg[k]; k++) shown[k] = dlg[k];
        shown[k] = 0;
        int row = 0;
        if (dlg_who) { text(back, 8, y, cust_names[cu.name], UI_YELLOW); row = 1; }
        int n = wrap(shown, 37, lines, 4 - row);
        for (int i = 0; i < n; i++) text(back, 8, y + 9 * (row + i), lines[i], UI_WHITE);
        if (shop_st == ST_CRAFT) {
            fill_rect(back, 8, 146, 224, 6, UI_SHADOW);
            fill_rect(back, 8, 146, 224 * st_t / 72, 6, UI_YELLOW);
        }
        // A blinking arrow when the text waits for A.
        if (dlg_shown >= dlg_len && shop_st != ST_ARRIVE && shop_st != ST_CRAFT && shop_st != ST_LEAVE &&
            (frame & 16))
            fill_rect(back, 226, 150, 5, 3, UI_YELLOW);
    }

    // Menu box at the right.
    if (is_menu()) {
        int w = 0;
        for (int i = 0; i < menu_n; i++) {
            int tw = text_w(menu_txt[i]);
            if (tw > w) w = tw;
        }
        w += 22;
        // At most MENU_ROWS rows show; longer lists scroll with the cursor.
        int rows = menu_n < MENU_ROWS ? menu_n : MENU_ROWS;
        int top = cursor >= rows ? cursor - rows + 1 : 0;
        int h = rows * 9 + 10;
        int x = 236 - w, y0 = 14;
        box(back, x, y0, w, h);
        for (int r = 0; r < rows; r++) {
            int i = top + r, yy = y0 + 6 + r * 9;
            int c = !menu_ok[i] ? UI_GRAY : i == cursor ? UI_YELLOW : UI_WHITE;
            if (shop_st == ST_COLOR && menu_ok[i])
                fill_rect(back, x + w - 12, yy, 4, 5, 16 + menu_val[i]);   // a swatch of the color
            text(back, x + 13, yy, menu_txt[i], c);
            if (i == cursor) text(back, x + 5, yy, ">", UI_YELLOW);
        }
        if (top > 0) fill_rect(back, x + w / 2 - 2, y0 + 2, 5, 2, UI_YELLOW);
        if (top + rows < menu_n) fill_rect(back, x + w / 2 - 2, y0 + h - 4, 5, 2, UI_YELLOW);
    }

}

static void draw_sprites(void) {
    // The reaction bubble sits on top of the customer.
    if (show_cust) {
        int bob = (shop_st == ST_ARRIVE || shop_st == ST_LEAVE) ? ((st_t >> 2) & 1) : 0;
        if (bubble >= 0) sprite(cx + 22, CUST_Y - 12, T_BUBBLE + bubble * 4, 0, 1, 1);
        sprite(cx, CUST_Y - bob, T_CUST + cu.style * 32, 2, 3, 0);
    }
}

void shop_update(u8* back) {
    step();
    draw(back);
}
