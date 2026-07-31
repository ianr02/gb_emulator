#include "cpu/cpu_instructions.h"
#include "core/emulator_core.h"
#include "oam_dma/oam.h"

bool prefix_flag = false;
bool halt_bug = false;

#define GEN_LD_N(reg_name) \
void LD_##reg_name##_n() { \
    uint8_t val = read_byte(reg->pc++); \
    reg->reg_name = val; \
}

#define GEN_LD_n_hl(reg_name) \
void LD_##reg_name##_hl() { \
    reg->reg_name = read_byte(reg->hl); \
}

#define GEN_LD_R1_R2(r1, r2) \
void LD_##r1##_##r2() { \
    reg->r1 = reg->r2; \
}

#define GEN_REG_n(reg_name) \
void LD_a_##reg_name() { \
    reg->a = read_byte(reg->reg_name); \
}

#define GEN_LD_ADDR_R(reg_addr, reg_name) \
void SV_##reg_addr##_##reg_name() { \
    save_byte(reg->reg_addr, reg->reg_name); \
}

#define GEN_LD_IMM_REG16(reg_name) \
void LD_##reg_name##_nn() { \
    uint8_t low = read_byte(reg->pc++); \
    uint8_t high = read_byte(reg->pc++); \
    uint16_t value = (high << 8) | low; \
    reg->reg_name = value; \
}

#define PUSH_REG16(reg_name) \
void PUSH_##reg_name() { \
    if (reg->sp >= 0xFE00 && reg->sp <= 0xFEFF) oam_bug_incdec(); \
    update_timers(4); \
    save_byte(--reg->sp, (uint8_t)(reg->reg_name >> 8)); \
    save_byte(--reg->sp, (uint8_t)(reg->reg_name & 0xFF)); \
}

#define POP_REG16(reg_name) \
void POP_##reg_name() { \
    oam_set_read_inc(reg->sp >= 0xFE00 && reg->sp <= 0xFEFF); \
    uint8_t low = read_byte(reg->sp++); \
    oam_set_read_inc(false); \
    uint8_t high = read_byte(reg->sp++); \
    uint16_t val = (high << 8) | low; \
    reg->reg_name = val; \
}

#define GEN_ADD_A_REG(reg_name) \
void ADD_A_##reg_name(){ \
    uint8_t val = reg->reg_name; \
    uint16_t result = reg->a + val; \
    reg->f = 0; \
    if ((result & 0xFF) == 0) \
        reg->f |= 0x80; \
    if (((reg->a & 0x0F) + (val & 0x0F)) > 0xF) \
        reg->f |= 0x20; \
    if (result > 0xFF) \
        reg->f |= 0x10; \
    reg->a = (uint8_t)result; \
}

#define GEN_ADC_A_REG(reg_name) \
void ADC_A_##reg_name(){ \
    uint8_t val = reg->reg_name; \
    uint8_t carry = (reg->f & 0x10) ? 1 : 0; \
    uint16_t result = reg->a + val + carry; \
    reg->f = 0; \
    if ((result & 0xFF) == 0) \
        reg->f |= 0x80; \
    if (((reg->a & 0x0F) + (val & 0x0F) + carry) > 0xF) \
        reg->f |= 0x20; \
    if (result > 0xFF) \
        reg->f |= 0x10; \
    reg->a = (uint8_t)result; \
}

#define GEN_SUB_A_REG(reg_name) \
void SUB_A_##reg_name(){ \
    uint8_t val = reg->reg_name; \
    int16_t result = (int16_t)reg->a - (int16_t)val; \
    reg->f = 0x40; \
    if ((result & 0xFF) == 0) \
        reg->f |= 0x80; \
    if (((reg->a & 0x0F) - (val & 0x0F)) < 0) \
        reg->f |= 0x20; \
    if (result < 0) \
        reg->f |= 0x10; \
    reg->a = (uint8_t)result; \
}

#define GEN_SBC_A_REG(reg_name) \
void SBC_A_##reg_name(){ \
    uint8_t val = reg->reg_name; \
    uint8_t carry = (reg->f & 0x10) ? 1 : 0; \
    int16_t result = (int16_t)reg->a - (int16_t)val - carry; \
    reg->f = 0x40; \
    if ((result & 0xFF) == 0) \
        reg->f |= 0x80; \
    if (((reg->a & 0x0F) - (val & 0x0F) - carry) < 0) \
        reg->f |= 0x20; \
    if (result < 0) \
        reg->f |= 0x10; \
    reg->a = (uint8_t)result; \
}

#define GEN_AND_A_REG(reg_name) \
void AND_A_##reg_name() { \
    reg->f = 0x20; \
    if ((reg->a & reg->reg_name) == 0) \
        reg->f |= 0x80; \
    reg->a &= reg->reg_name; \
}

#define GEN_OR_A_REG(reg_name) \
void OR_A_##reg_name() { \
    reg->f = 0x0; \
    if ((reg->a | reg->reg_name) == 0) \
        reg->f |= 0x80; \
    reg->a |= reg->reg_name; \
}

#define GEN_XOR_A_REG(reg_name) \
void XOR_A_##reg_name() { \
    reg->f = 0x0; \
    if ((reg->a ^ reg->reg_name)== 0) \
        reg->f |= 0x80; \
    reg->a ^= reg->reg_name; \
}

#define GEN_CP_A_REG(reg_name) \
void CP_A_##reg_name() { \
    uint8_t val = reg->reg_name; \
    int16_t result = reg->a - val; \
    reg->f = 0x40; \
    if ((result & 0xFF) == 0) { \
        reg->f |= 0x80; \
    } \
    if ((reg->a & 0xF) - (val & 0xF) < 0) { \
        reg->f |= 0x20; \
    } \
    if (result < 0) { \
        reg->f |= 0x10; \
    } \
}

#define GEN_INC_REG(reg_name) \
void INC_##reg_name() { \
    reg->f &= 0x10; \
    if((reg->reg_name & 0xF) + 0x01 > 0xF) { \
        reg->f |= 0x20; \
    } \
    ++reg->reg_name;  \
    if (reg->reg_name == 0) { \
        reg->f |= 0x80; \
    } \
}

#define GEN_DEC_REG(reg_name) \
void DEC_##reg_name() { \
    reg->f &= 0x10; \
    reg->f |= 0x40; \
    if((reg->reg_name & 0xF) == 0x0) \
        reg->f |= 0x20; \
    --reg->reg_name; \
    if(reg->reg_name == 0) \
        reg->f |= 0x80; \
}

#define GEN_ADD_HL_REG(reg_name) \
void ADD_HL_##reg_name(){ \
    reg->f &= 0x80; \
    if(reg->hl + reg->reg_name > 0xFFFF) \
        reg->f |= 0x10; \
    if((reg->hl & 0xFFF) + (reg->reg_name & 0xFFF) > 0xFFF) \
        reg->f |= 0x20; \
    reg->hl += reg->reg_name; \
    update_timers(4); \
}

#define GEN_INC_REG16(reg_name) \
void INC_##reg_name(){ \
    if (reg->reg_name >= 0xFE00 && reg->reg_name <= 0xFEFF) oam_bug_incdec(); \
    ++reg->reg_name; \
    update_timers(4); \
}

