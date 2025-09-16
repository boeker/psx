#include "disassembler.h"

#include <format>

namespace util {

const char* Disassembler::REGISTER_NAMES[] = {
    "zr", // always returns zero
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

std::string Disassembler::getRegisterName(uint8_t reg) {
    return REGISTER_NAMES[reg];
}

std::string Disassembler::disassemble(uint32_t ins) {
    instruction = ins;

    opcode = instruction >> 26;
    return (*opcodes[opcode])();
}

uint32_t Disassembler::instruction = 0;
uint8_t Disassembler::opcode = 0;
uint8_t Disassembler::funct = 0;
uint8_t Disassembler::move = 0;
uint8_t Disassembler::instructionRt = 0;

const Disassembler::Opcode Disassembler::opcodes[] = {
    // 0b000000
    &Disassembler::SPECIAL,  &Disassembler::REGIMM,   &Disassembler::J,        &Disassembler::JAL,
    // 0b000100
    &Disassembler::BEQ,      &Disassembler::BNE,      &Disassembler::BLEZ,     &Disassembler::BGTZ,
    // 0b001000
    &Disassembler::ADDI,     &Disassembler::ADDIU,    &Disassembler::SLTI,     &Disassembler::SLTIU,
    // 0b001100
    &Disassembler::ANDI,     &Disassembler::ORI,      &Disassembler::XORI,     &Disassembler::LUI,
    // 0b010000
    &Disassembler::CP0,      &Disassembler::UNK,      &Disassembler::CP2,      &Disassembler::UNK,
    // 0b010100
    &Disassembler::UNK,      &Disassembler::UNK,      &Disassembler::UNK,      &Disassembler::UNK,
    // 0b011000
    &Disassembler::UNK,      &Disassembler::UNK,      &Disassembler::UNK,      &Disassembler::UNK,
    // 0b011100
    &Disassembler::UNK,      &Disassembler::UNK,      &Disassembler::UNK,      &Disassembler::UNK,
    // 0b100000
    &Disassembler::LB,       &Disassembler::LH,       &Disassembler::LWL,      &Disassembler::LW,
    // 0b100100
    &Disassembler::LBU,      &Disassembler::LHU,      &Disassembler::LWR,      &Disassembler::UNK,
    // 0b101000
    &Disassembler::SB,       &Disassembler::SH,       &Disassembler::SWL,      &Disassembler::SW,
    // 0b101100
    &Disassembler::UNK,      &Disassembler::UNK,      &Disassembler::SWR,      &Disassembler::UNK,
    // 0b110000
    &Disassembler::UNK,      &Disassembler::UNK,      &Disassembler::UNK,      &Disassembler::UNK,
    // 0b110100
    &Disassembler::UNK,      &Disassembler::UNK,      &Disassembler::UNK,      &Disassembler::UNK,
    // 0b111000
    &Disassembler::UNK,      &Disassembler::UNK,      &Disassembler::UNK,      &Disassembler::UNK,
    // 0b111100
    &Disassembler::UNK,      &Disassembler::UNK,      &Disassembler::UNK,      &Disassembler::UNK
};

const Disassembler::Opcode Disassembler::special[] = {
    // 0b000000
    &Disassembler::SLL,      &Disassembler::UNKSPCL,  &Disassembler::SRL,      &Disassembler::SRA,
    // 0b000100
    &Disassembler::SLLV,     &Disassembler::UNKSPCL,  &Disassembler::SRLV,     &Disassembler::SRAV,
    // 0b001000
    &Disassembler::JR,       &Disassembler::JALR,     &Disassembler::UNKSPCL,  &Disassembler::UNKSPCL,
    // 0b001100
    &Disassembler::SYSCALL,  &Disassembler::UNKSPCL,  &Disassembler::UNKSPCL,  &Disassembler::UNKSPCL,
    // 0b010000
    &Disassembler::MFHI,     &Disassembler::MTHI,     &Disassembler::MFLO,     &Disassembler::MTLO,
    // 0b010100
    &Disassembler::UNKSPCL,  &Disassembler::UNKSPCL,  &Disassembler::UNKSPCL,  &Disassembler::UNKSPCL,
    // 0b011000
    &Disassembler::UNKSPCL,  &Disassembler::MULTU,    &Disassembler::DIV,      &Disassembler::DIVU,
    // 0b011100
    &Disassembler::UNKSPCL,  &Disassembler::UNKSPCL,  &Disassembler::UNKSPCL,  &Disassembler::UNKSPCL,
    // 0b100000
    &Disassembler::ADD,      &Disassembler::ADDU,     &Disassembler::UNKSPCL,  &Disassembler::SUBU,
    // 0b100100
    &Disassembler::AND,      &Disassembler::OR,       &Disassembler::XOR,      &Disassembler::NOR,
    // 0b101000
    &Disassembler::UNKSPCL,  &Disassembler::UNKSPCL,  &Disassembler::SLT,      &Disassembler::SLTU,
    // 0b101100
    &Disassembler::UNKSPCL,  &Disassembler::UNKSPCL,  &Disassembler::UNKSPCL,  &Disassembler::UNKSPCL,
    // 0b110000
    &Disassembler::UNKSPCL,  &Disassembler::UNKSPCL,  &Disassembler::UNKSPCL,  &Disassembler::UNKSPCL,
    // 0b110100
    &Disassembler::UNKSPCL,  &Disassembler::UNKSPCL,  &Disassembler::UNKSPCL,  &Disassembler::UNKSPCL,
    // 0b111000
    &Disassembler::UNKSPCL,  &Disassembler::UNKSPCL,  &Disassembler::UNKSPCL,  &Disassembler::UNKSPCL,
    // 0b111100
    &Disassembler::UNKSPCL,  &Disassembler::UNKSPCL,  &Disassembler::UNKSPCL,  &Disassembler::UNKSPCL
};

const Disassembler::Opcode Disassembler::cp0[] = {
    // 0b000000
    &Disassembler::CP0MOVE,  &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,
    // 0b000100
    &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,
    // 0b001000
    &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,
    // 0b001100
    &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,
    // 0b010000
    &Disassembler::RFE,      &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,
    // 0b010100
    &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,
    // 0b011000
    &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,
    // 0b011100
    &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,
    // 0b100000
    &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,
    // 0b100100
    &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,
    // 0b101000
    &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,
    // 0b101100
    &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,
    // 0b110000
    &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,
    // 0b110100
    &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,
    // 0b111000
    &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,
    // 0b111100
    &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0,   &Disassembler::UNKCP0
};

const Disassembler::Opcode Disassembler::cp0Move[] = {
    // 0b00000
    &Disassembler::MFC0,     &Disassembler::UNKCP0M,  &Disassembler::UNKCP0M,  &Disassembler::UNKCP0M,
    // 0b00100
    &Disassembler::MTC0,     &Disassembler::UNKCP0M,  &Disassembler::UNKCP0M,  &Disassembler::UNKCP0M,
    // 0b01000
    &Disassembler::UNKCP0M,  &Disassembler::UNKCP0M,  &Disassembler::UNKCP0M,  &Disassembler::UNKCP0M,
    // 0b01100
    &Disassembler::UNKCP0M,  &Disassembler::UNKCP0M,  &Disassembler::UNKCP0M,  &Disassembler::UNKCP0M,
    // 0b10000
    &Disassembler::UNKCP0M,  &Disassembler::UNKCP0M,  &Disassembler::UNKCP0M,  &Disassembler::UNKCP0M,
    // 0b10100
    &Disassembler::UNKCP0M,  &Disassembler::UNKCP0M,  &Disassembler::UNKCP0M,  &Disassembler::UNKCP0M,
    // 0b11000
    &Disassembler::UNKCP0M,  &Disassembler::UNKCP0M,  &Disassembler::UNKCP0M,  &Disassembler::UNKCP0M,
    // 0b11100
    &Disassembler::UNKCP0M,  &Disassembler::UNKCP0M,  &Disassembler::UNKCP0M,  &Disassembler::UNKCP0M
};

const Disassembler::Opcode Disassembler::cp2[] = {
    // 0b000000
    &Disassembler::CP2MOVE,  &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,
    // 0b000100
    &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,
    // 0b001000
    &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,
    // 0b001100
    &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,
    // 0b010000
    &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,
    // 0b010100
    &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,
    // 0b011000
    &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,
    // 0b011100
    &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,
    // 0b100000
    &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,
    // 0b100100
    &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,
    // 0b101000
    &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,
    // 0b101100
    &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,
    // 0b110000
    &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,
    // 0b110100
    &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,
    // 0b111000
    &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,
    // 0b111100
    &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2,   &Disassembler::UNKCP2
};

const Disassembler::Opcode Disassembler::cp2Move[] = {
    // 0b00000
    &Disassembler::UNKCP2M,  &Disassembler::UNKCP2M,  &Disassembler::UNKCP2M,  &Disassembler::UNKCP2M,
    // 0b00100
    &Disassembler::MTC2,     &Disassembler::UNKCP2M,  &Disassembler::CTC2,     &Disassembler::UNKCP2M,
    // 0b01000
    &Disassembler::UNKCP2M,  &Disassembler::UNKCP2M,  &Disassembler::UNKCP2M,  &Disassembler::UNKCP2M,
    // 0b01100
    &Disassembler::UNKCP2M,  &Disassembler::UNKCP2M,  &Disassembler::UNKCP2M,  &Disassembler::UNKCP2M,
    // 0b10000
    &Disassembler::UNKCP2M,  &Disassembler::UNKCP2M,  &Disassembler::UNKCP2M,  &Disassembler::UNKCP2M,
    // 0b10100
    &Disassembler::UNKCP2M,  &Disassembler::UNKCP2M,  &Disassembler::UNKCP2M,  &Disassembler::UNKCP2M,
    // 0b11000
    &Disassembler::UNKCP2M,  &Disassembler::UNKCP2M,  &Disassembler::UNKCP2M,  &Disassembler::UNKCP2M,
    // 0b11100
    &Disassembler::UNKCP2M,  &Disassembler::UNKCP2M,  &Disassembler::UNKCP2M,  &Disassembler::UNKCP2M
};

const Disassembler::Opcode Disassembler::regimm[] = {
    // 0b00000
    &Disassembler::BLTZ,     &Disassembler::BGEZ,     &Disassembler::UNKRGMM,  &Disassembler::UNKRGMM,
    // 0b00100
    &Disassembler::UNKRGMM,  &Disassembler::UNKRGMM,  &Disassembler::UNKRGMM,  &Disassembler::UNKRGMM,
    // 0b01000
    &Disassembler::UNKRGMM,  &Disassembler::UNKRGMM,  &Disassembler::UNKRGMM,  &Disassembler::UNKRGMM,
    // 0b01100
    &Disassembler::UNKRGMM,  &Disassembler::UNKRGMM,  &Disassembler::UNKRGMM,  &Disassembler::UNKRGMM,
    // 0b10000
    &Disassembler::UNKRGMM,  &Disassembler::UNKRGMM,  &Disassembler::UNKRGMM,  &Disassembler::UNKRGMM,
    // 0b10100
    &Disassembler::UNKRGMM,  &Disassembler::UNKRGMM,  &Disassembler::UNKRGMM,  &Disassembler::UNKRGMM,
    // 0b11000
    &Disassembler::UNKRGMM,  &Disassembler::UNKRGMM,  &Disassembler::UNKRGMM,  &Disassembler::UNKRGMM,
    // 0b11100
    &Disassembler::UNKRGMM,  &Disassembler::UNKRGMM,  &Disassembler::UNKRGMM,  &Disassembler::UNKRGMM
};

std::string Disassembler::UNK() {
    return "UNK";
}

std::string Disassembler::SPECIAL() {
    // SPECIAL
    // Operation depends on function field
    funct = 0x3F & instruction;

    return (*special[funct])();
}

std::string Disassembler::CP0() {
    // CP0
    // Operation depends on function field
    funct = 0x3F & instruction;

    return (*cp0[funct])();
}

std::string Disassembler::CP2() {
    // CP2
    // Operation depends on function field
    funct = 0x3F & instruction;

    return (*cp2[funct])();
}

std::string Disassembler::REGIMM() {
    // REGIMM
    // Operation depends on rt field
    instructionRt = 0x1F & (instruction >> 16);

    return (*regimm[instructionRt])();
}

std::string Disassembler::LUI() {
    // Load Upper Immediate
    // GPR[rt] <- immediate || 0^{16}
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t immediate = 0xFFFF & instruction;

    return std::format("LUI {:s},0x{:04X}",
                       getRegisterName(rt),
                       immediate);
}

std::string Disassembler::ORI() {
    // Or Immediate
    // T: GPR[rt] <- GPR[rs]_{31...36} || (immedate or GPR[rs]_{15...0})
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t immediate = 0xFFFF & instruction;

    return std::format("ORI {:s},{:s},0x{:04x}",
                       getRegisterName(rt),
                       getRegisterName(rs),
                       immediate);
}

std::string Disassembler::SW() {
    // Store Word
    // T: vAddr <- ((offset_{15})^{16} | offset_{15...0}) + GPR[base]
    // (pAddr, uncached) <- AddressTranslation(vAddr, DATA)
    // data <- GPR[rt]
    // StoreMemory(uncached, WORD, data, pAddr, vAddr, DATA)
    uint8_t base = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    return std::format("SW {:s},0x{:04X}({:s})",
                       getRegisterName(rt),
                       offset,
                       getRegisterName(base));
}

std::string Disassembler::ADDIU() {
    // Add Immediate Unsigned Word
    // T: temp <- GPR[rs] + (immediate_{15})^{48} || immediate_{15...0}
    // if 32-bit-overflow(temp) then
    //     GPR[rt] <- (temp_{31})^{32} || temp_{31...0}
    // else
    //     GPR[rt] <- temp
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t immediate = 0xFFFF & instruction;

    return std::format("ADDIU {:s},{:s},0x{:04X}",
                       getRegisterName(rt),
                       getRegisterName(rs),
                       immediate);
}

std::string Disassembler::J() {
    // Jump
    // T: temp <- target
    // T+1: pc <- pc_{31...28} || temp || 0^2
    uint32_t target = 0x3FFFFFF & instruction;

    return std::format("J 0x{:08X}", target);
}

std::string Disassembler::BNE() {
    // Branch On Not Equal
    // T: target <- (offset_{15})^{14} || offset || 0^2
    //    condition <- (GPR[rs] != GPR[rt])
    // T+1: if condition then
    //      PC <- PC + target
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    uint32_t target = ((offset >> 15) ? 0xFFFC0000 : 0x00000000) | (offset << 2);

    return std::format("BNE {:s},{:s},0x{:04X} (+0x{:08X})",
                       getRegisterName(rt),
                       getRegisterName(rs),
                       offset,
                       target);
}

std::string Disassembler::ADDI() {
    // Add Immediate Word
    // T: GPR[rt] <- GPR[rs] + (immediate_{15})^{16} || immediate_{15...0}
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t immediate = 0xFFFF & instruction;

    return std::format("ADDI {:s},{:s},0x{:04X}",
                       getRegisterName(rt),
                       getRegisterName(rs),
                       immediate);
}

std::string Disassembler::LW() {
    // Load Word
    // T: vAddr <- ((offset_{15})^{16} | offset_{15...0}) + GPR[base]
    // (pAddr, uncached) <- AddressTranslation(vAddr, DATA)
    // mem <- LoadMemory(uncached, WORD, pAddr, vAddr, DATA)
    // T+1: GPR[rt] <- mem
    uint8_t base = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    return std::format("LW {:s},0x{:04X}({:s})",
                        getRegisterName(rt),
                        offset,
                        getRegisterName(base));
}

std::string Disassembler::SH() {
    // Store Halfword
    // T: vAddr <- ((offset_{15})^{16} | offset_{15...0}) + GPR[base]
    // (pAddr, uncached) <- AddressTranslation(vAddr, DATA)
    // pAddr <- pAddr_{PSIZE-1...2} || (pAddr_{1...0} xor (ReverseEndian || 0))
    // byte <- vAddr_{1...0} xor (BigEndianDisassembler || 0)
    // data <- GPR[rt]_{31-8*byte...0} || 0^{8*byte}
    // StoreMemory(uncached, HALFWORD, data, pAddr, vAddr, DATA)
    uint8_t base = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    return std::format("SH {:s},0x{:08X}({:s})",
                        getRegisterName(rt),
                        offset,
                        getRegisterName(base));
}

std::string Disassembler::JAL() {
    // Jump And Link
    // T: temp <- target
    //    GPR[31] <- PC + 8
    // T+1: PC <- PC_{31...28} || temp || 0^2
    uint32_t target = 0x03FFFFFF & instruction;

    return std::format("JAL 0x{:06X}", target);
}

std::string Disassembler::ANDI() {
    // And Immediate
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t immediate = 0xFFFF & instruction;

    return std::format("ANDI {:s},{:s},0x{:04X}",
                       getRegisterName(rt),
                       getRegisterName(rs),
                       immediate);
}

std::string Disassembler::SB() {
    // Store Byte
    // T: vAddr <- ((offset_{15})^{16} | offset_{15...0}) + GPR[base]
    // (pAddr, uncached) <- AddressTranslation(vAddr, DATA)
    // pAddr <- pAddr_{PSIZE-1...2} || (pAddr_{1...0} xor ReverseEndian^2)
    // byte <- vAddr_{1...0} xor BigEndianDisassembler^2
    // data <- GPR[rt]_{31-8*byte...0} || 0^{8*byte}
    // StoreMemory(uncached, BYTE, data, pAddr, vAddr, DATA)
    uint8_t base = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    return std::format("SB {:s},0x{:04X}({:s})",
                       getRegisterName(rt),
                       offset,
                       getRegisterName(base));
}

std::string Disassembler::LB() {
    // Load Byte
    // T: vAddr <- ((offset_{15})^{16} | offset_{15...0}) + GPR[base]
    // (pAddr, uncached) <- AddressTranslation(vAddr, DATA)
    // pAddr <- pAddr_{PSIZE-1...2} || (pAddr_{1...0} xor ReverseEndian^2)
    // mem <- LoadMemory(uncached, BYTE, pAddr, vAddr, DATA)
    // byte <- vAddr_{1...0} xor BigEndianDisassembler^2
    // T+1: GPR[rt] <- (mem_{7+8*byte)^{24} || mem_{7+8*byte...8*byte}
    uint8_t base = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    return std::format("LB {:s},0x{:04X}({:s})",
                       getRegisterName(rt),
                       offset,
                       getRegisterName(base));
}

std::string Disassembler::BEQ() {
    // Branch On Equal
    // T: target <- (offset_{15})^{14} || offset || 0^2
    //    condition <- (GPR[rs] = GPR[rt])
    // T+1: if condition then
    //          PC <- PC + target
    //      endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    return std::format("BEQ {:s},{:s},{:04X}",
                       getRegisterName(rs),
                       getRegisterName(rt),
                       offset);
}

std::string Disassembler::BGTZ() {
    // Branch On Greater Than Zero
    // T: target <- (offset_{15})^{14} || offset || 0^2
    //    condition <- (GPR[rs]_{31} = 0 and GPR[rs] != 0^{32})
    // T+1: if condition then
    //          PC <- PC + target
    //      endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint32_t offset = 0xFFFF & instruction;

    return std::format("BGTZ {:s},{:04X}",
                       getRegisterName(rs),
                       offset);
}

std::string Disassembler::BLEZ() {
    // Branch On Less Than Or Equal To Zero
    // T: target <- (offset_{15})^{14} || offset || 0^2
    //    condition <- (GPR[rs]_{31} = 1) or (GPR[rs] == 0^{32})
    // T+1: if condition then
    //          PC <- PC + target
    //      endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint32_t offset = 0xFFFF & instruction;

    return std::format("BLEZ {:s},{:04X}",
                        getRegisterName(rs),
                        offset);
}

std::string Disassembler::LBU() {
    // Load Byte Unsigned
    // T: vAddr <- ((offset_{15})^{16} | offset_{15...0}) + GPR[base]
    // (pAddr, uncached) <- AddressTranslation(vAddr, DATA)
    // pAddr <- pAddr_{PSIZE-1...2} || (pAddr_{1...0} xor ReverseEndian^2)
    // mem <- LoadMemory(uncached, BYTE, pAddr, vAddr, DATA)
    // byte <- vAddr_{1...0} xor BigEndianDisassembler^2
    // T+1: GPR[rt] <- 0^{24} || mem_{7+8*byte...8*byte}
    uint8_t base = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    return std::format("LBU {:s},0x{:04X}({:s})",
                       getRegisterName(rt),
                       offset,
                       getRegisterName(base));
}

std::string Disassembler::SLTI() {
    // Set On Less Than Immediate
    // T: if GPR[rs] < (immediate_{15})^{16} || immediate_{15...0} then
    //        GPR[rt] <- 0^{31} || 1
    //    else
    //        GPR[rt] <- 0^{32}
    //    endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t immediate = 0xFFFF & instruction;

    return std::format("SLTI {:s},{:s},0x{:04x}",
                       getRegisterName(rt),
                       getRegisterName(rs),
                       immediate);
}

std::string Disassembler::SLTIU() {
    // Set On Less Than Immediate Unsigned
    // T: if (0 || GPR[rs]) < (immediate_{15})^{16} || immediate_{15...0} then
    //        GPR[rt] <- 0^{31} || 1
    //    else
    //        GPR[rt] <- 0^{32}
    //    endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t immediate = 0xFFFF & instruction;

    return std::format("SLTI {:s},{:s},0x{:04x}",
                       getRegisterName(rt),
                       getRegisterName(rs),
                       immediate);
}

std::string Disassembler::LHU() {
    // Load Halfword Unsigned
    // T: vAddr <- ((offset_{15})^{16} | offset_{15...0}) + GPR[base]
    // (pAddr, uncached) <- AddressTranslation(vAddr, DATA)
    // pAddr <- pAddr_{PSIZE-1...2} || (pAddr_{1...0} xor (ReverseEndian || 0)
    // mem <- LoadMemory(uncached, HALFWORD, pAddr, vAddr, DATA)
    // byte <- vAddr_{1...0} xor (BigEndianDisassembler || 0)
    // T+1: GPR[rt] <- 0^{16} || mem_{15+8*byte...8*byte}
    uint8_t base = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    return std::format("LHU {:s},0x{:04X}({:s})",
                       getRegisterName(rt),
                       offset,
                       getRegisterName(base));
}

std::string Disassembler::LH() {
    // Load Halfword
    // T: vAddr <- ((offset_{15})^{16} | offset_{15...0}) + GPR[base]
    // (pAddr, uncached) <- AddressTranslation(vAddr, DATA)
    // pAddr <- pAddr_{PSIZE-1...2} || (pAddr_{1...0} xor (ReverseEndian || 0))
    // mem <- LoadMemory(uncached, HALFWORD, pAddr, vAddr, DATA)
    // byte <- vAddr_{1...0} xor (BigEndianDisassembler || 0)
    // T+1: GPR[rt] <- (mem_{15+8*byte)^{16} || mem_{15+8*byte...8*byte}
    uint8_t base = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    return std::format("LH {:s},0x{:04X}({:s})",
                       getRegisterName(rt),
                       offset,
                       getRegisterName(base));
}

std::string Disassembler::LWL() {
    // Load Word Left
    // T: vAddr <- ((offset_{15})^{16} | offset_{15...0}) + GPR[base]
    // (pAddr, uncached) <- AddressTranslation(vAddr, DATA)
    // pAddr <- pAddr_{PSIZE-1...2} || (pAddr_{1...0} xor ReverseEndian^2)
    // if BigEndianMem = 0 then
    //    pAddr <- pAddr_{PSIZE-1...2} || 0^2
    // endif
    // byte <- vAddr_{1...0} xor BigEndianDisassembler^2
    // mem <- LoadMemory(uncached, byte, pAddr, vAddr, DATA)
    // GPR[rt] <- mem_{7+8*byte...0} || GPR[rt]_{23-8*byte...0}
    uint8_t base = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    return std::format("LWL {:s},0x{:04X}({:s})",
                       getRegisterName(rt),
                       offset,
                       getRegisterName(base));
}

std::string Disassembler::LWR() {
    // Load Word Right
    // T: vAddr <- ((offset_{15})^{16} | offset_{15...0}) + GPR[base]
    // (pAddr, uncached) <- AddressTranslation(vAddr, DATA)
    // pAddr <- pAddr_{PSIZE-1...2} || (pAddr_{1...0} xor ReverseEndian^2)
    // if BigEndianMem = 0 then
    //    pAddr <- pAddr_{PSIZE-1...2} || 0^2
    // endif
    // byte <- vAddr_{1...0} xor BigEndianDisassembler^2
    // mem <- LoadMemory(uncached, byte, pAddr, vAddr, DATA)
    // GPR[rt] <- mem_{31...32-8*byte} || GPR[rt]_{31-8*byte...0}
    uint8_t base = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    return std::format("LWR {:s},0x{:04X}({:s})",
                       getRegisterName(rt),
                       offset,
                       getRegisterName(base));
}

std::string Disassembler::SWL() {
    // Store Word Left
    // T: vAddr <- ((offset_{15})^{16} | offset_{15...0}) + GPR[base]
    // (pAddr, uncached) <- AddressTranslation(vAddr, DATA)
    // pAddr <- pAddr_{PSIZE-1...2} || (pAddr_{1...0} xor ReverseEndian^2)
    // if BigEndianMem = 0 then
    //    pAddr <- pAddr_{PSIZE-1...2} || 0^2
    // endif
    // byte <- vAddr_{1...0} xor BigEndianDisassembler^2
    // data <- 0^{24-8*byte} || GPR[rt]_{31...24-8*byte}
    // StoreMemory(uncached, byte, data, pAddr, vAddr, DATA)
    uint8_t base = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    return std::format("SWL {:s},0x{:04X}({:s})",
                       getRegisterName(rt),
                       offset,
                       getRegisterName(base));
}

std::string Disassembler::SWR() {
    // Store Word Right
    // T: vAddr <- ((offset_{15})^{16} | offset_{15...0}) + GPR[base]
    // (pAddr, uncached) <- AddressTranslation(vAddr, DATA)
    // pAddr <- pAddr_{PSIZE-1...2} || (pAddr_{1...0} xor ReverseEndian^2)
    // if BigEndianMem = 0 then
    //    pAddr <- pAddr_{PSIZE-1...2} || 0^2
    // endif
    // byte <- vAddr_{1...0} xor BigEndianDisassembler^2
    // data <- GPR[rt]_{31-8*byte} || 0^{8*byte}
    // StoreMemory(uncached, WORD - byte, data, pAddr, vAddr, DATA)
    uint8_t base = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    return std::format("SWR {:s},0x{:04X}({:s})",
                       getRegisterName(rt),
                       offset,
                       getRegisterName(base));
}

std::string Disassembler::XORI() {
    // Exclusive OR Immediate
    // T: GPR[rt] <- GPR[rs] xor (0^{16} || immediate)
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t immediate = 0xFFFF & instruction;

    return std::format("XORI {:s},{:s},0x{:04X}",
                       getRegisterName(rt),
                       getRegisterName(rs),
                       immediate);
}

std::string Disassembler::UNKSPCL() {
    return "UNKSPCL";
}

std::string Disassembler::SLL() {
    // Shift Word Left Logical
    // T: GPR[rd] <- GPR[rt]_{31 - sa...0} || 0^{sa}
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);
    uint8_t sa = 0x1F & (instruction >> 6);

