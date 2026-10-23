# Fireworks (working title)

A cozy firework-making game for the Game Boy Advance, played in emulators (built for the R36 Ultra handheld).

Planned loop: by day, gather materials around town and sometimes find new patterns. By night, customers ask for fireworks and you build them from a quick menu. At the end of the night, everything you made goes up in a show.

## Current state: v0.2.0, the night shop
- **Title:** NEW GAME or FREE PLAY (the v0.1 sandbox; SELECT+START returns to the title).
- **The shop (night):** customers walk up with requests, some exact ("A BIG RED PEONY"), some vague ("A PERFECT CIRCLE IN THE SKY"). Build each one from quick menus: size, pattern, color. Bigger and fancier fireworks take more time on the night clock (8:00 PM to 1:00 AM) and use powder, shells, and colored stars. Or turn a customer away.
- **The show:** at closing time, every firework you made goes up. You don't see whose is whose.
- **Review time:** each customer's comment pops up over the town, then the night gets a 0-5 star rating with coins, reputation, and gifts.
- **Hidden likes:** a customer may secretly love a color, pattern, or size. Hit it (even by accident) and they love it: more coins and rep, plus a gift: colored stars, a tip, or a scroll that teaches a new pattern.
- You start knowing peony, chrysanthemum, willow, and ring. Palm, crossette, heart, crackle, and glitter come from scrolls.
- No day phase yet: materials top back up to the starting amounts each night. No saving yet. No sound.

## How it draws
- Mode 4 bitmap, two pages. Up to 1600 particles, simulated and drawn by ARM code in IWRAM (`src/render.c`).
- In the show, the particle layer is blended additively over the sky, a per-scanline gradient set by an HBlank interrupt. Bursts light up the sky and town in their color.
- The shop is a full bitmap scene redrawn only when something changes; menus and text are drawn into the bitmap. Customers are sprites recolored per customer.
- All art is drawn in code by `tools/make_art.py` (previews in `art/`).

## Build
Needs `arm-none-eabi-gcc` and Python 3 with Pillow.

```sh
sh fireworks/build.sh                          # -> fireworks/fireworks.gba
sh fireworks/test/run.sh test/night.txt        # headless run, screenshots in fireworks/test/
```

Tests need `libmgba` (uses `../third_party/mgba` when present).
