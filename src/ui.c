// Text, boxes, and word wrap, drawn straight into a mode 4 page.
#include "game.h"
#include "art.h"

static const u8* glyphs[128];

const u8* glyph_rows(char c) { return glyphs[(u8)c & 127]; }

void ui_init(void) {
    for (int i = 0; i < FONT_N; i++) glyphs[(u8)FONT_CHARS[i]] = &font_rows[i * 7];
}

// BG palette 1..15: the menu colors, the same in every scene.
void ui_palette(void) {
    static const u16 cols[16] = {
        0, RGB(31, 31, 30), RGB(3, 3, 8), RGB(5, 5, 12), RGB(29, 23, 10), RGB(12, 8, 4),
        RGB(16, 16, 19), RGB(31, 11, 10), RGB(11, 28, 12), RGB(31, 13, 19), RGB(31, 27, 12), RGB(9, 9, 20),
        RGB(2, 13, 3), RGB(20, 2, 2), RGB(2, 2, 7), RGB(18, 10, 0),
    };
    for (int i = 1; i < 16; i++) BG_PAL[i] = cols[i];
}

void text_sh(u8* page, int x, int y, const char* s, int color, int shadow) {
    for (; *s; s++, x += 6) {
        const u8* g = glyphs[(u8)*s & 127];
        if (g && *s != ' ') draw_glyph(page, x, y, g, color, shadow);
    }
}

void text(u8* page, int x, int y, const char* s, int color) {
    text_sh(page, x, y, s, color, UI_SHADOW);
}

void text_big(u8* page, int x, int y, const char* s, int color) {
    for (; *s; s++, x += 12) {
        const u8* g = glyphs[(u8)*s & 127];
        if (!g) continue;
        for (int r = 0; r < 7; r++)
            for (int b = 0; b < 5; b++)
                if (g[r] & (16 >> b)) fill_rect(page, x + b * 2, y + r * 2, 2, 2, color);
    }
}

int text_w(const char* s) {
    int n = 0;
    while (s[n]) n++;
    return n * 6 - 1;
}

// A rectangle outline, t pixels thick.
void frame_rect(u8* page, int x, int y, int w, int h, int t, int c) {
    fill_rect(page, x, y, w, t, c);
    fill_rect(page, x, y + h - t, w, t, c);
    fill_rect(page, x, y + t, t, h - 2 * t, c);
    fill_rect(page, x + w - t, y + t, t, h - 2 * t, c);
}

// Gold-trimmed window, in the spirit of Pokemon Emerald's. Each pixel is drawn once.
void box(u8* page, int x, int y, int w, int h) {
    frame_rect(page, x, y, w, h, 1, UI_GOLD_DK);
    frame_rect(page, x + 1, y + 1, w - 2, h - 2, 2, UI_GOLD);
    frame_rect(page, x + 3, y + 3, w - 6, h - 6, 1, UI_GOLD_DK);
    fill_rect(page, x + 4, y + 4, w - 8, h - 8, UI_FILL);
}

// Splits text into lines of at most maxchars, at spaces and '\n'. Returns the line count.
int wrap(const char* s, int maxchars, char out[][41], int maxlines) {
    int n = 0;
    while (*s && n < maxlines) {
        int len = 0, cut = -1;
        while (s[len] && s[len] != '\n' && len < maxchars) {
            if (s[len] == ' ') cut = len;
            len++;
        }
        if (s[len] && s[len] != '\n' && s[len] != ' ' && cut > 0) len = cut;
        for (int i = 0; i < len; i++) out[n][i] = s[i];
        out[n][len] = 0;
        n++;
        s += len;
        while (*s == ' ') s++;
        if (*s == '\n') s++;
    }
    return n;
}

char* cat(char* d, const char* s) {
    while (*s) *d++ = *s++;
    *d = 0;
    return d;
}

char* cat_num(char* d, int v) {
    if (v < 0) { *d++ = '-'; v = -v; }
    char tmp[10];
    int n = 0;
    do { tmp[n++] = '0' + v % 10; v /= 10; } while (v);
    while (n) *d++ = tmp[--n];
    *d = 0;
    return d;
}
