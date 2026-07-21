#include "instructions.h"

extern GameBoyMemory *memory;
extern registers *reg;

SDL_Window   *ppu_window;
SDL_Renderer *ppu_renderer;
SDL_Texture  *ppu_texture;

uint8_t opcode;
uint8_t joypad_dpad = 0x0F; 
uint8_t joypad_btn  = 0x0F; 

int main(int argc, char *argv[]) {
    if(argc < 2) {
        printf("Usage: %s <path_to_rom>\n", argv[0]);
        return 1;
    }
    memory = calloc(GAMEBOY_MEMORY_SIZE, sizeof(uint8_t));
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

    SDL_Init(SDL_INIT_VIDEO);
    ppu_window   = SDL_CreateWindow("Game Boy", SDL_WINDOWPOS_CENTERED,
                                    SDL_WINDOWPOS_CENTERED, 160*4, 144*4, 0);
    ppu_renderer = SDL_CreateRenderer(ppu_window, -1, SDL_RENDERER_ACCELERATED);
    ppu_texture  = SDL_CreateTexture(ppu_renderer, SDL_PIXELFORMAT_ARGB8888,
                                     SDL_TEXTUREACCESS_STREAMING, 160, 144);
    SDL_RenderSetLogicalSize(ppu_renderer, 160, 144);

    bool go = true;
    while(go){
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) go = false;
            else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                bool pressed = (e.type == SDL_KEYDOWN);
                switch (e.key.keysym.sym) {
                    case SDLK_RIGHT:   joypad_dpad = pressed ? joypad_dpad & ~1  : joypad_dpad | 1;  break; // right
                    case SDLK_LEFT:    joypad_dpad = pressed ? joypad_dpad & ~2  : joypad_dpad | 2;  break; // left
                    case SDLK_UP:      joypad_dpad = pressed ? joypad_dpad & ~4  : joypad_dpad | 4;  break; // up
                    case SDLK_DOWN:    joypad_dpad = pressed ? joypad_dpad & ~8  : joypad_dpad | 8;  break; // down
                    case SDLK_z:       joypad_btn  = pressed ? joypad_btn & ~1   : joypad_btn | 1;   break;  // A
                    case SDLK_x:       joypad_btn  = pressed ? joypad_btn & ~2   : joypad_btn | 2;   break;  // B
                    case SDLK_RSHIFT:  joypad_btn  = pressed ? joypad_btn & ~4   : joypad_btn | 4;   break;  // Select
                    case SDLK_RETURN:  joypad_btn  = pressed ? joypad_btn & ~8   : joypad_btn | 8;   break;  // Start
                    case SDLK_ESCAPE:  go = false; break;
                }
            }
        }
        if (ime) {
            handle_interrupts();
        }
        opcode = read_byte(reg->pc);
        // printf("PC: 0x%04X | Opcode: 0x%02X\n", reg->pc, opcode);
        ++reg->pc;
        if (opcode_table[opcode] != NULL) {
            opcode_table[opcode]();
        } else {
            exit(EXIT_FAILURE);
        }
        if (ime_next >= 0) {
            if (ime_next == 0) {
                ime = ei;            
                ime_next = -1; // Reset tracker
            }
            ime_next--;
        }
    }
    printf("exiting");
    SDL_DestroyTexture(ppu_texture);
    SDL_DestroyRenderer(ppu_renderer);
    SDL_DestroyWindow(ppu_window);
    SDL_Quit();
    fclose(file);
    free(memory);
    free(reg);
    exit(EXIT_SUCCESS);
}
