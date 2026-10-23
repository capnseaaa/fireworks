// Shared types for the fireworks engine. render.c runs from IWRAM in ARM mode.
#pragma once

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned int u32;
typedef signed int s32;
typedef volatile u16 vu16;
typedef volatile u32 vu32;

// One particle. Position is 10.6 fixed point (pixels), velocity 8.8 (pixels per frame).
typedef struct __attribute__((aligned(4))) {
    s16 x, y, vx, vy;
    u8 life, hue, kind, aux;
} Part;

// How a kind of particle moves and draws.
typedef struct {
    u8 grav;      // added to vy each frame (8.8)
    u8 drag;      // v -= v >> drag (0 = none)
    u8 trail;     // trail points drawn behind the head
    u8 spacing;   // trail point spacing, in frames of travel
    u8 fade;      // brightness lost per trail point
    u8 flags;
} Kind;

enum { K_DEAD, K_ROCKET, K_SPARK, K_PEONY, K_CHRYS, K_WILLOW, K_PALM, K_CROSS, K_CRACKLE,
       K_GLITTER, K_POP, K_KID, K_COUNT };
enum { F_ROCKET = 1, F_SPLIT = 2, F_CRACKLE = 4, F_FLICKER = 8, F_BRIGHT = 16 };

// Hues index the BG palette: color = 16 + hue * 16 + brightness (0..15).
enum { H_RED, H_ORANGE, H_GOLD, H_YELLOW, H_LIME, H_GREEN, H_CYAN, H_BLUE, H_VIOLET, H_PINK,
       H_WHITE, H_SILVER, H_WILLOW, H_MAGENTA, H_TEAL };

#define MAXP 1600
#define MAXBURST 8

// A rocket that reached its apex this frame. main.c turns it into a burst.
typedef struct { s16 x, y; u8 aux, hue; } Burst;

extern Part parts[MAXP];
extern int nparts;
extern Burst bursts[MAXBURST];
extern int nbursts;
extern int pops;          // crackle pops this frame (for sound)
extern u32 rng_state;
extern Kind kinds[K_COUNT];

static inline u32 rnd(void) {
    u32 x = rng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return rng_state = x;
}

extern u16 grad[160];     // sky color per scanline, set by the HBlank interrupt

// Moves every particle one frame and draws it into a mode 4 page. Lives in IWRAM.
__attribute__((long_call)) void step_draw(u8* page);
// Clears a mode 4 page with the CPU, so the HBlank interrupt is never held up by DMA.
__attribute__((long_call)) void clear_page(u8* page);
void irq_handler(void);
