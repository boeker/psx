#include "interrupts.h"

#include <cassert>
#include <cstring>
#include <format>
#include <sstream>

#include "bus.h"
#include "util/log.h"

using namespace util;

namespace PSX {

std::ostream& operator<<(std::ostream &os, const Interrupts &interrupts) {
    os << "I_STAT: ";
    os << interrupts.getInterruptStatusRegisterExplanation();
    os << std::endl;
    os << "I_MASK: ";
    os << interrupts.getInterruptMaskRegisterExplanation();

    return os;
}

Interrupts::Interrupts(Bus *bus) {
    this->bus = bus;

    reset();
}

void Interrupts::reset() {
    std::memset(interruptStatusRegister, 0, 4);
    std::memset(interruptMaskRegister, 0, 4);
}

template <typename T>
void Interrupts::write(uint32_t address, T value) {
    assert ((address >= 0x1F801070) && (address < 0x1F801074 + sizeof(T)));

    LOG_INT_IO(std::format("Interrupt write 0x{:0{}X} -> @0x{:08X}",
                           value, 2*sizeof(T), address));

    if (address < 0x1F801074) { // I_STAT
        assert (address + sizeof(T) <= 0x1F801074);
        uint32_t offset = address & 0x00000007;

        // Writing 0 to I_STAT bit clears the bit
        // Writing 1 to I_STAT bit does not change the bit
        T *istat = (T*)(interruptStatusRegister + offset);
        *istat = *istat & value;

        LOG_INT(std::format("I_STAT updated: {:s}",
                            getInterruptStatusRegisterExplanation()));

    } else { // I_MASK
        assert (address + sizeof(T) <= 0x1F801078);
        uint32_t offset = address & 0x00000003;

        *((T*)(interruptMaskRegister + offset)) = value;

        LOG_INT(std::format("I_MASK updated: {:s}",
                            getInterruptMaskRegisterExplanation()));
    }

    checkAndExecuteInterrupts();
}

template void Interrupts::write(uint32_t address, uint32_t value);
template void Interrupts::write(uint32_t address, uint16_t value);
template void Interrupts::write(uint32_t address, uint8_t value);

template <typename T>
T Interrupts::read(uint32_t address) {
    assert ((address >= 0x1F801070) && (address < 0x1F801074 + sizeof(T)));

    T value;

    if (address < 0x1F801074) { // I_STAT
        assert (address + sizeof(T) <= 0x1F801074);
        uint32_t offset = address & 0x00000007;

        value = *((T*)(interruptStatusRegister + offset));

    } else { // I_MASK
        assert (address + sizeof(T) <= 0x1F801078);
        uint32_t offset = address & 0x00000003;

        value = *((T*)(interruptMaskRegister + offset));
    }

    LOG_INT_IO(std::format("Interrupt read @0x{:08X} -> 0x{:0{}X}",
                           address, value, 2*sizeof(T)));

    return value;
}

template uint32_t Interrupts::read(uint32_t address);
template uint16_t Interrupts::read(uint32_t address);
template uint8_t Interrupts::read(uint32_t address);

void Interrupts::checkAndExecuteInterrupts() {
    uint32_t istat = *((uint32_t*)(interruptStatusRegister));
    uint32_t imask = *(((uint32_t*)interruptMaskRegister));

    if ((istat & imask) & 0x3FF) { // one or more interrupts is requested and enabled
        LOG_INT_VERB(std::format("Interrupt requested and enabled: 0x{:03X}",
                                 (istat & imask) & 0x3FF));
        bus->cpu.cp0regs.setBit(CP0_REGISTER_CAUSE, CAUSE_BIT_IP2);

        bus->cpu.checkAndExecuteInterrupts();

    } else {
        // CAUSE_BIT_IP2 is not a latch, has to be set to 0 once I_STAT and I_MASK are zero
        bus->cpu.cp0regs.clearBit(CP0_REGISTER_CAUSE, CAUSE_BIT_IP2);
    }
}

void Interrupts::notifyAboutVBLANK() {
    uint32_t *istat = ((uint32_t*)(interruptStatusRegister));
    *istat = (*istat) | (1 << INTERRUPT_BIT_VBLANK);
    // the bit is edge triggered
    // hence, only setting it once should be fine
    checkAndExecuteInterrupts();
}

void Interrupts::notifyAboutInterrupt(uint32_t interruptBit) {
    uint32_t *istat = ((uint32_t*)(interruptStatusRegister));
    *istat = (*istat) | (1 << interruptBit);
    // the bit is edge triggered
    // hence, only setting it once should be fine
    checkAndExecuteInterrupts();
}

std::string Interrupts::getInterruptStatusRegisterExplanation() const {
    uint32_t istat = *((uint32_t*)(interruptStatusRegister));
    std::stringstream ss;

    ss << std::format("IRQ10(Light.)[{:01b}] ", (istat >> 10) & 0x1);
    ss << std::format("IRQ9(SPU)[{:01b}] ", (istat >> 9) & 0x1);
    ss << std::format("IRQ8(SIO)[{:01b}] ", (istat >> 8) & 0x1);
    ss << std::format("IRQ7(Cont. and Mem.)[{:01b}] ", (istat >> 7) & 0x1);
    ss << std::format("IRQ6(TMR2)[{:01b}] ", (istat >> 6) & 0x1);
    ss << std::format("IRQ5(TMR1)[{:01b}] ", (istat >> 5) & 0x1);
    ss << std::format("IRQ4(TMR0)[{:01b}] ", (istat >> 4) & 0x1);
    ss << std::format("IRQ3(DMA)[{:01b}] ", (istat >> 3) & 0x1);
    ss << std::format("IRQ2(CDROM)[{:01b}] ", (istat >> 2) & 0x1);
    ss << std::format("IRQ1(GPU)[{:01b}] ", (istat >> 1) & 0x1);
    ss << std::format("IRQ0(VBLANK)[{:01b}] ", (istat >> 0) & 0x1);

    return ss.str();
}

std::string Interrupts::getInterruptMaskRegisterExplanation() const {
    uint32_t imask = *(((uint32_t*)interruptMaskRegister));
    std::stringstream ss;

    ss << std::format("IRQ10(Light.)[{:01b}] ", (imask >> 10) & 0x1);
    ss << std::format("IRQ9(SPU)[{:01b}] ", (imask >> 9) & 0x1);
    ss << std::format("IRQ8(SIO)[{:01b}] ", (imask >> 8) & 0x1);
    ss << std::format("IRQ7(Cont. and Mem.)[{:01b}] ", (imask >> 7) & 0x1);
    ss << std::format("IRQ6(TMR2)[{:01b}] ", (imask >> 6) & 0x1);
    ss << std::format("IRQ5(TMR1)[{:01b}] ", (imask >> 5) & 0x1);
    ss << std::format("IRQ4(TMR0)[{:01b}] ", (imask >> 4) & 0x1);
    ss << std::format("IRQ3(DMA)[{:01b}] ", (imask >> 3) & 0x1);
    ss << std::format("IRQ2(CDROM)[{:01b}] ", (imask >> 2) & 0x1);
    ss << std::format("IRQ1(GPU)[{:01b}] ", (imask >> 1) & 0x1);
    ss << std::format("IRQ0(VBLANK)[{:01b}] ", (imask >> 0) & 0x1);

    return ss.str();
}

}

