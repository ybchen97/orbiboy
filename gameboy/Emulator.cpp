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

#include "Emulator.hpp"

/*
********************************************************************************
TOP LEVEL CPU FUNCTIONS
********************************************************************************
*/
void Emulator::resetCPU() {

    this->regAF.regstr = 0x01B0; 
    this->regBC.regstr = 0x0013; 
    this->regDE.regstr = 0x00D8;
    this->regHL.regstr = 0x014D;
    this->stackPointer.regstr = 0xFFFE;

    // After the 256 bytes of bootROM is executed, PC lands at 0x100 
    // bootROM is not implemented for this emulator
    this->programCounter.regstr = 0x100; 

    this->internalMem[0xFF05] = 0x00; // TIMA
    this->internalMem[0xFF06] = 0x00; // TMA
    this->internalMem[0xFF07] = 0x00; // TAC
    this->internalMem[0xFF10] = 0x80; 
    this->internalMem[0xFF11] = 0xBF; 
    this->internalMem[0xFF12] = 0xF3; 
    this->internalMem[0xFF14] = 0xBF; 
    this->internalMem[0xFF16] = 0x3F; 
    this->internalMem[0xFF17] = 0x00; 
    this->internalMem[0xFF19] = 0xBF; 
    this->internalMem[0xFF1A] = 0x7F; 
    this->internalMem[0xFF1B] = 0xFF; 
    this->internalMem[0xFF1C] = 0x9F; 
    this->internalMem[0xFF1E] = 0xBF; 
    this->internalMem[0xFF20] = 0xFF; 
    this->internalMem[0xFF21] = 0x00; 
    this->internalMem[0xFF22] = 0x00; 
    this->internalMem[0xFF23] = 0xBF; 
    this->internalMem[0xFF24] = 0x77; 
    this->internalMem[0xFF25] = 0xF3;
    this->internalMem[0xFF26] = 0xF1; 
    this->internalMem[0xFF40] = 0x91; // LCDC - LCD control register
    this->internalMem[0xFF42] = 0x00; // SCY - scroll Y
    this->internalMem[0xFF43] = 0x00; // SCX - scroll X
    this->internalMem[0xFF45] = 0x00; // LYC - LY Compare
    this->internalMem[0xFF47] = 0xFC; // BGP - background colour palette
    this->internalMem[0xFF48] = 0xFF; // OBP0 - Object Palette 0 (Sprites)
    this->internalMem[0xFF49] = 0xFF; // OBP1 - Object Palette 1 (Sprites)
    this->internalMem[0xFF4A] = 0x00; // WX - window X
    this->internalMem[0xFF4B] = 0x00; // WY - window Y
    this->internalMem[0xFFFF] = 0x00; // IE - Interrupt enable

    this->MBC1 = false;
    this->MBC2 = false;

    // Choosing which MBC to use
    switch (this->cartridgeMem[0x147]) {
        case 1 : this->MBC1 = true ; break;
        case 2 : this->MBC1 = true ; break;
        case 3 : this->MBC1 = true ; break;
        case 5 : this->MBC2 = true ; break;
        case 6 : this->MBC2 = true ; break;
        default : break; 
    }

    this->currentROMBank = 1;

    memset(this->RAMBanks, 0, sizeof(this->RAMBanks));
    this->currentRAMBank = 0;

    // Initialize timers. Initial clock speed is 4096hz
    this->timerUpdateConstant = 1024;
    this->timerCounter = 1024;
    this->dividerCounter = 0;

    // Interrupts
    this->InterruptMasterEnabled = false;
    this->isHalted = false;

    // Joypad
    this->joypadState = 0xFF;

    // Graphics
    this->scanlineCycleCount = 456;
    this->doRenderPtr = nullptr;
    memset(this->displayPixels, 0, sizeof(this->displayPixels));

    cout << "CPU Resetted!" << endl;

}

bool Emulator::loadGame(string file_path) {

    memset(this->cartridgeMem, 0, sizeof(this->cartridgeMem));
    
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
    
    // Read file into catridgeMem
    fread(this->cartridgeMem, fileSize, 1, in);
    fclose(in);

    return true;
}

void Emulator::update() { // MAIN UPDATE LOOP

    // update function called 60 times per second -> screen rendered @ 60fps

    const int maxCycles = 70224;
    int cyclesCount = 0;
    int cycles;

    while (cyclesCount < maxCycles) {

        int cycles = this->executeNextOpcode(); //executeNextOpcode will return the number of cycles taken
        cyclesCount += cycles;

        this->updateTimers(cycles);
        this->updateGraphics(cycles);
        this->handleInterrupts();

    }

}

void Emulator::setRenderGraphics(void(*funcPtr)()) {
    this->doRenderPtr = funcPtr;
}

int Emulator::executeNextOpcode() {
    int clockCycles;

    BYTE opcode = this->readMem(this->programCounter.regstr);

    if (this->isHalted) {
        clockCycles = this->NOP();
    } else {
        clockCycles = this->executeOpcode(opcode);
        this->programCounter.regstr++;
    }

    return clockCycles;
}

int Emulator::executeOpcode(BYTE opcode) {
    
    int cycles;

    switch (opcode) {

        /* 
        ************************************************************************
        8 bit Load Commands
        ************************************************************************
        */
        
        // Load B, R/(HL)
        case 0x40: cycles = this->LD_r_R(this->regBC.high, this->regBC.high); break;
        case 0x41: cycles = this->LD_r_R(this->regBC.high, this->regBC.low); break;
        case 0x42: cycles = this->LD_r_R(this->regBC.high, this->regDE.high); break;
        case 0x43: cycles = this->LD_r_R(this->regBC.high, this->regDE.low); break;
        case 0x44: cycles = this->LD_r_R(this->regBC.high, this->regHL.high); break;
        case 0x45: cycles = this->LD_r_R(this->regBC.high, this->regHL.low); break;
        case 0x46: cycles = this->LD_r_HL(this->regBC.high); break;
        case 0x47: cycles = this->LD_r_R(this->regBC.high, this->regAF.high); break;

        // Load C, R/(HL)
        case 0x48: cycles = this->LD_r_R(this->regBC.low, this->regBC.high); break;
        case 0x49: cycles = this->LD_r_R(this->regBC.low, this->regBC.low); break;
        case 0x4A: cycles = this->LD_r_R(this->regBC.low, this->regDE.high); break;
        case 0x4B: cycles = this->LD_r_R(this->regBC.low, this->regDE.low); break;
        case 0x4C: cycles = this->LD_r_R(this->regBC.low, this->regHL.high); break;
        case 0x4D: cycles = this->LD_r_R(this->regBC.low, this->regHL.low); break;
        case 0x4E: cycles = this->LD_r_HL(this->regBC.low); break;
        case 0x4F: cycles = this->LD_r_R(this->regBC.low, this->regAF.high); break;

        // Load D, R/(HL)
        case 0x50: cycles = this->LD_r_R(this->regDE.high, this->regBC.high); break;
        case 0x51: cycles = this->LD_r_R(this->regDE.high, this->regBC.low); break;
        case 0x52: cycles = this->LD_r_R(this->regDE.high, this->regDE.high); break;
        case 0x53: cycles = this->LD_r_R(this->regDE.high, this->regDE.low); break;
        case 0x54: cycles = this->LD_r_R(this->regDE.high, this->regHL.high); break;
        case 0x55: cycles = this->LD_r_R(this->regDE.high, this->regHL.low); break;
        case 0x56: cycles = this->LD_r_HL(this->regDE.high); break;
        case 0x57: cycles = this->LD_r_R(this->regDE.high, this->regAF.high); break;

        // Load E, R/(HL)
        case 0x58: cycles = this->LD_r_R(this->regDE.low, this->regBC.high); break;
        case 0x59: cycles = this->LD_r_R(this->regDE.low, this->regBC.low); break;
        case 0x5A: cycles = this->LD_r_R(this->regDE.low, this->regDE.high); break;
        case 0x5B: cycles = this->LD_r_R(this->regDE.low, this->regDE.low); break;
        case 0x5C: cycles = this->LD_r_R(this->regDE.low, this->regHL.high); break;
        case 0x5D: cycles = this->LD_r_R(this->regDE.low, this->regHL.low); break;
        case 0x5E: cycles = this->LD_r_HL(this->regDE.low); break;
        case 0x5F: cycles = this->LD_r_R(this->regDE.low, this->regAF.high); break;

        // Load H, R/(HL)
        case 0x60: cycles = this->LD_r_R(this->regHL.high, this->regBC.high); break;
        case 0x61: cycles = this->LD_r_R(this->regHL.high, this->regBC.low); break;
        case 0x62: cycles = this->LD_r_R(this->regHL.high, this->regDE.high); break;
        case 0x63: cycles = this->LD_r_R(this->regHL.high, this->regDE.low); break;
        case 0x64: cycles = this->LD_r_R(this->regHL.high, this->regHL.high); break;
        case 0x65: cycles = this->LD_r_R(this->regHL.high, this->regHL.low); break;
        case 0x66: cycles = this->LD_r_HL(this->regHL.high); break;
        case 0x67: cycles = this->LD_r_R(this->regHL.high, this->regAF.high); break;

        // Load L, R/(HL)
        case 0x68: cycles = this->LD_r_R(this->regHL.low, this->regBC.high); break;
        case 0x69: cycles = this->LD_r_R(this->regHL.low, this->regBC.low); break;
        case 0x6A: cycles = this->LD_r_R(this->regHL.low, this->regDE.high); break;
        case 0x6B: cycles = this->LD_r_R(this->regHL.low, this->regDE.low); break;
        case 0x6C: cycles = this->LD_r_R(this->regHL.low, this->regHL.high); break;
        case 0x6D: cycles = this->LD_r_R(this->regHL.low, this->regHL.low); break;
        case 0x6E: cycles = this->LD_r_HL(this->regHL.low); break;
        case 0x6F: cycles = this->LD_r_R(this->regHL.low, this->regAF.high); break;

        // Load (HL), R
        case 0x70: cycles = this->LD_HL_r(this->regBC.high); break;
        case 0x71: cycles = this->LD_HL_r(this->regBC.low); break;
        case 0x72: cycles = this->LD_HL_r(this->regDE.high); break;
        case 0x73: cycles = this->LD_HL_r(this->regDE.low); break;
        case 0x74: cycles = this->LD_HL_r(this->regHL.high); break;
        case 0x75: cycles = this->LD_HL_r(this->regHL.low); break;
        case 0x77: cycles = this->LD_HL_r(this->regAF.high); break;

        // Load A, R/(HL)
        case 0x78: cycles = this->LD_r_R(this->regAF.high, this->regBC.high); break;
        case 0x79: cycles = this->LD_r_R(this->regAF.high, this->regBC.low); break;
        case 0x7A: cycles = this->LD_r_R(this->regAF.high, this->regDE.high); break;
        case 0x7B: cycles = this->LD_r_R(this->regAF.high, this->regDE.low); break;
        case 0x7C: cycles = this->LD_r_R(this->regAF.high, this->regHL.high); break;
        case 0x7D: cycles = this->LD_r_R(this->regAF.high, this->regHL.low); break;
        case 0x7E: cycles = this->LD_r_HL(this->regAF.high); break;
        case 0x7F: cycles = this->LD_r_R(this->regAF.high, this->regAF.high); break;

        // Load R, n
        case 0x06: cycles = this->LD_r_n(this->regBC.high); break;
        case 0x0E: cycles = this->LD_r_n(this->regBC.low); break;
        case 0x16: cycles = this->LD_r_n(this->regDE.high); break;
        case 0x1E: cycles = this->LD_r_n(this->regDE.low); break;
        case 0x26: cycles = this->LD_r_n(this->regHL.high); break;
        case 0x2E: cycles = this->LD_r_n(this->regHL.low); break;
        case 0x36: cycles = this->LD_HL_n(); break;
        case 0x3E: cycles = this->LD_r_n(this->regAF.high); break;

        // Load A, RR
        case 0x0A: cycles = this->LD_A_BC(); break;
        case 0x1A: cycles = this->LD_A_DE(); break;

        // Load RR, A
        case 0x02: cycles = this->LD_BC_A(); break;
        case 0x12: cycles = this->LD_DE_A(); break;

        // Load A, nn
        case 0xFA: cycles = this->LD_A_nn(); break;

        // Load nn, A
        case 0xEA: cycles = this->LD_nn_A(); break;

        // Load A, FF00+n, FF00+c, vice versa
        case 0xF0: cycles = this->LD_A_FF00n(); break;
        case 0xE0: cycles = this->LD_FF00n_A(); break;
        case 0xF2: cycles = this->LD_A_FF00C(); break;
        case 0xE2: cycles = this->LD_FF00C_A(); break;

        // Load increment/decrement HL, A, vice versa
        case 0x22: cycles = this->LDI_HL_A(); break;
        case 0x2A: cycles = this->LDI_A_HL(); break;
        case 0x32: cycles = this->LDD_HL_A(); break;
        case 0x3A: cycles = this->LDD_A_HL(); break;

        /* 
        ************************************************************************
        16 bit Load Commands
        ************************************************************************
        */

        // Load rr, nn
        case 0x01: cycles = this->LD_rr_nn(this->regBC); break;
        case 0x11: cycles = this->LD_rr_nn(this->regDE); break;
        case 0x21: cycles = this->LD_rr_nn(this->regHL); break;
        case 0x31: cycles = this->LD_rr_nn(this->stackPointer); break;

        // Load SP, HL
        case 0xF9: cycles = this->LD_SP_HL(); break;

        // Load nn, SP
        case 0x08: cycles = this->LD_nn_SP(); break;

        // Push rr
        case 0xC5: cycles = this->PUSH_rr(this->regBC); break;
        case 0xD5: cycles = this->PUSH_rr(this->regDE); break;
        case 0xE5: cycles = this->PUSH_rr(this->regHL); break;
        case 0xF5: cycles = this->PUSH_rr(this->regAF); break;

        // Pop rr
        case 0xC1: cycles = this->POP_rr(this->regBC); break;
        case 0xD1: cycles = this->POP_rr(this->regDE); break;
        case 0xE1: cycles = this->POP_rr(this->regHL); break;
        case 0xF1: cycles = this->POP_rr(this->regAF); break;

        /* 
        ************************************************************************
        8 bit Arithmetic/Logical commands
        ************************************************************************
        */

        // Add A, r
        case 0x80: cycles = this->ADD_A_r(this->regBC.high); break;
        case 0x81: cycles = this->ADD_A_r(this->regBC.low); break;
        case 0x82: cycles = this->ADD_A_r(this->regDE.high); break;
        case 0x83: cycles = this->ADD_A_r(this->regDE.low); break;
        case 0x84: cycles = this->ADD_A_r(this->regHL.high); break;
        case 0x85: cycles = this->ADD_A_r(this->regHL.low); break;
        case 0x86: cycles = this->ADD_A_HL(); break;
        case 0x87: cycles = this->ADD_A_r(this->regAF.high); break;

        // Add A, n
        case 0xC6: cycles = this->ADD_A_n(); break;

        // ADC A, r
        case 0x88: cycles = this->ADC_A_r(this->regBC.high); break;
        case 0x89: cycles = this->ADC_A_r(this->regBC.low); break;
        case 0x8A: cycles = this->ADC_A_r(this->regDE.high); break;
        case 0x8B: cycles = this->ADC_A_r(this->regDE.low); break;
        case 0x8C: cycles = this->ADC_A_r(this->regHL.high); break;
        case 0x8D: cycles = this->ADC_A_r(this->regHL.low); break;
        case 0x8E: cycles = this->ADC_A_HL(); break;
        case 0x8F: cycles = this->ADC_A_r(this->regAF.high); break;

        // ADC A, n
        case 0xCE: cycles = this->ADC_A_n(); break;

        // Sub r
        case 0x90: cycles = this->SUB_r(this->regBC.high); break;
        case 0x91: cycles = this->SUB_r(this->regBC.low); break;
        case 0x92: cycles = this->SUB_r(this->regDE.high); break;
        case 0x93: cycles = this->SUB_r(this->regDE.low); break;
        case 0x94: cycles = this->SUB_r(this->regHL.high); break;
        case 0x95: cycles = this->SUB_r(this->regHL.low); break;
        case 0x96: cycles = this->SUB_HL(); break;
        case 0x97: cycles = this->SUB_r(this->regAF.high); break;

        // Sub n
        case 0xD6: cycles = this->SUB_n(); break;

        // SBC A, r
        case 0x98: cycles = this->SBC_A_r(this->regBC.high); break;
        case 0x99: cycles = this->SBC_A_r(this->regBC.low); break;
        case 0x9A: cycles = this->SBC_A_r(this->regDE.high); break;
        case 0x9B: cycles = this->SBC_A_r(this->regDE.low); break;
        case 0x9C: cycles = this->SBC_A_r(this->regHL.high); break;
        case 0x9D: cycles = this->SBC_A_r(this->regHL.low); break;
        case 0x9E: cycles = this->SBC_A_HL(); break;
        case 0x9F: cycles = this->SBC_A_r(this->regAF.high); break;  

        // SBC A, n
        case 0xDE: cycles = this->SBC_A_n(); break;

        // AND r
        case 0xA0: cycles = this->AND_r(this->regBC.high); break;
        case 0xA1: cycles = this->AND_r(this->regBC.low); break;
        case 0xA2: cycles = this->AND_r(this->regDE.high); break;
        case 0xA3: cycles = this->AND_r(this->regDE.low); break;
        case 0xA4: cycles = this->AND_r(this->regHL.high); break;
        case 0xA5: cycles = this->AND_r(this->regHL.low); break;
        case 0xA6: cycles = this->AND_HL();
        case 0xA7: cycles = this->AND_r(this->regBC.high); break;

        // AND n 
        case 0xE6: cycles = this->AND_n(); break;

        // XOR r
        case 0xA8: cycles = this->XOR_r(this->regBC.high); break;
        case 0xA9: cycles = this->XOR_r(this->regBC.low); break;
        case 0xAA: cycles = this->XOR_r(this->regDE.high); break;
        case 0xAB: cycles = this->XOR_r(this->regDE.low); break;
        case 0xAC: cycles = this->XOR_r(this->regHL.high); break;
        case 0xAD: cycles = this->XOR_r(this->regHL.low); break;
        case 0xAE: cycles = this->XOR_HL();
        case 0xAF: cycles = this->XOR_r(this->regBC.high); break;

        // XOR n
        case 0xEE: cycles = this->XOR_n(); break;

        // OR r
        case 0xB0: cycles = this->OR_r(this->regBC.high); break;
        case 0xB1: cycles = this->OR_r(this->regBC.low); break;
        case 0xB2: cycles = this->OR_r(this->regDE.high); break;
        case 0xB3: cycles = this->OR_r(this->regDE.low); break;
        case 0xB4: cycles = this->OR_r(this->regHL.high); break;
        case 0xB5: cycles = this->OR_r(this->regHL.low); break;
        case 0xB6: cycles = this->OR_HL();
        case 0xB7: cycles = this->OR_r(this->regBC.high); break;

        // OR n 
        case 0xF6: cycles = this->OR_n(); break;

        // CPr
        case 0xB8: cycles = this->CP_r(this->regBC.high); break;
        case 0xB9: cycles = this->CP_r(this->regBC.low); break;
        case 0xBA: cycles = this->CP_r(this->regDE.high); break;
        case 0xBB: cycles = this->CP_r(this->regDE.low); break;
        case 0xBC: cycles = this->CP_r(this->regHL.high); break;
        case 0xBD: cycles = this->CP_r(this->regHL.low); break;
        case 0xBE: cycles = this->CP_HL();
        case 0xBF: cycles = this->CP_r(this->regBC.high); break;

        // CP n
        case 0xFE: cycles = this->CP_n(); break;

        // INC r
        case 0x04: cycles = this->INC_r(this->regBC.high); break;
        case 0x14: cycles = this->INC_r(this->regDE.high); break;
        case 0x24: cycles = this->INC_r(this->regHL.high); break;
        case 0x34: cycles = this->INC_HL(); break;
        case 0x0C: cycles = this->INC_r(this->regBC.low); break; 
        case 0x1C: cycles = this->INC_r(this->regDE.low); break;
        case 0x2C: cycles = this->INC_r(this->regHL.low); break;
        case 0x3C: cycles = this->INC_r(this->regAF.high); break;  

        // DEC r
        case 0x05: cycles = this->DEC_r(this->regBC.high); break;
        case 0x15: cycles = this->DEC_r(this->regDE.high); break;
        case 0x25: cycles = this->DEC_r(this->regHL.high); break;
        case 0x35: cycles = this->DEC_HL(); break;
        case 0x0D: cycles = this->DEC_r(this->regBC.low); break; 
        case 0x1D: cycles = this->DEC_r(this->regDE.low); break;
        case 0x2D: cycles = this->DEC_r(this->regHL.low); break;
        case 0x3D: cycles = this->DEC_r(this->regAF.high); break;

        // DAA
        case 0x27: cycles = this->DAA(); break;

        // CPL
        case 0x2F: cycles = this->CPL(); break;

        /* 
        ************************************************************************
        16 bit Arithmetic/Logical commands
        ************************************************************************
        */

        // Add HL, rr
        case 0x09: cycles = this->ADD_HL_rr(this->regBC.regstr); break;
        case 0x19: cycles = this->ADD_HL_rr(this->regDE.regstr); break;
        case 0x29: cycles = this->ADD_HL_rr(this->regHL.regstr); break;
        case 0x39: cycles = this->ADD_HL_rr(this->stackPointer.regstr); break;

        // INC rr
        case 0x03: cycles = this->INC_rr(this->regBC.regstr); break;
        case 0x13: cycles = this->INC_rr(this->regDE.regstr); break;
        case 0x23: cycles = this->INC_rr(this->regHL.regstr); break;
        case 0x33: cycles = this->INC_rr(this->stackPointer.regstr); break;

        // DEC rr
        case 0x0B: cycles = this->DEC_rr(this->regBC.regstr); break;
        case 0x1B: cycles = this->DEC_rr(this->regDE.regstr); break;
        case 0x2B: cycles = this->DEC_rr(this->regHL.regstr); break;
        case 0x3B: cycles = this->DEC_rr(this->stackPointer.regstr); break;

        // Add SP, dd
        case 0xE8: cycles = this->ADD_SP_dd(); break;

        // Load HL, SP + dd
        case 0xF8: cycles = this->LD_HL_SPdd(); break;

        /* 
        ************************************************************************
        Rotate and Shift commands
        ************************************************************************
        */

        // Non CB-prefixed Rotate commands
        case 0x07: cycles = this->RLCA(); break; // RLCA        
        case 0x17: cycles = this->RLA(); break; // RLA
        case 0x0F: cycles = this->RRCA(); break; // RRCA
        case 0x1F: cycles = this->RRA(); break; // RRA

        /* 
        ************************************************************************
        CPU Control commands
        ************************************************************************
        */

        case 0x3F: cycles = this->CCF(); break; // CCF
        case 0x37: cycles = this->SCF(); break; // SCF
        case 0x00: cycles = this->NOP(); break; // NOP
        case 0x76: cycles = this->HALT(); break; // HALT
        case 0x10: cycles = this->STOP(); break; // STOP
        case 0xF3: cycles = this->DI(); break; // DI
        case 0xFB: cycles = this->EI(); break; // EI

        /* 
        ************************************************************************
        Jump commands
        ************************************************************************
        */

        // JP nn
        case 0xC3: cycles = this->JP_nn(); break;

        // JP HL
        case 0xE9: cycles = this->JP_HL(); break;

        // JP f, nn
        case 0xC2: cycles = this->JP_f_nn(opcode); break;
        case 0xCA: cycles = this->JP_f_nn(opcode); break;
        case 0xD2: cycles = this->JP_f_nn(opcode); break;
        case 0xDA: cycles = this->JP_f_nn(opcode); break;

        // JR PC + dd
        case 0x18: cycles = this->JR_PCdd(); break;

        // JR f, PC + dd
        case 0x20: cycles = this->JR_f_PCdd(opcode); break;
        case 0x28: cycles = this->JR_f_PCdd(opcode); break;
        case 0x30: cycles = this->JR_f_PCdd(opcode); break;
        case 0x38: cycles = this->JR_f_PCdd(opcode); break;

        // Call nn
        case 0xCD: cycles = this->CALL_nn(); break;

        // Call f, nn
        case 0xC4: cycles = this->CALL_f_nn(opcode); break;
        case 0xCC: cycles = this->CALL_f_nn(opcode); break;
        case 0xD4: cycles = this->CALL_f_nn(opcode); break;
        case 0xDC: cycles = this->CALL_f_nn(opcode); break;

        // RET
        case 0xC9: cycles = this->RET(); break;
        
        // RET f
        case 0xC0: cycles = this->RET_f(opcode); break; 
        case 0xC8: cycles = this->RET_f(opcode); break;
        case 0xD0: cycles = this->RET_f(opcode); break;
        case 0xD8: cycles = this->RET_f(opcode); break;

        // RETI
        case 0xD9: cycles = this->RETI(); break;

        // RST n
        case 0xC7: cycles = this->RST_n(opcode); break;
        case 0xD7: cycles = this->RST_n(opcode); break;
        case 0xE7: cycles = this->RST_n(opcode); break;
        case 0xF7: cycles = this->RST_n(opcode); break;
        case 0xCF: cycles = this->RST_n(opcode); break;
        case 0xDF: cycles = this->RST_n(opcode); break;
        case 0xEF: cycles = this->RST_n(opcode); break;
        case 0xFF: cycles = this->RST_n(opcode); break;

        /* 
        ************************************************************************
        CB-prefix commands
        ************************************************************************
        */ 

        case 0xCB: cycles = executeCBOpcode(); break;
    }

    return cycles;

}

