#ifndef UTIL_DISASSEMBLER_H
#define UTIL_DISASSEMBLER_h

#include <cstdint>
#include <string>

namespace util {

class Disassembler {
public:
    static const char* REGISTER_NAMES[];
    static std::string getRegisterName(uint8_t reg);

    static std::string disassemble(uint32_t ins);

    static uint32_t instruction;
    // Information extracted from instruction
    static uint8_t opcode;
    static uint8_t funct;
    static uint8_t move;
    static uint8_t instructionRt;

private:
    // Opcode tables and implementations
    typedef std::string (*Opcode) ();

    static const Opcode opcodes[];
    static std::string UNK();
    static std::string SPECIAL();
    static std::string CP0();
    static std::string CP2();
    static std::string REGIMM();
    static std::string LUI();
    static std::string ORI();
    static std::string SW();
    static std::string ADDIU();
    static std::string J();
    static std::string BNE();
    static std::string ADDI();
    static std::string LW();
    static std::string SH();
    static std::string JAL();
    static std::string ANDI();
    static std::string SB();
    static std::string LB();
    static std::string BEQ();
    static std::string BGTZ();
    static std::string BLEZ();
    static std::string LBU();
    static std::string SLTI();
    static std::string SLTIU();
    static std::string LHU();
    static std::string LH();
    static std::string LWL();
    static std::string LWR();
    static std::string SWL();
    static std::string SWR();
    static std::string XORI();

    // Opcode SPECIAL encodes further instructions via function field
    static const Opcode special[];
    static std::string UNKSPCL();
    static std::string SLL();
    static std::string OR();
    static std::string SLTU();
    static std::string ADDU();
    static std::string JR();
    static std::string JALR();
    static std::string AND();
    static std::string ADD();
    static std::string SUB();
    static std::string SUBU();
    static std::string SRA();
    static std::string DIV();
    static std::string MFLO();
    static std::string SRL();
    static std::string DIVU();
    static std::string MFHI();
    static std::string SLT();
    static std::string SYSCALL();
    static std::string MTLO();
    static std::string MTHI();
    static std::string SLLV();
    static std::string NOR();
    static std::string SRAV();
    static std::string SRLV();
    static std::string MULT();
    static std::string MULTU();
    static std::string XOR();

    // Opcode CP0 encodes further instructions
    static const Opcode cp0[];
    static std::string UNKCP0();
    static std::string CP0MOVE();
    static std::string RFE();

    // CP0 instructions are identified via move field
    static const Opcode cp0Move[];
    static std::string UNKCP0M();
    static std::string MTC0();
    static std::string MFC0();

    // Opcode CP2 encodes further instructions
    static const Opcode cp2[];
    static std::string UNKCP2();
    static std::string CP2MOVE();

    // CP2 instructions are identified via move field
    static const Opcode cp2Move[];
    static std::string UNKCP2M();
    static std::string CTC2();
    static std::string MTC2();

    // Opcode REGIMM encodes further instructions via rt field
    static const Opcode regimm[];
    static std::string UNKRGMM();
    static std::string BLTZ();
    static std::string BLTZAL();
    static std::string BGEZ();

};
}

#endif
