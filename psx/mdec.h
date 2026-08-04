#ifndef PSX_MDEC_H
#define PSX_MDEC_H

#include <cstdint>
#include <deque>
#include <iostream>
#include <string>

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

class Bus;

class MacroblockDecoder {
private:
    Bus *bus;

    enum class State {
        IDLE,
        CMD_DECODE_MACROBLOCK,
        CMD_SET_IQTAB,
        CMD_SET_SCALE
    };
    State state;

    std::deque<uint32_t> data_out_queue;
    std::deque<uint32_t> data_in_queue;
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

public:
    MacroblockDecoder(Bus *bus);
    void reset();

    bool data_in_request() const;
    bool data_out_request() const;

    void process(uint32_t value);

    template <typename T>
    void write(uint32_t address, T value);

    template <typename T>
    T read(uint32_t address);
};

}

#endif
