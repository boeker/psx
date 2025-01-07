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
    uint32_t instruction;
    uint8_t opcode;

    // Opcode tables and implementations
    typedef void (Core::*Opcode) ();
    static const Opcode opcodes[];
    void UNK();
    void LUI();
    void ORI();
    void SW();

public:
    void readBIOS(const std::string &file);

    void step();
};
}

#endif