int Emulator::executeCBOpcode() {
    
    BYTE opcode = this->readMem(this->programCounter.regstr);
    this->programCounter.regstr++;
    int cycles;

    switch (opcode) {

        /* 
        ************************************************************************
        Rotate and Shift commands
        ************************************************************************
        */

        // RLC r
        case 0x00: cycles = this->RLC_r(this->regBC.high); break;
        case 0x01: cycles = this->RLC_r(this->regBC.low); break;
        case 0x02: cycles = this->RLC_r(this->regDE.high); break;
        case 0x03: cycles = this->RLC_r(this->regDE.low); break;
        case 0x04: cycles = this->RLC_r(this->regHL.high); break;
        case 0x05: cycles = this->RLC_r(this->regHL.low); break;
        case 0x06: cycles = this->RLC_HL(); break;
        case 0x07: cycles = this->RLC_r(this->regAF.high); break;

        // RRC r
        case 0x08: cycles = this->RRC_r(this->regBC.high); break;
        case 0x09: cycles = this->RRC_r(this->regBC.low); break;
        case 0x0A: cycles = this->RRC_r(this->regDE.high); break;
        case 0x0B: cycles = this->RRC_r(this->regDE.low); break;
        case 0x0C: cycles = this->RRC_r(this->regHL.high); break;
        case 0x0D: cycles = this->RRC_r(this->regHL.low); break;
        case 0x0E: cycles = this->RRC_HL(); break;
        case 0x0F: cycles = this->RRC_r(this->regAF.high); break;

        // RL r
        case 0x10: cycles = this->RL_r(this->regBC.high); break;
        case 0x11: cycles = this->RL_r(this->regBC.low); break;
        case 0x12: cycles = this->RL_r(this->regDE.high); break;
        case 0x13: cycles = this->RL_r(this->regDE.low); break;
        case 0x14: cycles = this->RL_r(this->regHL.high); break;
        case 0x15: cycles = this->RL_r(this->regHL.low); break;
        case 0x16: cycles = this->RL_HL(); break;
        case 0x17: cycles = this->RL_r(this->regAF.high); break;

        // RR r
        case 0x18: cycles = this->RR_r(this->regBC.high); break;
        case 0x19: cycles = this->RR_r(this->regBC.low); break;
        case 0x1A: cycles = this->RR_r(this->regDE.high); break;
        case 0x1B: cycles = this->RR_r(this->regDE.low); break;
        case 0x1C: cycles = this->RR_r(this->regHL.high); break;
        case 0x1D: cycles = this->RR_r(this->regHL.low); break;
        case 0x1E: cycles = this->RR_HL(); break;
        case 0x1F: cycles = this->RR_r(this->regAF.high); break;

        // SLA r
        case 0x20: cycles = this->SLA_r(this->regBC.high); break;
        case 0x21: cycles = this->SLA_r(this->regBC.low); break;
        case 0x22: cycles = this->SLA_r(this->regDE.high); break;
        case 0x23: cycles = this->SLA_r(this->regDE.low); break;
        case 0x24: cycles = this->SLA_r(this->regHL.high); break;
        case 0x25: cycles = this->SLA_r(this->regHL.low); break;
        case 0x26: cycles = this->SLA_HL(); break;
        case 0x27: cycles = this->SLA_r(this->regAF.high); break;

        // SRA r
        case 0x28: cycles = this->SRA_r(this->regBC.high); break;
        case 0x29: cycles = this->SRA_r(this->regBC.low); break;
        case 0x2A: cycles = this->SRA_r(this->regDE.high); break;
        case 0x2B: cycles = this->SRA_r(this->regDE.low); break;
        case 0x2C: cycles = this->SRA_r(this->regHL.high); break;
        case 0x2D: cycles = this->SRA_r(this->regHL.low); break;
        case 0x2E: cycles = this->SRA_HL(); break;
        case 0x2F: cycles = this->SRA_r(this->regAF.high); break;

        // Swap r
        case 0x30: cycles = this->SWAP_r(this->regBC.high); break;
        case 0x31: cycles = this->SWAP_r(this->regBC.low); break;
        case 0x32: cycles = this->SWAP_r(this->regDE.high); break;
        case 0x33: cycles = this->SWAP_r(this->regDE.low); break;
        case 0x34: cycles = this->SWAP_r(this->regHL.high); break;
        case 0x35: cycles = this->SWAP_r(this->regHL.low); break;
        case 0x36: cycles = this->SWAP_HL(); break;
        case 0x37: cycles = this->SWAP_r(this->regAF.high); break;

        // SRL r
        case 0x38: cycles = this->SRL_r(this->regBC.high); break;
        case 0x39: cycles = this->SRL_r(this->regBC.low); break;
        case 0x3A: cycles = this->SRL_r(this->regDE.high); break;
        case 0x3B: cycles = this->SRL_r(this->regDE.low); break;
        case 0x3C: cycles = this->SRL_r(this->regHL.high); break;
        case 0x3D: cycles = this->SRL_r(this->regHL.low); break;
        case 0x3E: cycles = this->SRL_HL(); break;
        case 0x3F: cycles = this->SRL_r(this->regAF.high); break;

        /* 
        ************************************************************************
        Single bit operation commands
        ************************************************************************
        */   

        // BIT 0, r
        case 0x40: cycles = this->BIT_n_r(this->regBC.high, 0); break;
        case 0x41: cycles = this->BIT_n_r(this->regBC.low, 0); break;
        case 0x42: cycles = this->BIT_n_r(this->regDE.high, 0); break;
        case 0x43: cycles = this->BIT_n_r(this->regDE.low, 0); break;
        case 0x44: cycles = this->BIT_n_r(this->regHL.high, 0); break;
        case 0x45: cycles = this->BIT_n_r(this->regHL.low, 0); break;
        case 0x46: cycles = this->BIT_n_HL(0); break;
        case 0x47: cycles = this->BIT_n_r(this->regAF.high, 0); break;

        // BIT 1, r
        case 0x48: cycles = this->BIT_n_r(this->regBC.high, 1); break;
        case 0x49: cycles = this->BIT_n_r(this->regBC.low, 1); break;
        case 0x4A: cycles = this->BIT_n_r(this->regDE.high, 1); break;
        case 0x4B: cycles = this->BIT_n_r(this->regDE.low, 1); break;
        case 0x4C: cycles = this->BIT_n_r(this->regHL.high, 1); break;
        case 0x4D: cycles = this->BIT_n_r(this->regHL.low, 1); break;
        case 0x4E: cycles = this->BIT_n_HL(1); break;
        case 0x4F: cycles = this->BIT_n_r(this->regAF.high, 1); break;

        // BIT 2, r
        case 0x50: cycles = this->BIT_n_r(this->regBC.high, 2); break;
        case 0x51: cycles = this->BIT_n_r(this->regBC.low, 2); break;
        case 0x52: cycles = this->BIT_n_r(this->regDE.high, 2); break;
        case 0x53: cycles = this->BIT_n_r(this->regDE.low, 2); break;
        case 0x54: cycles = this->BIT_n_r(this->regHL.high, 2); break;
        case 0x55: cycles = this->BIT_n_r(this->regHL.low, 2); break;
        case 0x56: cycles = this->BIT_n_HL(2); break;
        case 0x57: cycles = this->BIT_n_r(this->regAF.high, 2); break;

        // BIT 3, r
        case 0x58: cycles = this->BIT_n_r(this->regBC.high, 3); break;
        case 0x59: cycles = this->BIT_n_r(this->regBC.low, 3); break;
        case 0x5A: cycles = this->BIT_n_r(this->regDE.high, 3); break;
        case 0x5B: cycles = this->BIT_n_r(this->regDE.low, 3); break;
        case 0x5C: cycles = this->BIT_n_r(this->regHL.high, 3); break;
        case 0x5D: cycles = this->BIT_n_r(this->regHL.low, 3); break;
        case 0x5E: cycles = this->BIT_n_HL(3); break;
        case 0x5F: cycles = this->BIT_n_r(this->regAF.high, 3); break;

        // BIT 4, r
        case 0x60: cycles = this->BIT_n_r(this->regBC.high, 4); break;
        case 0x61: cycles = this->BIT_n_r(this->regBC.low, 4); break;
        case 0x62: cycles = this->BIT_n_r(this->regDE.high, 4); break;
        case 0x63: cycles = this->BIT_n_r(this->regDE.low, 4); break;
        case 0x64: cycles = this->BIT_n_r(this->regHL.high, 4); break;
        case 0x65: cycles = this->BIT_n_r(this->regHL.low, 4); break;
        case 0x66: cycles = this->BIT_n_HL(4); break;
        case 0x67: cycles = this->BIT_n_r(this->regAF.high, 4); break;

        // BIT 5, r
        case 0x68: cycles = this->BIT_n_r(this->regBC.high, 5); break;
        case 0x69: cycles = this->BIT_n_r(this->regBC.low, 5); break;
        case 0x6A: cycles = this->BIT_n_r(this->regDE.high, 5); break;
        case 0x6B: cycles = this->BIT_n_r(this->regDE.low, 5); break;
        case 0x6C: cycles = this->BIT_n_r(this->regHL.high, 5); break;
        case 0x6D: cycles = this->BIT_n_r(this->regHL.low, 5); break;
        case 0x6E: cycles = this->BIT_n_HL(5); break;
        case 0x6F: cycles = this->BIT_n_r(this->regAF.high, 5); break;

        // BIT 6, r
        case 0x70: cycles = this->BIT_n_r(this->regBC.high, 6); break;
        case 0x71: cycles = this->BIT_n_r(this->regBC.low, 6); break;
        case 0x72: cycles = this->BIT_n_r(this->regDE.high, 6); break;
        case 0x73: cycles = this->BIT_n_r(this->regDE.low, 6); break;
        case 0x74: cycles = this->BIT_n_r(this->regHL.high, 6); break;
        case 0x75: cycles = this->BIT_n_r(this->regHL.low, 6); break;
        case 0x76: cycles = this->BIT_n_HL(6); break;
        case 0x77: cycles = this->BIT_n_r(this->regAF.high, 4); break;

        // BIT 7, r
        case 0x78: cycles = this->BIT_n_r(this->regBC.high, 7); break;
        case 0x79: cycles = this->BIT_n_r(this->regBC.low, 7); break;
        case 0x7A: cycles = this->BIT_n_r(this->regDE.high, 7); break;
        case 0x7B: cycles = this->BIT_n_r(this->regDE.low, 7); break;
        case 0x7C: cycles = this->BIT_n_r(this->regHL.high, 7); break;
        case 0x7D: cycles = this->BIT_n_r(this->regHL.low, 7); break;
        case 0x7E: cycles = this->BIT_n_HL(6); break;
        case 0x77F:this->BIT_n_r(this->regAF.high, 7); break;

        // RES 0, r
        case 0x80: cycles = this->RES_n_r(this->regBC.high, 0); break;
        case 0x81: cycles = this->RES_n_r(this->regBC.low, 0); break;
        case 0x82: cycles = this->RES_n_r(this->regDE.high, 0); break;
        case 0x83: cycles = this->RES_n_r(this->regDE.low, 0); break;
        case 0x84: cycles = this->RES_n_r(this->regHL.high, 0); break;
        case 0x85: cycles = this->RES_n_r(this->regHL.low, 0); break;
        case 0x86: cycles = this->RES_n_HL(0); break;
        case 0x87: cycles = this->RES_n_r(this->regAF.high, 0); break;

        // RES 1, r
        case 0x88: cycles = this->RES_n_r(this->regBC.high, 1); break;
        case 0x89: cycles = this->RES_n_r(this->regBC.low, 1); break;
        case 0x8A: cycles = this->RES_n_r(this->regDE.high, 1); break;
        case 0x8B: cycles = this->RES_n_r(this->regDE.low, 1); break;
        case 0x8C: cycles = this->RES_n_r(this->regHL.high, 1); break;
        case 0x8D: cycles = this->RES_n_r(this->regHL.low, 1); break;
        case 0x8E: cycles = this->RES_n_HL(1); break;
        case 0x8F: cycles = this->RES_n_r(this->regAF.high, 1); break;

        // RES 2, r
        case 0x90: cycles = this->RES_n_r(this->regBC.high, 2); break;
        case 0x91: cycles = this->RES_n_r(this->regBC.low, 2); break;
        case 0x92: cycles = this->RES_n_r(this->regDE.high, 2); break;
        case 0x93: cycles = this->RES_n_r(this->regDE.low, 2); break;
        case 0x94: cycles = this->RES_n_r(this->regHL.high, 2); break;
        case 0x95: cycles = this->RES_n_r(this->regHL.low, 2); break;
        case 0x96: cycles = this->RES_n_HL(2); break;
        case 0x97: cycles = this->RES_n_r(this->regAF.high, 2); break;

        // RES 3, r
        case 0x98: cycles = this->RES_n_r(this->regBC.high, 3); break;
        case 0x99: cycles = this->RES_n_r(this->regBC.low, 3); break;
        case 0x9A: cycles = this->RES_n_r(this->regDE.high, 3); break;
        case 0x9B: cycles = this->RES_n_r(this->regDE.low, 3); break;
        case 0x9C: cycles = this->RES_n_r(this->regHL.high, 3); break;
        case 0x9D: cycles = this->RES_n_r(this->regHL.low, 3); break;
        case 0x9E: cycles = this->RES_n_HL(3); break;
        case 0x9F: cycles = this->RES_n_r(this->regAF.high, 3); break;

        // RES 4, r
        case 0xA0: cycles = this->RES_n_r(this->regBC.high, 4); break;
        case 0xA1: cycles = this->RES_n_r(this->regBC.low, 4); break;
        case 0xA2: cycles = this->RES_n_r(this->regDE.high, 4); break;
        case 0xA3: cycles = this->RES_n_r(this->regDE.low, 4); break;
        case 0xA4: cycles = this->RES_n_r(this->regHL.high, 4); break;
        case 0xA5: cycles = this->RES_n_r(this->regHL.low, 4); break;
        case 0xA6: cycles = this->RES_n_HL(4); break;
        case 0xA7: cycles = this->RES_n_r(this->regAF.high, 4); break;

        // RES 5, r
        case 0xA8: cycles = this->RES_n_r(this->regBC.high, 5); break;
        case 0xA9: cycles = this->RES_n_r(this->regBC.low, 5); break;
        case 0xAA: cycles = this->RES_n_r(this->regDE.high, 5); break;
        case 0xAB: cycles = this->RES_n_r(this->regDE.low, 5); break;
        case 0xAC: cycles = this->RES_n_r(this->regHL.high, 5); break;
        case 0xAD: cycles = this->RES_n_r(this->regHL.low, 5); break;
        case 0xAE: cycles = this->RES_n_HL(5); break;
        case 0xAF: cycles = this->RES_n_r(this->regAF.high, 5); break;

        // RES 6, r
        case 0xB0: cycles = this->RES_n_r(this->regBC.high, 6); break;
        case 0xB1: cycles = this->RES_n_r(this->regBC.low, 6); break;
        case 0xB2: cycles = this->RES_n_r(this->regDE.high, 6); break;
        case 0xB3: cycles = this->RES_n_r(this->regDE.low, 6); break;
        case 0xB4: cycles = this->RES_n_r(this->regHL.high, 6); break;
        case 0xB5: cycles = this->RES_n_r(this->regHL.low, 6); break;
        case 0xB6: cycles = this->RES_n_HL(6); break;
        case 0xB7: cycles = this->RES_n_r(this->regAF.high, 4); break;

        // RES 7, r
        case 0xB8: cycles = this->RES_n_r(this->regBC.high, 7); break;
        case 0xB9: cycles = this->RES_n_r(this->regBC.low, 7); break;
        case 0xBA: cycles = this->RES_n_r(this->regDE.high, 7); break;
        case 0xBB: cycles = this->RES_n_r(this->regDE.low, 7); break;
        case 0xBC: cycles = this->RES_n_r(this->regHL.high, 7); break;
        case 0xBD: cycles = this->RES_n_r(this->regHL.low, 7); break;
        case 0xBE: cycles = this->RES_n_HL(6); break;
        case 0xB7F:this->RES_n_r(this->regAF.high, 7); break;

        // SET 0, r
        case 0xC0: cycles = this->SET_n_r(this->regBC.high, 0); break;
        case 0xC1: cycles = this->SET_n_r(this->regBC.low, 0); break;
        case 0xC2: cycles = this->SET_n_r(this->regDE.high, 0); break;
        case 0xC3: cycles = this->SET_n_r(this->regDE.low, 0); break;
        case 0xC4: cycles = this->SET_n_r(this->regHL.high, 0); break;
        case 0xC5: cycles = this->SET_n_r(this->regHL.low, 0); break;
        case 0xC6: cycles = this->SET_n_HL(0); break;
        case 0xC7: cycles = this->SET_n_r(this->regAF.high, 0); break;

        // SET 1, r
        case 0xC8: cycles = this->SET_n_r(this->regBC.high, 1); break;
        case 0xC9: cycles = this->SET_n_r(this->regBC.low, 1); break;
        case 0xCA: cycles = this->SET_n_r(this->regDE.high, 1); break;
        case 0xCB: cycles = this->SET_n_r(this->regDE.low, 1); break;
        case 0xCC: cycles = this->SET_n_r(this->regHL.high, 1); break;
        case 0xCD: cycles = this->SET_n_r(this->regHL.low, 1); break;
        case 0xCE: cycles = this->SET_n_HL(1); break;
        case 0xCF: cycles = this->SET_n_r(this->regAF.high, 1); break;

        // SET 2, r
        case 0xD0: cycles = this->SET_n_r(this->regBC.high, 2); break;
        case 0xD1: cycles = this->SET_n_r(this->regBC.low, 2); break;
        case 0xD2: cycles = this->SET_n_r(this->regDE.high, 2); break;
        case 0xD3: cycles = this->SET_n_r(this->regDE.low, 2); break;
        case 0xD4: cycles = this->SET_n_r(this->regHL.high, 2); break;
        case 0xD5: cycles = this->SET_n_r(this->regHL.low, 2); break;
        case 0xD6: cycles = this->SET_n_HL(2); break;
        case 0xD7: cycles = this->SET_n_r(this->regAF.high, 2); break;

        // SET 3, r
        case 0xD8: cycles = this->SET_n_r(this->regBC.high, 3); break;
        case 0xD9: cycles = this->SET_n_r(this->regBC.low, 3); break;
        case 0xDA: cycles = this->SET_n_r(this->regDE.high, 3); break;
        case 0xDB: cycles = this->SET_n_r(this->regDE.low, 3); break;
        case 0xDC: cycles = this->SET_n_r(this->regHL.high, 3); break;
        case 0xDD: cycles = this->SET_n_r(this->regHL.low, 3); break;
        case 0xDE: cycles = this->SET_n_HL(3); break;
        case 0xDF: cycles = this->SET_n_r(this->regAF.high, 3); break;

        // SET 4, r
        case 0xE0: cycles = this->SET_n_r(this->regBC.high, 4); break;
        case 0xE1: cycles = this->SET_n_r(this->regBC.low, 4); break;
        case 0xE2: cycles = this->SET_n_r(this->regDE.high, 4); break;
        case 0xE3: cycles = this->SET_n_r(this->regDE.low, 4); break;
        case 0xE4: cycles = this->SET_n_r(this->regHL.high, 4); break;
        case 0xE5: cycles = this->SET_n_r(this->regHL.low, 4); break;
        case 0xE6: cycles = this->SET_n_HL(4); break;
        case 0xE7: cycles = this->SET_n_r(this->regAF.high, 4); break;

        // SET 5, r
        case 0xE8: cycles = this->SET_n_r(this->regBC.high, 5); break;
        case 0xE9: cycles = this->SET_n_r(this->regBC.low, 5); break;
        case 0xEA: cycles = this->SET_n_r(this->regDE.high, 5); break;
        case 0xEB: cycles = this->SET_n_r(this->regDE.low, 5); break;
        case 0xEC: cycles = this->SET_n_r(this->regHL.high, 5); break;
        case 0xED: cycles = this->SET_n_r(this->regHL.low, 5); break;
        case 0xEE: cycles = this->SET_n_HL(5); break;
        case 0xEF: cycles = this->SET_n_r(this->regAF.high, 5); break;

        // SET 6, r
        case 0xF0: cycles = this->SET_n_r(this->regBC.high, 6); break;
        case 0xF1: cycles = this->SET_n_r(this->regBC.low, 6); break;
        case 0xF2: cycles = this->SET_n_r(this->regDE.high, 6); break;
        case 0xF3: cycles = this->SET_n_r(this->regDE.low, 6); break;
        case 0xF4: cycles = this->SET_n_r(this->regHL.high, 6); break;
        case 0xF5: cycles = this->SET_n_r(this->regHL.low, 6); break;
        case 0xF6: cycles = this->SET_n_HL(6); break;
        case 0xF7: cycles = this->SET_n_r(this->regAF.high, 4); break;

        // SET 7, r
        case 0xF8: cycles = this->SET_n_r(this->regBC.high, 7); break;
        case 0xF9: cycles = this->SET_n_r(this->regBC.low, 7); break;
        case 0xFA: cycles = this->SET_n_r(this->regDE.high, 7); break;
        case 0xFB: cycles = this->SET_n_r(this->regDE.low, 7); break;
        case 0xFC: cycles = this->SET_n_r(this->regHL.high, 7); break;
        case 0xFD: cycles = this->SET_n_r(this->regHL.low, 7); break;
        case 0xFE: cycles = this->SET_n_HL(6); break;
        case 0xFF: cycles = this->SET_n_r(this->regAF.high, 7); break;
    }

    return cycles;

}


