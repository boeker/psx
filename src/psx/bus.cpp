#include "bus.h"

#include <cassert>
#include <cstring>
#include <format>
#include <fstream>
#include <sstream>

#include "util/log.h"
#include "exceptions/exceptions.h"

using namespace util;

namespace PSX {

Bus::Bus()
    : cpu(this), interrupts(this), gpu(this) {
    reset();
}

void Bus::reset() {
    cpu.reset();
    memory.reset();
    bios.reset();
    timers.reset();
    dma.reset();
    interrupts.reset();
    spu.reset();
    gpu.reset();
}

Bus::~Bus() {
}

template <typename T>
T Bus::read(uint32_t address) {
    Log::log(std::format(" [@0x{:08X} -> ", address), Log::Type::MEMORY);
    T value = 0;

    if ((address & 0x1FE00000) == 0x00000000) { // Main RAM
        if (cpu.cp0regs.statusRegisterIsolateCacheIsSet()) {
            value = memory.readDCache<T>(address);

        } else {
            value = memory.readMainRAM<T>(address);
        }

    } else if ((address & 0x1F800000) == 0x1F000000) { // Expansion Region 1
        //uint32_t offset = address & 0x007FFFFF;

        Log::log(std::format("Unimplemented Expansion Region 1 read @0x{:08X}", address), Log::Type::WARNING);

    } else if ((address & 0x1FFFFC00) == 0x1F8FFC00) { // D-Cache (Scratchpad)
        // Also in KSEG1 for now
        value = memory.readDCache<T>(address);

    } else if (((address & 0x1FFFF000) == 0x1F801000)) { // Hardware Registers (I/0 Ports)
        if (address <= 0x1F801060) {
            value = memory.readMemoryControlRegisters<T>(address);

        } else if ((address >= 0x1F801040) && (address <= 0x1F80105F)) {
            Log::log(std::format("Unimplemented peripheral read @0x{:08X}", address), Log::Type::WARNING);

        } else if ((address >= 0x1F801070) && (address <= 0x1F801077)) {
            value = interrupts.read<T>(address);

        } else if ((address >= 0x1F801080) && (address <= 0x1F8010FF)) {
            value = dma.read<T>(address);

        } else if ((address >= 0x1F801100) && (address <= 0x1F80112A)) {
            value = timers.read<T>(address);

        } else if ((address >= 0x1F801800) && (address <= 0x1F801803)) {
            Log::log(std::format("Unimplemented CDROM read @0x{:08X}", address), Log::Type::WARNING);

        } else if ((address >= 0x1F801810) && (address <= 0x1F801817)) {
            value = gpu.read<T>(address);

        } else if ((address >= 0x1F801820) && (address <= 0x1F801827)) {
            Log::log(std::format("Unimplemented MDEC read @0x{:08X}", address), Log::Type::WARNING);

        } else if ((address >= 0x1F801C00) && (address <= 0x1F801FFF)) {
            value = spu.read<T>(address);

        } else {
            //Log::log(std::format("Unimplemented Expansion Region 1 read @0x{:08X}", address), Log::Type::WARNING);
            throw exceptions::AddressOutOfBounds(std::format(
                "Read from unimplemented IO register @0x{:08X}", address));
        }
        //uint32_t offset = address & 0x00000FFF;

    } else if (((address & 0x1FFFF000) == 0x1F802000)) {
        //if ((address >= 0x1F802020) && (address <= 0x1F80202F)) {
        //    Log::log("~Dual Serial Port~", Log::Type::GPU);
        //}

        Log::log(std::format("Unimplemented Expansion Region 2 read @0x{:08X}", address), Log::Type::WARNING);

        //uint32_t offset = 0x00001000 + (address & 0x00000FFF);

    } else if ((address & 0x1FF80000) == 0x1FC00000) { // Bios ROM
        value = bios.read<T>(address);

    } else if (address == 0xFFFE0130) { // Cache Control Register 
        value = memory.readCacheControlRegister<T>(address);

    } else {
        throw exceptions::AddressOutOfBounds(std::format("Read @0x{:08X}", address ));
    }
    
    Log::log(std::format("0x{:0{}X}]", value, 2*sizeof(T)), Log::Type::MEMORY);
    return value;
}

template uint32_t Bus::read<uint32_t>(uint32_t address);
template uint16_t Bus::read<uint16_t>(uint32_t address);
template uint8_t Bus::read<uint8_t>(uint32_t address);

template <typename T>
void Bus::write (uint32_t address, T value) {
    Log::log(std::format(" [0x{:0{}X} -> @0x{:08X}]", value, 2*sizeof(T), address), Log::Type::MEMORY);

    if ((address & 0x1FE00000) == 0x00000000) { // Main RAM
        if (cpu.cp0regs.statusRegisterIsolateCacheIsSet()) {
            return memory.writeDCache<T>(address, value);

        } else {
            return memory.writeMainRAM<T>(address, value);
        }

    } else if ((address & 0x1F800000) == 0x1F000000) { // Expansion Region 1
        //uint32_t offset = address & 0x007FFFFF;

        throw exceptions::AddressOutOfBounds(std::format(
            "Write to unimplemented Expansion Region 1 @0x{:08X}", address));
        return;

    } else if ((address & 0x1FFFFC00) == 0x1F8FFC00) { // D-Cache (Scratchpad)
        // Also in KSEG1 for now
        return memory.writeDCache<T>(address, value);

    } if (((address & 0x1FFFF000) == 0x1F801000)) { // Hardware Registers (I/0 Ports)
        if (address <= 0x1F801060) {
            memory.writeMemoryControlRegisters<T>(address, value);

        } else if ((address >= 0x1F801040) && (address <= 0x1F80105F)) {
            Log::log(std::format("Unimplemented peripheral write @0x{:08X}", address), Log::Type::WARNING);

        } else if ((address >= 0x1F801070) && (address <= 0x1F801077)) {
            interrupts.write<T>(address, value);

        } else if ((address >= 0x1F801080) && (address <= 0x1F8010FF)) {
            dma.write<T>(address, value);

        } else if ((address >= 0x1F801100) && (address <= 0x1F80112A)) {
            timers.write<T>(address, value);

        } else if ((address >= 0x1F801800) && (address <= 0x1F801803)) {
            Log::log(std::format("Unimplemented CDROM write @0x{:08X}", address), Log::Type::WARNING);

        } else if ((address >= 0x1F801810) && (address <= 0x1F801817)) {
            gpu.write<T>(address, value);

        } else if ((address >= 0x1F801820) && (address <= 0x1F801827)) {
            Log::log(std::format("Unimplemented MDEC write @0x{:08X}", address), Log::Type::WARNING);

        } else if ((address >= 0x1F801C00) && (address <= 0x1F801FFF)) {
            spu.write<T>(address, value);

        } else {
            throw exceptions::AddressOutOfBounds(std::format(
                "Write to unimplemented IO register @0x{:08X}", address));
        }

        //uint32_t offset = address & 0x00000FFF;

    } else if (((address & 0x1FFFF000) == 0x1F802000)) {
        //if ((address >= 0x1F802020) && (address <= 0x1F80202F)) {
        //    Log::log("~Dual Serial Port~", Log::Type::GPU);
        //}
        if (address == 0x1F802041) {
            std::cout << std::format("Boot status: 0x{:02X}", 0xFF & value) << std::endl;

        } else {
            Log::log(std::format("Unimplemented Expansion Region 2 write @0x{:08X}", address), Log::Type::WARNING);
        }

        //uint32_t offset = 0x00001000 + (address & 0x00000FFF);
    } else if ((address & 0x1FF80000) == 0x1FC00000) { // Bios ROM
        // Bios is read only
        Log::log(std::format("Write to Bios @0x{:08X}", address), Log::Type::WARNING);
        return;

    } else if (address == 0xFFFE0130) { // Cache Control Register
        return memory.writeCacheControlRegister<T>(address, value);

    } else {
        throw exceptions::AddressOutOfBounds(std::format("Write @0x{:08X}", address ));
    }
}

template void Bus::write<uint32_t>(uint32_t address, uint32_t value);
template void Bus::write<uint16_t>(uint32_t address, uint16_t value);
template void Bus::write<uint8_t>(uint32_t address, uint8_t value);

uint8_t Bus::readByte(uint32_t address) {
    return read<uint8_t>(address);
    //Log::log(std::format(" [rb "), Log::Type::MEMORY);
    //uint8_t *memory = (uint8_t*)resolveAddress(address);

    //uint8_t byte = *memory;
    //Log::log(std::format("@0x{:08X} -0x{:02X}->]", address, byte), Log::Type::MEMORY);

    //return byte;
}

uint16_t Bus::readHalfWord(uint32_t address) {
    return read<uint16_t>(address);
    //Log::log(std::format(" [rhw "), Log::Type::MEMORY);
    //uint16_t *memory = (uint16_t*)resolveAddress(address); // PSX is little endian, so is x86

    //uint16_t halfWord = *memory;
    //Log::log(std::format("@0x{:08X} -0x{:04X}->]", address, halfWord), Log::Type::MEMORY);

    //return halfWord;
}

uint32_t Bus::readWord(uint32_t address) {
    return read<uint32_t>(address);
    //Log::log(std::format(" [rw "), Log::Type::MEMORY);
    //uint32_t *memory = (uint32_t*)resolveAddress(address); // PSX is little endian, so is x86

    //uint32_t word = *memory;
    //Log::log(std::format("@0x{:08X} -0x{:08X}->]", address, word), Log::Type::MEMORY);

    //return word;
}

void Bus::writeByte(uint32_t address, uint8_t byte) {
    write<uint8_t>(address, byte);
    //Log::log(std::format(" [wb -0x{:02X}-> @0x{:08X}]", byte, address), Log::Type::MEMORY);


    //if ((address >= 0x1F801070) && (address <= 0x1F801077)) {
    //    Log::log(std::format("\n~Interrupt write 0x{:02X} --> @0x{:08X}~\n", byte, address), Log::Type::INTERRUPT);
    //}
    //else if ((address >= 0x1F801100) && (address <= 0x1F80112A)) {
    //    Log::log(std::format("\n~Timer write 0x{:02X} --> @0x{:08X}~\n", byte, address), Log::Type::TIMER);
    //} else if ((address >= 0x1F801080) && (address <= 0x1F8010FF)) {
    //    Log::log(std::format("\n~DMA write 0x{:02X} --> @0x{:08X}~\n", byte, address), Log::Type::DMA);
    //} else if ((address >= 0x1F801810) && (address <= 0x1F801817)) {
    //    Log::log(std::format("\n~GPU write 0x{:02X} --> @0x{:08X}~\n", byte, address), Log::Type::GPU);
    //}



    //uint8_t *memory = (uint8_t*)resolveAddress(address);
    //*memory = byte;
}

void Bus::writeHalfWord(uint32_t address, uint16_t halfWord) {
    write<uint16_t>(address, halfWord);
    //Log::log(std::format(" [whw -0x{:04X}-> @0x{:08X}]", halfWord, address), Log::Type::MEMORY);


    //if ((address >= 0x1F801070) && (address <= 0x1F801077)) {
    //    Log::log(std::format("\n~Interrupt write 0x{:04X} --> @0x{:08X}~\n", halfWord, address), Log::Type::INTERRUPT);
    //}
    //else if ((address >= 0x1F801100) && (address <= 0x1F80112A)) {
    //    Log::log(std::format("\n~Timer write 0x{:04X} --> @0x{:08X}~\n", halfWord, address), Log::Type::TIMER);
    //} else if ((address >= 0x1F801080) && (address <= 0x1F8010FF)) {
    //    Log::log(std::format("\n~DMA write 0x{:04X} --> @0x{:08X}~\n", halfWord, address), Log::Type::DMA);
    //} else if ((address >= 0x1F801810) && (address <= 0x1F801817)) {
    //    Log::log(std::format("\n~GPU write 0x{:04X} --> @0x{:08X}~\n", halfWord, address), Log::Type::GPU);
    //}


    //uint16_t *memory = (uint16_t*)resolveAddress(address); // PSX is little endian, so is x86
    //*memory = halfWord;
}

void Bus::writeWord(uint32_t address, uint32_t word) {
    write<uint32_t>(address, word);
    //Log::log(std::format(" [ww -0x{:08X}-> @0x{:08X}]", word, address), Log::Type::MEMORY);

    //if ((address >= 0x1F801070) && (address <= 0x1F801077)) {
    //    Log::log(std::format("\n~Interrupt write 0x{:08X} --> @0x{:08X}~\n", word, address), Log::Type::INTERRUPT);
    //}
    //else if ((address >= 0x1F801100) && (address <= 0x1F80112A)) {
    //    Log::log(std::format("\n~Timer write 0x{:08X} --> @0x{:08X}~\n", word, address), Log::Type::TIMER);
    //} else if ((address >= 0x1F801080) && (address <= 0x1F8010FF)) {
    //    Log::log(std::format("\n~DMA write 0x{:08X} --> @0x{:08X}~\n", word, address), Log::Type::DMA);
    //} else if ((address >= 0x1F801810) && (address <= 0x1F801817)) {
    //    Log::log(std::format("\n~GPU write 0x{:08X} --> @0x{:08X}~\n", word, address), Log::Type::GPU);
    //}

    //uint32_t *memory = (uint32_t*)resolveAddress(address); // PSX is little endian, so is x86
    //*memory = word;
}

}
