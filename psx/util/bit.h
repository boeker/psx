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

    inline int16_t unpack_first_int16_t(uint32_t value) {
        return value & 0xFFFF;
    }

    inline int16_t unpack_second_int16_t(uint32_t value) {
        return value >> 16;
    }

    inline uint32_t pack_int16_ts(int16_t x, int16_t y) {
        return (static_cast<uint32_t>(y) << 16) | (static_cast<uint32_t>(x) & 0xFFFF);
    }

    inline uint32_t pack_int16_t(int16_t z) {
        return static_cast<int32_t>(z);
    }

    struct int16_t_pair {
        int16_t x;
        int16_t y;

        void reset() {
            x = 0;
            y = 0;
        }

        uint32_t pack() const {
            return pack_int16_ts(x, y);
        }
        void unpack(uint32_t value) {
            y = value >> 16;
            x = value & 0xFFFF;
        }
    };

    struct int16_t_triple {
        int16_t x;
        int16_t y;
        int16_t z;

        void reset() {
            x = 0;
            y = 0;
            z = 0;
        }

        uint32_t packXY() const {
            return pack_int16_ts(x, y);
        }
        uint32_t packZ() const {
            return pack_int16_t(z);
        }
        void unpackXY(uint32_t value) {
            y = value >> 16;
            x = value & 0xFFFF;
        }
        void unpackZ(uint32_t value) {
            z = value & 0xFFFF;
        }
    };
};

}

#endif
