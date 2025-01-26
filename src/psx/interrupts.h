#ifndef PSX_INTERRUPTS_H
#define PSX_INTERRUPTS_H

#include <cstdint>

namespace PSX {

class Interrupts {
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
