#ifndef PSX_GPU_H
#define PSX_GPU_H

#include <cstdint>
#include <iostream>

namespace PSX {

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
// 0 = display enabled
#define GPUSTAT_DISPLAY_DISABLE 23
// Vertical interlace
#define GPUSTAT_VERTICAL_INTERLACE 22
// Display area color depth (0 = 15 bit, 1 = 24 bit)
#define GPUSTAT_DISPLAY_AREA_COLOR_DEPTH 21
// Video mode (0 = NTSC/60Hz, 1 = PAL/50Hz)
#define GPUSTAT_VIDEO_MODE 20
// Vertical Resolution
#define GPUSTAT_VERTICAL_RESOLUTION 19
// Horizontal resolution (0 = 256/320/512/640, 1 = 368
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

class GPU {
private:
    // 1F801810
    // Write GP0     Send GP0 Commands/Packets (Rendering and VRAM Access)
    // Read  GPUREAD Receive responses to GP0(C0h) and GP1(10h) commands
    uint32_t gp0;

    // 1F801814
    // Write GP1     Send GP1 Commands (Display Control) (and DMA Control)
    // Read  GPUSTAT Receive GPU Status Register
    uint32_t gp1;
    uint32_t gpuStatusRegister;

    bool texturedRectangleXFlip;
    bool texturedRectangleYFlip;

    friend std::ostream& operator<<(std::ostream &os, const GPU &gpu);

public:
    void reset();

    template <typename T>
    void write(uint32_t address, T value);

    template <typename T>
    T read(uint32_t address);

private:
    void decodeAndExecuteGP0();
    // 0xE1
    void GP0DrawModeSetting();

    void decodeAndExecuteGP1();
};

}

#endif
