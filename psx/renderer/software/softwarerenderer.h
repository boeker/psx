#ifndef PSX_RENDERER_SOFTWARERENDERER_H
#define PSX_RENDERER_SOFTWARERENDERER_H

#include <cstdint>
#include <vector>

#include "renderer/renderer.h"

namespace PSX {

#define VRAM_SIZE (1024 * 1024)

class Screen;

class SoftwareRenderer : public Renderer {
public:
    SoftwareRenderer(Screen *screen, Screen *vramViewer);
    virtual ~SoftwareRenderer();

    void initialize();

    void installVRAMViewer(Screen *vramViewer);

    void reset() override;
    void clear() override;
    void computeViewport();
    void computeVRAMViewport();
    void swapBuffers() override;

    void writeToVRAM(uint32_t line, uint32_t pos, uint16_t value) override;
    uint16_t readFromVRAM(uint32_t line, uint32_t pos) override;
    void fillRectangleInVRAM(const Color &c, uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;

    void setDrawingAreaTopLeft(uint32_t x, uint32_t y) override;
    void setDrawingAreaBottomRight(uint32_t x, uint32_t y) override;

    void drawTriangle(const Triangle &triangle) override;
    void drawTexturedTriangle(const TexturedTriangle &triangle) override;

private:
    void drawTriangle(int ax, int ay, int bx, int by, int cx, int cy, Color ac, Color bc, Color cc);
    void drawTexturedTriangle(int ax, int ay, int bx, int by, int cx, int cy, int tx1, int ty1, int tx2, int ty2, int tx3, int ty3, uint16_t texpage, uint16_t palette);

private:
    Screen *screen;
    Screen *vramViewer;
    uint8_t *vram;

    unsigned int vramFramebuffer;
    unsigned int vramTexture;

    int viewportX, viewportY;
    int viewportWidth, viewportHeight;
    int vramViewportX, vramViewportY;
    int vramViewportWidth, vramViewportHeight;

    uint32_t drawingAreaTopLeftX;
    uint32_t drawingAreaTopLeftY;
    uint32_t drawingAreaBottomRightX;
    uint32_t drawingAreaBottomRightY;
};

}

#endif

