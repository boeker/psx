#ifndef PSX_GAMEPAD_H
#define PSX_GAMEPAD_H

#include <atomic>

namespace PSX {

class Gamepad {
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

private:
    std::atomic<bool> up;
    std::atomic<bool> down;
    std::atomic<bool> left;
    std::atomic<bool> right;

};
}

#endif

