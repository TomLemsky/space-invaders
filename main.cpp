#include "Emulator.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_timer.h>

// TODO: std::chrono::high_resolution_clock for instructions or bundle multiple instr

int WIDTH  = 256;
int HEIGHT = 224;

void updateScreen(SDL_Texture* texture, uint32_t *textureBuffer, uint8_t* memory){
    uint16_t framebuffer_loc = 0x2400;
    uint16_t byte_width = 32;//WIDTH/8;
    uint16_t current_loc;

    for(int y=0; y < HEIGHT; y++){
        for(int x = 0; x < byte_width; x++){
            current_loc = framebuffer_loc + byte_width*y + x;
            for(int b=0; b<8;b++){
                //printf("\n %d,%d,%d: %04X",y,x,b,current_loc);

                if((memory[current_loc]>>b) & 1){
                    textureBuffer[(y*WIDTH)+8*x+b] = 0xFF0000;
                } else {
                    textureBuffer[(y*WIDTH)+8*x+b] = 0x0000FF;
                }

            }
        }
    }
    SDL_UpdateTexture(texture, NULL, textureBuffer, WIDTH*sizeof(uint32_t));
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
                                       WIDTH, HEIGHT, 0);

    SDL_Renderer* renderer = SDL_CreateRenderer(win, -1,
		SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);

    SDL_Texture* texture = SDL_CreateTexture( renderer,
        SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT );

    //while (1) ;

    Emulator emu;
    emu.load_program_from_file("invaders.bin");

    SDL_Event event;

    SDL_Rect texture_rect;
    texture_rect.x=0;
    texture_rect.y=0;
    texture_rect.w=WIDTH;
    texture_rect.h=HEIGHT;
    while(1){
        emu.execute_next_instruction();
        updateScreen(texture,textureBuffer,emu.memory.get());
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, &texture_rect);
        SDL_RenderPresent(renderer);

        SDL_PollEvent(&event);
        if(event.type==SDL_QUIT) break;

        uint32_t t_current = SDL_GetTicks();
        if(t_current-t_lastInterrupt < 16){
            emu.call(0x0010, 0);// interrupt 2
        }
    }
    return 0;
}
