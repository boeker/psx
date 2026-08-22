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

CD::BinaryFile::BinaryFile(std::ifstream&& stream, uint32_t sectors)
    : SectorFile(sectors), stream(std::move(stream)) {
    reset();
}

void CD::BinaryFile::reset() {
    stream.seekg(0, std::ios::beg);
    assert(stream.good());
}

void CD::BinaryFile::seek_by(uint32_t sectors) {
    assert(sectors <= get_remaining_sectors());
    stream.seekg(sectors * SECTOR_SIZE, std::ios::cur);
    assert(stream.good());
}

void CD::BinaryFile::read_sector(uint8_t* buffer) {
    assert(stream.good());
    stream.read(reinterpret_cast<char*>(buffer), SECTOR_SIZE);
}

uint32_t CD::BinaryFile::get_read_sectors() {
    auto pos = stream.tellg();
    assert(pos % SECTOR_SIZE == 0);
    uint32_t read = pos / SECTOR_SIZE;
    assert(read <= get_total_sectors());
    return read;
}

CD::Gap::Gap(uint32_t sectors)
    : SectorFile(sectors) {
    reset();
}

void CD::Gap::reset() {
    read_sectors = 0;
}

void CD::Gap::seek_by(uint32_t sectors) {
    assert(sectors <= get_remaining_sectors());
    read_sectors += sectors;
}

void CD::Gap::read_sector(uint8_t* buffer) {
    assert(read_sectors < get_total_sectors());
    ++read_sectors;
    std::memset(buffer, 0, SECTOR_SIZE);
}

uint32_t CD::Gap::get_read_sectors() {
    return read_sectors;
}


CD::CD(const std::string &cue_sheet_filename) {
    open_cue_sheet(cue_sheet_filename);
    reset();
}

void CD::reset() {
    reset_position();
}

void CD::open_cue_sheet(const std::string &filename) {
    LOG_CDIMG(std::format("Opening cue sheet \"{:s}\"", filename));
    std::filesystem::path path_to_cue_sheet(filename);

    cue::Sheet cue_sheet = cue::Parser::parse(filename);

    // Keep track where on the disc we are
    uint32_t current_position_on_disc = 0;

    uint32_t expected_track_number = 1;
    for (const cue::File& file : cue_sheet.files) {
        // Open the file
        std::shared_ptr<SectorFile> sector_file;
        uint32_t sectors_in_file = 0;
        if (file.type == cue::File::Type::BINARY) {
            std::filesystem::path path_to_file(path_to_cue_sheet.replace_filename(file.filename));
            LOG_CDIMG(std::format("About to open file \"{:s}\"", path_to_file.native()));

            std::uintmax_t size = 0;
            try {
                size = std::filesystem::file_size(path_to_file);
            } catch (std::filesystem::filesystem_error &e) {
                throw exceptions::FileReadError(std::format("Failed to determine size of file: {:s}", e.what()));
            }
            if (size % CD::SECTOR_SIZE != 0) {
                throw exceptions::FileReadError("File does not divide evenly into sectors of size " + CD::SECTOR_SIZE);
            }
            sectors_in_file = size / CD::SECTOR_SIZE;
            if (sectors_in_file == 0) {
                throw exceptions::FileReadError("File contains no sectors");
            }

            std::ifstream stream;
            stream.open(path_to_file.c_str(), std::ios::binary);
            if (!stream.good()) {
                throw exceptions::FileReadError("Failed to open file for reading");
            }

            sector_file = std::make_shared<BinaryFile>(std::move(stream), sectors_in_file);
            files.emplace_back(sector_file);
        } else {
            throw exceptions::FileReadError(std::format("Unsupported type for file \"{:s}\": {:s}", file.filename, cue::File::type_to_string(file.type)));
        }

        if (file.tracks.empty()) {
            throw exceptions::FileReadError(std::format("File \"{:s}\" has no tracks", file.filename));
        }

        for (auto track_it = file.tracks.cbegin(); track_it != file.tracks.cend(); ++track_it) {
            if (track_it->number != expected_track_number) {
                throw exceptions::FileReadError(std::format("Non-consecutive track numbers: Encountered track {:d} in file \"{:s}\", but expected {:d}", track_it->number, file.filename, expected_track_number));
            }
            ++expected_track_number;

            if (track_it->indexes.empty()) {
                throw exceptions::FileReadError(std::format("Track {:d} in file \"{:s}\" has no indexes", track_it->number, file.filename));
            }

            tracks.emplace_back(track_it->mode, track_it->number, current_position_on_disc);
            const TrackOnDisc& track_on_disc(tracks.back());

            // No INDEX 00 => pre-gap not contained in file, we have to manually add one of two-second length
            // TODO Add support for PREGAP statements (instead of INDEX 00 statements, states length of pre-gap)
            if (track_it->indexes.front().number != 0) {
                // INDEX 00 00:00:00 of two-second length
                indexes.emplace_back(track_on_disc, 0, 0, CD_TWO_SECONDS, std::make_shared<Gap>(CD_TWO_SECONDS), 0);
                current_position_on_disc += CD_TWO_SECONDS; // Two-second offset caused by gap
            }

            // Where in the sector_file we are.
            // No the position in the cue FILE (gaps are added there, but not here).
            uint32_t current_position_in_file = 0;

            uint32_t expected_index_number = 1;
            for (auto index_it = track_it->indexes.cbegin(); index_it != track_it->indexes.cend(); ++index_it) {
                if (index_it->number != expected_index_number) {
                    throw exceptions::FileReadError(std::format("Non-consecutive index numbers: Encountered index number {:d} in track {:d} in file \"{:s}\", but expected index number was {:d}", index_it->number, track_it->number, file.filename, expected_index_number));
                }
                ++expected_index_number;


                // Determine length of the current index, i.e., sectors that follow until the next index/end of file.
                uint32_t index_length = 0;
                auto next_index_it = std::next(index_it);
                auto next_track_it = std::next(track_it);
                if (next_index_it != track_it->indexes.cend()) { // Another index in track
                    uint32_t index_sectors = index_it->index.total_sectors();
                    uint32_t next_index_sectors = next_index_it->index.total_sectors();

                    if (next_index_sectors < index_sectors) {
                        throw exceptions::FileReadError(std::format("Index following {:s} in track {:d} in file \"{:s}\" is earlier on disc: {:s}", index_it->index, track_it->number, file.filename, next_index_it->index));
                    }
                    index_length = next_index_sectors - index_sectors;

                } else if (next_track_it != file.tracks.cend()) { // Another track in file
                    const auto& next_indexes = next_track_it->indexes;
                    if (!next_indexes.empty()) { // Empty indexes will produce error message on next iteration
                        uint32_t index_sectors = index_it->index.total_sectors();
                        uint32_t next_index_sectors = next_indexes.front().index.total_sectors();;

                        // TODO Avoid code duplication
                        if (next_index_sectors < index_sectors) {
                            throw exceptions::FileReadError(std::format("Index following {:s} in track {:d} in file \"{:s}\" is earlier on disc: {:s}", index_it->index, track_it->number, file.filename, next_index_it->index));
                        }
                        index_length = next_index_sectors - index_sectors;
                    }
                } else { // Last index in file
                    index_length = sectors_in_file - current_position_in_file;
                }

                // TODO position_in_track
                uint32_t position_in_track = 0;
                indexes.emplace_back(track_on_disc, index_it->number, position_in_track, index_length, sector_file, current_position_in_file);

                current_position_on_disc += index_length;
                current_position_in_file += index_length;

                if (current_position_in_file > sectors_in_file) {
                    throw exceptions::FileReadError(std::format("Index {:s} in track {:d} in file \"{:s}\" goes beyond sectors contained in file: {:s}", index_it->index, track_it->number, file.filename, Index(current_position_in_file)));
                }
            }
        }
    }
}

