#include "spu.h"

#include <cassert>
#include <cstring>
#include <format>

#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>

#include "bus.h"
#include "util/bit.h"
#include "util/log.h"
#include "exceptions/exceptions.h"

using namespace util;

namespace PSX {

SPU::SPU(Bus *bus)
    : bus(bus),
      ram(std::make_unique<uint8_t[]>(SPU_RAM_SIZE)) {
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    SDL_Init(SDL_INIT_AUDIO);
    audio_spec.format = SDL_AUDIO_U8;
    audio_spec.channels = 2;
    audio_spec.freq = 44100;
    audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audio_spec, nullptr, nullptr);
    if (!audio_stream) {
        LOGW_SPU(std::format("Audio stream could not be opened: {:s}", SDL_GetError()));
    }
    if (!SDL_ResumeAudioStreamDevice(audio_stream)) {
        LOGW_SPU(std::format("Audio stream could not be started"));
    }

    //// Short audio test
    //for (uint32_t i = 0; i < 10; ++i) {
    //    std::vector<uint8_t> foo;
    //    for (uint32_t t = 0; t < 1000; ++t) {
    //        foo.push_back(0xFF);
    //    }
    //    for (uint32_t t = 0; t < 1000; ++t) {
    //        foo.push_back(0x0);
    //    }
    //    if (!SDL_PutAudioStreamData(audio_stream, foo.data(), 2000)) {
    //        LOGW_SPU(std::format("Error writing data to audio stream"));
    //    }
    //}

    reset();
}

SPU::~SPU() {
    SDL_DestroyAudioStream(audio_stream);
}

void SPU::reset() {
    std::memset(ram.get(), 0, SPU_RAM_SIZE);

    irq_address_register = 0;
    data_transfer_address_register = 0;
    data_transfer_address = 0;
    data_transfer_queue.clear();
    control_register = 0;
    data_transfer_control_register = 0;
    status_register = 0;
}

bool SPU::dma_write_to_spu_requested() const {
    return Bit::getBit(status_register, SPU_STATUS_TRANSFER_WRITE_REQUEST);
}

bool SPU::dma_read_from_spu_requested() const {
    return Bit::getBit(status_register, SPU_STATUS_TRANSFER_READ_REQUEST);
}

void SPU::start_dma_transfer() {
    LOGV_SPU(std::format("Preparing DMA transfer to/from SPU"));
    Bit::setBit(status_register, SPU_STATUS_TRANSFER_BUSY);
    if (data_transfer_control_register != 0x0004) { // 0x0004 is normal transfer
        LOGW_SPU(std::format("Unimplemented transfer type 0x{:04X}", data_transfer_control_register));
    }
    LOGV_SPU(std::format("DMA transfer address is 0x{:05X}", data_transfer_address));
}

void SPU::write_to_ram(uint16_t value) {
    LOGT_SPU(std::format("Write 0x{:04X} to SPU RAM @0x{:05X}", value, data_transfer_address));
    *((uint16_t*)(ram.get() + data_transfer_address)) = value;
    data_transfer_address += 2;
}

uint16_t SPU::read_from_ram() {
    uint16_t value = *((uint16_t*)(ram.get() + data_transfer_address));
    LOGT_SPU(std::format("Read 0x{:04X} from SPU RAM @0x{:05X}", value, data_transfer_address));
    data_transfer_address += 2;
    return value;
}

void SPU::finish_dma_transfer() {
    LOGV_SPU(std::format("Finishing DMA transfer to/from SPU"));
    Bit::clearBit(status_register, SPU_STATUS_TRANSFER_BUSY);
    issue_interrupt_if_enabled();
}

