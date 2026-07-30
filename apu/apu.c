#include "apu/apu.h"
#include "core/emulator_core.h"
#include <string.h>

APU *apu;

static const uint8_t duty[4][8] = {
    {0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,1},
    {1,0,0,0,0,1,1,1},
    {0,1,1,1,1,1,1,0},
};

static uint16_t get_period(int ch) {
    uint16_t freq;
    switch (ch) {
        case 0: freq = apu->ch1_freq_latched; break;
        case 1: freq = apu->ch2_freq_latched; break;
        default: freq = apu->ch3_freq_latched; break;
    }
    return (2048 - freq) * (ch == 2 ? 2 : 4);
}

static void clock_length(int ch, uint16_t *len, uint8_t *enabled) {
    if (*len > 0) {
        (*len)--;
        if (*len == 0) {
            *enabled = 0;
            memory->io[_NR52 - 0xFF00] &= ~(1 << ch);
        }
    }
}

static void clock_sweep(void) {
    if (!apu->ch1_sweep_enabled) return;
    if (apu->ch1_sweep_timer > 0) {
        apu->ch1_sweep_timer--;
        if (apu->ch1_sweep_timer == 0) {
            uint8_t pace = (memory->io[_NR10 - 0xFF00] >> 4) & 0x07;
            if (pace > 0) {
                apu->ch1_sweep_timer = pace;
                uint8_t shift = memory->io[_NR10 - 0xFF00] & 0x07;
                uint8_t dir = (memory->io[_NR10 - 0xFF00] >> 3) & 0x01;
                uint16_t new_freq;
                if (dir == 0) {
                    new_freq = apu->ch1_sweep_shadow + (apu->ch1_sweep_shadow >> shift);
                } else {
                    new_freq = apu->ch1_sweep_shadow - (apu->ch1_sweep_shadow >> shift);
                }
                apu->ch1_sweep_shadow = new_freq;
                memory->io[_NR13 - 0xFF00] = new_freq & 0xFF;
                memory->io[_NR14 - 0xFF00] = (memory->io[_NR14 - 0xFF00] & ~0x07) | ((new_freq >> 8) & 0x07);
                if (new_freq > 2047) {
                    apu->ch1_enabled = 0;
                    memory->io[_NR52 - 0xFF00] &= ~1;
                }
            }
        }
    }
}

static void clock_envelope(int ch) {
    uint8_t *vol, *pace;
    uint16_t nrx2_addr;
    switch (ch) {
        case 0: vol = &apu->ch1_volume; pace = &apu->ch1_env_pace; nrx2_addr = _NR12; break;
        case 1: vol = &apu->ch2_volume; pace = &apu->ch2_env_pace; nrx2_addr = _NR22; break;
        default: vol = &apu->ch4_volume; pace = &apu->ch4_env_pace; nrx2_addr = _NR42; break;
    }
    uint8_t reg = memory->io[nrx2_addr - 0xFF00];
    uint8_t p = reg & 0x07;
    if (p == 0) return;
    if (*pace > 0) {
        (*pace)--;
        if (*pace == 0) {
            *pace = p;
            uint8_t dir = (reg >> 3) & 0x01;
            if (dir) {
                if (*vol < 15) (*vol)++;
            } else {
                if (*vol > 0) (*vol)--;
            }
        }
    }
}

static void trigger_ch1(void) {
    apu->ch1_enabled = 1;
    memory->io[_NR52 - 0xFF00] |= 1;
    uint8_t len = memory->io[_NR11 - 0xFF00] & 0x3F;
    apu->ch1_length = 64 - len;
    apu->ch1_freq_latched = (memory->io[_NR14 - 0xFF00] & 0x07) << 8 | memory->io[_NR13 - 0xFF00];
    uint16_t freq = get_period(0);
    apu->ch1_freq_timer = freq;
    uint8_t reg = memory->io[_NR12 - 0xFF00];
    uint8_t init_vol = reg >> 4;
    uint8_t dir = (reg >> 3) & 0x01;
    apu->ch1_dac_enabled = (init_vol > 0 || dir == 1);
    if (!apu->ch1_dac_enabled) { apu->ch1_enabled = 0; memory->io[_NR52 - 0xFF00] &= ~1; return; }
    apu->ch1_volume = init_vol;
    apu->ch1_env_pace = reg & 0x07;
    apu->ch1_env_timer = reg & 0x07;
    apu->ch1_sweep_shadow = freq;
    uint8_t pace = (memory->io[_NR10 - 0xFF00] >> 4) & 0x07;
    apu->ch1_sweep_timer = pace;
    apu->ch1_sweep_enabled = (pace > 0 || (memory->io[_NR10 - 0xFF00] & 0x07) > 0);
    uint8_t shift = memory->io[_NR10 - 0xFF00] & 0x07;
    uint8_t dir_sweep = (memory->io[_NR10 - 0xFF00] >> 3) & 0x01;
    uint16_t delta = apu->ch1_sweep_shadow >> shift;
    uint16_t new_freq;
    if (dir_sweep == 0)
        new_freq = apu->ch1_sweep_shadow + delta;
    else
        new_freq = apu->ch1_sweep_shadow - delta;
    if (new_freq > 2047) {
        apu->ch1_enabled = 0;
        memory->io[_NR52 - 0xFF00] &= ~1;
    }
}

