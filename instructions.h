#ifndef FUNCTIONS_H
#include "structs.h"
#include <SDL2/SDL.h>
#define FUNCTIONS_H

extern GameBoyMemory *memory;
extern registers *reg;
extern uint8_t opcode;

extern SDL_Texture *ppu_texture;
extern SDL_Renderer *ppu_renderer;
extern uint32_t framebuffer[160 * 144];

extern uint8_t joypad_dpad;
extern uint8_t joypad_btn;
extern const uint32_t shades[4];

void prefix_function();

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
        if (address == _JOYP) {
            uint8_t val = 0xC0 | (memory->io[0] & 0x30);
            if (!(memory->io[0] & 0x10)) val |= joypad_dpad;  // d-pad column
            if (!(memory->io[0] & 0x20)) val |= joypad_btn;   // button column
            return val;
        }
        return memory->io[address - 0xFF00];
    } else if (address >= 0xFF80 && address <= 0xFFFE){
        return memory->hram[address - 0xFF80];
    } else if (address == 0xFFFF){
        return memory->ie;
    }
    return 0xFF;
}

int save_byte(uint16_t address, uint8_t val){
    if (address >= 0x0000 && address <= 0x7FFF) {
        memory->rom[address] = val;
    } else if (address >= 0x8000 && address <= 0x9FFF) {
        memory->vram[address - 0x8000] = val;
    } else if (address >= 0xA000 && address <= 0xBFFF) {
        memory->external[address - 0xA000] = val;
    } else if (address >= 0xC000 && address <= 0xDFFF) {
        memory->wram[address - 0xC000] = val;
    } else if (address >= 0xE000 && address <= 0xFDFF) {
        memory->wram[address - 0xE000] = val;
    } else if (address >= 0xFE00 && address <= 0xFE9F) {
        memory->oam[address - 0xFE00] = val;
    } else if (address >= 0xFF00 && address <= 0xFF7F){
        if (address == _DIV) internalClock = 0;
        else if (address == _JOYP) memory->io[0] = val & 0x30;
        else if (address == _DMA) {
            uint16_t src = val << 8;
            for (int i = 0; i < 0xA0; i++)
                memory->oam[i] = read_byte(src + i);
            update_timers(640);
        }
        else memory->io[address - 0xFF00] = val;
    } else if (address >= 0xFF80 && address <= 0xFFFE){
        memory->hram[address - 0xFF80] = val;
    } else if (address == 0xFFFF){
        memory->ie = val;
    } else {
        exit(EXIT_FAILURE);
    }
    return 0;
}

void init_io_ports(void) {
    // Input Default
    save_byte(_JOYP, 0x30);


    save_byte(_LY,   0x00);
    save_byte(_STAT, 0x00);

    // Timer Defaults
    save_byte(_TIMA, 0x0);
    save_byte(_TMA, 0x0);
    save_byte(_TAC, 0x0);

    // Audio Defaults (Channel 1)
    save_byte(_NR10, 0x80);
    save_byte(_NR11, 0xBF);
    save_byte(_NR12, 0xF3);
    save_byte(_NR14, 0xBF);
    
    // Audio Defaults (Channel 2)
    save_byte(_NR21, 0x3F);
    save_byte(_NR22, 0x0);
    save_byte(_NR24, 0xBF);

    // Audio Defaults (Channel 3)
    save_byte(_NR30, 0x7F);
    save_byte(_NR31, 0xFF);
    save_byte(_NR32, 0x9F);
    save_byte(_NR33, 0xBF);

    // Audio Channel 4 & Master Controls
    save_byte(_NR41, 0xFF);
    save_byte(_NR42, 0x00);
    save_byte(_NR43, 0x00);
    save_byte(_NR44, 0xBF);
    save_byte(_NR50, 0x77);
    save_byte(_NR51, 0xF3);
    save_byte(_NR52, 0xF1); 

    // Video / PPU
    save_byte(_LCDC, 0x91);
    save_byte(_SCY,  0x00);
    save_byte(_SCX,  0x00);
    save_byte(_LYC,  0x00);
    save_byte(_BGP,  0xFC);
    save_byte(_OBP0, 0xFF);
    save_byte(_OBP1, 0xFF);
    save_byte(_WY,   0x00);
    save_byte(_WX,   0x00);

    // Interrupt Enable
    save_byte(_IF,   0x00);
    save_byte(_IE,   0x00);
}

void render_scanline(uint8_t ly) {
    uint8_t lcdc = memory->io[_LCDC - 0xFF00];

    if (!(lcdc & 0x80)) {
        for (int x = 0; x < 160; x++)
            framebuffer[ly * 160 + x] = shades[0];
        return;
    }

    uint8_t bgp  = memory->io[_BGP  - 0xFF00];
    uint8_t scy  = memory->io[_SCY  - 0xFF00];
    uint8_t scx  = memory->io[_SCX  - 0xFF00];

    bool signed_mode = !(lcdc & 0x10);

    // Background
    if (lcdc & 0x01) {
        uint16_t map_base = (lcdc & 0x08) ? 0x9C00 : 0x9800;

        for (int x = 0; x < 160; ++x) {
            uint16_t scroll_x = (x + scx) & 0xFF;
            uint16_t scroll_y = (ly + scy) & 0xFF;
            uint16_t map_addr = map_base + (scroll_y / 8) * 32 + (scroll_x / 8);
            int8_t tile = memory->vram[map_addr - 0x8000];

            uint16_t tile_addr;
            if (signed_mode)
                tile_addr = 0x9000 + tile * 16;
            else
                tile_addr = 0x8000 + tile * 16;

            uint8_t pixel_x = scroll_x & 7;
            uint8_t pixel_y = scroll_y & 7;

            uint8_t byte0 = memory->vram[tile_addr + pixel_y * 2 - 0x8000];
            uint8_t byte1 = memory->vram[tile_addr + pixel_y * 2 + 1 - 0x8000];

            uint8_t color = ((byte1 >> (7 - pixel_x)) & 1) << 1 | ((byte0 >> (7 - pixel_x)) & 1);
            uint8_t shade = (bgp >> (color * 2)) & 3;
            framebuffer[ly * 160 + x] = shades[shade];
        }
    } else {
        for (int x = 0; x < 160; x++)
            framebuffer[ly * 160 + x] = shades[0];
    }

    // Window 
    if (lcdc & 0x20) {
        uint8_t wy = memory->io[_WY - 0xFF00];
        uint8_t wx = memory->io[_WX - 0xFF00];
        wx = (wx >= 7) ? wx - 7 : 0;

        if (ly >= wy) {
            uint16_t win_map = (lcdc & 0x40) ? 0x9C00 : 0x9800;

            for (int x = 0; x < 160; x++) {
                int win_x = x - wx;
                if (win_x < 0) continue;

                uint16_t map_addr = win_map + ((ly - wy) / 8) * 32 + (win_x / 8);
                int8_t tile = memory->vram[map_addr - 0x8000];

                uint16_t tile_addr;
                if (signed_mode)
                    tile_addr = 0x9000 + tile * 16;
                else
                    tile_addr = 0x8000 + tile * 16;

                uint8_t pixel_x = win_x & 7;
                uint8_t pixel_y = (ly - wy) & 7;

                uint8_t byte0 = memory->vram[tile_addr + pixel_y * 2 - 0x8000];
                uint8_t byte1 = memory->vram[tile_addr + pixel_y * 2 + 1 - 0x8000];

                uint8_t color = ((byte1 >> (7 - pixel_x)) & 1) << 1 |
                                ((byte0 >> (7 - pixel_x)) & 1);
                uint8_t shade = (bgp >> (color * 2)) & 3;
                framebuffer[ly * 160 + x] = shades[shade];
            }
        }
    }

    // Layer 3: Sprites
    if (lcdc & 0x02) {
        uint8_t obp0 = memory->io[_OBP0 - 0xFF00];
        uint8_t obp1 = memory->io[_OBP1 - 0xFF00];
        int height = (lcdc & 0x04) ? 16 : 8;

        uint8_t visible[10];
        int count = 0;
        for (int i = 0; i < 40 && count < 10; i++) {
            int sprite_y = memory->oam[i * 4] - 16;
            if (ly >= sprite_y && ly < sprite_y + height)
                visible[count++] = i;
        }

        for (int i = 0; i < count; i++) {
            int idx = visible[i];
            int sprite_y = memory->oam[idx * 4] - 16;
            int sprite_x = memory->oam[idx * 4 + 1] - 8;
            uint8_t tile = memory->oam[idx * 4 + 2];
            uint8_t flags = memory->oam[idx * 4 + 3];

            if (height == 16) tile &= 0xFE;

            int y_in = ly - sprite_y;
            if (flags & 0x40) y_in = height - 1 - y_in;

            for (int x = sprite_x; x < sprite_x + 8 && x < 160; x++) {
                if (x < 0) continue;

                int pixel_x = x - sprite_x;
                if (flags & 0x20) pixel_x = 7 - pixel_x;

                uint16_t tile_addr = 0x8000 + tile * 16 + y_in * 2;
                uint8_t byte0 = memory->vram[tile_addr - 0x8000];
                uint8_t byte1 = memory->vram[tile_addr + 1 - 0x8000];

                uint8_t color = ((byte1 >> (7 - pixel_x)) & 1) << 1 |
                                ((byte0 >> (7 - pixel_x)) & 1);
                if (color == 0) continue;

                uint8_t pal = (flags & 0x10) ? obp1 : obp0;
                uint8_t shade = (pal >> (color * 2)) & 3;

                if (!(flags & 0x80) || framebuffer[ly * 160 + x] == shades[0])
                    framebuffer[ly * 160 + x] = shades[shade];
            }
        }
    }

    // Per-scanline texture upload
    SDL_UpdateTexture(ppu_texture, &(SDL_Rect){0, ly, 160, 1},
                      &framebuffer[ly * 160], 160 * sizeof(uint32_t));
}

