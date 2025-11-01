#include "cpu.h"

#include <cassert>
#include <format>
#include <iostream>
#include <sstream>

#include "bus.h"
#include "exceptions/exceptions.h"
#include "util/log.h"

using namespace util;

namespace PSX {

CPU::CPU(Bus *bus) {
    this->bus = bus;

    reset();
}

void CPU::reset() {
    regs.reset();
    cp0regs.reset();
    gte.reset();

    cycles = 0;

    instructionPC = 0;
    instruction = 0;
    isBranchDelaySlot = false;

    delaySlotPC = 0;
    delaySlot = 0;
    delaySlotIsBranchDelaySlot = false;
}

void CPU::step() {
    instructionPC = delaySlotPC;
    instruction = delaySlot;
    isBranchDelaySlot = delaySlotIsBranchDelaySlot;
    //if (instructionPC == 0x80030000) {
    //    Log::loggingEnabled = true;
    //}
    //if (instructionPC == 0x8004442C) {
    //    Log::loggingEnabled = true;
    //    Log::busLogEnabled = true;
    //}
    //if (instructionPC == 0xBFC01918) {
    //    std::cerr << regs << std::endl;
    //}

    // load delay-slot instruction from memory at program counter
    // and increase program counter
    fetchDelaySlot();

    // check if TTY output is being made
    interceptTTYOutput();

    // Executable sideloading
    if ((instructionPC & 0x1FFFFFFF) == 0x00030000 && bus->executable.loaded()) {
        LOG_EXE("Sideloading executable");
        bus->executable.writeToMemory();
    }

    // execute instruction
    LOGT_CPU(std::format("@0x{:08X}: ", instructionPC));
    opcode = instruction >> 26;
    assert (opcode <= 0b111111);
    (this->*opcodes[opcode])();

    cycles += 1;
}

void CPU::fetchDelaySlot() {
    // load delay-slot instruction from memory at program counter
    delaySlotPC = regs.getPC();
    delaySlot = bus->readWord(delaySlotPC);
    delaySlotIsBranchDelaySlot = false; // this will be set to true be branch instructions

    // increase program counter
    // by increasing it before executing the instruction,
    // it may be overwritten by the instruction
    regs.setPC(delaySlotPC + 4);
}

void CPU::interceptTTYOutput() {
    uint32_t pc = instructionPC & 0x01FFFFFF;
    if ((pc == 0xA0 && regs.getRegister(9) == 0x3C)
        || (pc == 0xB0 && regs.getRegister(9) == 0x3D)) {
        char c = 0xFF & regs.getRegister(4);
        if (c != '\n') {
            ttyOutput << (char)c;
        } else {
            LOG_TTY(ttyOutput.str());
            ttyOutput.str(std::string());
        }
    }
}

void CPU::generateException(uint8_t exccode) {
    LOG_EXC(std::format("Exception @0x{:08X} with code {:d}", instructionPC, exccode));

    // make EPC point to restart location
    // the EPC has to point to the instruction which caused the error
    // if the instruction is in a branch delay slot, then
    // it has to point to the preceding branch instruction
    // and signal this via the BD bit
    if (isBranchDelaySlot) {
        cp0regs.setCP0Register(CP0_REGISTER_EPC, instructionPC - 4);
        // set BD bit
        uint32_t cause = cp0regs.getCP0Register(CP0_REGISTER_CAUSE);
        cp0regs.setCP0Register(CP0_REGISTER_CAUSE, cause | (0x1 << CAUSE_BIT_BD));

    } else {
        cp0regs.setCP0Register(CP0_REGISTER_EPC, instructionPC);
    }

    // save user-mode-enable and interrupt-enable flags in SR
    // by pushing the 3-entry stack inside of SR
    uint32_t sr = cp0regs.getCP0Register(CP0_REGISTER_SR);
    // we clear KUc and IEc, not sure if this is correct
    cp0regs.setCP0Register(CP0_REGISTER_SR, (sr & 0xFFFFFFC0) | ((sr & 0x3F) << 2));

    // set up ExcCode in Cause register
    uint32_t cause = cp0regs.getCP0Register(CP0_REGISTER_CAUSE);
    cause = (cause & 0xFFFFFF83) | (exccode << 2);
    cp0regs.setCP0Register(CP0_REGISTER_CAUSE, cause);

    // TODO: set BadVaddr on address exception

    // transfer control to exception entry point
    if (cp0regs.getCP0Register(CP0_REGISTER_SR) & (1 << SR_BIT_BEV)) {
        regs.setPC(0xBFC00180);

    } else {
        regs.setPC(0x80000080);
    }

    // fetch instruction at new program counter into the delay slot
    // in order to avoid executing the instruction in the delay slot
    // before handling the exception
    fetchDelaySlot();
}

void CPU::checkAndExecuteInterrupts() {
    LOGV_EXC(std::format("Checking if interrupt exception should be issued"));

    if (cp0regs.getBit(CP0_REGISTER_SR, SR_BIT_IEC)) {
        uint32_t ip = (cp0regs.getCP0Register(CP0_REGISTER_CAUSE) >> CAUSE_BIT_IP0) & 0xFF;
        uint32_t im = (cp0regs.getCP0Register(CP0_REGISTER_SR) >> SR_BIT_IM0) & 0xFF;

        if (ip & im) {
            generateException(EXCCODE_INT);

        } else {
            LOGV_EXC(std::format("IEc set but no interrupt enabled and issued"));
        }
    } else {
        LOGV_EXC(std::format("IEc not set"));
    }
}

uint32_t CPU::getDelaySlotPC() const {
    return delaySlotPC;
}

std::ostream& operator<<(std::ostream &os, const CPU &cpu) {
    os << "Register contents: " << std::endl;
    os << cpu.regs << std::endl;

    os << "CP0 register contents: " << std::endl;
    os << cpu.cp0regs;

    return os;
}

const CPU::Opcode CPU::opcodes[] = {
    // 0b000000
    &CPU::SPECIAL,  &CPU::REGIMM,   &CPU::J,        &CPU::JAL,
    // 0b000100
    &CPU::BEQ,      &CPU::BNE,      &CPU::BLEZ,     &CPU::BGTZ,
    // 0b001000
    &CPU::ADDI,     &CPU::ADDIU,    &CPU::SLTI,     &CPU::SLTIU,
    // 0b001100
    &CPU::ANDI,     &CPU::ORI,      &CPU::XORI,     &CPU::LUI,
    // 0b010000
    &CPU::CP0,      &CPU::UNK,      &CPU::CP2,      &CPU::UNK,
    // 0b010100
    &CPU::UNK,      &CPU::UNK,      &CPU::UNK,      &CPU::UNK,
    // 0b011000
    &CPU::UNK,      &CPU::UNK,      &CPU::UNK,      &CPU::UNK,
    // 0b011100
    &CPU::UNK,      &CPU::UNK,      &CPU::UNK,      &CPU::UNK,
    // 0b100000
    &CPU::LB,       &CPU::LH,       &CPU::LWL,      &CPU::LW,
    // 0b100100
    &CPU::LBU,      &CPU::LHU,      &CPU::LWR,      &CPU::UNK,
    // 0b101000
    &CPU::SB,       &CPU::SH,       &CPU::SWL,      &CPU::SW,
    // 0b101100
    &CPU::UNK,      &CPU::UNK,      &CPU::SWR,      &CPU::UNK,
    // 0b110000
    &CPU::UNK,      &CPU::UNK,      &CPU::UNK,      &CPU::UNK,
    // 0b110100
    &CPU::UNK,      &CPU::UNK,      &CPU::UNK,      &CPU::UNK,
    // 0b111000
    &CPU::UNK,      &CPU::UNK,      &CPU::UNK,      &CPU::UNK,
    // 0b111100
    &CPU::UNK,      &CPU::UNK,      &CPU::UNK,      &CPU::UNK
};

const CPU::Opcode CPU::special[] = {
    // 0b000000
    &CPU::SLL,      &CPU::UNKSPCL,  &CPU::SRL,      &CPU::SRA,
    // 0b000100
    &CPU::SLLV,     &CPU::UNKSPCL,  &CPU::SRLV,     &CPU::SRAV,
    // 0b001000
    &CPU::JR,       &CPU::JALR,     &CPU::UNKSPCL,  &CPU::UNKSPCL,
    // 0b001100
    &CPU::SYSCALL,  &CPU::UNKSPCL,  &CPU::UNKSPCL,  &CPU::UNKSPCL,
    // 0b010000
    &CPU::MFHI,     &CPU::MTHI,     &CPU::MFLO,     &CPU::MTLO,
    // 0b010100
    &CPU::UNKSPCL,  &CPU::UNKSPCL,  &CPU::UNKSPCL,  &CPU::UNKSPCL,
    // 0b011000
    &CPU::MULT,     &CPU::MULTU,    &CPU::DIV,      &CPU::DIVU,
    // 0b011100
    &CPU::UNKSPCL,  &CPU::UNKSPCL,  &CPU::UNKSPCL,  &CPU::UNKSPCL,
    // 0b100000
    &CPU::ADD,      &CPU::ADDU,     &CPU::SUB,      &CPU::SUBU,
    // 0b100100
    &CPU::AND,      &CPU::OR,       &CPU::XOR,      &CPU::NOR,
    // 0b101000
    &CPU::UNKSPCL,  &CPU::UNKSPCL,  &CPU::SLT,      &CPU::SLTU,
    // 0b101100
    &CPU::UNKSPCL,  &CPU::UNKSPCL,  &CPU::UNKSPCL,  &CPU::UNKSPCL,
    // 0b110000
    &CPU::UNKSPCL,  &CPU::UNKSPCL,  &CPU::UNKSPCL,  &CPU::UNKSPCL,
    // 0b110100
    &CPU::UNKSPCL,  &CPU::UNKSPCL,  &CPU::UNKSPCL,  &CPU::UNKSPCL,
    // 0b111000
    &CPU::UNKSPCL,  &CPU::UNKSPCL,  &CPU::UNKSPCL,  &CPU::UNKSPCL,
    // 0b111100
    &CPU::UNKSPCL,  &CPU::UNKSPCL,  &CPU::UNKSPCL,  &CPU::UNKSPCL
};

const CPU::Opcode CPU::cp0[] = {
    // 0b000000
    &CPU::CP0MOVE,  &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,
    // 0b000100
    &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,
    // 0b001000
    &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,
    // 0b001100
    &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,
    // 0b010000
    &CPU::RFE,      &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,
    // 0b010100
    &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,
    // 0b011000
    &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,
    // 0b011100
    &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,
    // 0b100000
    &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,
    // 0b100100
    &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,
    // 0b101000
    &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,
    // 0b101100
    &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,
    // 0b110000
    &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,
    // 0b110100
    &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,
    // 0b111000
    &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,
    // 0b111100
    &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0,   &CPU::UNKCP0
};

const CPU::Opcode CPU::cp0Move[] = {
    // 0b00000
    &CPU::MFC0,     &CPU::UNKCP0M,  &CPU::UNKCP0M,  &CPU::UNKCP0M,
    // 0b00100
    &CPU::MTC0,     &CPU::UNKCP0M,  &CPU::UNKCP0M,  &CPU::UNKCP0M,
    // 0b01000
    &CPU::UNKCP0M,  &CPU::UNKCP0M,  &CPU::UNKCP0M,  &CPU::UNKCP0M,
    // 0b01100
    &CPU::UNKCP0M,  &CPU::UNKCP0M,  &CPU::UNKCP0M,  &CPU::UNKCP0M,
    // 0b10000
    &CPU::UNKCP0M,  &CPU::UNKCP0M,  &CPU::UNKCP0M,  &CPU::UNKCP0M,
    // 0b10100
    &CPU::UNKCP0M,  &CPU::UNKCP0M,  &CPU::UNKCP0M,  &CPU::UNKCP0M,
    // 0b11000
    &CPU::UNKCP0M,  &CPU::UNKCP0M,  &CPU::UNKCP0M,  &CPU::UNKCP0M,
    // 0b11100
    &CPU::UNKCP0M,  &CPU::UNKCP0M,  &CPU::UNKCP0M,  &CPU::UNKCP0M
};

const CPU::Opcode CPU::cp2[] = {
    // 0b000000
    &CPU::CP2MOVE,  &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,
    // 0b000100
    &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,
    // 0b001000
    &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,
    // 0b001100
    &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,
    // 0b010000
    &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,
    // 0b010100
    &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,
    // 0b011000
    &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,
    // 0b011100
    &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,
    // 0b100000
    &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,
    // 0b100100
    &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,
    // 0b101000
    &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,
    // 0b101100
    &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,
    // 0b110000
    &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,
    // 0b110100
    &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,
    // 0b111000
    &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,
    // 0b111100
    &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2,   &CPU::UNKCP2
};

const CPU::Opcode CPU::cp2Move[] = {
    // 0b00000
    &CPU::UNKCP2M,  &CPU::UNKCP2M,  &CPU::UNKCP2M,  &CPU::UNKCP2M,
    // 0b00100
    &CPU::MTC2,     &CPU::UNKCP2M,  &CPU::CTC2,     &CPU::UNKCP2M,
    // 0b01000
    &CPU::UNKCP2M,  &CPU::UNKCP2M,  &CPU::UNKCP2M,  &CPU::UNKCP2M,
    // 0b01100
    &CPU::UNKCP2M,  &CPU::UNKCP2M,  &CPU::UNKCP2M,  &CPU::UNKCP2M,
    // 0b10000
    &CPU::UNKCP2M,  &CPU::UNKCP2M,  &CPU::UNKCP2M,  &CPU::UNKCP2M,
    // 0b10100
    &CPU::UNKCP2M,  &CPU::UNKCP2M,  &CPU::UNKCP2M,  &CPU::UNKCP2M,
    // 0b11000
    &CPU::UNKCP2M,  &CPU::UNKCP2M,  &CPU::UNKCP2M,  &CPU::UNKCP2M,
    // 0b11100
    &CPU::UNKCP2M,  &CPU::UNKCP2M,  &CPU::UNKCP2M,  &CPU::UNKCP2M
};

const CPU::Opcode CPU::regimm[] = {
    // 0b00000
    &CPU::BLTZ,     &CPU::BGEZ,     &CPU::UNKRGMM,  &CPU::UNKRGMM,
    // 0b00100
    &CPU::UNKRGMM,  &CPU::UNKRGMM,  &CPU::UNKRGMM,  &CPU::UNKRGMM,
    // 0b01000
    &CPU::UNKRGMM,  &CPU::UNKRGMM,  &CPU::UNKRGMM,  &CPU::UNKRGMM,
    // 0b01100
    &CPU::UNKRGMM,  &CPU::UNKRGMM,  &CPU::UNKRGMM,  &CPU::UNKRGMM,
    // 0b10000
    &CPU::UNKRGMM,  &CPU::UNKRGMM,  &CPU::UNKRGMM,  &CPU::UNKRGMM,
    // 0b10100
    &CPU::UNKRGMM,  &CPU::UNKRGMM,  &CPU::UNKRGMM,  &CPU::UNKRGMM,
    // 0b11000
    &CPU::UNKRGMM,  &CPU::UNKRGMM,  &CPU::UNKRGMM,  &CPU::UNKRGMM,
    // 0b11100
    &CPU::UNKRGMM,  &CPU::UNKRGMM,  &CPU::UNKRGMM,  &CPU::UNKRGMM
};

void CPU::UNK() {
    throw exceptions::UnknownOpcodeError(std::format("Unknown opcode @0x{:x}: opcode 0b{:06b}", instructionPC, opcode));
}

void CPU::SPECIAL() {
    // SPECIAL
    // Operation depends on function field
    funct = 0x3F & instruction;

    (this->*special[funct])();
}

void CPU::CP0() {
    // CP0
    // Operation depends on function field
    funct = 0x3F & instruction;

    (this->*cp0[funct])();
}

void CPU::CP2() {
    // CP2
    // Operation depends on function field
    funct = 0x3F & instruction;

    (this->*cp2[funct])();
}

void CPU::REGIMM() {
    // REGIMM
    // Operation depends on rt field
    instructionRt = 0x1F & (instruction >> 16);

    (this->*regimm[instructionRt])();
}

void CPU::LUI() {
    // Load Upper Immediate
    // GPR[rt] <- immediate || 0^{16}
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t immediate = 0xFFFF & instruction;

    LOGT_CPU(std::format("LUI {:s},0x{:04X}",
                         regs.getRegisterName(rt),
                         immediate));

    uint32_t data = immediate << 16;

    regs.setRegister(rt, data);
}

void CPU::ORI() {
    // Or Immediate
    // T: GPR[rt] <- GPR[rs]_{31...36} || (immedate or GPR[rs]_{15...0})
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t immediate = 0xFFFF & instruction;
    LOGT_CPU(std::format("ORI {:s},{:s},0x{:04x}",
                        regs.getRegisterName(rt),
                        regs.getRegisterName(rs),
                        immediate));

    regs.setRegister(rt, regs.getRegister(rs) | immediate);
}

void CPU::SW() {
    // Store Word
    // T: vAddr <- ((offset_{15})^{16} | offset_{15...0}) + GPR[base]
    // (pAddr, uncached) <- AddressTranslation(vAddr, DATA)
    // data <- GPR[rt]
    // StoreMemory(uncached, WORD, data, pAddr, vAddr, DATA)
    uint8_t base = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    LOGT_CPU(std::format("SW {:s},0x{:04X}({:s})",
                        regs.getRegisterName(rt),
                        offset,
                        regs.getRegisterName(base)));

    uint32_t vAddr = (((offset >> 15) ? 0xFFFF0000 : 0x00000000) | offset)
                     + regs.getRegister(base);
    uint32_t data = regs.getRegister(rt);
    bus->writeWord(vAddr, data);
    if (vAddr & 0x3) {
        throw exceptions::ExceptionNotImplemented("Address Error");
    }
}

void CPU::ADDIU() {
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

    LOGT_CPU(std::format("ADDIU {:s},{:s},0x{:04X}",
                        regs.getRegisterName(rt),
                        regs.getRegisterName(rs),
                        immediate));

    regs.setRegister(rt, regs.getRegister(rs) + signExtension);
}

void CPU::J() {
    // Jump
    // T: temp <- target
    // T+1: pc <- pc_{31...28} || temp || 0^2
    uint32_t target = 0x3FFFFFF & instruction;
    LOGT_CPU(std::format("J 0x{:08X}", target));

    uint32_t actualTarget = (delaySlotPC & 0xF0000000) | (target << 2);

    regs.setPC(actualTarget);
    delaySlotIsBranchDelaySlot = true;
}

void CPU::BNE() {
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
    LOGT_CPU(std::format("BNE {:s},{:s},0x{:04X} (+0x{:08X}, -> @0x{:08X})",
                        regs.getRegisterName(rt),
                        regs.getRegisterName(rs),
                        offset,
                        target,
                        actualTarget));

    if (regs.getRegister(rs) != regs.getRegister(rt)) {
        regs.setPC(actualTarget);
        delaySlotIsBranchDelaySlot = true;
    }
}

void CPU::ADDI() {
    // Add Immediate Word
    // T: GPR[rt] <- GPR[rs] + (immediate_{15})^{16} || immediate_{15...0}
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t immediate = 0xFFFF & instruction;
    LOGT_CPU(std::format("ADDI {:s},{:s},0x{:04X}",
                        regs.getRegisterName(rt),
                        regs.getRegisterName(rs),
                        immediate));

    uint32_t rsValue = regs.getRegister(rs);
    uint32_t signExtension = ((immediate >> 15) ? 0xFFFF0000 : 0x00000000) + immediate;
    bool carry30 = ((0x7FFFFFFF & rsValue) + (0x7FFFFFFF & signExtension)) & 0x80000000;
    bool carry31 = ((rsValue >> 31) + (signExtension >> 31) + (carry30 ? 1 : 0) >= 2);

    if (carry30 == carry31) {
        regs.setRegister(rt, rsValue + signExtension);

    } else {
        throw exceptions::ExceptionNotImplemented("Integer Overflow");
    }
}

void CPU::LW() {
    // Load Word
    // T: vAddr <- ((offset_{15})^{16} | offset_{15...0}) + GPR[base]
    // (pAddr, uncached) <- AddressTranslation(vAddr, DATA)
    // mem <- LoadMemory(uncached, WORD, pAddr, vAddr, DATA)
    // T+1: GPR[rt] <- mem
    uint8_t base = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    LOGT_CPU(std::format("LW {:s},0x{:04X}({:s})",
                        regs.getRegisterName(rt),
                        offset,
                        regs.getRegisterName(base)));

    uint32_t vAddr = (((offset >> 15) ? 0xFFFF0000 : 0x00000000) | offset) + regs.getRegister(base);
    uint32_t data = bus->readWord(vAddr);
    regs.setRegister(rt, data);

if (vAddr & 0x3) {
    throw exceptions::ExceptionNotImplemented("Address Error");
    }
}

void CPU::SH() {
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

    LOGT_CPU(std::format("SH {:s},0x{:08X}({:s})",
                        regs.getRegisterName(rt),
                        offset,
                        regs.getRegisterName(base)));

    uint32_t vAddr = (((offset >> 15) ? 0xFFFF0000 : 0x0000) | offset) + regs.getRegister(base);
    uint16_t data = (uint16_t)(0x0000FFFF & regs.getRegister(rt));
    bus->writeHalfWord(vAddr, data);

    if (vAddr & 0x1) {
        throw exceptions::ExceptionNotImplemented("Address Error");
    }
}

void CPU::JAL() {
    // Jump And Link
    // T: temp <- target
    //    GPR[31] <- PC + 8
    // T+1: PC <- PC_{31...28} || temp || 0^2
    uint32_t target = 0x03FFFFFF & instruction;

    LOGT_CPU(std::format("JAL 0x{:06X}", target));

    uint32_t actualTarget = (delaySlotPC &  0xF0000000) | (target << 2);
    uint32_t newPC = instructionPC + 8;

    regs.setRegister(31, newPC);
    regs.setPC(actualTarget);
    delaySlotIsBranchDelaySlot = true;
}

void CPU::ANDI() {
    // And Immediate
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t immediate = 0xFFFF & instruction;

    LOGT_CPU(std::format("ANDI {:s},{:s},0x{:04X}",
                        regs.getRegisterName(rt),
                        regs.getRegisterName(rs),
                        immediate));

    regs.setRegister(rt, immediate & regs.getRegister(rs));
}

void CPU::SB() {
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

    LOGT_CPU(std::format("SB {:s},0x{:04X}({:s})",
                        regs.getRegisterName(rt),
                        offset,
                        regs.getRegisterName(base)));

    uint32_t vAddr = (((offset >> 15) ? 0xFFFF0000 : 0x0000) | offset) + regs.getRegister(base);
    uint8_t data = (uint8_t)(0x000000FF & regs.getRegister(rt));
    bus->writeByte(vAddr, data);
}

void CPU::LB() {
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

    LOGT_CPU(std::format("LB {:s},0x{:04X}({:s})",
                        regs.getRegisterName(rt),
                        offset,
                        regs.getRegisterName(base)));

    uint32_t vAddr = (((offset >> 15) ? 0xFFFF0000 : 0x0000) | offset) + regs.getRegister(base);
    uint8_t mem = bus->readByte(vAddr);
    uint32_t signExtension = ((mem >> 7) ? 0xFFFFFF00 : 0x00000000) + mem;
    regs.setRegister(rt, signExtension);
}

void CPU::BEQ() {
    // Branch On Equal
    // T: target <- (offset_{15})^{14} || offset || 0^2
    //    condition <- (GPR[rs] = GPR[rt])
    // T+1: if condition then
    //          PC <- PC + target
    //      endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    LOGT_CPU(std::format("BEQ {:s},{:s},{:04X}",
                        regs.getRegisterName(rs),
                        regs.getRegisterName(rt),
                        offset));

    uint32_t signExtension = ((offset >> 15) ? 0xFFFF0000 : 0x00000000) + offset;
    uint32_t target = signExtension << 2;
    uint32_t actualTarget = delaySlotPC + target;

    uint32_t rsValue = regs.getRegister(rs);
    uint32_t rtValue = regs.getRegister(rt);
    LOGT_CPU(std::format(" (0x{:08X} == 0x{:08X}? -0x{:08X}-> pc)",
                        rsValue, rtValue, actualTarget));

    if (rsValue == rtValue) {
        regs.setPC(actualTarget);
        delaySlotIsBranchDelaySlot = true;
    }
}

