#include "interrupts.h"

#include <cassert>
#include <cstring>
#include <format>
#include <sstream>

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

Interrupts::Interrupts() {
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

    if (address < 0x1F801074) {
        assert (address + sizeof(T) <= 0x1F801074);
        uint32_t offset = address & 0x00000007;

        *((T*)(interruptStatusRegister + offset)) = value;

    } else {
        assert (address + sizeof(T) <= 0x1F801078);
        uint32_t offset = address & 0x00000003;

        *((T*)(interruptMaskRegister + offset)) = value;
    }

    std::stringstream ss;
    ss << *this;
    Log::log(ss.str(), Log::Type::INTERRUPTS);
}

template void Interrupts::write(uint32_t address, uint32_t value);
template void Interrupts::write(uint32_t address, uint16_t value);
template void Interrupts::write(uint32_t address, uint8_t value);

template <typename T>
T Interrupts::read(uint32_t address) {
    Log::log(std::format("Interrupts unimplemented: read @0x{:08X}", address), Log::Type::INTERRUPTS);
    return 0;
}

template uint32_t Interrupts::read(uint32_t address);
template uint16_t Interrupts::read(uint32_t address);
template uint8_t Interrupts::read(uint32_t address);

}

