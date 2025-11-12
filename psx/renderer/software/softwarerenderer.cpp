#include "softwarerenderer.h"

#include <glad/glad.h>
#include <cmath>
#include <cstring>
#include <format>
#include <iostream>

#include "renderer/screen.h"
#include "util/log.h"
#include "shader.h"
#include "gl.h"

using namespace util;

namespace PSX {

SoftwareRenderer::SoftwareRenderer(Screen *screen, Screen *vramViewer)
    : screen(screen), vramViewer(vramViewer) {

    vramCache = new uint8_t[VRAM_SIZE];
    reset();
}

void SoftwareRenderer::initialize() {
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

SoftwareRenderer::~SoftwareRenderer() {
    delete[] vramCache;

    //glDeleteVertexArrays(1, &vao);
    //glDeleteBuffers(1, &vbo);
}

void SoftwareRenderer::installVRAMViewer(Screen *vramViewer) {
    this->vramViewer = vramViewer;
}

void SoftwareRenderer::reset() {
    std::memset(vramCache, 0, VRAM_SIZE);

    drawingAreaTopLeftX = 0;
    drawingAreaTopLeftY = 0;
    drawingAreaBottomRightX = 639;
    drawingAreaBottomRightY = 479;

    decodedTexture.clear();
}

void SoftwareRenderer::clear() {
    //glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    //glClear(GL_COLOR_BUFFER_BIT);
}

void SoftwareRenderer::computeViewport() {
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

void SoftwareRenderer::computeVRAMViewport() {
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

void SoftwareRenderer::swapBuffers() {
    transferToVRAM.clear();
    for (int y = 0; y < 512; ++y) {
        for (int x = 0; x < 1024; ++x) {
            uint16_t *vramLine = (uint16_t*)&(vramCache[2048 * y]);
            uint16_t value = vramLine[x];

            transferToVRAM.push_back(((value >> 0) & 0x1F) << 3);
            transferToVRAM.push_back(((value >> 5) & 0x1F) << 3);
            transferToVRAM.push_back(((value >> 10) & 0x1F) << 3);
        }
    }
    //writeToVRAM(0, 0, 1024, 512, &transferToVRAM[0]);
    glBindTexture(GL_TEXTURE_2D, vramTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1024, 512, GL_RGB, GL_UNSIGNED_BYTE, &transferToVRAM[0]);


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

void SoftwareRenderer::drawTriangle(const Triangle &t) {
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

    drawLine(30, 40, 200, 210, 0x0000FF);
    drawTriangle(30, 30, 40, 150, 117, 60, 0x00FF00);
}

void SoftwareRenderer::loadTexture(uint8_t *textureData) {
    glCheckError();
    glBindTexture(GL_TEXTURE_2D, textureTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureData);

    glBindTexture(GL_TEXTURE_2D, 0);
    glCheckError();
}

uint8_t* SoftwareRenderer::decodeTexture(uint16_t texpage, uint16_t palette) {
    LOGT_GPU(std::format("Decoding texture"));
    decodedTexture.clear();

    uint32_t xBase = (texpage & 0xF) * 64; // in halfwords
    uint32_t yBase = ((texpage >> 4) & 1) * 256; // in lines
    uint8_t semiTransparency = (texpage >> 5) & 3;
    uint8_t texturePageColors = (texpage >> 7) & 3;
    uint8_t textureDisable = (texpage >> 11) & 1;

    uint32_t xPalette = (palette & 0x3F) * 16; // in halfwords
    uint32_t yPalette = (palette >> 6) & 0x1FF; // in lines

    LOG_REND(std::format("Decoding texture: XBase[{:d}], YBase[{:d}], Semi Transparency[{:d}], Texture Page Colors[{:d}], Texture Disable[{:d}], XPalette[{:d}], YPalette[{:d}]", xBase, yBase, semiTransparency, texturePageColors, textureDisable, xPalette, yPalette));

    if (texturePageColors == 0) { // 4-bit colors
        uint8_t colors[64];
        for (uint32_t x = 0; x < 16; ++x) {
            uint16_t halfword = readFromVRAM(yPalette, xPalette + x);
            uint8_t r = halfword & 0x1F;
            uint8_t g = (halfword >> 5) & 0x1F;
            uint8_t b = (halfword >> 10) & 0x1F;
            uint8_t semiTransparency = (halfword >> 15) & 1;

            colors[4*x+0] = r << 3;
            colors[4*x+1] = g << 3;
            colors[4*x+2] = b << 3;

            colors[4*x+3] = 255;
            if (semiTransparency == 0 && r == 0 & g == 0 & b == 0) {
                colors[4*x+3] = 0;
            }
        }

        for (uint32_t y = yBase; y < yBase + 256; ++y) {
            for (uint32_t x = xBase; x < xBase + 64; ++x) {
                uint16_t halfword = readFromVRAM(y, x);

                uint8_t p1 = halfword & 0xF;
                uint8_t p2 = (halfword >> 4) & 0xF;
                uint8_t p3 = (halfword >> 8) & 0xF;
                uint8_t p4 = (halfword >> 12) & 0xF;

                decodedTexture.push_back(colors[4*p1+0]);
                decodedTexture.push_back(colors[4*p1+1]);
                decodedTexture.push_back(colors[4*p1+2]);
                decodedTexture.push_back(colors[4*p1+3]);

                decodedTexture.push_back(colors[4*p2+0]);
                decodedTexture.push_back(colors[4*p2+1]);
                decodedTexture.push_back(colors[4*p2+2]);
                decodedTexture.push_back(colors[4*p2+3]);

                decodedTexture.push_back(colors[4*p3+0]);
                decodedTexture.push_back(colors[4*p3+1]);
                decodedTexture.push_back(colors[4*p3+2]);
                decodedTexture.push_back(colors[4*p3+3]);

                decodedTexture.push_back(colors[4*p4+0]);
                decodedTexture.push_back(colors[4*p4+1]);
                decodedTexture.push_back(colors[4*p4+2]);
                decodedTexture.push_back(colors[4*p4+3]);
            }
        }

    } else {
        LOG_GPU(std::format("Texture format not implemented"));
    }

    LOGT_GPU(std::format("Decoded texture size: {:d} bytes", decodedTexture.size()));

    if (decodedTexture.size() > 0) {
        return &decodedTexture[0];

    } else {
        return nullptr;
    }
}

void SoftwareRenderer::drawTexturedTriangle(const TexturedTriangle &t) {
    uint8_t *texture = decodeTexture(t.texpage, t.palette);
    loadTexture(texture);

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

void SoftwareRenderer::writeToVRAM(uint32_t line, uint32_t pos, uint16_t value) {
    LOGT_REND(std::format("VRAM write 0x{:04X} -> line {:d}, position {:d}",
                              value, line, pos));

    uint16_t *vramLine = (uint16_t*)&(vramCache[2048 * line]);
    vramLine[pos] = value;
}

void SoftwareRenderer::writeToVRAM(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint8_t *data) {
    LOGV_REND(std::format("VRAM write to {:d}, {:d} of size {:d}x{:d}",
                              x, y, width, height));

    //glBindTexture(GL_TEXTURE_2D, vramTexture);
    //glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, data);
}

void SoftwareRenderer::prepareReadFromVRAM(uint32_t line, uint32_t pos, uint32_t width, uint32_t height) {
    LOGV_REND(std::format("prepareReadFromVRAM: from {:d}, {:d} of size {:d}x{:d}",
                              line, pos, width, height));

}

uint16_t SoftwareRenderer::readFromVRAM(uint32_t line, uint32_t pos) {
    uint16_t *vramLine = (uint16_t*)&(vramCache[2048 * line]);
    uint16_t value = vramLine[pos];

    LOGT_REND(std::format("VRAM read line {:d}, position {:d} -> 0x{:04X}",
                              line, pos, value));

    return value;
}


void SoftwareRenderer::fillRectangleInVRAM(const Color &c, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
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

void SoftwareRenderer::setDrawingAreaTopLeft(uint32_t x, uint32_t y) {
    shader->use();
    shader->setIVec2("drawingAreaTopLeft", x, y);
    drawingAreaTopLeftX = x;
    drawingAreaTopLeftY = y;
}

void SoftwareRenderer::setDrawingAreaBottomRight(uint32_t x, uint32_t y) {
    shader->use();
    shader->setIVec2("drawingAreaBottomRight", x, y);
    drawingAreaBottomRightX = x;
    drawingAreaBottomRightY = y;
}

void SoftwareRenderer::setViewportIntoVRAM() {
    glViewport(drawingAreaTopLeftX, drawingAreaTopLeftY,
               drawingAreaBottomRightX - drawingAreaTopLeftX + 1,
               drawingAreaBottomRightY - drawingAreaTopLeftY + 1);
}

void SoftwareRenderer::drawLine(int ax, int ay, int bx, int by, uint16_t color) {
    bool swapCoordinates = std::abs(ax - bx) < std::abs(ay - by);
    if (swapCoordinates) { // Make sure that we avoid gaps if line is too steep by swapping x and y
        std::swap(ax, ay);
        std::swap(bx, by);
    }

    if (ax > bx) { // Make sure that we are drawing from left to right
        std::swap(ax, bx);
        std::swap(ay, by);
    }

    for (int x = ax; x <= bx; x++) {
        // f(x) = ay + (by - ay) * (x - ax) / (bx - ax)
        float t = (x - ax) / static_cast<float>(bx - ax);
        int y = std::round(ay + (by - ay) * t);

        if (!swapCoordinates) {
            write(x, y, color);
        } else {
            write(y, x, color);
        }
    }
}

void SoftwareRenderer::drawTriangle(int ax, int ay, int bx, int by, int cx, int cy, uint16_t color) {
    // Sort the points by their y-coordinates
    if (ay > by) {
        std::swap(ax, bx);
        std::swap(ay, by);
    }
    if (ay > cy) {
        std::swap(ax, cx);
        std::swap(ay, cy);
    }
    if (by > cy) {
        std::swap(bx, cx);
        std::swap(by, cy);
    }
    // We now have ay <= by <= cy
    int total_height = cy - ay;

    // Bottom half
    if (ay != by) {
        int segment_height = by - ay;
        for (int y = ay; y <= by; y++) {
            int x1 = ax + ((cx - ax) * (y - ay)) / total_height;
            int x2 = ax + ((bx - ax) * (y - ay)) / segment_height;

            // Draw line from left to right
            for (int x = std::min(x1, x2); x < std::max(x1, x2); x++) {
                write(x, y, color);
            }
        }
    }

    // Top half
    if (by != cy) {
        int segment_height = cy - by;
        for (int y = by; y <= cy; y++) {
            int x1 = ax + ((cx - ax) * (y - ay)) / total_height;
            int x2 = bx + ((cx - bx) * (y - by)) / segment_height;

            // Draw line from left to right
            for (int x = std::min(x1, x2); x < std::max(x1, x2); x++) {
                write(x, y, color);
            }
        }
    }
}

void SoftwareRenderer::write(uint32_t x, uint32_t y, uint16_t value) {
        ((uint16_t*)vramCache)[y * 1024 + x] = value;
}

}

