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

    // Information extracted from instruction
    uint8_t opcode;
    uint8_t funct;
    uint8_t move;

    // Delay Slot
    uint32_t delaySlotPC;
    uint32_t delaySlot;

    // Opcode tables and implementations
    typedef void (Core::*Opcode) ();

    static const Opcode opcodes[];
    void UNK();
    void SPECIAL();
    void CP0();
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

    static const Opcode cp0[];
    void UNKCP0();
    void CP0MOVE();

    static const Opcode cp0Move[];
    void UNKCP0M();
    void MTC0();
    void MFC0();

public:
    Core();
    void reset();
    
    void readBIOS(const std::string &file);

    void step();
};
}

#endif
