#ifndef PSX_DMA_H
#define PSX_DMA_H

#include <cstdint>

namespace PSX {

class DMA {
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
