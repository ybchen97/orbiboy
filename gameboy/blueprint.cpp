/*

********************************************************************************
PARTS NEEDED FOR EMULATOR - General Information
********************************************************************************

1. CPU : 
- main CPU functions such as initialize, load, fetch, execute
- all opcode functions 
- cpu attributes like pc, registers, stack etc.

2. Memory :
- 0x10000 long memory array (0x0000 - 0xFFFF)
- memory initialize function (to set all locations to 0 except for the special
locations)
- Read & write memory functions

3. Memory Bank Controller :

4. GPU : 

5. APU : 

6. Joypad : 

********************************************************************************
Memory Details
********************************************************************************

General Memory Map:

0000-3FFF   16KB ROM Bank 00     (in cartridge, fixed at bank 00)
4000-7FFF   16KB ROM Bank 01..NN (in cartridge, switchable bank number)
8000-9FFF   8KB Video RAM (VRAM) (switchable bank 0-1 in CGB Mode)
A000-BFFF   8KB External RAM     (in cartridge, switchable bank, if any)
C000-CFFF   4KB Work RAM Bank 0 (WRAM)
D000-DFFF   4KB Work RAM Bank 1 (WRAM)  (switchable bank 1-7 in CGB Mode)
E000-FDFF   Same as C000-DDFF (ECHO)    (typically not used)
FE00-FE9F   Sprite Attribute Table (OAM)
FEA0-FEFF   Not Usable
FF00-FF7F   I/O Ports
FF80-FFFE   High RAM (HRAM)
FFFF        Interrupt Enable Register


When powering up, these locations are set to specific values:

[$FF05] = $00   ; TIMA
[$FF06] = $00   ; TMA
[$FF07] = $00   ; TAC
[$FF10] = $80   ; NR10
[$FF11] = $BF   ; NR11
[$FF12] = $F3   ; NR12
[$FF14] = $BF   ; NR14
[$FF16] = $3F   ; NR21
[$FF17] = $00   ; NR22
[$FF19] = $BF   ; NR24
[$FF1A] = $7F   ; NR30
[$FF1B] = $FF   ; NR31
[$FF1C] = $9F   ; NR32
[$FF1E] = $BF   ; NR33
[$FF20] = $FF   ; NR41
[$FF21] = $00   ; NR42
[$FF22] = $00   ; NR43
[$FF23] = $BF   ; NR30
[$FF24] = $77   ; NR50
[$FF25] = $F3   ; NR51
[$FF26] = $F1-GB, $F0-SGB ; NR52
[$FF40] = $91   ; LCDC
[$FF42] = $00   ; SCY
[$FF43] = $00   ; SCX
[$FF45] = $00   ; LYC
[$FF47] = $FC   ; BGP
[$FF48] = $FF   ; OBP0
[$FF49] = $FF   ; OBP1
[$FF4A] = $00   ; WY
[$FF4B] = $00   ; WX
[$FFFF] = $00   ; IE

*/