#define GEN_DEC_REG16(reg_name) \
void DEC_##reg_name(){ \
    if (reg->reg_name >= 0xFE00 && reg->reg_name <= 0xFEFF) oam_bug_incdec(); \
    --reg->reg_name; \
    update_timers(4); \
}

#define GEN_SWAP_REG(reg_name) \
void SWAP_##reg_name() { \
    reg->f = 0x0; \
    if (reg->reg_name == 0x0) \
        reg->f = 0x80; \
    reg->reg_name = (reg->reg_name << 4) | (reg->reg_name >> 4); \
}

#define GEN_RLC_n(reg_name) \
void RLC_##reg_name() { \
    reg->f = 0x0; \
    uint8_t carry = (reg->reg_name & 0x80) >> 7; \
    reg->reg_name = (reg->reg_name << 1) | carry; \
    reg->f |= (carry << 4); \
    if (prefix_flag && reg->reg_name == 0) \
        reg->f |= 0x80; \
}

#define GEN_RL_n(reg_name) \
void RL_##reg_name() { \
    uint8_t new = (reg->reg_name & 0x80) >> 7; \
    uint8_t old = (reg->f & 0x10) >> 4; \
    reg->reg_name = (reg->reg_name << 1) | old; \
    reg->f = (new << 4); \
    if (prefix_flag && reg->reg_name == 0) \
        reg->f |= 0x80; \
}

#define GEN_RRC_n(reg_name) \
void RRC_##reg_name() { \
    reg->f = 0x0; \
    uint8_t carry = (reg->reg_name & 0x01); \
    reg->reg_name = (carry << 7) | (reg->reg_name >> 1); \
    reg->f |= (carry << 4); \
    if (prefix_flag && reg->reg_name == 0) \
        reg->f |= 0x80; \
}

#define GEN_RR_n(reg_name) \
void RR_##reg_name() { \
    uint8_t new = (reg->reg_name & 0x01); \
    uint8_t old = (reg->f & 0x10) << 3; \
    reg->reg_name = old | (reg->reg_name >> 1); \
    reg->f = (new << 4); \
    if (prefix_flag && reg->reg_name == 0) \
        reg->f |= 0x80; \
}

#define GEN_SL_n(reg_name) \
void SL_##reg_name() { \
    uint8_t carry = (reg->reg_name & 0x80) >> 7; \
    reg->f = carry << 4; \
    reg->reg_name <<= 1; \
    if (reg->reg_name == 0) \
        reg->f |= 0x80; \
}

#define GEN_SRA_n(reg_name) \
void SRA_##reg_name() { \
    uint8_t msb = reg->reg_name & 0x80; \
    uint8_t carry = reg->reg_name & 0x01; \
    reg->reg_name = msb | (reg->reg_name >> 1); \
    reg->f = (carry << 4); \
    if (reg->reg_name == 0x0) \
        reg->f |= 0x80; \
}

#define GEN_SRL_n(reg_name) \
void SRL_##reg_name() { \
    uint8_t carry = reg->reg_name & 0x01; \
    reg->reg_name >>= 1; \
    reg->f = (carry << 4); \
    if (reg->reg_name == 0x0) \
        reg->f |= 0x80; \
}

#define GEN_BIT_n(reg_name) \
void BIT_##reg_name() { \
    uint8_t bit = (opcode >> 3) & 0x07; \
    uint8_t mask = 1 << bit; \
    reg->f &= 0x10; \
    reg->f |= 0x20; \
    if (!(reg->reg_name & mask)) \
        reg->f |= 0x80; \
}

#define GEN_SET_n(reg_name) \
void SET_##reg_name() { \
    uint8_t bit = (opcode >> 3) & 0x07; \
    uint8_t mask = 1 << bit; \
    reg->reg_name |= mask; \
}

#define GEN_RESET_n(reg_name) \
void RESET_##reg_name() { \
    uint8_t bit = (opcode >> 3) & 0x07; \
    uint8_t mask = ~(1 << bit); \
    reg->reg_name &= mask;\
}

void NOP(){ }

void LD_a_nn() {
    uint8_t low = read_byte(reg->pc ++);
    uint8_t high = read_byte(reg->pc ++);
    uint16_t address = (high << 8) | low;
    reg->a = read_byte(address);
}

void LDH_a_c() {
    reg->a = read_byte(0xFF00 | reg->c);
}

void SLD_a_c() {
    save_byte(0xFF00 | reg->c, reg->a);
}

void POP_af() {
    uint8_t low = read_byte(reg->sp++);
    uint8_t high = read_byte(reg->sp++);
    reg->af = (high << 8) | (low & 0xF0);
}

void LD_sp_hl() {
    reg->sp = reg->hl;
    update_timers(4);
}

void SV_hl_n() {
    uint8_t val = read_byte(reg->pc++);
    save_byte(reg->hl, val);
}

void SV_nn_a() {
    uint8_t low = read_byte(reg->pc++);
    uint8_t high = read_byte(reg->pc++);
    uint16_t address = low | (high << 8);
    save_byte(address, reg->a);
}

void SVH_imm_a(){
    uint16_t address = 0xFF00 | read_byte(reg->pc++);
    save_byte(address, reg->a);
}

void LDH_imm_a(){
    uint16_t address = 0xFF00 | read_byte(reg->pc++);
    reg->a = read_byte(address);
}

void LDHL_sp_n(){
    int8_t n = read_byte(reg->pc++);
    reg->f = 0x0;
    if ((reg->sp & 0xF) + ((uint8_t)n & 0xF) > 0xF)
        reg->f |= 0x20;
    if (((reg->sp & 0xFF) + (uint8_t)n) > (0xFF))
        reg->f |= 0x10;
    reg->hl = reg->sp + n;
    update_timers(4);
}

void SV_nn_sp(){
    uint8_t low = read_byte(reg->pc++);
    uint8_t high = read_byte(reg->pc++);
    uint16_t address = low | (high << 8);
    save_byte(address, (uint8_t)(reg->sp & 0xFF));
    save_byte(address+1, (uint8_t)(reg->sp >> 8));
}

void LDD_a_hl() {
    oam_set_read_inc(reg->hl >= 0xFE00 && reg->hl <= 0xFEFF);
    reg->a = read_byte(reg->hl);
    oam_set_read_inc(false);
    --reg->hl;
}

void SVD_a_hl() {
    save_byte(reg->hl, reg->a);
    --reg->hl;
}

void LDI_a_hl() {
    oam_set_read_inc(reg->hl >= 0xFE00 && reg->hl <= 0xFEFF);
    reg->a = read_byte(reg->hl);
    oam_set_read_inc(false);
    ++reg->hl;
}

void SVI_a_hl() {
    save_byte(reg->hl, reg->a);
    ++reg->hl;
}

void ADD_A_hl(){
    uint8_t val = read_byte(reg->hl);
    uint16_t result = reg->a + val;
    reg->f = 0;
    if ((result & 0xFF) == 0)
        reg->f |= 0x80;
    if (((reg->a & 0x0F) + (val & 0x0F)) > 0xF)
        reg->f |= 0x20;
    if (result > 0xFF)
        reg->f |= 0x10;
    reg->a = (uint8_t) result;
}