static void trigger_ch2(void) {
    apu->ch2_enabled = 1;
    memory->io[_NR52 - 0xFF00] |= 2;
    uint8_t len = memory->io[_NR21 - 0xFF00] & 0x3F;
    apu->ch2_length = 64 - len;
    apu->ch2_freq_latched = (memory->io[_NR24 - 0xFF00] & 0x07) << 8 | memory->io[_NR23 - 0xFF00];
    uint16_t freq = get_period(1);
    apu->ch2_freq_timer = freq;
    uint8_t reg = memory->io[_NR22 - 0xFF00];
    uint8_t init_vol = reg >> 4;
    uint8_t dir = (reg >> 3) & 0x01;
    apu->ch2_dac_enabled = (init_vol > 0 || dir == 1);
    if (!apu->ch2_dac_enabled) { apu->ch2_enabled = 0; memory->io[_NR52 - 0xFF00] &= ~2; return; }
    apu->ch2_volume = init_vol;
    apu->ch2_env_pace = reg & 0x07;
    apu->ch2_env_timer = reg & 0x07;
}

static void trigger_ch3(void) {
    apu->ch3_dac_enabled = (memory->io[_NR30 - 0xFF00] >> 7) & 1;
    if (!apu->ch3_dac_enabled) { apu->ch3_enabled = 0; memory->io[_NR52 - 0xFF00] &= ~4; return; }
    apu->ch3_enabled = 1;
    memory->io[_NR52 - 0xFF00] |= 4;
    apu->ch3_length = 256 - memory->io[_NR31 - 0xFF00];
    apu->ch3_freq_latched = (memory->io[_NR34 - 0xFF00] & 0x07) << 8 | memory->io[_NR33 - 0xFF00];
    uint16_t freq = get_period(2);
    apu->ch3_freq_timer = freq;
    apu->ch3_wave_pos = 0;
    uint8_t byte = apu->wave_ram[0];
    apu->ch3_sample = (apu->ch3_wave_pos & 1) ? (byte & 0x0F) : (byte >> 4);
}

static void trigger_ch4(void) {
    apu->ch4_enabled = 1;
    memory->io[_NR52 - 0xFF00] |= 8;
    uint8_t len = memory->io[_NR41 - 0xFF00] & 0x3F;
    apu->ch4_length = 64 - len;
    uint8_t reg = memory->io[_NR42 - 0xFF00];
    uint8_t init_vol = reg >> 4;
    uint8_t dir = (reg >> 3) & 0x01;
    apu->ch4_dac_enabled = (init_vol > 0 || dir == 1);
    if (!apu->ch4_dac_enabled) { apu->ch4_enabled = 0; memory->io[_NR52 - 0xFF00] &= ~8; return; }
    apu->ch4_volume = init_vol;
    apu->ch4_env_pace = reg & 0x07;
    apu->ch4_env_timer = reg & 0x07;
    apu->ch4_lfsr = 0x7FFF;
    uint8_t nr43 = memory->io[_NR43 - 0xFF00];
    uint8_t shift = (nr43 >> 4) & 0x0F;
    uint8_t divisor_code = nr43 & 0x07;
    uint32_t divisor = (divisor_code == 0) ? 8 : divisor_code * 16;
    apu->ch4_freq_timer = divisor << shift;
}

