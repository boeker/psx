#include "executable.h"

#include <cassert>
#include <cstring>
#include <format>
#include <fstream>
#include <sstream>

#include "util/log.h"
#include "exceptions/exceptions.h"

using namespace util;

namespace PSX {
Executable::Executable() {
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
}

void Executable::readFromFile(const std::string &file) {
    std::ifstream exeFile(file.c_str(), std::ios::binary | std::ios::ate);
    exeLength = exeFile.tellg();
    exeFile.seekg(0);
    LOG_EXE(std::format("Opening file \"{:s}\" of size {:d}", file, exeLength));

    if (!exeFile.good()) {
        throw exceptions::FileReadError("File \"" + file + "\"not found");
    }

    reset();
    exe = new uint8_t[exeLength];
    exeFile.read(reinterpret_cast<char*>(exe), exeLength);

    if (exeFile.fail()) {
        throw exceptions::FileReadError("Executable file read failed");
    }

    writeExecutableToMemory();
}

void Executable::writeExecutableToMemory() {
    char asciiID[9];
    std::memcpy(asciiID, exe, 8);
    asciiID[8] = '\0';

    uint32_t initialPC = *((uint32_t*)(exe + 0x10));
    uint32_t initialR28 = *((uint32_t*)(exe + 0x14));
    uint32_t destination = *((uint32_t*)(exe + 0x18));
    uint32_t fileSize = *((uint32_t*)(exe + 0x1C));
    uint32_t memfillStart = *((uint32_t*)(exe + 0x28));
    uint32_t memfillSize = *((uint32_t*)(exe + 0x2C));
    uint32_t initialR2930Base = *((uint32_t*)(exe + 0x30));
    uint32_t initialR2930Offset = *((uint32_t*)(exe + 0x34));

    std::stringstream asciiMarker;
    for (int i = 0x4C; i < 0x800; ++i) {
        char nextChar = (i < exeLength) ? *((char*)(exe + i)) : '\0';
        if (nextChar != '\0') {
            asciiMarker << nextChar;
        } else {
            break;
        }
    }

    LOG_EXE(std::format("ASCII ID: {:s}", asciiID));
    LOG_EXE(std::format("Initial PC: 0x{:08X}", initialPC));
    LOG_EXE(std::format("Initial R28: 0x{:08X}", initialR28));
    LOG_EXE(std::format("Destination: 0x{:08X}", destination));
    LOG_EXE(std::format("File Size: 0x{:08X}", fileSize));
    LOG_EXE(std::format("Memfill Start: 0x{:08X}", memfillStart));
    LOG_EXE(std::format("Memfill Size: 0x{:08X}", memfillSize));
    LOG_EXE(std::format("Initial R29 and R30 Base: 0x{:08X}", initialR2930Base));
    LOG_EXE(std::format("Initial R29 and R30 Offset: 0x{:08X}", initialR2930Offset));
    LOG_EXE(std::format("ASCII Marker: {:s}", asciiMarker.str()));
}

}
