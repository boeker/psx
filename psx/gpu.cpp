#include "gpu.h"

#include <cassert>
#include <format>
#include <sstream>

#include "bus.h"
#include "renderer/renderer.h"
#include "exceptions/exceptions.h"
#include "util/bit.h"
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

    throw std::runtime_error("Queue is empty");

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
    this->renderer = nullptr;

    reset();
}

GPU::~GPU() {
}

void GPU::reset() {
    state = State::IDLE;

    gp0 = 0;
    queue.clear();
    gpuReadResponse = 0;

    gp1 = 0;
    gpuStatusRegister = 0x144E200D;

    texturedRectangleXFlip = false;
    texturedRectangleYFlip = false;

    //if (renderer) {
    //    renderer->clear();
    //    renderer->swapBuffers();
    //}
}

void GPU::setRenderer(Renderer *renderer) {
    this->renderer = renderer;
}

void GPU::catchUpToCPU(uint32_t cpuCycles) {
    while (true) {
        switch (state) {
            case State::IDLE:
                if (!queue.isEmpty()) {
                    gp0 = queue.pop();
                    gp0Command = gp0 >> 24;

                    gp0Parameters.clear();
                    neededParams = gp0ParameterNumbers[gp0Command];
                    state = WAITING_FOR_GP0_PARAMS;

                } else {
                    return;
                }
                break;

            case State::WAITING_FOR_GP0_PARAMS:
                while (gp0Parameters.size() < neededParams
                       && !queue.isEmpty()) {
                    gp0Parameters.push_back(queue.pop());
                }
                if (gp0Parameters.size() == neededParams) {
                    state = EXECUTING_CP0;

                } else {
                    return;
                }
                break;

            case State::EXECUTING_CP0:
                state = State::IDLE;
                (this->*gp0Commands[gp0Command])();
                break;

            case State::TRANSFER_TO_CPU:
            case State::TRANSFER_TO_VRAM:
                return;
                break;
        }
    }
}

void GPU::receiveGP0Data(uint32_t word) {
    LOGT_GPU(std::format("Received word 0x{:08X}", word));

    switch (state) {
        case State::TRANSFER_TO_VRAM:
            LOGT_GPU(std::format("To VRAM: Remaining words: {:d}", transferToVRAMRemainingWords));

            renderer->writeToVRAM(destinationCurrentY, destinationCurrentX, (uint16_t)(word & 0x0000FFFF));
            advanceCurrentDestinationPosition();
            renderer->writeToVRAM(destinationCurrentY, destinationCurrentX, (uint16_t)(word >> 16));
            advanceCurrentDestinationPosition();

            //toVRAMBuffer.push_back((word >> 24) & 0xFF);
            //toVRAMBuffer.push_back((word >> 16) & 0xFF);
            //toVRAMBuffer.push_back((word >> 8) & 0xFF);
            //toVRAMBuffer.push_back(word & 0xFF);

            toVRAMBuffer.push_back(((word >> 0) & 0x1F) << 3);
            toVRAMBuffer.push_back(((word >> 5) & 0x1F) << 3);
            toVRAMBuffer.push_back(((word >> 10) & 0x1F) << 3);

            toVRAMBuffer.push_back(((word >> 16) & 0x1F) << 3);
            toVRAMBuffer.push_back(((word >> 21) & 0x1F) << 3);
            toVRAMBuffer.push_back(((word >> 26) & 0x1F) << 3);


            //toVRAMBuffer.push_back(((word >> 16) & 0xF) << 4);
            //toVRAMBuffer.push_back(((word >> 12) & 0xF) << 4);
            //toVRAMBuffer.push_back(((word >> 8) & 0xF) << 4);
            //toVRAMBuffer.push_back(((word >> 4) & 0xF) << 4);
            //toVRAMBuffer.push_back(((word >> 0) & 0xF) << 4);

            //toVRAMBuffer.push_back((word >> 16) & 0xFF);
            //toVRAMBuffer.push_back((word >> 8) & 0xFF);
            //toVRAMBuffer.push_back(word & 0xFF);

            //toVRAMBuffer.push_back((word >> 24) & 0xF);
            //toVRAMBuffer.push_back((word >> 20) & 0xF);
            //toVRAMBuffer.push_back((word >> 16) & 0xF);
            ////toVRAMBuffer.push_back((word >> 16) & 0xF);
            //toVRAMBuffer.push_back((word >> 12) & 0xF);
            //toVRAMBuffer.push_back((word >> 8) & 0xF);
            //toVRAMBuffer.push_back((word >> 4) & 0xF);
            //toVRAMBuffer.push_back((word & 0x0000FFFF) & 0xF);

            transferToVRAMRemainingWords--;

            if (transferToVRAMRemainingWords == 0) {
                renderer->writeToVRAM(destinationX, destinationY, destinationSizeX, destinationSizeY, &toVRAMBuffer[0]);
                state = State::IDLE;
                LOG_GPU(std::format("State::IDLE"));
            }
            break;

        default:
            if (!queue.isFull()) {
                queue.push(word);

                if (queue.isFull()) {
                    if ((gpuStatusRegister >> GPUSTAT_DMA_DIRECTION0 & 3) == 1) {
                        setGPUStatusRegisterBit(GPUSTAT_CMDWORD_RECEIVE_READY, 0);
                    }
                }

            } else {
                LOG_GPU(std::format("Queue is full"));
            }
            break;
    }

}

