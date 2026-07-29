#ifndef PPU_H
#define PPU_H

#include "src/structs.h"
#include <SDL2/SDL.h>

extern const uint32_t shades[4];
extern uint32_t framebuffer[160 * 144];
extern uint8_t bg_color[160 * 144];
extern uint8_t sprites_this_line;
extern uint8_t scx_sampled;
extern uint8_t window_line_counter;
extern bool prev_stat_line;

extern SDL_Texture *ppu_texture;
extern SDL_Renderer *ppu_renderer;

bool get_stat_line(uint8_t stat);
void render_scanline(uint8_t ly);
void update_ppu(uint16_t cycles);

#endif