/*
********************************************************************************
MEMORY MANAGEMENT FUNCTIONS
********************************************************************************
*/

BYTE Emulator::readMem(WORD address) const {

    // If reading from switchable ROM banking area
    if ((address >= 0x4000) && (address <= 0x7FFF)) {
        WORD newAddress = (this->currentROMBank * 0x4000) + (address - 0x4000);
        return this->cartridgeMem[newAddress];
    } 
    
    // If reading from the switchable RAM banking area
    else if ((address >= 0xA000) && (address <= 0xBFFF)) {
        WORD newAddress = (this->currentRAMBank * 0x2000) + (address - 0xA000);
        return this->cartridgeMem[newAddress];
    }
    // Joypad Register
    else if (address == 0xFF00) { 
        return this->getJoypadState();
    }

    // else return what's in the memory
    return this->cartridgeMem[address];

}

void Emulator::writeMem(WORD address, BYTE data) {

    // write attempts to ROM
    if (address < 0x8000) {
        this->handleBanking(address, data);
    }

    // write attempts to RAM
    else if ((address >= 0xA000) && (address <= 0xBFFF)) {
        if (this->enableRAM) {
            WORD newAddress = (address - 0xA000) + (this->currentRAMBank * 0x2000);
            this->RAMBanks[newAddress] = data;
        }
    }

    // writing to Echo RAM also writes to work RAM
    else if ((address >= 0xE000) && (address <= 0xFDFF)) {
        this->internalMem[address] = data;
        this->writeMem(address - 0x2000, data);
    }

    else if ((address >= 0xFEA0) && (address <= 0xFEFF)) {
        cout << "Something wrong in WriteMem. Unusable location." << endl;
    }

    // FF04 is divider register, its value is reset to 0 if game attempts to 
    // write to it
    else if (address == DIVIDER) { 
        this->internalMem[DIVIDER] = 0;
    }

    // if game changes the freq, the counter must change accordingly
    else if (address == TAC) { 
        // get the currentFreq, do the writing, then compare with newFreq. if different, counter must be updated

        // to extract bit 1 and 0 of timer controller register
        BYTE currentFreq = this->readMem(TAC) & 0x3; 
        this->internalMem[TAC] = data; // write the data to the address
        BYTE newFreq = this->readMem(TAC) & 0x3;

        // if the freq has changed
        if (currentFreq != newFreq) { 
            switch (newFreq) {
                case 0b00: 
                    this->timerCounter = 1024; 
                    this->timerUpdateConstant = 1024; 
                    break; // 4096Hz
                case 0b01: 
                    this->timerCounter = 16;
                    this->timerUpdateConstant = 16;
                    break; // 262144Hz
                case 0b10: 
                    this->timerCounter = 64; 
                    this->timerUpdateConstant = 64;
                    break; // 65536Hz
                case 0b11: 
                    this->timerCounter = 256; 
                    this->timerUpdateConstant = 256; 
                    break; // 16384Hz
                default:
                    cout << "Something is wrong!" << endl;
            }
        }
    }

    // reset the current scanline to 0 if game tries to write to it
    else if (address == 0xFF44) {
        this->internalMem[address] = 0;
    }
    
    // launches a DMA to access the Sprites Attributes table
    else if (address == 0xFF46) {
        this->doDMATransfer(data);
    }

    else {
        this->internalMem[address] = data;
    }

}

void Emulator::handleBanking(WORD address, BYTE data) {
    // do RAM enabling
    if (address < 0x2000) {
        this->doRAMBankEnable(address, data);
    }

    // do ROM bank change
    else if ((address >= 0x2000) && (address <= 0x3FFF)) {
        if (this->MBC1) this->doChangeLoROMBank(data);
        // if MBC2, LSB of upper address byte must be 1 to select ROM bank
        else if (this->isBitSet(address, 8)) this->doChangeLoROMBank(data);
    }

    // do ROM or RAM bank change
    else if ((address >= 0x4000) && (address <= 0x5FFF)) {
        if (this->MBC1) {
            if (this->ROMBanking) {
                this->doChangeHiROMBank(data);
            } else {
                this->doRAMBankChange(data);
            }
        }
    }

    // this changes whether we are doing ROM banking
    // or RAM banking with the above if statement
    else if ((address >= 0x6000) && (address <= 0x7FFF)) {
        if (this->MBC1) {
            this->doChangeROMRAMMode(data);
        }
    }

}

void Emulator::doRAMBankEnable(WORD address, BYTE data) {

    // for MBC2, LSB of upper byte of address must be 0 to do enable
    if (this->MBC2) {
        if (this->isBitSet(address, 8)) return;
    }

    BYTE testData = data & 0xF;
    if (testData == 0xA) {
        this->enableRAM = true;
    } else if (testData == 0x0) {
        this->enableRAM = false;
    }

}

void Emulator::doChangeLoROMBank(BYTE data) {
    
    // if MBC2, current ROM bank is lower nibble of data
    if (this->MBC2) {
        this->currentROMBank = data & 0xF;
        if (this->currentROMBank == 0x0) this->currentROMBank = 0x1;
        return;
    }

    BYTE lower5bits = data & 0x1F;
    
    // if lower5bits == 0x0, gameboy automatically sets it to 0x1 as ROM 0 can 
    // always be accessed from 0x0000-3FFF
    if (lower5bits == 0x0) lower5bits = 0x1;
    
    this->currentROMBank &= 0xE0; // mask the last 5 bits to 0
    this->currentROMBank |= lower5bits; // match last 5 bits to lower5bits

}

void Emulator::doChangeHiROMBank(BYTE data) {

    // change bit 6-5 of currentROMBank to bit 6-5 of data

    // turn off the upper 3 bits of the current rom (since bit 7 must == 0)
    this->currentROMBank &= 0x1F;

    data &= 0xE0; // turn off the lower 5 bits of the data
    this->currentROMBank |= data; // match higher 3 bits of data

    // to make sure bit 7 == 0? might cause error here, might just only read 
    // first 7 bits from the 8 bit address to find which ROM bank to use. not 
    // sure, please check!
    assert(((this->currentROMBank >> 7) & 0x1) == 0x0);

}

void Emulator::doRAMBankChange(BYTE data) {
    // only 4 RAM banks to choose from, 0x0-3
    this->currentRAMBank = data & 0x3;
}

void Emulator::doChangeROMRAMMode(BYTE data) {
    
    // ROM banking mode: 0x0
    // RAM banking mode: 0x1
    BYTE newData = data & 0x1;
    this->ROMBanking = newData == 0x0;
    
    // The program may freely switch between both modes, the only limitiation 
    // is that only RAM Bank 00h can be used during Mode 0, and only ROM Banks 
    // 00-1Fh can be used during Mode 1.
    if (this->ROMBanking) {
        this->currentRAMBank = 0x0;
    }
}

/*
********************************************************************************
INTERRUPT FUNCTIONS
********************************************************************************
*/

/*

Within the main emulation update loop, interrupts can be flagged (the 4 different interrupts being emulated).

Interrupt Master Enabled switch - bool turned on and off by CPU instructions EI & DI (Enable/Disable Interrupts)

Interrupt Request Register - 0xFF0F
Interrupt Enabled Register - 0xFFFF

After interrupts are flagged, interrupts are handled at the end of the loop.
While handling interrupts, for any flagged interrupts, they will be triggered.

*/

void Emulator::flagInterrupt(int interruptID) { 
    BYTE requestReg = this->readMem(0xFF0F);
    requestReg = this->bitSet(requestReg, interruptID); // Set the corresponding bit in the interrupt req register 0xFF0F

    // If halted, wake up
    this->isHalted = false;

    this->writeMem(0xFF0F, requestReg); // Update the request register;
}

void Emulator::handleInterrupts() {
    if (InterruptMasterEnabled) { // Check if the IME switch is true
        BYTE requestReg = this->readMem(0xFF0F);
        BYTE enabledReg = this->readMem(0xFFFF);

        if ((requestReg & enabledReg) > 0) { // If there are any valid interrupt requests enabled
            this->InterruptMasterEnabled = false; // Disable further interrupts
            
            this->stackPointer.regstr--;
            this->writeMem(this->stackPointer.regstr, this->programCounter.high);
            this->stackPointer.regstr--;
            this->writeMem(this->stackPointer.regstr, this->programCounter.low);
            // Saves current PC to SP, SP is now pointing at bottom of PC. Need to increment SP by 2 when returning

            for (int i = 0; i < 5; i++) { // Go through the bits and service the flagged interrupts
                bool isFlagged = this->isBitSet(requestReg, i);
                bool isEnabled = this->isBitSet(enabledReg, i);
                if (isFlagged && isEnabled) { // If n-th bit is flagged and enabled, trigger the corresponding interrupt
                    triggerInterrupt(i);
                }
            }
        }
    }
}

void Emulator::triggerInterrupt(int interruptID) {
    BYTE requestReg = readMem(0xFF0F);
    requestReg = this->bitReset(requestReg, interruptID); // Resetting the n-th bit
    this->writeMem(0xFF0F, requestReg); 
    switch (interruptID) {
        case 0 : // V-Blank
            this->programCounter.regstr = 0x40;
            break;
        case 1 : // LCD
            this->programCounter.regstr = 0x48;
            break;
        case 2 : // Timer
            this->programCounter.regstr = 0x50;
            break;
        case 4 : // Joypad
            this->programCounter.regstr = 0x60;
            break;
    }
}

/*
********************************************************************************
TIMER UPDATE FUNCTIONS
********************************************************************************
*/

void Emulator::updateTimers(int cycles) {

    /*

    Cycles -> Length of time that the instructions take to execute
        Too short to meaningfully use seconds, so use cycles instead
        Multiples of 4
    
    FF04 Divider Register
    FF05 Timer Counter TIMA
    FF06 Timer Modulo TMA
    FF07 Timer Control TAC/TMC

    FF07 Timer control -> 3 bit register, | 2 1 0 |
    Bit 2 -> whether timer is enabled (1)
    Bit 1 and 0:
    00 -> 4096Hz (1024 counter)
    01 -> 262144Hz (16 counter)
    10 -> 65536Hz (64 counter)
    11 -> 16384Hz (256 counter)


    Divider is incremented at 16384Hz, writing any value to this register resets it to 0x00

    FF05 TIMA is incremented at a freq specified by FF07 TAC. If TIMA overflows (>0xFF), trigger an interrupt 
        and reset it to the value specified by FF06 TMA

    By default, TIMA should increment at 4096HZ (4096 times per second). 
    So, TIMA works as a timer (to keep track of time, to emulate/represent the passage of time basically).

    The CPU clock speed is 4194304Hz, which to my understanding can just be interpreted as 4194304 cycles / second.
    Peg the increment of TIMA to that, and we should increment TIMA every 4194304/4096 = 1024 cycles.

    In main emulation update loop (which is run at 60Hz, not relevant to timer?), an opcode is executed.
        The execution takes a certain number of cycles (4, 8, 12, 16 etc.)
        Within updateTimer, keep a counter of cycles elapsed.
        If the number of cycles reaches 1024 (or whatever it should be depending on the timer control),
            update the timer registers accordingly.
            Else, just updateTimers does nothing (?).

    Divider Register is special in that it is independent from the other Timer registers. 
    It is always incremented at 16384Hz, and when it overflows, it is set back to 0.

    Consider defining DIVIDER TIMA TMA TAC as 0xFF04 0xFF05 0xFF06 0xFF07 respectively
    */

    this->dividerCounter += cycles; // TO DECLARE SOMEWHERE
    // Handle divider register first

    if (this->dividerCounter >= 256) {
       this->dividerCounter = 0; // reset it to start counting for upcoming cycles
       this->internalMem[DIVIDER]++; // directly modifying instead of using writeMem
    }

    if (this->clockEnabled()) {

        this->timerCounter -= cycles; // TO DECLARE SOMEWHERE. DECLARE timerUpdateConstant AS WELL!!!
        // Decrement counter instead of increment so just need to keep track if <= 0

        if (this->timerCounter <= 0) { // To increment TIMA

            // Reset counter to prep for next update
            this->timerCounter = this->timerUpdateConstant; 

            // TIMA is at 255, about to overflow
            if (this->readMem(TIMA) == 0xFF) { 
                this->writeMem(TIMA, this->readMem(TMA)); // set value of TIMA to value of TMA
                
                this->flagInterrupt(2); // The interrupt flagged is corresponded to bit 2 of interrupt register
                
            } else {
                this->writeMem(TIMA, this->readMem(TIMA) + 1); // TIMA is incremented by 1
            }

        }

    }

}

