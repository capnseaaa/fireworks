// Starbloom's sound renderer, run at build time on the computer (not on the GBA).
//   synth song in.ev out.raw   renders a song from note events (written by make_music.py)
//   synth sfx dir              renders the synthesized firework sounds into dir/*.raw
// Output is signed 8-bit mono PCM, the GBA's DirectSound format.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265358979

static double R;                       // sample rate
static unsigned rs = 12345;
static double noise(void) { rs = rs * 1664525 + 1013904223; return (rs >> 8) / 8388608.0 - 1.0; }

// ---- reverb (Freeverb's shape: 4 combs into 2 allpasses) -------------------------------
typedef struct { double* b; int n, i; double fb, damp, lp; } Comb;
typedef struct { double* b; int n, i; } Allpass;
typedef struct { Comb c[4]; Allpass a[2]; } Verb;

static void verb_init(Verb* v, double size, double fb, double damp) {
    static const int cn[4] = { 1116, 1188, 1277, 1356 }, an[2] = { 556, 441 };
    for (int k = 0; k < 4; k++) {
        v->c[k].n = (int)(cn[k] * R / 44100 * size);
        v->c[k].b = calloc(v->c[k].n, sizeof(double));
        v->c[k].i = 0; v->c[k].fb = fb; v->c[k].damp = damp; v->c[k].lp = 0;
    }
    for (int k = 0; k < 2; k++) {
        v->a[k].n = (int)(an[k] * R / 44100 * size);
        v->a[k].b = calloc(v->a[k].n, sizeof(double));
        v->a[k].i = 0;
    }
}

static double verb_step(Verb* v, double x) {
    double out = 0;
    for (int k = 0; k < 4; k++) {
        Comb* c = &v->c[k];
        double y = c->b[c->i];
        c->lp = y * (1 - c->damp) + c->lp * c->damp;
        c->b[c->i] = x + c->lp * c->fb;
        if (++c->i >= c->n) c->i = 0;
        out += y;
    }
    out *= 0.25;
    for (int k = 0; k < 2; k++) {
        Allpass* a = &v->a[k];
        double b = a->b[a->i];
        a->b[a->i] = out + b * 0.5;
        if (++a->i >= a->n) a->i = 0;
        out = b - out * 0.5;
    }
    return out;
}

// ---- instruments ------------------------------------------------------------------------
enum { I_KEYS, I_BELL, I_HARP, I_PAD, I_BASS, I_FLUTE, I_SHAKER, I_KICK, I_MARIMBA };

static double env_adr(double t, double dur, double a, double r) {   // attack, hold, release
    double e = t < a ? t / a : 1.0;
    if (t > dur) e *= exp(-(t - dur) / r);
    return e;
}

