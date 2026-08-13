#ifndef PSX_SPU_H
#define PSX_SPU_H

#include <cstdint>
#include <memory>

#include <SDL3/SDL_audio.h>

#include "util/queue.h"

#define SPU_RAM_SIZE (512 * 1024)

#define SPU_CONTROL_ENABLE 15 // 0 = off, 1 = on
#define SPU_CONTROL_MUTE 14 // 0 = mute, 1 = unmute
#define SPU_CONTROL_NOISE_FREQUENCY_SHIFT3 13 //0,...,15 = Low Frequency,...,High Frequency
#define SPU_CONTROL_NOISE_FREQUENCY_SHIFT2 12
#define SPU_CONTROL_NOISE_FREQUENCY_SHIFT1 11
#define SPU_CONTROL_NOISE_FREQUENCY_SHIFT0 10
#define SPU_CONTROL_NOISE_FREQUENCY_STEP1 9 // 0,...,3 = Step 4,...,7
#define SPU_CONTROL_NOISE_FREQUENCY_STEP0 8
#define SPU_CONTROL_REVERB_MASTER_ENABLE 7 // 0 = disabled, 1 = enabled
#define SPU_CONTROL_IRQ9_ENABLE 6 // Read: 0 = disabled, 1 = enabled (if SPU_CONTROL_ENABLE is set), Write: 0 = Acknowledge
#define SPU_CONTROL_RAM_TRANSFER_MODE1 5 // 0 = stop, 1 = manual write,
#define SPU_CONTROL_RAM_TRANSFER_MODE0 4 // 2 = DMA write to SPU RAM, 3 = DMA read from SPU RAM
#define SPU_CONTROL_EXTERNAL_AUDIO_REVERB 3 // 0 = off, 1 = on
#define SPU_CONTROL_CD_AUDIO_REVERB 2 // 0 = off, 1 = on
#define SPU_CONTROL_EXTERNAL_AUDIO_ENABLE 1 // 0 = off, 1 = on
#define SPU_CONTROL_CD_AUDIO_ENABLE 0 // 0 = off, 1 = on

#define SPU_STATUS_CAPTURE_HALF 11 // 0 = write to first half, 1 = write to second half
#define SPU_STATUS_TRANSFER_BUSY 10 // 0 = ready, 1 = busy
#define SPU_STATUS_TRANSFER_READ_REQUEST 9 // 0 = no, 1 = yes
#define SPU_STATUS_TRANSFER_WRITE_REQUEST 8 // 0 = no, 1 = yes
#define SPU_STATUS_TRANSFER_READ_WRITE_REQUEST 7 // 0 = no, 1 = yes, this is SPU_CONTROL_RAM_TRANSFER_MODE1
#define SPU_STATUS_IRQ9_FLAG 6 // 0 = no, 1 = interrupt request
#define SPU_STATUS_RAM_TRANSFER_MODE1 5 // cf. SPU_CONTROL
#define SPU_STATUS_RAM_TRANSFER_MODE0 4
#define SPU_STATUS_EXTERNAL_AUDIO_REVERB 3 // cf. SPU_CONTROL
#define SPU_STATUS_CD_AUDIO_REVERB 2 // cf. SPU_CONTROL
#define SPU_STATUS_EXTERNAL_AUDIO_ENABLE 1 // cf. SPU_CONTROL
#define SPU_STATUS_CD_AUDIO_ENABLE 0 // cf. SPU_CONTROL

namespace PSX {

class Bus;

class SPU {
private:
    SDL_AudioSpec audio_spec;
    SDL_AudioStream* audio_stream;

    Bus *bus;

    std::unique_ptr<uint8_t[]> ram;

    // 0x1F80'1DA4: SPU RAM IRQ Address (halfword)
    uint16_t irq_address_register;
    // 0x1F80'1DA6: SPU RAM Data Transfer Address (divided by 8, halfword)
    uint16_t data_transfer_address_register;
    uint32_t data_transfer_address;
    // 0x1F80'1DA8: SPU RAM Data Transfer Queue (stores up to 32 halfwords)
    util::RingBuffer<uint16_t, 32> data_transfer_queue;
    // 0x1F80'1DAA: SPU Control Register (SPUCNT)
    uint16_t control_register;
    // 0x1F80'1DAC: SPU RAM Data Transfer Control (halfword, should be 0x0004)
    uint16_t data_transfer_control_register;
    // 0x1F80'1DAE: SPU Status Register (SPUSTAT)
    uint16_t status_register;

public:
    SPU(Bus *bus);
    ~SPU();
    void reset();

    bool dma_write_to_spu_requested() const;
    bool dma_read_from_spu_requested() const;

    void start_dma_transfer();
    void write_to_ram(uint16_t value);
    uint16_t read_from_ram();
    void finish_dma_transfer();

    void handle_control_write(uint32_t address, uint16_t value);
    uint16_t handle_control_read(uint32_t address);

    template <typename T>
    void write(uint32_t address, T value);

    template <typename T>
    T read(uint32_t address);

private:
    void perform_manual_transfer();
    void issue_interrupt_if_enabled();
};

}

#endif