void CPU::BGTZ() {
    // Branch On Greater Than Zero
    // T: target <- (offset_{15})^{14} || offset || 0^2
    //    condition <- (GPR[rs]_{31} = 0 and GPR[rs] != 0^{32})
    // T+1: if condition then
    //          PC <- PC + target
    //      endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint32_t offset = 0xFFFF & instruction;

    LOGT_CPU(std::format("BGTZ {:s},{:04X}",
                        regs.getRegisterName(rs),
                        offset));

    uint32_t signExtension = ((offset >> 15) ? 0xFFFF0000 : 0x00000000) + offset;
    uint32_t target = signExtension << 2;
    uint32_t actualTarget = delaySlotPC + target;

    uint32_t rsValue = regs.getRegister(rs);
    LOGT_CPU(std::format(" (0x{:08X} > 0? -0x{:08X}-> pc)",
                        rsValue, actualTarget));

    if (!(rsValue >> 31) && (rsValue != 0)) {
        regs.setPC(actualTarget);
        delaySlotIsBranchDelaySlot = true;
    }
}

void CPU::BLEZ() {
    // Branch On Less Than Or Equal To Zero
    // T: target <- (offset_{15})^{14} || offset || 0^2
    //    condition <- (GPR[rs]_{31} = 1) or (GPR[rs] == 0^{32})
    // T+1: if condition then
    //          PC <- PC + target
    //      endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint32_t offset = 0xFFFF & instruction;

    LOGT_CPU(std::format("BLEZ {:s},{:04X}",
                        regs.getRegisterName(rs),
                        offset));

    uint32_t signExtension = ((offset >> 15) ? 0xFFFF0000 : 0x00000000) + offset;
    uint32_t target = signExtension << 2;
    uint32_t actualTarget = delaySlotPC + target;

    uint32_t rsValue = regs.getRegister(rs);
    LOGT_CPU(std::format(" (0x{:08X} <= 0? -0x{:08X}-> pc)",
                        rsValue, actualTarget));

    if ((rsValue >> 31) || (rsValue == 0)) {
        regs.setPC(actualTarget);
        delaySlotIsBranchDelaySlot = true;
    }
}

