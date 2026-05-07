#ifndef FUNCTIONS_H
#include "structs.h"
#include <stdio.h>
#include <stdlib.h>
#define FUNCTIONS_H

extern GameBoyMemory *memory;
extern registers *reg;

uint8_t read_byte(uint16_t address) {
    if (address >= 0x0000 && address <= 0x7FFF) {
        return memory->rom[address];
    } else if (address >= 0x8000 && address <= 0x9FFF) {
        return memory->vram[address - 0x8000];
    } else if (address >= 0xA000 && address <= 0xBFFF) {
        return memory->external[address - 0xA000];
    } else if (address >= 0xC000 && address <= 0xDFFF) {
        return memory->wram[address - 0xC000];
    } else if (address >= 0xE000 && address <= 0xFDFF) {
        return memory->wram[address - 0xE000];
    } else if (address >= 0xFE00 && address <= 0xFE9F) {
        return memory->oam[address - 0xFE00];
    } else if (address >= 0xFF00 && address <= 0xFF7F){
        return memory->io[address - 0xFF4C];
    } else if (address >= 0xFF80 && address <= 0xFFFE){
        return memory->hram[address - 0xFF80];
    } else if (address == 0xFFFF){
        return memory->ie;
    }
    return 0xFF;
}

int save_byte(uint16_t address, uint16_t val){
    if (address >= 0x0000 && address <= 0x7FFF) {
        memory->rom[address] = val;
        exit(EXIT_SUCCESS);
    } else if (address >= 0x8000 && address <= 0x9FFF) {
        memory->vram[address - 0x8000] = val;
        exit(EXIT_SUCCESS);
    } else if (address >= 0xA000 && address <= 0xBFFF) {
        memory->external[address - 0xA000] = val;
        exit(EXIT_SUCCESS);
    } else if (address >= 0xC000 && address <= 0xDFFF) {
        memory->wram[address - 0xC000] = val;
        exit(EXIT_SUCCESS);
    } else if (address >= 0xE000 && address <= 0xFDFF) {
        memory->wram[address - 0xE000] = val;
        exit(EXIT_SUCCESS);
    } else if (address >= 0xFE00 && address <= 0xFE9F) {
        memory->oam[address - 0xFE00] = val;
        exit(EXIT_SUCCESS);
    } else if (address >= 0xFF00 && address <= 0xFF7F){
        memory->io[address - 0xFF4C] = val;
        exit(EXIT_SUCCESS);
    } else if (address >= 0xFF80 && address <= 0xFFFE){
        memory->hram[address - 0xFF80] = val;
        exit(EXIT_SUCCESS);
    } else if (address == 0xFFFF){
        memory->ie = val;
        exit(EXIT_SUCCESS);
    }
    exit(EXIT_FAILURE);
}


// load inmediate value into register
#define GEN_LD_N(reg_name) \
void LD_##reg_name##_n() { \
    reg->reg_name = read_byte(reg->pc++); \
} 

// copy value from r2 to r1
#define GEN_LD_R1_R2(r1, r2) \
void LD_##r1##_##r2() { \
    reg->r1 = reg->r2; \
}

// load value from memory in bc into register
#define GEN_REG_BC(reg_name) \
void LD_##reg_name##_bc() { \
    reg->reg_name = read_byte(reg->bc); \
}

// load value from memory in de into register
#define GEN_REG_DE(reg_name) \
void LD_##reg_name##_de() { \
    reg->reg_name = read_byte(reg->de); \
}

// load value from memory in hl into register
#define GEN_REG_HL(reg_name) \
void LD_##reg_name##_hl() { \
    reg->reg_name = read_byte(reg->hl); \
}

// load value from memory in [nn], 16 bit value, into register
#define GEN_REG_NN(reg_name) \
void LD_##reg_name##_nn() { \
    uint16_t address = read_byte(reg->pc++) | (read_byte(reg->pc++) << 8); \
    reg->reg_name = read_byte(address); \
}

// save value from register into memory in [register]
#define GEN_LD_ADDR_R(reg_addr, reg_name) \
void SV_##reg_addr##_##reg_name() { \
    save_byte(reg->reg_addr, reg->reg_name); \
}

// save inmediate value into memory in [register]
#define GEN_LD_ADDR_IMM(reg_addr) \
void SV_##reg_addr##_n() { \
    uint8_t val = read_byte(reg->pc++); \
    save_byte(reg->reg_addr, val); \
}

// load form value in reg2 + 0xFF00 ($FF00) into reg1
#define GEN_LD_REG_REG(reg1, reg2) \
void SLD_##reg1##_##reg2() { \
    reg->reg1 = 0xFF00 |reg->reg2; \
}

// save form value in reg1 into [reg2 + 0xFF00 ($FF00)]
#define GEN_SV_REG_REG(reg1, reg2) \
void SLD_##reg1##_##reg2() { \
    save_byte(0xFF00 | reg->reg2, reg->reg1); \
}

