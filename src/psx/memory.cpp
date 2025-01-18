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
    ioPorts = new uint8_t[IO_PORTS_SIZE];
    bios = new uint8_t[BIOS_SIZE];
    dCache = new uint8_t[DCACHE_SIZE];

    reset();
}

Memory::~Memory() {
    delete[] mainRAM;
    delete[] ioPorts;
    delete[] bios;
    delete[] dCache;
}

void Memory::reset() {
    regs.reset();

    std::memset(mainRAM, 0, MAIN_RAM_SIZE);
    std::memset(ioPorts, 0, IO_PORTS_SIZE);
    std::memset(bios, 0, BIOS_SIZE);
    std::memset(dCache, 0, DCACHE_SIZE);
    std::memset(&cacheControlRegister, 0, 4);
}

void Memory::readBIOS(const std::string &file) {
    std::ifstream biosFile(file.c_str(), std::ios::binary);
    if (!biosFile.good()) {
        throw exceptions::FileReadError("BIOS file \"" + file + "\"not found");
    }
    
    biosFile.read(reinterpret_cast<char*>(bios), BIOS_SIZE);

    if (biosFile.fail()) {
        throw exceptions::FileReadError("BIOS file read failed");
    }
}

uint8_t Memory::readByte(uint32_t address) {
    Log::log(std::format(" [rb "), Log::Type::MEMORY);
    uint8_t *memory = (uint8_t*)resolveAddress2(address);

    uint8_t byte = *memory;
    Log::log(std::format("@0x{:08X} -0x{:02X}->]", address, byte), Log::Type::MEMORY);

    return byte;
}

uint16_t Memory::readHalfWord(uint16_t address) {
    Log::log(std::format(" [rhw "), Log::Type::MEMORY);
    uint16_t *memory = (uint16_t*)resolveAddress2(address); // PSX is little endian, so is x86

    uint16_t halfWord = *memory;
    Log::log(std::format("@0x{:08X} -0x{:04X}->]", address, halfWord), Log::Type::MEMORY);

    return halfWord;
}

uint32_t Memory::readWord(uint32_t address) {
    Log::log(std::format(" [rw "), Log::Type::MEMORY);
    uint32_t *memory = (uint32_t*)resolveAddress2(address); // PSX is little endian, so is x86

    uint32_t word = *memory;
    Log::log(std::format("@0x{:08X} -0x{:08X}->]", address, word), Log::Type::MEMORY);

    return word;
}

void Memory::writeByte(uint32_t address, uint8_t byte) {
    Log::log(std::format(" [wb -0x{:02X}-> @0x{:08X}]", byte, address), Log::Type::MEMORY);
    uint8_t *memory = (uint8_t*)resolveAddress2(address);
    *memory = byte;
}

void Memory::writeHalfWord(uint32_t address, uint16_t halfWord) {
    Log::log(std::format(" [whw -0x{:04X}-> @0x{:08X}]", halfWord, address), Log::Type::MEMORY);
    uint16_t *memory = (uint16_t*)resolveAddress2(address); // PSX is little endian, so is x86
    *memory = halfWord;
}

void Memory::writeWord(uint32_t address, uint32_t word) {
    Log::log(std::format(" [ww -0x{:08X}-> @0x{:08X}]", word, address), Log::Type::MEMORY);
    uint32_t *memory = (uint32_t*)resolveAddress2(address); // PSX is little endian, so is x86
    *memory = word;
}

void* Memory::resolveAddress(uint32_t address) {
    if (regs.statusRegisterIsolateCacheIsSet()) {
        //return dCache + (address & 0x3FF);
    }

    if (address < MAIN_RAM_SIZE) {
        return mainRAM + address;
    }
    if ((address >= 0x1F801000) && (address < 0x1F801000 + IO_PORTS_SIZE)) {
        return ioPorts + (address - 0x1F801000);
    }
    if ((address >= 0x80000000) && (address < 0x80000000 + MAIN_RAM_SIZE)) {
        return mainRAM + (address - 0x80000000);
    }
    if ((address >= 0x9FC00000) && (address < 0x9FC00000 + BIOS_SIZE)) {
        return bios + (address - 0x9FC00000);
    }
    if ((address >= 0xA0000000) && (address < 0xA0000000 + MAIN_RAM_SIZE)) {
        return mainRAM + (address - 0xA0000000);
    }
    if ((address >= 0xBFC00000) && (address < 0xBFC00000 + BIOS_SIZE)) {
        return bios + (address - 0xBFC00000);
    }
    if (address == 0xFFFE0130) {
        return &cacheControlRegister;
    }

    std::stringstream ss;
    ss << regs;
    throw exceptions::AddressOutOfBounds(std::format("@{:08X}, register contents:\n{:s}", address, ss.str()));
}