void CPU::LBU() {
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

    LOGT_CPU(std::format("LBU {:s},0x{:04X}({:s})",
                        regs.getRegisterName(rt),
                        offset,
                        regs.getRegisterName(base)));

    uint32_t vAddr = (((offset >> 15) ? 0xFFFF0000 : 0x0000) | offset) + regs.getRegister(base);
    uint8_t mem = bus->readByte(vAddr);
    uint32_t zeroExtension = mem;
    regs.setRegister(rt, zeroExtension);
}

void CPU::SLTI() {
    // Set On Less Than Immediate
    // T: if GPR[rs] < (immediate_{15})^{16} || immediate_{15...0} then
    //        GPR[rt] <- 0^{31} || 1
    //    else
    //        GPR[rt] <- 0^{32}
    //    endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t immediate = 0xFFFF & instruction;

    LOGT_CPU(std::format("SLTI {:s},{:s},0x{:04x}",
                        regs.getRegisterName(rt),
                        regs.getRegisterName(rs),
                        immediate));

    uint32_t rsValue = regs.getRegister(rs);
    uint32_t signExtension = ((immediate >> 15) ? 0xFFFF0000 : 0x00000000) + immediate;

    int32_t rsValueSigned = (int32_t)rsValue;
    int32_t signExtensionSigned = (int32_t)signExtension;

    if (rsValueSigned < signExtensionSigned) {
        regs.setRegister(rt, 1);

    } else {
        regs.setRegister(rt, 0);
    }
}

