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
    Bit::setBit(reg, MDEC_STATUS_DATA_IN_REQ, data_in_request());
    Bit::setBit(reg, MDEC_STATUS_DATA_OUT_REQ, data_out_request());
    Bit::setBits<2>(reg, MDEC_STATUS_DATA_OUTPUT_DEPTH0, data_output_depth);
    Bit::setBit(reg, MDEC_STATUS_DATA_OUTPUT_SIGNED, data_output_signed);
    Bit::setBit(reg, MDEC_STATUS_DATA_OUTPUT_BIT15, data_output_bit15);
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
    ss << std::format("PARAMETER_WORDS_REMAINING[{:04X}]", (reg >> MDEC_STATUS_PARAMETER_WORDS_REMAINING0) & 0xFFFF);

    return ss.str();
}

void MacroblockDecoder::extract_data_output_bits(uint32_t command) {
    data_output_depth = Bit::getBits<2>(command, MDEC_CMD_DATA_OUTPUT_DEPTH0);
    data_output_signed = Bit::getBit(command, MDEC_CMD_DATA_OUTPUT_SIGNED);
    data_output_bit15 = Bit::getBit(command, MDEC_CMD_DATA_OUTPUT_BIT15);

    LOGT_MDEC(std::format("Data Output Depth: {:d}", data_output_depth));
    LOGT_MDEC(std::format("Data Output Signed: {:s}", data_output_signed));
    LOGT_MDEC(std::format("Data Output Bit 15: {:s}", data_output_bit15));
}

void MacroblockDecoder::decode_macroblock(uint32_t command) {
    LOGT_MDEC("decode_macroblock");
    state = State::CMD_DECODE_MACROBLOCK;
    extract_data_output_bits(command);
    remaining_parameter_words = static_cast<uint16_t>(command & 0x0000'FFFF) - 1;
    LOGT_MDEC(std::format("Remaining parameter words (one was subtracted): 0x{:04X}", remaining_parameter_words));
}

void MacroblockDecoder::set_iqtab(uint32_t command) {
    LOGT_MDEC("set_iqtab");
    state = State::CMD_SET_IQTAB;
    extract_data_output_bits(command);
    if (!Bit::getBit(command, MDEC_CMD_COLOR)) {
        LOGT_MDEC(std::format("Luminance only, setting remaining parameter words to 64/4 - 1 = 15"));
        remaining_parameter_words = 15;
    } else {
        LOGT_MDEC(std::format("Luminance and color, setting remaining parameter words to 128/4 - 1 = 31"));
        remaining_parameter_words = 31;
    }
}

void MacroblockDecoder::set_scale(uint32_t command) {
    LOGT_MDEC("set_scale");
    state = State::CMD_SET_SCALE;
    extract_data_output_bits(command);
    LOGT_MDEC(std::format("Setting remaining parameter words to 64/2 - 1 = 31"));
    remaining_parameter_words = 31;
}

void MacroblockDecoder::no_function(uint32_t command) {
    LOGT_MDEC("no_function");
    state = State::IDLE;
    extract_data_output_bits(command);
    remaining_parameter_words = static_cast<uint16_t>(command & 0x0000'FFFF);
    LOGT_MDEC(std::format("Remaining parameter words: 0x{:04X}", remaining_parameter_words));
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

    data_output_depth = 0;
    data_output_signed = false;
    data_output_bit15 = false;

    current_block = 4; // Default is 4 = Y
}

bool MacroblockDecoder::data_in_request() const {
    return data_in_enabled && state != State::IDLE;
}

bool MacroblockDecoder::data_out_request() const {
    return data_out_enabled && false; // TODO Replace false by "have something to send"
}

void MacroblockDecoder::process(uint32_t value) {
    if (state == State::IDLE) { // Has to be a command
        uint8_t command_num = value >> MDEC_CMD_CMD0;
        LOGT_MDEC(std::format("Executing command #{:d}", command_num));

        switch (command_num) {
            case 1:
                decode_macroblock(value);
                break;
            case 2:
                set_iqtab(value);
                break;
            case 3:
                set_scale(value);
                break;
            default:
                no_function(value);
                break;
        }

    } else {
        switch (state) {
            case State::CMD_DECODE_MACROBLOCK:
                LOGT_MDEC(std::format("Received macroblock value 0x{:08X}", value));
                // TODO: Store and process
                break;
            case State::CMD_SET_IQTAB:
                LOGT_MDEC(std::format("Received iqtab value 0x{:08X}", value));
                // TODO: Store
                break;
            case State::CMD_SET_SCALE:
                LOGT_MDEC(std::format("Received scale value 0x{:08X}", value));
                // TODO: Store
                break;
            case State::IDLE: // Not reachable
                break;
        }

        --remaining_parameter_words;
        if (remaining_parameter_words == 0xFFFF) { // The stored value is minus one
            LOGT_MDEC(std::format("Received last parameter word"));
            state = State::IDLE;
            received_all_parameters = true;
        }
    }
}

template <>
void MacroblockDecoder::write(uint32_t address, uint32_t value) {
    assert(address == 0x1F80'1820 || address == 0x1F80'1824);

    LOGT_MDEC(std::format("Write to MDEC: 0x{:08X} --> @0x{:08X}", value, address));

    if (address == 0x1F80'1820) { // Command/Parameter Register
        LOGT_MDEC(std::format("Write to Command Register: 0x{:08X}", value));
        process(value);

    } else { // 0x1F80'1824: Control/Reset Register
        LOGT_MDEC(std::format("Write to Control Register: 0x{:08X}", value));
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

