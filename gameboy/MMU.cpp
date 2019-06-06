#include "MMU.hpp";

void Emulator::writeMem(WORD address, BYTE data) {
    if (address < 0x8000) {}

    // writing to Echo RAM also writes to work RAM
    else if ((address >= 0xE000) && (address <= 0xFDFF)) {
        this->internalMem[address] = data;
        writeMem(address - 0x2000, data);
    }

    else if ((address >= 0xFEA0) && (address <= 0xFEFF)) {}

    else {
        this->internalMem[address] = data;
    }

}

BYTE Emulator::readMem(WORD address) const {

}