void SPU::handle_control_write(uint32_t address, uint16_t value) {
    assert(address >= 0x1F80'1DA0 && address < 0x1F80'1DBF);
    assert((address & 1) == 0);

    uint32_t offset = address & 0x0000'00FF;
    if (offset == 0x0A4) {
        LOGV_SPU(std::format("Setting RAM IRQ Address to 0x{:04X}", value));
        irq_address_register = value;
        // TODO: Implement interrupts via this register
    } else if (offset == 0xA6) {
        LOGV_SPU(std::format("Setting RAM Data Transfer Address to 0x{:04X}", value));
        data_transfer_address_register = value;
        data_transfer_address = 8 * data_transfer_address_register;
        LOGV_SPU(std::format("Updated actual address to 0x{:05X}", data_transfer_address));
    } else if (offset == 0xA8) {
        LOGT_SPU(std::format("Write to RAM Data Transfer Queue: 0x{:04X}", value));
        if (!data_transfer_queue.push(value)) {
            LOGW_SPU(std::format("Write to full RAM Data Transfer Queue!"));
        }
    } else if (offset == 0xAA) {
        control_register = value;

        // Update Status Register
        // TODO: Update Bit 11 (Half Bit) after transfer?
        // Bit 6
        // Check if pending interrupt is acknowledged
        if (Bit::getBit(status_register, SPU_STATUS_IRQ9_FLAG)
            && !Bit::getBit(value, SPU_CONTROL_IRQ9_ENABLE)) {
            Bit::clearBit(status_register, SPU_STATUS_IRQ9_FLAG);
            LOGV_SPU(std::format("Acknowledged IRQ9"));
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
                LOGV_SPU(std::format("Stop SPU RAM transfer"));
                break;
            case 1: // manual write
                Bit::clearBit(status_register, SPU_STATUS_TRANSFER_READ_REQUEST);
                Bit::clearBit(status_register, SPU_STATUS_TRANSFER_WRITE_REQUEST);
                perform_manual_transfer();
                break;
            case 2: // DMA write (to SPU RAM)
                Bit::clearBit(status_register, SPU_STATUS_TRANSFER_BUSY);
                Bit::clearBit(status_register, SPU_STATUS_TRANSFER_READ_REQUEST);
                Bit::setBit(status_register, SPU_STATUS_TRANSFER_WRITE_REQUEST);
                break;
            case 3: // DMA read (from SPU RAM)
                Bit::clearBit(status_register, SPU_STATUS_TRANSFER_BUSY);
                Bit::setBit(status_register, SPU_STATUS_TRANSFER_READ_REQUEST);
                Bit::clearBit(status_register, SPU_STATUS_TRANSFER_WRITE_REQUEST);
                break;
            case 4:
                assert(false);
        }

    } else if (offset == 0xAC) {
        LOGV_SPU(std::format("Setting RAM Data Transfer Control to 0x{:04X}", value));
        data_transfer_control_register = value;
    } else if (offset == 0xAE) {
        LOGW_SPU(std::format("Attempted write to read-only Status Register @0x{:08X}", address));
    } else {
        LOGW_SPU(std::format("Write to unknown control register @0x{:08X}", address));
    }
}

uint16_t SPU::handle_control_read(uint32_t address) {
    assert(address >= 0x1F80'1DA0 && address < 0x1F80'1DBF);
    assert((address & 1) == 0);

    uint16_t value = 0;
    uint32_t offset = address & 0x0000'00FF;
    if (offset == 0x0A4) {
        value = irq_address_register;
        LOGV_SPU(std::format("Read from RAM IRQ Address: 0x{:04X}", value));
    } else if (offset == 0xA6) {
        value = data_transfer_address_register;
        LOGV_SPU(std::format("Read from RAM Data Transfer Address: 0x{:04X}", value));
    } else if (offset == 0xA8) {
        LOGW_SPU(std::format("Attempted read from RAM Data Transfer Queue"));
    } else if (offset == 0xAA) {
        LOGV_SPU(std::format("Read from Control Register @0x{:08X}", address));
        // TODO Explain
        return control_register;
    } else if (offset == 0xAC) {
        value = data_transfer_control_register;
        LOGV_SPU(std::format("Read from RAM Data Transfer Control: 0x{:04X}", value));
    } else if (offset == 0xAE) {
        LOGV_SPU(std::format("Read from Status Register @0x{:08X}", address));
        // TODO Explain
        return status_register;
    } else {
        LOGW_SPU(std::format("Read from unknown control register @0x{:08X}", address));
    }

    return value;
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

void SPU::perform_manual_transfer() {
    Bit::setBit(status_register, SPU_STATUS_TRANSFER_BUSY);
    LOGV_SPU(std::format("Manual transfer to SPU RAM to 0x{:05X}", data_transfer_address));

    if (data_transfer_control_register != 0x0004) { // 0x0004 is normal transfer
        LOGW_SPU(std::format("Unimplemented transfer type 0x{:04X}", data_transfer_control_register));
    }

    while (!data_transfer_queue.is_empty()) {
        write_to_ram(data_transfer_queue.pop());
    }

    Bit::clearBit(status_register, SPU_STATUS_TRANSFER_BUSY);
}

void SPU::issue_interrupt_if_enabled() {
    LOGV_SPU(std::format("Checking if interrupt is enabled"));
    if (Bit::getBit(control_register, SPU_CONTROL_ENABLE)
        && Bit::getBit(control_register, SPU_CONTROL_IRQ9_ENABLE)) {
        LOGV_SPU(std::format("Issuing interrupt"));
        Bit::setBit(status_register, SPU_STATUS_IRQ9_FLAG);
        bus->interrupts.notifyAboutInterrupt(INTERRUPT_BIT_SPU);
    }
}

}

