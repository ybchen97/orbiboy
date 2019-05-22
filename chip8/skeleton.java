/** 

Hardware details:

Memory                   : 4096 locations, each location 1 byte
16 x Data registers      : 8 bit registers from V0 to VF
Address register         : 16 bit address register I used to access memory
Program counter          : 16 bit program counter
Stack                    : size-16 stack & a pointer
Opcodes                  : 35 opcodes, each 2 byte long

Game is loaded into 0x200 of the memory, so starting address of program counter is 0x200.

*/

void fetch() {
    // fetches two successive bytes from the memory, since opcodes are 16 bit (i.e. 2 bytes long)
    
    // get data in memory where PC is pointing at, shift it left 8 places, call it MSB
    // get data at PC+1, do bitwise or with MSB to get full opcode
    
    // increment program counter i.e. PC += 2
}
