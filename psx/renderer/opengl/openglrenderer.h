#ifndef PSX_RENDERER_OPENGLRENDERER_H
#define PSX_RENDERER_OPENGLRENDERER_H

#include <cstdint>

#include "renderer/renderer.h"

namespace PSX {

class Screen;

class OpenGLRenderer : public Renderer {
public:
    OpenGLRenderer(Screen *screen);
    virtual ~OpenGLRenderer();

    void draw();
    void clear() override;
    void drawTriangle(const Triangle &triangle) override;
    void swapBuffers() override;

private:
    Screen *screen;

    unsigned int program;
    unsigned int vbo;
    unsigned int vao;
};

}

#endif

