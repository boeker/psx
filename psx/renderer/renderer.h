#ifndef PSX_RENDERER_RENDERER_H
#define PSX_RENDERER_RENDERER_H

#include <format>

#include <util/bit.h>

using namespace util;

namespace PSX {

struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;

    Color(uint32_t color) {
        r = color & 0x000000FF;
        g = (color >> 8) & 0x000000FF;
        b = (color >> 16) & 0x000000FF;
    }

    Color(uint8_t r, uint8_t g, uint8_t b)
        : r(r), g(g), b(b) {
    }

    uint16_t to16Bit() const {
        return ((r >> 3) << 0)
               | ((g >> 3) << 5)
               | ((b >> 3) << 10);
    }
};

struct TextureCoordinate {
    uint8_t x;
    uint8_t y;

    TextureCoordinate(uint32_t coordinate) {
        x = coordinate & 0xFF;
        y = (coordinate >> 8) & 0xFF;
    }
};

struct Vertex {
    int16_t x;
    int16_t y;

    Vertex(int16_t x, int16_t y)
        : x(x), y(y) {
    }

    Vertex(uint32_t vertex) {
        x = Bit::extendSign11To16(vertex & 0x000007FF);
        y = Bit::extendSign11To16((vertex >> 16) & 0x000007FF);
    }
};

struct Triangle {
    Vertex v1;
    Color c1;
    Vertex v2;
    Color c2;
    Vertex v3;
    Color c3;

    Triangle(Vertex v1, Color c1, Vertex v2, Color c2, Vertex v3, Color c3)
        : v1(v1), c1(c1), v2(v2), c2(c2), v3(v3), c3(c3) {
    }
};

struct TexturedTriangle {
    Color c;
    Vertex v1;
    TextureCoordinate tc1;
    Vertex v2;
    TextureCoordinate tc2;
    Vertex v3;
    TextureCoordinate tc3;

    uint16_t texpage;
    uint16_t palette;

    TexturedTriangle(Color c, Vertex v1, TextureCoordinate tc1, Vertex v2, TextureCoordinate tc2, Vertex v3, TextureCoordinate tc3, uint16_t texpage, uint16_t palette)
        : c(c), v1(v1), tc1(tc1), v2(v2), tc2(tc2), v3(v3), tc3(tc3), texpage(texpage), palette(palette) {
    }
};

class Renderer {
public:
    Renderer() = default;
    Renderer(const Renderer &) = delete;
    virtual ~Renderer() = default;

    virtual void reset() = 0;
    virtual void clear() = 0;
    virtual void swapBuffers() = 0;
    virtual void drawTriangle(const Triangle &triangle) = 0;
    virtual void loadTexture(uint8_t *textureData) = 0;
    virtual void drawTexturedTriangle(const TexturedTriangle &triangle) = 0;

    //virtual void prepareWriteToVRAM(uint32_t line, uint32_t pos, uint32_t width, uint32_t height);
    virtual void writeToVRAM(uint32_t line, uint32_t pos, uint16_t value) = 0;
    virtual void writeToVRAM(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint8_t *data) = 0;

    virtual uint16_t readFromVRAM(uint32_t line, uint32_t pos) = 0;
    virtual void prepareReadFromVRAM(uint32_t line, uint32_t pos, uint32_t width, uint32_t height) = 0;

    virtual void fillRectangleInVRAM(const Color &c, uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;

    virtual void setDrawingAreaTopLeft(uint32_t x, uint32_t y) = 0;
    virtual void setDrawingAreaBottomRight(uint32_t x, uint32_t y) = 0;
};

}

template <>
struct std::formatter<PSX::Color> : std::formatter<string_view> {
    auto format(const PSX::Color& c, std::format_context& ctx) const {
        std::string temp;
        std::format_to(std::back_inserter(temp), "({}, {}, {})", c.r, c.g, c.b);
        return std::formatter<string_view>::format(temp, ctx);
    }
};

template <>
struct std::formatter<PSX::Vertex> : std::formatter<string_view> {
    auto format(const PSX::Vertex& v, std::format_context& ctx) const {
        std::string temp;
        std::format_to(std::back_inserter(temp), "({}, {})", v.x, v.y);
        return std::formatter<string_view>::format(temp, ctx);
    }
};

template <>
struct std::formatter<PSX::TextureCoordinate> : std::formatter<string_view> {
    auto format(const PSX::TextureCoordinate& tc, std::format_context& ctx) const {
        std::string temp;
        std::format_to(std::back_inserter(temp), "({}, {})", tc.x, tc.y);
        return std::formatter<string_view>::format(temp, ctx);
    }
};

#endif

