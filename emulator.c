#include "functions.h"
#include <stdbool.h>

extern GameBoyMemory *memory;
extern registers *reg;

int main(int argc, char *argv[]) {
    memory = malloc(GAMEBOY_MEMORY_SIZE);
    reg = malloc(11 * sizeof(registers));
    reg->pc = INIT_PC;
    if(argc < 2) {
        printf("Usage: %s <path_to_rom>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        printf("Error: Could not open ROM file %s\n", argv[1]);
        return 1;
    }

    fread(memory->rom, 1, 0x8000, file);
    fclose(file);

    bool go = true;
    while(go){
        uint8_t opcode = read_byte(reg->pc);

        printf("PC: 0x%04X | Opcode: 0x%02X\n", reg->pc, opcode);

        reg->pc++;

        if (opcode == 0xCB) {
            uint8_t cb_opcode = read_byte(reg->pc);
            reg->pc++;
            prefix_opcode_table[cb_opcode](); 
        } else {
            if (opcode_table[opcode] != NULL) {
                opcode_table[opcode]();
            } else {
                printf("Error: Unimplemented Opcode 0x%02X at 0x%04X\n", opcode, reg->pc - 1);
                break;
            }
        }
    }
    exit(0);
}
