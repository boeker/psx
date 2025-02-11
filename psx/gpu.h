#ifndef PSX_GPU_H
#define PSX_GPU_H

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace PSX {

#define VRAM_SIZE (1024 * 1024)

// Even/odd lines in interlace mode (0 = even or vblank, 1 = odd)
#define GPUSTAT_INTERLACE_EVEN_ODD 31
// DMA Direction (0 = off, 1 = FIFO, 2 = CPU to GP0, 3 = GPUREAD to CPU)
#define GPUSTAT_DMA_DIRECTION1 30
#define GPUSTAT_DMA_DIRECTION0 29
// Ready to receive DMA block?
#define GPUSTAT_DMA_RECEIVE_READY 28
// Ready to send VRAM TO CPU?
#define GPUSTAT_VRAM_SEND_READY 27
// Ready to receive command word?
#define GPUSTAT_CMDWORD_RECEIVE_READY 26
// DMA / Data Request, meaning depends on DMA Direction
// direction 0 -> always zero
// direction 1 -> FIFO State (0 = Full, 1 = Not Full)
// direction 2 -> same as bit 28
// direction 3 -> same as bit 27
#define GPUSTAT_DATAREQUEST 25
// Interrupt request (IRQ1)
#define GPUSTAT_IRQ 24
// 0 = display enabled, 1 = display disabled
#define GPUSTAT_DISPLAY_ENABLE 23
// Vertical interlace (0 = off, 1 = on)
#define GPUSTAT_VERTICAL_INTERLACE 22
// Display area color depth (0 = 15 bit, 1 = 24 bit)
#define GPUSTAT_DISPLAY_AREA_COLOR_DEPTH 21
// Video mode (0 = NTSC/60Hz, 1 = PAL/50Hz)
#define GPUSTAT_VIDEO_MODE 20
// Vertical Resolution (0 = 240, 1 = 480 if vertical interlace enabled) 
#define GPUSTAT_VERTICAL_RESOLUTION 19
// Horizontal resolution (0 = 256/320/512/640, 1 = 368)
#define GPUSTAT_HORIZONTAL_RESOLUTION2 18
// (0 = 256, 1 = 320, 2 = 512, 3 = 640)
#define GPUSTAT_HORIZONTAL_RESOLUTION11 17
#define GPUSTAT_HORIZONTAL_RESOLUTION10 16
// Texture disable (0 = normal, 1 = disable textures)
#define GPUSTAT_TEXTURE_DISABLE 15
// Reverse flag (0 = normal, 1 = distorted)
#define GPUSTAT_REVERSEFLAG 14
// Interlace field (always 1 when interlacing disabled)
#define GPUSTAT_INTERLACE_FIELD 13
// Draw pixels (0 = always, 1 = not to masked areas)
#define GPUSTAT_DRAW_PIXELS 12
// Set mask bit when drawing pixels (0 = no, 1 = yes/mask)
#define GPUSTAT_SET_MASK 11
// Drawing to display area allowed (0 = prohibited, 1 = allowed)
#define GPUSTAT_DRAWING_TO_DISPLAY_AREA_ALLOWED 10
// Dither 24bit to 15bit (0 = off/strip LSBs, 1 = dithering enabled)
#define GPUSTAT_DITHER 9
// Texture page colors (0 = 4bit, 1 = 8bit, 2 = 15bit, 3 = reserved)
#define GPUSTAT_TEXTURE_PAGE_COLORS1 8
#define GPUSTAT_TEXTURE_PAGE_COLORS0 7
// Semi transparency (0 = B/2 + F/2, 1 = B + F, 2 = B - F, 3 = B + F/4)
#define GPUSTAT_SEMI_TRANSPARENCY1 6
#define GPUSTAT_SEMI_TRANSPARENCY0 5
// Texture page Y base (0 = 0, 1 = 256)
#define GPUSTAT_TEXTURE_PAGE_Y_BASE 4
// Texture page X base (N * 64)
#define GPUSTAT_TEXTURE_PAGE_X_BASE3 3
#define GPUSTAT_TEXTURE_PAGE_X_BASE2 2
#define GPUSTAT_TEXTURE_PAGE_X_BASE1 1
#define GPUSTAT_TEXTURE_PAGE_X_BASE0 0

class CommandQueue {
private:
    uint32_t queue[16];
    uint8_t in;
    uint8_t out;
    uint8_t elements;

    friend std::ostream& operator<<(std::ostream &os, const CommandQueue &queue);

public:
    CommandQueue();
    void clear();
    void push(uint32_t command);
    uint32_t pop();
    bool isEmpty();
    bool isFull();
};

class Bus;
class Screen;

class GPU {
private:
    Bus *bus;
    Screen *screen;

    enum State {
        IDLE,
        EXECUTING_CP0,
        WAITING_FOR_GP0_PARAMS,
        TRANSFER_TO_VRAM,
        TRANSFER_TO_CPU
    };
    State state;

    uint8_t *vram;

    // 1F801810
    // Write GP0     Send GP0 Commands/Packets (Rendering and VRAM Access)
    // Read  GPUREAD Receive responses to GP0(C0h) and GP1(10h) commands
    CommandQueue queue;
    uint32_t gp0;
    uint8_t gp0Command;
    uint8_t neededParams;
    std::vector<uint32_t> gp0Parameters;
    uint32_t transferToVRAMRemainingWords;
    uint32_t destinationX;
    uint32_t destinationY;
    uint32_t destinationSizeX;
    uint32_t destinationSizeY;
    uint32_t destinationCurrentX;
    uint32_t destinationCurrentY;
    uint32_t transferToCPURemainingWords;
    uint32_t sourceX;
    uint32_t sourceY;
    uint32_t sourceSizeX;
    uint32_t sourceSizeY;
    uint32_t sourceCurrentX;
    uint32_t sourceCurrentY;

    uint32_t gpuReadResponse;

    // 1F801814
    // Write GP1     Send GP1 Commands (Display Control) (and DMA Control)
    // Read  GPUSTAT Receive GPU Status Register
    uint32_t gp1;
    uint32_t gpuStatusRegister;

    bool texturedRectangleXFlip;
    bool texturedRectangleYFlip;
    uint16_t startOfDisplayAreaX; // half-word address in VRAM
    uint16_t startOfDisplayAreaY; // scanline number in VRAM

    uint16_t horizontalDisplayRangeX1; // on screen
    uint16_t horizontalDisplayRangeX2; // on screen

    uint16_t verticalDisplayRangeY1; // on screen
    uint16_t verticalDisplayRangeY2; // on screen

    // top left
    uint16_t drawingAreaX1;
    uint16_t drawingAreaY1;

    // bottom right
    uint16_t drawingAreaX2;
    uint16_t drawingAreaY2;

    int32_t drawingOffsetX;
    int32_t drawingOffsetY;

    uint8_t textureWindowMaskX;
    uint8_t textureWindowMaskY;
    uint8_t textureWindowOffsetX;
    uint8_t textureWindowOffsetY;

    friend std::ostream& operator<<(std::ostream &os, const GPU &gpu);

public:
    GPU(Bus *bus);
    virtual ~GPU();
    void reset();
    void setScreen(Screen *screen);

    void catchUpToCPU(uint32_t cpuCycles);
    void decodeAndExecuteGP1();

    void writeToVRAM(uint32_t line, uint32_t pos, uint16_t value);
    uint16_t readFromVRAM(uint32_t line, uint32_t pos);
    void advanceCurrentDestinationPosition();
    void advanceCurrentSourcePosition();

    template <typename T>
    void write(uint32_t address, T value);

    template <typename T>
    T read(uint32_t address);

    bool transferFromGPURequested();
    bool transferToGPURequested();
    void receiveGP0Data(uint32_t word);
    uint32_t sendGP0Data();

private:
    std::string getGPUStatusRegisterExplanation() const;
    std::string getGPUStatusRegisterExplanation2() const;
    void setGPUStatusRegisterBit(uint32_t bit, uint32_t value);

    typedef void (GPU::*Command) ();
    static const Command gp0Commands[];
    static const uint8_t gp0ParameterNumbers[];

    void GP0Unknown();

    // 0x00
    void GP0NOP();
    // 0x01
    void GP0ClearCache();
    // 0x28
    void GP0MonochromeFourPointPolygonOpaque();
    // 0x2C
    void GP0TexturedFourPointPolygonOpaqueTextureBlending();
    // 0x30
    void GP0ShadedThreePointPolygonOpaque();
    // 0x38
    void GP0ShadedFourPointPolygonOpaque();
    // 0xA0
    void GP0CopyRectangleToVRAM();
    // 0xC0
    void GP0CopyRectangleVRAMToCPU();
    // 0xE1
    void GP0DrawModeSetting();
    // 0xE2
    void GP0TextureWindowSetting();
    // 0xE3
    void GP0SetDrawingAreaTopLeft();
    // 0xE4
    void GP0SetDrawingAreaBottomRight();
    // 0xE5
    void GP0SetDrawingOffset();
    // 0xE6
    void GP0MaskBitSetting();

    // 0x00
    void GP1ResetGPU();
    // 0x01
    void GP1ResetCommandBuffer();
    // 0x02
    void GP1AcknowledgeGPUInterrupt();
    // 0x03
    void GP1DisplayEnable();
    // 0x04
    void GP1DMADirection();
    // 0x05
    void GP1StartOfDisplayArea();
    // 0x06
    void GP1HorizontalDisplayRange();
    // 0x07
    void GP1VerticalDisplayRange();
    // 0x08
    void GP1DisplayMode();
    // 0x09
    void GP1NewTextureDisable();
    // 0x10...0x1F
    void GP1GetGPUInfo();
};

}

#endif
