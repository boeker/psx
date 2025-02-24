#include "cdrom.h"

#include <cassert>
#include <format>

#include "util/log.h"

using namespace util;

namespace PSX {

void CDROM::reset() {
}

template <typename T>
void CDROM::write(uint32_t address, T value) {
    assert ((address >= 0x1F801800) && (address < 0x1F801800 + sizeof(T)));
    LOG_CDROM(std::format("CDROM unimplemented: write 0x{:0{}X} -> @0x{:08X}", value, 2*sizeof(T), address));
}

template void CDROM::write(uint32_t address, uint32_t value);
template void CDROM::write(uint32_t address, uint16_t value);
template void CDROM::write(uint32_t address, uint8_t value);

template <typename T>
T CDROM::read(uint32_t address) {
    LOG_CDROM(std::format("CDROM unimplemented: read @0x{:08X}", address));
    return (1 << 3);
}

template uint32_t CDROM::read(uint32_t address);
template uint16_t CDROM::read(uint32_t address);
template uint8_t CDROM::read(uint32_t address);

}

