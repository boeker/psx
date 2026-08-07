#ifndef PSX_MDEC_H
#define PSX_MDEC_H

#include <cstdint>
#include <deque>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace PSX {

// 0x1F80'1820 Write: MDEC Command/Parameter Register
// 0x1F80'1820 Read: MDEC Data/Response Register

// 0x1F80'1824 Write: MDEC Control/Reset Register
#define MDEC_CONTROL_RESET 31 // 0 = no change, 1 = abort, status 0x8004'0000
#define MDEC_CONTROL_ENABLE_DATA_IN_REQ 30 // 0 = disable, 1 = enable DMA0 and status bit
#define MDEC_CONTROL_ENABLE_DATA_OUT_REQ 29 // 0 = disable, 1 = enable DMA1 and status bit

// 0x1F80'1824 Read: MDEC Status Register
#define MDEC_STATUS_DATA_OUT_QUEUE_EMPTY 31 // 0 = no, 1 = empty
#define MDEC_STATUS_DATA_IN_QUEUE_FULL 30 // 0 = no, 1 = full or last word received
#define MDEC_STATUS_CMD_BUSY 29 // 0 = ready, 1 = busy receiving or processing params
#define MDEC_STATUS_DATA_IN_REQ 28 // DMA0 enabled and ready to receive?
#define MDEC_STATUS_DATA_OUT_REQ 27 // DMA1 enabled and ready to send?
#define MDEC_STATUS_DATA_OUTPUT_DEPTH1 26 // 0 = 4bit, 1 = 8bit, 2 = 24bit, 3 = 15bit
#define MDEC_STATUS_DATA_OUTPUT_DEPTH0 25
#define MDEC_STATUS_DATA_OUTPUT_SIGNED 24 // 0 = unsigned, 1 = signed
#define MDEC_STATUS_DATA_OUTPUT_BIT15 23 // 0 = clear, 1 = set
#define MDEC_STATUS_CURRENT_BLOCK2 18 // 0...3 = Y1...Y4, 4 = Cr/Y, 5 = Cb
#define MDEC_STATUS_CURRENT_BLOCK1 17
#define MDEC_STATUS_CURRENT_BLOCK0 16
#define MDEC_STATUS_PARAMETER_WORDS_REMAINING15 15 // Number of parameter words remaining (minus one)
#define MDEC_STATUS_PARAMETER_WORDS_REMAINING0 0

#define MDEC_CMD_CMD2 31 // 1 = decode_macroblock, 2 = set_iqtab, 3 = set_scale, 0, 4...7 = no_function
#define MDEC_CMD_CMD1 30
#define MDEC_CMD_CMD0 29
#define MDEC_CMD_DATA_OUTPUT_DEPTH1 28
#define MDEC_CMD_DATA_OUTPUT_DEPTH0 27
#define MDEC_CMD_DATA_OUTPUT_SIGNED 26 // 0 = unsigned, 1 = signed
#define MDEC_CMD_DATA_OUTPUT_BIT15 25 // 0 = clear, 1 = set
#define MDEC_CMD_PARAMETER_WORDS_REMAINING15 15 // Number of parameter words remaining (minus one)
#define MDEC_CMD_PARAMETER_WORDS_REMAINING0 0
#define MDEC_CMD_COLOR 0 // for set_iqtab, 0 = luminance only, 1 = luminance and color

#define MDEC_END_OF_BLOCK 0xFE00

class Bus;

class MacroblockDecoder {
private:
    static uint8_t zigzag[64];
    uint8_t zagzig[64];

    Bus *bus;

    enum class State {
        IDLE,
        CMD_DECODE_MACROBLOCK,
        CMD_SET_IQTAB,
        CMD_SET_SCALE
    };
    State state;

    std::vector<uint8_t> luminance_quantization_table;
    std::vector<uint8_t> color_quantization_table;
    std::vector<int16_t> scale_table;

    // Stores the incoming, RLE-coded blocks
    std::deque<uint16_t> data_input_queue;
    // Stores the outgoing, decompressed macroblocks
    std::deque<uint16_t> data_output_queue;

    bool received_all_parameters;
    uint16_t remaining_parameter_words;

    bool data_in_enabled;
    bool data_out_enabled;

    uint8_t data_output_depth;
    bool data_output_signed;
    bool data_output_bit15;
    uint8_t current_block;

    friend std::ostream& operator<<(std::ostream &os, const MacroblockDecoder &mdec);

    uint32_t get_status_register() const;
    static std::string get_status_register_explanation(uint32_t reg);

    void extract_data_output_bits(uint32_t command);

    // Commands
    void decode_macroblock(uint32_t command); // 1
    void set_iqtab(uint32_t command); // 2
    void set_scale(uint32_t command); // 3
    void no_function(uint32_t command); // 0, 4...7

    template<std::input_iterator ITER, std::sentinel_for<ITER> SENT>
    static void trace_values_as_table(ITER begin, SENT end, uint32_t width = 8);
    void decode_collected_blocks();

    static int16_t sign_extend(uint16_t value);
    static int16_t clamp(int16_t value);
    // Reads the next block from the input queue into the provided buffer
    bool read_next_block(std::vector<uint16_t>& block);
    // Decode the RLE-encoded block
    void rle_decode_block(std::vector<int16_t>& decoded_block, const std::vector<uint16_t>& encoded_block);
    // Undo zigzag order
    void zagzig_block(std::vector<int16_t>& zagzig_blck, const std::vector<int16_t>& zigzag_block);
    // De-quantize block
    void dequantize_block(const std::vector<uint8_t>& q_table, std::vector<int16_t>& dequantized_block, const std::vector<int16_t>& quantized_block);
    // Reads, RLE-decodes, zagzigs, and de-quantizes the next block from the input queue with debug output after every step
    bool decode_next_block_stepwise(const std::vector<uint8_t>& q_table, std::vector<int16_t>& block);
    // Reads, RLE-decodes, zagzigs, and de-quantizes the next block from the input queue
    bool decode_next_block(const std::vector<uint8_t>& quant, std::vector<int16_t>& buffer);
    static uint8_t index(uint8_t i, uint8_t j) { return i * 8 + j; }
    void idct(std::vector<int32_t>& result, std::vector<int16_t>& block);
    static int16_t clamp_color(int16_t value);
    void yuv_to_rgb(std::vector<uint8_t>& r, std::vector<uint8_t>& g, std::vector<uint8_t>& b,
                    const std::vector<int32_t>& cr, const std::vector<int32_t>& cb, const std::vector<int32_t>& y,
                    uint8_t x_offset, uint8_t y_offset);

public:
    MacroblockDecoder(Bus *bus);
    void reset();

    bool data_in_request() const;
    bool data_out_request() const;

    void process(uint32_t value);
    uint16_t read();

    template <typename T>
    void write(uint32_t address, T value);

    template <typename T>
    T read(uint32_t address);
};

}

#endif
