#ifndef PSX_INTERRUPTS_H
#define PSX_INTERRUPTS_H

#include <cstdint>
#include <iostream>
#include <string>

namespace PSX {

#define INTERRUPT_BIT_VBLANK 0
#define INTERRUPT_BIT_GPU 1
#define INTERRUPT_BIT_CDROM 2
#define INTERRUPT_BIT_DMA 3
#define INTERRUPT_BIT_TMR0 4
#define INTERRUPT_BIT_TMR1 5
#define INTERRUPT_BIT_TMR2 6
#define INTERRUPT_BIT_CTRL_MEM 7
#define INTERRUPT_BIT_SIO 8
#define INTERRUPT_BIT_SPU 9
#define INTERRUPT_BIT_CTRL_LGT 10

class Bus;

class Interrupts {
private:
    Bus *bus;

    // I_STAT
    // 0x1F801070
    uint8_t interruptStatusRegister[4];

    // I_MASK
    // 0x1F801074
    uint8_t interruptMaskRegister[4];

    friend std::ostream& operator<<(std::ostream &os, const Interrupts &interrupts);

public:
    Interrupts(Bus *bus);
    void reset();

    template <typename T>
    void write(uint32_t address, T value);

    template <typename T>
    T read(uint32_t address);

    void checkAndExecuteInterrupts();
    void notifyAboutVBLANK();
    void notifyAboutInterrupt(uint32_t interruptBit);

private:
    std::string getInterruptStatusRegisterExplanation() const;
    std::string getInterruptMaskRegisterExplanation() const;
};

}

#endif
