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

    regAF.regstr = 0x01B0; 
    regBC.regstr = 0x0013; 
    regDE.regstr = 0x00D8;
    regHL.regstr = 0x014D;
    stackPointer.regstr = 0xFFFE;

    // After the 256 bytes of bootROM is executed, PC lands at 0x100 
    // bootROM is not implemented for this emulator
    programCounter.regstr = 0x100; 

    internalMem[0xFF05] = 0x00; // TIMA
    internalMem[0xFF06] = 0x00; // TMA
    internalMem[0xFF07] = 0x00; // TAC
    internalMem[0xFF10] = 0x80; 
    internalMem[0xFF11] = 0xBF; 
    internalMem[0xFF12] = 0xF3; 
    internalMem[0xFF14] = 0xBF; 
    internalMem[0xFF16] = 0x3F; 
    internalMem[0xFF17] = 0x00; 
    internalMem[0xFF19] = 0xBF; 
    internalMem[0xFF1A] = 0x7F; 
    internalMem[0xFF1B] = 0xFF; 
    internalMem[0xFF1C] = 0x9F; 
    internalMem[0xFF1E] = 0xBF; 
    internalMem[0xFF20] = 0xFF; 
    internalMem[0xFF21] = 0x00; 
    internalMem[0xFF22] = 0x00; 
    internalMem[0xFF23] = 0xBF; 
    internalMem[0xFF24] = 0x77; 
    internalMem[0xFF25] = 0xF3;
    internalMem[0xFF26] = 0xF1; 
    internalMem[0xFF40] = 0x91; // LCDC - LCD control register
    internalMem[0xFF42] = 0x00; // SCY - scroll Y
    internalMem[0xFF43] = 0x00; // SCX - scroll X
    internalMem[0xFF45] = 0x00; // LYC - LY Compare
    internalMem[0xFF47] = 0xFC; // BGP - background colour palette
    internalMem[0xFF48] = 0xFF; // OBP0 - Object Palette 0 (Sprites)
    internalMem[0xFF49] = 0xFF; // OBP1 - Object Palette 1 (Sprites)
    internalMem[0xFF4A] = 0x00; // WX - window X
    internalMem[0xFF4B] = 0x00; // WY - window Y
    internalMem[0xFFFF] = 0x00; // IE - Interrupt enable

    MBC1 = false;
    MBC2 = false;

    // Choosing which MBC to use
    switch (cartridgeMem[0x147]) {
        case 0 : break; // No memory swapping needed
        case 1 : MBC1 = true ; break;
        case 2 : MBC1 = true ; break;
        case 3 : MBC1 = true ; break;
        case 5 : MBC2 = true ; break;
        case 6 : MBC2 = true ; break;
        default : break; 
    }

    currentROMBank = 1;

    memset(RAMBanks, 0, sizeof(RAMBanks));
    currentRAMBank = 0;

    // Initialize timers. Initial clock speed is 4096hz
    timerUpdateConstant = 1024;
    timerCounter = 1024;
    dividerCounter = 0;

    // Interrupts
    InterruptMasterEnabled = false;
    isHalted = false;

    // Joypad
    joypadState = 0xFF;

    // Graphics
    scanlineCycleCount = 456;
    doRenderPtr = nullptr;
    memset(displayPixels, 0, sizeof(displayPixels));

}

bool Emulator::loadGame(string file_path) {

    memset(cartridgeMem, 0, sizeof(cartridgeMem));
    
    // //load in the game
    // FILE* in;
    // in = fopen(file_path.c_str(), "rb") ;

    // // check rom exists
    // if (in == NULL) {
    //     printf("Cannot load game file ERROR!!!");
    //     return false;
    // }

    // // Get file size
    // fseek(in, 0, SEEK_END);
    // int fileSize = ftell(in);
    // rewind(in);
    
    // // Read file into cartridgeMem
    // fread(cartridgeMem, fileSize, 1, in);
    // fclose(in);

    // // Copy ROM Banks 0 & 1 to internal memory
    // memcpy(internalMem, cartridgeMem, 0x8000) ; // this is read only and never changes

        // read rom into vector
    ifstream file(file_path.c_str(), ios::binary);
    // assert(file.good());

    // resize & read rom
    file.seekg(0, ios::end);
    streampos size = file.tellg();
    file.seekg(0, ios::beg);
    file.read(reinterpret_cast<char *>(&cartridgeMem[0]), size);
    copy_n(&cartridgeMem[0], 0x8000, &internalMem[0]);

    return true;
}

void Emulator::update() { // MAIN UPDATE LOOP

    // update function called 60 times per second -> screen rendered @ 60fps

    const int maxCycles = 70224;
    int cyclesCount = 0;
    int cycles;

    while (cyclesCount < maxCycles) {

        int cycles = executeNextOpcode(); //executeNextOpcode will return the number of cycles taken
        cyclesCount += cycles;

        updateTimers(cycles);
        updateGraphics(cycles);
        handleInterrupts();

    }

}

void Emulator::setRenderGraphics(void(*funcPtr)()) {
    doRenderPtr = funcPtr;
}

