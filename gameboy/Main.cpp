#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <SDL2/SDL.h>

#include "Emulator.hpp"

const float millisPerFrame = 1000.0 / 59.7275;
const chrono::duration<float, milli> timePerFrame(millisPerFrame);

void processInput(Emulator& emulator, SDL_Event& event) {

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
        windowWidth,
        windowHeight,
        SDL_WINDOW_SHOWN
    );
    if (window == nullptr) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        exit(2);
    }

    // Create renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, 
        -1, 
        SDL_RENDERER_ACCELERATED
    );
    if (renderer = nullptr) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        exit(3);
    }

    // Create texture
    SDL_Texture* sdlTexture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        160, 144
    );

    // Initialize emulator
    Emulator emulator = Emulator();
    emulator.resetCPU();

    // Load game
    string romPATH = "ROM PATH FILES";
    if (!emulator.loadGame(romPATH)) {
        cout << "Something wrong occured while loading!" << endl;
        exit(4);
    }

    bool gameRunning = true;

    // Emulation loop
    SDL_Event event;
    auto previous = chrono::high_resolution_clock::now();
    while (gameRunning) {
        
        // Process user input
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                gameRunning = false;
                continue;
            }
            processInput(emulator, event);
        }

        emulator.update();

        // Sleep to use up the rest of the frame time
        auto current = chrono::high_resolution_clock::now();
        auto timeElapsed = current - previous;
        if (timeElapsed < timePerFrame) {
            this_thread::sleep_for(chrono::duration<float, milli> (timePerFrame - timeElapsed));
        }

        previous = chrono::high_resolution_clock::now();

    }

    SDL_Quit();

    return 0;

}