void update_ppu(uint8_t cycles) {
    static uint16_t ppu_cycle = 0;

    // Check LCD enable
    uint8_t lcdc = memory->io[_LCDC - 0xFF00];
    if (!(lcdc & 0x80)) return;  

    uint8_t ly = memory->io[_LY - 0xFF00];
    uint8_t lyc = memory->io[_LYC - 0xFF00];
    uint8_t stat = memory->io[_STAT - 0xFF00];

    ppu_cycle += cycles;

    while (ppu_cycle >= 456) {
        ppu_cycle -= 456;

        if (ly < 144)
            render_scanline(ly);
        ly++;

        if (ly == lyc) {
            if (!(stat & 0x04)) {
                stat |= 0x04; // set ly == lyc
                if (stat & 0x40)
                    memory->io[_IF - 0xFF00] |= 0x02;  // STAT INT
            }
        } else {
            stat &= ~0x04;
        }

        if (ly == 144) {
            memory->io[_IF - 0xFF00] |= 0x01;       // VBlank INT
            stat = (stat & 0xFC) | 0x01;             // set mode 1
            if (stat & 0x10)
                memory->io[_IF - 0xFF00] |= 0x02;    // STAT INT 
            SDL_RenderCopy(ppu_renderer, ppu_texture, NULL, NULL);
            SDL_RenderPresent(ppu_renderer);
        } else if (ly >= 145 && ly <= 153) {
            stat = (stat & 0xFC) | 0x01;              // Stay in VBlank
        } else if (ly == 154) {
            ly = 0;
            stat = (stat & 0xFC) | 0x02;              // set mode 2
        } else {
            stat = (stat & 0xFC) | 0x02;              //set mode 2
        }
        memory->io[_LY - 0xFF00] = ly;
    }

    if (ly < 144 && (lcdc & 0x80)) {
        uint8_t old_mode = stat & 0x03;
        uint8_t new_mode;

        if (ppu_cycle < 80)       new_mode = 2;  // OAM search
        else if (ppu_cycle < 252) new_mode = 3;  // Pixel transfer
        else                      new_mode = 0;  // HBlank

        if (new_mode != old_mode) {
            if ((new_mode == 0 && (stat & 0x08)) || (new_mode == 2 && (stat & 0x20))) 
                memory->io[_IF - 0xFF00] |= 0x02;
        }
        stat = (stat & 0xFC) | new_mode;
    }

    memory->io[_STAT - 0xFF00] = stat;
}

void update_timers(uint8_t cycles) {
    uint8_t tac = memory->io[_TAC - 0xFF00];
    for (int i = 0; i < cycles; i++) {
        uint16_t oldClock = internalClock;
        internalClock++;
        // Check for overflow if timer is enabled
        if (tac & 0x04) {
            static const uint8_t bitPos[] = {9, 3, 5, 7}; 
            uint8_t bit = bitPos[tac & 0x03];  
            int oldBit = (oldClock >> bit) & 1;
            int newBit = (internalClock >> bit) & 1;
            if (oldBit == 1 && newBit == 0) {
                uint8_t tima = memory->io[_TIMA - 0xFF00] + 1;
                if (tima == 0) {
                    tima = memory->io[_TMA - 0xFF00];
                    uint8_t if_reg = memory->io[_IF - 0xFF00];
                    memory->io[_IF - 0xFF00] = if_reg | 0x04;
                }
                memory->io[_TIMA - 0xFF00] = tima;
            }
        }
    }
    update_ppu(cycles);
    memory->io[_DIV - 0xff00] = (internalClock >> 8) & 0xFF;
}

void handle_interrupts() {
    uint8_t pending = memory->io[_IF - 0xFF00] & memory->ie & 0x1F;
    if (!pending) return;

    static const uint16_t vectors[] = {0x40, 0x48, 0x50, 0x58, 0x60};
    for (int i = 0; i < 5; i++) {
        uint8_t bit = 1 << i;
        if (pending & bit) {
            ime = false;
            update_timers(20);
            save_byte(--reg->sp, (reg->pc >> 8) & 0xFF);
            save_byte(--reg->sp, reg->pc & 0xFF);
            memory->io[_IF - 0xFF00] &= ~bit;

            reg->pc = vectors[i];
            break;
        }
    }
}

// load inmediate value into register (checked)
#define GEN_LD_N(reg_name) \
void LD_##reg_name##_n() { \
    reg->reg_name = read_byte(reg->pc++); \
    update_timers(8); \
} 

// load value from [hl] into reg_name (checked)
#define GEN_LD_n_hl(reg_name) \
void LD_##reg_name##_hl() { \
    reg->reg_name = read_byte(reg->hl); \
    update_timers(8); \
}

// copy value from r2 to r1 (checked)
#define GEN_LD_R1_R2(r1, r2) \
void LD_##r1##_##r2() { \
    reg->r1 = reg->r2; \
    update_timers(4); \
}

// load value from memory in [reg_name] into register A (checked)
#define GEN_REG_n(reg_name) \
void LD_a_##reg_name() { \
    reg->a = read_byte(reg->reg_name); \
    update_timers(8); \
}

// load value from memory in [nn], 16 bit value, into register
#define GEN_REG_NN(reg_name) \
void LD_##reg_name##_nn() { \
    uint8_t low = read_byte(reg->pc ++); \
    uint8_t high = read_byte(reg->pc ++); \
    uint16_t address = (high << 8) | low; \
    reg->reg_name = read_byte(address); \
    update_timers(16); \
}

// save value from register into memory in [register] (checked)
#define GEN_LD_ADDR_R(reg_addr, reg_name) \
void SV_##reg_addr##_##reg_name() { \
    save_byte(reg->reg_addr, reg->reg_name); \
    update_timers(8); \
}

// load form value in reg2 + 0xFF00 ($FF00) into reg1
#define GEN_LD_REG_REG(reg1, reg2) \
void SLD_##reg1##_##reg2() { \
    reg->reg1 = 0xFF00 |reg->reg2; \
    update_timers(8); \
}

// save form value in reg1 into [reg2 + 0xFF00 ($FF00)]
#define GEN_SV_REG_REG(reg1, reg2) \
void SLD_##reg1##_##reg2() { \
    save_byte(0xFF00 | reg->reg2, reg->reg1); \
    update_timers(8);  \
}

// load inmediate 16 bit value into register of 16 bit (check)
#define GEN_LD_IMM_REG16(reg_name) \
void LD_##reg_name##_nn() { \
    uint8_t low = read_byte(reg->pc++); \
    uint8_t high = read_byte(reg->pc++); \
    uint16_t value = high << 8 | low; \
    reg->reg_name = value; \
    update_timers(12); \
}

