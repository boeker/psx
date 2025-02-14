#ifndef GLRENDER_H
#define GLRENDER_H

#include <cstdint>

#include "gl/common.h"

namespace PSX {

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

