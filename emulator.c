#include "instructions.h"

extern GameBoyMemory *memory;
extern registers *reg;
uint8_t opcode;

int main(int argc, char *argv[]) {
    if(argc < 2) {
        printf("Usage: %s <path_to_rom>\n", argv[0]);
        return 1;
    }
    memory = malloc(GAMEBOY_MEMORY_SIZE);
    reg = malloc(sizeof(registers));
    init_io_ports();
    reg->pc = INIT_PC;
    reg->af = 0x01B0;
    reg->bc = 0x0013;
    reg->de = 0x00D8;
    reg->hl = 0x014D;
    reg->sp = 0xFFFE; 

    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        printf("Error: Could not open ROM file %s\n", argv[1]);
        return 1;
    }

    fread(memory->rom, 1, 0x8000, file);
    fclose(file);

    bool go = true;
    while(go){
         if (ime_next >= 0) {
            ime_next--;
            if (ime_next == 0) {
                ime = ei;            
                ime_next = -1; // Reset tracker
            }
        }
        if (ime) {
            // check_and_handle_interrupts();
        }
        opcode = read_byte(reg->pc);
        printf("PC: 0x%04X | Opcode: 0x%02X\n", reg->pc, opcode);
        ++reg->pc;
        if (opcode_table[opcode] != NULL) {
            opcode_table[opcode]();
        } else {
            printf("Error: Unimplemented Opcode 0x%02X at 0x%04X\n", opcode, reg->pc - 1);
            exit(EXIT_FAILURE);
        }
    }
    fclose(file);
    free(memory);
    free(reg);
    exit(EXIT_SUCCESS);
}
