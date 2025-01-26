#ifndef PSX_TIMER_H
#define PSX_TIMER_H

#include <cstdint>

namespace PSX {

class Timers {
private:
    // 0x1F801100: Timer 0 Current Counter Value
    // 0x1F801104: Timer 0 Counter Mode
    // 0x1F801108: Timer 0 Counter Target Value
    uint8_t *timerRegisters;

public:
    void reset();

    template <typename T>
    void writeToIO(uint32_t address, T value);

    template <typename T>
    T readFromIO(uint32_t address);
};

}

#endif
