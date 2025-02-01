#include "dma.h"

#include <cassert>
#include <cstring>
#include <format>
#include <sstream>

#include "bus.h"
#include "util/log.h"
#include "exceptions/exceptions.h"

using namespace util;

namespace PSX {

const char* DMA::CHANNEL_NAMES[] = {
    "MDECIn",
    "MDECOut",
    "GPU",
    "CDROM",
    "SPU",
    "PIO",
    "OTC"
};

std::ostream& operator<<(std::ostream &os, const DMA &dma) {
    os << "DPCR: ";
    os << dma.getDPCRExplanation();
    os << std::endl;
    os << "DICR: ";
    os << dma.getDICRExplanation();

    return os;
}

DMA::DMA(Bus *bus) {
    this->bus = bus;

    reset();
}

void DMA::reset() {
    std::memset(dmaBaseAddress, 0, sizeof(dmaBaseAddress));
    std::memset(dmaBlockControl, 0, sizeof(dmaBlockControl));
    std::memset(dmaChannelControl, 0, sizeof(dmaChannelControl));

    dmaControlRegister = 0x07654321;
    dmaInterruptRegister = 0;
}

template <>
void DMA::write(uint32_t address, uint32_t value) {
    assert ((address >= 0x1F801080) && (address <= 0x1F8010FF));

    Log::log(std::format("write 0x{:08X} -> @0x{:08X}",
                         value, address), Log::Type::DMA_WRITE);

    if ((address & 0xFFFFFFF0) == 0x1F8010F0) { // 0x1F8010Fx: DPCR or DICR
        if (address == 0x1F8010F0) {
            updateControlRegister(value);

        } else if (address == 0x1F8010F4) {
            updateInterruptRegister(value);

        } else {
            throw exceptions::UnknownDMACommandError(
                  std::format("Read from unknown DMA register 0x{:08X}", address));
        }

    } else { // 0x1F8018x...0x1F801Ex: DMA channels 0...6
        uint32_t channel = ((address & 0x000000F0) >> 4) - 8;
        assert (channel >= 0 && channel <= 6);

        uint32_t reg = (address & 0x0000000F);
        assert ((reg == 0) || (reg == 4) || (reg == 8));

        switch (reg) {
            case 0: // D#_MADR - DMA Base Address
                updateBaseAddress(channel, value);
                break;
            case 4: // D#_BCR - DMA Block Control
                updateBlockControl(channel, value);
                break;
            case 8: // D#_CHCR - DMA Channel Control
                updateChannelControl(channel, value);
                break;
            default:
                throw exceptions::UnknownDMACommandError(
                      std::format("Write to unknown DMA register 0x{:08X}", address));
                break;
        }
    }

}

template <> void DMA::write(uint32_t address, uint16_t value) {
    throw exceptions::UnimplementedAddressingError(std::format("DMA: half-word write @0x{:08X}", address));
}

template <> void DMA::write(uint32_t address, uint8_t value) {
    throw exceptions::UnimplementedAddressingError(std::format("DMA: byte write @0x{:08X}", address));
}

template <>
uint32_t DMA::read(uint32_t address) {
    assert ((address >= 0x1F801080) && (address <= 0x1F8010FF));

    Log::log(std::format("read @0x{:08X}", address), Log::Type::DMA_WRITE);

    if ((address & 0xFFFFFFF0) == 0x1F8010F0) { // 0x1F8010Fx: DPCR or DICR
        if (address == 0x1F8010F0) {
            return dmaControlRegister;

        } else if (address == 0x1F8010F4) {
            return dmaInterruptRegister;

        } else {
            throw exceptions::UnknownDMACommandError(
                  std::format("Read from unknown DMA register 0x{:08X}", address));
        }

    } else { // 0x1F8018x...0x1F801Ex: DMA channels 0...6
        uint32_t channel = ((address & 0x000000F0) >> 4) - 8;
        assert (channel >= 0 && channel <= 6);

        uint32_t reg = (address & 0x0000000F);
        assert ((reg == 0) || (reg == 4) || (reg == 8));

        switch (reg) {
            case 0: // D#_MADR - DMA Base Address
                return dmaBaseAddress[channel];
            case 4: // D#_BCR - DMA Block Control
                return dmaBlockControl[channel];
            case 8: // D#_CHCR - DMA Channel Control
                return dmaChannelControl[channel];
            default:
                throw exceptions::UnknownDMACommandError(
                      std::format("Read from unknown DMA register 0x{:08X}", address));
                break;
        }
    }
}

template <> uint16_t DMA::read(uint32_t address) {
    throw exceptions::UnimplementedAddressingError(std::format("DMA: half-word read @0x{:08X}", address));
}

template <> uint8_t DMA::read(uint32_t address) {
    throw exceptions::UnimplementedAddressingError(std::format("DMA: byte read @0x{:08X}", address));
}

void DMA::handlePendingTransfers() {
    uint32_t channel;
    uint32_t priority;

    Log::log(std::format("Checking for pending DMA transfers"), Log::Type::DMA);

    do {
        channel = 7;
        priority = 8;

        // larger priority number -> higher priority
        // if priority numbers are the same, then the channel with the larger
        // channel number has higher priority
        for (int i = 6; i >= 0; --i) {
            if ((dmaControlRegister >> (4 * i + 3)) & 1) { // master enable
                uint32_t currentPriority = (dmaControlRegister >> 4 * i) & 7;

                if ((dmaChannelControl[i] & (1 << DCHR_START_BUSY)) // start/busy bit
                    && (currentPriority > priority)
                    && dataRequested(i)) {
                    channel = i;
                    currentPriority = priority;
                }
            }
        }

        if (priority != 8) {
            // initiate transfer
            transfer(channel);
        }

    } while (priority != 8);
}

bool DMA::dataRequested(uint32_t channel) {
    bool fromMainRAM = dmaChannelControl[channel] & (1 << DCHR_TRANSFER_DIRECTION);

    switch(channel) {
        case 2:
            return (fromMainRAM && bus->gpu.transferToGPURequested())
                   || (!fromMainRAM && bus->gpu.transferToGPURequested());
        case 6:
            return false;
        default:
            return false;
    }
}

void DMA::transfer(uint32_t channel) {
    switch (channel) {
        case 6:
            transferOTC();
            break;
        default:
            Log::log(std::format("Channel {:d}: transfer not implemented",
                                 channel,
                                 CHANNEL_NAMES[channel]), Log::Type::DMA);
    }

    Log::log(std::format("Channel {:d} ({:s}) channel control: {:s}",
                         channel,
                         CHANNEL_NAMES[channel],
                         getDCHRExplanation(channel)), Log::Type::DMA);
}

void DMA::transferOTC() {
    uint32_t baseAddress = dmaBaseAddress[6];
    uint32_t blocks = dmaBlockControl[6] & 0x0000FFFF;
    if (blocks == 0) {
        blocks = 0x10000;
    }

    Log::log(std::format("Channel 6 (OTC) transfer: 0x{:08X} blocks to @0x{:08X}",
                         blocks,
                         baseAddress), Log::Type::DMA);

    // Clear start/trigger on beginning of transfer
    dmaChannelControl[6] = dmaChannelControl[6] & ~(1 << DCHR_START_TRIGGER);
    
    // Create a linked list in memory by going backwards from baseAddress
    // The lower 24 bits of every entry specify the next address
    for (uint32_t i = 1; i < blocks; ++i) {
        uint32_t nextAddress = baseAddress - 4;
        bus->write<uint32_t>(baseAddress, nextAddress & 0x00FFFFFF);

        baseAddress = nextAddress;
    }

    bus->write<uint32_t>(baseAddress, 0x00FFFFFF);

    // Clear start/busy on completion of transfer
    dmaChannelControl[6] = dmaChannelControl[6] & ~(1 << DCHR_START_BUSY);
}

void DMA::updateControlRegister(uint32_t value) {
    dmaControlRegister = value;

    Log::log(std::format("DPCR updated: {:s}", getDPCRExplanation()), Log::Type::DMA_WRITE);

    handlePendingTransfers();
}

void DMA::updateInterruptRegister(uint32_t value) {
    // Write to interrupt register
    // Bits 0--5 are read/write-able
    // Bits 6--14 are not used, always zero
    // Bit 15: Force IRQ: Writing 1 here sets bit 31 to 1
    // Bits 16...22: Enable setting bits 24...30 upon DMA
    // Bit 23: Enable setting bit 31 when bits 24...30 are non-zero
    // Bits 24...30: IRQ Flags. Writing 1 here resets them to zero
    // Bit 31: IRQ Signal: 0-to-1 triggers interrupt (IRQ3 in I_STAT gets set)

    dmaInterruptRegister = (dmaInterruptRegister & 0xFF000000) // keep flags for now
                           | (value & 0x00FF003F); // update values

    // reset flags, keep IRQ signal
    dmaInterruptRegister = dmaInterruptRegister & ~(value & 0x7F000000);

    if ((((value >> 15) & 1) // Force IRQ?
         || (((dmaInterruptRegister >> 23) & 1) // Set bit 31 on IRQ flag?
             && ((dmaInterruptRegister >> 24) & 0x7F))) // IRQ flag set?
        && !((dmaInterruptRegister >> 31) & 1)) { // Not already set?
        dmaInterruptRegister = dmaInterruptRegister | (1 << 31);
        bus->interrupts.notifyAboutInterrupt(INTERRUPT_BIT_DMA);
    }

    Log::log(std::format("DICR updated: {:s}", getDICRExplanation()), Log::Type::DMA_WRITE);
}

void DMA::updateBaseAddress(uint32_t channel, uint32_t value) {
    assert (channel <= 6);
    dmaBaseAddress[channel] = value & 0xFFFFFFFC; // word-align address

    Log::log(std::format("Channel {:d} ({:s}) base address updated: 0x{:08X}",
                         channel,
                         CHANNEL_NAMES[channel],
                         dmaBaseAddress[channel]), Log::Type::DMA);
}

void DMA::updateBlockControl(uint32_t channel, uint32_t value) {
    assert (channel <= 6);
    dmaBlockControl[channel] = value;

    uint16_t blockSize = value & 0x0000FFFF;
    uint16_t blocks = value >> 16;
    Log::log(std::format("Channel {:d} ({:s}) block control updated: 0x{:04X} blocks of 0x{:04X} words",
                         channel,
                         CHANNEL_NAMES[channel],
                         blocks, blockSize), Log::Type::DMA);
}

void DMA::updateChannelControl(uint32_t channel, uint32_t value) {
    assert (channel <= 6);
    if (channel == 6) {
        // channel 6 only has three read-/writeable bits
        // 30, 28, 24 (UNK2, START_TRIGGER, START_BUSY)
        // MEMORY_ADDRESS_STEP is always 1 (= backward)
        dmaChannelControl[6] = (value & 0x51000000) | (1 << DCHR_MEMORY_ADDRESS_STEP);
    } else {
        // we only keep the relevant bits
        dmaChannelControl[channel] = (value & 0x31770703);
    }

    //uint32_t transferDirection = value & 1;
    //uint32_t memoryAddressStep = (value >> 1) & 1;
    //bool chopping = (value >> 8) & 1;
    //uint32_t syncMode = (value >> 9) & 3;
    //uint32_t choppingDMAWindowSize = (value >> 16) & 7;
    //uint32_t choppingCPUWindowSize = (value >> 20) & 7;
    //bool startBusy = (value >> 24) & 1;
    //bool startTrigger = (value >> (28 >> 1);
    Log::log(std::format("Channel {:d} ({:s}) channel control updated: {:s}",
                         channel,
                         CHANNEL_NAMES[channel],
                         getDCHRExplanation(channel)), Log::Type::DMA);


    uint32_t dchr = dmaChannelControl[channel];
    if ((dchr & (1 << DCHR_START_BUSY))
        && (dmaControlRegister & (1 << (4*channel + 3)))) {

        if (dchr & (1 << DCHR_START_TRIGGER)) { // manual start requested
            Log::log(std::format("Channel {:d} ({:s}) immediate transfer requested",
                                 channel,
                                 CHANNEL_NAMES[channel]), Log::Type::DMA);
            transfer(channel);

        } else { // check for data request
            if (dataRequested(channel)) {
                transfer(channel);
            }
        }
    }
}

std::string DMA::getDCHRExplanation(uint32_t channel) const {
    assert (channel <= 6);
    uint32_t dchr = dmaChannelControl[channel];

    std::stringstream ss;
    ss << std::format("START_TRIGGER[{:01b}], ",
                      (dchr >> DCHR_START_TRIGGER) & 1);
    ss << std::format("START_BUSY[{:01b}], ",
                      (dchr >> DCHR_START_BUSY) & 1);
    ss << std::format("CHOPPING_CPU_WIN_SIZE[{:01d}], ",
                      (dchr >> DCHR_CHOPPING_CPU_WINDOW_SIZE0) & 7);
    ss << std::format("CHOPPING_DMA_WIN_SIZE[{:01d}], ",
                      (dchr >> DCHR_CHOPPING_DMA_WINDOW_SIZE0) & 7);
    ss << std::format("SYNC_MODE[{:01d}], ",
                      (dchr >> DCHR_SYNC_MODE0) & 3);
    ss << std::format("CHOPPING_ENABLE[{:01b}], ",
                      (dchr >> DCHR_CHOPPING_ENABLE) & 1);
    ss << std::format("MEMORY_ADDRESS_STEP[{:01b}], ",
                      (dchr >> DCHR_MEMORY_ADDRESS_STEP) & 1);
    ss << std::format("TRANSFER_DIRECTION[{:01b}]",
                      (dchr >> DCHR_TRANSFER_DIRECTION) & 1);

    return ss.str();
}

std::string DMA::getDPCRExplanation() const {
    std::stringstream ss;
    uint32_t dpcr = dmaControlRegister;

    ss << std::format("DMA6_OTC[{:01b},",
                      (dpcr >> DPCR_DMA6_OTC_MASTER_ENABLE) & 1);
    ss << std::format("PRIO {:d}], ",
                      (dpcr >> DPCR_DMA6_OTC_PRIORITY0) & 7);

    ss << std::format("DMA5_PIO[{:01b},",
                      (dpcr >> DPCR_DMA5_PIO_MASTER_ENABLE) & 1);
    ss << std::format("PRIO {:d}], ",
                      (dpcr >> DPCR_DMA5_PIO_PRIORITY0) & 7);

    ss << std::format("DMA4_SPU[{:01b},",
                      (dpcr >> DPCR_DMA4_SPU_MASTER_ENABLE) & 1);
    ss << std::format("PRIO {:d}], ",
                      (dpcr >> DPCR_DMA6_OTC_PRIORITY0) & 7);

    ss << std::format("DMA3_CDROM[{:01b},",
                      (dpcr >> DPCR_DMA3_CDROM_MASTER_ENABLE) & 1);
    ss << std::format("PRIO {:d}], ",
                      (dpcr >> DPCR_DMA3_CDROM_PRIORITY0) & 7);

    ss << std::format("DMA2_GPU[{:01b},",
                      (dpcr >> DPCR_DMA2_GPU_MASTER_ENABLE) & 1);
    ss << std::format("PRIO {:d}], ",
                      (dpcr >> DPCR_DMA2_GPU_PRIORITY0) & 7);

    ss << std::format("DMA1_MDECOUT[{:01b},",
                      (dpcr >> DPCR_DMA1_MDECOUT_MASTER_ENABLE) & 1);
    ss << std::format("PRIO {:d}], ",
                      (dpcr >> DPCR_DMA1_MDECOUT_PRIORITY0) & 7);

    ss << std::format("DMA0_MDECIN[{:01b},",
                      (dpcr >> DPCR_DMA0_MDECIN_MASTER_ENABLE) & 1);
    ss << std::format("PRIO {:d}]",
                      (dpcr >> DPCR_DMA0_MDECIN_PRIORITY0) & 7);

    return ss.str();
}

std::string DMA::getDICRExplanation() const {
    std::stringstream ss;
    uint32_t dicr = dmaInterruptRegister;

    ss << std::format("IRQ_SIGNAL[{:01b}], ", (dicr >> DICR_IRQ_SIGNAL) & 1);
    ss << "FLAGS: ";
    ss << std::format("DMA6_OTC[{:01b}], ", (dicr >> DICR_IRQ_FLAG_DMA6) & 1);
    ss << std::format("DMA5_PIO[{:01b}], ", (dicr >> DICR_IRQ_FLAG_DMA5) & 1);
    ss << std::format("DMA4_SPU[{:01b}], ", (dicr >> DICR_IRQ_FLAG_DMA4) & 1);
    ss << std::format("DMA3_CDROM[{:01b}], ", (dicr >> DICR_IRQ_FLAG_DMA3) & 1);
    ss << std::format("DMA2_GPU[{:01b}], ", (dicr >> DICR_IRQ_FLAG_DMA2) & 1);
    ss << std::format("DMA1_MDECIN[{:01b}], ", (dicr >> DICR_IRQ_FLAG_DMA1) & 1);
    ss << std::format("DMA0_MDECOUT[{:01b}], ", (dicr >> DICR_IRQ_FLAG_DMA0) & 1);

    ss << std::format("ENABLE_IRQ_SIGNAL[{:01b}], ", (dicr >> DICR_ENABLE_IRQ_SIGNAL) & 1);

    ss << "ENABLE_FLAGS: ";
    ss << std::format("DMA6_OTC[{:01b}], ", (dicr >> DICR_ENABLE_FLAG_DMA6) & 1);
    ss << std::format("DMA5_PIO[{:01b}], ", (dicr >> DICR_ENABLE_FLAG_DMA5) & 1);
    ss << std::format("DMA4_SPU[{:01b}], ", (dicr >> DICR_ENABLE_FLAG_DMA4) & 1);
    ss << std::format("DMA3_CDROM[{:01b}], ", (dicr >> DICR_ENABLE_FLAG_DMA3) & 1);
    ss << std::format("DMA2_GPU[{:01b}], ", (dicr >> DICR_ENABLE_FLAG_DMA2) & 1);
    ss << std::format("DMA1_MDECIN[{:01b}], ", (dicr >> DICR_ENABLE_FLAG_DMA1) & 1);
    ss << std::format("DMA0_MDECOUT[{:01b}], ", (dicr >> DICR_ENABLE_FLAG_DMA0) & 1);

    ss << std::format("FORCE_IRQ[{:01b}]", (dicr >> DICR_FORCE_IRQ) & 1);

    return ss.str();
}

}

