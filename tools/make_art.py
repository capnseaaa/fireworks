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
    '"': "01010 01010 10100 00000 00000 00000 00000",
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

# ---- forest field (walk around and gather) --------------------------------------------
# Same palette as the town map. Dense trees close in the edges; the clearing is open
# ground for the trees, bushes, and kiln the game places as sprites. The path out is at
# the bottom.
fd = Image.new("P", (W, H), M["g1"])
mp, mpx, md = fd, fd.load(), ImageDraw.Draw(fd)   # the map helpers draw into whatever these are
mrng = random.Random(23)
for y in range(12, H):
    for x in range(W):
        n = noise(x + 300, y, 10) + (mrng.random() - 0.5) * 0.35
        P(x, y, "g0" if n < -0.6 else "g1" if n < 0.2 else "g2" if n < 0.85 else "g3")
for _ in range(220):
    x, y = mrng.randrange(W), mrng.randrange(14, H)
    P(x, y, "gsh"); P(x + 2, y, "gsh"); P(x + 1, y + 1, "gsh")
for _ in range(16):
    cx, cy = mrng.randrange(30, 210), mrng.randrange(40, 150)
    col = mrng.choice(["fl_y", "fl_p", "fl_w"])
    for _ in range(4):
        x, y = cx + mrng.randint(-3, 3), cy + mrng.randint(-2, 2)
        P(x, y + 1, "gsh"); P(x, y, col)
# The path out (bottom middle) and a worn patch in the clearing.
path_px = set()
for y in range(96, H):
    w = 9 + int(2 * math.sin(y / 7.0))
    for x in range(120 - w, 120 + w):
        path_px.add((x, y))
for y in range(82, 100):
    for x in range(88, 152):
        if ((x - 120) / 32) ** 2 + ((y - 92) / 10) ** 2 <= 1:
            path_px.add((x, y))
