#!/bin/sh
# Build fireworks.gba. render.c is ARM code that runs from IWRAM; the rest is Thumb in ROM.
set -e
cd "$(dirname "$0")"
python3 tools/make_tables.py
CFLAGS="-mcpu=arm7tdmi -mthumb-interwork -O2 -ffreestanding -fno-builtin -Wall"
mkdir -p build
arm-none-eabi-gcc -mcpu=arm7tdmi -c src/crt0.s -o build/crt0.o
arm-none-eabi-gcc $CFLAGS -mthumb -c src/main.c -o build/main.o
arm-none-eabi-gcc $CFLAGS -marm -c src/render.c -o build/render.o
arm-none-eabi-gcc -mcpu=arm7tdmi -mthumb-interwork -nostdlib -T src/gba.ld build/crt0.o build/main.o build/render.o -lgcc -o build/fireworks.elf
arm-none-eabi-objcopy -O binary build/fireworks.elf fireworks.gba
python3 tools/fixheader.py fireworks.gba
