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
typedef char SIGNED_BYTE;
typedef unsigned short WORD; 
typedef signed short SIGNED_WORD;

union Register {
    WORD regstr;
    struct {
        BYTE low;
        BYTE high;
    };
};


class Emulator {

    public:
        // ATTRIBUTES

        // FUNCTIONS
        void resetCPU();
        bool loadGame(string);
        void writeMem(WORD, BYTE);
        BYTE readMem(WORD) const;

        // Utility
        bool isBitSet(BYTE, int);
        BYTE bitSet(BYTE, int);
        BYTE bitReset(BYTE, int);

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

        // Graphics
        int scanlineCycleCount;

        // FUNCTIONS
        void update();

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

        // Graphics
        void updateGraphics(int);
        void setLCDStatus();
        bool LCDEnabled();

};
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
    this->internalMem[0xFF40] = 0x91; 
    this->internalMem[0xFF42] = 0x00; 
    this->internalMem[0xFF43] = 0x00; 
    this->internalMem[0xFF45] = 0x00; // LYC
    this->internalMem[0xFF47] = 0xFC; 
    this->internalMem[0xFF48] = 0xFF; 
    this->internalMem[0xFF49] = 0xFF; 
    this->internalMem[0xFF4A] = 0x00; 
    this->internalMem[0xFF4B] = 0x00; 
    this->internalMem[0xFFFF] = 0x00; // IE

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

    // Graphics
    this->scanlineCycleCount = 456;

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
    
    // Read file into gameMemory
    fread(this->cartridgeMem, fileSize, 1, in);
    fclose(in);

    return true;
}

void Emulator::update() { // MAIN UPDATE LOOP

    // update function called 60 times per second -> screen rendered @ 60fps

    const int maxCycles = 69905;
    int cyclesCount = 0;

    while (cyclesCount < maxCycles) {
        int cycles = this->executeNextOpcode(); //executeNextOpcode will return the number of cycles taken
        cyclesCount += cycles;

        this->updateTimers(cycles); //wishful thinking
        this->updateGraphics(cycles); //wishful thinking
        this->handleInterrupts(); //wishful thinking
    }
    this->RenderScreen(); //wishful thinking
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

    else if ((address >= 0xFEA0) && (address <= 0xFEFF)) {}

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
    bitSet(requestReg, interruptID); // Set the corresponding bit in the interrupt req register 0xFF0F
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
    bitReset(requestReg, interruptID); // Resetting the n-th bit
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
        // set the mode to 1 during lcd disabled and reset scanline
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

/*
********************************************************************************
Utility Functions
********************************************************************************
*/

bool Emulator::isBitSet(BYTE data, int position) {
    return (data >> position) & 0x1 == 0x1;
}

BYTE Emulator::bitSet(BYTE data, int position) {
    int mask = 1 << position;
    return data |= mask;
}

BYTE Emulator::bitReset(BYTE data, int position) {
    int mask = ~(1 << position);
    return data &= mask;
}