    if (rt == 0 && rd == 0 && sa == 0) {
        return std::format("NOP");
    } else {
        return std::format("SLL {:s},{:s},{:s}",
                            getRegisterName(rd),
                            getRegisterName(rt),
                            getRegisterName(sa));
    }
}

std::string Disassembler::OR() {
    // Or
    // T: GPR[rd] <- GPR[rs] or GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);
    return std::format("OR {:s},{:s},{:s}",
                       getRegisterName(rd),
                       getRegisterName(rs),
                       getRegisterName(rt));
}

std::string Disassembler::SLTU() {
    // Set on Less Than Unsigned
    // T: if (0 || GPR[rs]) < 0 || GPR[rt] then
    //     GPR[rd] <- 0^{31} || 1
    // else
    //     GPR[rd] <- 0^{32}
    // endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    return std::format("SLTU {:s},{:s},{:s}",
                       getRegisterName(rd),
                       getRegisterName(rs),
                       getRegisterName(rt));
}

std::string Disassembler::ADDU() {
    // Add Unsigned Word
    // T: GPR[rd] <- GPR[rs] + GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    return std::format("ADDU {:s},{:s},{:s}",
                       getRegisterName(rd),
                       getRegisterName(rs),
                       getRegisterName(rt));
}

std::string Disassembler::JR() {
    // Jump Register
    // T: temp <- GPR[rs]
    // T+1: PC <- PC + target
    // Should be temp, right?
    uint8_t rs = 0x1F & (instruction >> 21);

    return std::format("JR {:s}",
                        getRegisterName(rs));
}

