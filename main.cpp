#include "Emulator.h"
#include "Machine.h"
#include "reference_implementation/8080emu.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_timer.h>
#include <chrono>
#include <thread>

// TODO: Get the timing right!
// Fixed amount of instructions per frame? or just more accurate clock?
// Bundle instructions?
// TODO: std::chrono::high_resolution_clock for instructions or bundle multiple instr

// HInts: Can't shoot. Reset after alien's shot at half distance. Coin amounts change when button pressed

int WIDTH  = 256;
int HEIGHT = 224;

void ReadFileIntoMemoryAt(State8080* state, char* filename, uint32_t offset)
{
	FILE *f= fopen(filename, "rb");
	if (f==NULL)
	{
		printf("error: Couldn't open %s\n", filename);
		exit(1);
	}
	fseek(f, 0L, SEEK_END);
	int fsize = ftell(f);
	fseek(f, 0L, SEEK_SET);

	uint8_t *buffer = &state->memory[offset];
	fread(buffer, fsize, 1, f);
	fclose(f);
}

State8080* Init8080(void)
{
	State8080* state = (State8080*) calloc(1,sizeof(State8080));
	state->memory = (uint8_t*) malloc(0x10000);  //16K
	return state;
}

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

uint8_t keyToByte(SDL_Keysym key){
    switch(key.sym){
        case SDLK_RETURN: // Coin = ENTER
            return 0x01; // 4 = bit 2
        case SDLK_1: // 1 Player start
            return 0x04; // 4 = bit 2
        case SDLK_UP:
            return 0x10; // 32 = bit 4
        case SDLK_LEFT:
            return 0x20; // 32 = bit 5
        case SDLK_RIGHT:
            return 0x40; // 32 = bit 6
    }
    return 0x00;
}


int main(int argc, char *argv[]){
    Machine machine;
    machine.run();

}


/*
int main(int argc, char *argv[])
{
    Machine machine;
    machine.run();
    return 0;

    uint32_t *textureBuffer = new uint32_t[ WIDTH * HEIGHT ];

    uint32_t t_lastInterrupt = SDL_GetTicks();
    //std::chrono::steady_clock::time_point time_lastInterrupt = std::chrono::steady_clock::now();
    //(std::chrono::steady_clock::now()-time_lastInterrupt).count()

    // returns zero on success else non-zero
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        printf("error initializing SDL: %s\n", SDL_GetError());
        return 1;
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

    // load reference_implementation
    State8080* state = Init8080();
	ReadFileIntoMemoryAt(state, "invaders.bin", 0);



    bool first_half = true; // Are we drawing the first half of the screen?

    uint8_t shift0; // lower byte of shift register
    uint8_t shift1; // higher byte of shift register
    uint8_t shift_amount;

    uint8_t button_port = 0x00; //0x09; // single player, CREDIT, bit 3 is always 1

    while(1){
        if(emu.memory[emu.pc] == 0xd3){ // OUT instruction
            switch(emu.memory[emu.pc+1]){ // PORT NUMBER
                case 2:
                    shift_amount = emu.a & 0x07;
                    break;
                case 3:
                    // Sounds (Unimplemented)
                    break;
                case 4:
                    shift0 = shift1;
                    shift1 = emu.a;
                    break;
                case 5:
                    // more sounds (Unimplemented)
                    break;
            }
        } else if(emu.memory[emu.pc] == 0xdb){ // IN instruction
            switch(emu.memory[emu.pc+1]){ // PORT NUMBER
                case 0:
                    emu.a = 0xF;
                    break;
                case 1: // Button presses here
                    emu.a = button_port;
                    break;
                case 2: // settings and player 2 controls
                    emu.a = 0x0;
                    break;
                case 3:{
                    uint16_t v = (shift1<<8) | shift0;
                    emu.a = ((v >> (8-shift_amount))& 0xFF);
                    }
                    break;
            }
            state->a = emu.a;
        }


        emu.execute_next_instruction();
        Emulate8080Op(state); // reference_implementation
        std::this_thread::sleep_for(std::chrono::nanoseconds(1));



        SDL_PollEvent(&event);
        switch(event.type){
            case SDL_QUIT:
                return 1;
                break;
            case SDL_KEYDOWN:
                button_port = button_port | keyToByte(event.key.keysym);
                break;
            case SDL_KEYUP:
                button_port = button_port & (~keyToByte(event.key.keysym));
                break;
        }

        uint32_t t_current = SDL_GetTicks();
        if(emu.interrupt_enabled && (first_half && t_current-t_lastInterrupt > 8)){
            emu.call(0x08, 0);// RST 1 interrupt at half drawn screen
            emu.interrupt_enabled = false;
            GenerateInterrupt(state, 1);
            first_half = false;
        }

        if(emu.interrupt_enabled && (t_current-t_lastInterrupt > 17)){
             t_lastInterrupt = t_current;
             emu.call(0x10, 0);// RST 2 interrupt at end of screen
             GenerateInterrupt(state, 2);
             emu.interrupt_enabled = false;
             first_half = true;
             updateScreen(renderer, texture, textureBuffer, emu.memory.get());
        }

        // Compare ref impl to my impl
        //if (1){printf("hello");} else
        if(state->pc!= emu.pc){
            printf("Reference pc: %04X  My pc: %04X",state->pc,emu.pc);
            printf("\nError in pc!\n");
            printf("Stack: reference: %04x; Mine: %04x", state->memory[state->sp-1],emu.memory[emu.sp-1]);
            return 1;
        } else if (state->a!= emu.a){
            printf("\nError in Register a!\n");
            return 1;
        } else if (state->b!= emu.b){
            printf("\nError in Register b!\n");
            return 1;
        } else if (state->c!= emu.c){
            printf("\nError in Register c!\n");
            return 1;
        } else if (state->d!= emu.d){
            printf("\nError in Register d!\n");
            return 1;
        } else if (state->e!= emu.e){
            printf("\nError in Register e!\n");
            return 1;
        } else if (state->h!= emu.h){
            printf("\nError in Register h!\n");
            return 1;
        } else if (state->l!= emu.l){
            printf("\nError in Register l!\n");
            return 1;
        } else if (state->sp!= emu.sp){
            printf("\nError in sp!\n");
            return 1;
        } else if (state->cc.z != emu.flags.z){
            printf("\nError in flag z!\n");
            return 1;
        } else if (state->cc.s != emu.flags.s){
            printf("\nError in flag s!\n");
            return 1;
        } else if (state->cc.p != emu.flags.p){
            printf("\nError in flag p!\n");
            return 1;
        } else if (state->cc.cy != emu.flags.cy){
            printf("\nError in flag cy!\n");
            return 1;
        } else if (state->cc.ac != emu.flags.ac){
            printf("\nError in flag ac!\n");
            return 1;
        } else if (state->memory[state->sp]!= emu.memory[emu.sp]) {
            printf("Last stack position is wrong!");
            printf("reference: %02X; Mine: %02X", state->memory[state->sp],emu.memory[emu.sp]);
            return 1;
        }
    }
    return 0;
}
//*/
