#include "mdec.h"

#include <cassert>
#include <format>
#include <sstream>

#include "bus.h"
#include "exceptions/exceptions.h"
#include "util/bit.h"
#include "util/log.h"

using namespace util;

namespace PSX {

std::ostream& operator<<(std::ostream &os, const MacroblockDecoder &mdec) {
    return os;
}

std::string MacroblockDecoder::get_status_explanation() const {
    std::stringstream ss;

    return ss.str();
}

MacroblockDecoder::MacroblockDecoder(Bus *bus) {
    this->bus = bus;

    reset();
}

void MacroblockDecoder::reset() {
}

template <>
void MacroblockDecoder::write(uint32_t address, uint32_t value) {
    assert(address == 0x1F80'1820 || address == 0x1F80'1824);

    LOGT_MDEC(std::format("Write to MDEC: 0x{:08X} --> @0x{:08X}", value, address));

    if (address == 0x1F80'1820) { // Command/Parameter Register
        LOGW_MDEC("Unimplemented write to Command Register");
    } else { // 0x1F80'1824: Control/Reset Register
        LOGW_MDEC("Unimplemented write to Control Register");
    }
}

template <>
void MacroblockDecoder::write(uint32_t address, uint16_t value) {
    throw exceptions::UnimplementedAddressingError(std::format("halfword write to MDEC @0x{:08X}", address));
}

template <>
void MacroblockDecoder::write(uint32_t address, uint8_t value) {
    throw exceptions::UnimplementedAddressingError(std::format("byte write to MDEC @0x{:08X}", address));
}

template <>
uint32_t MacroblockDecoder::read(uint32_t address) {
    assert(address == 0x1F80'1820 || address == 0x1F80'1824);

    uint32_t value = 0;
    if (address == 0x1F80'1820) { // Command/Parameter Register
        LOGW_MDEC("Unimplemented read from Data Register");
    } else { // 0x1F80'1824: Control/Reset Register
        LOGW_MDEC("Unimplemented read from Status Register");
    }

    LOGT_MDEC(std::format("Read from MDEC: @0x{:08X} --> 0x{:08X}", address, value));

    return value;
}

template <>
uint16_t MacroblockDecoder::read(uint32_t address) {
    throw exceptions::UnimplementedAddressingError(std::format("halfword read from MDEC @0x{:08X}", address));
}

template <>
uint8_t MacroblockDecoder::read(uint32_t address) {
    throw exceptions::UnimplementedAddressingError(std::format("byte read from MDEC @0x{:08X}", address));
}

}

