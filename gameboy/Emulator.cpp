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

    // Joypad
    this->joypadState = 0xFF;

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

int Emulator::executeNextOpcode() {
    int clockCycles;

    BYTE opcode = this->readMem(this->programCounter.regstr);
    clockCycles = this->executeOpcode(opcode);
    this->programCounter.regstr++;

    return clockCycles;
}

int Emulator::executeOpcode(BYTE opcode) {

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
    this->bitSet(requestReg, interruptID); // Set the corresponding bit in the interrupt req register 0xFF0F
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
    BYTE requestReg = this->readMem(0xFF0F);
    this->bitReset(requestReg, interruptID); // Resetting the n-th bit
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

    this->joypadState = bitReset(this->joypadState, key); // Set the key to 0 (pressed)

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

Sprites always use 8000 addressing, but the BG and Window can use either mode, controlled by LCDC bit 4.

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
    if (this->isBitSet(lcdControl, 0)) {
        this->renderTiles(lcdControl);
    }

    if (this->isBitSet(lcdControl, 1)) {
        this->renderSprites(lcdControl);
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
        int red = 0; 
        int green = 0;
        int blue = 0;

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
        }

        // Update Screen pixels
        // BYTE currentLine = this->readMem(0xFF44);
        // this->gameScreen[pixel][currentLine][0] = red;
        // this->gameScreen[pixel][currentLine][1] = green;
        // this->gameScreen[pixel][currentLine][2] = blue;

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

                // white is transparent for sprites
                if (colour == WHITE) {
                    continue;
                }

                // Default colour is black where RGB = [0,0,0]
                int red = 0;
                int green = 0;
                int blue = 0;

                switch (colour) {
                    // IS THIS CASE REALLY NEEDED??
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
                }

                // Get the pixel to draw
                int xPix = 0 - tilePixel;
                xPix += 7;
                int pixel = xPos + xPix;

                // check if pixel is hidden behind background
                if (this->isBitSet(attributes, 7)) {
                    if ((m_ScreenData[scanLine][pixel][0] != 255) || (m_ScreenData[scanLine][pixel][1] != 255) || (m_ScreenData[scanLine][pixel][2] != 255) )
                        continue ;
                }
                // Update Screen pixels
                // this->gameScreen[pixel][scanLine][0] = red;
                // this->gameScreen[pixel][scanLine][1] = green;
                // this->gameScreen[pixel][scanLine][2] = blue;

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
16 bit Arithmetic/logical Commands
********************************************************************************
*/

/*
    ADD HL, ss  (0xX9)

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
    RRA  (0x17)

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
int Emulator::BIT_n_r(BYTE& r, BYTE n) {

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
int Emulator::BIT_n_HL(BYTE n) {

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
int Emulator::SET_n_r(BYTE& r, BYTE n) {

    r = this->bitSet(r, n);

    return 8;

}

/*
    SET n, (HL)  (CB 0xXX)

    Set bit n of contents at memory location HL.

    16 cycles

    Flags affected (znhc): ----
*/
int Emulator::SET_n_HL(BYTE n) {

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
int Emulator::RES_n_r(BYTE& r, BYTE n) {

    r = this->bitReset(r, n);

    return 8;

}

/*
    RES n, (HL)  (CB 0xXX)

    Reset bit n of contents at memory location HL.

    16 cycles

    Flags affected (znhc): ----
*/
int Emulator::RES_n_HL(BYTE n) {

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