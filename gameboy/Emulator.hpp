#include <iostream>
#include <string>
#include <cassert>
#include <fstream>

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
        uint32_t displayPixels[160 * 144];

        // FUNCTIONS
        bool loadGame(string);
        void saveGame(string);
        void loadState(string);
        void saveState(string);

        void resetCPU();
        void update();
        void buttonPressed(int);
        void buttonReleased(int);
        void setRenderGraphics(void(*funcPtr)());

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
        bool isHalted;

        // Joypad
        BYTE joypadState;

        // Graphics
        int scanlineCycleCount;
        void(*doRenderPtr)();

        // FUNCTIONS
        int executeNextOpcode();
        int executeOpcode(BYTE);
        int executeCBOpcode();

        // Memory
        void writeMem(WORD, BYTE);
        BYTE readMem(WORD) const;
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

        // Graphics
        void updateGraphics(int);
        void setLCDStatus();
        bool LCDEnabled();

        void drawScanLine();
        void renderTiles(BYTE);
        void renderSprites(BYTE);
        COLOUR getColour(BYTE, WORD) const;

        void doDMATransfer(BYTE);

        void renderGraphics();

        ////////// Start of opcodes //////////
        // 8 bit Load Commands
        int LD_r_R(BYTE&, BYTE);
        int LD_r_n(BYTE&);
        int LD_r_HL(BYTE&);
        int LD_HL_r(BYTE);
        int LD_HL_n();
        int LD_A_BC();
        int LD_A_DE();
        int LD_A_nn();
        int LD_BC_A();
        int LD_DE_A();
        int LD_nn_A();
        int LD_A_FF00n();
        int LD_FF00n_A();
        int LD_A_FF00C();
        int LD_FF00C_A();
        int LDI_HL_A();
        int LDI_A_HL();
        int LDD_HL_A();
        int LDD_A_HL();
        
        // 16 bit Load Commands
        int LD_rr_nn(Register&);
        int LD_SP_HL();
        int LD_nn_SP();
        int PUSH_rr(Register);
        int POP_rr(Register&);

        // 8 bit Arithmetic/Logical Commands
        int ADD_A_r(BYTE);
        int ADD_A_n();
        int ADD_A_HL();
        int ADC_A_r(BYTE);
        int ADC_A_n();
        int ADC_A_HL();
        int SUB_r(BYTE);
        int SUB_n();
        int SUB_HL();
        int SBC_A_r(BYTE);
        int SBC_A_n();
        int SBC_A_HL();
        int AND_r(BYTE);
        int AND_n();
        int AND_HL();
        int XOR_r(BYTE);
        int XOR_n();
        int XOR_HL();
        int OR_r(BYTE);
        int OR_n();
        int OR_HL();
        int CP_r(BYTE);
        int CP_n();
        int CP_HL();
        int INC_r(BYTE&);
        int INC_HL();
        int DEC_r(BYTE&);
        int DEC_HL();
        int DAA();
        int CPL();

        // 16 bit Arithmetic/Logical Commands
        int ADD_HL_rr(WORD);
        int INC_rr(WORD&);
        int DEC_rr(WORD&);
        int ADD_SP_dd();
        int LD_HL_SPdd();

        // Rotate and Shift Commands
        int RLCA();
        int RLA();
        int RRCA();
        int RRA();
        int RLC_r(BYTE&);
        int RLC_HL();
        int RL_r(BYTE&);
        int RL_HL();
        int RRC_r(BYTE&);
        int RRC_HL();
        int RR_r(BYTE&);
        int RR_HL();
        int SLA_r(BYTE&);
        int SLA_HL();
        int SWAP_r(BYTE&);
        int SWAP_HL();
        int SRA_r(BYTE&);
        int SRA_HL();
        int SRL_r(BYTE&);
        int SRL_HL();

        // Single Bit Operation Commands
        int BIT_n_r(BYTE&, int);
        int BIT_n_HL(int);
        int SET_n_r(BYTE&, int);
        int SET_n_HL(int);
        int RES_n_r(BYTE&, int);
        int RES_n_HL(int);

        // CPU Control Commands
        int CCF();
        int SCF();
        int NOP();
        int HALT();
        int STOP();
        int DI();
        int EI();

        // Jump Commands
        int JP_nn();
        int JP_HL();
        int JP_f_nn(BYTE);
        int JR_PCdd();
        int JR_f_PCdd(BYTE);
        int CALL_nn();
        int CALL_f_nn(BYTE);
        int RET();
        int RET_f(BYTE);
        int RETI();
        int RST_n(BYTE);
        ////////// end of opcodes //////////

};
