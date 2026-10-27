"""Starbloom's music, written as notes. Writes build/audio/<song>.ev for tools/synth.c, which
renders each to 8-bit PCM at 18157 Hz (304 samples per GBA frame, exactly).

A step is an eighth note. Notes are "NAME STEPS" pairs ("F#5 2", "r 4" for a rest); a
chord is names joined by + ("D4+F#4+A4 6"). Chord progressions drive the arpeggios, pads,
and bass, so most parts are one line."""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "build", "audio")
RATE = 18157
FRAME = 304
I_KEYS, I_BELL, I_HARP, I_PAD, I_BASS, I_FLUTE, I_SHAKER, I_KICK, I_MARIMBA = range(9)

NOTE_INDEX = {"C": 0, "C#": 1, "Db": 1, "D": 2, "D#": 3, "Eb": 3, "E": 4, "F": 5, "F#": 6, "Gb": 6,
              "G": 7, "G#": 8, "Ab": 8, "A": 9, "A#": 10, "Bb": 10, "B": 11}


def midi(name):
    return 12 * (int(name[-1]) + 1) + NOTE_INDEX[name[:-1]]


def freq(m):
    return 440.0 * 2 ** ((m - 69) / 12)


def parse(text):
    """-> [(start step, length, [midi notes])]; rests advance time."""
    tok = text.split()
    out, t = [], 0
    for name, steps in zip(tok[0::2], tok[1::2]):
        n = int(steps)
        if name != "r":
            out.append((t, n, [midi(x) for x in name.split("+")]))
        t += n
    return out, t


# Chords by name: root and quality, voiced from a given octave.
QUAL = {"": (0, 4, 7), "m": (0, 3, 7), "7": (0, 4, 7, 10), "maj7": (0, 4, 7, 11), "m7": (0, 3, 7, 10),
        "sus": (0, 5, 7), "add9": (0, 4, 7, 14)}


def chord(sym, octave):
    root = sym[:2] if len(sym) > 1 and sym[1] in "#b" else sym[:1]
    q = QUAL[sym[len(root):]]
    base = 12 * (octave + 1) + NOTE_INDEX[root]
    return [base + i for i in q]