void CPU::SLTIU() {
    // Set On Less Than Immediate Unsigned
    // T: if (0 || GPR[rs]) < (immediate_{15})^{16} || immediate_{15...0} then
    //        GPR[rt] <- 0^{31} || 1
    //    else
    //        GPR[rt] <- 0^{32}
    //    endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t immediate = 0xFFFF & instruction;

    LOGT_CPU(std::format("SLTI {:s},{:s},0x{:04x}",
                        regs.getRegisterName(rt),
                        regs.getRegisterName(rs),
                        immediate));

    uint32_t rsValue = regs.getRegister(rs);
    uint32_t signExtension = ((immediate >> 15) ? 0xFFFF0000 : 0x00000000) + immediate;

    if (rsValue < signExtension) {
        regs.setRegister(rt, 1);

    } else {
        regs.setRegister(rt, 0);
    }
}

void CPU::LHU() {
    // Load Halfword Unsigned
    // T: vAddr <- ((offset_{15})^{16} | offset_{15...0}) + GPR[base]
    // (pAddr, uncached) <- AddressTranslation(vAddr, DATA)
    // pAddr <- pAddr_{PSIZE-1...2} || (pAddr_{1...0} xor (ReverseEndian || 0)
    // mem <- LoadMemory(uncached, HALFWORD, pAddr, vAddr, DATA)
    // byte <- vAddr_{1...0} xor (BigEndianCPU || 0)
    // T+1: GPR[rt] <- 0^{16} || mem_{15+8*byte...8*byte}
    uint8_t base = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    LOGT_CPU(std::format("LHU {:s},0x{:04X}({:s})",
                        regs.getRegisterName(rt),
                        offset,
                        regs.getRegisterName(base)));

    uint32_t vAddr = (((offset >> 15) ? 0xFFFF0000 : 0x0000) | offset) + regs.getRegister(base);
    uint16_t mem = bus->readHalfWord(vAddr);
    uint32_t zeroExtension = mem;
    regs.setRegister(rt, zeroExtension);
}

