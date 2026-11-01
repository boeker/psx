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
    state = IDLE;

    up = false;
    down = false;
    left= false;
    right = false;

    triangle = false;
    cross = false;
    square = false;
    circle = false;

    l1 = false;
    l2 = false;
    l3 = false;
    r1 = false;
    r2 = false;
    r3 = false;

    select = false;
    start = false;
}

void Gamepad::setUp(bool pressed) {
    LOGL_PAD(LogLevel::TRACE, "Up {:s}", pressed ? "pressed" : "released");
    up = pressed;
}

void Gamepad::setDown(bool pressed) {
    LOGL_PAD(LogLevel::TRACE, "Down {:s}", pressed ? "pressed" : "released");
    down = pressed;
}

void Gamepad::setLeft(bool pressed) {
    LOGL_PAD(LogLevel::TRACE, "Left {:s}", pressed ? "pressed" : "released");
    left = pressed;
}

void Gamepad::setRight(bool pressed) {
    LOGL_PAD(LogLevel::TRACE, "Right {:s}", pressed ? "pressed" : "released");
    right = pressed;
}

void Gamepad::setTriangle(bool pressed) {
    LOGL_PAD(LogLevel::TRACE, "Triangle {:s}", pressed ? "pressed" : "released");
    triangle = pressed;
}

void Gamepad::setCross(bool pressed) {
    LOGL_PAD(LogLevel::TRACE, "Cross {:s}", pressed ? "pressed" : "released");
    cross = pressed;
}

void Gamepad::setSquare(bool pressed) {
    LOGL_PAD(LogLevel::TRACE, "Square {:s}", pressed ? "pressed" : "released");
    square = pressed;
}

void Gamepad::setCircle(bool pressed) {
    LOGL_PAD(LogLevel::TRACE, "Circle {:s}", pressed ? "pressed" : "released");
    circle = pressed;
}

void Gamepad::setL1(bool pressed) {
    LOGL_PAD(LogLevel::TRACE, "L1 {:s}", pressed ? "pressed" : "released");
    l1 = pressed;
}

void Gamepad::setL2(bool pressed) {
    LOGL_PAD(LogLevel::TRACE, "L2 {:s}", pressed ? "pressed" : "released");
    l2 = pressed;
}

void Gamepad::setL3(bool pressed) {
    LOGL_PAD(LogLevel::TRACE, "L3 {:s}", pressed ? "pressed" : "released");
    l3 = pressed;
}

void Gamepad::setR1(bool pressed) {
    LOGL_PAD(LogLevel::TRACE, "R1 {:s}", pressed ? "pressed" : "released");
    r1 = pressed;
}

void Gamepad::setR2(bool pressed) {
    LOGL_PAD(LogLevel::TRACE, "R2 {:s}", pressed ? "pressed" : "released");
    r2 = pressed;
}

void Gamepad::setR3(bool pressed) {
    LOGL_PAD(LogLevel::TRACE, "R3 {:s}", pressed ? "pressed" : "released");
    r3 = pressed;
}

void Gamepad::setSelect(bool pressed) {
    LOGL_PAD(LogLevel::TRACE, "Select {:s}", pressed ? "pressed" : "released");
    select = pressed;
}

void Gamepad::setStart(bool pressed) {
    LOGL_PAD(LogLevel::TRACE, "Start {:s}", pressed ? "pressed" : "released");
    start = pressed;
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
                    LOGL_PAD(LogLevel::INFO, "Unexpected message 0x{:02X}", message);
                }
                state = ID_LO_SENT;
                break;
            case ID_LO_SENT:
                if (message == 0x00) { // TAP
                    answer = 0x5A;
                } else {
                    LOGL_PAD(LogLevel::INFO, "Unexpected message 0x{:02X}", message);
                }
                state = ID_HI_SENT;
                break;
            case ID_HI_SENT:
                // message is MOT

                answer = 0xFF;
                Bit::setBit(answer, 0, !select);
                Bit::setBit(answer, 1, !l3);
                Bit::setBit(answer, 2, !r3);
                Bit::setBit(answer, 3, !start);
                Bit::setBit(answer, 4, !up);
                Bit::setBit(answer, 5, !right);
                Bit::setBit(answer, 6, !down);
                Bit::setBit(answer, 7, !left);

                state = SW_LO_SENT;
                break;
            case SW_LO_SENT:
                // message is MOT

                answer = 0xFF;
                Bit::setBit(answer, 0, !l2);
                Bit::setBit(answer, 1, !r2);
                Bit::setBit(answer, 2, !l1);
                Bit::setBit(answer, 3, !r1);
                Bit::setBit(answer, 4, !triangle);
                Bit::setBit(answer, 5, !circle);
                Bit::setBit(answer, 6, !cross);
                Bit::setBit(answer, 7, !square);

                state = IDLE;
                break;
            default:
                break;
        }
    }

    LOGL_PAD(LogLevel::TRACE, "0x{:02X} -> [{:s}] -> [{:s}] -> 0x{:02X}", message, stateToString(oldState), stateToString(state), answer);

    return answer;
}

bool Gamepad::ackForLastByte() {
    return state != IDLE;
}

}