void apu_init(void) {
    apu = calloc(1, sizeof(APU));
    apu->ch4_lfsr = 0x7FFF;
    apu->apu_enabled = (memory->io[_NR52 - 0xFF00] >> 7) & 1;
}

void apu_write_io(uint16_t addr, uint8_t val) {
    uint16_t idx = addr - 0xFF00;

    if (addr == _NR52) {
        if (!(val & 0x80)) {
            memset(memory->io + 0x10, 0, 0x16);
            memory->io[_NR52 - 0xFF00] = 0x00;
            memset(apu, 0, sizeof(APU));
            apu->ch4_lfsr = 0x7FFF;
        } else {
            memory->io[_NR52 - 0xFF00] = (val & 0xF0) | (memory->io[_NR52 - 0xFF00] & 0x0F);
            apu->apu_enabled = 1;
            apu->system_divider = 0;
            apu->frame_step = 0;
        }
        return;
    }

    if (!apu->apu_enabled) {
        if (addr >= 0xFF30 && addr <= 0xFF3F) {
            apu->wave_ram[addr - 0xFF30] = val;
        }
        return;
    }

    uint8_t old_val = memory->io[idx];
    memory->io[idx] = val;

    if (addr >= _NR10 && addr <= _NR14) {
        if (addr == _NR14) {
            apu->ch1_freq_latched = (val & 0x07) << 8 | memory->io[_NR13 - 0xFF00];
            if (val & 0x80) trigger_ch1();
            uint8_t prev_bit6 = (old_val >> 6) & 1;
            uint8_t new_bit6 = (val >> 6) & 1;
            if (!prev_bit6 && new_bit6 && (apu->frame_step & 1)) {
                if (apu->ch1_length > 0) {
                    apu->ch1_length--;
                    if (apu->ch1_length == 0) {
                        apu->ch1_enabled = 0;
                        memory->io[_NR52 - 0xFF00] &= ~1;
                    }
                }
            }
        }
        if (addr == _NR10) {
            uint8_t pace = (val >> 4) & 0x07;
            uint8_t shift = val & 0x07;
            if (pace == 0 && shift == 0) apu->ch1_sweep_enabled = 0;
        }
    } else if (addr >= _NR21 && addr <= _NR24) {
        if (addr == _NR24) {
            apu->ch2_freq_latched = (val & 0x07) << 8 | memory->io[_NR23 - 0xFF00];
            if (val & 0x80) trigger_ch2();
            uint8_t prev_bit6 = (old_val >> 6) & 1;
            uint8_t new_bit6 = (val >> 6) & 1;
            if (!prev_bit6 && new_bit6 && (apu->frame_step & 1)) {
                if (apu->ch2_length > 0) {
                    apu->ch2_length--;
                    if (apu->ch2_length == 0) {
                        apu->ch2_enabled = 0;
                        memory->io[_NR52 - 0xFF00] &= ~2;
                    }
                }
            }
        }
    } else if (addr >= _NR30 && addr <= _NR34) {
        if (addr == _NR34) {
            apu->ch3_freq_latched = (val & 0x07) << 8 | memory->io[_NR33 - 0xFF00];
            if (val & 0x80) trigger_ch3();
            uint8_t prev_bit6 = (old_val >> 6) & 1;
            uint8_t new_bit6 = (val >> 6) & 1;
            if (!prev_bit6 && new_bit6 && (apu->frame_step & 1)) {
                if (apu->ch3_length > 0) {
                    apu->ch3_length--;
                    if (apu->ch3_length == 0) {
                        apu->ch3_enabled = 0;
                        memory->io[_NR52 - 0xFF00] &= ~4;
                    }
                }
            }
        }
        apu->ch3_dac_enabled = (memory->io[_NR30 - 0xFF00] >> 7) & 1;
        if (!apu->ch3_dac_enabled && apu->ch3_enabled) {
            apu->ch3_enabled = 0;
            memory->io[_NR52 - 0xFF00] &= ~4;
        }
    } else if (addr >= _NR41 && addr <= _NR44) {
        if (addr == _NR44 && (val & 0x80)) trigger_ch4();
        if (addr == _NR44) {
            uint8_t prev_bit6 = (old_val >> 6) & 1;
            uint8_t new_bit6 = (val >> 6) & 1;
            if (!prev_bit6 && new_bit6 && (apu->frame_step & 1)) {
                if (apu->ch4_length > 0) {
                    apu->ch4_length--;
                    if (apu->ch4_length == 0) {
                        apu->ch4_enabled = 0;
                        memory->io[_NR52 - 0xFF00] &= ~8;
                    }
                }
            }
        }
    } else if (addr == _NR50 || addr == _NR51) {
    }
}

