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
    uint32_t istat = *((uint32_t*)(interrupts.interruptStatusRegister));

    os << "I_STAT: ";
    os << std::format("IRQ10(Controller - Lightpen)[{:01b}] ", (istat >> 10) & 0x1);
    os << std::format("IRQ9(SPU)[{:01b}] ", (istat >> 9) & 0x1);
    os << std::format("IRQ8(SIO)[{:01b}] ", (istat >> 8) & 0x1);
    os << std::format("IRQ7(Controller and Memcard)[{:01b}] ", (istat >> 7) & 0x1);
    os << std::format("IRQ6(TMR2)[{:01b}] ", (istat >> 6) & 0x1);
    os << std::format("IRQ5(TMR1)[{:01b}] ", (istat >> 5) & 0x1);
    os << std::format("IRQ4(TMR0)[{:01b}] ", (istat >> 4) & 0x1);
    os << std::format("IRQ3(DMA)[{:01b}] ", (istat >> 3) & 0x1);
    os << std::format("IRQ2(CDROM)[{:01b}] ", (istat >> 2) & 0x1);
    os << std::format("IRQ1(GPU)[{:01b}] ", (istat >> 1) & 0x1);
    os << std::format("IRQ0(VBLANK)[{:01b}] ", (istat >> 0) & 0x1);
    os << std::endl;

    uint32_t imask = *(((uint32_t*)interrupts.interruptMaskRegister));

    os << "I_MASK: ";
    os << std::format("IRQ10(Controller - Lightpen)[{:01b}] ", (imask >> 10) & 0x1);
    os << std::format("IRQ9(SPU)[{:01b}] ", (imask >> 9) & 0x1);
    os << std::format("IRQ8(SIO)[{:01b}] ", (imask >> 8) & 0x1);
    os << std::format("IRQ7(Controller and Memcard)[{:01b}] ", (imask >> 7) & 0x1);
    os << std::format("IRQ6(TMR2)[{:01b}] ", (imask >> 6) & 0x1);
    os << std::format("IRQ5(TMR1)[{:01b}] ", (imask >> 5) & 0x1);
    os << std::format("IRQ4(TMR0)[{:01b}] ", (imask >> 4) & 0x1);
    os << std::format("IRQ3(DMA)[{:01b}] ", (imask >> 3) & 0x1);
    os << std::format("IRQ2(CDROM)[{:01b}] ", (imask >> 2) & 0x1);
    os << std::format("IRQ1(GPU)[{:01b}] ", (imask >> 1) & 0x1);
    os << std::format("IRQ0(VBLANK)[{:01b}] ", (imask >> 0) & 0x1);

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

    Log::log(std::format("Interrupt write 0x{:0{}X} -> @0x{:08X}",
                         value, 2*sizeof(T), address), Log::Type::INTERRUPTS);

    if (address < 0x1F801074) { // I_STAT
        assert (address + sizeof(T) <= 0x1F801074);
        uint32_t offset = address & 0x00000007;

        // Writing 0 to I_STAT bit clears the bit
        // Writing 1 to I_STAT bit does not change the bit
        T *istat = (T*)(interruptStatusRegister + offset);
        *istat = *istat & value;

    } else { // I_MASK
        assert (address + sizeof(T) <= 0x1F801078);
        uint32_t offset = address & 0x00000003;

        *((T*)(interruptMaskRegister + offset)) = value;
    }

    std::stringstream ss;
    ss << *this;
    Log::log(ss.str(), Log::Type::INTERRUPTS);

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

    Log::log(std::format("Interrupt read @0x{:08X} -> 0x{:0{}X}",
                         address, value, 2*sizeof(T)), Log::Type::INTERRUPTS);

    return value;
}

template uint32_t Interrupts::read(uint32_t address);
template uint16_t Interrupts::read(uint32_t address);
template uint8_t Interrupts::read(uint32_t address);

void Interrupts::checkAndExecuteInterrupts() {
    uint32_t istat = *((uint32_t*)(interruptStatusRegister));
    uint32_t imask = *(((uint32_t*)interruptMaskRegister));

    if ((istat & imask) & 0x3FF) { // one or more interrupts is requested and enabled
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

}

