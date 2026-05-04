#ifndef STRUCTS_H
#include <stdint.h>
#define STRUCTS_H
#define GAMEBOY_MEMORY_SIZE (2 << 16) // 64KB
#define INIT_PC 0x0100

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

#endif