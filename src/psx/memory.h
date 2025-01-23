#ifndef PSX_MEMORY_H
#define PSX_MEMORY_H

#include <cstdint>
#include <string>

#include "registers.h"

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

#define MAIN_RAM_SIZE (2048 * 1024)
#define IO_PORTS_SIZE (8 * 1024)
#define BIOS_SIZE (512 * 1024)
#define DCACHE_SIZE 1024

namespace PSX {

class Memory {
private:
    uint8_t* mainRAM;
    uint8_t* ioPorts;
    uint8_t* bios;
    uint8_t* dCache;

    uint8_t cacheControlRegister[4];
    uint32_t null;

public:
    Registers regs;

    Memory();
    virtual ~Memory();
    void reset();

    void readBIOS(const std::string &file);

    uint8_t readByte(uint32_t address);
    uint16_t readHalfWord(uint32_t address);
    uint32_t readWord(uint32_t address);

    void writeByte(uint32_t address, uint8_t byte);
    void writeHalfWord(uint32_t address, uint16_t halfWord);
    void writeWord(uint32_t address, uint32_t word);

private:
    void* resolveAddress(uint32_t address);
};
}

#endif
