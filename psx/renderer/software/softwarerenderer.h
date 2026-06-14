#ifndef PSX_RENDERER_SOFTWARERENDERER_H
#define PSX_RENDERER_SOFTWARERENDERER_H

#include <cstdint>
#include <vector>

#include "renderer/renderer.h"

namespace PSX {

#define VRAM_SIZE (1024 * 1024)

class Screen;

class SoftwareRenderer : public Renderer {
public:
    SoftwareRenderer(Screen *screen, Screen *vramViewer);
    virtual ~SoftwareRenderer();

    void initialize();

    void installVRAMViewer(Screen *vramViewer);

    void reset() override;
    void clear() override;
    void computeViewport();
    void computeVRAMViewport();
    void swapBuffers() override;

    void writeToVRAM(uint32_t line, uint32_t pos, uint16_t value) override;
    uint16_t readFromVRAM(uint32_t line, uint32_t pos) override;
    void fillRectangleInVRAM(const Color &c, uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;

    void set_drawing_area(uint32_t top_left_x, uint32_t top_left_y, uint32_t bot_right_x, uint32_t bot_right_y) override;
    void set_drawing_offset(int32_t x, int32_t y) override;
    void set_display_area(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;

    void drawTriangle(const Triangle &triangle) override;
    void drawTexturedTriangle(const TexturedTriangle &triangle) override;

private:
    void drawTriangle(int ax, int ay, int bx, int by, int cx, int cy, Color ac, Color bc, Color cc);
    void drawTexturedTriangle(int ax, int ay, int bx, int by, int cx, int cy, int tx1, int ty1, int tx2, int ty2, int tx3, int ty3, uint16_t texpage, uint16_t palette);

    struct ColoredPoint {
        int32_t x;
        int32_t y;
        Color c;
    };

    struct TextureCoordinate {
        int32_t x;
        int32_t y;
        static TextureCoordinate interpolate(const TextureCoordinate &a, int32_t a_fact, const TextureCoordinate &b, int32_t b_fact, int32_t divisor) {
            return {
                (a.x * a_fact + b.x * b_fact) / divisor,
                (a.y * a_fact + b.y * b_fact) / divisor
            };
        }
    };

    struct TexturedPoint {
        using Color = TextureCoordinate;
        int32_t x;
        int32_t y;
        TextureCoordinate c;
    };

    struct TextureContext {
        uint32_t xBase;
        uint32_t yBase;
        uint8_t semiTransparency;
        uint8_t texturePageColors;
        uint8_t textureDisable;

        uint32_t xPalette;
        uint32_t yPalette;

        TextureContext(uint16_t texpage, uint16_t palette) {
            xBase = (texpage & 0xF) * 64; // in halfwords
            yBase = ((texpage >> 4) & 1) * 256; // in lines
            semiTransparency = (texpage >> 5) & 3;
            texturePageColors = (texpage >> 7) & 3;
            textureDisable = (texpage >> 11) & 1;

            xPalette = (palette & 0x3F) * 16; // in halfwords
            yPalette = (palette >> 6) & 0x1FF; // in lines
        }

        bool valid() const {
            return texturePageColors == 0; // 4-bit colors, everything else not implemented yet
        }
    };

    template<typename Point, typename Context>
    void draw_triangle(Point a, Point b, Point c, Context context);

    void draw_pixel(int32_t x, int32_t y, const TextureContext &context, const TextureCoordinate &c) {
        uint16_t halfword = readFromVRAM(context.xBase + (c.x / 4), context.yBase + c.y);
        uint8_t textureIndex = (halfword >> (4 * (c.x % 4))) & 0xF;
        uint16_t color = readFromVRAM(context.xPalette + textureIndex, context.yPalette);

        if (color) { // nothing, not even semiTransparency set -> transparent
            writeToVRAM(x, y, color & 0x7FFF); // set mask bit to 0 for now
        }
    }

private:
    Screen *screen;
    Screen *vramViewer;
    uint8_t *vram;

    unsigned int vramFramebuffer;
    unsigned int vramTexture;

    int viewportX, viewportY;
    int viewportWidth, viewportHeight;
    int vramViewportX, vramViewportY;
    int vramViewportWidth, vramViewportHeight;

    uint32_t drawing_area_top_left_x;
    uint32_t drawing_area_top_left_y;
    uint32_t drawing_area_bot_right_x;
    uint32_t drawing_area_bot_right_y;

    int32_t drawing_offset_x;
    int32_t drawing_offset_y;

    uint32_t display_area_top_left_x;
    uint32_t display_area_top_left_y;
    uint32_t display_area_width;
    uint32_t display_area_height;
};

}

#endif

