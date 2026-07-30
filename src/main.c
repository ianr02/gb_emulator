#include "cpu/cpu_instructions.h"
#include "core/emulator_core.h"
#include "ppu/ppu.h"
#include "apu/apu.h"

SDL_Window   *ppu_window;
SDL_Renderer *ppu_renderer;
SDL_Texture  *ppu_texture;

uint8_t joypad_dpad = 0x0F; 
uint8_t joypad_btn  = 0x0F; 
char savepath[256];
size_t save_size;
static SDL_AudioDeviceID audio_dev = 0;

void exit_game();

int main(int argc, char *argv[]) {
    if(argc < 2) {
        printf("Usage: %s <path_to_rom>\n", argv[0]);
        return 1;
    }
    memory = calloc(1, sizeof(GameBoyMemory));
    reg = malloc(sizeof(registers));

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        printf("Error: Could not open ROM file %s\n", argv[1]);
        return 1;
    }

    struct stat st;
    fstat(fd, &st);                        
    memory->rom_size = st.st_size;
    if (read(fd, memory->rom, memory->rom_size) != memory->rom_size) {
        printf("Error: Could not load ROM file %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }
    close(fd);

    init_io_ports();
    apu_init();

    reg->pc = INIT_PC;
    reg->af = 0x01B0;
    reg->bc = 0x0013;
    reg->de = 0x00D8;
    reg->hl = 0x014D;
    reg->sp = 0xFFFE;

    // Read cartridge type for MBC detection:
    uint8_t cart = memory->rom[0x0147];
    if(cart >= 0x01 && cart <= 0x03) {
        memory->cart_type = CART_MBC1;
        memory->rom_bank = 1;
    } else if (cart == 0x05 || cart == 0x06){
        memory->cart_type = CART_MBC2;
        memory->rom_bank = 1;
    } else if(cart >= 0x0F && cart <= 0x13)
        memory->cart_type = CART_MBC3;
    else if (cart >= 0x19 && cart <= 0x1E) 
        memory->cart_type = CART_MBC5;
    else {
        memory->cart_type = CART_ROM_ONLY;
    }
    memory->rom_bank = 1;

    char title[17];
    memcpy(title, &memory->rom[0x0134], 16);
    title[16] = '\0';

    switch (memory->rom[0x0149]) {
        case 1: save_size = 0x800;   break;   // 2KB
        case 2: save_size = 0x2000;  break;   // 8KB
        case 3: save_size = 0x8000;  break;   // 32KB
        case 4: save_size = 0x20000; break;   // 128KB 
        case 5: save_size = 0x10000; break;   // 64KB
        default: save_size = 0;
    }
    if (memory->cart_type == CART_MBC2) save_size = 256;

    if (mkdir(".saves", 0755) != 0 && errno != EEXIST) {
        perror("mkdir");
        exit(EXIT_FAILURE);
    }

    snprintf(savepath, sizeof(savepath), ".saves/%s.sav", title);
    int sf = open(savepath, O_RDONLY);
    if (sf != -1) {
        read(sf, memory->external, save_size);
        if (memory->cart_type == CART_MBC3) {
            struct stat sav_st;
            fstat(sf, &sav_st);
            if (sav_st.st_size >= (off_t)(save_size + 5)) {
                lseek(sf, save_size, SEEK_SET);
                read(sf, memory->rtc_regs, 5);
            }
        }
        close(sf);
    }

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    ppu_window   = SDL_CreateWindow("Game Boy", SDL_WINDOWPOS_CENTERED,
                                    SDL_WINDOWPOS_CENTERED, 160*4, 144*4, 0);
    ppu_renderer = SDL_CreateRenderer(ppu_window, -1, SDL_RENDERER_ACCELERATED);
    ppu_texture  = SDL_CreateTexture(ppu_renderer, SDL_PIXELFORMAT_ARGB8888,
                                     SDL_TEXTUREACCESS_STREAMING, 160, 144);
    SDL_RenderSetLogicalSize(ppu_renderer, 160, 144);

    SDL_AudioSpec want = {44100, AUDIO_S16SYS, 2, 0, 4096, 0, 0};
    SDL_AudioSpec have;
    audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audio_dev > 0) SDL_PauseAudioDevice(audio_dev, 0);

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
                memory->io[_IF - 0xFF00] |= 0x10;
            }
        }
        if (ime) {
            handle_interrupts();
        }
        opcode = read_byte(reg->pc);
        if (!halt_bug) {
            ++reg->pc;
        } else
            halt_bug = false;

        if (opcode_table[opcode] != NULL) {
            opcode_table[opcode]();
        } else {
            exit(EXIT_FAILURE);
        }

        if (ime_next >= 0) {
            if (ime_next == 0) {
                ime = ei;            
                ime_next = -1; // Reset tracker
            } else 
                ime_next--;
        }

        static int audio_tick = 0;
        audio_tick = (audio_tick + 1) & 7;
        if (audio_tick == 0 && audio_dev > 0) {
            int16_t buf[512];
            uint16_t n = apu_read_samples(buf, 256);
            if (n > 0)
                SDL_QueueAudio(audio_dev, buf, n * 4);
        }
    }
    exit_game();
    exit(EXIT_SUCCESS);
}

void exit_game() {
    int sf = open(savepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (sf != -1) {
        write(sf, memory->external, save_size);
        if (memory->cart_type == CART_MBC3)
            write(sf, memory->rtc_regs, 5);
        close(sf);
    } 
    SDL_DestroyTexture(ppu_texture);
    SDL_DestroyRenderer(ppu_renderer);
    SDL_DestroyWindow(ppu_window);
    SDL_Quit();
    free(memory);
    free(reg);
}