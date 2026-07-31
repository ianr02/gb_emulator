#include "ppu/ppu.h"
#include "core/emulator_core.h"

const uint32_t shades[4] = {
    0xFFFFFF, 0xAAAAAA, 0x555555, 0x000000
};
uint32_t framebuffer[160 * 144];
uint8_t  bg_color[160 * 144];
uint8_t sprites_this_line = 0;
uint8_t scx_sampled = 0;
uint8_t window_line_counter = 0;
bool prev_stat_line = false;

static uint32_t ppu_cycle = 0;

static inline uint8_t vram_read(uint16_t addr) {
    if (addr >= 0x8000 && addr <= 0x9FFF)
        return memory->vram[addr - 0x8000];
    return 0xFF;
}

void render_scanline(uint8_t ly) {
    uint8_t lcdc = memory->io[_LCDC - 0xFF00];

    if (!(lcdc & 0x80)) {
        for (int x = 0; x < 160; x++) {
            framebuffer[ly * 160 + x] = shades[0];
            bg_color[ly * 160 + x] = 0;
        }
        return;
    }

    uint8_t bgp  = memory->io[_BGP  - 0xFF00];
    uint8_t scy  = memory->io[_SCY  - 0xFF00];
    uint8_t scx  = memory->io[_SCX  - 0xFF00];

    bool signed_mode = !(lcdc & 0x10);

    if (lcdc & 0x01) {
        uint16_t map_base = (lcdc & 0x08) ? 0x9C00 : 0x9800;

        for (int x = 0; x < 160; ++x) {
            uint16_t scroll_x = (x + scx) & 0xFF;
            uint16_t scroll_y = (ly + scy) & 0xFF;
            uint16_t map_addr = map_base + (scroll_y / 8) * 32 + (scroll_x / 8);
            uint8_t tile = vram_read(map_addr);

            uint16_t tile_addr;
            if (signed_mode)
                tile_addr = 0x9000 + (int8_t)tile * 16;
            else
                tile_addr = 0x8000 + tile * 16;

            uint8_t pixel_x = scroll_x & 7;
            uint8_t pixel_y = scroll_y & 7;

            uint8_t byte0 = vram_read(tile_addr + pixel_y * 2);
            uint8_t byte1 = vram_read(tile_addr + pixel_y * 2 + 1);

            uint8_t color = ((byte1 >> (7 - pixel_x)) & 1) << 1 | ((byte0 >> (7 - pixel_x)) & 1);
            uint8_t shade = (bgp >> (color * 2)) & 3;
            framebuffer[ly * 160 + x] = shades[shade];
            bg_color[ly * 160 + x] = color;
        }
    } else {
        for (int x = 0; x < 160; x++) {
            framebuffer[ly * 160 + x] = shades[0];
            bg_color[ly * 160 + x] = 0;
        }
    }

    if (lcdc & 0x20) {
        uint8_t wy = memory->io[_WY - 0xFF00];
        int16_t wx = memory->io[_WX - 0xFF00] - 7;

        if (ly >= wy) {
            uint16_t win_map = (lcdc & 0x40) ? 0x9C00 : 0x9800;

            for (int x = 0; x < 160; x++) {
                int win_x = x - wx;
                if (win_x < 0) continue;

                uint16_t map_addr = win_map + (window_line_counter / 8) * 32 + (win_x / 8);
                uint8_t tile = vram_read(map_addr);

                uint16_t tile_addr;
                if (signed_mode)
                    tile_addr = 0x9000 + (int8_t)tile * 16;
                else
                    tile_addr = 0x8000 + tile * 16;

                uint8_t pixel_x = win_x & 7;
                uint8_t pixel_y = window_line_counter & 7;

                uint8_t byte0 = vram_read(tile_addr + pixel_y * 2);
                uint8_t byte1 = vram_read(tile_addr + pixel_y * 2 + 1);

                uint8_t color = ((byte1 >> (7 - pixel_x)) & 1) << 1 |
                                ((byte0 >> (7 - pixel_x)) & 1);
                uint8_t shade = (bgp >> (color * 2)) & 3;
                framebuffer[ly * 160 + x] = shades[shade];
                bg_color[ly * 160 + x] = color;
            }
        }
        if (wx < 160) window_line_counter++;
    }

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
            for (int j = i + 1; j < count; j++) {
                int xi = memory->oam[visible[i] * 4 + 1] - 8;
                int xj = memory->oam[visible[j] * 4 + 1] - 8;
                if (xj < xi || (xj == xi && visible[j] < visible[i])) {
                    uint8_t tmp = visible[i];
                    visible[i] = visible[j];
                    visible[j] = tmp;
                }
            }
        }

        for (int i = count - 1; i >= 0; i--) {
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
                uint8_t byte0 = vram_read(tile_addr);
                uint8_t byte1 = vram_read(tile_addr + 1);

                uint8_t color = ((byte1 >> (7 - pixel_x)) & 1) << 1 |
                                ((byte0 >> (7 - pixel_x)) & 1);
                if (color == 0) continue;

                uint8_t pal = (flags & 0x10) ? obp1 : obp0;
                uint8_t shade = (pal >> (color * 2)) & 3;

                if (!(flags & 0x80) || bg_color[ly * 160 + x] == 0)
                    framebuffer[ly * 160 + x] = shades[shade];
            }
        }
    }
}

