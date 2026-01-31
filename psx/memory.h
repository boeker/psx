#ifndef PSX_MEMORY_H
#define PSX_MEMORY_H

#include <cstdint>
#include <string>

#define MAIN_RAM_SIZE (2048 * 1024)
#define DCACHE_SIZE 1024
#define EXPANSION_AND_DELAY_SIZE 36 // 0x24

namespace PSX {

class Memory {
private:
    uint8_t *mainRAM;
    uint8_t *dCache;
    uint8_t *expansionAndDelayRegisters; // 0x1F801000 to 0x1F801023
    uint8_t ramSizeRegister[4]; // 0x1F801060
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
    T readExpansionAndDelayRegisters(uint32_t address);
    template <typename T>
    void writeExpansionAndDelayRegisters(uint32_t address, T value);

    template <typename T>
    T readRAMSizeRegister(uint32_t address);
    template <typename T>
    void writeRAMSizeRegister(uint32_t address, T value);

    template <typename T>
    T readCacheControlRegister(uint32_t address);
    template <typename T>
    void writeCacheControlRegister(uint32_t address, T value);
};
}

#endif
