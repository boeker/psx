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
    reset();
}

Executable::~Executable() {
}

void Executable::reset() {
}

void Executable::readFromFile(const std::string &file) {
    std::ifstream biosFile(file.c_str(), std::ios::binary | std::ios::ate);
    uint32_t size = biosFile.tellg();
    LOG_EXE(std::format("Opening file \"{:s}\" of size {:d}", file, size));
}

}
