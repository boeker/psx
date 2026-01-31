#ifndef PSX_GAMEPAD_H
#define PSX_GAMEPAD_H

#include <cstdint>
#include <iostream>
#include <string>

namespace PSX {

class Bus;

class Gamepad {
private:
    Bus *bus;

    // 0x1F801040
    // JOY_RX_DATA/JOY_TX_DATA

    // JOY_STAT
    // 0x1F801044
    uint32_t joyStat;

    // JOY_MODE
    // 0x1F801048
    uint16_t joyMode;

    // JOY_CTRL
    // 0x1F80104A
    uint16_t joyCtrl;

    // JOY_BAUD
    // 0x1F80104E
    uint16_t joyBaud;

    friend std::ostream& operator<<(std::ostream &os, const Gamepad &gamepad);

public:
    Gamepad(Bus *bus);
    void reset();

    template <typename T>
    void write(uint32_t address, T value);

    template <typename T>
    T read(uint32_t address);

private:

    std::string getJoyStatExplanation() const;
    std::string getJoyModeExplanation() const;
    std::string getJoyCtrlExplanation() const;
    std::string getJoyBaudExplanation() const;
};

}

#endif
