/** 

Hardware details:

Memory                   : 4096 locations, each location 1 byte
16 x Data registers      : 8 bit registers from V0 to VF
Address register         : 16 bit address register I used to access memory
Program counter          : 16 bit program counter
Stack                    : size-16 stack & a pointer
Opcodes                  : 35 opcodes, each 2 byte long

Game is loaded into 0x200 of the memory, so starting address of program counter is 0x200.

********************************************************************************
INSIDE CHIP8 FILE:
********************************************************************************

void runEmulationCycle() {
    
    // run main game loop:
    while (true) {

        // fetch, decode & execute
        opcode = this.fetch(programCounter);
        functionNo = this.decode(opcode);
        functionArray[funcitonNo].execute(opcode);
        
        programCounter += 2;

        delayTimer?

    }

}
void initialize() {

    // Memory as a size 4096 array
    // 16 8-bit data registers V0 - VF
    // 16-bit address register I
    // 16-bit program counter -> set to 0x200 i.e. 512
    // 16 level stack that can be pushed and popped to store program counter

}

void loadGame(game) {

    // load progam byte by byte into memory at location 0x200 i.e. 512

}

void fetch() {
    // fetches two successive bytes from the memory, since opcodes are 16 bit (i.e. 2 bytes long)
    
    // get data in memory where PC is pointing at, shift it left 8 places, call it MSB
    // get data at PC+1, do bitwise or with MSB to get full opcode
    
    // increment program counter i.e. PC += 2
}

void decode() {
    // switch cases for the different opcodes
    // 35 different opcodes
    
    // decode the opcode from fetch()
    // execute the function represented by the opcode
}

********************************************************************************
INSIDE MAIN FILE:
********************************************************************************

public static void main(String[] args) {

    chip8.initialize(); // initializes variables, clears memory, screen, registers
    chip8.loadGame(game);
    chip8.runEmulationCycle;


}

*/