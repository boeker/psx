#ifndef PSX_RENDERER_SOFTWARERENDERER_H
#define PSX_RENDERER_SOFTWARERENDERER_H

#include <cstdint>
#include <vector>

#include "renderer/renderer.h"

namespace PSX {

#define VRAM_SIZE (1024 * 1024)

class Screen;
class Shader;

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
    void drawTriangle(const Triangle &triangle) override;
    void loadTexture(uint8_t *textureData) override;
    void drawTexturedTriangle(const TexturedTriangle &triangle) override;

    void writeToVRAM(uint32_t line, uint32_t pos, uint16_t value) override;
    uint16_t readFromVRAM(uint32_t line, uint32_t pos) override;
    void prepareReadFromVRAM(uint32_t line, uint32_t pos, uint32_t width, uint32_t height) override;

    void writeToVRAM(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint8_t *data) override;
    void fillRectangleInVRAM(const Color &c, uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;

    void setDrawingAreaTopLeft(uint32_t x, uint32_t y) override;
    void setDrawingAreaBottomRight(uint32_t x, uint32_t y) override;
    void setViewportIntoVRAM();

    uint8_t* decodeTexture(uint16_t texpage, uint16_t palette);

    void drawLine(int ax, int ay, int bx, int by, uint16_t color);
    void drawTriangleTest(const Triangle &triangle);
    void drawTriangle(int ax, int ay, int bx, int by, int cx, int cy, Color ac, Color bc, Color cc);

    void write(uint32_t x, uint32_t y, uint16_t value);

private:
    Screen *screen;
    Screen *vramViewer;
    uint8_t *vramCache;

    Shader *shader;
    unsigned int vbo;
    unsigned int vao;

    unsigned int vramFramebuffer;
    unsigned int vramTexture;
    unsigned int quadVAO;
    unsigned int quadVBO;

    Shader *screenShader;

    int viewportX, viewportY;
    int viewportWidth, viewportHeight;
    int vramViewportX, vramViewportY;
    int vramViewportWidth, vramViewportHeight;

    Shader *textureShader;
    unsigned int textureVBO;
    unsigned int textureVAO;
    unsigned int textureTexture;

    uint32_t drawingAreaTopLeftX;
    uint32_t drawingAreaTopLeftY;
    uint32_t drawingAreaBottomRightX;
    uint32_t drawingAreaBottomRightY;

    std::vector<uint8_t> decodedTexture;

    std::vector<uint8_t> transferToVRAM;

};

}

#endif

