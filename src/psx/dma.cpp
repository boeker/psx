#include "dma.h"

#include <cassert>
#include <format>

#include "util/log.h"

using namespace util;

namespace PSX {

void DMA::reset() {
}

template <typename T>
void DMA::write(uint32_t address, T value) {
    Log::log(std::format("DMA unimplemented: write @0x{:08X}", address), Log::Type::DMA);
}

template void DMA::write(uint32_t address, uint32_t value);
template void DMA::write(uint32_t address, uint16_t value);
template void DMA::write(uint32_t address, uint8_t value);

template <typename T>
T DMA::read(uint32_t address) {
    Log::log(std::format("DMA unimplemented: read @0x{:08X}", address), Log::Type::DMA);
    return 0;
}

template uint32_t DMA::read(uint32_t address);
template uint16_t DMA::read(uint32_t address);
template uint8_t DMA::read(uint32_t address);

}

