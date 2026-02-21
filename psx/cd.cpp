#include "cd.h"

#include <format>

#include "exceptions/exceptions.h"
#include "util/log.h"

namespace PSX {

CD::CD(const std::string &filename)
    : image() {
    LOG_CDROM(std::format("Opening CD image \"{:s}\"", filename));

    image.open(filename.c_str(), std::ios::binary);
    if (!image.good()) {
        throw exceptions::FileReadError("Failed to open file \"" + filename + "\"");
    }
}

void CD::seekTo(uint8_t minutes, uint8_t seconds, uint8_t sectors) {
    LOG_CDROM(std::format("Seek to 0x{:02X},0x{:02X},0x{:02X}", minutes, seconds, sectors));

    uint32_t pos = ((minutes * 60 + (seconds - 2)) * 75 + sectors) * SECTOR_SIZE + 12;
    LOG_CDROM(std::format("File seek to position 0x{:08X}", pos));

    image.seekg(pos, image.beg);
    if (!image.good()) {
        throw exceptions::FileReadError(std::format("File seek to 0x{:08X} failed", pos));
    }
}

uint8_t CD::readByte() {
    uint8_t val;
    image.read(reinterpret_cast<char*>(&val), 1);
    return val;
}

}

