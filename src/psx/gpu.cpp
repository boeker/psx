#include "gpu.h"

#include <cassert>
#include <format>
#include <sstream>

#include "bus.h"
#include "exceptions/exceptions.h"
#include "util/log.h"

using namespace util;

namespace PSX {

CommandQueue::CommandQueue() {
    clear();
}

void CommandQueue::clear() {
    for (int i = 0; i < 16; ++i) {
        queue[i] = 0;
    }

    in = 0;
    out = 0;
}

void CommandQueue::push(uint32_t command) {
    uint8_t next = (in + 1) % 16;

    if (next != out) {
        in = next;
        queue[in] = command;
    }
}

uint32_t CommandQueue::pop() {
    if (out != in) {
        uint32_t value = queue[out];

        out = (out + 1) % 16;
        return value;
    }

    return 0;
}

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
    os << std::format("DISPLAY_ENABLE: {:01b}\n",
                      (gpustat >> GPUSTAT_DISPLAY_ENABLE) & 1);
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

GPU::GPU(Bus *bus) {
    this->bus = bus;

    reset();
}

void GPU::reset() {
    gp0 = 0;
    queue.clear();
    gpuReadResponse = 0;

    gp1 = 0;
    gpuStatusRegister = 0x144E200D;

    texturedRectangleXFlip = false;
    texturedRectangleYFlip = false;
}

template <> void GPU::write(uint32_t address, uint32_t value) {
    assert ((address == 0x1F801810) || (address == 0x1F801814));

    Log::log(std::format("GPU write 0x{:08X} -> @0x{:08X}",
                         value, address), Log::Type::GPU_IO);

    if (address == 0x1F801810) { // GP0
        gp0 = value;

        decodeAndExecuteGP0();

    } else { // GP1
        gp1 = value;

        decodeAndExecuteGP1();
    }
}

template <> void GPU::write(uint32_t address, uint16_t value) {
    throw exceptions::UnimplementedAddressingError(std::format("half-word write @0x{:08X}", address));
}

template <> void GPU::write(uint32_t address, uint8_t value) {
    throw exceptions::UnimplementedAddressingError(std::format("byte write @0x{:08X}", address));
}

template <>
uint32_t GPU::read(uint32_t address) {
    assert ((address == 0x1F801810) || (address == 0x1F801814));

    if (address == 0x1F801810) { // GPUREAD
        Log::log(std::format("GPUREAD -> 0x{:08X}", gpuReadResponse), Log::Type::GPU_IO);

        return gpuReadResponse;

    } else { // GPUSTAT
        assert (address == 0x1F801814);

        Log::log(std::format("GPUSTAT -> 0x{:08X}", gpuStatusRegister), Log::Type::GPU_IO);

        return gpuStatusRegister;

    }

}

template <> uint16_t GPU::read(uint32_t address) {
    throw exceptions::UnimplementedAddressingError(std::format("half-word read @0x{:08X}", address));
}

template <> uint8_t GPU::read(uint32_t address) {
    throw exceptions::UnimplementedAddressingError(std::format("byte read @0x{:08X}", address));
}

void GPU::setGPUStatusRegisterBit(uint32_t bit, uint32_t value) {
    gpuStatusRegister = (gpuStatusRegister & ~(1 << bit)) | ((value & 1) << bit);
}

void GPU::decodeAndExecuteGP0() {
    uint8_t command = gp0 >> 24;

    if (command == 0x00) {
        GP0NOP();

    } else if (command == 0xE1) {
        GP0DrawModeSetting();

    } else {
        throw exceptions::UnknownGPUCommandError(std::format("GP0: 0x{:08X}, command 0x{:02X}", gp0, command));
    }
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

    Log::log(std::format("GP0 - DrawModeSetting"), Log::Type::GPU);


    gpuStatusRegister = (gpuStatusRegister & 0xFFFFFC00) | (gp0 & 0x000003FF);
    gpuStatusRegister = (gpuStatusRegister & ~(1 << 15)) | ((gp0 & (1 << 11)) << 4);
    texturedRectangleXFlip = (gp0 & (1 << 12));
    texturedRectangleYFlip = (gp0 & (1 << 13));
}

void GPU::GP0NOP() {
    Log::log(std::format("GP0 - NOP"), Log::Type::GPU);
}

void GPU::decodeAndExecuteGP1() {
    uint8_t command = gp1 >> 24;

    if (command == 0x00) {
        GP1ResetGPU();

    } else if (command == 0x01) {
        GP1ResetCommandBuffer();

    } else if (command == 0x02) {
        GP1AcknowledgeGPUInterrupt();

    } else if (command == 0x03) {
        GP1DisplayEnable();

    } else if (command == 0x04) {
        GP1DMADirection();

    } else if (command == 0x05) {
        GP1StartOfDisplayArea();

    } else if (command == 0x06) {
        GP1HorizontalDisplayRange();

    } else if (command == 0x07) {
        GP1VerticalDisplayRange();

    } else if (command == 0x08) {
        GP1DisplayMode();

    } else if (command == 0x09) {
        GP1NewTextureDisable();

    } else {
        throw exceptions::UnknownGPUCommandError(std::format("GP1: 0x{:08X}, command 0x{:02X}", gp1, command));
    }
}

void GPU::GP1ResetGPU() {
    // 0x00
    Log::log(std::format("GP1 - ResetGPU"), Log::Type::GPU);

    queue.clear();
    gpuStatusRegister = gpuStatusRegister & ~(1 << GPUSTAT_IRQ);
    setGPUStatusRegisterBit(GPUSTAT_DISPLAY_ENABLE, 1);
    gpuStatusRegister = gpuStatusRegister & ~(3 << GPUSTAT_DMA_DIRECTION0);
    startOfDisplayAreaX = 0;
    startOfDisplayAreaY = 0;
    horizontalDisplayRangeX1 = 0x200;
    horizontalDisplayRangeX2 = 0x200 + 256 * 10;
    verticalDisplayRangeY1 = 0x010;
    verticalDisplayRangeY2 = 0x010 + 0x010 + 240;
    setGPUStatusRegisterBit(17, 0);
    setGPUStatusRegisterBit(18, 0);
    setGPUStatusRegisterBit(19, 0);
    setGPUStatusRegisterBit(20, 0);
    setGPUStatusRegisterBit(21, 0);
    setGPUStatusRegisterBit(22, 0);
    setGPUStatusRegisterBit(16, 0);
    setGPUStatusRegisterBit(14, 0);

    // TODO GP0(E1...E6)
}

void GPU::GP1ResetCommandBuffer() {
    // 0x01
    Log::log(std::format("GP1 - ResetCommandBuffer"), Log::Type::GPU);
    queue.clear();
}

void GPU::GP1AcknowledgeGPUInterrupt() {
    // 0x02
    Log::log(std::format("GP1 - AcknowledgeGPUInterrupt"), Log::Type::GPU);

    gpuStatusRegister = gpuStatusRegister & ~(1 << GPUSTAT_IRQ);
}

void GPU::GP1DisplayEnable() {
    // 0x03
    uint32_t parameter = gp1 & 0x00FFFFFF;

    Log::log(std::format("GP1 - DisplayEnable (0x{:06X})", parameter), Log::Type::GPU);
    
    setGPUStatusRegisterBit(GPUSTAT_DISPLAY_ENABLE, parameter & 1);
}

void GPU::GP1DMADirection() {
    // 0x04
    uint32_t parameter = gp1 & 0x00FFFFFF;

    Log::log(std::format("GP1 - DMADirection(0x{:06X})", parameter), Log::Type::GPU);

    gpuStatusRegister = (gpuStatusRegister & ~(3 << GPUSTAT_DMA_DIRECTION0))
                        | ((parameter & 3) << GPUSTAT_DMA_DIRECTION0);
}

void GPU::GP1StartOfDisplayArea() {
    // 0x05
    uint32_t parameter = gp1 & 0x00FFFFFF;

    Log::log(std::format("GP1 - StartOfDisplayArena(0x{:06X})", parameter), Log::Type::GPU);
    startOfDisplayAreaX = parameter & 0x000003FF;
    startOfDisplayAreaY = parameter & 0x0007FC00;
}

void GPU::GP1HorizontalDisplayRange() {
    // 0x06
    uint32_t parameter = gp1 & 0x00FFFFFF;

    Log::log(std::format("GP1 - HorizontalDisplayRange(0x{:06X})", parameter), Log::Type::GPU);
    horizontalDisplayRangeX1 = parameter & 0x00000FFF;
    horizontalDisplayRangeX2 = parameter & 0x00FFF000;
}

void GPU::GP1VerticalDisplayRange() {
    // 0x07
    uint32_t parameter = gp1 & 0x00FFFFFF;

    Log::log(std::format("GP1 - VerticalDisplayRange(0x{:06X})", parameter), Log::Type::GPU);
    verticalDisplayRangeY1 = parameter & 0x000003FF;
    verticalDisplayRangeY2 = parameter & 0x0003FC00;
}

void GPU::GP1DisplayMode() {
    // 0x08
    // parameter bits 0...5 are GPUSTAT bits 17...22
    // parameter bit 6 is GPUSTAT bit 16
    // parameter bit 5 is GPUSTAT bit 14
    uint32_t parameter = gp1 & 0x00FFFFFF;

    Log::log(std::format("GP1 - DisplayMode(0x{:06X})", parameter), Log::Type::GPU);

    gpuStatusRegister = (gpuStatusRegister & 0xFFFFFC00) | (gp0 & 0x000003FF);
    setGPUStatusRegisterBit(16 , (parameter >> 6) & 1);
    setGPUStatusRegisterBit(14 , (parameter >> 5) & 1);
}

void GPU::GP1NewTextureDisable() {
    // 0x09
    uint32_t parameter = gp1 & 0x00FFFFFF;

    Log::log(std::format("GP1 - NewTextureDisable(0x{:06X})", parameter), Log::Type::GPU);

    setGPUStatusRegisterBit(GPUSTAT_TEXTURE_DISABLE, parameter & 1);
}

void GPU::GP1GetGPUInfo() {
    // 0x10...0x1F
}

}