class Song:
    def __init__(self, name, bpm, bar, prog, loop=True, wet=0.3, room=1.0, gain=1.0, tail=0):
        self.name, self.bpm, self.bar, self.loop = name, bpm, bar, loop
        self.prog = prog.split()
        self.steps = len(self.prog) * bar + tail
        self.wet, self.room, self.gain = wet, room, gain
        self.notes = []          # (inst, start step, length steps, midi, velocity)

    def add(self, inst, t, n, m, vel):
        self.notes.append((inst, t, n, m, vel))

    def line(self, inst, text, vel, at=0, octave=0):
        notes, n = parse(text)
        assert at + n == self.steps or not at, f"{self.name}: a line is {n} steps, the song is {self.steps}"
        for t, n, ms in notes:
            for m in ms:
                self.add(inst, at + t, n, m + 12 * octave, vel)

    def each_bar(self, fn):
        for i, sym in enumerate(self.prog):
            fn(i * self.bar, sym)

    def pad(self, octave, vel):
        self.each_bar(lambda t, s: [self.add(I_PAD, t, self.bar, m, vel) for m in chord(s, octave)])

    def bass(self, rhythm, vel, octave=2):
        """rhythm: (step, length, degree) per bar; degree 0 = root, 1 = fifth, 2 = octave."""
        def f(t, s):
            r = chord(s, octave)[0]
            for st, n, d in rhythm:
                self.add(I_BASS, t + st, n, r + (0, 7, 12)[d], vel)
        self.each_bar(f)

    def arp(self, inst, octave, pattern, vel, length=2, accent=None):
        """pattern: one chord-tone index per step (None = rest); index 3+ wraps up an octave."""
        def f(t, s):
            c = chord(s, octave)[:3]
            for k, p in enumerate(pattern):
                if p is None:
                    continue
                m = c[p % 3] + 12 * (p // 3)
                v = vel * (accent if accent and k == 0 else 1)
                self.add(inst, t + k, length, m, v)
        self.each_bar(f)

    def comp(self, inst, octave, hits, vel):
        """hits: (step, length) per bar, playing the whole chord."""
        self.each_bar(lambda t, s: [self.add(inst, t + st, n, m, vel) for st, n in hits for m in chord(s, octave)])

    def drums(self, inst, steps, vel, every=1):
        for b in range(len(self.prog)):
            if b % every:
                continue
            for st in steps:
                self.add(inst, b * self.bar + st, 1, 60, vel)

    def write(self):
        step_s = 30.0 / self.bpm
        total = round(self.steps * step_s * RATE / FRAME) * FRAME
        if not self.loop:
            total += 3 * FRAME * 60          # three seconds to ring out
        per_step = round(self.steps * step_s * RATE / FRAME) * FRAME / self.steps
        lines = [f"{RATE} {total} {1 if self.loop else 0} {self.wet} {self.room} {self.gain}"]
        for inst, t, n, m, vel in sorted(self.notes, key=lambda x: x[1]):
            lines.append(f"{inst} {round(t * per_step)} {n * step_s:.4f} {freq(m):.3f} {vel:.3f}")
        with open(os.path.join(OUT, self.name + ".ev"), "w") as f:
            f.write("\n".join(lines) + "\n")
        print(f"{self.name}: {total / RATE:.1f} s, {len(self.notes)} notes")


def title():
    # A slow waltz in D: a music box over a harp, the shop sign swinging in the evening.
    s = Song("title", 84, 6, "D A Bm F#m G D Em A D A Bm F#m G A D D", wet=0.35, room=1.2)
    s.line(I_BELL, "A5 4 F#5 2  E5 3 F#5 1 E5 2  D5 4 F#5 2  C#5 6 "
                   "B4 2 D5 2 G5 2  F#5 4 A5 2  G5 3 F#5 1 E5 2  E5 6 "
                   "A5 4 F#5 2  E5 3 F#5 1 E5 2  D5 2 F#5 2 B5 2  A5 4 F#5 2 "
                   "G5 3 A5 1 B5 2  A5 3 G5 1 E5 2  F#5 4 E5 2  D5 6", 0.42)
    s.arp(I_HARP, 3, [0, 1, 2, 4, 2, 1], 0.2)
    s.bass([(0, 6, 0)], 0.3)
    s.pad(4, 0.07)
    return s


def day():
    # Market morning in G: bouncing marimba, an ocarina tune, a shaker on the off beats.
    s = Song("day", 104, 8, "G D Em C G D C D Em C G D C D G G", wet=0.22, room=0.8)
    s.line(I_FLUTE, "B4 2 D5 2 G5 3 F#5 1  A5 4 F#5 2 D5 2  E5 2 G5 2 B5 3 A5 1  G5 6 r 2 "
                    "B5 2 A5 2 G5 2 D5 2  F#5 2 E5 2 D5 2 A4 2  C5 2 E5 2 G5 2 E5 2  F#5 6 r 2 "
                    "G5 3 F#5 1 E5 2 B4 2  C5 2 E5 2 G5 4  D5 3 E5 1 D5 2 B4 2  A4 4 D5 4 "
                    "E5 2 G5 2 C6 3 B5 1  A5 3 G5 1 F#5 2 A5 2  G5 8  r 8", 0.3)
    s.arp(I_MARIMBA, 4, [0, 2, 1, 2, 0, 2, 1, 2], 0.18, length=1, accent=1.3)
    s.bass([(0, 3, 0), (4, 2, 1), (6, 2, 2)], 0.32)
    s.drums(I_SHAKER, [1, 3, 5, 7], 0.12)
    s.drums(I_KICK, [0, 4], 0.4)
    return s


def mine():
    # Down in the mine, in A minor: low warm chords, slow bass, drips of a music box.
    s = Song("mine", 70, 8, "Am F C G Am F Dm E Am F C G", wet=0.5, room=1.5)
    s.comp(I_KEYS, 3, [(0, 7)], 0.16)
    s.bass([(0, 8, 0)], 0.3)
    s.pad(3, 0.05)
    drips = [(0, "E6"), (5, "A5"), (11, "C6"), (19, "B5"), (22, "G5"), (30, "E6"), (37, "D6"), (44, "A5"),
             (50, "F6"), (53, "E6"), (61, "B5"), (66, "C6"), (75, "E6"), (79, "A5"), (84, "G6"), (90, "E6")]
    for t, n in drips:
        s.add(I_BELL, t, 2, midi(n), 0.2)
    s.line(I_MARIMBA, "r 32  A4 2 C5 2 E5 2 D5 2  C5 4 A4 4  r 8  E5 2 D5 2 B4 4 "
                      "r 8  A4 2 C5 2 E5 2 G5 2  E5 8  r 8", 0.22)
    return s


def shop():
    # The shop at night, in F: soft electric piano, a walking-ish bass, the ocarina again.
    s = Song("shop", 84, 8, "Fmaj7 Dm7 Gm7 C7 Fmaj7 Am7 Bbmaj7 C7 Dm7 Am7 Bbmaj7 Fmaj7 Gm7 C7 Fmaj7 Fmaj7",
             wet=0.3, room=1.0)
    s.line(I_FLUTE, "A5 3 G5 1 F5 2 C5 2  D5 4 F5 2 A5 2  G5 3 F5 1 D5 2 Bb4 2  C5 6 r 2 "
                    "A5 2 C6 2 A5 2 G5 2  E5 4 C5 2 E5 2  D5 3 F5 1 A5 4  G5 6 r 2 "
                    "F5 3 E5 1 D5 2 A4 2  C5 4 E5 4  D5 2 F5 2 A5 2 G5 2  F5 6 r 2 "
                    "Bb5 3 A5 1 G5 2 D5 2  E5 3 F5 1 G5 2 Bb5 2  A5 8  r 8", 0.26)
    s.comp(I_KEYS, 3, [(0, 3), (3, 1), (5, 3)], 0.11)
    s.bass([(0, 4, 0), (4, 3, 1)], 0.3)
    return s


def show():
    # Under the fireworks, in C: just a pad and a slow harp, quiet enough for the booms.
    s = Song("show", 64, 8, "Cmaj7 Fmaj7 Am7 G Cmaj7 Fmaj7 Dm7 G", wet=0.5, room=1.5, gain=0.8)
    s.pad(4, 0.1)
    s.arp(I_HARP, 4, [0, None, 1, None, 2, None, 4, None], 0.14, length=2)
    s.bass([(0, 8, 0)], 0.2)
    s.line(I_BELL, "r 6 E6 2  r 6 C6 2  r 6 E6 2  r 4 D6 4  r 6 G6 2  r 6 A6 2  r 6 F6 2  r 4 D6 4", 0.12)
    return s


def review():
    # Review time, in D: warm and a little proud.
    s = Song("review", 76, 8, "Dmaj7 Bm7 Gmaj7 A Dmaj7 F#m7 Gmaj7 A", wet=0.32, room=1.1)
    s.line(I_BELL, "A5 4 F#5 4  D5 4 F#5 2 E5 2  D5 6 B4 2  C#5 8 "
                   "A5 4 F#5 2 A5 2  C#6 4 A5 4  B5 3 A5 1 G5 2 F#5 2  E5 8", 0.36)
    s.arp(I_KEYS, 4, [0, 1, 2, 1, 0, 1, 2, 4], 0.1, length=2)
    s.bass([(0, 6, 0), (6, 2, 1)], 0.3)
    s.pad(3, 0.05)
    return s


SONGS = [title, day, mine, shop, show, review]

if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    for make in SONGS:
        make().write()
