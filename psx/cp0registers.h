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
#define SR_BIT_IEC 22

#define SR_BIT_IM7 15
#define SR_BIT_IM6 14
#define SR_BIT_IM5 13
#define SR_BIT_IM4 12
#define SR_BIT_IM3 11
#define SR_BIT_IM2 10
#define SR_BIT_IM1 9
#define SR_BIT_IM0 8

#define CAUSE_BIT_BD 31
#define CAUSE_BIT_IP7 15
#define CAUSE_BIT_IP6 14
#define CAUSE_BIT_IP5 13
#define CAUSE_BIT_IP4 12
#define CAUSE_BIT_IP3 11
#define CAUSE_BIT_IP2 10
#define CAUSE_BIT_IP1 9
#define CAUSE_BIT_IP0 8

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
    bool getBit(uint8_t reg, uint8_t bit) const;
    void setBit(uint8_t reg, uint8_t bit, bool value);
    void setBit(uint8_t reg, uint8_t bit);
    void clearBit(uint8_t reg, uint8_t bit);
};
}

#endif