std::string Disassembler::JALR() {
    // Jump And Link Register
    // T: temp <- GPR[rs]
    //    GPR[rd] <- PC + 8
    // T+1: PC <- PC + target
    // Should be temp, right?
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rd = 0x1F & (instruction >> 11);

    return std::format("JALR {:s},{:s}",
                       getRegisterName(rd),
                       getRegisterName(rs));
}

std::string Disassembler::AND() {
    // And
    // T: GPR[rd] <- GPR[rs] and GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    return std::format("AND {:s},{:s},{:s}",
                       getRegisterName(rd),
                       getRegisterName(rs),
                       getRegisterName(rt));
}

std::string Disassembler::ADD() {
    // Add Word
    // T: GPR[rd] <- GPR[rs] + GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    return std::format("ADD {:s},{:s},{:s}",
                       getRegisterName(rd),
                       getRegisterName(rs),
                       getRegisterName(rt));
}

std::string Disassembler::SUBU() {
    // Subtract Unsigned Word
    // T: GPR[rd] <- GPR[rs] - GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    return std::format("SUBU {:s},{:s},{:s}",
                       getRegisterName(rd),
                       getRegisterName(rs),
                       getRegisterName(rt));
}

std::string Disassembler::SRA() {
    // Shift Word Right Arithmetic
    // T: GPR[rd] <- (GPR[rt]_{31})^{sa} || GPR[rt]_{31...sa}
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);
    uint8_t sa = 0x1F & (instruction >> 6);

    return std::format("SRA {:s},{:s},{:s}",
                       getRegisterName(rd),
                       getRegisterName(rt),
                       getRegisterName(sa));
}

