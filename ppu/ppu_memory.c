#include "ppu/ppu_memory.h"
#include "ppu/ppu.h"
#include "core/emulator_core.h"
#include "core/dma.h"

uint8_t ppu_read_vram(uint16_t addr) {
    uint8_t stat_mode = memory->io[_STAT - 0xFF00] & 0x03;
    if (stat_mode == 3) return 0xFF;
    return memory->vram[addr - 0x8000];
}

void ppu_write_vram(uint16_t addr, uint8_t val) {
    uint8_t stat_mode = memory->io[_STAT - 0xFF00] & 0x03;
    if (stat_mode != 3)
        memory->vram[addr - 0x8000] = val;
}

void ppu_write_io(uint16_t addr, uint8_t val) {
    if (addr == _LCDC) {
        memory->io[_LCDC - 0xFF00] = val;
        if (!(val & 0x80)) {
            memory->io[_LY - 0xFF00] = 0;
            memory->io[_STAT - 0xFF00] &= 0xFC;
        }
    } else if (addr == _STAT) {
        memory->io[_STAT - 0xFF00] = (val & 0x78) | (memory->io[_STAT - 0xFF00] & 0x07);
        uint8_t s = memory->io[_STAT - 0xFF00];
        bool cur = get_stat_line(s);
        if (cur && !prev_stat_line)
            memory->io[_IF - 0xFF00] |= 0x02;
        prev_stat_line = cur;
    } else if (addr == _LY) {
        return;
    } else if (addr == _DMA) {
        dma_start(val);
    } else {
        memory->io[addr - 0xFF00] = val;
    }
}
