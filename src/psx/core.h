#ifndef PSX_CORE_H
#define PSX_CORE_H

#include <string>

#include "memory.h"

namespace PSX {

class Core {
private:
    Memory memory;

    // Current instruction and opcode
    uint32_t instructionPC;
    uint32_t instruction;
    bool isBranchDelaySlot;

    // Information extracted from instruction
    uint8_t opcode;
    uint8_t funct;
    uint8_t move;
    uint8_t instructionRt;

    // Delay Slot
    uint32_t delaySlotPC;
    uint32_t delaySlot;
    bool delaySlotIsBranchDelaySlot;

    friend std::ostream& operator<<(std::ostream &os, const Core &core);

    // Opcode tables and implementations
    typedef void (Core::*Opcode) ();

    static const Opcode opcodes[];
    void UNK();
    void SPECIAL();
    void CP0();
    void REGIMM();
    void LUI();
    void ORI();
    void SW();
    void ADDIU();
    void J();
    void BNE();
    void ADDI();
    void LW();
    void SH();
    void JAL();
    void ANDI();
    void SB();
    void LB();
    void BEQ();
    void BGTZ();
    void BLEZ();
    void LBU();
    void SLTI();
    void SLTIU();
    void LHU();
    void LH();

    // Opcode SPECIAL encodes further instructions via function field
    static const Opcode special[];
    void UNKSPCL();
    void SLL();
    void OR();
    void SLTU();
    void ADDU();
    void JR();
    void JALR();
    void AND();
    void ADD();
    void SUBU();
    void SRA();
    void DIV();
    void MFLO();
    void SRL();
    void DIVU();
    void MFHI();
    void SLT();
    void SYSCALL();
    void MTLO();
    void MTHI();
    void SLLV();
    void NOR();
    void SRAV();
    void SRLV();
    void MULTU();

    // Opcode CP0 encodes further instructions
    static const Opcode cp0[];
    void UNKCP0();
    void CP0MOVE();
    void RFE();

    // CP0 instructions are identified via move field
    static const Opcode cp0Move[];
    void UNKCP0M();
    void MTC0();
    void MFC0();

    // Opcode REGIMM encodes further instructions via rt field
    static const Opcode regimm[];
    void UNKRGMM();
    void BLTZ();
    void BLTZAL();
    void BGEZ();

public:
    Core();
    void reset();
    
    void readBIOS(const std::string &file);

    void step();
    void fetchDelaySlot();
};
}

#endif
