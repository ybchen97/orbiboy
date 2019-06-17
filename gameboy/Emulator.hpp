#include <iostream>
#include <string>
#include <cassert>

// For the flag bits in register F
#define FLAG_ZERO 7
#define FLAG_SUB 6
#define FLAG_HALFCARRY 5
#define FLAG_CARRY 4

// For timer registers
#define DIVIDER 0xFF04
#define TIMA 0xFF05
#define TMA 0xFF06
#define TAC 0xFF07

using namespace std;

typedef unsigned char BYTE;
typedef signed char SIGNED_BYTE;
typedef unsigned short WORD; 
typedef signed short SIGNED_WORD;

union Register {
    WORD regstr;
    struct {
        BYTE low;
        BYTE high;
    };
};

enum COLOUR {
    WHITE,
    LIGHT_GRAY,
    DARK_GRAY,
    BLACK
};

class Emulator {

    public:
        // ATTRIBUTES

        // FUNCTIONS
        void resetCPU();
        bool loadGame(string);
        void writeMem(WORD, BYTE);
        BYTE readMem(WORD) const;
        void update();

        // Utility
        bool isBitSet(BYTE, int) const;
        BYTE bitSet(BYTE, int) const;
        BYTE bitReset(BYTE, int) const;

    private:
        // ATTRIBUTES

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

        // Memory items
        BYTE internalMem[0x10000]; // internal memory from 0x0000 - 0xFFFF
        BYTE cartridgeMem[0x200000]; // Catridge memory up to 2MB (2 * 2^20)
        BYTE currentROMBank; // tells which ROM bank the game is using
        BYTE RAMBanks[0x8000]; // all RAM banks
        BYTE currentRAMBank; // tells which RAM bank the game is using
        
        bool enableRAM;
        bool MBC1;
        bool MBC2;
        bool ROMBanking;

        // Timer attributes
        int timerCounter;
        int timerUpdateConstant;
        int dividerCounter;

        // Interrupt
        bool InterruptMasterEnabled; // Interrupt Master Enabledswitch

        // Joypad
        BYTE joypadState;

        // Graphics
        int scanlineCycleCount;

        // FUNCTIONS
        int executeNextOpcode();
        int executeOpcode(BYTE);

        // Memory
        void handleBanking(WORD, BYTE);
        void doRAMBankEnable(WORD, BYTE);
        void doChangeLoROMBank(BYTE);
        void doChangeHiROMBank(BYTE);
        void doRAMBankChange(BYTE);
        void doChangeROMRAMMode(BYTE);

        // Timer
        void updateTimers(int);
        bool clockEnabled();

        // Interrupt
        void flagInterrupt(int);
        void handleInterrupts();
        void triggerInterrupt(int);

        // Joypad
        BYTE getJoypadState() const;
        void buttonPressed(int);
        void buttonReleased(int);

        // Graphics
        void updateGraphics(int);
        void setLCDStatus();
        bool LCDEnabled();

        void drawScanLine();
        void renderTiles(BYTE);
        void renderSprites(BYTE);
        COLOUR getColour(BYTE, WORD) const;

        void doDMATransfer(BYTE);

};