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
        void execute(WORD);
        void opcode1NNN(WORD opcode);

        // Opcode functions

        // Opcode tables
        typedef void (*functionPtr)(WORD);

        functionPtr opcodeRootTable[18] =
        {

        };

        functionPtr opcode8Table[8] =
        {

        };
        
        functionPtr opcodeFTable[9] =
        {

        };

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
        this->execute(opcode);

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

void Chip8::execute(WORD opcode) {

    // Use an array of function pointers (fTable) to call functions directly
    // 18 elements in top level function table (fTable)
    // Opcodes starting with 8 has 8 distinct functions
    // Opcodes starting with F has 9 distinct functions

    // Get the first 4 bits to call the appropriate function
    this->opcodeRootTable[(opcode & 0xF000) >> 12](opcode);

    // Use an array of function pointers to call functions directly
    
    
}

//Jumps to address NNN
void Chip8::opcode1NNN(WORD opcode) {
    programCounter = opcode & 0x0FFF;
}

//Calls subroutine at NNN
void Chip8::opcode2NNN(WORD opcode) {
    gameStack.push_back(programCounter);
    programCounter = opcode & 0x0FFF;
}

//Skips the next instruction if VX equals NN
void Chip8::opcode3XNN(WORD opcode) {
    int NN = opcode & 0x00FF;
    int X = (opcode & 0x0F00) >> 8;

    if (dataRegisters[X] == NN) {
        programCounter += 2;
    }
}

//Skips the next instruction if VX doesn't equal NN
void Chip8::opcode4XNN(WORD opcode) {
    int NN = opcode & 0x0FF;
    int X = (opcode & 0x0F00) >> 8;

    if (dataRegisters[X] != NN) {
        programCounter += 2;
    }
}

//Skips the next instruction if VX equals VY
void Chip8::opcode5XY0(WORD opcode) {
    int X = (opcode & 0x0F00) >> 8;
    int Y = (opcode & 0x00F0) >> 4;

    if (dataRegisters[X] == dataRegisters[Y]) {
        programCounter += 2;
    }
}

//Sets VX to NN
void Chip8::opcode6XNN(WORD opcode) {
    int NN = opcode & 0x00FF;
    int X = (opcode & 0x0F00) >> 8;

    dataRegisters[X] = NN;
}

//Adds NN to VX
void Chip8::opcode7XNN(WORD opcode) {
    int NN = opcode * 0x00FF;
    int X = (opcode & 0x0F00) >> 8;

    dataRegisters[X] += NN;
}