#ifndef EMULATOR_CORE_H
#define EMULATOR_CORE_H

#include "src/structs.h"

extern GameBoyMemory *memory;
extern registers *reg;
extern uint32_t div_counter;
extern uint32_t tima_accumulator;
extern int8_t ime_next;
extern bool ei, ime;
extern uint16_t tima_overflow_cycles;
extern bool tima_write_during_overflow;
extern uint8_t opcode;

extern uint8_t joypad_dpad;
extern uint8_t joypad_btn;

void update_timers(uint16_t cycles);
uint8_t read_byte(uint16_t address);
void save_byte(uint16_t address, uint8_t val);
void init_io_ports(void);
void handle_interrupts(void);

#endif
