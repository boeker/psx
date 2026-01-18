#include "timers.h"

#include <cassert>
#include <format>

#include "bus.h"
#include "exceptions/exceptions.h"
#include "util/bit.h"
#include "util/log.h"

using namespace util;

namespace PSX {

Timers::Timers(Bus *bus) {
    this->bus = bus;

    reset();
}

void Timers::reset() {
    for (int i = 0; i < 3; ++i) {
        current[i] = 0;
        mode[i] = 0;
        target[i] = 0;

        remainingCycles[i] = 0;
        resetPulse[i] = false;
        oneShotFired[i] = false;
    }

    for (int i = 0; i < 2; ++i) {
        startConditionMet[i] = false;
    }

    horizontalRetrace = false;
    verticalRetrace = false;
}

void Timers::catchUpToCPU(uint32_t cpuCycles) {
    if (!((mode[0] >> 8) & 0x1)) { // Clock source is system clock
        updateTimer0(cpuCycles);
    }

    if (!((mode[1] >> 8) & 0x1)) { // Clock source is system clock
        updateTimer1(cpuCycles);
    }

    if (!((mode[2] >> 9) & 0x1)) { // Clock source is system clock
        remainingCycles[2] += cpuCycles;
        updateTimer2(remainingCycles[2]);
        remainingCycles[2] = 0;

    } else { // Clock source is system clock / 8
        remainingCycles[2] += cpuCycles;
        updateTimer2(remainingCycles[2] / 8);
        remainingCycles[2] = remainingCycles[2] % 8;
    }
}

void Timers::notifyAboutDots(uint32_t dots) {
    if ((mode[0] >> 8) & 0x1) { // Clock source is dotclock
        updateTimer0(dots);
    }
}

void Timers::notifyAboutHBlankStart() {
    horizontalRetrace = true;

    // Check if timer 0 has to be reset or started
    if (mode[0] & 0x1) { // Synchronize via bit 1 and 2
        uint32_t synchronizationMode = (mode[0] >> 1) & 0x3;

        if (synchronizationMode == 1 || synchronizationMode == 2) { // Reset to 0 at VBlanks/Reset to 0 at VBlanks and pause outside of VBlanks
            current[0] = 0;

        } else if (synchronizationMode == 3) { // Pause until first VBlank, then free run
            startConditionMet[0] = true;
        }
    }

    // Check if timer 1 has to be increased
    if ((mode[1] >> 8) & 0x1) { // Clock source is HBlanks
        updateTimer1(1);
    }
}

void Timers::notifyAboutHBlankEnd() {
    horizontalRetrace = false;
}

void Timers::notifyAboutVBlankStart() {
    verticalRetrace = true;

    if (mode[1] & 0x1) { // Synchronize via bit 1 and 2
        uint32_t synchronizationMode = (mode[1] >> 1) & 0x3;

        if (synchronizationMode == 1 || synchronizationMode == 2) { // Reset to 0 at VBlanks/Reset to 0 at VBlanks and pause outside of VBlanks
            current[1] = 0;

        } else if (synchronizationMode == 3) { // Pause until first VBlank, then free run
            startConditionMet[1] = true;
        }
    }
}

void Timers::notifyAboutVBlankEnd() {
    verticalRetrace = false;
}

void Timers::updateTimer0(uint32_t increase) {
    // We already checked the clock source, no need to do that here

    // Check if we have to reset the current pulse
    checkResetPulse<0>();

    // Increase counter
    increaseTimer0Or1<0>(increase);

    // Check if reset value reached
    checkResetValue<0>();
}

void Timers::updateTimer1(uint32_t increase) {
    // We already checked the clock source, no need to do that here

    // Check if we have to reset the current pulse
    checkResetPulse<1>();

    // Increase counter
    increaseTimer0Or1<1>(increase);

    // Check if reset value reached
    checkResetValue<1>();
}

void Timers::updateTimer2(uint32_t increase) {
    // We already checked the clock source, no need to do that here

    // Check if we have to reset the current pulse
    checkResetPulse<2>();

    // Increase counter
    if (Bit::getBit(mode[2], 0)) { // Synchronize via bit 1 and 2
        uint8_t synchronizationMode = (mode[2] >> 1) & 0x3;

        // Synchronization mode 0 and 3 are stop at current value
        // Nothing to do for those

        if (synchronizationMode == 1 || synchronizationMode == 2) { // Free run
            current[2] += increase;
        }

    } else {
        // Free run
        current[2] += increase;
    }

    // Check if reset value reached
    checkResetValue<2>();
}

template<uint32_t N>
void Timers::checkResetPulse() {
    if (resetPulse[N]) {
        Bit::setBit(mode[N], 10);
        resetPulse[N] = false;
    }
}

template<uint32_t N>
void Timers::increaseTimer0Or1(uint32_t increase) {
    if (Bit::getBit(mode[N], 0)) { // Synchronize via bit 1 and 2
        uint32_t synchronizationMode = (mode[N] >> 1) & 0x3;

        if (synchronizationMode == 0) { // Pause during H/VBlanks
            if (!verticalRetrace) {
                current[N] += increase;
            }

        } else if (synchronizationMode == 1) { // Reset to 0 at H/VBlanks
            current[N] += increase;

        } else if (synchronizationMode == 2) { // Reset to 0 at H/VBlanks and pause outside of H/VBlanks
            if (verticalRetrace) {
                current[N] += increase;
            }

        } else if (synchronizationMode == 3) { // Pause until first H/VBlank, then free run
            if (startConditionMet[N]) {
                current[N] += increase;
            }
        }

    } else { // Free run
        current[N] += increase;
    }
}

template<uint32_t N>
void Timers::checkResetValue() {
    if (Bit::getBit(mode[N], 3) && (current[N] >= target[N])) { // Reset after counter reaches target
        current[N] = 0;
        Bit::setBit(mode[N], 11); // Bit 11 marks reached target value, reset after reading
        handleInterrupt<N>();

    } else if (!Bit::getBit(mode[N], 3) && (current[N] >= 0xFFFF)) { // Reset after counter reaches 0xFFFF
        current[N] = 0;
        Bit::setBit(mode[N], 12); // Bit 12 marks reached 0xFFFF, reset after reading
        handleInterrupt<N>();
    }
}

template<uint32_t N>
void Timers::handleInterrupt() {
    if (Bit::getBit(mode[N], 4)) { // IRQ when target reached
        if (Bit::getBit(mode[N], 7)) { // Toggle from 1 to 0 and back
            // Toggle bit 10, issue interrupt if it goes from 1 to 0

            if ((Bit::getBit(mode[N], 6) || !oneShotFired[N]) && Bit::getBit(mode[N], 10)) {
                Bit::clearBit(mode[N], 10);
                oneShotFired[N] = true;
                LOG_TMR(std::format("Timer {:d} is issuing interrupt", N));
                bus->interrupts.notifyAboutInterrupt(INTERRUPT_BIT_TMR0 + N);

            } else if (Bit::getBit(mode[N], 6) && oneShotFired[N]) { // Toggle back from 0 to 1 (if we toggled to 0 before)
                Bit::setBit(mode[N], 10);
            }

        } else { // Pulse from 1 to 0
            // Short pulse of bit 10 to 0, issue interrupt if it goes from 1 to 0
            // This is the "normal" mode
            // How do we realize this?
            // Use resetPulse to mark that we are currently pulsing
            // In the next update, we check if we are pulsing and reset the bit if necessary

            if (Bit::getBit(mode[N], 10)) {
                Bit::clearBit(mode[N], 10);
                resetPulse[N] = true;
                bus->interrupts.notifyAboutInterrupt(INTERRUPT_BIT_TMR0 + N);
            }
        }
    }
}

template <>
void Timers::write(uint32_t address, uint16_t value) {
    assert ((address & 0xFFFFFF00) == 0x1F801100);

    uint32_t noNumberAddress = address & 0xFFFFFF0F;
    uint32_t number = (address & 0x000000F0) >> 4;
    assert (number <= 2);

    if (noNumberAddress == 0x1F801100) { // Current value
        LOGT_TMR(std::format("Write to timer: 0x{:08X} -> @0x{:08X}: timer {:d} current value", value, address, number));
        current[number] = value;

    } else if (noNumberAddress == 0x1F801104) { // Mode
        LOGT_TMR(std::format("Write to timer: 0x{:08X} -> @0x{:08X}: timer {:d} mode", value, address, number));
        mode[number] = value & 0x3FF;
        if (value & (1 << 11)) { // Interrupt Request? 1 -> No, i.e., writing 1 resets interrupt request. 0 cannot be written.
            mode[number] = mode[number] | (1 << 11);
        }
        current[number] = 0; // reset counter
        oneShotFired[number] = false;
        startConditionMet[number] = false;

    } else if (noNumberAddress == 0x1F801108) { // Target value
        LOGT_TMR(std::format("Write to timer: 0x{:08X} -> @0x{:08X}: timer {:d} target value", value, address, number));
        target[number] = value;

    } else {
        LOG_TMR(std::format("Invalid write to Timer: write 0x{:08X} -> @0x{:08X}", value, address));
    }
}

template <> void Timers::write(uint32_t address, uint32_t value) {
    write<uint16_t>(address, (uint32_t)value);
}

template <> void Timers::write(uint32_t address, uint8_t value) {
    throw exceptions::UnimplementedAddressingError(std::format("byte write to timer @0x{:08X}", address));
}

template <>
uint16_t Timers::read(uint32_t address) {
    assert (address & 0xFFFFFF00 == 0x1F801100);

    uint32_t noNumberAddress = address & 0xFFFFFF0F;
    uint32_t number = (address & 0x000000F0) >> 4;
    assert (number <= 2);

    uint16_t value = 0;
    if (noNumberAddress == 0x1F801100) { // Current value
        value = current[number];
        LOGT_TMR(std::format("Read from timer: @0x{:08X} -0x{:08X}->: timer {:d} current value", address, value, number));
        LOGT_TMR(std::format("At PC 0x{:08X}", bus->cpu.regs.getPC()));

    } else if (noNumberAddress == 0x1F801104) { // Mode
        value = mode[number];
        LOGT_TMR(std::format("Read from timer: @0x{:08X} -0x{:08X}->: timer {:d} mode", address, value, number));
        mode[number] = value & ~(0x3 << 11); // Reset bits 11 and 12 (reached target/0xFFFF)

    } else if (noNumberAddress == 0x1F801108) { // Target value
        value = target[number];
        LOGT_TMR(std::format("Read from timer: @0x{:08X} -0x{:08X}->: timer {:d} target value", address, value, number));

    } else {
        LOG_TMR(std::format("Invalid read to Timer: write 0x{:08X} -> @0x{:08X}", value, address));
    }

    //return value;
    return 0;
}

template <> uint32_t Timers::read(uint32_t address) {
    return (uint32_t)read<uint16_t>(address);
}

template <> uint8_t Timers::read(uint32_t address) {
    throw exceptions::UnimplementedAddressingError(std::format("byte read from timer @0x{:08X}", address));
}

}

