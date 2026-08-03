#ifndef PSX_MDEC_H
#define PSX_MDEC_H

#include <cstdint>
#include <iostream>
#include <string>

namespace PSX {

// 0x1F80'1820 Write: MDEC Command/Parameter Register
// 0x1F80'1820 Read: MDEC Data/Response Register

// 0x1F80'1824 Write: MDEC Control/Reset Register
#define MDEC_CONTROL_RESET 31 // (0 = no change, 1 = abort, status 0x8004'0000)
#define MDEC_CONTROL_ENABLE_DATA_IN_REQ 30 // (0 = disable, 1 = enable DMA0 and status bit)
#define MDEC_CONTROL_ENABLE_DATA_OUT_REQ 29 // (0 = disable, 1 = enable DMA1 and status bit)

// 0x1F80'1824 Read: MDEC Status Register
#define MDEC_STATUS_DATA_OUT_QUEUE_EMPTY 31 // (0 = no, 1 = empty)
#define MDEC_STATUS_DATA_IN_QUEUE_FULL 30 // (0 = no, 1 = full or last word received)
//...


class Bus;

class MacroblockDecoder {
private:
    Bus *bus;

    friend std::ostream& operator<<(std::ostream &os, const MacroblockDecoder &mdec);

    std::string get_status_explanation() const;

public:
    MacroblockDecoder(Bus *bus);
    void reset();

    template <typename T>
    void write(uint32_t address, T value);

    template <typename T>
    T read(uint32_t address);
};

}

#endif
