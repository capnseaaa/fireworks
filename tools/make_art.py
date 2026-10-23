"""Draw the shop, customers, emote bubbles, and font, and write src/art.c + src/art.h.
Everything is drawn here in code; previews land in art/ (not committed)."""
import os, random
from PIL import Image, ImageDraw

ROOT = os.path.join(os.path.dirname(__file__), "..")
OUT_C = os.path.join(ROOT, "src", "art.c")
OUT_H = os.path.join(ROOT, "src", "art.h")
PREVIEW = os.path.join(ROOT, "art")
os.makedirs(PREVIEW, exist_ok=True)

# ---- shop palette ----------------------------------------------------------------
# BG palette 16..27 are the firework hues (tubes on shelves and in the crate), the same
# order as the game's hue list. The shop art colors follow from 28.
HUES = [(248, 40, 40), (248, 110, 30), (240, 180, 50), (248, 236, 80), (140, 240, 50), (40, 200, 80),
        (50, 210, 240), (60, 100, 240), (150, 70, 240), (248, 100, 180), (240, 240, 240), (180, 196, 230)]
COLORS = {}
pal = list(HUES)


def col(name, rgb):
    COLORS[name] = 16 + len(pal)
    pal.append(rgb)


for name, rgb in [
    ("wall", (92, 56, 40)), ("wall_line", (62, 38, 28)), ("wall_hi", (106, 66, 46)),
    ("wall_lit", (124, 80, 52)), ("wall_lit_line", (90, 56, 38)), ("wall_lit_hi", (142, 94, 60)),
    ("wall_dk", (70, 42, 32)), ("wall_dk_line", (48, 30, 24)), ("wall_dk_hi", (80, 50, 36)),
    ("frame", (52, 32, 24)), ("frame_hi", (96, 62, 40)), ("star", (230, 226, 196)), ("moon", (246, 232, 170)),
    ("shelf", (66, 40, 28)), ("shelf_hi", (126, 84, 52)), ("paper", (226, 206, 164)), ("fuse", (40, 28, 20)),
    ("jar", (128, 150, 156)), ("jar_hi", (190, 210, 214)), ("powder", (46, 44, 50)),
    ("lamp", (58, 40, 30)), ("glow", (255, 220, 130)), ("glow2", (255, 184, 90)),
    ("top", (160, 108, 64)), ("top_hi", (190, 134, 80)), ("front", (104, 64, 42)), ("panel", (88, 54, 36)),
    ("edge", (56, 34, 24)), ("bar", (24, 18, 30)), ("bell", (214, 178, 84)), ("bell_dk", (150, 116, 50)),
    ("crate", (140, 98, 58)), ("crate_dk", (96, 64, 38)),
]:
    col(name, rgb)
assert 16 + len(pal) <= 256
C = COLORS

W, H = 240, 160
bg = Image.new("P", (W, H), 0)
px = bg.load()
d = ImageDraw.Draw(bg)
rng = random.Random(3)

# Wall: vertical planks, lit around the lantern, darker at the edges.
LAMP = (120, 24)
for y in range(12, 116):
    for x in range(W):
        dx, dy = x - LAMP[0], (y - LAMP[1]) * 1.3
        dist = (dx * dx + dy * dy) ** 0.5
        dith = ((x * 7 + y * 13) % 5) * 3
        tier = "_lit" if dist + dith < 56 else ("" if dist + dith < 120 else "_dk")
        k = x % 12
        name = "wall" + tier
        if k == 0:
            px[x, y] = C["wall" + tier + "_line"] if tier else C["wall_line"]
        elif k == 1:
            px[x, y] = C["wall" + tier + "_hi"] if tier else C["wall_hi"]
        else:
            px[x, y] = C[name]
        if k > 1 and rng.random() < 0.02:     # wood grain
            px[x, y] = C["wall" + tier + "_line"] if tier else C["wall_line"]

