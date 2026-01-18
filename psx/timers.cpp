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

        resetPulse[i] = false;
        isToggling[i] = false;
        oneShotFired[i] = false;
        remainingCycles[i] = 0;
    }

    horizontalRetrace = false;
    verticalRetrace = false;

    timer1HadVBlank = false;
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
            timer1HadVBlank = true;
        }

    }
}

void Timers::notifyAboutVBlankEnd() {
    verticalRetrace = false;
}

void Timers::updateTimer0(uint32_t increase) {
    
}

void Timers::updateTimer1(uint32_t increase) {
    // We already checked the clock source, no need to do that here
    // Check if we have to reset the current pulse

    if (resetPulse[1]) {
        Bit::setBit(mode[1], 10);
        resetPulse[1] = false;
    }

    // Increase counter
    if (mode[1] & 0x1) { // Synchronize via bit 1 and 2
        uint32_t synchronizationMode = (mode[1] >> 1) & 0x3;

        if (synchronizationMode == 0) { // Pause during VBlanks
            if (!verticalRetrace) {
                current[1] += increase;
            }

        } else if (synchronizationMode == 1) { // Reset to 0 at VBlanks
            current[1] += increase;

        } else if (synchronizationMode == 2) { // Reset to 0 at VBlanks and pause outside of VBlanks
            if (verticalRetrace) {
                current[1] += increase;
            }

        } else if (synchronizationMode == 3) { // Pause until first VBlank, then free run
            if (timer1HadVBlank) {
                current[1] += increase;
            }
        }

    } else {
        // Free run
        current[1] += increase;
    }

    // Check if reset value reached
    if (Bit::getBit(mode[1], 3) && (current[1] >= target[1])) { // Reset after counter reaches target
        current[1] = 0;
        Bit::setBit(mode[1], 11); // Bit 11 marks reached target value, reset after reading
        handleInterruptTimer1();

    } else if (!Bit::getBit(mode[1], 3) && (current[1] >= 0xFFFF)) { // Reset after counter reaches 0xFFFF
        current[1] = 0;
        Bit::setBit(mode[1], 12); // Bit 12 marks reached 0xFFFF, reset after reading
        handleInterruptTimer1();
    }
}

void Timers::handleInterruptTimer1() {
    if ((mode[1] >> 4) & 0x1) { // IRQ when target reached
        if ((mode[1] >> 7) & 0x1) { // Toggle
            // Toggle bit 10, issue interrupt if it goes from 1 to 0

            if ((Bit::getBit(mode[1], 6) || !oneShotFired[1]) && Bit::getBit(mode[1], 10)) {
                Bit::clearBit(mode[1], 10);
                oneShotFired[1] = true;
                isToggling[1] = true;
                LOG_TMR(std::format("Timer 1 is issuing interrupt"));
                bus->interrupts.notifyAboutInterrupt(INTERRUPT_BIT_TMR1);

            } else if (Bit::getBit(mode[1], 6) && isToggling[1]) {
                Bit::setBit(mode[1], 10);
            }

        } else { // Pulse
            // Short pulse of bit 10 to 0, issue interrupt if it goes from 1 to 0
            // This is the "normal" mode
            // How do we realize this? In the next update, we check if we have a pulse going and reset the bit if necessary

            if (Bit::getBit(mode[1], 10)) {
                Bit::clearBit(mode[1], 10);
                resetPulse[1] = true;
                bus->interrupts.notifyAboutInterrupt(INTERRUPT_BIT_TMR1);
            }
        }
    }
}

void Timers::updateTimer2(uint32_t increase) {
    //remainingCycles[2] += cpuCycles;

    //// Determine increase
    //uint32_t increase = 0;
    //if ((mode[2] >> 9) & 0x1) { // Clock source is system clock / 8
    //    increase = remainingCycles[2] / 8;
    //    remainingCycles[2] -= increase * 8;

    //} else {
    //    increase = remainingCycles[2];
    //    remainingCycles[2] = 0;
    //}

    //// Increase counter
    //if (mode[2] & 0x1) { // Synchronize via bit 1 and 2
    //    uint8_t synchronizationMode = (mode[2] >> 1) & 0x3;
    //    if (synchronizationMode == 1 || synchronizationMode == 2) { // Free run
    //        current[2] += increase;
    //    }
    //    // Synchronization mode 0 and 3 are stop at current value

    //} else {
    //    // Free run
    //    current[2] += increase;
    //}

    //// Check if reset value reached
    //if ((mode[2] >> 3) & 0x1) { // Reset after counter reaches target
    //    if (current[2] >= target[2]) {
    //        current[2] = 0;

    //        mode[2] = mode[2] | (1 << 11); // Bit 11 marks reached target value, reset after reading

    //        if ((mode[3] >> 4) & 0x1) { // IRQ when target reached
    //            // TODO: Handle bit 6 one-shot/repeat
    //            // TODO: Handle bit 7 pulse/toggle
    //            LOGV_TMR(std::format("Timer 2 is issuing interrupt"));
    //            mode[2] = mode[2] & ~(1 << 10); // Bit 10 going from 1 to 0 signalizes interrupt
    //            bus->interrupts.notifyAboutInterrupt(INTERRUPT_BIT_TMR2);
    //        }
    //    }

    //} else { // Reset after counter reaches 0xFFFF
    //    if (current[2] >= 0xFFFF) {
    //        current[2] = 0;

    //        mode[2] = mode[2] | (1 << 12); // Bit 12 marks reached 0xFFFF, reset after reading

    //        if ((mode[2] >> 5) & 0x1) { // IRQ when 0xFFFF reached
    //            // TODO: Handle bit 6 one-shot/repeat
    //            // TODO: Handle bit 7 pulse/toggle
    //            LOGV_TMR(std::format("Timer 2 is issuing interrupt"));
    //            mode[2] = mode[2] & ~(1 << 10); // Bit 10 going from 1 to 0 signalizes interrupt
    //            bus->interrupts.notifyAboutInterrupt(INTERRUPT_BIT_TMR2);
    //        }
    //    }
    //}
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
        isToggling[number] = false;

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

