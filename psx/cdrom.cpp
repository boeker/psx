#include "cdrom.h"

#include <cassert>
#include <format>

#include "exceptions/exceptions.h"
#include "util/log.h"

using namespace util;

namespace PSX {

CDROM::CDROM() {
    reset();
}

void CDROM::reset() {
    index = 0;
}

uint8_t CDROM::getStatusRegister() {
    return (0 << CDROMSTAT_BUSYSTS)
           || (0 << CDROMSTAT_DRQSTS)
           || (0 << CDROMSTAT_RSLRRDY)
           || (1 << CDROMSTAT_PRMWRDY)
           || (1 << CDROMSTAT_PRMEMPT)
           || (0 << CDROMSTAT_ADPBUSY)
           || ((index & 3) << CDROMSTAT_INDEX0);
}

template <>
void CDROM::write(uint32_t address, uint32_t value) {
    throw exceptions::UnimplementedAddressingError(std::format("word write @0x{:08X}", address));
}

template <>
void CDROM::write(uint32_t address, uint16_t value) {
    throw exceptions::UnimplementedAddressingError(std::format("half-word write @0x{:08X}", address));
}

template <>
void CDROM::write(uint32_t address, uint8_t value) {
    assert ((address >= 0x1F801800) && (address < 0x1F801804));

    if (address == 0x1F801800) { // status register
        LOG_CDROM(std::format("Write to status register: 0x{:0{}X}", value, 2*sizeof(value)));
        index = value & 3; // only the index can be written to

    } else if (address == 0x1F801802) {
        if (index == 1) { // interrupt enable register
            LOG_CDROM(std::format("Write to interrupt enable register: 0x{:0{}X}", value, 2*sizeof(value)));
            interruptEnableRegister = value & 0x1F; // bits 5--7 are unused

        } else {
            LOG_CDROM(std::format("Unimplemented write 0x{:0{}X} -> @0x{:08X}, index {:d}", value, 2*sizeof(value), address, index));
        }

    } else if (address == 0x1F801803) {
        if (index == 1) { // interrupt flag register
            LOG_CDROM(std::format("Write to interrupt flag register: 0x{:0{}X}", value, 2*sizeof(value)));
            // Writing 1 to bits 0--4 resets them
            interruptFlagRegister = interruptFlagRegister & ~(value & 0x1F);

            // TODO Writing 1 to bit 6 resets parameter queue

        } else {
            LOG_CDROM(std::format("Unimplemented write 0x{:0{}X} -> @0x{:08X}, index {:d}", value, 2*sizeof(value), address, index));
        }

    } else {
        LOG_CDROM(std::format("Unimplemented write 0x{:0{}X} -> @0x{:08X}, index {:d}", value, 2*sizeof(value), address, index));
    }
}

template <>
uint32_t CDROM::read(uint32_t address) {
    throw exceptions::UnimplementedAddressingError(std::format("word read @0x{:08X}", address));
}

template <>
uint16_t CDROM::read(uint32_t address) {
    throw exceptions::UnimplementedAddressingError(std::format("half-word read @0x{:08X}", address));
}

template <>
uint8_t CDROM::read(uint32_t address) {
    assert ((address >= 0x1F801800) && (address < 0x1F801804));

    uint8_t value = 0;

    if (address == 0x1F801800) { // status register
        value = getStatusRegister();

        LOG_CDROM(std::format("Read from status register: 0x{:0{}X}", value, 2*sizeof(value)));

    } else {
        LOG_CDROM(std::format("Unimplemented read @0x{:08X}, index {:d}", address, index));
    }

    return value;
}

}

