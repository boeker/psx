#include "core.h"

#include <cassert>
#include <format>
#include <iostream>

#include "exceptions/exceptions.h"
#include "util/log.h"

using namespace util;

namespace PSX {

std::ostream& operator<<(std::ostream &os, const Core &core) {
    os << "Register contents: " << std::endl;
    os << core.memory.regs;
    return os;
}

const Core::Opcode Core::opcodes[] = {
    // 0b000000
    &Core::SPECIAL,  &Core::REGIMM,   &Core::J,        &Core::JAL,
    // 0b000100
    &Core::BEQ,      &Core::BNE,      &Core::BLEZ,     &Core::BGTZ,
    // 0b001000
    &Core::ADDI,     &Core::ADDIU,    &Core::SLTI,     &Core::SLTIU,
    // 0b001100
    &Core::ANDI,     &Core::ORI,      &Core::UNK,      &Core::LUI,
    // 0b010000
    &Core::CP0,      &Core::UNK,      &Core::UNK,      &Core::UNK,
    // 0b010100
    &Core::UNK,      &Core::UNK,      &Core::UNK,      &Core::UNK,
    // 0b011000
    &Core::UNK,      &Core::UNK,      &Core::UNK,      &Core::UNK,
    // 0b011100
    &Core::UNK,      &Core::UNK,      &Core::UNK,      &Core::UNK,
    // 0b100000
    &Core::LB,       &Core::UNK,      &Core::UNK,      &Core::LW,
    // 0b100100
    &Core::LBU,      &Core::UNK,      &Core::UNK,      &Core::UNK,
    // 0b101000
    &Core::SB,       &Core::SH,       &Core::UNK,      &Core::SW,
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
    &Core::SLL,      &Core::UNKSPCL,  &Core::SRL,      &Core::SRA,
    // 0b000100
    &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,
    // 0b001000
    &Core::JR,       &Core::JALR,     &Core::UNKSPCL,  &Core::UNKSPCL,
    // 0b001100
    &Core::SYSCALL,  &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,
    // 0b010000
    &Core::MFHI,     &Core::UNKSPCL,  &Core::MFLO,     &Core::UNKSPCL,
    // 0b010100
    &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,
    // 0b011000
    &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::DIV,      &Core::DIVU,
    // 0b011100
    &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::UNKSPCL,
    // 0b100000
    &Core::ADD,      &Core::ADDU,     &Core::UNKSPCL,  &Core::SUBU,
    // 0b100100
    &Core::AND,      &Core::OR,       &Core::UNKSPCL,  &Core::UNKSPCL,
    // 0b101000
    &Core::UNKSPCL,  &Core::UNKSPCL,  &Core::SLT,      &Core::SLTU,
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
    &Core::MFC0,     &Core::UNKCP0M,  &Core::UNKCP0M,  &Core::UNKCP0M,
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

const Core::Opcode Core::regimm[] = {
    // 0b00000
    &Core::BLTZ,     &Core::BGEZ,     &Core::UNKRGMM,  &Core::UNKRGMM,
    // 0b00100
    &Core::UNKRGMM,  &Core::UNKRGMM,  &Core::UNKRGMM,  &Core::UNKRGMM,
    // 0b01000
    &Core::UNKRGMM,  &Core::UNKRGMM,  &Core::UNKRGMM,  &Core::UNKRGMM,
    // 0b01100
    &Core::UNKRGMM,  &Core::UNKRGMM,  &Core::UNKRGMM,  &Core::UNKRGMM,
    // 0b10000
    &Core::UNKRGMM,  &Core::UNKRGMM,  &Core::UNKRGMM,  &Core::UNKRGMM,
    // 0b10100
    &Core::UNKRGMM,  &Core::UNKRGMM,  &Core::UNKRGMM,  &Core::UNKRGMM,
    // 0b11000
    &Core::UNKRGMM,  &Core::UNKRGMM,  &Core::UNKRGMM,  &Core::UNKRGMM,
    // 0b11100
    &Core::UNKRGMM,  &Core::UNKRGMM,  &Core::UNKRGMM,  &Core::UNKRGMM
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

void Core::REGIMM() {
    // REGIMM
    // Operation depends on rt field
    instructionRt = 0x1F & (instruction >> 16);

    (this->*regimm[instructionRt])();
}

void Core::LUI() {
    // Load Upper Immediate
    // GPR[rt] <- immediate || 0^{16}
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t immediate = 0xFFFF & instruction;

    Log::log(std::format("LUI {:s},0x{:04X}",
                          memory.regs.getRegisterName(rt),
                          immediate));

    uint32_t data = immediate << 16;

    memory.regs.setRegister(rt, data);
}

void Core::ORI() {
    // Or Immediate
    // T: GPR[rt] <- GPR[rs]_{31...36} || (immedate or GPR[rs]_{15...0})
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t immediate = 0xFFFF & instruction;
    Log::log(std::format("ORI {:s},{:s},0x{:04x}",
                         memory.regs.getRegisterName(rt),
                         memory.regs.getRegisterName(rs),
                         immediate));

    memory.regs.setRegister(rt, memory.regs.getRegister(rs) | immediate);
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

    Log::log(std::format("SW {:s},0x{:04X}({:s})",
                    memory.regs.getRegisterName(rt),
                    offset,
                    memory.regs.getRegisterName(base)));

    uint32_t vAddr = (((offset >> 15) ? 0xFFFF0000 : 0x00000000) | offset)
                     + memory.regs.getRegister(base);
    uint32_t data = memory.regs.getRegister(rt);
    memory.writeWord(vAddr, data);
    if (vAddr & 0x3) {
        throw exceptions::ExceptionNotImplemented("Address Error");
    }
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

    Log::log(std::format("ADDIU {:s},{:s},0x{:04X}",
                         memory.regs.getRegisterName(rt),
                         memory.regs.getRegisterName(rs),
                         immediate));

    memory.regs.setRegister(rt, memory.regs.getRegister(rs) + signExtension);
}

void Core::J() {
    // Jump
    // T: temp <- target
    // T+1: pc <- pc_{31...28} || temp || 0^2
    uint32_t target = 0x3FFFFFF & instruction;
    Log::log(std::format("J 0x{:08X}", target));

    uint32_t actualTarget = (delaySlotPC & 0xF0000000) | (target << 2);

    memory.regs.setPC(actualTarget);
    delaySlotIsBranchDelaySlot = true;
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
    uint32_t actualTarget = delaySlotPC + target;
    Log::log(std::format("BNE {:s},{:s},0x{:04X} (+0x{:08X}, -> @0x{:08X})",
                         memory.regs.getRegisterName(rt),
                         memory.regs.getRegisterName(rs),
                         offset,
                         target,
                         actualTarget));
    if (memory.regs.getRegister(rs) != memory.regs.getRegister(rt)) {
        memory.regs.setPC(actualTarget);
        delaySlotIsBranchDelaySlot = true;
    }
}

void Core::ADDI() {
    // Add Immediate Word
    // T: GPR[rt] <- GPR[rs] + (immediate_{15})^{16} || immediate_{15...0}
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t immediate = 0xFFFF & instruction;
    Log::log(std::format("ADDI {:s},{:s},0x{:04X}",
                         memory.regs.getRegisterName(rt),
                         memory.regs.getRegisterName(rs),
                         immediate));

    uint32_t rsValue = memory.regs.getRegister(rs);
    uint32_t signExtension = ((immediate >> 15) ? 0xFFFF0000 : 0x00000000) + immediate;
    bool carry30 = ((0x7FFFFFFF & rsValue) + (0x7FFFFFFF & signExtension)) & 0x80000000;
    bool carry31 = ((rsValue >> 31) + (signExtension >> 31) + (carry30 ? 1 : 0) >= 2);

    if (carry30 == carry31) {
        memory.regs.setRegister(rt, rsValue + signExtension);

    } else {
        throw exceptions::ExceptionNotImplemented("Integer Overflow");
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

    Log::log(std::format("LW {:s},0x{:04X}({:s})",
                         memory.regs.getRegisterName(rt),
                         offset,
                         memory.regs.getRegisterName(base)));

    uint32_t vAddr = (((offset >> 15) ? 0xFFFF0000 : 0x00000000) | offset) + memory.regs.getRegister(base);
    uint32_t data = memory.readWord(vAddr);
    memory.regs.setRegister(rt, data);

if (vAddr & 0x3) {
    throw exceptions::ExceptionNotImplemented("Address Error");
    }
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

    Log::log(std::format("SH {:s},0x{:08X}({:s})",
                         memory.regs.getRegisterName(rt),
                         offset,
                         memory.regs.getRegisterName(base)));

    uint32_t vAddr = (((offset >> 15) ? 0xFFFF0000 : 0x0000) | offset) + memory.regs.getRegister(base);
    uint16_t data = (uint16_t)(0x0000FFFF & memory.regs.getRegister(rt));
    memory.writeHalfWord(vAddr, data);

    if (vAddr & 0x1) {
        throw exceptions::ExceptionNotImplemented("Address Error");
    }
}

void Core::JAL() {
    // Jump And Link
    // T: temp <- target
    //    GPR[31] <- PC + 8
    // T+1: PC <- PC_{31...28} || temp || 0^2
    uint32_t target = 0x03FFFFFF & instruction;

    Log::log(std::format("JAL 0x{:06X}", target));

    uint32_t actualTarget = (delaySlotPC &  0xF0000000) | (target << 2);
    uint32_t newPC = instructionPC + 8;

    memory.regs.setRegister(31, newPC);
    memory.regs.setPC(actualTarget);
    delaySlotIsBranchDelaySlot = true;
}

void Core::ANDI() {
    // And Immediate
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t immediate = 0xFFFF & instruction;

    Log::log(std::format("ANDI {:s},{:s},0x{:04X}",
                         memory.regs.getRegisterName(rt),
                         memory.regs.getRegisterName(rs),
                         immediate));
    memory.regs.setRegister(rt, immediate & memory.regs.getRegister(rs));
}

void Core::SB() {
    // Store Byte
    // T: vAddr <- ((offset_{15})^{16} | offset_{15...0}) + GPR[base]
    // (pAddr, uncached) <- AddressTranslation(vAddr, DATA)
    // pAddr <- pAddr_{PSIZE-1...2} || (pAddr_{1...0} xor ReverseEndian^2)
    // byte <- vAddr_{1...0} xor BigEndianCPU^2
    // data <- GPR[rt]_{31-8*byte...0} || 0^{8*byte}
    // StoreMemory(uncached, BYTE, data, pAddr, vAddr, DATA)
    uint8_t base = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    Log::log(std::format("SB {:s},0x{:04X}({:s})",
                         memory.regs.getRegisterName(rt),
                         offset,
                         memory.regs.getRegisterName(base)));

    uint32_t vAddr = (((offset >> 15) ? 0xFFFF0000 : 0x0000) | offset) + memory.regs.getRegister(base);
    uint8_t data = (uint8_t)(0x000000FF & memory.regs.getRegister(rt));
    memory.writeByte(vAddr, data);
}

void Core::LB() {
    // Load Byte
    // T: vAddr <- ((offset_{15})^{16} | offset_{15...0}) + GPR[base]
    // (pAddr, uncached) <- AddressTranslation(vAddr, DATA)
    // pAddr <- pAddr_{PSIZE-1...2} || (pAddr_{1...0} xor ReverseEndian^2)
    // mem <- LoadMemory(uncached, BYTE, pAddr, vAddr, DATA)
    // byte <- vAddr_{1...0} xor BigEndianCPU^2
    // T+1: GPR[rt] <- (mem_{7+8*byte)^{24} || mem_{7+8*byte...8*byte}
    uint8_t base = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    Log::log(std::format("LB {:s},0x{:04X}({:s})",
                         memory.regs.getRegisterName(rt),
                         offset,
                         memory.regs.getRegisterName(base)));

    uint32_t vAddr = (((offset >> 15) ? 0xFFFF0000 : 0x0000) | offset) + memory.regs.getRegister(base);
    uint8_t mem = memory.readByte(vAddr);
    uint32_t signExtension = ((mem >> 7) ? 0xFFFFFF00 : 0x00000000) + mem;
    memory.regs.setRegister(rt, signExtension);
}

void Core::BEQ() {
    // Branch On Equal
    // T: target <- (offset_{15})^{14} || offset || 0^2
    //    condition <- (GPR[rs] = GPR[rt])
    // T+1: if condition then
    //          PC <- PC + target
    //      endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    Log::log(std::format("BEQ {:s},{:s},{:04X}",
                         memory.regs.getRegisterName(rs),
                         memory.regs.getRegisterName(rt),
                         offset));

    uint32_t signExtension = ((offset >> 15) ? 0xFFFF0000 : 0x00000000) + offset;
    uint32_t target = signExtension << 2;
    uint32_t actualTarget = delaySlotPC + target;

    uint32_t rsValue = memory.regs.getRegister(rs);
    uint32_t rtValue = memory.regs.getRegister(rt);
    Log::log(std::format(" (0x{:08X} == 0x{:08X}? -0x{:08X}-> pc)",
                         rsValue, rtValue, actualTarget));

    if (rsValue == rtValue) {
        memory.regs.setPC(actualTarget);
        delaySlotIsBranchDelaySlot = true;
    }
}

void Core::BGTZ() {
    // Branch On Greater Than Zero
    // T: target <- (offset_{15})^{14} || offset || 0^2
    //    condition <- (GPR[rs]_{31} = 0 and GPR[rs] != 0^{32})
    // T+1: if condition then
    //          PC <- PC + target
    //      endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint32_t offset = 0xFFFF & instruction;

    Log::log(std::format("BGTZ {:s},{:04X}",
                         memory.regs.getRegisterName(rs),
                         offset));

    uint32_t signExtension = ((offset >> 15) ? 0xFFFF0000 : 0x00000000) + offset;
    uint32_t target = signExtension << 2;
    uint32_t actualTarget = delaySlotPC + target;

    uint32_t rsValue = memory.regs.getRegister(rs);
    Log::log(std::format(" (0x{:08X} > 0? -0x{:08X}-> pc)",
                         rsValue, actualTarget));

    if (!(rsValue >> 31) && (rsValue != 0)) {
        memory.regs.setPC(actualTarget);
        delaySlotIsBranchDelaySlot = true;
    }
}

void Core::BLEZ() {
    // Branch On Less Than Or Equal To Zero
    // T: target <- (offset_{15})^{14} || offset || 0^2
    //    condition <- (GPR[rs]_{31} = 1) or (GPR[rs] == 0^{32})
    // T+1: if condition then
    //          PC <- PC + target
    //      endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint32_t offset = 0xFFFF & instruction;

    Log::log(std::format("BLEZ {:s},{:04X}",
                         memory.regs.getRegisterName(rs),
                         offset));

    uint32_t signExtension = ((offset >> 15) ? 0xFFFF0000 : 0x00000000) + offset;
    uint32_t target = signExtension << 2;
    uint32_t actualTarget = delaySlotPC + target;

    uint32_t rsValue = memory.regs.getRegister(rs);
    Log::log(std::format(" (0x{:08X} <= 0? -0x{:08X}-> pc)",
                         rsValue, actualTarget));

    if ((rsValue >> 31) || (rsValue == 0)) {
        memory.regs.setPC(actualTarget);
        delaySlotIsBranchDelaySlot = true;
    }
}

void Core::LBU() {
    // Load Byte Unsigned
    // T: vAddr <- ((offset_{15})^{16} | offset_{15...0}) + GPR[base]
    // (pAddr, uncached) <- AddressTranslation(vAddr, DATA)
    // pAddr <- pAddr_{PSIZE-1...2} || (pAddr_{1...0} xor ReverseEndian^2)
    // mem <- LoadMemory(uncached, BYTE, pAddr, vAddr, DATA)
    // byte <- vAddr_{1...0} xor BigEndianCPU^2
    // T+1: GPR[rt] <- 0^{24} || mem_{7+8*byte...8*byte}
    uint8_t base = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    Log::log(std::format("LBU {:s},0x{:04X}({:s})",
                         memory.regs.getRegisterName(rt),
                         offset,
                         memory.regs.getRegisterName(base)));

    uint32_t vAddr = (((offset >> 15) ? 0xFFFF0000 : 0x0000) | offset) + memory.regs.getRegister(base);
    uint8_t mem = memory.readByte(vAddr);
    uint32_t zeroExtension = mem;
    memory.regs.setRegister(rt, zeroExtension);
}

void Core::SLTI() {
    // Set On Less Than Immediate
    // T: if GPR[rs] < (immediate_{15})^{16} || immediate_{15...0} then
    //        GPR[rt] <- 0^{31} || 1
    //    else
    //        GPR[rt] <- 0^{32}
    //    endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t immediate = 0xFFFF & instruction;

    Log::log(std::format("SLTI {:s},{:s},0x{:04x}",
                         memory.regs.getRegisterName(rt),
                         memory.regs.getRegisterName(rs),
                         immediate));

    uint32_t rsValue = memory.regs.getRegister(rs);
    uint32_t signExtension = ((immediate >> 15) ? 0xFFFF0000 : 0x00000000) + immediate;
    
    int32_t rsValueSigned = (int32_t)rsValue;
    int32_t signExtensionSigned = (int32_t)signExtension;

    if (rsValueSigned < signExtensionSigned) {
        memory.regs.setRegister(rt, 1);

    } else {
        memory.regs.setRegister(rt, 0);
    }
}

void Core::SLTIU() {
    // Set On Less Than Immediate Unsigned
    // T: if (0 || GPR[rs]) < (immediate_{15})^{16} || immediate_{15...0} then
    //        GPR[rt] <- 0^{31} || 1
    //    else
    //        GPR[rt] <- 0^{32}
    //    endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t immediate = 0xFFFF & instruction;

    Log::log(std::format("SLTI {:s},{:s},0x{:04x}",
                         memory.regs.getRegisterName(rt),
                         memory.regs.getRegisterName(rs),
                         immediate));

    uint32_t rsValue = memory.regs.getRegister(rs);
    uint32_t signExtension = ((immediate >> 15) ? 0xFFFF0000 : 0x00000000) + immediate;

    if (rsValue < signExtension) {
        memory.regs.setRegister(rt, 1);

    } else {
        memory.regs.setRegister(rt, 0);
    }
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

    if (rt == 0 && rd == 0 && sa == 0) {
        Log::log(std::format("NOP"));
    } else {
        Log::log(std::format("SLL {:s},{:s},{:s}",
                             memory.regs.getRegisterName(rd),
                             memory.regs.getRegisterName(rt),
                             memory.regs.getRegisterName(sa)));
    }

    memory.regs.setRegister(rd, memory.regs.getRegister(rt) << sa);
}

void Core::OR() {
    // Or
    // T: GPR[rd] <- GPR[rs] or GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);
    Log::log(std::format("OR {:s},{:s},{:s}",
                         memory.regs.getRegisterName(rd),
                         memory.regs.getRegisterName(rs),
                         memory.regs.getRegisterName(rt)));

    memory.regs.setRegister(rd, memory.regs.getRegister(rs) | memory.regs.getRegister(rt));
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

    Log::log(std::format("SLTU {:s},{:s},{:s}",
                         memory.regs.getRegisterName(rd),
                         memory.regs.getRegisterName(rs),
                         memory.regs.getRegisterName(rt)));

    uint32_t rsValue = memory.regs.getRegister(rs);
    uint32_t rtValue = memory.regs.getRegister(rt);
    Log::log(std::format(" (0x{:08x} < 0x{:08x}?)",
                         rsValue, rtValue));

    if (rsValue < rtValue) {
        memory.regs.setRegister(rd, 1);

    } else {
        memory.regs.setRegister(rd, 0);
    }
}

void Core::ADDU() {
    // Add Unsigned Word
    // T: GPR[rd] <- GPR[rs] + GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    Log::log(std::format("ADDU {:s},{:s},{:s}",
                         memory.regs.getRegisterName(rd),
                         memory.regs.getRegisterName(rs),
                         memory.regs.getRegisterName(rt)));