void ADD_A_n(){
    uint8_t val = read_byte(reg->pc++);
    uint16_t result = reg->a + val;
    reg->f = 0;
    if ((result & 0xFF) == 0)
        reg->f |= 0x80;
    if (((reg->a & 0x0F) + (val & 0x0F)) > 0xF)
        reg->f |= 0x20;
    if (result > 0xFF)
        reg->f |= 0x10;
    reg->a = (uint8_t) result;
}

void ADC_A_hl(){
    uint8_t val = read_byte(reg->hl);
    uint8_t carry = (reg->f & 0x10) ? 1 : 0;
    uint16_t result = reg->a + val + carry;
    reg->f = 0;
    if ((result & 0xFF) == 0)
        reg->f |= 0x80;
    if (((reg->a & 0x0F) + (val & 0x0F) + carry) > 0xF)
        reg->f |= 0x20;
    if (result > 0xFF)
        reg->f |= 0x10;
    reg->a = (uint8_t)result;
}

void ADC_A_n(){
    uint8_t val = read_byte(reg->pc++);
    uint8_t carry = (reg->f & 0x10) ? 1 : 0;
    uint16_t result = reg->a + val + carry;
    reg->f = 0;
    if ((result & 0xFF) == 0)
        reg->f |= 0x80;
    if (((reg->a & 0x0F) + (val & 0x0F) + carry) > 0xF)
        reg->f |= 0x20;
    if (result > 0xFF)
        reg->f |= 0x10;
    reg->a = (uint8_t)result;
}

void SUB_A_hl(){
    uint8_t val = read_byte(reg->hl);
    int16_t result = (int16_t)reg->a - (int16_t)val;
    reg->f = 0x40;
    if ((result & 0xFF) == 0)
        reg->f |= 0x80;
    if (((reg->a & 0x0F) - (val & 0x0F)) < 0)
        reg->f |= 0x20;
    if (result < 0)
        reg->f |= 0x10;
    reg->a = (uint8_t)result;
}

void SUB_A_n(){
    uint8_t val = read_byte(reg->pc++);
    int16_t result = (int16_t)reg->a - (int16_t)val;
    reg->f = 0x40;
    if ((result & 0xFF) == 0)
        reg->f |= 0x80;
    if (((reg->a & 0x0F) - (val & 0x0F)) < 0)
        reg->f |= 0x20;
    if (result < 0)
        reg->f |= 0x10;
    reg->a = (uint8_t)result;
}

void SBC_A_hl(){
    uint8_t val = read_byte(reg->hl);
    uint8_t carry = (reg->f & 0x10) ? 1 : 0;
    int16_t result = (int16_t)reg->a - (int16_t)val - carry;
    reg->f = 0x40;
    if ((result & 0xFF) == 0)
        reg->f |= 0x80;
    if (((reg->a & 0x0F) - (val & 0x0F) - carry) < 0)
        reg->f |= 0x20;
    if (result < 0)
        reg->f |= 0x10;
    reg->a = (uint8_t)result;
}

void SBC_A_imm(){
    uint8_t val = read_byte(reg->pc++);
    uint8_t carry = (reg->f & 0x10) ? 1 : 0;
    int16_t result = (int16_t)reg->a - (int16_t)val - carry;
    reg->f = 0x40;
    if ((result & 0xFF) == 0)
        reg->f |= 0x80;
    if (((reg->a & 0x0F) - (val & 0x0F) - carry) < 0)
        reg->f |= 0x20;
    if (result < 0)
        reg->f |= 0x10;
    reg->a = (uint8_t)result;
}

void AND_A_hl() {
    uint8_t val = read_byte(reg->hl);
    reg->f = 0x20;
    if ((reg->a & val) == 0)
        reg->f |= 0x80;
    reg->a &= val;
}

void AND_A_n() {
    uint8_t val = read_byte(reg->pc++);
    reg->f = 0x20;
    if ((reg->a & val) == 0)
        reg->f |= 0x80;
    reg->a &= val;
}

void OR_A_hl() {
    uint8_t val = read_byte(reg->hl);
    reg->f = 0x0;
    if ((reg->a | val) == 0)
        reg->f |= 0x80;
    reg->a |= val;
}

void OR_A_n() {
    uint8_t val = read_byte(reg->pc++);
    reg->f = 0x0;
    if ((reg->a | val) == 0)
        reg->f |= 0x80;
    reg->a |= val;
}

void XOR_A_hl() {
    uint8_t val = read_byte(reg->hl);
    reg->f = 0x0;
    if ((reg->a ^ val) == 0)
        reg->f |= 0x80;
    reg->a ^= val;
}

void XOR_A_n() {
    uint8_t val = read_byte(reg->pc++);
    reg->f = 0x0;
    if ((reg->a ^ val) == 0)
        reg->f |= 0x80;
    reg->a ^= val;
}

void CP_A_hl() {
    uint8_t val = read_byte(reg->hl);
    int16_t result = reg->a - val;
    reg->f = 0x40;
    if ((result & 0xFF) == 0)
        reg->f |= 0x80;
    if ((reg->a & 0xF) - (val & 0xF) < 0)
        reg->f |= 0x20;
    if (result < 0)
        reg->f |= 0x10;
}

void CP_A_n() {
    uint8_t val = read_byte(reg->pc++);
    int16_t result = reg->a - val;
    reg->f = 0x40;
    if ((result & 0xFF) == 0)
        reg->f |= 0x80;
    if ((reg->a & 0xF) - (val & 0xF) < 0)
        reg->f |= 0x20;
    if (result < 0)
        reg->f |= 0x10;
}

void INC_REG_hl() {
    uint8_t val = read_byte(reg->hl);
    reg->f &= 0x10;
    if((val & 0xF) + 0x01 > 0xF)
        reg->f |= 0x20;
    ++val;
    save_byte(reg->hl, val);
    if (val == 0)
        reg->f |= 0x80;
}

void DEC_REG_hl() {
    uint8_t val = read_byte(reg->hl);
    reg->f &= 0x10;
    reg->f |= 0x40;
    if((val & 0xF) == 0x0)
        reg->f |= 0x20;
    --val;
    save_byte(reg->hl, val);
    if(val == 0)
        reg->f |= 0x80;
}

void ADD_SP_n() {
    reg->f = 0x0;
    int8_t val = (int8_t) read_byte(reg->pc++);
    uint16_t imm = (uint16_t)(uint8_t) val;
    if((reg->sp & 0xF) + (imm & 0xF) > 0xF)
        reg->f |= 0x20;
    if((reg->sp & 0xFF) + (imm & 0xFF) > 0xFF)
        reg->f |= 0x10;
    reg->sp += val;
    update_timers(8);
}

void SWAP_hl() {
    uint8_t val = read_byte(reg->hl);
    reg->f = 0x0;
    if (val == 0x0)
        reg->f = 0x80;
    val = (val << 4) | (val >> 4);
    save_byte(reg->hl, val);
}