// Push register onto the stack, decrementing SP by 2
#define PUSH_REG16(reg_name) \
void PUSH_##reg_name() { \
    save_byte(--reg->sp, (uint8_t)(reg->reg_name >> 8)); \
    save_byte(--reg->sp, (uint8_t)(reg->reg_name & 0xFF)); \
    update_timers(16); \
}

// Pop register off the stack, incrementing SP by 2
#define POP_REG16(reg_name) \
void POP_##reg_name() { \
    uint8_t low = read_byte(reg->sp++); \
    uint8_t high = read_byte(reg->sp++); \
    reg->reg_name =  (high << 8) | low; \
    update_timers(12); \
}

// ADD reg to register A
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
    update_timers(4); \
}

// ADD reg and carry bit to register A
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
    update_timers(4); \
}

// SUB reg to register A
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
    update_timers(4); \
}

// SUB reg and carry bit to register A
#define GEN_SBC_A_REG(reg_name) \
void SBC_A_##reg_name(){ \
    uint8_t val = reg->reg_name; \
    uint8_t carry = (reg->f & 0x10) ? 1 : 0; \
    int16_t result = (int16_t)reg->a - (int16_t)val - carry; \
    reg->f = 0x40; \
    if ((result & 0xFF) == 0) \
        reg->f |= 0x80; \
    if (((reg->a & 0x0F) - (val & 0x0F)) < 0) \
        reg->f |= 0x20; \
    if (result < 0) \
        reg->f |= 0x10; \
    reg->a = (uint8_t)result; \
    update_timers(4); \
}

// AND between register A and register
#define GEN_AND_A_REG(reg_name) \
void AND_A_##reg_name() { \
    reg->f = 0x20; \
    if ((reg->a & reg->reg_name) == 0) \
        reg->f |= 0x80; \
    reg->a &= reg->reg_name; \
    update_timers(4); \
}

// OR between register A and register
#define GEN_OR_A_REG(reg_name) \
void OR_A_##reg_name() { \
    reg->f = 0x0; \
    if ((reg->a | reg->reg_name) == 0) \
        reg->f |= 0x80; \
    reg->a |= reg->reg_name; \
    update_timers(4); \
}

// XOR between register A and register
#define GEN_XOR_A_REG(reg_name) \
void XOR_A_##reg_name() { \
    reg->f = 0x0; \
    if ((reg->a ^ reg->reg_name)== 0) \
        reg->f |= 0x80; \
    reg->a ^= reg->reg_name; \
    update_timers(4); \
}

// CMP between register A and register
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
    update_timers(4); \
}

// INC register
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
    update_timers(4); \
}

// DEC register
#define GEN_DEC_REG(reg_name) \
void DEC_##reg_name() { \
    reg->f &= 0x10; \
    reg->f |= 0x40; \
    if((reg->reg_name & 0xF) == 0x0) \
        reg->f |= 0x20; \
    --reg->reg_name; \
    if(reg->reg_name == 0) \
        reg->f |= 0x80; \
    update_timers(4); \
}

// ADD register into hl register
#define GEN_ADD_HL_REG(reg_name) \
void ADD_HL_##reg_name(){ \
    reg->f &= 0x80; \
    if(reg->hl + reg->reg_name > 0xFFFF) \
        reg->f |= 0x10; \
    if((reg->hl & 0xFFF) + (reg->reg_name & 0xFFF) > 0xFFF) \
        reg->f |= 0x20; \
    reg->hl += reg->reg_name; \
    update_timers(8); \
}

// increment register of 16 bits
#define GEN_INC_REG16(reg_name) \
void INC_##reg_name(){ \
    ++reg->reg_name; \
    update_timers(8); \
}

// decrement register of 16 bits
#define GEN_DEC_REG16(reg_name) \
void DEC_##reg_name(){ \
    --reg->reg_name; \
    update_timers(8); \
}

// swap bits 0-3 and 4-7
#define GEN_SWAP_REG(reg_name) \
void SWAP_##reg_name() { \
    reg->f = 0x0; \
    if (reg->reg_name == 0x0) \
        reg-> f = 0x80; \
    reg->reg_name = (reg->reg_name << 4) | (reg->reg_name >> 4); \
    update_timers(8); \
}

#define GEN_RLC_n(reg_name) \
void RLC_##reg_name() { \
    reg->f = 0x0; \
    uint8_t carry = (reg->reg_name & 0x80) >> 7; \
    reg->reg_name = (reg->reg_name << 1) | carry; \
    reg->f |= (carry << 4); \
    if (prefix_flag && reg->reg_name == 0) \
        reg->f |= 0x80; \
    if (prefix_flag) update_timers(8); \
    else update_timers(4); \
}

#define GEN_RL_n(reg_name) \
void RL_##reg_name() { \
    uint8_t new = (reg->reg_name & 0x80) >> 7; \
    uint8_t old = (reg->f & 0x10) >> 4; \
    reg->reg_name = (reg->reg_name << 1) | old; \
    reg->f = (new << 4); \
    if (prefix_flag && reg->reg_name == 0) \
        reg->f |= 0x80; \
    if (prefix_flag) update_timers(8); \
    else update_timers(4); \
}

#define GEN_RRC_n(reg_name) \
void RRC_##reg_name() { \
    reg->f = 0x0; \
    uint8_t carry = (reg->reg_name & 0x01); \
    reg->reg_name = (carry << 7) | (reg->reg_name >> 1); \
    reg->f |= (carry << 4); \
    if (prefix_flag && reg->reg_name == 0) \
        reg->f |= 0x80; \
    if (prefix_flag) update_timers(8); \
    else update_timers(4); \
}

#define GEN_RR_n(reg_name) \
void RR_##reg_name() { \
    uint8_t new = (reg->reg_name & 0x01); \
    uint8_t old = (reg->f & 0x10) << 3; \
    reg->reg_name = old | (reg->reg_name >> 1); \
    reg->f = (new << 4); \
    if (prefix_flag && reg->reg_name == 0) \
        reg->f |= 0x80; \
    if (prefix_flag) update_timers(8); \
    else update_timers(4); \
}

#define GEN_SL_n(reg_name) \
void SL_##reg_name() { \
    uint8_t carry = (reg->reg_name & 0x80) >> 7; \
    reg->f = carry << 4; \
    reg->reg_name <<= 1; \
    if (reg->reg_name == 0) \
        reg->f |= 0x80; \
    if (prefix_flag) update_timers(8); \
    else update_timers(4); \
}

#define GEN_SRA_n(reg_name) \
void SRA_##reg_name() { \
    uint8_t msb = reg->reg_name & 0x80; \
    uint8_t carry = reg->reg_name & 0x01; \
    reg->reg_name = msb | (reg->reg_name >> 1); \
    reg->f = (carry << 4); \
    if (reg->reg_name == 0x0) \
        reg->f |= 0x80; \
    if (prefix_flag) update_timers(8); \
    else update_timers(4); \
}

#define GEN_SRL_n(reg_name) \
void SRL_##reg_name() { \
    uint8_t carry = reg->reg_name & 0x01; \
    reg->reg_name >>= 1; \
    reg->f = (carry << 4); \
    if (reg->reg_name == 0x0) \
        reg->f |= 0x80; \
    if (prefix_flag) update_timers(8); \
    else update_timers(4); \
}

#define GEN_BIT_n(reg_name) \
void BIT_##reg_name() { \
    uint8_t bit = (opcode >> 3) & 0x07; \
    uint8_t mask = 1 << bit; \
    reg->f &= 0x10; \
    reg->f |= 0x20; \
    if (!(reg->reg_name & mask)) \
        reg->f |= 0x80; \
    update_timers(8); \
}

#define GEN_SET_n(reg_name) \
void SET_##reg_name() { \
    uint8_t bit = (opcode >> 3) & 0x07; \
    uint8_t mask = 1 << bit; \
    reg->reg_name |= mask; \
    update_timers(8); \
}

#define GEN_RESET_n(reg_name) \
void RESET_##reg_name() { \
    uint8_t bit = (opcode >> 3) & 0x07; \
    uint8_t mask = ~(1 << bit); \
    reg->reg_name &= mask;\
    update_timers(8); \
}


void NOP(){
    update_timers(4);
}

// Put HL into Stack Pointer (SP)
void LD_sp_hl() {
    reg->sp = reg->hl;
    update_timers(8);
}

// save n8 in [hl] (checked)
void SV_hl_n() { \
    uint8_t val = read_byte(reg->pc++); \
    save_byte(reg->hl, val); \
    update_timers(12); \
}

