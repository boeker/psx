#include "dma.h"

#include <cassert>
#include <format>
#include <sstream>

#include "util/log.h"
#include "exceptions/exceptions.h"

using namespace util;

namespace PSX {

std::ostream& operator<<(std::ostream &os, const DMA &dma) {
    os << "DPCR: ";
    uint32_t dpcr = dma.dmaControlRegister;

    os << std::format("DMA6_OTC[{:01b},",
                      (dpcr >> DPCR_DMA6_OTC_MASTER_ENABLE) & 1);
    os << std::format("PRIO {:d}], ",
                      (dpcr >> DPCR_DMA6_OTC_PRIORITY0) & 7);

    os << std::format("DMA5_PIO[{:01b},",
                      (dpcr >> DPCR_DMA5_PIO_MASTER_ENABLE) & 1);
    os << std::format("PRIO {:d}], ",
                      (dpcr >> DPCR_DMA5_PIO_PRIORITY0) & 7);

    os << std::format("DMA4_SPU[{:01b},",
                      (dpcr >> DPCR_DMA4_SPU_MASTER_ENABLE) & 1);
    os << std::format("PRIO {:d}], ",
                      (dpcr >> DPCR_DMA6_OTC_PRIORITY0) & 7);

    os << std::format("DMA3_CDROM[{:01b},",
                      (dpcr >> DPCR_DMA3_CDROM_MASTER_ENABLE) & 1);
    os << std::format("PRIO {:d}], ",
                      (dpcr >> DPCR_DMA3_CDROM_PRIORITY0) & 7);

    os << std::format("DMA2_GPU[{:01b},",
                      (dpcr >> DPCR_DMA2_GPU_MASTER_ENABLE) & 1);
    os << std::format("PRIO {:d}], ",
                      (dpcr >> DPCR_DMA2_GPU_PRIORITY0) & 7);

    os << std::format("DMA1_MDECOUT[{:01b},",
                      (dpcr >> DPCR_DMA1_MDECOUT_MASTER_ENABLE) & 1);
    os << std::format("PRIO {:d}], ",
                      (dpcr >> DPCR_DMA1_MDECOUT_PRIORITY0) & 7);

    os << std::format("DMA0_MDECIN[{:01b},",
                      (dpcr >> DPCR_DMA0_MDECIN_MASTER_ENABLE) & 1);
    os << std::format("PRIO {:d}]\n",
                      (dpcr >> DPCR_DMA0_MDECIN_PRIORITY0) & 7);

    os << "DICR: ";
    uint32_t dicr = dma.dmaInterruptRegister;

    os << std::format("IRQ_SIGNAL[{:01b}], ", (dicr >> DICR_IRQ_SIGNAL) & 1);
    os << std::format("FLAG_DMA6[{:01b}], ", (dicr >> DICR_IRQ_FLAG_DMA6) & 1);
    os << std::format("FLAG_DMA5[{:01b}], ", (dicr >> DICR_IRQ_FLAG_DMA5) & 1);
    os << std::format("FLAG_DMA4[{:01b}], ", (dicr >> DICR_IRQ_FLAG_DMA4) & 1);
    os << std::format("FLAG_DMA3[{:01b}], ", (dicr >> DICR_IRQ_FLAG_DMA3) & 1);
    os << std::format("FLAG_DMA2[{:01b}], ", (dicr >> DICR_IRQ_FLAG_DMA2) & 1);
    os << std::format("FLAG_DMA1[{:01b}], ", (dicr >> DICR_IRQ_FLAG_DMA1) & 1);
    os << std::format("FLAG_DMA0[{:01b}], ", (dicr >> DICR_IRQ_FLAG_DMA0) & 1);

    os << std::format("ENABLE_IRQ_SIGNAL[{:01b}], ", (dicr >> DICR_ENABLE_IRQ_SIGNAL) & 1);
    os << std::format("ENABLE_FLAG_DMA6[{:01b}], ", (dicr >> DICR_ENABLE_FLAG_DMA6) & 1);
    os << std::format("ENABLE_FLAG_DMA5[{:01b}], ", (dicr >> DICR_ENABLE_FLAG_DMA5) & 1);
    os << std::format("ENABLE_FLAG_DMA4[{:01b}], ", (dicr >> DICR_ENABLE_FLAG_DMA4) & 1);
    os << std::format("ENABLE_FLAG_DMA3[{:01b}], ", (dicr >> DICR_ENABLE_FLAG_DMA3) & 1);
    os << std::format("ENABLE_FLAG_DMA2[{:01b}], ", (dicr >> DICR_ENABLE_FLAG_DMA2) & 1);
    os << std::format("ENABLE_FLAG_DMA1[{:01b}], ", (dicr >> DICR_ENABLE_FLAG_DMA1) & 1);
    os << std::format("ENABLE_FLAG_DMA0[{:01b}], ", (dicr >> DICR_ENABLE_FLAG_DMA0) & 1);

    os << std::format("FORCE_IRQ[{:01b}]", (dicr >> DICR_FORCE_IRQ) & 1);

    return os;
}

DMA::DMA() {
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

        std::stringstream ss;
        ss << *this;
        Log::log(ss.str(), Log::Type::DMA_WRITE);

    } else if (address == 0x1F8010F4) {
        dmaInterruptRegister = value;

        std::stringstream ss;
        ss << *this;
        Log::log(ss.str(), Log::Type::DMA_WRITE);

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

}