void DAA() {
    uint8_t correction = 0;
    bool carry = false;
    if (!(reg->f & 0x40)) {
        if ((reg->f & 0x20) || (reg->a & 0x0F) > 0x09) {
            correction |= 0x06;
        }
        if ((reg->f & 0x10) || reg->a > 0x99) {
            correction |= 0x60;
            carry = true;
        }
    } else {
        if (reg->f & 0x20) {
            correction |= 0x06;
        }
        if (reg->f & 0x10) {
            correction |= 0x60;
            carry = true;
        }
    }

    if (!(reg->f & 0x40)) {
        reg->a += correction;
    } else {
        reg->a -= correction;
    }

    reg->f &= 0x40;
    if (reg->a == 0)
        reg->f |= 0x80;
    if (carry)
        reg->f |= 0x10;
}

void CPL() {
    reg->a ^= 0xFF;
    reg->f |= 0x60;
}

void CCF() {
    reg->f ^= 0x10;
    reg->f &= ~0x60;
}

void SCF() {
    reg->f &= ~0x60;
    reg->f |= 0x10;
}

void HALT() {
    if (ime) {
        while (!(memory->io[_IF - 0xFF00] & memory->ie & 0x1F))
            update_timers(4);
        handle_interrupts();
    } else if (!(memory->io[_IF - 0xFF00] & memory->ie & 0x1F)) {
        while (!(memory->io[_IF - 0xFF00] & memory->ie & 0x1F))
            update_timers(4);
    } else
        halt_bug = true;
}

void STOP() {
    ++reg->pc;

    if (ime) {
        while (!(memory->io[_IF - 0xFF00] & memory->ie & 0x1F))
            update_timers(4);
    } else {
        while (!(memory->io[_IF - 0xFF00] & memory->ie & 0x1F))
            update_timers(4);
    }
}

void DI() {
    ei = false;
    ime_next = 0;
}

void EI() {
    ei = true;
    ime_next = 1;
}

void RLC_hl() {
    reg->f = 0x0;
    uint8_t val = read_byte(reg->hl);
    uint8_t carry = (val & 0x80) >> 7;
    val = (val << 1) | carry;
    save_byte(reg->hl, val);
    reg->f |= (carry << 4);
    if(val == 0)
        reg->f |= 0x80;
}

void RL_hl() {
    uint8_t val = read_byte(reg->hl);
    uint8_t new = (val & 0x80) >> 7;
    uint8_t old = (reg->f & 0x10) >> 4;
    val = (val << 1) | old;
    save_byte(reg->hl, val);
    reg->f = (new << 4);
    if(val == 0)
        reg->f |= 0x80;
}

void RRC_hl() {
    reg->f = 0x0;
    uint8_t val = read_byte(reg->hl);
    uint8_t carry = (val & 0x01);
    val = (carry << 7) | (val >> 1);
    save_byte(reg->hl, val);
    reg->f |= (carry << 4);
    if(val == 0)
        reg->f |= 0x80;
}

void RR_hl() {
    uint8_t val = read_byte(reg->hl);
    uint8_t new = (val & 0x01);
    uint8_t old = (reg->f & 0x10) << 3;
    val = old | (val >> 1);
    save_byte(reg->hl, val);
    reg->f = (new << 4);
    if(val == 0)
        reg->f |= 0x80;
}

void SL_hl() {
    uint8_t val = read_byte(reg->hl);
    int8_t carry = (val & 0x80) >> 7;
    reg->f = carry << 4;
    val <<= 1;
    save_byte(reg->hl, val);
    if (val == 0)
        reg->f |= 0x80;
}

void SRA_hl() {
    uint8_t val = read_byte(reg->hl);
    uint8_t msb = val & 0x80;
    uint8_t carry = val & 0x01;
    val = msb | (val >> 1);
    save_byte(reg->hl, val);
    reg->f = (carry << 4);
    if (val == 0x0)
        reg->f |= 0x80;
}

void SRL_hl() {
    uint8_t val = read_byte(reg->hl);
    uint8_t carry = val & 0x01;
    val >>= 1;
    save_byte(reg->hl, val);
    reg->f = (carry << 4);
    if (val == 0x0)
        reg->f |= 0x80;
}

void BIT_hl() {
    uint8_t val = read_byte(reg->hl);
    uint8_t bit = (opcode >> 3) & 0x07;
    uint8_t mask = 1 << bit;
    reg->f &= 0x10;
    reg->f |= 0x20;
    if (!(val & mask))
        reg->f |= 0x80;
}

void SET_hl() {
    uint8_t val = read_byte(reg->hl);
    uint8_t bit = (opcode >> 3) & 0x07;
    uint8_t mask = 1 << bit;
    val |= mask;
    save_byte(reg->hl, val);
}

void RESET_hl() {
    uint8_t val = read_byte(reg->hl);
    uint8_t bit = (opcode >> 3) & 0x07;
    uint8_t mask = ~(1 << bit);
    val &= mask;
    save_byte(reg->hl, val);
}

void JP() {
    uint8_t low = read_byte(reg->pc++);
    uint8_t high = read_byte(reg->pc++);
    uint16_t address = (high << 8) | low;
    reg->pc = address;
    update_timers(4);
}

void JP_hl() {
    reg->pc = reg->hl;
}

void JP_COND() {
    bool condition_met = false;
    switch (opcode)
    {
    case 0xC2:
        if (!(reg->f & 0x80))
            condition_met = true;
        break;
    case 0xCA:
        if (reg->f & 0x80)
            condition_met = true;
        break;
    case 0xD2:
        if (!(reg->f & 0x10))
            condition_met = true;
        break;
    case 0xDA:
        if (reg->f & 0x10)
            condition_met = true;
        break;
    }
    if(condition_met){
        uint8_t low = read_byte(reg->pc++);
        uint8_t high = read_byte(reg->pc++);
        uint16_t address = (high << 8) | low ;
        reg->pc = address;
        update_timers(4);
    } else {
        read_byte(reg->pc++); read_byte(reg->pc++);
    }
}

void JR() {
    int8_t n = (int8_t) read_byte(reg->pc++);
    reg->pc = (uint16_t)(reg->pc + n);
    update_timers(4);
}

void JR_COND() {
    bool condition_met = false;
    switch (opcode) {
    case 0x20:
        if (!(reg->f & 0x80)){
            condition_met = true;
        }
        break;

    case 0x28:
        if (reg->f & 0x80){
            condition_met = true;
        }
        break;

    case 0x30:
        if (!(reg->f & 0x10)){
            condition_met = true;
        }
        break;

    case 0x38:
        if (reg->f & 0x10){
            condition_met = true;
        }
        break;
    }
    if(condition_met){
        int8_t val = read_byte(reg->pc++);
        reg->pc = (uint16_t)((int32_t)reg->pc + val);
        update_timers(4);
    } else {
        ++reg->pc;
        update_timers(4);
    }
}

void CALL() {
    uint8_t low = read_byte(reg->pc++);
    uint8_t high = read_byte(reg->pc++);
    uint16_t address = (high<<8) | low;
    if (reg->sp >= 0xFE00 && reg->sp <= 0xFEFF) oam_bug_incdec();
    save_byte(--reg->sp, (reg->pc >> 8) & 0xFF);
    save_byte(--reg->sp, reg->pc & 0xFF);
    reg->pc = address;
    update_timers(4);
}

