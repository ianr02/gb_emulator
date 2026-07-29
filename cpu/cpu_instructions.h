#ifndef CPU_INSTRUCTIONS_H
#define CPU_INSTRUCTIONS_H

#include "src/structs.h"

extern bool prefix_flag;
extern bool halt_bug;

extern instruction_ptr opcode_table[256];
extern instruction_ptr prefix_opcode_table[256];

void prefix_function(void);

#endif
