#include "core.h"

#include <format>
#include <iostream>

#include "renderer/renderer.h"

namespace PSX {

Core::Core(Renderer *renderer)
    : renderer(renderer) {
    bus.gpu.setRenderer(renderer);
    reset();
}

void Core::reset() {
    bus.reset();

    renderer->clear();
    renderer->swapBuffers();
}

void Core::emulateBlock() {
    uint32_t cyclesTaken = bus.cpu.cycles;

    for (int i = 0; i < 10; ++i) {
        bus.cpu.step();
    }

    cyclesTaken = bus.cpu.cycles - cyclesTaken;

    bus.gpu.catchUpToCPU(cyclesTaken);
}

void Core::emulateUntilVBLANK() {
    while (true) {
        emulateBlock();

        if (bus.cpu.cycles >= CPU_VBLANK_FREQUENCY) {
            bus.cpu.cycles -= CPU_VBLANK_FREQUENCY;

            bus.interrupts.notifyAboutVBLANK();
            bus.gpu.notifyAboutVBLANK();

            break;
        }
    }
}

void Core::run() {
    try {
        while (true) {
            emulateUntilVBLANK();
        }

    } catch (const std::runtime_error &e) {
        std::cout << std::endl;
        std::cout << "Execution halted at exception: " << e.what() << std::endl;
        std::cout << bus << std::endl;
    }
}

}

