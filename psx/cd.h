#ifndef PSX_CD_H
#define PSX_CD_H

#include <cstdint>
#include <fstream>
#include <string>

namespace PSX {

class CD {
private:
    std::ifstream image;

    static const uint32_t SECTOR_SIZE = 2352;

public:
    CD(const std::string &filename);

    void seekTo(uint8_t minutes, uint8_t seconds, uint8_t sectors);
    uint8_t readByte();

    uint32_t getSectorSize() const;
};

}

#endif

