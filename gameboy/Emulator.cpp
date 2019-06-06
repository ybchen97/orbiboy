/*

1. CPU : 
- main CPU functions such as initialize, load, fetch, execute
- all opcode functions 
- cpu attributes like pc, registers, stack etc.

http://www.codeslinger.co.uk/pages/projects/gameboy/hardware.html

REGISTERS

8 bit registers 
A is special in that it is the only register where addition is performed
F is flag register
Can be combined to behave like a 16 bit register AF (less common) BC DE HL
    Union declaration with lo and hi BYTE components
typedef unsigned char BYTE 
*How to implement?* Array like Chip8?
A F
B C
D E
H L

16 bit registers
typedef unsigned short int WORD 
SP PC

Program Counter is intialised to 0x100
PC is WORD
Stack Pointer is intialised to 0xFFFE
SP is Register (union struct from 8 bit register pairing)

8 bit F flag register 
Bits 7 6 5 4 3 2 1 0
7 - Zero Flag
6 - Subtract flag
5 - Half Carry flag (https://en.wikipedia.org/wiki/Half-carry_flag)
4 - Carry flag (indicates if result overflowed 255)
3-0 - not used

INSTRUCTION SET
https://gekkio.fi/files/gb-docs/gbctr.pdf



*/

#include <string>

//For the flag bits in register F
#define FLAG_ZERO 7;
#define FLAG_SUB 6;
#define FLAG_HALFCARRY 5;
#define FLAG_CARRY 4;

using namespace std;

typedef unsigned char BYTE;
typedef char SIGNED_BYTE;
typedef unsigned short WORD; 
typedef signed short SIGNED_WORD;

union Register {
    WORD regstr;
    struct {
        BYTE low;
        BYTE high;
    };
};


class Emulator {

    public:
        // Attributes

        // Functions
        void initialize();
        bool loadGame(string);
        void writeMem(WORD, BYTE);
        BYTE readMem(WORD) const;

    private:
        //Attributes

        //8 bit registers, which are paired to behave like a 16 bit register
        //To accesss the first register, RegXX.high
        //To access the second register, RegXX.low
        //To access both, RegXX.regstr
        Register regAF;
        Register regBC;
        Register regDE;
        Register regHL;

        //16 bit registers
        Register programCounter;
        Register stackPointer;

        BYTE internalMem[0x10000]; // internal memory from 0x0000 - 0xFFFF
        BYTE catridgeMem[0x200000]; // Catridge memory up to 2MB (2 * 2^20)


};

void Emulator::initialize() {

    regAF.regstr = 0x01B0; 
    regBC.regstr = 0x0013; 
    regDE.regstr = 0x00D8;
    regHL.regstr = 0x014D;
    stackPointer.regstr = 0xFFFE;
    programCounter.regstr = 0x100; 

    gbMem[0xFF05] = 0x00; 
    gbMem[0xFF06] = 0x00;
    gbMem[0xFF07] = 0x00; 
    gbMem[0xFF10] = 0x80; 
    gbMem[0xFF11] = 0xBF; 
    gbMem[0xFF12] = 0xF3; 
    gbMem[0xFF14] = 0xBF; 
    gbMem[0xFF16] = 0x3F; 
    gbMem[0xFF17] = 0x00; 
    gbMem[0xFF19] = 0xBF; 
    gbMem[0xFF1A] = 0x7F; 
    gbMem[0xFF1B] = 0xFF; 
    gbMem[0xFF1C] = 0x9F; 
    gbMem[0xFF1E] = 0xBF; 
    gbMem[0xFF20] = 0xFF; 
    gbMem[0xFF21] = 0x00; 
    gbMem[0xFF22] = 0x00; 
    gbMem[0xFF23] = 0xBF; 
    gbMem[0xFF24] = 0x77; 
    gbMem[0xFF25] = 0xF3;
    gbMem[0xFF26] = 0xF1; 
    gbMem[0xFF40] = 0x91; 
    gbMem[0xFF42] = 0x00; 
    gbMem[0xFF43] = 0x00; 
    gbMem[0xFF45] = 0x00; 
    gbMem[0xFF47] = 0xFC; 
    gbMem[0xFF48] = 0xFF; 
    gbMem[0xFF49] = 0xFF; 
    gbMem[0xFF4A] = 0x00; 
    gbMem[0xFF4B] = 0x00; 
    gbMem[0xFFFF] = 0x00;

}

bool Emulator::loadGame(string file_path) {

    memset(cartridgeMem, 0, sizeof(cartridgeMem));
    
    //load in the game
    FILE* in;
    in = fopen(file_path.c_str(), "rb") ;

    // check rom exists
    if (in == NULL) {
        printf("Cannot load game file ERROR!!!");
        return false;
    }

    // Get file size
    fseek(in, 0, SEEK_END);
    int fileSize = ftell(in);
    rewind(in);
    
    // Read file into gameMemory
    fread(&cartridgeMem, fileSize, 1, in);
    fclose(in);

    return true;
}

void Emulator::update() {

    const int maxCycles = 000000000;
    int cyclesCount = 0;

    while (cyclesCount < maxCycles) {
        int cycles = this->executeNextOpcode(); //executeNextOpcode will return the number of cycles taken
        cyclesCount += cycles;            //wishful thinking now

        this->updateTimers(cycles); //wishful thinking
        this->updateGraphics(cycles); //wishful thinking
        this->handleInterrupts(); //wishful thinking
    }
    this->RenderScreen(); //wishful thinking
}

void Emulator::writeMem(WORD address, BYTE data) {
    if (address < 0x8000) {}

    // writing to Echo RAM also writes to work RAM
    else if ((address >= 0xE000) && (address <= 0xFDFF)) {
        this->internalMem[address] = data;
        writeMem(address - 0x2000, data);
    }

    else if ((address >= 0xFEA0) && (address <= 0xFEFF)) {}

    else {
        this->internalMem[address] = data;
    }

}

BYTE Emulator::readMem(WORD address) const {

}