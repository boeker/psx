#ifndef PSX_DMA_H
#define PSX_DMA_H

#include <cstdint>
#include <iostream>
#include <string>

namespace PSX {

#define DCHR_UNK2 30
#define DCHR_UNK1 29
#define DCHR_START_TRIGGER 28
#define DCHR_START_BUSY 24
#define DCHR_CHOPPING_CPU_WINDOW_SIZE2 22
#define DCHR_CHOPPING_CPU_WINDOW_SIZE1 21
#define DCHR_CHOPPING_CPU_WINDOW_SIZE0 20
#define DCHR_CHOPPING_DMA_WINDOW_SIZE2 18
#define DCHR_CHOPPING_DMA_WINDOW_SIZE1 17
#define DCHR_CHOPPING_DMA_WINDOW_SIZE0 16
#define DCHR_SYNC_MODE1 10
#define DCHR_SYNC_MODE0 9
#define DCHR_CHOPPING_ENABLE 8
#define DCHR_MEMORY_ADDRESS_STEP 1
#define DCHR_TRANSFER_DIRECTION 0

#define DPCR_DMA6_OTC_MASTER_ENABLE 27
#define DPCR_DMA6_OTC_PRIORITY2 26
#define DPCR_DMA6_OTC_PRIORITY1 25
#define DPCR_DMA6_OTC_PRIORITY0 24
#define DPCR_DMA5_PIO_MASTER_ENABLE 23
#define DPCR_DMA5_PIO_PRIORITY2 22
#define DPCR_DMA5_PIO_PRIORITY1 21
#define DPCR_DMA5_PIO_PRIORITY0 20
#define DPCR_DMA4_SPU_MASTER_ENABLE 19
#define DPCR_DMA4_SPU_PRIORITY2 18
#define DPCR_DMA4_SPU_PRIORITY1 17
#define DPCR_DMA4_SPU_PRIORITY0 16
#define DPCR_DMA3_CDROM_MASTER_ENABLE 15
#define DPCR_DMA3_CDROM_PRIORITY2 14
#define DPCR_DMA3_CDROM_PRIORITY1 13
#define DPCR_DMA3_CDROM_PRIORITY0 12
#define DPCR_DMA2_GPU_MASTER_ENABLE 11
#define DPCR_DMA2_GPU_PRIORITY2 10
#define DPCR_DMA2_GPU_PRIORITY1 9
#define DPCR_DMA2_GPU_PRIORITY0 8
#define DPCR_DMA1_MDECOUT_MASTER_ENABLE 7
#define DPCR_DMA1_MDECOUT_PRIORITY2 6
#define DPCR_DMA1_MDECOUT_PRIORITY1 5
#define DPCR_DMA1_MDECOUT_PRIORITY0 4
#define DPCR_DMA0_MDECIN_MASTER_ENABLE 3
#define DPCR_DMA0_MDECIN_PRIORITY2 2
#define DPCR_DMA0_MDECIN_PRIORITY1 1
#define DPCR_DMA0_MDECIN_PRIORITY0 0

#define DICR_IRQ_SIGNAL 31
#define DICR_IRQ_FLAG_DMA6 30
#define DICR_IRQ_FLAG_DMA5 29
#define DICR_IRQ_FLAG_DMA4 28
#define DICR_IRQ_FLAG_DMA3 27
#define DICR_IRQ_FLAG_DMA2 26
#define DICR_IRQ_FLAG_DMA1 25
#define DICR_IRQ_FLAG_DMA0 24
#define DICR_ENABLE_IRQ_SIGNAL 23
#define DICR_ENABLE_FLAG_DMA6 22
#define DICR_ENABLE_FLAG_DMA5 21
#define DICR_ENABLE_FLAG_DMA4 20
#define DICR_ENABLE_FLAG_DMA3 19
#define DICR_ENABLE_FLAG_DMA2 18
#define DICR_ENABLE_FLAG_DMA1 17
#define DICR_ENABLE_FLAG_DMA0 16
#define DICR_FORCE_IRQ 15

class Bus;

class DMA {
private:
    Bus *bus;

    // D#_MADR - DMA Base Address
    // 0x1F801080...0x1F8010E0
    uint32_t dmaBaseAddress[7];

    // D#_BCR - DMA Block Control
    // 0x1F801084...0x1F8010E4
    uint32_t dmaBlockControl[7];

    // D#_CHR - DMA Channel Control
    // 0x1F801088...0x1F8010E8
    uint32_t dmaChannelControl[7];

    // DPCR
    // 0x1F8010F0
    uint32_t dmaControlRegister;

    // DICR
    // 0x1F8010F4
    uint32_t dmaInterruptRegister;

    friend std::ostream& operator<<(std::ostream &os, const DMA &dma);

public:
    static const char* CHANNEL_NAMES[];

    DMA(Bus *bus);
    void reset();

    template <typename T>
    void write(uint32_t address, T value);

    template <typename T>
    T read(uint32_t address);

    void handlePendingTransfers();
    bool dataRequested(uint32_t channel);
    void transfer(uint32_t channel);
    void transferOTC();

private:
    void updateControlRegister(uint32_t value);
    void updateInterruptRegister(uint32_t value);
    void updateBaseAddress(uint32_t channel, uint32_t value);
    void updateBlockControl(uint32_t channel, uint32_t value);
    void updateChannelControl(uint32_t channel, uint32_t value);

    std::string getDCHRExplanation(uint32_t channel) const;
    std::string getDPCRExplanation() const;
    std::string getDICRExplanation() const;
};

}

#endif