// load inmediate 16 bit value into register of 16 bit
#define GEN_LD_IMM_REG16(reg_name) \
void LD_##reg_name##_nn() { \
    uint16_t value = read_byte(reg->pc++) | (read_byte(reg->pc++) << 8); \
    reg->reg_name = value; \
}

void NOP(){
    
}

// Load value from A into memory in [HL], then increment HL
void LDI_hl_a(){
    save_byte(reg->hl, reg->a);
    reg->hl++;
}

// Save value from A into memory in [0xFF00 + n], where n is an 8 bit immediate value
void SVH_imm_a(){
    uint8_t address = 0xFF00 | read_byte(reg->pc++);
    save_byte(address, reg->a);
}

// Load value from memory in [0xFF00 + n], where n is an 8 bit immediate value, into A
void LDH_imm_a(){
    uint8_t address = 0xFF00 | read_byte(reg->pc++);
    reg->a = read_byte(address);
}

// Save value in HL, being value of SP + n, where n is an 8 bit signed immediate value
void LDHL_sp_n(){
    int8_t n = read_byte(reg->pc++);
    reg->hl = reg->sp + n;
}

// Save SP into [nn], being 16 bit immediate value
void SV_nn_sp(){
    uint16_t address = read_byte(reg->pc++) | (read_byte(reg->pc++) << 8);
    save_byte(address, reg->sp);
}

GEN_REG_BC(a);
GEN_REG_BC(b);
GEN_REG_BC(c);
GEN_REG_BC(d);
GEN_REG_BC(e);
GEN_REG_BC(h);
GEN_REG_BC(l);

GEN_REG_DE(a);
GEN_REG_DE(b);
GEN_REG_DE(c);
GEN_REG_DE(d);
GEN_REG_DE(e);
GEN_REG_DE(h);
GEN_REG_DE(l);

GEN_REG_HL(a);
GEN_REG_HL(b);
GEN_REG_HL(c);
GEN_REG_HL(d);
GEN_REG_HL(e);
GEN_REG_HL(h);
GEN_REG_HL(l);

GEN_REG_NN(a);
GEN_REG_NN(b);
GEN_REG_NN(c);
GEN_REG_NN(d);
GEN_REG_NN(e);
GEN_REG_NN(h);
GEN_REG_NN(l);

GEN_LD_REG_REG(a, c);
GEN_SV_REG_REG(c, a);

GEN_LD_ADDR_R(hl, a);
GEN_LD_ADDR_R(hl, b);
GEN_LD_ADDR_R(hl, c);
GEN_LD_ADDR_R(hl, d);
GEN_LD_ADDR_R(hl, e);
GEN_LD_ADDR_R(hl, h);
GEN_LD_ADDR_R(hl, l);
GEN_LD_ADDR_R(bc, a);
GEN_LD_ADDR_R(bc, b);
GEN_LD_ADDR_R(bc, c);
GEN_LD_ADDR_R(bc, d);
GEN_LD_ADDR_R(bc, e);
GEN_LD_ADDR_R(bc, h);
GEN_LD_ADDR_R(bc, l);
GEN_LD_ADDR_R(de, a);
GEN_LD_ADDR_R(de, b);
GEN_LD_ADDR_R(de, c);
GEN_LD_ADDR_R(de, d);
GEN_LD_ADDR_R(de, e);
GEN_LD_ADDR_R(de, h);
GEN_LD_ADDR_R(de, l);

GEN_LD_ADDR_IMM(hl);
GEN_LD_ADDR_IMM(bc);
GEN_LD_ADDR_IMM(de);
GEN_LD_ADDR_IMM(a);
GEN_LD_ADDR_IMM(b);
GEN_LD_ADDR_IMM(c); 
GEN_LD_ADDR_IMM(d);
GEN_LD_ADDR_IMM(e);
GEN_LD_ADDR_IMM(h);
GEN_LD_ADDR_IMM(l);
GEN_LD_IMM_REG16(bc);
GEN_LD_IMM_REG16(de);
GEN_LD_IMM_REG16(hl);
GEN_LD_IMM_REG16(sp);

GEN_LD_N(a);
GEN_LD_N(b);
GEN_LD_N(c);
GEN_LD_N(d);
GEN_LD_N(e);
GEN_LD_N(h);
GEN_LD_N(l);