void CALL_COND() {
    bool condition_met = false;
    uint8_t low = read_byte(reg->pc++);
    uint8_t high = read_byte(reg->pc++);
    uint16_t address = (high<<8) | low;
    switch (opcode) {
    case 0xC4:
        if(!(reg->f & 0x80)) {
            condition_met = true;
        }
        break;
    case 0xCC:
        if(reg->f & 0x80) {
            condition_met = true;
        }
        break;
    case 0xD4:
        if(!(reg->f & 0x10)) {
            condition_met = true;
        }
        break;
    case 0xDC:
        if(reg->f & 0x10) {
            condition_met = true;
        }
        break;
    }
    if(condition_met){
        if (reg->sp >= 0xFE00 && reg->sp <= 0xFEFF) oam_bug_incdec();
        save_byte(--reg->sp, (reg->pc >> 8) & 0xFF);
        save_byte(--reg->sp, reg->pc & 0xFF);
        reg->pc = address;
        update_timers(4);
    }
}

void RST() {
    uint8_t offset = 0x0;
    switch (opcode){
        case 0xC7:
            break;
        case 0xCF:
            offset = 0x08;
            break;
        case 0xD7:
            offset = 0x10;
            break;
        case 0xDF:
            offset = 0x18;
            break;
        case 0xE7:
            offset = 0x20;
            break;
        case 0xEF:
            offset = 0x28;
            break;
        case 0xF7:
            offset = 0x30;
            break;
        case 0xFF:
            offset = 0x38;
            break;
    }
    if (reg->sp >= 0xFE00 && reg->sp <= 0xFEFF) oam_bug_incdec();
    save_byte(--reg->sp, (reg->pc >> 8) & 0xFF);
    save_byte(--reg->sp, reg->pc & 0xFF);
    reg->pc = 0x0000 + offset;
    update_timers(4);
}

void RET() {
    oam_set_read_inc(reg->sp >= 0xFE00 && reg->sp <= 0xFEFF);
    uint8_t low = read_byte(reg->sp++);
    oam_set_read_inc(false);
    uint8_t high = read_byte(reg->sp++);
    uint16_t address = (high<<8) | low;
    reg->pc = address;
    update_timers(4);
}

void RET_COND() {
    bool condition_met = false;
    switch (opcode) {
        case 0xC0:
            if(!(reg->f & 0x80))
                condition_met = true;
            break;
        case 0xC8:
            if( (reg->f & 0x80))
                condition_met = true;
            break;
        case 0xD0:
            if(!(reg->f & 0x10))
                condition_met = true;
            break;
        case 0xD8:
            if( (reg->f & 0x10))
                condition_met = true;
            break;
    }

    if (condition_met) {
        oam_set_read_inc(reg->sp >= 0xFE00 && reg->sp <= 0xFEFF);
        uint8_t low = read_byte(reg->sp++);
        oam_set_read_inc(false);
        uint8_t high = read_byte(reg->sp++);
        reg->pc = (high << 8) | low;
        update_timers(8);
    } else {
        update_timers(4);
    }
}

void RETI() {
    oam_set_read_inc(reg->sp >= 0xFE00 && reg->sp <= 0xFEFF);
    uint8_t low = read_byte(reg->sp++);
    oam_set_read_inc(false);
    uint8_t high = read_byte(reg->sp++);
    uint16_t address = (high<<8) | low;
    reg->pc = address;
    ei = true; ime_next = 0;
    update_timers(4);
}

GEN_RESET_n(a);
GEN_RESET_n(b);
GEN_RESET_n(c);
GEN_RESET_n(d);
GEN_RESET_n(e);
GEN_RESET_n(h);
GEN_RESET_n(l);

GEN_SET_n(a);
GEN_SET_n(b);
GEN_SET_n(c);
GEN_SET_n(d);
GEN_SET_n(e);
GEN_SET_n(h);
GEN_SET_n(l);

GEN_BIT_n(a);
GEN_BIT_n(b);
GEN_BIT_n(c);
GEN_BIT_n(d);
GEN_BIT_n(e);
GEN_BIT_n(h);
GEN_BIT_n(l);

GEN_SWAP_REG(a);
GEN_SWAP_REG(b);
GEN_SWAP_REG(c);
GEN_SWAP_REG(d);
GEN_SWAP_REG(e);
GEN_SWAP_REG(h);
GEN_SWAP_REG(l);

GEN_SRL_n(a);
GEN_SRL_n(b);
GEN_SRL_n(c);
GEN_SRL_n(d);
GEN_SRL_n(e);
GEN_SRL_n(h);
GEN_SRL_n(l);

GEN_SRA_n(a);
GEN_SRA_n(b);
GEN_SRA_n(c);
GEN_SRA_n(d);
GEN_SRA_n(e);
GEN_SRA_n(h);
GEN_SRA_n(l);

GEN_SL_n(a);
GEN_SL_n(b);
GEN_SL_n(c);
GEN_SL_n(d);
GEN_SL_n(e);
GEN_SL_n(h);
GEN_SL_n(l);

GEN_RR_n(a);
GEN_RR_n(b);
GEN_RR_n(c);
GEN_RR_n(d);
GEN_RR_n(e);
GEN_RR_n(h);
GEN_RR_n(l);

GEN_RRC_n(a);
GEN_RRC_n(b);
GEN_RRC_n(c);
GEN_RRC_n(d);
GEN_RRC_n(e);
GEN_RRC_n(h);
GEN_RRC_n(l);

GEN_RL_n(a);
GEN_RL_n(b);
GEN_RL_n(c);
GEN_RL_n(d);
GEN_RL_n(e);
GEN_RL_n(h);
GEN_RL_n(l);

GEN_RLC_n(a);
GEN_RLC_n(b);
GEN_RLC_n(c);
GEN_RLC_n(d);
GEN_RLC_n(e);
GEN_RLC_n(h);
GEN_RLC_n(l);

GEN_DEC_REG16(bc);
GEN_DEC_REG16(de);
GEN_DEC_REG16(hl);
GEN_DEC_REG16(sp);

GEN_INC_REG16(bc);
GEN_INC_REG16(de);
GEN_INC_REG16(hl);
GEN_INC_REG16(sp);

GEN_ADD_HL_REG(bc);
GEN_ADD_HL_REG(de);
GEN_ADD_HL_REG(hl);
GEN_ADD_HL_REG(sp);

GEN_CP_A_REG(a);
GEN_CP_A_REG(b);
GEN_CP_A_REG(c);
GEN_CP_A_REG(d);
GEN_CP_A_REG(e);
GEN_CP_A_REG(h);
GEN_CP_A_REG(l);

GEN_DEC_REG(a);
GEN_DEC_REG(b);
GEN_DEC_REG(c);
GEN_DEC_REG(d);
GEN_DEC_REG(e);
GEN_DEC_REG(h);
GEN_DEC_REG(l);

GEN_INC_REG(a);
GEN_INC_REG(b);
GEN_INC_REG(c);
GEN_INC_REG(d);
GEN_INC_REG(e);
GEN_INC_REG(l);
GEN_INC_REG(h);

GEN_XOR_A_REG(a);
GEN_XOR_A_REG(b);
GEN_XOR_A_REG(c);
GEN_XOR_A_REG(d);
GEN_XOR_A_REG(e);
GEN_XOR_A_REG(h);
GEN_XOR_A_REG(l);

