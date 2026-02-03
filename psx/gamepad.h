#ifndef PSX_GAMEPAD_H
#define PSX_GAMEPAD_H

#include <atomic>

namespace PSX {

class Gamepad {
private:
    std::atomic<bool> up;
    std::atomic<bool> down;
    std::atomic<bool> left;
    std::atomic<bool> right;

    enum State {
        IDLE,
        ACTIVE,
        ID_LO_SENT,
        ID_HI_SENT,
        SW_LO_SENT
        //SW_HI_SENT,
        //ADC0_SENT,
        //ADC1_SENT,
        //ADC2_SENT,
        //ADC3_SENT
    };

    State state;
public:
    Gamepad();

    void reset();

    void setUp(bool pressed);
    void setDown(bool pressed);
    void setLeft(bool pressed);
    void setRight(bool pressed);

    bool getUp() const;
    bool getDown() const;
    bool getLeft() const;
    bool getRight() const;

    uint8_t send(uint8_t message);
    bool ackForLastByte();
};
}

#endif

