#include <iostream>
#include <stack>
#include <string>

using namespace std;

// using aliases for easy typing of registers
typedef unsigned char BYTE; // 8 bits long, used for data registers V0-VF
typedef unsigned short int WORD; // two bytes long, used for 16-bit register I and progam counter

class Chip8 {
    
    /*
    Hardware details:
    - Game memory of size 4096, each location stores 1 BYTE of data
    - 16 BYTE sized data registers V0-VF, represented by with a size 16 array
    - Address register I
    - Program counter;
    */

    public:
        // Functions
        void initialize();
        void loadGame(string);
        void runEmulationCycle(); 

    private:
        // Attributes
        BYTE gameMemory[4096];
        BYTE dataRegisters[16];
        WORD I;
        WORD programCounter;
        stack<WORD> gameStack;

        // Functions
        WORD fetch(WORD);
        int decode(WORD);
        void execute(int);

};

void Chip8::initialize() {
    
    // Clears memory and registers, sets pc to 0x200
    memset(gameMemory, 0, sizeof(gameMemory));
    memset(dataRegisters, 0, sizeof(dataRegisters));
    I = 0;
    programCounter = 0x200;

}

void Chip8::loadGame(string path) {

}

void Chip8::runEmulationCycle() {

    // running emulation cycle
    while (true) {
        
        // fetch, decode and execute cycle
        WORD opcode = this->fetch(this->programCounter);
        int functionNum = this->decode(opcode);
        this->execute(functionNum);

    }

}

WORD Chip8::fetch(WORD pc) {

    /*
    Fetches two successive bytes from the memory, since opcodes are 16 bit 
    (i.e. 2 bytes long). 
    1. Get data in memory where PC is pointing at, shift it left 8 places, call 
    it MSB
    2. Get data at PC+1, do bitwise or with MSB to get full opcode
    */
    
    WORD opcode = (gameMemory[this->programCounter] << 8) | gameMemory[this->programCounter];

    // increment program counter i.e. PC += 
    this->programCounter += 2;

}

int Chip8::decode(WORD opcode) {

}

void Chip8::execute(int num) {

}