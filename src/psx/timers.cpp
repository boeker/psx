#include "timers.h"

#include <cassert>

namespace PSX {

void Timers::reset() {
}

template <typename T>
void writeToIO(uint32_t address, T value) {
    assert ((address >= 0x1F801100) && (address < 0x1F801128 + sizeof(T)));
}

template void writeToIO(uint32_t address, uint32_t value);
template void writeToIO(uint32_t address, uint16_t value);
template void writeToIO(uint32_t address, uint8_t value);

template <typename T>
T readFromIO(uint32_t address) {
    return 0;
}

template uint32_t readFromIO(uint32_t address);
template uint16_t readFromIO(uint32_t address);
template uint8_t readFromIO(uint32_t address);

}

