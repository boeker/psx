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

        if (file.tracks.empty()) {
            throw exceptions::FileReadError(std::format("File \"{:s}\" has no tracks", file.filename));
        }

        for (const cue::Track& track : file.tracks) {
            if (track.number != previous_track_number + 1) {
                throw exceptions::FileReadError(std::format("Non-consecutive track numbers: Encountered track {:d} in file \"{:s}\", but previous track was {:d}", track.number, file.filename, previous_track_number));
            }
            previous_track_number = track.number;

            if (track.indexes.empty()) {
                throw exceptions::FileReadError(std::format("Track {:d} in file \"{:s}\" has no indexes", track.number, file.filename));
            }

            uint32_t previous_index_number = 0;
            Index previous_index(0, 0, 0);
            bool first = true;
            for (const cue::NumberedIndex& index : track.indexes) {
                // A file might have a zero index
                if (first && index.number == 0) {
                    first = false;
                    continue;
                }
                if (index.number != previous_index_number + 1) {
                    throw exceptions::FileReadError(std::format("Non-consecutive index numbers: Encountered index number {:d} in track {:d} in file \"{:s}\", but previous index number was {:d}", index.number, track.number, file.filename, previous_index_number));
                }
                previous_index_number = index.number;

                if (!first && index.index <= previous_index) {
                    throw exceptions::FileReadError(std::format("Non-strictly increasing indexes: Encountered index {:s} in track {:d} in file \"{:s}\", but previous index was {:s}", index.index, track.number, file.filename, previous_index));
                }
                previous_index = index.index;
                first = false;
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
    reset_position();
}

void CD::reset_position() {
    current_sector = 2 * 75; // First track always has a 2-second pregap
    current_sector_in_file = current_sector;

    current_file = files.begin();
    if (current_file != files.end()) {
        current_file->stream.seekg(0, std::ios::beg);
        current_track = current_file->tracks.begin();

        if (current_track != current_file->tracks.end()) {
            current_index = current_track->indexes.begin();
        }
    }
}

CD::Index CD::get_current_position() {
    return Index(current_sector);
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
    LOG_CDROM(std::format("Seek to next sector from {}", get_current_position()));
    seek_by(0, 0, 1);
    LOG_CDROM(std::format("At {} now", get_current_position()));
}

bool CD::at_end_of_disc() const {
    return current_file == files.end();
}

bool CD::read_sector_and_advance(uint8_t* buffer) {
    LOG_CDROM(std::format("Reading sector into buffer"));
    while (current_file != files.end()) {
        current_file->stream.read(reinterpret_cast<char*>(buffer), SECTOR_SIZE);

        // We might have been at the end of the file, have to jump to the next file
        if (current_file->stream.eof()) {
            LOG_CDROM(std::format("Reached eof, moving to next file and reading from there instead"));
            move_to_next_file();
            continue;
        }

        increment_current_position();
        return true;
    }

    LOG_CDROM(std::format("Read from end of disc"));
    return false;
}

void CD::seek_to(uint8_t minutes, uint8_t seconds, uint8_t sectors) {
    reset_position();

    Index target_position(minutes, seconds, sectors);
    Index relative_target_position = target_position - get_current_position();

    seek_by(relative_target_position.minutes,
            relative_target_position.seconds,
            relative_target_position.sectors);
}

void CD::seek_by(uint8_t minutes, uint8_t seconds, uint8_t sectors) {
    Index target_position = get_current_position() + Index(minutes, seconds, sectors);

    while (current_file != files.end() && get_current_position() < target_position) {
        current_file->stream.seekg(SECTOR_SIZE, std::ios::cur);

        // We might have been at the end of the file, have to jump to the next file
        if (current_file->stream.eof()) {
            move_to_next_file();
            continue;
        }

        increment_current_position();
    }
}

void CD::move_to_next_file() {
    ++current_file;
    if (current_file != files.end()) {
        current_file->stream.seekg(0, std::ios::beg);
        current_sector_in_file = 2 * 75;

        current_track = current_file->tracks.begin();
        if (current_track != current_file->tracks.end()) {
            current_index = current_track->indexes.begin();
        }
    }
}

void CD::increment_current_position() {
    ++current_sector;
    ++current_sector_in_file;

    // Keep track of the track and index we currently are in
    auto next_track = current_track;
    auto next_index = current_index + 1;
    bool has_next_index = true;
    if (next_index == next_track->indexes.end()) {
        next_track = current_track + 1;
        if (next_track != current_file->tracks.end()) {
            next_index = next_track->indexes.begin();

        } else {
            has_next_index = false;
        }
    }

    if (has_next_index && Index(current_sector_in_file) >= next_index->index) {
        current_track = next_track;
        current_index = next_index;
    }
}

}

