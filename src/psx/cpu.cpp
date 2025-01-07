#include "cpu.h"

#include <cassert>

namespace PSX {
CPU::CPU() {
    reset();
}

void CPU::reset() {
    for (int i = 0; i < 32; ++i) {
        this->registers[i] = 0;
    }
    this->pc = 0xBFC00000; // default value after power on reset
    this->hi = 0;
    this->lo = 0;
}

uint32_t CPU::getPC() {
    return this->pc;
}

void CPU::setPC(uint32_t pc) {
    this->pc = pc;
}

uint32_t CPU::getRegister(uint8_t rt) {
    assert (rt < 32);
    return registers[rt];
}

void CPU::setRegister(uint8_t rt, uint32_t value) {
    assert (rt < 32);
    if (rt > 0) {
        registers[rt] = value;
    }
}
}