uint8_t apu_read_wave(uint16_t addr) {
    if (!apu->apu_enabled) return apu->wave_ram[addr - 0xFF30];
    if (apu->ch3_dac_enabled && apu->ch3_enabled) return 0xFF;
    return apu->wave_ram[addr - 0xFF30];
}

void apu_write_wave(uint16_t addr, uint8_t val) {
    if (!apu->apu_enabled) { apu->wave_ram[addr - 0xFF30] = val; return; }
    if (apu->ch3_dac_enabled && apu->ch3_enabled) return;
    apu->wave_ram[addr - 0xFF30] = val;
    memory->io[addr - 0xFF00] = val;
}

static void mix(int16_t *left, int16_t *right) {
    int ch1_out = 0, ch2_out = 0, ch3_out = 0, ch4_out = 0;
    if (apu->ch1_enabled && apu->ch1_dac_enabled) {
        uint8_t duty_code = (memory->io[_NR11 - 0xFF00] >> 6) & 0x03;
        ch1_out = duty[duty_code][apu->ch1_duty_pos] ? apu->ch1_volume : 0;
    }
    if (apu->ch2_enabled && apu->ch2_dac_enabled) {
        uint8_t duty_code = (memory->io[_NR21 - 0xFF00] >> 6) & 0x03;
        ch2_out = duty[duty_code][apu->ch2_duty_pos] ? apu->ch2_volume : 0;
    }
    if (apu->ch3_enabled && apu->ch3_dac_enabled) {
        uint8_t vol_code = (memory->io[_NR32 - 0xFF00] >> 5) & 0x03;
        uint8_t s = apu->ch3_sample;
        switch (vol_code) {
            case 0: s = 0; break;
            case 1: break;
            case 2: s >>= 1; break;
            case 3: s >>= 2; break;
        }
        ch3_out = s;
    }
    if (apu->ch4_enabled && apu->ch4_dac_enabled) {
        ch4_out = (~apu->ch4_lfsr & 1) ? apu->ch4_volume : 0;
    }
    uint8_t nr51 = memory->io[_NR51 - 0xFF00];
    uint8_t nr50 = memory->io[_NR50 - 0xFF00];
    int l_vol = ((nr50 >> 4) & 0x07);
    int r_vol = (nr50 & 0x07);
    int l_sum = 0, r_sum = 0;
    if (nr51 & 0x10) l_sum += ch1_out;
    if (nr51 & 0x20) l_sum += ch2_out;
    if (nr51 & 0x40) l_sum += ch3_out;
    if (nr51 & 0x80) l_sum += ch4_out;
    if (nr51 & 0x01) r_sum += ch1_out;
    if (nr51 & 0x02) r_sum += ch2_out;
    if (nr51 & 0x04) r_sum += ch3_out;
    if (nr51 & 0x08) r_sum += ch4_out;
    if (l_vol == 0) l_sum = 0;
    if (r_vol == 0) r_sum = 0;

    int32_t raw_l = (l_sum * 2 - 60) * 546 * l_vol / 8;
    int32_t raw_r = (r_sum * 2 - 60) * 546 * r_vol / 8;

    int32_t out_l, out_r;
    if (l_sum == 0 && r_sum == 0) {
        out_l = 0;
        out_r = 0;
    } else {
        out_l = raw_l - apu->hp_cap_l;
        out_r = raw_r - apu->hp_cap_r;
        apu->hp_cap_l += out_l / 1024;
        apu->hp_cap_r += out_r / 1024;
    }

    if (out_l < -32768) out_l = -32768;
    else if (out_l > 32767) out_l = 32767;
    if (out_r < -32768) out_r = -32768;
    else if (out_r > 32767) out_r = 32767;
    *left  = (int16_t)out_l;
    *right = (int16_t)out_r;
}