// Adds one note into buf (length n, starting at sample s0). Notes that run off the end
// wrap around when wrap is set, so a looping song's tails ring into its start.
static void note(double* buf, long n, int wrap, int inst, long s0, double dur, double f, double vel) {
    long len = (long)((dur + 3.0) * R);
    if (inst == I_SHAKER) len = (long)(0.12 * R);
    if (inst == I_KICK) len = (long)(0.4 * R);
    double ph = 0, ph2 = 0, ph3 = 0;
    double lp = 0, lp2 = 0;
    for (long i = 0; i < len; i++) {
        double t = i / R, x = 0, w = 2 * PI * f / R;
        switch (inst) {
        case I_KEYS: {           // a soft electric piano: light FM that mellows as it rings
            double tau = 1.4 * pow(440.0 / f, 0.35);
            double idx = 1.1 * exp(-t / 0.35) + 0.15;
            x = sin(ph + idx * sin(ph)) * exp(-t / tau) + 0.12 * sin(2 * ph) * exp(-t / 0.15);
            x *= env_adr(t, dur, 0.004, 0.18);
            ph += w;
            break;
        }
        case I_BELL: {           // a music-box / celesta chime
            x = sin(ph + 1.6 * exp(-t / 0.4) * sin(ph2)) * exp(-t / 1.1);
            x *= t < 0.002 ? t / 0.002 : 1;
            ph += w; ph2 += w * 3.5;
            break;
        }
        case I_MARIMBA: {        // a wooden mallet: fast decay, a little knock
            x = (sin(ph) + 0.25 * sin(ph3) * exp(-t / 0.03)) * exp(-t / 0.45);
            x *= t < 0.002 ? t / 0.002 : 1;
            ph += w; ph3 += w * 4;
            break;
        }
        case I_HARP: {           // a plucked string: bright harmonics that fade faster than the note
            for (int h = 1; h <= 6; h++)
                if (f * h < R * 0.45) x += sin(ph * h) / (h * h * 0.7 + 0.3) * exp(-t * (1.2 + h * h * 0.9) * pow(f / 440.0, 0.5));
            x *= env_adr(t, dur + 0.8, 0.0015, 0.3);
            ph += w;
            break;
        }
        case I_PAD: {            // three detuned soft voices, slow in and out
            for (int v = 0; v < 3; v++) {
                double d = v == 0 ? 1.0 : v == 1 ? 1.0035 : 0.9965;
                double p = (v == 0 ? ph : v == 1 ? ph2 : ph3);
                for (int h = 1; h <= 6; h++) if (f * d * h < R * 0.45) x += sin(p * h) / pow(h, 1.6);
            }
            x *= 0.33;
            lp += (x - lp) * 0.25;
            x = lp;
            x *= env_adr(t, dur, 0.45, 0.7);
            ph += w; ph2 += w * 1.0035; ph3 += w * 0.9965;
            break;
        }
        case I_BASS: {
            x = sin(ph) + 0.22 * sin(2 * ph) + 0.06 * sin(3 * ph);
            x *= (0.55 + 0.45 * exp(-t / 0.3)) * env_adr(t, dur, 0.008, 0.08);
            ph += w;
            break;
        }
        case I_FLUTE: {          // an ocarina-ish lead with a little breath and late vibrato
            double vib = 1 + 0.004 * sin(2 * PI * 5.0 * t) * (t > 0.25 ? 1 : t / 0.25);
            x = sin(ph) + 0.12 * sin(2 * ph) + 0.04 * sin(3 * ph);
            lp2 += (noise() - lp2) * 0.2;
            x += lp2 * 0.08;
            x *= env_adr(t, dur, 0.05, 0.1);
            ph += w * vib;
            break;
        }
        case I_SHAKER: {
            double nz = noise();
            x = nz - lp; lp = nz;                 // a crude high pass
            x *= exp(-t / 0.025) * (t < 0.006 ? t / 0.006 : 1);
            break;
        }
        case I_KICK: {           // a soft, round thump
            double fr = 45 + 50 * exp(-t / 0.04);
            x = sin(ph) * exp(-t / 0.13);
            ph += 2 * PI * fr / R;
            break;
        }
        }
        long j = s0 + i;
        if (j >= n) { if (!wrap) break; j %= n; }
        buf[j] += x * vel;
        if (inst != I_PAD && inst != I_FLUTE && t > dur + 0.5 && fabs(x) < 1e-5 && i > R) break;
    }
}

// Writes buf as signed 8-bit with a little dither, scaled so the peak is at `peak`.
static void write_raw(const char* path, const double* buf, long n, double peak) {
    double m = 1e-9;
    for (long i = 0; i < n; i++) if (fabs(buf[i]) > m) m = fabs(buf[i]);
    FILE* f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    for (long i = 0; i < n; i++) {
        double v = buf[i] / m * peak;          // no dither: at 8 bits it is a hiss
        long q = lround(v);
        if (q > 127) q = 127;
        if (q < -127) q = -127;
        fputc((signed char)q, f);
    }
    fclose(f);
}

// ---- songs ------------------------------------------------------------------------------
// in.ev: "rate total loop wet room gain", then one note per line: "inst start dur freq vel".
static void render_song(const char* in, const char* out) {
    FILE* f = fopen(in, "r");
    if (!f) { perror(in); exit(1); }
    long total; int loop; double wet, room, gain;
    if (fscanf(f, "%lf %ld %d %lf %lf %lf", &R, &total, &loop, &wet, &room, &gain) != 6) exit(1);
    double* dry = calloc(total, sizeof(double));
    int inst; long s0; double dur, fr, vel;
    while (fscanf(f, "%d %ld %lf %lf %lf", &inst, &s0, &dur, &fr, &vel) == 5)
        note(dry, total, loop, inst, s0, dur, fr, vel);
    fclose(f);
    // The reverb runs over the song twice when it loops, keeping the second pass, so the
    // echo of the end carries into the start and the loop has no seam.
    Verb v;
    verb_init(&v, room, 0.78 + 0.1 * room, 0.35);
    double* wetb = calloc(total, sizeof(double));
    for (int pass = 0; pass < (loop ? 2 : 1); pass++)
        for (long i = 0; i < total; i++) wetb[i] = verb_step(&v, dry[i]);
    double* mix = calloc(total, sizeof(double));
    for (long i = 0; i < total; i++) mix[i] = dry[i] * (1 - wet * 0.5) + wetb[i] * wet * 3;
    // Gentle low pass: 8-bit sound gets harsh up top.
    double l = 0;
    for (int pass = 0; pass < 2; pass++)
        for (long i = 0; i < total; i++) { l += (mix[i] - l) * 0.55; if (pass) mix[i] = l; }
    // Even the level out (a soft compressor), so quiet notes stay well above the 8-bit floor.
    double ss = 0;
    for (long i = 0; i < total; i++) ss += mix[i] * mix[i];
    double rms = sqrt(ss / total) + 1e-9;
    for (long i = 0; i < total; i++) mix[i] = tanh(mix[i] / rms * 0.28 * 1.6);
    write_raw(out, mix, total, 124 * gain);
    free(dry); free(wetb); free(mix);
}