bool Emulator::clockEnabled() {
    // Bit 2 of TAC specifies whether timer is enabled(1) or disabled(0)
    return this->isBitSet(this->readMem(TAC), 2);
}

/*
********************************************************************************
JOYPAD
********************************************************************************
*/

/*

There are 8 buttons on the Gameboy, so the joypad state can be neatly represented in a BYTE.
However, the joypad register uses only 6 bits, which complicates things a little.

JOYPAD REGISTER
                    7
                    6
      |-------------5
      |        |----4
Start | Down   |    3
Select| Up     |    2
B     | Left   |    1
A     | Right  |    0

Bits 4 and 5 are used to represent which buttons (directional or normal buttons), while 0-3 are the buttons themselves.
For the joypad, it's default state is 1 (not pressed), and turns to 0 when it is pressed.

When we readMem the joypad register,

SDLK_a : key = 4 (A)
SDLK_s : key = 5 (B)
SDLK_RETURN : key = 7 (Start)
SDLK_SPACE : key = 6 (Select)
SDLK_RIGHT : key = 0 
SDLK_LEFT : key = 1 
SDLK_UP : key = 2 
SDLK_DOWN : key = 3 

In handling key presses and releases, directly modifying the register is difficult 
due to the convoluted representation. Instead, we can keep a BYTE joypadState which
represents the state of all the buttons neatly.

Start   Select  A   B   Down    Up  Left    Right   Keys
7       6       5   4   3       2   1       0       Bits

When the game reads the joypad register, we can return it by deriving it from
joypadState and the keys the game is requesting (depending on 0xFF00 bit 4 and 5).

When a button is pressed (from 1 to 0), an interrupt will be flagged.

 */

// Called when handling input, modifying joypadState instead of joypad register
void Emulator::buttonPressed(int key) {
    
    bool previouslyUnpressed = false; // Keeps track if the button was previously unpressed
    bool flagIntrpt = false; // Keeps track if interrupt should be flagged

    if (this->isBitSet(this->joypadState, key)) { // If the key was set at 1 (unpressed)
        previouslyUnpressed = true;
    }

    this->joypadState = this->bitReset(this->joypadState, key); // Set the key to 0 (pressed)

    // Now, determine if the key is directional or normal button

    bool directionalButton = key < 4;

    // flagIntrpt is true if previouslyUnpressed AND bit 4 and 5 of joypad register
    // corresponds to the directionalButton bool.
    //  => Bit 4 (dxn) of joypad register is 0 (on) and directionalButton is true
    //  => Bit 5 (normal) of joypad register is 0, and directionalButton is false

    BYTE joypadReg = internalMem[0xFF00];

    if ((!this->isBitSet(joypadReg, 4) && directionalButton) || 
            (!this->isBitSet(joypadReg, 5) && !directionalButton)) {
                if (previouslyUnpressed) {
                    flagIntrpt = true;
                }
            }
    
    if (flagIntrpt) {
        flagInterrupt(4);
    }
}

void Emulator::buttonReleased(int key) {
    this->joypadState = this->bitSet(this->joypadState, key);
}

BYTE Emulator::getJoypadState() const {
    BYTE joypadReg = this->internalMem[0xFF00];
    joypadReg &= 0xF0; // Sets bits 0-3 to 0;

    // If program requests for directional buttons
    if (!this->isBitSet(joypadReg, 4)) {
        BYTE directionals = this->joypadState & 0x0F; // Sets bits 4-7 to 0
        joypadReg |= directionals;
    }

    // If program requests for normal buttons
    else if (!this->isBitSet(joypadReg, 5)) {
        BYTE normalButtons = this->joypadState >> 4;
        joypadReg |= normalButtons;
    }

    return joypadReg;
}


/*
********************************************************************************
GRAPHICS
********************************************************************************
*/

/*

LCD Controller operates at 2^22 Hz dot clock (pixels drawn per second) which is 
the same as the CPU clock speed. This means each dot takes 1 cycle to be drawn.

An entire frame consists of 154 scanlines
Screen resolution: 160x144 (scanlines 0-143: visible)
10 line vblank (scanlines 144-153: invisible)
Each scanline 456 clock cycles to run -> entire frame 70224 clock cycles

Register 0xFF44 is the current scanline

++ Register 0xFF41 ++
Register 0xFF41 holds current status of LCD controller
The LCD controller has 4 different modes, reflected by bits 1 & 0 of 0xFF41:

00: H-Blank
01: V-Blank
10: Searching Sprites Atts (OAM: Object Attribute Memory)
11: Transfering Data to LCD Driver

Bits 3, 4, and 5 represents mode 0, 1, and 2 interrupts respectively if set:

Bit 3: Mode 0 H-BlankInterrupt enabled
Bit 4: Mode 1 V-Blank Interrupt enabled
Bit 5: Mode 2 OAM Interrupt enabled

Thus, when the LCD mode changes to 0, 1 or 2 and the corresponding bit is set, 
then an LCD interrupt is requested. This is only checked when the mode changes
and not during the duration of these modes.

When LCD is disabled, LCD controller mode must be set to mode 1

Bit 2 is the coincidence flag. It is set to 1 if register 0xFF44 has the same 
value of register 0xFF55 otherwise it is 0.
Bit 6 is the coincidence flag interrupt. Works the same as bits 3, 4 & 5: if
Bit 2 is set and Bit 6 is enabled (set to 1), an LCD interrupt is requested.

Bit 7 is unimplemented
++ End of 0xFF41 ++

++ Register 0xFF40 ++
Register 0xFF40 is the LCD main control register.

Bit 7 - LCD Display Enable             (0=Off, 1=On)
Bit 6 - Window Tile Map Display Select (0=9800-9BFF, 1=9C00-9FFF)
Bit 5 - Window Display Enable          (0=Off, 1=On)
Bit 4 - BG & Window Tile Data Select   (0=8800-97FF, 1=8000-8FFF)
Bit 3 - BG Tile Map Display Select     (0=9800-9BFF, 1=9C00-9FFF)
Bit 2 - OBJ (Sprite) Size              (0=8x8, 1=8x16)
Bit 1 - OBJ (Sprite) Display Enable    (0=Off, 1=On)
Bit 0 - BG/Window Display/Priority     (0=Off, 1=On)
++ End of 0xFF40 ++

Difference between Tile Data & Tile Map:
Tile Data is the the information that describes what the tile looks like and is 
stored in VRAM area $8000-97FF. Tile Map, on the other hand, refers to the 
32 X 32 background tile mapping that the gameboy screen can possibly show. The 
actual resolution of the gameboy screen is 256x256 pixels (32x32 tiles), but 
can only show up to 160x144 pixels of viewing area on this 256x256 background. 
The Tile Map contains the number of the tiles going to be displayed, and this 
number is used to retrieve its data from the Tile Data memory location. It is 
literally the tile number, so you take the number, multiply it by the size of 
each tile in the memory, and add it to the base address of where the tile data 
is located at to obtain the tile data information.

Tile Data is stored in VRAM at addresses $8000-97FF; with one tile being 16 
bytes large, this area defines data for 384 tiles.

There are three "blocks" of 128 tiles each:
- Block 0 is $8000-87FF
- Block 1 is $8800-8FFF
- Block 2 is $9000-97FF

Tiles are always indexed using a 8-bit integer, but the addressing method may 
differ. The "8000 method" uses $8000 as its base pointer and uses an unsigned 
addressing, meaning that tiles 0-127 are in block 0, and tiles 128-255 are in 
block 1. The "8800 method" uses $9000 as its base pointer and uses a signed 
addressing. To put it differently, "8000 addressing" takes tiles 0-127 from 
block 0 and tiles 128-255 from block 1, whereas "8800 addressing" takes tiles 
0-127 from block 2 and tiles 128-255 from block 1. (You can notice that block 1 
is shared by both addressing methods)

Sprites always use 8000 addressing, but the BG and Window can use either mode, 
controlled by LCDC bit 4.

Sprites are located in VRAM at address 0x8000-8FFF. Sprite attributes are 
located in the Sprite Attribute Table (OAM - Object Attribute Memory) at 
0xFE00-FE9F. The Gameboy video controller can display up to 40 sprites, with 
each sprite taking up 4 bytes in the OAM.

*/

void Emulator::updateGraphics(int cycles) {

    this->setLCDStatus();

    if (LCDEnabled()) {
        this->scanlineCycleCount -= cycles;
    } else {
        return;
    }

    // move onto the next scanline
    if (this->scanlineCycleCount <= 0) {
        // need to update directly since gameboy will always reset scanline to 0
        // if attempting to write to 0xFF44 in memory
        this->internalMem[0xFF44]++;
        BYTE currentLine = this->readMem(0xFF44);

        this->scanlineCycleCount = 456;

        // encountered vblank period
        if (currentLine == 144) {
            this->renderGraphics();
            this->flagInterrupt(0);
        }

        // if gone past scanline 153 reset to 0
        else if (currentLine > 153) {
            this->internalMem[0xFF44] = 0;
        }

        // draw the current scanline
        else if (currentLine < 144) {
            this->drawScanLine();
        }
    }

}

void Emulator::setLCDStatus() {

    BYTE status = this->readMem(0xFF41);

    if (!LCDEnabled()) {
        // set the mode to 1 (vblank) during lcd disabled and reset scanline
        this->scanlineCycleCount = 456;
        this->internalMem[0xFF44] = 0;
        // set last 2 bits of status to 01
        status = this->bitSet(status, 0);
        status = this->bitReset(status, 1);

        this->writeMem(0xFF41, status);
        return;
    }

    BYTE currentLine = this->readMem(0xFF44);
    BYTE currentMode = status & 0x3;

    BYTE newMode = 0;
    bool needInterrupt = false;

    // in vblank so set mode to 1
    if (currentLine >= 144) {
        newMode = 1;
        // set last 2 bits of status to 01
        status = this->bitSet(status, 0);
        status = this->bitReset(status, 1);
        // check if vblank interrupt (bit 4) is enabled
        needInterrupt = this->isBitSet(status, 4);
    } else  {
        
        /*
        LCD controller cycles through modes 2, 3 & 0
        Mode 2 lasts roughly 80 cycles
        Mode 3 lasts roughly 172 cycles
        Mode 0 takes up the remaining cycles

        Each time a new mode is entered (except mode 3), an interrupt will be 
        called
        */

        // mode 2: 456 - 80 = 376
        if (this->scanlineCycleCount > 376) {
            newMode = 2;
            // set last 2 bits of status to 10
            status = this->bitReset(status, 0);
            status = this->bitSet(status, 1);
            // check if OAM interrupt (bit 5) is enabled
            needInterrupt = this->isBitSet(status, 5);
        }

        // mode 3: 376 - 172 = 204
        else if (this->scanlineCycleCount > 204) {
            newMode = 3;
            // set last 2 bits of status to 11
            status = this->bitSet(status, 0);
            status = this->bitSet(status, 1);
        }

        // mode 0
        else {
            newMode = 0;
            // set last 2 bits of status to 00
            status = this->bitReset(status, 0);
            status = this->bitReset(status, 1);
            // check if hblank interrupt (bit 3) is enabled
            needInterrupt = this->isBitSet(status, 3);
        }
    }

    // if a new mode is entered, request interrupt
    if (needInterrupt && (newMode != currentMode)) {
        this->flagInterrupt(1);
    }

    // check for the coincidence flag
    if (currentLine == this->readMem(0xFF45)) {
        // set coincidence flag (bit 2) to 1
        status = this->bitSet(status, 2);
        // check if coincidence flag interrupt (bit 6) is enabled
        if (this->isBitSet(status, 6)) {
            this->flagInterrupt(1);
        }
    } else {
        // set coincidence flag (bit 2) to 0
        status = this->bitReset(status, 2);
    }

    this->writeMem(0xFF41, status);

}

bool Emulator::LCDEnabled() {
    return this->isBitSet(this->readMem(0xFF40), 7);
}

void Emulator::drawScanLine() {
    BYTE lcdControl = this->readMem(0xFF40);

    // Draw only if LCD is enabled
    if (this->LCDEnabled()) {
        if (this->isBitSet(lcdControl, 0)) {
            this->renderTiles(lcdControl);
        }

        if (this->isBitSet(lcdControl, 1)) {
            this->renderSprites(lcdControl);
        }
    }
}

void Emulator::renderTiles(BYTE lcdControl) {

    /*
    Steps to render tiles:
    1. Find out tile identifier number from background tile map
    2. Using the tile identifier number, get the tile data from VRAM
    3. Using the tile data, draw out the tile
    */

    // Get coordinates of viewport
    BYTE scrollY = this->readMem(0xFF42);
    BYTE scrollX = this->readMem(0xFF43);
    BYTE windowY = this->readMem(0xFF4A);
    BYTE windowX = this->readMem(0xFF4B) - 7;

    // Check if window is enabled and if current scanline is within windowY
    bool usingWindow = false;
    if (this->isBitSet(lcdControl, 5) && (windowY <= this->readMem(0xFF44))) {
        usingWindow = true;
    }

    // Get Tile Data location & addressing mode
    WORD tileDataLocation;
    bool unsignedAddressing;

    if (this->isBitSet(lcdControl, 4)) {
        // location: 0x8000-8FFF
        tileDataLocation = 0x8000;
        unsignedAddressing = true;
    } else {
        // location: 0x8800-97FF
        tileDataLocation = 0x8800;
        unsignedAddressing = false;
    }

    // Get background Tile Map location
    WORD tileMapLocation;
    if (!usingWindow) {
        if (this->isBitSet(lcdControl, 3)) {
            tileMapLocation = 0x9C00;
        } else {
            tileMapLocation = 0x9800;
        }
    } else {
        if (this->isBitSet(lcdControl, 3)) {
            tileMapLocation = 0x9C00;
        } else {
            tileMapLocation = 0x9800;
        }
    }

    // Get tile row the "offset" for the current line of pixels in the tile
    BYTE tileY;
    BYTE tileYOffset;
    if (!usingWindow) {
        tileY = (BYTE)(((scrollY + this->readMem(0xFF44)) / 8) % 32);
        tileYOffset = (BYTE)((scrollY + this->readMem(0xFF04)) % 8);
    } else {
        // POSSIBLE BUG: need to % 32 to wrap???
        // Or because window is not scrollable and always display from top left?
        tileY = (BYTE)((this->readMem(0xFF44) - windowY) / 8);
        tileYOffset = (BYTE)((this->readMem(0xFF04) - windowY) % 8);
    }

    // For loop to draw the current line of pixels
    for (int pixel = 0; pixel < 160; pixel++) {

        // Get tile x
        BYTE tileX = (BYTE)(((scrollX + pixel) / 8) % 32);

        if (usingWindow && (pixel >= windowX)) {
            // This suggests that the window tile map only ever starts 
            // displaying from the left
            tileX = (BYTE)((pixel - windowX) / 8);
        }

        // Calculate tile identifier number from tileY & tileX
        BYTE tileNum = this->readMem(tileMapLocation + (tileY*32) + tileX);
        
        // Get tile data address
        WORD tileDataAddress;
        if (unsignedAddressing) {
            // Tile number is unsigned and each tile is 16 bytes
            tileDataAddress = tileDataLocation + (tileNum * 16);
        } else {
            // Tile number is signed and each tile is 16 bytes
            tileDataAddress = tileDataLocation + (static_cast<SIGNED_BYTE>(tileNum) * 16);
            // tileDataAddress here is in the region 0x8800-97FF
            assert(tileDataAddress >= 0x8800 == true);
        }

        // Each line is 2 bytes long, to get the current line, add the offset
        tileDataAddress += (tileYOffset << 1);

        // Read the 2 bytes of data
        BYTE b1 = this->readMem(tileDataAddress);
        BYTE b2 = this->readMem(tileDataAddress + 1);

        // Figure out the colour palette
        BYTE bit = 7 - ((scrollX + pixel) % 8);
        BYTE colourBit0 = this->isBitSet(b1, bit) ? 0b01 : 0b00;
        BYTE colourBit1 = this->isBitSet(b2, bit) ? 0b10 : 0b00;

        // Get the colour
        COLOUR colour = this->getColour(colourBit1 + colourBit0, 0xFF47);
        
        // Default colour is black where RGB = [0,0,0]
        int red, green, blue; 

        switch (colour) {
            case WHITE: 
                red = 255;
                green = 255;
                blue = 255;
                break;
            case LIGHT_GRAY:
                red = 0xCC;
                green = 0xCC;
                blue = 0xCC;
                break;
            case DARK_GRAY:
                red = 0x77;
                green = 0x77;
                blue = 0x77;
                break;
            default:
                red = green = blue = 0;
                break;
        }

        // Update Screen pixels
        BYTE currentLine = this->readMem(0xFF44);
        // Store in pixel format ARGB8888
        this->displayPixels[pixel + (currentLine * 160)] = (0xFF << 24) | (red << 16) | (green << 8) | blue;

    }

}

COLOUR Emulator::getColour(BYTE colourNum, WORD address) const {

    // Reading colour palette from memory
    BYTE palette = this->readMem(address);
    /*
    Register FF47 contains the colour palette for background. It assigns gray 
    shades to the colour numbers as follows:
    Bit 7-6 - Shade for Color Number 3
    Bit 5-4 - Shade for Color Number 2
    Bit 3-2 - Shade for Color Number 1
    Bit 1-0 - Shade for Color Number 0 */

    // Get the actual colourID
    int mask = 0b11;
    int colourID = (palette >> (colourNum << 1)) & mask;

    // Convert ID into emulator colour
    COLOUR res;
    switch (colourID) {
        case 0b00: res = WHITE; break;
        case 0b01: res = LIGHT_GRAY; break;
        case 0b10: res = DARK_GRAY; break;
        case 0b11: res = BLACK; break;
    }

    return res;

}

