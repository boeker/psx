#include "core.h"

#include <cassert>
#include <format>
#include <iostream>

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
    &Core::UNK, &Core::ORI, &Core::UNK, &Core::LUI,
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
    &Core::UNK, &Core::UNK, &Core::UNK, &Core::SW,
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
    // GPR[rt] <- immediate || 0^{16}
    uint8_t rt = 0b11111 & (instruction >> 16);
    uint32_t immediate = 0xFF & instruction;
    std::cerr << "LUI " << (uint32_t)rt << "," << immediate << std::endl;

    cpu.setRegister(rt, immediate << 16);
}

void Core::ORI() {
    // Or Immediate
    // T: GPR[rt] <- GPR[rs]_{31...36} || (immedate or GPR[rs]_{15...0})
    uint8_t rs = 0b11111 & (instruction >> 21);
    uint8_t rt = 0b11111 & (instruction >> 16);
    uint32_t immediate = 0xFF & instruction;
    std::cerr << "ORI " << (uint32_t)rt << "," << (uint32_t)rs << "," << immediate << std::endl;

    cpu.setRegister(rt, cpu.getRegister(rs) | immediate);
}

void Core::SW() {
    // Store Word
    // T: vAddr <- ((offset_{15})^{16} | offset_{15...0}) + GPR[base]
    // (pAddr, uncached) <- AddressTranslation(vAddr, DATA)
    // data <- GPR[rt]
    // StoreMemory(uncached, WORD, data, pAddr, vAddr, DATA)
    uint8_t base = 0b11111 & (instruction >> 21);
    uint8_t rt = 0b11111 & (instruction >> 16);
    uint32_t offset = 0xFF & instruction;
    std::cerr << "SW " << (uint32_t)rt << "," << offset << "(" << (uint32_t)base << ")" << std::endl;

    uint32_t vAddr = (((offset >> 15) ? 0xFF00 : 0x0000) | offset) + cpu.getRegister(base);
    uint32_t data = cpu.getRegister(rt);
    memory.writeWord(vAddr, data);
    // TODO Address Error Exception if the two least-significat bits of effective address are non-zero
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
