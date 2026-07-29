#include "core/emulator_core.h"
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

    for (int ch = 0; ch < 4; ch++) {
        if (memory->apu_ch_remaining[ch] > 0 && memory->apu_ch_remaining[ch] != 0xFFFFFFFF) {
            if (memory->apu_ch_remaining[ch] <= cycles) {
                memory->apu_ch_remaining[ch] = 0;
                memory->io[_NR52 - 0xFF00] &= ~(1 << ch);
            } else {
                memory->apu_ch_remaining[ch] -= cycles;
            }
        }
    }

    div_counter += cycles;
    update_ppu(cycles);
    memory->io[_DIV - 0xff00] = (div_counter >> 8) & 0xFF;
}

uint8_t read_byte(uint16_t address) {
    uint8_t stat_mode = memory->io[_STAT - 0xFF00] & 0x03;
    update_timers(4);
    if (address <= 0x3FFF) {
        return memory->rom[address];
    } else if (address >= 0x4000 && address <= 0x7FFF) {
        uint8_t rval = memory->rom[(memory->rom_bank * 0x4000) + (address - 0x4000)];
        return rval;
    } else if (address >= 0x8000 && address <= 0x9FFF) {
        if (stat_mode == 3) return 0xFF;
        return memory->vram[address - 0x8000];
    } else if (address >= 0xA000 && address <= 0xBFFF) {
        switch (memory->cart_type) {
            case CART_MBC2:
                if (address <= 0xA1FF)
                    return memory->external[address - 0xA000] | 0xF0;
                return 0xFF;

            case CART_MBC3:
                if (memory->ram_bank >= 0x08 && memory->ram_bank <= 0x0C) {
                    if (memory->ram_enable)
                        return memory->rtc_regs[memory->ram_bank - 0x08];
                    return 0xFF;
                }
            default: {
                size_t idx = (memory->ram_bank * 0x2000) + (address - 0xA000);
                if (idx < sizeof(memory->external))
                    return memory->external[idx];
                return 0xFF;
            }
        }
    } else if (address >= 0xC000 && address <= 0xDFFF) {
        return memory->wram[address - 0xC000];
    } else if (address >= 0xE000 && address <= 0xFDFF) {
        return memory->wram[address - 0xE000];
    } else if (address >= 0xFE00 && address <= 0xFE9F) {
        if (stat_mode >= 2) return 0xFF;
        return memory->oam[address - 0xFE00];
    } else if (address >= 0xFF00 && address <= 0xFF7F){
        if (address == _JOYP) {
            uint8_t val = 0xC0 | (memory->io[0] & 0x30);
            uint8_t both = 0x0F;
            if (!(memory->io[0] & 0x10)) both &= joypad_dpad;
            if (!(memory->io[0] & 0x20)) both &= joypad_btn;
            val |= both;
            return val;
        } else if (address == _NR52)
            return memory->io[_NR52 - 0xFF00];
        else
            return memory->io[address - 0xFF00];
    } else if (address >= 0xFF80 && address <= 0xFFFE)
        return memory->hram[address - 0xFF80];
    else if (address == 0xFFFF)
        return memory->ie;
    else
        return 0xFF;
}

