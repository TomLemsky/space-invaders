#include "Machine.h"

Machine::Machine(const std::string& filename)
{
    // if ROM is provided as invaders.e, invaders.h, ...
    if(filename.back() == '.'){
        string endings = "hgfe"; // standard file endings are "little endian"
        for(int i=0; i<4;i++){
            emu.load_program_from_file(filename+endings[i],0x0800*i);
        }
    } else {
        emu.load_program_from_file(filename);
    }

    int n = this->screen_width * this->screen_height;
    this->textureBuffer = make_unique<uint32_t[]>(n);

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        printf("error initializing SDL: %s\n", SDL_GetError());
        exit(1);
    }
    this->win = SDL_CreateWindow("Space Invaders",
                                       SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED,
                                       this->window_width, this->window_height, 0);

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

void Machine::keyPress(SDL_Keysym key, bool key_pressed){
    // which bit to change in port 1
    uint8_t bit_port0 = 0x00;
    uint8_t bit_port1 = 0x00;
    uint8_t bit_port2 = 0x00;

    switch(key.sym){
        // port 0: alternative controls?
        case SDLK_i: // fire
            bit_port0 = 0x10; // bit 4
            break;
        case SDLK_j: // Left
            bit_port0 = 0x20; // bit 5
            break;
        case SDLK_l: // Right
            bit_port0 = 0x40; // bit 6
            break;
        // port 1: start game and player 1 controls
        case SDLK_RETURN: // Coin = ENTER
            bit_port1 = 0x01; // bit 0
            break;
        case SDLK_2: // 2 Player start
            bit_port1 = 0x02; // bit 1
            break;
        case SDLK_1: // 1 Player start
            bit_port1 = 0x04; // bit 2
            break;
        case SDLK_SPACE: // FIRE
        case SDLK_UP:
            bit_port1 = 0x10; // bit 4
            break;
        case SDLK_LEFT:
            bit_port1 = 0x20; // bit 5
            break;
        case SDLK_RIGHT:
            bit_port1 = 0x40; // bit 6
            break;
        // port 2: player 2 controls and (unimplemented) difficulty dip switches
        case SDLK_t: // TILT
            bit_port2 = 0x04; // bit 2
            break;
        case SDLK_w: // Player 2 fire
            bit_port2 = 0x10; // bit 4
            break;
        case SDLK_a: // Player 2 Left
            bit_port2 = 0x20; // bit 5
            break;
        case SDLK_d: // Player 2 Right
            bit_port2 = 0x40; // bit 6
            break;
    }

    if(key_pressed){
        this->out_port0 |=  bit_port0; // set the bit to 1
        this->out_port1 |=  bit_port1; // set the bit to 1
        this->out_port2 |=  bit_port2; // set the bit to 1
    } else {
        this->out_port0 &= ~bit_port0; // reset bit to 0
        this->out_port1 &= ~bit_port1; // reset bit to 0
        this->out_port2 &= ~bit_port2; // reset bit to 0
    }
}

void Machine::updateScreen(){
    uint16_t framebuffer_loc = 0x2400;
    uint16_t byte_width = 32;//WIDTH/8;
    uint16_t current_loc;

    int n = this->screen_width * this->screen_height;


    uint8_t* emumem = this->emu.memory.get();
    for(int y=0; y < this->screen_height; y++){
        for(int x = 0; x < byte_width; x++){
            current_loc = framebuffer_loc + byte_width*y + x;
            for(int b=0; b<8;b++){
                // the screen is the VRAM rotated 90 degrees counterclockwise
                int screen_location = n-(this->screen_height-y)-this->screen_height*(8*x+b);
                uint32_t color = ((emumem[current_loc]>>b) & 1)? 0xFFFFFF : 0x000000;
                this->textureBuffer[screen_location] = color;
            }
        }
    }

    SDL_Rect texture_rect;
    texture_rect.x = 0;
    texture_rect.y = 0;
    texture_rect.w = this->screen_height;
    texture_rect.h = this->screen_width;

    SDL_Rect window_rect;
    window_rect.x = 0;
    window_rect.y = 0;
    window_rect.w = this->window_width;
    window_rect.h = this->window_height;

    SDL_UpdateTexture(this->texture, NULL, textureBuffer.get(), this->screen_height*sizeof(uint32_t));
    SDL_RenderClear(this->renderer);
    SDL_RenderCopy(this->renderer, this->texture, &texture_rect, &window_rect);
    SDL_RenderPresent(this->renderer);
}

void Machine::interrupt(int num){
    if(!this->emu.interrupt_enabled) return;
    emu.call(0x08*num, 0); // Instruction: RST 1 -> Call 0x08
    emu.interrupt_enabled = false;
}

void Machine::run(){

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
                keyPress(this->event.key.keysym, true);  // true = key pressed
                break;
            case SDL_KEYUP:
                keyPress(this->event.key.keysym, false); // false = key depressed
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
        return;
    } else if(this->emu.memory[this->emu.pc] == 0xdb){ // IN instruction
        switch(this->emu.memory[this->emu.pc+1]){ // PORT NUMBER
            case 0:
                this->emu.a = this->out_port0;
                break;
            case 1: // Button presses here
                this->emu.a = this->out_port1;
                break;
            case 2: // settings and player 2 controls (Unimplemented)
                this->emu.a = this->out_port2;
                break;
            case 3:{
                uint16_t v = (this->shift1<<8) | this->shift0;
                this->emu.a = ((v >> (8-this->shift_amount))& 0xFF);
                }
                break;
        }
        return;
    }

    this->emu.execute_next_instruction();
}
