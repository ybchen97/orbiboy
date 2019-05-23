#include <iostream>
#include <vector>
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
        void initialize() {
            // Initialize 
        }

        void loadGame(string) {

        }

        void runEmulationCycle() {

        }

    private:
        // Attributes
        BYTE gameMemory[4096];
        BYTE dataRegisters[16];
        WORD I;
        WORD programCounter;

        // Functions
        BYTE fetch(WORD) {

        }

        int decode(BYTE) {

        }

        void execute(int) {

        }

    BYTE gameMemory[4096];
    BYTE dataRegisters[16];
    WORD I;
    WORD pc;

};