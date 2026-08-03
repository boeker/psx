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
    os << "Status Register: " << MacroblockDecoder::get_status_register_explanation(mdec.get_status_register());

    return os;
}

uint32_t MacroblockDecoder::get_status_register() const {
    uint32_t reg = static_cast<uint32_t>(remaining_parameter_words);

    Bit::setBit(reg, MDEC_STATUS_DATA_OUT_QUEUE_EMPTY, data_out_queue.empty());
    Bit::setBit(reg, MDEC_STATUS_DATA_IN_QUEUE_FULL, received_all_parameters);
    Bit::setBit(reg, MDEC_STATUS_CMD_BUSY, state != State::IDLE);
    Bit::setBit(reg, MDEC_STATUS_DATA_IN_REQ, data_in_enabled); // TODO: Check if DMA0 enabled?
    Bit::setBit(reg, MDEC_STATUS_DATA_OUT_REQ, data_out_enabled && false); // TODO: Check if DMA1 enabled? Check if something to send?
    // TODO output depth
    // TODO output signed
    // TODO output bit15
    Bit::setBits<3>(reg, MDEC_STATUS_CURRENT_BLOCK0, current_block);

    return reg;
}

std::string MacroblockDecoder::get_status_register_explanation(uint32_t reg) {
    std::stringstream ss;

    ss << std::format("DATA_OUT_QUEUE_EMPTY[{:01b}], ", (reg >> MDEC_STATUS_DATA_OUT_QUEUE_EMPTY) & 1);
    ss << std::format("DATA_IN_QUEUE_FULL[{:01b}], ", (reg >> MDEC_STATUS_DATA_IN_QUEUE_FULL) & 1);
    ss << std::format("CMD_BUSY[{:01b}], ", (reg >> MDEC_STATUS_CMD_BUSY) & 1);
    ss << std::format("DATA_IN_REQ[{:01b}], ", (reg >> MDEC_STATUS_DATA_IN_REQ) & 1);
    ss << std::format("DATA_OUT_REQ[{:01b}], ", (reg >> MDEC_STATUS_DATA_OUT_REQ) & 1);
    ss << std::format("DATA_OUTPUT_DEPTH[{:d}], ", (reg >> MDEC_STATUS_DATA_OUTPUT_DEPTH0) & 3);
    ss << std::format("DATA_OUTPUT_SIGNED[{:01b}], ", (reg >> MDEC_STATUS_DATA_OUTPUT_SIGNED) & 1);
    ss << std::format("DATA_OUTPUT_BIT15[{:01b}], ", (reg >> MDEC_STATUS_DATA_OUTPUT_BIT15) & 1);
    ss << std::format("CURRENT_BLOCK[{:d}], ", (reg >> MDEC_STATUS_CURRENT_BLOCK0) & 7);
    ss << std::format("PARAMETER_WORDS_REMAINING[{:04X}] ", (reg >> MDEC_STATUS_PARAMETER_WORDS_REMAINING0) & 0xFFFF);

    return ss.str();
}

MacroblockDecoder::MacroblockDecoder(Bus *bus) {
    this->bus = bus;

    reset();
}

void MacroblockDecoder::reset() {
    state = State::IDLE;
    data_out_queue.clear();
    data_in_queue.clear();
    received_all_parameters = false;
    remaining_parameter_words = 0;

    data_in_enabled = false;
    data_out_enabled = false;

    current_block = 4; // Default is 4 = Y
}

template <>
void MacroblockDecoder::write(uint32_t address, uint32_t value) {
    assert(address == 0x1F80'1820 || address == 0x1F80'1824);

    LOGT_MDEC(std::format("Write to MDEC: 0x{:08X} --> @0x{:08X}", value, address));

    if (address == 0x1F80'1820) { // Command/Parameter Register
        LOGW_MDEC("Unimplemented write to Command Register");
    } else { // 0x1F80'1824: Control/Reset Register
        LOGT_MDEC("Write to Control Register");
        if (Bit::getBit(value, MDEC_CONTROL_RESET)) {
            reset();
            LOGV_MDEC("Reset MDEC");
            assert(get_status_register() == 0x8004'0000);
        }
        data_in_enabled = Bit::getBit(value, MDEC_CONTROL_ENABLE_DATA_IN_REQ);
        if (data_in_enabled) {
            LOGV_MDEC("Enable data in");
        } else {
            LOGV_MDEC("Disable data in");
        }
        data_out_enabled = Bit::getBit(value, MDEC_CONTROL_ENABLE_DATA_OUT_REQ);
        if (data_out_enabled) {
            LOGV_MDEC("Enable data out");
        } else {
            LOGV_MDEC("Disable data out");
        }

        LOGV_MDEC(std::format("Status Register: {:s}", get_status_register_explanation(get_status_register())));
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

    LOGT_MDEC(std::format("Read from MDEC: @0x{:08X}", address));

    uint32_t value = 0;
    if (address == 0x1F80'1820) { // Command/Parameter Register
        LOGW_MDEC("Unimplemented read from Data Register");
    } else { // 0x1F80'1824: Control/Reset Register
        value = get_status_register();
        LOGV_MDEC(std::format("Read from Status Register: {:s}", get_status_register_explanation(value)));
    }

    LOGT_MDEC(std::format("--> 0x{:08X}", value));

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