void Emulator::renderSprites(BYTE lcdControl) {

    // Check sprite size
    bool use8x16 = false;
    if (this->isBitSet(lcdControl, 2)) {
        use8x16 = true;
    }
    
    // Cycling through the 40 sprites in OAM for draw loop
    for (int sprite = 0; sprite < 40; sprite++) {

        // Sprite occupies 4 bytes in OAM
        // BYTE0: Y position - 16
        // BYTE1: X position - 8
        // BYTE2: Tile identifier number. Used to look up tile pattern in VRAM
        // BYTE3: Sprite attributes
        BYTE index = sprite << 2;
        BYTE yPos = this->readMem(0xFE00 + index) - 16;
        BYTE xPos = this->readMem(0xFE00 + index + 1) - 8;
        BYTE tileNum = this->readMem(0xFE00 + index + 2);
        BYTE attributes = this->readMem(0xFE00 + index + 3);

        bool yFlip = this->isBitSet(attributes, 6);
        bool xFlip = this->isBitSet(attributes, 5);

        int scanLine = this->readMem(0xFF44);

        int ySize = use8x16 ? 16 : 8;

        // Is the current scanline being drawn at the sprite location?
        if ((scanLine >= yPos) && (scanLine < (yPos + ySize))) {

            // Get the offset for the current line being drawn in the tile
            int tileYOffset = scanLine - yPos;

            // Read the sprite backwards in y axis if yFlip == true
            if (yFlip) {
                tileYOffset -= ySize;
                tileYOffset *= -1;
            }

            tileYOffset <<= 1; // since each line is 2 bytes long
            // Get the data address for the current line from the tile number
            WORD lineDataAddress = (0x8000 + (tileNum * 16)) + tileYOffset;

            // Read the 2 bytes of data
            BYTE b1 = this->readMem(lineDataAddress);
            BYTE b2 = this->readMem(lineDataAddress + 1);

            // It is easier to read in from right to left as
            // pixel 0 is bit 7
            // pixel 1 is bit 6...
            for (int tilePixel = 7; tilePixel >= 0; tilePixel--) {
                
                int colourBit = tilePixel;
                
                // read the sprite backwards in x axis if xFlip == true
                if (xFlip) {
                    colourBit -= 7;
                    colourBit *= -1;
                }

                // The rest is the same as in renderTiles
                // Figure out the colour palette
                BYTE colourBit0 = this->isBitSet(b1, colourBit) ? 0b01 : 0b00;
                BYTE colourBit1 = this->isBitSet(b2, colourBit) ? 0b10 : 0b00;

                // Get the colour
                WORD cAddress = this->isBitSet(attributes, 4) ? 0xFF49 : 0xFF48;
                COLOUR colour = this->getColour(colourBit1+colourBit0,cAddress);

                // Default colour is black where RGB = [0,0,0]
                int red, green, blue;

                switch (colour) {
                    case WHITE:
                        // White is transparent for sprites
                        continue;
                    case LIGHT_GRAY:
                        red = 0xCC;
                        green = 0xCC;
                        blue = 0xCC;
                        break;
                    case DARK_GRAY:
                        red = 0x77;
                        green = 0x77;
                        blue = 0x77;
                        break;        
                    default:
                        red = green = blue = 0;
                }

                // Get the pixel to draw
                int pixel = xPos + (0 - tilePixel + 7);

                // check if pixel is hidden behind background
                if (this->isBitSet(attributes, 7)) {

                    if (this->displayPixels[pixel + (scanLine * 160)] != 0xFFFFFFFF) {
                        continue ;
                    }
                    
                }
                // Update Screen pixels
                this->displayPixels[pixel + (scanLine * 160)] = (0xFF << 24) | (red << 16) | (green << 8) | blue;

            }

        }

    }

}

void Emulator::doDMATransfer(BYTE data) {
    
    /*
    Data written in the DMA register is the first byte of actual address.
    DMA transfers always begin with 0x00 in the lower byte, and it copies 
    exactly 160 bytes (0x00-9F) so the lower bits will never be in the 0xA0-FF 
    range.

    Destination is 0xFE00-FE9F (160 bytes), which is the Sprite Attribute Table

    Part on only being able to access HRAM during DMA transfer is unimplemented
    */
    WORD address = data << 8; 
    for (int i = 0x00; i < 0xA0; i++) {
        this->writeMem(0xFE00 + i, this->readMem(address + i));
    }
}

void Emulator::renderGraphics() {
    if (this->doRenderPtr != nullptr) {
        this->doRenderPtr();
    }
}

/*
********************************************************************************
Utility Functions
********************************************************************************
*/

bool Emulator::isBitSet(BYTE data, int position) const {
    return ((data >> position) & 0x1) == 0x1;
}

BYTE Emulator::bitSet(BYTE data, int position) const {
    int mask = 1 << position;
    return data | mask;
}

BYTE Emulator::bitReset(BYTE data, int position) const {
    int mask = ~(1 << position);
    return data & mask;
}

/*
********************************************************************************

START OF OPCODES

8 bit Load Commands
********************************************************************************
*/

/*  
    LD r, R 

    Loads the contents of R register to r register.
    r/R can be A, B, C, D, E, H, L

    4 cycles

    Flags affected(znhc): ----
*/
int Emulator::LD_r_R(BYTE& loadTo, BYTE loadFrom) { 
    loadTo = loadFrom; // to is regXX.high/low, from is regXX.high/low
    return 4; 
}

/*
    LD r, n  (0x06, 0x16, 0x26   0x0E, 0x1E, 0x2E, 0x3E)

    Loads immediate 8 bit data into r register.
    r/R can be A, B, C, D, E, F, H, L

    8 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_r_n(BYTE& reg) {
    BYTE n = this->readMem(this->programCounter.regstr);
    this->programCounter.regstr++;
    reg = n;
    return 8;
}

/*
    LD r, HL  (0x46, 0x56, 0x66   0x4E, 0x5E, 0x6E, 0x7E))
    
    Loads content of memory location specified by HL into r register.
    r can be A, B, C, D, E, F, H, L

    8 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_r_HL(BYTE& reg) {
    reg = this->readMem(this->regHL.regstr);

    return 8;
}

/*
    LD HL, r  (0x70 - 0x77 except 0x76)

    Loads content of r register into memory location specified by HL.

    8 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_HL_r(BYTE reg) {
    this->writeMem(this->regHL.regstr, reg);

    return 8;
}

/*
    LD HL, n  (0x36)

    Loads immediate 8 bit data into memory location specified by HL.

    12 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_HL_n() {
    this->writeMem(this->regHL.regstr, this->readMem(this->programCounter.regstr));
    this->programCounter.regstr++;

    return 12;
}

/*
    LD A, BC  (0x0A)

    Loads content of memory location specified by BC into register A.

    8 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_A_BC() {
    this->regAF.high = this->readMem(this->regBC.regstr);

    return 8;
}

/*
    LD A, DE  (0x1A)

    Loads content of memory location specified by DE into register A.

    8 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_A_DE() {
    this->regAF.high = this->readMem(this->regDE.regstr);

    return 8;
}

/*
    LD A, nn  (0xFA)

    Loads content of memory location specified by immediate 16 bit address into register A.

    16 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_A_nn() {
    WORD nn = this->readMem(this->programCounter.regstr + 1) << 8;
    nn |= this->readMem(this->programCounter.regstr);
    this->programCounter.regstr += 2;
    this->regAF.high = nn;

    return 16;
}

/*
    LD BC, A  (0x02)

    Loads content of register A into memory location specified by BC.

    8 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_BC_A() {
    this->writeMem(this->regBC.regstr, this->regAF.high);

    return 8;
}

/*
    LD DE, A  (0x12)

    Loads content of register A into memory location specified by DE.

    8 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_DE_A() {
    this->writeMem(this->regDE.regstr, this->regAF.high);

    return 8;
}

/*
    LD nn, A  (0xEA)

    Loads content of register A into memory location specified by immediate 16 bit address.

    16 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_nn_A() {
    WORD nn = this->readMem(this->programCounter.regstr + 1) << 8;
    nn |= this->readMem(this->programCounter.regstr);
    this->programCounter.regstr += 2;
    this->writeMem(nn, this->regAF.high);

    return 16;
}

/*
    LD A, FF00+n  (0xF0)

    Loads content of memory location specified by FF00+n into register A,
    where n is the immediate 8 bit data

    12 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_A_FF00n() {
    BYTE n = this->readMem(this->programCounter.regstr);
    this->programCounter.regstr++;
    this->regAF.high = this->readMem(0xFF00 + n);

    return 12;
}

/*
    LD FF00+n, A  (0xE0)

    Loads content of register A into memory location specified by FF00+n,
    where n is the immediate 8 bit data.

    12 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_FF00n_A() {
    BYTE n = this->readMem(this->programCounter.regstr);
    this->programCounter.regstr++;
    this->writeMem(0xFF00 + n, this->regAF.high);

    return 12;
}

/*
    LD A, FF00+C  (0xF2)

    Loads content of memory location specified by FF00+C into register A,
    where C is the content of register C

    8 cycles 

    Flags affected(znhc): ----
 */
int Emulator::LD_A_FF00C() {
    this->regAF.high = this->readMem(0xFF00 + this->regBC.low);

    return 8;
}

/*
    LD FF00+C, A  (0xE2)

    Loads content of register A into memory location specified by FF00+C,
    where C is the content of register C

    8 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_FF00C_A() {
    this->writeMem(0xFF00 + this->regBC.low, this->regAF.high);

    return 8;
}

/*
    LDI HL, A  (0x22)

    Loads content of register A into memory location specified by HL, then increment HL.

    8 cycles

    Flags affected(znhc): ----
 */
int Emulator::LDI_HL_A() {
    this->writeMem(this->regHL.regstr, this->regAF.high);
    this->regHL.regstr++;

    return 8;
}

/*
    LDI A, HL  (0x2A)

    Loads content of memory location specified by HL into register A, then increment HL.

    8 cycles

    Flags affected(znhc): ----
 */
int Emulator::LDI_A_HL() {
    this->regAF.high = this->readMem(this->regHL.regstr);
    this->regHL.regstr++;

    return 8;
}

/*
    LDD HL, A  (0x32)

    Loads content of register A into memory location specified by HL, then decrement HL.

    8 cycles

    Flags affected(znhc): ----
 */
int Emulator::LDD_HL_A() {
    this->writeMem(this->regHL.regstr, this->regAF.high);
    this->regHL.regstr--;

    return 8;
}

/*
    LDD A, HL  (0x3A)

    Loads content of memory location specified by HL into register A, then decrement HL.

    8 cycles

    Flags affected(znhc): ----
 */
int Emulator::LDD_A_HL() {
    this->regAF.high = this->readMem(this->regHL.regstr);
    this->regHL.regstr--;

    return 8;
}

/*
********************************************************************************
16 bit Load Commands
********************************************************************************
*/

/*
    LD rr, nn  (0x01 - 0x31)

    Loads immediate 16 bit data into register rr
    rr can be (BC, DE, HL, SP)

    12 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_rr_nn(Register& reg) {
    WORD nn = this->readMem(this->programCounter.regstr + 1) << 8;
    nn |= this->readMem(this->programCounter.regstr);
    this->programCounter.regstr += 2;
    reg.regstr = nn;

    return 12;
}

/*
    LD SP, HL  (0xF9)

    Loads content of HL into SP.

    8 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_SP_HL() {
    this->stackPointer.regstr = this->regHL.regstr;

    return 8;
}

/*
    LD nn, SP  (0x08)

    Loads content of SP into memory location specified by nn

    20 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_nn_SP() {
    WORD nn = this->readMem(this->programCounter.regstr + 1) << 8;
    nn |= this->readMem(this->programCounter.regstr);
    this->programCounter.regstr += 2;

    this->writeMem(nn + 1, this->stackPointer.high);
    this->writeMem(nn, this->stackPointer.low);

    return 20;
}

/*
    PUSH_rr  (0xC5 - 0xF5)

    Push content of rr into SP.
    rr can be (AF, BC, DE, HL)

    16 cycles

    Flags affected(znhc): ----
 */
int Emulator::PUSH_rr(Register reg) {
    this->stackPointer.regstr--;
    this->writeMem(this->stackPointer.regstr, reg.high);
    this->stackPointer.regstr--;
    this->writeMem(this->stackPointer.regstr, reg.low);

    return 16;
}

/*
    POP_rr  (0xC1 - 0xF1)

    Pop the top 16 bit data into register rr.
    rr can be (AF, BC, DE, HL)

    12 cycles

    Flags affected(znhc): ----
 */
int Emulator::POP_rr(Register& reg) {
    reg.low = this->readMem(this->stackPointer.regstr);
    this->stackPointer.regstr++;
    reg.high = this->readMem(this->stackPointer.regstr);
    this->stackPointer.regstr++;

    if (reg.regstr == this->regAF.regstr) {
        this->regAF.regstr &= 0xFFF0;
    }

    return 12;
}

/*
********************************************************************************
8 bit Arithmetic/Logical Commands
********************************************************************************
*/

/*
    ADD A, r  (0x80 - 0x87 except for 0x86)

    Adds content of register r to register A.
    r can be (B, C, D, E, H, L)

    4 cycles

    Flags affected(znhc): 
    - z: Set if result is zero
    - n: 0
    - h: Set if carry from bit 3
    - c: Set if carry from bit 7
 */
int Emulator::ADD_A_r(BYTE regR) {
    BYTE result = this->regAF.high + regR;
    
    // Reset the flags
    this->regAF.regstr &= 0xFFF0;

    // Zero flag
    if (result == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    // Half carry flag 
    if (((regR ^ this->regAF.high ^ result) & 0x10) == 0x10) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (result < this->regAF.high) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_CARRY);
    }

    this->regAF.high = result;

    return 4;
}

/*
    ADD A, n  (0xC6)

    Adds immediate 8 bit data into content of register A.

    8 cycles

    Flags affected(znhc): 
    - z: Set if result is zero
    - n: 0
    - h: Set if carry from bit 3
    - c: Set if carry from bit 7
 */
int Emulator::ADD_A_n() {
    BYTE n = this->readMem(this->programCounter.regstr);
    this->programCounter.regstr++;
    BYTE result = this->regAF.high + n;

    // Reset the flags
    this->regAF.regstr &= 0xFFF0;

    // Zero flag
    if (result == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    // Half carry flag 
    if (((n ^ this->regAF.high ^ result) & 0x10) == 0x10) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (result < this->regAF.high) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_CARRY);
    }

    this->regAF.high = result;

    return 8;
}

/*
    ADD A, HL  (0x86)

    Adds the content of the memory location specified by HL into register A.

    8 cycles

    Flags affected(znhc): 
    - z: Set if result is zero
    - n: 0
    - h: Set if carry from bit 3
    - c: Set if carry from bit 7
 */
int Emulator::ADD_A_HL() {
    BYTE toAdd = this->readMem(this->regHL.regstr);
    BYTE result = this->regAF.high + toAdd;

    // Reset the flags
    this->regAF.regstr &= 0xFFF0;

    // Zero flag
    if (result == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    // Half carry flag 
    if (((toAdd ^ this->regAF.high ^ result) & 0x10) == 0x10) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (result < this->regAF.high) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_CARRY);
    }

    this->regAF.high = result;

    return 8;
}

/*
    ADC A, r  (0x88 to 0x8F except for 0x8E)

    Adds content of register r, and carry flag into register A
    r can be (A, B, C, D, E, H, L)

    4 cycles

    Flags affected(znhc): 
    - z: Set if result is zero
    - n: 0
    - h: Set if carry from bit 3
    - c: Set if carry from bit 7
 */
int Emulator::ADC_A_r(BYTE reg) {
    BYTE carry = this->isBitSet(this->regAF.low, FLAG_CARRY) ? 0x01 : 0x00;
    BYTE toAdd = carry + reg;
    BYTE result = this->regAF.high + carry + toAdd;

    // Reset the flags
    this->regAF.regstr &= 0xFFF0;

    // Zero flag
    if (result == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    // Half carry flag 
    if (((toAdd ^ this->regAF.high ^ result) & 0x10) == 0x10) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (result < this->regAF.high) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_CARRY);
    }

    this->regAF.high = result;

    return 4;
}

/*
    ADC, A, n  (0xCE)

    Adds immediate 8 bit data, and carry flag into contents of register A.

    8 cycles

    Flags affected(znhc): 
    - z: Set if result is zero
    - n: 0
    - h: Set if carry from bit 3
    - c: Set if carry from bit 7
 */
int Emulator::ADC_A_n() {
    BYTE carry = this->isBitSet(this->regAF.low, FLAG_CARRY) ? 0x01 : 0x00;
    BYTE toAdd = carry + this->readMem(this->programCounter.regstr);
    BYTE result = this->regAF.high + carry + toAdd;
    this->programCounter.regstr++;

    // Reset the flags
    this->regAF.regstr &= 0xFFF0;

    // Zero flag
    if (result == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    // Half carry flag 
    if (((toAdd ^ this->regAF.high ^ result) & 0x10) == 0x10) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (result < this->regAF.high) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_CARRY);
    }

    this->regAF.high = result;

    return 8;
}

/*
    ADC A, HL  (0x8E)

    Adds content of memory location specified by HL, and carry flag into contents of register A.

    8 cycles

    Flags affected(znhc): 
    - z: Set if result is zero
    - n: 0
    - h: Set if carry from bit 3
    - c: Set if carry from bit 7
 */
int Emulator::ADC_A_HL() {
    BYTE carry = this->isBitSet(this->regAF.low, FLAG_CARRY) ? 0x01 : 0x00;
    BYTE toAdd = carry + this->readMem(this->regHL.regstr);
    BYTE result = this->regAF.high + carry + toAdd;

    // Reset the flags
    this->regAF.regstr &= 0xFFF0;

    // Zero flag
    if (result == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    // Half carry flag 
    if (((toAdd ^ this->regAF.high ^ result) & 0x10) == 0x10) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (result < this->regAF.high) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_CARRY);
    }

    this->regAF.high = result;

    return 8;
}

/*
    SUB r  (0x90 - 0x97 except of 0x96)

    Subtracts content of register r from register A.
    r can be (A, B, C, D, E, H, L)

    4 cycles

    Flags affected(znhc): 
    - z: Set if result is zero
    - n: 1
    - h: Set if A lower nibble less than r lower nibble)
    - c: Set if A less than r
 */
int Emulator::SUB_r(BYTE reg) {
    BYTE result = this->regAF.high - reg;

    // Reset the flags
    this->regAF.regstr &= 0xFFF0;

    // Zero flag
    if (result == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    // Subtract flag
    this->regAF.low = this->bitSet(this->regAF.low, FLAG_SUB);

    // Half carry flag 
    // If lower nibble of A is less than lower nibble of r, set HCF
    if ((this->regAF.high & 0x0F) < (reg & 0x0F)) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (this->regAF.high < reg) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_CARRY);
    }

    this->regAF.high = result;

    return 4;
}

/*
    SUB n  (0xD6)

    Subtracts immediate 8 bit data from contents of register A.

    8 cycles

    Flags affected(znhc): 
    - z: Set if result is zero
    - n: 1
    - h: Set if A lower nibble less than n lower nibble)
    - c: Set if A less than n
 */
int Emulator::SUB_n() {
    BYTE n = this->readMem(this->programCounter.regstr);
    this->programCounter.regstr++;

    BYTE result = this->regAF.high - n;

    // Reset the flags
    this->regAF.regstr &= 0xFFF0;

    // Zero flag
    if (result == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    // Subtract flag
    this->regAF.low = this->bitSet(this->regAF.low, FLAG_SUB);

    // Half carry flag 
    // If lower nibble of A is less than lower nibble of n, set HCF
    if ((this->regAF.high & 0x0F) < (n & 0x0F)) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (this->regAF.high < n) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_CARRY);
    }

    this->regAF.high = result;

    return 8;
}

