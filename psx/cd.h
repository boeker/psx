#ifndef PSX_CD_H
#define PSX_CD_H

#include <cstdint>
#include <fstream>
#include <string>

#define CD_MODE2_SYNC_BYTES 0xC
#define CD_MODE2_HEADER 0x4
#define CD_MODE2_SUB_HEADER 0x4
#define CD_MODE2_SUB_HEADER_COPY 0x4

#define CD_MODE2_HEADER_OFFSET 0xC
#define CD_MODE2_DATA_OFFSET 0x18

namespace PSX {

class CD {
private:
    static const uint32_t SECTOR_SIZE = 2352; // 0x930

    std::ifstream image;
    bool read_whole_sector;
    uint8_t minutes;
    uint8_t seconds;
    uint8_t sectors;
    uint32_t offset_in_sector;

public:
    CD(const std::string &filename);
    void reset();

    void seekTo(uint8_t minutes, uint8_t seconds, uint8_t sectors);
    uint8_t readByte();
    uint32_t readWord();

    void set_read_whole_sector(bool read_whole_sector);
    uint32_t get_remaining_bytes_in_sector() const;

private:
    void seek_to_next_sector();
    void seek_in_file();
};

}

#endif

