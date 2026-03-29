#ifndef PSX_CPU_H
#define PSX_CPU_H

#include <string>
#include <sstream>

#include "registers.h"
#include "cp0.h"
#include "gte.h"

namespace PSX {

#define CPU_FREQUENCY 33868800
#define CPU_VBLANK_FREQUENCY (CPU_FREQUENCY / 60)

#define EXCCODE_INT 0
#define EXCCODE_MOD 1
#define EXCCODE_TLBL 2
#define EXCCODE_TLBS 3
#define EXCCODE_ADEL 4
#define EXCCODE_ADES 5
#define EXCCODE_IBE 6
#define EXCCODE_DBE 7
#define EXCCODE_SYSCALL 8
#define EXCCODE_BP 9
#define EXCCODE_RI 10
#define EXCCODE_CPU 11
#define EXCCODE_OV 12

class Bus;

class CPU {
public:
    Registers regs;
    CP0 cp0;
    GTE gte;

public:
    CPU(Bus *bus);
    void reset();
    
    void step();
    void fetchDelaySlot();
    void interceptTTYOutput();
    void generateException(uint8_t exccode, bool epcShouldBeNextInstruction = false);
    void checkAndExecuteInterrupts();

    uint32_t getDelaySlotPC() const;

private:
    Bus *bus;
    std::stringstream ttyOutput;

public:
    uint32_t cycles;

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

    //bool shouldCheckInterrupts;

    friend std::ostream& operator<<(std::ostream &os, const CPU &cpu);

private:
    // Opcode tables and implementations
    typedef void (CPU::*Opcode) ();

    static const Opcode opcodes[];
    void UNK();
    void SPECIAL();
    void CP0MOVE();
    void CP2MOVE();
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
    void LWL();
    void LWR();
    void SWL();
    void SWR();
    void XORI();
    void LWC2();
    void SWC2();

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
    void SUB();
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
    void MULT();
    void MULTU();
    void XOR();
    void BREAK();

    // CP0 instructions are identified via move field
    static const Opcode cp0Move[];
    void UNKCP0M();
    void MTC0();
    void MFC0();
    void CP0INST();

    // CP2 instructions are identified via move field
    static const Opcode cp2Move[];
    void UNKCP2M();
    void CTC2();
    void MTC2();
    void CFC2();
    void MFC2();
    void CP2INST();

    // Opcode REGIMM encodes further instructions via rt field
    static const Opcode regimm[];
    void UNKRGMM();
    void BLTZ();
    void BLTZAL();
    void BGEZ();
    void BGEZAL();

};
}

#endif
