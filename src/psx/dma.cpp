#include "dma.h"

#include <cassert>
#include <format>
#include <sstream>

#include "bus.h"
#include "util/log.h"
#include "exceptions/exceptions.h"

using namespace util;

namespace PSX {

std::ostream& operator<<(std::ostream &os, const DMA &dma) {
    os << dma.getDPCRExplanation();
    os << dma.getDICRExplanation();

    return os;
}

DMA::DMA(Bus *bus) {
    this->bus = bus;

    reset();
}

void DMA::reset() {
    dmaControlRegister = 0x07654321;
    dmaInterruptRegister = 0;
}

template <>
void DMA::write(uint32_t address, uint32_t value) {
    assert ((address >= 0x1F801080) && (address <= 0x1F8010FF));

    Log::log(std::format("DMA write 0x{:08X} -> @0x{:08X}",
                         value, address), Log::Type::DMA_WRITE);

    if (address == 0x1F8010F0) {
        dmaControlRegister = value;

        Log::log(getDPCRExplanation(), Log::Type::DMA_WRITE);

    } else if (address == 0x1F8010F4) {
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

        Log::log(getDICRExplanation(), Log::Type::DMA_WRITE);

    } else {
        Log::log(std::format("Unimplemented DMA write: 0x{:08X} -> @0x{:08X}",
                             value, address), Log::Type::DMA);
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

    Log::log(std::format("DMA read @0x{:08X}", address), Log::Type::DMA_WRITE);

    if (address == 0x1F8010F0) {
        return dmaControlRegister;

    } else if (address == 0x1F8010F4) {
        return dmaInterruptRegister;

    } else {
        Log::log(std::format("Unimplemented DMA read @0x{:08X}", address), Log::Type::DMA);
        return 0;
    }
}

template <> uint16_t DMA::read(uint32_t address) {
    throw exceptions::UnimplementedAddressingError(std::format("DMA: half-word read @0x{:08X}", address));
}

template <> uint8_t DMA::read(uint32_t address) {
    throw exceptions::UnimplementedAddressingError(std::format("DMA: byte read @0x{:08X}", address));
}

std::string DMA::getDPCRExplanation() const {
    std::stringstream ss;
    uint32_t dpcr = dmaControlRegister;

    ss << "DPCR: ";
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

    ss << "DICR: ";
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

