#ifndef PSX_SPU_H
#define PSX_SPU_H

#include <cstdint>

namespace PSX {

class SPU {
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
