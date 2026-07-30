#include "core/bus.h"
#include "core/emulator_core.h"
#include "core/oam.h"
#include "ppu/ppu_memory.h"
#include "apu/apu.h"

uint8_t bus_read(uint16_t address) {
    if (address <= 0x3FFF) {
        return memory->rom[address];
    } else if (address >= 0x4000 && address <= 0x7FFF) {
        return memory->rom[(memory->rom_bank * 0x4000) + (address - 0x4000)];
    } else if (address >= 0x8000 && address <= 0x9FFF) {
        return ppu_read_vram(address);
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
    } else if (address >= 0xFE00 && address <= 0xFEFF) {
        return oam_read(address);
    } else if (address >= 0xFF00 && address <= 0xFF7F){
        if (address == _JOYP) {
            uint8_t val = 0xC0 | (memory->io[0] & 0x30);
            uint8_t both = 0x0F;
            if (!(memory->io[0] & 0x10)) both &= joypad_dpad;
            if (!(memory->io[0] & 0x20)) both &= joypad_btn;
            val |= both;
            return val;
        } else if (address >= 0xFF30 && address <= 0xFF3F)
            return apu_read_wave(address);
        else {
            uint16_t idx = address - 0xFF00;
            uint8_t val = memory->io[idx];
            if (address >= 0xFF10 && address <= 0xFF2F) {
                static const uint8_t masks[32] = {
                    0x80, 0x3F, 0x00, 0xFF, 0xBF, // NR10-NR14
                    0xFF, 0x3F, 0x00, 0xFF, 0xBF, // 0xFF15-NR24
                    0x7F, 0xFF, 0x9F, 0xFF, 0xBF, // NR30-NR34
                    0xFF, 0xFF, 0x00, 0x00, 0xBF, // 0xFF1F-NR44
                    0x00, 0x00, 0x70,              // NR50-NR52
                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF // 0xFF27-0xFF2F
                };
                return val | masks[idx - 0x10];
            }
            return val;
        }
    } else if (address >= 0xFF80 && address <= 0xFFFE)
        return memory->hram[address - 0xFF80];
    else if (address == 0xFFFF)
        return memory->ie;
    else
        return 0xFF;
}

void bus_write(uint16_t address, uint8_t val) {
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
        ppu_write_vram(address, val);
        return;
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
    } else if (address >= 0xFE00 && address <= 0xFEFF) {
        oam_write(address, val);
        return;
    } else if (address >= 0xFF00 && address <= 0xFF7F){
        if (address == _DIV) { div_counter = 0; apu_div_write(); }
        else if (address == _JOYP) memory->io[0] = val & 0x30;
        else if (address == _TIMA) {
            if (tima_overflow_cycles > 0)
                tima_write_during_overflow = true;
            memory->io[_TIMA - 0xFF00] = val;
        } else if (address >= _NR10 && address <= _NR52) {
            apu_write_io(address, val);
        } else if (address >= 0xFF30 && address <= 0xFF3F) {
            apu_write_wave(address, val);
        } else if (address >= _LCDC && address <= _WX) {
            ppu_write_io(address, val);
        } else
            memory->io[address - 0xFF00] = val;
    } else if (address >= 0xFF80 && address <= 0xFFFE){
        memory->hram[address - 0xFF80] = val;
    } else if (address == 0xFFFF){
        memory->ie = val;
    }
}
