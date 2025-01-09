#ifndef PSX_CORE_H
#define PSX_CORE_H

#include <string>

#include "cpu.h"
#include "memory.h"

namespace PSX {

class Core {
private:
    CPU cpu;
    Memory memory;

    // Current instruction and opcode
    uint32_t instructionPC;
    uint32_t instruction;

    // Information extracted from instruction
    uint8_t opcode;
    uint8_t funct;

    // Next instruction
    uint32_t nextInstructionPC;
    uint32_t nextInstruction;

    // Opcode tables and implementations
    typedef void (Core::*Opcode) ();

    static const Opcode opcodes[];
    void UNK();
    void SPECIAL();
    void LUI();
    void ORI();
    void SW();
    void ADDIU();
    void J();

    static const Opcode functions[];
    void UNKFUNCT();
    void SLL();
    void OR();

public:
    Core();
    void reset();
    
    void readBIOS(const std::string &file);

    void step();
};
}

#endif