void CPU::LH() {
    // Load Halfword
    // T: vAddr <- ((offset_{15})^{16} | offset_{15...0}) + GPR[base]
    // (pAddr, uncached) <- AddressTranslation(vAddr, DATA)
    // pAddr <- pAddr_{PSIZE-1...2} || (pAddr_{1...0} xor (ReverseEndian || 0))
    // mem <- LoadMemory(uncached, HALFWORD, pAddr, vAddr, DATA)
    // byte <- vAddr_{1...0} xor (BigEndianCPU || 0)
    // T+1: GPR[rt] <- (mem_{15+8*byte)^{16} || mem_{15+8*byte...8*byte}
    uint8_t base = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    LOGT_CPU(std::format("LH {:s},0x{:04X}({:s})",
                        regs.getRegisterName(rt),
                        offset,
                        regs.getRegisterName(base)));

    uint32_t vAddr = (((offset >> 15) ? 0xFFFF0000 : 0x0000) | offset) + regs.getRegister(base);
    uint16_t mem = bus->readHalfWord(vAddr);
    uint32_t signExtension = ((mem >> 15) ? 0xFFFF0000 : 0x00000000) + mem;
    regs.setRegister(rt, signExtension);
}

void CPU::LWL() {
    // Load Word Left
    // T: vAddr <- ((offset_{15})^{16} | offset_{15...0}) + GPR[base]
    // (pAddr, uncached) <- AddressTranslation(vAddr, DATA)
    // pAddr <- pAddr_{PSIZE-1...2} || (pAddr_{1...0} xor ReverseEndian^2)
    // if BigEndianMem = 0 then
    //    pAddr <- pAddr_{PSIZE-1...2} || 0^2
    // endif
    // byte <- vAddr_{1...0} xor BigEndianCPU^2
    // mem <- LoadMemory(uncached, byte, pAddr, vAddr, DATA)
    // GPR[rt] <- mem_{7+8*byte...0} || GPR[rt]_{23-8*byte...0}
    uint8_t base = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    LOGT_CPU(std::format("LWL {:s},0x{:04X}({:s})",
                        regs.getRegisterName(rt),
                        offset,
                        regs.getRegisterName(base)));

    uint32_t vAddr = (((offset >> 15) ? 0xFFFF0000 : 0x00000000) | offset)
                     + regs.getRegister(base);
    uint32_t mem = bus->readWord(vAddr & 0xFFFFFFFC);

    uint8_t shiftBy = (24 - 8 * (vAddr & 3));
    regs.setRegister(rt, (mem << shiftBy)
                         | (regs.getRegister(rt) & ~(0xFFFFFFFF << shiftBy)));
}

void CPU::LWR() {
    // Load Word Right
    // T: vAddr <- ((offset_{15})^{16} | offset_{15...0}) + GPR[base]
    // (pAddr, uncached) <- AddressTranslation(vAddr, DATA)
    // pAddr <- pAddr_{PSIZE-1...2} || (pAddr_{1...0} xor ReverseEndian^2)
    // if BigEndianMem = 0 then
    //    pAddr <- pAddr_{PSIZE-1...2} || 0^2
    // endif
    // byte <- vAddr_{1...0} xor BigEndianCPU^2
    // mem <- LoadMemory(uncached, byte, pAddr, vAddr, DATA)
    // GPR[rt] <- mem_{31...32-8*byte} || GPR[rt]_{31-8*byte...0}
    uint8_t base = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    LOGT_CPU(std::format("LWR {:s},0x{:04X}({:s})",
                        regs.getRegisterName(rt),
                        offset,
                        regs.getRegisterName(base)));

    uint32_t vAddr = (((offset >> 15) ? 0xFFFF0000 : 0x00000000) | offset)
                     + regs.getRegister(base);
    uint32_t mem = bus->readWord(vAddr & 0xFFFFFFFC);

    uint8_t shiftBy = 8 * (vAddr & 3);
    regs.setRegister(rt, (mem >> shiftBy)
                         | (regs.getRegister(rt) & ~(0xFFFFFFFF >> shiftBy)));
}

void CPU::SWL() {
    // Store Word Left
    // T: vAddr <- ((offset_{15})^{16} | offset_{15...0}) + GPR[base]
    // (pAddr, uncached) <- AddressTranslation(vAddr, DATA)
    // pAddr <- pAddr_{PSIZE-1...2} || (pAddr_{1...0} xor ReverseEndian^2)
    // if BigEndianMem = 0 then
    //    pAddr <- pAddr_{PSIZE-1...2} || 0^2
    // endif
    // byte <- vAddr_{1...0} xor BigEndianCPU^2
    // data <- 0^{24-8*byte} || GPR[rt]_{31...24-8*byte}
    // StoreMemory(uncached, byte, data, pAddr, vAddr, DATA)
    uint8_t base = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    LOGT_CPU(std::format("SWL {:s},0x{:04X}({:s})",
                        regs.getRegisterName(rt),
                        offset,
                        regs.getRegisterName(base)));

    uint32_t vAddr = (((offset >> 15) ? 0xFFFF0000 : 0x00000000) | offset)
                     + regs.getRegister(base);

    uint8_t shiftBy = (24 - 8 * (vAddr & 3));
    bus->writeWord(vAddr & 0xFFFFFFFC,
                   (regs.getRegister(rt) >> shiftBy)
                   | (bus->readWord(vAddr & 0xFFFFFFFC) & ~(0xFFFFFFFF >> shiftBy)));
}

void CPU::SWR() {
    // Store Word Right
    // T: vAddr <- ((offset_{15})^{16} | offset_{15...0}) + GPR[base]
    // (pAddr, uncached) <- AddressTranslation(vAddr, DATA)
    // pAddr <- pAddr_{PSIZE-1...2} || (pAddr_{1...0} xor ReverseEndian^2)
    // if BigEndianMem = 0 then
    //    pAddr <- pAddr_{PSIZE-1...2} || 0^2
    // endif
    // byte <- vAddr_{1...0} xor BigEndianCPU^2
    // data <- GPR[rt]_{31-8*byte} || 0^{8*byte}
    // StoreMemory(uncached, WORD - byte, data, pAddr, vAddr, DATA)
    uint8_t base = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t offset = 0xFFFF & instruction;

    LOGT_CPU(std::format("SWR {:s},0x{:04X}({:s})",
                        regs.getRegisterName(rt),
                        offset,
                        regs.getRegisterName(base)));

    uint32_t vAddr = (((offset >> 15) ? 0xFFFF0000 : 0x00000000) | offset)
                     + regs.getRegister(base);

    uint8_t shiftBy = 8 * (vAddr & 3);
    bus->writeWord(vAddr & 0xFFFFFFFC,
                   (regs.getRegister(rt) << shiftBy)
                   | (bus->readWord(vAddr & 0xFFFFFFFC) & ~(0xFFFFFFFF << shiftBy)));
}

void CPU::XORI() {
    // Exclusive OR Immediate
    // T: GPR[rt] <- GPR[rs] xor (0^{16} || immediate)
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint32_t immediate = 0xFFFF & instruction;

    LOGT_CPU(std::format("XORI {:s},{:s},0x{:04X}",
                        regs.getRegisterName(rt),
                        regs.getRegisterName(rs),
                        immediate));

    regs.setRegister(rt, immediate ^ regs.getRegister(rs));
}

