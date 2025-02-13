#include "registers.h"

#include <cassert>
#include <format>
#include <sstream>

#include "util/log.h"

using namespace util;

namespace PSX {

std::ostream& operator<<(std::ostream &os, const Registers &registers) {
    os <<  std::format("pc\t0x{:08X}\n", registers.pc);
    for (int i = 0; i < 16; ++i) {
        os << Registers::REGISTER_NAMES[i] << "/r" << i << "\t"
           << std::format("0x{:08X}", registers.registers[i]);
        os << ", ";
        os << Registers::REGISTER_NAMES[16 + i] << "/r" << 16 + i << "\t"
           << std::format("0x{:08X}", registers.registers[16 + i]);
        os << ",\n";
    }
    os << "hi\t" << std::format("0x{:08X}", registers.hi);
    os << ", ";
    os << "lo\t\t" << std::format("0x{:08X}", registers.lo);

    return os;
}

const char* Registers::REGISTER_NAMES[] = {
    "zero", // always returns zero
    "at", // assembler temporary (reserved for use by assembler)
    "v0", "v1", // value returned by subroutine
    "a0", "a1", "a2", "a3", // arguments: first four parameters for a subroutine
    "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
        // temporaries: subroutines may use without saving
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
        // subroutine register variables:
        // subroutines have to save and restore these so that
        // the calling subroutine sees their values preserved
    "t8", "t9", // more temporaries
    "k0", "k1", // reserved for use by exception handler
    "gp", // global pointer: can be maintained by OS for easy access to static variables
    "sp", // stack pointer
    "fp", // (also called s8) 9th subroutine register variable / frame pointer
    "ra" // return address for subroutine
};

std::string Registers::getRegisterName(uint8_t reg) {
    return REGISTER_NAMES[reg];
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
}

uint32_t Registers::getPC() {
    LOG_REG_PC_READ(std::format(" {{pc -> 0x{:08X}}}", this->pc));

    return this->pc;
}

void Registers::setPC(uint32_t pc) {
    if (pc != this->pc + 4) {
        LOG_REG_PC_WRITE(std::format(" {{0x{:08X} -> pc}}", pc));
    } else {
        LOG_REG_PC_WRITE(std::format(" {{pc}}"));
    }

    this->pc = pc;
}

uint32_t Registers::getRegister(uint8_t rt) {
    LOG_REG_READ(std::format(" {{"));
    assert (rt < 32);
    uint32_t word = registers[rt];

    LOG_REG_READ(std::format("{:s} -> 0x{:08X}}}",
                         getRegisterName(rt), word));

    return word;
}

void Registers::setRegister(uint8_t rt, uint32_t value) {
    assert (rt < 32);
    LOG_REG_WRITE(std::format(" {{0x{:08X} -> {:s}}}", value, getRegisterName(rt)));

    if (rt > 0) {
        registers[rt] = value;
    }
}

uint32_t Registers::getHi() {
    LOG_REG_READ(std::format(" {{hi -> 0x{:08X}}}", this->hi));

    return this->hi;
}

void Registers::setHi(uint32_t value) {
    LOG_REG_WRITE(std::format(" {{0x{:08X} -> hi}}", value));

    this->hi = value;
}

uint32_t Registers::getLo() {
    LOG_REG_READ(std::format(" {{lo -> 0x{:08X}}}", this->lo));

    return this->lo;
}

void Registers::setLo(uint32_t value) {
    LOG_REG_WRITE(std::format(" {{0x{:08X} -> lo}}", value));

    this->lo = value;
}
}