std::string Disassembler::DIV() {
    // Divide Word
    // T-2: LO <- undefined
    //      HI <- undefined
    // T-1: LO <- undefined
    //      HI <- undefined
    // T:   LO <- GPR[rs] div GPR[rt]
    //      HI <- GPR[rs] mod GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);

    return std::format("DIV {:s},{:s}",
                       getRegisterName(rs),
                       getRegisterName(rt));
}

std::string Disassembler::MFLO() {
    // Move From Lo
    // T: GPR[rd] <- LO
    uint8_t rd = 0x1F & (instruction >> 11);

    return std::format("MFLO {:s}", getRegisterName(rd));
}

std::string Disassembler::SRL() {
    // Shift Word Right Logical
    // T: GPR[rd] <- 0^{sa} || GPR[rt]_{31...sa}
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);
    uint8_t sa = 0x1F & (instruction >> 6);

    return std::format("SRL {:s},{:s},{:s}",
                        getRegisterName(rd),
                        getRegisterName(rt),
                        getRegisterName(sa));
}

std::string Disassembler::DIVU() {
    // Divide Word Unsigned
    // T-2: LO <- undefined
    //      HI <- undefined
    // T-1: LO <- undefined
    //      HI <- undefined
    // T:   LO <- GPR[rs] div GPR[rt]
    //      HI <- GPR[rs] mod GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);

    return std::format("DIVU {:s},{:s}",
                        getRegisterName(rs),
                        getRegisterName(rt));
}

