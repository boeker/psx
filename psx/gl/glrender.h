#ifndef GLRENDER_H
#define GLRENDER_H

#include <cstdint>

namespace PSX {

struct Triangle {
    int16_t x1, y1, x2, y2, x3, y3;
    uint8_t r1, g1, b1, r2, g2, b2, r3, g3, b3;
};

class GLRender {
public:
    GLRender();
    virtual ~GLRender();

    void draw();
    void clear();
    void drawTriangle(const Triangle &triangle);

private:
    unsigned int program;
    unsigned int vbo;
    unsigned int vao;
};

}

#endif

