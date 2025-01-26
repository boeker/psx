#include "spu.h"

#include <cassert>
#include <format>

#include "util/log.h"

using namespace util;

namespace PSX {

void SPU::reset() {
}

template <typename T>
void SPU::write(uint32_t address, T value) {
    Log::log(std::format("SPU unimplemented: write @0x{:08X}", address), Log::Type::SPU);
}

template void SPU::write(uint32_t address, uint32_t value);
template void SPU::write(uint32_t address, uint16_t value);
template void SPU::write(uint32_t address, uint8_t value);

template <typename T>
T SPU::read(uint32_t address) {
    Log::log(std::format("SPU unimplemented: read @0x{:08X}", address), Log::Type::SPU);
    return 0;
}

template uint32_t SPU::read(uint32_t address);
template uint16_t SPU::read(uint32_t address);
template uint8_t SPU::read(uint32_t address);

}

