#include "core.h"

#include <cassert>
#include <format>
#include <iostream>

#include "exceptions/core.h"

namespace PSX {

const Core::Opcode Core::opcodes[] = {
    // 0b000000
    &Core::SPECIAL,  &Core::UNK,      &Core::J,        &Core::UNK,
    // 0b000100
    &Core::UNK,      &Core::UNK,      &Core::UNK,      &Core::UNK,
    // 0b001000
    &Core::UNK,      &Core::ADDIU,    &Core::UNK,      &Core::UNK,
    // 0b001100
    &Core::UNK,      &Core::ORI,      &Core::UNK,      &Core::LUI,
    // 0b010000
    &Core::UNK,      &Core::UNK,      &Core::UNK,      &Core::UNK,
    // 0b010100
    &Core::UNK,      &Core::UNK,      &Core::UNK,      &Core::UNK,
    // 0b011000
    &Core::UNK,      &Core::UNK,      &Core::UNK,      &Core::UNK,
    // 0b011100
    &Core::UNK,      &Core::UNK,      &Core::UNK,      &Core::UNK,
    // 0b100000
    &Core::UNK,      &Core::UNK,      &Core::UNK,      &Core::UNK,
    // 0b100100
    &Core::UNK,      &Core::UNK,      &Core::UNK,      &Core::UNK,
    // 0b101000
    &Core::UNK,      &Core::UNK,      &Core::UNK,      &Core::SW,
    // 0b101100
    &Core::UNK,      &Core::UNK,      &Core::UNK,      &Core::UNK,
    // 0b110000
    &Core::UNK,      &Core::UNK,      &Core::UNK,      &Core::UNK,
    // 0b110100
    &Core::UNK,      &Core::UNK,      &Core::UNK,      &Core::UNK,
    // 0b111000
    &Core::UNK,      &Core::UNK,      &Core::UNK,      &Core::UNK,
    // 0b111100
    &Core::UNK,      &Core::UNK,      &Core::UNK,      &Core::UNK
};

const Core::Opcode Core::functions[] = {
    // 0b000000
    &Core::SLL,      &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT,
    // 0b000100
    &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT,
    // 0b001000
    &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT,
    // 0b001100
    &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT,
    // 0b010000
    &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT,
    // 0b010100
    &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT,
    // 0b011000
    &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT,
    // 0b011100
    &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT,
    // 0b100000
    &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT,
    // 0b100100
    &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT,
    // 0b101000
    &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT,
    // 0b101100
    &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT,
    // 0b110000
    &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT,
    // 0b110100
    &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT,
    // 0b111000
    &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT,
    // 0b111100
    &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT, &Core::UNKFUNCT
};

void Core::UNK() {
    throw exceptions::UnknownOpcodeError(std::format("{:06b}", opcode));
}

void Core::SPECIAL() {
    // SPECIAL
    // Operation depends on function field
    funct = 0x1F & instruction;
    
    (this->*functions[funct])();
}

void Core::LUI() {
    // Load Upper Immediate
    // GPR[rt] <- immediate || 0^{16}
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t immediate = 0xFFFF & instruction;
    std::cerr << std::format("0x{:x}", instructionPC) << ": LUI " << (uint32_t)rt << "," << std::format("0x{:x}", immediate) << std::endl;

    cpu.setRegister(rt, immediate << 16);
}

void Core::ORI() {
    // Or Immediate
    // T: GPR[rt] <- GPR[rs]_{31...36} || (immedate or GPR[rs]_{15...0})
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t immediate = 0xFFFF & instruction;
    std::cerr << std::format("0x{:x}", instructionPC) << ": ORI " << (uint32_t)rt << "," << (uint32_t)rs << "," << std::format("0x{:x}", immediate) << std::endl;

    cpu.setRegister(rt, cpu.getRegister(rs) | immediate);
}

void Core::SW() {
    // Store Word
    // T: vAddr <- ((offset_{15})^{16} | offset_{15...0}) + GPR[base]
    // (pAddr, uncached) <- AddressTranslation(vAddr, DATA)
    // data <- GPR[rt]
    // StoreMemory(uncached, WORD, data, pAddr, vAddr, DATA)
    uint8_t base = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;
    std::cerr << std::format("0x{:x}", instructionPC) << ": SW " << (uint32_t)rt << "," << std::format("0x{:x}", offset) << "(" << (uint32_t)base << ")" << std::endl;

    uint32_t vAddr = (((offset >> 15) ? 0xFFFF0000 : 0x0000) | offset) + cpu.getRegister(base);
    uint32_t data = cpu.getRegister(rt);
    memory.writeWord(vAddr, data);
    // TODO Address Error Exception if the two least-significat bits of effective address are non-zero
}

void Core::ADDIU() {
    // Add Immediate Unsigned Word
    // T: temp <- GPR[rs] + (immediate_{15})^{48} || immediate_{15...0}
    // if 32-bit-overflow(temp) then
    //     GPR[rt] <- (temp_{31})^{32} || temp_{31...0}
    // else
    //     GPR[rt] <- temp
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t immediate = 0xFFFF & instruction;

    cpu.setRegister(rt, cpu.getRegister(rs) + immediate);
}

void Core::J() {
    // Jump
    // T: temp <- target
    // T+1: pc <- pc_{31...28} || temp || 0^2
    uint32_t target = 0x3FFFFFF & instruction;
    std::cerr << std::format("0x{:x}", instructionPC) << ": J " << std::format("0x{:x}", target) << std::endl;

    cpu.setPC((cpu.getPC() & 0xF0000000) | (target << 2));
}

void Core::UNKFUNCT() {
    throw exceptions::UnknownFunctionError(std::format("{:06b}", funct));
}

void Core::SLL() {
    // Shift Word Left Logical
    // T: GPR[rd] <- GPR[rt]_{31 - sa...0} || 0^{sa}
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);
    uint8_t sa = 0x1F & (instruction >> 6);
    std::cerr << std::format("0x{:x}", instructionPC) << ": SLL " << (uint32_t)rd << "," << (uint32_t)rt << "," << (uint32_t)sa << std::endl;

    cpu.setRegister(rd, cpu.getRegister(rt) << sa);
}

Core::Core() {
    reset();
}

void Core::reset() {
    this->cpu.reset();
    this->memory.reset();

    instructionPC = 0;
    instruction = 0;

    nextInstructionPC = 0;
    nextInstruction = 0;
}

void Core::readBIOS(const std::string &file) {
    memory.readBIOS(file);
}

void Core::step() {
    instructionPC = nextInstructionPC;
    instruction = nextInstruction;

    // load instruction from memory at program counter
    nextInstructionPC = cpu.getPC();
    nextInstruction = memory.readWord(nextInstructionPC);

    // increase program counter
    cpu.setPC(nextInstructionPC + 4);
    
    // execute instruction
    opcode = instruction >> 26;
    assert (opcode <= 0b111111);
    (this->*opcodes[opcode])();

}
}
