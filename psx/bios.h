#ifndef PSX_BIOS_H
#define PSX_BIOS_H

#include <cstdint>
#include <string>

#include "registers.h"

#define BIOS_SIZE (512 * 1024)

namespace PSX {

class Bios {
private:
    uint8_t* bios;

public:
    Bios();
    virtual ~Bios();
    void reset();

    void readFromFile(const std::string &file);

    template <typename T>
    T read(uint32_t address);
};
}

#endif
