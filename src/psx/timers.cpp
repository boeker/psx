#include "timers.h"

#include <cassert>
#include <format>

#include "util/log.h"

using namespace util;

namespace PSX {

void Timers::reset() {
}

template <typename T>
void Timers::write(uint32_t address, T value) {
    assert ((address >= 0x1F801100) && (address < 0x1F801128 + sizeof(T)));
    Log::log(std::format("Timers unimplemented: write @0x{:08X}", address), Log::Type::TIMERS);
}

template void Timers::write(uint32_t address, uint32_t value);
template void Timers::write(uint32_t address, uint16_t value);
template void Timers::write(uint32_t address, uint8_t value);

template <typename T>
T Timers::read(uint32_t address) {
    Log::log(std::format("Timers unimplemented: read @0x{:08X}", address), Log::Type::TIMERS);
    return 0;
}

template uint32_t Timers::read(uint32_t address);
template uint16_t Timers::read(uint32_t address);
template uint8_t Timers::read(uint32_t address);

}