GEN_OR_A_REG(a);
GEN_OR_A_REG(b);
GEN_OR_A_REG(c);
GEN_OR_A_REG(d);
GEN_OR_A_REG(e);
GEN_OR_A_REG(h);
GEN_OR_A_REG(l);

GEN_AND_A_REG(a);
GEN_AND_A_REG(b);
GEN_AND_A_REG(c);
GEN_AND_A_REG(d);
GEN_AND_A_REG(e);
GEN_AND_A_REG(h);
GEN_AND_A_REG(l);

GEN_SUB_A_REG(a);
GEN_SUB_A_REG(b);
GEN_SUB_A_REG(c);
GEN_SUB_A_REG(d);
GEN_SUB_A_REG(e);
GEN_SUB_A_REG(h);
GEN_SUB_A_REG(l);

GEN_SBC_A_REG(a);
GEN_SBC_A_REG(b);
GEN_SBC_A_REG(c);
GEN_SBC_A_REG(d);
GEN_SBC_A_REG(e);
GEN_SBC_A_REG(h);
GEN_SBC_A_REG(l);

GEN_ADD_A_REG(a);
GEN_ADD_A_REG(b);
GEN_ADD_A_REG(c);
GEN_ADD_A_REG(d);
GEN_ADD_A_REG(e);
GEN_ADD_A_REG(h);
GEN_ADD_A_REG(l);

GEN_ADC_A_REG(a);
GEN_ADC_A_REG(b);
GEN_ADC_A_REG(c);
GEN_ADC_A_REG(d);
GEN_ADC_A_REG(e);
GEN_ADC_A_REG(h);
GEN_ADC_A_REG(l);

PUSH_REG16(af);
PUSH_REG16(bc);
PUSH_REG16(de);
PUSH_REG16(hl);

POP_REG16(bc);
POP_REG16(de);
POP_REG16(hl);

GEN_REG_n(bc);
GEN_REG_n(de);

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

GEN_LD_n_hl(a);
GEN_LD_n_hl(b);
GEN_LD_n_hl(c);
GEN_LD_n_hl(d);
GEN_LD_n_hl(e);
GEN_LD_n_hl(h);
GEN_LD_n_hl(l);

