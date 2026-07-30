#ifndef PPU_MEMORY_H
#define PPU_MEMORY_H

#include "src/structs.h"

uint8_t ppu_read_vram(uint16_t addr);
void ppu_write_vram(uint16_t addr, uint8_t val);
void ppu_write_io(uint16_t addr, uint8_t val);

#endif
