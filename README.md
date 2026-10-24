# Fireworks (working title)

A cozy firework-making game for the Game Boy Advance, played in emulators (built for the R36 Ultra handheld).

Planned loop: by day, gather materials around town and sometimes find new patterns. By night, customers ask for fireworks and you build them from a quick menu. At the end of the night, everything you made goes up in a show.

## Current state: v0.9.0, the quarry
- **Title:** CONTINUE, NEW GAME, or FREE PLAY (the v0.1 sandbox; SELECT+START returns to the title).
- **Intro:** you've just taken over Grandma's firework shop in a seaside town. Nine short pages in your own words, with the controls in brackets. START skips it.
- **Saving:** the game saves itself at the start of every night and every day. CONTINUE picks up there. NEW GAME asks before replacing a save.
- **Night:** customers walk up with requests, some exact ("A BIG RED PEONY"), some vague ("A PERFECT CIRCLE IN THE SKY"). Build each one from quick menus: size, pattern, color. Bigger and fancier fireworks take more time on the night clock (8:00 PM to 1:00 AM) and use powder, shells, and colored stars. Or turn a customer away.
- **The show:** at closing time, every firework you made goes up. You don't see whose is whose.
- **Review time:** each customer's comment pops up over the town, then the night gets a 0-5 star rating with coins, reputation, and gifts.
- **Day:** walk the town map from 8:00 AM to 6:00 PM. Every walk (20 min) and job costs time, and each spot works once a day.
  - Forest (EXPLORE): walk around, Stardew style. Chop oaks (several axe swings; shells), search bushes (green stars), pick sparkling flowers (red, blue, yellow, violet, pink, orange stars), clear twigs, weeds, and stones (a pickaxe), and fire the kiln: hold A to keep the heat in the moving green band for powder. The big old tree needs a better axe and may hide a scroll. Flowers and debris come back fresh each day. Walking is free; each action costs a few minutes.
  - Gear at the market: axe (3 tiers), pickaxe (3 tiers), lantern, bag (holds 20, 30, 40 things a day).
  - Mine (ENTER THE MINE): Stardew-style floors. Break rocks with the pickaxe; one rock on each floor hides the ladder down. Ore by depth: sulfur (powder), red, and blue near the top; green and cyan from floor 4; violet and silver from floor 7. Hard rock needs a better pickaxe; floors 5-10 need a lantern. The ladder up leaves. CRACK GEODES: stop a swinging pointer on the crack, three geodes a day; dead center gives rare colors.
  - Beach (EXPLORE): a rocky coast of tide pools on three terraces stepping down to the sea, joined by worn ramps; the way down is a wooden stair in the top-left corner. Finds wash up on the rocks each day: scallops (pink, white), sea glass (cyan, green, blue), coral (red, orange), amber (gold), salt crystals (yellow), driftwood (shells). The pools need a net (pink, cyan, blue, sometimes a bottle with a scroll). From the rocky point, CATCH DRIFTWOOD: drop the hook on the logs that float past (six casts a day; bottles hold scrolls).
  - Quarry (EXPLORE, needs the rope): a stepped stone pit, terraces of cut stone winding down to a pond, with an old workshop shed and a rusted crane on the rim. Break white stone (white stars), gold-flecked stone (gold), and silver (deeper down; needs pickaxe II); search old crates for scrolls or odd stars. CLIMB THE CLIFF: press the hold shown (L, R, or UP) before it fades; higher means more stars, and an eagle's nest at the top may hold a scroll.
  - Market: powder, shells, any colored star, and the tools.
  - START opens the bag. Home opens the shop for the night.
- **Upgrades** (Home > IMPROVE THE SHOP, during the day): Workbench (3 levels, 15% faster each), Late Hours (2 levels, open until 2 then 3 AM), Shop Sign (2 levels, 15% more pay each), Display Shelf (you notice what customers secretly love). The top levels need a reputation title.
- **Reputation titles:** Newcomer, Local Favorite (10), Town Treasure (25), Legend (50). Each title means bigger orders and 10% more pay. A new title is announced at review time.
- **Hidden likes:** a customer may secretly love a color, pattern, or size. Hit it (even by accident) and they love it: more coins and rep, plus a gift: colored stars, a tip, or a scroll that teaches a new pattern.
- You start knowing peony, chrysanthemum, willow, and ring. Scrolls teach the rest.
- No sound yet.

## Dev mode
Hold L+R and press B (almost anywhere) for a test menu: +1000 coins, max materials, all patterns, all upgrades and gear, +10 rep, jump to a new day or tonight's shop, set the day clock to 5:50 PM, mark mine floor 9 as reached, or go back to the title.

## How it draws
- Mode 4 bitmap, two pages. Up to 1600 particles, simulated and drawn by ARM code in IWRAM (`src/render.c`).
- In the show, the particle layer is blended additively over the sky, a per-scanline gradient set by an HBlank interrupt. Bursts light up the sky and town in their color.
- The shop is a full bitmap scene redrawn only when something changes; menus and text are drawn into the bitmap. Customers are sprites recolored per customer.
- The day map is a full bitmap in its own palette, drawn GBA-RPG style (outlines, top-left light, shadows).
- All art is drawn in code by `tools/make_art.py` (previews in `art/`).

## Build
Needs `arm-none-eabi-gcc` and Python 3 with Pillow.

```sh
sh fireworks/build.sh                          # -> fireworks/fireworks.gba
sh fireworks/test/run.sh test/night.txt        # headless run, screenshots in fireworks/test/
```

Tests need `libmgba` (uses `../third_party/mgba` when present).
