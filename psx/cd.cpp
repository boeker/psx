#include "cd.h"

#include <cassert>
#include <filesystem>
#include <format>
#include <iostream>

#include "exceptions/exceptions.h"
#include "util/cue.h"
#include "util/log.h"

using namespace util;

namespace PSX {

CD::CD(const std::string &cue_sheet_filename) {
    LOG_CDROM(std::format("Opening cue sheet \"{:s}\"", cue_sheet_filename));
    std::filesystem::path path_to_cue_sheet(cue_sheet_filename);

    cue::Sheet cue_sheet = cue::Parser::parse(cue_sheet_filename);
    for (const cue::File& file : cue_sheet.files) {
        std::filesystem::path path(path_to_cue_sheet.replace_filename(file.filename));
        LOG_CDROM(std::format("Opening file \"{:s}\"", path.native()));
        std::shared_ptr<std::ifstream> ifstream = std::make_shared<std::ifstream>();
        ifstream->open(path.c_str(), std::ios::binary);
        if (!ifstream->good()) {
            throw exceptions::FileReadError("Failed to open file \"" + path.native() + "\"");
        }
        for (const cue::Track& track : file.tracks) {
            LOG_CDROM(std::format("Track number {:d}", track.number));
            assert(track.number > 0);
            tracks.resize(track.number);

            Track &t = tracks[track.number - 1];
            t.file = std::move(ifstream);
            t.mode = track.mode;

            for (const Index& index : track.indexes) {
                LOG_CDROM(std::format("Index number {:d}", index.number));
                t.indexes.push_back(index);
            }
        }
    }


    std::filesystem::path path_to_bin(path_to_cue_sheet.replace_filename(cue_sheet.files[0].filename));

    reset();
}

void CD::reset() {
    for (Track &track : tracks) {
        track.file->seekg(0, track.file->beg);
    }
    read_whole_sector = false;

    minutes = 0;
    seconds = 0;
    sectors = 0;
    offset_in_sector = 0;
}

void CD::seekTo(uint8_t minutes, uint8_t seconds, uint8_t sectors) {
    LOG_CDROM(std::format("Seek to 0x{:02X},0x{:02X},0x{:02X}", minutes, seconds, sectors));

    // The values are in binary coded decimal
    this->minutes = (minutes >> 4) * 10 + (minutes & 0x0F);
    this->seconds = (seconds >> 4) * 10 + (seconds & 0x0F);
    this->sectors = (sectors >> 4) * 10 + (sectors & 0x0F);
    offset_in_sector = read_whole_sector ? CD_MODE2_SYNC_BYTES : CD_MODE2_DATA_OFFSET;
    seek_in_file();
}

uint8_t CD::readByte() {
    if (get_remaining_bytes_in_sector() == 0) {
        // TODO Handle mix of readWord and readByte
        seek_to_next_sector();
    }
    uint8_t val;
    tracks[0].file->read(reinterpret_cast<char*>(&val), 1);
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
    tracks[0].file->read(reinterpret_cast<char*>(&val), 4);
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

void CD::parse_cue_sheet(const std::string &filename) {
    std::ifstream file(filename);

}

void CD::seek_to_next_sector() {
    sectors++;
    if (sectors == 75) {
        sectors = 0;
        seconds++;
        if (seconds == 60) {
            seconds = 0;
            minutes++;
            // TODO Handle reaching end of disc
        }
    }
    LOG_CDROM(std::format("Auto-seek to 0x{:02X},0x{:02X},0x{:02X}", minutes, seconds, sectors));
    offset_in_sector = read_whole_sector ? CD_MODE2_SYNC_BYTES : CD_MODE2_DATA_OFFSET;
    seek_in_file();
    // TODO Handle correctly
}

void CD::seek_in_file() {
    uint32_t pos = ((minutes * 60 + (seconds - 2)) * 75 + sectors) * SECTOR_SIZE + offset_in_sector;

    LOG_CDROM(std::format("File seek to position 0x{:08X}", pos));

    tracks[0].file->seekg(pos, tracks[0].file->beg);
    if (!tracks[0].file->good()) {
        throw exceptions::FileReadError(std::format("File seek to 0x{:08X} failed", pos));
    }
}

}

