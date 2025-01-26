#ifndef PSX_GPU_H
#define PSX_GPU_H

#include <cstdint>

namespace PSX {

class GPU {
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
