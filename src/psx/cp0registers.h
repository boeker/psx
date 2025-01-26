#ifndef PSX_CP0REGISTERS_H
#define PSX_CP0REGISTERS_H

#include <cstdint>
#include <iostream>

// CPU Control Registers
#define CP0_REGISTER_BUSCTRL  2
#define CP0_REGISTER_BPC      3
#define CP0_REGISTER_BDA      5
#define CP0_REGISTER_JUMPDEST 6
#define CP0_REGISTER_DCIC     7
#define CP0_REGISTER_BADVADDR 8
#define CP0_REGISTER_BDAM     9
#define CP0_REGISTER_BPCM     10
#define CP0_REGISTER_SR       12
#define CP0_REGISTER_CAUSE    13
#define CP0_REGISTER_EPC      14
#define CP0_REGISTER_PRID     15

#define SR_BIT_BEV 22
#define CAUSE_BIT_BD 31

namespace PSX {
class CP0Registers {
private:
    uint32_t cp0Registers[32];

    std::string getSRExplanation() const;
    std::string getCauseExplanation() const;
    friend std::ostream& operator<<(std::ostream &os, const CP0Registers &registers);

public:
    static const char* CP0_REGISTER_NAMES[];
    std::string getCP0RegisterName(uint8_t reg);

    CP0Registers();
    void reset();
    
    uint32_t getCP0Register(uint8_t reg);
    void setCP0Register(uint8_t reg, uint32_t value);

    bool statusRegisterIsolateCacheIsSet() const;
};
}

#endif
