#include "memory.h"

#include <cstring>
#include <format>
#include <fstream>

#include "exceptions/addressoutofbounds.h"
#include "exceptions/memory.h"

namespace PSX {
Memory::Memory() {
    mainRAM = new uint8_t[MAIN_RAM_SIZE];
    ioPorts = new uint8_t[IO_PORTS_SIZE];
    bios = new uint8_t[BIOS_SIZE];

    reset();
}

Memory::~Memory() {
    delete[] mainRAM;
    delete[] bios;
}

void Memory::reset() {
    std::memset(mainRAM, 0, MAIN_RAM_SIZE);
    std::memset(ioPorts, 0, IO_PORTS_SIZE);
    std::memset(bios, 0, BIOS_SIZE);
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
    uint8_t *memory = (uint8_t*)resolveAddress(address);
    return *memory;
}

uint32_t Memory::readWord(uint32_t address) {
    uint32_t *memory = (uint32_t*)resolveAddress(address); // PSX is little endian, so is x86
    return *memory;
}

void Memory::writeWord(uint32_t address, uint32_t word) {
    uint32_t *memory = (uint32_t*)resolveAddress(address); // PSX is little endian, so is x86
    *memory = word;
}

void* Memory::resolveAddress(uint32_t address) {
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
    
    throw exceptions::AddressOutOfBounds(std::format("{:x}", address));
}
}
