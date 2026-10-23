// GBA registers and memory used by the game.
#pragma once
#include "fw.h"

#define REG16(a) (*(vu16*)(a))
#define REG32(a) (*(vu32*)(a))
#define REG_DISPCNT   REG16(0x04000000)
#define REG_DISPSTAT  REG16(0x04000004)
#define REG_VCOUNT    REG16(0x04000006)
#define REG_BG2PA     REG16(0x04000020)
#define REG_BG2PB     REG16(0x04000022)
#define REG_BG2PC     REG16(0x04000024)
#define REG_BG2PD     REG16(0x04000026)
#define REG_BG2X      REG32(0x04000028)
#define REG_BG2Y      REG32(0x0400002C)
#define REG_WIN0H     REG16(0x04000040)
#define REG_WIN0V     REG16(0x04000044)
#define REG_WININ     REG16(0x04000048)
#define REG_WINOUT    REG16(0x0400004A)
#define REG_BLDCNT    REG16(0x04000050)
#define REG_BLDALPHA  REG16(0x04000052)
#define REG_DMA3SAD   REG32(0x040000D4)
#define REG_DMA3DAD   REG32(0x040000D8)
#define REG_DMA3CNT   REG32(0x040000DC)
#define REG_TM2D      REG16(0x04000108)
#define REG_TM2CNT    REG16(0x0400010A)
#define REG_KEYINPUT  REG16(0x04000130)
#define REG_IE        REG16(0x04000200)
#define REG_IF        REG16(0x04000202)
#define REG_WAITCNT   REG16(0x04000204)
#define REG_IME       REG16(0x04000208)
#define IRQ_VECTOR    REG32(0x03007FFC)

#define BG_PAL    ((vu16*)0x05000000)
#define OBJ_PAL   ((vu16*)0x05000200)
#define OAM       ((vu16*)0x07000000)
#define PAGE0     ((u8*)0x06000000)
#define PAGE1     ((u8*)0x0600A000)
#define OBJ_TILES ((vu16*)0x06010000)   // mode 4: only tiles 512+ are free

#define RGB(r, g, b) ((r) | ((g) << 5) | ((b) << 10))

enum { KEY_A = 1, KEY_B = 2, KEY_SELECT = 4, KEY_START = 8, KEY_RIGHT = 16, KEY_LEFT = 32,
       KEY_UP = 64, KEY_DOWN = 128, KEY_R = 256, KEY_L = 512 };