GEN_LD_R1_R2(a, a);
GEN_LD_R1_R2(a, b);
GEN_LD_R1_R2(a, c);
GEN_LD_R1_R2(a, d);
GEN_LD_R1_R2(a, e);
GEN_LD_R1_R2(a, h);
GEN_LD_R1_R2(a, l);
GEN_LD_R1_R2(b, a);
GEN_LD_R1_R2(b, b);
GEN_LD_R1_R2(b, c);
GEN_LD_R1_R2(b, d);
GEN_LD_R1_R2(b, e);
GEN_LD_R1_R2(b, h);
GEN_LD_R1_R2(b, l);
GEN_LD_R1_R2(c, a);
GEN_LD_R1_R2(c, b);
GEN_LD_R1_R2(c, c);
GEN_LD_R1_R2(c, d);
GEN_LD_R1_R2(c, e);
GEN_LD_R1_R2(c, h);
GEN_LD_R1_R2(c, l);
GEN_LD_R1_R2(d, a);
GEN_LD_R1_R2(d, b);
GEN_LD_R1_R2(d, c);
GEN_LD_R1_R2(d, d);
GEN_LD_R1_R2(d, e);
GEN_LD_R1_R2(d, h);
GEN_LD_R1_R2(d, l);
GEN_LD_R1_R2(e, a);
GEN_LD_R1_R2(e, b);
GEN_LD_R1_R2(e, c);
GEN_LD_R1_R2(e, d);
GEN_LD_R1_R2(e, e);
GEN_LD_R1_R2(e, h);
GEN_LD_R1_R2(e, l);
GEN_LD_R1_R2(h, a);
GEN_LD_R1_R2(h, b);
GEN_LD_R1_R2(h, c);
GEN_LD_R1_R2(h, d);
GEN_LD_R1_R2(h, e);
GEN_LD_R1_R2(h, h);
GEN_LD_R1_R2(h, l);
GEN_LD_R1_R2(l, a);
GEN_LD_R1_R2(l, b);
GEN_LD_R1_R2(l, c);
GEN_LD_R1_R2(l, d);
GEN_LD_R1_R2(l, e);
GEN_LD_R1_R2(l, h);
GEN_LD_R1_R2(l, l);
GEN_LD_R1_R2(hl, a);
GEN_LD_R1_R2(hl, b);
GEN_LD_R1_R2(hl, c);
GEN_LD_R1_R2(hl, d);
GEN_LD_R1_R2(hl, e);
GEN_LD_R1_R2(hl, h);
GEN_LD_R1_R2(hl, l);
GEN_LD_R1_R2(sp, hl);

