#ifndef PSX_GPU_H
#define PSX_GPU_H

#include <cstdint>

namespace PSX {

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
