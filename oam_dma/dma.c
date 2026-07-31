#include "oam_dma/dma.h"
#include "core/bus.h"
#include "core/emulator_core.h"

void dma_start(uint8_t val) {
    uint16_t src = val << 8;
    for (int i = 0; i < 0xA0; i++) {
        memory->oam[i] = bus_read(src + i);
        update_timers(4);
    }
}
