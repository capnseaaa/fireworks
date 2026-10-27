#!/bin/sh
# Build starbloom.gba. render.c is ARM code that runs from IWRAM; the rest is Thumb in ROM.
set -e
cd "$(dirname "$0")"
python3 tools/make_tables.py
python3 tools/make_art.py
# Sound: synth.c renders the music (notes in make_music.py) and the firework sounds, then
# make_sfx.py writes the tables. Skipped when nothing changed (it takes a few seconds).
mkdir -p build/audio
if [ ! -f build/audio/.done ] || [ tools/synth.c -nt build/audio/.done ] || [ tools/make_music.py -nt build/audio/.done ]; then
    cc -O2 tools/synth.c -o build/synth -lm
    build/synth sfx build/audio
    python3 tools/make_music.py
    for f in build/audio/*.ev; do build/synth song "$f" "${f%.ev}.raw"; done
    touch build/audio/.done
fi
python3 tools/make_sfx.py
CFLAGS="-mcpu=arm7tdmi -mthumb-interwork -O2 -ffreestanding -fno-builtin -Wall"
mkdir -p build
arm-none-eabi-gcc -mcpu=arm7tdmi -c src/crt0.s -o build/crt0.o
arm-none-eabi-gcc -mcpu=arm7tdmi -c src/audio.s -o build/audio.o
for f in main show shop day field dev ui art sound; do
    arm-none-eabi-gcc $CFLAGS -mthumb -c src/$f.c -o build/$f.o
done
arm-none-eabi-gcc $CFLAGS -marm -c src/render.c -o build/render.o
arm-none-eabi-gcc -mcpu=arm7tdmi -mthumb-interwork -nostdlib -T src/gba.ld build/crt0.o build/main.o build/show.o build/shop.o build/day.o build/field.o build/dev.o build/ui.o build/art.o build/sound.o build/audio.o build/render.o -lgcc -o build/starbloom.elf
arm-none-eabi-objcopy -O binary build/starbloom.elf starbloom.gba
python3 tools/fixheader.py starbloom.gba
