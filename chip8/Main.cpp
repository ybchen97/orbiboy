#include<iostream>
#include<cstdlib>
#include<chrono>
#include<thread>
#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdio.h>

#include "Chip8.cpp"

using namespace std;

// Keypad keymap
BYTE keymap[16] = {
    SDLK_x,
    SDLK_1,
    SDLK_2,
    SDLK_3,
    SDLK_q,
    SDLK_w,
    SDLK_e,
    SDLK_a,
    SDLK_s,
    SDLK_d,
    SDLK_z,
    SDLK_c,
    SDLK_4,
    SDLK_r,
    SDLK_f,
    SDLK_v,
};

int main(int argc, char **argv) {

    // Initialize Chip8
    Chip8 chip8 = Chip8();
    chip8.initialize();

    int w = 1024;                   // Window width
    int h = 512;                    // Window height

    // The window we'll be rendering to
    SDL_Window* window = NULL;

    // Initialize SDL
    if ( SDL_Init(SDL_INIT_EVERYTHING) < 0 ) {
        printf( "SDL could not initialize! SDL_Error: %s\n", SDL_GetError() );
        exit(1);
    }
    // Create window
    window = SDL_CreateWindow(
            "CHIP-8 Emulator",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            w, h, SDL_WINDOW_SHOWN
    );
    if (window == NULL){
        printf( "Window could not be created! SDL_Error: %s\n",
                SDL_GetError() );
        exit(2);
    }

    // Create renderer
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_RenderSetLogicalSize(renderer, w, h);

    // Create texture that stores frame buffer
    SDL_Texture* sdlTexture = SDL_CreateTexture(renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            64, 32);

    // Temporary pixel buffer
    uint32_t pixels[2048];

    // Attempt to load ROM
    if (!chip8.loadGame("INVADERS")) {
        printf("Some problem occured while loading!\n");
        return 2;
    }

    // Emulation speed settings
    int fps = 60;
    int cyclesPerSecond = 400;
    int cyclesPerFrame = cyclesPerSecond / fps;
    chrono::duration<int, milli> oneSecond(1000);
    auto oneFrame = oneSecond/ fps;

    auto start = std::chrono::high_resolution_clock::now();


    // Emulation loop
    while (true) {

        // Process SDL events
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) exit(0);

            // Process keydown events
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE)
                    exit(0);

                for (int i = 0; i < 16; ++i) {
                    if (e.key.keysym.sym == keymap[i]) {
                        chip8.keyState[i] = 1;
                        cout << "Key " << i << " state: " << chip8.keyState[i] << endl;
                    }
                }
            }
            // Process keyup events
            if (e.type == SDL_KEYUP) {
                for (int i = 0; i < 16; ++i) {
                    if (e.key.keysym.sym == keymap[i]) {
                        chip8.keyState[i] = 0;
                        cout << "Key " << i << " state: " << chip8.keyState[i] << endl;
                    }
                }
            }
        }

        auto current = std::chrono::high_resolution_clock::now();

        if (current > start + oneFrame) {
            chip8.decrementTimers();

            // Run emulation cycle
            for (int i = 0; i < cyclesPerFrame; ++i) {
                chip8.runEmulationCycle();
            }
            start = current;

            // Update screen pixels
            // Store pixels in temporary buffer
            for (int i = 0; i < 2048; ++i) {
                BYTE pixel = chip8.gameScreen[i];
                pixels[i] = (0x00FFFFFF * pixel) | 0xFF000000;
            }
            // Update SDL texture
            SDL_UpdateTexture(sdlTexture, NULL, pixels, 64 * sizeof(Uint32));
            // Clear screen and render
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, sdlTexture, NULL, NULL);
            SDL_RenderPresent(renderer);
        }

    }

}