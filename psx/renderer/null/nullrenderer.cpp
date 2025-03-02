#include "nullrenderer.h"

#include <cstring>
#include <format>

#include "util/log.h"

using namespace util;

namespace PSX {

NullRenderer::NullRenderer() {
    vram = new uint8_t[VRAM_SIZE];
}

NullRenderer::~NullRenderer() {
    delete[] vram;
}

void NullRenderer::reset() {
    std::memset(vram, 0, VRAM_SIZE);
}

void NullRenderer::clear() {
}

void NullRenderer::swapBuffers() {
}

void NullRenderer::drawTriangle(const Triangle &t) {
}

void NullRenderer::writeToVRAM(uint32_t line, uint32_t pos, uint16_t value) {
    LOGT_REND(std::format("VRAM write 0x{:04X} -> line {:d}, position {:d}",
                              value, line, pos));

    uint16_t *vramLine = (uint16_t*)&(vram[512 * line]);
    vramLine[pos] = value;
}

uint16_t NullRenderer::readFromVRAM(uint32_t line, uint32_t pos) {
    uint16_t *vramLine = (uint16_t*)&(vram[512 * line]);
    uint16_t value = vramLine[pos];

    LOGT_REND(std::format("VRAM read line {:d}, position {:d} -> 0x{:04X}",
                              line, pos, value));

    return value;
}


}

