#ifndef PSX_CD_H
#define PSX_CD_H

#include <cstdint>
#include <memory>
#include <fstream>
#include <span>
#include <string>
#include <tuple>

#include "util/cue.h"

#define CD_MODE2_SYNC_BYTES 0xC
#define CD_MODE2_HEADER 0x4
#define CD_MODE2_SUB_HEADER 0x4
#define CD_MODE2_SUB_HEADER_COPY 0x4

#define CD_MODE2_HEADER_OFFSET 0xC
#define CD_MODE2_DATA_OFFSET 0x18

namespace PSX {

class CD {
private:
    using Index = util::cue::Index;
    using NumberedIndex = util::cue::NumberedIndex;
    using Track = util::cue::Track;
    struct File {
        std::ifstream stream;
        // Type implicitly is BINARY
        std::vector<Track> tracks;
    };

    std::vector<File> files;
    std::vector<File>::iterator current_file;
    std::vector<Track>::iterator current_track;
    std::vector<NumberedIndex>::iterator current_index;
    Index current_position;
    Index current_position_in_file;

    static const uint32_t SECTOR_SIZE = 2352; // 0x930
    uint8_t sector_buffer[SECTOR_SIZE];

    // Legacy
    bool read_whole_sector;
    uint8_t minutes;
    uint8_t seconds;
    uint8_t sectors;
    uint32_t offset_in_sector;

public:
    CD(const std::string &filename);
    void open_cue_sheet(const std::string &filename);
    void reset();
    void reset_position();

    void seek_to_bcd(uint8_t bcd_minutes, uint8_t bcd_seconds, uint8_t bcd_sectors);
    void seek_to_dec(uint8_t minutes, uint8_t seconds, uint8_t sectors);
    void seek_to_next_sector();
    bool at_end_of_disc() const;
    bool read_sector_into_buffer();
    std::span<uint8_t> get_sector_buffer();

private:
    void seek_to(uint8_t minutes, uint8_t seconds, uint8_t sectors);
    void seek_by(uint8_t minutes, uint8_t seconds, uint8_t sectors);

public:
    // Legacy TODO: Remove
    void set_read_whole_sector(bool read_whole_sector);
    uint32_t get_remaining_bytes_in_sector() const;

    void seekTo(uint8_t minutes, uint8_t seconds, uint8_t sectors);
    uint8_t readByte();
    uint32_t readWord();

private:
    void leg_seek_to_next_sector();
    void seek_in_file();
};

}

#endif

