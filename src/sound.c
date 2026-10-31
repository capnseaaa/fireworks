// Sound effects and music. The mixing and streaming happen in the VBlank interrupt
// (render.c); this file starts sounds and songs. Data comes from audio.s (tools/make_sfx.py).
#include "game.h"

typedef struct { const s8* data; u32 len; } Clip;
extern const Clip sfx_table[NUM_SFX], song_table[NUM_SONGS];

static int cur_song = -1;
s8 sfx_ring[RING * 224] __attribute__((aligned(4)));   // mixed sound effects (render.c)
int music_off, sfx_off;           // dev menu switches, for tracking down sounds
static u32 voice_age;

// A sound for later: a burst is seen before it is heard.
typedef struct { s16 id, vol, pitch, wait; } Later;
static Later later[8];

void sound_init(void) {
    *(vu16*)0x04000084 = 0x80;             // sound on
    *(vu16*)0x04000080 = 0;                // the tone channels stay quiet
    *(vu16*)0x04000082 = (1 << 2) | (1 << 3) | (1 << 8) | (1 << 9) | (1 << 12) | (1 << 13) | (1 << 14) | (1 << 11) | (1 << 15);
    *(vu16*)0x04000100 = 65536 - 1254;     // timer 0: 13379 Hz, effects
    *(vu16*)0x04000102 = 0x80;
    *(vu16*)0x04000104 = 65536 - 924;      // timer 1: 18157 Hz, music
    *(vu16*)0x04000106 = 0x80;
}

// vol 0..64; pitch 256 = as recorded (higher is higher and shorter).
void sfx_ex(int id, int vol, int pitch) {
    if (id < 0 || id >= NUM_SFX || vol <= 0 || sfx_off) return;
    // A free voice, or else the oldest one.
    int best = 0;
    for (int v = 0; v < NVOICE; v++) {
        if (!voices[v].data) { best = v; break; }
        if (voices[v].age < voices[best].age) best = v;
    }
    REG_IME = 0;
    Voice* vo = &voices[best];
    vo->data = 0;
    vo->pos = 0;
    vo->end = sfx_table[id].len << 12;
    vo->step = pitch << 4;
    vo->vol = vol > 64 ? 64 : vol;
    vo->age = ++voice_age;
    vo->data = sfx_table[id].data;
    REG_IME = 1;
}

void sfx(int id) { sfx_ex(id, 64, 256); }

void sfx_later(int id, int vol, int pitch, int frames) {
    if (frames <= 0) { sfx_ex(id, vol, pitch); return; }
    for (int i = 0; i < 8; i++)
        if (!later[i].wait) { later[i] = (Later){ id, vol, pitch, frames }; return; }
    sfx_ex(id, vol, pitch);
}

void sound_frame(void) {
    for (int i = 0; i < 8; i++)
        if (later[i].wait && --later[i].wait == 0) sfx_ex(later[i].id, later[i].vol, later[i].pitch);
}

void sfx_stop_all(void) {
    REG_IME = 0;
    for (int v = 0; v < NVOICE; v++) voices[v].data = 0;
    REG_IME = 1;
    for (int i = 0; i < 8; i++) later[i].wait = 0;
}

// The OPTIONS settings live at the far end of the save memory, away from the game's save,
// so they never touch it: a tag, then music off, sound effects off.
#define SETTINGS ((volatile u8*)0x0E007F00)

void settings_load(void) {
    if (SETTINGS[0] != 'S' || SETTINGS[1] != 'B' || SETTINGS[2] != 'O' || SETTINGS[3] != '1') return;
    music_off = SETTINGS[4] & 1;
    sfx_off = SETTINGS[5] & 1;
}

void settings_save(void) {
    SETTINGS[0] = 'S'; SETTINGS[1] = 'B'; SETTINGS[2] = 'O'; SETTINGS[3] = '1';
    SETTINGS[4] = music_off;
    SETTINGS[5] = sfx_off;
}

// Turns the music on or off (OPTIONS, the dev menu). On again, the song starts over.
void music_toggle(void) {
    int s = cur_song;
    music_off ^= 1;
    cur_song = -1;
    music(s);
}

// Starts a song from the top, unless it is already playing. -1 = silence. The day's
// versions (town, forest, beach, quarry) are one song with different sounds of the
// place over it, all the same length, so walking between them carries on mid-tune.
void music(int song) {
    if (song == cur_song) return;
    int same = cur_song >= 0 && song >= 0 && mus_data && song_table[song].len == mus_len;
    cur_song = song;
    REG_IME = 0;
    mus_data = 0;
    mus_restart = 1;                 // the next VBlank starts (or stops) the stream
    if (song >= 0 && !music_off) {
        if (same) mus_restart = 2;
        else mus_pos = 0;
        mus_len = song_table[song].len;
        mus_data = song_table[song].data;
    }
    REG_IME = 1;
}
