#ifndef APU_H
#define APU_H

#include "src/structs.h"

#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_CYCLES_PER_SAMPLE (4194304 / 44100)
#define AUDIO_BUFFER_SAMPLES 4096

typedef struct {
    uint16_t ch1_freq_timer;
    uint16_t ch1_freq_latched;
    uint8_t  ch1_duty_pos;
    uint8_t  ch1_volume;
    uint16_t ch1_length;
    uint8_t  ch1_enabled;
    uint8_t  ch1_dac_enabled;
    uint8_t  ch1_len_enabled;
    uint8_t  ch1_env_pace;
    uint8_t  ch1_triggered_once;

    uint16_t ch1_sweep_shadow;
    uint8_t  ch1_sweep_timer;
    uint8_t  ch1_sweep_enabled;

    uint16_t ch2_freq_timer;
    uint16_t ch2_freq_latched;
    uint8_t  ch2_duty_pos;
    uint8_t  ch2_volume;
    uint16_t ch2_length;
    uint8_t  ch2_enabled;
    uint8_t  ch2_dac_enabled;
    uint8_t  ch2_len_enabled;
    uint8_t  ch2_env_pace;
    uint8_t  ch2_triggered_once;

    uint16_t ch3_freq_timer;
    uint16_t ch3_freq_latched;
    uint8_t  ch3_wave_pos;
    uint8_t  ch3_sample;
    uint16_t ch3_length;
    uint8_t  ch3_enabled;
    uint8_t  ch3_dac_enabled;
    uint8_t  ch3_len_enabled;

    uint16_t ch4_lfsr;
    uint32_t ch4_freq_timer;
    uint8_t  ch4_volume;
    uint16_t ch4_length;
    uint8_t  ch4_enabled;
    uint8_t  ch4_dac_enabled;
    uint8_t  ch4_len_enabled;
    uint8_t  ch4_env_pace;

    uint8_t  wave_ram[16];

    uint16_t system_divider;
    uint8_t  frame_step;
    uint8_t  apu_enabled;
    uint16_t last_len_clock_div;
    uint8_t  len_phase_counter;
    uint8_t  _pad[1];

    uint32_t cycle_acc;
    int32_t  hp_cap_l;
    int32_t  hp_cap_r;
    int16_t  sample_buf[AUDIO_BUFFER_SAMPLES * 2];
    uint16_t sample_wpos;
    uint16_t sample_rpos;
} APU;

extern APU *apu;

void apu_init(void);
void apu_step(uint16_t cycles);
void apu_div_write(void);
void apu_write_io(uint16_t addr, uint8_t val);
uint8_t apu_read_wave(uint16_t addr);
void apu_write_wave(uint16_t addr, uint8_t val);
uint16_t apu_read_samples(int16_t *buf, uint16_t max);

#endif
