#ifndef MACHINE_H
#define MACHINE_H

#include "Emulator.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_timer.h>
#include <chrono>
#include <thread>
#include <string>

// This class represents the arcade machine and displays video signal with SDL

class Machine
{
    public:
        Machine(const std::string& filename="invaders.bin");
        virtual ~Machine();
        void run();

    private:

        SDL_Window* win;
        SDL_Renderer* renderer;
        SDL_Texture* texture;
        unique_ptr<uint32_t[]> textureBuffer;
        Emulator emu;
        SDL_Event event;

        int screen_width  = 256;
        int screen_height = 224;
        int window_width  = 224*3;
        int window_height = 256*3;

        uint8_t shift0; // lower byte of shift register
        uint8_t shift1; // higher byte of shift register
        uint8_t shift_amount; // how much to shift the shift register

        uint8_t out_port0 = 0x0F; // first four bits always 1, then fire, left, right
        uint8_t out_port1 = 0x09; // player 1 controls, 1P/2P START, CREDIT, bit 3 is always 1
        uint8_t out_port2 = 0x03; // player 2 controls, difficulty dip switches, lives: 3+2*(bit1)+(bit0)

        void keyPress(SDL_Keysym key, bool key_pressed);
        void updateScreen();
        void execute_next_instruction();
        void interrupt(int num);
};

#endif // MACHINE_H
