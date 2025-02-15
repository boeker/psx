#ifndef PSX_RENDERER_NULLRENDERER_H
#define PSX_RENDERER_NULLRENDERER_H

#include <cstdint>

#include "renderer/renderer.h"

namespace PSX {

#define VRAM_SIZE (1024 * 1024)

class NullRenderer : public Renderer {
public:
    NullRenderer();
    virtual ~NullRenderer();

    void reset() override;
    void clear() override;
    void swapBuffers() override;
    void drawTriangle(const Triangle &triangle) override;

    void writeToVRAM(uint32_t line, uint32_t pos, uint16_t value) override;
    uint16_t readFromVRAM(uint32_t line, uint32_t pos) override;

private:
    uint8_t *vram;
};

}

#endif

