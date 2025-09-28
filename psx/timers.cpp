#include "timers.h"

#include <cassert>
#include <format>

#include "bus.h"
#include "exceptions/exceptions.h"
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
    }
}

void Timers::catchUpToCPU(uint32_t cpuCycles) {
    // Timer 0
    // TODO

    // Timer 1
    // TODO

    // Timer 2
    remainingCycles[2] += cpuCycles;

    // Determine increase
    uint32_t increase = 0;
    if ((mode[2] >> 9) & 0x1) { // Clock source is system clock / 8
        increase = remainingCycles[2] / 8;
        remainingCycles[2] -= increase * 8;

    } else {
        increase = remainingCycles[2];
        remainingCycles[2] = 0;
    }

    // Increase counter
    if (mode[2] & 0x1) { // Synchronize via bit 1 and 2
        uint8_t synchronizationMode = (mode[2] >> 1) & 0x3;
        if (synchronizationMode == 1 || synchronizationMode == 2) { // Free run
            current[2] += increase;
        }
        // Synchronization mode 0 and 3 are stop at current value

    } else {
        // Free run
        current[2] += increase;
    }

    // Check if reset value reached
    if ((mode[3] >> 3) & 0x1) { // Reset after counter reaches target
        if (current[2] >= target[2]) {
            current[2] = 0;

            mode[2] = mode[2] | (1 << 11); // Bit 11 marks reached target value, reset after reading

            if ((mode[3] >> 4) & 0x1) { // IRQ when target reached
                // TODO: Handle bit 6 one-shot/repeat
                // TODO: Handle bit 7 pulse/toggle
                LOGV_TMR(std::format("Timer 2 is issuing interrupt"));
                bus->interrupts.notifyAboutInterrupt(INTERRUPT_BIT_TMR2);
            }
        }

    } else { // Reset after counter reaches 0xFFFF
        if (current[2] >= 0xFFFF) {
            current[2] = 0;

            mode[2] = mode[2] | (1 << 12); // Bit 12 marks reached 0xFFFF, reset after reading

            if ((mode[3] >> 5) & 0x1) { // IRQ when 0xFFFF reached
                // TODO: Handle bit 6 one-shot/repeat
                // TODO: Handle bit 7 pulse/toggle
                LOGV_TMR(std::format("Timer 2 is issuing interrupt"));
                bus->interrupts.notifyAboutInterrupt(INTERRUPT_BIT_TMR2);
            }
        }
    }
}

template <>
void Timers::write(uint32_t address, uint16_t value) {
    assert (address & 0xFFFFFF00 == 0x1F801100);

    uint32_t noNumberAddress = address & 0xFFFFFF0F;
    uint32_t number = (address & 0x000000F0) >> 4;
    assert (number <= 2);

    if (noNumberAddress == 0x1F801100) { // Current value
        LOGT_TMR(std::format("Write to timer: 0x{:08X} -> @0x{:08X}: timer {:d} current value", value, address, number));
        current[number] = value;

    } else if (noNumberAddress == 0x1F801104) { // Mode
        LOGT_TMR(std::format("Write to timer: 0x{:08X} -> @0x{:08X}: timer {:d} mode", value, address, number));
        mode[number] = value;

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

    } else if (noNumberAddress == 0x1F801104) { // Mode
        value = mode[number];
        LOGT_TMR(std::format("Read from timer: @0x{:08X} -0x{:08X}->: timer {:d} mode", address, value, number));

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

