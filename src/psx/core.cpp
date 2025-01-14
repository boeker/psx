#include "core.h"

#include <cassert>
#include <format>
#include <iostream>

#include "exceptions/core.h"

namespace PSX {

void Core::log(const std::string &message) {
    std::cerr << message;
}

const Core::Opcode Core::opcodes[] = {
    // 0b000000
    &Core::SPECIAL,  &Core::UNK,      &Core::J,        &Core::UNK,
    // 0b000100
    &Core::UNK,      &Core::BNE,      &Core::UNK,      &Core::UNK,
    // 0b001000
    &Core::ADDI,     &Core::ADDIU,    &Core::UNK,      &Core::UNK,
    // 0b001100
    &Core::UNK,      &Core::ORI,      &Core::UNK,      &Core::LUI,
    // 0b010000
    &Core::CP0,      &Core::UNK,      &Core::UNK,      &Core::UNK,
    // 0b010100
    &Core::UNK,      &Core::UNK,      &Core::UNK,      &Core::UNK,
    // 0b011000
    &Core::UNK,      &Core::UNK,      &Core::UNK,      &Core::UNK,
    // 0b011100
    &Core::UNK,      &Core::UNK,      &Core::UNK,      &Core::UNK,
    // 0b100000
    &Core::UNK,      &Core::UNK,      &Core::UNK,      &Core::LW,
    // 0b100100
    &Core::UNK,      &Core::UNK,      &Core::UNK,      &Core::UNK,
    // 0b101000
    &Core::UNK,      &Core::SH,       &Core::UNK,      &Core::SW,
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

const Core::Opcode Core::special[] = {
    // 0b000000
    &Core::SLL,      &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,
    // 0b000100
    &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,
    // 0b001000
    &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,
    // 0b001100
    &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,
    // 0b010000
    &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,
    // 0b010100
    &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,
    // 0b011000
    &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,
    // 0b011100
    &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,
    // 0b100000
    &Core::UNKSPCL,  &Core::ADDU,     &Core::UNKSPCL,  &Core::UNKSPCL,
    // 0b100100
    &Core::UNKSPCL,  &Core::OR,       &Core::UNKSPCL,  &Core::UNKSPCL,
    // 0b101000
    &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::SLTU,
    // 0b101100
    &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,
    // 0b110000
    &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,
    // 0b110100
    &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,
    // 0b111000
    &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,
    // 0b111100
    &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL
};

const Core::Opcode Core::cp0[] = {
    // 0b000000
    &Core::CP0MOVE,  &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,
    // 0b000100
    &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,
    // 0b001000
    &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,
    // 0b001100
    &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,
    // 0b010000
    &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,
    // 0b010100
    &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,
    // 0b011000
    &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,
    // 0b011100
    &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,
    // 0b100000
    &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,
    // 0b100100
    &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,
    // 0b101000
    &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,
    // 0b101100
    &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,
    // 0b110000
    &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,
    // 0b110100
    &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,
    // 0b111000
    &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,
    // 0b111100
    &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0,   &Core::UNKCP0
};

const Core::Opcode Core::cp0Move[] = {
    // 0b00000
    &Core::UNKCP0M,  &Core::UNKCP0M,  &Core::UNKCP0M,  &Core::UNKCP0M,
    // 0b00100
    &Core::MTC0,     &Core::UNKCP0M,  &Core::UNKCP0M,  &Core::UNKCP0M,
    // 0b01000
    &Core::UNKCP0M,  &Core::UNKCP0M,  &Core::UNKCP0M,  &Core::UNKCP0M,
    // 0b01100
    &Core::UNKCP0M,  &Core::UNKCP0M,  &Core::UNKCP0M,  &Core::UNKCP0M,
    // 0b10000
    &Core::UNKCP0M,  &Core::UNKCP0M,  &Core::UNKCP0M,  &Core::UNKCP0M,
    // 0b10100
    &Core::UNKCP0M,  &Core::UNKCP0M,  &Core::UNKCP0M,  &Core::UNKCP0M,
    // 0b11000
    &Core::UNKCP0M,  &Core::UNKCP0M,  &Core::UNKCP0M,  &Core::UNKCP0M,
    // 0b11100
    &Core::UNKCP0M,  &Core::UNKCP0M,  &Core::UNKCP0M,  &Core::UNKCP0M
};

void Core::UNK() {
    throw exceptions::UnknownOpcodeError(std::format("instruction 0x{:x}: 0b{:06b}", instructionPC, opcode));
}

void Core::SPECIAL() {
    // SPECIAL
    // Operation depends on function field
    funct = 0x3F & instruction;
    
    (this->*special[funct])();
}

void Core::CP0() {
    // CP0
    // Operation depends on function field
    funct = 0x3F & instruction;
    
    (this->*cp0[funct])();
}

void Core::LUI() {
    // Load Upper Immediate
    // GPR[rt] <- immediate || 0^{16}
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t immediate = 0xFFFF & instruction;

    uint32_t data = immediate << 16;
    log(std::format("LUI {:d},0x{:x} (0x{:08x} -> {:d})", rt, immediate, data, rt));

    memory.registers.setRegister(rt, data);
}

void Core::ORI() {
    // Or Immediate
    // T: GPR[rt] <- GPR[rs]_{31...36} || (immedate or GPR[rs]_{15...0})
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t immediate = 0xFFFF & instruction;
    log(std::format("ORI {:d},{:d},0x{:x}", rt, rs, immediate));

    memory.registers.setRegister(rt, memory.registers.getRegister(rs) | immediate);
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

    uint32_t vAddr = (((offset >> 15) ? 0xFFFF0000 : 0x0000) | offset) + memory.registers.getRegister(base);
    uint32_t data = memory.registers.getRegister(rt);
    log(std::format("SW {:s},0x{:x}({:s}) (0x{:x} -> 0x{:x})",
                    memory.registers.getRegisterName(rt),
                    offset,
                    memory.registers.getRegisterName(base),
                    data,
                    vAddr));
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
    uint32_t signExtension = ((immediate >> 15) ? 0xFFFF0000 : 0x00000000) + immediate;

    log(std::format("ADDIU {:s},{:s},0x{:04X}", memory.registers.getRegisterName(rt), memory.registers.getRegisterName(rs), immediate));

    memory.registers.setRegister(rt, memory.registers.getRegister(rs) + signExtension);
}

void Core::J() {
    // Jump
    // T: temp <- target
    // T+1: pc <- pc_{31...28} || temp || 0^2
    uint32_t target = 0x3FFFFFF & instruction;

    uint32_t actualTarget = (memory.registers.getPC() & 0xF0000000) | (target << 2);
    log(std::format("J 0x{:x} (-> 0x{:x})", target, actualTarget));
    memory.registers.setPC(actualTarget);
}

void Core::BNE() {
    // Branch On Not Equal
    // T: target <- (offset_{15})^{14} || offset || 0^2
    //    condition <- (GPR[rs] != GPR[rt])
    // T+1: if condition then
    //      PC <- PC + target
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    uint32_t target = ((offset >> 15) ? 0xFFFC0000 : 0x00000000) | (offset << 2);
    uint32_t actualTarget = memory.registers.getPC() + target;
    log(std::format("BNE {:d},{:d},0x{:04x} (+0x{:08x}, -> 0x{:08x})", rt, rs, offset, target, actualTarget));
    if (memory.registers.getRegister(rs) != memory.registers.getRegister(rt)) {
        memory.registers.setPC(actualTarget);
    }
}

void Core::ADDI() {
    // Add Immediate Word
    // T: GPR[rt] <- GPR[rs] + (immediate_{15})^{16} || immediate_{15...0}
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t immediate = 0xFFFF & instruction;
    log(std::format("ADDI {:d},{:d},0x{:04X}", rt, rs, immediate));

    uint32_t rsValue = memory.registers.getRegister(rs);
    uint32_t signExtension = ((immediate >> 15) ? 0xFFFF0000 : 0x00000000) + immediate;
    bool carry30 = ((0x7FFFFFFF & rsValue) + (0x7FFFFFFF & signExtension)) & 0x80000000;
    bool carry31 = ((rsValue >> 31) + (signExtension >> 31) + (carry30 ? 1 : 0) >= 2);

    if (carry30 == carry31) {
        memory.registers.setRegister(rt, rsValue + signExtension);

    } else {
        // TODO integer overflow exception
        log(" !!! TODO: integer overflow exception occurred !!!\n");
        std::exit(1);
    }
}

void Core::LW() {
    // Load Word
    // T: vAddr <- ((offset_{15})^{16} | offset_{15...0}) + GPR[base]
    // (pAddr, uncached) <- AddressTranslation(vAddr, DATA)
    // mem <- LoadMemory(uncached, WORD, pAddr, vAddr, DATA)
    // T+1: GPR[rt] <- mem
    uint8_t base = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    uint32_t vAddr = (((offset >> 15) ? 0xFFFF0000 : 0x00000000) | offset) + memory.registers.getRegister(base);
    uint32_t data = memory.readWord(vAddr);
    log(std::format("LW {:d},0x{:04x}({:d}) (0x{:08x} -> {:d})", rt, offset, base, data, rt));
    memory.registers.setRegister(rt, data);
    // TODO Address Error Exception
}

void Core::SH() {
    // Store Halfword
    // T: vAddr <- ((offset_{15})^{16} | offset_{15...0}) + GPR[base]
    // (pAddr, uncached) <- AddressTranslation(vAddr, DATA)
    // pAddr <- pAddr_{PSIZE-1...2} || (pAddr_{1...0} xor (ReverseEndian || 0))
    // byte <- vAddr_{1...0} xor (BigEndianCPU || 0)
    // data <- GPR[rt]_{31-8*byte...0} || 0^{8*byte}
    // StoreMemory(uncached, HALFWORD, data, pAddr, vAddr, DATA)
    uint8_t base = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    uint32_t vAddr = (((offset >> 15) ? 0xFFFF0000 : 0x0000) | offset) + memory.registers.getRegister(base);
    uint16_t data = (uint16_t)(0x0000FFFF & memory.registers.getRegister(rt));
    log(std::format("SW {:d},0x{:x}({:d}) (0x{:04x} -> 0x{:08x})", rt, offset, base, data, vAddr));
    memory.writeHalfWord(vAddr, data);
    // TODO Address Error Exception if the least-significat bit of effective address is non-zero
}

void Core::UNKSPCL() {
    throw exceptions::UnknownFunctionError(std::format("0x{:x}: instruction 0x{:x} (SPECIAL), function 0b{:06b}", instructionPC, instruction, funct));
}

void Core::SLL() {
    // Shift Word Left Logical
    // T: GPR[rd] <- GPR[rt]_{31 - sa...0} || 0^{sa}
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);
    uint8_t sa = 0x1F & (instruction >> 6);
    log(std::format("SLL {:d},{:d},{:d}", rd, rt, sa));

    memory.registers.setRegister(rd, memory.registers.getRegister(rt) << sa);
}

void Core::OR() {
    // Or
    // T: GPR[rd] <- GPR[rs] or GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);
    log(std::format("OR {:d},{:d},{:d}", rd, rs, rt));

    memory.registers.setRegister(rd, memory.registers.getRegister(rs) | memory.registers.getRegister(rt));
}

void Core::SLTU() {
    // Set on Less Than Unsigned
    // T: if (0 || GPR[rs]) < 0 || GPR[rt] then
    //     GPR[rd] <- 0^{31} || 1
    // else
    //     GPR[rd] <- 0^{32}
    // endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    uint32_t rsValue = memory.registers.getRegister(rs);
    uint32_t rtValue = memory.registers.getRegister(rt);
    log(std::format("SLTU {:d},{:d},{:d} (0x{:08x} < 0x{:08x}?)", rd, rs, rt, rsValue, rtValue));

    if (rsValue < rtValue) {
        memory.registers.setRegister(rd, 1);

    } else {
        memory.registers.setRegister(rd, 0);
    }
}

void Core::ADDU() {
    // Add Unsigned Word
    // T: GPR[rd] <- GPR[rs] + GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    uint32_t rsValue = memory.registers.getRegister(rs);
    uint32_t rtValue = memory.registers.getRegister(rt);
    log(std::format("ADDU {:d},{:d},{:d}", rd, rs, rt));

    memory.registers.setRegister(rd, rsValue + rtValue);
}

void Core::UNKCP0() {
    throw exceptions::UnknownFunctionError(std::format("0x{:x}: instruction 0x{:x} (CP0), function 0b{:06b}", instructionPC, instruction, funct));
}

void Core::CP0MOVE() {
    // CP0 Move
    // Operation depends on function field
    move = 0x1F & (instruction >> 21);
    
    (this->*cp0Move[move])();
}

void Core::UNKCP0M() {
    throw exceptions::UnknownOpcodeError(std::format("0x{:x}: instruction 0x{:x} (CP0Move), move 0b{:05b}", instructionPC, instruction, move));
}

void Core::MTC0() {
    // Move To Coprocessor 0
    // T: data <- GPR[rt]
    // T+1: CPR[0,rd] <- data
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    uint32_t data = memory.registers.getRegister(rt);
    log(std::format("MTC0 {:d},{:d} (0x{:x} -> CP0 {:d})", rt, rd, data, rd));
    memory.registers.setCP0Register(rd, data);
}

Core::Core() {
    reset();
}

void Core::reset() {
    this->memory.registers.reset();
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
    nextInstructionPC = memory.registers.getPC();
    nextInstruction = memory.readWord(nextInstructionPC);

    // increase program counter
    memory.registers.setPC(nextInstructionPC + 4);
    
    // execute instruction
    log(std::format("0x{:08x}: ", instructionPC));
    opcode = instruction >> 26;
    assert (opcode <= 0b111111);
    (this->*opcodes[opcode])();
    log("\n");
}
}
