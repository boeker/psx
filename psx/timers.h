#ifndef PSX_TIMERS_H
#define PSX_TIMERS_H

#include <cstdint>
#include <iostream>
#include <string>

namespace PSX {

// Timer reached 0xFFFF (0 = no, 1 = yes), reset on read
#define TIMER_MODE_REACHED_FFFF_VALUE 12
// Timer reached target value (0 = no, 1 = yes), reset on read
#define TIMER_MODE_REACHED_TARGET_VALUE 11
// Interrupt request (0 = yes, 1 = no), set after writing 1
// Issues interrupt when going from 1 to 0
#define TIMER_MODE_INTERRUPT_REQUEST 10
// Clock source: 
// Counter 0: 0 or 2 = system clock, 1 or 3 = dotclock
// Counter 1: 0 or 2 = system clock, 1 or 3 = hblank
// Counter 2: 0 or 1 = system clock, 2 or 3 = system clock / 8
#define TIMER_MODE_CLOCK_SOURCE1 9
#define TIMER_MODE_CLOCK_SOURCE0 8
// Pulse or toggle mode for INTERRUPT_REQUEST bit (0 = short pulse to 0, 1 = toggle)
#define TIMER_MODE_IRQ_PULSE_TOGGLE_MODE 7
// Once or repeat mode for INTERRUPT_REQUEST (0 = once, 1 = repeatedly)
#define TIMER_MODE_IRQ_ONCE_REPEAT_MODE 6
// INTERRUPT_REQUEST when counter reaches 0xFFFF (0 = disabled, 1 = enabled)
#define TIMER_MODE_IRQ_WHEN_COUNTER_IS_FFFF 5
// INTERRUPT_REQUEST when counter reaches target value (0 = disabled, 1 = enabled)
#define TIMER_MODE_IRQ_WHEN_COUNTER_IS_TARGET 4
// When to reset counter (0 = after 0xFFFF, 1 after target value)
#define TIMER_MODE_RESET_COUNTER_TO_0000 3
// Synchronization mode
// Timer 0: 0 = pause during hblank, 1 = reset to 0 at hblank, 2 = reset to 0 at hblank
//          and pause outside of hblank, 3 = pause until hblank, then free run
// Timer 1: as for timer 0 but with vblank instead
// Timer 2: 0 or 3 = pause, 1 or 2 = free run
#define TIMER_MODE_SYNCHRONIZATION_MODE1 2
#define TIMER_MODE_SYNCHRONIZATION_MODE0 1
// Synchronization enabled (0 = free run, 1 = use SYNCHRONIZATION_MODE)
#define TIMER_MODE_SYNCHRONIZATION_ENABLE 0

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

    // Timer registers
    uint32_t current[3];
    uint16_t mode[3];
    uint16_t target[3];

    // Internal state
    // Timer 0, 1, and 3
    uint32_t remainingCycles[3];
    bool resetPulse[3];
    bool oneShotFired[3];
    // Timer 0 and 1
    bool startConditionMet[2];
    // General
    bool horizontalRetrace;
    bool verticalRetrace;

    friend std::ostream& operator<<(std::ostream &os, const Timers &timers);

    std::string getModeExplanation(uint32_t md) const;

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

    template<uint32_t N>
    void checkResetPulse();

    template<uint32_t N>
    void increaseTimer0Or1(uint32_t increase);

    template<uint32_t N>
    void checkResetValue();

    template<uint32_t N>
    void handleInterrupt();

    template <typename T>
    void write(uint32_t address, T value);

    template <typename T>
    T read(uint32_t address);
};

}

#endif