int Emulator::executeNextOpcode() {
    int clockCycles;

    BYTE opcode = readMem(programCounter.regstr);

    // if (opcode != 0) {
    //     //cout << "opcode: " << hex << (int) opcode << endl;
    // }

    if (isHalted) {
        clockCycles = NOP();
    } else {
        programCounter.regstr++;
        clockCycles = executeOpcode(opcode);
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
        case 0x40: cycles = LD_r_R(regBC.high, regBC.high); break;
        case 0x41: cycles = LD_r_R(regBC.high, regBC.low); break;
        case 0x42: cycles = LD_r_R(regBC.high, regDE.high); break;
        case 0x43: cycles = LD_r_R(regBC.high, regDE.low); break;
        case 0x44: cycles = LD_r_R(regBC.high, regHL.high); break;
        case 0x45: cycles = LD_r_R(regBC.high, regHL.low); break;
        case 0x46: cycles = LD_r_HL(regBC.high); break;
        case 0x47: cycles = LD_r_R(regBC.high, regAF.high); break;

        // Load C, R/(HL)
        case 0x48: cycles = LD_r_R(regBC.low, regBC.high); break;
        case 0x49: cycles = LD_r_R(regBC.low, regBC.low); break;
        case 0x4A: cycles = LD_r_R(regBC.low, regDE.high); break;
        case 0x4B: cycles = LD_r_R(regBC.low, regDE.low); break;
        case 0x4C: cycles = LD_r_R(regBC.low, regHL.high); break;
        case 0x4D: cycles = LD_r_R(regBC.low, regHL.low); break;
        case 0x4E: cycles = LD_r_HL(regBC.low); break;
        case 0x4F: cycles = LD_r_R(regBC.low, regAF.high); break;

        // Load D, R/(HL)
        case 0x50: cycles = LD_r_R(regDE.high, regBC.high); break;
        case 0x51: cycles = LD_r_R(regDE.high, regBC.low); break;
        case 0x52: cycles = LD_r_R(regDE.high, regDE.high); break;
        case 0x53: cycles = LD_r_R(regDE.high, regDE.low); break;
        case 0x54: cycles = LD_r_R(regDE.high, regHL.high); break;
        case 0x55: cycles = LD_r_R(regDE.high, regHL.low); break;
        case 0x56: cycles = LD_r_HL(regDE.high); break;
        case 0x57: cycles = LD_r_R(regDE.high, regAF.high); break;

        // Load E, R/(HL)
        case 0x58: cycles = LD_r_R(regDE.low, regBC.high); break;
        case 0x59: cycles = LD_r_R(regDE.low, regBC.low); break;
        case 0x5A: cycles = LD_r_R(regDE.low, regDE.high); break;
        case 0x5B: cycles = LD_r_R(regDE.low, regDE.low); break;
        case 0x5C: cycles = LD_r_R(regDE.low, regHL.high); break;
        case 0x5D: cycles = LD_r_R(regDE.low, regHL.low); break;
        case 0x5E: cycles = LD_r_HL(regDE.low); break;
        case 0x5F: cycles = LD_r_R(regDE.low, regAF.high); break;

        // Load H, R/(HL)
        case 0x60: cycles = LD_r_R(regHL.high, regBC.high); break;
        case 0x61: cycles = LD_r_R(regHL.high, regBC.low); break;
        case 0x62: cycles = LD_r_R(regHL.high, regDE.high); break;
        case 0x63: cycles = LD_r_R(regHL.high, regDE.low); break;
        case 0x64: cycles = LD_r_R(regHL.high, regHL.high); break;
        case 0x65: cycles = LD_r_R(regHL.high, regHL.low); break;
        case 0x66: cycles = LD_r_HL(regHL.high); break;
        case 0x67: cycles = LD_r_R(regHL.high, regAF.high); break;

        // Load L, R/(HL)
        case 0x68: cycles = LD_r_R(regHL.low, regBC.high); break;
        case 0x69: cycles = LD_r_R(regHL.low, regBC.low); break;
        case 0x6A: cycles = LD_r_R(regHL.low, regDE.high); break;
        case 0x6B: cycles = LD_r_R(regHL.low, regDE.low); break;
        case 0x6C: cycles = LD_r_R(regHL.low, regHL.high); break;
        case 0x6D: cycles = LD_r_R(regHL.low, regHL.low); break;
        case 0x6E: cycles = LD_r_HL(regHL.low); break;
        case 0x6F: cycles = LD_r_R(regHL.low, regAF.high); break;

        // Load (HL), R
        case 0x70: cycles = LD_HL_r(regBC.high); break;
        case 0x71: cycles = LD_HL_r(regBC.low); break;
        case 0x72: cycles = LD_HL_r(regDE.high); break;
        case 0x73: cycles = LD_HL_r(regDE.low); break;
        case 0x74: cycles = LD_HL_r(regHL.high); break;
        case 0x75: cycles = LD_HL_r(regHL.low); break;
        case 0x77: cycles = LD_HL_r(regAF.high); break;

        // Load A, R/(HL)
        case 0x78: cycles = LD_r_R(regAF.high, regBC.high); break;
        case 0x79: cycles = LD_r_R(regAF.high, regBC.low); break;
        case 0x7A: cycles = LD_r_R(regAF.high, regDE.high); break;
        case 0x7B: cycles = LD_r_R(regAF.high, regDE.low); break;
        case 0x7C: cycles = LD_r_R(regAF.high, regHL.high); break;
        case 0x7D: cycles = LD_r_R(regAF.high, regHL.low); break;
        case 0x7E: cycles = LD_r_HL(regAF.high); break;
        case 0x7F: cycles = LD_r_R(regAF.high, regAF.high); break;

        // Load R, n
        case 0x06: cycles = LD_r_n(regBC.high); break;
        case 0x0E: cycles = LD_r_n(regBC.low); break;
        case 0x16: cycles = LD_r_n(regDE.high); break;
        case 0x1E: cycles = LD_r_n(regDE.low); break;
        case 0x26: cycles = LD_r_n(regHL.high); break;
        case 0x2E: cycles = LD_r_n(regHL.low); break;
        case 0x36: cycles = LD_HL_n(); break;
        case 0x3E: cycles = LD_r_n(regAF.high); break;

        // Load A, RR
        case 0x0A: cycles = LD_A_BC(); break;
        case 0x1A: cycles = LD_A_DE(); break;

        // Load RR, A
        case 0x02: cycles = LD_BC_A(); break;
        case 0x12: cycles = LD_DE_A(); break;

        // Load A, nn
        case 0xFA: cycles = LD_A_nn(); break;

        // Load nn, A
        case 0xEA: cycles = LD_nn_A(); break;

        // Load A, FF00+n, FF00+c, vice versa
        case 0xF0: cycles = LD_A_FF00n(); break;
        case 0xE0: cycles = LD_FF00n_A(); break;
        case 0xF2: cycles = LD_A_FF00C(); break;
        case 0xE2: cycles = LD_FF00C_A(); break;

        // Load increment/decrement HL, A, vice versa
        case 0x22: cycles = LDI_HL_A(); break;
        case 0x2A: cycles = LDI_A_HL(); break;
        case 0x32: cycles = LDD_HL_A(); break;
        case 0x3A: cycles = LDD_A_HL(); break;

        /* 
        ************************************************************************
        16 bit Load Commands
        ************************************************************************
        */

        // Load rr, nn
        case 0x01: cycles = LD_rr_nn(regBC); break;
        case 0x11: cycles = LD_rr_nn(regDE); break;
        case 0x21: cycles = LD_rr_nn(regHL); break;
        case 0x31: cycles = LD_rr_nn(stackPointer); break;

        // Load SP, HL
        case 0xF9: cycles = LD_SP_HL(); break;

        // Load nn, SP
        case 0x08: cycles = LD_nn_SP(); break;

        // Push rr
        case 0xC5: cycles = PUSH_rr(regBC); break;
        case 0xD5: cycles = PUSH_rr(regDE); break;
        case 0xE5: cycles = PUSH_rr(regHL); break;
        case 0xF5: cycles = PUSH_rr(regAF); break;

        // Pop rr
        case 0xC1: cycles = POP_rr(regBC); break;
        case 0xD1: cycles = POP_rr(regDE); break;
        case 0xE1: cycles = POP_rr(regHL); break;
        case 0xF1: cycles = POP_rr(regAF); break;

        /* 
        ************************************************************************
        8 bit Arithmetic/Logical commands
        ************************************************************************
        */

        // Add A, r
        case 0x80: cycles = ADD_A_r(regBC.high); break;
        case 0x81: cycles = ADD_A_r(regBC.low); break;
        case 0x82: cycles = ADD_A_r(regDE.high); break;
        case 0x83: cycles = ADD_A_r(regDE.low); break;
        case 0x84: cycles = ADD_A_r(regHL.high); break;
        case 0x85: cycles = ADD_A_r(regHL.low); break;
        case 0x86: cycles = ADD_A_HL(); break;
        case 0x87: cycles = ADD_A_r(regAF.high); break;

        // Add A, n
        case 0xC6: cycles = ADD_A_n(); break;

        // ADC A, r
        case 0x88: cycles = ADC_A_r(regBC.high); break;
        case 0x89: cycles = ADC_A_r(regBC.low); break;
        case 0x8A: cycles = ADC_A_r(regDE.high); break;
        case 0x8B: cycles = ADC_A_r(regDE.low); break;
        case 0x8C: cycles = ADC_A_r(regHL.high); break;
        case 0x8D: cycles = ADC_A_r(regHL.low); break;
        case 0x8E: cycles = ADC_A_HL(); break;
        case 0x8F: cycles = ADC_A_r(regAF.high); break;

        // ADC A, n
        case 0xCE: cycles = ADC_A_n(); break;

        // Sub r
        case 0x90: cycles = SUB_r(regBC.high); break;
        case 0x91: cycles = SUB_r(regBC.low); break;
        case 0x92: cycles = SUB_r(regDE.high); break;
        case 0x93: cycles = SUB_r(regDE.low); break;
        case 0x94: cycles = SUB_r(regHL.high); break;
        case 0x95: cycles = SUB_r(regHL.low); break;
        case 0x96: cycles = SUB_HL(); break;
        case 0x97: cycles = SUB_r(regAF.high); break;

        // Sub n
        case 0xD6: cycles = SUB_n(); break;

        // SBC A, r
        case 0x98: cycles = SBC_A_r(regBC.high); break;
        case 0x99: cycles = SBC_A_r(regBC.low); break;
        case 0x9A: cycles = SBC_A_r(regDE.high); break;
        case 0x9B: cycles = SBC_A_r(regDE.low); break;
        case 0x9C: cycles = SBC_A_r(regHL.high); break;
        case 0x9D: cycles = SBC_A_r(regHL.low); break;
        case 0x9E: cycles = SBC_A_HL(); break;
        case 0x9F: cycles = SBC_A_r(regAF.high); break;  

        // SBC A, n
        case 0xDE: cycles = SBC_A_n(); break;

        // AND r
        case 0xA0: cycles = AND_r(regBC.high); break;
        case 0xA1: cycles = AND_r(regBC.low); break;
        case 0xA2: cycles = AND_r(regDE.high); break;
        case 0xA3: cycles = AND_r(regDE.low); break;
        case 0xA4: cycles = AND_r(regHL.high); break;
        case 0xA5: cycles = AND_r(regHL.low); break;
        case 0xA6: cycles = AND_HL();
        case 0xA7: cycles = AND_r(regAF.high); break;

        // AND n 
        case 0xE6: cycles = AND_n(); break;

        // XOR r
        case 0xA8: cycles = XOR_r(regBC.high); break;
        case 0xA9: cycles = XOR_r(regBC.low); break;
        case 0xAA: cycles = XOR_r(regDE.high); break;
        case 0xAB: cycles = XOR_r(regDE.low); break;
        case 0xAC: cycles = XOR_r(regHL.high); break;
        case 0xAD: cycles = XOR_r(regHL.low); break;
        case 0xAE: cycles = XOR_HL();
        case 0xAF: cycles = XOR_r(regAF.high); break;

        // XOR n
        case 0xEE: cycles = XOR_n(); break;

        // OR r
        case 0xB0: cycles = OR_r(regBC.high); break;
        case 0xB1: cycles = OR_r(regBC.low); break;
        case 0xB2: cycles = OR_r(regDE.high); break;
        case 0xB3: cycles = OR_r(regDE.low); break;
        case 0xB4: cycles = OR_r(regHL.high); break;
        case 0xB5: cycles = OR_r(regHL.low); break;
        case 0xB6: cycles = OR_HL();
        case 0xB7: cycles = OR_r(regAF.high); break;

        // OR n 
        case 0xF6: cycles = OR_n(); break;

        // CPr
        case 0xB8: cycles = CP_r(regBC.high); break;
        case 0xB9: cycles = CP_r(regBC.low); break;
        case 0xBA: cycles = CP_r(regDE.high); break;
        case 0xBB: cycles = CP_r(regDE.low); break;
        case 0xBC: cycles = CP_r(regHL.high); break;
        case 0xBD: cycles = CP_r(regHL.low); break;
        case 0xBE: cycles = CP_HL();
        case 0xBF: cycles = CP_r(regAF.high); break;

        // CP n
        case 0xFE: cycles = CP_n(); break;

        // INC r
        case 0x04: cycles = INC_r(regBC.high); break;
        case 0x14: cycles = INC_r(regDE.high); break;
        case 0x24: cycles = INC_r(regHL.high); break;
        case 0x34: cycles = INC_HL(); break;
        case 0x0C: cycles = INC_r(regBC.low); break; 
        case 0x1C: cycles = INC_r(regDE.low); break;
        case 0x2C: cycles = INC_r(regHL.low); break;
        case 0x3C: cycles = INC_r(regAF.high); break;  

        // DEC r
        case 0x05: cycles = DEC_r(regBC.high); break;
        case 0x15: cycles = DEC_r(regDE.high); break;
        case 0x25: cycles = DEC_r(regHL.high); break;
        case 0x35: cycles = DEC_HL(); break;
        case 0x0D: cycles = DEC_r(regBC.low); break; 
        case 0x1D: cycles = DEC_r(regDE.low); break;
        case 0x2D: cycles = DEC_r(regHL.low); break;
        case 0x3D: cycles = DEC_r(regAF.high); break;

        // DAA
        case 0x27: cycles = DAA(); break;

        // CPL
        case 0x2F: cycles = CPL(); break;

        /* 
        ************************************************************************
        16 bit Arithmetic/Logical commands
        ************************************************************************
        */

        // Add HL, rr
        case 0x09: cycles = ADD_HL_rr(regBC.regstr); break;
        case 0x19: cycles = ADD_HL_rr(regDE.regstr); break;
        case 0x29: cycles = ADD_HL_rr(regHL.regstr); break;
        case 0x39: cycles = ADD_HL_rr(stackPointer.regstr); break;

        // INC rr
        case 0x03: cycles = INC_rr(regBC.regstr); break;
        case 0x13: cycles = INC_rr(regDE.regstr); break;
        case 0x23: cycles = INC_rr(regHL.regstr); break;
        case 0x33: cycles = INC_rr(stackPointer.regstr); break;

        // DEC rr
        case 0x0B: cycles = DEC_rr(regBC.regstr); break;
        case 0x1B: cycles = DEC_rr(regDE.regstr); break;
        case 0x2B: cycles = DEC_rr(regHL.regstr); break;
        case 0x3B: cycles = DEC_rr(stackPointer.regstr); break;

        // Add SP, dd
        case 0xE8: cycles = ADD_SP_dd(); break;

        // Load HL, SP + dd
        case 0xF8: cycles = LD_HL_SPdd(); break;

        /* 
        ************************************************************************
        Rotate and Shift commands
        ************************************************************************
        */

        // Non CB-prefixed Rotate commands
        case 0x07: cycles = RLCA(); break; // RLCA        
        case 0x17: cycles = RLA(); break; // RLA
        case 0x0F: cycles = RRCA(); break; // RRCA
        case 0x1F: cycles = RRA(); break; // RRA

        /* 
        ************************************************************************
        CPU Control commands
        ************************************************************************
        */

        case 0x3F: cycles = CCF(); break; // CCF
        case 0x37: cycles = SCF(); break; // SCF
        case 0x00: cycles = NOP(); break; // NOP
        case 0x76: cycles = HALT(); break; // HALT
        case 0x10: cycles = STOP(); break; // STOP
        case 0xF3: cycles = DI(); break; // DI
        case 0xFB: cycles = EI(); break; // EI

        /* 
        ************************************************************************
        Jump commands
        ************************************************************************
        */

        // JP nn
        case 0xC3: cycles = JP_nn(); break;

        // JP HL
        case 0xE9: cycles = JP_HL(); break;

        // JP f, nn
        case 0xC2: cycles = JP_f_nn(opcode); break;
        case 0xCA: cycles = JP_f_nn(opcode); break;
        case 0xD2: cycles = JP_f_nn(opcode); break;
        case 0xDA: cycles = JP_f_nn(opcode); break;

        // JR PC + dd
        case 0x18: cycles = JR_PCdd(); break;

        // JR f, PC + dd
        case 0x20: cycles = JR_f_PCdd(opcode); break;
        case 0x28: cycles = JR_f_PCdd(opcode); break;
        case 0x30: cycles = JR_f_PCdd(opcode); break;
        case 0x38: cycles = JR_f_PCdd(opcode); break;

        // Call nn
        case 0xCD: cycles = CALL_nn(); break;

        // Call f, nn
        case 0xC4: cycles = CALL_f_nn(opcode); break;
        case 0xCC: cycles = CALL_f_nn(opcode); break;
        case 0xD4: cycles = CALL_f_nn(opcode); break;
        case 0xDC: cycles = CALL_f_nn(opcode); break;

        // RET
        case 0xC9: cycles = RET(); break;
        
        // RET f
        case 0xC0: cycles = RET_f(opcode); break; 
        case 0xC8: cycles = RET_f(opcode); break;
        case 0xD0: cycles = RET_f(opcode); break;
        case 0xD8: cycles = RET_f(opcode); break;

        // RETI
        case 0xD9: cycles = RETI(); break;

        // RST n
        case 0xC7: cycles = RST_n(opcode); break;
        case 0xD7: cycles = RST_n(opcode); break;
        case 0xE7: cycles = RST_n(opcode); break;
        case 0xF7: cycles = RST_n(opcode); break;
        case 0xCF: cycles = RST_n(opcode); break;
        case 0xDF: cycles = RST_n(opcode); break;
        case 0xEF: cycles = RST_n(opcode); break;
        case 0xFF: cycles = RST_n(opcode); break;

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
    
    BYTE opcode = readMem(programCounter.regstr);
    programCounter.regstr++;
    int cycles;

    switch (opcode) {

        /* 
        ************************************************************************
        Rotate and Shift commands
        ************************************************************************
        */

        // RLC r
        case 0x00: cycles = RLC_r(regBC.high); break;
        case 0x01: cycles = RLC_r(regBC.low); break;
        case 0x02: cycles = RLC_r(regDE.high); break;
        case 0x03: cycles = RLC_r(regDE.low); break;
        case 0x04: cycles = RLC_r(regHL.high); break;
        case 0x05: cycles = RLC_r(regHL.low); break;
        case 0x06: cycles = RLC_HL(); break;
        case 0x07: cycles = RLC_r(regAF.high); break;

        // RRC r
        case 0x08: cycles = RRC_r(regBC.high); break;
        case 0x09: cycles = RRC_r(regBC.low); break;
        case 0x0A: cycles = RRC_r(regDE.high); break;
        case 0x0B: cycles = RRC_r(regDE.low); break;
        case 0x0C: cycles = RRC_r(regHL.high); break;
        case 0x0D: cycles = RRC_r(regHL.low); break;
        case 0x0E: cycles = RRC_HL(); break;
        case 0x0F: cycles = RRC_r(regAF.high); break;

        // RL r
        case 0x10: cycles = RL_r(regBC.high); break;
        case 0x11: cycles = RL_r(regBC.low); break;
        case 0x12: cycles = RL_r(regDE.high); break;
        case 0x13: cycles = RL_r(regDE.low); break;
        case 0x14: cycles = RL_r(regHL.high); break;
        case 0x15: cycles = RL_r(regHL.low); break;
        case 0x16: cycles = RL_HL(); break;
        case 0x17: cycles = RL_r(regAF.high); break;

        // RR r
        case 0x18: cycles = RR_r(regBC.high); break;
        case 0x19: cycles = RR_r(regBC.low); break;
        case 0x1A: cycles = RR_r(regDE.high); break;
        case 0x1B: cycles = RR_r(regDE.low); break;
        case 0x1C: cycles = RR_r(regHL.high); break;
        case 0x1D: cycles = RR_r(regHL.low); break;
        case 0x1E: cycles = RR_HL(); break;
        case 0x1F: cycles = RR_r(regAF.high); break;

        // SLA r
        case 0x20: cycles = SLA_r(regBC.high); break;
        case 0x21: cycles = SLA_r(regBC.low); break;
        case 0x22: cycles = SLA_r(regDE.high); break;
        case 0x23: cycles = SLA_r(regDE.low); break;
        case 0x24: cycles = SLA_r(regHL.high); break;
        case 0x25: cycles = SLA_r(regHL.low); break;
        case 0x26: cycles = SLA_HL(); break;
        case 0x27: cycles = SLA_r(regAF.high); break;

        // SRA r
        case 0x28: cycles = SRA_r(regBC.high); break;
        case 0x29: cycles = SRA_r(regBC.low); break;
        case 0x2A: cycles = SRA_r(regDE.high); break;
        case 0x2B: cycles = SRA_r(regDE.low); break;
        case 0x2C: cycles = SRA_r(regHL.high); break;
        case 0x2D: cycles = SRA_r(regHL.low); break;
        case 0x2E: cycles = SRA_HL(); break;
        case 0x2F: cycles = SRA_r(regAF.high); break;

        // Swap r
        case 0x30: cycles = SWAP_r(regBC.high); break;
        case 0x31: cycles = SWAP_r(regBC.low); break;
        case 0x32: cycles = SWAP_r(regDE.high); break;
        case 0x33: cycles = SWAP_r(regDE.low); break;
        case 0x34: cycles = SWAP_r(regHL.high); break;
        case 0x35: cycles = SWAP_r(regHL.low); break;
        case 0x36: cycles = SWAP_HL(); break;
        case 0x37: cycles = SWAP_r(regAF.high); break;

        // SRL r
        case 0x38: cycles = SRL_r(regBC.high); break;
        case 0x39: cycles = SRL_r(regBC.low); break;
        case 0x3A: cycles = SRL_r(regDE.high); break;
        case 0x3B: cycles = SRL_r(regDE.low); break;
        case 0x3C: cycles = SRL_r(regHL.high); break;
        case 0x3D: cycles = SRL_r(regHL.low); break;
        case 0x3E: cycles = SRL_HL(); break;
        case 0x3F: cycles = SRL_r(regAF.high); break;

        /* 
        ************************************************************************
        Single bit operation commands
        ************************************************************************
        */   

        // BIT 0, r
        case 0x40: cycles = BIT_n_r(regBC.high, 0); break;
        case 0x41: cycles = BIT_n_r(regBC.low, 0); break;
        case 0x42: cycles = BIT_n_r(regDE.high, 0); break;
        case 0x43: cycles = BIT_n_r(regDE.low, 0); break;
        case 0x44: cycles = BIT_n_r(regHL.high, 0); break;
        case 0x45: cycles = BIT_n_r(regHL.low, 0); break;
        case 0x46: cycles = BIT_n_HL(0); break;
        case 0x47: cycles = BIT_n_r(regAF.high, 0); break;

        // BIT 1, r
        case 0x48: cycles = BIT_n_r(regBC.high, 1); break;
        case 0x49: cycles = BIT_n_r(regBC.low, 1); break;
        case 0x4A: cycles = BIT_n_r(regDE.high, 1); break;
        case 0x4B: cycles = BIT_n_r(regDE.low, 1); break;
        case 0x4C: cycles = BIT_n_r(regHL.high, 1); break;
        case 0x4D: cycles = BIT_n_r(regHL.low, 1); break;
        case 0x4E: cycles = BIT_n_HL(1); break;
        case 0x4F: cycles = BIT_n_r(regAF.high, 1); break;

        // BIT 2, r
        case 0x50: cycles = BIT_n_r(regBC.high, 2); break;
        case 0x51: cycles = BIT_n_r(regBC.low, 2); break;
        case 0x52: cycles = BIT_n_r(regDE.high, 2); break;
        case 0x53: cycles = BIT_n_r(regDE.low, 2); break;
        case 0x54: cycles = BIT_n_r(regHL.high, 2); break;
        case 0x55: cycles = BIT_n_r(regHL.low, 2); break;
        case 0x56: cycles = BIT_n_HL(2); break;
        case 0x57: cycles = BIT_n_r(regAF.high, 2); break;

        // BIT 3, r
        case 0x58: cycles = BIT_n_r(regBC.high, 3); break;
        case 0x59: cycles = BIT_n_r(regBC.low, 3); break;
        case 0x5A: cycles = BIT_n_r(regDE.high, 3); break;
        case 0x5B: cycles = BIT_n_r(regDE.low, 3); break;
        case 0x5C: cycles = BIT_n_r(regHL.high, 3); break;
        case 0x5D: cycles = BIT_n_r(regHL.low, 3); break;
        case 0x5E: cycles = BIT_n_HL(3); break;
        case 0x5F: cycles = BIT_n_r(regAF.high, 3); break;

        // BIT 4, r
        case 0x60: cycles = BIT_n_r(regBC.high, 4); break;
        case 0x61: cycles = BIT_n_r(regBC.low, 4); break;
        case 0x62: cycles = BIT_n_r(regDE.high, 4); break;
        case 0x63: cycles = BIT_n_r(regDE.low, 4); break;
        case 0x64: cycles = BIT_n_r(regHL.high, 4); break;
        case 0x65: cycles = BIT_n_r(regHL.low, 4); break;
        case 0x66: cycles = BIT_n_HL(4); break;
        case 0x67: cycles = BIT_n_r(regAF.high, 4); break;

        // BIT 5, r
        case 0x68: cycles = BIT_n_r(regBC.high, 5); break;
        case 0x69: cycles = BIT_n_r(regBC.low, 5); break;
        case 0x6A: cycles = BIT_n_r(regDE.high, 5); break;
        case 0x6B: cycles = BIT_n_r(regDE.low, 5); break;
        case 0x6C: cycles = BIT_n_r(regHL.high, 5); break;
        case 0x6D: cycles = BIT_n_r(regHL.low, 5); break;
        case 0x6E: cycles = BIT_n_HL(5); break;
        case 0x6F: cycles = BIT_n_r(regAF.high, 5); break;

        // BIT 6, r
        case 0x70: cycles = BIT_n_r(regBC.high, 6); break;
        case 0x71: cycles = BIT_n_r(regBC.low, 6); break;
        case 0x72: cycles = BIT_n_r(regDE.high, 6); break;
        case 0x73: cycles = BIT_n_r(regDE.low, 6); break;
        case 0x74: cycles = BIT_n_r(regHL.high, 6); break;
        case 0x75: cycles = BIT_n_r(regHL.low, 6); break;
        case 0x76: cycles = BIT_n_HL(6); break;
        case 0x77: cycles = BIT_n_r(regAF.high, 6); break;

        // BIT 7, r
        case 0x78: cycles = BIT_n_r(regBC.high, 7); break;
        case 0x79: cycles = BIT_n_r(regBC.low, 7); break;
        case 0x7A: cycles = BIT_n_r(regDE.high, 7); break;
        case 0x7B: cycles = BIT_n_r(regDE.low, 7); break;
        case 0x7C: cycles = BIT_n_r(regHL.high, 7); break;
        case 0x7D: cycles = BIT_n_r(regHL.low, 7); break;
        case 0x7E: cycles = BIT_n_HL(7); break;
        case 0x7F: cycles = BIT_n_r(regAF.high, 7); break;

        // RES 0, r
        case 0x80: cycles = RES_n_r(regBC.high, 0); break;
        case 0x81: cycles = RES_n_r(regBC.low, 0); break;
        case 0x82: cycles = RES_n_r(regDE.high, 0); break;
        case 0x83: cycles = RES_n_r(regDE.low, 0); break;
        case 0x84: cycles = RES_n_r(regHL.high, 0); break;
        case 0x85: cycles = RES_n_r(regHL.low, 0); break;
        case 0x86: cycles = RES_n_HL(0); break;
        case 0x87: cycles = RES_n_r(regAF.high, 0); break;

        // RES 1, r
        case 0x88: cycles = RES_n_r(regBC.high, 1); break;
        case 0x89: cycles = RES_n_r(regBC.low, 1); break;
        case 0x8A: cycles = RES_n_r(regDE.high, 1); break;
        case 0x8B: cycles = RES_n_r(regDE.low, 1); break;
        case 0x8C: cycles = RES_n_r(regHL.high, 1); break;
        case 0x8D: cycles = RES_n_r(regHL.low, 1); break;
        case 0x8E: cycles = RES_n_HL(1); break;
        case 0x8F: cycles = RES_n_r(regAF.high, 1); break;

        // RES 2, r
        case 0x90: cycles = RES_n_r(regBC.high, 2); break;
        case 0x91: cycles = RES_n_r(regBC.low, 2); break;
        case 0x92: cycles = RES_n_r(regDE.high, 2); break;
        case 0x93: cycles = RES_n_r(regDE.low, 2); break;
        case 0x94: cycles = RES_n_r(regHL.high, 2); break;
        case 0x95: cycles = RES_n_r(regHL.low, 2); break;
        case 0x96: cycles = RES_n_HL(2); break;
        case 0x97: cycles = RES_n_r(regAF.high, 2); break;

        // RES 3, r
        case 0x98: cycles = RES_n_r(regBC.high, 3); break;
        case 0x99: cycles = RES_n_r(regBC.low, 3); break;
        case 0x9A: cycles = RES_n_r(regDE.high, 3); break;
        case 0x9B: cycles = RES_n_r(regDE.low, 3); break;
        case 0x9C: cycles = RES_n_r(regHL.high, 3); break;
        case 0x9D: cycles = RES_n_r(regHL.low, 3); break;
        case 0x9E: cycles = RES_n_HL(3); break;
        case 0x9F: cycles = RES_n_r(regAF.high, 3); break;

        // RES 4, r
        case 0xA0: cycles = RES_n_r(regBC.high, 4); break;
        case 0xA1: cycles = RES_n_r(regBC.low, 4); break;
        case 0xA2: cycles = RES_n_r(regDE.high, 4); break;
        case 0xA3: cycles = RES_n_r(regDE.low, 4); break;
        case 0xA4: cycles = RES_n_r(regHL.high, 4); break;
        case 0xA5: cycles = RES_n_r(regHL.low, 4); break;
        case 0xA6: cycles = RES_n_HL(4); break;
        case 0xA7: cycles = RES_n_r(regAF.high, 4); break;

        // RES 5, r
        case 0xA8: cycles = RES_n_r(regBC.high, 5); break;
        case 0xA9: cycles = RES_n_r(regBC.low, 5); break;
        case 0xAA: cycles = RES_n_r(regDE.high, 5); break;
        case 0xAB: cycles = RES_n_r(regDE.low, 5); break;
        case 0xAC: cycles = RES_n_r(regHL.high, 5); break;
        case 0xAD: cycles = RES_n_r(regHL.low, 5); break;
        case 0xAE: cycles = RES_n_HL(5); break;
        case 0xAF: cycles = RES_n_r(regAF.high, 5); break;

        // RES 6, r
        case 0xB0: cycles = RES_n_r(regBC.high, 6); break;
        case 0xB1: cycles = RES_n_r(regBC.low, 6); break;
        case 0xB2: cycles = RES_n_r(regDE.high, 6); break;
        case 0xB3: cycles = RES_n_r(regDE.low, 6); break;
        case 0xB4: cycles = RES_n_r(regHL.high, 6); break;
        case 0xB5: cycles = RES_n_r(regHL.low, 6); break;
        case 0xB6: cycles = RES_n_HL(6); break;
        case 0xB7: cycles = RES_n_r(regAF.high, 6); break;

        // RES 7, r
        case 0xB8: cycles = RES_n_r(regBC.high, 7); break;
        case 0xB9: cycles = RES_n_r(regBC.low, 7); break;
        case 0xBA: cycles = RES_n_r(regDE.high, 7); break;
        case 0xBB: cycles = RES_n_r(regDE.low, 7); break;
        case 0xBC: cycles = RES_n_r(regHL.high, 7); break;
        case 0xBD: cycles = RES_n_r(regHL.low, 7); break;
        case 0xBE: cycles = RES_n_HL(7); break;
        case 0xBF: cycles = RES_n_r(regAF.high, 7); break;

        // SET 0, r
        case 0xC0: cycles = SET_n_r(regBC.high, 0); break;
        case 0xC1: cycles = SET_n_r(regBC.low, 0); break;
        case 0xC2: cycles = SET_n_r(regDE.high, 0); break;
        case 0xC3: cycles = SET_n_r(regDE.low, 0); break;
        case 0xC4: cycles = SET_n_r(regHL.high, 0); break;
        case 0xC5: cycles = SET_n_r(regHL.low, 0); break;
        case 0xC6: cycles = SET_n_HL(0); break;
        case 0xC7: cycles = SET_n_r(regAF.high, 0); break;

        // SET 1, r
        case 0xC8: cycles = SET_n_r(regBC.high, 1); break;
        case 0xC9: cycles = SET_n_r(regBC.low, 1); break;
        case 0xCA: cycles = SET_n_r(regDE.high, 1); break;
        case 0xCB: cycles = SET_n_r(regDE.low, 1); break;
        case 0xCC: cycles = SET_n_r(regHL.high, 1); break;
        case 0xCD: cycles = SET_n_r(regHL.low, 1); break;
        case 0xCE: cycles = SET_n_HL(1); break;
        case 0xCF: cycles = SET_n_r(regAF.high, 1); break;

        // SET 2, r
        case 0xD0: cycles = SET_n_r(regBC.high, 2); break;
        case 0xD1: cycles = SET_n_r(regBC.low, 2); break;
        case 0xD2: cycles = SET_n_r(regDE.high, 2); break;
        case 0xD3: cycles = SET_n_r(regDE.low, 2); break;
        case 0xD4: cycles = SET_n_r(regHL.high, 2); break;
        case 0xD5: cycles = SET_n_r(regHL.low, 2); break;
        case 0xD6: cycles = SET_n_HL(2); break;
        case 0xD7: cycles = SET_n_r(regAF.high, 2); break;

        // SET 3, r
        case 0xD8: cycles = SET_n_r(regBC.high, 3); break;
        case 0xD9: cycles = SET_n_r(regBC.low, 3); break;
        case 0xDA: cycles = SET_n_r(regDE.high, 3); break;
        case 0xDB: cycles = SET_n_r(regDE.low, 3); break;
        case 0xDC: cycles = SET_n_r(regHL.high, 3); break;
        case 0xDD: cycles = SET_n_r(regHL.low, 3); break;
        case 0xDE: cycles = SET_n_HL(3); break;
        case 0xDF: cycles = SET_n_r(regAF.high, 3); break;

        // SET 4, r
        case 0xE0: cycles = SET_n_r(regBC.high, 4); break;
        case 0xE1: cycles = SET_n_r(regBC.low, 4); break;
        case 0xE2: cycles = SET_n_r(regDE.high, 4); break;
        case 0xE3: cycles = SET_n_r(regDE.low, 4); break;
        case 0xE4: cycles = SET_n_r(regHL.high, 4); break;
        case 0xE5: cycles = SET_n_r(regHL.low, 4); break;
        case 0xE6: cycles = SET_n_HL(4); break;
        case 0xE7: cycles = SET_n_r(regAF.high, 4); break;

        // SET 5, r
        case 0xE8: cycles = SET_n_r(regBC.high, 5); break;
        case 0xE9: cycles = SET_n_r(regBC.low, 5); break;
        case 0xEA: cycles = SET_n_r(regDE.high, 5); break;
        case 0xEB: cycles = SET_n_r(regDE.low, 5); break;
        case 0xEC: cycles = SET_n_r(regHL.high, 5); break;
        case 0xED: cycles = SET_n_r(regHL.low, 5); break;
        case 0xEE: cycles = SET_n_HL(5); break;
        case 0xEF: cycles = SET_n_r(regAF.high, 5); break;

        // SET 6, r
        case 0xF0: cycles = SET_n_r(regBC.high, 6); break;
        case 0xF1: cycles = SET_n_r(regBC.low, 6); break;
        case 0xF2: cycles = SET_n_r(regDE.high, 6); break;
        case 0xF3: cycles = SET_n_r(regDE.low, 6); break;
        case 0xF4: cycles = SET_n_r(regHL.high, 6); break;
        case 0xF5: cycles = SET_n_r(regHL.low, 6); break;
        case 0xF6: cycles = SET_n_HL(6); break;
        case 0xF7: cycles = SET_n_r(regAF.high, 6); break;

        // SET 7, r
        case 0xF8: cycles = SET_n_r(regBC.high, 7); break;
        case 0xF9: cycles = SET_n_r(regBC.low, 7); break;
        case 0xFA: cycles = SET_n_r(regDE.high, 7); break;
        case 0xFB: cycles = SET_n_r(regDE.low, 7); break;
        case 0xFC: cycles = SET_n_r(regHL.high, 7); break;
        case 0xFD: cycles = SET_n_r(regHL.low, 7); break;
        case 0xFE: cycles = SET_n_HL(7); break;
        case 0xFF: cycles = SET_n_r(regAF.high, 7); break;
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
        WORD newAddress = (currentROMBank * 0x4000) + (address - 0x4000);
        return cartridgeMem[newAddress];
    } 
    
    // If reading from the switchable external RAM banking area
    else if ((address >= 0xA000) && (address <= 0xBFFF)) {
        // if in ROM mode, only RAM bank 0 can be accessed
        assert(ROMBanking == (currentRAMBank == 0));
        WORD newAddress = (currentRAMBank * 0x2000) + (address - 0xA000);
        return RAMBanks[newAddress];
    }
    
    // Since ECHO RAM is the same as Work RAM
    else if ((address >= 0xE000) && (address <= 0xFDFF)) {
        return internalMem[address - 0x2000];
    }

    // Unusable area
    else if ((address >= 0xFEA0) && (address <= 0xFEFF)) {
        return 0x00;
    }

    // Joypad Register
    else if (address == 0xFF00) { 
        return getJoypadState();
    }

    // else return what's in the memory
    return internalMem[address];

}

void Emulator::writeMem(WORD address, BYTE data) {

    // write attempts to ROM
    if (address < 0x8000) {
        //cout << "banking occured" << endl;
        handleBanking(address, data);
    }

    // write attempts to external RAM
    else if ((address >= 0xA000) && (address <= 0xBFFF)) {
        if (enableRAM) {
            // In ROM mode, only RAM bank 0 can be accessed
            assert(ROMBanking == (currentRAMBank == 0));
            WORD newAddress = (address - 0xA000) + (currentRAMBank * 0x2000);
            RAMBanks[newAddress] = data;
        }
    }

    // writing to ECHO RAM also writes to work RAM (0xC000 - 0xDDFF)
    else if ((address >= 0xE000) && (address <= 0xFDFF)) {
        // internalMem[address] = data;
        writeMem(address - 0x2000, data);
    }

    else if ((address >= 0xFEA0) && (address <= 0xFEFF)) {
        //cout << "Something wrong in WriteMem. Unusable location." << endl;
        //assert(false);
    }

    // FF04 is divider register, its value is reset to 0 if game attempts to 
    // write to it
    else if (address == DIVIDER) { 
        internalMem[DIVIDER] = 0;
    }

    // if game changes the freq, the counter must change accordingly
    else if (address == TAC) { 
        // get the currentFreq, do the writing, then compare with newFreq. if different, counter must be updated

        // to extract bit 1 and 0 of timer controller register
        BYTE currentFreq = readMem(TAC) & 0x3; 
        internalMem[TAC] = data; // write the data to the address
        BYTE newFreq = readMem(TAC) & 0x3;

        // if the freq has changed
        if (currentFreq != newFreq) { 
            switch (newFreq) {
                case 0b00: 
                    timerCounter = 1024; 
                    timerUpdateConstant = 1024; 
                    break; // 4096Hz
                case 0b01: 
                    timerCounter = 16;
                    timerUpdateConstant = 16;
                    break; // 262144Hz
                case 0b10: 
                    timerCounter = 64; 
                    timerUpdateConstant = 64;
                    break; // 65536Hz
                case 0b11: 
                    timerCounter = 256; 
                    timerUpdateConstant = 256; 
                    break; // 16384Hz
                default:
                    break;
                    //cout << "Something is wrong!" << endl;
            }
        }
    }

    // reset the current scanline to 0 if game tries to write to it
    else if (address == 0xFF44) {
        internalMem[address] = 0;
    }
    
    // launches a DMA to access the Sprites Attributes table
    else if (address == 0xFF46) {
        doDMATransfer(data);
    }

    else {
        internalMem[address] = data;
    }

}

void Emulator::handleBanking(WORD address, BYTE data) {
    // do RAM enabling
    if (address < 0x2000) {
        doRAMBankEnable(address, data);
    }

    // do ROM bank change
    else if ((address >= 0x2000) && (address <= 0x3FFF)) {
        if (MBC1) doChangeLoROMBank(data);
        // if MBC2, LSB of upper address byte must be 1 to select ROM bank
        // wtf is happening??
        else if (!isBitSet(address, 8)) doChangeLoROMBank(data);
    }

    // do ROM or RAM bank change
    else if ((address >= 0x4000) && (address <= 0x5FFF)) {
        if (MBC1) {
            if (ROMBanking) {
                doChangeHiROMBank(data);
            } else {
                doRAMBankChange(data);
            }
        }
    }

    // this changes whether we are doing ROM banking
    // or RAM banking with the above if statement
    else if ((address >= 0x6000) && (address <= 0x7FFF)) {
        if (MBC1) {
            doChangeROMRAMMode(data);
        }
    }

}

void Emulator::doRAMBankEnable(WORD address, BYTE data) {

    // for MBC2, LSB of upper byte of address must be 0 to do enable
    if (MBC2) {
        if (isBitSet(address, 8)) return;
    }

    BYTE testData = data & 0xF;
    if (testData == 0xA) {
        enableRAM = true;
    } else {
        // Any other value written will disable RAM
        enableRAM = false;
    }

}

void Emulator::doChangeLoROMBank(BYTE data) {
    
    // if MBC2, current ROM bank is lower nibble of data
    if (MBC2) {
        currentROMBank = data & 0xF;
        if (currentROMBank == 0x0) currentROMBank = 0x1;
        return;
    }

    BYTE lower5bits = data & 0x1F;
    
    // if lower5bits == 0x0, gameboy automatically sets it to 0x1 as ROM 0 can 
    // always be accessed from 0x0000-3FFF
    if (lower5bits == 0x0) lower5bits = 0x1;
    
    currentROMBank &= 0xE0; // mask the last 5 bits to 0
    currentROMBank |= lower5bits; // match last 5 bits to lower5bits

}

void Emulator::doChangeHiROMBank(BYTE data) {

    // change bit 6-5 of currentROMBank to bit 6-5 of data

    // turn off the upper 3 bits of the current rom (since bit 7 must == 0)
    currentROMBank &= 0x1F;

    data &= 0x3; // Get the lower 2 bits of the data
    data <<= 5;
    currentROMBank |= data; // match higher 2 bits of data

}

void Emulator::doRAMBankChange(BYTE data) {
    // only 4 RAM banks to choose from, 0x0-3
    currentRAMBank = data & 0x3;
}

void Emulator::doChangeROMRAMMode(BYTE data) {
    
    // ROM banking mode: 0x0
    // RAM banking mode: 0x1
    ROMBanking = ((data & 0x1) == 0x0);
    
    // The program may freely switch between both modes, the only limitiation 
    // is that only RAM Bank 00h can be used during Mode 0, and only ROM Banks 
    // 00-1Fh can be used during Mode 1.
    if (ROMBanking) {
        currentRAMBank = 0x0;
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
    BYTE requestReg = readMem(0xFF0F);
    requestReg = bitSet(requestReg, interruptID); // Set the corresponding bit in the interrupt req register 0xFF0F

    // If halted, wake up
    isHalted = false;

    writeMem(0xFF0F, requestReg); // Update the request register;
}

void Emulator::handleInterrupts() {
    if (InterruptMasterEnabled) { // Check if the IME switch is true
        BYTE requestReg = readMem(0xFF0F);
        BYTE enabledReg = readMem(0xFFFF);

        if ((requestReg & enabledReg) > 0) { // If there are any valid interrupt requests enabled
            InterruptMasterEnabled = false; // Disable further interrupts
            
            stackPointer.regstr--;
            writeMem(stackPointer.regstr, programCounter.high);
            stackPointer.regstr--;
            writeMem(stackPointer.regstr, programCounter.low);
            // Saves current PC to SP, SP is now pointing at bottom of PC. Need to increment SP by 2 when returning

            for (int i = 0; i < 5; i++) { // Go through the bits and service the flagged interrupts
                bool isFlagged = isBitSet(requestReg, i);
                bool isEnabled = isBitSet(enabledReg, i);
                if (isFlagged && isEnabled) { // If n-th bit is flagged and enabled, trigger the corresponding interrupt
                    triggerInterrupt(i);
                }
            }
        }
    }
}

void Emulator::triggerInterrupt(int interruptID) {
    BYTE requestReg = readMem(0xFF0F);
    requestReg = bitReset(requestReg, interruptID); // Resetting the n-th bit
    writeMem(0xFF0F, requestReg); 
    switch (interruptID) {
        case 0 : // V-Blank
            programCounter.regstr = 0x40;
            break;
        case 1 : // LCD
            programCounter.regstr = 0x48;
            break;
        case 2 : // Timer
            programCounter.regstr = 0x50;
            break;
        case 4 : // Joypad
            programCounter.regstr = 0x60;
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

    dividerCounter += cycles; // TO DECLARE SOMEWHERE
    // Handle divider register first

    if (dividerCounter >= 256) {
       dividerCounter = 0; // reset it to start counting for upcoming cycles
       internalMem[DIVIDER]++; // directly modifying instead of using writeMem
    }

    if (clockEnabled()) {

        timerCounter -= cycles; // TO DECLARE SOMEWHERE. DECLARE timerUpdateConstant AS WELL!!!
        // Decrement counter instead of increment so just need to keep track if <= 0

        if (timerCounter <= 0) { // To increment TIMA

            // Reset counter to prep for next update
            timerCounter = timerUpdateConstant; 

            // TIMA is at 255, about to overflow
            if (readMem(TIMA) == 0xFF) { 

                writeMem(TIMA, readMem(TMA)); // set value of TIMA to value of TMA
                
                flagInterrupt(2); // The interrupt flagged is corresponded to bit 2 of interrupt register
                
            } else {
                writeMem(TIMA, readMem(TIMA) + 1); // TIMA is incremented by 1
            }

        }

    }

}

bool Emulator::clockEnabled() {
    // Bit 2 of TAC specifies whether timer is enabled(1) or disabled(0)
    return isBitSet(readMem(TAC), 2);
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

    if (isBitSet(joypadState, key)) { // If the key was set at 1 (unpressed)
        previouslyUnpressed = true;
    }

    joypadState = bitReset(joypadState, key); // Set the key to 0 (pressed)

    // Now, determine if the key is directional or normal button

    bool directionalButton = key < 4;

    // flagIntrpt is true if previouslyUnpressed AND bit 4 and 5 of joypad register
    // corresponds to the directionalButton bool.
    //  => Bit 4 (dxn) of joypad register is 0 (on) and directionalButton is true
    //  => Bit 5 (normal) of joypad register is 0, and directionalButton is false

    BYTE joypadReg = internalMem[0xFF00];

    if ((!isBitSet(joypadReg, 4) && directionalButton) || 
            (!isBitSet(joypadReg, 5) && !directionalButton)) {
                if (previouslyUnpressed) {
                    flagIntrpt = true;
                }
            }
    
    if (flagIntrpt) {
        flagInterrupt(4);
    }
}

void Emulator::buttonReleased(int key) {
    joypadState = bitSet(joypadState, key);
}

BYTE Emulator::getJoypadState() const {
    BYTE joypadReg = internalMem[0xFF00];
    joypadReg &= 0xF0; // Sets bits 0-3 to 0;

    // If program requests for directional buttons
    if (!isBitSet(joypadReg, 4)) {
        BYTE directionals = joypadState & 0x0F; // Sets bits 4-7 to 0
        joypadReg |= directionals;
    }

    // If program requests for normal buttons
    else if (!isBitSet(joypadReg, 5)) {
        BYTE normalButtons = joypadState >> 4;
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

    setLCDStatus();

    if (!LCDEnabled()) {
        return;
    }
    scanlineCycleCount -= cycles;

    // move onto the next scanline
    if (scanlineCycleCount <= 0) {
        // need to update directly since gameboy will always reset scanline to 0
        // if attempting to write to 0xFF44 in memory
        internalMem[0xFF44]++;
        BYTE currentLine = readMem(0xFF44);

        scanlineCycleCount = 456;

        // encountered vblank period
        if (currentLine == 144) {
            renderGraphics();
            flagInterrupt(0);
        }

        // if gone past scanline 153 reset to 0
        else if (currentLine > 153) {
            internalMem[0xFF44] = 0;
        }

        // draw the current scanline
        else if (currentLine < 144) {
            drawScanLine();
        }
    }

}

void Emulator::setLCDStatus() {

    BYTE status = readMem(0xFF41);

    if (!LCDEnabled()) {
        // set the mode to 1 (vblank) during lcd disabled and reset scanline
        scanlineCycleCount = 456;
        internalMem[0xFF44] = 0;
        // set last 2 bits of status to 01
        status = bitSet(status, 0);
        status = bitReset(status, 1);

        writeMem(0xFF41, status);
        return;
    }

    BYTE currentLine = readMem(0xFF44);
    BYTE currentMode = status & 0x3;

    BYTE newMode = 0;
    bool needInterrupt = false;

    // in vblank so set mode to 1
    if (currentLine >= 144) {
        newMode = 1;
        // set last 2 bits of status to 01
        status = bitSet(status, 0);
        status = bitReset(status, 1);
        // check if vblank interrupt (bit 4) is enabled
        needInterrupt = isBitSet(status, 4);
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
        if (scanlineCycleCount >= 376) {
            newMode = 2;
            // set last 2 bits of status to 10
            status = bitReset(status, 0);
            status = bitSet(status, 1);
            // check if OAM interrupt (bit 5) is enabled
            needInterrupt = isBitSet(status, 5);
        }

        // mode 3: 376 - 172 = 204
        else if (scanlineCycleCount >= 204) {
            newMode = 3;
            // set last 2 bits of status to 11
            status = bitSet(status, 0);
            status = bitSet(status, 1);
        }

        // mode 0
        else {
            newMode = 0;
            // set last 2 bits of status to 00
            status = bitReset(status, 0);
            status = bitReset(status, 1);
            // check if hblank interrupt (bit 3) is enabled
            needInterrupt = isBitSet(status, 3);
        }
    }

    // if a new mode is entered, request interrupt
    if (needInterrupt && (newMode != currentMode)) {
        flagInterrupt(1);
    }

    // check for the coincidence flag
    if (currentLine == readMem(0xFF45)) {
        // set coincidence flag (bit 2) to 1
        status = bitSet(status, 2);
        // check if coincidence flag interrupt (bit 6) is enabled
        if (isBitSet(status, 6)) {
            flagInterrupt(1);
        }
    } else {
        // set coincidence flag (bit 2) to 0
        status = bitReset(status, 2);
    }

    writeMem(0xFF41, status);

}

bool Emulator::LCDEnabled() {
    return isBitSet(readMem(0xFF40), 7);
}

void Emulator::drawScanLine() {
    BYTE lcdControl = readMem(0xFF40);

    // Draw only if LCD is enabled
    if (LCDEnabled()) {
        if (isBitSet(lcdControl, 0)) {
            renderTiles(lcdControl);
        }

        if (isBitSet(lcdControl, 1)) {
            renderSprites(lcdControl);
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
    BYTE scrollY = readMem(0xFF42);
    BYTE scrollX = readMem(0xFF43);
    BYTE windowY = readMem(0xFF4A);
    BYTE windowX = readMem(0xFF4B) - 7;

    // Check if window is enabled and if current scanline is within windowY
    bool usingWindow = false;
    BYTE currentLine = readMem(0xFF44);
    if (isBitSet(lcdControl, 5) && (windowY <= currentLine)) {
        usingWindow = true;
    }

    // Get Tile Data location & addressing mode
    WORD tileDataLocation;
    bool unsignedAddressing;

    if (isBitSet(lcdControl, 4)) {
        // location: 0x8000-8FFF
        tileDataLocation = 0x8000;
        unsignedAddressing = true;
    } else {
        // location: 0x8800-97FF
        // Tile #0 is actually at 0x9000
        // 0x80 (-128) is the lowest tile at 0x8800
        tileDataLocation = 0x9000;
        unsignedAddressing = false;
    }

    // Get Tile Map location
    WORD tileMapLocation;
    if (!usingWindow) {
        // BG Tile Map Display Select
        if (isBitSet(lcdControl, 3)) {
            tileMapLocation = 0x9C00;
        } else {
            tileMapLocation = 0x9800;
        }
    } else {
        // Window Tile Map Display Select
        if (isBitSet(lcdControl, 6)) {
            tileMapLocation = 0x9C00;
        } else {
            tileMapLocation = 0x9800;
        }
    }

    // Get tile row the "offset" for the current line of pixels in the tile
    BYTE tileY;
    BYTE tileYOffset;
    if (!usingWindow) {
        tileY = (BYTE)(((scrollY + currentLine) / 8) % 32);
        tileYOffset = (BYTE)((scrollY + currentLine) % 8);
    } else {
        // POSSIBLE BUG: need to % 32 to wrap???
        // Or because window is not scrollable and always display from top left?
        tileY = (BYTE)((currentLine - windowY) / 8);
        tileYOffset = (BYTE)((currentLine - windowY) % 8);
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
        BYTE tileNum = readMem(tileMapLocation + (tileY*32) + tileX);
        
        // Get tile data address
        WORD tileDataAddress;
        if (unsignedAddressing) {
            // Tile number is unsigned and each tile is 16 bytes
            tileDataAddress = tileDataLocation + (tileNum * 16);
        } else {
            // Tile number is signed and each tile is 16 bytes
            tileDataAddress = tileDataLocation + (static_cast<SIGNED_BYTE>(tileNum) * 16);
            // tileDataAddress here is in the region 0x8800-97FF
            assert(((tileDataAddress >= 0x8800) && (tileDataAddress <= 0x97FF))== true);
        }

        // Each line is 2 bytes long, to get the current line, add the offset
        tileDataAddress += (tileYOffset << 1);

        // Read the 2 bytes of data
        BYTE b1 = readMem(tileDataAddress);
        BYTE b2 = readMem(tileDataAddress + 1);

        // Figure out the colour palette
        BYTE bit = 7 - ((scrollX + pixel) % 8);
        BYTE colourBit0 = isBitSet(b1, bit) ? 0b01 : 0b00;
        BYTE colourBit1 = isBitSet(b2, bit) ? 0b10 : 0b00;

        // //cout << "Palette: " << (int) readMem(0xFF47) << " | colour bits: " << (int) (colourBit0 + colourBit1) << " | Colour: ";

        // Get the colour
        COLOUR colour = getColour(colourBit1 + colourBit0, 0xFF47);
        
        // Default colour is black where RGB = [0,0,0]
        int red, green, blue; 

        switch (colour) {
            case WHITE: 
                red = 255;
                green = 255;
                blue = 255;
                // //cout << "white" << endl;
                break;
            case LIGHT_GRAY:
                red = 0xCC;
                green = 0xCC;
                blue = 0xCC;
                // //cout << "light gray" << endl;
                break;
            case DARK_GRAY:
                red = 0x77;
                green = 0x77;
                blue = 0x77;
                // //cout << "dark gray" << endl;
                break;
            default:
                red = green = blue = 0;
                // //cout << "black" << endl;
                break;
        }

        // Update Screen pixels
        // Store in pixel format ARGB8888
        displayPixels[pixel + (currentLine * 160)] = (0xFF << 24) | (red << 16) | (green << 8) | blue;

    }

}

COLOUR Emulator::getColour(BYTE colourNum, WORD address) const {

    // Reading colour palette from memory
    BYTE palette = readMem(address);
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
    if (isBitSet(lcdControl, 2)) {
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
        BYTE yPos = readMem(0xFE00 + index) - 16;
        BYTE xPos = readMem(0xFE00 + index + 1) - 8;
        BYTE tileNum = readMem(0xFE00 + index + 2);
        BYTE attributes = readMem(0xFE00 + index + 3);

        bool yFlip = isBitSet(attributes, 6);
        bool xFlip = isBitSet(attributes, 5);

        int scanLine = readMem(0xFF44);

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
            BYTE b1 = readMem(lineDataAddress);
            BYTE b2 = readMem(lineDataAddress + 1);

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
                BYTE colourBit0 = isBitSet(b1, colourBit) ? 0b01 : 0b00;
                BYTE colourBit1 = isBitSet(b2, colourBit) ? 0b10 : 0b00;

                // Get the colour
                WORD cAddress = isBitSet(attributes, 4) ? 0xFF49 : 0xFF48;
                COLOUR colour = getColour(colourBit1 + colourBit0, cAddress);

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
                        break;
                }

                // Get the pixel to draw
                int pixel = xPos + (0 - tilePixel + 7);

                // check if pixel is hidden behind background
                if (isBitSet(attributes, 7)) {

                    if (displayPixels[pixel + (scanLine * 160)] != 0xFFFFFFFF) {
                        continue ;
                    }
                    
                }
                // Update Screen pixels
                displayPixels[pixel + (scanLine * 160)] = (0xFF << 24) | (red << 16) | (green << 8) | blue;

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
        writeMem(0xFE00 + i, readMem(address + i));
    }
}

void Emulator::renderGraphics() {
    if (doRenderPtr != nullptr) {
        doRenderPtr();
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

    //cout << "LD_r_R" << endl;

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
    BYTE n = readMem(programCounter.regstr);
    programCounter.regstr++;
    reg = n;

    //cout << "LD_r_n" << endl;

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
    reg = readMem(regHL.regstr);

    //cout << "LD_r_HL" << endl;

    return 8;
}

/*
    LD HL, r  (0x70 - 0x77 except 0x76)

    Loads content of r register into memory location specified by HL.

    8 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_HL_r(BYTE reg) {
    writeMem(regHL.regstr, reg);

    //cout << "LD_HL_r" << endl;

    return 8;
}

/*
    LD HL, n  (0x36)

    Loads immediate 8 bit data into memory location specified by HL.

    12 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_HL_n() {
    BYTE imm = readMem(programCounter.regstr);
    programCounter.regstr++;
    writeMem(regHL.regstr, imm);

    //cout << "LD_HL_n" << endl;

    return 12;
}

/*
    LD A, BC  (0x0A)

    Loads content of memory location specified by BC into register A.

    8 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_A_BC() {
    regAF.high = readMem(regBC.regstr);

    //cout << "LD_A_BC" << endl;

    return 8;
}

/*
    LD A, DE  (0x1A)

    Loads content of memory location specified by DE into register A.

    8 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_A_DE() {
    regAF.high = readMem(regDE.regstr);

    //cout << "LD_A_DE" << endl;

    return 8;
}

/*
    LD A, (nn)  (0xFA)

    Loads content of memory location specified by immediate 16 bit address into register A.

    16 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_A_nn() {
    WORD nn = readMem(programCounter.regstr + 1) << 8;
    nn |= readMem(programCounter.regstr);
    programCounter.regstr += 2;
    regAF.high = readMem(nn);

    //cout << "LD_A_nn" << endl;

    return 16;
}

/*
    LD (BC), A  (0x02)

    Loads content of register A into memory location specified by BC.

    8 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_BC_A() {
    writeMem(regBC.regstr, regAF.high);

    //cout << "LD_BC_A" << endl;

    return 8;
}

/*
    LD (DE), A  (0x12)

    Loads content of register A into memory location specified by DE.

    8 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_DE_A() {
    writeMem(regDE.regstr, regAF.high);

    assert(internalMem[regDE.regstr] == regAF.high);

    //cout << "LD_DE_A" << endl;

    return 8;
}

/*
    LD (nn), A  (0xEA)

    Loads content of register A into memory location specified by immediate 16 bit address.

    16 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_nn_A() {
    WORD nn = readMem(programCounter.regstr + 1) << 8;
    nn |= readMem(programCounter.regstr);
    programCounter.regstr += 2;
    writeMem(nn, regAF.high);

    //cout << "LD_nn_A" << endl;

    return 16;
}

/*
    LD A, (FF00+n)  (0xF0)

    Loads content of memory location specified by FF00+n into register A,
    where n is the immediate 8 bit data

    12 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_A_FF00n() {
    BYTE n = readMem(programCounter.regstr);
    programCounter.regstr++;
    regAF.high = readMem(0xFF00 + n);

    //cout << "LD_A_FF00n" << endl;

    return 12;
}

/*
    LD (FF00+n), A  (0xE0)

    Loads content of register A into memory location specified by FF00+n,
    where n is the immediate 8 bit data.

    12 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_FF00n_A() {
    BYTE n = readMem(programCounter.regstr);
    programCounter.regstr++;
    writeMem(0xFF00 + n, regAF.high);

    //cout << "LD_FF00n_A" << endl;

    return 12;
}

/*
    LD A, (FF00+C)  (0xF2)

    Loads content of memory location specified by FF00+C into register A,
    where C is the content of register C

    8 cycles 

    Flags affected(znhc): ----
 */
int Emulator::LD_A_FF00C() {
    regAF.high = readMem(0xFF00 + regBC.low);

    //cout << "LD_A_FF00C" << endl;

    return 8;
}

/*
    LD (FF00+C), A  (0xE2)

    Loads content of register A into memory location specified by FF00+C,
    where C is the content of register C

    8 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_FF00C_A() {
    writeMem(0xFF00 + regBC.low, regAF.high);

    //cout << "LD_FF00C_A" << endl;

    return 8;
}

/*
    LDI (HL), A  (0x22)

    Loads content of register A into memory location specified by HL, then increment HL.

    8 cycles

    Flags affected(znhc): ----
 */
int Emulator::LDI_HL_A() {
    writeMem(regHL.regstr, regAF.high);
    regHL.regstr++;

    //cout << "LDI_HL_A" << endl;

    return 8;
}

/*
    LDI A, (HL)  (0x2A)

    Loads content of memory location specified by HL into register A, then increment HL.

    8 cycles

    Flags affected(znhc): ----
 */
int Emulator::LDI_A_HL() {
    regAF.high = readMem(regHL.regstr);
    regHL.regstr++;

    //cout << "LDI_A_HL" << endl;

    return 8;
}

/*
    LDD (HL), A  (0x32)

    Loads content of register A into memory location specified by HL, then decrement HL.

    8 cycles

    Flags affected(znhc): ----
 */
int Emulator::LDD_HL_A() {
    writeMem(regHL.regstr, regAF.high);
    regHL.regstr--;

    //cout << "LDD_HL_A" << endl;

    return 8;
}

/*
    LDD A, (HL)  (0x3A)

    Loads content of memory location specified by HL into register A, then decrement HL.

    8 cycles

    Flags affected(znhc): ----
 */
int Emulator::LDD_A_HL() {
    regAF.high = readMem(regHL.regstr);
    regHL.regstr--;

    //cout << "LDD_A_HL" << endl;

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
    WORD nn = readMem(programCounter.regstr + 1) << 8;
    nn |= readMem(programCounter.regstr);
    programCounter.regstr += 2;
    reg.regstr = nn;

    //cout << "LD_rr_nn" << endl;

    return 12;
}

/*
    LD SP, HL  (0xF9)

    Loads content of HL into SP.

    8 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_SP_HL() {
    stackPointer.regstr = regHL.regstr;

    //cout << "LD_SP_HL" << endl;

    return 8;
}

/*
    LD (nn), SP  (0x08)

    Loads content of SP into memory location specified by nn

    20 cycles

    Flags affected(znhc): ----
 */
int Emulator::LD_nn_SP() {
    WORD nn = readMem(programCounter.regstr + 1) << 8;
    nn |= readMem(programCounter.regstr);
    programCounter.regstr += 2;

    writeMem(nn + 1, stackPointer.high);
    writeMem(nn, stackPointer.low);

    //cout << "LD_nn_SP" << endl;

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
    stackPointer.regstr--;
    writeMem(stackPointer.regstr, reg.high);
    stackPointer.regstr--;
    writeMem(stackPointer.regstr, reg.low);

    //cout << "PUSH_rr" << endl;

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
    reg.low = readMem(stackPointer.regstr);
    stackPointer.regstr++;
    reg.high = readMem(stackPointer.regstr);
    stackPointer.regstr++;

    if (&reg == &regAF) {
        regAF.regstr &= 0xFFF0;
    }

    //cout << "POP_rr" << endl;

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
    BYTE result = regAF.high + regR;
    
    // Reset the flags
    regAF.low &= 0x00;

    // Zero flag
    if (result == 0x0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    }

    // Half carry flag 
    if (((regR ^ regAF.high ^ result) & 0x10) == 0x10) {
        regAF.low = bitSet(regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (result < regAF.high) {
        regAF.low = bitSet(regAF.low, FLAG_CARRY);
    }

    regAF.high = result;

    //cout << "ADD_A_r" << endl;

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
    BYTE n = readMem(programCounter.regstr);
    programCounter.regstr++;
    BYTE result = regAF.high + n;

    // Reset the flags
    regAF.low &= 0x00;

    // Zero flag
    if (result == 0x0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    }

    // Half carry flag 
    if (((n ^ regAF.high ^ result) & 0x10) == 0x10) {
        regAF.low = bitSet(regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (result < regAF.high) {
        regAF.low = bitSet(regAF.low, FLAG_CARRY);
    }

    regAF.high = result;

    //cout << "ADD_A_n" << endl;

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
    BYTE toAdd = readMem(regHL.regstr);
    BYTE result = regAF.high + toAdd;

    // Reset the flags
    regAF.low &= 0x00;

    // Zero flag
    if (result == 0x0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    }

    // Half carry flag 
    if (((toAdd ^ regAF.high ^ result) & 0x10) == 0x10) {
        regAF.low = bitSet(regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (result < regAF.high) {
        regAF.low = bitSet(regAF.low, FLAG_CARRY);
    }

    regAF.high = result;

    //cout << "ADD_A_HL" << endl;

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
    BYTE carry = isBitSet(regAF.low, FLAG_CARRY) ? 0x01 : 0x00;
    BYTE toAdd = carry + reg;
    BYTE result = regAF.high + carry + toAdd;

    // Reset the flags
    regAF.low &= 0x00;

    // Zero flag
    if (result == 0x0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    }

    // Half carry flag 
    if (((toAdd ^ regAF.high ^ result) & 0x10) == 0x10) {
        regAF.low = bitSet(regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (result < regAF.high) {
        regAF.low = bitSet(regAF.low, FLAG_CARRY);
    }

    regAF.high = result;

    //cout << "ADC_A_r" << endl;

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
    BYTE carry = isBitSet(regAF.low, FLAG_CARRY) ? 0x01 : 0x00;
    BYTE toAdd = carry + readMem(programCounter.regstr);
    BYTE result = regAF.high + toAdd;
    programCounter.regstr++;

    // Reset the flags
    regAF.low &= 0x00;

    // Zero flag
    if (result == 0x0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    }

    // Half carry flag 
    if (((toAdd ^ regAF.high ^ result) & 0x10) == 0x10) {
        regAF.low = bitSet(regAF.low, FLAG_HALFCARRY);
        assert(((regAF.high & 0x0F) + (toAdd & 0x0F)) > 0x0F);
    }

    // Carry flag
    if (result < regAF.high) {
        regAF.low = bitSet(regAF.low, FLAG_CARRY);
    }

    regAF.high = result;

    //cout << "ADC_A_n" << endl;

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
    BYTE carry = isBitSet(regAF.low, FLAG_CARRY) ? 0x01 : 0x00;
    BYTE toAdd = carry + readMem(regHL.regstr);
    BYTE result = regAF.high + carry + toAdd;

    // Reset the flags
    regAF.low &= 0x00;

    // Zero flag
    if (result == 0x0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    }

    // Half carry flag 
    if (((toAdd ^ regAF.high ^ result) & 0x10) == 0x10) {
        regAF.low = bitSet(regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (result < regAF.high) {
        regAF.low = bitSet(regAF.low, FLAG_CARRY);
    }

    regAF.high = result;

    //cout << "ADC_A_HL" << endl;

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
    BYTE result = regAF.high - reg;

    // Reset the flags
    regAF.low &= 0x00;

    // Zero flag
    if (result == 0x0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    }

    // Subtract flag
    regAF.low = bitSet(regAF.low, FLAG_SUB);

    // Half carry flag 
    // If lower nibble of A is less than lower nibble of r, set HCF
    if ((regAF.high & 0x0F) < (reg & 0x0F)) {
        regAF.low = bitSet(regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (regAF.high < reg) {
        regAF.low = bitSet(regAF.low, FLAG_CARRY);
    }

    regAF.high = result;

    //cout << "SUB_r" << endl;

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
    BYTE n = readMem(programCounter.regstr);
    programCounter.regstr++;

    BYTE result = regAF.high - n;

    // Reset the flags
    regAF.low &= 0x00;

    // Zero flag
    if (result == 0x0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    }

    // Subtract flag
    regAF.low = bitSet(regAF.low, FLAG_SUB);

    // Half carry flag 
    // If lower nibble of A is less than lower nibble of n, set HCF
    if ((regAF.high & 0x0F) < (n & 0x0F)) {
        regAF.low = bitSet(regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (regAF.high < n) {
        regAF.low = bitSet(regAF.low, FLAG_CARRY);
        assert((regAF.high & 0xFF) < (n & 0xFF));
    }

    regAF.high = result;

    //cout << "SUB_n" << endl;

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
    BYTE toSub = readMem(regHL.regstr);
    BYTE result = regAF.high - toSub;

    // Reset the flags
    regAF.low &= 0x00;

    // Zero flag
    if (result == 0x0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    }

    // Subtract flag
    regAF.low = bitSet(regAF.low, FLAG_SUB);

    // Half carry flag 
    // If lower nibble of A is less than lower nibble of toSub, set HCF
    if ((regAF.high & 0x0F) < (toSub & 0x0F)) {
        regAF.low = bitSet(regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (regAF.high < toSub) {
        regAF.low = bitSet(regAF.low, FLAG_CARRY);
    }

    regAF.high = result;

    //cout << "SUB_HL" << endl;

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
    BYTE carry = isBitSet(regAF.low, FLAG_CARRY) ? 0x1 : 0x0;
    BYTE toSub = carry + reg;
    BYTE result = regAF.high - toSub;

    // Reset the flags
    regAF.low &= 0x00;

    // Zero flag
    if (result == 0x0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    }

    // Subtract flag
    regAF.low = bitSet(regAF.low, FLAG_SUB);

    // Half carry flag 
    // If lower nibble of A is less than lower nibble of toSub, set HCF
    if ((regAF.high & 0x0F) < (toSub & 0x0F)) {
        regAF.low = bitSet(regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (regAF.high < toSub) {
        regAF.low = bitSet(regAF.low, FLAG_CARRY);
    }

    regAF.high = result;

    //cout << "SBC_A_r" << endl;

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
    BYTE carry = isBitSet(regAF.low, FLAG_CARRY) ? 0x1 : 0x0;
    BYTE toSub = carry + readMem(programCounter.regstr);
    programCounter.regstr++;

    BYTE result = regAF.high - toSub;

    // Reset the flags
    regAF.low &= 0x00;

    // Zero flag
    if (result == 0x0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    }

    // Subtract flag
    regAF.low = bitSet(regAF.low, FLAG_SUB);

    // Half carry flag 
    // If lower nibble of A is less than lower nibble of toSub, set HCF
    if ((regAF.high & 0x0F) < (toSub & 0x0F)) {
        regAF.low = bitSet(regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (regAF.high < toSub) {
        regAF.low = bitSet(regAF.low, FLAG_CARRY);
    }

    regAF.high = result;

    //cout << "SBC_A_n" << endl;

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
    BYTE carry = isBitSet(regAF.low, FLAG_CARRY) ? 0x1 : 0x0;
    BYTE toSub = carry + readMem(regHL.regstr);
    BYTE result = regAF.high - toSub;

    // Reset the flags
    regAF.low &= 0x00;

    // Zero flag
    if (result == 0x0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    }

    // Subtract flag
    regAF.low = bitSet(regAF.low, FLAG_SUB);

    // Half carry flag 
    // If lower nibble of A is less than lower nibble of toSub, set HCF
    if ((regAF.high & 0x0F) < (toSub & 0x0F)) {
        regAF.low = bitSet(regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (regAF.high < toSub) {
        regAF.low = bitSet(regAF.low, FLAG_CARRY);
    }

    regAF.high = result;

    //cout << "SBC_A_HL" << endl;

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
    BYTE result = regAF.high & reg;
    
    // Reset the flags
    regAF.low &= 0x00;

    // Zero flag
    if (result == 0x0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    }

    // Half carry flag 
    regAF.low = bitSet(regAF.low, FLAG_HALFCARRY);

    regAF.high = result;

    //cout << "AND_r" << endl;

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
    BYTE result = regAF.high & readMem(programCounter.regstr);
    programCounter.regstr++;
    
    // Reset the flags
    regAF.low &= 0x00;

    // Zero flag
    if (result == 0x0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    }

    // Half carry flag 
    regAF.low = bitSet(regAF.low, FLAG_HALFCARRY);

    regAF.high = result;

    //cout << "AND_n" << endl;

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
    BYTE result = regAF.high & readMem(regHL.regstr);
    
    // Reset the flags
    regAF.low &= 0x00;

    // Zero flag
    if (result == 0x0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    }

    // Half carry flag 
    regAF.low = bitSet(regAF.low, FLAG_HALFCARRY);

    regAF.high = result;

    //cout << "AND_HL" << endl;

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
    BYTE result = regAF.high ^ reg;

    // Reset the flags
    regAF.low &= 0x00;

    // Zero flag
    if (result == 0x0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    }

    regAF.high = result;

    //cout << "XOR_r" << endl;

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
    BYTE result = regAF.high ^ readMem(programCounter.regstr);
    programCounter.regstr++;

    // Reset the flags
    regAF.low &= 0x00;

    // Zero flag
    if (result == 0x0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    }

    regAF.high = result;

    //cout << "XOR_n" << endl;

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
    BYTE result = regAF.high ^ readMem(regHL.regstr);

    // Reset the flags
    regAF.low &= 0x00;

    // Zero flag
    if (result == 0x0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    }

    regAF.high = result;

    //cout << "XOR_HL" << endl;

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
    BYTE result = regAF.high | reg;

    // Reset the flags
    regAF.low &= 0x00;

    // Zero flag
    if (result == 0x0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    }

    regAF.high = result;

    //cout << "OR_r" << endl;

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
    BYTE result = regAF.high | readMem(programCounter.regstr);
    programCounter.regstr++;

    // Reset the flags
    regAF.low &= 0x00;

    // Zero flag
    if (result == 0x0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    }

    regAF.high = result;

    //cout << "OR_n" << endl;

    return 8;
}

/*
    OR (HL)  (0xB6)

    Set register A to bitwise OR register A and content of memory location specified by HL.

    8 cycles

    Flags affected:
    - z: Set if result is zero
    - n: 0
    - h: 0
    - c: 0
 */
int Emulator::OR_HL() {
    BYTE result = regAF.high | readMem(regHL.regstr);

    // Reset the flags
    regAF.low &= 0x00;

    // Zero flag
    if (result == 0x0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    }

    regAF.high = result;

    //cout << "OR_HL" << endl;

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
    BYTE result = regAF.high - reg;

    // Reset the flags
    regAF.low &= 0x00;

    // Zero flag
    if (result == 0x0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    }

    // Subtract flag
    regAF.low = bitSet(regAF.low, FLAG_SUB);

    // Half carry flag 
    // If lower nibble of A is less than lower nibble of r, set HCF
    if ((regAF.high & 0x0F) < (reg & 0x0F)) {
        regAF.low = bitSet(regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (regAF.high < reg) {
        regAF.low = bitSet(regAF.low, FLAG_CARRY);
        assert((regAF.high & 0xFF) < (reg & 0xFF));
    }

    //cout << "CP_r" << endl;

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
    BYTE n = readMem(programCounter.regstr);
    programCounter.regstr++;

    BYTE result = regAF.high - n;

    // Reset the flags
    regAF.low &= 0x00;

    // Zero flag
    if (result == 0x0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    }

    // Subtract flag
    regAF.low = bitSet(regAF.low, FLAG_SUB);

    // Half carry flag 
    // If lower nibble of A is less than lower nibble of n, set HCF
    if ((regAF.high & 0x0F) < (n & 0x0F)) {
        regAF.low = bitSet(regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (regAF.high < n) {
        regAF.low = bitSet(regAF.low, FLAG_CARRY);
        assert((regAF.high & 0xFF) < (n & 0xFF));
    }

    //cout << "CP_n" << endl;

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
    BYTE toSub = readMem(regHL.regstr);
    BYTE result = regAF.high - toSub;

    // Reset the flags
    regAF.low &= 0x00;

    // Zero flag
    if (result == 0x0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    }

    // Subtract flag
    regAF.low = bitSet(regAF.low, FLAG_SUB);

    // Half carry flag 
    // If lower nibble of A is less than lower nibble of toSub, set HCF
    if ((regAF.high & 0x0F) < (toSub & 0x0F)) {
        regAF.low = bitSet(regAF.low, FLAG_HALFCARRY);
    }

    // Carry flag
    if (regAF.high < toSub) {
        regAF.low = bitSet(regAF.low, FLAG_CARRY);
    }

    //cout << "CP_HL" << endl;

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
    bool wasBit3Set = isBitSet(reg, 3);
    reg++;
    bool afterBit3Set = isBitSet(reg, 3);
    
    // Zero flag
    if (reg == 0x0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    } else {
        regAF.low = bitReset(regAF.low, FLAG_ZERO);
    }

    // Subtract flag
    regAF.low = bitReset(regAF.low, FLAG_SUB);

    // Half carry flag 
    // Set if bit 3 was set before the increment, then not set after the increment
    if (wasBit3Set && !afterBit3Set) {
        regAF.low = bitSet(regAF.low, FLAG_HALFCARRY);
    } 
    else {
        regAF.low = bitReset(regAF.low, FLAG_HALFCARRY);
    }

    //cout << "INC_r" << endl;

    return 4;
}

/*
    INC HL (0x34)

    Increments byte at address (HL) by 1.

    12 cycles

    Flags affected(znhc): 
    - z: Set if result is zero
    - n: 0
    - h: Set if bit 3 was set before the increment, then not set after the increment
    - c: Not affected
 */
int Emulator::INC_HL() {
    BYTE HLdata = readMem(regHL.regstr);
    bool wasBit3Set = isBitSet(HLdata, 3);
    HLdata++;
    bool afterBit3Set = isBitSet(HLdata, 3);
    
    // Zero flag
    if (HLdata == 0x0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    } else {
        regAF.low = bitReset(regAF.low, FLAG_ZERO);
    }

    // Subtract flag
    regAF.low = bitReset(regAF.low, FLAG_SUB);

    // Half carry flag 
    // Set if bit 3 was set before the increment, then not set after the increment
    if (wasBit3Set && !afterBit3Set) {
        regAF.low = bitSet(regAF.low, FLAG_HALFCARRY);
    } 
    else {
        regAF.low = bitReset(regAF.low, FLAG_HALFCARRY);
    }

    writeMem(regHL.regstr, HLdata);

    //cout << "INC_HL" << endl;

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
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    } else {
        regAF.low = bitReset(regAF.low, FLAG_ZERO);
    }

    // Subtract flag
    regAF.low = bitSet(regAF.low, FLAG_SUB);

    // Half carry flag 
    if (((result ^ reg ^ 0x1) & 0x10) == 0x10) {
        regAF.low = bitSet(regAF.low, FLAG_HALFCARRY);
    } 
    else {
        regAF.low = bitReset(regAF.low, FLAG_HALFCARRY);
    }

    reg = result;

    //cout << "DEC_r" << endl;

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
    BYTE initial = readMem(regHL.regstr);
    BYTE result =  initial - 1;
    
    // Zero flag
    if (result == 0x0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    } else {
        regAF.low = bitReset(regAF.low, FLAG_ZERO);
    }

    // Subtract flag
    regAF.low = bitSet(regAF.low, FLAG_SUB);

    // Half carry flag 
    if ((((result ^ initial ^ 0x1)) & 0x10) == 0x10) {
        regAF.low = bitSet(regAF.low, FLAG_HALFCARRY);
    } 
    else {
        regAF.low = bitReset(regAF.low, FLAG_HALFCARRY);
    }

    writeMem(regHL.regstr, result);

    //cout << "DEC_HL" << endl;

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
    int result = regAF.high;

    // After an addition
    if (!isBitSet(regAF.low, FLAG_SUB)) {
        if (isBitSet(regAF.low, FLAG_HALFCARRY) || (result & 0xF) > 9) {
            result += 0x06;
        }

        if (isBitSet(regAF.low, FLAG_CARRY) || (result > 0x9F)) {
            result += 0x60;
        }
    }
    // After a subtraction
    else
    {
        if (isBitSet(regAF.low, FLAG_HALFCARRY)) {
            result = (result - 0x06) & 0xFF;
        }

        if (isBitSet(regAF.low, FLAG_CARRY)) {
            result -= 0x60;
        }
    }

    // Half carry flag
    regAF.low = bitReset(regAF.low, FLAG_HALFCARRY);

    // Carry flag
    // If it overflowed
    if ((result & 0x100) == 0x100) {
        regAF.low = bitSet(regAF.low, FLAG_CARRY);
    }
    
    result &= 0xFF;

    // Zero flag
    if (result == 0) {
        regAF.low = bitSet(regAF.low, FLAG_ZERO);
    }
    else
    {
        regAF.low = bitReset(regAF.low, FLAG_ZERO);
    }

    regAF.high = (BYTE) result;

    //cout << "DAA" << endl;

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
    BYTE result = regAF.high ^ 0xFF;

    // Subtract flag
    regAF.low = bitSet(regAF.low, FLAG_SUB);

    // Half Carry flag
    regAF.low = bitSet(regAF.low, FLAG_HALFCARRY);

    regAF.high = result;

    //cout << "CPL" << endl;

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

    WORD before = regHL.regstr;
    
    // Add contents and store result in HL
    WORD result = before + rr;
    regHL.regstr = result;

    // Reset subtract flag
    regAF.low = bitReset(regAF.low, FLAG_SUB);

    // Update half carry flag
    regAF.low = ((result ^ before ^ rr) & 0x1000)
        ? bitSet(regAF.low, FLAG_HALFCARRY)
        : bitReset(regAF.low, FLAG_HALFCARRY);

    // Update carry flag
    regAF.low = (result < before) 
        ? bitSet(regAF.low, FLAG_CARRY)
        : bitReset(regAF.low, FLAG_CARRY);

    //cout << "ADD_HL_rr" << endl;

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

    //cout << "INC_rr" << endl;

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

    //cout << "DEC_rr" << endl;

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

    WORD before = stackPointer.regstr;
    SIGNED_BYTE dd = static_cast<SIGNED_BYTE>(readMem(programCounter.regstr));
    programCounter.regstr++;

    // Adding dd to SP and storing result in SP
    WORD result = before + dd;
    stackPointer.regstr = result;

    // Reset zero and subtract flag
    regAF.low = bitReset(regAF.low, FLAG_ZERO);
    regAF.low = bitReset(regAF.low, FLAG_SUB);

    // Update half carry flag
    regAF.low = ((result & 0x0F) < (before & 0x0F))
        ? bitSet(regAF.low, FLAG_HALFCARRY)
        : bitReset(regAF.low, FLAG_HALFCARRY);

    // Update carry flag
    regAF.low = ((result & 0xFF) < (before & 0xFF)) 
        ? bitSet(regAF.low, FLAG_CARRY)
        : bitReset(regAF.low, FLAG_CARRY);

    //cout << "ADD_SP_dd" << endl;

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

    WORD before = stackPointer.regstr;
    SIGNED_BYTE dd = static_cast<SIGNED_BYTE>(readMem(programCounter.regstr));
    programCounter.regstr++;

    // Adding dd to SP, and load result into HL
    WORD result = stackPointer.regstr + dd;
    regHL.regstr = result;

    // Reset zero and subtract flag
    regAF.low = bitReset(regAF.low, FLAG_ZERO);
    regAF.low = bitReset(regAF.low, FLAG_SUB);

    // Update half carry flag
    regAF.low = ((result & 0x0F) < (before & 0x0F))
        ? bitSet(regAF.low, FLAG_HALFCARRY)
        : bitReset(regAF.low, FLAG_HALFCARRY);

    // Update carry flag
    regAF.low = ((result & 0xFF) < (before & 0xFF)) 
        ? bitSet(regAF.low, FLAG_CARRY)
        : bitReset(regAF.low, FLAG_CARRY);

    //cout << "LD_HL_SPdd" << endl;

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

    BYTE data = regAF.high;
    BYTE bit7 = data >> 7;

    // Shift data left and copy bit 7 to bit 0
    data <<= 1;
    data |= bit7;

    // Store result back into accumulator
    regAF.high = data;

    // Reset zero, subtract and half carry flag
    regAF.low = bitReset(regAF.low, FLAG_ZERO);
    regAF.low = bitReset(regAF.low, FLAG_SUB);
    regAF.low = bitReset(regAF.low, FLAG_HALFCARRY);

    // Copy bit 7 into carry flag
    regAF.low = isBitSet(bit7, 0) 
        ? bitSet(regAF.low, FLAG_CARRY)
        : bitReset(regAF.low, FLAG_CARRY);

    //cout << "RLCA" << endl;

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

    BYTE data = regAF.high;
    BYTE bit7 = data >> 7;

    // Shift data left and copy old carry flag to bit 0
    data <<= 1;
    data |= (isBitSet(regAF.low, FLAG_CARRY) ? 0b1 : 0b0);

    // Store result back into accumulator
    regAF.high = data;

    // Reset zero, subtract and half carry flag
    regAF.low = bitReset(regAF.low, FLAG_ZERO);
    regAF.low = bitReset(regAF.low, FLAG_SUB);
    regAF.low = bitReset(regAF.low, FLAG_HALFCARRY);

    // Copy bit 7 into carry flag
    regAF.low = isBitSet(bit7, 0) 
        ? bitSet(regAF.low, FLAG_CARRY)
        : bitReset(regAF.low, FLAG_CARRY);

    //cout << "RLA" << endl;

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

    BYTE data = regAF.high;
    BYTE bit0 = data & 0b1;

    // Shift data right and copy bit 0 to bit 7
    data >>= 1;
    data |= (bit0 << 7);

    // Store result back into accumulator
    regAF.high = data;

    // Reset zero, subtract and half carry flag
    regAF.low = bitReset(regAF.low, FLAG_ZERO);
    regAF.low = bitReset(regAF.low, FLAG_SUB);
    regAF.low = bitReset(regAF.low, FLAG_HALFCARRY);

    // Copy bit 0 into carry flag
    regAF.low = isBitSet(bit0, 0) 
        ? bitSet(regAF.low, FLAG_CARRY)
        : bitReset(regAF.low, FLAG_CARRY);

    //cout << "RRCA" << endl;

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

    BYTE data = regAF.high;
    BYTE bit0 = data & 0b1;

    // Shift data right and copy old carry flag to bit 7
    data >>= 1;
    data |= ((isBitSet(regAF.low, FLAG_CARRY) ? 0b1 : 0b0) << 7);

    // Store result back into accumulator
    regAF.high = data;

    // Reset zero, subtract and half carry flag
    regAF.low = bitReset(regAF.low, FLAG_ZERO);
    regAF.low = bitReset(regAF.low, FLAG_SUB);
    regAF.low = bitReset(regAF.low, FLAG_HALFCARRY);

    // Copy bit 0 into carry flag
    regAF.low = isBitSet(bit0, 0) 
        ? bitSet(regAF.low, FLAG_CARRY)
        : bitReset(regAF.low, FLAG_CARRY);

    //cout << "RRA" << endl;

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
    regAF.low = bitReset(regAF.low, FLAG_SUB);
    regAF.low = bitReset(regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    regAF.low = (data == 0x0) 
        ? bitSet(regAF.low, FLAG_ZERO) 
        : bitReset(regAF.low, FLAG_ZERO);

    // Copy bit 7 into carry flag
    regAF.low = isBitSet(bit7, 0) 
        ? bitSet(regAF.low, FLAG_CARRY)
        : bitReset(regAF.low, FLAG_CARRY);

    //cout << "RLC_r" << endl;

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

    BYTE data = readMem(regHL.regstr);
    BYTE bit7 = data >> 7;

    // Shift data left and copy bit 7 to bit 0
    data <<= 1;
    data |= bit7;

    // Store result back into memory
    writeMem(regHL.regstr, data);

    // Reset subtract and half carry flag
    regAF.low = bitReset(regAF.low, FLAG_SUB);
    regAF.low = bitReset(regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    regAF.low = (data == 0x0) 
        ? bitSet(regAF.low, FLAG_ZERO) 
        : bitReset(regAF.low, FLAG_ZERO);

    // Copy bit 7 into carry flag
    regAF.low = isBitSet(bit7, 0) 
        ? bitSet(regAF.low, FLAG_CARRY)
        : bitReset(regAF.low, FLAG_CARRY);

    //cout << "RLC_HL" << endl;

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
    data |= (isBitSet(regAF.low, FLAG_CARRY) ? 0b1 : 0b0);

    // Store result back into r
    r = data;

    // Reset subtract and half carry flag
    regAF.low = bitReset(regAF.low, FLAG_SUB);
    regAF.low = bitReset(regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    regAF.low = (data == 0x0) 
        ? bitSet(regAF.low, FLAG_ZERO) 
        : bitReset(regAF.low, FLAG_ZERO);

    // Copy bit 7 into carry flag
    regAF.low = isBitSet(bit7, 0) 
        ? bitSet(regAF.low, FLAG_CARRY)
        : bitReset(regAF.low, FLAG_CARRY);

    //cout << "RL_r" << endl;

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

    BYTE data = readMem(regHL.regstr);
    BYTE bit7 = data >> 7;

    // Shift data left and copy old carry flag to bit 0
    data <<= 1;
    data |= (isBitSet(regAF.low, FLAG_CARRY) ? 0b1 : 0b0);

    // Store result back into memory
    writeMem(regHL.regstr, data);

    // Reset subtract and half carry flag
    regAF.low = bitReset(regAF.low, FLAG_SUB);
    regAF.low = bitReset(regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    regAF.low = (data == 0x0) 
        ? bitSet(regAF.low, FLAG_ZERO) 
        : bitReset(regAF.low, FLAG_ZERO);

    // Copy bit 7 into carry flag
    regAF.low = isBitSet(bit7, 0) 
        ? bitSet(regAF.low, FLAG_CARRY)
        : bitReset(regAF.low, FLAG_CARRY);

    //cout << "RL_HL" << endl;

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
    regAF.low = bitReset(regAF.low, FLAG_SUB);
    regAF.low = bitReset(regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    regAF.low = (data == 0x0) 
        ? bitSet(regAF.low, FLAG_ZERO) 
        : bitReset(regAF.low, FLAG_ZERO);

    // Copy bit 0 into carry flag
    regAF.low = isBitSet(bit0, 0) 
        ? bitSet(regAF.low, FLAG_CARRY)
        : bitReset(regAF.low, FLAG_CARRY);

    //cout << "RRC_r" << endl;

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

    BYTE data = readMem(regHL.regstr);
    BYTE bit0 = data & 0b1;

    // Shift data right and copy bit 0 to bit 7
    data >>= 1;
    data |= (bit0 << 7);

    // Store result back into memory
    writeMem(regHL.regstr, data);

    // Reset subtract and half carry flag
    regAF.low = bitReset(regAF.low, FLAG_SUB);
    regAF.low = bitReset(regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    regAF.low = (data == 0x0) 
        ? bitSet(regAF.low, FLAG_ZERO) 
        : bitReset(regAF.low, FLAG_ZERO);

    // Copy bit 0 into carry flag
    regAF.low = isBitSet(bit0, 0) 
        ? bitSet(regAF.low, FLAG_CARRY)
        : bitReset(regAF.low, FLAG_CARRY);

    //cout << "RRC_HL" << endl;

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
    data |= ((isBitSet(regAF.low, FLAG_CARRY) ? 0b1 : 0b0) << 7);

    // Store result back into r
    r = data;

    // Reset subtract and half carry flag
    regAF.low = bitReset(regAF.low, FLAG_SUB);
    regAF.low = bitReset(regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    regAF.low = (data == 0x0) 
        ? bitSet(regAF.low, FLAG_ZERO) 
        : bitReset(regAF.low, FLAG_ZERO);

    // Copy bit 0 into carry flag
    regAF.low = isBitSet(bit0, 0) 
        ? bitSet(regAF.low, FLAG_CARRY)
        : bitReset(regAF.low, FLAG_CARRY);

    //cout << "RR_r" << endl;

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

    BYTE data = readMem(regHL.regstr);
    BYTE bit0 = data & 0b1;

    // Shift data right and copy old carry flag to bit 7
    data >>= 1;
    data |= ((isBitSet(regAF.low, FLAG_CARRY) ? 0b1 : 0b0) << 7);

    // Store result back into memory
    writeMem(regHL.regstr, data);

    // Reset subtract and half carry flag
    regAF.low = bitReset(regAF.low, FLAG_SUB);
    regAF.low = bitReset(regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    regAF.low = (data == 0x0) 
        ? bitSet(regAF.low, FLAG_ZERO) 
        : bitReset(regAF.low, FLAG_ZERO);

    // Copy bit 0 into carry flag
    regAF.low = isBitSet(bit0, 0) 
        ? bitSet(regAF.low, FLAG_CARRY)
        : bitReset(regAF.low, FLAG_CARRY);

    //cout << "RR_HL" << endl;

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
    regAF.low = bitReset(regAF.low, FLAG_SUB);
    regAF.low = bitReset(regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    regAF.low = (data == 0x0) 
        ? bitSet(regAF.low, FLAG_ZERO) 
        : bitReset(regAF.low, FLAG_ZERO);

    // Copy bit 7 into carry flag
    regAF.low = isBitSet(bit7, 0) 
        ? bitSet(regAF.low, FLAG_CARRY)
        : bitReset(regAF.low, FLAG_CARRY);

    //cout << "SLA_r" << endl;

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

    BYTE data = readMem(regHL.regstr);
    BYTE bit7 = data >> 7;

    // Shift data left and store it back into memory
    data <<= 1;
    writeMem(regHL.regstr, data);

    // Reset subtract and half carry flag
    regAF.low = bitReset(regAF.low, FLAG_SUB);
    regAF.low = bitReset(regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    regAF.low = (data == 0x0) 
        ? bitSet(regAF.low, FLAG_ZERO) 
        : bitReset(regAF.low, FLAG_ZERO);

    // Copy bit 7 into carry flag
    regAF.low = isBitSet(bit7, 0) 
        ? bitSet(regAF.low, FLAG_CARRY)
        : bitReset(regAF.low, FLAG_CARRY);

    //cout << "SLA_HL" << endl;

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
    regAF.low = bitReset(regAF.low, FLAG_SUB);
    regAF.low = bitReset(regAF.low, FLAG_HALFCARRY);
    regAF.low = bitReset(regAF.low, FLAG_CARRY);

    // Update zero flag
    regAF.low = (data == 0x0) 
        ? bitSet(regAF.low, FLAG_ZERO) 
        : bitReset(regAF.low, FLAG_ZERO);

    //cout << "SWAP_r" << endl;

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

    BYTE data = readMem(regHL.regstr);
    BYTE lowerNibble = data & 0x0F;
    BYTE upperNibble = (data & 0xF0) >> 4;

    // Swap the nibbles in data and store it back into memory
    data = (lowerNibble << 4) | upperNibble;
    writeMem(regHL.regstr, data);

    // Reset subtract, halfcarry and carry flag
    regAF.low = bitReset(regAF.low, FLAG_SUB);
    regAF.low = bitReset(regAF.low, FLAG_HALFCARRY);
    regAF.low = bitReset(regAF.low, FLAG_CARRY);

    // Update zero flag
    regAF.low = (data == 0x0) 
        ? bitSet(regAF.low, FLAG_ZERO) 
        : bitReset(regAF.low, FLAG_ZERO);

    //cout << "SWAP_HL" << endl;

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
    regAF.low = bitReset(regAF.low, FLAG_SUB);
    regAF.low = bitReset(regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    regAF.low = (data == 0x0) 
        ? bitSet(regAF.low, FLAG_ZERO) 
        : bitReset(regAF.low, FLAG_ZERO);

    // Copy bit 0 into carry flag
    regAF.low = isBitSet(bit0, 0) 
        ? bitSet(regAF.low, FLAG_CARRY)
        : bitReset(regAF.low, FLAG_CARRY);

    //cout << "SRA_r" << endl;

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

    BYTE data = readMem(regHL.regstr);
    BYTE bit0 = data & 0b1;

    // Shift data right, persist bit 7, and store it back into memory
    data = (data >> 1) | (data & 0x80);
    writeMem(regHL.regstr, data);

    // Reset subtract and half carry flag
    regAF.low = bitReset(regAF.low, FLAG_SUB);
    regAF.low = bitReset(regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    regAF.low = (data == 0x0) 
        ? bitSet(regAF.low, FLAG_ZERO) 
        : bitReset(regAF.low, FLAG_ZERO);

    // Copy bit 0 into carry flag
    regAF.low = isBitSet(bit0, 0) 
        ? bitSet(regAF.low, FLAG_CARRY)
        : bitReset(regAF.low, FLAG_CARRY);

    //cout << "SRA_HL" << endl;

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
    data = bitReset(data, 7);

    // Store result back into r
    r = data;

    // Reset subtract and half carry flag
    regAF.low = bitReset(regAF.low, FLAG_SUB);
    regAF.low = bitReset(regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    regAF.low = (data == 0x0) 
        ? bitSet(regAF.low, FLAG_ZERO) 
        : bitReset(regAF.low, FLAG_ZERO);

    // Copy bit 0 into carry flag
    regAF.low = isBitSet(bit0, 0) 
        ? bitSet(regAF.low, FLAG_CARRY)
        : bitReset(regAF.low, FLAG_CARRY);

    //cout << "SRL_r" << endl;

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

    BYTE data = readMem(regHL.regstr);
    BYTE bit0 = data & 0b1;

    // Shift data right, reset bit 7
    data >>= 1;
    data = bitReset(data, 7);

    // Store result back into memory
    writeMem(regHL.regstr, data);

    // Reset subtract and half carry flag
    regAF.low = bitReset(regAF.low, FLAG_SUB);
    regAF.low = bitReset(regAF.low, FLAG_HALFCARRY);

    // Update zero flag
    regAF.low = (data == 0x0) 
        ? bitSet(regAF.low, FLAG_ZERO) 
        : bitReset(regAF.low, FLAG_ZERO);

    // Copy bit 0 into carry flag
    regAF.low = isBitSet(bit0, 0) 
        ? bitSet(regAF.low, FLAG_CARRY)
        : bitReset(regAF.low, FLAG_CARRY);

    //cout << "SRL_HL" << endl;

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
    regAF.low = isBitSet(r, n) 
        ? bitReset(regAF.low, FLAG_ZERO)
        : bitSet(regAF.low, FLAG_ZERO);
    
    // Reset subtract flag, set halfcarry flag
    regAF.low = bitReset(regAF.low, FLAG_SUB);
    regAF.low = bitSet(regAF.low, FLAG_HALFCARRY);

    //cout << "BIT_n_r" << endl;

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
    regAF.low = isBitSet(readMem(regHL.regstr), n) 
        ? bitReset(regAF.low, FLAG_ZERO)
        : bitSet(regAF.low, FLAG_ZERO);
    
    // Reset subtract flag, set halfcarry flag
    regAF.low = bitReset(regAF.low, FLAG_SUB);
    regAF.low = bitSet(regAF.low, FLAG_HALFCARRY);

    //cout << "BIT_n_HL" << endl;

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

    r = bitSet(r, n);

    //cout << "SET_n_r" << endl;

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
    BYTE result = bitSet(readMem(regHL.low), n);
    writeMem(regHL.low, result);

    //cout << "SET_n_HL" << endl;

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

    r = bitReset(r, n);

    //cout << "RES_n_r" << endl;

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
    BYTE result = bitReset(readMem(regHL.low), n);
    writeMem(regHL.low, result);

    //cout << "RES_n_HL" << endl;

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
    regAF.low = bitReset(regAF.low, FLAG_SUB);
    regAF.low = bitReset(regAF.low, FLAG_HALFCARRY);

    // Toggling carry flag
    regAF.low = isBitSet(regAF.low, FLAG_CARRY)
        ? bitReset(regAF.low, FLAG_CARRY)
        : bitSet(regAF.low, FLAG_CARRY);

    //cout << "CCF" << endl;

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
    regAF.low = bitReset(regAF.low, FLAG_SUB);
    regAF.low = bitReset(regAF.low, FLAG_HALFCARRY);

    // Set carry flag
    regAF.low = bitSet(regAF.low, FLAG_CARRY);

    //cout << "SCF" << endl;

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
    // //cout << "NOP" << endl;
    return 4;

}

/*
    HALT  (0x76)

    4 cycles

    Flags affect (znhc): ----
*/
int Emulator::HALT() {

    isHalted = true;

    //cout << "HALT" << endl;

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
    
    //cout << "STOP" << endl;

    return HALT();

}

/*
    DI  (0xF3)

    Disable interrupts, IME = 0.

    4 cycles

    Flags affected (znhc): ----
*/
int Emulator::DI() {
    
    InterruptMasterEnabled = false;

    //cout << "DI" << endl;

    return 4;

}

/*
    EI  (0xFB)

    Enable interrupts, IME = 1.

    4 cycles

    Flags affected (znhc): ----
*/
int Emulator::EI() {

    InterruptMasterEnabled = true;

    //cout << "EI" << endl;

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

    WORD lowByte = readMem(programCounter.regstr);
    WORD highByte = readMem(programCounter.regstr + 1);

    programCounter.regstr = (highByte << 8) | lowByte;

    //cout << "JP_nn" << endl;
    return 16;

}

/*
    JP nn  (0xE9)

    Jump to HL, PC = HL.

    4 cycles

    Flags affected (znhc): ----
*/
int Emulator::JP_HL() {

    programCounter.regstr = regHL.regstr;

    //cout << "JP_HL" << endl;

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
    WORD lowByte = readMem(programCounter.regstr);
    WORD highByte = readMem(programCounter.regstr + 1);
    WORD nn = (highByte << 8) | lowByte;
    programCounter.regstr += 2;

    bool jump = false;
    switch ((opcode >> 3) & 0x03) {
        case 0x00: // NZ
            jump = !(isBitSet(regAF.low, FLAG_ZERO));
            break;
        case 0x01: // Z
            jump = isBitSet(regAF.low, FLAG_ZERO);
            break;
        case 0x02: // NC
            jump = !(isBitSet(regAF.low, FLAG_CARRY));
            break;
        case 0x03: // C
            jump = isBitSet(regAF.low, FLAG_CARRY);
            break;
    }

    //cout << "JP_f_nn" << endl;

    if (jump) {
        programCounter.regstr = nn;
        return 16;
    } else {
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

    SIGNED_BYTE dd = static_cast<SIGNED_BYTE>(readMem(programCounter.regstr));
    programCounter.regstr++;

    programCounter.regstr += dd;

    //cout << "JR_PCdd" << endl;

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

    SIGNED_BYTE dd = static_cast<SIGNED_BYTE>(readMem(programCounter.regstr));
    programCounter.regstr++;

    bool jump = false;
    switch ((opcode >> 3) & 0x03) {
        case 0x00: // NZ
            jump = !(isBitSet(regAF.low, FLAG_ZERO));
            break;
        case 0x01: // Z
            jump = isBitSet(regAF.low, FLAG_ZERO);
            break;
        case 0x02: // NC
            jump = !(isBitSet(regAF.low, FLAG_CARRY));
            break;
        case 0x03: // C
            jump = isBitSet(regAF.low, FLAG_CARRY);
            break;
    }

    //cout << "JR_f_PCDD" << endl;

    if (jump) {
        programCounter.regstr += dd;
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
    WORD lowByte = readMem(programCounter.regstr);
    WORD highByte = readMem(programCounter.regstr + 1);
    programCounter.regstr += 2;
    WORD nn = (highByte << 8) | lowByte;

    // Push PC onto stack
    stackPointer.regstr--;
    writeMem(stackPointer.regstr, programCounter.high);
    stackPointer.regstr--;
    writeMem(stackPointer.regstr, programCounter.low);    

    // Set PC to nn
    programCounter.regstr = nn;

    //cout << "CALL_nn" << endl;

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
    WORD lowByte = readMem(programCounter.regstr);
    WORD highByte = readMem(programCounter.regstr + 1);
    programCounter.regstr += 2;
    WORD nn = (highByte << 8) | lowByte;

    bool call = false;
    switch ((opcode >> 3) & 0x03) {
        case 0x00: // NZ
            call = !(isBitSet(regAF.low, FLAG_ZERO));
            break;
        case 0x01: // Z
            call = isBitSet(regAF.low, FLAG_ZERO);
            break;
        case 0x02: // NC
            call = !(isBitSet(regAF.low, FLAG_CARRY));
            break;
        case 0x03: // C
            call = isBitSet(regAF.low, FLAG_CARRY);
            break;
    }

    //cout << "CALL_f_nn" << endl;

    if (call) {

        // Push PC onto stack
        stackPointer.regstr--;
        writeMem(stackPointer.regstr, programCounter.high); 
        stackPointer.regstr--;
        writeMem(stackPointer.regstr, programCounter.low);    

        // Set PC to nn
        programCounter.regstr = nn;

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
    WORD lowByte = readMem(stackPointer.regstr);
    stackPointer.regstr++;
    WORD highByte = readMem(stackPointer.regstr);
    stackPointer.regstr++;

    // Set PC to address
    programCounter.regstr = (highByte << 8) | lowByte;

    //cout << "RET" << endl;

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
            doRET = !(isBitSet(regAF.low, FLAG_ZERO));
            break;
        case 0x01: // Z
            doRET = isBitSet(regAF.low, FLAG_ZERO);
            break;
        case 0x02: // NC
            doRET = !(isBitSet(regAF.low, FLAG_CARRY));
            break;
        case 0x03: // C
            doRET = isBitSet(regAF.low, FLAG_CARRY);
            break;
    }

    //cout << "RET_f" << endl;

    if (doRET) {

        // Pop address from stack
        WORD lowByte = readMem(stackPointer.regstr);
        stackPointer.regstr++;
        WORD highByte = readMem(stackPointer.regstr);
        stackPointer.regstr++;

        // Set PC to address
        programCounter.regstr = (highByte << 8) | lowByte;

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
    WORD lowByte = readMem(stackPointer.regstr);
    stackPointer.regstr++;
    WORD highByte = readMem(stackPointer.regstr);
    stackPointer.regstr++;

    // Set PC to address
    programCounter.regstr = (highByte << 8) | lowByte;

    // Enable interrupts
    InterruptMasterEnabled = true;

    //cout << "RETI" << endl;

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
    stackPointer.regstr--;
    writeMem(stackPointer.regstr, programCounter.high);
    stackPointer.regstr--;
    writeMem(stackPointer.regstr, programCounter.low);    

    // Set PC to n
    BYTE t = ((opcode >> 3) & 0x07);
    programCounter.regstr = (WORD)(t * 0x08);

    //cout << "RST_n" << endl;

    return 16;
    
}