for (x, y) in path_px:
    edge = any((x + dx, y + dy) not in path_px for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
    P(x, y, "p1" if edge else ("p0" if mrng.random() > 0.08 else "p3"))
# A little pond in the top-left corner.
pond = [(x, y) for y in range(22, 42) for x in range(24, 62) if ((x - 43) / 18) ** 2 + ((y - 32) / 9) ** 2 <= 1]
for (x, y) in pond:
    P(x, y, "w1" if (x + y * 2) % 13 else "w2")
outline_region(pond, "wd")
for (x, y) in ((34, 30), (50, 34), (44, 27)):
    P(x, y, "w3"); P(x + 1, y, "w3")
# The wall of trees around the edges (not walkable), back rows first.
edge_trees = []
for x in range(-6, W + 8, 13):
    edge_trees.append((x + mrng.randint(-3, 3), 16 + mrng.randint(-3, 4), mrng.choice((9, 10, 11))))
for y in range(30, 150, 13):
    edge_trees.append((4 + mrng.randint(-4, 3), y + mrng.randint(-3, 3), mrng.choice((8, 9, 10))))
    edge_trees.append((236 + mrng.randint(-3, 4), y + mrng.randint(-3, 3), mrng.choice((8, 9, 10))))
for x in list(range(-4, 96, 14)) + list(range(150, W + 8, 14)):
    edge_trees.append((x + mrng.randint(-3, 3), 156 + mrng.randint(-2, 3), mrng.choice((9, 10))))
for (x, y, r) in sorted(edge_trees, key=lambda t: t[1]):
    if 62 < x < 70 and y < 30: continue          # keep the pond visible
    round_tree(x, y, r)
for (x, y) in ((70, 140), (176, 146), (200, 40), (30, 70)):
    rock(x, y)
md.rectangle((0, 0, W - 1, 10), fill=M["bar"])
md.line((0, 11, W - 1, 11), fill=M["frame"])
fd.convert("RGB") if False else None
flat = []
for c in [(0, 0, 0)] * 16 + mpal:
    flat += list(c)
fd.putpalette(flat + [0] * (768 - len(flat)))
fd.convert("RGB").save(os.path.join(PREVIEW, "forest_preview.png"))

# ---- field sprites --------------------------------------------------------------------
# Player (OBJ palette bank 0): 1 outline, 2 skin, 3 skin shade, 4 hair, 5 hair shade,
# 6 shirt, 7 shirt shade, 8 pants, 9 shoes, 10 white.
PLAYER_FRAMES = {
    "down0": ["................", ".....111111.....", "....14444441....", "...1444444441...", "...1444444441...",
              "...1454444541...", "...1422222241...", "...1212222121...", "....12222221....", ".....166661.....",
              "....16666671....", "...1266666721...", "...1216666121...", "....18888881....", "....188..881....", "....11....11...."],
    "down1": ["................", ".....111111.....", "....14444441....", "...1444444441...", "...1444444441...",
              "...1454444541...", "...1422222241...", "...1212222121...", "....12222221....", ".....166661.....",
              "....16666671....", "...1266666721...", "...1216666121...", "....18888881....", ".....18888.1....", ".....11..11....."],
    "up0":   ["................", ".....111111.....", "....14444441....", "...1444444441...", "...1444444441...",
              "...1444444441...", "...1454444541...", "...1445555441...", "....14444441....", ".....177771.....",
              "....17777771....", "...1277777721...", "...1217777121...", "....18888881....", "....188..881....", "....11....11...."],
    "up1":   ["................", ".....111111.....", "....14444441....", "...1444444441...", "...1444444441...",
              "...1444444441...", "...1454444541...", "...1445555441...", "....14444441....", ".....177771.....",
              "....17777771....", "...1277777721...", "...1217777121...", "....18888881....", ".....18888.1....", ".....11..11....."],
    "side0": ["................", ".....11111......", "....1444441.....", "...144444441....", "...144444441....",
              "...144444451....", "...122224451....", "...121224441....", "....12222241....", ".....166661.....",
              "....1666671.....", "....1666721.....", "....1666121.....", "....1888881.....", "....188.881.....", "....11...11....."],
    "side1": ["................", ".....11111......", "....1444441.....", "...144444441....", "...144444441....",
              "...144444451....", "...122224451....", "...121224441....", "....12222241....", ".....166661.....",
              "....1666671.....", "....1666721.....", "....1666121.....", "....1888881.....", ".....18881......", ".....1111......."],
}


def grid_image(rows):
    w, h = len(rows[0]), len(rows)
    im = Image.new("P", (w, h), 0)
    p = im.load()
    for y in range(h):
        for x in range(w):
            ch = rows[y][x]
            p[x, y] = int(ch, 16) if ch not in ". " else 0
    return im


player_words = []
for name in ("down0", "down1", "up0", "up1", "side0", "side1"):
    player_words += tiles_4bpp(grid_image(PLAYER_FRAMES[name]), 2, 2)

# Nature (OBJ palette bank 1): 1 outline, 2 leaf dark, 3 leaf, 4 leaf mid, 5 leaf light,
# 6 bark dark, 7 bark, 8 shadow, 9 berry, 10 sparkle, 11 cut wood, 12 stone, 13 stone dark,
# 14 fire, 15 fire light.
def sprite_canvas(w, h):
    im = Image.new("P", (w, h), 0)
    return im, im.load()


def outline_sprite(p, w, h):
    edge = []
    for y in range(h):
        for x in range(w):
            if p[x, y] == 0 and any(0 <= x + dx < w and 0 <= y + dy < h and p[x + dx, y + dy] not in (0, 1, 8)
                                     for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                edge.append((x, y))
    for (x, y) in edge:
        p[x, y] = 1


def canopy(p, cx, cy, r, rng):
    for y in range(int(cy - r - 2), int(cy + r + 2)):
        for x in range(int(cx - r - 2), int(cx + r + 2)):
            dx, dy = (x - cx) / r, (y - cy) / r
            bump = 0.1 * math.sin(math.atan2(dy, dx) * 6 + cx)
            if dx * dx + dy * dy <= (1 + bump) ** 2:
                l = -(dx * 0.7 + dy * 0.7) + (rng.random() - 0.5) * 0.45
                p[x, y] = 5 if l > 0.75 else 4 if l > 0.15 else 3 if l > -0.55 else 2


def tree_sprite(w, h, r, trunk_w, rng, hollow=False):
    im, p = sprite_canvas(w, h)
    cx = w // 2
    for y in range(h - 4, h):                     # shadow
        for x in range(w):
            if ((x - cx) / (w * 0.42)) ** 2 + ((y - (h - 2.5)) / 2) ** 2 <= 1: p[x, y] = 8
    for y in range(int(r * 1.5), h - 3):          # trunk
        for x in range(cx - trunk_w // 2, cx + (trunk_w + 1) // 2):
            p[x, y] = 7 if x < cx + trunk_w // 4 else 6
    if hollow:
        for y in range(h - 14, h - 8):
            for x in range(cx - 3, cx + 3): p[x, y] = 1
    canopy(p, cx, r + 2, r, rng)
    outline_sprite(p, w, h)
    return im


srng = random.Random(5)
oak = tree_sprite(32, 32, 11, 6, srng)
bigtree = tree_sprite(64, 64, 24, 12, srng, hollow=True)
for x in (20, 23, 40, 43):                         # roots on the old tree
    for k in range(3):
        bigtree.load()[x + (k if x < 32 else -k), 60 + k] = 6

stump, sp = sprite_canvas(16, 16)
for y in range(13, 16):
    for x in range(16):
        if ((x - 8) / 7) ** 2 + ((y - 14) / 1.8) ** 2 <= 1: sp[x, y] = 8
for y in range(8, 14):
    for x in range(4, 12): sp[x, y] = 7 if x < 9 else 6
for y in range(6, 10):
    for x in range(4, 12):
        if ((x - 7.5) / 4) ** 2 + ((y - 8) / 2) ** 2 <= 1: sp[x, y] = 11
sp[7, 8] = 6; sp[8, 8] = 6
outline_sprite(sp, 16, 16)


def bush_sprite(berries):
    im, p = sprite_canvas(16, 16)
    for y in range(13, 16):
        for x in range(16):
            if ((x - 8) / 7.5) ** 2 + ((y - 14) / 1.8) ** 2 <= 1: p[x, y] = 8
    rng = random.Random(9)
    canopy(p, 8, 8, 6, rng)
    if berries:
        for (x, y) in ((5, 6), (10, 7), (7, 10), (11, 11), (4, 10)): p[x, y] = 9
        p[9, 4] = 10
    outline_sprite(p, 16, 16)
    return im


bush_full, bush_bare = bush_sprite(True), bush_sprite(False)

kiln, kp = sprite_canvas(32, 32)
for y in range(27, 32):
    for x in range(32):
        if ((x - 16) / 14) ** 2 + ((y - 29.5) / 2.2) ** 2 <= 1: kp[x, y] = 8
for y in range(6, 29):                              # a stone dome
    for x in range(4, 28):
        if ((x - 16) / 12) ** 2 + ((y - 20) / 14) ** 2 <= 1 and y < 29:
            kp[x, y] = 12 if ((x // 4 + y // 3) % 2) else 13
for y in range(19, 28):                             # the fire door
    for x in range(12, 20):
        if ((x - 16) / 4) ** 2 + ((y - 24) / 5) ** 2 <= 1: kp[x, y] = 14 if y < 24 else 15
for y in range(2, 8):                               # chimney
    for x in range(14, 18): kp[x, y] = 13
outline_sprite(kp, 32, 32)

nature_words = []
for im, tw, th in ((oak, 4, 4), (bigtree, 8, 8), (stump, 2, 2), (bush_full, 2, 2), (bush_bare, 2, 2), (kiln, 4, 4)):
    nature_words += tiles_4bpp(im, tw, th)
NATURE_PAL = [(0, 0, 0), (36, 30, 40), (28, 82, 48), (44, 112, 58), (70, 146, 70), (112, 180, 84), (86, 56, 36),
              (122, 82, 50), (66, 118, 60), (220, 50, 60), (250, 230, 120), (214, 176, 120), (150, 146, 156),
              (100, 96, 110), (240, 120, 40), (255, 210, 90)]
PLAYER_PAL = [(0, 0, 0), (36, 30, 40), (248, 200, 160), (220, 160, 120), (110, 66, 40), (80, 46, 30),
              (200, 56, 56), (150, 40, 44), (60, 80, 140), (60, 44, 36), (250, 250, 250)]
prev = Image.new("RGB", (64 * 4, 64))
for i, (im, pal_) in enumerate(((oak, NATURE_PAL), (bigtree, NATURE_PAL), (kiln, NATURE_PAL), (grid_image(PLAYER_FRAMES["down0"]), PLAYER_PAL))):
    f = []
    for c in pal_ + [(0, 0, 0)] * (256 - len(pal_)):
        f += list(c)
    im2 = im.copy()
    im2.putpalette(f)
    prev.paste(im2.convert("RGB"), (i * 64, 0))
prev.resize((64 * 4 * 3, 64 * 3), Image.NEAREST).save(os.path.join(PREVIEW, "field_sprites.png"))


def rgb15_early(c):
    return (c[0] >> 3) | ((c[1] >> 3) << 5) | ((c[2] >> 3) << 10)

# ---- field sprites, Stardew-sized --------------------------------------------------------
# Player 16x32 (OBJ bank 0): 1 outline, 2 skin, 3 skin shade, 4 hair, 5 hair shade,
# 6 shirt, 7 shirt shade, 8 pants, 9 shoes, 10 white.
def pad16(rows, h=32):
    rows = [r.ljust(16, ".")[:16] for r in rows]
    return ["." * 16] * (h - len(rows)) + rows


HEAD_DOWN = [".....111111.....", "....14444441....", "...1444444441...", "...1444444441...", "...1445444541...",
             "...1422222241...", "...1422222241...", "...1212222121...", "...1222222221...", "....12222221....",
             ".....122221....."]
HEAD_UP = [".....111111.....", "....14444441....", "...1444444441...", "...1444444441...", "...1444444441...",
           "...1444444441...", "...1445444541...", "...1445555441...", "...1444444441...", "....14444441....",
           ".....155551....."]
BODY_DOWN = ["....16666661....", "...1666666661...", "..126666666621..", "..126666666621..", "..127666666721..",
             "..121766667121..", "...1177777711...", "....18888881....", "....18888881...."]
BODY_UP = ["....17777771....", "...1777777771...", "..127777777721..", "..127777777721..", "..127777777721..",
           "..121777777121..", "...1177777711...", "....18888881....", "....18888881...."]
LEGS_A = ["....188..881....", "....188..881....", "....188..881....", "....199..991....", "...1999..9991...",
          "...111....111..."]
LEGS_B = ["....188..881....", "....199..881....", "...1999..881....", "...111...991....", ".........9991...",
          ".........111...."]
HEAD_SIDE = [".....11111......", "....1444441.....", "...144444441....", "...144444441....", "...144444451....",
             "...122224451....", "...122224441....", "...121224441....", "...122222241....", "....1222221.....",
             ".....12221......"]
BODY_SIDE = [".....166661.....", "....16666661....", "....16666661....", "....16666621....", "....17666621....",
             "....17666121....", "....1777771.....", "....1888881.....", "....1888881....."]
LEGS_SIDE_A = ["....188.881.....", "....188.881.....", "....188.881.....", "....199.991.....", "...1999.9991....",
               "...1111.1111...."]
LEGS_SIDE_B = [".....18881......", ".....18881......", ".....18891......", ".....19991......", "....19991.......",
               "....11111......."]
PLAYER_FRAMES = {
    "down0": pad16(HEAD_DOWN + BODY_DOWN + LEGS_A), "down1": pad16(HEAD_DOWN + BODY_DOWN + LEGS_B),
    "up0": pad16(HEAD_UP + BODY_UP + LEGS_A), "up1": pad16(HEAD_UP + BODY_UP + LEGS_B),
    "side0": pad16(HEAD_SIDE + BODY_SIDE + LEGS_SIDE_A), "side1": pad16(HEAD_SIDE + BODY_SIDE + LEGS_SIDE_B),
}
player_words = []
for name in ("down0", "down1", "up0", "up1", "side0", "side1"):
    player_words += tiles_4bpp(grid_image(PLAYER_FRAMES[name]), 2, 4)

# The axe while swinging (16x16, nature palette): handle and head.
AXE = ["................", "............11..", "...........1cc1.", "..........1cdd1.", ".........16cd1..",
       "........167.1...", ".......167......", "......167.......", ".....167........", "....167.........",
       "...167..........", "..167...........", "..11............", "................", "................",
       "................"]

# Kiln, rebuilt from rows of stones with mortar lines (no checkerboard).
kiln, kp = sprite_canvas(32, 32)
for y in range(27, 32):
    for x in range(32):
        if ((x - 16) / 14) ** 2 + ((y - 29.5) / 2.2) ** 2 <= 1: kp[x, y] = 8
for y in range(6, 29):
    for x in range(4, 28):
        if ((x - 16) / 12) ** 2 + ((y - 21) / 15) ** 2 <= 1:
            row = (y - 6) // 4
            mortar = (y - 6) % 4 == 3 or (x + (row % 2) * 3) % 6 == 0
            light = x < 16 - (y - 6) // 4
            kp[x, y] = 13 if mortar else (12 if light else 13 if x > 22 else 12)
            if not mortar and (x + y) % 7 == 0: kp[x, y] = 11
for y in range(19, 28):
    for x in range(12, 20):
        if ((x - 16) / 4.2) ** 2 + ((y - 24) / 5) ** 2 <= 1: kp[x, y] = 14 if (y < 23 or x in (12, 13, 19)) else 15
for y in range(1, 8):
    for x in range(14, 19): kp[x, y] = 12 if x < 16 else 13
outline_sprite(kp, 32, 32)

# Debris: a twig, a stone, a clump of weeds (nature palette).
twig = grid_image(pad16(["................", "..........1.....", ".........171....", "...11...1761....",
                         "..1771.17611....", "...1767761......", "....167761......", ".....11161......",
                         "......1..1......", "................", "....88888888....", "................"], 16))
stone = grid_image(pad16(["................", ".....111111.....", "....1ccccccd1...", "...1cccccccdd1..",
                          "..1ccccccddddd1.", "..1cccdddddddd1.", "..1ddddddddddd1.", "...1dddddddd11..",
                          "....11111111....", "...8888888888...", "................"], 16))
weeds = grid_image(pad16(["................", ".....1....1.....", "....141..141....", "....1431.141..1.",
                          ".1..14311431.141", "141..1431431.131", "1431.143143114 1", ".1431.1431431431",
                          "..14311431431431", "...1443343343441", "....111111111111", "...8888888888888"], 16))
# Forage flowers (16x16): their own palette banks so each can take any petal color.
# 1 outline, 2 stem, 3 petal, 4 petal light, 5 center.
flower = grid_image(pad16(["................", "......111.......", ".....14431......", "....1443431.....",
                           "...144353441....", "....1435531.....", ".....14431......", "......121.......",
                           "...11..121......", "..1221.121......", "...1221121......", ".....12221......",
                           "......121.......", "....8882888.....", "................"], 16))
sparkle = grid_image(["...a....", "...a....", "..aaa...", "aaaaaaa.", "..aaa...", "...a....", "...a....", "........"])

nature_words = []
for im, tw, th in ((oak, 4, 4), (bigtree, 8, 8), (stump, 2, 2), (bush_full, 2, 2), (bush_bare, 2, 2), (kiln, 4, 4),
                   (twig, 2, 2), (stone, 2, 2), (weeds, 2, 2), (grid_image(AXE), 2, 2), (flower, 2, 2), (sparkle, 1, 1)):
    nature_words += tiles_4bpp(im, tw, th)
# Petal colors for the flowers (petal, light): RED, BLUE, YELLOW, VIOLET, PINK, ORANGE.
FLOWER_COLORS = [((210, 50, 50), (250, 120, 110)), ((60, 100, 220), (130, 170, 250)), ((236, 196, 40), (255, 236, 120)),
                 ((140, 70, 210), (190, 140, 250)), ((236, 110, 170), (255, 170, 210)), ((236, 120, 40), (255, 180, 100))]
flower_pals = []
for petal, light in FLOWER_COLORS:
    flower_pals += [rgb15_early(c) for c in [(0, 0, 0), (36, 30, 40), (60, 130, 60), petal, light, (250, 220, 90)]] + [0] * 10

prev = Image.new("RGB", (64 * 6, 64))
for i, (im, pal_) in enumerate(((oak, NATURE_PAL), (kiln, NATURE_PAL), (twig, NATURE_PAL), (weeds, NATURE_PAL),
                                 (grid_image(PLAYER_FRAMES["down0"]), PLAYER_PAL), (grid_image(PLAYER_FRAMES["side1"]), PLAYER_PAL))):
    f = []
    for c in pal_ + [(0, 0, 0)] * (256 - len(pal_)):
        f += list(c)
    im2 = im.copy()
    im2.putpalette(f)
    prev.paste(im2.convert("RGB"), (i * 64, 0))
prev.resize((64 * 6 * 3, 64 * 3), Image.NEAREST).save(os.path.join(PREVIEW, "field_sprites.png"))

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
src.append('const char FONT_CHARS[FONT_N + 1] = "' + FONT_CHARS.replace('\\', '\\\\').replace('"', '\\"') + '";')
rows = []
for ch in FONT_CHARS:
    for r in FONT[ch].split():
        rows.append(int(r, 2))
arr("u8", "font_rows", rows, 28)
hdr.append(f"#define MAP_PAL_N {len(mpal)}   // BG palette 16.. (the day map)")
arr("u16", "map_pal", [rgb15(c) for c in mpal])
arr("u8", "map_bg", list(mp.get_flattened_data() if hasattr(mp, "get_flattened_data") else mp.getdata()), 48, align=True)
arr("u32", "map_tiles", [f"0x{w:08X}" for w in map_words], 8)   # arrow, player (16x16 each)
arr("u8", "forest_bg", list(fd.get_flattened_data() if hasattr(fd, "get_flattened_data") else fd.getdata()), 48, align=True)
arr("u32", "player_tiles", [f"0x{w:08X}" for w in player_words], 8)   # 16x32: down0 down1 up0 up1 side0 side1
arr("u32", "nature_tiles", [f"0x{w:08X}" for w in nature_words], 8)   # see the nature_words list order
arr("u16", "nature_pal", [rgb15(c) for c in NATURE_PAL])
arr("u16", "player_pal", [rgb15(c) for c in PLAYER_PAL])
arr("u16", "flower_pals", flower_pals)   # 6 banks of 16: red, blue, yellow, violet, pink, orange
open(OUT_C, "w").write("\n".join(src) + "\n")
open(OUT_H, "w").write("\n".join(hdr) + "\n")
print(f"art: shop palette {len(pal)} colors, {len(cust_words) // 8} customer tiles, {len(FONT_CHARS)} glyphs")
