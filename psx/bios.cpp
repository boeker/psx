#include "bios.h"

#include <cassert>
#include <cstring>
#include <format>
#include <fstream>
#include <sstream>

#include "util/log.h"
#include "exceptions/exceptions.h"

using namespace util;

namespace PSX {
Bios::Bios() {
    bios = new uint8_t[BIOS_SIZE];
    reset();
}

Bios::~Bios() {
    delete[] bios;
}

void Bios::reset() {
    std::memset(bios, 0, BIOS_SIZE);
}

void Bios::readFromFile(const std::string &file) {
    std::ifstream biosFile(file.c_str(), std::ios::binary);
    if (!biosFile.good()) {
        throw exceptions::FileReadError("BIOS file \"" + file + "\"not found");
    }

    biosFile.read(reinterpret_cast<char*>(bios), BIOS_SIZE);

    if (biosFile.fail()) {
        throw exceptions::FileReadError("BIOS file read failed");
    }
}


template <typename T>
T Bios::read(uint32_t address) {
    uint32_t offset = address & 0x0007FFFF;
    assert(offset < BIOS_SIZE);

    return *((T*)(bios + offset));
}

template uint32_t Bios::read<uint32_t>(uint32_t address);
template uint16_t Bios::read<uint16_t>(uint32_t address);
template uint8_t Bios::read<uint8_t>(uint32_t address);

}
