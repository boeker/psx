#include "gpu.h"

#include <cassert>
#include <format>
#include <sstream>

#include "bus.h"
#include "exceptions/exceptions.h"
#include "util/log.h"

using namespace util;

namespace PSX {

std::ostream& operator<<(std::ostream &os, const CommandQueue &queue) {
    CommandQueue copy = queue;

    if (!copy.isEmpty()) {
        os << std::format("0x{:08X}", copy.pop());
    }

    while (!copy.isEmpty()) {
        os << std::format(", 0x{:08X}", copy.pop());
    }

    return os;
}

CommandQueue::CommandQueue() {
    clear();
}

void CommandQueue::clear() {
    for (int i = 0; i < 16; ++i) {
        queue[i] = 0;
    }

    in = 0;
    out = 0;
    elements = 0;
}

void CommandQueue::push(uint32_t command) {
    if (elements < 16) {
        queue[in] = command;

        in = (in + 1) % 16;
        ++elements;
    }
}

uint32_t CommandQueue::pop() {
    if (elements > 0) {
        uint32_t value = queue[out];

        out = (out + 1) % 16;
        --elements;
        return value;
    }

    return 0;
}

bool CommandQueue::isEmpty() {
    return elements == 0;
}

bool CommandQueue::isFull() {
    return elements == 16;
}

std::ostream& operator<<(std::ostream &os, const GPU &gpu) {
    os << "GPUSTAT: " << gpu.getGPUStatusRegisterExplanation() << std::endl;
    os << "GPUSTAT (cont.): " << gpu.getGPUStatusRegisterExplanation2() << std::endl;
    os << "Command queue: " << gpu.queue;
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

    drawingAreaX1 = 0;
    drawingAreaY1 = 0;

    drawingAreaX2 = 0;
    drawingAreaY2 = 0;
}

void GPU::catchUpToCPU(uint32_t cpuCycles) {
    while (!queue.isEmpty()) {
        gp0 = queue.pop();
        decodeAndExecuteGP0();
    }
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

bool GPU::transferFromGPURequested() {
    return (gpuStatusRegister & (1 << GPUSTAT_VRAM_SEND_READY))
           && (gpuStatusRegister & (1 << GPUSTAT_DMA_DIRECTION1))
           && (gpuStatusRegister & (1 << GPUSTAT_DMA_DIRECTION0))
           && (gpuStatusRegister & (1 << GPUSTAT_DATAREQUEST));
}

bool GPU::transferToGPURequested() {
    return (gpuStatusRegister & (1 << GPUSTAT_DMA_RECEIVE_READY))
           && (gpuStatusRegister & (1 << GPUSTAT_DMA_DIRECTION1))
           && !(gpuStatusRegister & (1 << GPUSTAT_DMA_DIRECTION0))
           && (gpuStatusRegister & (1 << GPUSTAT_DATAREQUEST));
}

void GPU::receiveGP0Command(uint32_t command) {
    Log::log(std::format("Received command 0x{:08X}", command), Log::Type::GPU);

    if (!queue.isFull()) {
        queue.push(command);

        if (queue.isFull()) {
            if ((gpuStatusRegister >> GPUSTAT_DMA_DIRECTION0 & 3) == 1) {
                setGPUStatusRegisterBit(GPUSTAT_CMDWORD_RECEIVE_READY, 0);
            }
        }

    } else {
        Log::log(std::format("Queue is full"), Log::Type::GPU);
    }
}

std::string GPU::getGPUStatusRegisterExplanation() const {
    std::stringstream ss;
    uint32_t gpustat = gpuStatusRegister;

    ss << std::format("INT_EVEN_ODD[{:01b}], ",
                      (gpustat >> GPUSTAT_INTERLACE_EVEN_ODD) & 1);
    ss << std::format("DMA_DIR[{:d}], ",
                      (gpustat >> GPUSTAT_DMA_DIRECTION0) & 3);
    ss << std::format("DMA_REC_RDY[{:01b}], ",
                      (gpustat >> GPUSTAT_DMA_RECEIVE_READY) & 1);
    ss << std::format("VRAM_SEND_RDY[{:01b}], ",
                      (gpustat >> GPUSTAT_VRAM_SEND_READY) & 1);
    ss << std::format("CMDWORD_REC_RDY[{:01b}], ",
                      (gpustat >> GPUSTAT_CMDWORD_RECEIVE_READY) & 1);
    ss << std::format("DATAREQ[{:01b}], ",
                      (gpustat >> GPUSTAT_DATAREQUEST) & 1);
    ss << std::format("IRQ[{:01b}], ",
                      (gpustat >> GPUSTAT_IRQ) & 1);
    ss << std::format("DISP_ENABLE[{:01b}], ",
                      (gpustat >> GPUSTAT_DISPLAY_ENABLE) & 1);
    ss << std::format("V_INT[{:01b}], ",
                      (gpustat >> GPUSTAT_VERTICAL_INTERLACE) & 1);
    ss << std::format("DA_COLOR_DEPTH[{:01b}], ",
                      (gpustat >> GPUSTAT_DISPLAY_AREA_COLOR_DEPTH) & 1);
    ss << std::format("VIDEO_MODE[{:01b}], ",
                      (gpustat >> GPUSTAT_VIDEO_MODE) & 1);
    ss << std::format("V_RES[{:01b}], ",
                      (gpustat >> GPUSTAT_VERTICAL_RESOLUTION) & 1);
    ss << std::format("H_RES2[{:01b}], ",
                      (gpustat >> GPUSTAT_HORIZONTAL_RESOLUTION2) & 1);
    ss << std::format("H_RES1[{:d}]",
                      (gpustat >> GPUSTAT_HORIZONTAL_RESOLUTION10) & 3);

    return ss.str();
}

std::string GPU::getGPUStatusRegisterExplanation2() const {
    std::stringstream ss;
    uint32_t gpustat = gpuStatusRegister;

    ss << std::format("TEX_DISABLE[{:01b}], ",
                      (gpustat >> GPUSTAT_TEXTURE_DISABLE) & 1);
    ss << std::format("REVERSEFLAG[{:01b}], ",
                      (gpustat >> GPUSTAT_REVERSEFLAG) & 1);
    ss << std::format("INT_FIELD[{:01b}], ",
                      (gpustat >> GPUSTAT_INTERLACE_FIELD) & 1);
    ss << std::format("DRAW_PIXELS[{:01b}], ",
                      (gpustat >> GPUSTAT_DRAW_PIXELS) & 1);
    ss << std::format("SET_MASK[{:01b}], ",
                      (gpustat >> GPUSTAT_SET_MASK) & 1);
    ss << std::format("DR_TO_DA_ALLOWED[{:01b}], ",
                      (gpustat >> GPUSTAT_DRAWING_TO_DISPLAY_AREA_ALLOWED) & 1);
    ss << std::format("DITHER[{:01b}], ",
                      (gpustat >> GPUSTAT_DITHER) & 1);
    ss << std::format("TEX_PAGE_COLORS[{:d}], ",
                      (gpustat >> GPUSTAT_TEXTURE_PAGE_COLORS0) & 3);
    ss << std::format("SEMI_TRANS[{:d}], ",
                      (gpustat >> GPUSTAT_SEMI_TRANSPARENCY0) & 3);
    ss << std::format("TEX_PAGE_Y_BASE[{:01b}], ",
                      (gpustat >> GPUSTAT_TEXTURE_PAGE_Y_BASE) & 1);
    ss << std::format("TEX_PAGE_X_BASE[{:d}]",
                      (gpustat >> GPUSTAT_TEXTURE_PAGE_X_BASE0) & 0xF);

    return ss.str();
}

void GPU::setGPUStatusRegisterBit(uint32_t bit, uint32_t value) {
    gpuStatusRegister = (gpuStatusRegister & ~(1 << bit)) | ((value & 1) << bit);
}

void GPU::decodeAndExecuteGP0() {
    uint8_t command = gp0 >> 24;

    (this->*gp0Commands[command])();
}

const GPU::Command GPU::gp0Commands[] = {
    // 0x00
    &GPU::GP0NOP,
    // 0x01
    &GPU::GP0ClearCache,
    &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    // 0x10
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    // 0x20
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    // 0x28
    &GPU::GP0MonochromeFourPointPolygonOpaque,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    // 0x30
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    // 0x40
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    // 0x50
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    // 0x60
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    // 0x70
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    // 0x80
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    // 0x90
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    // 0xA0
    &GPU::GP0CopyRectangleToVRAM,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    // 0xB0
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    // 0xC0
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    // 0xD0
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    // 0xE0
    &GPU::GP0Unknown,
    // 0xE1
    &GPU::GP0DrawModeSetting,
    // 0xE2
    &GPU::GP0TextureWindowSetting,
    // 0xE3
    &GPU::GP0SetDrawingAreaTopLeft,
    // 0xE4
    &GPU::GP0SetDrawingAreaBottomRight,
    // 0xE5
    &GPU::GP0SetDrawingOffset,
    // 0xE6
    &GPU::GP0MaskBitSetting,
    &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    // 0xF0
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
};

void GPU::GP0Unknown() {
    uint8_t command = gp0 >> 24;
    throw exceptions::UnknownGPUCommandError(std::format("GP0: 0x{:08X}, command 0x{:02X}", gp0, command));
}

void GPU::GP0NOP() {
    Log::log(std::format("GP0 - NOP"), Log::Type::GPU);
}

void GPU::GP0ClearCache() {
    Log::log(std::format("GP0 - ClearCache"), Log::Type::GPU);
    // TODO Implement?
}

void GPU::GP0CopyRectangleToVRAM() {
    uint32_t destinationCoord = queue.pop();
    uint32_t widthAndHeight = queue.pop();
    Log::log(std::format("GP0 - CopyRectangleToVRAM(0x{:08X}, 0x{:08X}, ...)",
                         destinationCoord, widthAndHeight), Log::Type::GPU);

    // read data from GPUREAD
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

void GPU::GP0TextureWindowSetting() {
    // 0xE2
    this->textureWindowMaskX = gp0 & 0x1F;
    this->textureWindowMaskY = (gp0 >> 5) & 0x1F;
    this->textureWindowOffsetX = (gp0 >> 10) & 0x1F;
    this->textureWindowOffsetY = (gp0 >> 15) & 0x1F;

    Log::log(std::format("GP0 - TextureWindowSetting({:d}, {:d}, {:d}, {:d})",
                         this->textureWindowMaskX,
                         this->textureWindowMaskY,
                         this->textureWindowOffsetX,
                         this->textureWindowOffsetY), Log::Type::GPU);
}

void GPU::GP0SetDrawingAreaTopLeft() {
    // 0xE3
    uint32_t xCoord = gp0 & 0x3FF;
    uint32_t yCoord = (gp0 >> 10) & 0x1FF;

    Log::log(std::format("GP0 - SetDrawingAreaTopLeft({:d}, {:d})", xCoord, yCoord), Log::Type::GPU);


    drawingAreaX1 = xCoord;
    drawingAreaY1 = yCoord;
}

void GPU::GP0SetDrawingAreaBottomRight() {
    // 0xE4
    uint32_t xCoord = gp0 & 0x3FF;
    uint32_t yCoord = (gp0 >> 10) & 0x1FF;

    Log::log(std::format("GP0 - SetDrawingAreaBottomRight({:d}, {:d})", xCoord, yCoord), Log::Type::GPU);

    drawingAreaX2 = xCoord;
    drawingAreaY2 = yCoord;
}

void GPU::GP0SetDrawingOffset() {
    // 0xE5
    uint32_t xOffset = gp0 & 0x7FF;
    uint32_t yOffset = (gp0 >> 11) & 0x7FF;

    int32_t signedXOffset = ((xOffset >> 10) ? 0xFFFFF800 : 0x00000000) | xOffset;
    int32_t signedYOffset = ((yOffset >> 10) ? 0xFFFFF800 : 0x00000000) | yOffset;

    Log::log(std::format("GP0 - SetDrawingOffset({:d}, {:d})", signedXOffset, signedYOffset), Log::Type::GPU);
    drawingOffsetX = signedXOffset;
    drawingOffsetY = signedYOffset;
}

void GPU::GP0MaskBitSetting() {
    // 0xE6
    uint8_t setMaskWhileDrawing = gp0 & 1;
    uint8_t checkMaskBeforeDraw = (gp0 >> 1) & 1;

    Log::log(std::format("GP0 - MaskBitSetting({:01b}, {:01b})",
                         setMaskWhileDrawing,
                         checkMaskBeforeDraw), Log::Type::GPU);

    setGPUStatusRegisterBit(GPUSTAT_SET_MASK, setMaskWhileDrawing);
    setGPUStatusRegisterBit(GPUSTAT_DRAW_PIXELS, checkMaskBeforeDraw);
}

void GPU::GP0MonochromeFourPointPolygonOpaque() {
    // 0x28
    uint32_t color = gp0 & 0x00FFFFFF;

    // TODO Check that there are at least four elements in the queue
    uint32_t vertex1 = queue.pop();
    uint32_t vertex2 = queue.pop();
    uint32_t vertex3 = queue.pop();
    uint32_t vertex4 = queue.pop();

    Log::log(std::format("GP0 - MonochromeFourPointPolygonOpaque(0x{:06X}, 0x{:08X}, 0x{:08X}, 0x{:08X}, 0x{:08X})",
                         color,
                         vertex1, vertex2, vertex3, vertex4), Log::Type::GPU);

    // TODO Render
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

    Log::log(std::format("GPUSTAT: {:s}", getGPUStatusRegisterExplanation()), Log::Type::GPU);
    Log::log(std::format("GPUSTAT: {:s}", getGPUStatusRegisterExplanation2()), Log::Type::GPU);
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

    Log::log(std::format("GPUSTAT: 0x{:08X}", gpuStatusRegister), Log::Type::GPU);
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

    uint32_t direction = parameter & 3;
    gpuStatusRegister = (gpuStatusRegister & ~(3 << GPUSTAT_DMA_DIRECTION0))
                        | ((direction) << GPUSTAT_DMA_DIRECTION0);

    uint32_t dataRequestBit = 0;
    switch (direction) {
        case 0:
            dataRequestBit = 0;
            break;
        case 1:
            setGPUStatusRegisterBit(GPUSTAT_DATAREQUEST, (queue.isFull() ? 0 : 1));
            dataRequestBit = queue.isFull() ? 0 : 1;
            break;
        case 2:
            dataRequestBit = (gpuStatusRegister >> GPUSTAT_DMA_RECEIVE_READY) & 1;
            break;
        case 3:
            dataRequestBit = (gpuStatusRegister >> GPUSTAT_VRAM_SEND_READY) & 1;
            break;
        default:
            break;
    }

    setGPUStatusRegisterBit(GPUSTAT_DATAREQUEST, dataRequestBit);
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