// Put value A into [nn]
void SV_nn_a() {
    uint8_t low = read_byte(reg->pc++);
    uint8_t high = read_byte(reg->pc++);
    uint16_t address = low | (high << 8);
    save_byte(address, reg->a);
    update_timers(16);
}

// Save value from A into memory in [0xFF00 + n], where n is an 8 bit immediate value
void SVH_imm_a(){
    uint16_t address = 0xFF00 | read_byte(reg->pc++);
    save_byte(address, reg->a);
    update_timers(12);
}

// Load value from memory in [0xFF00 + n], where n is an 8 bit immediate value, into A
void LDH_imm_a(){
    uint16_t address = 0xFF00 | read_byte(reg->pc++);
    reg->a = read_byte(address);
    update_timers(12);
}

// Save value in HL, being value of SP + n, where n is an 8 bit signed immediate value
void LDHL_sp_n(){
    int8_t n = read_byte(reg->pc++);
    reg->f = 0x0;
    if ((reg->sp & 0xF) + ((uint8_t)n & 0xF) > 0xF)
        reg->f |= 0x20; // half-carry for bits 3,4
    if (((reg->sp & 0xFF) + (uint8_t)n) > (0xFF))
        reg->f |= 0x10; // carry for bits 6,7
    reg->hl = reg->sp + n;
    update_timers(12);
}

// Save SP into [nn], being 16 bit immediate value
void SV_nn_sp(){
    uint8_t low = read_byte(reg->pc++);
    uint8_t high = read_byte(reg->pc++);
    uint16_t address = low | (high << 8);
    save_byte(address, (uint8_t)(reg->sp & 0xFF));
    save_byte(address+1, (uint8_t)(reg->sp >> 8));
    update_timers(20);
}

// Put value at address HL into A. Decrement HL
void LDD_a_hl() {
    reg->a = read_byte(reg->hl);
    --reg->hl;
    update_timers(8);
}

// Put A into memory address HL. Decrement HL.
void SVD_a_hl() {
    save_byte(reg->hl, reg->a);
    --reg->hl;
    update_timers(8);
}

// Put value at address HL into A. Increment HL
void LDI_a_hl() {
    reg->a = read_byte(reg->hl);
    ++reg->hl;
    update_timers(8);
}

// Put A into memory address HL. Increment HL.
void SVI_a_hl() {
    save_byte(reg->hl, reg->a);
    ++reg->hl;
    update_timers(8);
}

// Add to register a value from [hl]
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
    update_timers(8);
}

// Add to register an immediate value
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
    update_timers(8);
}

// Add to register value from [hl] + carry bit
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
    update_timers(8);
}

// Add to register an immediate value + carry bit
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
    update_timers(8);
}

// substract [hl] from a
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
    update_timers(8);
}

// substract n from a
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
    update_timers(8);
}

// substract [hl] - carry bit from register a
void SBC_A_hl(){ 
    uint8_t val = read_byte(reg->hl); 
    uint8_t carry = (reg->f & 0x10) ? 1 : 0; 
    int16_t result = (int16_t)reg->a - (int16_t)val - carry; 
    reg->f = 0x40; 
    if ((result & 0xFF) == 0) 
        reg->f |= 0x80; 
    if (((reg->a & 0x0F) - (val & 0x0F)) < 0) 
        reg->f |= 0x20; 
    if (result < 0) 
        reg->f |= 0x10; 
    reg->a = (uint8_t)result; 
    update_timers(8);
}

//substract immediate value - carry bit from register a
void SBC_A_imm(){ 
    uint8_t val = read_byte(reg->pc++); 
    uint8_t carry = (reg->f & 0x10) ? 1 : 0; 
    int16_t result = (int16_t)reg->a - (int16_t)val - carry; 
    reg->f = 0x40; 
    if ((result & 0xFF) == 0) 
        reg->f |= 0x80; 
    if (((reg->a & 0x0F) - (val & 0x0F)) < 0) 
        reg->f |= 0x20; 
    if (result < 0) 
        reg->f |= 0x10; 
    reg->a = (uint8_t)result; 
    update_timers(8);
}

// AND between register a and value in [hl]
void AND_A_hl() { 
    uint8_t val = read_byte(reg->hl);
    reg->f = 0x20; 
    if ((reg->a & val) == 0)  
        reg->f |= 0x80; 
    reg->a &= val; 
    update_timers(8);
}

// AND between register a and immediate value
void AND_A_n() { 
    uint8_t val = read_byte(reg->pc++);
    reg->f = 0x20; 
    if ((reg->a & val) == 0)  
        reg->f |= 0x80; 
    reg->a &= val; 
    update_timers(8);
}

// OR between register a and value in [hl]
void OR_A_hl() { 
    uint8_t val = read_byte(reg->hl);
    reg->f = 0x0; 
    if ((reg->a | val) == 0) 
        reg->f |= 0x80; 
    reg->a |= val;
    update_timers(8);
}

// OR between register a and immediate value
void OR_A_n() { 
    uint8_t val = read_byte(reg->pc++);
    reg->f = 0x0; 
    if ((reg->a | val) == 0) 
        reg->f |= 0x80; 
    reg->a |= val;
    update_timers(8);
}

// XOR between register a and value in [hl]
void XOR_A_hl() {
    uint8_t val = read_byte(reg->hl);
    reg->f = 0x0; 
    if ((reg->a ^ val) == 0) 
        reg->f |= 0x80; 
    reg->a ^= val;
    update_timers(8);
}

// XOR between register a and immediate value
void XOR_A_n() { 
    uint8_t val = read_byte(reg->pc++);
    reg->f = 0x0; 
    if ((reg->a ^ val) == 0) 
        reg->f |= 0x80; 
    reg->a ^= val;
    update_timers(8);
}

// CMP between A and [hl]
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
    update_timers(8);
}

// CMP between A and immediate value
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
    update_timers(8);
}

// INC value in [hl]
void INC_REGhl() { 
    uint8_t val = read_byte(reg->hl);
    reg->f &= 0x10; 
    if((val & 0xF) + 0x01 > 0xF) 
        reg->f |= 0x20; 
    ++val;
    save_byte(reg->hl, val);  
    if (val == 0) 
        reg->f |= 0x80; 
    update_timers(12);
}

// DEC value in [hl]
void DEC_REGhl() { 
    uint8_t val = read_byte(reg->hl);
    reg->f &= 0x10; 
    reg->f |= 0x40;
    if((val & 0xF) == 0x0) 
        reg->f |= 0x20;
    --val;
    if(val == 0)
        reg->f |= 0x80;
    save_byte(reg->hl, val);
    update_timers(12);
}

// add immediate value to stack pointer
void ADD_SP_n() {
    reg->f = 0x0;
    int8_t val = (int8_t) read_byte(reg->pc++); 
    uint16_t imm = (uint16_t)(uint8_t) val;
    if((reg->sp & 0xF) + (imm & 0xF) > 0xF)
        reg->f |= 0x20;
    if((reg->sp & 0xFF) + (imm & 0xFF) > 0xFF)
        reg->f |= 0x10;
    reg->sp += val;
    update_timers(16);
}

// swap bit 0-3 and 4-7 in [hl]
void SWAP_hl() { 
    uint8_t val = read_byte(reg->hl);
    reg->f = 0x0; 
    if (val == 0x0) 
        reg-> f = 0x80; 
    val = (val << 4) | (val >> 4); 
    save_byte(reg->hl, val);
    update_timers(16); \
}

