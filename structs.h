#ifndef STRUCTS_H
#define STRUCTS_H 

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#define GAMEBOY_MEMORY_SIZE (1 << 16) // 64KB
#define INIT_PC 0x0100

// OAM DMA transfer
#define _DMA  0xFF46

// Input Registers
#define _JOYP 0xFF00

// Timer Registers
#define _DIV  0xFF04
#define _TIMA 0xFF05
#define _TMA  0xFF06
#define _TAC  0xFF07

// Audio Registers (Sound Channel 1)
#define _NR10 0xFF10
#define _NR11 0xFF11
#define _NR12 0xFF12
#define _NR14 0xFF14

// Audio Registers (Sound Channel 2)
#define _NR21 0xFF16
#define _NR22 0xFF17
#define _NR24 0xFF19

// Audio Registers (Sound Channel 3)
#define _NR30 0xFF1A
#define _NR31 0xFF1B
#define _NR32 0xFF1C
#define _NR33 0xFF1E

// Audio Registers (Sound Channel 4)
#define _NR41 0xFF20
#define _NR42 0xFF21
#define _NR43 0xFF22
#define _NR44 0xFF23 // Note: $FF23 is actually NR44 on real hardware

// Audio Control Registers
#define _NR50 0xFF24
#define _NR51 0xFF25
#define _NR52 0xFF26

// Video/PPU Registers
#define _LCDC 0xFF40
#define _STAT 0xFF41
#define _SCY  0xFF42
#define _SCX  0xFF43
#define _LY   0xFF44
#define _LYC  0xFF45
#define _BGP  0xFF47
#define _OBP0 0xFF48
#define _OBP1 0xFF49
#define _WY   0xFF4A
#define _WX   0xFF4B

// Interrupt Enable Register
#define _IF   0xFF0F
#define _IE   0xFFFF


typedef void (*instruction_ptr)(void);

typedef struct {
    uint8_t rom[0x8000];    // 32KB Cartridge (Bank 0 and 1)
    uint8_t vram[0x2000];   // 8KB Video RAM
    uint8_t external[0x2000]; // 8KB External RAM (in the cartridge)
    uint8_t wram[0x2000];   // 8KB Main/Work RAM
    uint8_t oam[0xA0];      // Sprite Attribute Table
    uint8_t io[0x80];       // I/O Registers (FF00-FF7F)
    uint8_t hram[0x7F];     // High RAM (FF80-FFFE)
    uint8_t ie;             // Interrupt Enable Register (FFFF)
} GameBoyMemory;

typedef struct {
    struct {
        union {
            struct{
                uint8_t f;
                uint8_t a;
            };
            uint16_t af;
        };
    };
    struct {
        union {
            struct{
                uint8_t c;
                uint8_t b;
            };
            uint16_t bc;
        };
    };
    struct {
        union {
            struct{
                uint8_t e;
                uint8_t d;
            };
            uint16_t de;
        };
    };
    struct {
        union {
            struct{
                uint8_t l;
                uint8_t h;
            };
            uint16_t hl;
        };
    };
    uint16_t sp;
    uint16_t pc;
} registers;

static const uint32_t shades[4] = {
    0xFFFFFF,  // white
    0xAAAAAA,  // light gray
    0x555555,  // dark gray
    0x000000   // black
};

GameBoyMemory *memory;
registers *reg;

uint16_t internalClock = 0;
int8_t ime_next = -1;
bool ei = false, ime = false;

bool prefix_flag = false;

#endif