    uint32_t rsValue = memory.regs.getRegister(rs);
    uint32_t rtValue = memory.regs.getRegister(rt);

    memory.regs.setRegister(rd, rsValue + rtValue);
}

void Core::JR() {
    // Jump Register
    // T: temp <- GPR[rs]
    // T+1: PC <- PC + target
    // Should be temp, right?
    uint8_t rs = 0x1F & (instruction >> 21);

    Log::log(std::format("JR {:s}",
                         memory.regs.getRegisterName(rs)));

    uint32_t target = memory.regs.getRegister(rs);
    memory.regs.setPC(target);
    delaySlotIsBranchDelaySlot = true;

    if (target & 0x3) {
        throw exceptions::ExceptionNotImplemented("Address Error");
    }
}

void Core::JALR() {
    // Jump And Link Register
    // T: temp <- GPR[rs]
    //    GPR[rd] <- PC + 8
    // T+1: PC <- PC + target
    // Should be temp, right?
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rd = 0x1F & (instruction >> 11);

    Log::log(std::format("JALR {:s},{:s}",
                         memory.regs.getRegisterName(rd),
                         memory.regs.getRegisterName(rs)));

    uint32_t target = memory.regs.getRegister(rs);
    memory.regs.setPC(target);
    memory.regs.setRegister(rd, instructionPC + 8);
    delaySlotIsBranchDelaySlot = true;

    if (target & 0x3) {
        throw exceptions::ExceptionNotImplemented("Address Error");
    }
}