void apu_step(uint16_t cycles) {
    if (!apu->apu_enabled) return;

    uint16_t prev = apu->system_divider;
    apu->system_divider += cycles;

    if ((prev & 0x1000) && !(apu->system_divider & 0x1000)) {
        uint8_t step = (apu->frame_step + 1) & 7;
        apu->frame_step = step;
        if ((step & 1) == 0) {
            uint8_t len_en1 = (memory->io[_NR14 - 0xFF00] >> 6) & 1;
            uint8_t len_en2 = (memory->io[_NR24 - 0xFF00] >> 6) & 1;
            uint8_t len_en3 = (memory->io[_NR34 - 0xFF00] >> 6) & 1;
            uint8_t len_en4 = (memory->io[_NR44 - 0xFF00] >> 6) & 1;
            if (len_en1) clock_length(0, &apu->ch1_length, &apu->ch1_enabled);
            if (len_en2) clock_length(1, &apu->ch2_length, &apu->ch2_enabled);
            if (len_en3) clock_length(2, &apu->ch3_length, &apu->ch3_enabled);
            if (len_en4) clock_length(3, &apu->ch4_length, &apu->ch4_enabled);
        }
        if (step == 2 || step == 6) clock_sweep();
        if (step == 7) {
            clock_envelope(0);
            clock_envelope(1);
            clock_envelope(3);
        }
    }

    if (apu->ch1_enabled) {
        uint16_t period = get_period(0);
        apu->ch1_freq_timer -= cycles;
        while ((int16_t)apu->ch1_freq_timer <= 0) {
            apu->ch1_freq_timer += period;
            apu->ch1_duty_pos = (apu->ch1_duty_pos - 1) & 7;
        }
    }
    if (apu->ch2_enabled) {
        uint16_t period = get_period(1);
        apu->ch2_freq_timer -= cycles;
        while ((int16_t)apu->ch2_freq_timer <= 0) {
            apu->ch2_freq_timer += period;
            apu->ch2_duty_pos = (apu->ch2_duty_pos - 1) & 7;
        }
    }
    if (apu->ch3_enabled) {
        uint16_t period = get_period(2);
        apu->ch3_freq_timer -= cycles;
        while ((int16_t)apu->ch3_freq_timer <= 0) {
            apu->ch3_freq_timer += period;
            apu->ch3_wave_pos = (apu->ch3_wave_pos + 1) & 31;
            uint8_t byte = apu->wave_ram[apu->ch3_wave_pos >> 1];
            apu->ch3_sample = (apu->ch3_wave_pos & 1) ? (byte & 0x0F) : (byte >> 4);
        }
    }
    if (apu->ch4_enabled) {
        uint8_t nr43 = memory->io[_NR43 - 0xFF00];
        uint8_t shift = (nr43 >> 4) & 0x0F;
        uint8_t div_code = nr43 & 0x07;
        uint32_t divisor = (div_code == 0) ? 8 : div_code * 16;
        uint32_t period = divisor << shift;
        apu->ch4_freq_timer -= cycles;
        while ((int32_t)apu->ch4_freq_timer <= 0) {
            apu->ch4_freq_timer += period;
            uint8_t xor_bit = ((apu->ch4_lfsr ^ (apu->ch4_lfsr >> 1)) & 1);
            apu->ch4_lfsr >>= 1;
            apu->ch4_lfsr |= (xor_bit << 14);
            if (nr43 & 0x08)
                apu->ch4_lfsr = (apu->ch4_lfsr & ~0x40) | (xor_bit << 6);
        }
    }

    apu->cycle_acc += cycles;
    while (apu->cycle_acc >= AUDIO_CYCLES_PER_SAMPLE) {
        apu->cycle_acc -= AUDIO_CYCLES_PER_SAMPLE;
        int16_t l, r;
        mix(&l, &r);
        apu->sample_buf[apu->sample_wpos] = l;
        apu->sample_buf[apu->sample_wpos + 1] = r;
        uint16_t next = (apu->sample_wpos + 2) % (AUDIO_BUFFER_SAMPLES * 2);
        if (next == apu->sample_rpos)
            apu->sample_rpos = (apu->sample_rpos + 2) % (AUDIO_BUFFER_SAMPLES * 2);
        apu->sample_wpos = next;
    }
}

uint16_t apu_read_samples(int16_t *buf, uint16_t max) {
    uint16_t count = 0;
    while (apu->sample_rpos != apu->sample_wpos && count < max * 2) {
        buf[count++] = apu->sample_buf[apu->sample_rpos];
        apu->sample_rpos = (apu->sample_rpos + 1) % (AUDIO_BUFFER_SAMPLES * 2);
    }
    return count / 2;
}
