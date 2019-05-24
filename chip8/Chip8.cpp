#include <iostream>
#include <stack>
#include <string>
#include <cstdlib>
#include <ctime>

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
        BYTE delayTimer;
        BYTE soundTimer;

        // Functions
        WORD fetch(WORD);
        void execute(WORD);

        // Opcode functions
        void callOpcode8(WORD);
        void callOpcodeF(WORD);
        void callOpcodeFX_5(WORD);

        // opcode for 0
        void opcode1NNN(WORD);
        void opcode2NNN(WORD);
        void opcode3XNN(WORD);
        void opcode4XNN(WORD);
        void opcode5XY0(WORD);
        void opcode6XNN(WORD);
        void opcode7XNN(WORD);
        void opcode8XY0(WORD);
        void opcode8XY1(WORD);
        void opcode8XY2(WORD);
        void opcode8XY3(WORD);
        void opcode8XY4(WORD);
        void opcode8XY5(WORD);
        void opcode8XY6(WORD);
        void opcode8XY7(WORD);
        void opcode8XYE(WORD);
        void opcode9XY0(WORD);
        void opcodeANNN(WORD);
        void opcodeBNNN(WORD);
        void opcodeCXNN(WORD);
        void opcodeDXYN(WORD);
        // opcode for E
        void opcodeFX07(WORD);
        void opcodeFX0A(WORD);
        void opcodeFX15(WORD);
        void opcodeFX18(WORD);
        void opcodeFX1E(WORD);
        void opcodeFX29(WORD);
        void opcodeFX33(WORD);
        void opcodeFX55(WORD);
        void opcodeFX65(WORD);

        // Opcode tables
        typedef void (*functionPtr)(WORD);

        functionPtr opcodeRootTable[18] =
        {

        };

        functionPtr opcode8Table[8] =
        {

        };
        
        // Null method to fill spaces in opcodeFTable
        void opcodeNull(WORD opcode) {
            printf("Something went wrong!");
        }

        functionPtr opcodeFTable[15] =
        {
            // take note the order of opcodes are off, according to last 4 bits
        };

};

