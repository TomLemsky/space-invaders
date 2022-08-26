#include "Emulator.h"

//#define DEBUG

#ifdef DEBUG
#define DEBUG_PRINT printf
#else
#define DEBUG_PRINT(...)
#endif

// This class emulates the Intel 8080 CPU

Emulator::Emulator()
{
    //create memory and initialize with zero
    this->memory = make_unique<unsigned char[]>(this->RAM_size);
    for(unsigned int i=0; i < this->RAM_size; i++){
        this->memory[i] = 0;
    }
    this->pc = 0;
    this->flags.z = 0;
    this->flags.s = 0;
    this->flags.p = 0;
    this->flags.cy = 0;
    this->flags.ac = 0;
    this->interrupt_enabled = false;

}

Emulator::~Emulator()
{
    // nothing to delete. Smart pointer deletes memory automatically
}


void Emulator::load_program_from_file(string filename, uint16_t location)
{
    FILE * fp = fopen(filename.c_str(), "rb");
    if(fp==NULL){
        cout << "Please place ROM file in same directory as this executable: " << filename << endl;
        throw std::runtime_error("ROM File not found at location ./" + filename);
    }
    fseek(fp, 0L, SEEK_END);
    int filesize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    // copy file contents into memory
    fread(this->memory.get()+location, sizeof(char), filesize, fp);

    printf("Sucessfully read %d Bytes.\n", filesize);
    fclose(fp);
}

void Emulator::run(){
    while(1){
        execute_next_instruction();
    }
}

void Emulator::unimplemented_instruction(){
    printf("\n\nInstruction 0x%02x at location 0x%04x is unimplemented!\n\n", this->memory[this->pc], this->pc);
    // flush last printed messages to stdout before the exception stops the program
    fflush(stdout);
    throw std::runtime_error("Unimplemented instruction!");
}

void Emulator::set_flags_no_cy(uint16_t result){
    this->flags.z = (result & 0xff) == 0; // zero
    this->flags.s = (result & 0x80) != 0; // sign

    // calculate parity = even number of 1s
    uint8_t temp = result & 0xFF;
    temp ^= temp >> 4; // xor first 4 bits with last 4 bits
    temp ^= temp >> 2; // xor last 2 bits with previous 2 bits
    temp ^= temp >> 1; // xor last two bits together
    this->flags.p = (temp & 0x01) == 0;    // sum is even if last bit not set
}

void Emulator::set_flags(uint16_t result){
    set_flags_no_cy(result);
    this->flags.cy= result > 0xff; // carry
}

uint8_t Emulator::read_memory(uint16_t adress){
    return this->memory[adress];
}

// This overloaded methods combines two 1 bytes variables into a 2 byte adress and loads it from memory
uint8_t Emulator::read_memory(uint8_t adress_a, uint8_t adress_b){
    return read_memory((adress_a << 8) | adress_b);
}

void Emulator::write_memory(uint16_t adress, uint8_t data){
    // don't overwrite ROM (0000-1FFF) or out of memory
    adress = adress & 0x3FFF; // mirror adresses above 0x4000
    if(adress < 0x2000) return;
    this->memory[adress] = data;
}

// This overloaded methods combines two 1 bytes variables into a 2 byte adress and saves to that adress
void Emulator::write_memory(uint8_t adress_a, uint8_t adress_b, uint8_t data){
    write_memory((adress_a << 8) | adress_b, data);
}

void Emulator::call(uint16_t adress, uint8_t instruction_length){
    uint16_t temp = this->pc+instruction_length; // return address is the byte after the call instruction
    write_memory(this->sp-1, (temp >> 8) & 0xFF);// store high byte
    write_memory(this->sp-2, temp & 0xFF);       // store low byte
    this->sp -= 2;                               // decrement stack pointer by two bytes
    this->pc = adress;                           // set program counter to called adress
    instruction_length = 0;                      // don't increment the new adress
}

void Emulator::call(uint8_t adress1, uint8_t adress2, uint8_t instruction_length){
    // combine two 8bit adresses into 16bit and call
    call((adress1 << 8) | adress2, instruction_length);
}

void Emulator::ret(){
    // return to adress in stack pointer
    // get two byte return adress from stack
    this->pc = read_memory(this->sp) | (read_memory(this->sp+1) << 8);
    // set the stack pointer back.
    this->sp += 2;
}


void Emulator::arithmetic_instruction(){
    // this handles all instructions between 0x40 and 0xbf
    uint8_t opcode = read_memory(this->pc); //this->memory[this->pc];

    //special instruction in arithmetic block (replaces MOV M,M  [ M <- M]
    if (opcode == 0x76){ // HLT
        exit(0);
    }

    if ((opcode < 0x40) || (opcode > 0xbf)){
        throw std::runtime_error("arithmetic_instruction called on wrong instruction type");
    }
    // get the operand from lower three bits
    uint8_t operand;
    switch(opcode & 0x07){
        case 0x0:
            operand = this->b;
            break;
        case 0x1:
            operand = this->c;
            break;
        case 0x2:
            operand = this->d;
            break;
        case 0x3:
            operand = this->e;
            break;
        case 0x4:
            operand = this->h;
            break;
        case 0x5:
            operand = this->l;
            break;
        case 0x6:
            // m is a pseudo-register that is actually the memory location pointed to be registers H and L
            operand = read_memory(this->h,this->l);
            break;
        case 0x7:
            operand = this->a;
            break;
    }
    // the actual instruction from the higher bits
    // the value in the case is identical to the first instruction in the group

    uint16_t result = 0; // hold the result of addition, subtraction,...

    switch(opcode & 0xF8){
            case 0x40: // MOV B
                this->b = operand;
                break;
            case 0x48:
                this->c = operand;
                break;
            case 0x50:
                this->d = operand;
                break;
            case 0x58:
                this->e = operand;
                break;
            case 0x60:
                this->h = operand;
                break;
            case 0x68:
                this->l = operand;
                break;
            case 0x70:
                // m is a pseudo-register that is actually the memory location pointed to be registers H and L
                write_memory(this->h,this->l,operand);
                //this->memory[(this->h << 8) | this->l] = operand;
                break;
            case 0x78: // MOV A
                this->a = operand;
                break;
            case 0x80: // ADD
                result = (uint16_t) this->a + (uint16_t) operand;
                set_flags(result);
                this->a = result & 0xFF;
                break;
            case 0x88: // ADC
                result = (uint16_t) this->a + (uint16_t) operand + (uint16_t) this->flags.cy;
                set_flags(result);
                this->a = result & 0xFF;
                break;
            case 0x90: // SUB
                result = (uint16_t) this->a - (uint16_t) operand;
                set_flags(result);
                this->a = result & 0xFF;
                break;
            case 0x98: //SBB
                result = (uint16_t) this->a - (uint16_t) operand - (uint16_t) this->flags.cy;
                set_flags(result);
                this->a = result & 0xFF;
                break;
            case 0xA0: //ANA
                result = (uint16_t) this->a & (uint16_t) operand;
                set_flags(result);
                this->a = result & 0xFF;
                break;
            case 0xA8: //XRA
                result = (uint16_t) this->a ^ (uint16_t) operand;
                set_flags(result);
                this->a = result & 0xFF;
                break;
            case 0xB0: //ORA
                result = (uint16_t) this->a | (uint16_t) operand;
                set_flags(result);
                this->a = result & 0xFF;
                break;
            case 0xB8: //CMP
                result = (uint16_t) this->a - (uint16_t) operand;
                set_flags(result);
                // compare only sets the flags, doesn't change registers
                break;
    }

}