void CPU::UNKSPCL() {
    throw exceptions::UnknownFunctionError(std::format("Unknown function @0x{:x}: instruction 0x{:x} (SPECIAL), function 0b{:06b}", instructionPC, instruction, funct));
}

void CPU::SLL() {
    // Shift Word Left Logical
    // T: GPR[rd] <- GPR[rt]_{31 - sa...0} || 0^{sa}
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);
    uint8_t sa = 0x1F & (instruction >> 6);

    if (rt == 0 && rd == 0 && sa == 0) {
        LOGT_CPU(std::format("NOP"));
    } else {
        LOGT_CPU(std::format("SLL {:s},{:s},{:s}",
                            regs.getRegisterName(rd),
                            regs.getRegisterName(rt),
                            regs.getRegisterName(sa)));
    }

    regs.setRegister(rd, regs.getRegister(rt) << sa);
}

void CPU::OR() {
    // Or
    // T: GPR[rd] <- GPR[rs] or GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);
    LOGT_CPU(std::format("OR {:s},{:s},{:s}",
                        regs.getRegisterName(rd),
                        regs.getRegisterName(rs),
                        regs.getRegisterName(rt)));

    regs.setRegister(rd, regs.getRegister(rs) | regs.getRegister(rt));
}

void CPU::SLTU() {
    // Set on Less Than Unsigned
    // T: if (0 || GPR[rs]) < 0 || GPR[rt] then
    //     GPR[rd] <- 0^{31} || 1
    // else
    //     GPR[rd] <- 0^{32}
    // endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    LOGT_CPU(std::format("SLTU {:s},{:s},{:s}",
                        regs.getRegisterName(rd),
                        regs.getRegisterName(rs),
                        regs.getRegisterName(rt)));

    uint32_t rsValue = regs.getRegister(rs);
    uint32_t rtValue = regs.getRegister(rt);
    LOGT_CPU(std::format(" (0x{:08x} < 0x{:08x}?)",
                        rsValue, rtValue));

    if (rsValue < rtValue) {
        regs.setRegister(rd, 1);

    } else {
        regs.setRegister(rd, 0);
    }
}

void CPU::ADDU() {
    // Add Unsigned Word
    // T: GPR[rd] <- GPR[rs] + GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    LOGT_CPU(std::format("ADDU {:s},{:s},{:s}",
                        regs.getRegisterName(rd),
                        regs.getRegisterName(rs),
                        regs.getRegisterName(rt)));

    uint32_t rsValue = regs.getRegister(rs);
    uint32_t rtValue = regs.getRegister(rt);

    regs.setRegister(rd, rsValue + rtValue);
}

void CPU::JR() {
    // Jump Register
    // T: temp <- GPR[rs]
    // T+1: PC <- PC + target
    // Should be temp, right?
    uint8_t rs = 0x1F & (instruction >> 21);

    LOGT_CPU(std::format("JR {:s}",
                        regs.getRegisterName(rs)));

    uint32_t target = regs.getRegister(rs);
    regs.setPC(target);
    delaySlotIsBranchDelaySlot = true;

    if (target & 0x3) {
        throw exceptions::ExceptionNotImplemented("Address Error");
    }
}

void CPU::JALR() {
    // Jump And Link Register
    // T: temp <- GPR[rs]
    //    GPR[rd] <- PC + 8
    // T+1: PC <- PC + target
    // Should be temp, right?
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rd = 0x1F & (instruction >> 11);

    LOGT_CPU(std::format("JALR {:s},{:s}",
                        regs.getRegisterName(rd),
                        regs.getRegisterName(rs)));

    uint32_t target = regs.getRegister(rs);
    regs.setPC(target);
    regs.setRegister(rd, instructionPC + 8);
    delaySlotIsBranchDelaySlot = true;

    if (target & 0x3) {
        throw exceptions::ExceptionNotImplemented("Address Error");
    }
}

void CPU::AND() {
    // And
    // T: GPR[rd] <- GPR[rs] and GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    LOGT_CPU(std::format("AND {:s},{:s},{:s}",
                        regs.getRegisterName(rd),
                        regs.getRegisterName(rs),
                        regs.getRegisterName(rt)));

    regs.setRegister(rd, regs.getRegister(rs) & regs.getRegister(rt));
}

void CPU::ADD() {
    // Add Word
    // T: GPR[rd] <- GPR[rs] + GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    LOGT_CPU(std::format("ADD {:s},{:s},{:s}",
                        regs.getRegisterName(rd),
                        regs.getRegisterName(rs),
                        regs.getRegisterName(rt)));

    uint32_t rsValue = regs.getRegister(rs);
    uint32_t rtValue = regs.getRegister(rt);

    bool carry30 = ((0x7FFFFFFF & rsValue) + (0x7FFFFFFF & rtValue)) & 0x80000000;
    bool carry31 = ((rsValue >> 31) + (rtValue >> 31) + (carry30 ? 1 : 0) >= 2);

    if (carry30 == carry31) {
        regs.setRegister(rd, rsValue + rtValue);

    } else {
        throw exceptions::ExceptionNotImplemented("Integer Overflow");
    }
}

void CPU::SUB() {
    // Subtract Word
    // T: GPR[rd] <- GPR[rs] - GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    LOGT_CPU(std::format("SUB {:s},{:s},{:s}",
                        regs.getRegisterName(rd),
                        regs.getRegisterName(rs),
                        regs.getRegisterName(rt)));

    uint32_t rsValue = regs.getRegister(rs);
    uint32_t rtValue = regs.getRegister(rt);

    regs.setRegister(rd, rsValue - rtValue);

    bool carry30 = ((0x7FFFFFFF & rsValue) - (0x7FFFFFFF & rtValue)) & 0x80000000;
    bool carry31 = ((rsValue >> 31) - (rtValue >> 31) + (carry30 ? 1 : 0) >= 2);

    if (carry30 == carry31) {
        regs.setRegister(rd, rsValue - rtValue);

    } else {
        throw exceptions::ExceptionNotImplemented("Integer Overflow");
    }
}

void CPU::SUBU() {
    // Subtract Unsigned Word
    // T: GPR[rd] <- GPR[rs] - GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    LOGT_CPU(std::format("SUBU {:s},{:s},{:s}",
                        regs.getRegisterName(rd),
                        regs.getRegisterName(rs),
                        regs.getRegisterName(rt)));

    uint32_t rsValue = regs.getRegister(rs);
    uint32_t rtValue = regs.getRegister(rt);

    regs.setRegister(rd, rsValue - rtValue);
}

void CPU::SRA() {
    // Shift Word Right Arithmetic
    // T: GPR[rd] <- (GPR[rt]_{31})^{sa} || GPR[rt]_{31...sa}
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);
    uint8_t sa = 0x1F & (instruction >> 6);

    LOGT_CPU(std::format("SRA {:s},{:s},{:s}",
                        regs.getRegisterName(rd),
                        regs.getRegisterName(rt),
                        regs.getRegisterName(sa)));

    uint32_t rtValue = regs.getRegister(rt);
    uint32_t result = rtValue >> sa;
    if (rtValue >> 31) {
        assert (sa <= 32);
        result = result | (0xFFFFFFFF << (32 - sa));
    }

    regs.setRegister(rd, result);
}

void CPU::DIV() {
    // Divide Word
    // T-2: LO <- undefined
    //      HI <- undefined
    // T-1: LO <- undefined
    //      HI <- undefined
    // T:   LO <- GPR[rs] div GPR[rt]
    //      HI <- GPR[rs] mod GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);

    LOGT_CPU(std::format("DIV {:s},{:s}",
                        regs.getRegisterName(rs),
                        regs.getRegisterName(rt)));

    int32_t rsValue = (int32_t)regs.getRegister(rs);
    int32_t rtValue = (int32_t)regs.getRegister(rt);

    regs.setLo(rsValue / rtValue);
    regs.setHi(rsValue % rtValue);
}