instruction_ptr opcode_table[256] = {
    // 0x0_
    [0x00] = NOP, // NOP
    [0x01] = LD_bc_nn, // LD BC, nn
    [0x02] = SV_bc_a, // LD (BC), A
    [0x03] = NULL, // INC BC
    [0x04] = NULL, // INC B
    [0x05] = NULL, // DEC B
    [0x06] = LD_b_n, // LD B, n
    [0x07] = NULL, // RLC A
    [0x08] = SV_nn_sp, // LD (nn), SP
    [0x09] = NULL, // ADD HL, BC
    [0x0A] = LD_a_bc, // LD A, (BC)
    [0x0B] = NULL, // DEC BC
    [0x0C] = NULL, // INC C
    [0x0D] = NULL, // DEC C
    [0x0E] = LD_c_n, // LD C, n
    [0x0F] = NULL, // RRC A

    // 0x1_
    [0x10] = NULL, // STOP
    [0x11] = LD_de_nn, // LD DE, nn
    [0x12] = SV_de_a, // LD (DE), A
    [0x13] = NULL, // INC DE
    [0x14] = NULL, // INC D
    [0x15] = NULL, // DEC D
    [0x16] = LD_d_n, // LD D, n
    [0x17] = NULL, // RL A
    [0x18] = NULL, // JR n
    [0x19] = NULL, // ADD HL, DE
    [0x1A] = LD_a_de, // LD A, (DE)
    [0x1B] = NULL, // DEC DE
    [0x1C] = NULL, // INC E
    [0x1D] = NULL, // DEC E
    [0x1E] = LD_e_n, // LD E, n
    [0x1F] = NULL, // RR A

    // 0x2_
    [0x20] = NULL, // JR NZ, n
    [0x21] = LD_hl_nn, // LD HL, nn
    [0x22] = LDI_hl_a, // LDI (HL), A
    [0x23] = NULL, // INC HL
    [0x24] = NULL, // INC H
    [0x25] = NULL, // DEC H
    [0x26] = LD_h_n, // LD H, n
    [0x27] = NULL, // DAA
    [0x28] = NULL, // JR Z, n
    [0x29] = NULL, // ADD HL, HL
    [0x2A] = NULL, // LDI A, (HL)
    [0x2B] = NULL, // DEC HL
    [0x2C] = NULL, // INC L
    [0x2D] = NULL, // DEC L
    [0x2E] = LD_l_n, // LD L, n
    [0x2F] = NULL, // CPL

    // 0x3_
    [0x30] = NULL, // JR NC, n
    [0x31] = LD_sp_nn, // LD SP, nn
    [0x32] = NULL, // LDD (HL), A
    [0x33] = NULL, // INC SP
    [0x34] = NULL, // INC (HL)
    [0x35] = NULL, // DEC (HL)
    [0x36] = SV_hl_n, // LD (HL), n
    [0x37] = NULL, // SCF
    [0x38] = NULL, // JR C, n
    [0x39] = NULL, // ADD HL, SP
    [0x3A] = NULL, // LDD A, (HL)
    [0x3B] = NULL, // DEC SP
    [0x3C] = NULL, // INC A
    [0x3D] = NULL, // DEC A
    [0x3E] = LD_a_n, // LD A, n
    [0x3F] = NULL, // CCF

    // 0x4_ (The LD B, r block)
    [0x40] = LD_b_b, // LD B, B
    [0x41] = LD_b_c, // LD B, C
    [0x42] = LD_b_d, // LD B, D
    [0x43] = LD_b_e, // LD B, E
    [0x44] = LD_b_h, // LD B, H
    [0x45] = LD_b_l, // LD B, L
    [0x46] = LD_b_hl, // LD B, (HL)
    [0x47] = LD_b_a, // LD B, A
    [0x48] = LD_c_b, // LD C, B
    [0x49] = LD_c_c, // LD C, C
    [0x4A] = LD_c_d, // LD C, D
    [0x4B] = LD_c_e, // LD C, E
    [0x4C] = LD_c_h, // LD C, H
    [0x4D] = LD_c_l, // LD C, L
    [0x4E] = LD_c_hl, // LD C, (HL)
    [0x4F] = LD_c_a, // LD C, A

    // 0x5_ (The LD D, r block)
    [0x50] = LD_d_b, // LD D, B
    [0x51] = LD_d_c, // LD D, C
    [0x52] = LD_d_d, // LD D, D
    [0x53] = LD_d_e, // LD D, E
    [0x54] = LD_d_h, // LD D, H
    [0x55] = LD_d_l, // LD D, L
    [0x56] = LD_d_hl, // LD D, (HL)
    [0x57] = LD_d_a, // LD D, A
    [0x58] = LD_e_b, // LD E, B
    [0x59] = LD_e_c, // LD E, C
    [0x5A] = LD_e_d, // LD E, D
    [0x5B] = LD_e_e, // LD E, E
    [0x5C] = LD_e_h, // LD E, H
    [0x5D] = LD_e_l, // LD E, L
    [0x5E] = LD_e_hl, // LD E, (HL)
    [0x5F] = LD_e_a, // LD E, A

    // 0x6_ (The LD H, r block)
    [0x60] = LD_h_b, // LD H, B
    [0x61] = LD_h_c, // LD H, C
    [0x62] = LD_h_d, // LD H, D
    [0x63] = LD_h_e, // LD H, E
    [0x64] = LD_h_h, // LD H, H
    [0x65] = LD_h_l, // LD H, L
    [0x66] = LD_h_hl, // LD H, (HL)
    [0x67] = LD_h_a, // LD H, A
    [0x68] = LD_l_b, // LD L, B
    [0x69] = LD_l_c, // LD L, C
    [0x6A] = LD_l_d, // LD L, D
    [0x6B] = LD_l_e, // LD L, E
    [0x6C] = LD_l_h, // LD L, H
    [0x6D] = LD_l_l, // LD L, L
    [0x6E] = LD_l_hl, // LD L, (HL)
    [0x6F] = LD_l_a, // LD L, A

    // 0x7_ (The LD (HL), r block)
    [0x70] = SV_hl_b, // LD (HL), B
    [0x71] = SV_hl_c, // LD (HL), C
    [0x72] = SV_hl_d, // LD (HL), D
    [0x73] = SV_hl_e, // LD (HL), E
    [0x74] = SV_hl_h, // LD (HL), H
    [0x75] = SV_hl_l, // LD (HL), L
    [0x76] = NULL, // HALT
    [0x77] = SV_hl_a, // LD (HL), A
    [0x78] = LD_a_b, // LD A, B
    [0x79] = LD_a_c, // LD A, C
    [0x7A] = LD_a_d, // LD A, D
    [0x7B] = LD_a_e, // LD A, E
    [0x7C] = LD_a_h, // LD A, H
    [0x7D] = LD_a_l, // LD A, L
    [0x7E] = LD_a_hl, // LD A, (HL)
    [0x7F] = LD_a_a, // LD A, A

    // 0x8_ (Arithmetic: ADD / ADC)
    [0x80] = NULL, // ADD A, B
    [0x81] = NULL, // ADD A, C
    [0x82] = NULL, // ADD A, D
    [0x83] = NULL, // ADD A, E
    [0x84] = NULL, // ADD A, H
    [0x85] = NULL, // ADD A, L
    [0x86] = NULL, // ADD A, (HL)
    [0x87] = NULL, // ADD A, A
    [0x88] = NULL, // ADC A, B
    [0x89] = NULL, // ADC A, C
    [0x8A] = NULL, // ADC A, D
    [0x8B] = NULL, // ADC A, E
    [0x8C] = NULL, // ADC A, H
    [0x8D] = NULL, // ADC A, L
    [0x8E] = NULL, // ADC A, (HL)
    [0x8F] = NULL, // ADC A, A

    // 0x9_ (Arithmetic: SUB / SBC)
    [0x90] = NULL, // SUB A, B
    [0x91] = NULL, // SUB A, C
    [0x92] = NULL, // SUB A, D
    [0x93] = NULL, // SUB A, E
    [0x94] = NULL, // SUB A, H
    [0x95] = NULL, // SUB A, L
    [0x96] = NULL, // SUB A, (HL)
    [0x97] = NULL, // SUB A, A
    [0x98] = NULL, // SBC

    // 0xA_ (Logic: AND / XOR)
    [0xA0] = NULL, // AND B
    [0xA1] = NULL, // AND C
    [0xA2] = NULL, // AND D
    [0xA3] = NULL, // AND E
    [0xA4] = NULL, // AND H
    [0xA5] = NULL, // AND L
    [0xA6] = NULL, // AND (HL)
    [0xA7] = NULL, // AND A
    [0xA8] = NULL, // XOR B
    [0xA9] = NULL, // XOR C
    [0xAA] = NULL, // XOR D
    [0xAB] = NULL, // XOR E
    [0xAC] = NULL, // XOR H
    [0xAD] = NULL, // XOR L
    [0xAE] = NULL, // XOR (HL)
    [0xAF] = NULL, // XOR A

    // 0xB_ (Logic: OR / CP)
    [0xB0] = NULL, // OR B
    [0xB1] = NULL, // OR C
    [0xB2] = NULL, // OR D
    [0xB3] = NULL, // OR E
    [0xB4] = NULL, // OR H
    [0xB5] = NULL, // OR L
    [0xB6] = NULL, // OR (HL)
    [0xB7] = NULL, // OR A
    [0xB8] = NULL, // CP B
    [0xB9] = NULL, // CP C
    [0xBA] = NULL, // CP D
    [0xBB] = NULL, // CP E
    [0xBC] = NULL, // CP H
    [0xBD] = NULL, // CP L
    [0xBE] = NULL, // CP (HL)
    [0xBF] = NULL, // CP A

    // 0xC_ (Control Flow / Stack)
    [0xC0] = NULL, // RET NZ
    [0xC1] = NULL, // POP BC
    [0xC2] = NULL, // JP NZ, nn
    [0xC3] = NULL, // JP nn
    [0xC4] = NULL, // CALL NZ, nn
    [0xC5] = NULL, // PUSH BC
    [0xC6] = NULL, // ADD A, n
    [0xC7] = NULL, // RST 0
    [0xC8] = NULL, // RET Z
    [0xC9] = NULL, // RET
    [0xCA] = NULL, // JP Z, nn
    [0xCB] = NULL, // Ext ops (Prefix CB)
    [0xCC] = NULL, // CALL Z, nn
    [0xCD] = NULL, // CALL nn
    [0xCE] = NULL, // ADC A, n
    [0xCF] = NULL, // RST 8

    // 0xD_
    [0xD0] = NULL, // RET NC
    [0xD1] = NULL, // POP DE
    [0xD2] = NULL, // JP NC, nn
    [0xD3] = NULL, // XX (Invalid)
    [0xD4] = NULL, // CALL NC, nn
    [0xD5] = NULL, // PUSH DE
    [0xD6] = NULL, // SUB A, n
    [0xD7] = NULL, // RST 10
    [0xD8] = NULL, // RET C
    [0xD9] = NULL, // RETI
    [0xDA] = NULL, // JP C, nn
    [0xDB] = NULL, // XX (Invalid)
    [0xDC] = NULL, // CALL C, nn
    [0xDD] = NULL, // XX (Invalid)
    [0xDE] = NULL, // SBC A, n
    [0xDF] = NULL, // RST 18

    // 0xE_
    [0xE0] = SVH_imm_a, // LDH (n), A
    [0xE1] = NULL, // POP HL
    [0xE2] = SLD_c_a, // LDH (C), A
    [0xE3] = NULL, // XX (Invalid)
    [0xE4] = NULL, // XX (Invalid)
    [0xE5] = NULL, // PUSH HL
    [0xE6] = NULL, // AND n
    [0xE7] = NULL, // RST 20
    [0xE8] = NULL, // ADD SP, d
    [0xE9] = NULL, // JP (HL)
    [0xEA] = SV_a_n, // LD (nn), A
    [0xEB] = NULL, // XX (Invalid)
    [0xEC] = NULL, // XX (Invalid)
    [0xED] = NULL, // XX (Invalid)
    [0xEE] = NULL, // XOR n
    [0xEF] = NULL, // RST 28

    // 0xF_
    [0xF0] = LDH_imm_a, // LDH A, (n)
    [0xF1] = NULL, // POP AF
    [0xF2] = SLD_a_c, // LDH A, [C] 
    [0xF3] = NULL, // DI (Disable Interrupts)
    [0xF4] = NULL, // XX (Invalid)
    [0xF5] = NULL, // PUSH AF
    [0xF6] = NULL, // OR n
    [0xF7] = NULL, // RST 30
    [0xF8] = NULL, // LDHL SP, d
    [0xF9] = LD_sp_hl, // LD SP, HL
    [0xFA] = LD_a_nn, // LD A, (nn)
    [0xFB] = NULL, // EI (Enable Interrupts)
    [0xFC] = NULL, // XX (Invalid)
    [0xFD] = NULL, // XX (Invalid)
    [0xFE] = NULL, // CP n
    [0xFF] = NULL, // RST 38
};