// ---- firework sounds (13379 Hz) ---------------------------------------------------------
static double* sbuf;
static long sn;

static void sfx_begin(double secs) { sn = (long)(secs * R); sbuf = calloc(sn, sizeof(double)); }

static void sfx_end(const char* dir, const char* name, double wet, double room, double peak) {
    char path[512];
    snprintf(path, sizeof path, "%s/%s.raw", dir, name);
    if (wet > 0) {
        Verb v;
        verb_init(&v, room, 0.84, 0.3);
        for (long i = 0; i < sn; i++) sbuf[i] = sbuf[i] + verb_step(&v, sbuf[i]) * wet;
    }
    double px = 0, py = 0;
    for (long i = 0; i < sn; i++) {     // block DC (a low rumble can drift off center)
        double y = sbuf[i] - px + 0.993 * py;
        px = sbuf[i]; py = y; sbuf[i] = y;
    }
    for (long i = 0; i < sn; i++) {     // fade the last 40 ms, then soft-limit
        double fo = (sn - i) / (0.04 * R);
        if (fo < 1) sbuf[i] *= fo;
        sbuf[i] = tanh(sbuf[i] * 1.2);
    }
    write_raw(path, sbuf, sn, peak);
    free(sbuf);
}

// A burst: a deep, round pop that rings out long and low, with a soft puff of air
// and the echo off the town. No sharp crack.
static void boom(const char* dir, const char* name, double secs, double body, double bright, unsigned seed) {
    rs = seed;
    sfx_begin(secs);
    double ph = 0, lp = 0, lp2 = 0;
    for (long i = 0; i < sn; i++) {
        double t = i / R;
        double fr = 38 + 70 * exp(-t / 0.035);           // drops fast into a low hum
        double att = t < 0.004 ? t / 0.004 : 1;
        double thump = sin(ph) * exp(-t / (0.32 * body)) * att;
        ph += 2 * PI * fr / R;
        double nz = noise();
        double c = (150 + bright * 500 * exp(-t / 0.04)) / R * 2 * PI;
        lp += (nz - lp) * c;
        lp2 += (lp - lp2) * c;
        double puff = lp2 * 2.2 * exp(-t / (0.09 * body)) * att;
        sbuf[i] = thump + puff;
    }
    sfx_end(dir, name, 0.35, 1.5, 110);
}

// Going up: a soft whoosh of air that rises and fades, like a shell climbing.
static void launch(const char* dir) {
    rs = 777;
    sfx_begin(1.0);
    double lp = 0, lp2 = 0, hp = 0;
    for (long i = 0; i < sn; i++) {
        double t = i / R;
        double c = (300 + 1800 * (t / 1.0)) / R * 2 * PI;   // the band climbs
        double nz = noise();
        lp += (nz - lp) * c;
        lp2 += (lp - lp2) * c;
        double b = lp2 - hp; hp += (lp2 - hp) * (150 / R * 2 * PI);
        double env = t < 0.25 ? t / 0.25 : exp(-(t - 0.25) / 0.25);
        sbuf[i] = b * env * 3;
    }
    sfx_end(dir, "launch", 0.15, 0.8, 80);
}

// Crackling stars: soft little pops, thick at first and thinning out.
static void crackle(const char* dir) {
    rs = 4242;
    sfx_begin(1.8);
    for (long i = 0; i < sn; i++) {
        double t = i / R;
        double rate = 70 * exp(-t / 0.5) + 4;
        if ((noise() * 0.5 + 0.5) < rate / R) {
            double a = 0.3 + 0.4 * (noise() * 0.5 + 0.5), fr = 900 + 900 * (noise() * 0.5 + 0.5);
            for (int k = 0; k < (int)(0.012 * R) && i + k < sn; k++)
                sbuf[i + k] += a * sin(2 * PI * fr * k / R) * exp(-k / (0.003 * R)) * exp(-t / 0.9);
        }
    }
    sfx_end(dir, "crackle", 0.5, 1.2, 80);
}