void CD::seek_to_bcd(uint8_t bcd_minutes, uint8_t bcd_seconds, uint8_t bcd_sectors) {
    LOGV_CDIMG(std::format("Absolute seek to 0x{:02X},0x{:02X},0x{:02X} (BCD)", bcd_minutes, bcd_seconds, bcd_sectors));

    uint8_t minutes = (bcd_minutes >> 4) * 10 + (bcd_minutes & 0x0F);
    uint8_t seconds = (bcd_seconds >> 4) * 10 + (bcd_seconds & 0x0F);
    uint8_t sectors = (bcd_sectors >> 4) * 10 + (bcd_sectors & 0x0F);
    seek_to(Index(minutes, seconds, sectors).total_sectors());
}

bool CD::read_sector_and_advance(uint8_t *buffer) {
    LOGV_CDIMG(std::format("Reading sector into buffer"));

    while (current_index != indexes.end()) {
        auto& file = current_index->file;
        if (file->get_remaining_sectors() > 0) {
            file->read_sector(buffer);

            return true;
        } else {
            LOGV_CDIMG(std::format("No remaining sectors for current index, moving to next one"));
            ++current_index;
        }
    }

    LOGW_CDIMG(std::format("Read from end of disc"));
    return false;
}

bool CD::at_end_of_disc() const {
    return current_index == indexes.end();
}

CD::TrackOnDisc::Mode CD::get_current_track_mode() const {
    return current_index->track.mode;
}

uint32_t CD::get_current_track_number() const {
    return current_index->track.number;
}

uint32_t CD::get_current_index_number() const {
    return current_index->number;
}

CD::Index CD::get_current_position_in_track() const {
    if (current_index != indexes.end()) {
        const auto& index = *current_index;
        return Index(index.position_in_track + index.file->get_read_sectors());
    } else {
        // Get total number of sectors from last index
        const auto& index = indexes.back();
        return Index(index.position_in_track + index.length);
    }
}

CD::Index CD::get_current_position_on_disc() const {
    if (current_index != indexes.end()) {
        const auto& index = *current_index;
        return Index(index.track.position_on_disc + index.position_in_track + index.file->get_read_sectors());
    } else {
        // Get total number of sectors from last index
        const auto& index = indexes.back();
        return Index(index.track.position_on_disc + index.position_in_track + index.length);
    }
}

void CD::seek_to(uint32_t sectors) {
    LOGV_CDIMG(std::format("Seek to {}", Index(sectors)));

    reset_position();
    seek_by(sectors);
}

void CD::seek_by(uint32_t sectors) {
    LOGV_CDIMG(std::format("Seek by {}", Index(sectors)));

    uint32_t remaining_sectors = sectors;
    while (current_index != indexes.end() && remaining_sectors > 0) {
        if (current_index->file->get_remaining_sectors() > remaining_sectors) { // Target sector is in current index
            current_index->file->seek_by(remaining_sectors);

        } else { // Target sector is in upcoming index
            // Seek the current file to the end of the current index
            // This is important if the current index uses the same file
            current_index->file->seek_to_end();
            ++current_index;
        }
    }

    if (current_index == indexes.end() && remaining_sectors > 0) {
        LOGW_CDIMG(std::format("Seek by {:d} sectors past end of disc", sectors));
    }
}

void CD::reset_position() {
    current_index = indexes.begin();

    // Reset all files
    // We depend on this when seeking (new files are at their beginning)
    for (const auto& file : files) {
        file->reset();
    }
}

}

