#include "mdec.h"

#include <cassert>
#include <format>
#include <sstream>
#include <string>

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

    Bit::setBit(reg, MDEC_STATUS_DATA_OUT_QUEUE_EMPTY, data_output_queue.empty());
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
    luminance_quantization_table.clear();
    color_quantization_table.clear();
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
    scale_table.clear();
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

template<std::input_iterator ITER, std::sentinel_for<ITER> SENT>
void MacroblockDecoder::trace_values_as_table(ITER begin, SENT end) {
    std::stringstream ss;
    auto it = begin;
    while (it != end) {
        for (uint32_t i = 0; i < 8 && it != end; ++i, ++it) {
            ss << std::format("0x{:0{}X}", *it, 2 * sizeof(std::iter_value_t<ITER>)) << ((i < 7) ? ", " : ",");
        }
        LOGT_MDEC(ss.str());
        ss.str(std::string());
    }
}

void MacroblockDecoder::decode_collected_blocks() {
    LOGT_MDEC("Decoding collected blocks");

    // Dump encoding information
    LOGT_MDEC(std::format("Data Output Depth: {:d}", data_output_depth));
    LOGT_MDEC(std::format("Data Output Signed: {:s}", data_output_signed));
    LOGT_MDEC(std::format("Data Output Bit 15: {:s}", data_output_bit15));

    // Dump tables
    LOGT_MDEC(std::format("Luminance Quantization Table ({:d} bytes):", luminance_quantization_table.size()));
    trace_values_as_table(luminance_quantization_table.cbegin(), luminance_quantization_table.cend());
    LOGT_MDEC(std::format("Color Quantization Table ({:d} bytes):", color_quantization_table.size()));
    trace_values_as_table(color_quantization_table.cbegin(), color_quantization_table.cend());
    LOGT_MDEC(std::format("Scale Table ({:d} halfwords):", scale_table.size()));
    trace_values_as_table(scale_table.cbegin(), scale_table.cend());

    // Dump input blocks
    //LOGT_MDEC(std::format("Block Input Queue ({:d} halfwords):", data_input_queue.size()));
    //std::stringstream ss;
    //for (uint16_t value : data_input_queue) {
    //    ss << std::format("0x{:02X}", value);

    //    if (value == MDEC_END_OF_BLOCK) {
    //        LOGT_MDEC(ss.str());
    //        ss.str(std::string());
    //    } else {
    //        ss << ", ";
    //    }
    //}
    //// Check if input ended without end-of-block marker
    //if (!ss.str().empty()) {
    //    LOGW_MDEC(std::format("Macroblock input ended without end-of-block marker: {:s}", ss.str()));
    //}

    // Decompress and combine blocks into macroblocks
    assert(data_output_depth <= 3); // 0 = 4bit, 1 = 8bit, 2 = 24bit, 3 = 15bit
    if (data_output_depth == 0 || data_output_depth == 1) { // Monochrome
        std::vector<uint16_t> y;
        while (rle_decode_next_block(y)) {
            // TODO Decode
            if (data_output_depth == 0) { // 4bit: 8 * 8 * 4 bit values = 16 halfwords
                // TODO: Replace dummy data
                LOGW_MDEC(std::format("4 bit macroblocks not implemented: producing dummy macroblock"));
                for (uint32_t i = 0; i < 16; ++i) {
                    data_output_queue.push_back(0x48CF);
                }

            } else { // 8 bit: 8 * 8 * 8 bit values = 32 halfwords
                // TODO: Replace dummy data
                LOGW_MDEC(std::format("8 bit macroblocks not implemented: producing dummy macroblock"));
                for (uint32_t i = 0; i < 32; ++i) {
                    data_output_queue.push_back(0x7FFF);
                }
            }

            y.clear();
        }

    } else { // Colored
        std::vector<uint16_t> cr, cb, y1, y2, y3, y4;
        bool first_success;

        while ((first_success = rle_decode_next_block(cr))
               && rle_decode_next_block(cb)
               && rle_decode_next_block(y1)
               && rle_decode_next_block(y2)
               && rle_decode_next_block(y3)
               && rle_decode_next_block(y4)) {
            // TODO Decode
            if (data_output_depth == 2) { // 24 bit: 16 * 16 * 24 bit values = 384 halfwords
                // TODO: Replace dummy data
                LOGW_MDEC(std::format("24 bit macroblocks not implemented: producing dummy macroblock"));
                for (uint32_t i = 0; i < 384; ++i) {
                    data_output_queue.push_back(0x00FF); // color pattern
                }

            } else { // 15 bit: 16 * 16 * 16 bit values = 256 halfwords
                // TODO: Replace dummy data
                LOGW_MDEC(std::format("16 bit macroblocks not implemented: producing dummy macroblock"));
                for (uint32_t i = 0; i < 256; ++i) {
                    data_output_queue.push_back(0x001F); // blue
                }
            }
        }

        if (first_success) {
            // The first block was decoded successfully, but a subsequent block failed
            LOGW_MDEC(std::format("Not enough RLE-encoded blocks to decode next macroblock"));
        }

    }
}