void* Memory::resolveAddress2(uint32_t address) {
    // 512MiB Memory Regions
    // 0x00000000 KUSEG
    // 0x20000000 KUSEG (Error)
    // 0x40000000 KUSEG (Error)
    // 0x60000000 KUSEG (Error)
    // 0x80000000 KSEG0
    // 0xA0000000 KSEG1 (No Scratchpad!)
    // 0xC0000000 KSEG2
    // 0xE0000000 KSEG2

    // Main RAM
    // 0x00000000 - 0x001FFFFF (0b0000 0000 0000 ... - 0b0000 0000 0001 ...)
    // 0x80000000 - 0x801FFFFF (0b1000 0000 0000 ... - 0b1000 0000 0001 ...)
    // 0xA0000000 - 0xA01FFFFF (0b1010 0000 0000 ... - 0b1010 0000 0001 ...)
    if ((address & 0x1FE00000) == 0x00000000) {
        if (regs.statusRegisterIsolateCacheIsSet()) {
            uint32_t offset = address & 0x000003FF;
            assert(offset < DCACHE_SIZE);
            return dCache + offset;

        } else {
            uint32_t offset = address & 0x001FFFFF;
            assert(offset < MAIN_RAM_SIZE);
            return mainRAM + offset;
        }
    }

    // D-Cache (Scratchpad)
    // 0x1F800000 - 0x1F8003FF (0b0001 1111 1000 ... - 0b0001 1111 1000 ...)
    // Also in KSEG0?
    // 0x9F800000 - 0x9F8003FF (0b1001 1111 1000 ... - 0b1001 1111 1000 ...)
    // But not in KSEG1?
    if ((address & 0x1FFFFC00) == 0x1F8FFC00) {
        // Also in KSEG1 for now
        uint32_t offset = address & 0x000003FF;
        assert(offset < DCACHE_SIZE);
        return dCache + offset;
    }

    // Hardware Registers (I/0 Ports)
    // 0x1F801000 - 0x1FBFFFFF (0b0001 1111 1000 0000 0001 ... - 0b0001 1111 1011 1111 ...)
    // our memory is smaller (0x1FFF is max value)
    // 0x1F801000 - 0x1F802FFF (0b0001 1111 1000 0000 0001 ... - 0b0001 1111 1000 0000 ...)
    if (((address & 0x1FFFF000) == 0x1F801000)) {
        uint32_t offset = address & 0x00000FFF;
        assert(offset < IO_PORTS_SIZE);
        return ioPorts + offset;
    }
    if (((address & 0x1FFFF000) == 0x1F802000)) {
        uint32_t offset = 0x00001000 + (address & 0x00000FFF);
        assert(offset < IO_PORTS_SIZE);
        return ioPorts + offset;
    }

    // Bios ROM
    // 0x1FC00000 - 0x1FC7FFFF (0b0001 1111 1100 ... - 0b0001 1111 1100 ...)
    // 0x9FC00000 - 0x9FC7FFFF (0b1001 1111 1100 ... - 0b1001 1111 1100 ...)
    // 0xBFC00000 - 0xBFC7FFFF (0b1011 1111 1100 ... - 0b0011 1111 1100 ...)
    if ((address & 0x1FF80000) == 0x1FC00000) {
        uint32_t offset = address & 0x0007FFFF;
        assert(offset < BIOS_SIZE);
        return bios + offset;
    }

    // Cache Control Register
    // 0xFFFE0130
    if (address == 0xFFFE0130) {
        return &cacheControlRegister;
    }
    std::stringstream ss;
    ss << regs;
    throw exceptions::AddressOutOfBounds(std::format("@0x{:08X}, register contents:\n{:s}", address, ss.str()));
}

}