void CPU::MFLO() {
    // Move From Lo
    // T: GPR[rd] <- LO
    uint8_t rd = 0x1F & (instruction >> 11);

    LOGT_CPU(std::format("MFLO {:s}", regs.getRegisterName(rd)));

    regs.setRegister(rd, regs.getLo());
}

void CPU::SRL() {
    // Shift Word Right Logical
    // T: GPR[rd] <- 0^{sa} || GPR[rt]_{31...sa}
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);
    uint8_t sa = 0x1F & (instruction >> 6);

    LOGT_CPU(std::format("SRL {:s},{:s},{:s}",
                        regs.getRegisterName(rd),
                        regs.getRegisterName(rt),
                        regs.getRegisterName(sa)));

    regs.setRegister(rd, regs.getRegister(rt) >> sa);
}

void CPU::DIVU() {
    // Divide Word Unsigned
    // T-2: LO <- undefined
    //      HI <- undefined
    // T-1: LO <- undefined
    //      HI <- undefined
    // T:   LO <- GPR[rs] div GPR[rt]
    //      HI <- GPR[rs] mod GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);

    LOGT_CPU(std::format("DIVU {:s},{:s}",
                        regs.getRegisterName(rs),
                        regs.getRegisterName(rt)));

    uint32_t rsValue = regs.getRegister(rs);
    uint32_t rtValue = regs.getRegister(rt);

    regs.setLo(rsValue / rtValue);
    regs.setHi(rsValue % rtValue);
}

void CPU::MFHI() {
    // Move From Hi
    // T: GPR[rd] <- HI
    uint8_t rd = 0x1F & (instruction >> 11);

    LOGT_CPU(std::format("MFHI {:s}", regs.getRegisterName(rd)));

    regs.setRegister(rd, regs.getHi());
}

void CPU::SLT() {
    // Set on Less Than
    // T: if GPR[rs] < GPR[rt] then
    //     GPR[rd] <- 0^{31} || 1
    // else
    //     GPR[rd] <- 0^{32}
    // endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    LOGT_CPU(std::format("SLT {:s},{:s},{:s}",
                        regs.getRegisterName(rd),
                        regs.getRegisterName(rs),
                        regs.getRegisterName(rt)));

    int32_t rsValue = (int32_t)regs.getRegister(rs);
    int32_t rtValue = (int32_t)regs.getRegister(rt);
    LOGT_CPU(std::format(" (0x{:08x} < 0x{:08x}?)",
                        rsValue, rtValue));

    if (rsValue < rtValue) {
        regs.setRegister(rd, 1);

    } else {
        regs.setRegister(rd, 0);
    }
}

void CPU::SYSCALL() {
    // System Call
    // T: SystemCallException
    //uint32_t code = 0xFFFFF & (instruction >> 6);
    // the exception handler can access "code" by manually loading the instruction

    LOGT_CPU("SYSCALL");

    generateException(EXCCODE_SYSCALL);
}

void CPU::MTLO() {
    // Move To Lo
    // T-2: LO <- undefined
    // T-1: LO <- undefined
    // T: LO <- GPR[rs]
    uint8_t rs = 0x1F & (instruction >> 21);

    LOGT_CPU(std::format("MTLO {:s}", regs.getRegisterName(rs)));

    regs.setLo(regs.getRegister(rs));
}

void CPU::MTHI() {
    // Move To HI
    // T-2: HI <- undefined
    // T-1: HI <- undefined
    // T: HI <- GPR[rs]
    uint8_t rs = 0x1F & (instruction >> 21);

    LOGT_CPU(std::format("MTHI {:s}", regs.getRegisterName(rs)));

    regs.setHi(regs.getRegister(rs));
}

void CPU::SLLV() {
    // Shift Word Left Logical Variable
    // s <- GP[rs]_{4...0}
    // GPR[rd] <- GPR[rt]_{(31-s)...0} || 0^s
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    LOGT_CPU(std::format("SLLV {:s},{:s},{:s}",
                        regs.getRegisterName(rd),
                        regs.getRegisterName(rt),
                        regs.getRegisterName(rs)));

    uint8_t s = regs.getRegister(rs) & 0x1F;

    regs.setRegister(rd, regs.getRegister(rt) << s);
}

void CPU::NOR() {
    // Nor
    // T: GPR[rd] <- GPR[rs] nor GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);
    LOGT_CPU(std::format("NOR {:s},{:s},{:s}",
                        regs.getRegisterName(rd),
                        regs.getRegisterName(rs),
                        regs.getRegisterName(rt)));

    regs.setRegister(rd, ~(regs.getRegister(rs) | regs.getRegister(rt)));
}

void CPU::SRAV() {
    // Shift Word Right Arithmetic Variable
    // T: s <- GPR[rs]_{4...0}
    //    GPR[rd] <- (GPR[rt]_{31})^{s} || GPR[rt]_{31...s}
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    LOGT_CPU(std::format("SRAV {:s},{:s},{:s}",
                        regs.getRegisterName(rd),
                        regs.getRegisterName(rt),
                        regs.getRegisterName(rs)));

    uint8_t s = regs.getRegister(rs);
    uint32_t rtValue = regs.getRegister(rt);
    uint32_t result = rtValue >> s;
    if (rtValue >> 31) {
        assert (s <= 32);
        result = result | (0xFFFFFFFF << (32 - s));
    }

    regs.setRegister(rd, result);
}

void CPU::SRLV() {
    // Shift Word Right Logical Variable
    // T: s <- GPR[rs]_{4...0}
    //    GPR[rd] <- 0^{s} || GPR[rt]_{31...s}
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    LOGT_CPU(std::format("SRLV {:s},{:s},{:s}",
                        regs.getRegisterName(rd),
                        regs.getRegisterName(rt),
                        regs.getRegisterName(rs)));

    uint8_t s = regs.getRegister(rs);
    uint32_t rtValue = regs.getRegister(rt);
    uint32_t result = rtValue >> s;

    regs.setRegister(rd, result);
}

void CPU::MULT() {
    // Multiply Word
    // T-2: LO <- undefined
    //      HI <- undefined
    // T-1: LO <- undefined
    //      HI <- undefined
    // T:   t <- GPR[rs] * GPR[rt]
    //      LO <- t_{31...0}
    //      HI <- t_{63...32}
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);

    LOGT_CPU(std::format("MULT {:s},{:s}",
                        regs.getRegisterName(rs),
                        regs.getRegisterName(rt)));

    int64_t rsValue = regs.getRegister(rs);
    int64_t rtValue = regs.getRegister(rt);
    int64_t result = rsValue * rtValue;

    regs.setLo(0x00000000FFFFFFFF & result);
    regs.setHi(result >> 32);
}

void CPU::MULTU() {
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

    LOGT_CPU(std::format("MULTU {:s},{:s}",
                        regs.getRegisterName(rs),
                        regs.getRegisterName(rt)));

    uint64_t rsValue = regs.getRegister(rs);
    uint64_t rtValue = regs.getRegister(rt);
    uint64_t result = rsValue * rtValue;

    regs.setLo(0x00000000FFFFFFFF & result);
    regs.setHi(result >> 32);
}