// Chip 8 font set: to be loaded into first 80 bytes of memory
BYTE fontSet[80] =
{ 
  0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
  0x20, 0x60, 0x20, 0x20, 0x70, // 1
  0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
  0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
  0x90, 0x90, 0xF0, 0x10, 0x10, // 4
  0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
  0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
  0xF0, 0x10, 0x20, 0x40, 0x40, // 7
  0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
  0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
  0xF0, 0x90, 0xF0, 0x90, 0x90, // A
  0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
  0xF0, 0x80, 0x80, 0x80, 0xF0, // C
  0xE0, 0x90, 0x90, 0x90, 0xE0, // D
  0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
  0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

/*
================================================================================
Start of Chip8-level Functions
================================================================================
*/

void Chip8::initialize() {
    
    // Clears memory and registers, sets pc to 0x200
    memset(gameMemory, 0, sizeof(gameMemory));
    memset(dataRegisters, 0, sizeof(dataRegisters));
    I = 0;
    programCounter = 0x200;

    // Loading font set into first 80 bytes of gameMemory
    for (int i = 0; i < 80; ++i) {
        this->gameMemory[i] = fontSet[i];
    }

    // Setting timers to 0
    this->delayTimer = 0;
    this->soundTimer = 0;

}

void Chip8::loadGame(string path) {

}

void Chip8::runEmulationCycle() {

    // running emulation cycle
    while (true) {
        
        // fetch, decode and execute cycle
        WORD opcode = this->fetch(this->programCounter);
        this->execute(opcode);

        // decrement delay and sound timers if greater than 0
        if (this->delayTimer > 0) {
            --(this->delayTimer);
        }

        if (this->soundTimer > 0) {
            --(this->soundTimer);
        }

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

    // Use an array of function pointers (opcodeRootTable) to call functions directly
    // 18 elements in top level function table (opcodeRootTable)
    // Opcodes starting with 8 has 8 distinct functions (opcode8Table)
    // Opcodes starting with F has 9 distinct functions (opcodeFTable)

    // Get the first 4 bits to call the appropriate function
    this->opcodeRootTable[(opcode & 0xF000) >> 12](opcode);
    
}

void Chip8::callOpcode8(WORD opcode) {

    // Use the last 4 bits to differentiate opcodes
    BYTE last4bits = opcode & 0x00F;
    if (last4bits == 0xE) {
        // run opcode 8XYE
        this->opcode8XYE(opcode);
    } else {
        this->opcode8Table[last4bits](opcode);
    }

}

void Chip8::callOpcodeF(WORD opcode) {

    // Use the last 4 bits to differentiate opcodes
    BYTE last4bits = opcode & 0x00F;
    this->opcodeFTable[last4bits](opcode);

}

void Chip8::callOpcodeFX_5(WORD opcode) {

    // Get 7-4th bits to differentiate opcodes
    BYTE bits = (opcode & 0x00F0) >> 4;
    switch (bits) {
        case 0x1: this->opcodeFX15(opcode); break;
        case 0x5: this->opcodeFX55(opcode); break;
        case 0x6: this->opcodeFX65(opcode); break;
        default: break;
    }

}

/*
================================================================================
Start of Opcode Functions
================================================================================
*/

//Calls RCA 1802 program at address NNN. Not necessary for most ROMs.
void Chip8::opcode0NNN(WORD opcode) {

}

//Clears the screen
void Chip8::opcode00E0() {

}

//Returns from a subroutine
void Chip8::opcode00EE() {
    programCounter = gameStack.back();
    gameStack.pop_back();
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

//Sets VX to the value of VY
void Chip8::opcode8XY0(WORD opcode) {
    int X = (opcode & 0x0F00) >> 8;
    int Y = (opcode & 0x00F0) >> 4;

    dataRegisters[X] = dataRegisters[Y];
}

//Sets VX to VX OR VY
void Chip8::opcode8XY1(WORD opcode) {
    int X = (opcode & 0x0F00) >> 8;
    int Y = (opcode & 0x00F0) >> 4;

    dataRegisters[X] = dataRegisters[X] | dataRegisters[Y];
}

//Sets VX to VX AND VY
void Chip8::opcode8XY2(WORD opcode) {
    int X = (opcode & 0x0F00) >> 8;
    int Y = (opcode & 0x00F0) >> 4;

    dataRegisters[X] = dataRegisters[X] & dataRegisters[Y];
}

//Sets VX to VX XOR VY
void Chip8::opcode8XY3(WORD opcode) {
    int X = (opcode & 0x0F00) >> 8;
    int Y = (opcode & 0x00F0) >> 4;

    dataRegisters[X] = dataRegisters[X] ^ dataRegisters[Y];
}

//Adds VY to VX. VF is set to 1 when there's a carry, and to 0 when there isn't
void Chip8::opcode8XY4(WORD opcode) {
    int X = (opcode & 0x0F00) >> 8;
    int Y = (opcode & 0x00F0) >> 4;

    dataRegisters[X] = dataRegisters[X] + dataRegisters[Y];

    if (dataRegisters[X] > 255) {
        dataRegisters[0xF] = 1;
    } else {
        dataRegisters[0xF] = 0;
    }
}

//VY is subtracted from VX. VF is set to 0 when there's a borrow, and 1 when there isn't.
void Chip8::opcode8XY5(WORD opcode) {
    int X = (opcode & 0x0F00) >> 8;
    int Y = (opcode & 0x00F0) >> 4;

    if (dataRegisters[X] > dataRegisters[Y]) {
        dataRegisters[0xF] = 1;
    } else {
        dataRegisters[0xF] = 0;
    }

    dataRegisters[X] = dataRegisters[X] - dataRegisters[Y];
}

//Stores the least significant bit of VX in VF and then shifts VX to the right by 1
void Chip8::opcode8XY6(WORD opcode) {
    int X = (opcode & 0x0F00) >> 8;

    dataRegisters[0xF] = dataRegisters[X] & 0x1;
    dataRegisters[X] >>= 1;
}

//Sets VX to VY minus VX. VF is set to 0 when there's a borrow, and 1 when there isn't
void Chip8::opcode8XY7(WORD opcode) {
    int X = (opcode & 0x0F00) >> 8;
    int Y = (opcode & 0x00F0) >> 4;

    if (dataRegisters[Y] > dataRegisters[X]) {
        dataRegisters[0xF] = 1;
    } else {
        dataRegisters[0xF] = 0;
    }

    dataRegisters[X] = dataRegisters[Y] - dataRegisters[X];
}

//Stores the most significant bit of VX in VF and then shifts VX to the left by 1
void Chip8::opcode8XYE(WORD opcode) {
    int X = (opcode & 0x0F00) >> 8;

    dataRegisters[0xF] = dataRegisters[X] >> 7;
    dataRegisters[X] <<= 1;
}

//Skips the next instruction if VX doesn't equal VY
void Chip8::opcode9XY0(WORD opcode) {
    int X = (opcode & 0x0F00) >> 8;
    int Y = (opcode & 0x00F0) >> 4;

    if (dataRegisters[X] != dataRegisters[Y]) {
        programCounter += 2;
    }
}

//Sets I to the address NNN
void Chip8::opcodeANNN(WORD opcode) {
    I = opcode & 0x0FFF;
}

//Jumps to the address NNN plus V0
void Chip8::opcodeBNNN(WORD opcode) {
    programCounter = (opcode & 0x0FFF) + dataRegisters[0];
}

//Sets VX to the result of a bitwise and operation on a random number (Typically: 0 to 255) and NN
void Chip8::opcodeCXNN(WORD opcode) {
    int NN = opcode & 0x00FF;
    int X = (opcode & 0x0F00) >> 8;

    dataRegisters[X] = (rand() % 256) & NN;
}

//Draws a sprite at coordinate (VX, VY) that has a width of 8 pixels and a height of N pixels.
//Each row of 8 pixels is read as bit-coded starting from memory location I; 
//I value doesn’t change after the execution of this instruction.
//As described above, VF is set to 1 if any screen pixels are flipped from set to unset when the sprite is drawn,
//and to 0 if that doesn’t happen.
void Chip8::opcodeDXYN(WORD opcode) {

}

//Skips the next instruction if the key stored in VX is pressed
void Chip8::opcodeEX9E(WORD opcode) {

}

//Skips the next instruction if the key stored in VX isn't pressed
void Chip8::opcodeEXA1(WORD opcode) {

}

//Sets VX to the value of the delay timer
void Chip8::opcodeFX07(WORD opcode) {
    int X = (opcode & 0x0F00) >> 8;

    dataRegisters[X] = delayTimer;
}

//A key press is awaited, and then stored in VX
void Chip8::opcodeFX0A(WORD opcode) {
    
}

//Sets the delay timer to VX
void Chip8::opcodeFX15(WORD opcode) {
    int X = (opcode & 0x0F00) >> 8;

    delayTimer = dataRegisters[X];
}

//Sets the sound timer to VX
void Chip8::opcodeFX18(WORD opcode) {
    int X = (opcode & 0x0F00) >> 8;

    soundTimer = dataRegisters[X];
}

//Adds VX to I
void Chip8::opcodeFX1E(WORD opcode) {
    int X = (opcode & 0x0F00) >> 8;

    I += dataRegisters[X];
}

//Sets I to the location of the sprite for the character in VX. 
//Characters 0-F (in hexadecimal) are represented by a 4x5 font.
void Chip8::opcodeFX29(WORD opcode) {
    
}

//Stores the binary-coded decimal representation of VX, 
//with the most significant of three digits at the address in I, 
//the middle digit at I plus 1, and the least significant digit at I plus 2. 
//(In other words, take the decimal representation of VX, place the hundreds digit 
//in memory at location in I, the tens digit at location I+1, and the ones digit at location I+2.)
void Chip8::opcodeFX33(WORD opcode) {
    int X = (opcode & 0x0F00) >> 8;

    int decimalVX = dataRegisters[X];
    int ones = decimalVX % 10;
    int tens = (decimalVX % 100) / 10;
    int hundreds = decimalVX / 100;

    gameMemory[I] = hundreds;
    gameMemory[I + 1] = tens;
    gameMemory[I + 2] = ones;
}

//Stores V0 to VX (including VX) in memory starting at address I. 
//The offset from I is increased by 1 for each value written, but I itself is left unmodified.
void Chip8::opcodeFX55(WORD opcode) {
    int X = (opcode & 0x0F00) >> 8;

    for (int i = 0; i <= X; ++i) {
        gameMemory[I + i] = dataRegisters[i]; 
    }

    I += (X + 1);
}

//Fills V0 to VX (including VX) with values from memory starting at address I. 
//The offset from I is increased by 1 for each value written, but I itself is left unmodified.
void Chip8::opcodeFX65(WORD opcode) {
    int X = (opcode & 0x0F00) >> 8;

    for (int i = 0; i <= X; ++i) {
        dataRegisters[i] = gameMemory[I + i];
    }

    I += (X + 1);
}