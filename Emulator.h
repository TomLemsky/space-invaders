#ifndef EMULATOR_H
#define EMULATOR_H

#include <string>
#include <memory>
#include <iostream>
#include <stdexcept>

using namespace std;

// flags as a bit field

typedef struct flags_st {
    uint8_t z:1; //result is zero
    uint8_t s:1; //most significant bit of result
    uint8_t p:1; //parity of result
    uint8_t cy:1;// carry
    uint8_t ac:1;// aux carry
    uint8_t pad:3; // padding to a byte?
} flags_st;


class Emulator
{
    public:
        //unique_ptr<unsigned char[]> machinecode;
        //unsigned int filesize;
        //unsigned int program_counter;

        Emulator();
        virtual ~Emulator();

        void load_program_from_file(string filename);
        void run();
        void execute_next_instruction();
        void call(uint16_t adress, uint8_t instruction_length);
        void call(uint8_t adress1, uint8_t adress2, uint8_t instruction_length);

        unsigned int RAM_size = 0x4000;

        //Registers
        uint8_t a = 0;
        uint8_t b = 0;
        uint8_t c = 0;
        uint8_t d = 0;
        uint8_t e = 0;
        uint8_t h = 0;
        uint8_t l = 0;
        uint16_t sp = 0; // stack pointer
        uint16_t pc = 0; // program counter
        unique_ptr<uint8_t[]> memory; // pointer to RAM
        struct flags_st flags;
        bool interrupt_enabled; // is interrupt enabled?


    private:
        // internal function to implement opcodes
        void unimplemented_instruction();
        void set_flags_no_cy(uint16_t result);
        void set_flags(uint16_t result);
        void arithmetic_instruction();
        uint8_t read_memory(uint16_t adress);
        uint8_t read_memory(uint8_t adress_a, uint8_t adress_b);
        void write_memory(uint16_t adress, uint8_t data);
        void write_memory(uint8_t adress_a, uint8_t adress_b, uint8_t data);
        void ret();
};

#endif // EMULATOR_H
