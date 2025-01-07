#include "core.h"

#include <cassert>
#include <format>

#include "exceptions/core.h"

namespace PSX {

const Core::Opcode Core::opcodes[] = {
    // 0b000000
    &Core::UNK, &Core::UNK, &Core::UNK, &Core::UNK,
    // 0b000100
    &Core::UNK, &Core::UNK, &Core::UNK, &Core::UNK,
    // 0b001000
    &Core::UNK, &Core::UNK, &Core::UNK, &Core::UNK,
    // 0b001100
    &Core::UNK, &Core::UNK, &Core::UNK, &Core::LUI,
    // 0b010000
    &Core::UNK, &Core::UNK, &Core::UNK, &Core::UNK,
    // 0b010100
    &Core::UNK, &Core::UNK, &Core::UNK, &Core::UNK,
    // 0b011000
    &Core::UNK, &Core::UNK, &Core::UNK, &Core::UNK,
    // 0b011100
    &Core::UNK, &Core::UNK, &Core::UNK, &Core::UNK,
    // 0b100000
    &Core::UNK, &Core::UNK, &Core::UNK, &Core::UNK,
    // 0b100100
    &Core::UNK, &Core::UNK, &Core::UNK, &Core::UNK,
    // 0b101000
    &Core::UNK, &Core::UNK, &Core::UNK, &Core::UNK,
    // 0b101100
    &Core::UNK, &Core::UNK, &Core::UNK, &Core::UNK,
    // 0b110000
    &Core::UNK, &Core::UNK, &Core::UNK, &Core::UNK,
    // 0b110100
    &Core::UNK, &Core::UNK, &Core::UNK, &Core::UNK,
    // 0b111000
    &Core::UNK, &Core::UNK, &Core::UNK, &Core::UNK,
    // 0b111100
    &Core::UNK, &Core::UNK, &Core::UNK, &Core::UNK
};

void Core::UNK() {
    throw exceptions::UnknownOpcodeError(std::format("{:06b}", opcode));
}
void Core::LUI() {
    // Load Upper Immediate
    // GPR[rt] <- immediate || 0^(16)
    uint8_t rt = 0b11111 & (instruction >> 16);
    uint32_t immediate = 0xFF & instruction;
    cpu.setRegister(rt, immediate << 16);
}

void Core::readBIOS(const std::string &file) {
    memory.readBIOS(file);
}

void Core::step() {
    // load instruction from memory at program counter
    instruction = memory.readWord(cpu.getPC());
    opcode = instruction >> 26;

    //increase program counter
    cpu.setPC(cpu.getPC() + 4);
    
    // execute instruction
    assert (opcode <= 0b111111);
    (this->*opcodes[opcode])();
}
}
