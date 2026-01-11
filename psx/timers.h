#ifndef PSX_TIMERS_H
#define PSX_TIMERS_H

#include <cstdint>

namespace PSX {

class Bus;

class Timers {
private:
    Bus *bus;

    // 0x1F801100: Timer 0 Current Counter Value
    // 0x1F801104: Timer 0 Counter Mode
    // 0x1F801108: Timer 0 Counter Target Value
    //
    // 0x1F801110: Timer 1 Current Counter Value
    // 0x1F801114: Timer 1 Counter Mode
    // 0x1F801118: Timer 1 Counter Target Value
    //
    // 0x1F801120: Timer 2 Current Counter Value
    // 0x1F801124: Timer 2 Counter Mode
    // 0x1F801128: Timer 2 Counter Target Value

    uint32_t current[3];
    uint16_t mode[3];
    uint16_t target[3];

    bool oneShotFired[3];
    uint32_t remainingCycles[3];

    bool horizontalRetrace;
    bool verticalRetrace;

    bool timer1HadVBlank;

public:
    Timers(Bus *bus);
    void reset();

    void catchUpToCPU(uint32_t cpuCycles);
    void notifyAboutDots(uint32_t dots);
    void notifyAboutHBlankStart();
    void notifyAboutHBlankEnd();
    void notifyAboutVBlankStart();
    void notifyAboutVBlankEnd();

    void updateTimer0(uint32_t increase);
    void updateTimer1(uint32_t increase);
    void updateTimer2(uint32_t increase);

    template <typename T>
    void write(uint32_t address, T value);

    template <typename T>
    T read(uint32_t address);
};

}

#endif
