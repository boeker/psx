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

void SoftwareRenderer::fillRectangleInVRAM(const Color &c, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
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
    drawTriangle(triangle.v1.x + drawing_offset_x,
                 triangle.v1.y + drawing_offset_y,
                 triangle.v2.x + drawing_offset_x,
                 triangle.v2.y + drawing_offset_y,
                 triangle.v3.x + drawing_offset_x,
                 triangle.v3.y + drawing_offset_y,
                 //Color(0xFF, 0, 0),
                 //Color(0, 0xFF, 0),
                 //Color(0, 0, 0xFF)
                 triangle.c1,
                 triangle.c2,
                 triangle.c3
                 );
}

void SoftwareRenderer::drawTriangle(int ax, int ay, int bx, int by, int cx, int cy, Color ac, Color bc, Color cc) {
    // Sort the points by their y-coordinates
    if (ay > by) {
        std::swap(ax, bx);
        std::swap(ay, by);
        std::swap(ac, bc);
    }
    if (ay > cy) {
        std::swap(ax, cx);
        std::swap(ay, cy);
        std::swap(ac, cc);
    }
    if (by > cy) {
        std::swap(bx, cx);
        std::swap(by, cy);
        std::swap(bc, cc);
    }
    // We now have ay <= by <= cy
    int total_height = cy - ay;

    // Bottom half
    if (ay != by) {
        int segment_height = by - ay;
        for (int y = std::max(std::max(0, ay), (int)drawing_area_top_left_y); y < std::min(std::min(512, by), (int)drawing_area_bot_right_y); y++) { // Exclude by
            int x1 = ax + ((cx - ax) * (y - ay)) / total_height;
            int x2 = ax + ((bx - ax) * (y - ay)) / segment_height;

            uint32_t r1 = (cc.r * (y - ay) + ac.r * (cy - y)) / total_height;
            uint32_t g1 = (cc.g * (y - ay) + ac.g * (cy - y)) / total_height;
            uint32_t b1 = (cc.b * (y - ay) + ac.b * (cy - y)) / total_height;

            uint32_t r2 = (bc.r * (y - ay) + ac.r * (by - y)) / segment_height;
            uint32_t g2 = (bc.g * (y - ay) + ac.g * (by - y)) / segment_height;
            uint32_t b2 = (bc.b * (y - ay) + ac.b * (by - y)) / segment_height;

            // Draw line from left to right
            int min, max;
            uint32_t minr, ming, minb, maxr, maxg, maxb;

            if (x1 < x2) {
                min = x1;
                max = x2;
                minr = r1;
                ming = g1;
                minb = b1;
                maxr = r2;
                maxg = g2;
                maxb = b2;
            } else {
                min = x2;
                max = x1;
                minr = r2;
                ming = g2;
                minb = b2;
                maxr = r1;
                maxg = g1;
                maxb = b1;
            }
            int line_length = max - min;
            for (int x = std::max(min, (int)drawing_area_top_left_x); x < std::min(max, (int)drawing_area_bot_right_x); x++) {
                uint32_t r = (maxr * (x - min) + minr * (max - x)) / line_length;
                uint32_t g = (maxg * (x - min) + ming * (max - x)) / line_length;
                uint32_t b = (maxb * (x - min) + minb * (max - x)) / line_length;

                Color c(r, g, b);
                writeToVRAM(x, y, c.to16Bit());
            }
        }
    }

    // Top half
    if (by != cy) {
        int segment_height = cy - by;
        for (int y = std::max(std::max(0, by), (int)drawing_area_top_left_y); y < std::min(std::min(512, cy), (int)drawing_area_bot_right_y); y++) { // Exclude last point
            int x1 = ax + ((cx - ax) * (y - ay)) / total_height;
            int x2 = bx + ((cx - bx) * (y - by)) / segment_height;

            uint32_t r1 = (cc.r * (y - ay) + ac.r * (cy - y)) / total_height;
            uint32_t g1 = (cc.g * (y - ay) + ac.g * (cy - y)) / total_height;
            uint32_t b1 = (cc.b * (y - ay) + ac.b * (cy - y)) / total_height;

            uint32_t r2 = (cc.r * (y - by) + bc.r * (cy - y)) / segment_height;
            uint32_t g2 = (cc.g * (y - by) + bc.g * (cy - y)) / segment_height;
            uint32_t b2 = (cc.b * (y - by) + bc.b * (cy - y)) / segment_height;


            // Draw line from left to right
            int min, max;
            uint32_t minr, ming, minb, maxr, maxg, maxb;

            if (x1 < x2) {
                min = x1;
                max = x2;
                minr = r1;
                ming = g1;
                minb = b1;
                maxr = r2;
                maxg = g2;
                maxb = b2;
            } else {
                min = x2;
                max = x1;
                minr = r2;
                ming = g2;
                minb = b2;
                maxr = r1;
                maxg = g1;
                maxb = b1;
            }
            int line_length = max - min;
            for (int x = std::max(min, (int)drawing_area_top_left_x); x < std::min(max, (int)drawing_area_bot_right_x); x++) {
                uint32_t r = (maxr * (x - min) + minr * (max - x)) / line_length;
                uint32_t g = (maxg * (x - min) + ming * (max - x)) / line_length;
                uint32_t b = (maxb * (x - min) + minb * (max - x)) / line_length;

                Color c(r, g, b);
                writeToVRAM(x, y, c.to16Bit());
            }
        }
    }
}

