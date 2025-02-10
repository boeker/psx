#ifndef GLRENDER_H
#define GLRENDER_H

namespace PSX {

class GLRender {
public:
    GLRender();
    virtual ~GLRender();

    void draw();

private:
    unsigned int program;
    unsigned int vbo;
    unsigned int vao;
};

}

#endif

