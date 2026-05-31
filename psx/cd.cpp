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

uint32_t CD::File::read_sectors() {
    auto pos = stream.tellg();
    assert(pos % SECTOR_SIZE == 0);
    uint32_t read = pos / SECTOR_SIZE;
    assert(read <= sectors);
    return read;
}

uint32_t CD::File::remaining_sectors() {
    return sectors - read_sectors();
}

uint32_t CD::File::current_sector() {
    return read_sectors() + 2 * 75;
}

CD::CD(const std::string &cue_sheet_filename) {
    open_cue_sheet(cue_sheet_filename);
    reset();
}

void CD::reset() {
    reset_position();
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
        std::uintmax_t size = 0;
        try {
            size = std::filesystem::file_size(path);
        } catch (std::filesystem::filesystem_error &e) {
            throw exceptions::FileReadError("Failed to determine size of file \"" + path.native() + "\": " + e.what());
        }
        if (size % CD::SECTOR_SIZE != 0) {
            throw exceptions::FileReadError("File does not divide evenly into sectors of size " + CD::SECTOR_SIZE);
        }
        file.sectors = size / CD::SECTOR_SIZE;
        if (file.sectors == 0) {
            throw exceptions::FileReadError("File contains no sectors");
        }
        file.stream.open(path.c_str(), std::ios::binary);
        if (!file.stream.good()) {
            throw exceptions::FileReadError("Failed to open file \"" + path.native() + "\"");
        }
        file.tracks = cue_file.tracks;
        files.emplace_back(std::move(file));
    }
}

void CD::seek_to_bcd(uint8_t bcd_minutes, uint8_t bcd_seconds, uint8_t bcd_sectors) {
    LOG_CDROM(std::format("Absolute seek to 0x{:02X},0x{:02X},0x{:02X} (BCD)", bcd_minutes, bcd_seconds, bcd_sectors));

    uint8_t minutes = (bcd_minutes >> 4) * 10 + (bcd_minutes & 0x0F);
    uint8_t seconds = (bcd_seconds >> 4) * 10 + (bcd_seconds & 0x0F);
    uint8_t sectors = (bcd_sectors >> 4) * 10 + (bcd_sectors & 0x0F);
    seek_to(Index(minutes, seconds, sectors).total_sectors());
}

bool CD::read_sector_and_advance(uint8_t *buffer) {
    LOG_CDROM(std::format("Reading sector into buffer"));
    while (current_file != files.end()) {
        current_file->stream.read(reinterpret_cast<char*>(buffer), SECTOR_SIZE);

        // We might have been at the end of the file, have to jump to the next file
        if (current_file->stream.eof()) {
            LOG_CDROM(std::format("Reached eof, moving to next file and reading from there instead"));
            move_to_next_file();
            continue;
        }

        ++current_sector;
        move_to_track_and_index();

        return true;
    }

    LOG_CDROM(std::format("Read from end of disc"));
    return false;
}

CD::Index CD::get_current_position() {
    return Index(current_sector);
}

bool CD::at_end_of_disc() const {
    return current_file == files.end();
}

void CD::seek_to(uint32_t sectors) {
    LOGV_CDROM(std::format("Seek to {}", Index(sectors)));
    reset_position();

    seek_by(sectors - current_sector);
}

void CD::seek_by(uint32_t sectors) {
    LOGV_CDROM(std::format("Seek by {}", Index(sectors)));

    while (current_file != files.end() && sectors > 0) {
        uint32_t remaining_sectors = current_file->remaining_sectors();
        if (sectors < remaining_sectors) { // Target sector is in current file
            current_file->stream.seekg(sectors * SECTOR_SIZE, std::ios::cur);
            current_sector += sectors;
            move_to_track_and_index();
            break;

        } else { // Target sector is in upcoming file
            current_sector += remaining_sectors;
            sectors -= remaining_sectors;
            // Every file has a 2-second pregap
            // TODO: Handle this
            assert(sectors >= 2 * 75);
            current_sector += 2 * 75;
            sectors -= 2 * 75;
            move_to_next_file();
        }
    }
}

void CD::reset_position() {
    current_sector = 2 * 75; // First track always has a 2-second pregap

    current_file = files.begin();
    if (current_file != files.end()) {
        current_file->stream.seekg(0, std::ios::beg);
        current_track = current_file->tracks.begin();

        if (current_track != current_file->tracks.end()) {
            current_index = current_track->indexes.begin();
        }
    }
}

void CD::move_to_next_file() {
    ++current_file;
    if (current_file != files.end()) {
        current_file->stream.seekg(0, std::ios::beg);

        current_track = current_file->tracks.begin();
        if (current_track != current_file->tracks.end()) {
            current_index = current_track->indexes.begin();
        }
    }
}

void CD::move_to_track_and_index() {
    bool try_next_index = true;
    while (try_next_index) {
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

        bool advance_to_next_index = has_next_index && current_file->current_sector() >= next_index->index;
        if (advance_to_next_index) {
            current_track = next_track;
            current_index = next_index;
        }

        // Also check the next index if we advanced
        try_next_index = advance_to_next_index;
    }
}

}

