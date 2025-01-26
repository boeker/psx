#include "gpu.h"

#include <cassert>
#include <format>

#include "util/log.h"

using namespace util;

namespace PSX {

void GPU::reset() {
}

template <typename T>
void GPU::write(uint32_t address, T value) {
    Log::log(std::format("GPU unimplemented: write @0x{:08X}", address), Log::Type::GPU);
}

template void GPU::write(uint32_t address, uint32_t value);
template void GPU::write(uint32_t address, uint16_t value);
template void GPU::write(uint32_t address, uint8_t value);

template <typename T>
T GPU::read(uint32_t address) {
    Log::log(std::format("GPU unimplemented: read @0x{:08X}", address), Log::Type::GPU);
    return 0;
}

template uint32_t GPU::read(uint32_t address);
template uint16_t GPU::read(uint32_t address);
template uint8_t GPU::read(uint32_t address);

}

