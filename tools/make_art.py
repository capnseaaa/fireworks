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

# ---- town map (day) -----------------------------------------------------------------
# Drawn like a GBA RPG overworld: dark outlines, light from the top left, ground shadows.
# Its own palette, loaded during the day. 16..27 are the hues again (color swatches).
import math

MC = {}
mpal = list(HUES)


def mcol(name, rgb):
    MC[name] = 16 + len(mpal)
    mpal.append(rgb)


for name, rgb in [
    ("ol", (36, 30, 40)),                                   # outline
    ("g0", (78, 140, 64)), ("g1", (98, 164, 74)), ("g2", (120, 186, 86)), ("g3", (150, 206, 104)), ("gsh", (66, 118, 60)),
    ("fl_y", (248, 224, 96)), ("fl_p", (240, 140, 180)), ("fl_w", (248, 248, 240)), ("fl_r", (230, 80, 70)),
    ("p0", (214, 184, 128)), ("p1", (190, 158, 104)), ("p2", (160, 128, 84)), ("p3", (232, 208, 160)),
    ("w0", (52, 96, 184)), ("w1", (70, 128, 214)), ("w2", (110, 168, 236)), ("w3", (190, 224, 248)), ("wd", (40, 74, 150)),
    ("s0", (236, 216, 156)), ("s1", (214, 190, 128)), ("s2", (248, 236, 196)),
    ("l0", (28, 82, 48)), ("l1", (44, 112, 58)), ("l2", (70, 146, 70)), ("l3", (112, 180, 84)),
    ("t0", (86, 56, 36)), ("t1", (122, 82, 50)),
    ("r0", (92, 88, 104)), ("r1", (130, 126, 140)), ("r2", (168, 164, 176)), ("r3", (204, 200, 210)), ("snow", (244, 246, 252)),
    ("cave", (24, 18, 26)), ("wd0", (110, 74, 44)), ("wd1", (150, 104, 62)), ("wd2", (186, 136, 84)),
    ("wl0", (232, 220, 196)), ("wl1", (200, 186, 160)),
    ("rf0", (150, 46, 44)), ("rf1", (196, 70, 60)), ("rf2", (226, 104, 84)),
    ("bl0", (48, 80, 150)), ("bl1", (74, 112, 190)), ("bl2", (110, 150, 220)),
    ("gr0", (52, 110, 70)), ("gr1", (80, 146, 90)), ("gr2", (112, 176, 116)),
    ("win", (120, 180, 230)), ("win2", (200, 232, 250)), ("tn_y", (236, 196, 72)), ("tn_y2", (196, 150, 48)),
    ("white", (246, 244, 236)), ("white2", (210, 206, 200)), ("iron", (70, 70, 80)), ("lamp", (255, 226, 140)),
    ("bar", (24, 18, 30)), ("frame", (52, 32, 24)),
]:
    mcol(name, rgb)
assert 16 + len(mpal) <= 256
M = MC

mp = Image.new("P", (W, H), M["g1"])
mpx = mp.load()
md = ImageDraw.Draw(mp)
mrng = random.Random(11)


def P(x, y, c):
    if 0 <= x < W and 12 <= y < H:
        mpx[int(x), int(y)] = M[c] if isinstance(c, str) else c


def G(x, y):
    return mpx[x, y] if 0 <= x < W and 0 <= y < H else None


def noise(x, y, s=9.0):
    return (math.sin(x / s * 1.7 + math.sin(y / s)) + math.sin(y / s * 1.3 + math.cos(x / s * 0.7))) / 2


# --- ground: grass with lighter and darker patches, tufts, and flower clusters
for y in range(12, H):
    for x in range(W):
        n = noise(x, y, 11) + (mrng.random() - 0.5) * 0.35
        c = "g0" if n < -0.55 else "g1" if n < 0.25 else "g2" if n < 0.8 else "g3"
        P(x, y, c)
for _ in range(260):                   # grass tufts: a small dark "v"
    x, y = mrng.randrange(W), mrng.randrange(14, H)
    P(x, y, "gsh"); P(x + 2, y, "gsh"); P(x + 1, y + 1, "gsh")
    P(x, y - 1, "g3") if mrng.random() < 0.5 else None
for _ in range(22):                    # flower clusters
    cx, cy = mrng.randrange(4, W - 4), mrng.randrange(16, H - 4)
    col = mrng.choice(["fl_y", "fl_p", "fl_w", "fl_r"])
    for _ in range(5):
        x, y = cx + mrng.randint(-3, 3), cy + mrng.randint(-2, 2)
        P(x, y + 1, "gsh"); P(x, y, col)

