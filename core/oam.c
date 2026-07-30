#include "core/oam.h"
#include "core/emulator_core.h"

uint8_t oam_read(uint16_t addr) {
    if (addr >= 0xFEA0 && addr <= 0xFEFF) return 0x00;
    uint8_t stat_mode = memory->io[_STAT - 0xFF00] & 0x03;
    if (stat_mode >= 2) return 0xFF;
    return memory->oam[addr - 0xFE00];
}

void oam_write(uint16_t addr, uint8_t val) {
    if (addr >= 0xFEA0 && addr <= 0xFEFF) return;
    uint8_t stat_mode = memory->io[_STAT - 0xFF00] & 0x03;
    if (stat_mode == 2) {
        size_t offset = addr - 0xFE00;
        size_t base = offset & 0xFE;
        memory->oam[base] = val;
        memory->oam[base | 1] = val;
    } else if (stat_mode < 2) {
        memory->oam[addr - 0xFE00] = val;
    }
}
