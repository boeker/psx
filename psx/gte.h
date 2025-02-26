#ifndef PSX_GTE_H
#define PSX_GTE_H

#include <cstdint>
#include <iostream>

namespace PSX {

class GTE {
private:
    uint32_t registers[64];

    friend std::ostream& operator<<(std::ostream &os, const GTE &gte);

public:
    GTE();
    void reset();

    uint32_t getRegister(uint8_t reg);
    void setRegister(uint8_t reg, uint32_t value);
    uint32_t getControlRegister(uint8_t reg);
    void setControlRegister(uint8_t reg, uint32_t value);
};

}

#endif
