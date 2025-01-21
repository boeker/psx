#include "registers.h"

#include <cassert>
#include <format>

#include "util/log.h"

using namespace util;

namespace PSX {

std::ostream& operator<<(std::ostream &os, const Registers &registers) {
    for (int i = 0; i < 16; ++i) {
        os << Registers::REGISTER_NAMES[i] << "/r" << i << ": " << std::format("0x{:08X}", registers.registers[i]);
        os << ", ";
        os << Registers::REGISTER_NAMES[16 + i] << "/r" << 16 + i << ": " << std::format("0x{:08X}", registers.registers[16 + i]);
        os << ",\n";
    }
    os << "hi: " << std::format("0x{:08X}", registers.hi);
    os << ", ";
    os << "lo: " << std::format("0x{:08X}", registers.lo);

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

const char* Registers::CP0_REGISTER_NAMES[] = {
    // unknown
    "CP0_r0",        "CP0_r1",
    "BusCtrl",  // 2: configure bus interface signals
    "BPC",      // 3: Breakpoint on execute
    "CP0_r4",
    "BDA",      // 5: Breakpoint on data access
    "JUMPDEST", // 6: Randomly memorized jump address
    "DCIC",     // 7: Breakpoint control
    "BadVaddr", // 8: Contains the last invalid program address which caused a trap
    "BDAM",     // 9: Data-access-breakpoint mask
    "CP0_r10",
    "BPCM",     // 11: Execute-breakpoint mask
    "SR",       // 12: (status register) cpu mode flags
    "Cause",    // 13: Describes the most recently recognized exception
    "EPC",      // 14: Return address from trap
    "PRId",     // 15: CP0 type and rev level
    "CP0_r16",       "CP0_r17",       "CP0_r18",       "CP0_r19",
    "CP0_r20",       "CP0_r21",       "CP0_r22",       "CP0_r23",
    "CP0_r24",       "CP0_r25",       "CP0_r26",       "CP0_r27",
    "CP0_r28",       "CP0_r29",       "CP0_r30",       "CP0_r31"
};

std::string Registers::getRegisterName(uint8_t reg) {
    return REGISTER_NAMES[reg];
}

std::string Registers::getCP0RegisterName(uint8_t reg) {
    return CP0_REGISTER_NAMES[reg];
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
    Log::log(std::format(" {{r pc -0x{:08X}->}}", this->pc), Log::Type::REGISTER_PC_READ);

    return this->pc;
}

void Registers::setPC(uint32_t pc) {
    if (pc != this->pc + 4) {
        Log::log(std::format(" {{w -0x{:08X}-> pc}}", pc), Log::Type::REGISTER_PC_WRITE);
    } else {
        Log::log(std::format(" {{w pc}}"), Log::Type::REGISTER_PC_WRITE);
    }

    this->pc = pc;
}

uint32_t Registers::getRegister(uint8_t rt) {
    Log::log(std::format(" {{r "), Log::Type::REGISTER_READ);
    assert (rt < 32);
    uint32_t word = registers[rt];

    Log::log(std::format("{:s} -0x{:08X}->}}",
                         getRegisterName(rt), word), Log::Type::REGISTER_READ);

    return word;
}

void Registers::setRegister(uint8_t rt, uint32_t value) {
    assert (rt < 32);
    Log::log(std::format(" {{w -0x{:08X}-> {:s}}}", value, getRegisterName(rt)),
             Log::Type::REGISTER_WRITE);

    if (rt > 0) {
        registers[rt] = value;
    }
}

uint32_t Registers::getHi() {
    return this->hi;
}

void Registers::setHi(uint32_t value) {
    this->hi = value;
}

uint32_t Registers::getLo() {
    return this->lo;
}

void Registers::setLo(uint32_t value) {
    this->lo = value;
}

uint32_t Registers::getCP0Register(uint8_t rt) {
    Log::log(std::format(" {{r "), Log::Type::CP0_REGISTER_READ);
    assert (rt < 32);

    uint32_t word = cp0Registers[rt];
    Log::log(std::format("{:s} -0x{:08X}->}}",
                         getCP0RegisterName(rt), word), Log::Type::CP0_REGISTER_READ);

    return word;
}

void Registers::setCP0Register(uint8_t rt, uint32_t value) {
    assert (rt < 32);
    Log::log(std::format(" {{w -0x{:08X}-> {:s}}}", value, getCP0RegisterName(rt)),
             Log::Type::CP0_REGISTER_WRITE);
    if (rt > 0) {
        cp0Registers[rt] = value;
    }
}

bool Registers::statusRegisterIsolateCacheIsSet() const {
    return cp0Registers[CP0_REGISTER_SR] & (1 << 16);
}
}