std::string Disassembler::MFHI() {
    // Move From Hi
    // T: GPR[rd] <- HI
    uint8_t rd = 0x1F & (instruction >> 11);

    return std::format("MFHI {:s}", getRegisterName(rd));
}

std::string Disassembler::SLT() {
    // Set on Less Than
    // T: if GPR[rs] < GPR[rt] then
    //     GPR[rd] <- 0^{31} || 1
    // else
    //     GPR[rd] <- 0^{32}
    // endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    return std::format("SLT {:s},{:s},{:s}",
                       getRegisterName(rd),
                       getRegisterName(rs),
                       getRegisterName(rt));
}

std::string Disassembler::SYSCALL() {
    // System Call
    // T: SystemCallException
    //uint32_t code = 0xFFFFF & (instruction >> 6);
    // the exception handler can access "code" by manually loading the instruction

    return "SYSCALL";
}

std::string Disassembler::MTLO() {
    // Move To Lo
    // T-2: LO <- undefined
    // T-1: LO <- undefined
    // T: LO <- GPR[rs]
    uint8_t rs = 0x1F & (instruction >> 21);

    return std::format("MTLO {:s}", getRegisterName(rs));
}

std::string Disassembler::MTHI() {
    // Move To HI
    // T-2: HI <- undefined
    // T-1: HI <- undefined
    // T: HI <- GPR[rs]
    uint8_t rs = 0x1F & (instruction >> 21);

    return std::format("MTHI {:s}", getRegisterName(rs));
}

