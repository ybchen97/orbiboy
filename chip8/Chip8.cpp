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
    - A delay timer that counts down to 0 at 60Hz if non-zero
    - A sound timer - same as delay timer
    - Game screen of 64 x 32 pixels
    - 16 keys, represented by an array of size 16 which stores its state of
    being pressed or not pressed. 1 == pressed, 0 == pressed
    */

    public:
        // Attributes
        BYTE keyState[16];
        BYTE gameScreen[2048];
        int drawFlag;

        // Functions
        void initialize();
        bool loadGame(string);
        void runEmulationCycle(); 

        // Opcode functions
        void callOpcode8(WORD);
        void callOpcodeF(WORD);
        void callOpcodeFX_5(WORD);
        void opcodeNull(WORD);

        void opcode0(WORD);
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
        void opcodeE(WORD);
        void opcodeFX07(WORD);
        void opcodeFX0A(WORD);
        void opcodeFX15(WORD);
        void opcodeFX18(WORD);
        void opcodeFX1E(WORD);
        void opcodeFX29(WORD);
        void opcodeFX33(WORD);
        void opcodeFX55(WORD);
        void opcodeFX65(WORD);

    private:
        // Attributes
        BYTE gameMemory[4096];
        BYTE dataRegisters[16];
        WORD I;
        WORD programCounter;
        stack<WORD> gameStack;
        BYTE delayTimer;
        BYTE soundTimer;
        int jumpFlag; // used for jumps and subroutines

        // Functions
        WORD fetch(WORD);
        void execute(WORD);

};

// Opcode tables
typedef void (Chip8::*opcodePtr)(WORD);

opcodePtr opcodeRootTable[16] =
{
    &Chip8::opcode0,    
    &Chip8::opcode1NNN, 
    &Chip8::opcode2NNN,
    &Chip8::opcode3XNN, 
    &Chip8::opcode4XNN, 
    &Chip8::opcode5XY0,
    &Chip8::opcode6XNN, 
    &Chip8::opcode7XNN, 
    &Chip8::callOpcode8,
    &Chip8::opcode9XY0, 
    &Chip8::opcodeANNN, 
    &Chip8::opcodeBNNN,
    &Chip8::opcodeCXNN, 
    &Chip8::opcodeDXYN, 
    &Chip8::opcodeE, 
    &Chip8::callOpcodeF
};

opcodePtr opcode8Table[8] =
{
    &Chip8::opcode8XY0, 
    &Chip8::opcode8XY1, 
    &Chip8::opcode8XY2,
    &Chip8::opcode8XY3, 
    &Chip8::opcode8XY4, 
    &Chip8::opcode8XY5,
    &Chip8::opcode8XY6, 
    &Chip8::opcode8XY7
};

// Null method to fill spaces in opcodeFTable
void Chip8::opcodeNull(WORD opcode) {
    printf("Something went wrong! opcodeNull is called!\n");
}

opcodePtr opcodeFTable[15] =
{
    // take note the order of opcodes are off, according to last 4 bits
    &Chip8::opcodeNull,
    &Chip8::opcodeNull,
    &Chip8::opcodeNull,
    &Chip8::opcodeFX33,
    &Chip8::opcodeNull,
    &Chip8::callOpcodeFX_5,
    &Chip8::opcodeNull,
    &Chip8::opcodeFX07,
    &Chip8::opcodeFX18,
    &Chip8::opcodeFX29,
    &Chip8::opcodeFX0A,
    &Chip8::opcodeNull,
    &Chip8::opcodeNull,
    &Chip8::opcodeNull,
    &Chip8::opcodeFX1E
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
    
    // Clears memory and registers, sets pc to 0x200, clears screen
    memset(gameMemory, 0, sizeof(gameMemory));
    memset(dataRegisters, 0, sizeof(dataRegisters));
    I = 0;
    programCounter = 0x200;
    memset(gameScreen, 0, 2048); 

    // Loading font set into first 80 bytes of gameMemory
    for (int i = 0; i < 80; ++i) {
        this->gameMemory[i] = fontSet[i];
    }

    // Setting draw and jump flags to 0
    this->drawFlag = 0;
    this->jumpFlag = 0;

    // Setting timers to 0
    this->delayTimer = 0;
    this->soundTimer = 0;

}

bool Chip8::loadGame(string file_path) {
    //load in the game
    FILE* in;
    in = fopen(file_path.c_str(), "rb") ;

    // check rom exists
    if (in == NULL) {
        printf("Cannot load game file ERROR!!!");
        return false ;
    }

    // Get file size
    fseek(in, 0, SEEK_END);
    int fileSize = ftell(in);
    rewind(in);
    
    // Read file into gameMemory
    fread(&gameMemory[0x200], fileSize, 1, in) ;
    fclose(in) ;

    return true ;
}

