// Particle update and drawing. Compiled as ARM code and copied to IWRAM at boot:
// this loop is the whole frame budget, so it must not call into ROM (no / or %).
#include "fw.h"

#define IWRAM_CODE __attribute__((section(".iwram"), noinline))

Part parts[MAXP];
int nparts;
Burst bursts[MAXBURST];
int nbursts;
u32 rng_state = 0x1234567;
u16 grad[160];
int lod;                        // 0 full detail; 1, 2: trails drawn with 1/2, 1/4 the dots

// ---- sound -------------------------------------------------------------------------
// Sound effects: up to NVOICE samples mixed in software into DirectSound A at 13379 Hz,
// 224 samples a frame, written two frames ahead into a ring of RING frames. Music: a
// pre-rendered song streamed straight from the ROM to DirectSound B at 18157 Hz.
//
// Both DMAs run straight on; a stream restarts only when it wraps (the ring's end, a
// song's loop) or the song changes. (v0.11.0 restarted both every frame. VBA-M, the core
// in Delta, keeps looser time, so each restart could drop or repeat a sample: static.)
// A restart empties the FIFO, the CPU puts the first 16 samples in itself, and the DMA
// carries on from there, so the FIFO never plays a stray sample while it refills.
Voice voices[NVOICE];
const s8* mus_data;
u32 mus_pos, mus_len;
u8 mus_restart;                 // 1: start at mus_pos; 2: carry on at the next frame's spot
static s16 acc[224];
static int ring_slot = RING - 1;

// Music at 100%: VBA-M makes 50% by dropping the low bit, which doubles the hiss.
#define SNDH_BASE ((1 << 2) | (1 << 3) | (1 << 8) | (1 << 9) | (1 << 12) | (1 << 13) | (1 << 14))

IWRAM_CODE static void mix(s8* out) {
    for (int i = 0; i < 224; i++) acc[i] = 0;
    for (int v = 0; v < NVOICE; v++) {
        Voice* vo = &voices[v];
        const s8* d = vo->data;
        if (!d) continue;
        u32 pos = vo->pos, end = vo->end, step = vo->step;
        int vol = vo->vol;
        for (int i = 0; i < 224; i++) {
            if (pos >= end) { vo->data = 0; break; }
            acc[i] += d[pos >> 12] * vol;
            pos += step;
        }
        vo->pos = pos;
    }
    for (int i = 0; i < 224; i++) {
        int x = acc[i] >> 6;                 // one sound at full volume peaks near 120
        out[i] = x > 127 ? 127 : x < -127 ? -127 : x;
    }
}

// Restarts FIFO A (b = 0) or B (b = 1) at p, or stops it (p = 0).
IWRAM_CODE static void restart_fifo(int b, const u32* p) {
    vu32* fifo = (vu32*)(b ? 0x040000A4 : 0x040000A0);
    vu16* cnt = (vu16*)(b ? 0x040000D2 : 0x040000C6);
    *cnt = 0;
    *(vu16*)0x04000082 = SNDH_BASE | (b ? 1 << 15 : 1 << 11);
    if (!p) return;
    *fifo = p[0]; *fifo = p[1]; *fifo = p[2]; *fifo = p[3];
    *(vu32*)(b ? 0x040000C8 : 0x040000BC) = (u32)(p + 4);
    *(vu32*)(b ? 0x040000CC : 0x040000C0) = (u32)fifo;
    *cnt = 0xB640;                           // on, sound FIFO timing, 32-bit, repeat
}

IWRAM_CODE static void sound_vblank(void) {
    if (++ring_slot == RING) { ring_slot = 0; restart_fifo(0, (const u32*)sfx_ring); }
    if (mus_restart) {
        if (mus_restart == 2) { mus_pos += 304; if (mus_pos + 304 > mus_len) mus_pos = 0; }
        mus_restart = 0;
        restart_fifo(1, mus_data ? (const u32*)(mus_data + mus_pos) : 0);
    } else if (mus_data) {
        mus_pos += 304;
        if (mus_pos + 304 > mus_len) {       // songs are a whole number of frames
            mus_pos = 0;
            restart_fifo(1, (const u32*)mus_data);
        }
    }
    mix(sfx_ring + ((ring_slot + 2) & (RING - 1)) * 224);
}

// Called by the BIOS on each interrupt (ARM mode). On HBlank, it sets the backdrop for
// the next line from VCOUNT, so it stays right even if some HBlanks were missed.
// (v0.1.0 used HBlank DMA for this; Delta on iOS drew it as colored stripes.)
IWRAM_CODE void irq_handler(void) {
    u16 f = *(vu16*)0x04000202 & *(vu16*)0x04000200;
    if (f & 1) sound_vblank();
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
    [K_GLINT]   = { 1, 4, 3, 1, 4, F_BRIGHT },   // review time sparkles
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
            // When the sky is busy, the same trail with fewer, wider-spaced dots.
            int l = lod;
            t >>= l;
            if (!t) t = 1;
            int dx = ((vx >> 2) * k->spacing) << l, dy = ((vy >> 2) * k->spacing) << l;
            int fade = k->fade << l;
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

// --- page drawing for the menus and text ----------------------------------------

static inline void put(u8* page, int x, int y, int c) {
    vu16* p = (vu16*)(page + y * 240 + (x & ~1));
    if (x & 1) *p = (*p & 0xFF) | (c << 8);
    else *p = (*p & 0xFF00) | c;
}

IWRAM_CODE void copy_page(u8* dst, const u8* src) {
    register u32* d asm("r0") = (u32*)dst;
    register const u32* s asm("r1") = (const u32*)src;
    register u32* end asm("r2") = (u32*)(dst + 240 * 160);
    asm volatile(
        "1: ldmia %1!, {r3-r10}\n stmia %0!, {r3-r10}\n cmp %0, %2\n blt 1b\n"
        : "+r"(d), "+r"(s) : "r"(end) : "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "cc", "memory");
}

IWRAM_CODE void fill_rect(u8* page, int x, int y, int w, int h, int c) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > 240) w = 240 - x;
    if (y + h > 160) h = 160 - y;
    if (w <= 0 || h <= 0) return;
    u32 cc = c | (c << 8);
    cc |= cc << 16;
    int x1 = x + w;                           // one past the last column
    for (int yy = y; yy < y + h; yy++) {
        u8* row = page + yy * 240;
        int xx = x;
        if (xx & 1) { put(page, xx, yy, c); xx++; }
        if ((xx & 2) && xx + 2 <= x1) { *(vu16*)(row + xx) = cc; xx += 2; }
        while (xx + 4 <= x1) { *(vu32*)(row + xx) = cc; xx += 4; }
        if (xx + 2 <= x1) { *(vu16*)(row + xx) = cc; xx += 2; }
        if (xx < x1) put(page, xx, yy, c);
    }
}

// A 5x7 glyph: rows hold 5 bits, the leftmost pixel in bit 4. Shadow goes 1 px down-right.
IWRAM_CODE void draw_glyph(u8* page, int x, int y, const u8* rows, int c, int shadow) {
    if (x < 0 || y < 0 || x > 233 || y > 151) return;
    if (shadow)
        for (int r = 0; r < 7; r++)
            for (int b = 0; b < 5; b++)
                if (rows[r] & (16 >> b)) put(page, x + b + 1, y + r + 1, shadow);
    for (int r = 0; r < 7; r++)
        for (int b = 0; b < 5; b++)
            if (rows[r] & (16 >> b)) put(page, x + b, y + r, c);
}
