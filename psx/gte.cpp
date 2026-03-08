#include "gte.h"

#include <cassert>
#include <format>
#include <sstream>

#include "util/log.h"

using namespace util;

namespace PSX {

std::ostream& operator<<(std::ostream &os, const GTE &gte) {
    return os;
}

const char* GTE::REGISTER_NAMES[] = {
    "vxy0", "vz0", // 3xS16: Vector 0 (X,Y,Z)
    "vxy1", "vz1", // 3xS16: Vector 1 (X,Y,Z)
    "vxy2", "vz2", // 3xS16: Vector 2 (X,Y,Z)
    "rgbc", // 4xU8: Color/code value
    "otz", // 1xS16: Average Z value
    "ir0", // 1xS16: 16bit accumulator (interpolate)
    "ir1", "ir2", "ir3", // 3xS16: 16bit accumulator (vector)
    "sxy0", "sxy1", "sxy2", "sxyp", // 6xS16: Screen XY-coordinate queue (3 items), what does sxyp do?
    "sz0", "sz1", "sz2", "sz3", // 4xU16: Screen Z-coordinate queue (4 items)
    "rgb0", "rgb1", "rgb2", // 12xU8: Color CRGB-code/color queue (3 items)
    "res1", // 4xU8: Prohibited
    "mac0", // 1xS32: 32bit math accumulator (value)
    "mac1", "mac2", "mac3", // 3xS32: 32bit math accumulator (vector)
    "irgb", "orgb", // 1xU15: convert RGB color (48bit vs 15bit?)
    "lzcs", "lzcr", // 2xS32: Count leading zeros/ones (sign bits)
    "rt11rt12", "rt13rt21", "rt22rt23", "rt31rt32", "rt33", // 9xS16: Rotation matrix (3x3)
    "trx", "try", "trz", // 3x32: Translaction vector (X,Y,Z)
    "l11l12", "l13l21", "l22l23", "l31l32", "l33", // 9xS16: Light source matrix (3x3)
    "rbk", "gbk", "bbk", // 3x32: Background color (R,G,B)
    "lr1lr2", "lr3lg1", "lg2lg3", "lb1lb2", "lb3", // 9xS16: Light color matrix source (3x3)
    "rfc", "gfc", "bfc", // 3x32: Far color (R,G,B)
    "ofx", "ofy", // 2x32: Screen offset (X,Y)
    "h", // BuggyU16: Projection plane distance
    "dqa", // S16: Depth queueing parameter A (coeff)
    "dqb", // 32: Depth queueing parameter B (offset)
    "zsf3", "zsf4", // 2xS16: Average Z scale factors
    "flag" // U20: Calculation errors
};

std::string GTE::getRegisterName(uint8_t reg) {
    return REGISTER_NAMES[reg];
}

std::string GTE::getControlRegisterName(uint8_t reg) {
    return REGISTER_NAMES[32 + reg];
}


GTE::GTE() {
    reset();
}

void GTE::reset() {
    for (int i = 0; i < 64; ++i) {
        this->registers[i] = 0;
    }
}

uint32_t GTE::getRegister(uint8_t rt) {
    assert (rt < 32);
    uint32_t word = this->registers[rt];

    LOGT_GTE(std::format("{{gte r{:d} -> 0x{:08X}}}", rt, word));

    return word;
}

void GTE::setRegister(uint8_t rt, uint32_t value) {
    assert (rt < 32);
    LOGT_GTE(std::format(" {{0x{:08X} -> gte r{:d}}}", value, rt));

    this->registers[rt] = value;
}

uint32_t GTE::getControlRegister(uint8_t rt) {
    assert (rt < 32);
    uint32_t word = this->registers[32 + rt];

    LOGT_GTE(std::format("{{gte c{:d} -> 0x{:08X}}}", rt, word));

    return word;
}

void GTE::setControlRegister(uint8_t rt, uint32_t value) {
    assert (rt < 32);
    LOGT_GTE(std::format(" {{0x{:08X} -> gte c{:d}}}", value, rt));

    this->registers[32 + rt] = value;
}

}
