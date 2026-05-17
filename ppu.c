#include "structs.h"

#include <stdio.h>
#include <unistd.h>

#define GB_WIDTH  160
#define GB_HEIGHT 144
#define SCALE     5

extern GameBoyMemory *memory;

uint32_t framebuffer[GB_WIDTH * GB_HEIGHT]; 
uint32_t *scaled_buffer; 

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
        memory->io[address - 0xFF4C] = val;
    } else if (address >= 0xFF80 && address <= 0xFFFE){
        memory->hram[address - 0xFF80] = val;
    } else if (address == 0xFFFF){
        memory->ie = val;
    } else {
        exit(EXIT_FAILURE);
    }
    return 0;
}

#if defined(__linux__)
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>

    Display *display;
    Window window;
    GC gc;
    XImage *x_image;

    void init_window() {
    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Error: Cannot open X display. Is your desktop environment running?\n");
        exit(1);
    }

    int screen = DefaultScreen(display);
    Visual *visual = DefaultVisual(display, screen);
    int depth = DefaultDepth(display, screen);
    
    window = XCreateSimpleWindow(
        display, RootWindow(display, screen), 
        10, 10, GB_WIDTH * SCALE, GB_HEIGHT * SCALE, 
        1, BlackPixel(display, screen), 0x26FF99
    );

    XStoreName(display, window, "X11 GameBoy Emulator");
    XSelectInput(display, window, ExposureMask | KeyPressMask | StructureNotifyMask);
    XMapWindow(display, window);
    gc = XCreateGC(display, window, 0, NULL);
    scaled_buffer = malloc(GB_WIDTH * SCALE * GB_HEIGHT * SCALE * sizeof(uint32_t));

    x_image = XCreateImage(
        display, visual, depth, ZPixmap, 0, 
        (char *)scaled_buffer, GB_WIDTH * SCALE, GB_HEIGHT * SCALE, 
        32, 0
    );
    }

    void render_frame() {
        for (int y = 0; y < GB_HEIGHT; y++) {
            for (int x = 0; x < GB_WIDTH; x++) {
                uint32_t color = framebuffer[y * GB_WIDTH + x];
                
                for (int sy = 0; sy < SCALE; sy++) {
                    for (int sx = 0; sx < SCALE; sx++) {
                        int dest_x = (x * SCALE) + sx;
                        int dest_y = (y * SCALE) + sy;
                        scaled_buffer[dest_y * (GB_WIDTH * SCALE) + dest_x] = color;
                    }
                }
            }
        }

        XPutImage(display, window, gc, x_image, 0, 0, 0, 0, GB_WIDTH * SCALE, GB_HEIGHT * SCALE);
        XFlush(display); 
    }

    void close_window() {
        XDestroyImage(x_image); 
        XFreeGC(display, gc);
        XDestroyWindow(display, window);
        XCloseDisplay(display);
    }

    int main() {
        init_window();
        printf("Window initialized successfully via XWayland!\n");

        int running = 1;
        while (running) {
            while (XPending(display)) {
                XEvent event;
                XNextEvent(display, &event);
                if (event.type == ClientMessage) {
                    running = 0;
                }
            }

            for (int i = 0; i < GB_WIDTH * GB_HEIGHT; i++) {
                uint8_t noise = rand() % 256;
                framebuffer[i] = (noise << 16) | (noise << 8) | noise; 
            }

            render_frame();
            usleep(16666); 
        }

        close_window();
        printf("Window closed cleanly.\n");
        return 0;
    }
#elif defined(__APPLE__) && defined(__MACH__) && TARGET_OS_OSX
    #include <TargetConditionals.h>
#endif

free(scaled_buffer);