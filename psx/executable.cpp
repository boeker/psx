#include "executable.h"

#include <cassert>
#include <cstring>
#include <format>
#include <fstream>
#include <sstream>

#include "bus.h"
#include "util/log.h"
#include "exceptions/exceptions.h"

using namespace util;

namespace PSX {
Executable::Executable(Bus *bus) {
    this->bus = bus;

    exe = nullptr;
    reset();
}

Executable::~Executable() {
    if (exe != nullptr) {
        delete[] exe;
    }
}

void Executable::reset() {
    if (exe != nullptr) {
        delete[] exe;
    }
    exe = nullptr;
    exeLength = 0;
}

void Executable::readFromFile(const std::string &file) {
    reset();

    std::ifstream exeFile(file.c_str(), std::ios::binary | std::ios::ate);
    exeLength = exeFile.tellg();
    exeFile.seekg(0);
    LOG_EXE(std::format("Opening file \"{:s}\" of size {:d}", file, exeLength));

    if (!exeFile.good()) {
        throw exceptions::FileReadError("File \"" + file + "\"not found");
    }

    exe = new uint8_t[exeLength];
    exeFile.read(reinterpret_cast<char*>(exe), exeLength);

    if (exeFile.fail()) {
        throw exceptions::FileReadError("Executable file read failed");
    }

    parseHeader();
}

bool Executable::loaded() {
    return exeLength > 0;
}

void Executable::parseHeader() {
    std::memcpy(header.asciiID, exe, 8);
    header.asciiID[8] = '\0';

    header.initialPC = *((uint32_t*)(exe + 0x10));
    header.initialR28 = *((uint32_t*)(exe + 0x14));
    header.destination = *((uint32_t*)(exe + 0x18));
    header.fileSize = *((uint32_t*)(exe + 0x1C));
    header.memfillStart = *((uint32_t*)(exe + 0x28));
    header.memfillSize = *((uint32_t*)(exe + 0x2C));
    header.initialR2930Base = *((uint32_t*)(exe + 0x30));
    header.initialR2930Offset = *((uint32_t*)(exe + 0x34));

    std::stringstream asciiMarker;
    for (int i = 0x4C; i < 0x800; ++i) {
        char nextChar = (i < exeLength) ? *((char*)(exe + i)) : '\0';
        if (nextChar != '\0') {
            asciiMarker << nextChar;
        } else {
            break;
        }
    }
    header.asciiMarker = asciiMarker.str();

    LOG_EXE(std::format("ASCII ID: {:s}", header.asciiID));
    LOG_EXE(std::format("Initial PC: 0x{:08X}", header.initialPC));
    LOG_EXE(std::format("Initial R28: 0x{:08X}", header.initialR28));
    LOG_EXE(std::format("Destination: 0x{:08X}", header.destination));
    LOG_EXE(std::format("File Size: 0x{:08X}", header.fileSize));
    LOG_EXE(std::format("Memfill Start: 0x{:08X}", header.memfillStart));
    LOG_EXE(std::format("Memfill Size: 0x{:08X}", header.memfillSize));
    LOG_EXE(std::format("Initial R29 and R30 Base: 0x{:08X}", header.initialR2930Base));
    LOG_EXE(std::format("Initial R29 and R30 Offset: 0x{:08X}", header.initialR2930Offset));
    LOG_EXE(std::format("ASCII Marker: {:s}", header.asciiMarker));
}

void Executable::writeToMemory() {
    // Copy to RAM
    assert(header.fileSize <= exeLength + 0x800);
    for (uint32_t i = 0; i <= header.fileSize; ++i) {
        bus->write<uint8_t>(header.destination + i, exe[0x800 + i]);
    }

    // Set PC and register values
    bus->cpu.regs.setPC(header.initialPC);
    bus->cpu.regs.setRegister(28, header.initialR28);

    // When should this be set?
    if (header.initialR2930Base != 0) {
        bus->cpu.regs.setRegister(29, header.initialR2930Base + header.initialR2930Offset);
        bus->cpu.regs.setRegister(30, header.initialR2930Base + header.initialR2930Offset);
    }

    // Memfill
    for (uint32_t i = 0; i <= header.memfillSize; ++i) {
        bus->write<uint8_t>(header.memfillStart + i, 0);
    }
}

}