std::string Disassembler::SLLV() {
    // Shift Word Left Logical Variable
    // s <- GP[rs]_{4...0}
    // GPR[rd] <- GPR[rt]_{(31-s)...0} || 0^s
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    return std::format("SLLV {:s},{:s},{:s}",
                        getRegisterName(rd),
                        getRegisterName(rt),
                        getRegisterName(rs));
}

std::string Disassembler::NOR() {
    // Nor
    // T: GPR[rd] <- GPR[rs] nor GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);
    return std::format("NOR {:s},{:s},{:s}",
                       getRegisterName(rd),
                       getRegisterName(rs),
                       getRegisterName(rt));
}

std::string Disassembler::SRAV() {
    // Shift Word Right Arithmetic Variable
    // T: s <- GPR[rs]_{4...0}
    //    GPR[rd] <- (GPR[rt]_{31})^{s} || GPR[rt]_{31...s}
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    return std::format("SRAV {:s},{:s},{:s}",
                        getRegisterName(rd),
                        getRegisterName(rt),
                        getRegisterName(rs));
}

std::string Disassembler::SRLV() {
    // Shift Word Right Logical Variable
    // T: s <- GPR[rs]_{4...0}
    //    GPR[rd] <- 0^{s} || GPR[rt]_{31...s}
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    return std::format("SRLV {:s},{:s},{:s}",
                        getRegisterName(rd),
                        getRegisterName(rt),
                        getRegisterName(rs));
}

