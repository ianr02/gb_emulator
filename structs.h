#ifndef STRUCTS_H
#include <stdint.h>
#include <stdlib.h>
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

uint8_t read_byte(uint16_t address) {
    if (address >= 0x0000 && address <= 0x7FFF) {
        return memory->rom[address];
    } else if (address >= 0x8000 && address <= 0x9FFF) {
        return memory->vram[address - 0x8000];
    } else if (address >= 0xA000 && address <= 0xBFFF) {
        return memory->external[address - 0xA000];
    } else if (address >= 0xC000 && address <= 0xDFFF) {
        return memory->wram[address - 0xC000];
    } else if (address >= 0xE000 && address <= 0xFDFF) {
        return memory->wram[address - 0xE000];
    } else if (address >= 0xFE00 && address <= 0xFE9F) {
        return memory->oam[address - 0xFE00];
    } else if (address >= 0xFF00 && address <= 0xFF7F){
        return memory->io[address - 0xFF4C];
    } else if (address >= 0xFF80 && address <= 0xFFFE){
        return memory->hram[address - 0xFF80];
    } else if (address == 0xFFFF){
        return memory->ie;
    }
    return 0xFF;
}

int save_byte(uint16_t address, uint16_t val){
    if (address >= 0x0000 && address <= 0x7FFF) {
        memory->rom[address] = val;
    } else if (address >= 0x8000 && address <= 0x9FFF) {
        memory->vram[address - 0x8000] = val;
    } else if (address >= 0xA000 && address <= 0xBFFF) {
        memory->external[address - 0xA000] = val;
    } else if (address >= 0xC000 && address <= 0xDFFF) {
        memory->wram[address - 0xC000] = val;
    } else if (address >= 0xE000 && address <= 0xFDFF) {
        memory->wram[address - 0xE000] = val;
    } else if (address >= 0xFE00 && address <= 0xFE9F) {
        memory->oam[address - 0xFE00] = val;
    } else if (address >= 0xFF00 && address <= 0xFF7F){
        memory->io[address - 0xFF4C] = val;
    } else if (address >= 0xFF80 && address <= 0xFFFE){
        memory->hram[address - 0xFF80] = val;
    } else if (address == 0xFFFF){
        memory->ie = val;
    } else {
        exit(EXIT_FAILURE);
    }
    return 0;
}


#endif