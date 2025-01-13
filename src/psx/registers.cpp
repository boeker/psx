#include "registers.h"

#include <cassert>
#include <format>

namespace PSX {

const char* Registers::REGISTER_NAMES[] = {
    // always returns zero
    "zero",
    // assembler temporary (reserved for use by assembler)
    "at",
    // value returned by subroutine
    "v0", "v1",
    // arguments: first four parameters for a subroutine
    "a0", "a1", "a2", "a3",
    // temporaries: subroutines may use without saving
    "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
    // subroutine register variables:
    // subroutines have to save and restore these so that
    // the calling subroutine sees their values preserved
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    // more temporaries
    "t8", "t9",
    // reserved for use by exception handler
    "k0", "k1",
    // global pointer: can be maintained by OS for easy access to static variables
    "gp",
    // stack pointer
    "sp",
    // (also called s8) 9th subroutine register variable / frame pointer
    "fp",
    // return address for subroutine
    "ra"
};


std::ostream& operator<<(std::ostream &os, const Registers &registers) {
    os << "[";
    for (int i = 0; i < 32; ++i) {
        os << Registers::REGISTER_NAMES[i] << "/r" << i << ": " << std::format("0x{:08X}", registers.registers[i]);
        if (i < 31) {
            os << ", ";
        }
    }
    os << "]";

    return os;
}

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
