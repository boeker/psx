#include "openglrenderer.h"

#include <glad/glad.h>
#include <cstring>
#include <format>
#include <iostream>

#include "renderer/screen.h"
#include "util/log.h"
#include "shader.h"
#include "gl.h"

using namespace util;

namespace PSX {

OpenGLRenderer::OpenGLRenderer(Screen *screen, Screen *vramViewer)
    : screen(screen), vramViewer(vramViewer) {

    vram = new uint8_t[VRAM_SIZE];
    reset();
}

void OpenGLRenderer::initialize() {
    glCheckError();

    shader = new Shader("shaders/color.vs", "shaders/color.fs");
    shader->use();

    glCheckError();

    // vertex array object and vertex buffer object
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    //float vertices[] =  {
    //    -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
    //    1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
    //    1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f
    //};
    //int vertices[] =  {
    //    0, 100,  255, 250, 100,
    //    100, 0,  255, 250, 0,
    //    100, 100,  255, 250, 0
    //};


    //glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 2, GL_INT, GL_FALSE, 5 * sizeof(int), (void*)0);
    glEnableVertexAttribArray(0);
    // color attribute
    glVertexAttribPointer(1, 3, GL_INT, GL_FALSE, 5 * sizeof(int), (void*)(2* sizeof(int)));
    glEnableVertexAttribArray(1);

    glCheckError();

    // store VRAM in a texture
    glGenFramebuffers(1, &vramFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, vramFramebuffer);

    glCheckError();

    // texture attachment
    glGenTextures(1, &vramTexture);
    glBindTexture(GL_TEXTURE_2D, vramTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1024, 512, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, vramTexture, 0);

    glCheckError();

    // create a renderbuffer object for depth and stencil attachment (we won't be sampling these)
    //unsigned int rbo;
    //glGenRenderbuffers(1, &rbo);
    //glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    //glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1024, 512); // use a single renderbuffer object for both a depth AND stencil buffer.
    //glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo); // now actually attach it

    glCheckError();

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;

    //// fill the vram texture with red for debugging purposes
    //glViewport(0, 32, 640, 480);
    //glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    //glClear(GL_COLOR_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glCheckError();

    // quad
    float quadVertices[] = { // vertex attributes for a quad that fills the entire screen in Normalized Device Coordinates.
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    glCheckError();

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glCheckError();

    screenShader = new Shader("shaders/screen.vs", "shaders/screen.fs");
    screenShader->use();
    screenShader->setInt("screenTexture", 0);

    textureShader = new Shader("shaders/texture.vs", "shaders/texture.fs");
    textureShader->use();
    textureShader->setInt("textureTexture", 0);

    glCheckError();

    // texture for rendering textured triangles
    glGenTextures(1, &textureTexture);
    glBindTexture(GL_TEXTURE_2D, textureTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glCheckError();

    // vertex array object and vertex buffer object
    glGenVertexArrays(1, &textureVAO);
    glGenBuffers(1, &textureVBO);
    glBindVertexArray(textureVAO);

    glBindBuffer(GL_ARRAY_BUFFER, textureVBO);
    float textureVertices[] =  {
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 0.0f, 0.0f, 1.0f
    };

    glBufferData(GL_ARRAY_BUFFER, sizeof(textureVertices), textureVertices, GL_DYNAMIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // texture coordinates
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3* sizeof(float)));
    glEnableVertexAttribArray(1);

    // wireframe mode
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
}

OpenGLRenderer::~OpenGLRenderer() {
    delete[] vram;

    //glDeleteVertexArrays(1, &vao);
    //glDeleteBuffers(1, &vbo);
}

void OpenGLRenderer::installVRAMViewer(Screen *vramViewer) {
    this->vramViewer = vramViewer;
}

void OpenGLRenderer::reset() {
    std::memset(vram, 0, VRAM_SIZE);

    drawingAreaTopLeftX = 0;
    drawingAreaTopLeftY = 0;
    drawingAreaBottomRightX = 639;
    drawingAreaBottomRightY = 479;
}

void OpenGLRenderer::clear() {
    //glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    //glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGLRenderer::computeViewport() {
    // compute where to place vram framebuffer on screen
    int windowHeight = screen->getHeight();
    int windowWidth = screen->getWidth();

    int height = windowHeight;
    int width = (windowHeight / 3) * 4;
    if (width > windowWidth) {
        height = (windowWidth / 4) * 3;
        width = windowWidth;
    }

    viewportWidth = width;
    viewportHeight = height;
    viewportX = (windowWidth - width) / 2;
    viewportY = (windowHeight - height) / 2;
}

void OpenGLRenderer::computeVRAMViewport() {
    int windowHeight = vramViewer->getHeight();
    int windowWidth = vramViewer->getWidth();

    int height = windowHeight;
    int width = windowHeight * 2;
    if (width > windowWidth) {
        height = windowWidth / 2;
        width = windowWidth;
    }

    vramViewportWidth = width;
    vramViewportHeight = height;
    vramViewportX = (windowWidth - width) / 2;
    vramViewportY = (windowHeight - height) / 2;
}

void OpenGLRenderer::swapBuffers() {
    glCheckError();

    // compute viewport coordinates from window size
    computeViewport();

    // screen window
    screen->makeContextCurrent();

    // set new viewport
    glViewport(viewportX, viewportY, viewportWidth, viewportHeight);

    //// render (part of) vram texture to screen (default framebuffer)
    //glBindFramebuffer(GL_FRAMEBUFFER, 0);
    //screenShader->use();
    //glBindVertexArray(quadVAO);
    //glBindTexture(GL_TEXTURE_2D, vramTexture);
    //glDrawArrays(GL_TRIANGLES, 0, 6);
    //glCheckError();


    // blit vram framebuffer to default framebuffer
    glBindFramebuffer(GL_READ_FRAMEBUFFER, vramFramebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    //glBlitFramebuffer(0, 0, 1024, 512,
    glBlitFramebuffer(0, 0, 640, 480,
                      //viewportX, viewportY, viewportX + viewportWidth, viewportY + viewportHeight,
                      viewportX, viewportY + viewportHeight, viewportX + viewportWidth, viewportY, // flip texture along y-axis
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glCheckError();

    // swap buffers
    screen->swapBuffers();

    if (vramViewer && vramViewer->isVisible()) {
        computeVRAMViewport();

        // VRAM-viewer window
        vramViewer->makeContextCurrent();

        // set new viewport
        glViewport(0, 0, 1024, 512);

        // blit vram framebuffer to default framebuffer
        glBindFramebuffer(GL_READ_FRAMEBUFFER, vramFramebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, 1024, 512,
                          //viewportX, viewportY, viewportX + viewportWidth, viewportY + viewportHeight,
                          //0, 512, 1024, 0, // flip texture along y-axis
                          vramViewportX, vramViewportY + vramViewportHeight, vramViewportX + vramViewportWidth, vramViewportY, // flip texture along y-axis
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);

        glCheckError();

        // swap buffers
        vramViewer->swapBuffers();
    }
}

void OpenGLRenderer::drawTriangle(const Triangle &t) {
    glCheckError();

    setViewportIntoVRAM();
    glBindFramebuffer(GL_FRAMEBUFFER, vramFramebuffer);

    int vertices[] =  {
        t.v1.x, t.v1.y, t.c1.r, t.c1.g, t.c1.b,
        t.v2.x, t.v2.y, t.c2.r, t.c2.g, t.c2.b,
        t.v3.x, t.v3.y, t.c3.r, t.c3.g, t.c3.b
    };
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_DYNAMIC_DRAW);

    shader->use();
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // unbind VBO and VAO
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // unbind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLRenderer::loadTexture(uint8_t *textureData) {
    glCheckError();
    glBindTexture(GL_TEXTURE_2D, textureTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureData);

    glBindTexture(GL_TEXTURE_2D, 0);
    glCheckError();
}

void OpenGLRenderer::drawTexturedTriangle(const TexturedTriangle &t) {
    glCheckError();

    setViewportIntoVRAM();
    glBindFramebuffer(GL_FRAMEBUFFER, vramFramebuffer);

    float vertices[] =  {
        t.v1.x/320.0f - 1.0f, t.v1.y/240.0f - 1.0f, 0.0f, t.tc1.x/255.0f, t.tc1.y/255.0f,
        t.v2.x/320.0f - 1.0f, t.v2.y/240.0f - 1.0f, 0.0f, t.tc2.x/255.0f, t.tc2.y/255.0f,
        t.v3.x/320.0f - 1.0f, t.v3.y/240.0f - 1.0f, 0.0f, t.tc3.x/255.0f, t.tc3.y/255.0f
    };

    glBindBuffer(GL_ARRAY_BUFFER, textureVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    textureShader->use();
    glBindVertexArray(textureVAO);
    glBindTexture(GL_TEXTURE_2D, textureTexture);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // unbind VBO and VAO
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // unbind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLRenderer::writeToVRAM(uint32_t line, uint32_t pos, uint16_t value) {
    LOGT_REND(std::format("VRAM write 0x{:04X} -> line {:d}, position {:d}",
                              value, line, pos));

    uint16_t *vramLine = (uint16_t*)&(vram[512 * line]);
    vramLine[pos] = value;
}

uint16_t OpenGLRenderer::readFromVRAM(uint32_t line, uint32_t pos) {
    uint16_t *vramLine = (uint16_t*)&(vram[512 * line]);
    uint16_t value = vramLine[pos];

    LOGT_REND(std::format("VRAM read line {:d}, position {:d} -> 0x{:04X}",
                              line, pos, value));

    return value;
}

void OpenGLRenderer::writeToVRAM(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint8_t *data) {
    LOGV_REND(std::format("VRAM write to {:d}, {:d} of size {:d}x{:d}",
                              x, y, width, height));

    glBindTexture(GL_TEXTURE_2D, vramTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, data);
}

void OpenGLRenderer::fillRectangleInVRAM(const Color &c, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    glCheckError();

    glBindFramebuffer(GL_FRAMEBUFFER, vramFramebuffer);

    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, width, height);

    glClearColor(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // reset clear color
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // disable scissor test
    glDisable(GL_SCISSOR_TEST);

    // unbind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLRenderer::setDrawingAreaTopLeft(uint32_t x, uint32_t y) {
    shader->use();
    shader->setIVec2("drawingAreaTopLeft", x, y);
    drawingAreaTopLeftX = x;
    drawingAreaTopLeftY = y;
}

void OpenGLRenderer::setDrawingAreaBottomRight(uint32_t x, uint32_t y) {
    shader->use();
    shader->setIVec2("drawingAreaBottomRight", x, y);
    drawingAreaBottomRightX = x;
    drawingAreaBottomRightY = y;
}

void OpenGLRenderer::setViewportIntoVRAM() {
    glViewport(drawingAreaTopLeftX, drawingAreaTopLeftY,
               drawingAreaBottomRightX - drawingAreaTopLeftX + 1,
               drawingAreaBottomRightY - drawingAreaTopLeftY + 1);
}

}

