# Fireworks (working title)

A cozy firework-making game for the Game Boy Advance, played in emulators (built for the R36 Ultra handheld).

Planned loop: by day, gather materials around town and sometimes find new patterns. By night, customers ask for fireworks and you build them from a quick menu. At the end of the night, everything you made goes up in a show.

## Current state: v0.1.1, show prototype
Only the show exists so far: a night sky over a small town where you launch shells.

- D-pad UP/DOWN: color. L/R: shell pattern. A: fire. B: volley of 5 random shells.
- START: auto show (about half a minute, ends in a finale). SELECT: hide the text.
- Patterns: peony, chrysanthemum, willow, palm, ring, crossette, heart, crackle, glitter.

## How it draws
- Mode 4 bitmap, two pages. Up to 1600 particles, simulated and drawn by ARM code in IWRAM (`src/render.c`).
- The particle layer is blended additively over the sky, which is a per-scanline gradient set by an HBlank interrupt. Bursts light up the sky and town in their color.
- Town, stars, moon, and text are sprites. Sound uses the PSG channels only (launch, boom, crackle).
- The top-right numbers show live particles and CPU use (100% = one frame at 60 fps).

## Build
Needs `arm-none-eabi-gcc` and Python 3.

```sh
sh fireworks/build.sh                          # -> fireworks/fireworks.gba
sh fireworks/test/run.sh test/patterns.txt     # headless run, screenshots in fireworks/test/
```

Tests need `libmgba` (uses `../third_party/mgba` when present).