void save_byte(uint16_t address, uint8_t val){
    uint8_t stat_mode = memory->io[_STAT - 0xFF00] & 0x03;
    update_timers(4);
    if (address <= 0x3FFF) {
        if (memory->cart_type == CART_MBC2) {
            if (address & 0x0100) {
                uint8_t bank = val & 0x0F;
                if (bank == 0) bank = 1;
                memory->rom_bank = bank;
            } else {
                memory->ram_enable = (val & 0x0F) == 0x0A;
            }
        } else {
            if (address <= 0x1FFF) {
                switch (memory->cart_type) {
                    case CART_ROM_ONLY:
                        break;
                    default:
                        memory->ram_enable = (val & 0x0F) == 0x0A;
                        break;
                }
            } else {
                uint8_t bank;
                switch (memory->cart_type) {
                    case CART_MBC1:
                        bank = val & 0x1F;
                        if (bank == 0) bank = 1;
                        memory->rom_bank = (memory->rom_bank & 0xE0) | bank;
                        break;
                    case CART_MBC3:
                        memory->rom_bank = val & 0x7F;
                        if (memory->rom_bank == 0) memory->rom_bank = 1;
                        break;
                    case CART_MBC5:
                        if (address <= 0x2FFF)
                            memory->rom_bank = (memory->rom_bank & 0x100) | val;
                        else
                            memory->rom_bank = (memory->rom_bank & 0xFF) | ((val & 0x01) << 8);
                        break;
                    default: break;
                }
            }
        }
        return;
    } else if (address >= 0x4000 && address <= 0x5FFF) {
         switch (memory->cart_type) {
            case CART_MBC1:
                if (memory->banking_mode == 0)
                    memory->rom_bank = (memory->rom_bank & 0x1F) | ((val & 0x03) << 5);
                else
                    memory->ram_bank = val & 0x03;
                break;
            case CART_MBC3:
                memory->ram_bank = val & 0x0F;
                break;
            case CART_MBC5:
                memory->ram_bank = val & 0x0F;
                break;
            default: break;
        }
    } else if (address >= 0x6000 && address <= 0x7FFF) {
        switch (memory->cart_type) {
            case CART_MBC1:
                memory->banking_mode = val & 0x01;
                break;
            case CART_MBC3:
                if (memory->rtc_latch_state == 0 && val == 0x00)
                    memory->rtc_latch_state = 1;
                else if (memory->rtc_latch_state == 1 && val == 0x01) {
                    time_t t = time(NULL);
                    struct tm *tm = localtime(&t);
                    memory->rtc_regs[0] = tm->tm_sec;
                    memory->rtc_regs[1] = tm->tm_min;
                    memory->rtc_regs[2] = tm->tm_hour;
                    uint16_t days = t / 86400;
                    memory->rtc_regs[3] = days & 0xFF;
                    memory->rtc_regs[4] = (memory->rtc_regs[4] & 0x80) | ((days >> 8) & 0x01);
                    memory->rtc_latch_state = 0;
                } else
                    memory->rtc_latch_state = 0;
                break;
            default: break;
        }
    } else if (address >= 0x8000 && address <= 0x9FFF) {
        if (stat_mode != 3)
            memory->vram[address - 0x8000] = val;
    } else if (address >= 0xA000 && address <= 0xBFFF) {
        if (memory->ram_enable) {
            switch (memory->cart_type) {
                case CART_ROM_ONLY:
                    break;
                case CART_MBC2:
                    if (address <= 0xA1FF)
                        memory->external[address - 0xA000] = val & 0x0F;
                    break;
                default:
                    if (memory->ram_bank >= 0x08 && memory->ram_bank <= 0x0C) {
                        memory->rtc_regs[memory->ram_bank - 0x08] = val;
                    } else {
                        size_t idx = (memory->ram_bank * 0x2000) + (address - 0xA000);
                        if (idx < sizeof(memory->external))
                            memory->external[idx] = val;
                    }
            }
        }
    } else if (address >= 0xC000 && address <= 0xDFFF) {
        memory->wram[address - 0xC000] = val;
    } else if (address >= 0xE000 && address <= 0xFDFF) {
        memory->wram[address - 0xE000] = val;
    } else if (address >= 0xFE00 && address <= 0xFE9F) {
        if (stat_mode == 2) {
            size_t offset = address - 0xFE00;
            size_t base = offset & 0xFE;
            memory->oam[base] = val;
            memory->oam[base | 1] = val;
        } else if (stat_mode < 2) {
            memory->oam[address - 0xFE00] = val;
        }
    } else if (address >= 0xFF00 && address <= 0xFF7F){
        if (address == _DIV) div_counter = 0;
        else if (address == _LY) return;
        else if (address == _JOYP) memory->io[0] = val & 0x30;
        else if (address == _TIMA) {
            if (tima_overflow_cycles > 0)
                tima_write_during_overflow = true;
            memory->io[_TIMA - 0xFF00] = val;
        } else if (address == _NR52) {
            memory->io[_NR52 - 0xFF00] = (memory->io[_NR52 - 0xFF00] & 0x0F) | (val & 0xF0);
            if (!(val & 0x80))
                for (int i = 0; i < 4; i++) memory->apu_ch_remaining[i] = 0;
        } else if (address == _LCDC) {
            memory->io[_LCDC - 0xFF00] = val;
            if (!(val & 0x80)) {
                memory->io[_LY - 0xFF00] = 0;
                memory->io[_STAT - 0xFF00] &= 0xFC;
            }
        } else if (address == _DMA) {
            uint16_t src = val << 8;
            if (src <= 0x3FFF) {
                memcpy(memory->oam, memory->rom + src, 0xA0);
            } else if (src >= 0x4000 && src <= 0x7FFF) {
                memcpy(memory->oam, memory->rom + (memory->rom_bank * 0x4000) + (src - 0x4000), 0xA0);
            } else if (src >= 0x8000 && src <= 0x9FFF) {
                memcpy(memory->oam, memory->vram + (src - 0x8000), 0xA0);
            } else if (src >= 0xC000 && src <= 0xDFFF) {
                memcpy(memory->oam, memory->wram + (src - 0xC000), 0xA0);
            } else if (src >= 0xE000 && src <= 0xFDFF) {
                memcpy(memory->oam, memory->wram + (src - 0xE000), 0xA0);
            }
            for (int i = 0; i < 0xa0; ++i) update_timers(4);
        } else if (address == _STAT) {
            memory->io[_STAT - 0xFF00] = (val & 0x78) | (memory->io[_STAT - 0xFF00] & 0x07);
            uint8_t s = memory->io[_STAT - 0xFF00];
            bool cur = get_stat_line(s);
            if (cur && !prev_stat_line)
                memory->io[_IF - 0xFF00] |= 0x02;
            prev_stat_line = cur;
        }         else if (address == _NR14 || address == _NR24 || address == _NR33 || address == _NR44) {
            if (val & 0x80) {
                static const uint16_t len_map[] = {_NR11, _NR21, _NR31, _NR41};
                int idx = (address == _NR14) ? 0 : (address == _NR24) ? 1 : (address == _NR33) ? 2 : 3;
                uint8_t len_data = memory->io[len_map[idx] - 0xFF00] & 0x3F;
                uint8_t ticks = 64 - len_data;
                if (val & 0x40)
                    memory->apu_ch_remaining[idx] = ticks * 16384u;
                else
                    memory->apu_ch_remaining[idx] = 0xFFFFFFFF;
                memory->io[_NR52 - 0xFF00] |= (1 << idx);
            }
            memory->io[address - 0xFF00] = val;
        }
        else
            memory->io[address - 0xFF00] = val;
    } else if (address >= 0xFF80 && address <= 0xFFFE){
        memory->hram[address - 0xFF80] = val;
    } else if (address == 0xFFFF){
        memory->ie = val;
    } else {
        return;
    }
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