// Decimal adjust register A from binary to BCD. 
void DAA() {
    uint8_t correction = 0;
    bool carry = false;
    if (!(reg->f & 0x40)) {
        // if there is a half carry or bit 0-3 are greater than 9
        if ((reg->f & 0x20) || (reg->a & 0x0F) > 0x09) {
            correction |= 0x06;
        }
        // if there is a full carry or register A is greater than 99
        if ((reg->f & 0x10) || reg->a > 0x99) {
            correction |= 0x60;
            carry = true;
        }
    } else { 
        // if there is a half carry
        if (reg->f & 0x20) {
            correction |= 0x06;
        }
        // if there is a full carry
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
    update_timers(4);
}

// flip a register
void CPL() {
    reg->a ^= 0xFF;
    reg->f |= 0x60;
    update_timers(4);
}

// Complement carry flag
void CCF() {
    reg->f ^= 0x10;   
    reg->f &= ~0x60;  
    update_timers(4);
}

// Set Carry flag
void SCF() {
    reg->f &= ~0x60;  
    reg->f |= 0x10; 
    update_timers(4);
}

// Halt CPU
void HALT() {
    update_timers(4);
    if (ime) {
        while (!(read_byte(_IF) & memory->ie))
            update_timers(4);
        handle_interrupts();
    } else if (!(read_byte(_IF) & memory->ie)){
        while (!(read_byte(_IF) & memory->ie))
            update_timers(4);
    } else {
        update_timers(4);
    }
}

// Halt CPU and Display
void STOP() {
    ++reg->pc;
    update_timers(4);
}

// disable interrupts after the instruction (cycles)
/*
This instruction disables interrupts but not
immediately. Interrupts are disabled after
instruction after DI is executed.
*/
void DI() {
    ei = false;
    ime_next = 1;
    update_timers(4);
}

// enable interrupts after the instruction (cycles)
// same shis
void EI() {
    ei = true;
    ime_next = 1;
    update_timers(4);
}

// Rotate [hl] left. Old bit 7 to Carry flag.
void RLC_hl() {
    reg->f = 0x0;
    uint8_t val = read_byte(reg->hl);
    uint8_t carry = (val & 0x80) >> 7;
    val = (val << 1) | carry;
    save_byte(reg->hl, val);
    reg->f |= (carry << 4);
    if(val == 0)
        reg->f |= 0x80;
    update_timers(16);
}

// Rotate [hl] left through Carry flag
void RL_hl() {
    uint8_t val = read_byte(reg->hl);
    uint8_t new = (val & 0x80) >> 7;
    uint8_t old = (reg->f & 0x10) >> 4;
    val = (val << 1) | old;
    save_byte(reg->hl, val);
    reg->f = (new << 4);
    if(val == 0)
        reg->f |= 0x80;
    update_timers(16);
}

// Rotate [hl] right. Old bit 0 to Carry flag.
void RRC_hl() {
    reg->f = 0x0;
    uint8_t val = read_byte(reg->hl);
    uint8_t carry = (val & 0x01);
    val = (carry << 7) | (val >> 1);
    save_byte(reg->hl, val);
    reg->f |= (carry << 4);
    if(val == 0)
        reg->f |= 0x80;
    update_timers(16);
}

// Rotate [hl] right through Carry flag
void RR_hl() {
    uint8_t val = read_byte(reg->hl);
    uint8_t new = (val & 0x01);
    uint8_t old = (reg->f & 0x10) << 3;
    val = old | (val >> 1);
    save_byte(reg->hl, val);
    reg->f = (new << 4);
    if(val == 0)
        reg->f |= 0x80;
    update_timers(16);
}

// shift left [hl]
void SL_hl() {
    uint8_t val = read_byte(reg->hl);
    int8_t carry = (val & 0x80) >> 7; 
    reg->f = carry << 4; 
    val <<= 1; 
    save_byte(reg->hl, val);
    if (val == 0) 
        reg->f |= 0x80; 
    update_timers(16);
}

// shift right [hl] preserving msb
void SRA_hl() {
    uint8_t val = read_byte(reg->hl);
    uint8_t msb = val & 0x80; 
    uint8_t carry = val & 0x01; 
    val = msb | (val >> 1); 
    save_byte(reg->hl, val);
    reg->f = (carry << 4); 
    if (val == 0x0) 
        reg->f |= 0x80; 
    update_timers(16);
}

// shift right [hl] without preserving msb
void SRL_hl() {
    uint8_t val = read_byte(reg->hl);
    uint8_t carry = val & 0x01; 
    val >>= 1; 
    save_byte(reg->hl, val);
    reg->f = (carry << 4); 
    if (val == 0x0) 
        reg->f |= 0x80; 
    update_timers(16);
}

// tests bit n in [hl]
void BIT_hl() { 
    uint8_t val = read_byte(reg->hl);
    uint8_t bit = (opcode >> 3) & 0x07; 
    uint8_t mask = 1 << bit;
    reg->f &= 0x10; 
    reg->f |= 0x20; 
    if (!(val & mask))
        reg->f |= 0x80; 
    update_timers(16);
}

// set bit n in [hl]
void SET_hl() { \
    uint8_t val = read_byte(reg->hl);
    uint8_t bit = (opcode >> 3) & 0x07; 
    uint8_t mask = 1 << bit;
    val |= mask; 
    save_byte(reg->hl, val);
    update_timers(16);
}

// reset bit n in [hl]
void RESET_hl() { \
    uint8_t val = read_byte(reg->hl);
    uint8_t bit = (opcode >> 3) & 0x07; 
    uint8_t mask = ~(1 << bit);
    val &= mask;
    save_byte(reg->hl, val);
    update_timers(16);
}

// jp to address
void JP() {
    uint8_t low = read_byte(reg->pc++);
    uint8_t high = read_byte(reg->pc++);
    uint16_t address = (high << 8) | low;
    reg->pc = address;
    update_timers(16);
}

 // jp to address in hl
void JP_hl() {
    reg->pc = reg->hl;
    update_timers(4);
}

// jp if some flags are set
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
        uint8_t high = read_byte(reg->pc);
        uint16_t address = (high << 8) | low ;
        reg->pc = address;
        update_timers(16);
    } else {
        update_timers(12);
    }
}

// add n to current address
void JR() {
    int8_t n = (int8_t) read_byte(reg->pc++);
    reg->pc = (uint16_t)(reg->pc + n);
    update_timers(12);
}

// add n to current address if some flags are set
void JR_COND() {
    bool condition_met = false;
    int8_t val = read_byte(reg->pc++);
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
        reg->pc = (uint16_t) reg->pc + val;
        update_timers(12);
    } else {
        update_timers(8);
    }
} 

void CALL() {
    uint8_t low = read_byte(reg->pc++);
    uint8_t high = read_byte(reg->pc++);
    uint16_t address = (high<<8) | low;
    save_byte(--reg->sp, (reg->pc >> 8) & 0xFF);
    save_byte(--reg->sp, reg->pc & 0xFF);
    reg->pc = address;
    update_timers(24);
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
    }
    if(condition_met){
        save_byte(--reg->sp, (reg->pc >> 8) & 0xFF);
        save_byte(--reg->sp, reg->pc & 0xFF);
        reg->pc = address;
        update_timers(24);
    } else {
        update_timers(12);
    }
}

// Push present address onto stack. Jump to address 0x0000 + n
void RST() {
    uint8_t offset = 0x0;
    switch (opcode){
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
    save_byte(--reg->sp, (reg->pc >> 8) & 0xFF);
    save_byte(--reg->sp, reg->pc & 0xFF);
    reg->pc = 0x0000 + offset;
    update_timers(16);
}

// Pop two bytes from stack & jump to that address
void RET() {
    uint8_t low = read_byte(reg->sp++);
    uint8_t high = read_byte(reg->sp++);
    uint16_t address = (high<<8) | low;
    reg->pc = address;
    update_timers(16);
}

// Pop two bytes from stack & jump to that address if cond is met
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
        uint8_t low = read_byte(reg->sp++);
        uint8_t high = read_byte(reg->sp++);
        reg->pc = (high << 8) | low;  
        update_timers(20); 
    } else {
        update_timers(8);
    }
}