void Chip8::runEmulationCycle() {

    // running emulation cycle
        
    // fetch, decode and execute cycle
    WORD opcode = this->fetch(this->programCounter);
    this->execute(opcode);

    // increment program counter if there is no jump or no subroutine is called.
    if (this->jumpFlag == 0) {
        this->programCounter += 2;
    } else {
        this->jumpFlag = 0;
    }

    // decrement delay and sound timers if greater than 0
    if (this->delayTimer > 0) {
        --(this->delayTimer);
    }

    if (this->soundTimer > 0) {
        --(this->soundTimer);
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
    
    WORD opcode = (gameMemory[pc] << 8) | gameMemory[pc + 1];

    cout << "Opcode " << hex << opcode << " fetched" << endl;

    return opcode;

}

void Chip8::execute(WORD opcode) {

    // Use an array of function pointers (opcodeRootTable) to call functions directly
    // 18 elements in top level function table (opcodeRootTable)
    // Opcodes starting with 8 has 8 distinct functions (opcode8Table)
    // Opcodes starting with F has 9 distinct functions (opcodeFTable)

    // Get the first 4 bits to call the appropriate function
    (this->*(opcodeRootTable[(opcode & 0xF000) >> 12]))(opcode);
    
}

void Chip8::callOpcode8(WORD opcode) {

    // Use the last 4 bits to differentiate opcodes
    BYTE last4bits = opcode & 0x000F;
    if (last4bits == 0xE) {
        // run opcode 8XYE
        this->opcode8XYE(opcode);
    } else {
        (this->*(opcode8Table[last4bits]))(opcode);
    }

}

void Chip8::callOpcodeF(WORD opcode) {

    // Use the last 4 bits to differentiate opcodes
    BYTE last4bits = opcode & 0x000F;
    (this->*(opcodeFTable[last4bits]))(opcode);

}

void Chip8::callOpcodeFX_5(WORD opcode) {

    // Get 7-4th bits to differentiate opcodes
    BYTE bits = (opcode & 0x00F0) >> 4;
    switch (bits) {
        case 0x1: this->opcodeFX15(opcode); break;
        case 0x5: this->opcodeFX55(opcode); break;
        case 0x6: this->opcodeFX65(opcode); break;
        default: 
            printf("Something went wrong in callOpcodeFX_5!\n");
            break;
    }

}

/*
================================================================================
Start of Opcode Functions
================================================================================
*/

void Chip8::opcode0(WORD opcode) {

    printf("opcode0 called\n");

    WORD last12bits = opcode & 0x0FFF;
    switch (last12bits) {
        case 0x0E0: 
            // Clears the screen
            memset(gameScreen, 0, 2048); 
            drawFlag = 1;
            break;
        case 0x0EE: 
            // Returns from subroutine
            programCounter = gameStack.top();
            gameStack.pop();
            break;
        default:
            // Calls RCA 1802 program at address NNN.
            // Not necessary for most ROMs.
            printf("Opcode 0NNN is called! Not implemented!\n");
            break;
    }

}

//Jumps to address NNN
void Chip8::opcode1NNN(WORD opcode) {

    printf("opcode1NNN called\n");

    programCounter = opcode & 0x0FFF;
    this->jumpFlag = 1;
}

//Calls subroutine at NNN
void Chip8::opcode2NNN(WORD opcode) {

    printf("opcode2NNN called\n");

    gameStack.push(programCounter);
    programCounter = opcode & 0x0FFF;
    this->jumpFlag = 1;
}

//Skips the next instruction if VX equals NN
void Chip8::opcode3XNN(WORD opcode) {

    printf("opcode3XNN called\n");

    int NN = opcode & 0x00FF;
    int X = (opcode & 0x0F00) >> 8;

    if (dataRegisters[X] == NN) {
        programCounter += 2;
    }
}

//Skips the next instruction if VX doesn't equal NN
void Chip8::opcode4XNN(WORD opcode) {

    printf("opcode4XNN called\n");

    int NN = opcode & 0x00FF;
    int X = (opcode & 0x0F00) >> 8;

    if (dataRegisters[X] != NN) {
        programCounter += 2;
    }
}

//Skips the next instruction if VX equals VY
void Chip8::opcode5XY0(WORD opcode) {

    printf("opcode5XY0 called\n");

    int X = (opcode & 0x0F00) >> 8;
    int Y = (opcode & 0x00F0) >> 4;

    if (dataRegisters[X] == dataRegisters[Y]) {
        programCounter += 2;
    }
}

//Sets VX to NN
void Chip8::opcode6XNN(WORD opcode) {

    printf("opcode6XNN called\n");

    int NN = opcode & 0x00FF;
    int X = (opcode & 0x0F00) >> 8;

    dataRegisters[X] = NN;
}

//Adds NN to VX
void Chip8::opcode7XNN(WORD opcode) {

    printf("opcode7XNN called\n");

    int NN = opcode & 0x00FF;
    int X = (opcode & 0x0F00) >> 8;

    dataRegisters[X] += NN;
}

//Sets VX to the value of VY
void Chip8::opcode8XY0(WORD opcode) {

    printf("opcode8XY0 called\n");

    int X = (opcode & 0x0F00) >> 8;
    int Y = (opcode & 0x00F0) >> 4;

    dataRegisters[X] = dataRegisters[Y];
}

//Sets VX to VX OR VY
void Chip8::opcode8XY1(WORD opcode) {

    printf("opcode8XY1 called\n");

    int X = (opcode & 0x0F00) >> 8;
    int Y = (opcode & 0x00F0) >> 4;

    dataRegisters[X] = dataRegisters[X] | dataRegisters[Y];
}

//Sets VX to VX AND VY
void Chip8::opcode8XY2(WORD opcode) {

    printf("opcode8XY2 called\n");

    int X = (opcode & 0x0F00) >> 8;
    int Y = (opcode & 0x00F0) >> 4;

    dataRegisters[X] = dataRegisters[X] & dataRegisters[Y];
}

//Sets VX to VX XOR VY
void Chip8::opcode8XY3(WORD opcode) {

    printf("opcode8XY3 called\n");

    int X = (opcode & 0x0F00) >> 8;
    int Y = (opcode & 0x00F0) >> 4;

    dataRegisters[X] = dataRegisters[X] ^ dataRegisters[Y];
}

//Adds VY to VX. VF is set to 1 when there's a carry, and to 0 when there isn't
void Chip8::opcode8XY4(WORD opcode) {

    printf("opcode8XY4 called\n");

    int X = (opcode & 0x0F00) >> 8;
    int Y = (opcode & 0x00F0) >> 4;

    dataRegisters[X] = dataRegisters[X] + dataRegisters[Y];

    if(dataRegisters[(opcode & 0x00F0) >> 4] > (0xFF - dataRegisters[(opcode & 0x0F00) >> 8])) {
        dataRegisters[0xF] = 1; // carry
    } else {
        dataRegisters[0xF] = 0;
    }
}

//VY is subtracted from VX. VF is set to 0 when there's a borrow, and 1 when there isn't.
void Chip8::opcode8XY5(WORD opcode) {

    printf("opcode8XY5 called\n");

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

    printf("opcode8XY6 called\n");

    int X = (opcode & 0x0F00) >> 8;

    dataRegisters[0xF] = dataRegisters[X] & 0x1;
    dataRegisters[X] >>= 1;
}

//Sets VX to VY minus VX. VF is set to 0 when there's a borrow, and 1 when there isn't
void Chip8::opcode8XY7(WORD opcode) {

    printf("opcode8XY7 called\n");

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

    printf("opcode8XYE called\n");

    int X = (opcode & 0x0F00) >> 8;

    dataRegisters[0xF] = dataRegisters[X] >> 7;
    dataRegisters[X] <<= 1;
}

//Skips the next instruction if VX doesn't equal VY
void Chip8::opcode9XY0(WORD opcode) {

    printf("opcode9XY0 called\n");

    int X = (opcode & 0x0F00) >> 8;
    int Y = (opcode & 0x00F0) >> 4;

    if (dataRegisters[X] != dataRegisters[Y]) {
        programCounter += 2;
    }
}

//Sets I to the address NNN
void Chip8::opcodeANNN(WORD opcode) {

    printf("opcodeANNN called\n");

    I = opcode & 0x0FFF;
}

//Jumps to the address NNN plus V0
void Chip8::opcodeBNNN(WORD opcode) {

    printf("opcodeBNNN called\n");

    programCounter = (opcode & 0x0FFF) + dataRegisters[0];
    this->jumpFlag = 1;
}

//Sets VX to the result of a bitwise and operation on a random number (Typically: 0 to 255) and NN
void Chip8::opcodeCXNN(WORD opcode) {

    printf("opcodeCXNN called\n");

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

    printf("opcodeDXYN called\n");

    // Get x y coordinate and height
    int xCoord = dataRegisters[(opcode & 0x0F00) >> 8];
    int yCoord = dataRegisters[(opcode & 0x00F0) >> 4];
    int height = opcode & 0x000F;

    dataRegisters[0xF] = 0;
    
    // Drawing sprite of width 8 and height height
    for (int y = 0; y < height; ++y) {
        // Get row of pixels from sprite 
        BYTE row = gameMemory[I + y];

        for (int x = 0; x < 8; ++x) {
            // Get individual sprite pixel using bit mask
            int mask = 1 << (7 - x);
            
            // if sprite pixel == 1, toggle screen pixel
            if ((row & mask) != 0) {
                int drawX = xCoord + x;
                int drawY = yCoord + y;

                if (gameScreen[drawY * 64 + drawX] == 1) {
                    dataRegisters[0xF] = 1;
                }
                
                // Toggling screen pixel with XOR operation
                gameScreen[drawY * 64 + drawX] ^= 1;
            }

        }
    }

    drawFlag = 1;

}

void Chip8::opcodeE(WORD opcode) {

    printf("opcodeE called\n");

    BYTE last4bits = opcode & 0x000F;
    switch (last4bits) {
        case 0xE: {
            // Opcode EX9E
            //Skips the next instruction if the key stored in VX is pressed
            BYTE keyReg = (opcode & 0x0F00) >> 8;
            if (dataRegisters[keyReg] == 1) {
                programCounter += 2;
            }
            break;
        }
        case 0x1: {
            // Opcode EXA1
            //Skips the next instruction if the key stored in VX isn't pressed
            BYTE keyReg = (opcode & 0x0F00) >> 8;
            if (dataRegisters[keyReg] == 0) {
                programCounter += 2;
            }
            break;
        }
        default:
            printf("Something is wrong in opcodeE()!");
            break;
    }
}

//Sets VX to the value of the delay timer
void Chip8::opcodeFX07(WORD opcode) {

    printf("opcodeFX07 called\n");

    int X = (opcode & 0x0F00) >> 8;

    dataRegisters[X] = delayTimer;
}

//A key press is awaited, and then stored in VX
void Chip8::opcodeFX0A(WORD opcode) {

    printf("opcodeFX0A called\n");

    bool key_pressed = false;

    for (int i = 0; i < 16; ++i) {
        if(keyState[i] != 0) {
            dataRegisters[(opcode & 0x0F00) >> 8] = i;
            key_pressed = true;
        }
    }

    // If no key is pressed, return and try again.
    if(!key_pressed) {
        this->jumpFlag = 1;
        return;
    }

}

//Sets the delay timer to VX
void Chip8::opcodeFX15(WORD opcode) {

    printf("opcodeFX15 called\n");

    int X = (opcode & 0x0F00) >> 8;

    delayTimer = dataRegisters[X];
}

//Sets the sound timer to VX
void Chip8::opcodeFX18(WORD opcode) {

    printf("opcodeFX18 called\n");

    int X = (opcode & 0x0F00) >> 8;

    soundTimer = dataRegisters[X];
}

//Adds VX to I
void Chip8::opcodeFX1E(WORD opcode) {

    printf("opcodeFX1E called\n");

    int X = (opcode & 0x0F00) >> 8;

    I += dataRegisters[X];
}

//Sets I to the location of the sprite for the character in VX. 
//Characters 0-F (in hexadecimal) are represented by a 4x5 font.
void Chip8::opcodeFX29(WORD opcode) {

    printf("opcodeFX29 called\n");

    I = dataRegisters[(opcode & 0x0F00) >> 8] * 5; // since each font is 5 bytes long and starts at 0x0000 in gameMemory
}

//Stores the binary-coded decimal representation of VX, 
//with the most significant of three digits at the address in I, 
//the middle digit at I plus 1, and the least significant digit at I plus 2. 
//(In other words, take the decimal representation of VX, place the hundreds digit 
//in memory at location in I, the tens digit at location I+1, and the ones digit at location I+2.)
void Chip8::opcodeFX33(WORD opcode) {

    printf("opcodeFX33 called\n");

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

    printf("opcodeFX55 called\n");

    int X = (opcode & 0x0F00) >> 8;

    for (int i = 0; i <= X; ++i) {
        gameMemory[I + i] = dataRegisters[i]; 
    }

    I += (X + 1);
}

//Fills V0 to VX (including VX) with values from memory starting at address I. 
//The offset from I is increased by 1 for each value written, but I itself is left unmodified.
void Chip8::opcodeFX65(WORD opcode) {

    printf("opcodeFX65 called\n");

    int X = (opcode & 0x0F00) >> 8;

    for (int i = 0; i <= X; ++i) {
        dataRegisters[i] = gameMemory[I + i];
    }

    I += (X + 1);
}