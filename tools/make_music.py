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
    # The main theme, in D: the shop's warm piano and ocarina, a little slower and dreamier.
    s = Song("title", 76, 8, "Dmaj7 Gmaj7 Dmaj7 Gmaj7 Bm7 Em7 Gmaj7 A7 Dmaj7 Gmaj7 F#m7 Bm7 Em7 A7 Dmaj7 Dmaj7",
             wet=0.32, room=1.1)
    s.line(I_FLUTE, "A5 4 F#5 2 E5 2  D5 6 r 2  A5 3 B5 1 A5 2 F#5 2  G5 6 r 2 "
                    "F#5 3 D5 1 B4 4  E5 3 G5 1 B5 4  A5 3 G5 1 F#5 2 D5 2  E5 6 r 2 "
                    "F#5 4 A5 2 C#6 2  B5 4 A5 2 G5 2  A5 3 F#5 1 E5 2 C#5 2  D5 6 r 2 "
                    "G5 3 F#5 1 E5 2 B4 2  C#5 3 D5 1 E5 2 G5 2  F#5 8  r 8", 0.25)
    s.comp(I_KEYS, 3, [(0, 3), (3, 1), (5, 3)], 0.11)
    s.bass([(0, 4, 0), (4, 3, 1)], 0.28)
    return s


def day():
    # Market morning in G: the same warm band, a bit livelier.
    s = Song("day", 96, 8, "Gmaj7 D7 Em7 Cmaj7 Gmaj7 D7 Cmaj7 D7 Em7 Cmaj7 Gmaj7 D7 Cmaj7 D7 Gmaj7 Gmaj7",
             wet=0.28, room=0.9)
    s.line(I_FLUTE, "B4 2 D5 2 G5 3 F#5 1  A5 4 F#5 2 D5 2  E5 2 G5 2 B5 3 A5 1  G5 6 r 2 "
                    "B5 2 A5 2 G5 2 D5 2  F#5 2 E5 2 D5 2 A4 2  C5 2 E5 2 G5 2 E5 2  F#5 6 r 2 "
                    "G5 3 F#5 1 E5 2 B4 2  C5 2 E5 2 G5 4  D5 3 E5 1 D5 2 B4 2  A4 4 D5 4 "
                    "E5 2 G5 2 C6 3 B5 1  A5 3 G5 1 F#5 2 A5 2  G5 8  r 8", 0.25)
    s.comp(I_KEYS, 3, [(0, 2), (2, 2), (4, 2), (6, 1)], 0.1)
    s.bass([(0, 3, 0), (4, 2, 1), (6, 2, 2)], 0.28)
    return s


def mine():
    # Down in the mine, in A minor: slower and lower, still warm (no music box).
    s = Song("mine", 72, 8, "Am7 Fmaj7 Cmaj7 G Am7 Fmaj7 Dm7 E7 Am7 Fmaj7 Dm7 E7", wet=0.38, room=1.3)
    s.line(I_FLUTE, "E5 4 C5 2 A4 2  A4 6 r 2  G4 2 C5 2 E5 2 G5 2  D5 6 r 2 "
                    "E5 3 D5 1 C5 2 A4 2  C5 4 A4 4  F5 3 E5 1 D5 2 A4 2  B4 6 r 2 "
                    "C5 2 E5 2 A5 4  G5 3 F5 1 E5 2 C5 2  D5 4 F5 2 E5 2  E5 6 r 2", 0.22)
    s.comp(I_KEYS, 3, [(0, 4), (4, 3)], 0.1)
    s.bass([(0, 6, 0), (6, 2, 1)], 0.28)
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
    # Under the fireworks, in C: slow piano, long ocarina notes, quiet enough for the pops.
    s = Song("show", 66, 8, "Cmaj7 Am7 Dm7 G7 Cmaj7 Am7 Fmaj7 G7", wet=0.38, room=1.3, gain=0.85)
    s.line(I_FLUTE, "E5 8  r 4 C5 4  F5 6 r 2  D5 8  G5 6 r 2  E5 4 C5 4  A5 6 r 2  G5 8", 0.2)
    s.comp(I_KEYS, 3, [(0, 4), (4, 4)], 0.1)
    s.bass([(0, 8, 0)], 0.25)
    return s


def review():
    # Review time, in D: warm and a little proud.
    s = Song("review", 80, 8, "Dmaj7 Bm7 Gmaj7 A7 Dmaj7 F#m7 Gmaj7 A7", wet=0.3, room=1.0)
    s.line(I_FLUTE, "A5 4 F#5 4  D5 4 F#5 2 E5 2  D5 6 B4 2  C#5 8 "
                    "A5 4 F#5 2 A5 2  C#6 4 A5 4  B5 3 A5 1 G5 2 F#5 2  E5 8", 0.25)
    s.comp(I_KEYS, 3, [(0, 3), (3, 1), (5, 3)], 0.11)
    s.bass([(0, 4, 0), (4, 3, 1)], 0.28)
    return s


SONGS = [title, day, mine, shop, show, review]

if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    for make in SONGS:
        make().write()