void Core::AND() {
    // And
    // T: GPR[rd] <- GPR[rs] and GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    Log::log(std::format("AND {:s},{:s},{:s}",
                         memory.regs.getRegisterName(rd),
                         memory.regs.getRegisterName(rs),
                         memory.regs.getRegisterName(rt)));

    memory.regs.setRegister(rd, memory.regs.getRegister(rs) & memory.regs.getRegister(rt));
}

void Core::ADD() {
    // Add Word
    // T: GPR[rd] <- GPR[rs] + GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    Log::log(std::format("ADD {:s},{:s},{:s}",
                         memory.regs.getRegisterName(rd),
                         memory.regs.getRegisterName(rs),
                         memory.regs.getRegisterName(rt)));

    uint32_t rsValue = memory.regs.getRegister(rs);
    uint32_t rtValue = memory.regs.getRegister(rt);

    bool carry30 = ((0x7FFFFFFF & rsValue) + (0x7FFFFFFF & rtValue)) & 0x80000000;
    bool carry31 = ((rsValue >> 31) + (rtValue >> 31) + (carry30 ? 1 : 0) >= 2);

    if (carry30 == carry31) {
        memory.regs.setRegister(rd, rsValue + rtValue);

    } else {
        throw exceptions::ExceptionNotImplemented("Integer Overflow");
    }
}