// Shimmer: a soft wash of high, bell-like sparkles that swells and fades slowly, for
// glitter, willows, and chrysanthemums.
static void glitter(const char* dir) {
    rs = 99;
    sfx_begin(2.2);
    for (long i = 0; i < sn; i++) {
        double t = i / R;
        double rate = 45 * (t < 0.3 ? t / 0.3 : exp(-(t - 0.3) / 0.7)) + 1;
        if ((noise() * 0.5 + 0.5) < rate / R) {
            double fr = 1800 + 2600 * (noise() * 0.5 + 0.5), a = 0.15 + 0.2 * (noise() * 0.5 + 0.5);
            for (int k = 0; k < (int)(0.18 * R) && i + k < sn; k++) {
                double kt = k / R;
                sbuf[i + k] += a * sin(2 * PI * fr * kt) * exp(-kt / 0.05) * (kt < 0.004 ? kt / 0.004 : 1);
            }
        }
    }
    sfx_end(dir, "glitter", 0.7, 1.3, 75);
}

// A bubble popping into view at review time.
static void pop(const char* dir) {
    rs = 5;
    sfx_begin(0.18);
    double ph = 0;
    for (long i = 0; i < sn; i++) {
        double t = i / R;
        double fr = 380 + 700 * exp(-t / 0.025);
        sbuf[i] = sin(ph) * exp(-t / 0.045) * (t < 0.002 ? t / 0.002 : 1);
        ph += 2 * PI * fr / R;
    }
    sfx_end(dir, "pop", 0.15, 0.6, 105);
}

// A glassy chime for each star of the rating (the game raises its pitch star by star).
static void chime(const char* dir) {
    rs = 6;
    sfx_begin(1.1);
    double ph = 0, ph2 = 0, f = 1046.5;
    for (long i = 0; i < sn; i++) {
        double t = i / R;
        sbuf[i] = sin(ph + 1.2 * exp(-t / 0.15) * sin(ph2)) * exp(-t / 0.35) * (t < 0.002 ? t / 0.002 : 1)
                + 0.3 * sin(ph * 2) * exp(-t / 0.12);
        ph += 2 * PI * f / R; ph2 += 2 * PI * f * 2 / R;
    }
    sfx_end(dir, "chime", 0.3, 0.8, 100);
}

// Water: a splash for the tide pools and the hook.
static void splash(const char* dir) {
    rs = 31337;
    sfx_begin(0.6);
    double lp = 0, hp = 0, ph = 0;
    for (long i = 0; i < sn; i++) {
        double t = i / R;
        double nz = noise();
        lp += (nz - lp) * 0.3;
        double b = lp - hp; hp += (lp - hp) * 0.05;
        double fr = 500 + 900 * exp(-t / 0.08);
        double bub = sin(ph) * 0.25 * exp(-t / 0.1);
        ph += 2 * PI * fr / R;
        sbuf[i] = b * (t < 0.01 ? t / 0.01 : exp(-(t - 0.01) / 0.13)) + bub;
    }
    sfx_end(dir, "splash", 0.15, 0.7, 100);
}

// A warm "ta-da" for crafting a firework: two quick plucked notes, a fifth apart.
static void crafted(const char* dir) {
    rs = 8;
    sfx_begin(0.9);
    double fs[2] = { 784, 1175 };
    for (int k = 0; k < 2; k++) {
        double ph = 0;
        long s0 = (long)(k * 0.08 * R);
        for (long i = 0; s0 + i < sn; i++) {
            double t = i / R;
            sbuf[s0 + i] += 0.6 * (sin(ph) + 0.3 * sin(2 * ph) * exp(-t / 0.1)) * exp(-t / 0.3) * (t < 0.002 ? t / 0.002 : 1);
            ph += 2 * PI * fs[k] / R;
        }
    }
    sfx_end(dir, "crafted", 0.25, 0.8, 100);
}

int main(int argc, char** argv) {
    if (argc == 4 && !strcmp(argv[1], "song")) { render_song(argv[2], argv[3]); return 0; }
    if (argc == 3 && !strcmp(argv[1], "sfx")) {
        R = 13379;
        const char* d = argv[2];
        launch(d);
        boom(d, "boom_s", 1.4, 0.7, 0.6, 11);
        boom(d, "boom_m", 1.9, 1.0, 0.8, 12);
        boom(d, "boom_l", 2.5, 1.4, 1.0, 13);
        crackle(d);
        glitter(d);
        pop(d);
        chime(d);
        splash(d);
        crafted(d);
        return 0;
    }
    fprintf(stderr, "usage: synth song in.ev out.raw | synth sfx dir\n");
    return 1;
}