bool get_stat_line(uint8_t stat) {
    uint8_t mode = stat & 0x03;
    bool mode0 = (mode == 0) && (stat & 0x08);
    bool mode1 = (mode == 1) && (stat & 0x10);
    bool mode2 = (mode == 2) && (stat & 0x20);
    bool lyc   = (stat & 0x04) && (stat & 0x40);
    return mode0 || mode1 || mode2 || lyc;
}

// OAM corruption bug window (DMG). Calibrated against blargg's
// oam_bug test ROM: a 16-bit register inc/dec (whose M-cycle is tracked
// by the emulator as M-cycle index m) corrupts the OAM row being read by
// the PPU when m is within the first ~20 M-cycles of a visible scanline.
// m in [3,21] maps to rows 1..19 (rows 0/1, objects 0 and 1, are never
// corrupted). When the LCD is enabled the dot counter starts at dot 16 so
// the first scanline is 113 M-cycles long (lcd_sync test).
#define OAM_BUG_M_CYCLE_START 0
#define OAM_BUG_M_CYCLE_END   18

// Returns the OAM row (0-19) being accessed at emulator M-cycle index m,
// or 0xFF when no corruption can occur.
uint8_t ppu_oam_row_at(int m) {
    uint8_t lcdc = memory->io[_LCDC - 0xFF00];
    if (!(lcdc & 0x80)) return 0xFF;
    uint8_t ly = memory->io[_LY - 0xFF00];
    if (ly >= 144) return 0xFF;
    m = ((m % 114) + 114) % 114;
    if (m >= OAM_BUG_M_CYCLE_START && m <= OAM_BUG_M_CYCLE_END)
        return (uint8_t)(m + 1);
    return 0xFF;
}

uint8_t ppu_oam_row(void) {
    return ppu_oam_row_at(ppu_m_cycle());
}

int ppu_m_cycle(void) {
    return (int)(ppu_cycle / 4);
}

// LCD is turned on: the PPU resets its dot counter so that the first
// scanline is 113 M-cycles (452 dots) long, i.e. LY increments 113
// M-cycles after the LCDC write (lcd_sync test).
void ppu_lcd_on_phase(void) {
    ppu_cycle = 4;
}