void Core::SUBU() {
    // Subtract Unsigned Word
    // T: GPR[rd] <- GPR[rs] - GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    Log::log(std::format("SUBU {:s},{:s},{:s}",
                         memory.regs.getRegisterName(rd),
                         memory.regs.getRegisterName(rs),
                         memory.regs.getRegisterName(rt)));

    uint32_t rsValue = memory.regs.getRegister(rs);
    uint32_t rtValue = memory.regs.getRegister(rt);

    memory.regs.setRegister(rd, rsValue - rtValue);
}

void Core::SRA() {
    // Shift Word Right Arithmetic
    // T: GPR[rd] <- (GPR[rt]_{31})^{sa} || GPR[rt]_{31...sa}
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);
    uint8_t sa = 0x1F & (instruction >> 6);

    Log::log(std::format("SRA {:s},{:s},{:s}",
                         memory.regs.getRegisterName(rd),
                         memory.regs.getRegisterName(rt),
                         memory.regs.getRegisterName(sa)));

    uint32_t rtValue = memory.regs.getRegister(rt);
    uint32_t result = rtValue >> sa;
    if (rtValue >> 31) {
        assert (sa <= 32);
        result = result | (0xFFFFFFFF << (32 - sa));
    }

    memory.regs.setRegister(rd, result);
}

