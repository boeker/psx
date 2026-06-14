#include "softwarerenderer.h"

#include <glad/glad.h>
#include <cmath>
#include <cstring>
#include <format>
#include <iostream>

#include "renderer/screen.h"
#include "util/log.h"
#include "gl.h"

using namespace util;

namespace PSX {

SoftwareRenderer::SoftwareRenderer(Screen *screen, Screen *vramViewer)
    : screen(screen), vramViewer(vramViewer) {

    vram = new uint8_t[VRAM_SIZE];
    reset();
}

void SoftwareRenderer::initialize() {
    glCheckError();

    // store VRAM in a texture
    glGenFramebuffers(1, &vramFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, vramFramebuffer);

    glCheckError();

    // texture attachment
    glGenTextures(1, &vramTexture);
    glBindTexture(GL_TEXTURE_2D, vramTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1024, 512, 0, GL_RGBA,  GL_UNSIGNED_SHORT_1_5_5_5_REV, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, vramTexture, 0);

    glCheckError();

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;

    // bind default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glCheckError();
}

SoftwareRenderer::~SoftwareRenderer() {
    delete[] vram;
}

void SoftwareRenderer::installVRAMViewer(Screen *vramViewer) {
    this->vramViewer = vramViewer;
}

void SoftwareRenderer::reset() {
    std::memset(vram, 0, VRAM_SIZE);

    drawing_area_top_left_x = 0;
    drawing_area_top_left_y = 0;
    drawing_area_bot_right_x = 639;
    drawing_area_bot_right_y = 479;

    drawing_offset_x = 0;
    drawing_offset_y = 0;

    display_area_top_left_x = 0;
    display_area_top_left_y = 0;
    display_area_width = 640;
    display_area_height = 480;
}

void SoftwareRenderer::clear() {
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
    // upload vram to texture
    glBindTexture(GL_TEXTURE_2D, vramTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1024, 512, GL_RGBA,  GL_UNSIGNED_SHORT_1_5_5_5_REV, vram);

    glCheckError();

    // compute viewport coordinates from window size
    computeViewport();

    // screen window
    screen->makeContextCurrent();

    // set new viewport
    glViewport(viewportX, viewportY, viewportWidth, viewportHeight);

    // blit vram framebuffer to default framebuffer
    glBindFramebuffer(GL_READ_FRAMEBUFFER, vramFramebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    //glBlitFramebuffer(0, 0, 1024, 512,
    glBlitFramebuffer(display_area_top_left_x, display_area_top_left_y, display_area_top_left_x + display_area_width, display_area_top_left_y + display_area_height,
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

void SoftwareRenderer::writeToVRAM(uint32_t x, uint32_t y, uint16_t value) {
    //LOGT_REND(std::format("VRAM write 0x{:04X} -> line {:d}, position {:d}",
    //                          value, line, pos));

    if (2 * y * 1024 + x >= VRAM_SIZE) {
        LOG_WRN(std::format("writeToVRAM({:d}, {:d}, 0x{:04X}): out of bound", x, y, value));
    } else {
        ((uint16_t*)vram)[y * 1024 + x] = value;
    }
}

uint16_t SoftwareRenderer::readFromVRAM(uint32_t x, uint32_t y) {
    uint16_t value = ((uint16_t*)vram)[y * 1024 + x];

    //LOGT_REND(std::format("VRAM read line {:d}, position {:d} -> 0x{:04X}",
    //                          line, pos, value));

    return value;
}

void SoftwareRenderer::fillRectangleInVRAM(const PSX::Color &c, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    for (uint32_t i = x; i < x + width; ++i) {
        for (uint32_t j = y; j < y + height; ++j) {
            writeToVRAM(i, j, c.to16Bit());
        }
    }
}

void SoftwareRenderer::set_drawing_area(uint32_t top_left_x, uint32_t top_left_y, uint32_t bot_right_x, uint32_t bot_right_y) {
    drawing_area_top_left_x = std::min(top_left_x, 1024U);
    drawing_area_top_left_y = std::min(top_left_y, 512U);
    drawing_area_bot_right_x = std::min(bot_right_x, 1024U);
    drawing_area_bot_right_y = std::min(bot_right_y, 512U);
}

void SoftwareRenderer::set_drawing_offset(int32_t x, int32_t y) {
    drawing_offset_x = x;
    drawing_offset_y = y;
}


void SoftwareRenderer::set_display_area(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    display_area_top_left_x = x;
    display_area_top_left_y = y;
    display_area_width = width;
    display_area_height = height;

}

void SoftwareRenderer::drawTriangle(const Triangle &triangle) {
    LOGT_REND(std::format("drawTriangle({},{},{})", triangle.v1, triangle.v2, triangle.v3));
    ColoredPoint a = {
        triangle.v1.x + drawing_offset_x, triangle.v1.y + drawing_offset_y,
        { triangle.c1.r, triangle.c1.g, triangle.c1.b }
    };
    ColoredPoint b = {
        triangle.v2.x + drawing_offset_x, triangle.v2.y + drawing_offset_y,
        { triangle.c2.r, triangle.c2.g, triangle.c2.b }
    };
    ColoredPoint c = {
        triangle.v3.x + drawing_offset_x, triangle.v3.y + drawing_offset_y,
        { triangle.c3.r, triangle.c3.g, triangle.c3.b }
    };
    ColorContext context;

    draw_triangle<ColoredPoint, ColorContext>(a, b, c, context);
}

void SoftwareRenderer::drawTexturedTriangle(const TexturedTriangle &triangle) {
    LOGT_REND(std::format("drawTexturedTriangle({},{},{})", triangle.v1, triangle.v2, triangle.v3));
    TexturedPoint a = {
        triangle.v1.x + drawing_offset_x, triangle.v1.y + drawing_offset_y,
        { triangle.tc1.x, triangle.tc1.y }
    };
    TexturedPoint b = {
        triangle.v2.x + drawing_offset_x, triangle.v2.y + drawing_offset_y,
        { triangle.tc2.x, triangle.tc2.y }
    };
    TexturedPoint c = {
        triangle.v3.x + drawing_offset_x, triangle.v3.y + drawing_offset_y,
        { triangle.tc3.x, triangle.tc3.y }
    };
    TextureContext context(triangle.texpage, triangle.palette);

    draw_triangle<TexturedPoint, TextureContext>(a, b, c, context);
}

template<typename Point, typename Context>
void SoftwareRenderer::draw_triangle(Point a, Point b, Point c, Context context) {
    if (!context.valid()) {
        LOG_REND(std::format("DrawingContext not valid: format not implemented?"));
        return;
    }

    // Sort the points by their y-coordinates
    // such that a.y <= b.y <= c.y
    if (a.y > b.y) {
        std::swap(a, b);
    }
    if (a.y > c.y) {
        std::swap(a, c);
    }
    if (b.y > c.y) {
        std::swap(b, c);
    }

    // Bottom half of the triangle
    if (a.y != b.y) {
        draw_triangle_half<Point, Context>(a, c, a, b, context);
    }

    // Top half of the triangle
    if (b.y != c.y) {
        draw_triangle_half<Point, Context>(a, c, b, c, context);
    }
}

template void SoftwareRenderer::draw_triangle(TexturedPoint a, TexturedPoint b, TexturedPoint c, TextureContext context);

template<typename Point, typename Context>
void SoftwareRenderer::draw_triangle_half(Point a, Point c, Point f, Point t, Context context) {
    int32_t total_height = c.y - a.y;
    int32_t segment_height = t.y - f.y;

    // Draw lines from f.y to t.y
    for (int32_t y = std::max(f.y, (int32_t)drawing_area_top_left_y); y < std::min(t.y, (int32_t)drawing_area_bot_right_y); y++) { // Exclude t.y
        // Determine x-coordinates on triangle borders at height y
        // x_ac by interpolating between a.x and c.x
        // x_ft by interpolating between f.x and t.x
        int32_t x_ac = a.x + ((c.x - a.x) * (y - a.y)) / total_height;
        int32_t x_ft = f.x + ((t.x - f.x) * (y - f.y)) / segment_height;

        // Determine color/texture coordinate for point (x_ac, y) and (x_ft, y)
        // (x_ac, y) by interpolating a and c along y
        // (x_ft, y) by interpolating f and t along y
        typename Context::ColorType c_ac = Context::interpolate(c.c, y - a.y, a.c, c.y - y, total_height);
        typename Context::ColorType c_ft = Context::interpolate(t.c, y - f.y, f.c, t.y - y, segment_height);

        // Draw line from left to right
        Point left = { x_ac, y, c_ac };
        Point right = { x_ft, y, c_ft };
        if (x_ft < x_ac) {
            std::swap(left, right);
        }

        int32_t line_length = right.x - left.x;
        for (int x = std::max(left.x, (int)drawing_area_top_left_x); x < std::min(right.x, (int)drawing_area_bot_right_x); x++) {
            typename Context::ColorType c = Context::interpolate(right.c, x - left.x, left.c, right.x - x, line_length);
            draw_pixel(x, y, context, c);
        }
    }
}

template void SoftwareRenderer::draw_triangle_half(TexturedPoint a, TexturedPoint c, TexturedPoint f, TexturedPoint t, TextureContext context);

}

