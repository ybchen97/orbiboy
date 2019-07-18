#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <SDL2/SDL.h>


#include "Emulator.hpp"

#ifdef __EMSCRIPTEN__
#include "emscripten.h"
#endif

using namespace std;

const float millisPerFrame = 1000.0 / 59.7275;
const chrono::duration<float, milli> timePerFrame(millisPerFrame);

SDL_Renderer* sdlRenderer;
SDL_Texture* sdlTexture;
Emulator emulator;
bool continueGame;

void render(SDL_Renderer* renderer, SDL_Texture* texture, Emulator& emu) {

    SDL_UpdateTexture(texture, NULL, emu.displayPixels, 160 * sizeof(Uint32));
    SDL_RenderClear(renderer);
    const SDL_Rect dest = {.x = 0, .y = 0, .w = 160*4, .h = 144*4};
    SDL_RenderCopy(renderer, texture, NULL, &dest);
    SDL_RenderPresent(renderer);

}

void doRender() {
    
    render(sdlRenderer, sdlTexture, emulator);

}

void processInput(Emulator& emulator, SDL_Event& event, bool& gameRunning) {

    if (event.type == SDL_KEYDOWN) {
        int key = -1;
        switch (event.key.keysym.sym) {
            case SDLK_a:        key = 4; break;
            case SDLK_s:        key = 5; break;
            case SDLK_RETURN:   key = 7; break;
            case SDLK_SPACE:    key = 6; break;
            case SDLK_RIGHT:    key = 0; break;
            case SDLK_LEFT:     key = 1; break;
            case SDLK_UP:       key = 2; break;
            case SDLK_DOWN:     key = 3; break;
            case SDLK_ESCAPE:   gameRunning = false;
        }
        if (key != -1) {
            emulator.buttonPressed(key);
        }
    } else if (event.type == SDL_KEYUP) {
        int key = -1;
        switch (event.key.keysym.sym) {
            case SDLK_a:        key = 4; break;
            case SDLK_s:        key = 5; break;
            case SDLK_RETURN:   key = 7; break;
            case SDLK_SPACE:    key = 6; break;
            case SDLK_RIGHT:    key = 0; break;
            case SDLK_LEFT:     key = 1; break;
            case SDLK_UP:       key = 2; break;
            case SDLK_DOWN:     key = 3; break;
        }
        if (key != -1) {
            emulator.buttonReleased(key);
        }
    }

}

void mainloop() {
    bool gameRunning = true;

    // Emulation loop
    SDL_Event event;
    auto previous = chrono::high_resolution_clock::now();

        
        // Process user input
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                gameRunning = false;
                continue;
            }
            processInput(emulator, event, gameRunning);
        }

        emulator.update();

        // Sleep to use up the rest of the frame time
        auto current = chrono::high_resolution_clock::now();
        auto timeElapsed = current - previous;
        if (timeElapsed < timePerFrame) {
            this_thread::sleep_for(chrono::duration<float, milli> (timePerFrame - timeElapsed));
        }

        previous = chrono::high_resolution_clock::now();

    if (!gameRunning) { 
        #ifdef __EMSCRIPTEN__
            emscripten_cancel_main_loop();
        #endif

        SDL_Quit();
        continueGame = false;
    }
}

// Loading function abstracted so it can be called by javascript from the client
extern "C" {
void load(string romPath) {

    emulator.resetCPU();
    emulator.setRenderGraphics(&doRender);

    if (!emulator.loadGame(romPath)) {
        cout << "Something wrong occured while loading!" << endl;
        exit(4);
    }

}
}

extern "C" {
int main(int argc, char** argv) {

    // Screen dimensions
    int windowWidth = 160;
    int windowHeight = 144;

    // The window we'll be rendering to
    SDL_Window* window;

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        exit(1);
    }

    // Create window
    window = SDL_CreateWindow(
        "orbiboy",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        windowWidth*4,
        windowHeight*4,
        SDL_WINDOW_SHOWN
    );
    if (window == nullptr) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        exit(2);
    }

    // Create renderer
    sdlRenderer = SDL_CreateRenderer(
        window, 
        -1, 
        SDL_RENDERER_ACCELERATED
    );
    if (sdlRenderer == nullptr) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        exit(3);
    }

    // Create texture
    sdlTexture = SDL_CreateTexture(
        sdlRenderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        160, 144
    );

    // Initialize emulator
    emulator = Emulator();
    emulator.resetCPU();
    emulator.setRenderGraphics(&doRender);

    // Load game
    string romPath = "Tetris.gb";
    // string romPath = "marioland.gb";
    // string romPath = "marioland4.gb";

    // string romPath = "../../gb-test-roms/cpu_instrs/cpu_instrs.gb"; // Passed all but first one
    // string romPath = "../../gb-test-roms/cpu_instrs/individual/01-special.gb"; // FAILED

    // All Passed
    // string romPath = "../../gb-test-roms/cpu_instrs/opus5.gb";
    // string romPath = "../../gb-test-roms/cpu_instrs/individual/02-interrupts.gb";
    // string romPath = "../../gb-test-roms/cpu_instrs/individual/03-op sp,hl.gb";
    // string romPath = "../../gb-test-roms/cpu_instrs/individual/04-op r,imm.gb";
    // string romPath = "../../gb-test-roms/cpu_instrs/individual/05-op rp.gb";
    // string romPath = "../../gb-test-roms/cpu_instrs/individual/06-ld r,r.gb";
    // string romPath = "../../gb-test-roms/cpu_instrs/individual/07-jr,jp,call,ret,rst.gb";
    // string romPath = "../../gb-test-roms/cpu_instrs/individual/08-misc instrs.gb";
    // string romPath = "../../gb-test-roms/cpu_instrs/individual/09-op r,r.gb";
    // string romPath = "../../gb-test-roms/cpu_instrs/individual/10-bit ops.gb";
    // string romPath = "../../gb-test-roms/cpu_instrs/individual/11-op a,(hl).gb";

    // if (!emulator.loadGame(romPath)) {
    //     cout << "Something wrong occured while loading!" << endl;
    //     exit(4);
    // }

    load(romPath);

    #ifdef __EMSCRIPTEN__
        emscripten_set_main_loop(mainloop, 0, 1);
    #else
        continueGame = true;
        while (continueGame) {
            mainloop();
        }
    #endif

    return 0;

}
}