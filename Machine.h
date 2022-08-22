#ifndef MACHINE_H
#define MACHINE_H

#include "Emulator.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_timer.h>
#include <chrono>
#include <thread>

class Machine
{
    public:
        Machine();
        virtual ~Machine();
        void run();

        int screen_width = 256;
        int screen_height = 224;

    protected:

    private:
        SDL_Window* win;
        SDL_Renderer* renderer;
        SDL_Texture* texture;
        unique_ptr<uint32_t[]> textureBuffer;
        //uint32_t *textureBuffer;
        Emulator emu;
        SDL_Event event;

        uint8_t shift0; // lower byte of shift register
        uint8_t shift1; // higher byte of shift register
        uint8_t shift_amount;

        uint8_t button_port = 0x09; // single player, CREDIT, bit 3 is always 1

        uint8_t keyToByte(SDL_Keysym key);
        void updateScreen();
        void execute_next_instruction();
        void interrupt(int num);
};

#endif // MACHINE_H