void Emulator::execute_next_instruction(){
    // temporary variables for briefness
    uint8_t* code = this->memory.get();
    uint16_t pc = this->pc;

    // temporary variable to calculate math results and flags
    uint32_t temp = 0;
    // how much to increment the program counter
    int instruction_length = 1;

    DEBUG_PRINT("%04x ", pc);
    DEBUG_PRINT("  Hex: %02x       ",this->memory[this->pc]);

    switch(code[pc]){
        case 0x00:
            DEBUG_PRINT("NOP");
            break;
        case 0x01:
            DEBUG_PRINT("LXI    B,#$%02x%02x", code[pc+2],code[pc+1]);
            this->b = code[pc+2];
            this->c = code[pc+1];
            instruction_length = 3;
            break;
        case 0x02:
            DEBUG_PRINT("STAX   B");
            write_memory(this->b, this->c, this->a); // Store value of accumulator A in adress (BC)
            break;
        case 0x03:
            DEBUG_PRINT("INX    B");
            // add 1 to 16bit adress in BC
            temp = ((this->b << 8) | this->c) + 1; // Combine BC and add one
            this->b = (temp>>8) & 0xFF;
            this->c = temp & 0xFF;
            break;
        case 0x04:
            DEBUG_PRINT("INR    B");
            temp = (uint16_t) this->b + 1;
            set_flags_no_cy(temp);
            this->b = temp & 0xFF;
            break;
        case 0x05:
            DEBUG_PRINT("DCR    B");
            temp = ((uint16_t) this->b) - 1;
            set_flags_no_cy(temp);
            this->b = temp & 0xFF;
            break;
        case 0x06:
            DEBUG_PRINT("MVI    B,#$%02x", code[pc+1]);
            this->b = code[pc+1];
            instruction_length = 2;
            break;
        case 0x07:
            DEBUG_PRINT("RLC    B"); // Rotate Accumulator left
            this->flags.cy = (this->a & 0x80)!=0;
            this->a = (this->a << 1) | this->flags.cy;
            break;
        case 0x08:
            DEBUG_PRINT("Undefined");
            unimplemented_instruction();
            break;
        case 0x09:
            DEBUG_PRINT("DAD    B");
            temp = ((this->h << 8) | this->l) + ((this->b << 8) | this->c);
            this->flags.cy = temp > 0xFFFF;
            this->h = (temp >> 8) & 0xFF;
            this->l = temp & 0xFF;
            break;
        case 0x0A:
            DEBUG_PRINT("LDAX   B");
            this->a = read_memory(this->b,this->c);
            break;
        case 0x0B:
            DEBUG_PRINT("DCX    B");
            // subtract 1 from 16bit adress in BC
            temp = ((this->b << 8) | this->c) - 1; // Combine BC and subtract one
            this->b = (temp>>8) & 0xFF;
            this->c = temp & 0xFF;
            break;
        case 0x0C:
            DEBUG_PRINT("INR    C");
            temp = (uint16_t) this->c + 1;
            set_flags_no_cy(temp);
            this->c = temp & 0xFF;
            break;
        case 0x0D:
            DEBUG_PRINT("DCR    C");
            temp = (uint16_t) this->c - 1;
            set_flags_no_cy(temp);
            this->c = temp & 0xFF;
            break;
        case 0x0E:
            DEBUG_PRINT("MVI    C,#$%02x", code[pc+1]);
            this->c = code[pc+1];
            instruction_length = 2;
            break;
        case 0x0F:
            DEBUG_PRINT("RRC"); // Rotate Accumulator right
            this->flags.cy = this->a & 0x01;
            this->a = ((this->flags.cy) << 7) | (this->a >> 1);
            break;
        case 0x10:
            DEBUG_PRINT("Undefined");
            unimplemented_instruction();
            break;
        case 0x11:
            DEBUG_PRINT("LXI    D,#$%02x%02x", code[pc+2],code[pc+1]);
            this->d = code[pc+2];
            this->e = code[pc+1];
            instruction_length = 3;
            break;
        case 0x12:
            DEBUG_PRINT("STAX   D");
            write_memory(this->d, this->e, this->a);
            break;
        case 0x13:
            DEBUG_PRINT("INX    D");
            // add 1 to 16bit adress in DE
            temp = ((this->d << 8) | this->e) + 1; // Combine DE and add one
            this->d = (temp>>8) & 0xFF;
            this->e = temp & 0xFF;
            break;
        case 0x14:
            DEBUG_PRINT("INR    D");
            temp = (uint16_t) this->d + 1;
            set_flags_no_cy(temp);
            this->d = temp & 0xFF;
            break;
        case 0x15:
            DEBUG_PRINT("DCR    D");
            temp = (uint16_t) this->d - 1;
            set_flags_no_cy(temp);
            this->d = temp & 0xFF;
            break;
        case 0x16:
            DEBUG_PRINT("MVI    D,#$%02x", code[pc+1]);
            this->d = code[pc+1];
            instruction_length = 2;
            break;
        case 0x17:
            DEBUG_PRINT("RAL");  // rotate a left through carry
            temp = this->a;
            this->a = (temp << 1) | this->flags.cy;
            this->flags.cy = (temp & 0x80) != 0; // set carry to highest bit
            break;
        case 0x18:
            DEBUG_PRINT("Undefined");
            unimplemented_instruction();
            break;
        case 0x19:
            DEBUG_PRINT("DAD    D");
            temp = ((this->h << 8) | this->l) + ((this->d << 8) | this->e);
            this->flags.cy = temp > 0xFFFF;
            this->h = (temp >> 8) & 0xFF;
            this->l = temp & 0xFF;
            break;
        case 0x1A:
            DEBUG_PRINT("LDAX   D");
            this->a = read_memory(this->d,this->e);
            break;
        case 0x1B:
            DEBUG_PRINT("DCX    D");
            // Subtract 1 from 16bit adress in DE
            temp = ((this->d << 8) | this->e) - 1; // Combine DE and subtract one
            this->d = (temp>>8) & 0xFF;
            this->e = temp & 0xFF;
            break;
        case 0x1C:
            DEBUG_PRINT("INR    E");
            temp = (uint16_t) this->e + 1;
            set_flags_no_cy(temp);
            this->e = temp & 0xFF;
            break;
        case 0x1D:
            DEBUG_PRINT("DCR    E");
            temp = (uint16_t) this->e - 1;
            set_flags_no_cy(temp);
            this->e = temp & 0xFF;
            break;
        case 0x1E:
            DEBUG_PRINT("MVI    E,#$%02x", code[pc+1]);
            this->e = code[pc+1];
            instruction_length = 2;
            break;
        case 0x1F:
            DEBUG_PRINT("RAR"); // rotate a right through carry
            temp = this->a;
            this->a = (this->flags.cy << 7) | (temp >> 1);
            this->flags.cy = temp & 0x01;
            break;
        case 0x20:
            DEBUG_PRINT("Undefined");
            unimplemented_instruction();
            break;
        case 0x21:
            DEBUG_PRINT("LXI    H,#$%02x%02x", code[pc+2],code[pc+1]);
            this->h = code[pc+2];
            this->l = code[pc+1];
            instruction_length = 3;
            break;
        case 0x22:
            DEBUG_PRINT("SHLD   $%02x%02x", code[pc+2],code[pc+1]);
            // store l d at adress
            temp = (code[pc+2] << 8) | code[pc+1];
            write_memory(temp+1, this->h);
            write_memory(temp  , this->l);
            instruction_length = 3;
            break;
        case 0x23:
            DEBUG_PRINT("INX    H");
            // add 1 to 16bit adress in HL
            temp = ((this->h << 8) | this->l) + 1; // Combine HL and add one
            this->h = (temp>>8) & 0xFF;
            this->l = temp & 0xFF;
            break;
        case 0x24:
            DEBUG_PRINT("INR    H");
            temp = (uint16_t) this->h + 1;
            set_flags_no_cy(temp);
            this->h = temp & 0xFF;
            break;
        case 0x25:
            DEBUG_PRINT("DCR    H");
            temp = (uint16_t) this->h - 1;
            set_flags_no_cy(temp);
            this->h = temp & 0xFF;
            break;
        case 0x26:
            DEBUG_PRINT("MVI    H,#$%02x", code[pc+1]);
            this->h = code[pc+1];
            instruction_length = 2;
            break;
        case 0x27:
            DEBUG_PRINT("DAA");
            if ((this->a & 0x0F) > 9){
                this->a += 6;
            }
            // (this->flags.cy==1) ||
            if ( ((this->a & 0xF0) > 0x90)){
                temp = (uint16_t) this->a + 0x60;
                set_flags(temp);
                this->a = temp & 0xFF;
            }
            break;
        case 0x28:
            DEBUG_PRINT("Undefined");
            unimplemented_instruction();
            break;
        case 0x29:
            DEBUG_PRINT("DAD    H");
            temp = ((this->h << 8) | this->l) + ((this->h << 8) | this->l);
            this->flags.cy = temp > 0xFFFF;
            this->h = (temp >> 8) & 0xFF;
            this->l = temp & 0xFF;
            break;
        case 0x2A:
            DEBUG_PRINT("LHLD   $%02x%02x", code[pc+2], code[pc+1]);
            temp = (code[pc+2] << 8) | code[pc+1];
            this->h = read_memory(temp+1);
            this->l = read_memory(temp);
            instruction_length = 3;
            break;
        case 0x2B:
            DEBUG_PRINT("DCX    H");
            temp = ((this->h << 8) | this->l) - 1; // Combine HL and subtract one
            this->h = (temp>>8) & 0xFF;
            this->l = temp & 0xFF;
            break;
        case 0x2C:
            DEBUG_PRINT("INR    L");
            temp = (uint16_t) this->l + 1;
            set_flags_no_cy(temp);
            this->l = temp & 0xFF;
            break;
        case 0x2D:
            DEBUG_PRINT("DCR    L");
            temp = (uint16_t) this->l - 1;
            set_flags_no_cy(temp);
            this->l = temp & 0xFF;
            break;
        case 0x2E:
            DEBUG_PRINT("MVI    L,#$%02x", code[pc+1]);
            this->l = code[pc+1];
            instruction_length = 2;
            break;
        case 0x2F:
            DEBUG_PRINT("CMA");
            this->a = ~(this->a); //bitwise not
            break;
        case 0x30:
            DEBUG_PRINT("Undefined");
            unimplemented_instruction();
            break;
        case 0x31:
            DEBUG_PRINT("LXI    SP,#$%02x%02x", code[pc+2],code[pc+1]);
            this->sp = (code[pc+2]<<8) | (code[pc+1]);
            instruction_length = 3;
            break;
        case 0x32:
            DEBUG_PRINT("STA    $%02x%02x", code[pc+2], code[pc+1]);
            write_memory(code[pc+2], code[pc+1], this->a);
            instruction_length = 3;
            break;
        case 0x33:
            DEBUG_PRINT("INX    SP");
            this->sp++; // increment stack pointer by one
            break;
        case 0x34:
            DEBUG_PRINT("INR    M");
            temp = (uint16_t) read_memory(this->h,this->l) + 1;
            set_flags_no_cy(temp);
            write_memory(this->h,this->l,temp & 0xFF);
            break;
        case 0x35:
            DEBUG_PRINT("DCR    M");
            temp = (uint16_t) read_memory(this->h,this->l) - 1;
            set_flags_no_cy(temp);
            write_memory(this->h,this->l,temp & 0xFF);
            break;
        case 0x36:
            DEBUG_PRINT("MVI    M,#$%02x", code[pc+1]);
            write_memory(this->h,this->l,code[pc+1]);
            instruction_length = 2;
            break;
        case 0x37:
            DEBUG_PRINT("STC");
            this->flags.cy = 1;
            break;
        case 0x38:
            DEBUG_PRINT("Undefined");
            unimplemented_instruction();
            break;
        case 0x39:
            DEBUG_PRINT("DAD    SP");
            // make the registers a single 16 bit number and add stack pionter
            temp = ((this->h << 8) | this->l) + this->sp;
            this->flags.cy = temp > 0xFFFF;
            this->h = (temp >> 8) & 0xFF;
            this->l = temp & 0xFF;
            break;
        case 0x3A:
            DEBUG_PRINT("LDA    $%02x%02x", code[pc+2],code[pc+1]);
            this->a = read_memory(code[pc+2],code[pc+1]);
            instruction_length = 3;
            break;
        case 0x3B:
            DEBUG_PRINT("DCX    SP");
            this->sp--;
            break;
        case 0x3C:
            DEBUG_PRINT("INR    A");
            temp = this->a + 1;
            set_flags_no_cy(temp);
            this->a = temp & 0xFF;
            break;
        case 0x3D:
            DEBUG_PRINT("DCR    A");
            temp = this->a - 1;
            set_flags_no_cy(temp);
            this->a = temp & 0xFF;
            break;
        case 0x3E:
            DEBUG_PRINT("MVI    A,#$%02x", code[pc+1]);
            this->a = code[pc+1];
            instruction_length = 2;
            break;
        case 0x3F:
            DEBUG_PRINT("CMC");
            this->flags.cy = ~this->flags.cy;
            break;
        case 0x40:
            DEBUG_PRINT("MOV    B,B");
            arithmetic_instruction();
            break;
        case 0x41:
            DEBUG_PRINT("MOV    B,C");
            arithmetic_instruction();
            break;
        case 0x42:
            DEBUG_PRINT("MOV    B,D");
            arithmetic_instruction();
            break;
        case 0x43:
            DEBUG_PRINT("MOV    B,E");
            arithmetic_instruction();
            break;
        case 0x44:
            DEBUG_PRINT("MOV    B,H");
            arithmetic_instruction();
            break;
        case 0x45:
            DEBUG_PRINT("MOV    B,L");
            arithmetic_instruction();
            break;
        case 0x46:
            DEBUG_PRINT("MOV    B,M");
            arithmetic_instruction();
            break;
        case 0x47:
            DEBUG_PRINT("MOV    B,A");
            arithmetic_instruction();
            break;
        case 0x48:
            DEBUG_PRINT("MOV    C,B");
            arithmetic_instruction();
            break;
        case 0x49:
            DEBUG_PRINT("MOV    C,C");
            arithmetic_instruction();
            break;
        case 0x4A:
            DEBUG_PRINT("MOV    C,D");
            arithmetic_instruction();
            break;
        case 0x4B:
            DEBUG_PRINT("MOV    C,E");
            arithmetic_instruction();
            break;
        case 0x4C:
            DEBUG_PRINT("MOV    C,H");
            arithmetic_instruction();
            break;
        case 0x4D:
            DEBUG_PRINT("MOV    C,L");
            arithmetic_instruction();
            break;
        case 0x4E:
            DEBUG_PRINT("MOV    C,M");
            arithmetic_instruction();
            break;
        case 0x4F:
            DEBUG_PRINT("MOV    C,A");
            arithmetic_instruction();
            break;
        case 0x50:
            DEBUG_PRINT("MOV    D,B");
            arithmetic_instruction();
            break;
        case 0x51:
            DEBUG_PRINT("MOV    D,C");
            arithmetic_instruction();
            break;
        case 0x52:
            DEBUG_PRINT("MOV    D,D");
            arithmetic_instruction();
            break;
        case 0x53:
            DEBUG_PRINT("MOV    D,E");
            arithmetic_instruction();
            break;
        case 0x54:
            DEBUG_PRINT("MOV    D,H");
            arithmetic_instruction();
            break;
        case 0x55:
            DEBUG_PRINT("MOV    D,L");
            arithmetic_instruction();
            break;
        case 0x56:
            DEBUG_PRINT("MOV    D,M");
            arithmetic_instruction();
            break;
        case 0x57:
            DEBUG_PRINT("MOV    D,A");
            arithmetic_instruction();
            break;
        case 0x58:
            DEBUG_PRINT("MOV    E,B");
            arithmetic_instruction();
            break;
        case 0x59:
            DEBUG_PRINT("MOV    E,C");
            arithmetic_instruction();
            break;
        case 0x5A:
            DEBUG_PRINT("MOV    E,D");
            arithmetic_instruction();
            break;
        case 0x5B:
            DEBUG_PRINT("MOV    E,E");
            arithmetic_instruction();
            break;
        case 0x5C:
            DEBUG_PRINT("MOV    E,H");
            arithmetic_instruction();
            break;
        case 0x5D:
            DEBUG_PRINT("MOV    E,L");
            arithmetic_instruction();
            break;
        case 0x5E:
            DEBUG_PRINT("MOV    E,M");
            arithmetic_instruction();
            break;
        case 0x5F:
            DEBUG_PRINT("MOV    E,A");
            arithmetic_instruction();
            break;
        case 0x60:
            DEBUG_PRINT("MOV    H,B");
            arithmetic_instruction();
            break;
        case 0x61:
            DEBUG_PRINT("MOV    H,C");
            arithmetic_instruction();
            break;
        case 0x62:
            DEBUG_PRINT("MOV    H,D");
            arithmetic_instruction();
            break;
        case 0x63:
            DEBUG_PRINT("MOV    H,E");
            arithmetic_instruction();
            break;
        case 0x64:
            DEBUG_PRINT("MOV    H,H");
            arithmetic_instruction();
            break;
        case 0x65:
            DEBUG_PRINT("MOV    H,L");
            arithmetic_instruction();
            break;
        case 0x66:
            DEBUG_PRINT("MOV    H,M");
            arithmetic_instruction();
            break;
        case 0x67:
            DEBUG_PRINT("MOV    H,A");
            arithmetic_instruction();
            break;
        case 0x68:
            DEBUG_PRINT("MOV    L,B");
            arithmetic_instruction();
            break;
        case 0x69:
            DEBUG_PRINT("MOV    L,C");
            arithmetic_instruction();
            break;
        case 0x6A:
            DEBUG_PRINT("MOV    L,D");
            arithmetic_instruction();
            break;
        case 0x6B:
            DEBUG_PRINT("MOV    L,E");
            arithmetic_instruction();
            break;
        case 0x6C:
            DEBUG_PRINT("MOV    L,H");
            arithmetic_instruction();
            break;
        case 0x6D:
            DEBUG_PRINT("MOV    L,L");
            arithmetic_instruction();
            break;
        case 0x6E:
            DEBUG_PRINT("MOV    L,M");
            arithmetic_instruction();
            break;
        case 0x6F:
            DEBUG_PRINT("MOV    L,A");
            arithmetic_instruction();
            break;
        case 0x70:
            DEBUG_PRINT("MOV    M,B");
            arithmetic_instruction();
            break;
        case 0x71:
            DEBUG_PRINT("MOV    M,C");
            arithmetic_instruction();
            break;
        case 0x72:
            DEBUG_PRINT("MOV    M,D");
            arithmetic_instruction();
            break;
        case 0x73:
            DEBUG_PRINT("MOV    M,E");
            arithmetic_instruction();
            break;
        case 0x74:
            DEBUG_PRINT("MOV    M,H");
            arithmetic_instruction();
            break;
        case 0x75:
            DEBUG_PRINT("MOV    M,L");
            arithmetic_instruction();
            break;
        case 0x76:
            DEBUG_PRINT("HLT");
            exit(0); // halt = end program
            break;
        case 0x77:
            DEBUG_PRINT("MOV    M,A");
            arithmetic_instruction();
            break;
        case 0x78:
            DEBUG_PRINT("MOV    A,B");
            arithmetic_instruction();
            break;
        case 0x79:
            DEBUG_PRINT("MOV    A,C");
            arithmetic_instruction();
            break;
        case 0x7A:
            DEBUG_PRINT("MOV    A,D");
            arithmetic_instruction();
            break;
        case 0x7B:
            DEBUG_PRINT("MOV    A,E");
            arithmetic_instruction();
            break;
        case 0x7C:
            DEBUG_PRINT("MOV    A,H");
            arithmetic_instruction();
            break;
        case 0x7D:
            DEBUG_PRINT("MOV    A,L");
            arithmetic_instruction();
            break;
        case 0x7E:
            DEBUG_PRINT("MOV    A,M");
            arithmetic_instruction();
            break;
        case 0x7F:
            DEBUG_PRINT("MOV    A,A");
            arithmetic_instruction();
            break;
        case 0x80:
            DEBUG_PRINT("ADD    B");
            arithmetic_instruction();
            break;
        case 0x81:
            DEBUG_PRINT("ADD    C");
            arithmetic_instruction();
            break;
        case 0x82:
            DEBUG_PRINT("ADD    D");
            arithmetic_instruction();
            break;
        case 0x83:
            DEBUG_PRINT("ADD    E");
            arithmetic_instruction();
            break;
        case 0x84:
            DEBUG_PRINT("ADD    H");
            arithmetic_instruction();
            break;
        case 0x85:
            DEBUG_PRINT("ADD    L");
            arithmetic_instruction();
            break;
        case 0x86:
            DEBUG_PRINT("ADD    M");
            arithmetic_instruction();
            break;
        case 0x87:
            DEBUG_PRINT("ADD    A");
            arithmetic_instruction();
            break;
        case 0x88:
            DEBUG_PRINT("ADC    B");
            arithmetic_instruction();
            break;
        case 0x89:
            DEBUG_PRINT("ADC    C");
            arithmetic_instruction();
            break;
        case 0x8A:
            DEBUG_PRINT("ADC    D");
            arithmetic_instruction();
            break;
        case 0x8B:
            DEBUG_PRINT("ADC    E");
            arithmetic_instruction();
            break;
        case 0x8C:
            DEBUG_PRINT("ADC    H");
            arithmetic_instruction();
            break;
        case 0x8D:
            DEBUG_PRINT("ADC    L");
            arithmetic_instruction();
            break;
        case 0x8E:
            DEBUG_PRINT("ADC    M");
            arithmetic_instruction();
            break;
        case 0x8F:
            DEBUG_PRINT("ADC    A");
            arithmetic_instruction();
            break;
        case 0x90:
            DEBUG_PRINT("SUB    B");
            arithmetic_instruction();
            break;
        case 0x91:
            DEBUG_PRINT("SUB    C");
            arithmetic_instruction();
            break;
        case 0x92:
            DEBUG_PRINT("SUB    D");
            arithmetic_instruction();
            break;
        case 0x93:
            DEBUG_PRINT("SUB    E");
            arithmetic_instruction();
            break;
        case 0x94:
            DEBUG_PRINT("SUB    H");
            arithmetic_instruction();
            break;
        case 0x95:
            DEBUG_PRINT("SUB    L");
            arithmetic_instruction();
            break;
        case 0x96:
            DEBUG_PRINT("SUB    M");
            arithmetic_instruction();
            break;
        case 0x97:
            DEBUG_PRINT("SUB    A");
            arithmetic_instruction();
            break;
        case 0x98:
            DEBUG_PRINT("SBB    B");
            arithmetic_instruction();
            break;
        case 0x99:
            DEBUG_PRINT("SBB    C");
            arithmetic_instruction();
            break;
        case 0x9A:
            DEBUG_PRINT("SBB    D");
            arithmetic_instruction();
            break;
        case 0x9B:
            DEBUG_PRINT("SBB    E");
            arithmetic_instruction();
            break;
        case 0x9C:
            DEBUG_PRINT("SBB    H");
            arithmetic_instruction();
            break;
        case 0x9D:
            DEBUG_PRINT("SBB    L");
            arithmetic_instruction();
            break;
        case 0x9E:
            DEBUG_PRINT("SBB    M");
            arithmetic_instruction();
            break;
        case 0x9F:
            DEBUG_PRINT("SBB    A");
            arithmetic_instruction();
            break;
        case 0xA0:
            DEBUG_PRINT("ANA    B");
            arithmetic_instruction();
            break;
        case 0xA1:
            DEBUG_PRINT("ANA    C");
            arithmetic_instruction();
            break;
        case 0xA2:
            DEBUG_PRINT("ANA    D");
            arithmetic_instruction();
            break;
        case 0xA3:
            DEBUG_PRINT("ANA    E");
            arithmetic_instruction();
            break;
        case 0xA4:
            DEBUG_PRINT("ANA    H");
            arithmetic_instruction();
            break;
        case 0xA5:
            DEBUG_PRINT("ANA    L");
            arithmetic_instruction();
            break;
        case 0xA6:
            DEBUG_PRINT("ANA    M");
            arithmetic_instruction();
            break;
        case 0xA7:
            DEBUG_PRINT("ANA    A");
            arithmetic_instruction();
            break;
        case 0xA8:
            DEBUG_PRINT("XRA    B");
            arithmetic_instruction();
            break;
        case 0xA9:
            DEBUG_PRINT("XRA    C");
            arithmetic_instruction();
            break;
        case 0xAA:
            DEBUG_PRINT("XRA    D");
            arithmetic_instruction();
            break;
        case 0xAB:
            DEBUG_PRINT("XRA    E");
            arithmetic_instruction();
            break;
        case 0xAC:
            DEBUG_PRINT("XRA    H");
            arithmetic_instruction();
            break;
        case 0xAD:
            DEBUG_PRINT("XRA    L");
            arithmetic_instruction();
            break;
        case 0xAE:
            DEBUG_PRINT("XRA    M");
            arithmetic_instruction();
            break;
        case 0xAF:
            DEBUG_PRINT("XRA    A");
            arithmetic_instruction();
            break;
        case 0xB0:
            DEBUG_PRINT("ORA    B");
            arithmetic_instruction();
            break;
        case 0xB1:
            DEBUG_PRINT("ORA    C");
            arithmetic_instruction();
            break;
        case 0xB2:
            DEBUG_PRINT("ORA    D");
            arithmetic_instruction();
            break;
        case 0xB3:
            DEBUG_PRINT("ORA    E");
            arithmetic_instruction();
            break;
        case 0xB4:
            DEBUG_PRINT("ORA    H");
            arithmetic_instruction();
            break;
        case 0xB5:
            DEBUG_PRINT("ORA    L");
            arithmetic_instruction();
            break;
        case 0xB6:
            DEBUG_PRINT("ORA    M");
            arithmetic_instruction();
            break;
        case 0xB7:
            DEBUG_PRINT("ORA    A");
            arithmetic_instruction();
            break;
        case 0xB8:
            DEBUG_PRINT("CMP    B");
            arithmetic_instruction();
            break;
        case 0xB9:
            DEBUG_PRINT("CMP    C");
            arithmetic_instruction();
            break;
        case 0xBA:
            DEBUG_PRINT("CMP    D");
            arithmetic_instruction();
            break;
        case 0xBB:
            DEBUG_PRINT("CMP    E");
            arithmetic_instruction();
            break;
        case 0xBC:
            DEBUG_PRINT("CMP    H");
            arithmetic_instruction();
            break;
        case 0xBD:
            DEBUG_PRINT("CMP    L");
            arithmetic_instruction();
            break;
        case 0xBE:
            DEBUG_PRINT("CMP    M");
            arithmetic_instruction();
            break;
        case 0xBF:
            DEBUG_PRINT("CMP    A");
            arithmetic_instruction();
            break;
        case 0xC0:
            DEBUG_PRINT("RNZ     ");
            if(this->flags.z == 0){
                ret(); //return
                instruction_length = 0; // Don't increment pc after return (CALL aready set this to next instruction)
            } else {
                instruction_length = 1;
            }
            break;
        case 0xC1:
            DEBUG_PRINT("POP    B");
            this->b = read_memory(this->sp + 1);
            this->c = read_memory(this->sp);
            this->sp += 2;
            break;
        case 0xC2:
            DEBUG_PRINT("JNZ    $%02x%02x", code[pc+2],code[pc+1]);
            if(this->flags.z == 0){ // zero flag is 0 (not set), so jump
                this->pc = (code[pc+2] << 8) | code[pc+1];
                instruction_length = 0; // Keep the program counter at the pointed adress
            } else {
                instruction_length = 3;
            }
            break;
        case 0xC3:
            DEBUG_PRINT("JMP    $%02x%02x", code[pc+2],code[pc+1]);
            this->pc = (code[pc+2] << 8) | code[pc+1];
            instruction_length = 0; // Keep the program counter at the pointed adress
            break;
        case 0xC4:
            DEBUG_PRINT("CNZ    $%02x%02x", code[pc+2],code[pc+1]);
            // call if not zero
            if(this->flags.z == 0){
                call(code[pc+2], code[pc+1], 3);
                instruction_length = 0;
            } else {
                instruction_length = 3;
            }
            break;
        case 0xC5:
            DEBUG_PRINT("PUSH   B");
            write_memory(this->sp-1, b);
            write_memory(this->sp-2, c);
            this->sp -= 2;
            break;
        case 0xC6:
            DEBUG_PRINT("ADI    #$%02x", code[pc+1]);
            temp = this->a + code[pc+1];
            set_flags(temp);
            this->a = temp & 0xFF;
            instruction_length = 2;
            break;
        case 0xC7:
            DEBUG_PRINT("RST 0");
            call(0x00, 1);          // call $00, return adress is next byte
            instruction_length = 0; // don't increment the new adress
            break;
        case 0xC8:
            DEBUG_PRINT("RZ");
            // return if zero
            if(this->flags.z){
                ret(); //return
                instruction_length = 0; // Don't increment pc after return (CALL aready set this to next instruction)
            } else {
                instruction_length = 1;
            }
            break;
        case 0xC9:
            DEBUG_PRINT("RET");
            ret();
            instruction_length = 0; // Don't increment pc after return (CALL aready set this to next instruction)
            break;
        case 0xCA:
            DEBUG_PRINT("JZ     $%02x%02x", code[pc+2],code[pc+1]);
            // jump if zero
            if(this->flags.z){
                this->pc = (code[pc+2] << 8) | code[pc+1];
                instruction_length = 0; // don't increment pc beyond the jump adress
            } else {
                instruction_length = 3;
            }
            break;
        case 0xCB:
            DEBUG_PRINT("Undefined");
            unimplemented_instruction();
            break;
        case 0xCC:
            DEBUG_PRINT("CZ     $%02x%02x", code[pc+2],code[pc+1]);
            // call if zero flag
            if(this->flags.z == 1){
                call(code[pc+2], code[pc+1], 3);
                instruction_length = 0;
            } else {
                instruction_length = 3;
            }
            break;
        case 0xCD:
            DEBUG_PRINT("CALL   $%02x%02x", code[pc+2], code[pc+1]);
            call(code[pc+2], code[pc+1], 3); // call $38, return adress is the third byte after this
            instruction_length = 0;                      // don't increment the new adress
            break;
        case 0xCE:
            DEBUG_PRINT("ACI    #$%02x", code[pc+1]);
            temp = this->a + code[pc+1] + this->flags.cy;
            set_flags(temp);
            this->a = temp & 0xFF;
            instruction_length = 2;
            break;
        case 0xCF:
            DEBUG_PRINT("RST 1");
            call(0x08, 1);          // call $38, return adress is next byte
            instruction_length = 0; // don't increment the new adress
            break;
        case 0xD0:
            DEBUG_PRINT("RNC");
            // return if no carry (carry bit is zero)
            if(this->flags.cy == 0){
                ret(); //return
                instruction_length = 0; // Don't increment pc after return (CALL aready set this to next instruction)
            } else {
                instruction_length = 1;
            }
            break;
        case 0xD1:
            DEBUG_PRINT("POP    D");
            this->d = read_memory(this->sp + 1);
            this->e = read_memory(this->sp);
            this->sp += 2;
            break;
        case 0xD2:
            DEBUG_PRINT("JNC    $%02x%02x", code[pc+2],code[pc+1]);
            // jump if not carry
            if(this->flags.cy == 0){
                this->pc = (code[pc+2] << 8) | code[pc+1];
                instruction_length = 0; // don't increment pc beyond the jump adress
            } else {
                instruction_length = 3;
            }
            break;
        case 0xD3:
            DEBUG_PRINT("OUT    #$%02x", code[pc+1]);
            instruction_length = 2;
            break;
        case 0xD4:
            DEBUG_PRINT("CNC    $%02x%02x", code[pc+2],code[pc+1]);
            // call if not carry
            if(this->flags.cy == 0){
                call(code[pc+2], code[pc+1], 3);
                instruction_length = 0;
            } else {
                instruction_length = 3;
            }
            break;
        case 0xD5:
            DEBUG_PRINT("PUSH   D");
            write_memory(this->sp-1, d);
            write_memory(this->sp-2, e);
            this->sp -= 2;
            break;
        case 0xD6:
            DEBUG_PRINT("SUI    #$%02x", code[pc+1]);
            temp = this->a - code[pc+1];
            set_flags(temp);
            this->a = temp & 0xFF;
            instruction_length = 2;
            break;
        case 0xD7:
            DEBUG_PRINT("RST 2");
            call(0x10, 1);          // call $10, return adress is next byte
            instruction_length = 0; // don't increment the new adress
            break;
        case 0xD8:
            DEBUG_PRINT("RC");
            if(this->flags.cy){
                ret(); //return
                instruction_length = 0; // Don't increment pc after return (CALL aready set this to next instruction)
            } else {
                instruction_length = 1;
            }
            break;
        case 0xD9:
            DEBUG_PRINT("Undefined");
            unimplemented_instruction();
            break;
        case 0xDA:
            DEBUG_PRINT("JC    $%02x%02x", code[pc+2],code[pc+1]);
            // jump if carry
            if(this->flags.cy){
                this->pc = (code[pc+2] << 8) | code[pc+1];
                instruction_length = 0; // don't increment pc beyond the jump adress
            } else {
                instruction_length = 3;
            }
            break;
        case 0xDB:
            DEBUG_PRINT("IN    #$%02x", code[pc+1]);
            instruction_length = 2;
            break;
        case 0xDC:
            DEBUG_PRINT("CC    $%02x%02x", code[pc+2],code[pc+1]);
            // call if carry
            if(this->flags.cy){
                call(code[pc+2], code[pc+1], 3);
                instruction_length = 0;
            } else {
                instruction_length = 3;
            }
            break;
        case 0xDD:
            DEBUG_PRINT("Undefined");
            unimplemented_instruction();
            break;
        case 0xDE:
            DEBUG_PRINT("SBI   #$%02x", code[pc+1]);
            temp = this->a - code[pc+1] - this->flags.cy;
            set_flags(temp);
            this->a = temp & 0xFF;
            instruction_length = 2;
            break;
        case 0xDF:
            DEBUG_PRINT("RST 3");
            call(0x18, 1);          // call $18, return adress is next byte
            instruction_length = 0; // don't increment the new adress
            break;
        case 0xE0:
            DEBUG_PRINT("RPO");
            // return if parity odd (sign bit is zero)
            if(this->flags.s == 0){
                ret(); //return
                instruction_length = 0; // Don't increment pc after return (CALL aready set this to next instruction)
            } else {
                instruction_length = 1;
            }
            break;
        case 0xE1:
            DEBUG_PRINT("POP H");
            this->h = read_memory(this->sp + 1);
            this->l = read_memory(this->sp);
            this->sp += 2;
            break;
        case 0xE2:
            DEBUG_PRINT("JPO   $%02x%02x", code[pc+2],code[pc+1]);
            // jump if parity odd
            if(this->flags.p == 0){
                this->pc = (code[pc+2] << 8) | code[pc+1];
                instruction_length = 0; // don't increment pc beyond the jump adress
            } else {
                instruction_length = 3;
            }
            break;
        case 0xE3:
            DEBUG_PRINT("XTHL");
            // L <-> (SP);
            temp = this->l;
            this->l = read_memory(this->sp);
            write_memory(this->sp, temp & 0xFF);
            // H <-> (SP+1)
            temp = this->h;
            this->h = read_memory(this->sp + 1);
            write_memory(this->sp + 1, temp & 0xFF);
            break;
        case 0xE4:
            DEBUG_PRINT("CPO   $%02x%02x", code[pc+2],code[pc+1]);
            // call if parity odd
            if(this->flags.p == 0){
                call(code[pc+2], code[pc+1], 3);
                instruction_length = 0;
            } else {
                instruction_length = 3;
            }
            break;
        case 0xE5:
            DEBUG_PRINT("PUSH   H");
            write_memory(this->sp-1, h);
            write_memory(this->sp-2, l);
            this->sp -= 2;
            break;
        case 0xE6:
            DEBUG_PRINT("ANI   #$%02x", code[pc+1]);
            this->a = this->a & code[pc+1];
            set_flags(this->a); // this should also reset carry since temp <= 0xFF
            instruction_length = 2;
            break;
        case 0xE7:
            DEBUG_PRINT("RST 4");
            call(0x20, 1);          // call $38, return adress is next byte
            instruction_length = 0; // don't increment the new adress
            break;
        case 0xE8:
            DEBUG_PRINT("RPE");
            if(this->flags.p){
                ret(); //return
                instruction_length = 0; // Don't increment pc after return (CALL aready set this to next instruction)
            } else {
                instruction_length = 1;
            }
            break;
        case 0xE9:
            DEBUG_PRINT("PCHL");
            this->pc = (this->h << 8) | this->l;
            instruction_length = 0;
            break;
        case 0xEA:
            DEBUG_PRINT("JPE   $%02x%02x", code[pc+2],code[pc+1]);
            // jump if parity even
            if(this->flags.p){
                this->pc = (code[pc+2] << 8) | code[pc+1];
                instruction_length = 0; // don't increment pc beyond the jump adress
            } else {
                instruction_length = 3;
            }
            break;
        case 0xEB:
            DEBUG_PRINT("XCHG");
            //exchange H <-> D
            temp = this->h;
            this->h = this->d;
            this->d = temp & 0xFF;
            //exchange L <-> E
            temp = this-> l;
            this->l = this->e;
            this->e = temp & 0xFF;
            break;
        case 0xEC:
            DEBUG_PRINT("CPE   $%02x%02x", code[pc+2],code[pc+1]);
            // call if parity even
            if(this->flags.p){
                call(code[pc+2], code[pc+1], 3);
                instruction_length = 0;
            } else {
                instruction_length = 3;
            }
            break;
        case 0xED:
            DEBUG_PRINT("Undefined");
            unimplemented_instruction();
            break;
        case 0xEE:
            DEBUG_PRINT("XRI   #$%02x", code[pc+1]);
            this->a = this->a ^ code[pc+1];
            set_flags(this->a); // this should also reset carry since temp <= 0xFF
            instruction_length = 2;
            break;
        case 0xEF:
            DEBUG_PRINT("RST 5");
            call(0x28, 1);          // call $38, return adress is next byte
            instruction_length = 0; // don't increment the new adress
            break;
        case 0xF0:
            DEBUG_PRINT("RP");
            // return if plus
            if(this->flags.s == 0){
                ret(); //return
                instruction_length = 0; // Don't increment pc after return (CALL aready set this to next instruction)
            } else {
                instruction_length = 1;
            }
            break;
        case 0xF1:
            DEBUG_PRINT("POP PSW"); // load flags and accumulator from stack
            this->a = read_memory(this->sp+1);
            // Load flags from stack: sz0a0pc
            temp = read_memory(this->sp);

            this->flags.s  = (temp & 0x80) !=0;
            this->flags.z  = (temp & 0x40) !=0;
            this->flags.ac = (temp & 0x10) !=0;
            this->flags.p  = (temp & 0x04) !=0;
            this->flags.cy = (temp & 0x01) !=0;
            this->sp += 2;
            break;
        case 0xF2:
            DEBUG_PRINT("JP    $%02x%02x", code[pc+2],code[pc+1]);
            // jump if plus
            if(this->flags.s == 0){
                this->pc = (code[pc+2] << 8) | code[pc+1];
                instruction_length = 0; // don't increment pc beyond the jump adress
            } else {
                instruction_length = 3;
            }
            break;
        case 0xF3:
            DEBUG_PRINT("DI");
            this->interrupt_enabled = false;
            break;
        case 0xF4:
            DEBUG_PRINT("CP    $%02x%02x", code[pc+2],code[pc+1]);
            // call if plus
            if(this->flags.s == 0){
                call(code[pc+2], code[pc+1], 3);
                instruction_length = 0;
            } else {
                instruction_length = 3;
            }
            break;
        case 0xF5:
            DEBUG_PRINT("PUSH PSW");// (sp-2)<-flags; (sp-1)<-A; sp <- sp - 2
            // The byte looks like this: sz0a0p1c

            temp = (this->flags.s  << 7)
                 | (this->flags.z  << 6)
                 // bit 5 always zero
                 | (this->flags.ac << 4)
                 // bit 3 always zero
                 | (this->flags.p  << 2)
                 | (1              << 1) // bit one always once
                 | (this->flags.cy);
            write_memory(this->sp-1, this->a);
            write_memory(this->sp-2, temp & 0xFF);
            this->sp -= 2;
            break;
        case 0xF6:
            DEBUG_PRINT("ORI   #$%02x", code[pc+1]);
            this->a = this->a | code[pc+1];
            set_flags(this->a); // this should also reset carry since temp <= 0xFF
            instruction_length = 2;
            break;
        case 0xF7:
            DEBUG_PRINT("RST 6");
            call(0x30, 1);          // call $38, return adress is next byte
            instruction_length = 0; // don't increment the new adress
            break;
        case 0xF8:
            DEBUG_PRINT("RM");
            // Return if minus (sign flag)
            if(this->flags.s){
                ret(); //return
                instruction_length = 0; // Don't increment pc after return (CALL aready set this to next instruction)
            } else {
                instruction_length = 1;
            }
            break;
        case 0xF9:
            DEBUG_PRINT("SPHL");
            this->sp = (this->h << 8) | this->l;
            break;
        case 0xFA:
            DEBUG_PRINT("JM    $%02x%02x", code[pc+2],code[pc+1]);
            // Jump if minus (sign flag)
            if(this->flags.s){
                this->pc = (code[pc+2] << 8) | code[pc+1];
                instruction_length = 0;
            } else {
                instruction_length = 3;
            }
            break;
        case 0xFB:
            DEBUG_PRINT("EI");
            this->interrupt_enabled = true;
            break;
        case 0xFC:
            DEBUG_PRINT("CM    $%02x%02x", code[pc+2],code[pc+1]);
            // Call if minus (sign flag)
            if(this->flags.s){
                call(code[pc+2],code[pc+1],3);
                instruction_length = 0;
            } else {
                instruction_length = 3;
            }
            break;
        case 0xFD:
            DEBUG_PRINT("Undefined");
            unimplemented_instruction();
            break;
        case 0xFE:
            DEBUG_PRINT("CPI    #$%02x", code[pc+1]);
            temp = (uint16_t) this->a - (uint16_t) code[pc+1];
            set_flags(temp);
            instruction_length = 2;
            break;
        case 0xFF:
            DEBUG_PRINT("RST 7");
            call(0x38, 1); // call $38, return adress is next byte
            instruction_length = 0; // don't increment the new adress
            break;

        default:
            unimplemented_instruction();

    }
    //DEBUG_PRINT("%02X",read_memory(this->h,this->l));
    DEBUG_PRINT("\n%c%c%c%c%c", (this->flags.z == 0) ? ('.') : ('z'), (this->flags.s == 0) ? ('.') : ('s'), (this->flags.p == 0) ? ('.') : ('p'), (this->flags.cy == 0) ? ('.') : ('c'), (this->flags.ac == 0) ? ('.') : ('a'));
    DEBUG_PRINT(" REG: A %02X B %02X C %02X D %02X E %02X H %02X L %02X SP %02X", this->a,this->b,this->c,this->d,this->e,this->h,this->l,this->sp);
    DEBUG_PRINT("\n");

    this->pc += instruction_length;
}
