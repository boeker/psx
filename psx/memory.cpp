#include "memory.h"

#include <cassert>
#include <cstring>
#include <format>
#include <fstream>
#include <sstream>

#include "util/log.h"
#include "exceptions/exceptions.h"

using namespace util;

namespace PSX {
Memory::Memory() {
    mainRAM = new uint8_t[MAIN_RAM_SIZE];
    dCache = new uint8_t[DCACHE_SIZE];
    expansionAndDelayRegisters = new uint8_t[EXPANSION_AND_DELAY_SIZE];

    reset();
}

Memory::~Memory() {
    delete[] mainRAM;
    delete[] dCache;
    delete[] expansionAndDelayRegisters;
}

void Memory::reset() {
    std::memset(mainRAM, 0, MAIN_RAM_SIZE);
    std::memset(dCache, 0, DCACHE_SIZE);
    std::memset(expansionAndDelayRegisters, 0, EXPANSION_AND_DELAY_SIZE);
    std::memset(cacheControlRegister, 0, 4);
}


template <typename T>
T Memory::readMainRAM(uint32_t address) {
    uint32_t offset = address & 0x001FFFFF;
    assert(offset < MAIN_RAM_SIZE);

    return *((T*)(mainRAM + offset));
}

template uint32_t Memory::readMainRAM<uint32_t>(uint32_t address);
template uint16_t Memory::readMainRAM<uint16_t>(uint32_t address);
template uint8_t Memory::readMainRAM<uint8_t>(uint32_t address);

template <typename T>
void Memory::writeMainRAM(uint32_t address, T value) {
    uint32_t offset = address & 0x001FFFFF;
    assert(offset < MAIN_RAM_SIZE);

    *((T*)(mainRAM + offset)) = value;
}

template void Memory::writeMainRAM(uint32_t address, uint32_t value);
template void Memory::writeMainRAM(uint32_t address, uint16_t value);
template void Memory::writeMainRAM(uint32_t address, uint8_t value);

template <typename T>
T Memory::readDCache(uint32_t address) {
    uint32_t offset = address & 0x000003FF;
    assert(offset < DCACHE_SIZE);

    return *((T*)(dCache + offset));
}

template uint32_t Memory::readDCache<uint32_t>(uint32_t address);
template uint16_t Memory::readDCache<uint16_t>(uint32_t address);
template uint8_t Memory::readDCache<uint8_t>(uint32_t address);

template <typename T>
void Memory::writeDCache(uint32_t address, T value) {
    uint32_t offset = address & 0x000003FF;
    assert(offset < DCACHE_SIZE);

    *((T*)(dCache + offset)) = value;
}

template void Memory::writeDCache(uint32_t address, uint32_t value);
template void Memory::writeDCache(uint32_t address, uint16_t value);
template void Memory::writeDCache(uint32_t address, uint8_t value);

template <typename T>
T Memory::readExpansionAndDelayRegisters(uint32_t address) {
    uint32_t offset = address & 0x0000003F;
    assert(offset + sizeof(T) < MEMORY_CONTROL_SIZE);

    return *((T*)(expansionAndDelayRegisters + offset));
}

template uint32_t Memory::readExpansionAndDelayRegisters<uint32_t>(uint32_t address);
template uint16_t Memory::readExpansionAndDelayRegisters<uint16_t>(uint32_t address);
template uint8_t Memory::readExpansionAndDelayRegisters<uint8_t>(uint32_t address);

template <typename T>
void Memory::writeExpansionAndDelayRegisters(uint32_t address, T value) {
    uint32_t offset = address & 0x0000003F;
    assert(offset + sizeof(T) < MEMORY_CONTROL_SIZE);

    *((T*)(expansionAndDelayRegisters + offset)) = value;
}

template void Memory::writeExpansionAndDelayRegisters(uint32_t address, uint32_t value);
template void Memory::writeExpansionAndDelayRegisters(uint32_t address, uint16_t value);
template void Memory::writeExpansionAndDelayRegisters(uint32_t address, uint8_t value);

template <typename T>
T Memory::readRAMSizeRegister(uint32_t address) {
    assert(address == 0x1F801060);

    return *((T*)ramSizeRegister);
}

template uint32_t Memory::readRAMSizeRegister<uint32_t>(uint32_t address);
template uint16_t Memory::readRAMSizeRegister<uint16_t>(uint32_t address);
template uint8_t Memory::readRAMSizeRegister<uint8_t>(uint32_t address);

template <typename T>
void Memory::writeRAMSizeRegister(uint32_t address, T value) {
    assert(address == 0x1F801060);

    *((T*)(ramSizeRegister)) = value;
}

template void Memory::writeRAMSizeRegister(uint32_t address, uint32_t value);
template void Memory::writeRAMSizeRegister(uint32_t address, uint16_t value);
template void Memory::writeRAMSizeRegister(uint32_t address, uint8_t value);

template <typename T>
T Memory::readCacheControlRegister(uint32_t address) {
    assert(address == 0xFFFE0130);

    return *((T*)cacheControlRegister);
}

template uint32_t Memory::readCacheControlRegister<uint32_t>(uint32_t address);
template uint16_t Memory::readCacheControlRegister<uint16_t>(uint32_t address);
template uint8_t Memory::readCacheControlRegister<uint8_t>(uint32_t address);

template <typename T>
void Memory::writeCacheControlRegister(uint32_t address, T value) {
    assert(address == 0xFFFE0130);

    *((T*)(cacheControlRegister)) = value;
}

template void Memory::writeCacheControlRegister(uint32_t address, uint32_t value);
template void Memory::writeCacheControlRegister(uint32_t address, uint16_t value);
template void Memory::writeCacheControlRegister(uint32_t address, uint8_t value);

}
