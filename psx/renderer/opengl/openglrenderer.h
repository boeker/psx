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
    OpenGLRenderer(Screen *screen);
    virtual ~OpenGLRenderer();

    void reset() override;
    void clear() override;
    void computeViewport();
    void swapBuffers() override;
    void drawTriangle(const Triangle &triangle) override;

    void writeToVRAM(uint32_t line, uint32_t pos, uint16_t value) override;
    uint16_t readFromVRAM(uint32_t line, uint32_t pos) override;

    void writeToVRAM(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint8_t *data) override;

private:
    Screen *screen;
    uint8_t *vram;

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
};

}

#endif