/*
    SUB HL  (0x96)

    Subtracts contents of memory location specified by HL from contents of register A

    8 cycles

    Flags affected(znhc): 
    - z: Set if result is zero
    - n: 1
    - h: Set if A lower nibble less than (HL) lower nibble)
    - c: Set if A less than (HL)
 */
int Emulator::SUB_HL() {
    BYTE toSub = this->readMem(this->regHL.regstr);
    BYTE result = this->regAF.high - toSub;

    // Reset the flags
    this->regAF.regstr &= 0xFFF0;

    // Zero flag
    if (result == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    // Subtract flag
    this->regAF.low = this->bitSet(this->regAF.low, FLAG_SUB);

    // Half carry flag 
    // If lower nibble of A is less than lower nibble of toSub, set HCF
    if ((this->regAF.high & 0x0F) < (toSub & 0x0F)) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (this->regAF.high < toSub) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_CARRY);
    }

    this->regAF.high = result;

    return 8;
}

/*
    SBC A, r  (0x98 - 0x9F except for 0x9E)

    Subtracts content of register r, and carry flag from content of register A.
    r can be (A, B, C, D, E, H, L)

    4 cycles

    Flags affected(znhc): 
    - z: Set if result is zero
    - n: 1
    - h: Set if A lower nibble less than toSub lower nibble)
    - c: Set if A less than toSub
 */
int Emulator::SBC_A_r(BYTE reg) {
    BYTE carry = this->isBitSet(this->regAF.low, FLAG_CARRY) ? 0x1 : 0x0;
    BYTE toSub = carry + reg;
    BYTE result = this->regAF.high - toSub;

    // Reset the flags
    this->regAF.regstr &= 0xFFF0;

    // Zero flag
    if (result == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    // Subtract flag
    this->regAF.low = this->bitSet(this->regAF.low, FLAG_SUB);

    // Half carry flag 
    // If lower nibble of A is less than lower nibble of toSub, set HCF
    if ((this->regAF.high & 0x0F) < (toSub & 0x0F)) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (this->regAF.high < toSub) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_CARRY);
    }

    this->regAF.high = result;

    return 4;
}

/*
    SBC A, n  (0xDE)

    Subtracts immediate 8 bit data, and carry flag from content of register A.

    8 cycles

    Flags affected(znhc): 
    - z: Set if result is zero
    - n: 1
    - h: Set if A lower nibble less than toSub lower nibble)
    - c: Set if A less than toSub
 */
int Emulator::SBC_A_n() {
    BYTE carry = this->isBitSet(this->regAF.low, FLAG_CARRY) ? 0x1 : 0x0;
    BYTE toSub = carry + this->readMem(this->programCounter.regstr);
    this->programCounter.regstr++;

    BYTE result = this->regAF.high - toSub;

    // Reset the flags
    this->regAF.regstr &= 0xFFF0;

    // Zero flag
    if (result == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    // Subtract flag
    this->regAF.low = this->bitSet(this->regAF.low, FLAG_SUB);

    // Half carry flag 
    // If lower nibble of A is less than lower nibble of toSub, set HCF
    if ((this->regAF.high & 0x0F) < (toSub & 0x0F)) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (this->regAF.high < toSub) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_CARRY);
    }

    this->regAF.high = result;

    return 8;
}

/*
    SBC A, HL  (0x9E)

    Subtracts content of memory location specified by HL, and carry flag from content of register A.

    8 cycles

    Flags affected(znhc): 
    - z: Set if result is zero
    - n: 1
    - h: Set if A lower nibble less than toSub lower nibble)
    - c: Set if A less than toSub
 */
int Emulator::SBC_A_HL() {
    BYTE carry = this->isBitSet(this->regAF.low, FLAG_CARRY) ? 0x1 : 0x0;
    BYTE toSub = carry + this->readMem(this->regHL.regstr);
    BYTE result = this->regAF.high - toSub;

    // Reset the flags
    this->regAF.regstr &= 0xFFF0;

    // Zero flag
    if (result == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    // Subtract flag
    this->regAF.low = this->bitSet(this->regAF.low, FLAG_SUB);

    // Half carry flag 
    // If lower nibble of A is less than lower nibble of toSub, set HCF
    if ((this->regAF.high & 0x0F) < (toSub & 0x0F)) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (this->regAF.high < toSub) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_CARRY);
    }

    this->regAF.high = result;

    return 8;
}

/*
    AND r  (0xA0 - 0xA7 except for 0xA6)

    Set register A to bitwise AND register A and register r.
    r can be (A, B, C, D, E, H, L)

    4 cycles

    Flags affected:
    - z: Set if result is zero
    - n: 0
    - h: 1
    - c: 0
 */
int Emulator::AND_r(BYTE reg) {
    BYTE result = this->regAF.high & reg;
    
    // Reset the flags
    this->regAF.regstr &= 0xFFF0;

    // Zero flag
    if (result == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    // Half carry flag 
    this->regAF.low = this->bitSet(this->regAF.low, FLAG_HALFCARRY);

    this->regAF.high = result;

    return 4;
}

/*
    AND n  (0xE6)

    Set register A to bitwise AND register A and immediate 8 bit data.

    8 cycles

    Flags affected:
    - z: Set if result is zero
    - n: 0
    - h: 1
    - c: 0
 */
int Emulator::AND_n() {
    BYTE result = this->regAF.high & this->readMem(this->programCounter.regstr);
    this->programCounter.regstr++;
    
    // Reset the flags
    this->regAF.regstr &= 0xFFF0;

    // Zero flag
    if (result == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    // Half carry flag 
    this->regAF.low = this->bitSet(this->regAF.low, FLAG_HALFCARRY);

    this->regAF.high = result;

    return 8;
}

/*
    AND HL  (0xA6)

    Set register A to bitwise AND register A and content of memory location specified by HL.

    8 cycles

    Flags affected:
    - z: Set if result is zero
    - n: 0
    - h: 1
    - c: 0
 */
int Emulator::AND_HL() {
    BYTE result = this->regAF.high & this->readMem(this->regHL.regstr);
    
    // Reset the flags
    this->regAF.regstr &= 0xFFF0;

    // Zero flag
    if (result == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    // Half carry flag 
    this->regAF.low = this->bitSet(this->regAF.low, FLAG_HALFCARRY);

    this->regAF.high = result;

    return 8;
}

/*
    XOR r  (0xA8 - 0xAF except for 0xAE)

    Set register A to bitwise XOR register A and register r.

    4 cycles

    Flags affected:
    - z: Set if result is zero
    - n: 0
    - h: 0
    - c: 0
 */
int Emulator::XOR_r(BYTE reg) {
    BYTE result = this->regAF.high ^ reg;

    // Reset the flags
    this->regAF.regstr &= 0xFFF0;

    // Zero flag
    if (result == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    this->regAF.high = result;

    return 4;
}

/*
    XOR n  (0xEE)

    Set register A to bitwise XOR register A and immediate 8 bit data.

    8 cycles

    Flags affected:
    - z: Set if result is zero
    - n: 0
    - h: 0
    - c: 0
 */
int Emulator::XOR_n() {
    BYTE result = this->regAF.high ^ this->readMem(this->programCounter.regstr);
    this->programCounter.regstr++;

    // Reset the flags
    this->regAF.regstr &= 0xFFF0;

    // Zero flag
    if (result == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    this->regAF.high = result;

    return 8;
}

/*
    XOR HL  (0xAE)

    Set register A to bitwise XOR register A and content of memory location specified by HL.

    8 cycles

    Flags affected:
    - z: Set if result is zero
    - n: 0
    - h: 0
    - c: 0
 */
int Emulator::XOR_HL() {
    BYTE result = this->regAF.high ^ this->readMem(this->regHL.regstr);

    // Reset the flags
    this->regAF.regstr &= 0xFFF0;

    // Zero flag
    if (result == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    this->regAF.high = result;

    return 8;
}

/*
    OR r  (0xB0 - 0xB7 except for 0xB6)

    Sets register A to bitwise OR register A and register r.
    r can be (A, B, C, D, E, H, L)

    4 cycles

    Flags affected:
    - z: Set if result is zero
    - n: 0
    - h: 0
    - c: 0
 */
int Emulator::OR_r(BYTE reg) {
    BYTE result = this->regAF.high | reg;

    // Reset the flags
    this->regAF.regstr &= 0xFFF0;

    // Zero flag
    if (result == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    this->regAF.high = result;

    return 4;
}

/*
    OR n  (0xF6)

    Set register A to bitwise OR register A and immediate 8 bit data.

    8 cycles

    Flags affected:
    - z: Set if result is zero
    - n: 0
    - h: 0
    - c: 0
 */
int Emulator::OR_n() {
    BYTE result = this->regAF.high | this->readMem(this->programCounter.regstr);
    this->programCounter.regstr++;

    // Reset the flags
    this->regAF.regstr &= 0xFFF0;

    // Zero flag
    if (result == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    this->regAF.high = result;

    return 8;
}

/*
    OR HL  (0xB6)

    Set register A to bitwise OR register A and content of memory location specified by HL.

    8 cycles

    Flags affected:
    - z: Set if result is zero
    - n: 0
    - h: 0
    - c: 0
 */
int Emulator::OR_HL() {
    BYTE result = this->regAF.high | this->readMem(this->regHL.regstr);

    // Reset the flags
    this->regAF.regstr &= 0xFFF0;

    // Zero flag
    if (result == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    this->regAF.high = result;

    return 8;
}

/*
    CP r  (0xB8 - 0xBF except for 0xBE)

    Compares content of register A and register r.
    Basically a SUB r without storing the result in register A.
    r can be (A, B, C, D, E, H, L)

    4 cycles

    Flags affected(znhc): 
    - z: Set if result is zero
    - n: 1
    - h: Set if A lower nibble less than r lower nibble)
    - c: Set if A less than r
 */
int Emulator::CP_r(BYTE reg) {
    BYTE result = this->regAF.high - reg;

    // Reset the flags
    this->regAF.regstr &= 0xFFF0;

    // Zero flag
    if (result == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    // Subtract flag
    this->regAF.low = this->bitSet(this->regAF.low, FLAG_SUB);

    // Half carry flag 
    // If lower nibble of A is less than lower nibble of r, set HCF
    if ((this->regAF.high & 0x0F) < (reg & 0x0F)) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (this->regAF.high < reg) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_CARRY);
    }

    return 4;
}

/*
    CP n  (0xFE)

    Compares content of register A and immediate 8 bit data.
    Basically a SUB n without storing the result in register A.

    8 cycles

    Flags affected(znhc): 
    - z: Set if result is zero
    - n: 1
    - h: Set if A lower nibble less than n lower nibble)
    - c: Set if A less than n
 */
int Emulator::CP_n() {
    BYTE n = this->readMem(this->programCounter.regstr);
    this->programCounter.regstr++;

    BYTE result = this->regAF.high - n;

    // Reset the flags
    this->regAF.regstr &= 0xFFF0;

    // Zero flag
    if (result == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    // Subtract flag
    this->regAF.low = this->bitSet(this->regAF.low, FLAG_SUB);

    // Half carry flag 
    // If lower nibble of A is less than lower nibble of n, set HCF
    if ((this->regAF.high & 0x0F) < (n & 0x0F)) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (this->regAF.high < n) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_CARRY);
    }

    return 8;
}

/*
    CP HL  (0xBE)

    Compares content of register A and content of memory location specified by HL.
    Basically a SUB HL without storing the result in register A.
    
    8 cycles

    Flags affected(znhc): 
    - z: Set if result is zero
    - n: 1
    - h: Set if A lower nibble less than (HL) lower nibble)
    - c: Set if A less than (HL)
 */
int Emulator::CP_HL() {
    BYTE toSub = this->readMem(this->regHL.regstr);
    BYTE result = this->regAF.high - toSub;

    // Reset the flags
    this->regAF.regstr &= 0xFFF0;

    // Zero flag
    if (result == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    // Subtract flag
    this->regAF.low = this->bitSet(this->regAF.low, FLAG_SUB);

    // Half carry flag 
    // If lower nibble of A is less than lower nibble of toSub, set HCF
    if ((this->regAF.high & 0x0F) < (toSub & 0x0F)) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (this->regAF.high < toSub) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_CARRY);
    }

    return 8;
}

/*
    INC r (0x04 0x14 0x24  0x0C 0x1C 0x2C 0x3C)

    Increments register r by 1 
    r can be (A, B, C, D, E, H, L)

    4 cycles

    Flags affected(znhc): 
    - z: Set if result is zero
    - n: 0
    - h: Set if bit 3 was set before the increment, then not set after the increment
    - c: Not affected
 */
int Emulator::INC_r(BYTE& reg) {
    bool wasBit3Set = this->isBitSet(reg, 3);
    reg++;
    bool afterBit3Set = this->isBitSet(reg, 3);
    
    // Zero flag
    if (reg == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    // Subtract flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);

    // Half carry flag 
    // Set if bit 3 was set before the increment, then not set after the increment
    if (wasBit3Set && !afterBit3Set) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_HALFCARRY);
    } 
    else {
        this->regAF.low = this->bitReset(this->regAF.low, FLAG_HALFCARRY);
    }

    return 4;
}

/*
    INC HL (0x34)

    Increments register HL by 1.

    12 cycles

    Flags affected(znhc): 
    - z: Set if result is zero
    - n: 0
    - h: Set if bit 3 was set before the increment, then not set after the increment
    - c: Not affected
 */
int Emulator::INC_HL() {
    BYTE reg = this->readMem(this->regHL.regstr);
    bool wasBit3Set = this->isBitSet(reg, 3);
    reg++;
    bool afterBit3Set = this->isBitSet(reg, 3);
    
    // Zero flag
    if (reg == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    // Subtract flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);

    // Half carry flag 
    // Set if bit 3 was set before the increment, then not set after the increment
    if (wasBit3Set && !afterBit3Set) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_HALFCARRY);
    } 
    else {
        this->regAF.low = this->bitReset(this->regAF.low, FLAG_HALFCARRY);
    }

    this->writeMem(this->regHL.regstr, reg);

    return 12;
}

/*
    DEC r (0x05 0x15 0x25  0x0D 0x1D 0x2D 0x3D)

    Derements register r by 1 
    r can be (A, B, C, D, E, H, L)

    4 cycles

    Flags affected(znhc): 
    - z: Set if result is zero
    - n: 1
    - h: Set if XOR between r, result, and 1 is 0x10
    - c: Not affected
 */
int Emulator::DEC_r(BYTE& reg) {
    BYTE result = reg - 1;
    
    // Zero flag
    if (result == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    // Subtract flag
    this->regAF.low = this->bitSet(this->regAF.low, FLAG_SUB);

    // Half carry flag 
    if ((((result ^ reg ^ 0x1)) & 0x10) == 0x10) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_HALFCARRY);
    } 
    else {
        this->regAF.low = this->bitReset(this->regAF.low, FLAG_HALFCARRY);
    }

    reg = result;

    return 4;
}

/*
    DEC HL 

    Derements content of memory location specified by HL by 1.

    12 cycles

    Flags affected(znhc): 
    - z: Set if result is zero
    - n: 1
    - h: Set if XOR between (HL), result, and 1 is 0x10
    - c: Not affected
 */
int Emulator::DEC_HL() {
    BYTE initial = this->readMem(this->regHL.regstr);
    BYTE result =  initial - 1;
    
    // Zero flag
    if (result == 0x0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }

    // Subtract flag
    this->regAF.low = this->bitSet(this->regAF.low, FLAG_SUB);

    // Half carry flag 
    if ((((result ^ initial ^ 0x1)) & 0x10) == 0x10) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_HALFCARRY);
    } 
    else {
        this->regAF.low = this->bitReset(this->regAF.low, FLAG_HALFCARRY);
    }

    this->writeMem(this->regHL.regstr, result);

    return 12;
}

/*
    DAA  (0x27)

    Decimal adjust register A to Binary Coded Decimal.

    4 cycles

    Flags affected(znhc): 
    - z: Set if result is zero
    - n: Not affected
    - h: 0
    - c: x
 */
int Emulator::DAA() {
    int result = this->regAF.high;

    // After an addition
    if (!this->isBitSet(this->regAF.low, FLAG_SUB)) {
        if (this->isBitSet(this->regAF.low, FLAG_HALFCARRY) || (result & 0xF) > 9) {
            result += 0x06;
        }

        if (this->isBitSet(this->regAF.low, FLAG_CARRY) || (result > 0x9F)) {
            result += 0x60;
        }
    }
    // After a subtraction
    else
    {
        if (this->isBitSet(this->regAF.low, FLAG_HALFCARRY)) {
            result = (result - 0x06) & 0xFF;
        }

        if (this->isBitSet(this->regAF.low, FLAG_CARRY)) {
            result -= 0x60;
        }
    }

    // Half carry flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_HALFCARRY);

    // Carry flag
    // If it overflowed
    if ((result & 0x100) == 0x100) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_CARRY);
    }
    
    result &= 0xFF;

    // Zero flag
    if (result == 0) {
        this->regAF.low = this->bitSet(this->regAF.low, FLAG_ZERO);
    }
    else
    {
        this->regAF.low = this->bitReset(this->regAF.low, FLAG_ZERO);
    }

    this->regAF.high = (BYTE) result;

    return 4;
}

/*
    CPL  (0x2F)

    Sets register A to its complement

    4 cycles

    Flags affected(znhc): 
    - z: Not affected
    - n: 1
    - h: 1
    - c: Not affected
 */
int Emulator::CPL() {
    BYTE result = this->regAF.high ^ 0xFF;

    // Half carry flag
    this->regAF.low = this->bitSet(this->regAF.low, FLAG_HALFCARRY);

    // Carry flag
    this->regAF.low = this->bitSet(this->regAF.low, FLAG_CARRY);

    this->regAF.high = result;

    return 4;
}

/*
********************************************************************************
16 bit Arithmetic/logical Commands
********************************************************************************
*/

/*
    ADD HL, rr  (0xX9)

    Adds contents of rr to contents of HL and stores the result in HL.
    rr can be register pairs BC, DE, HL or SP.

    8 cycles

    Flags affected:
    - z: -
    - n: 0
    - h: set if overflow from bit 11 (lower to upper nibble of upper byte)
    - c: set if overflow from bit 15
*/
int Emulator::ADD_HL_rr(WORD rr) {

    WORD before = this->regHL.regstr;
    
    // Add contents and store result in HL
    WORD result = before + rr;
    this->regHL.regstr = result;

    // Reset subtract flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);

    // Update half carry flag
    this->regAF.low = ((result ^ before ^ rr) & 0x1000)
        ? this->bitSet(this->regAF.low, FLAG_HALFCARRY)
        : this->bitReset(this->regAF.low, FLAG_HALFCARRY);

    // Update carry flag
    this->regAF.low = (result < before) 
        ? this->bitSet(this->regAF.low, FLAG_CARRY)
        : this->bitReset(this->regAF.low, FLAG_CARRY);

    return 8;

}

/*
    INC rr  (0xX3)

    16 bit register rr is incremented. 
    rr can be register pairs BC, DE, HL or SP.

    8 cycles
    
    Flags affected (znhc): ----
*/
int Emulator::INC_rr(WORD& rr) {

    rr++;

    return 8;

}