void Core::DIV() {
    // Divide Word
    // T-2: LO <- undefined
    //      HI <- undefined
    // T-1: LO <- undefined
    //      HI <- undefined
    // T:   LO <- GPR[rs] div GPR[rt]
    //      HI <- GPR[rs] mod GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);

    Log::log(std::format("DIV {:s},{:s}",
                         memory.regs.getRegisterName(rs),
                         memory.regs.getRegisterName(rt)));

    int32_t rsValue = (int32_t)memory.regs.getRegister(rs);
    int32_t rtValue = (int32_t)memory.regs.getRegister(rt);

    memory.regs.setLo(rsValue / rtValue);
    memory.regs.setHi(rsValue % rtValue);
}

void Core::MFLO() {
    // Move From Lo
    // T: GPR[rd] <- LO
    uint8_t rd = 0x1F & (instruction >> 11);
    
    Log::log(std::format("MFLO {:s}", memory.regs.getRegisterName(rd)));

    memory.regs.setRegister(rd, memory.regs.getLo());
}

void Core::UNKCP0() {
    throw exceptions::UnknownFunctionError(std::format("0x{:x}: instruction 0x{:x} (CP0), function 0b{:06b}", instructionPC, instruction, funct));
}

void Core::SRL() {
    // Shift Word Right Logical
    // T: GPR[rd] <- 0^{sa} || GPR[rt]_{31...sa}
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);
    uint8_t sa = 0x1F & (instruction >> 6);

    Log::log(std::format("SRL {:s},{:s},{:s}",
                         memory.regs.getRegisterName(rd),
                         memory.regs.getRegisterName(rt),
                         memory.regs.getRegisterName(sa)));

    memory.regs.setRegister(rd, memory.regs.getRegister(rt) >> sa);
}