# --- sea and beach (bottom right)
def shore(x, y):
    return x + y * 1.45 + 5 * math.sin(y / 6.0) + 3 * math.sin(x / 9.0)


for y in range(12, H):
    for x in range(W):
        v = shore(x, y)
        if v > 360:
            d = v - 360
            c = "w2" if d < 6 else "w1" if d < 30 else "w0"
            if 0 < d < 2.2: c = "w3"                       # foam line
            P(x, y, c)
        elif v > 334:
            c = "s0"
            if mrng.random() < 0.06: c = "s1"
            if mrng.random() < 0.03: c = "s2"
            if v > 356: c = "s1"                           # wet sand
            P(x, y, c)
for _ in range(40):                    # wave crests
    x, y = mrng.randrange(W), mrng.randrange(70, H)
    if shore(x, y) > 372:
        for k in range(5):
            if shore(x + k, y) > 368: P(x + k, y - (1 if k in (1, 2, 3) else 0), "w3" if k in (1, 2, 3) else "w2")
for _ in range(10):                    # little shells and starfish on the sand
    x, y = mrng.randrange(150, W), mrng.randrange(60, H)
    if 336 < shore(x, y) < 352: P(x, y, mrng.choice(["fl_p", "white", "fl_r"]))

# --- river from the mountains to the sea
def river_x(y):
    return 150 + 12 * math.sin((y - 12) / 15.0) + (y - 12) * 0.38


for y in range(12, H):
    cx = river_x(y)
    for x in range(int(cx) - 6, int(cx) + 7):
        if shore(x, y) > 352: continue
        d = abs(x - cx)
        if d <= 3.6: P(x, y, "w1" if (x + y * 3) % 11 else "w2")
        elif d <= 4.6: P(x, y, "w0")
        elif d <= 5.6: P(x, y, "wd" if x < cx else "p2")    # banks
for y in range(16, 110, 5):            # ripples
    x = int(river_x(y))
    P(x - 1, y, "w3"); P(x, y, "w2")

AREAS = {"HOME": (124, 70), "MARKET": (48, 92), "FOREST": (30, 40), "QUARRY": (96, 30),
         "MINE": (204, 34), "BEACH": (206, 102)}


# --- paths: dirt with dark edges and pebbles, plank bridges over the river
def path_line(x0, y0, x1, y1):
    pts = []
    n = max(abs(x1 - x0), abs(y1 - y0)) + 1
    for i in range(n):
        pts.append((round(x0 + (x1 - x0) * i / max(1, n - 1)), round(y0 + (y1 - y0) * i / max(1, n - 1))))
    return pts


path_px = set()
for name, (x, y) in AREAS.items():
    if name == "HOME": continue
    hx, hy = AREAS["HOME"]
    my = (hy + y) // 2 if name != "BEACH" else hy + 10
    for seg in (path_line(hx, hy, hx, my), path_line(hx, my, x, my), path_line(x, my, x, y)):
        for (px_, py_) in seg:
            for dx in range(-2, 3):
                for dy in range(-2, 3):
                    path_px.add((px_ + dx, py_ + dy))