// Pop two bytes from stack & jump to that address then enable interrupts
void RETI() {
    uint8_t low = read_byte(reg->sp++);
    uint8_t high = read_byte(reg->sp++);
    uint16_t address = (high<<8) | low;
    reg->pc = address;
    update_timers(16);
    ime = true;
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

POP_REG16(af);
POP_REG16(bc);
POP_REG16(de);
POP_REG16(hl);

GEN_REG_n(bc);
GEN_REG_n(de);

GEN_REG_NN(a);

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
    // 0x0_
    [0x00] = NOP, // NOP
    [0x01] = LD_bc_nn, // LD BC, nn
    [0x02] = SV_bc_a, // LD (BC), A
    [0x03] = INC_bc, // INC BC
    [0x04] = INC_b, // INC B
    [0x05] = DEC_b, // DEC B
    [0x06] = LD_b_n, // LD B, n
    [0x07] = RLC_a, // RLC A
    [0x08] = SV_nn_sp, // LD (nn), SP
    [0x09] = ADD_HL_bc, // ADD HL, BC
    [0x0A] = LD_a_bc, // LD A, (BC)
    [0x0B] = DEC_bc, // DEC BC
    [0x0C] = INC_c, // INC C
    [0x0D] = DEC_c, // DEC C
    [0x0E] = LD_c_n, // LD C, n
    [0x0F] = RRC_a, // RRC A

    // 0x1_
    [0x10] = STOP, // STOP
    [0x11] = LD_de_nn, // LD DE, nn
    [0x12] = SV_de_a, // LD (DE), A
    [0x13] = INC_de, // INC DE
    [0x14] = INC_d, // INC D
    [0x15] = DEC_d, // DEC D
    [0x16] = LD_d_n, // LD D, n
    [0x17] = RL_a, // RL A
    [0x18] = JR, // JR n
    [0x19] = ADD_HL_de, // ADD HL, DE
    [0x1A] = LD_a_de, // LD A, (DE)
    [0x1B] = DEC_de, // DEC DE
    [0x1C] = INC_e, // INC E
    [0x1D] = DEC_e, // DEC E
    [0x1E] = LD_e_n, // LD E, n
    [0x1F] = RR_a, // RR A

    // 0x2_
    [0x20] = JR_COND, // JR NZ, n
    [0x21] = LD_hl_nn, // LD HL, nn
    [0x22] = SVI_a_hl, // LDI (HL), A
    [0x23] = INC_hl, // INC HL
    [0x24] = INC_h, // INC H
    [0x25] = DEC_h, // DEC H
    [0x26] = LD_h_n, // LD H, n
    [0x27] = DAA, // DAA
    [0x28] = JR_COND, // JR Z, n
    [0x29] = ADD_HL_hl, // ADD HL, HL
    [0x2A] = LDI_a_hl, // LDI A, (HL)
    [0x2B] = DEC_hl, // DEC HL
    [0x2C] = INC_l, // INC L
    [0x2D] = DEC_l, // DEC L
    [0x2E] = LD_l_n, // LD L, n
    [0x2F] = CPL, // CPL

    // 0x3_
    [0x30] = JR_COND, // JR NC, n
    [0x31] = LD_sp_nn, // LD SP, nn
    [0x32] = SVD_a_hl, // LDD (HL), A
    [0x33] = INC_sp, // INC SP
    [0x34] = INC_REGhl, // INC (HL)
    [0x35] = DEC_REGhl, // DEC (HL)
    [0x36] = SV_hl_n, // LD (HL), n
    [0x37] = SCF, // SCF
    [0x38] = JR_COND, // JR C, n
    [0x39] = ADD_HL_sp, // ADD HL, SP
    [0x3A] = LDD_a_hl, // LDD A, (HL)
    [0x3B] = DEC_sp, // DEC SP
    [0x3C] = INC_a, // INC A
    [0x3D] = DEC_a, // DEC A
    [0x3E] = LD_a_n, // LD A, n
    [0x3F] = CCF, // CCF

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
    [0x76] = HALT, // HALT
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
    [0x80] = ADD_A_b, // ADD A, B
    [0x81] = ADD_A_c, // ADD A, C
    [0x82] = ADD_A_d, // ADD A, D
    [0x83] = ADD_A_e, // ADD A, E
    [0x84] = ADD_A_h, // ADD A, H
    [0x85] = ADD_A_l, // ADD A, L
    [0x86] = ADD_A_hl, // ADD A, (HL)
    [0x87] = ADD_A_a, // ADD A, A
    [0x88] = ADC_A_b, // ADC A, B
    [0x89] = ADC_A_c, // ADC A, C
    [0x8A] = ADC_A_d, // ADC A, D
    [0x8B] = ADC_A_e, // ADC A, E
    [0x8C] = ADC_A_h, // ADC A, H
    [0x8D] = ADC_A_l, // ADC A, L
    [0x8E] = ADC_A_hl, // ADC A, (HL)
    [0x8F] = ADC_A_a, // ADC A, A

    // 0x9_ (Arithmetic: SUB / SBC)
    [0x90] = SUB_A_b, // SUB A, B
    [0x91] = SUB_A_c, // SUB A, C
    [0x92] = SUB_A_d, // SUB A, D
    [0x93] = SUB_A_e, // SUB A, E
    [0x94] = SUB_A_h, // SUB A, H
    [0x95] = SUB_A_l, // SUB A, L
    [0x96] = SUB_A_hl, // SUB A, (HL)
    [0x97] = SUB_A_a, // SUB A, A
    [0x98] = SBC_A_b, // SBC
    [0x99] = SBC_A_c,
    [0x9A] = SBC_A_d,
    [0X9B] = SBC_A_e,
    [0x9C] = SBC_A_h,
    [0x9D] = SBC_A_l,
    [0x9E] = SBC_A_hl,
    [0x9F] = SBC_A_a,

    // 0xA_ (Logic: AND / XOR)
    [0xA0] = AND_A_b, // AND B
    [0xA1] = AND_A_c, // AND C
    [0xA2] = AND_A_d, // AND D
    [0xA3] = AND_A_e, // AND E
    [0xA4] = AND_A_h, // AND H
    [0xA5] = AND_A_l, // AND L
    [0xA6] = AND_A_hl, // AND (HL)
    [0xA7] = AND_A_a, // AND A
    [0xA8] = XOR_A_b, // XOR B
    [0xA9] = XOR_A_c, // XOR C
    [0xAA] = XOR_A_d, // XOR D
    [0xAB] = XOR_A_e, // XOR E
    [0xAC] = XOR_A_h, // XOR H
    [0xAD] = XOR_A_l, // XOR L
    [0xAE] = XOR_A_hl, // XOR (HL)
    [0xAF] = XOR_A_a, // XOR A

    // 0xB_ (Logic: OR / CP)
    [0xB0] = OR_A_b, // OR B
    [0xB1] = OR_A_c, // OR C
    [0xB2] = OR_A_d, // OR D
    [0xB3] = OR_A_e, // OR E
    [0xB4] = OR_A_h, // OR H
    [0xB5] = OR_A_l, // OR L
    [0xB6] = OR_A_hl, // OR (HL)
    [0xB7] = OR_A_a, // OR A
    [0xB8] = CP_A_b, // CP B
    [0xB9] = CP_A_c, // CP C
    [0xBA] = CP_A_d, // CP D
    [0xBB] = CP_A_e, // CP E
    [0xBC] = CP_A_h, // CP H
    [0xBD] = CP_A_l, // CP L
    [0xBE] = CP_A_hl, // CP (HL)
    [0xBF] = CP_A_a, // CP A

    // 0xC_ (Control Flow / Stack)
    [0xC0] = RET_COND, // RET NZ
    [0xC1] = POP_bc, // POP BC
    [0xC2] = JP_COND, // JP NZ, nn
    [0xC3] = JP, // JP nn
    [0xC4] = CALL_COND, // CALL NZ, nn
    [0xC5] = PUSH_bc, // PUSH BC
    [0xC6] = ADD_A_n, // ADD A, n
    [0xC7] = RST, // RST 0
    [0xC8] = RET_COND, // RET Z
    [0xC9] = RET, // RET
    [0xCA] = JP_COND, // JP Z, nn
    [0xCB] = prefix_function, // Ext ops (Prefix CB)
    [0xCC] = CALL_COND, // CALL Z, nn
    [0xCD] = CALL, // CALL nn
    [0xCE] = ADC_A_n, // ADC A, n
    [0xCF] = RST, // RST 8

    // 0xD_
    [0xD0] = RET_COND, // RET NC
    [0xD1] = POP_de, // POP DE
    [0xD2] = JP_COND, // JP NC, nn
    [0xD3] = NULL, // XX (Invalid)
    [0xD4] = CALL_COND, // CALL NC, nn
    [0xD5] = PUSH_de, // PUSH DE
    [0xD6] = SUB_A_n, // SUB A, n
    [0xD7] = RST, // RST 10
    [0xD8] = RET_COND, // RET C
    [0xD9] = RETI, // RETI
    [0xDA] = JP_COND, // JP C, nn
    [0xDB] = NULL, // XX (Invalid)
    [0xDC] = CALL_COND, // CALL C, nn
    [0xDD] = NULL, // XX (Invalid)
    [0xDE] = SBC_A_imm, // SBC A, n
    [0xDF] = RST, // RST 18

    // 0xE_
    [0xE0] = SVH_imm_a, // LDH (n), A
    [0xE1] = POP_hl, // POP HL
    [0xE2] = SLD_c_a, // LDH (C), A
    [0xE3] = NULL, // XX (Invalid)
    [0xE4] = NULL, // XX (Invalid)
    [0xE5] = PUSH_hl, // PUSH HL
    [0xE6] = AND_A_n, // AND n
    [0xE7] = RST, // RST 20
    [0xE8] = ADD_SP_n, // ADD SP, d
    [0xE9] = JP_hl, // JP (HL)
    [0xEA] = SV_nn_a, // LD (nn), A
    [0xEB] = NULL, // XX (Invalid)
    [0xEC] = NULL, // XX (Invalid)
    [0xED] = NULL, // XX (Invalid)
    [0xEE] = XOR_A_n, // XOR n
    [0xEF] = RST, // RST 28

    // 0xF_
    [0xF0] = LDH_imm_a, // LDH A, (n)
    [0xF1] = POP_af , // POP AF
    [0xF2] = SLD_a_c, // LDH A, [C] 
    [0xF3] = DI, // DI (Disable Interrupts)
    [0xF4] = NULL, // XX (Invalid)
    [0xF5] = PUSH_af, // PUSH AF
    [0xF6] = OR_A_n, // OR n
    [0xF7] = RST, // RST 30
    [0xF8] = LDHL_sp_n, // LDHL SP, d
    [0xF9] = LD_sp_hl, // LD SP, HL
    [0xFA] = LD_a_nn, // LD A, (nn)
    [0xFB] = EI, // EI (Enable Interrupts)
    [0xFC] = NULL, // XX (Invalid)
    [0xFD] = NULL, // XX (Invalid)
    [0xFE] = CP_A_n, // CP n
    [0xFF] = RST, // RST 38
};