std::string Disassembler::MULTU() {
    // Multiply Unsigned Word
    // T-2: LO <- undefined
    //      HI <- undefined
    // T-1: LO <- undefined
    //      HI <- undefined
    // T:   t <- (0 || GPR[rs]) * (0 || GPR[rt])
    //      LO <- t_{31...0}
    //      HI <- t_{63...32}
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);

    return std::format("MULTU {:s},{:s}",
                        getRegisterName(rs),
                        getRegisterName(rt));
}

std::string Disassembler::XOR() {
    // Exclusive Or
    // T: GPR[rd] <- GPR[rs] xor GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    return std::format("XOR {:s},{:s},{:s}",
                        getRegisterName(rd),
                        getRegisterName(rs),
                        getRegisterName(rt));
}

std::string Disassembler::UNKCP0() {
    return "UNKCP0";
}


std::string Disassembler::CP0MOVE() {
    // CP0 Move
    // Operation depends on function field
    move = 0x1F & (instruction >> 21);

    return (*cp0Move[move])();
}

std::string Disassembler::RFE() {
    // Restore From Exception
    // T: SR <- SR_{31...4} || SR_{5...2}

    return "RFE";
}

std::string Disassembler::UNKCP0M() {
    return "UNKCP0M";
}

std::string Disassembler::MTC0() {
    // Move To Coprocessor 0
    // T: data <- GPR[rt]
    // T+1: CPR[0,rd] <- data
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    return std::format("MTC0 {:s},{:d}",
                       getRegisterName(rt),
                       rd);
}