# Window: index 0 inside, so the sky gradient behind the bitmap shows through.
wx0, wy0, wx1, wy1 = 12, 20, 92, 76
d.rectangle((wx0, wy0, wx1, wy1), fill=C["frame"])
d.rectangle((wx0 + 3, wy0 + 3, wx1 - 3, wy1 - 3), fill=0)
d.rectangle(((wx0 + wx1) // 2 - 1, wy0, (wx0 + wx1) // 2, wy1), fill=C["frame"])
d.rectangle((wx0, (wy0 + wy1) // 2 - 1, wx1, (wy0 + wy1) // 2), fill=C["frame"])
d.rectangle((wx0 - 2, wy1 + 1, wx1 + 2, wy1 + 4), fill=C["frame_hi"])
d.line((wx0 - 2, wy1 + 4, wx1 + 2, wy1 + 4), fill=C["frame"])
for _ in range(14):
    x, y = rng.randrange(wx0 + 4, wx1 - 3), rng.randrange(wy0 + 4, wy1 - 3)
    if px[x, y] == 0:
        px[x, y] = C["star"]
for y in range(26, 36):            # crescent moon in the top right pane
    for x in range(72, 84):
        if (x - 78) ** 2 + (y - 31) ** 2 <= 16 and (x - 80) ** 2 + (y - 29) ** 2 > 12:
            px[x, y] = C["moon"]

# Shelves on the right with firework tubes and jars.
for sy in (36, 60, 84):
    for x in range(156, 236):
        px[x, sy] = C["shelf_hi"]
        px[x, sy + 1] = C["shelf"]
        px[x, sy + 2] = C["shelf"]
    x = 158
    while x < 230:
        if rng.random() < 0.25:            # jar of powder
            d.rectangle((x, sy - 9, x + 6, sy - 1), fill=C["jar"])
            d.rectangle((x + 1, sy - 5, x + 5, sy - 1), fill=C["powder"])
            d.line((x + 1, sy - 8, x + 1, sy - 6), fill=C["jar_hi"])
            d.rectangle((x + 1, sy - 11, x + 5, sy - 10), fill=C["frame_hi"])
            x += 9
        else:                               # tube: colored body, paper top, fuse
            h = rng.choice((10, 12, 14))
            hue = rng.randrange(12)
            d.rectangle((x, sy - h, x + 3, sy - 1), fill=16 + hue)
            d.line((x, sy - h, x + 3, sy - h), fill=C["paper"])
            d.line((x, sy - 4, x + 3, sy - 4), fill=C["paper"])
            px[x + 1, sy - h - 1] = C["fuse"]
            px[x + 2, sy - h - 2] = C["fuse"]
            x += 5 + rng.randrange(2)

# Hanging lantern.
d.line((LAMP[0], 12, LAMP[0], 17), fill=C["lamp"])
d.rectangle((LAMP[0] - 4, 18, LAMP[0] + 4, 20), fill=C["lamp"])
d.rectangle((LAMP[0] - 3, 21, LAMP[0] + 3, 29), fill=C["glow"])
d.rectangle((LAMP[0] - 3, 21, LAMP[0] - 3, 29), fill=C["glow2"])
d.rectangle((LAMP[0] + 3, 21, LAMP[0] + 3, 29), fill=C["glow2"])
d.rectangle((LAMP[0] - 4, 30, LAMP[0] + 4, 31), fill=C["lamp"])

# Counter: top at y 98, front down to the dialog box.
d.rectangle((0, 98, W - 1, 103), fill=C["top"])
d.line((0, 98, W - 1, 98), fill=C["top_hi"])
d.line((0, 104, W - 1, 104), fill=C["edge"])
d.rectangle((0, 105, W - 1, 115), fill=C["front"])
for x in range(6, W, 40):
    d.rectangle((x, 107, x + 30, 115), outline=C["panel"])
# Bell on the counter, left side.
d.ellipse((40, 89, 50, 99), fill=C["bell"])
d.rectangle((40, 95, 50, 97), fill=C["bell_dk"])
d.rectangle((44, 87, 46, 88), fill=C["bell_dk"])
# Status bar.
d.rectangle((0, 0, W - 1, 10), fill=C["bar"])
d.line((0, 11, W - 1, 11), fill=C["frame"])

bg_rgb = Image.new("RGB", (W, H))
flat = []
for c in [(20, 24, 60)] + [(0, 0, 0)] * 15 + pal:
    flat += list(c)
bg.putpalette(flat + [0] * (768 - len(flat)))
bg.convert("RGB").save(os.path.join(PREVIEW, "shop_preview.png"))

# ---- customers ---------------------------------------------------------------------
# 32x64 sprites, cut off at the counter. Colors are palette slots the game fills per customer:
# 1 outline, 2 skin, 3 skin shade, 4 hair, 5 hair shade, 6 shirt, 7 shirt shade,
# 8 eyes, 9 blush, 10 accessory, 11 accessory shade, 12 white.
OUT_, SKIN, SKIN_S, HAIR, HAIR_S, SHIRT, SHIRT_S, EYE, BLUSH, ACC, ACC_S, WHITE = range(1, 13)


def customer(style):
    im = Image.new("P", (32, 64), 0)
    p = im.load()
    dd = ImageDraw.Draw(im)
    # Long hair falls behind the shoulders.
    if style == 1:
        dd.rectangle((5, 12, 26, 44), fill=HAIR)
    dd.ellipse((4, 32, 27, 46), fill=SHIRT)
    dd.polygon([(5, 38), (26, 38), (29, 63), (2, 63)], fill=SHIRT)
    for y in range(32, 64):
        for x in range(32):
            if p[x, y] == SHIRT and (x >= 22 or x <= 6):
                p[x, y] = SHIRT_S
    for y in range(44, 64):            # arms
        p[8, y] = SHIRT_S
        p[23, y] = SHIRT_S
    dd.rectangle((13, 26, 18, 34), fill=SKIN_S)
    dd.polygon([(12, 33), (19, 33), (15, 38), (16, 38)], fill=SKIN)
    if style == 3:                     # scarf
        dd.rectangle((10, 31, 21, 34), fill=ACC)
        dd.rectangle((17, 34, 19, 41), fill=ACC_S)
    dd.ellipse((7, 8, 24, 28), fill=SKIN)
    dd.rectangle((6, 17, 7, 20), fill=SKIN_S)
    dd.rectangle((24, 17, 25, 20), fill=SKIN_S)
    for y in range(8, 29):             # shade under the chin
        for x in range(32):
            if p[x, y] == SKIN and y >= 26:
                p[x, y] = SKIN_S
    # Hair (and hat).
    for y in range(4, 24):
        for x in range(5, 27):
            inside = ((x - 15.5) / 10.5) ** 2 + ((y - 15) / 10.5) ** 2 <= 1
            if not inside:
                continue
            fringe = 13 + (1 if (x + style) % 4 == 0 else 0) + (1 if x in (11, 20) else 0)
            side = (x <= 8 or x >= 23) and y < (24 if style == 1 else 19)
            if y < fringe or side:
                p[x, y] = HAIR
    if style == 2:                     # bun
        dd.ellipse((11, 1, 20, 9), fill=HAIR)
    if style == 3:                     # knit cap
        dd.ellipse((6, 3, 25, 18), fill=ACC)
        for y in range(3, 19):
            for x in range(32):
                if p[x, y] == ACC and y > 11:
                    p[x, y] = HAIR if y > 12 and (x <= 8 or x >= 23) else p[x, y]
        dd.rectangle((6, 11, 25, 13), fill=ACC_S)
        dd.ellipse((13, 0, 18, 5), fill=ACC_S)
    # Hair shade: the bottom pixel of each hair column run.
    for y in range(0, 63):
        for x in range(32):
            if p[x, y] == HAIR and p[x, y + 1] not in (HAIR, HAIR_S):
                p[x, y] = HAIR_S
    # Face.
    for x, y in ((12, 18), (12, 19), (19, 18), (19, 19)):
        p[x, y] = EYE
    for x in (10, 11, 20, 21):
        p[x, 21] = BLUSH
    # Outline: clear pixels next to drawn ones.
    edge = []
    for y in range(64):
        for x in range(32):
            if p[x, y] == 0:
                for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                    if 0 <= nx < 32 and 0 <= ny < 64 and p[nx, ny] not in (0, OUT_):
                        edge.append((x, y))
                        break
    for x, y in edge:
        p[x, y] = OUT_
    return im


def tiles_4bpp(im, tw, th):
    """1D sprite tile order: rows of tiles, left to right. Returns u32 words."""
    p = im.load()
    words = []
    for ty in range(th):
        for tx in range(tw):
            for y in range(8):
                w = 0
                for x in range(8):
                    w |= (p[tx * 8 + x, ty * 8 + y] & 15) << (4 * x)
                words.append(w)
    return words


cust_words = []
sheet = Image.new("P", (32 * 4, 64), 0)
for s in range(4):
    im = customer(s)
    sheet.paste(im, (s * 32, 0))
    cust_words += tiles_4bpp(im, 4, 8)
demo = [(0, 0, 0), (40, 24, 30), (248, 200, 160), (220, 160, 120), (120, 70, 40), (90, 50, 30),
        (90, 140, 200), (60, 100, 160), (40, 30, 40), (240, 140, 140), (200, 60, 60), (150, 40, 40), (255, 255, 255)]
flat = []
for c in demo:
    flat += list(c)
sheet.putpalette(flat + [0] * (768 - len(flat)))
sheet.convert("RGB").resize((32 * 4 * 4, 64 * 4), Image.NEAREST).save(os.path.join(PREVIEW, "customers_preview.png"))

# Bubbles (16x16, own palette): 1 outline, 2 white, 3 red, 4 gold, 5 gray.
BUBBLE = [
    "..111111111111..",
    ".12222222222221.",
    "1222222222222221",
    "1222222222222221",
    "1222222222222221",
    "1222222222222221",
    "1222222222222221",
    "1222222222222221",
    "1222222222222221",
    "1222222222222221",
    ".12222222222221.",
    "..1122111111111.",
    "...1221.........",
    "...121..........",
    "...11...........",
    "................",
]
ICONS = {
    "heart": [".33.33.", "3333333", "3333333", ".33333.", "..333..", "...3..."],
    "dots": [".......", ".......", "5..5..5", "5..5..5", ".......", "......."],
    "bang": ["..44...", "..44...", "..44...", "..44...", ".......", "..44..."],
    "ask": [".1111..", "1....1.", "....1..", "...1...", ".......", "...1..."],
    "spark": ["...4...", "...4...", "4444444", "...4...", "...4...", "......."],
    "coin": ["..444..", ".44444.", "4444444", "4444444", ".44444.", "..444.."],
}
bubble_words = []
bubble_names = []
for name, icon in ICONS.items():
    rows = [list(r) for r in BUBBLE]
    for y, r in enumerate(icon):
        for x, ch in enumerate(r):
            if ch != ".":
                rows[3 + y][4 + x] = ch
    im = Image.new("P", (16, 16), 0)
    p = im.load()
    for y in range(16):
        for x in range(16):
            ch = rows[y][x]
            p[x, y] = int(ch) if ch.isdigit() else 0
    bubble_words += tiles_4bpp(im, 2, 2)
    bubble_names.append(name)

# ---- 5x7 font (same glyphs as Rootflow, plus a few) -------------------------------
FONT = {
    "+": "00000 00100 00100 11111 00100 00100 00000",
    "/": "00001 00010 00010 00100 01000 01000 10000",
    "-": "00000 00000 00000 11111 00000 00000 00000",
    ",": "00000 00000 00000 00000 00110 00100 01000",
    "A": "01110 10001 10001 11111 10001 10001 10001",
    "B": "11110 10001 10001 11110 10001 10001 11110",
    "C": "01110 10001 10000 10000 10000 10001 01110",
    "D": "11110 10001 10001 10001 10001 10001 11110",
    "E": "11111 10000 10000 11110 10000 10000 11111",
    "F": "11111 10000 10000 11110 10000 10000 10000",
    "G": "01110 10001 10000 10111 10001 10001 01111",
    "H": "10001 10001 10001 11111 10001 10001 10001",
    "I": "01110 00100 00100 00100 00100 00100 01110",
    "J": "00111 00010 00010 00010 00010 10010 01100",
    "K": "10001 10010 10100 11000 10100 10010 10001",
    "L": "10000 10000 10000 10000 10000 10000 11111",
    "M": "10001 11011 10101 10101 10001 10001 10001",
    "N": "10001 10001 11001 10101 10011 10001 10001",
    "O": "01110 10001 10001 10001 10001 10001 01110",
    "P": "11110 10001 10001 11110 10000 10000 10000",
    "Q": "01110 10001 10001 10001 10101 10010 01101",
    "R": "11110 10001 10001 11110 10100 10010 10001",
    "S": "01111 10000 10000 01110 00001 00001 11110",
    "T": "11111 00100 00100 00100 00100 00100 00100",
    "U": "10001 10001 10001 10001 10001 10001 01110",
    "V": "10001 10001 10001 10001 10001 01010 00100",
    "W": "10001 10001 10001 10101 10101 10101 01010",
    "X": "10001 10001 01010 00100 01010 10001 10001",
    "Y": "10001 10001 01010 00100 00100 00100 00100",
    "Z": "11111 00001 00010 00100 01000 10000 11111",
    "0": "01110 10001 10011 10101 11001 10001 01110",
    "1": "00100 01100 00100 00100 00100 00100 01110",
    "2": "01110 10001 00001 00010 00100 01000 11111",
    "3": "11111 00010 00100 00010 00001 10001 01110",
    "4": "00010 00110 01010 10010 11111 00010 00010",
    "5": "11111 10000 11110 00001 00001 10001 01110",
    "6": "00110 01000 10000 11110 10001 10001 01110",
    "7": "11111 00001 00010 00100 01000 01000 01000",
    "8": "01110 10001 10001 01110 10001 10001 01110",
    "9": "01110 10001 10001 01111 00001 00010 01100",
    "%": "11001 11010 00010 00100 01000 01011 10011",
    ":": "00000 01100 01100 00000 01100 01100 00000",
    "!": "00100 00100 00100 00100 00100 00000 00100",
    ".": "00000 00000 00000 00000 00000 01100 01100",
    "?": "01110 10001 00001 00010 00100 00000 00100",
    "'": "00100 00100 01000 00000 00000 00000 00000",
    "(": "00010 00100 01000 01000 01000 00100 00010",
    ")": "01000 00100 00010 00010 00010 00100 01000",
    ">": "01000 01100 01110 01111 01110 01100 01000",
    "*": "00000 01010 11111 11111 01110 00100 00000",
    "#": "00100 01110 11111 01110 01010 10001 00000",
    " ": "00000 00000 00000 00000 00000 00000 00000",
}
FONT_CHARS = "".join(FONT.keys())

# ---- write art.c / art.h ----------------------------------------------------------


def rgb15(c):
    return (c[0] >> 3) | ((c[1] >> 3) << 5) | ((c[2] >> 3) << 10)


src = ['// Generated by tools/make_art.py. Do not edit.', '#include "fw.h"', '#include "art.h"', '']
hdr = ['// Generated by tools/make_art.py. Do not edit.', '#pragma once', '']


def arr(ctype, name, vals, per=16, align=False):
    hdr.append(f"extern const {ctype} {name}[{len(vals)}];")
    src.append(("__attribute__((aligned(4))) " if align else "") + f"const {ctype} {name}[{len(vals)}] = {{")
    for i in range(0, len(vals), per):
        src.append("    " + ",".join(str(v) for v in vals[i:i + per]) + ",")
    src.append("};")


hdr.append(f"#define SHOP_PAL_N {len(pal)}   // BG palette 16..")
arr("u16", "shop_pal", [rgb15(c) for c in pal])
arr("u8", "shop_bg", list(bg.get_flattened_data() if hasattr(bg, "get_flattened_data") else bg.getdata()), 48, align=True)
arr("u32", "cust_tiles", [f"0x{w:08X}" for w in cust_words], 8)
arr("u32", "bubble_tiles", [f"0x{w:08X}" for w in bubble_words], 8)
hdr.append("enum { " + ", ".join(f"BUB_{n.upper()}" for n in bubble_names) + ", BUB_COUNT };")
hdr.append(f"#define FONT_N {len(FONT_CHARS)}")
hdr.append("extern const char FONT_CHARS[FONT_N + 1];")
src.append(f'const char FONT_CHARS[FONT_N + 1] = "{FONT_CHARS}";')
rows = []
for ch in FONT_CHARS:
    for r in FONT[ch].split():
        rows.append(int(r, 2))
arr("u8", "font_rows", rows, 28)
open(OUT_C, "w").write("\n".join(src) + "\n")
open(OUT_H, "w").write("\n".join(hdr) + "\n")
print(f"art: shop palette {len(pal)} colors, {len(cust_words) // 8} customer tiles, {len(FONT_CHARS)} glyphs")
