#ifndef PSX_CORE_H
#define PSX_CORE_H

#include <string>

#include "memory.h"

namespace PSX {

class Core {
private:
    Memory memory;

    void log(const std::string &string);

    // Current instruction and opcode
    uint32_t instructionPC;
    uint32_t instruction;

    // Information extracted from instruction
    uint8_t opcode;
    uint8_t funct;
    uint8_t move;

    // Next instruction
    uint32_t nextInstructionPC;
    uint32_t nextInstruction;

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

    static const Opcode special[];
    void UNKSPCL();
    void SLL();
    void OR();

    static const Opcode cp0[];
    void UNKCP0();
    void CP0MOVE();

    static const Opcode cp0Move[];
    void UNKCP0M();
    void MTC0();

public:
    Core();
    void reset();
    
    void readBIOS(const std::string &file);

    void step();
};
}

#endif
