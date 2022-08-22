#ifndef MACHINE_H
#define MACHINE_H

#include "Emulator.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_timer.h>
#include <chrono>
#include <thread>
#include <string>

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

        uint8_t button_port = 0x09; // single player, CREDIT, bit 3 is always 1

        uint8_t keyToByte(SDL_Keysym key);
        void updateScreen();
        void execute_next_instruction();
        void interrupt(int num);
};

#endif // MACHINE_H
