#include "cd.h"

#include <cassert>
#include <cstring>
#include <filesystem>
#include <format>
#include <iostream>
#include <limits>

#include "exceptions/exceptions.h"
#include "util/cue.h"
#include "util/log.h"

using namespace util;

namespace PSX {

CD::CD(const std::string &cue_sheet_filename) {
    open_cue_sheet(cue_sheet_filename);
    reset();
}

void CD::open_cue_sheet(const std::string &filename) {
    LOG_CDROM(std::format("Opening cue sheet \"{:s}\"", filename));
    std::filesystem::path path_to_cue_sheet(filename);

    cue::Sheet cue_sheet = cue::Parser::parse(filename);

    // Verify validity first
    uint32_t previous_track_number = 0;
    for (const cue::File& file : cue_sheet.files) {
        if (file.type != cue::File::Type::BINARY) {
            throw exceptions::FileReadError(std::format("Unsupported type for file \"{:s}\": {:s}", file.filename, cue::File::type_to_string(file.type)));
        }
        for (const cue::Track& track : file.tracks) {
            if (track.number != previous_track_number + 1) {
                throw exceptions::FileReadError(std::format("Non-consecutive track numbers: Encountered track {:d} in file \"{:s}\", but previous track was {:d}", track.number, file.filename, previous_track_number));
            }
            previous_track_number = track.number;

            uint32_t previous_index_number = 0;
            Index previous_index(0, 0, 0);
            bool has_zero_index = false;
            for (const cue::NumberedIndex& index : track.indexes) {
                // A file might have a zero index
                if (index.number == 0 && !has_zero_index) {
                    has_zero_index = true;
                    previous_index = index.index;
                    continue;
                }
                if (index.number != previous_index_number + 1) {
                    throw exceptions::FileReadError(std::format("Non-consecutive index numbers: Encountered index number {:d} in track {:d} in file \"{:s}\", but previous index number was {:d}", index.number, track.number, file.filename, previous_index_number));
                }
                previous_index_number = index.number;

                if (index.index < previous_index) {
                    throw exceptions::FileReadError(std::format("Non-consecutive indexes: Encountered index {:s} in track {:d} in file \"{:s}\", but previous index was {:s}", index.index, track.number, file.filename, previous_index));
                }
                previous_index = index.index;
            }
        }
    }

    for (const cue::File& cue_file : cue_sheet.files) {
        std::filesystem::path path(path_to_cue_sheet.replace_filename(cue_file.filename));
        LOG_CDROM(std::format("Opening file \"{:s}\"", path.native()));

        File file;
        file.stream.open(path.c_str(), std::ios::binary);
        if (!file.stream.good()) {
            throw exceptions::FileReadError("Failed to open file \"" + path.native() + "\"");
        }
        file.tracks = cue_file.tracks;
        files.emplace_back(std::move(file));
    }
}

void CD::reset() {
    for (File &file: files) {
        file.stream.seekg(0, std::ios::beg);
    }
    current_file = files.begin();
    current_position.reset();
    current_position_in_track.reset();

    read_whole_sector = false;

    minutes = 0;
    seconds = 0;
    sectors = 0;
    offset_in_sector = 0;
}

void CD::seek_to_bcd(uint8_t bcd_minutes, uint8_t bcd_seconds, uint8_t bcd_sectors) {
    LOG_CDROM(std::format("Absolute seek to 0x{:02X},0x{:02X},0x{:02X} (BCD)", bcd_minutes, bcd_seconds, bcd_sectors));

    uint8_t minutes = (bcd_minutes >> 4) * 10 + (bcd_minutes & 0x0F);
    uint8_t seconds = (bcd_seconds >> 4) * 10 + (bcd_seconds & 0x0F);
    uint8_t sectors = (bcd_sectors >> 4) * 10 + (bcd_sectors & 0x0F);
    seek_to(minutes, seconds, sectors);
}

void CD::seek_to_dec(uint8_t minutes, uint8_t seconds, uint8_t sectors) {
    LOG_CDROM(std::format("Absolute seek to {:d},{:d},{:d} (decimal)", minutes, seconds, sectors));

    seek_to(minutes, seconds, sectors);
}

void CD::seek_to_next_sector() {
    LOG_CDROM(std::format("Seek to next sector"));
    seek_by(0, 0, 1);
}

bool CD::at_end_of_disc() const {
    return current_file == files.end();
}

bool CD::read_sector_into_buffer() {
    current_file->stream.read(reinterpret_cast<char*>(sector_buffer), SECTOR_SIZE);
    return !current_file->stream.eof();
}

std::span<uint8_t> CD::get_sector_buffer() {
    return { &sector_buffer[0], SECTOR_SIZE };
}

void CD::seek_to(uint8_t minutes, uint8_t seconds, uint8_t sectors) {
    current_position = { 0, 2, 0 }; // First track always has a 2-second pregap

    Index target_position(minutes, seconds, sectors);
    Index relative_target_position = target_position - current_position;

    seek_by(relative_target_position.minutes,
            relative_target_position.seconds,
            relative_target_position.sectors);
}

void CD::seek_by(uint8_t minutes, uint8_t seconds, uint8_t sectors) {
    Index relative_position(minutes, seconds, sectors);
    Index target_position = current_position + relative_position;

    current_file = files.begin();
    for (current_file = files.begin(); current_file != files.end(); ++current_file) {
        std::ifstream& stream = current_file->stream;
        stream.seekg(0, std::ios::beg);

        while (current_position < target_position) {
            stream.seekg(SECTOR_SIZE, std::ios::cur);
            ++current_position;
        }

        if ((current_position == target_position)
            && !stream.read(reinterpret_cast<char*>(sector_buffer), SECTOR_SIZE).eof()) {
            break;
        }

        // TODO Track track and index numbers
    }
}

void CD::set_read_whole_sector(bool read_whole_sector) {
    this->read_whole_sector = read_whole_sector;
}

uint32_t CD::get_remaining_bytes_in_sector() const {
    return read_whole_sector ? (SECTOR_SIZE - offset_in_sector)
                             : (SECTOR_SIZE - offset_in_sector - 0x118);
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
        leg_seek_to_next_sector();
    }
    uint8_t val;
    files[0].stream.read(reinterpret_cast<char*>(&val), 1);
    offset_in_sector++;
    if (get_remaining_bytes_in_sector() == 0) {
        leg_seek_to_next_sector();
    }
    return val;
}

uint32_t CD::readWord() {
    if (get_remaining_bytes_in_sector() == 0) {
        // TODO Handle mix of readWord and readByte
        leg_seek_to_next_sector();
    }
    uint32_t val;
    files[0].stream.read(reinterpret_cast<char*>(&val), 4);
    offset_in_sector += 4;
    return val;
}

void CD::leg_seek_to_next_sector() {
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

    files[0].stream.seekg(pos, std::ios::beg);
    if (!files[0].stream.good()) {
        throw exceptions::FileReadError(std::format("File seek to 0x{:08X} failed", pos));
    }
}

}