uint32_t GPU::sendGP0Data() {
    LOGT_GPU(std::format("Sending word"));

    uint32_t value1, value2;

    switch (state) {
        case State::TRANSFER_TO_CPU:
            LOGT_GPU(std::format("To CPU: Remaining words: {:d}", transferToCPURemainingWords));
            transferToCPURemainingWords--;

            if (transferToCPURemainingWords == 0) {
                state = State::IDLE;
                setGPUStatusRegisterBit(GPUSTAT_VRAM_SEND_READY, 0);
                LOG_GPU(std::format("State::IDLE"));
            }

            value2 = renderer->readFromVRAM(sourceCurrentY, sourceCurrentX);
            advanceCurrentSourcePosition();
            value1 = renderer->readFromVRAM(sourceCurrentY, sourceCurrentX);
            advanceCurrentSourcePosition();

            return (value1 << 16) | value2;

        default:
            return 0;
    }
}

void GPU::notifyAboutVBLANK() {
    LOGT_GPU(std::format("VBLANK"));
    renderer->swapBuffers();
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

    LOG_GPU(std::format("GPUSTAT: {:s}", getGPUStatusRegisterExplanation()));
    LOG_GPU(std::format("GPUSTAT: {:s}", getGPUStatusRegisterExplanation2()));
}

void GPU::advanceCurrentDestinationPosition() {
    ++destinationCurrentX;
    if (destinationCurrentX >= destinationX + destinationSizeX) {
        ++destinationCurrentY;
        destinationCurrentX = destinationX;
    }
}

void GPU::advanceCurrentSourcePosition() {
    ++sourceCurrentX;
    if (sourceCurrentX >= sourceX + sourceSizeX) {
        ++sourceCurrentY;
        sourceCurrentX = sourceX;
    }
}