instruction_ptr opcode_table[256] = {
    [0x00] = NOP,
    [0x01] = LD_bc_nn,
    [0x02] = SV_bc_a,
    [0x03] = INC_bc,
    [0x04] = INC_b,
    [0x05] = DEC_b,
    [0x06] = LD_b_n,
    [0x07] = RLC_a,
    [0x08] = SV_nn_sp,
    [0x09] = ADD_HL_bc,
    [0x0A] = LD_a_bc,
    [0x0B] = DEC_bc,
    [0x0C] = INC_c,
    [0x0D] = DEC_c,
    [0x0E] = LD_c_n,
    [0x0F] = RRC_a,

    [0x10] = STOP,
    [0x11] = LD_de_nn,
    [0x12] = SV_de_a,
    [0x13] = INC_de,
    [0x14] = INC_d,
    [0x15] = DEC_d,
    [0x16] = LD_d_n,
    [0x17] = RL_a,
    [0x18] = JR,
    [0x19] = ADD_HL_de,
    [0x1A] = LD_a_de,
    [0x1B] = DEC_de,
    [0x1C] = INC_e,
    [0x1D] = DEC_e,
    [0x1E] = LD_e_n,
    [0x1F] = RR_a,

    [0x20] = JR_COND,
    [0x21] = LD_hl_nn,
    [0x22] = SVI_a_hl,
    [0x23] = INC_hl,
    [0x24] = INC_h,
    [0x25] = DEC_h,
    [0x26] = LD_h_n,
    [0x27] = DAA,
    [0x28] = JR_COND,
    [0x29] = ADD_HL_hl,
    [0x2A] = LDI_a_hl,
    [0x2B] = DEC_hl,
    [0x2C] = INC_l,
    [0x2D] = DEC_l,
    [0x2E] = LD_l_n,
    [0x2F] = CPL,

    [0x30] = JR_COND,
    [0x31] = LD_sp_nn,
    [0x32] = SVD_a_hl,
    [0x33] = INC_sp,
    [0x34] = INC_REG_hl,
    [0x35] = DEC_REG_hl,
    [0x36] = SV_hl_n,
    [0x37] = SCF,
    [0x38] = JR_COND,
    [0x39] = ADD_HL_sp,
    [0x3A] = LDD_a_hl,
    [0x3B] = DEC_sp,
    [0x3C] = INC_a,
    [0x3D] = DEC_a,
    [0x3E] = LD_a_n,
    [0x3F] = CCF,

    [0x40] = LD_b_b,
    [0x41] = LD_b_c,
    [0x42] = LD_b_d,
    [0x43] = LD_b_e,
    [0x44] = LD_b_h,
    [0x45] = LD_b_l,
    [0x46] = LD_b_hl,
    [0x47] = LD_b_a,
    [0x48] = LD_c_b,
    [0x49] = LD_c_c,
    [0x4A] = LD_c_d,
    [0x4B] = LD_c_e,
    [0x4C] = LD_c_h,
    [0x4D] = LD_c_l,
    [0x4E] = LD_c_hl,
    [0x4F] = LD_c_a,

    [0x50] = LD_d_b,
    [0x51] = LD_d_c,
    [0x52] = LD_d_d,
    [0x53] = LD_d_e,
    [0x54] = LD_d_h,
    [0x55] = LD_d_l,
    [0x56] = LD_d_hl,
    [0x57] = LD_d_a,
    [0x58] = LD_e_b,
    [0x59] = LD_e_c,
    [0x5A] = LD_e_d,
    [0x5B] = LD_e_e,
    [0x5C] = LD_e_h,
    [0x5D] = LD_e_l,
    [0x5E] = LD_e_hl,
    [0x5F] = LD_e_a,

    [0x60] = LD_h_b,
    [0x61] = LD_h_c,
    [0x62] = LD_h_d,
    [0x63] = LD_h_e,
    [0x64] = LD_h_h,
    [0x65] = LD_h_l,
    [0x66] = LD_h_hl,
    [0x67] = LD_h_a,
    [0x68] = LD_l_b,
    [0x69] = LD_l_c,
    [0x6A] = LD_l_d,
    [0x6B] = LD_l_e,
    [0x6C] = LD_l_h,
    [0x6D] = LD_l_l,
    [0x6E] = LD_l_hl,
    [0x6F] = LD_l_a,

    [0x70] = SV_hl_b,
    [0x71] = SV_hl_c,
    [0x72] = SV_hl_d,
    [0x73] = SV_hl_e,
    [0x74] = SV_hl_h,
    [0x75] = SV_hl_l,
    [0x76] = HALT,
    [0x77] = SV_hl_a,
    [0x78] = LD_a_b,
    [0x79] = LD_a_c,
    [0x7A] = LD_a_d,
    [0x7B] = LD_a_e,
    [0x7C] = LD_a_h,
    [0x7D] = LD_a_l,
    [0x7E] = LD_a_hl,
    [0x7F] = LD_a_a,

    [0x80] = ADD_A_b,
    [0x81] = ADD_A_c,
    [0x82] = ADD_A_d,
    [0x83] = ADD_A_e,
    [0x84] = ADD_A_h,
    [0x85] = ADD_A_l,
    [0x86] = ADD_A_hl,
    [0x87] = ADD_A_a,
    [0x88] = ADC_A_b,
    [0x89] = ADC_A_c,
    [0x8A] = ADC_A_d,
    [0x8B] = ADC_A_e,
    [0x8C] = ADC_A_h,
    [0x8D] = ADC_A_l,
    [0x8E] = ADC_A_hl,
    [0x8F] = ADC_A_a,

    [0x90] = SUB_A_b,
    [0x91] = SUB_A_c,
    [0x92] = SUB_A_d,
    [0x93] = SUB_A_e,
    [0x94] = SUB_A_h,
    [0x95] = SUB_A_l,
    [0x96] = SUB_A_hl,
    [0x97] = SUB_A_a,
    [0x98] = SBC_A_b,
    [0x99] = SBC_A_c,
    [0x9A] = SBC_A_d,
    [0x9B] = SBC_A_e,
    [0x9C] = SBC_A_h,
    [0x9D] = SBC_A_l,
    [0x9E] = SBC_A_hl,
    [0x9F] = SBC_A_a,

    [0xA0] = AND_A_b,
    [0xA1] = AND_A_c,
    [0xA2] = AND_A_d,
    [0xA3] = AND_A_e,
    [0xA4] = AND_A_h,
    [0xA5] = AND_A_l,
    [0xA6] = AND_A_hl,
    [0xA7] = AND_A_a,
    [0xA8] = XOR_A_b,
    [0xA9] = XOR_A_c,
    [0xAA] = XOR_A_d,
    [0xAB] = XOR_A_e,
    [0xAC] = XOR_A_h,
    [0xAD] = XOR_A_l,
    [0xAE] = XOR_A_hl,
    [0xAF] = XOR_A_a,

    [0xB0] = OR_A_b,
    [0xB1] = OR_A_c,
    [0xB2] = OR_A_d,
    [0xB3] = OR_A_e,
    [0xB4] = OR_A_h,
    [0xB5] = OR_A_l,
    [0xB6] = OR_A_hl,
    [0xB7] = OR_A_a,
    [0xB8] = CP_A_b,
    [0xB9] = CP_A_c,
    [0xBA] = CP_A_d,
    [0xBB] = CP_A_e,
    [0xBC] = CP_A_h,
    [0xBD] = CP_A_l,
    [0xBE] = CP_A_hl,
    [0xBF] = CP_A_a,

    [0xC0] = RET_COND,
    [0xC1] = POP_bc,
    [0xC2] = JP_COND,
    [0xC3] = JP,
    [0xC4] = CALL_COND,
    [0xC5] = PUSH_bc,
    [0xC6] = ADD_A_n,
    [0xC7] = RST,
    [0xC8] = RET_COND,
    [0xC9] = RET,
    [0xCA] = JP_COND,
    [0xCB] = prefix_function,
    [0xCC] = CALL_COND,
    [0xCD] = CALL,
    [0xCE] = ADC_A_n,
    [0xCF] = RST,

    [0xD0] = RET_COND,
    [0xD1] = POP_de,
    [0xD2] = JP_COND,
    [0xD3] = NULL,
    [0xD4] = CALL_COND,
    [0xD5] = PUSH_de,
    [0xD6] = SUB_A_n,
    [0xD7] = RST,
    [0xD8] = RET_COND,
    [0xD9] = RETI,
    [0xDA] = JP_COND,
    [0xDB] = NULL,
    [0xDC] = CALL_COND,
    [0xDD] = NULL,
    [0xDE] = SBC_A_imm,
    [0xDF] = RST,

    [0xE0] = SVH_imm_a,
    [0xE1] = POP_hl,
    [0xE2] = SLD_a_c,
    [0xE3] = NULL,
    [0xE4] = NULL,
    [0xE5] = PUSH_hl,
    [0xE6] = AND_A_n,
    [0xE7] = RST,
    [0xE8] = ADD_SP_n,
    [0xE9] = JP_hl,
    [0xEA] = SV_nn_a,
    [0xEB] = NULL,
    [0xEC] = NULL,
    [0xED] = NULL,
    [0xEE] = XOR_A_n,
    [0xEF] = RST,

    [0xF0] = LDH_imm_a,
    [0xF1] = POP_af,
    [0xF2] = LDH_a_c,
    [0xF3] = DI,
    [0xF4] = NULL,
    [0xF5] = PUSH_af,
    [0xF6] = OR_A_n,
    [0xF7] = RST,
    [0xF8] = LDHL_sp_n,
    [0xF9] = LD_sp_hl,
    [0xFA] = LD_a_nn,
    [0xFB] = EI,
    [0xFC] = NULL,
    [0xFD] = NULL,
    [0xFE] = CP_A_n,
    [0xFF] = RST,
};

