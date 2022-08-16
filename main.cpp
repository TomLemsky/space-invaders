#include "Emulator.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_timer.h>

// TODO: std::chrono::high_resolution_clock for instructions or bundle multiple instr

int WIDTH  = 256;
int HEIGHT = 224;

void updateScreen(SDL_Renderer* renderer, SDL_Texture* texture, uint32_t *textureBuffer, uint8_t* memory){
    uint16_t framebuffer_loc = 0x2400;
    uint16_t byte_width = 32;//WIDTH/8;
    uint16_t current_loc;

    for(int y=0; y < HEIGHT; y++){
        for(int x = 0; x < byte_width; x++){
            current_loc = framebuffer_loc + byte_width*y + x;
            for(int b=0; b<8;b++){
                //printf("\n %d,%d,%d: %04X",y,x,b,current_loc);
                textureBuffer[y+(WIDTH-8*x-b)*HEIGHT] = ((memory[current_loc]>>b) & 1)? 0xFF0000 : 0x0000FF;
            }
        }
    }

    SDL_Rect texture_rect;
    texture_rect.x=0;
    texture_rect.y=0;
    texture_rect.w=HEIGHT;
    texture_rect.h=WIDTH;

    SDL_UpdateTexture(texture, NULL, textureBuffer, HEIGHT*sizeof(uint32_t));

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, &texture_rect);
    SDL_RenderPresent(renderer);
    //exit(1);
}




int main(int argc, char *argv[])
{
    uint32_t *textureBuffer = new uint32_t[ WIDTH * HEIGHT ];

    uint32_t t_lastInterrupt = SDL_GetTicks();

    // returns zero on success else non-zero
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        printf("error initializing SDL: %s\n", SDL_GetError());
    }
    SDL_Window* win = SDL_CreateWindow("GAME",
                                       SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED,
                                       HEIGHT, WIDTH, 0);

    SDL_Renderer* renderer = SDL_CreateRenderer(win, -1,
		SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);

    SDL_Texture* texture = SDL_CreateTexture( renderer,
        SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, HEIGHT, WIDTH );

    //while (1) ;

    Emulator emu;
    emu.load_program_from_file("invaders.bin");

    SDL_Event event;


    bool first_half = true; // Are we drawing the first half of the screen?

    uint8_t shift0; // lower byte of shift register
    uint8_t shift1; // higher byte of shift register
    uint8_t shift_amount;

    while(1){
        // if(emu.memory[emu.pc] == 0xd3){ // OUT instruction
        //     switch(emu.memory[emu.pc+1]){ // PORT NUMBER
        //         case 2:
        //             shift_amount = emu.a & 0x07;
        //             break;
        //         case 3:
        //             // Sounds (Unimplemented)
        //             break;
        //         case 4:
        //             shift0 = shift1;
        //             shift1 = emu.a;
        //             break;
        //         case 5:
        //             // more sounds (Unimplemented)
        //             break;
        //     }
        // } else if(emu.memory[emu.pc] == 0xdb){ // IN instruction
        //     switch(emu.memory[emu.pc+1]){ // PORT NUMBER
        //         case 0:
        //             emu.a = 0xF;
        //             break;
        //         case 1: // Button presses here
        //             emu.a = 0x0;
        //             break;
        //         case 2:
        //             emu.a = 0x0;
        //             break;
        //         case 3:{
        //             uint16_t v = (shift1<<8) | shift0;
        //             emu.a = ((v >> (8-shift_amount))& 0xFF);
        //             }
        //             break;
        //     }
        //
        //
        // }

        emu.execute_next_instruction();

        SDL_PollEvent(&event);
        if(event.type==SDL_QUIT) break;

        uint32_t t_current = SDL_GetTicks();
        if(first_half && t_current-t_lastInterrupt > 8){
            emu.call(0x08, 0);// interrupt at half drawn screen
            first_half = false;
        }

        if(t_current-t_lastInterrupt > 17){
             t_lastInterrupt = t_current;
             emu.call(0x10, 0);// interrupt at end of screen
             first_half = true;
             updateScreen(renderer, texture, textureBuffer, emu.memory.get());
        }
    }
    return 0;
}