/*
    DEC rr  (0xXB)

    16 bit register rr is decremented.
    rr can be register pairs BC, DE, HL or SP.

    8 cycles

    Flags affected (znhc): ----
*/
int Emulator::DEC_rr(WORD& rr) {

    rr--;

    return 8;

}

/*
    ADD SP, dd  (0xE8)

    Add dd to SP.
    dd is an 8 bit signed number. Read from memory at PC.

    16 cycles

    Flags affected:
    - z: 0
    - n: 0
    - h: set if overflow from bit 3 (lower to upper nibble)
    - c: set if overflow from bit 7
*/
int Emulator::ADD_SP_dd() {

    WORD before = this->stackPointer.regstr;
    SIGNED_BYTE dd = static_cast<SIGNED_BYTE>(this->readMem(this->programCounter.regstr));
    this->programCounter.regstr++;

    // Adding dd to SP and storing result in SP
    WORD result = before + dd;
    this->stackPointer.regstr = result;

    // Reset zero and subtract flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_ZERO);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);

    // Update half carry flag
    this->regAF.low = ((result & 0x0F) < (before & 0x0F))
        ? this->bitSet(this->regAF.low, FLAG_HALFCARRY)
        : this->bitReset(this->regAF.low, FLAG_HALFCARRY);

    // Update carry flag
    this->regAF.low = ((result & 0xFF) < (before & 0xFF)) 
        ? this->bitSet(this->regAF.low, FLAG_CARRY)
        : this->bitReset(this->regAF.low, FLAG_CARRY);

    return 16;

}

/*
    LD HL, SP + dd (0xF8)

    Add dd to SP, and load the result into HL.
    dd is an 8 bit signed number. Read from memory at PC.

    12 cycles

    Flags affected:
    - z: 0
    - n: 0
    - h: set if overflow from bit 3 (lower to upper nibble)
    - c: set if overflow from bit 7
*/
int Emulator::LD_HL_SPdd() {

    WORD before = this->stackPointer.regstr;
    SIGNED_BYTE dd = static_cast<SIGNED_BYTE>(this->readMem(this->programCounter.regstr));
    this->programCounter.regstr++;

    // Adding dd to SP, and load result into HL
    WORD result = this->stackPointer.regstr + dd;
    this->regHL.regstr = result;

    // Reset zero and subtract flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_ZERO);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);

    // Update half carry flag
    this->regAF.low = ((result & 0x0F) < (before & 0x0F))
        ? this->bitSet(this->regAF.low, FLAG_HALFCARRY)
        : this->bitReset(this->regAF.low, FLAG_HALFCARRY);

    // Update carry flag
    this->regAF.low = ((result & 0xFF) < (before & 0xFF)) 
        ? this->bitSet(this->regAF.low, FLAG_CARRY)
        : this->bitReset(this->regAF.low, FLAG_CARRY);

    return 12;

}

/*
********************************************************************************
Rotate and Shift Commands
********************************************************************************
*/

/*
    RLCA  (0x07)

    The contents of the accumulator are rotated 1 bit to the left.
    Bit 7 is copied to the carry flag and bit 0.

    4 cycles

    Flags affected:
    - z: 0
    - n: 0
    - h: 0
    - c: Bit 7 of accumulator
*/

int Emulator::RLCA() {

    BYTE data = this->regAF.high;
    BYTE bit7 = data >> 7;

    // Shift data left and copy bit 7 to bit 0
    data <<= 1;
    data |= bit7;

    // Store result back into accumulator
    this->regAF.high = data;

    // Reset zero, subtract and half carry flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_ZERO);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_HALFCARRY);

    // Copy bit 7 into carry flag
    this->regAF.low = this->isBitSet(bit7, 0) 
        ? this->bitSet(this->regAF.low, FLAG_CARRY)
        : this->bitReset(this->regAF.low, FLAG_CARRY);

    return 4;

}

/*
    RLA  (0x17)

    The contents of the accumulator are rotated 1 bit to the left through carry.
    Bit 7 is copied to the carry flag and carry is copied to bit 0.

    4 cycles

    Flags affected:
    - z: 0
    - n: 0
    - h: 0
    - c: Bit 7 of accumulator
*/

int Emulator::RLA() {

    BYTE data = this->regAF.high;
    BYTE bit7 = data >> 7;

    // Shift data left and copy old carry flag to bit 0
    data <<= 1;
    data |= (this->isBitSet(this->regAF.low, FLAG_CARRY) ? 0b1 : 0b0);

    // Store result back into accumulator
    this->regAF.high = data;

    // Reset zero, subtract and half carry flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_ZERO);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_HALFCARRY);

    // Copy bit 7 into carry flag
    this->regAF.low = this->isBitSet(bit7, 0) 
        ? this->bitSet(this->regAF.low, FLAG_CARRY)
        : this->bitReset(this->regAF.low, FLAG_CARRY);

    return 4;

}

/*
    RRCA  (0x0F)
    
    The contents of the accumulator are rotated 1 bit to the right.
    Bit 0 is copied to the carry flag and bit 7.

    4 cycles

    Flags affected:
    - z: 0
    - n: 0
    - h: 0
    - c: Bit 0 of accumulator
*/
int Emulator::RRCA() {

    BYTE data = this->regAF.high;
    BYTE bit0 = data & 0b1;

    // Shift data right and copy bit 0 to bit 7
    data >>= 1;
    data |= (bit0 << 7);

    // Store result back into accumulator
    this->regAF.high = data;

    // Reset zero, subtract and half carry flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_ZERO);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_HALFCARRY);

    // Copy bit 0 into carry flag
    this->regAF.low = this->isBitSet(bit0, 0) 
        ? this->bitSet(this->regAF.low, FLAG_CARRY)
        : this->bitReset(this->regAF.low, FLAG_CARRY);

    return 4;
}

/*
    RRA  (0x1F)

    The contents of the accumulator are rotated 1 bit right through carry.
    Bit 0 is copied to the carry flag and carry is copied to bit 7.

    4 cycles

    Flags affected:
    - z: 0
    - n: 0
    - h: 0
    - c: Bit 0 of accumulator
*/

int Emulator::RRA() {

    BYTE data = this->regAF.high;
    BYTE bit0 = data & 0b1;

    // Shift data right and copy old carry flag to bit 7
    data >>= 1;
    data |= ((this->isBitSet(this->regAF.low, FLAG_CARRY) ? 0b1 : 0b0) << 7);

    // Store result back into accumulator
    this->regAF.high = data;

    // Reset zero, subtract and half carry flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_ZERO);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_HALFCARRY);

    // Copy bit 0 into carry flag
    this->regAF.low = this->isBitSet(bit0, 0) 
        ? this->bitSet(this->regAF.low, FLAG_CARRY)
        : this->bitReset(this->regAF.low, FLAG_CARRY);

    return 4;

}

/*
    RLC r  (CB 0x0X)

    The contents of 8 bit register r is rotated 1 bit left.
    Bit 7 of the register is copied to carry flag and also bit 0.
    r can be registers A, B, C, D, E, H or L.

    8 cycles

    Flags affected:
    - z: Set if result is 0, else reset
    - n: 0
    - h: 0
    - c: Bit 7 of register
*/
int Emulator::RLC_r(BYTE& r) {

    BYTE data = r;
    BYTE bit7 = data >> 7;

    // Shift data left and copy bit 7 to bit 0
    data <<= 1;
    data |= bit7;

    // Store result back into r
    r = data;

    // Reset subtract and half carry flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    this->regAF.low = (data == 0x0) 
        ? this->bitSet(this->regAF.low, FLAG_ZERO) 
        : this->bitReset(this->regAF.low, FLAG_ZERO);

    // Copy bit 7 into carry flag
    this->regAF.low = this->isBitSet(bit7, 0) 
        ? this->bitSet(this->regAF.low, FLAG_CARRY)
        : this->bitReset(this->regAF.low, FLAG_CARRY);

    return 8;

}

/*
    RLC (HL)  (CB 0x06)

    The contents stored at memory location HL is rotated 1 bit left.
    Bit 7 of the contents is copied to carry flag and also bit 0.

    16 cycles

    Flags affected:
    - z: Set if result is 0, else reset
    - n: 0
    - h: 0
    - c: Bit 7 of contents
*/
int Emulator::RLC_HL() {

    BYTE data = this->readMem(this->regHL.regstr);
    BYTE bit7 = data >> 7;

    // Shift data left and copy bit 7 to bit 0
    data <<= 1;
    data |= bit7;

    // Store result back into memory
    this->writeMem(this->regHL.regstr, data);

    // Reset subtract and half carry flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    this->regAF.low = (data == 0x0) 
        ? this->bitSet(this->regAF.low, FLAG_ZERO) 
        : this->bitReset(this->regAF.low, FLAG_ZERO);

    // Copy bit 7 into carry flag
    this->regAF.low = this->isBitSet(bit7, 0) 
        ? this->bitSet(this->regAF.low, FLAG_CARRY)
        : this->bitReset(this->regAF.low, FLAG_CARRY);

    return 16;

}

/*
    RL r  (CB 0x1X)

    The contents of 8 bit register r is rotated 1 bit left through carry.
    Bit 7 of the register is copied to carry flag and carry is copied to bit 0.
    r can be registers A, B, C, D, E, H or L.

    8 cycles

    Flags affected:
    - z: Set if result is 0, else reset
    - n: 0
    - h: 0
    - c: Bit 7 of register
*/

int Emulator::RL_r(BYTE& r) {

    BYTE data = r;
    BYTE bit7 = data >> 7;

    // Shift data left and copy old carry flag to bit 0
    data <<= 1;
    data |= (this->isBitSet(this->regAF.low, FLAG_CARRY) ? 0b1 : 0b0);

    // Store result back into r
    r = data;

    // Reset subtract and half carry flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    this->regAF.low = (data == 0x0) 
        ? this->bitSet(this->regAF.low, FLAG_ZERO) 
        : this->bitReset(this->regAF.low, FLAG_ZERO);

    // Copy bit 7 into carry flag
    this->regAF.low = this->isBitSet(bit7, 0) 
        ? this->bitSet(this->regAF.low, FLAG_CARRY)
        : this->bitReset(this->regAF.low, FLAG_CARRY);

    return 8;

}

/*
    RL (HL)  (CB 0x16)

    The contents stored at memory location HL is rotated 1 bit left through 
    carry.
    Bit 7 of the contents is copied to carry flag and carry is copied to bit 0.

    16 cycles

    Flags affected:
    - z: Set if result is 0, else reset
    - n: 0
    - h: 0
    - c: Bit 7 of contents
*/

int Emulator::RL_HL() {

    BYTE data = this->readMem(this->regHL.regstr);
    BYTE bit7 = data >> 7;

    // Shift data left and copy old carry flag to bit 0
    data <<= 1;
    data |= (this->isBitSet(this->regAF.low, FLAG_CARRY) ? 0b1 : 0b0);

    // Store result back into memory
    this->writeMem(this->regHL.regstr, data);

    // Reset subtract and half carry flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    this->regAF.low = (data == 0x0) 
        ? this->bitSet(this->regAF.low, FLAG_ZERO) 
        : this->bitReset(this->regAF.low, FLAG_ZERO);

    // Copy bit 7 into carry flag
    this->regAF.low = this->isBitSet(bit7, 0) 
        ? this->bitSet(this->regAF.low, FLAG_CARRY)
        : this->bitReset(this->regAF.low, FLAG_CARRY);

    return 16;

}

/*
    RRC r  (CB 0x0X)

    The contents of 8 bit register r are rotated 1 bit right.
    Bit 0 of the register is copied to carry flag and also bit 7.
    r can be registers A, B, C, D, E, H or L.

    8 cycles

    Flags affected:
    - z: Set if result is 0, else reset
    - n: 0
    - h: 0
    - c: Bit 0 of register
*/
int Emulator::RRC_r(BYTE& r) {

    BYTE data = r;
    BYTE bit0 = data & 0b1;

    // Shift data right and copy bit 0 to bit 7
    data >>= 1;
    data |= (bit0 << 7);

    // Store result back into r
    r = data;

    // Reset subtract and half carry flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    this->regAF.low = (data == 0x0) 
        ? this->bitSet(this->regAF.low, FLAG_ZERO) 
        : this->bitReset(this->regAF.low, FLAG_ZERO);

    // Copy bit 0 into carry flag
    this->regAF.low = this->isBitSet(bit0, 0) 
        ? this->bitSet(this->regAF.low, FLAG_CARRY)
        : this->bitReset(this->regAF.low, FLAG_CARRY);

    return 8;

}

/*
    RRC (HL)  (CB 0x0E)

    The contents stored at memory location HL is rotated 1 bit right.
    Bit 0 of the contents is copied to carry flag and also bit 7.

    16 cycles

    Flags affected:
    - z: Set if result is 0, else reset
    - n: 0
    - h: 0
    - c: Bit 0 of contents
*/
int Emulator::RRC_HL() {

    BYTE data = this->readMem(this->regHL.regstr);
    BYTE bit0 = data & 0b1;

    // Shift data right and copy bit 0 to bit 7
    data >>= 1;
    data |= (bit0 << 7);

    // Store result back into memory
    this->writeMem(this->regHL.regstr, data);

    // Reset subtract and half carry flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    this->regAF.low = (data == 0x0) 
        ? this->bitSet(this->regAF.low, FLAG_ZERO) 
        : this->bitReset(this->regAF.low, FLAG_ZERO);

    // Copy bit 0 into carry flag
    this->regAF.low = this->isBitSet(bit0, 0) 
        ? this->bitSet(this->regAF.low, FLAG_CARRY)
        : this->bitReset(this->regAF.low, FLAG_CARRY);

    return 16;

}

/*
    RR r  (CB 0x1X)

    The contents of 8 bit register r is rotated 1 bit right through carry.
    Bit 0 of the register is copied to carry flag and carry is copied to bit 7.
    r can be registers A, B, C, D, E, H or L.

    8 cycles

    Flags affected:
    - z: Set if result is 0, else reset
    - n: 0
    - h: 0
    - c: Bit 0 of register
*/

int Emulator::RR_r(BYTE& r) {

    BYTE data = r;
    BYTE bit0 = data & 0b1;

    // Shift data right and copy old carry flag to bit 7
    data >>= 1;
    data |= ((this->isBitSet(this->regAF.low, FLAG_CARRY) ? 0b1 : 0b0) << 7);

    // Store result back into r
    r = data;

    // Reset subtract and half carry flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    this->regAF.low = (data == 0x0) 
        ? this->bitSet(this->regAF.low, FLAG_ZERO) 
        : this->bitReset(this->regAF.low, FLAG_ZERO);

    // Copy bit 0 into carry flag
    this->regAF.low = this->isBitSet(bit0, 0) 
        ? this->bitSet(this->regAF.low, FLAG_CARRY)
        : this->bitReset(this->regAF.low, FLAG_CARRY);

    return 8;

}

/*
    RR (HL)  (CB 0x1E)

    The contents stored at memory location HL is rotated 1 bit right through 
    carry.
    Bit 0 of the contents is copied to carry flag and carry is copied to bit 7.

    16 cycles

    Flags affected:
    - z: Set if result is 0, else reset
    - n: 0
    - h: 0
    - c: Bit 0 of contents
*/

int Emulator::RR_HL() {

    BYTE data = this->readMem(this->regHL.regstr);
    BYTE bit0 = data & 0b1;

    // Shift data right and copy old carry flag to bit 7
    data >>= 1;
    data |= ((this->isBitSet(this->regAF.low, FLAG_CARRY) ? 0b1 : 0b0) << 7);

    // Store result back into memory
    this->writeMem(this->regHL.regstr, data);

    // Reset subtract and half carry flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    this->regAF.low = (data == 0x0) 
        ? this->bitSet(this->regAF.low, FLAG_ZERO) 
        : this->bitReset(this->regAF.low, FLAG_ZERO);

    // Copy bit 0 into carry flag
    this->regAF.low = this->isBitSet(bit0, 0) 
        ? this->bitSet(this->regAF.low, FLAG_CARRY)
        : this->bitReset(this->regAF.low, FLAG_CARRY);

    return 16;

}

/*
    SLA r  (CB 0x2X)

    An arithmetic left shift of contents in 8 bit register r.
    Bit 7 of contents is copied into carry flag.
    r can be registers A, B, C, D, E, H or L.

    8 cycles
    
    Flags affected:
    - z: Set if result is 0, else reset
    - n: 0
    - h: 0
    - c: Bit 7 of contents
*/
int Emulator::SLA_r(BYTE& r) {

    BYTE data = r;
    BYTE bit7 = data >> 7;

    // Shift data left and store it in r
    data <<= 1;
    r = data;

    // Reset subtract and half carry flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    this->regAF.low = (data == 0x0) 
        ? this->bitSet(this->regAF.low, FLAG_ZERO) 
        : this->bitReset(this->regAF.low, FLAG_ZERO);

    // Copy bit 7 into carry flag
    this->regAF.low = this->isBitSet(bit7, 0) 
        ? this->bitSet(this->regAF.low, FLAG_CARRY)
        : this->bitReset(this->regAF.low, FLAG_CARRY);

    return 8;

}

/*
    SLA (HL)  (CB 0x26)

    An arithmetic left shift of contents stored at memory location HL.
    Bit 7 of the contents is copied to carry flag.

    16 cycles

    Flags affected:
    - z: Set if result is 0, else reset
    - n: 0
    - h: 0
    - c: Bit 7 of contents
*/
int Emulator::SLA_HL() {

    BYTE data = this->readMem(this->regHL.regstr);
    BYTE bit7 = data >> 7;

    // Shift data left and store it back into memory
    data <<= 1;
    this->writeMem(this->regHL.regstr, data);

    // Reset subtract and half carry flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    this->regAF.low = (data == 0x0) 
        ? this->bitSet(this->regAF.low, FLAG_ZERO) 
        : this->bitReset(this->regAF.low, FLAG_ZERO);

    // Copy bit 7 into carry flag
    this->regAF.low = this->isBitSet(bit7, 0) 
        ? this->bitSet(this->regAF.low, FLAG_CARRY)
        : this->bitReset(this->regAF.low, FLAG_CARRY);

    return 16;

}

/*
    SWAP r  (CB 0x3X)

    Swap the lower and upper nibble of contents in register r.
    r can be registers A, B, C, D, E, H or L.

    8 cycles

    Flags affected:
    - z: Set if result is 0, else reset
    - n: 0
    - h: 0
    - c: 0
*/
int Emulator::SWAP_r(BYTE& r) {

    BYTE data = r;
    BYTE lowerNibble = data & 0x0F;
    BYTE upperNibble = (data & 0xF0) >> 4;

    // Swap the nibbles in data and store it in r
    data = (lowerNibble << 4) | upperNibble;
    r = data;

    // Reset subtract, halfcarry and carry flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_HALFCARRY);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_CARRY);

    // Update zero flag
    this->regAF.low = (data == 0x0) 
        ? this->bitSet(this->regAF.low, FLAG_ZERO) 
        : this->bitReset(this->regAF.low, FLAG_ZERO);

    return 8;

}

