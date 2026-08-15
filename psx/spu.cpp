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

std::string SPU::get_control_register_explanation(uint16_t reg) {
    std::stringstream ss;

    ss << std::format("ENABLE[{:01b}], ", (reg >> SPU_CONTROL_ENABLE) & 1);
    ss << std::format("MUTE[{:01b}], ", (reg >> SPU_CONTROL_MUTE) & 1);
    ss << std::format("NOISE_FREQUENCY_SHIFT[{:02d}], ", (reg >> SPU_CONTROL_NOISE_FREQUENCY_SHIFT0) & 0xF);
    ss << std::format("NOISE_FREQUENCY_STEP[{:01d}], ", (reg >> SPU_CONTROL_NOISE_FREQUENCY_STEP0) & 0x3);
    ss << std::format("REVERB_MASTER_ENABLE[{:01b}], ", (reg >> SPU_CONTROL_REVERB_MASTER_ENABLE) & 1);
    ss << std::format("IRQ9_ENABLE[{:01b}], ", (reg >> SPU_CONTROL_IRQ9_ENABLE) & 1);
    ss << std::format("RAM_TRANSFER_MODE[{:01X}], ", (reg >> SPU_CONTROL_RAM_TRANSFER_MODE0) & 0x3);
    ss << std::format("EXTERNAL_AUDIO_REVERB[{:01b}], ", (reg >> SPU_CONTROL_EXTERNAL_AUDIO_REVERB) & 1);
    ss << std::format("CD_AUDIO_REVERB[{:01b}], ", (reg >> SPU_CONTROL_CD_AUDIO_REVERB) & 1);
    ss << std::format("EXTERNAL_AUDIO_ENABLE[{:01b}], ", (reg >> SPU_CONTROL_EXTERNAL_AUDIO_ENABLE) & 1);
    ss << std::format("CD_AUDIO_ENABLE[{:01b}]", (reg >> SPU_CONTROL_CD_AUDIO_ENABLE) & 1);

    return ss.str();
}

std::string SPU::get_status_register_explanation(uint16_t reg) {
    std::stringstream ss;

    ss << std::format("CAPTURE_HALF[{:01b}], ", (reg >> SPU_STATUS_CAPTURE_HALF) & 1);
    ss << std::format("TRANSFER_BUSY[{:01b}], ", (reg >> SPU_STATUS_TRANSFER_BUSY) & 1);
    ss << std::format("TRANSFER_READ_REQUEST[{:01b}], ", (reg >> SPU_STATUS_TRANSFER_READ_REQUEST) & 1);
    ss << std::format("TRANSFER_WRITE_REQUEST[{:01b}], ", (reg >> SPU_STATUS_TRANSFER_WRITE_REQUEST) & 1);
    ss << std::format("TRANSFER_READ_WRITE_REQUEST[{:01b}], ", (reg >> SPU_STATUS_TRANSFER_READ_WRITE_REQUEST) & 1);
    ss << std::format("IRQ9_FLAG[{:01b}], ", (reg >> SPU_STATUS_IRQ9_FLAG) & 1);
    ss << std::format("RAM_TRANSFER_MODE[{:01X}], ", (reg >> SPU_STATUS_RAM_TRANSFER_MODE0) & 0x3);
    ss << std::format("EXTERNAL_AUDIO_REVERB[{:01b}], ", (reg >> SPU_STATUS_EXTERNAL_AUDIO_REVERB) & 1);
    ss << std::format("CD_AUDIO_REVERB[{:01b}], ", (reg >> SPU_STATUS_CD_AUDIO_REVERB) & 1);
    ss << std::format("EXTERNAL_AUDIO_ENABLE[{:01b}], ", (reg >> SPU_STATUS_EXTERNAL_AUDIO_ENABLE) & 1);
    ss << std::format("CD_AUDIO_ENABLE[{:01b}]", (reg >> SPU_STATUS_CD_AUDIO_ENABLE) & 1);

    return ss.str();
}

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

