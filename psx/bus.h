#ifndef PSX_BUS_H
#define PSX_BUS_H

#include <cstdint>
#include <string>

#include "cdrom.h"
#include "cpu.h"
#include "memory.h"
#include "bios.h"
#include "dma.h"
#include "timers.h"
#include "interrupts.h"
#include "dma.h"
#include "spu.h"
#include "gpu.h"

/*
KUSEG     KSEG0     KSEG1
00000000h 80000000h A0000000h  2048K  Main RAM (first 64K reserved for BIOS)
1F000000h 9F000000h BF000000h  8192K  Expansion Region 1 (ROM/RAM)
1F800000h 9F800000h    --      1K     Scratchpad (D-Cache used as Fast RAM)
1F801000h 9F801000h BF801000h  8K     I/O Ports
1F802000h 9F802000h BF802000h  8K     Expansion Region 2 (I/O Ports)
1FA00000h 9FA00000h BFA00000h  2048K  Expansion Region 3 (whatever purpose)
1FC00000h 9FC00000h BFC00000h  512K   BIOS ROM (Kernel) (4096K max)
      FFFE0000h (KSEG2)        0.5K   I/O Ports (Cache Control)
*/

// 512MiB Memory Regions
// 0x00000000 KUSEG
// 0x20000000 KUSEG (Error)
// 0x40000000 KUSEG (Error)
// 0x60000000 KUSEG (Error)
// 0x80000000 KSEG0
// 0xA0000000 KSEG1 (No Scratchpad!)
// 0xC0000000 KSEG2
// 0xE0000000 KSEG2

// Main RAM
// 0x00000000 - 0x001FFFFF (0b0000 0000 0000 ... - 0b0000 0000 0001 ...)
// 0x80000000 - 0x801FFFFF (0b1000 0000 0000 ... - 0b1000 0000 0001 ...)
// 0xA0000000 - 0xA01FFFFF (0b1010 0000 0000 ... - 0b1010 0000 0001 ...)

// Expansion Region 1
// 0x1F000000 - 0x1F7FFFFF (0b0001 1111 0000 ... - 0b0001 1111 0111 ...)
// 0x9F000000 - 0x9F7FFFFF (0b1001 1111 0000 ... - 0b1001 1111 0111 ...)
// 0xBF000000 - 0xBF7FFFFF (0b1011 1111 0000 ... - 0b1011 1111 0111 ...)

// D-Cache (Scratchpad)
// 0x1F800000 - 0x1F8003FF (0b0001 1111 1000 ... - 0b0001 1111 1000 ...)
// Also in KSEG0?
// 0x9F800000 - 0x9F8003FF (0b1001 1111 1000 ... - 0b1001 1111 1000 ...)
// But not in KSEG1?

// Hardware Registers (I/0 Ports)
// 0x1F801000 - 0x1FBFFFFF (0b0001 1111 1000 0000 0001 ... - 0b0001 1111 1011 1111 ...)
// our memory is smaller (0x1FFF is max value)
// 0x1F801000 - 0x1F802FFF (0b0001 1111 1000 0000 0001 ... - 0b0001 1111 1000 0000 ...)

// Bios ROM
// 0x1FC00000 - 0x1FC7FFFF (0b0001 1111 1100 ... - 0b0001 1111 1100 ...)
// 0x9FC00000 - 0x9FC7FFFF (0b1001 1111 1100 ... - 0b1001 1111 1100 ...)
// 0xBFC00000 - 0xBFC7FFFF (0b1011 1111 1100 ... - 0b0011 1111 1100 ...)

// Cache Control Register
// 0xFFFE0130


//#define IO_PORTS_SIZE (8 * 1024)

namespace PSX {

class Bus {
public:
    CDROM cdrom;
    CPU cpu;
    Memory memory;
    Bios bios;
    Timers timers;
    DMA dma;
    Interrupts interrupts;
    SPU spu;
    GPU gpu;

    friend std::ostream& operator<<(std::ostream &os, const Bus &bus);

public:
    Bus();
    virtual ~Bus();
    void reset();

    template <typename T> T read(uint32_t address);
    template <typename T> void write (uint32_t address, T value);

    uint8_t readByte(uint32_t address);
    uint16_t readHalfWord(uint32_t address);
    uint32_t readWord(uint32_t address);

    void writeByte(uint32_t address, uint8_t byte);
    void writeHalfWord(uint32_t address, uint16_t halfWord);
    void writeWord(uint32_t address, uint32_t word);
};
}

#endif