void Core::DIVU() {
    // Divide Word Unsigned
    // T-2: LO <- undefined
    //      HI <- undefined
    // T-1: LO <- undefined
    //      HI <- undefined
    // T:   LO <- GPR[rs] div GPR[rt]
    //      HI <- GPR[rs] mod GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);

    Log::log(std::format("DIVU {:s},{:s}",
                         memory.regs.getRegisterName(rs),
                         memory.regs.getRegisterName(rt)));

    uint32_t rsValue = memory.regs.getRegister(rs);
    uint32_t rtValue = memory.regs.getRegister(rt);

    memory.regs.setLo(rsValue / rtValue);
    memory.regs.setHi(rsValue % rtValue);
}

void Core::MFHI() {
    // Move From Hi
    // T: GPR[rd] <- HI
    uint8_t rd = 0x1F & (instruction >> 11);
    
    Log::log(std::format("MFHI {:s}", memory.regs.getRegisterName(rd)));

    memory.regs.setRegister(rd, memory.regs.getHi());
}

void Core::SLT() {
    // Set on Less Than
    // T: if GPR[rs] < GPR[rt] then
    //     GPR[rd] <- 0^{31} || 1
    // else
    //     GPR[rd] <- 0^{32}
    // endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    Log::log(std::format("SLT {:s},{:s},{:s}",
                         memory.regs.getRegisterName(rd),
                         memory.regs.getRegisterName(rs),
                         memory.regs.getRegisterName(rt)));

    int32_t rsValue = (int32_t)memory.regs.getRegister(rs);
    int32_t rtValue = (int32_t)memory.regs.getRegister(rt);
    Log::log(std::format(" (0x{:08x} < 0x{:08x}?)",
                         rsValue, rtValue));

    if (rsValue < rtValue) {
        memory.regs.setRegister(rd, 1);

    } else {
        memory.regs.setRegister(rd, 0);
    }
}

