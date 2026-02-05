#include "gamepad.h"

#include <format>

#include "util/bit.h"
#include "util/log.h"

using namespace util;

namespace PSX {

std::string Gamepad::stateToString(State state) {
    switch (state) {
        case IDLE:
            return "IDLE";
        case ACTIVE:
            return "ACTIVE";
        case ID_LO_SENT:
            return "ID_LO_SENT";
        case ID_HI_SENT:
            return "ID_HI_SENT";
        case SW_LO_SENT:
            return "SW_LO_SENT";
    }

    return "INVALID";
}

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
    State oldState = state;
    uint8_t answer = 0;

    if ((state == IDLE || state == ACTIVE) && message == 0x01) {
        state = ACTIVE;
        answer = 0x00;

    } else {
        switch (state) {
            case ACTIVE:
                if (message == 0x42) {
                    answer = 0x41;
                } else {
                    LOG_PAD(std::format("Unexpected message 0x{:02X}", message));
                }
                state = ID_LO_SENT;
                break;
            case ID_LO_SENT:
                if (message == 0x00) { // TAP
                    answer = 0x5A;
                } else {
                    LOG_PAD(std::format("Unexpected message 0x{:02X}", message));
                }
                state = ID_HI_SENT;
                break;
            case ID_HI_SENT:
                // message is MOT

                // TODO implement other buttons
                answer = 0xFF;
                Bit::setBit(answer, 4, !up);
                Bit::setBit(answer, 5, !right);
                Bit::setBit(answer, 6, !down);
                Bit::setBit(answer, 7, !left);

                state = SW_LO_SENT;
                break;
            case SW_LO_SENT:
                // message is MOT
                answer = 0xFF; // TODO implement other buttons
                state = IDLE;
                break;
            default:
                break;
        }
    }

    LOGT_PAD(std::format("0x{:02X} -> [{:s}] -> [{:s}] -> 0x{:02X}", message, stateToString(oldState), stateToString(state), answer));

    return answer;
}

bool Gamepad::ackForLastByte() {
    return state != IDLE;
}

}