std::string Disassembler::MFC0() {
    // Move From Coprocessor 0
    // T: data <- CPR[0, rd]
    // T+1: GPR[rt] <- data
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    return std::format("MFC0 {:s},{:d}",
                       getRegisterName(rt),
                       rd);
}

std::string Disassembler::UNKCP2() {
    return "UNKCP2";
}

std::string Disassembler::CP2MOVE() {
    // CP2 Move
    // Operation depends on function field
    move = 0x1F & (instruction >> 21);

    return (*cp2Move[move])();
}

std::string Disassembler::UNKCP2M() {
    return "UNKCP2M";
}

std::string Disassembler::CTC2() {
    // Move Control To Coprocessor 2
    // T: data <- GPR[rt]
    // T+1: CCR[2,rd] <- data
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    return std::format("CTC2 {:s},{:d}",
                       getRegisterName(rt),
                       rd);
}

std::string Disassembler::MTC2() {
    // Move To Coprocessor 2
    // T: data <- GPR[rt]
    // T+1: CPR[2,rd] <- data
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    return std::format("MTC2 {:s},{:d}",
                       getRegisterName(rt),
                       rd);
}

std::string Disassembler::UNKRGMM() {
    return "UNKRGMM";
}

std::string Disassembler::BLTZ() {
    // Branch On Less Than Zero
    // T: target <- (offset_{15})^{14} || offset || 0^2
    //    condition <- (GPR[rs]_{31} = 1)
    //    GPR[31] <- PC + 8
    // T+1: if condition then
    //          PC <- PC + target
    //      endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint32_t offset = 0xFFFF & instruction;

    return std::format("BLTZ {:s},{:04X}",
                       getRegisterName(rs),
                       offset);
}

std::string Disassembler::BLTZAL() {
    // Branch On Less Than Zero And Link
    // T: target <- (offset_{15})^{14} || offset || 0^2
    //    condition <- (GPR[rs]_{31} = 1)
    //    GPR[31] <- PC + 8
    // T+1: if condition then
    //          PC <- PC + target
    //      endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint32_t offset = 0xFFFF & instruction;

    return std::format("BLTZAL {:s},{:04X}",
                        getRegisterName(rs),
                        offset);
}

std::string Disassembler::BGEZ() {
    // Branch On Greater Than Or Equal To Zero
    // T: target <- (offset_{15})^{14} || offset || 0^2
    //    condition <- (GPR[rs]_{31} = 0)
    // T+1: if condition then
    //          PC <- PC + target
    //      endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint32_t offset = 0xFFFF & instruction;

    return std::format("BGEZ {:s},{:04X}",
                        getRegisterName(rs),
                        offset);
}

}

