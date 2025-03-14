#ifndef PSX_RENDERER_OPENGLRENDERER_H
#define PSX_RENDERER_OPENGLRENDERER_H

#include <cstdint>

#include "renderer/renderer.h"

namespace PSX {

#define VRAM_SIZE (1024 * 1024)

class Screen;
class Shader;

class OpenGLRenderer : public Renderer {
public:
    OpenGLRenderer(Screen *screen, Screen *vramViewer);
    virtual ~OpenGLRenderer();

    void reset() override;
    void clear() override;
    void computeViewport();
    void swapBuffers() override;
    void drawTriangle(const Triangle &triangle) override;
    void loadTexture(uint8_t *textureData) override;
    void drawTexturedTriangle(const TexturedTriangle &triangle) override;

    void writeToVRAM(uint32_t line, uint32_t pos, uint16_t value) override;
    uint16_t readFromVRAM(uint32_t line, uint32_t pos) override;

    void writeToVRAM(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint8_t *data) override;

    void setDrawingAreaTopLeft(uint32_t x, uint32_t y) override;
    void setDrawingAreaBottomRight(uint32_t x, uint32_t y) override;

private:
    Screen *screen;
    Screen *vramViewer;
    uint8_t *vram;

    Shader *shader;
    unsigned int program;
    unsigned int vbo;
    unsigned int vao;

    unsigned int vramFramebuffer;
    unsigned int vramTexture;
    unsigned int quadVAO;
    unsigned int quadVBO;

    Shader *screenShader;

    int viewportX, viewportY;
    int viewportWidth, viewportHeight;

    Shader *textureShader;
    unsigned int textureVBO;
    unsigned int textureVAO;
    unsigned int textureTexture;
};

}

#endif

