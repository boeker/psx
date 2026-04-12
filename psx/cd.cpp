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

    reset();
}

void CD::reset() {
    image.seekg(0, image.beg);
    read_whole_sector = false;

    minutes = 0;
    seconds = 0;
    sectors = 0;
    offset_in_sector = 0;
}

void CD::seekTo(uint8_t minutes, uint8_t seconds, uint8_t sectors) {
    LOG_CDROM(std::format("Seek to 0x{:02X},0x{:02X},0x{:02X}", minutes, seconds, sectors));

    this->minutes = minutes;
    this->seconds = seconds;
    this->sectors = sectors;
    offset_in_sector = read_whole_sector ? CD_MODE2_SYNC_BYTES : CD_MODE2_DATA_OFFSET;
    seek_in_file();
}

uint8_t CD::readByte() {
    if (get_remaining_bytes_in_sector() == 0) {
        // TODO Handle mix of readWord and readByte
        seek_to_next_sector();
    }
    uint8_t val;
    image.read(reinterpret_cast<char*>(&val), 1);
    offset_in_sector++;
    if (get_remaining_bytes_in_sector() == 0) {
        seek_to_next_sector();
    }
    return val;
}

uint32_t CD::readWord() {
    if (get_remaining_bytes_in_sector() == 0) {
        // TODO Handle mix of readWord and readByte
        seek_to_next_sector();
    }
    uint32_t val;
    image.read(reinterpret_cast<char*>(&val), 4);
    offset_in_sector += 4;
    return val;
}

void CD::set_read_whole_sector(bool read_whole_sector) {
    this->read_whole_sector = read_whole_sector;
}

uint32_t CD::get_remaining_bytes_in_sector() const {
    return read_whole_sector ? (SECTOR_SIZE - offset_in_sector)
                             : (SECTOR_SIZE - offset_in_sector - 0x118);
}

void CD::seek_to_next_sector() {
    sectors++;
    LOG_CDROM(std::format("Auto-seek to 0x{:02X},0x{:02X},0x{:02X}", minutes, seconds, sectors));
    offset_in_sector = read_whole_sector ? CD_MODE2_SYNC_BYTES : CD_MODE2_DATA_OFFSET;
    seek_in_file();
    // TODO Handle correctly
}

void CD::seek_in_file() {
    uint32_t pos = ((minutes * 60 + (seconds - 2)) * 75 + sectors) * SECTOR_SIZE + offset_in_sector;

    LOG_CDROM(std::format("File seek to position 0x{:08X}", pos));

    image.seekg(pos, image.beg);
    if (!image.good()) {
        throw exceptions::FileReadError(std::format("File seek to 0x{:08X} failed", pos));
    }
}

}