static void sample_line_state(uint8_t ly) {
    if (ly >= 144) { sprites_this_line = 0; return; }
    int height = (memory->io[_LCDC - 0xFF00] & 0x04) ? 16 : 8;
    int count = 0;
    for (int i = 0; i < 40 && count < 10; i++) {
        int sprite_y = memory->oam[i * 4] - 16;
        if (ly >= sprite_y && ly < sprite_y + height)
            count++;
    }
    sprites_this_line = count;
    scx_sampled = memory->io[_SCX - 0xFF00] & 0x07;
}

void update_ppu(uint16_t cycles) {
    static uint8_t prev_ly = 0xFF;

    uint8_t lcdc = memory->io[_LCDC - 0xFF00];
    if (!(lcdc & 0x80)) {
        memory->io[_LY - 0xFF00] = 0;
        memory->io[_STAT - 0xFF00] &= 0xFC;
        ppu_cycle = (ppu_cycle + cycles) % 456;
        prev_stat_line = false;
        return;
    }

    uint8_t ly = memory->io[_LY - 0xFF00];
    if (ly != prev_ly && (lcdc & 0x80)) {
        prev_ly = ly;
        if (ly < 144) sample_line_state(ly);
    }
    uint8_t lyc = memory->io[_LYC - 0xFF00];
    uint8_t stat = memory->io[_STAT - 0xFF00];
    uint16_t scanline_cycles = 456;

    ppu_cycle += cycles;

    while (ppu_cycle >= scanline_cycles) {
        ppu_cycle -= scanline_cycles;

        if (ly < 144) {
            render_scanline(ly);
        }

        ++ly;

        if (ly < 144) sample_line_state(ly);

        if (ly == 144) {
            memory->io[_IF - 0xFF00] |= 0x01;
            stat = (stat & 0xFC) | 0x01;
            SDL_UpdateTexture(ppu_texture, NULL, framebuffer, 160 * sizeof(uint32_t));
            static uint32_t last_frame = 0;
            uint32_t now = SDL_GetTicks();
            uint32_t elapsed = now - last_frame;
            if (elapsed < 16 && last_frame != 0)
                SDL_Delay(16 - elapsed);
            last_frame = SDL_GetTicks();
            SDL_RenderCopy(ppu_renderer, ppu_texture, NULL, NULL);
            SDL_RenderPresent(ppu_renderer);
        } else if (ly >= 145 && ly <= 153) {
            stat = (stat & 0xFC) | 0x01;
        } else if (ly == 154) {
            ly = 0;
            sample_line_state(0);
            window_line_counter = 0;
            stat = (stat & 0xFC) | 0x01;
        } else {
            stat = (stat & 0xFC) | 0x02;
        }

        if (ly == lyc) {
            if (!(stat & 0x04)) {
                stat |= 0x04;
            }
        } else {
            stat &= ~0x04;
        }

        memory->io[_LY - 0xFF00] = ly;
    }

    if (ly < 144 && (lcdc & 0x80)) {
        uint8_t old_mode = stat & 0x03;
        uint8_t new_mode;
        uint16_t mode2_end = 80;
        uint16_t mode3_base = 252;
        uint16_t mode3_extra = scx_sampled + sprites_this_line * 6;
        if ((lcdc & 0x20) && (lcdc & 0x01)) {
            uint8_t wy = memory->io[_WY - 0xFF00];
            int16_t wx = memory->io[_WX - 0xFF00] - 7;
            if (ly >= wy && wx < 160)
                mode3_extra += 6;
        }
        uint16_t mode3_end = mode3_base + mode3_extra;

        if (ppu_cycle < mode2_end)       new_mode = 2;
        else if (ppu_cycle < mode3_end)  new_mode = 3;
        else                             new_mode = 0;

        if (new_mode != old_mode)
            stat = (stat & 0xFC) | new_mode;
    }

    memory->io[_STAT - 0xFF00] = stat;

    bool cur = get_stat_line(stat);
    if (cur && !prev_stat_line)
        memory->io[_IF - 0xFF00] |= 0x02;
    prev_stat_line = cur;
}
