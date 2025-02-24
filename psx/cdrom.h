#ifndef PSX_CDROM_H
#define PSX_CDROM_H

#include <cstdint>

namespace PSX {

class CDROM {
private:

public:
    void reset();

    template <typename T>
    void write(uint32_t address, T value);

    template <typename T>
    T read(uint32_t address);
};

}

#endif