template <> void GPU::write(uint32_t address, uint32_t value) {
    assert ((address == 0x1F801810) || (address == 0x1F801814));

    LOGT_GPU(std::format("GPU write 0x{:08X} -> @0x{:08X}",
                         value, address));

    if (address == 0x1F801810) { // GP0
        receiveGP0Data(value);

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
        uint32_t word = sendGP0Data();

        LOGT_GPU(std::format("GPUREAD -> 0x{:08X}", word));

        return word;

    } else { // GPUSTAT
        assert (address == 0x1F801814);

        LOGT_GPU(std::format("GPUSTAT -> 0x{:08X}", gpuStatusRegister));

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

uint8_t* GPU::decodeTexture(uint16_t texpage, uint16_t palette) {
    LOG_GPU(std::format("Decoding texture"));
    decodedTexture.clear();

    uint32_t xBase = (texpage & 0xF) * 64; // in halfwords
    uint32_t yBase = ((texpage >> 4) & 1) * 256; // in lines
    uint8_t semiTransparency = (texpage >> 5) & 3;
    uint8_t texturePageColors = (texpage >> 7) & 3;
    uint8_t textureDisable = (texpage >> 11) & 1;

    uint32_t xPalette = (palette & 0x3F) * 16; // in halfwords
    uint32_t yPalette = (palette >> 6) & 0x1FF; // in lines

    LOG_GPU(std::format("XBase[{:d}], YBase[{:d}], Semi Transparency[{:d}], Texture Page Colors[{:d}], Texture Disable[{:d}], XPalette[{:d}], YPalette[{:d}]", xBase, yBase, semiTransparency, texturePageColors, textureDisable, xPalette, yPalette));

    if (texturePageColors == 0) { // 4-bit colors
        uint8_t colors[64];
        for (uint32_t x = 0; x < 16; ++x) {
            uint16_t halfword = renderer->readFromVRAM(yPalette, xPalette + x);
            uint8_t r = halfword & 0x1F;
            uint8_t g = (halfword >> 5) & 0x1F;
            uint8_t b = (halfword >> 10) & 0x1F;
            uint8_t semiTransparency = (halfword >> 15) & 1;

            colors[4*x+0] = r << 3;
            colors[4*x+1] = g << 3;
            colors[4*x+2] = b << 3;

            colors[4*x+3] = 255;
            if (semiTransparency == 0 && r == 0 & g == 0 & b == 0) {
                colors[4*x+3] = 0;
            }
        }

        for (uint32_t y = yBase; y < yBase + 256; ++y) {
            for (uint32_t x = xBase; x < xBase + 64; ++x) {
                uint16_t halfword = renderer->readFromVRAM(y, x);

                uint8_t p1 = halfword & 0xF;
                uint8_t p2 = (halfword >> 4) & 0xF;
                uint8_t p3 = (halfword >> 8) & 0xF;
                uint8_t p4 = (halfword >> 12) & 0xF;

                decodedTexture.push_back(colors[4*p1+0]);
                decodedTexture.push_back(colors[4*p1+1]);
                decodedTexture.push_back(colors[4*p1+2]);
                decodedTexture.push_back(colors[4*p1+3]);

                decodedTexture.push_back(colors[4*p2+0]);
                decodedTexture.push_back(colors[4*p2+1]);
                decodedTexture.push_back(colors[4*p2+2]);
                decodedTexture.push_back(colors[4*p2+3]);

                decodedTexture.push_back(colors[4*p3+0]);
                decodedTexture.push_back(colors[4*p3+1]);
                decodedTexture.push_back(colors[4*p3+2]);
                decodedTexture.push_back(colors[4*p3+3]);

                decodedTexture.push_back(colors[4*p4+0]);
                decodedTexture.push_back(colors[4*p4+1]);
                decodedTexture.push_back(colors[4*p4+2]);
                decodedTexture.push_back(colors[4*p4+3]);
            }
        }

    } else {
        LOG_GPU(std::format("Texture format not implemented"));
    }

    LOG_GPU(std::format("Decoded texture size: {:d} bytes", decodedTexture.size()));

    if (decodedTexture.size() > 0) {
        return &decodedTexture[0];

    } else {
        return nullptr;
    }
}

const GPU::Command GPU::gp0Commands[] = {
    // 0x00
    &GPU::GP0NOP,
    // 0x01
    &GPU::GP0ClearCache,
    // 0x02
    &GPU::GP0FillRectangleInVRAM,
    &GPU::GP0Unknown,
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
    // 0x2C
    &GPU::GP0TexturedFourPointPolygonOpaqueTextureBlending,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    // 0x30
    &GPU::GP0ShadedThreePointPolygonOpaque,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
    // 0x38
    &GPU::GP0ShadedFourPointPolygonOpaque,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
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
    &GPU::GP0CopyRectangleVRAMToCPU,
    &GPU::GP0Unknown, &GPU::GP0Unknown, &GPU::GP0Unknown,
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

const uint8_t GPU::gp0ParameterNumbers[] = {
    // 0x00
    0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x10
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x20
    0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 8, 0, 0, 0,
    // 0x30
    5, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0,
    // 0x40
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x50
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x60
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x70
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x80
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x90
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0xA0
    2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0xB0
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0xC0
    2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0xD0
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0xE0
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0xF0
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

void GPU::GP0Unknown() {
    uint8_t command = gp0 >> 24;
    throw exceptions::UnknownGPUCommandError(std::format("GP0: 0x{:08X}, command 0x{:02X}", gp0, command));
}

void GPU::GP0NOP() {
    LOG_GPU(std::format("GP0 - NOP"));
}

void GPU::GP0ClearCache() {
    // 0x01
    LOG_GPU(std::format("GP0 - ClearCache"));
    // TODO Implement?
}

void GPU::GP0FillRectangleInVRAM() {
    // 0x02
    Color c(gp0);

    uint32_t topLeft = gp0Parameters[0];
    uint32_t topLeftX = topLeft & 0xFFFF;
    uint32_t topLeftY = (topLeft >> 16) & 0xFFFF;

    uint32_t widthAndHeight = gp0Parameters[1];
    uint32_t width = widthAndHeight & 0xFFFF;
    uint32_t height = (widthAndHeight >> 16) & 0xFFFF;

    LOG_GPU(std::format("GP0 - FillRectangleInVRAM({}, {}, {}, {}, {})",
                        c, topLeftX, topLeftY, width, height));

    renderer->fillRectangleInVRAM(c, topLeftX, topLeftY, width, height);
}

void GPU::GP0MonochromeFourPointPolygonOpaque() {
    // 0x28
    Color c(gp0);

    Vertex v1(gp0Parameters[0]);
    Vertex v2(gp0Parameters[1]);
    Vertex v3(gp0Parameters[2]);
    Vertex v4(gp0Parameters[3]);

    LOG_GPU(std::format("GP0 - MonochromeFourPointPolygonOpaque({}, {}, {}, {}, {})",
                        c, v1, v2, v3, v4));

    Triangle t(v1, c, v2, c, v3, c);
    Triangle t2(v2, c, v3, c, v4, c);

    renderer->drawTriangle(t);
    renderer->drawTriangle(t2);
}

void GPU::GP0TexturedFourPointPolygonOpaqueTextureBlending() {
    // 0x2C
    Color c(gp0);

    Vertex v1(gp0Parameters[0]);
    Vertex v2(gp0Parameters[2]);
    Vertex v3(gp0Parameters[4]);
    Vertex v4(gp0Parameters[6]);

    uint16_t palette = gp0Parameters[1] >> 16;
    uint16_t texpage = gp0Parameters[3] >> 16;

    TextureCoordinate tc1(gp0Parameters[1] & 0xFFFF);
    TextureCoordinate tc2(gp0Parameters[3] & 0xFFFF);
    TextureCoordinate tc3(gp0Parameters[5] & 0xFFFF);
    TextureCoordinate tc4(gp0Parameters[7] & 0xFFFF);

    LOG_GPU(std::format("GP0 - TexturedFourPointPolygonOpaqueTextureBlending(0x{:04X}, 0x{:04X}, {}, {}, {}, {}, {}, {}, {}, {}, {})",
                         palette, texpage, c, v1, tc1, v2, tc2, v3, tc3, v4, tc4));

    uint8_t *texture = decodeTexture(texpage, palette);
    renderer->loadTexture(texture);

    TexturedTriangle t(c, v1, tc1, v2, tc2, v3, tc3);
    TexturedTriangle t2(c, v2, tc2, v3, tc3, v4, tc4);

    renderer->drawTexturedTriangle(t);
    renderer->drawTexturedTriangle(t2);
}

void GPU::GP0ShadedThreePointPolygonOpaque() {
    // 0x30
    Color c1(gp0);
    Color c2(gp0Parameters[1]);
    Color c3(gp0Parameters[3]);

    Vertex v1(gp0Parameters[0]);
    Vertex v2(gp0Parameters[2]);
    Vertex v3(gp0Parameters[4]);

    LOG_GPU(std::format("GP0 - ShadedThreePointPolygonOpaque({}, {}, {}, {}, {}, {})",
                         c1, v1, c2, v2, c3, v3));

    Triangle t(v1, c1, v2, c2, v3, c3);
    renderer->drawTriangle(t);
}

void GPU::GP0ShadedFourPointPolygonOpaque() {
    // 0x38
    Color c1(gp0);
    Color c2(gp0Parameters[1]);
    Color c3(gp0Parameters[3]);
    Color c4(gp0Parameters[5]);

    Vertex v1(gp0Parameters[0]);
    Vertex v2(gp0Parameters[2]);
    Vertex v3(gp0Parameters[4]);
    Vertex v4(gp0Parameters[6]);

    LOG_GPU(std::format("GP0 - ShadedFourPointPolygonOpaque({}, {}, {}, {}, {}, {}, {}, {})",
                         c1, v1, c2, v2, c3, v3, c4, v4));

    Triangle t(v1, c1, v2, c2, v3, c3);
    Triangle t2(v2, c2, v3, c3, v4, c4);

    renderer->drawTriangle(t);
    renderer->drawTriangle(t2);
}


void GPU::GP0CopyRectangleToVRAM() {
    // 0xA0
    uint32_t destinationCoord = gp0Parameters[0];
    uint32_t widthAndHeight = gp0Parameters[1];

    destinationX = destinationCoord & 0x0000FFFF;
    destinationY = destinationCoord >> 16;

    // counted in half words
    destinationSizeX = widthAndHeight & 0x0000FFFF;
    destinationSizeY  = widthAndHeight >> 16;

    LOG_GPU(std::format("GP0 - CopyRectangleToVRAM({:d}, {:d}, {:d}x{:d})",
                         destinationX, destinationY, destinationSizeX, destinationSizeY));

    // read data from GPUREAD
    transferToVRAMRemainingWords = (destinationSizeX * destinationSizeY) / 2;
    if (transferToVRAMRemainingWords > 0) {
        state = State::TRANSFER_TO_VRAM;
        LOG_GPU(std::format("State::TRANSFER_TO_VRAM"));
        destinationCurrentX = destinationX;
        destinationCurrentY = destinationY;
        toVRAMBuffer.clear();
    }
}

void GPU::GP0CopyRectangleVRAMToCPU() {
    // 0xC0
    uint32_t sourceCoord = gp0Parameters[0];
    uint32_t widthAndHeight = gp0Parameters[1];

    sourceX = sourceCoord & 0x0000FFFF;
    sourceY = sourceCoord >> 16;

    // counted in half words
    sourceSizeX = widthAndHeight & 0x0000FFFF;
    sourceSizeY = widthAndHeight >> 16;

    LOG_GPU(std::format("GP0 - CopyRectangleVRAMToCPU({:d}, {:d}, {:d}x{:d})",
                         sourceX, sourceY, sourceSizeX, sourceSizeY));

    // write data to GPUREAD
    transferToCPURemainingWords = (sourceSizeX * sourceSizeY) / 2;
    if (transferToCPURemainingWords > 0) {
        state = State::TRANSFER_TO_CPU;
        setGPUStatusRegisterBit(GPUSTAT_VRAM_SEND_READY, 1);
        LOG_GPU(std::format("State::TRANSFER_TO_CPU"));
        sourceCurrentX = sourceX;
        sourceCurrentY = sourceY;
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

    LOG_GPU(std::format("GP0 - DrawModeSetting"));


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

    LOG_GPU(std::format("GP0 - TextureWindowSetting({:d}, {:d}, {:d}, {:d})",
                         this->textureWindowMaskX,
                         this->textureWindowMaskY,
                         this->textureWindowOffsetX,
                         this->textureWindowOffsetY));
}

void GPU::GP0SetDrawingAreaTopLeft() {
    // 0xE3
    uint32_t xCoord = gp0 & 0x3FF;
    uint32_t yCoord = (gp0 >> 10) & 0x1FF;

    LOG_GPU(std::format("GP0 - SetDrawingAreaTopLeft({:d}, {:d})", xCoord, yCoord));


    renderer->setDrawingAreaTopLeft(xCoord, yCoord);
}

void GPU::GP0SetDrawingAreaBottomRight() {
    // 0xE4
    uint32_t xCoord = gp0 & 0x3FF;
    uint32_t yCoord = (gp0 >> 10) & 0x1FF;

    LOG_GPU(std::format("GP0 - SetDrawingAreaBottomRight({:d}, {:d})", xCoord, yCoord));

    renderer->setDrawingAreaBottomRight(xCoord, yCoord);
}

void GPU::GP0SetDrawingOffset() {
    // 0xE5
    uint32_t xOffset = gp0 & 0x7FF;
    uint32_t yOffset = (gp0 >> 11) & 0x7FF;

    int32_t signedXOffset = ((xOffset >> 10) ? 0xFFFFF800 : 0x00000000) | xOffset;
    int32_t signedYOffset = ((yOffset >> 10) ? 0xFFFFF800 : 0x00000000) | yOffset;

    LOG_GPU(std::format("GP0 - SetDrawingOffset({:d}, {:d})", signedXOffset, signedYOffset));
    drawingOffsetX = signedXOffset;
    drawingOffsetY = signedYOffset;
}

void GPU::GP0MaskBitSetting() {
    // 0xE6
    uint8_t setMaskWhileDrawing = gp0 & 1;
    uint8_t checkMaskBeforeDraw = (gp0 >> 1) & 1;

    LOG_GPU(std::format("GP0 - MaskBitSetting({:01b}, {:01b})",
                         setMaskWhileDrawing,
                         checkMaskBeforeDraw));

    setGPUStatusRegisterBit(GPUSTAT_SET_MASK, setMaskWhileDrawing);
    setGPUStatusRegisterBit(GPUSTAT_DRAW_PIXELS, checkMaskBeforeDraw);
}

void GPU::GP1ResetGPU() {
    // 0x00
    LOG_GPU(std::format("GP1 - ResetGPU"));

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

    LOG_GPU(std::format("GPUSTAT: 0x{:08X}", gpuStatusRegister));
}

void GPU::GP1ResetCommandBuffer() {
    // 0x01
    LOG_GPU(std::format("GP1 - ResetCommandBuffer"));
    queue.clear();
}

void GPU::GP1AcknowledgeGPUInterrupt() {
    // 0x02
    LOG_GPU(std::format("GP1 - AcknowledgeGPUInterrupt"));

    gpuStatusRegister = gpuStatusRegister & ~(1 << GPUSTAT_IRQ);
}

void GPU::GP1DisplayEnable() {
    // 0x03
    uint32_t parameter = gp1 & 0x00FFFFFF;

    LOG_GPU(std::format("GP1 - DisplayEnable (0x{:06X})", parameter));

    setGPUStatusRegisterBit(GPUSTAT_DISPLAY_ENABLE, parameter & 1);
}

void GPU::GP1DMADirection() {
    // 0x04
    uint32_t parameter = gp1 & 0x00FFFFFF;

    LOG_GPU(std::format("GP1 - DMADirection(0x{:06X})", parameter));

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

    startOfDisplayAreaX = parameter & 0x000003FF;
    startOfDisplayAreaY = (parameter >> 10) & 0x000001FF;

    LOG_GPU(std::format("GP1 - StartOfDisplayArena({:d}, {:d})",
                        startOfDisplayAreaX, startOfDisplayAreaY));
}

void GPU::GP1HorizontalDisplayRange() {
    // 0x06
    uint32_t parameter = gp1 & 0x00FFFFFF;

    horizontalDisplayRangeX1 = parameter & 0x00000FFF;
    horizontalDisplayRangeX2 = (parameter >> 12) & 0x00000FFF;

    LOG_GPU(std::format("GP1 - HorizontalDisplayRange(0x{:03X}, 0x{:03X})",
                        horizontalDisplayRangeX1, horizontalDisplayRangeX2));
}

void GPU::GP1VerticalDisplayRange() {
    // 0x07
    uint32_t parameter = gp1 & 0x00FFFFFF;

    verticalDisplayRangeY1 = parameter & 0x000003FF;
    verticalDisplayRangeY2 = (parameter >> 10) & 0x000003FF;

    LOG_GPU(std::format("GP1 - VerticalDisplayRange(0x{:03X}, 0x{:03X})",
                        verticalDisplayRangeY1, verticalDisplayRangeY2));
}

void GPU::GP1DisplayMode() {
    // 0x08
    // parameter bits 0...5 are GPUSTAT bits 17...22
    // parameter bit 6 is GPUSTAT bit 16
    // parameter bit 5 is GPUSTAT bit 14
    uint32_t parameter = gp1 & 0x00FFFFFF;

    LOG_GPU(std::format("GP1 - DisplayMode(0x{:06X})", parameter));

    gpuStatusRegister = (gpuStatusRegister & 0xFFFFFC00) | (gp0 & 0x000003FF);
    setGPUStatusRegisterBit(16 , (parameter >> 6) & 1);
    setGPUStatusRegisterBit(14 , (parameter >> 5) & 1);
}

void GPU::GP1NewTextureDisable() {
    // 0x09
    uint32_t parameter = gp1 & 0x00FFFFFF;

    LOG_GPU(std::format("GP1 - NewTextureDisable(0x{:06X})", parameter));

    setGPUStatusRegisterBit(GPUSTAT_TEXTURE_DISABLE, parameter & 1);
}

void GPU::GP1GetGPUInfo() {
    // 0x10...0x1F
}

}

