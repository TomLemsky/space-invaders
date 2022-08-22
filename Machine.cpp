#include "Machine.h"

Machine::Machine()
{
    emu.load_program_from_file("invaders.bin");

    int n = this->screen_width * this->screen_height;
    this->textureBuffer = make_unique<uint32_t[]>(n);

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        printf("error initializing SDL: %s\n", SDL_GetError());
        exit(1);
    }
    this->win = SDL_CreateWindow("GAME",
                                       SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED,
                                       this->screen_height, this->screen_width, 0);

    this->renderer = SDL_CreateRenderer(this->win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);

    this->texture = SDL_CreateTexture( this->renderer,
        SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, this->screen_height, this->screen_width );
}

Machine::~Machine()
{
    SDL_DestroyRenderer(this->renderer);
    //delete this->textureBuffer;
}

uint8_t Machine::keyToByte(SDL_Keysym key){
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

void Machine::updateScreen(){
    uint16_t framebuffer_loc = 0x2400;
    uint16_t byte_width = 32;//WIDTH/8;
    uint16_t current_loc;

    int n = this->screen_width * this->screen_height;
    uint32_t *textureBuffer = new uint32_t[ n ];

    uint8_t* emumem = this->emu.memory.get();
    for(int y=0; y < this->screen_height; y++){
        for(int x = 0; x < byte_width; x++){
            current_loc = framebuffer_loc + byte_width*y + x;
            for(int b=0; b<8;b++){
                //printf("\n %d,%d,%d: %04X",y,x,b,current_loc);
                textureBuffer[n-(this->screen_height-y)-this->screen_height*(8*x+b)] = ((emumem[current_loc]>>b) & 1)? 0xFF0000 : 0x0000FF;
            }
        }
    }

    SDL_Rect texture_rect;
    texture_rect.x=0;
    texture_rect.y=0;
    texture_rect.w=this->screen_height;
    texture_rect.h=this->screen_width;


    SDL_UpdateTexture(this->texture, NULL, textureBuffer, this->screen_height*sizeof(uint32_t));

    SDL_RenderClear(this->renderer);
    SDL_RenderCopy(this->renderer, this->texture, NULL, &texture_rect);
    SDL_RenderPresent(this->renderer);
    //exit(1);
}

void Machine::interrupt(int num){
    if(!this->emu.interrupt_enabled) return;
    emu.call(0x08*num, 0); // Instruction: RST 1 -> Call 0x08
    emu.interrupt_enabled = false;
}

void Machine::run(){
    int instr_per_frame  = 2000000 / 60; // 2MHz CPU divided by 60Hz screen
    int millis_per_frame = 1000 / 60;

    uint32_t t_lastFrame = SDL_GetTicks();
    uint32_t t_lastInterrupt = SDL_GetTicks();
    bool first_half = true;
    bool exit_clicked = false;
    while(!exit_clicked){
        execute_next_instruction();
        SDL_PollEvent(&this->event);
        switch(this->event.type){
            case SDL_QUIT:
                exit_clicked = true;
                exit(0);
                break;
            case SDL_KEYDOWN:
                this->button_port = this->button_port | keyToByte(this->event.key.keysym);
                break;
            case SDL_KEYUP:
                this->button_port = this->button_port & (~keyToByte(this->event.key.keysym));
                break;
        }
        uint32_t t_current = SDL_GetTicks();
        if(this->emu.interrupt_enabled && (first_half && t_current-t_lastInterrupt > 8)){
            this->emu.call(0x08, 0);// RST 1 interrupt at half drawn screen
            this->emu.interrupt_enabled = false;
            first_half = false;
        }

        if(this->emu.interrupt_enabled && (t_current-t_lastInterrupt > 17)){
             t_lastInterrupt = t_current;
             this->emu.call(0x10, 0);// RST 2 interrupt at end of screen
             this->emu.interrupt_enabled = false;
             first_half = true;
             updateScreen();
        }
    }
    return;

    // while(1){
    //     t_lastFrame = SDL_GetTicks();
    //
    //     for(int i; i < (instr_per_frame/2); i++){
    //         execute_next_instruction();
    //     }
    //     SDL_Delay(millis_per_frame/2);
    //     interrupt(1);
    //     for(int i; i < (instr_per_frame/2); i++){
    //         execute_next_instruction();
    //         printf("%d\n",millis_per_frame);
    //     }
    //     SDL_Delay(millis_per_frame/2);
    //     interrupt(2);
    //
    //     updateScreen();
    //
    //
    //
    //     SDL_PollEvent(&this->event);
    //     switch(this->event.type){
    //         case SDL_QUIT:
    //             exit(0);
    //             break;
    //         case SDL_KEYDOWN:
    //             this->button_port = this->button_port | keyToByte(this->event.key.keysym);
    //             break;
    //         case SDL_KEYUP:
    //             this->button_port = this->button_port & (~keyToByte(this->event.key.keysym));
    //             break;
    //     }
    //
    // }
}

void Machine::execute_next_instruction(){

    // IN and OUT instructions get intercepted

    if(this->emu.memory[this->emu.pc] == 0xd3){ // OUT instruction
        switch(this->emu.memory[this->emu.pc+1]){ // PORT NUMBER
            case 2:
                this->shift_amount = this->emu.a & 0x07;
                break;
            case 3:
                // Sounds (Unimplemented)
                break;
            case 4:
                this->shift0 = this->shift1;
                this->shift1 = this->emu.a;
                break;
            case 5:
                // more sounds (Unimplemented)
                break;
        }
    } else if(this->emu.memory[this->emu.pc] == 0xdb){ // IN instruction
        switch(this->emu.memory[this->emu.pc+1]){ // PORT NUMBER
            case 0:
                this->emu.a = 0xF;
                break;
            case 1: // Button presses here
                this->emu.a = this->button_port;
                break;
            case 2: // settings and player 2 controls
                this->emu.a = 0x0;
                break;
            case 3:{
                uint16_t v = (this->shift1<<8) | this->shift0;
                this->emu.a = ((v >> (8-this->shift_amount))& 0xFF);
                }
                break;
        }
    }

    this->emu.execute_next_instruction();
}
