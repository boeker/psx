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

    LOG_GTE_VERB(std::format("{{gte r{:d} -> 0x{:08X}}}", rt, word));

    return word;
}

void GTE::setRegister(uint8_t rt, uint32_t value) {
    assert (rt < 32);
    LOG_GTE_VERB(std::format(" {{0x{:08X} -> gte r{:d}}}", value, rt));

    this->registers[rt] = value;
}

uint32_t GTE::getControlRegister(uint8_t rt) {
    assert (rt < 32);
    uint32_t word = this->registers[32 + rt];

    LOG_GTE_VERB(std::format("{{gte c{:d} -> 0x{:08X}}}", rt, word));

    return word;
}

void GTE::setControlRegister(uint8_t rt, uint32_t value) {
    assert (rt < 32);
    LOG_GTE_VERB(std::format(" {{0x{:08X} -> gte c{:d}}}", value, rt));

    this->registers[32 + rt] = value;
}

}