void CPU::XOR() {
    // Exclusive Or
    // T: GPR[rd] <- GPR[rs] xor GPR[rt]
    uint8_t rs = 0x1F & (instruction >> 21);
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);
    LOGT_CPU(std::format("XOR {:s},{:s},{:s}",
                        regs.getRegisterName(rd),
                        regs.getRegisterName(rs),
                        regs.getRegisterName(rt)));

    uint32_t rsValue = regs.getRegister(rs);
    uint32_t rtValue = regs.getRegister(rt);

    regs.setRegister(rd, (rsValue & ~rtValue) | (~rsValue & rtValue));
}

void CPU::UNKCP0() {
    throw exceptions::UnknownFunctionError(std::format("Unknown CP0 opcode @0x{:x}: instruction 0x{:x} (CP0), function 0b{:06b}", instructionPC, instruction, funct));
}


void CPU::CP0MOVE() {
    // CP0 Move
    // Operation depends on function field
    move = 0x1F & (instruction >> 21);

    (this->*cp0Move[move])();
}

void CPU::RFE() {
    // Restore From Exception
    // T: SR <- SR_{31...4} || SR_{5...2}

    LOGT_CPU("RFE");

    uint32_t sr = cp0regs.getCP0Register(CP0_REGISTER_SR);
    cp0regs.setCP0Register(CP0_REGISTER_SR, (sr & 0xFFFFFFF0) | ((sr & 0x3C) >> 2));
}

void CPU::UNKCP0M() {
    throw exceptions::UnknownOpcodeError(std::format("0x{:x}: instruction 0x{:x} (CP0Move), move 0b{:05b}", instructionPC, instruction, move));
}

void CPU::MTC0() {
    // Move To Coprocessor 0
    // T: data <- GPR[rt]
    // T+1: CPR[0,rd] <- data
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    LOGT_CPU(std::format("MTC0 {:s},{:d}",
                         regs.getRegisterName(rt),
                         rd));

    uint32_t data = regs.getRegister(rt);
    LOGT_CPU(std::format(" (0x{:08X} -> CP0 {:d})",data, rd));

    cp0regs.setCP0Register(rd, data);
    checkAndExecuteInterrupts();
}

void CPU::MFC0() {
    // Move From Coprocessor 0
    // T: data <- CPR[0, rd]
    // T+1: GPR[rt] <- data
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    LOGT_CPU(std::format("MFC0 {:s},{:d}",
                        regs.getRegisterName(rt),
                        rd));

    uint32_t data = cp0regs.getCP0Register(rd);
    LOGT_CPU(std::format(" (CP0 {:d} -0x{:08X}->)",
                        rd, data));

    regs.setRegister(rt, data);
}

void CPU::UNKCP2() {
    throw exceptions::UnknownFunctionError(std::format("Unknown CP2 opcode @0x{:x}: instruction 0x{:x} (CP0), function 0b{:06b}", instructionPC, instruction, funct));
}


void CPU::CP2MOVE() {
    // CP2 Move
    // Operation depends on function field
    move = 0x1F & (instruction >> 21);

    (this->*cp2Move[move])();
}

void CPU::UNKCP2M() {
    throw exceptions::UnknownOpcodeError(std::format("0x{:x}: instruction 0x{:x} (CP2Move), move 0b{:05b}", instructionPC, instruction, move));
}

void CPU::CTC2() {
    // Move Control To Coprocessor 2
    // T: data <- GPR[rt]
    // T+1: CCR[2,rd] <- data
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    LOGT_CPU(std::format("CTC2 {:s},{:d}",
                         regs.getRegisterName(rt),
                         rd));

    uint32_t data = regs.getRegister(rt);
    LOGT_CPU(std::format(" (0x{:08X} -> CP2 c{:d})",data, rd));

    gte.setControlRegister(rd, data);
}

void CPU::MTC2() {
    // Move To Coprocessor 2
    // T: data <- GPR[rt]
    // T+1: CPR[2,rd] <- data
    uint8_t rt = 0x1F & (instruction >> 16);
    uint8_t rd = 0x1F & (instruction >> 11);

    LOGT_CPU(std::format("MTC2 {:s},{:d}",
                         regs.getRegisterName(rt),
                         rd));

    uint32_t data = regs.getRegister(rt);
    LOGT_CPU(std::format(" (0x{:08X} -> CP2 {:d})",data, rd));

    gte.setRegister(rd, data);
}

void CPU::UNKRGMM() {
    throw exceptions::UnknownOpcodeError(std::format("Unknown REGIMM @0x{:x}: instruction 0x{:x} (REGIMM), rt 0b{:05b}", instructionPC, instruction, instructionRt));
}

void CPU::BLTZ() {
    // Branch On Less Than Zero
    // T: target <- (offset_{15})^{14} || offset || 0^2
    //    condition <- (GPR[rs]_{31} = 1)
    //    GPR[31] <- PC + 8
    // T+1: if condition then
    //          PC <- PC + target
    //      endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint32_t offset = 0xFFFF & instruction;

    LOGT_CPU(std::format("BLTZ {:s},{:04X}",
                        regs.getRegisterName(rs),
                        offset));

    uint32_t signExtension = ((offset >> 15) ? 0xFFFF0000 : 0x00000000) + offset;
    uint32_t target = signExtension << 2;
    uint32_t actualTarget = delaySlotPC + target;

    uint32_t rsValue = regs.getRegister(rs);
    LOGT_CPU(std::format(" (0x{:08X} < 0? -0x{:08X}-> pc)",
                        rsValue, actualTarget));

    if (rsValue >> 31) {
        regs.setPC(actualTarget);
        delaySlotIsBranchDelaySlot = true;
    }
}

void CPU::BLTZAL() {
    // Branch On Less Than Zero And Link
    // T: target <- (offset_{15})^{14} || offset || 0^2
    //    condition <- (GPR[rs]_{31} = 1)
    //    GPR[31] <- PC + 8
    // T+1: if condition then
    //          PC <- PC + target
    //      endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint32_t offset = 0xFFFF & instruction;

    LOGT_CPU(std::format("BLTZAL {:s},{:04X}",
                        regs.getRegisterName(rs),
                        offset));

    uint32_t signExtension = ((offset >> 15) ? 0xFFFF0000 : 0x00000000) + offset;
    uint32_t target = signExtension << 2;
    uint32_t actualTarget = delaySlotPC + target;

    uint32_t rsValue = regs.getRegister(rs);
    LOGT_CPU(std::format(" (0x{:08X} < 0? -0x{:08X}-> pc)",
                        rsValue, actualTarget));

    if (rsValue >> 31) {
        regs.setRegister(31, instructionPC + 8);
        regs.setPC(actualTarget);
        delaySlotIsBranchDelaySlot = true;
    }
}

void CPU::BGEZ() {
    // Branch On Greater Than Or Equal To Zero
    // T: target <- (offset_{15})^{14} || offset || 0^2
    //    condition <- (GPR[rs]_{31} = 0)
    // T+1: if condition then
    //          PC <- PC + target
    //      endif
    uint8_t rs = 0x1F & (instruction >> 21);
    uint32_t offset = 0xFFFF & instruction;

    LOGT_CPU(std::format("BGEZ {:s},{:04X}",
                        regs.getRegisterName(rs),
                        offset));

    uint32_t signExtension = ((offset >> 15) ? 0xFFFF0000 : 0x00000000) + offset;
    uint32_t target = signExtension << 2;
    uint32_t actualTarget = delaySlotPC + target;

    uint32_t rsValue = regs.getRegister(rs);
    LOGT_CPU(std::format(" (0x{:08X} > 0? -0x{:08X}-> pc)",
                        rsValue, actualTarget));

    if (!(rsValue >> 31)) {
        regs.setPC(actualTarget);
        delaySlotIsBranchDelaySlot = true;
    }
}

}
