#ifndef PSX_CPU_H
#define PSX_CPU_H

#include <cstdint>
#include <iostream>

/*
All registers are 32bit wide.
  Name       Alias    Common Usage
  (R0)       zero     Constant (always 0) (this one isn't a real register)
  R1         at       Assembler temporary (destroyed by some pseudo opcodes!)
  R2-R3      v0-v1    Subroutine return values, may be changed by subroutines
  R4-R7      a0-a3    Subroutine arguments, may be changed by subroutines
  R8-R15     t0-t7    Temporaries, may be changed by subroutines
  R16-R23    s0-s7    Static variables, must be saved by subs
  R24-R25    t8-t9    Temporaries, may be changed by subroutines
  R26-R27    k0-k1    Reserved for kernel (destroyed by some IRQ handlers!)
  R28        gp       Global pointer (rarely used)
  R29        sp       Stack pointer
  R30        fp(s8)   Frame Pointer, or 9th Static variable, must be saved
  R31        ra       Return address (used so by JAL,BLTZAL,BGEZAL opcodes)
  -          pc       Program counter
  -          hi,lo    Multiply/divide results, may be changed by subroutines
R0 is always zero.
R31 can be used as general purpose register, however, some opcodes are using it
to store the return address: JAL, BLTZAL, BGEZAL. (Note: JALR can optionally
store the return address in R31, or in R1..R30. Exceptions store the return
address in cop0r14 - EPC).

R29 (SP) - Full Decrementing Wasted Stack Pointer
The CPU doesn't explicitly have stack-related registers or opcodes, however,
conventionally, R29 is used as stack pointer (SP). The stack can be accessed
with normal load/store opcodes, which do not automatically increase/decrease
SP, so the SP register must be manually modified to (de-)allocate data.
The PSX BIOS is using "Full Decrementing Wasted Stack".
Decrementing means that SP gets decremented when allocating data (that's common
for most CPUs) - Full means that SP points to the first ALLOCATED word on the
stack, so the allocated memory is at SP+0 and above, free memory at SP-1 and
below, Wasted means that when calling a sub-function with N parameters, then
the caller must pre-allocate N works on stack, and the sub-function may freely
use and destroy these words; at [SP+0..N*4-1].

For example, "push ra,r16,r17" would be implemented as:
  sub  sp,20h
  mov  [sp+14h],ra
  mov  [sp+18h],r16
  mov  [sp+1Ch],r17
where the allocated 20h bytes have the following purpose:
  [sp+00h..0Fh]  wasted stack (may, or may not, be used by sub-functions)
  [sp+10h..13h]  8-byte alignment padding (not used)
  [sp+14h..1Fh]  pushed registers
*/

// CPU Control Registers
#define CP0_REGISTER_SR 12 // Status Register

namespace PSX {
class Registers {
private:
    uint32_t registers[32];
    uint32_t pc;
    uint32_t hi;
    uint32_t lo;

    uint32_t cp0Registers[32];

    friend std::ostream& operator<<(std::ostream &os, const Registers &registers);

public:
    static const char* REGISTER_NAMES[];
    std::string getRegisterName(uint8_t reg);

    Registers();
    void reset();
    
    uint32_t getPC();
    void setPC(uint32_t pc);
    uint32_t getRegister(uint8_t reg);
    void setRegister(uint8_t reg, uint32_t value);
    uint32_t getCP0Register(uint8_t reg);
    void setCP0Register(uint8_t reg, uint32_t value);

    bool statusRegisterIsolateCacheIsSet() const;
};
}

#endif
