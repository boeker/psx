#ifndef PSX_MEMORY_H
#define PSX_MEMORY_H

#include <cstdint>
#include <string>

#define MAIN_RAM_SIZE (2048 * 1024)
#define DCACHE_SIZE 1024
#define MEMORY_CONTROL_SIZE 100

namespace PSX {

class Memory {
private:
    uint8_t *mainRAM;
    uint8_t *dCache;
    uint8_t *memoryControlRegisters;
    uint8_t cacheControlRegister[4]; // 0xFFFE0000

public:
    Memory();
    void reset();
    virtual ~Memory();

    template <typename T>
    T readMainRAM(uint32_t address);
    template <typename T>
    void writeMainRAM(uint32_t address, T value);

    template <typename T>
    T readDCache(uint32_t address);
    template <typename T>
    void writeDCache(uint32_t address, T value);

    template <typename T>
    T readMemoryControlRegisters(uint32_t address);
    template <typename T>
    void writeMemoryControlRegisters(uint32_t address, T value);

    template <typename T>
    T readCacheControlRegister(uint32_t address);
    template <typename T>
    void writeCacheControlRegister(uint32_t address, T value);
};
}

#endif