/*
    SWAP (HL)  (CB 0x36)

    Swap the lower and upper nibble of contents at memory location HL.

    16 cycles

    Flags affected:
    - z: Set if result is 0, else reset
    - n: 0
    - h: 0
    - c: 0
*/
int Emulator::SWAP_HL() {

    BYTE data = this->readMem(this->regHL.regstr);
    BYTE lowerNibble = data & 0x0F;
    BYTE upperNibble = (data & 0xF0) >> 4;

    // Swap the nibbles in data and store it back into memory
    data = (lowerNibble << 4) | upperNibble;
    this->writeMem(this->regHL.regstr, data);

    // Reset subtract, halfcarry and carry flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_HALFCARRY);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_CARRY);

    // Update zero flag
    this->regAF.low = (data == 0x0) 
        ? this->bitSet(this->regAF.low, FLAG_ZERO) 
        : this->bitReset(this->regAF.low, FLAG_ZERO);

    return 16;

}

/*
    SRA r  (CB 0x2X)

    An arithmetic right shift of contents in 8 bit register r.
    Bit 0 of contents is copied into carry flag.
    MSB does not change.
    r can be registers A, B, C, D, E, H or L.

    8 cycles
    
    Flags affected:
    - z: Set if result is 0, else reset
    - n: 0
    - h: 0
    - c: Bit 0 of contents
*/
int Emulator::SRA_r(BYTE& r) {

    BYTE data = r;
    BYTE bit0 = data & 0b1;

    // Shift data right, persist bit 7, and store it in r
    data = (data >> 1) | (data & 0x80);
    r = data;

    // Reset subtract and half carry flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    this->regAF.low = (data == 0x0) 
        ? this->bitSet(this->regAF.low, FLAG_ZERO) 
        : this->bitReset(this->regAF.low, FLAG_ZERO);

    // Copy bit 0 into carry flag
    this->regAF.low = this->isBitSet(bit0, 0) 
        ? this->bitSet(this->regAF.low, FLAG_CARRY)
        : this->bitReset(this->regAF.low, FLAG_CARRY);

    return 8;

}

/*
    SRA (HL)  (CB 0x2E)

    An arithmetic right shift of contents stored at memory location HL.
    Bit 0 of the contents is copied to carry flag.
    MSB does not change.

    16 cycles

    Flags affected:
    - z: Set if result is 0, else reset
    - n: 0
    - h: 0
    - c: Bit 0 of contents
*/
int Emulator::SRA_HL() {

    BYTE data = this->readMem(this->regHL.regstr);
    BYTE bit0 = data & 0b1;

    // Shift data right, persist bit 7, and store it back into memory
    data = (data >> 1) | (data & 0x80);
    this->writeMem(this->regHL.regstr, data);

    // Reset subtract and half carry flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    this->regAF.low = (data == 0x0) 
        ? this->bitSet(this->regAF.low, FLAG_ZERO) 
        : this->bitReset(this->regAF.low, FLAG_ZERO);

    // Copy bit 0 into carry flag
    this->regAF.low = this->isBitSet(bit0, 0) 
        ? this->bitSet(this->regAF.low, FLAG_CARRY)
        : this->bitReset(this->regAF.low, FLAG_CARRY);

    return 16;

}

/*
    SRL r  (CB 0x3X)

    The contents of 8 bit register r are shifted 1 bit right.
    Bit 0 of the register is copied to carry flag
    Bit 7 is reset.
    r can be registers A, B, C, D, E, H or L.

    8 cycles

    Flags affected:
    - z: Set if result is 0, else reset
    - n: 0
    - h: 0
    - c: Bit 0 of register
*/
int Emulator::SRL_r(BYTE& r) {

    BYTE data = r;
    BYTE bit0 = data & 0b1;

    // Shift data right, reset bit 7
    data >>= 1;
    data = this->bitReset(data, 7);

    // Store result back into r
    r = data;

    // Reset subtract and half carry flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    this->regAF.low = (data == 0x0) 
        ? this->bitSet(this->regAF.low, FLAG_ZERO) 
        : this->bitReset(this->regAF.low, FLAG_ZERO);

    // Copy bit 0 into carry flag
    this->regAF.low = this->isBitSet(bit0, 0) 
        ? this->bitSet(this->regAF.low, FLAG_CARRY)
        : this->bitReset(this->regAF.low, FLAG_CARRY);

    return 8;

}

/*
    SRL (HL)  (CB 0x3E)

    The contents stored at memory location HL is shifted 1 bit right.
    Bit 0 of the contents is copied to carry flag.
    Bit 7 is reset.

    16 cycles

    Flags affected:
    - z: Set if result is 0, else reset
    - n: 0
    - h: 0
    - c: Bit 0 of contents
*/
int Emulator::SRL_HL() {

    BYTE data = this->readMem(this->regHL.regstr);
    BYTE bit0 = data & 0b1;

    // Shift data right, reset bit 7
    data >>= 1;
    data = this->bitReset(data, 7);

    // Store result back into memory
    this->writeMem(this->regHL.regstr, data);

    // Reset subtract and half carry flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    this->regAF.low = (data == 0x0) 
        ? this->bitSet(this->regAF.low, FLAG_ZERO) 
        : this->bitReset(this->regAF.low, FLAG_ZERO);

    // Copy bit 0 into carry flag
    this->regAF.low = this->isBitSet(bit0, 0) 
        ? this->bitSet(this->regAF.low, FLAG_CARRY)
        : this->bitReset(this->regAF.low, FLAG_CARRY);

    return 16;

}

/*
********************************************************************************
Single Bit Operation Commands
********************************************************************************
*/

/*
    BIT n, r  (CB 0xXX)

    Test bit n in register r and sets the zero flag accordingly.

    8 cycles

    Flags affected:
    - z: Set if bit n of register r is 0, else reset
    - n: 0
    - h: 1
    - c: -
*/
int Emulator::BIT_n_r(BYTE& r, int n) {

    // Update zero flag
    this->regAF.low = this->isBitSet(r, n) 
        ? this->bitReset(this->regAF.low, FLAG_ZERO)
        : this->bitSet(this->regAF.low, FLAG_ZERO);
    
    // Reset subtract flag, set halfcarry flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);
    this->regAF.low = this->bitSet(this->regAF.low, FLAG_HALFCARRY);

    return 8;

}

/*
    BIT n, (HL)  (CB 0xXX)

    Test bit n of contents at memory location HL and sets the zero flag 
    accordingly.

    12 cycles

    Flags affected:
    - z: Set if bit n of contents is 0, else reset
    - n: 0
    - h: 1
    - c: -
*/
int Emulator::BIT_n_HL(int n) {

    // Update zero flag
    this->regAF.low = this->isBitSet(this->readMem(this->regHL.regstr), n) 
        ? this->bitReset(this->regAF.low, FLAG_ZERO)
        : this->bitSet(this->regAF.low, FLAG_ZERO);
    
    // Reset subtract flag, set halfcarry flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);
    this->regAF.low = this->bitSet(this->regAF.low, FLAG_HALFCARRY);

    return 12;

}

/*
    SET n, r  (CB 0xXX)

    Set bit n of register r.
    r can be registers A, B, C, D, E, H or L.

    8 cycles

    Flags affected (znhc): ----
*/
int Emulator::SET_n_r(BYTE& r, int n) {

    r = this->bitSet(r, n);

    return 8;

}

/*
    SET n, (HL)  (CB 0xXX)

    Set bit n of contents at memory location HL.

    16 cycles

    Flags affected (znhc): ----
*/
int Emulator::SET_n_HL(int n) {

    // this works for utility function!
    BYTE result = this->bitSet(this->readMem(this->regHL.low), n);
    this->writeMem(this->regHL.low, result);

    return 16;

}

/*
    RES n, r  (CB 0xXX)

    Reset bit n of register r.
    r can be registers A, B, C, D, E, H or L.

    8 cycles

    Flags affected (znhc): ----
*/
int Emulator::RES_n_r(BYTE& r, int n) {

    r = this->bitReset(r, n);

    return 8;

}

/*
    RES n, (HL)  (CB 0xXX)

    Reset bit n of contents at memory location HL.

    16 cycles

    Flags affected (znhc): ----
*/
int Emulator::RES_n_HL(int n) {

    // this works for utility function!
    BYTE result = this->bitReset(this->readMem(this->regHL.low), n);
    this->writeMem(this->regHL.low, result);

    return 16;

}

/*
********************************************************************************
CPU Control Commands
********************************************************************************
*/

/*
    CCF  (0x3F)

    Complement carry flag. If carry is set, reset it, vice versa.

    4 cycles

    Flags affected: 
    - z: -
    - n: 0
    - h: 0
    - c: Set if reset, reset if set.
*/
int Emulator::CCF() {

    // Reset subtract and halfcarry flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_HALFCARRY);

    // Toggling carry flag
    this->regAF.low = this->isBitSet(this->regAF.low, FLAG_CARRY)
        ? this->bitReset(this->regAF.low, FLAG_CARRY)
        : this->bitSet(this->regAF.low, FLAG_CARRY);

    return 4;

}

/*
    SCF  (0x37)

    Set carry flag.

    4 cycles

    Flags affected:
    - z: -
    - n: 0
    - h: 0
    - c: 1
*/
int Emulator::SCF() {
    
    // Reset subtract and halfcarry flag
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_SUB);
    this->regAF.low = this->bitReset(this->regAF.low, FLAG_HALFCARRY);

    // Set carry flag
    this->regAF.low = this->bitSet(this->regAF.low, FLAG_CARRY);

    return 4;

}

/*
    NOP  (0x00)

    No operation.

    4 cycles

    Flags affected (znhc): ----
*/
int Emulator::NOP() {

    // No flags affected
    cout << "NOP" << endl;
    return 4;

}

/*
    HALT  (0x76)

    4 cycles

    Flags affect (znhc): ----
*/
int Emulator::HALT() {

    this->isHalted = true;

    // Different docs say different things, we follow the gameboy manual.
    return 4;

}

/*
    STOP  (0x10 00)

    Functionally similar to HALT.

    4 cycles

    Flags affected (znhc): ----
*/
int Emulator::STOP() {
    
    return this->HALT();

}

/*
    DI  (0xF3)

    Disable interrupts, IME = 0.

    4 cycles

    Flags affected (znhc): ----
*/
int Emulator::DI() {
    
    this->InterruptMasterEnabled = false;

    return 4;

}

/*
    EI  (0xFB)

    Enable interrupts, IME = 1.

    4 cycles

    Flags affected (znhc): ----
*/
int Emulator::EI() {

    this->InterruptMasterEnabled = true;

    return 4;

}

/*
********************************************************************************
Jump Commands
********************************************************************************
*/

/*
    JP nn  (0xC3)

    Jump to nn, PC = nn. (LS byte first)

    16 cycles

    Flags affected (znhc): ----
*/
int Emulator::JP_nn() {

    WORD lowByte = this->readMem(this->programCounter.regstr);
    WORD highByte = this->readMem(this->programCounter.regstr + 1);

    this->programCounter.regstr = (highByte << 8) | lowByte;

    cout << "JP_nn" << endl;
    return 16;

}

/*
    JP nn  (0xE9)

    Jump to HL, PC = HL.

    4 cycles

    Flags affected (znhc): ----
*/
int Emulator::JP_HL() {

    this->programCounter.regstr = this->regHL.regstr;

    return 4;

}

/*
    JP f, nn (0xXX)

    Jump to address nn if following condition is true:
    f = NZ, Jump if Z flag is reset.
    f = Z, Jump if Z flag is set.
    f = NC, Jump if C flag is reset.
    f = C, Jump if C flag is set.

    16 cycles if taken
    12 cycles if not taken

    Flags affected (znhc): ----

*/
int Emulator::JP_f_nn(BYTE opcode) {

    // Get nn
    WORD lowByte = this->readMem(this->programCounter.regstr);
    WORD highByte = this->readMem(this->programCounter.regstr + 1);
    WORD nn = (highByte << 8) | lowByte;

    bool jump = false;
    switch ((opcode >> 3) & 0x03) {
        case 0x00: // NZ
            jump = !(this->isBitSet(this->regAF.low, FLAG_ZERO));
            break;
        case 0x01: // Z
            jump = this->isBitSet(this->regAF.low, FLAG_ZERO);
            break;
        case 0x02: // NC
            jump = !(this->isBitSet(this->regAF.low, FLAG_CARRY));
            break;
        case 0x03: // C
            jump = this->isBitSet(this->regAF.low, FLAG_CARRY);
            break;
    }

    if (jump) {
        this->programCounter.regstr = nn;
        return 16;
    } else {
        this->programCounter.regstr += 2;
        return 12;
    }

}

/*
    JR PC+dd  (0x18)

    Jump relative to the offset dd.
    dd is an 8 bit signed number. Read from memory at PC.

    12 cycles

    Flags affected (znhc): ----
*/
int Emulator::JR_PCdd() {

    SIGNED_BYTE dd = static_cast<SIGNED_BYTE>(this->readMem(this->programCounter.regstr));
    this->programCounter.regstr++;

    this->programCounter.regstr += dd;

    return 12;

}

/*
    JR f, PC+dd  (0xXX)

    Jump relative to the offset dd if following condition is true:
    f = NZ, Jump if Z flag is reset.
    f = Z, Jump if Z flag is set.
    f = NC, Jump if C flag is reset.
    f = C, Jump if C flag is set.    
    dd is an 8 bit signed number. Read from memory at PC.

    12 cycles if taken
    8 cycles if not taken

*/
int Emulator::JR_f_PCdd(BYTE opcode) {

    SIGNED_BYTE dd = static_cast<SIGNED_BYTE>(this->readMem(this->programCounter.regstr));
    this->programCounter.regstr++;

    bool jump = false;
    switch ((opcode >> 3) & 0x03) {
        case 0x00: // NZ
            jump = !(this->isBitSet(this->regAF.low, FLAG_ZERO));
            break;
        case 0x01: // Z
            jump = this->isBitSet(this->regAF.low, FLAG_ZERO);
            break;
        case 0x02: // NC
            jump = !(this->isBitSet(this->regAF.low, FLAG_CARRY));
            break;
        case 0x03: // C
            jump = this->isBitSet(this->regAF.low, FLAG_CARRY);
            break;
    }

    if (jump) {
        this->programCounter.regstr += dd;
        return 12;
    } else {
        return 8;
    }

}

/*
    CALL nn  (0xCD)

    Pushes the PC to SP, then sets PC to target address nn.

    24 cycles

    Flags affected (znhc): ----
*/
int Emulator::CALL_nn() {

    // Get nn
    WORD lowByte = this->readMem(this->programCounter.regstr);
    WORD highByte = this->readMem(this->programCounter.regstr + 1);
    this->programCounter.regstr += 2;
    WORD nn = (highByte << 8) | lowByte;

    // Push PC onto stack
    this->stackPointer.regstr--;
    this->writeMem(this->stackPointer.regstr, this->programCounter.high);
    this->stackPointer.regstr--;
    this->writeMem(this->stackPointer.regstr, this->programCounter.low);    

    // Set PC to nn
    this->programCounter.regstr = nn;

    return 24;

}

/*
    CALL f, nn  (0xXX)

    CALL_nn if following condition is true:
    f = NZ, Jump if Z flag is reset.
    f = Z, Jump if Z flag is set.
    f = NC, Jump if C flag is reset.
    f = C, Jump if C flag is set.    

    24 cycles if taken
    12 cycles if not taken

    Flags affected (znhc): ----
*/
int Emulator::CALL_f_nn(BYTE opcode) {

    // Get nn
    WORD lowByte = this->readMem(this->programCounter.regstr);
    WORD highByte = this->readMem(this->programCounter.regstr + 1);
    this->programCounter.regstr += 2;
    WORD nn = (highByte << 8) | lowByte;

    bool call = false;
    switch ((opcode >> 3) & 0x03) {
        case 0x00: // NZ
            call = !(this->isBitSet(this->regAF.low, FLAG_ZERO));
            break;
        case 0x01: // Z
            call = this->isBitSet(this->regAF.low, FLAG_ZERO);
            break;
        case 0x02: // NC
            call = !(this->isBitSet(this->regAF.low, FLAG_CARRY));
            break;
        case 0x03: // C
            call = this->isBitSet(this->regAF.low, FLAG_CARRY);
            break;
    }

    if (call) {

        // Push PC onto stack
        this->stackPointer.regstr--;
        this->writeMem(this->stackPointer.regstr, this->programCounter.high); 
        this->stackPointer.regstr--;
        this->writeMem(this->stackPointer.regstr, this->programCounter.low);    

        // Set PC to nn
        this->programCounter.regstr = nn;

        return 24;

    } else {
        return 12;
    }

}

/*
    RET  (0xC9)

    Return from subroutine.
    Pop two bytes from the stack and jump to that address.

    16 cycles

    Flags affected (znhc): ----
*/
int Emulator::RET() {

    // Pop address from stack
    WORD lowByte = this->readMem(this->stackPointer.regstr);
    this->stackPointer.regstr++;
    WORD highByte = this->readMem(this->stackPointer.regstr);
    this->stackPointer.regstr++;

    // Set PC to address
    this->programCounter.regstr = (highByte << 8) | lowByte;

    return 16;

}

/*
    RET f  (0xXX)

    Conditional return from subroutine.
    RET if following condition is true:
    f = NZ, Jump if Z flag is reset.
    f = Z, Jump if Z flag is set.
    f = NC, Jump if C flag is reset.
    f = C, Jump if C flag is set.    

    20 cycles if taken
    8 cycles if not taken

    Flags affected (znhc): ----
*/
int Emulator::RET_f(BYTE opcode) {

    bool doRET = false;
    switch ((opcode >> 3) & 0x03) {
        case 0x00: // NZ
            doRET = !(this->isBitSet(this->regAF.low, FLAG_ZERO));
            break;
        case 0x01: // Z
            doRET = this->isBitSet(this->regAF.low, FLAG_ZERO);
            break;
        case 0x02: // NC
            doRET = !(this->isBitSet(this->regAF.low, FLAG_CARRY));
            break;
        case 0x03: // C
            doRET = this->isBitSet(this->regAF.low, FLAG_CARRY);
            break;
    }

    if (doRET) {

        // Pop address from stack
        WORD lowByte = this->readMem(this->stackPointer.regstr);
        this->stackPointer.regstr++;
        WORD highByte = this->readMem(this->stackPointer.regstr);
        this->stackPointer.regstr++;

        // Set PC to address
        this->programCounter.regstr = (highByte << 8) | lowByte;

        return 20;

    } else {
        return 8;
    }

}

/*
    RETI  (0xD9)

    Return and enable interrupts.

    16 cycles

    Flags affected (znhc): ----
*/
int Emulator::RETI() {

    // Pop address from stack
    WORD lowByte = this->readMem(this->stackPointer.regstr);
    this->stackPointer.regstr++;
    WORD highByte = this->readMem(this->stackPointer.regstr);
    this->stackPointer.regstr++;

    // Set PC to address
    this->programCounter.regstr = (highByte << 8) | lowByte;

    // Enable interrupts
    this->InterruptMasterEnabled = true;

    return 16;

}

/*
    RST n  (0xXX)

    Push present address onto stack, jump to address 0x0000 + n.
    Opcode:
    11ttt111

    000 0x00
    001 0x08
    010 0x10
    011 0x18
    100 0x20
    101 0x28
    110 0x30
    111 0x38

    16 cycles

    Flags affected (znhc): ----
*/
int Emulator::RST_n(BYTE opcode) {

    // Push PC onto stack
    this->stackPointer.regstr--;
    this->writeMem(this->stackPointer.regstr, this->programCounter.high);
    this->stackPointer.regstr--;
    this->writeMem(this->stackPointer.regstr, this->programCounter.low);    

    // Set PC to n
    BYTE t = ((opcode >> 3) & 0x07);
    this->programCounter.regstr = (WORD)(t * 0x08);

    return 16;
    
}