for (x, y) in path_px:
    if not (0 <= x < W and 12 <= y < H): continue
    edge = any((x + dx, y + dy) not in path_px for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
    on_water = abs(x - river_x(y)) <= 5.6 and shore(x, y) <= 352
    if on_water:
        P(x, y, "wd1" if (x + y) % 3 else "wd0")         # bridge planks
        if edge: P(x, y, "ol")
    else:
        P(x, y, "p1" if edge else ("p0" if mrng.random() > 0.08 else "p3"))
for _ in range(60):                    # pebbles
    x, y = mrng.randrange(W), mrng.randrange(12, H)
    if (x, y) in path_px and G(x, y) == M["p0"]: P(x, y, "p2")


# --- shading helpers
def ellipse_shadow(cx, cy, rx, ry):
    for y in range(cy - ry, cy + ry + 1):
        for x in range(cx - rx, cx + rx + 1):
            if ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1 and G(x, y) in (M["g0"], M["g1"], M["g2"], M["g3"], M["gsh"]):
                P(x, y, "gsh")


def outline_region(pixels, color="ol"):
    s = set(pixels)
    for (x, y) in s:
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            if (x + dx, y + dy) not in s:
                P(x, y, color)
                break


def round_tree(cx, cy, r=6):
    ellipse_shadow(cx + 3, cy + r + 2, r - 1, 2)
    for y in range(cy + r - 2, cy + r + 4):              # trunk
        P(cx - 1, y, "t1"); P(cx, y, "t0"); P(cx - 2, y, "ol"); P(cx + 1, y, "ol")
    P(cx - 1, cy + r + 4, "ol"); P(cx, cy + r + 4, "ol")
    pix = []
    for y in range(cy - r, cy + r + 1):
        for x in range(cx - r, cx + r + 1):
            dx, dy = (x - cx) / r, (y - cy) / r
            bump = 0.12 * math.sin(math.atan2(dy, dx) * 5 + cx)
            if dx * dx + dy * dy <= (1 + bump) ** 2:
                pix.append((x, y))
                l = -(dx * 0.7 + dy * 0.7) + (mrng.random() - 0.5) * 0.5
                P(x, y, "l3" if l > 0.75 else "l2" if l > 0.1 else "l1" if l > -0.6 else "l0")
    outline_region(pix)


def pine(cx, cy, h=14):
    ellipse_shadow(cx + 3, cy + h // 2 + 2, 4, 2)
    for y in range(cy + h // 2 - 1, cy + h // 2 + 3):
        P(cx, y, "t0"); P(cx - 1, y, "ol"); P(cx + 1, y, "ol")
    pix = []
    for y in range(cy - h // 2, cy + h // 2):
        t = (y - (cy - h // 2)) / h
        half = int(1 + t * 6) - (1 if (y % 4 == 0 and t > 0.2) else 0)
        for x in range(cx - half, cx + half + 1):
            pix.append((x, y))
            P(x, y, "l2" if x < cx - half / 3 else "l1" if x < cx + half / 3 else "l0")
    outline_region(pix)


def rock(cx, cy, r=3):
    pix = [(x, y) for y in range(cy - r, cy + r) for x in range(cx - r - 1, cx + r + 2)
           if ((x - cx) / (r + 1.5)) ** 2 + ((y - cy) / r) ** 2 <= 1]
    for (x, y) in pix:
        P(x, y, "r3" if x < cx and y < cy - 1 else "r2" if y < cy else "r1")
    outline_region(pix)


def bush(cx, cy):
    pix = [(x, y) for y in range(cy - 3, cy + 3) for x in range(cx - 4, cx + 5) if ((x - cx) / 4.5) ** 2 + ((y - cy) / 3.2) ** 2 <= 1]
    for (x, y) in pix:
        P(x, y, "l3" if y < cy - 1 and x < cx else "l2" if y < cy + 1 else "l1")
    outline_region(pix)


def house(cx, base, w, h, roof="rf", chimney=True, sign=False):
    x0, x1 = cx - w // 2, cx + w // 2
    for y in range(base - 1, base + 3):                  # ground shadow
        for x in range(x0 + 2, x1 + 4):
            if G(x, y) in (M["g0"], M["g1"], M["g2"], M["g3"], M["gsh"], M["p0"], M["p3"]): P(x, y, "gsh")
    for y in range(base - h, base):                       # walls
        for x in range(x0, x1 + 1):
            P(x, y, "wl1" if x > x1 - 2 else "wl0")
    for x in range(x0, x1 + 1):
        P(x, base - 1, "wl1")
    pix = [(x, y) for y in range(base - h, base) for x in range(x0, x1 + 1)]
    outline_region(pix)
    rh = h - 2                                            # roof: rows of shingles
    rpix = []
    for i, y in enumerate(range(base - h - rh, base - h + 2)):
        t = i / rh
        half = int(3 + t * (w // 2 + 2))
        for x in range(cx - half, cx + half + 1):
            rpix.append((x, y))
            c = roof + ("2" if (y % 3 == 0) else "1")
            if x > cx + half - 3: c = roof + "0"
            P(x, y, c)
    outline_region(rpix)
    if chimney:
        for y in range(base - h - rh - 2, base - h - rh + 4):
            P(cx + w // 4, y, "r2"); P(cx + w // 4 + 1, y, "r1"); P(cx + w // 4 - 1, y, "ol"); P(cx + w // 4 + 2, y, "ol")
        P(cx + w // 4, base - h - rh - 3, "ol"); P(cx + w // 4 + 1, base - h - rh - 3, "ol")
    # door and windows
    for y in range(base - 7, base - 1):
        for x in range(cx - 2, cx + 3):
            P(x, y, "wd1" if x < cx + 2 else "wd0")
    for x in range(cx - 3, cx + 4): P(x, base - 8, "ol")
    for y in range(base - 8, base - 1): P(cx - 3, y, "ol"); P(cx + 3, y, "ol")
    P(cx + 1, base - 4, "fl_y")
    for wx in (x0 + 3, x1 - 6):
        if wx + 3 > cx - 3 and wx < cx + 4: continue
        for y in range(base - h + 3, base - h + 7):
            for x in range(wx, wx + 4):
                P(x, y, "win2" if (x - wx) + (y - (base - h + 3)) < 2 else "win")
        for x in range(wx - 1, wx + 5): P(x, base - h + 2, "ol"); P(x, base - h + 7, "ol")
        for y in range(base - h + 2, base - h + 8): P(wx - 1, y, "ol"); P(wx + 4, y, "ol")
    if sign:                                              # a little rocket sign on the roof
        sx, sy = cx - w // 2 - 2, base - h - 6
        for y in range(sy, sy + 7): P(sx, y, "rf1"); P(sx + 1, y, "rf0"); P(sx - 1, y, "ol"); P(sx + 2, y, "ol")
        P(sx, sy - 1, "white"); P(sx + 1, sy - 1, "white"); P(sx, sy - 2, "ol"); P(sx + 1, sy - 2, "ol")
        P(sx - 1, sy + 7, "fl_y"); P(sx + 2, sy + 7, "fl_y")


def fence(x0, x1, y):
    for x in range(x0, x1 + 1):
        P(x, y, "wd2"); P(x, y + 2, "wd1"); P(x, y - 1, "ol") if x % 4 else None
        if x % 4 == 0:
            for yy in range(y - 2, y + 4): P(x, yy, "wd0")
            P(x, y - 3, "ol")


def lamp(x, y):
    for yy in range(y - 8, y + 1): P(x, yy, "iron")
    P(x - 1, y - 10, "ol"); P(x, y - 10, "ol"); P(x + 1, y - 10, "ol")
    P(x - 1, y - 9, "lamp"); P(x, y - 9, "lamp"); P(x + 1, y - 9, "lamp")
    P(x, y + 1, "gsh"); P(x + 1, y + 1, "gsh")


# --- forest (top left): pines in the back, round trees in front, bushes at the edge
spots = []
for _ in range(60):
    x, y = mrng.randint(-4, 66), mrng.randint(14, 70)
    if (x - 28) ** 2 / 1600 + (y - 40) ** 2 / 900 > 1.1: continue
    if any(abs(x - a) < 8 and abs(y - b) < 7 for a, b in spots): continue
    spots.append((x, y))
for (x, y) in sorted(spots, key=lambda p: p[1]):
    if (x, y) == spots[0]: pass
    if y < 32 and mrng.random() < 0.6: pine(x, y)
    else: round_tree(x, y, mrng.choice((5, 6, 6, 7)))
for (x, y) in ((70, 58), (64, 66), (74, 46)):
    bush(x, y)

# --- quarry (top middle): stepped pit, rocks, a little crane
qx, qy = AREAS["QUARRY"]
for k, (dx, dy, c) in enumerate(((20, 13, "r3"), (16, 10, "r2"), (12, 7, "r1"), (8, 4, "r0"))):
    pix = [(x, y) for y in range(qy - dy, qy + dy + 1) for x in range(qx - dx, qx + dx + 1)
           if abs(x - qx) / dx + abs(y - qy) / dy * 0.35 <= 1.2]
    for (x, y) in pix: P(x, y, c)
    outline_region(pix)
for y in range(qy - 9, qy + 8):         # ladder
    P(qx - 2, y, "wd0"); P(qx + 1, y, "wd0")
    if y % 3 == 0: P(qx - 1, y, "wd2"); P(qx, y, "wd2")
for (x, y) in ((qx - 22, qy + 10), (qx + 21, qy - 6), (qx + 16, qy + 12)):
    rock(x, y)
for y in range(qy - 16, qy + 2):        # crane
    P(qx + 12, y, "wd1"); P(qx + 13, y, "wd0")
for x in range(qx + 4, qx + 14): P(x, qy - 16, "wd1")
for y in range(qy - 15, qy - 8): P(qx + 5, y, "iron")
P(qx + 4, qy - 8, "r1"); P(qx + 5, qy - 8, "r1"); P(qx + 6, qy - 8, "r1")

# --- mountains and the mine (top right)
for (cx, base, h) in ((176, 52, 24), (234, 50, 30), (206, 52, 40), (190, 48, 30)):
    pix = []
    for y in range(base - h, base + 1):
        half = int((y - (base - h)) * 1.2) + 1
        jag = int(2 * math.sin(y * 1.3 + cx))
        for x in range(cx - half + jag, cx + half + 1):
            pix.append((x, y))
            sh = (x - cx) / max(1, half)
            c = "r3" if sh < -0.45 else "r2" if sh < 0.05 else "r1" if sh < 0.55 else "r0"
            if y < base - h + h * 0.28 and sh < 0.6: c = "snow" if sh < 0.1 else "r3"
            P(x, y, c)
    for i in range(3):                                   # ridges
        x, y = cx - 4 + i * 4, base - h // 2 + i * 3
        for k in range(5): P(x + k // 2, y + k, "r0")
    outline_region(pix)
mx, my = AREAS["MINE"]
for y in range(my - 12, my + 6):        # timber frame and the dark entrance
    for x in range(mx - 9, mx + 10):
        P(x, y, "wd1" if (x in (mx - 9, mx - 8, mx + 8, mx + 9) or y in (my - 12, my - 11)) else "cave")
for x in range(mx - 9, mx + 10): P(x, my - 13, "ol")
for y in range(my - 13, my + 6): P(mx - 10, y, "ol"); P(mx + 10, y, "ol")
for x in range(mx - 9, mx + 10): P(x, my - 12, "wd2")
for y in range(my + 6, my + 22):        # rails down the slope
    P(mx - 4, y, "iron"); P(mx + 3, y, "iron")
    if y % 3 == 0:
        for x in range(mx - 5, mx + 5): P(x, y, "wd0")
for y in range(my + 12, my + 18):       # a mine cart
    for x in range(mx - 6, mx + 6): P(x, y, "iron" if y > my + 12 else "ol")
for x in range(mx - 5, mx + 5): P(x, my + 11, "r1" if x % 2 else "r0")
P(mx - 4, my + 18, "ol"); P(mx + 3, my + 18, "ol")
for (x, y) in ((mx - 20, my + 16), (mx + 22, my + 14)):
    rock(x, y)

# --- market (left): striped tents, crates, barrels
def tent(cx, base, c):
    pix = []
    for y in range(base - 12, base - 4):
        half = (y - (base - 12)) + 2
        for x in range(cx - half, cx + half + 1):
            pix.append((x, y))
            P(x, y, (c + "1") if ((x - cx) // 3) % 2 == 0 else "white")
            if x > cx + half - 2: P(x, y, c + "0" if ((x - cx) // 3) % 2 == 0 else "white2")
    for y in range(base - 4, base + 1):
        for x in range(cx - 8, cx + 9):
            pix.append((x, y))
            P(x, y, "wd1" if y == base - 4 else ("wl1" if abs(x - cx) < 3 else "wd2"))
    outline_region(pix)
    for y in range(base + 1, base + 3):
        for x in range(cx - 6, cx + 11):
            if G(x, y) not in (M["ol"],): P(x, y, "gsh") if G(x, y) in (M["g0"], M["g1"], M["g2"], M["g3"]) else None


def crate(x, y):
    pix = [(xx, yy) for yy in range(y, y + 6) for xx in range(x, x + 7)]
    for (xx, yy) in pix: P(xx, yy, "wd2" if yy == y else "wd1" if (xx - x) % 3 else "wd0")
    outline_region(pix)


def barrel(x, y):
    pix = [(xx, yy) for yy in range(y, y + 7) for xx in range(x, x + 6)]
    for (xx, yy) in pix: P(xx, yy, "iron" if yy in (y + 1, y + 5) else ("wd2" if xx < x + 2 else "wd1"))
    outline_region(pix)


kx, ky = AREAS["MARKET"]
tent(kx - 18, ky - 2, "rf")
tent(kx + 2, ky + 2, "bl")
tent(kx + 22, ky - 1, "gr")
crate(kx - 30, ky + 6); crate(kx - 24, ky + 9); barrel(kx + 12, ky + 8); barrel(kx + 32, ky + 4)

# --- town around home: the shop, neighbors, fences, lamps, a well
hx, hy = AREAS["HOME"]
house(hx - 30, hy - 8, 18, 10, "bl", chimney=True)
house(hx + 30, hy + 12, 18, 10, "gr", chimney=False)
house(hx - 26, hy + 20, 16, 9, "rf", chimney=True)
house(hx, hy + 8, 24, 13, "rf", chimney=True, sign=True)
fence(hx - 14, hx - 4, hy + 12)
fence(hx + 4, hx + 14, hy + 12)
lamp(hx - 8, hy - 2); lamp(hx + 18, hy + 2); lamp(hx - 6, hy + 30)
wx, wy = hx + 22, hy - 12             # well
pix = [(x, y) for y in range(wy, wy + 6) for x in range(wx - 4, wx + 5)]
for (x, y) in pix: P(x, y, "r2" if y < wy + 2 else "r1")
outline_region(pix)
for x in range(wx - 3, wx + 4): P(x, wy + 1, "w0")
for y in range(wy - 6, wy): P(wx - 4, y, "wd0"); P(wx + 4, y, "wd0")
for x in range(wx - 5, wx + 6): P(x, wy - 7, "rf1"); P(x, wy - 8, "ol")
for (x, y) in ((hx - 44, hy + 2), (hx + 46, hy - 6), (hx - 10, hy - 26), (hx + 8, hy - 30)):
    bush(x, y)
round_tree(hx + 46, hy + 30, 6)
round_tree(hx - 48, hy + 34, 5)

# --- beach: a dock with a little boat
bx, by = AREAS["BEACH"]
pix = [(x, y) for y in range(by + 2, by + 6) for x in range(bx + 4, bx + 32)]
for (x, y) in pix: P(x, y, "wd2" if y == by + 2 else "wd1" if x % 4 else "wd0")
outline_region(pix)
for x in (bx + 6, bx + 16, bx + 26):
    for y in range(by + 6, by + 10): P(x, y, "t0"); P(x + 1, y, "ol")
boat = [(x, y) for y in range(by + 12, by + 16) for x in range(bx + 18 + (y - by - 12), bx + 34 - (y - by - 12))]
for (x, y) in boat: P(x, y, "rf1" if y == by + 12 else "wd1")
outline_region(boat)
for y in range(by - 2, by + 12): P(bx + 25, y, "t0")
sail = [(x, y) for y in range(by - 1, by + 10) for x in range(bx + 26, bx + 26 + (y - by + 1) // 2 + 1)]
for (x, y) in sail: P(x, y, "white")
outline_region(sail)
for (x, y) in ((bx - 14, by - 10), (bx - 20, by + 4)):
    rock(x, y, 2)

# --- status bar
md.rectangle((0, 0, W - 1, 10), fill=M["bar"])
md.line((0, 11, W - 1, 11), fill=M["frame"])

flat = []
for c in [(0, 0, 0)] * 16 + mpal:
    flat += list(c)
mp.putpalette(flat + [0] * (768 - len(flat)))
mp.convert("RGB").save(os.path.join(PREVIEW, "map_preview.png"))

# Map sprites (16x16, own palette bank): 1 outline, 2 gold, 3 white, 4 skin, 5 shirt.
ARROW = [
    "................", "....11111111....", "....12222221....", "....12222221....", "....12222221....",
    "..111222222111..", "..122222222221..", "...1222222221...", "....12222221....", ".....122221.....",
    "......1221......", ".......11.......", "................", "................", "................", "................",
]
PLAYER = [
    "................", "......1111......", ".....122221.....", ".....144441.....", ".....143341.....",
    ".....144441.....", "......1441......", ".....155551.....", "....15555551....", "....15555551....",
    "....14555541....", ".....155551.....", ".....155551.....", ".....11..11.....", "................", "................",
]
map_words = []
for g in (ARROW, PLAYER):
    im = Image.new("P", (16, 16), 0)
    p = im.load()
    for y in range(16):
        for x in range(16):
            ch = g[y][x]
            p[x, y] = int(ch) if ch.isdigit() else 0
    map_words += tiles_4bpp(im, 2, 2)

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
hdr.append(f"#define MAP_PAL_N {len(mpal)}   // BG palette 16.. (the day map)")
arr("u16", "map_pal", [rgb15(c) for c in mpal])
arr("u8", "map_bg", list(mp.get_flattened_data() if hasattr(mp, "get_flattened_data") else mp.getdata()), 48, align=True)
arr("u32", "map_tiles", [f"0x{w:08X}" for w in map_words], 8)   # arrow, player (16x16 each)
open(OUT_C, "w").write("\n".join(src) + "\n")
open(OUT_H, "w").write("\n".join(hdr) + "\n")
print(f"art: shop palette {len(pal)} colors, {len(cust_words) // 8} customer tiles, {len(FONT_CHARS)} glyphs")
