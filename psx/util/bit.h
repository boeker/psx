#ifndef UTIL_BIT_H
#define UTIL_BIT_H

#include <cstdint>

namespace util {

namespace Bit {
    inline uint16_t extendSign11To16(uint16_t value) {
        return ((value >> 10) ? 0xF800 : 0x0000) + value;
    }

    template<typename T>
    inline bool getBit(T target, uint8_t bit) {
        return target & (1 << bit);
    }

    template<uint32_t N, typename T>
    inline T getBits(T target) {
        return target & ~(~0u << N);
    }

    template<uint32_t N, typename T>
    inline T getBits(T target, uint8_t bit) {
        return (target >> bit) & ~(~0u << N);
    }

    template<typename T>
    inline void setBit(T &target, uint8_t bit, bool value) {
        uint32_t selectedBit = 1 << bit;
        target = (target & ~selectedBit) | ((value ? 1 : 0) << bit);
    }

    template<typename T>
    inline void setBit(T &target, uint8_t bit) {
        target = target | (1 << bit);
    }

    template<typename T>
    inline void clearBit(T &target, uint8_t bit) {
        uint32_t selectedBit = 1 << bit;
        target = target & ~selectedBit;
    }
};

}

#endif