bool MacroblockDecoder::rle_decode_next_block(std::vector<uint16_t>& buffer) {
    while (!data_input_queue.empty() && data_input_queue.front() == MDEC_END_OF_BLOCK) {
        data_input_queue.pop_front();
    }
    if (data_input_queue.empty()) {
        // No block left
        return false;
    }

    LOGT_MDEC(std::format("Decoding RLE-encoded block"));

    // TODO Decode
    // Just throw away for now
    while (!data_input_queue.empty() && data_input_queue.front() != MDEC_END_OF_BLOCK) {
        data_input_queue.pop_front();
    }

    if (data_input_queue.empty()) {
        // We did not end with an end-of-block marker. This is wrong!
        LOGW_MDEC(std::format("RLE-encoded block ended unexpectedly"));
        return false;
    }

    return true;
}

MacroblockDecoder::MacroblockDecoder(Bus *bus) {
    this->bus = bus;

    reset();
}

void MacroblockDecoder::reset() {
    state = State::IDLE;

    luminance_quantization_table.clear();
    color_quantization_table.clear();
    scale_table.clear();

    data_input_queue.clear();
    data_output_queue.clear();

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
    return data_out_enabled && !data_output_queue.empty();
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
                data_input_queue.push_back(value & 0xFFFF);
                data_input_queue.push_back((value >> 16) & 0xFFFF);

                break;
            case State::CMD_SET_IQTAB:
                LOGT_MDEC(std::format("Received iqtab value 0x{:08X}", value));
                if (luminance_quantization_table.size() < 64) {
                    luminance_quantization_table.push_back(value & 0xFF);
                    luminance_quantization_table.push_back((value >> 8) & 0xFF);
                    luminance_quantization_table.push_back((value >> 16) & 0xFF);
                    luminance_quantization_table.push_back((value >> 24) & 0xFF);
                } else {
                    color_quantization_table.push_back(value & 0xFF);
                    color_quantization_table.push_back((value >> 8) & 0xFF);
                    color_quantization_table.push_back((value >> 16) & 0xFF);
                    color_quantization_table.push_back((value >> 24) & 0xFF);
                }
                break;
            case State::CMD_SET_SCALE:
                LOGT_MDEC(std::format("Received scale value 0x{:08X}", value));
                scale_table.push_back(value & 0xFFFF);
                scale_table.push_back((value >> 16) & 0xFFFF);
                break;
            case State::IDLE: // Not reachable
                break;
        }

        --remaining_parameter_words;
        if (remaining_parameter_words == 0xFFFF) { // The stored value is minus one
            LOGT_MDEC(std::format("Received last parameter word"));
            if (state == State::CMD_DECODE_MACROBLOCK) {
                decode_collected_blocks();
            }
            state = State::IDLE;
            received_all_parameters = true;
        }
    }
}

uint16_t MacroblockDecoder::read() {
    uint16_t value = 0;
    if (!data_output_queue.empty()) {
        value = data_output_queue.front();
        data_output_queue.pop_front();
    } else {
        LOGW_MDEC("Read from MDEC, but output queue is empty!");
    }

    return value;
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

