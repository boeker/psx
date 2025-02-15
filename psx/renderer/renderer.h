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
};

struct Vertex {
    int16_t x;
    int16_t y;

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

class Renderer {
public:
    Renderer() = default;
    Renderer(const Renderer &) = delete;
    virtual ~Renderer() = default;

    virtual void clear() = 0;
    virtual void drawTriangle(const Triangle &triangle) = 0;
    virtual void swapBuffers() = 0;
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

#endif

