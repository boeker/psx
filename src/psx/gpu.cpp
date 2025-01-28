#include "gpu.h"

#include <cassert>
#include <format>
#include <sstream>

#include "exceptions/exceptions.h"
#include "util/log.h"

using namespace util;

namespace PSX {

std::ostream& operator<<(std::ostream &os, const GPU &gpu) {
    os << "GPU Status Register:\n";
    uint32_t gpustat = gpu.gpuStatusRegister;

    os << std::format("INTERLACE_EVEN_ODD: {:01b}\n",
                      (gpustat >> GPUSTAT_INTERLACE_EVEN_ODD) & 1);
    os << std::format("DMA_DIRECTION: {:d}\n",
                      (gpustat >> GPUSTAT_DMA_DIRECTION0) & 3);
    os << std::format("DMA_RECEIVE_READY: {:01b}\n",
                      (gpustat >> GPUSTAT_DMA_RECEIVE_READY) & 1);
    os << std::format("VRAM_SEND_READY: {:01b}\n",
                      (gpustat >> GPUSTAT_VRAM_SEND_READY) & 1);
    os << std::format("CMDWORD_RECEIVE_READY: {:01b}\n",
                      (gpustat >> GPUSTAT_CMDWORD_RECEIVE_READY) & 1);
    os << std::format("DATAREQUEST: {:01b}\n",
                      (gpustat >> GPUSTAT_DATAREQUEST) & 1);
    os << std::format("IRQ: {:01b}\n",
                      (gpustat >> GPUSTAT_IRQ) & 1);
    os << std::format("DISPLAY_DISABLE: {:01b}\n",
                      (gpustat >> GPUSTAT_DISPLAY_DISABLE) & 1);
    os << std::format("VERTICAL_INTERLACE: {:01b}\n",
                      (gpustat >> GPUSTAT_VERTICAL_INTERLACE) & 1);
    os << std::format("DISPLAY_AREA_COLOR_DEPTH: {:01b}\n",
                      (gpustat >> GPUSTAT_DISPLAY_AREA_COLOR_DEPTH) & 1);
    os << std::format("VIDEO_MODE: {:01b}\n",
                      (gpustat >> GPUSTAT_VIDEO_MODE) & 1);
    os << std::format("VERTICAL_RESOLUTION: {:01b}\n",
                      (gpustat >> GPUSTAT_VERTICAL_RESOLUTION) & 1);
    os << std::format("HORIZONTAL_RESOLUTION2: {:01b}\n",
                      (gpustat >> GPUSTAT_HORIZONTAL_RESOLUTION2) & 1);
    os << std::format("HORIZONTAL_RESOLUTION1: {:d}\n",
                      (gpustat >> GPUSTAT_HORIZONTAL_RESOLUTION10) & 3);
    os << std::format("TEXTURE_DISABLE: {:01b}\n",
                      (gpustat >> GPUSTAT_TEXTURE_DISABLE) & 1);
    os << std::format("REVERSEFLAG: {:01b}\n",
                      (gpustat >> GPUSTAT_REVERSEFLAG) & 1);
    os << std::format("INTERLACE_FIELD: {:01b}\n",
                      (gpustat >> GPUSTAT_INTERLACE_FIELD) & 1);
    os << std::format("DRAW_PIXELS: {:01b}\n",
                      (gpustat >> GPUSTAT_DRAW_PIXELS) & 1);
    os << std::format("SET_MASK: {:01b}\n",
                      (gpustat >> GPUSTAT_SET_MASK) & 1);
    os << std::format("DRAWING_TO_DISPLAY_AREA_ALLOWED: {:01b}\n",
                      (gpustat >> GPUSTAT_DRAWING_TO_DISPLAY_AREA_ALLOWED) & 1);
    os << std::format("DITHER: {:01b}\n",
                      (gpustat >> GPUSTAT_DITHER) & 1);
    os << std::format("TEXTURE_PAGE_COLORS: {:d}\n",
                      (gpustat >> GPUSTAT_TEXTURE_PAGE_COLORS0) & 3);
    os << std::format("SEMI_TRANSPARENCY: {:d}\n",
                      (gpustat >> GPUSTAT_SEMI_TRANSPARENCY0) & 3);
    os << std::format("TEXTURE_PAGE_Y_BASE: {:01b}\n",
                      (gpustat >> GPUSTAT_TEXTURE_PAGE_Y_BASE) & 1);
    os << std::format("TEXTURE_PAGE_X_BASE: {:d}\n",
                      (gpustat >> GPUSTAT_TEXTURE_PAGE_X_BASE0) & 0xF);

    return os;
}

void GPU::reset() {
    gpuStatusRegister = 0;
}

template <> void GPU::write(uint32_t address, uint32_t value) {
    assert ((address == 0x1F801810) || (address == 0x1F801814));

    Log::log(std::format("GPU write 0x{:08X} -> @0x{:08X}",
                         value, address), Log::Type::GPU);


    if (address == 0x1F801810) { // GP0
        gp0 = value;

        decodeAndExecuteGP0();

    } else { // GP1
        gp1 = value;

        decodeAndExecuteGP1();
    }

    //std::stringstream ss;
    //ss << *this;
    //Log::log(ss.str(), Log::Type::INTERRUPTS);
}

template <> void GPU::write(uint32_t address, uint16_t value) {
    throw exceptions::UnimplementedAddressingError(std::format("half word write @0x{:08X}", address));
}

template <> void GPU::write(uint32_t address, uint8_t value) {
    throw exceptions::UnimplementedAddressingError(std::format("byte write @0x{:08X}", address));
}

template <typename T>
T GPU::read(uint32_t address) {
    assert ((address >= 0x1F801810) && (address < 0x1F801814 + sizeof(T)));

    T value;

    if (address < 0x1F801814) { // GPUREAD
        throw exceptions::UnknownGPUCommandError(std::format("GPUREAD"));
        //assert (address + sizeof(T) <= 0x1F801074);
        //uint32_t offset = address & 0x00000007;
        //assert (offset == 0);

        //value = *((T*)(interruptStatusRegister + offset));

    } else { // GPUSTAT
        assert (address + sizeof(T) <= 0x1F801818);
        uint32_t offset = address & 0x00000003;

        value = *((T*)(((uint8_t*)&gpuStatusRegister) + offset));

        std::stringstream ss;
        ss << *this;
        Log::log(std::format("GPUSTAT -> 0x{:08X}", value), Log::Type::GPU);
        Log::log(ss.str(), Log::Type::GPU);
    }

    Log::log(std::format("GPU read @0x{:08X} -> 0x{:0{}X}",
                         address, value, 2*sizeof(T)), Log::Type::INTERRUPTS);

    return value;
}

template uint32_t GPU::read(uint32_t address);
template uint16_t GPU::read(uint32_t address);
template uint8_t GPU::read(uint32_t address);

void GPU::decodeAndExecuteGP0() {
    uint8_t command = gp0 >> 24;

    if (command == 0xE1) {
        GP0DrawModeSetting();

    } else {
        throw exceptions::UnknownGPUCommandError(std::format("GP0: 0x{:08X}, command 0x{:02X}", gp0, command));
    }

    Log::log("\n", Log::Type::GPU);
}

void GPU::GP0DrawModeSetting() {
    // 0xE1h - Draw Mode setting (aka "Texpage")
    // 0-3   Texture page X Base   (N*64) (ie. in 64-halfword steps)    ;GPUSTAT.0-3
    // 4     Texture page Y Base   (N*256) (ie. 0 or 256)               ;GPUSTAT.4
    // 5-6   Semi Transparency     (0=B/2+F/2, 1=B+F, 2=B-F, 3=B+F/4)   ;GPUSTAT.5-6
    // 7-8   Texture page colors   (0=4bit, 1=8bit, 2=15bit, 3=Reserved);GPUSTAT.7-8
    // 9     Dither 24bit to 15bit (0=Off/strip LSBs, 1=Dither Enabled) ;GPUSTAT.9
    // 10    Drawing to display area (0=Prohibited, 1=Allowed)          ;GPUSTAT.10
    // 11    Texture Disable (0=Normal, 1=Disable if GP1(09h).Bit0=1)   ;GPUSTAT.15
    //         (Above might be chipselect for (absent) second VRAM chip?)
    // 12    Textured Rectangle X-Flip   (BIOS does set this bit on power-up...?)
    // 13    Textured Rectangle Y-Flip   (BIOS does set it equal to GPUSTAT.13...?)
    // 14-23 Not used (should be 0) 

    Log::log(std::format("GP0DrawModeSetting()"), Log::Type::GPU);


    gpuStatusRegister = (gpuStatusRegister & 0xFFFFFC00) | (gp0 & 0x000003FF);
    gpuStatusRegister = (gpuStatusRegister & ~(1 << 15)) | (gp0 & (1 << 11)) << 4;
    texturedRectangleXFlip = (gp0 & (1 << 12));
    texturedRectangleYFlip = (gp0 & (1 << 13));
}

void GPU::decodeAndExecuteGP1() {
    uint8_t command = gp1 >> 24;

    throw exceptions::UnknownGPUCommandError(std::format("GP1: 0x{:08X}, command 0x{:02X}", gp1, command));
}

}

