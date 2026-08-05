#include "spu.h"

#include <cassert>
#include <cstring>
#include <format>

#include "util/bit.h"
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
        control_register = value;

        // Update Status Register
        // TODO: Update Bit 11 (Half Bit) after transfer?
        // Bit 6
        // Check if pending interrupt is acknowledged
        if (Bit::getBit(status_register, SPU_STATUS_IRQ9_FLAG)
            && !Bit::getBit(value, SPU_CONTROL_IRQ9_ENABLE)) {
            Bit::clearBit(status_register, SPU_STATUS_IRQ9_FLAG);
            LOGT_SPU(std::format("Acknowledged IRQ9"));
        }

        // Bit 7
        Bit::setBit(status_register, SPU_STATUS_TRANSFER_READ_WRITE_REQUEST, Bit::getBit(value, SPU_CONTROL_RAM_TRANSFER_MODE1));
        // Bit 5 to 0
        Bit::setBits<6>(status_register, SPU_STATUS_CD_AUDIO_ENABLE, Bit::getBits<6>(value, SPU_STATUS_CD_AUDIO_ENABLE));
        switch (Bit::getBits<2>(value, SPU_CONTROL_RAM_TRANSFER_MODE0)) {
            case 0: // stop
                Bit::clearBit(status_register, SPU_STATUS_TRANSFER_BUSY);
                Bit::clearBit(status_register, SPU_STATUS_TRANSFER_READ_REQUEST);
                Bit::clearBit(status_register, SPU_STATUS_TRANSFER_WRITE_REQUEST);
                LOGT_SPU(std::format("Stop SPU RAM transfer"));
                break;
            case 1: // manual write
                Bit::setBit(status_register, SPU_STATUS_TRANSFER_BUSY);
                Bit::clearBit(status_register, SPU_STATUS_TRANSFER_READ_REQUEST);
                Bit::clearBit(status_register, SPU_STATUS_TRANSFER_WRITE_REQUEST);
                // TODO Perform manual transfer
                LOGW_SPU(std::format("Unimplemented manual transfer to SPU RAM requested"));
                break;
            case 2: // DMA write (to SPU RAM)
                Bit::clearBit(status_register, SPU_STATUS_TRANSFER_BUSY);
                Bit::clearBit(status_register, SPU_STATUS_TRANSFER_READ_REQUEST);
                Bit::setBit(status_register, SPU_STATUS_TRANSFER_WRITE_REQUEST);
                LOGW_SPU(std::format("Unimplemented DMA transfer to SPU RAM requested"));
                // TODO: Implement DMA transfer
                break;
            case 3: // DMA read (from SPU RAM)
                Bit::clearBit(status_register, SPU_STATUS_TRANSFER_BUSY);
                Bit::setBit(status_register, SPU_STATUS_TRANSFER_READ_REQUEST);
                Bit::clearBit(status_register, SPU_STATUS_TRANSFER_WRITE_REQUEST);
                LOGW_SPU(std::format("Unimplemented DMA transfer from SPU RAM requested"));
                // TODO: Implement DMA transfer
                break;
            case 4:
                assert(false);
        }

    } else if (offset == 0xAC) {
        LOGW_SPU(std::format("Unimplemented write to RAM Data Transfer Control @0x{:08X}", address));
        // TODO
    } else if (offset == 0xAE) {
        LOGW_SPU(std::format("Attempted write to read-only Status Register @0x{:08X}", address));
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
        LOGT_SPU(std::format("Read from Control Register @0x{:08X}", address));
        // TODO Explain
        return control_register;
    } else if (offset == 0xAC) {
        LOGW_SPU(std::format("Unimplemented read from RAM Data Transfer Control @0x{:08X}", address));
        // TODO
    } else if (offset == 0xAE) {
        LOGT_SPU(std::format("Read from Status Register @0x{:08X}", address));
        // TODO Explain
        return status_register;
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