instruction_ptr prefix_opcode_table[256] = {
    // 0x0_ (Rotates & Shifts)
    [0x00] = NULL, // RLC B
    [0x01] = NULL, // RLC C
    [0x02] = NULL, // RLC D
    [0x03] = NULL, // RLC E
    [0x04] = NULL, // RLC H
    [0x05] = NULL, // RLC L
    [0x06] = NULL, // RLC (HL)
    [0x07] = NULL, // RLC A
    [0x08] = NULL, // RRC B
    [0x09] = NULL, // RRC C
    [0x0A] = NULL, // RRC D
    [0x0B] = NULL, // RRC E
    [0x0C] = NULL, // RRC H
    [0x0D] = NULL, // RRC L
    [0x0E] = NULL, // RRC (HL)
    [0x0F] = NULL, // RRC A

    // 0x1_ (Rotates & Shifts through Carry)
    [0x10] = NULL, // RL B
    [0x11] = NULL, // RL C
    [0x12] = NULL, // RL D
    [0x13] = NULL, // RL E
    [0x14] = NULL, // RL H
    [0x15] = NULL, // RL L
    [0x16] = NULL, // RL (HL)
    [0x17] = NULL, // RL A
    [0x18] = NULL, // RR B
    [0x19] = NULL, // RR C
    [0x1A] = NULL, // RR D
    [0x1B] = NULL, // RR E
    [0x1C] = NULL, // RR H
    [0x1D] = NULL, // RR L
    [0x1E] = NULL, // RR (HL)
    [0x1F] = NULL, // RR A

    // 0x2_ (Arithmetic Shifts)
    [0x20] = NULL, // SLA B
    [0x21] = NULL, // SLA C
    [0x22] = NULL, // SLA D
    [0x23] = NULL, // SLA E
    [0x24] = NULL, // SLA H
    [0x25] = NULL, // SLA L
    [0x26] = NULL, // SLA (HL)
    [0x27] = NULL, // SLA A
    [0x28] = NULL, // SRA B
    [0x29] = NULL, // SRA C
    [0x2A] = NULL, // SRA D
    [0x2B] = NULL, // SRA E
    [0x2C] = NULL, // SRA H
    [0x2D] = NULL, // SRA L
    [0x2E] = NULL, // SRA (HL)
    [0x2F] = NULL, // SRA A

    // 0x3_ (Swaps & Logical Shifts)
    [0x30] = NULL, // SWAP B
    [0x31] = NULL, // SWAP C
    [0x32] = NULL, // SWAP D
    [0x33] = NULL, // SWAP E
    [0x34] = NULL, // SWAP H
    [0x35] = NULL, // SWAP L
    [0x36] = NULL, // SWAP (HL)
    [0x37] = NULL, // SWAP A
    [0x38] = NULL, // SRL B
    [0x39] = NULL, // SRL C
    [0x3A] = NULL, // SRL D
    [0x3B] = NULL, // SRL E
    [0x3C] = NULL, // SRL H
    [0x3D] = NULL, // SRL L
    [0x3E] = NULL, // SRL (HL)
    [0x3F] = NULL, // SRL A

    // 0x4_ (BIT 0 and BIT 1)
    [0x40] = NULL, // BIT 0, B
    [0x41] = NULL, // BIT 0, C
    [0x42] = NULL, // BIT 0, D
    [0x43] = NULL, // BIT 0, E
    [0x44] = NULL, // BIT 0, H
    [0x45] = NULL, // BIT 0, L
    [0x46] = NULL, // BIT 0, (HL)
    [0x47] = NULL, // BIT 0, A
    [0x48] = NULL, // BIT 1, B
    [0x49] = NULL, // BIT 1, C
    [0x4A] = NULL, // BIT 1, D
    [0x4B] = NULL, // BIT 1, E
    [0x4C] = NULL, // BIT 1, H
    [0x4D] = NULL, // BIT 1, L
    [0x4E] = NULL, // BIT 1, (HL)
    [0x4F] = NULL, // BIT 1, A

    // 0x5_ (BIT 2 and BIT 3)
    [0x50] = NULL, // BIT 2, B
    [0x51] = NULL, // BIT 2, C
    [0x52] = NULL, // BIT 2, D
    [0x53] = NULL, // BIT 2, E
    [0x54] = NULL, // BIT 2, H
    [0x55] = NULL, // BIT 2, L
    [0x56] = NULL, // BIT 2, (HL)
    [0x57] = NULL, // BIT 2, A
    [0x58] = NULL, // BIT 3, B
    [0x59] = NULL, // BIT 3, C
    [0x5A] = NULL, // BIT 3, D
    [0x5B] = NULL, // BIT 3, E
    [0x5C] = NULL, // BIT 3, H
    [0x5D] = NULL, // BIT 3, L
    [0x5E] = NULL, // BIT 3, (HL)
    [0x5F] = NULL, // BIT 3, A

    // 0x6_ (BIT 4 and BIT 5)
    [0x60] = NULL, // BIT 4, B
    [0x61] = NULL, // BIT 4, C
    [0x62] = NULL, // BIT 4, D
    [0x63] = NULL, // BIT 4, E
    [0x64] = NULL, // BIT 4, H
    [0x65] = NULL, // BIT 4, L
    [0x66] = NULL, // BIT 4, (HL)
    [0x67] = NULL, // BIT 4, A
    [0x68] = NULL, // BIT 5, B
    [0x69] = NULL, // BIT 5, C
    [0x6A] = NULL, // BIT 5, D
    [0x6B] = NULL, // BIT 5, E
    [0x6C] = NULL, // BIT 5, H
    [0x6D] = NULL, // BIT 5, L
    [0x6E] = NULL, // BIT 5, (HL)
    [0x6F] = NULL, // BIT 5, A

    // 0x7_ (BIT 6 and BIT 7)
    [0x70] = NULL, // BIT 6, B
    [0x71] = NULL, // BIT 6, C
    [0x72] = NULL, // BIT 6, D
    [0x73] = NULL, // BIT 6, E
    [0x74] = NULL, // BIT 6, H
    [0x75] = NULL, // BIT 6, L
    [0x76] = NULL, // BIT 6, (HL)
    [0x77] = NULL, // BIT 6, A
    [0x78] = NULL, // BIT 7, B
    [0x79] = NULL, // BIT 7, C
    [0x7A] = NULL, // BIT 7, D
    [0x7B] = NULL, // BIT 7, E
    [0x7C] = NULL, // BIT 7, H
    [0x7D] = NULL, // BIT 7, L
    [0x7E] = NULL, // BIT 7, (HL)
    [0x7F] = NULL, // BIT 7, A

    // 0x8_ (RES 0 and RES 1)
    [0x80] = NULL, // RES 0, B
    [0x81] = NULL, // RES 0, C
    [0x82] = NULL, // RES 0, D
    [0x83] = NULL, // RES 0, E
    [0x84] = NULL, // RES 0, H
    [0x85] = NULL, // RES 0, L
    [0x86] = NULL, // RES 0, (HL)
    [0x87] = NULL, // RES 0, A
    [0x88] = NULL, // RES 1, B
    [0x89] = NULL, // RES 1, C
    [0x8A] = NULL, // RES 1, D
    [0x8B] = NULL, // RES 1, E
    [0x8C] = NULL, // RES 1, H
    [0x8D] = NULL, // RES 1, L
    [0x8E] = NULL, // RES 1, (HL)
    [0x8F] = NULL, // RES 1, A

    // 0x9_ (RES 2 and RES 3)
    [0x90] = NULL, // RES 2, B
    [0x91] = NULL, // RES 2, C
    [0x92] = NULL, // RES 2, D
    [0x93] = NULL, // RES 2, E
    [0x94] = NULL, // RES 2, H
    [0x95] = NULL, // RES 2, L
    [0x96] = NULL, // RES 2, (HL)
    [0x97] = NULL, // RES 2, A
    [0x98] = NULL, // RES 3, B
    [0x99] = NULL, // RES 3, C
    [0x9A] = NULL, // RES 3, D
    [0x9B] = NULL, // RES 3, E
    [0x9C] = NULL, // RES 3, H
    [0x9D] = NULL, // RES 3, L
    [0x9E] = NULL, // RES 3, (HL)
    [0x9F] = NULL, // RES 3, A

    // 0xA_ (RES 4 and RES 5)
    [0xA0] = NULL, // RES 4, B
    [0xA1] = NULL, // RES 4, C
    [0xA2] = NULL, // RES 4, D
    [0xA3] = NULL, // RES 4, E
    [0xA4] = NULL, // RES 4, H
    [0xA5] = NULL, // RES 4, L
    [0xA6] = NULL, // RES 4, (HL)
    [0xA7] = NULL, // RES 4, A
    [0xA8] = NULL, // RES 5, B
    [0xA9] = NULL, // RES 5, C
    [0xAA] = NULL, // RES 5, D
    [0xAB] = NULL, // RES 5, E
    [0xAC] = NULL, // RES 5, H
    [0xAD] = NULL, // RES 5, L
    [0xAE] = NULL, // RES 5, (HL)
    [0xAF] = NULL, // RES 5, A

    // 0xB_ (RES 6 and RES 7)
    [0xB0] = NULL, // RES 6, B
    [0xB1] = NULL, // RES 6, C
    [0xB2] = NULL, // RES 6, D
    [0xB3] = NULL, // RES 6, E
    [0xB4] = NULL, // RES 6, H
    [0xB5] = NULL, // RES 6, L
    [0xB6] = NULL, // RES 6, (HL)
    [0xB7] = NULL, // RES 6, A
    [0xB8] = NULL, // RES 7, B
    [0xB9] = NULL, // RES 7, C
    [0xBA] = NULL, // RES 7, D
    [0xBB] = NULL, // RES 7, E
    [0xBC] = NULL, // RES 7, H
    [0xBD] = NULL, // RES 7, L
    [0xBE] = NULL, // RES 7, (HL)
    [0xBF] = NULL, // RES 7, A

    // 0xC_ (SET 0 and SET 1)
    [0xC0] = NULL, // SET 0, B
    [0xC1] = NULL, // SET 0, C
    [0xC2] = NULL, // SET 0, D
    [0xC3] = NULL, // SET 0, E
    [0xC4] = NULL, // SET 0, H
    [0xC5] = NULL, // SET 0, L
    [0xC6] = NULL, // SET 0, (HL)
    [0xC7] = NULL, // SET 0, A
    [0xC8] = NULL, // SET 1, B
    [0xC9] = NULL, // SET 1, C
    [0xCA] = NULL, // SET 1, D
    [0xCB] = NULL, // SET 1, E
    [0xCC] = NULL, // SET 1, H
    [0xCD] = NULL, // SET 1, L
    [0xCE] = NULL, // SET 1, (HL)
    [0xCF] = NULL, // SET 1, A

    // 0xD_ (SET 2 and SET 3)
    [0xD0] = NULL, // SET 2, B
    [0xD1] = NULL, // SET 2, C
    [0xD2] = NULL, // SET 2, D
    [0xD3] = NULL, // SET 2, E
    [0xD4] = NULL, // SET 2, H
    [0xD5] = NULL, // SET 2, L
    [0xD6] = NULL, // SET 2, (HL)
    [0xD7] = NULL, // SET 2, A
    [0xD8] = NULL, // SET 3, B
    [0xD9] = NULL, // SET 3, C
    [0xDA] = NULL, // SET 3, D
    [0xDB] = NULL, // SET 3, E
    [0xDC] = NULL, // SET 3, H
    [0xDD] = NULL, // SET 3, L
    [0xDE] = NULL, // SET 3, (HL)
    [0xDF] = NULL, // SET 3, A

    // 0xE_ (SET 4 and SET 5)
    [0xE0] = NULL, // SET 4, B
    [0xE1] = NULL, // SET 4, C
    [0xE2] = NULL, // SET 4, D
    [0xE3] = NULL, // SET 4, E
    [0xE4] = NULL, // SET 4, H
    [0xE5] = NULL, // SET 4, L
    [0xE6] = NULL, // SET 4, (HL)
    [0xE7] = NULL, // SET 4, A
    [0xE8] = NULL, // SET 5, B
    [0xE9] = NULL, // SET 5, C
    [0xEA] = NULL, // SET 5, D
    [0xEB] = NULL, // SET 5, E
    [0xEC] = NULL, // SET 5, H
    [0xED] = NULL, // SET 5, L
    [0xEE] = NULL, // SET 5, (HL)
    [0xEF] = NULL, // SET 5, A

    // 0xF_ (SET 6 and SET 7)
    [0xF0] = NULL, // SET 6, B
    [0xF1] = NULL, // SET 6, C
    [0xF2] = NULL, // SET 6, D
    [0xF3] = NULL, // SET 6, E
    [0xF4] = NULL, // SET 6, H
    [0xF5] = NULL, // SET 6, L
    [0xF6] = NULL, // SET 6, (HL)
    [0xF7] = NULL, // SET 6, A
    [0xF8] = NULL, // SET 7, B
    [0xF9] = NULL, // SET 7, C
    [0xFA] = NULL, // SET 7, D
    [0xFB] = NULL, // SET 7, E
    [0xFC] = NULL, // SET 7, H
    [0xFD] = NULL, // SET 7, L
    [0xFE] = NULL, // SET 7, (HL)
    [0xFF] = NULL, // SET 7, A
};

#endif