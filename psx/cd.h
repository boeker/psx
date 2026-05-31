#ifndef PSX_CD_H
#define PSX_CD_H

#include <cstdint>
#include <fstream>
#include <string>

#include "util/cue.h"

namespace PSX {

#define CD_MODE2_SYNC_BYTES 0xC
#define CD_MODE2_HEADER 0x4
#define CD_MODE2_SUB_HEADER 0x4
#define CD_MODE2_SUB_HEADER_COPY 0x4

#define CD_MODE2_HEADER_OFFSET 0xC
#define CD_MODE2_DATA_OFFSET 0x18

class CD {
private:
    using Index = util::cue::Index;
    using NumberedIndex = util::cue::NumberedIndex;
    using Track = util::cue::Track;
    struct File {
        uint32_t sectors;
        std::ifstream stream;
        // Type implicitly is BINARY
        std::vector<Track> tracks;

        // Number of sectors from this file that have already been read
        uint32_t read_sectors();
        // Number of remaining sectors, including the one we currently are at
        // Zero means that the whole file has been read
        uint32_t remaining_sectors();
        // Number of read sectors + two minutes
        uint32_t current_sector();
    };

    std::vector<File> files;
    std::vector<File>::iterator current_file;
    std::vector<Track>::iterator current_track;
    std::vector<NumberedIndex>::iterator current_index;
    uint32_t current_sector;

public:
    static const uint32_t SECTOR_SIZE = 2352; // 0x930

    CD(const std::string &filename);
    void reset();

    void open_cue_sheet(const std::string &filename);
    void seek_to_bcd(uint8_t bcd_minutes, uint8_t bcd_seconds, uint8_t bcd_sectors);
    bool read_sector_and_advance(uint8_t *buffer);

    Index get_current_position();
    bool at_end_of_disc() const;

private:
    void seek_to(uint32_t sectors);
    void seek_by(uint32_t sectors);
    void reset_position();
    void move_to_next_file();
    void move_to_track_and_index();
};

}

#endif

