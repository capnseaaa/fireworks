// Grandma's visits and letters, and Port Willow's festival nights. Which days get one is
// worked out from the day number (a hash), so nothing new is saved, reloading a day gives
// the same morning, and the gaps between events are irregular.
#include "game.h"

static u32 mix32(u32 a) { a ^= a >> 16; a *= 0x7feb352d; a ^= a >> 15; a *= 0x846ca68b; a ^= a >> 16; return a; }
static u32 roll(int n, u32 salt) { return mix32((u32)n * 2654435761u ^ salt); }

// The first festival is night 6; after that, about one night in five, never within two
// nights of another.
static int festival_raw(int n) { return n == 6 || (n >= 9 && roll(n, 0xF357) % 100 < 20); }

int festival_night(int n) {
    if (!festival_raw(n) || festival_raw(n - 1) || festival_raw(n - 2)) return 0;
    return n == 6 ? 1 : 1 + roll(n, 0xF00D) % 4;
}

const char* const festival_names[5] = { "", "LANTERN FESTIVAL", "HARBOR DAYS", "STARFALL FAIR", "WILLOW NIGHT" };

// Grandma checks in on day 3. After that: a visit about one morning in five (never two
// days running), a letter about one in six.
static int visit_raw(int n) { return n == 3 || (n >= 4 && roll(n, 0x6A11) % 100 < 20); }

int day_event(int n) {
    if (n < 3) return EV_NONE;
    if (visit_raw(n) && !visit_raw(n - 1)) return EV_VISIT;
    u32 r = roll(n, 0x6A11) % 100;
    if (r >= 20 && r < 37) return EV_LETTER;
    return EV_NONE;
}

static const char* const visits[] = {
    "MORNING, LUKE! I BAKED TOO MANY MUFFINS AGAIN. AND I FOUND SOMETHING IN THE ATTIC FOR YOU.",
    "I WAS WALKING BY AND SAW THE SIGN. YOU KEEP IT SO NICE. HERE, I BROUGHT YOU A LITTLE SOMETHING.",
    "YOUR GRANDPA AND I WATCHED THE SHOWS FROM THE PIER. HE'D HAVE LOVED YOURS. TAKE THIS, DEAR.",
    "DON'T WORK TOO HARD, SWEETHEART. EAT SOMETHING! AND HERE, THIS IS FOR THE SHOP.",
    "THE NEIGHBORS WON'T STOP TALKING ABOUT YOUR FIREWORKS. I TOLD THEM IT RUNS IN THE FAMILY. HERE.",
};

static const char* const letters[] = {
    "DEAR LUKE, A WILLOW LOOKS BEST IN GOLD. YOUR GRANDPA SWORE BY IT. LOVE, GRANDMA",
    "DEAR LUKE, WHEN SOMEONE IS VAGUE, ANY PATTERN THAT FITS WILL DO. TRUST YOURSELF. LOVE, GRANDMA",
    "DEAR LUKE, AT MY FIRST SHOW I SET THE AWNING ON FIRE. YOU'RE DOING BETTER ALREADY! LOVE, GRANDMA",
    "DEAR LUKE, END THE NIGHT ON A BIG ONE. A GRAND FINALE SENDS THEM HOME SMILING. LOVE, GRANDMA",
    "DEAR LUKE, THE TIDE POOLS FILL UP AGAIN EVERY DAY. DON'T FORGET YOUR NET. LOVE, GRANDMA",
    "DEAR LUKE, SOME FOLKS NEVER SAY WHAT THEY LOVE. WATCH WHAT THEY REACH FOR. LOVE, GRANDMA",
    "DEAR LUKE, THE OLD TREE IN THE FOREST HIDES MORE THAN SHELLS. A SHARP AXE HELPS. LOVE, GRANDMA",
    "DEAR LUKE, PORT WILLOW IS NAMED FOR THE WILLOWS BY THE HARBOR, NOT THE FIREWORK. MOSTLY. LOVE, GRANDMA",
};

const char* visit_text(int n) { return visits[roll(n, 0x7171) % (sizeof visits / sizeof visits[0])]; }
const char* letter_text(int n) { return letters[roll(n, 0x1E77) % (sizeof letters / sizeof letters[0])]; }

// Grandma's gift: a scroll if there's one left to learn (half the time), rare stars, or
// coins. Writes what Luke got into out, in his words.
void visit_gift(int n, char* out) {
    u32 r = roll(n, 0x9177);
    char* p = out;
    if (r % 2 == 0) {
        int pat = unlock_pattern();
        if (pat >= 0) {
            p = cat(p, "SHE GAVE ME ONE OF HER OLD SCROLLS. NOW I CAN MAKE ");
            p = cat(p, pat_plural[pat]);
            cat(p, "!");
            return;
        }
    }
    if ((r >> 4) % 2 == 0) {
        static const u8 rare[4] = { H_SILVER, H_GOLD, H_VIOLET, H_CYAN };
        int h = rare[(r >> 8) % 4];
        stock[h] += 3;
        p = cat(p, "SHE GAVE ME 3 ");
        p = cat(p, hue_names[h]);
        cat(p, " STARS FROM HER OLD STASH.");
    } else {
        coins += 50;
        cat(p, "SHE SLIPPED ME 50 COINS. \"FOR THE SHOP,\" SHE SAID.");
    }
}
