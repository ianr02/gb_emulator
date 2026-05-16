#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#define GB_WIDTH  160
#define GB_HEIGHT 144
#define SCALE     5  

Display *display;
Window window;
GC gc;
XImage *x_image;

uint32_t framebuffer[GB_WIDTH * GB_HEIGHT]; 
uint32_t *scaled_buffer; 

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
        1, BlackPixel(display, window), 0x26FF99
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

    // 2. Blit the image to the window
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
        // Handle window events so it doesn't look frozen to Fedora
        while (XPending(display)) {
            XEvent event;
            XNextEvent(display, &event);
            if (event.type == ClientMessage || event.type == KeyPress) {
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