void SPU::handle_voice_write(uint32_t address, uint16_t value) {
    assert(address >= 0x1F80'1C00 && address < 0x1F80'1D7F);
    assert((address & 1) == 0);

    uint32_t voice_number = (address & 0x0000'01F0) >> 4;
    uint32_t offset = address & 0x0000'000F;
    if (offset == 0x0) {
        LOGV_SPU(std::format("Write to Voice {:d} Volume (Left): 0x{:04X}", voice_number, value));
        // TODO Handle
    } else if (offset == 0x2) {
        LOGV_SPU(std::format("Write to Voice {:d} Volume (Right): 0x{:04X}", voice_number, value));
        // TODO Handle
    } else if (offset == 0x4) {
        LOGV_SPU(std::format("Write to Voice {:d} ADPCM Sample Rate (VxPitch): 0x{:04X}", voice_number, value));
        // TODO Handle
    } else if (offset == 0x6) {
        LOGV_SPU(std::format("Write to Voice {:d} ADPCM Start Address: 0x{:04X}", voice_number, value));
        // TODO Handle
    } else if (offset == 0x8) {
        LOGV_SPU(std::format("Write to Voice {:d} ADSR (Lower): 0x{:04X}", voice_number, value));
        // TODO Handle
    } else if (offset == 0xA) {
        LOGV_SPU(std::format("Write to Voice {:d} ADSR (Lower): 0x{:04X}", voice_number, value));
        // TODO Handle
    } else if (offset == 0xC) {
        LOGV_SPU(std::format("Write to Voice {:d} ADSR Current Volume: 0x{:04X}", voice_number, value));
        // TODO Handle
    } else if (offset == 0xE) {
        LOGV_SPU(std::format("Write to Voice {:d} ADPCM Repeat Address: 0x{:04X}", voice_number, value));
        // TODO Handle
    } else {
        // Should not be reachable
        assert(false);
    }
}

void SPU::handle_volume_write(uint32_t address, uint16_t value) {
    assert(address >= 0x1F80'1D80 && address < 0x1F80'1D87);
    assert((address & 1) == 0);

    uint32_t offset = address & 0x0000'00FF;
    if (offset == 0x80) {
        LOGV_SPU(std::format("Write to Main Volume (Left): 0x{:04X}", value));
        // TODO Handle
    } else if (offset == 0x82) {
        LOGV_SPU(std::format("Write to Main Volume (Right): 0x{:04X}", value));
        // TODO Handle
    } else if (offset == 0x84) {
        LOGV_SPU(std::format("Write to Reverb Output Volume (Left): 0x{:04X}", value));
        // TODO Handle
    } else if (offset == 0x86) {
        LOGV_SPU(std::format("Write to Reverb Output Volume (Right): 0x{:04X}", value));
        // TODO Handle
    } else {
        // Should not be reachable
        assert(false);
    }
}

void SPU::handle_voice_flags_write(uint32_t address, uint16_t value) {
    assert(address >= 0x1F80'1D88 && address < 0x1F80'1D9F);
    assert((address & 1) == 0);

    uint32_t offset = address & 0x0000'00FF;
    if (offset == 0x88) {
        LOGV_SPU(std::format("Write to Key On (KON) (Voices 0...15): 0x{:04X}", value));
        // TODO Handle: Start Attack, Decay, Sustain
    } else if (offset == 0x8A) {
        LOGV_SPU(std::format("Write to Key On (KON) (Voices 16...23): 0x{:04X}", value));
        // TODO Handle: Switch to Release
    } else if (offset == 0x8C) {
        LOGV_SPU(std::format("Write to Key Off (KOFF) (Voices 0...15): 0x{:04X}", value));
        // TODO Handle
    } else if (offset == 0x8E) {
        LOGV_SPU(std::format("Write to Key Off (KOFF) (Voices 16...23): 0x{:04X}", value));
        // TODO Handle
    } else if (offset == 0x90) {
        LOGV_SPU(std::format("Write to Pitch Modulation Enable (PMON) (Voices 1...15): 0x{:04X}", value));
        // TODO Handle
    } else if (offset == 0x92) {
        LOGV_SPU(std::format("Write to Pitch Modulation Enable (PMON) (Voices 16...23): 0x{:04X}", value));
        // TODO Handle
    } else if (offset == 0x94) {
        LOGV_SPU(std::format("Write to Noise Enable (NON) (Voices 0...15): 0x{:04X}", value));
        // TODO Handle
    } else if (offset == 0x96) {
        LOGV_SPU(std::format("Write to Noise Enable (NON) (Voices 16...23): 0x{:04X}", value));
        // TODO Handle
    } else if (offset == 0x98) {
        LOGV_SPU(std::format("Write to Reverb On (EON) (Voices 0...15): 0x{:04X}", value));
        // TODO Handle
    } else if (offset == 0x9A) {
        LOGV_SPU(std::format("Write to Reverb On (EON) (Voices 16...23): 0x{:04X}", value));
        // TODO Handle
    } else if (offset == 0x9C) {
        LOGW_SPU(std::format("Write to read-only register: Key On/Off Status (Voices 0...15): 0x{:04X}", value));
    } else if (offset == 0x9E) {
        LOGW_SPU(std::format("Write to read-only register: Key On/Off Status (Voices 16...23): 0x{:04X}", value));
    } else {
        // Should not be reachable
        assert(false);
    }
}

void SPU::handle_control_write(uint32_t address, uint16_t value) {
    assert(address >= 0x1F80'1DA0 && address < 0x1F80'1DBF);
    assert((address & 1) == 0);

    uint32_t offset = address & 0x0000'00FF;
    if (offset == 0xA0) {
        LOGW_SPU(std::format("Write to unknown register 0x1F80'1DA0"));
    } else if (offset == 0xA2) {
        LOGV_SPU(std::format("Setting RAM Reverb Work Area Start Address to 0x{:04X}", value));
        // TODO Handle
    } else if (offset == 0xA4) {
        LOGV_SPU(std::format("Setting RAM IRQ Address to 0x{:04X}", value));
        irq_address_register = value;
        // TODO Implement interrupts via this register
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
        LOGV_SPU(std::format("Write to SPU control register:\n{:s}", get_control_register_explanation(value)));
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
                LOGV_SPU(std::format("Setting Transfer Mode: Stop"));
                break;
            case 1: // manual write
                Bit::clearBit(status_register, SPU_STATUS_TRANSFER_READ_REQUEST);
                Bit::clearBit(status_register, SPU_STATUS_TRANSFER_WRITE_REQUEST);
                LOGV_SPU(std::format("Setting Transfer Mode: Manual Transfer"));
                perform_manual_transfer();
                break;
            case 2: // DMA write (to SPU RAM)
                Bit::clearBit(status_register, SPU_STATUS_TRANSFER_BUSY);
                Bit::clearBit(status_register, SPU_STATUS_TRANSFER_READ_REQUEST);
                Bit::setBit(status_register, SPU_STATUS_TRANSFER_WRITE_REQUEST);
                LOGV_SPU(std::format("Setting Transfer Mode: DMA Write to SPU"));
                break;
            case 3: // DMA read (from SPU RAM)
                Bit::clearBit(status_register, SPU_STATUS_TRANSFER_BUSY);
                Bit::setBit(status_register, SPU_STATUS_TRANSFER_READ_REQUEST);
                Bit::clearBit(status_register, SPU_STATUS_TRANSFER_WRITE_REQUEST);
                LOGV_SPU(std::format("Setting Transfer Mode: DMA Read from SPU"));
                break;
            case 4:
                assert(false);
        }

    } else if (offset == 0xAC) {
        LOGV_SPU(std::format("Setting RAM Data Transfer Control to 0x{:04X}", value));
        data_transfer_control_register = value;
    } else if (offset == 0xAE) {
        LOGW_SPU(std::format("Attempted write to read-only SPU Status Register @0x{:08X}", address));
    } else if (offset == 0xB0) {
        LOGV_SPU(std::format("Write to CD Audio Input Volume (Left): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xB2) {
        LOGV_SPU(std::format("Write to CD Audio Input Volume (Right): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xB4) {
        LOGV_SPU(std::format("Write to External Audio Input Volume (Left): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xB6) {
        LOGV_SPU(std::format("Write to External Audio Input Volume (Right): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xB8) {
        LOGW_SPU(std::format("Ignoring write to internal register: Current Volume (Left): 0x{:04X}", value));
    } else if (offset == 0xBA) {
        LOGW_SPU(std::format("Ignoring write to internal register: Current Volume (Right): 0x{:04X}", value));
    } else if (offset == 0xBC) {
        LOGW_SPU(std::format("Ignoring write to unknown register 0x1F80'1DBC: 0x{:04X}", value));
    } else if (offset == 0xBE) {
        LOGW_SPU(std::format("Ignoring write to unknown register 0x1F80'1DBE: 0x{:04X}", value));
    } else {
        assert(false);
    }
}

void SPU::handle_reverb_control_write(uint32_t address, uint16_t value) {
    assert(address >= 0x1F80'1DC0 && address < 0x1F80'1DFF);
    assert((address & 1) == 0);

    uint32_t offset = address & 0x0000'00FF;
    if (offset == 0xC0) {
        LOGV_SPU(std::format("Write to Reverb APF Offset 1 (dAPF1): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xC2) {
        LOGV_SPU(std::format("Write to Reverb APF Offset 2 (dAPF2): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xC4) {
        LOGV_SPU(std::format("Write to Reverb Reflection Volume 1 (vIIR): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xC6) {
        LOGV_SPU(std::format("Write to Reverb Comb Volume 1 (vCOMB1): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xC8) {
        LOGV_SPU(std::format("Write to Reverb Comb Volume 2 (vCOMB2): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xCA) {
        LOGV_SPU(std::format("Write to Reverb Comb Volume 3 (vCOMB3): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xCC) {
        LOGV_SPU(std::format("Write to Reverb Comb Volume 4 (vCOMB4): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xCE) {
        LOGV_SPU(std::format("Write to Reverb Reflection Volume 2 (vWALL): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xD0) {
        LOGV_SPU(std::format("Write to Reverb APF Volume 1 (vAPF1): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xD2) {
        LOGV_SPU(std::format("Write to Reverb APF Volume 2 (vAPF2): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xD4) {
        LOGV_SPU(std::format("Write to Reverb Same Side Reflection Address 1 (Left) (mSAME): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xD6) {
        LOGV_SPU(std::format("Write to Reverb Same Side Reflection Address 1 (Right) (mSAME): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xD8) {
        LOGV_SPU(std::format("Write to Reverb Comb Address 1 (Left) (mCOMB1): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xDA) {
        LOGV_SPU(std::format("Write to Reverb Comb Address 1 (Right) (mCOMB1): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xDC) {
        LOGV_SPU(std::format("Write to Reverb Comb Address 2 (Left) (mCOMB2): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xDE) {
        LOGV_SPU(std::format("Write to Reverb Comb Address 2 (Right) (mCOMB2): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xE0) {
        LOGV_SPU(std::format("Write to Reverb Same Side Reflection Address 2 (Left) (dSAME): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xE2) {
        LOGV_SPU(std::format("Write to Reverb Same Side Reflection Address 2 (Right) (dSAME): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xE4) {
        LOGV_SPU(std::format("Write to Reverb Different Side Reflection Address 1 (Left) (mDIFF): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xE6) {
        LOGV_SPU(std::format("Write to Reverb Different Side Reflection Address 1 (Right) (mDIFF): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xE8) {
        LOGV_SPU(std::format("Write to Reverb Comb Address 3 (Left) (mCOMB4): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xEA) {
        LOGV_SPU(std::format("Write to Reverb Comb Address 3 (Right) (mCOMB4): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xEC) {
        LOGV_SPU(std::format("Write to Reverb Comb Address 3 (Left) (mCOMB4): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xEE) {
        LOGV_SPU(std::format("Write to Reverb Comb Address 3 (Right) (mCOMB4): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xF0) {
        LOGV_SPU(std::format("Write to Reverb Different Side Reflection Address 2 (Left) (dDIFF): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xF2) {
        LOGV_SPU(std::format("Write to Reverb Different Side Reflection Address 2 (Right) (dDIFF): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xF4) {
        LOGV_SPU(std::format("Write to Reverb APF Address 1 (Left) (mAPF1): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xF6) {
        LOGV_SPU(std::format("Write to Reverb APF Address 1 (Right) (mAPF1): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xF8) {
        LOGV_SPU(std::format("Write to Reverb APF Address 2 (Left) (mAPF2): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xFA) {
        LOGV_SPU(std::format("Write to Reverb APF Address 2 (Right) (mAPF2): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xFC) {
        LOGV_SPU(std::format("Write to Reverb Input Volume (Left) (vIN): 0x{:04X}", value));
        // TODO
    } else if (offset == 0xFE) {
        LOGV_SPU(std::format("Write to Reverb Input Volume (Right) (vIN): 0x{:04X}", value));
        // TODO
    } else {
        // Should not be reachable
        assert(false);
    }
}

void SPU::handle_internal_register_write(uint32_t address, uint16_t value) {
    assert(address >= 0x1F80'1E00 && address < 0x1F80'1E5F);
    assert((address & 1) == 0);

    LOGW_SPU(std::format("Unimplemented write to internal voice registers @0x{:08X}", address));
}

void SPU::handle_unknown_write(uint32_t address, uint16_t value) {
    assert(address >= 0x1F80'1E60 && address < 0x1F80'1E7F);
    assert((address & 1) == 0);

    LOGW_SPU(std::format("Unimplemented write to unknown register @0x{:08X}", address));
}

void SPU::handle_unused_write(uint32_t address, uint16_t value) {
    assert(address >= 0x1F80'1E80 && address < 0x1F80'1FFF);
    assert((address & 1) == 0);

    LOGW_SPU(std::format("Unimplemented write to unused register @0x{:08X}", address));
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
        value = control_register;
        LOGV_SPU(std::format("Read from SPU control register:\n{:s}", get_control_register_explanation(value)));
    } else if (offset == 0xAC) {
        value = data_transfer_control_register;
        LOGV_SPU(std::format("Read from RAM Data Transfer Control: 0x{:04X}", value));
    } else if (offset == 0xAE) {
        value = status_register;
        LOGV_SPU(std::format("Read from SPU status register:\n{:s}", get_status_register_explanation(value)));
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

    LOGT_SPU(std::format("Half-word write to SPU @0x{:08X}: 0x{:04X}", address, value));

    uint32_t offset = address & 0x0000'FFFF;
    if (offset < 0x1D7F) {
        handle_voice_write(address, value);
    } else if (offset < 0x1D87) {
        handle_volume_write(address, value);
    } else if (offset < 0x1D9F) {
        handle_voice_flags_write(address, value);
    } else if (offset < 0x1DBF) {
        handle_control_write(address, value);
    } else if (offset < 0x1DFF) {
        handle_reverb_control_write(address, value);
    } else if (offset < 0x1E5F) {
        handle_internal_register_write(address, value);
    } else if (offset < 0x1E7F) {
        handle_unknown_write(address, value);
    } else if (offset < 0x1FFF) {
        handle_unused_write(address, value);
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

    LOGT_SPU(std::format("Half-word read from SPU @0x{:08X}", address));

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
    LOGV_SPU(std::format("Manual transfer of 0x{:02X} halfwords to SPU RAM @0x{:05X}", data_transfer_queue.size(), data_transfer_address));

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

