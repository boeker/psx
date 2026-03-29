#include "cp0.h"

#include <cassert>
#include <format>
#include <sstream>

#include "exceptions/exceptions.h"
#include "util/log.h"

using namespace util;

namespace PSX {

std::string CP0::getSRExplanation() const {
    std::stringstream ss;

    uint32_t sr = cp0Registers[CP0_REGISTER_SR];

    ss << std::format("CU3[{:01b}] ", (sr >> 31) & 0x1);
    ss << std::format("CU2[{:01b}] ", (sr >> 30) & 0x1);
    ss << std::format("CU1[{:01b}] ", (sr >> 29) & 0x1);
    ss << std::format("CU0[{:01b}] ", (sr >> 28) & 0x1);
    ss << std::format("RE[{:01b}] ", (sr >> 25) & 0x1);
    ss << std::format("BEV[{:01b}] ", (sr >> 22) & 0x1);
    ss << std::format("TS[{:01b}] ", (sr >> 21) & 0x1);
    ss << std::format("PE[{:01b}] ", (sr >> 20) & 0x1);
    ss << std::format("CM[{:01b}] ", (sr >> 19) & 0x1);
    ss << std::format("PZ[{:01b}] ", (sr >> 18) & 0x1);
    ss << std::format("SwC[{:01b}] ", (sr >> 17) & 0x1);
    ss << std::format("IsC[{:01b}] ", (sr >> 16) & 0x1);
    ss << std::format("IM[{:08b}] ", (sr >> 8) & 0xFF);
    ss << std::format("KUo[{:01b}] ", (sr >> 5) & 0x1);
    ss << std::format("IEo[{:01b}] ", (sr >> 4) & 0x1);
    ss << std::format("KUp[{:01b}] ", (sr >> 3) & 0x1);
    ss << std::format("IEp[{:01b}] ", (sr >> 2) & 0x1);
    ss << std::format("KUc[{:01b}] ", (sr >> 1) & 0x1);
    ss << std::format("IEc[{:01b}] ", (sr >> 0) & 0x1);

    return ss.str();
}

std::string CP0::getCauseExplanation() const {
    std::stringstream ss;

    uint32_t cause = cp0Registers[CP0_REGISTER_CAUSE];

    ss << std::format("BD[{:01b}] ", (cause >> 31) & 0x1);
    ss << std::format("CE[{:02b}] ", (cause >> 28) & 0x3);
    ss << std::format("IP[{:08b}] ", (cause >> 8) & 0xFF);
    ss << std::format("ExcCode[{:05b}] ", (cause >> 2) & 0x1F);

    return ss.str();
}

std::ostream& operator<<(std::ostream &os, const CP0 &registers) {
    for (int i = 0; i < 8; ++i) {
        os << CP0::CP0_REGISTER_NAMES[i] << (i != 6 ? "\t" : "")
           << std::format("0x{:08X}", registers.cp0Registers[i]);
        os << ", ";
        os << CP0::CP0_REGISTER_NAMES[8 + i] << (i != 4 && i != 6 ? "\t" : "\t\t")
           << std::format("0x{:08X}", registers.cp0Registers[8 + i]);
        os << ",\n";
    }
    os << std::endl;

    os << "SR: " << registers.getSRExplanation() << std::endl;
    os << "Cause: " << registers.getCauseExplanation() << std::endl;

    return os;
}

const char* CP0::CP0_REGISTER_NAMES[] = {
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

const CP0::Opcode CP0::cp0[] = {
    // 0b000000
    &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,
    // 0b000100
    &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,
    // 0b001000
    &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,
    // 0b001100
    &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,
    // 0b010000
    &CP0::RFE,      &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,
    // 0b010100
    &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,
    // 0b011000
    &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,
    // 0b011100
    &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,
    // 0b100000
    &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,
    // 0b100100
    &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,
    // 0b101000
    &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,
    // 0b101100
    &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,
    // 0b110000
    &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,
    // 0b110100
    &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,
    // 0b111000
    &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,
    // 0b111100
    &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0,   &CP0::UNKCP0
};


std::string CP0::getCP0RegisterName(uint8_t reg) {
    return CP0_REGISTER_NAMES[reg];
}

CP0::CP0() {
    reset();
}

void CP0::reset() {
    for (int i = 0; i < 32; ++i) {
        this->cp0Registers[i] = 0;
    }
    cp0Registers[15] = 0x00000001;
    instruction = 0;
    funct = 0;
}

uint32_t CP0::getCP0Register(uint8_t rt) {
    assert (rt < 32);

    uint32_t word = cp0Registers[rt];
    LOGT_CPU(std::format("{{{:s} -> 0x{:08X}}}", getCP0RegisterName(rt), word));

    return word;
}

void CP0::setCP0Register(uint8_t rt, uint32_t value) {
    assert (rt < 32);
    LOGT_CPU(std::format("{{0x{:08X} -> {:s}}}", value, getCP0RegisterName(rt)));
    cp0Registers[rt] = value;
}

bool CP0::statusRegisterIsolateCacheIsSet() const {
    return cp0Registers[CP0_REGISTER_SR] & (1 << 16);
}

bool CP0::getBit(uint8_t reg, uint8_t bit) const {
    return cp0Registers[reg] & (1 << bit);
}

void CP0::setBit(uint8_t reg, uint8_t bit, bool value) {
    uint32_t selectedBit = 1 << bit;
    cp0Registers[reg] = (cp0Registers[reg] & ~selectedBit) | ((value ? 1 : 0) << bit);
}

void CP0::setBit(uint8_t reg, uint8_t bit) {
    cp0Registers[reg] = cp0Registers[reg] | (1 << bit);
}

void CP0::clearBit(uint8_t reg, uint8_t bit) {
    uint32_t selectedBit = 1 << bit;
    cp0Registers[reg] = cp0Registers[reg] & ~selectedBit;
}

void CP0::execute(uint32_t instruction) {
    // Operation depends on function field
    this->instruction = instruction;
    funct = 0x3F & instruction;

    (this->*cp0[funct])();
}

void CP0::UNKCP0() {
    throw exceptions::UnknownFunctionError(std::format("Unknown CP0 opcode instruction 0x{:x} (CP0), function 0b{:06b}", instruction, funct));
}

void CP0::RFE() {
    // Restore From Exception
    // T: SR <- SR_{31...4} || SR_{5...2}

    LOGT_CP0("RFE");

    uint32_t sr = getCP0Register(CP0_REGISTER_SR);
    setCP0Register(CP0_REGISTER_SR, (sr & 0xFFFFFFF0) | ((sr & 0x3C) >> 2));
}

}

