#include "core/emulator_core.h"
#include "core/bus.h"
#include "ppu/ppu.h"

GameBoyMemory *memory;
registers *reg;
uint32_t div_counter = 0;
uint32_t tima_accumulator = 0;
int8_t ime_next = -1;
bool ei = false, ime = false;
uint16_t tima_overflow_cycles = 0;
bool tima_write_during_overflow = false;
uint8_t opcode;

void update_timers(uint16_t cycles) {
    static const uint8_t bitPos[] = {9, 3, 5, 7};
    uint8_t tac = memory->io[_TAC - 0xFF00];
    if (tima_overflow_cycles > 0) {
        if (tima_overflow_cycles <= cycles) {
            tima_overflow_cycles = 0;
            if (!tima_write_during_overflow)
                memory->io[_TIMA - 0xFF00] = memory->io[_TMA - 0xFF00];
            memory->io[_IF - 0xFF00] |= 0x04;
            tima_write_during_overflow = false;
        } else {
            tima_overflow_cycles -= cycles;
        }
    }
    if (tac & 0x04) {
        uint8_t bit = bitPos[tac & 0x03];
        uint16_t mask = 1 << bit;
        uint16_t period = mask << 1;

        tima_accumulator += cycles;
        while (tima_accumulator >= period) {
            tima_accumulator -= period;
            uint8_t tima = memory->io[_TIMA - 0xFF00] + 1;
            if (tima == 0) {
                tima = 0x00;
                tima_overflow_cycles = 4;
            }
            memory->io[_TIMA - 0xFF00] = tima;
        }
    }

    apu_step(cycles);

    div_counter += cycles;
    update_ppu(cycles);
    memory->io[_DIV - 0xff00] = (div_counter >> 8) & 0xFF;
}

uint8_t read_byte(uint16_t address) {
    update_timers(4);
    return bus_read(address);
}

void save_byte(uint16_t address, uint8_t val){
    update_timers(4);
    bus_write(address, val);
}

void init_io_ports(void) {
    memory->io[_JOYP - 0xFF00] = 0x30;
    memory->io[_STAT - 0xFF00] = 0x00;

    memory->io[_TIMA - 0xFF00] = 0x00;
    memory->io[_TMA  - 0xFF00] = 0x00;
    memory->io[_TAC  - 0xFF00] = 0x00;

    memory->io[_NR10 - 0xFF00] = 0x80;
    memory->io[_NR11 - 0xFF00] = 0xBF;
    memory->io[_NR12 - 0xFF00] = 0xF3;
    memory->io[_NR14 - 0xFF00] = 0x3F;

    memory->io[_NR21 - 0xFF00] = 0x3F;
    memory->io[_NR22 - 0xFF00] = 0x00;
    memory->io[_NR24 - 0xFF00] = 0x3F;

    memory->io[_NR30 - 0xFF00] = 0x7F;
    memory->io[_NR31 - 0xFF00] = 0xFF;
    memory->io[_NR32 - 0xFF00] = 0x9F;
    memory->io[_NR33 - 0xFF00] = 0x3F;

    memory->io[_NR41 - 0xFF00] = 0xFF;
    memory->io[_NR42 - 0xFF00] = 0x00;
    memory->io[_NR43 - 0xFF00] = 0x00;
    memory->io[_NR44 - 0xFF00] = 0x3F;

    memory->io[_NR50 - 0xFF00] = 0x77;
    memory->io[_NR51 - 0xFF00] = 0xF3;
    memory->io[_NR52 - 0xFF00] = 0xF0;

    memory->io[_LCDC - 0xFF00] = 0x91;
    memory->io[_SCY  - 0xFF00] = 0x00;
    memory->io[_SCX  - 0xFF00] = 0x00;
    memory->io[_LYC  - 0xFF00] = 0x00;
    memory->io[_BGP  - 0xFF00] = 0xFC;
    memory->io[_OBP0 - 0xFF00] = 0xFF;
    memory->io[_OBP1 - 0xFF00] = 0xFF;
    memory->io[_WY   - 0xFF00] = 0x00;
    memory->io[_WX   - 0xFF00] = 0x00;

    memory->io[_IF   - 0xFF00] = 0x00;
    memory->ie                  = 0x00;
}

void handle_interrupts() {
    uint8_t pending = memory->io[_IF - 0xFF00] & memory->ie & 0x1F;
    if (!pending) return;

    static const uint16_t vectors[] = {0x40, 0x48, 0x50, 0x58, 0x60};
    for (int i = 0; i < 5; i++) {
        uint8_t bit = 1 << i;
        if (pending & bit) {
            ime = false;
            ime_next = -1;
            memory->io[_IF - 0xFF00] &= ~bit;
            update_timers(4);
            save_byte(--reg->sp, (reg->pc >> 8) & 0xFF);
            save_byte(--reg->sp, reg->pc & 0xFF);
            update_timers(8);

            reg->pc = vectors[i];
            break;
        }
    }
}