instruction_ptr prefix_opcode_table[256] = {
    [0x00] = RLC_b,
    [0x01] = RLC_c,
    [0x02] = RLC_d,
    [0x03] = RLC_e,
    [0x04] = RLC_h,
    [0x05] = RLC_l,
    [0x06] = RLC_hl,
    [0x07] = RLC_a,
    [0x08] = RRC_b,
    [0x09] = RRC_c,
    [0x0A] = RRC_d,
    [0x0B] = RRC_e,
    [0x0C] = RRC_h,
    [0x0D] = RRC_l,
    [0x0E] = RRC_hl,
    [0x0F] = RRC_a,

    [0x10] = RL_b,
    [0x11] = RL_c,
    [0x12] = RL_d,
    [0x13] = RL_e,
    [0x14] = RL_h,
    [0x15] = RL_l,
    [0x16] = RL_hl,
    [0x17] = RL_a,
    [0x18] = RR_b,
    [0x19] = RR_c,
    [0x1A] = RR_d,
    [0x1B] = RR_e,
    [0x1C] = RR_h,
    [0x1D] = RR_l,
    [0x1E] = RR_hl,
    [0x1F] = RR_a,

    [0x20] = SL_b,
    [0x21] = SL_c,
    [0x22] = SL_d,
    [0x23] = SL_e,
    [0x24] = SL_h,
    [0x25] = SL_l,
    [0x26] = SL_hl,
    [0x27] = SL_a,
    [0x28] = SRA_b,
    [0x29] = SRA_c,
    [0x2A] = SRA_d,
    [0x2B] = SRA_e,
    [0x2C] = SRA_h,
    [0x2D] = SRA_l,
    [0x2E] = SRA_hl,
    [0x2F] = SRA_a,

    [0x30] = SWAP_b,
    [0x31] = SWAP_c,
    [0x32] = SWAP_d,
    [0x33] = SWAP_e,
    [0x34] = SWAP_h,
    [0x35] = SWAP_l,
    [0x36] = SWAP_hl,
    [0x37] = SWAP_a,
    [0x38] = SRL_b,
    [0x39] = SRL_c,
    [0x3A] = SRL_d,
    [0x3B] = SRL_e,
    [0x3C] = SRL_h,
    [0x3D] = SRL_l,
    [0x3E] = SRL_hl,
    [0x3F] = SRL_a,

    [0x40] = BIT_b,
    [0x41] = BIT_c,
    [0x42] = BIT_d,
    [0x43] = BIT_e,
    [0x44] = BIT_h,
    [0x45] = BIT_l,
    [0x46] = BIT_hl,
    [0x47] = BIT_a,
    [0x48] = BIT_b,
    [0x49] = BIT_c,
    [0x4A] = BIT_d,
    [0x4B] = BIT_e,
    [0x4C] = BIT_h,
    [0x4D] = BIT_l,
    [0x4E] = BIT_hl,
    [0x4F] = BIT_a,

    [0x50] = BIT_b,
    [0x51] = BIT_c,
    [0x52] = BIT_d,
    [0x53] = BIT_e,
    [0x54] = BIT_h,
    [0x55] = BIT_l,
    [0x56] = BIT_hl,
    [0x57] = BIT_a,
    [0x58] = BIT_b,
    [0x59] = BIT_c,
    [0x5A] = BIT_d,
    [0x5B] = BIT_e,
    [0x5C] = BIT_h,
    [0x5D] = BIT_l,
    [0x5E] = BIT_hl,
    [0x5F] = BIT_a,

    [0x60] = BIT_b,
    [0x61] = BIT_c,
    [0x62] = BIT_d,
    [0x63] = BIT_e,
    [0x64] = BIT_h,
    [0x65] = BIT_l,
    [0x66] = BIT_hl,
    [0x67] = BIT_a,
    [0x68] = BIT_b,
    [0x69] = BIT_c,
    [0x6A] = BIT_d,
    [0x6B] = BIT_e,
    [0x6C] = BIT_h,
    [0x6D] = BIT_l,
    [0x6E] = BIT_hl,
    [0x6F] = BIT_a,

    [0x70] = BIT_b,
    [0x71] = BIT_c,
    [0x72] = BIT_d,
    [0x73] = BIT_e,
    [0x74] = BIT_h,
    [0x75] = BIT_l,
    [0x76] = BIT_hl,
    [0x77] = BIT_a,
    [0x78] = BIT_b,
    [0x79] = BIT_c,
    [0x7A] = BIT_d,
    [0x7B] = BIT_e,
    [0x7C] = BIT_h,
    [0x7D] = BIT_l,
    [0x7E] = BIT_hl,
    [0x7F] = BIT_a,

    [0x80] = RESET_b,
    [0x81] = RESET_c,
    [0x82] = RESET_d,
    [0x83] = RESET_e,
    [0x84] = RESET_h,
    [0x85] = RESET_l,
    [0x86] = RESET_hl,
    [0x87] = RESET_a,
    [0x88] = RESET_b,
    [0x89] = RESET_c,
    [0x8A] = RESET_d,
    [0x8B] = RESET_e,
    [0x8C] = RESET_h,
    [0x8D] = RESET_l,
    [0x8E] = RESET_hl,
    [0x8F] = RESET_a,

    [0x90] = RESET_b,
    [0x91] = RESET_c,
    [0x92] = RESET_d,
    [0x93] = RESET_e,
    [0x94] = RESET_h,
    [0x95] = RESET_l,
    [0x96] = RESET_hl,
    [0x97] = RESET_a,
    [0x98] = RESET_b,
    [0x99] = RESET_c,
    [0x9A] = RESET_d,
    [0x9B] = RESET_e,
    [0x9C] = RESET_h,
    [0x9D] = RESET_l,
    [0x9E] = RESET_hl,
    [0x9F] = RESET_a,

    [0xA0] = RESET_b,
    [0xA1] = RESET_c,
    [0xA2] = RESET_d,
    [0xA3] = RESET_e,
    [0xA4] = RESET_h,
    [0xA5] = RESET_l,
    [0xA6] = RESET_hl,
    [0xA7] = RESET_a,
    [0xA8] = RESET_b,
    [0xA9] = RESET_c,
    [0xAA] = RESET_d,
    [0xAB] = RESET_e,
    [0xAC] = RESET_h,
    [0xAD] = RESET_l,
    [0xAE] = RESET_hl,
    [0xAF] = RESET_a,

    [0xB0] = RESET_b,
    [0xB1] = RESET_c,
    [0xB2] = RESET_d,
    [0xB3] = RESET_e,
    [0xB4] = RESET_h,
    [0xB5] = RESET_l,
    [0xB6] = RESET_hl,
    [0xB7] = RESET_a,
    [0xB8] = RESET_b,
    [0xB9] = RESET_c,
    [0xBA] = RESET_d,
    [0xBB] = RESET_e,
    [0xBC] = RESET_h,
    [0xBD] = RESET_l,
    [0xBE] = RESET_hl,
    [0xBF] = RESET_a,

    [0xC0] = SET_b,
    [0xC1] = SET_c,
    [0xC2] = SET_d,
    [0xC3] = SET_e,
    [0xC4] = SET_h,
    [0xC5] = SET_l,
    [0xC6] = SET_hl,
    [0xC7] = SET_a,
    [0xC8] = SET_b,
    [0xC9] = SET_c,
    [0xCA] = SET_d,
    [0xCB] = SET_e,
    [0xCC] = SET_h,
    [0xCD] = SET_l,
    [0xCE] = SET_hl,
    [0xCF] = SET_a,

    [0xD0] = SET_b,
    [0xD1] = SET_c,
    [0xD2] = SET_d,
    [0xD3] = SET_e,
    [0xD4] = SET_h,
    [0xD5] = SET_l,
    [0xD6] = SET_hl,
    [0xD7] = SET_a,
    [0xD8] = SET_b,
    [0xD9] = SET_c,
    [0xDA] = SET_d,
    [0xDB] = SET_e,
    [0xDC] = SET_h,
    [0xDD] = SET_l,
    [0xDE] = SET_hl,
    [0xDF] = SET_a,

    [0xE0] = SET_b,
    [0xE1] = SET_c,
    [0xE2] = SET_d,
    [0xE3] = SET_e,
    [0xE4] = SET_h,
    [0xE5] = SET_l,
    [0xE6] = SET_hl,
    [0xE7] = SET_a,
    [0xE8] = SET_b,
    [0xE9] = SET_c,
    [0xEA] = SET_d,
    [0xEB] = SET_e,
    [0xEC] = SET_h,
    [0xED] = SET_l,
    [0xEE] = SET_hl,
    [0xEF] = SET_a,

    [0xF0] = SET_b,
    [0xF1] = SET_c,
    [0xF2] = SET_d,
    [0xF3] = SET_e,
    [0xF4] = SET_h,
    [0xF5] = SET_l,
    [0xF6] = SET_hl,
    [0xF7] = SET_a,
    [0xF8] = SET_b,
    [0xF9] = SET_c,
    [0xFA] = SET_d,
    [0xFB] = SET_e,
    [0xFC] = SET_h,
    [0xFD] = SET_l,
    [0xFE] = SET_hl,
    [0xFF] = SET_a,
};

void prefix_function() {
    prefix_flag = true;
    opcode = read_byte(reg->pc++);
    prefix_opcode_table[opcode]();
    prefix_flag = false;
}