void Core::SYSCALL() {
    // System Call
    // T: SystemCallException
    //uint32_t code = 0xFFFFF & (instruction >> 6);
    // the exception handler can access "code" by manually loading the instruction

    Log::log("SYSCALL");

    // make EPC point to restart location
    // the EPC has to point to the instruction which caused the error
    // if the instruction is in a branch delay slot, then
    // it has to point to the preceding branch instruction
    // and signal this via the BD bit
    if (isBranchDelaySlot) {
        memory.regs.setCP0Register(CP0_REGISTER_EPC, instructionPC - 4);
        // set BD bit
        uint32_t cause = memory.regs.getCP0Register(CP0_REGISTER_CAUSE);
        memory.regs.setCP0Register(CP0_REGISTER_CAUSE, cause | (0x1 << CAUSE_BIT_BD));

    } else {
        memory.regs.setCP0Register(CP0_REGISTER_EPC, instructionPC); 
    }
    
    // save user-mode-enable and interrupt-enable flags in SR
    // by pushing the 3-entry stack inside of SR
    uint32_t sr = memory.regs.getCP0Register(CP0_REGISTER_SR);
    // we clear KUc and IEc, not sure if this is correct
    memory.regs.setCP0Register(CP0_REGISTER_SR, (sr & 0xFFFFFFC0) | ((sr & 0x3F) << 2));

    // set up ExcCode in Cause register
    uint32_t cause = memory.regs.getCP0Register(CP0_REGISTER_CAUSE);
    cause = (cause & 0xFFFFFF83) | (8 << 2); // 8 is syscall
    memory.regs.setCP0Register(CP0_REGISTER_CAUSE, cause);
    
    // set BadVaddr on address exception
    // not for syscall

    // transfer control to exception entry point
    if (memory.regs.getCP0Register(CP0_REGISTER_SR) & (1 << SR_BIT_BEV)) {
        memory.regs.setPC(0xBFC00180);

    } else {
        memory.regs.setPC(0x80000080);
    }
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

    Log::log(std::format("MTC0 {:s},{:d}",
                         memory.regs.getRegisterName(rt),
                         rd));

    uint32_t data = memory.regs.getRegister(rt);
    Log::log(std::format(" (0x{:08X} -> CP0 {:d})",data, rd));

    memory.regs.setCP0Register(rd, data);
}

void Core::MFC0() {
    // Move From Coprocessor 0
    // T: data <- CPR[0, rd]
    // T+1: GPR[rt] <- data
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    Log::log(std::format("MFC0 {:s},{:d}",
                         memory.regs.getRegisterName(rt),
                         rd));

    uint32_t data = memory.regs.getCP0Register(rd);
    Log::log(std::format(" (CP0 {:d} -0x{:08X}->)",
                         rd, data));

    memory.regs.setRegister(rt, data);
}

void Core::UNKRGMM() {
    throw exceptions::UnknownOpcodeError(std::format("0x{:x}: instruction 0x{:x} (REGIMM), rt 0b{:05b}", instructionPC, instruction, instructionRt));
}

