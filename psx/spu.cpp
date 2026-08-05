#include "spu.h"

#include <cassert>
#include <cstring>
#include <format>

#include "util/log.h"
#include "exceptions/exceptions.h"

using namespace util;

namespace PSX {

SPU::SPU()
    : ram(std::make_unique<uint8_t[]>(SPU_RAM_SIZE)) {
    reset();
}

void SPU::reset() {
    std::memset(ram.get(), 0, SPU_RAM_SIZE);

    irq_address_register = 0;
    data_transfer_address_register = 0;
    control_register = 0;
    data_transfer_control_register = 0;
    status_register = 0;
}

void SPU::handle_control_write(uint32_t address, uint16_t value) {
    assert(address >= 0x1F80'1DA0 && address < 0x1F80'1DBF);
    assert((address & 1) == 0);

    uint32_t offset = address & 0x0000'00FF;
    if (offset == 0x0A4) {
        LOGW_SPU(std::format("Unimplemented write to RAM IRQ Address @0x{:08X}", address));
        // TODO
    } else if (offset == 0xA6) {
        LOGW_SPU(std::format("Unimplemented write to RAM Data Transfer Address @0x{:08X}", address));
        // TODO
    } else if (offset == 0xA8) {
        LOGW_SPU(std::format("Unimplemented write to RAM Data Transfer Queue @0x{:08X}", address));
        // TODO
    } else if (offset == 0xAA) {
        LOGW_SPU(std::format("Unimplemented write to Control Register @0x{:08X}", address));
        // TODO
    } else if (offset == 0xAC) {
        LOGW_SPU(std::format("Unimplemented write to RAM Data Transfer Control @0x{:08X}", address));
        // TODO
    } else if (offset == 0xAE) {
        LOGW_SPU(std::format("Unimplemented write to Status Register @0x{:08X}", address));
        // TODO
    } else {
        LOGW_SPU(std::format("Write to unknown control register @0x{:08X}", address));
    }
}

uint16_t SPU::handle_control_read(uint32_t address) {
    assert(address >= 0x1F80'1DA0 && address < 0x1F80'1DBF);
    assert((address & 1) == 0);

    uint32_t offset = address & 0x0000'00FF;
    if (offset == 0x0A4) {
        LOGW_SPU(std::format("Unimplemented read from RAM IRQ Address @0x{:08X}", address));
        // TODO
    } else if (offset == 0xA6) {
        LOGW_SPU(std::format("Unimplemented read from RAM Data Transfer Address @0x{:08X}", address));
        // TODO
    } else if (offset == 0xA8) {
        LOGW_SPU(std::format("Unimplemented read from RAM Data Transfer Queue @0x{:08X}", address));
        // TODO
    } else if (offset == 0xAA) {
        LOGW_SPU(std::format("Unimplemented read from Control Register @0x{:08X}", address));
        // TODO
    } else if (offset == 0xAC) {
        LOGW_SPU(std::format("Unimplemented read from RAM Data Transfer Control @0x{:08X}", address));
        // TODO
    } else if (offset == 0xAE) {
        LOGW_SPU(std::format("Unimplemented read from Status Register @0x{:08X}", address));
        // TODO
    } else {
        LOGW_SPU(std::format("Read from unknown control register @0x{:08X}", address));
    }

    return 0;
}

template <>
void SPU::write(uint32_t address, uint32_t value) {
    throw exceptions::UnimplementedAddressingError(std::format("SPU: word write @0x{:08X}", address));
}

template <>
void SPU::write(uint32_t address, uint16_t value) {
    assert(address >= 0x1F80'1C00 && address < 0x1F80'1FFF);
    assert((address & 1) == 0); // Only aligned writes supported for now

    uint32_t offset = address & 0x0000'FFFF;
    if (offset < 0x1D7F) {
        LOGW_SPU(std::format("Unimplemented write to voice register @0x{:08X}", address));
    } else if (offset < 0x1D87) {
        LOGW_SPU(std::format("Unimplemented write to volume register @0x{:08X}", address));
    } else if (offset < 0x1D9F) {
        LOGW_SPU(std::format("Unimplemented write to voice flags @0x{:08X}", address));
    } else if (offset < 0x1DBF) {
        handle_control_write(address, value);
    } else if (offset < 0x1DFF) {
        LOGW_SPU(std::format("Unimplemented write to reverb configuration area @0x{:08X}", address));
    } else if (offset < 0x1E5F) {
        LOGW_SPU(std::format("Unimplemented write to internal voice registers @0x{:08X}", address));
    } else if (offset < 0x1FFF) {
        LOGW_SPU(std::format("Unimplemented write to unknown area @0x{:08X}", address));
    }
}

template <>
void SPU::write(uint32_t address, uint8_t value) {
    throw exceptions::UnimplementedAddressingError(std::format("SPU: byte write @0x{:08X}", address));
}

template <>
uint32_t SPU::read(uint32_t address) {
    throw exceptions::UnimplementedAddressingError(std::format("SPU: word read @0x{:08X}", address));
    return 0;
}

template <>
uint16_t SPU::read(uint32_t address) {
    assert(address >= 0x1F80'1C00 && address < 0x1F80'1FFF);
    assert((address & 1) == 0); // Only aligned reads supported for now

    uint16_t value = 0;
    uint32_t offset = address & 0x0000'FFFF;
    if (offset < 0x1D7F) {
        LOGW_SPU(std::format("Unimplemented read from voice register @0x{:08X}", address));
    } else if (offset < 0x1D87) {
        LOGW_SPU(std::format("Unimplemented read from volume register @0x{:08X}", address));
    } else if (offset < 0x1D9F) {
        LOGW_SPU(std::format("Unimplemented read from voice flags @0x{:08X}", address));
    } else if (offset < 0x1DBF) {
        value = handle_control_read(address);
    } else if (offset < 0x1DFF) {
        LOGW_SPU(std::format("Unimplemented read from reverb configuration area @0x{:08X}", address));
    } else if (offset < 0x1E5F) {
        LOGW_SPU(std::format("Unimplemented read from internal voice registers @0x{:08X}", address));
    } else if (offset < 0x1FFF) {
        LOGW_SPU(std::format("Unimplemented read from unknown area @0x{:08X}", address));
    }

    return value;
}

template <>
uint8_t SPU::read(uint32_t address) {
    throw exceptions::UnimplementedAddressingError(std::format("SPU: byte read @0x{:08X}", address));
    return 0;
}

}

