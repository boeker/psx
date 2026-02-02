#include "gamepad.h"

#include <format>

#include "util/bit.h"
#include "util/log.h"

using namespace util;

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

uint8_t Gamepad::send(uint8_t message) {
    uint8_t answer = 0;

    LOGT_PAD(std::format("Receiving 0x{:02X}", message));

    switch (state) {
        case IDLE:
            LOGT_PAD("[IDLE]");
            if (message == 0x01) {
                LOGT_PAD("[IDLE] -> [ACTIVE]");
                answer = 0x00;
            } else {
                LOG_PAD(std::format("Unexpected message 0x{:02X}", message));
            }
            state = ID_LO_SENT;
            break;
        case ACTIVE:
            LOGT_PAD("[ACTIVE]");
            if (message == 0x42) {
                LOGT_PAD("[ACTIVE] -> [ID_LO_SENT]");
                answer = 0x41;
            } else {
                LOG_PAD(std::format("Unexpected message 0x{:02X}", message));
            }
            state = ID_LO_SENT;
            break;
        case ID_LO_SENT:
            LOGT_PAD("[ID_LO_SENT]");
            if (message == 0x00) { // TAP
                LOGT_PAD("[ID_LO_SENT] -> [ID_HI_SENT]");
                answer = 0x5A;
            } else {
                LOG_PAD(std::format("Unexpected message 0x{:02X}", message));
            }
            state = ID_HI_SENT;
            break;
        case ID_HI_SENT:
            LOGT_PAD("[ID_HI_SENT]");
            // message is MOT
            LOGT_PAD("[ID_HI_SENT] -> [SW_LO_SENT]");

            // TODO implement other buttons
            answer = 0x41;
            Bit::setBit(answer, 4, up);
            Bit::setBit(answer, 5, right);
            Bit::setBit(answer, 6, down);
            Bit::setBit(answer, 7, left);

            state = SW_LO_SENT;
            break;
        case SW_LO_SENT:
            LOGT_PAD("[SW_LO_SENT]");
            // message is MOT
            LOGT_PAD("[SW_LO_SENT] -> [IDLE]");
            answer = 0x00; // TODO implement other buttons
            state = IDLE;
            break;
    }

    LOGT_PAD(std::format("Answering with 0x{:02X}", answer));

    return answer;
}

}

