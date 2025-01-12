#include "registers.h"

#include <cassert>

namespace PSX {
Registers::Registers() {
    reset();
}

void Registers::reset() {
    for (int i = 0; i < 32; ++i) {
        this->registers[i] = 0;
    }
    this->pc = 0xBFC00000; // default value after power on reset
    this->hi = 0;
    this->lo = 0;

    
    for (int i = 0; i < 32; ++i) {
        this->cp0Registers[i] = 0;
    }
    cp0Registers[15] = 0x00000001;
}

uint32_t Registers::getPC() {
    return this->pc;
}

void Registers::setPC(uint32_t pc) {
    this->pc = pc;
}

uint32_t Registers::getRegister(uint8_t rt) {
    assert (rt < 32);
    return registers[rt];
}

void Registers::setRegister(uint8_t rt, uint32_t value) {
    assert (rt < 32);
    if (rt > 0) {
        registers[rt] = value;
    }
}

uint32_t Registers::getCP0Register(uint8_t rt) {
    assert (rt < 32);
    return cp0Registers[rt];
}

void Registers::setCP0Register(uint8_t rt, uint32_t value) {
    assert (rt < 32);
    if (rt > 0) {
        cp0Registers[rt] = value;
    }
}

bool Registers::statusRegisterIsolateCacheIsSet() const {
    return cp0Registers[CP0_REGISTER_SR] & (1 << 16);
}
}
