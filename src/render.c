// Particle update and drawing. Compiled as ARM code and copied to IWRAM at boot:
// this loop is the whole frame budget, so it must not call into ROM (no / or %).
#include "fw.h"

#define IWRAM_CODE __attribute__((section(".iwram"), noinline))

Part parts[MAXP];
int nparts;
Burst bursts[MAXBURST];
int nbursts;
int pops;
u32 rng_state = 0x1234567;
u16 grad[160];

// Called by the BIOS on each interrupt (ARM mode). On HBlank, it sets the backdrop for
// the next line from VCOUNT, so it stays right even if some HBlanks were missed.
// (v0.1.0 used HBlank DMA for this; Delta on iOS drew it as colored stripes.)
IWRAM_CODE void irq_handler(void) {
    u16 f = *(vu16*)0x04000202 & *(vu16*)0x04000200;
    if (f & 2) {
        int next = *(vu16*)0x04000006 + 1;
        if (next >= 228) next = 0;
        if (next < 160) *(vu16*)0x05000000 = grad[next];
    }
    *(vu16*)0x04000202 = f;
    *(vu16*)0x03007FF8 |= f;          // BIOS copy of IF, for VBlankIntrWait users
}

IWRAM_CODE void clear_page(u8* page) {
    register u32* p asm("r0") = (u32*)page;
    register u32* end asm("r1") = (u32*)(page + 240 * 160);
    asm volatile(
        "mov r2, #0\n mov r3, #0\n mov r4, #0\n mov r5, #0\n"
        "mov r6, #0\n mov r7, #0\n mov r8, #0\n mov r9, #0\n"
        "1: stmia %0!, {r2-r9}\n stmia %0!, {r2-r9}\n cmp %0, %1\n blt 1b\n"
        : "+r"(p) : "r"(end) : "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "cc", "memory");
}

Kind kinds[K_COUNT] = {
    //            grav drag trail spacing fade flags
    [K_ROCKET]  = { 4, 0, 4, 1, 3, F_ROCKET },
    [K_SPARK]   = { 2, 4, 0, 1, 0, 0 },
    [K_PEONY]   = { 3, 5, 2, 2, 5, 0 },
    [K_CHRYS]   = { 3, 5, 6, 1, 2, 0 },
    [K_WILLOW]  = { 3, 5, 8, 3, 1, 0 },
    [K_PALM]    = { 4, 6, 7, 1, 2, 0 },
    [K_CROSS]   = { 2, 6, 4, 1, 3, F_SPLIT },
    [K_CRACKLE] = { 3, 5, 3, 1, 4, F_CRACKLE },
    [K_GLITTER] = { 2, 4, 0, 1, 0, F_FLICKER },
    [K_POP]     = { 0, 2, 0, 1, 0, F_BRIGHT },
    [K_KID]     = { 3, 5, 4, 1, 3, 0 },
};

// Mode 4 VRAM only takes 16-bit writes. A dimmer pixel never covers a brighter one.
static inline void plot(u8* page, int x, int y, int c) {
    if ((unsigned)x >= 240 || (unsigned)y >= 160) return;
    vu16* p = (vu16*)(page + y * 240 + (x & ~1));
    u16 o = *p;
    if (x & 1) { if (((o >> 8) & 15) <= (c & 15)) *p = (o & 0xFF) | (c << 8); }
    else       { if ((o & 15) <= (c & 15)) *p = (o & 0xFF00) | c; }
}

static inline void add(int* n, int x, int y, int vx, int vy, int life, int hue, int kind) {
    if (*n >= MAXP) return;
    Part* s = &parts[(*n)++];
    s->x = x; s->y = y; s->vx = vx; s->vy = vy;
    s->life = life; s->hue = hue; s->kind = kind; s->aux = 0;
}

IWRAM_CODE void step_draw(u8* page) {
    int n = nparts;
    int i = 0;
    while (i < n) {
        Part* p = &parts[i];
        const Kind* k = &kinds[p->kind];
        if (p->life == 0) {
            if (k->flags & F_ROCKET) {
                if (nbursts < MAXBURST) {
                    Burst* b = &bursts[nbursts++];
                    b->x = p->x; b->y = p->y; b->aux = p->aux; b->hue = p->hue;
                }
            } else if (k->flags & F_CRACKLE) {
                for (int j = 0; j < 2; j++) {
                    u32 r = rnd();
                    add(&n, p->x, p->y, (int)(r & 127) - 64, (int)((r >> 7) & 127) - 64,
                        3 + ((r >> 14) & 3), H_WHITE, K_POP);
                }
                pops++;
            } else if (k->flags & F_SPLIT) {
                // Four kids in a + or an x.
                int s = 260, d = 184;
                if (rnd() & 1) {
                    add(&n, p->x, p->y, s, 0, 40, p->hue, K_KID);
                    add(&n, p->x, p->y, -s, 0, 40, p->hue, K_KID);
                    add(&n, p->x, p->y, 0, s, 40, p->hue, K_KID);
                    add(&n, p->x, p->y, 0, -s, 40, p->hue, K_KID);
                } else {
                    add(&n, p->x, p->y, d, d, 40, p->hue, K_KID);
                    add(&n, p->x, p->y, -d, d, 40, p->hue, K_KID);
                    add(&n, p->x, p->y, d, -d, 40, p->hue, K_KID);
                    add(&n, p->x, p->y, -d, -d, 40, p->hue, K_KID);
                }
            }
            // Remove: move the last particle into this slot (word copy, no memcpy).
            n--;
            u32* dst = (u32*)p;
            u32* src = (u32*)&parts[n];
            dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2];
            continue;
        }
        p->life--;

        int vx = p->vx, vy = p->vy;
        if (k->drag) { vx -= vx >> k->drag; vy -= vy >> k->drag; }
        vy += k->grav;
        p->vx = vx; p->vy = vy;
        int x = p->x + (vx >> 2), y = p->y + (vy >> 2);
        p->x = x; p->y = y;

        if (y > (168 << 6) || x < (-24 << 6) || x > (264 << 6)) { p->life = 0; i++; continue; }

        int base = 16 + (p->hue << 4);
        int lv = p->life >= 30 ? 15 : p->life >> 1;
        if (k->flags & F_ROCKET) {
            if (vy >= 0) p->life = 0;          // apex: bursts next pass
            base = 16 + (H_GOLD << 4);
            lv = 15;
            u32 r = rnd();
            add(&n, x, y, (int)(r & 63) - 32, (vy >> 2) + 24 + (int)((r >> 6) & 15),
                8 + ((r >> 10) & 7), H_GOLD, K_SPARK);
        } else if (k->flags & F_BRIGHT) {
            lv = 15;
        } else if (k->flags & F_FLICKER) {
            if (rnd() & 3) lv >>= 2;
            else lv = 15;
        }

        plot(page, x >> 6, y >> 6, base + lv);
        int t = k->trail;
        if (t) {
            int dx = (vx >> 2) * k->spacing, dy = (vy >> 2) * k->spacing;
            int fade = k->fade;
            while (t--) {
                lv -= fade;
                if (lv < 0) break;
                x -= dx; y -= dy;
                plot(page, x >> 6, y >> 6, base + lv);
            }
        }
        i++;
    }
    nparts = n;
}
