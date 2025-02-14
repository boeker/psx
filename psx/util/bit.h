#ifndef UTIL_BIT_H
#define UTIL_BIT_H

#include <cstdint>

namespace util {

namespace Bit {
    inline uint16_t extendSign11To16(uint16_t value) {
        return ((value >> 10) ? 0xF800 : 0x0000) + value;
    }
};

}

#endif
