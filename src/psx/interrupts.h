#ifndef PSX_INTERRUPTS_H
#define PSX_INTERRUPTS_H

#include <cstdint>
#include <iostream>

namespace PSX {

class Interrupts {
private:
    uint8_t interruptStatusRegister[4]; // 0x1F801070
    uint8_t interruptMaskRegister[4]; // 0x1F801074

    friend std::ostream& operator<<(std::ostream &os, const Interrupts &interrupts);

public:
    Interrupts();
    void reset();

    template <typename T>
    void write(uint32_t address, T value);

    template <typename T>
    T read(uint32_t address);
};

}

#endif