void SoftwareRenderer::drawTexturedTriangle(const TexturedTriangle &triangle) {
    LOGT_REND(std::format("drawTexturedTriangle({},{},{})", triangle.v1, triangle.v2, triangle.v3));
    //drawTexturedTriangle(triangle.v1.x + drawing_offset_x,
    //                     triangle.v1.y + drawing_offset_y,
    //                     triangle.v2.x + drawing_offset_x,
    //                     triangle.v2.y + drawing_offset_y,
    //                     triangle.v3.x + drawing_offset_x,
    //                     triangle.v3.y + drawing_offset_y,
    //                     triangle.tc1.x,
    //                     triangle.tc1.y,
    //                     triangle.tc2.x,
    //                     triangle.tc2.y,
    //                     triangle.tc3.x,
    //                     triangle.tc3.y,
    //                     triangle.texpage,
    //                     triangle.palette
    //                     );
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

void SoftwareRenderer::drawTexturedTriangle(int ax, int ay, int bx, int by, int cx, int cy, int tx1, int ty1, int tx2, int ty2, int tx3, int ty3, uint16_t texpage, uint16_t palette) {
    uint32_t xBase = (texpage & 0xF) * 64; // in halfwords
    uint32_t yBase = ((texpage >> 4) & 1) * 256; // in lines
    uint8_t semiTransparency = (texpage >> 5) & 3;
    uint8_t texturePageColors = (texpage >> 7) & 3;
    uint8_t textureDisable = (texpage >> 11) & 1;

    uint32_t xPalette = (palette & 0x3F) * 16; // in halfwords
    uint32_t yPalette = (palette >> 6) & 0x1FF; // in lines

    LOGT_REND(std::format("drawTexturedTriangle() texture: XBase[{:d}], YBase[{:d}], Semi Transparency[{:d}], Texture Page Colors[{:d}], Texture Disable[{:d}], XPalette[{:d}], YPalette[{:d}]", xBase, yBase, semiTransparency, texturePageColors, textureDisable, xPalette, yPalette));

    if (texturePageColors != 0) { // 0 means 4-bit colors
        LOG_REND(std::format("Texture format not implemented"));
        return;
    }

    // Sort the points by their y-coordinates
    if (ay > by) {
        std::swap(ax, bx);
        std::swap(ay, by);
        std::swap(tx1, tx2);
        std::swap(ty1, ty2);
    }
    if (ay > cy) {
        std::swap(ax, cx);
        std::swap(ay, cy);
        std::swap(tx1, tx3);
        std::swap(ty1, ty3);
    }
    if (by > cy) {
        std::swap(bx, cx);
        std::swap(by, cy);
        std::swap(tx2, tx3);
        std::swap(ty2, ty3);
    }
    // We now have ay <= by <= cy
    int total_height = cy - ay;

    // Bottom half
    if (ay != by) {
        int segment_height = by - ay;

        for (int y = std::max(std::max(0, ay), (int)drawing_area_top_left_y); y < std::min(std::min(512, by), (int)drawing_area_bot_right_y); y++) { // Exclude by
            int x1 = ax + ((cx - ax) * (y - ay)) / total_height;
            int x2 = ax + ((bx - ax) * (y - ay)) / segment_height;

            uint32_t txl = (tx3 * (y - ay) + tx1 * (cy - y)) / total_height;
            uint32_t tyl = (ty3 * (y - ay) + ty1 * (cy - y)) / total_height;

            uint32_t txr = (tx2 * (y - ay) + tx1 * (by - y)) / segment_height;
            uint32_t tyr = (ty2 * (y - ay) + ty1 * (by - y)) / segment_height;

            // Draw line from left to right
            int min, max;
            uint32_t mintx, minty, maxtx, maxty;

            if (x1 < x2) {
                min = x1;
                max = x2;
                mintx = txl;
                minty = tyl;
                maxtx = txr;
                maxty = tyr;
            } else {
                min = x2;
                max = x1;
                mintx = txr;
                minty = tyr;
                maxtx = txl;
                maxty = tyl;
            }
            int line_length = max - min;
            for (int x = std::max(min, (int)drawing_area_top_left_x); x < std::min(max, (int)drawing_area_bot_right_x); x++) {
                uint32_t tx = (maxtx * (x - min) + mintx * (max - x)) / line_length;
                uint32_t ty = (maxty * (x - min) + minty * (max - x)) / line_length;

                uint16_t halfword = readFromVRAM(xBase + (tx / 4), yBase + ty);
                uint8_t textureIndex = (halfword >> (4 *(tx % 4))) & 0xF;
                uint16_t color = readFromVRAM(xPalette + textureIndex, yPalette);

                if (color) { // nothing, not even semiTransparency set -> transparent
                    writeToVRAM(x, y, color & 0x7FFF); // set mask bit to 0 for now
                }
            }
        }
    }

    // Top half
    if (by != cy) {
        int segment_height = cy - by;
        for (int y = std::max(std::max(0, by), (int)drawing_area_top_left_y); y < std::min(std::min(512, cy), (int)drawing_area_bot_right_y); y++) { // Exclude last point
            int x1 = ax + ((cx - ax) * (y - ay)) / total_height;
            int x2 = bx + ((cx - bx) * (y - by)) / segment_height;

            uint32_t txl = (tx3 * (y - ay) + tx1 * (cy - y)) / total_height;
            uint32_t tyl = (ty3 * (y - ay) + ty1 * (cy - y)) / total_height;

            uint32_t txr = (tx3 * (y - by) + tx2 * (cy - y)) / segment_height;
            uint32_t tyr = (ty3 * (y - by) + ty2 * (cy - y)) / segment_height;


            // Draw line from left to right
            int min, max;
            uint32_t mintx, minty, maxtx, maxty;

            if (x1 < x2) {
                min = x1;
                max = x2;
                mintx = txl;
                minty = tyl;
                maxtx = txr;
                maxty = tyr;
            } else {
                min = x2;
                max = x1;
                mintx = txr;
                minty = tyr;
                maxtx = txl;
                maxty = tyl;
            }
            int line_length = max - min;
            for (int x = std::max(min, (int)drawing_area_top_left_x); x < std::min(max, (int)drawing_area_bot_right_x); x++) {
                uint32_t tx = (maxtx * (x - min) + mintx * (max - x)) / line_length;
                uint32_t ty = (maxty * (x - min) + minty * (max - x)) / line_length;

                uint16_t halfword = readFromVRAM(xBase + (tx / 4), yBase + ty);
                uint8_t textureIndex = (halfword >> (4 *(tx % 4))) & 0xF;
                uint16_t color = readFromVRAM(xPalette + textureIndex, yPalette);

                if (color) { // nothing, not even semiTransparency set -> transparent
                    writeToVRAM(x, y, color & 0x7FFF); // set mask bit to 0 for now
                }
            }
        }
    }
}

template<typename Point, typename Context>
void SoftwareRenderer::draw_triangle(Point a, Point b, Point c, Context context) {
    //LOGT_REND(std::format("draw_triangle({}, {}, {}, {})", a, b, c, context));

    if (!context.valid()) {
        LOG_REND(std::format("DrawingContext not valid: format not implemented?"));
        return;
    }

    // Sort the points by their y-coordinates
    if (a.y > b.y) {
        std::swap(a, b);
    }
    if (a.y > c.y) {
        std::swap(a, c);
    }
    if (b.y > c.y) {
        std::swap(b, c);
    }

    // We now have a.y <= b.y <= c.y
    int32_t total_height = c.y - a.y;

    // Bottom half of the triangle
    if (a.y != b.y) {
        int32_t segment_height = b.y - a.y;

        // Draw lines from a.y to b.y
        for (int32_t y = std::max(a.y, (int32_t)drawing_area_top_left_y); y < std::min(b.y, (int32_t)drawing_area_bot_right_y); y++) { // Exclude by
            // Determine x-coordinates on triangle borders at height y
            // x_ac by interpolating between a.x and c.x
            // x_ab by interpolating between a.x and b.x
            int32_t x_ac = a.x + ((c.x - a.x) * (y - a.y)) / total_height;
            int32_t x_ab = a.x + ((b.x - a.x) * (y - a.y)) / segment_height;

            // Determine color/texture coordinate for point (x_ac, y) and (x_ab, y)
            // (x_ac, y) by interpolating a and c along y
            // (x_ab, y) by interpolating a and b along y
            typename Point::Color c_ac = Point::Color::interpolate(c.c, y - a.y, a.c, c.y - y, total_height);
            typename Point::Color c_ab = Point::Color::interpolate(b.c, y - a.y, a.c, b.y - y, segment_height);

            // Draw line from left to right
            Point left = { x_ac, y, c_ac };
            Point right = { x_ab, y, c_ab };
            if (x_ab < x_ac) {
                std::swap(left, right);
            }

            int32_t line_length = right.x - left.x;
            for (int x = std::max(left.x, (int)drawing_area_top_left_x); x < std::min(right.x, (int)drawing_area_bot_right_x); x++) {
                typename Point::Color c = Point::Color::interpolate(right.c, x - left.x, left.c, right.x - x, line_length);
                draw_pixel(x, y, context, c);
            }
        }
    }

    // Top half of the triangle
    if (b.y != c.y) {
        int32_t segment_height = c.y - b.y;

        // Draw lines from b.y to c.y
        for (int32_t y = std::max(b.y, (int32_t)drawing_area_top_left_y); y < std::min(c.y, (int32_t)drawing_area_bot_right_y); y++) { // Exclude last point
            // Determine x-coordinates on triangle borders at height y
            // x_ac by interpolating between a.x and c.x
            // x_bc by interpolating between b.x and c.x
            int32_t x_ac = a.x + ((c.x - a.x) * (y - a.y)) / total_height;
            int32_t x_bc = b.x + ((c.x - b.x) * (y - b.y)) / segment_height;

            // Determine color/texture coordinate for point (x_ac, y) and (x_ab, y)
            // (x_ac, y) by interpolating a and c along y
            // (x_bc, y) by interpolating b and c along y
            typename Point::Color c_ac = Point::Color::interpolate(c.c, y - a.y, a.c, c.y - y, total_height);
            typename Point::Color c_bc = Point::Color::interpolate(c.c, y - b.y, b.c, c.y - y, segment_height);

            // Draw line from left to right
            Point left = { x_ac, y, c_ac };
            Point right = { x_bc, y, c_bc };
            if (x_bc < x_ac) {
                std::swap(left, right);
            }

            int32_t line_length = right.x - left.x;
            for (int32_t x = std::max(left.x, (int32_t)drawing_area_top_left_x); x < std::min(right.x, (int32_t)drawing_area_bot_right_x); x++) {
                typename Point::Color c = Point::Color::interpolate(right.c, x - left.x, left.c, right.x - x, line_length);
                draw_pixel(x, y, context, c);
            }
        }
    }
}

template void SoftwareRenderer::draw_triangle(TexturedPoint a, TexturedPoint b, TexturedPoint c, TextureContext context);

}

