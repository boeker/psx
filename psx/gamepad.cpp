#include "gamepad.h"

#include <format>

#include "util/log.h"

namespace PSX {

Gamepad::Gamepad() {
}

void Gamepad::reset() {
    this->up = false;
    this->down = false;
    this->left= false;
    this->right = false;
}

void Gamepad::setUp(bool pressed) {
    LOGT_PAD(std::format("Up {:s}", pressed ? "pressed" : "released"));
    up = pressed;
}

void Gamepad::setDown(bool pressed) {
    LOGT_PAD(std::format("Down {:s}", pressed ? "pressed" : "released"));
    down = pressed;
}

void Gamepad::setLeft(bool pressed) {
    LOGT_PAD(std::format("Left {:s}", pressed ? "pressed" : "released"));
    left = pressed;
}

void Gamepad::setRight(bool pressed) {
    LOGT_PAD(std::format("Right {:s}", pressed ? "pressed" : "released"));
    right = pressed;
}

bool Gamepad::getUp() const {
    return up;
}

bool Gamepad::getDown() const {
    return down;
}

bool Gamepad::getLeft() const {
    return left;
}

bool Gamepad::getRight() const {
    return right;
}

}

