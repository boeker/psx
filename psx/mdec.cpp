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

uint8_t MacroblockDecoder::zigzag[] = {
     0,  1,  5,  6, 14, 15, 27, 28,
     2,  4,  7, 13, 16, 26, 29, 42,
     3,  8, 12, 17, 25, 30, 41, 43,
     9, 11, 18, 24, 31, 40, 44, 53,
    10, 19, 23, 32, 39, 45, 52, 54,
    20, 22, 33, 38, 46, 51, 55, 60,
    21, 34, 37, 47, 50, 56, 59, 61,
    35, 36, 48, 49, 57, 58, 62, 63
};

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
void MacroblockDecoder::trace_values_as_table(ITER begin, SENT end, uint32_t width) {
    std::stringstream ss;
    auto it = begin;
    while (it != end) {
        for (uint32_t i = 0; i < width && it != end; ++i, ++it) {
            ss << std::format("0x{:0{}X}", static_cast<std::make_unsigned_t<std::iter_value_t<ITER>>>(*it), 2 * sizeof(std::iter_value_t<ITER>)) << ((i < width - 1) ? ", " : ",");
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
        std::vector<int16_t> y;
        while (decode_next_block_stepwise(luminance_quantization_table, y)) {
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
        std::vector<int16_t> cr, cb, y1, y2, y3, y4;
        std::vector<uint8_t> macroblock_r, macroblock_g, macroblock_b;

        while (true) {
            LOGT_MDEC(std::format("Decoding and uncompressing block Cr"));
            std::vector<int32_t> cr_uncomp, cb_uncomp, y1_uncomp, y2_uncomp, y3_uncomp, y4_uncomp;
            if (!decode_next_block_stepwise(color_quantization_table, cr)) {
                break;
            }
            idct(cr_uncomp, cr);

            LOGT_MDEC(std::format("Decoding and uncompressing block Cb"));
            if (!decode_next_block_stepwise(color_quantization_table, cb)) {
                LOGW_MDEC(std::format("Not enough RLE-encoded blocks to decode next macroblock"));
                break;
            }
            idct(cb_uncomp, cb);

            LOGT_MDEC(std::format("Decoding and uncompressing block Y1"));
            if (!decode_next_block_stepwise(luminance_quantization_table, y1)) {
                LOGW_MDEC(std::format("Not enough RLE-encoded blocks to decode next macroblock"));
                break;
            }
            idct(y1_uncomp, y1);

            LOGT_MDEC(std::format("Decoding and uncompressing block Y2"));
            if (!decode_next_block_stepwise(luminance_quantization_table, y2)) {
                LOGW_MDEC(std::format("Not enough RLE-encoded blocks to decode next macroblock"));
                break;
            }
            idct(y2_uncomp, y2);

            LOGT_MDEC(std::format("Decoding and uncompressing block Y3"));
            if (!decode_next_block_stepwise(luminance_quantization_table, y3)) {
                LOGW_MDEC(std::format("Not enough RLE-encoded blocks to decode next macroblock"));
                break;
            }
            idct(y3_uncomp, y3);

            LOGT_MDEC(std::format("Decoding and uncompressing block Y4"));
            if (!decode_next_block_stepwise(luminance_quantization_table, y4)) {
                LOGW_MDEC(std::format("Not enough RLE-encoded blocks to decode next macroblock"));
                break;
            }
            idct(y4_uncomp, y4);

            yuv_to_rgb(macroblock_r, macroblock_g, macroblock_b,
                       cr_uncomp, cb_uncomp, y1_uncomp, 0, 0);
            yuv_to_rgb(macroblock_r, macroblock_g, macroblock_b,
                       cr_uncomp, cb_uncomp, y2_uncomp, 8, 0);
            yuv_to_rgb(macroblock_r, macroblock_g, macroblock_b,
                       cr_uncomp, cb_uncomp, y3_uncomp, 0, 8);
            yuv_to_rgb(macroblock_r, macroblock_g, macroblock_b,
                       cr_uncomp, cb_uncomp, y4_uncomp, 8, 8);

            if (data_output_depth == 2) { // 24 bit: 16 * 16 * 24 bit values = 384 halfwords

                LOGT_MDEC(std::format("Writing macroblock as 24 bit colors"));
                std::vector<uint8_t> macroblock;
                assert(macroblock_r.size() == 256);
                assert(macroblock_g.size() == 256);
                assert(macroblock_b.size() == 256);
                for (uint32_t i = 0; i < 256; ++i) {
                    macroblock.push_back(macroblock_r[i]);
                    macroblock.push_back(macroblock_g[i]);
                    macroblock.push_back(macroblock_b[i]);
                }
                for (uint32_t i = 0; i < macroblock.size(); i += 2) {
                    data_output_queue.push_back((static_cast<uint16_t>(macroblock[i + 1]) << 8) | static_cast<uint16_t>(macroblock[i]));
                }

            } else { // 15 bit: 16 * 16 * 16 bit values = 256 halfwords
                LOGT_MDEC(std::format("Writing macroblock as 15 bit colors"));
                std::vector<uint8_t> macroblock;
                assert(macroblock_r.size() == 256);
                assert(macroblock_g.size() == 256);
                assert(macroblock_b.size() == 256);
                for (uint32_t i = 0; i < 256; ++i) {
                    data_output_queue.push_back((static_cast<uint16_t>(macroblock_b[i] >> 3) << 10)
                                                | (static_cast<uint16_t>(macroblock_g[i] >> 3) << 5)
                                                | (static_cast<uint16_t>(macroblock_r[i] >> 3)));
                }
            }

            cr.clear();
            cb.clear();
            y1.clear();
            y2.clear();
            y3.clear();
            y4.clear();
        }

    }
}

int16_t MacroblockDecoder::sign_extend(uint16_t value) {
    if (value & 0x0200) {
        return 0xFC00 | value;
    } else {
        return value;
    }
}

int16_t MacroblockDecoder::clamp(int32_t value) {
    int32_t clamped = std::min(static_cast<int32_t>(0x03FF), value);
    clamped = std::max(static_cast<int32_t>(-0x0400), clamped);
    return clamped;
}

bool MacroblockDecoder::read_next_block(std::vector<uint16_t>& block) {
    LOGT_MDEC(std::format("Reading RLE-encoded block"));
    assert(block.empty());

    // Read the next block from the input queue. Do not decode.
    while (!data_input_queue.empty() && data_input_queue.front() == MDEC_END_OF_BLOCK) {
        data_input_queue.pop_front();
    }

    if (data_input_queue.empty()) {
        // No block left
        return false;
    }

    while (!data_input_queue.empty() && data_input_queue.front() != MDEC_END_OF_BLOCK) {
        block.push_back(data_input_queue.front());
        data_input_queue.pop_front();
    }

    if (data_input_queue.empty()) {
        // We did not end with an end-of-block marker. This is wrong!
        LOGW_MDEC(std::format("RLE-encoded block ended unexpectedly"));
        return false;
    }

    // End-of-block marker
    block.push_back(data_input_queue.front());
    data_input_queue.pop_front();

    // Trace for debugging purposes
    LOGT_MDEC(std::format("Read RLE-encoded block:"));
    trace_values_as_table(block.cbegin(), block.cend());

    return true;
}

void MacroblockDecoder::rle_decode_block(std::vector<int16_t>& decoded_block, const std::vector<uint16_t>& encoded_block) {
    LOGT_MDEC(std::format("RLE-decoding block"));
    assert(decoded_block.empty());
    assert(!encoded_block.empty());

    // RLE-decode the encoded block. Do not zagzig or de-quantize.
    auto it = encoded_block.cbegin();

    // DCT halfword
    uint16_t dct = *(it++);
    uint16_t quantization_factor = dct >> 10; // 6 bits, unsigned
    int16_t dc = sign_extend(dct & 0x03FF); // 10 bits, signed
    decoded_block.push_back(dc);

    // 0 to 63 RLE halfwords
    for (; it != encoded_block.cend() && *it != MDEC_END_OF_BLOCK; ++it) {
        uint16_t rle = *it;

        uint16_t len = rle >> 10; // 6 bits, unsigned
        int16_t ac = sign_extend(rle & 0x03FF); // 10 bits, signed

        for (uint16_t i = 0; i < len; ++i) {
            decoded_block.push_back(0U);
        }
        decoded_block.push_back(ac);
    }

    assert(it != encoded_block.cend() && *it == MDEC_END_OF_BLOCK);
    assert(++it == encoded_block.cend());

    // EOB: pad rest of block with 0
    while (decoded_block.size() < 64) {
        decoded_block.push_back(0U);
    }

    // Append quantization factor to block
    decoded_block.push_back(quantization_factor);

    // Trace for debugging purposes
    LOGT_MDEC(std::format("RLE-decoded block (with appended quantization_factor):"));
    trace_values_as_table(decoded_block.cbegin(), decoded_block.cend());
}

void MacroblockDecoder::zagzig_block(std::vector<int16_t>& zagzig_block, const std::vector<int16_t>& zigzag_block) {
    LOGT_MDEC(std::format("Zagzigging block"));
    assert(zagzig_block.empty());
    assert(zigzag_block.size() == 64 + 1);

    int16_t quantization_factor = zigzag_block[64];

    if (quantization_factor == 0) {
        LOGT_MDEC(std::format("Quantization factor is zero, not zagzigging"));
        zagzig_block.insert(zagzig_block.end(), zigzag_block.cbegin(), zigzag_block.cend());
    } else {
        for (uint32_t i = 0; i < 64; ++i) {
            zagzig_block.push_back(zigzag_block[zigzag[i]]);
        }
        // Quantization factor
        zagzig_block.push_back(zigzag_block[64]);
    }

    // Trace for debugging purposes
    LOGT_MDEC(std::format("Zagzigged block:"));
    trace_values_as_table(zagzig_block.cbegin(), zagzig_block.cend());
}

void MacroblockDecoder::dequantize_block(const std::vector<uint8_t>& q_table, std::vector<int16_t>& dequantized_block, const std::vector<int16_t>& quantized_block) {
    LOGT_MDEC(std::format("De-quantizing block"));
    assert(dequantized_block.empty());
    assert(quantized_block.size() == 64 + 1);

    int16_t quantization_factor = quantized_block[64];

    if (quantization_factor == 0) {
        for (uint32_t i = 0; i < 64; ++i) {
            dequantized_block.push_back(clamp(static_cast<int32_t>(quantized_block[i]) * 2));
        }
    } else {
        dequantized_block.push_back(clamp(quantized_block[0] * q_table[0]));
        for (uint32_t i = 1; i < 64; ++i) {
            dequantized_block.push_back(clamp((static_cast<int32_t>(quantized_block[i]) * q_table[i] * quantization_factor + 4) / 8));
        }
    }

    // Trace for debugging purposes
    LOGT_MDEC(std::format("De-quantized block:"));
    trace_values_as_table(dequantized_block.cbegin(), dequantized_block.cend());
}

bool MacroblockDecoder::decode_next_block_stepwise(const std::vector<uint8_t>& q_table, std::vector<int16_t>& block) {
    LOGT_MDEC(std::format("Decoding RLE-encoded block step by step"));
    assert(block.empty());

    std::vector<uint16_t> next_block;
    if (!read_next_block(next_block)) {
        return false;
    }
    std::vector<int16_t> rle_decoded_block;
    rle_decode_block(rle_decoded_block, next_block);
    std::vector<int16_t> zagzigged_block;
    zagzig_block(zagzigged_block, rle_decoded_block);
    dequantize_block(q_table, block, zagzigged_block);

    return true;
}

bool MacroblockDecoder::decode_next_block(const std::vector<uint8_t>& quant, std::vector<int16_t>& buffer) {
    // RLE-decoded, zagzig, and de-quantize
    while (!data_input_queue.empty() && data_input_queue.front() == MDEC_END_OF_BLOCK) {
        data_input_queue.pop_front();
    }
    if (data_input_queue.empty()) {
        // No block left
        return false;
    }

    LOGT_MDEC(std::format("Decoding RLE-encoded block"));
    buffer.resize(64);

    // DCT halfword
    uint16_t dct = data_input_queue.front();
    data_input_queue.pop_front();
    uint16_t quantization_factor = dct >> 10; // 6 bits, unsigned
    int16_t dc = sign_extend(dct & 0x03FF); // 10 bits, signed

    if (quantization_factor == 0) {
        buffer[0] = clamp(dc * 2);
    } else {
        buffer[0] = clamp(dc * quant[0]);
    }

    // 0 to 63 RLE halfwords
    uint8_t pos = 1;
    while (!data_input_queue.empty() && data_input_queue.front() != MDEC_END_OF_BLOCK) {
        uint16_t rle = data_input_queue.front();
        data_input_queue.pop_front();

        uint16_t len = rle >> 10; // 6 bits, unsigned
        int16_t ac = sign_extend(rle & 0x03FF); // 10 bits, signed
        for (uint16_t i = 0; i < len; ++i) {
            assert(pos < 64);
            if (quantization_factor == 0) {
                buffer[pos++] = 0;
            } else {
                buffer[zagzig[pos++]] = 0;
            }
        }
        assert(pos < 64);
        if (quantization_factor == 0) {
            buffer[pos++] = clamp(ac * 2);
        } else {
            buffer[zagzig[pos++]] = clamp((ac * quant[pos] * quantization_factor + 4) / 8);
        }
    }

    if (data_input_queue.empty()) {
        // We did not end with an end-of-block marker. This is wrong!
        LOGW_MDEC(std::format("RLE-encoded block ended unexpectedly"));
        return false;
    }

    // EOB: pad rest of block with 0
    data_input_queue.pop_front();
    while (pos < 64) {
        if (quantization_factor == 0) {
            buffer[pos++] = 0;
        } else {
            buffer[zagzig[pos++]] = 0;
        }
    }

    // Trace for debugging purposes
    LOGT_MDEC(std::format("Decoded block (quantization_factor = 0x{:04X}):", quantization_factor));
    trace_values_as_table(++buffer.cbegin(), buffer.cend());

    return true;
}

void MacroblockDecoder::idct(std::vector<int32_t>& result, std::vector<int16_t>& block) {
    // Computes IDCT^T * B * IDCT, where IDCT is the IDCT matrix and B the block
    LOGT_MDEC(std::format("Performing IDCT"));

    assert(block.size() == 64);
    result.resize(64);

    std::vector<int32_t> temp;
    temp.resize(64);

    // IDCT^T * B
    for (uint8_t i = 0; i < 8; ++i) {
        for (uint8_t j = 0; j < 8; ++j) {
            int32_t sum = 0;
            for (uint8_t k = 0; k < 8; ++k) {
                sum += (scale_table[index(k, i)] / 8) * static_cast<int32_t>(block[index(k, j)]);
            }
            temp[index(i, j)] = (sum + 0x0FFF) / 0x2000;
        }
    }

    // block * IDCT
    for (uint8_t i = 0; i < 8; ++i) {
        for (uint8_t j = 0; j < 8; ++j) {
            int32_t sum = 0;
            for (uint8_t k = 0; k < 8; ++k) {
                sum += static_cast<int32_t>(temp[index(i, k)]) * (scale_table[index(k, j)] / 8);
            }
            result[index(i, j)] = (sum + 0x0FFF) / 0x2000;
        }
    }

    // Trace for debugging purposes
    LOGT_MDEC(std::format("Performed IDCT:"));
    trace_values_as_table(result.cbegin(), result.cend());
}

int16_t MacroblockDecoder::clamp_color(int32_t value) {
    int32_t clamped = std::min(static_cast<int32_t>(127), value);
    clamped = std::max(static_cast<int32_t>(-128), clamped);
    return clamped;
}

void MacroblockDecoder::yuv_to_rgb(std::vector<uint8_t>& r, std::vector<uint8_t>& g, std::vector<uint8_t>& b,
                                   const std::vector<int32_t>& cr, const std::vector<int32_t>& cb, const std::vector<int32_t>& y_block,
                                   uint8_t x_offset, uint8_t y_offset) {
    LOGT_MDEC(std::format("Converting YUV blocks to (part of) RGB macroblock (offset {:d}, {:d})", x_offset, y_offset));
    r.resize(16 * 16);
    g.resize(16 * 16);
    b.resize(16 * 16);

    for (uint32_t y = 0; y < 8; ++y) {
        for (uint32_t x = 0; x < 8; ++x) {
            float r_fl = cr[(x_offset + x) / 2 + ((y_offset + y) / 2) * 8];
            float b_fl = cb[(x_offset + x) / 2 + ((y_offset + y) / 2) * 8];
            float g_fl = -0.3437 * b_fl - 0.7143 * r_fl;
            r_fl = 1.402 * r_fl;
            b_fl = 1.772 * b_fl;

            int32_t y_value = y_block[x + y * 8];
            int16_t r_value = clamp_color(y_value + static_cast<int32_t>(r_fl));
            int16_t g_value = clamp_color(y_value + static_cast<int32_t>(g_fl));
            int16_t b_value = clamp_color(y_value + static_cast<int32_t>(b_fl));

            if (!data_output_signed) {
                r_value += 128;
                g_value += 128;
                b_value += 128;
            }

            uint32_t coord = x_offset + x + (y_offset + y) * 16;
            assert(coord < 256);
            r[coord] = r_value;
            g[coord] = g_value;
            b[coord] = b_value;
        }
    }

    LOGT_MDEC(std::format("Converted YUV blocks to (part of) RGB macroblock:"));
    LOGT_MDEC("Macroblock (red):");
    trace_values_as_table(r.cbegin(), r.cend(), 16);
    LOGT_MDEC("Macroblock (green):");
    trace_values_as_table(g.cbegin(), g.cend(), 16);
    LOGT_MDEC("Macroblock (blue):");
    trace_values_as_table(b.cbegin(), b.cend(), 16);
}

MacroblockDecoder::MacroblockDecoder(Bus *bus) {
    this->bus = bus;

    for (uint8_t i = 0; i < 64; ++i) {
        zagzig[zigzag[i]] = i;
    }

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

