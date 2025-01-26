#include "interrupts.h"

#include <cassert>
#include <format>

#include "util/log.h"

using namespace util;

namespace PSX {

void Interrupts::reset() {
}

template <typename T>
void Interrupts::write(uint32_t address, T value) {
    Log::log(std::format("Interrupts unimplemented: write @0x{:08X}", address), Log::Type::INTERRUPTS);
}

template void Interrupts::write(uint32_t address, uint32_t value);
template void Interrupts::write(uint32_t address, uint16_t value);
template void Interrupts::write(uint32_t address, uint8_t value);

template <typename T>
T Interrupts::read(uint32_t address) {
    Log::log(std::format("Interrupts unimplemented: read @0x{:08X}", address), Log::Type::INTERRUPTS);
    return 0;
}

template uint32_t Interrupts::read(uint32_t address);
template uint16_t Interrupts::read(uint32_t address);
template uint8_t Interrupts::read(uint32_t address);

}