instruction_ptr prefix_opcode_table[256] = {
    // 0x0_ (Rotates & Shifts)
    [0x00] = RLC_b, // RLC B
    [0x01] = RLC_c, // RLC C
    [0x02] = RLC_d, // RLC D
    [0x03] = RLC_e, // RLC E
    [0x04] = RLC_h, // RLC H
    [0x05] = RLC_l, // RLC L
    [0x06] = RLC_hl, // RLC (HL)
    [0x07] = RLC_a, // RLC A
    [0x08] = RRC_b, // RRC B
    [0x09] = RRC_c, // RRC C
    [0x0A] = RRC_d, // RRC D
    [0x0B] = RRC_e, // RRC E
    [0x0C] = RRC_h, // RRC H
    [0x0D] = RRC_l, // RRC L
    [0x0E] = RRC_hl, // RRC (HL)
    [0x0F] = RRC_a, // RRC A

    // 0x1_ (Rotates & Shifts through Carry)
    [0x10] = RL_b, // RL B
    [0x11] = RL_c, // RL C
    [0x12] = RL_d, // RL D
    [0x13] = RL_e, // RL E
    [0x14] = RL_h, // RL H
    [0x15] = RL_l, // RL L
    [0x16] = RL_hl, // RL (HL)
    [0x17] = RL_a, // RL A
    [0x18] = RR_b, // RR B
    [0x19] = RR_c, // RR C
    [0x1A] = RR_d, // RR D
    [0x1B] = RR_e, // RR E
    [0x1C] = RR_h, // RR H
    [0x1D] = RR_l, // RR L
    [0x1E] = RR_hl, // RR (HL)
    [0x1F] = RR_a, // RR A

    // 0x2_ (Arithmetic Shifts)
    [0x20] = SL_b, // SLA B
    [0x21] = SL_c, // SLA C
    [0x22] = SL_d, // SLA Dj
    [0x23] = SL_e, // SLA E
    [0x24] = SL_h, // SLA H
    [0x25] = SL_l, // SLA L
    [0x26] = SL_hl, // SLA (HL)
    [0x27] = SL_a, // SLA A
    [0x28] = SRA_b, // SRA B
    [0x29] = SRA_c, // SRA C
    [0x2A] = SRA_d, // SRA D
    [0x2B] = SRA_e, // SRA E
    [0x2C] = SRA_h, // SRA H
    [0x2D] = SRA_l, // SRA L
    [0x2E] = SRA_hl, // SRA (HL)
    [0x2F] = SRA_a, // SRA A

    // 0x3_ (Swaps & Logical Shifts)
    [0x30] = SWAP_b, // SWAP B
    [0x31] = SWAP_c, // SWAP C
    [0x32] = SWAP_d, // SWAP D
    [0x33] = SWAP_e, // SWAP E
    [0x34] = SWAP_h, // SWAP H
    [0x35] = SWAP_l, // SWAP L
    [0x36] = SWAP_hl, // SWAP (HL)
    [0x37] = SWAP_a, // SWAP A
    [0x38] = SRL_b, // SRL B
    [0x39] = SRL_c, // SRL C
    [0x3A] = SRL_d, // SRL D
    [0x3B] = SRL_e, // SRL E
    [0x3C] = SRL_h, // SRL H
    [0x3D] = SRL_l, // SRL L
    [0x3E] = SRL_hl, // SRL (HL)
    [0x3F] = SRL_a, // SRL A

    // 0x4_ (BIT 0 and BIT 1)
    [0x40] = BIT_b, // BIT 0, B
    [0x41] = BIT_c, // BIT 0, C
    [0x42] = BIT_d, // BIT 0, D
    [0x43] = BIT_e, // BIT 0, E
    [0x44] = BIT_h, // BIT 0, H
    [0x45] = BIT_l, // BIT 0, L
    [0x46] = BIT_hl, // BIT 0, (HL)
    [0x47] = BIT_a, // BIT 0, A
    [0x48] = BIT_b, // BIT 1, B
    [0x49] = BIT_c, // BIT 1, C
    [0x4A] = BIT_d, // BIT 1, D
    [0x4B] = BIT_e, // BIT 1, E
    [0x4C] = BIT_h, // BIT 1, H
    [0x4D] = BIT_l, // BIT 1, L
    [0x4E] = BIT_hl, // BIT 1, (HL)
    [0x4F] = BIT_a, // BIT 1, A

    // 0x5_ (BIT 2 and BIT 3)
    [0x50] = BIT_b, // BIT 2, B
    [0x51] = BIT_c, // BIT 2, C
    [0x52] = BIT_d, // BIT 2, D
    [0x53] = BIT_e, // BIT 2, E
    [0x54] = BIT_h, // BIT 2, H
    [0x55] = BIT_l, // BIT 2, L
    [0x56] = BIT_hl, // BIT 2, (HL)
    [0x57] = BIT_a, // BIT 2, A
    [0x58] = BIT_b, // BIT 3, B
    [0x59] = BIT_c, // BIT 3, C
    [0x5A] = BIT_d, // BIT 3, D
    [0x5B] = BIT_e, // BIT 3, E
    [0x5C] = BIT_h, // BIT 3, H
    [0x5D] = BIT_l, // BIT 3, L
    [0x5E] = BIT_hl, // BIT 3, (HL)
    [0x5F] = BIT_a, // BIT 3, A

    // 0x6_ (BIT 4 and BIT 5)
    [0x60] = BIT_b, // BIT 4, B
    [0x61] = BIT_c, // BIT 4, C
    [0x62] = BIT_d, // BIT 4, D
    [0x63] = BIT_e, // BIT 4, E
    [0x64] = BIT_h, // BIT 4, H
    [0x65] = BIT_l, // BIT 4, L
    [0x66] = BIT_hl, // BIT 4, (HL)
    [0x67] = BIT_a, // BIT 4, A
    [0x68] = BIT_b, // BIT 5, B
    [0x69] = BIT_c, // BIT 5, C
    [0x6A] = BIT_d, // BIT 5, D
    [0x6B] = BIT_e, // BIT 5, E
    [0x6C] = BIT_h, // BIT 5, H
    [0x6D] = BIT_l, // BIT 5, L
    [0x6E] = BIT_hl, // BIT 5, (HL)
    [0x6F] = BIT_a, // BIT 5, A

    // 0x7_ (BIT 6 and BIT 7)
    [0x70] = BIT_b, // BIT 6, B
    [0x71] = BIT_c, // BIT 6, C
    [0x72] = BIT_d, // BIT 6, D
    [0x73] = BIT_e, // BIT 6, E
    [0x74] = BIT_h, // BIT 6, H
    [0x75] = BIT_l, // BIT 6, L
    [0x76] = BIT_hl, // BIT 6, (HL)
    [0x77] = BIT_a, // BIT 6, A
    [0x78] = BIT_b, // BIT 7, B
    [0x79] = BIT_c, // BIT 7, C
    [0x7A] = BIT_d, // BIT 7, D
    [0x7B] = BIT_e, // BIT 7, E
    [0x7C] = BIT_h, // BIT 7, H
    [0x7D] = BIT_l, // BIT 7, L
    [0x7E] = BIT_hl, // BIT 7, (HL)
    [0x7F] = BIT_a, // BIT 7, A

    // 0x8_ (RES 0 and RES 1)
    [0x80] = RESET_b, // RES 0, B
    [0x81] = RESET_c, // RES 0, C
    [0x82] = RESET_d, // RES 0, D
    [0x83] = RESET_e, // RES 0, E
    [0x84] = RESET_h, // RES 0, H
    [0x85] = RESET_l, // RES 0, L
    [0x86] = RESET_hl, // RES 0, (HL)
    [0x87] = RESET_a, // RES 0, A
    [0x88] = RESET_b, // RES 1, B
    [0x89] = RESET_c, // RES 1, C
    [0x8A] = RESET_d, // RES 1, D
    [0x8B] = RESET_e, // RES 1, E
    [0x8C] = RESET_h, // RES 1, H
    [0x8D] = RESET_l, // RES 1, L
    [0x8E] = RESET_hl, // RES 1, (HL)
    [0x8F] = RESET_a, // RES 1, A

    // 0x9_ (RES 2 and RES 3)
    [0x90] = RESET_b, // RES 2, B
    [0x91] = RESET_c, // RES 2, C
    [0x92] = RESET_d, // RES 2, D
    [0x93] = RESET_e, // RES 2, E
    [0x94] = RESET_h, // RES 2, H
    [0x95] = RESET_l, // RES 2, L
    [0x96] = RESET_hl, // RES 2, (HL)
    [0x97] = RESET_a, // RES 2, A
    [0x98] = RESET_b, // RES 3, B
    [0x99] = RESET_c, // RES 3, C
    [0x9A] = RESET_d, // RES 3, D
    [0x9B] = RESET_e, // RES 3, E
    [0x9C] = RESET_h, // RES 3, H
    [0x9D] = RESET_l, // RES 3, L
    [0x9E] = RESET_hl, // RES 3, (HL)
    [0x9F] = RESET_a, // RES 3, A

    // 0xA_ (RES 4 and RES 5)
    [0xA0] = RESET_b, // RES 4, B
    [0xA1] = RESET_c, // RES 4, C
    [0xA2] = RESET_d, // RES 4, D
    [0xA3] = RESET_e, // RES 4, E
    [0xA4] = RESET_h, // RES 4, H
    [0xA5] = RESET_l, // RES 4, L
    [0xA6] = RESET_hl, // RES 4, (HL)
    [0xA7] = RESET_a, // RES 4, A
    [0xA8] = RESET_b, // RES 5, B
    [0xA9] = RESET_c, // RES 5, C
    [0xAA] = RESET_d, // RES 5, D
    [0xAB] = RESET_e, // RES 5, E
    [0xAC] = RESET_h, // RES 5, H
    [0xAD] = RESET_l, // RES 5, L
    [0xAE] = RESET_hl, // RES 5, (HL)
    [0xAF] = RESET_a, // RES 5, A

    // 0xB_ (RES 6 and RES 7)
    [0xB0] = RESET_b, // RES 6, B
    [0xB1] = RESET_c, // RES 6, C
    [0xB2] = RESET_d, // RES 6, D
    [0xB3] = RESET_e, // RES 6, E
    [0xB4] = RESET_h, // RES 6, H
    [0xB5] = RESET_l, // RES 6, L
    [0xB6] = RESET_hl, // RES 6, (HL)
    [0xB7] = RESET_a, // RES 6, A
    [0xB8] = RESET_b, // RES 7, B
    [0xB9] = RESET_c, // RES 7, C
    [0xBA] = RESET_d, // RES 7, D
    [0xBB] = RESET_e, // RES 7, E
    [0xBC] = RESET_h, // RES 7, H
    [0xBD] = RESET_l, // RES 7, L
    [0xBE] = RESET_hl, // RES 7, (HL)
    [0xBF] = RESET_a, // RES 7, A

    // 0xC_ (SET 0 and SET 1)
    [0xC0] = SET_b, // SET 0, B
    [0xC1] = SET_c, // SET 0, C
    [0xC2] = SET_d, // SET 0, D
    [0xC3] = SET_e, // SET 0, E
    [0xC4] = SET_h, // SET 0, H
    [0xC5] = SET_l, // SET 0, L
    [0xC6] = SET_hl, // SET 0, (HL)
    [0xC7] = SET_a, // SET 0, A
    [0xC8] = SET_b, // SET 1, B
    [0xC9] = SET_c, // SET 1, C
    [0xCA] = SET_d, // SET 1, D
    [0xCB] = SET_e, // SET 1, E
    [0xCC] = SET_h, // SET 1, H
    [0xCD] = SET_l, // SET 1, L
    [0xCE] = SET_hl, // SET 1, (HL)
    [0xCF] = SET_a, // SET 1, A

   // 0xD_ (SET 2 and SET 3)
    [0xD0] = SET_b,  // SET 2, B
    [0xD1] = SET_c,  // SET 2, C
    [0xD2] = SET_d,  // SET 2, D
    [0xD3] = SET_e,  // SET 2, E
    [0xD4] = SET_h,  // SET 2, H
    [0xD5] = SET_l,  // SET 2, L
    [0xD6] = SET_hl, // SET 2, (HL)
    [0xD7] = SET_a,  // SET 2, A
    [0xD8] = SET_b,  // SET 3, B
    [0xD9] = SET_c,  // SET 3, C
    [0xDA] = SET_d,  // SET 3, D
    [0xDB] = SET_e,  // SET 3, E
    [0xDC] = SET_h,  // SET 3, H
    [0xDD] = SET_l,  // SET 3, L
    [0xDE] = SET_hl, // SET 3, (HL)
    [0xDF] = SET_a,  // SET 3, A

    // 0xE_ (SET 4 and SET 5)
    [0xE0] = SET_b,  // SET 4, B
    [0xE1] = SET_c,  // SET 4, C
    [0xE2] = SET_d,  // SET 4, D
    [0xE3] = SET_e,  // SET 4, E
    [0xE4] = SET_h,  // SET 4, H
    [0xE5] = SET_l,  // SET 4, L
    [0xE6] = SET_hl, // SET 4, (HL)
    [0xE7] = SET_a,  // SET 4, A
    [0xE8] = SET_b,  // SET 5, B
    [0xE9] = SET_c,  // SET 5, C
    [0xEA] = SET_d,  // SET 5, D
    [0xEB] = SET_e,  // SET 5, E
    [0xEC] = SET_h,  // SET 5, H
    [0xED] = SET_l,  // SET 5, L
    [0xEE] = SET_hl, // SET 5, (HL)
    [0xEF] = SET_a,  // SET 5, A

    // 0xF_ (SET 6 and SET 7)
    [0xF0] = SET_b,  // SET 6, B
    [0xF1] = SET_c,  // SET 6, C
    [0xF2] = SET_d,  // SET 6, D
    [0xF3] = SET_e,  // SET 6, E
    [0xF4] = SET_h,  // SET 6, H
    [0xF5] = SET_l,  // SET 6, L
    [0xF6] = SET_hl, // SET 6, (HL)
    [0xF7] = SET_a,  // SET 6, A
    [0xF8] = SET_b,  // SET 7, B
    [0xF9] = SET_c,  // SET 7, C
    [0xFA] = SET_d,  // SET 7, D
    [0xFB] = SET_e,  // SET 7, E
    [0xFC] = SET_h,  // SET 7, H
    [0xFD] = SET_l,  // SET 7, L
    [0xFE] = SET_hl, // SET 7, (HL)
    [0xFF] = SET_a,  // SET 7, A
};

void prefix_function() {
    prefix_flag = true;
    opcode = read_byte(reg->pc++);
    prefix_opcode_table[opcode]();
    prefix_flag = false;
}

#endif