void Core::BLTZ() {
    // Branch On Less Than Zero
    // T: target <- (offset_{15})^{14} || offset || 0^2
    //    condition <- (GPR[rs]_{31} = 1)
    //    GPR[31] <- PC + 8
    // T+1: if condition then
    //          PC <- PC + target
    //      endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint32_t offset = 0xFFFF & instruction;

    Log::log(std::format("BLTZ {:s},{:04X}",
                         memory.regs.getRegisterName(rs),
                         offset));

    uint32_t signExtension = ((offset >> 15) ? 0xFFFF0000 : 0x00000000) + offset;
    uint32_t target = signExtension << 2;
    uint32_t actualTarget = delaySlotPC + target;

    uint32_t rsValue = memory.regs.getRegister(rs);
    Log::log(std::format(" (0x{:08X} < 0? -0x{:08X}-> pc)",
                         rsValue, actualTarget));

    if (rsValue >> 31) {
        memory.regs.setPC(actualTarget);
        delaySlotIsBranchDelaySlot = true;
    }
}

void Core::BLTZAL() {
    // Branch On Less Than Zero And Link
    // T: target <- (offset_{15})^{14} || offset || 0^2
    //    condition <- (GPR[rs]_{31} = 1)
    //    GPR[31] <- PC + 8
    // T+1: if condition then
    //          PC <- PC + target
    //      endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint32_t offset = 0xFFFF & instruction;

    Log::log(std::format("BLTZAL {:s},{:04X}",
                         memory.regs.getRegisterName(rs),
                         offset));

    uint32_t signExtension = ((offset >> 15) ? 0xFFFF0000 : 0x00000000) + offset;
    uint32_t target = signExtension << 2;
    uint32_t actualTarget = delaySlotPC + target;

    uint32_t rsValue = memory.regs.getRegister(rs);
    Log::log(std::format(" (0x{:08X} < 0? -0x{:08X}-> pc)",
                         rsValue, actualTarget));

    if (rsValue >> 31) {
        memory.regs.setRegister(31, instructionPC + 8);
        memory.regs.setPC(actualTarget);
        delaySlotIsBranchDelaySlot = true;
    }
}

void Core::BGEZ() {
    // Branch On Greater Than Or Equal To Zero
    // T: target <- (offset_{15})^{14} || offset || 0^2
    //    condition <- (GPR[rs]_{31} = 0)
    // T+1: if condition then
    //          PC <- PC + target
    //      endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint32_t offset = 0xFFFF & instruction;

    Log::log(std::format("BGEZ {:s},{:04X}",
                         memory.regs.getRegisterName(rs),
                         offset));

    uint32_t signExtension = ((offset >> 15) ? 0xFFFF0000 : 0x00000000) + offset;
    uint32_t target = signExtension << 2;
    uint32_t actualTarget = delaySlotPC + target;

    uint32_t rsValue = memory.regs.getRegister(rs);
    Log::log(std::format(" (0x{:08X} > 0? -0x{:08X}-> pc)",
                         rsValue, actualTarget));

    if (!(rsValue >> 31)) {
        memory.regs.setPC(actualTarget);
        delaySlotIsBranchDelaySlot = true;
    }
}

Core::Core() {
    reset();
}

void Core::reset() {
    this->memory.regs.reset();
    this->memory.reset();

    instructionPC = 0;
    instruction = 0;
    isBranchDelaySlot = false;

    delaySlotPC = 0;
    delaySlot = 0;
    delaySlotIsBranchDelaySlot = false;
}

void Core::readBIOS(const std::string &file) {
    memory.readBIOS(file);
}

void Core::step() {
    instructionPC = delaySlotPC;
    instruction = delaySlot;
    isBranchDelaySlot = delaySlotIsBranchDelaySlot;
    if (instructionPC == 0x80030000) {
        Log::logEnabled = true;
    }

    // load delay-slot instruction from memory at program counter
    delaySlotPC = memory.regs.getPC();
    delaySlot = memory.readWord(delaySlotPC);
    delaySlotIsBranchDelaySlot = false; // this will be set to true be branch instructions

    // increase program counter
    // by increasing it before executing the instruction,
    // it may be overwritten by the instruction
    memory.regs.setPC(delaySlotPC + 4);
    
    // execute instruction
    Log::log(std::format("@0x{:08X}: ", instructionPC));
    opcode = instruction >> 26;
    assert (opcode <= 0b111111);
    (this->*opcodes[opcode])();
    Log::log("\n");
}
}
