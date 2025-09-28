#include "core.h"

#include <format>
#include <iostream>

#include "renderer/renderer.h"

namespace PSX {

Core::Core() {
    reset();
}

void Core::setRenderer(Renderer *renderer) {
    bus.gpu.setRenderer(renderer);
}

void Core::reset() {
    bus.reset();
}

void Core::emulateStep() {
    uint32_t cyclesTaken = bus.cpu.cycles;

    bus.cpu.step();

    cyclesTaken = bus.cpu.cycles - cyclesTaken;

    bus.gpu.catchUpToCPU(cyclesTaken);

    if (bus.cpu.cycles >= CPU_VBLANK_FREQUENCY) {
        bus.cpu.cycles -= CPU_VBLANK_FREQUENCY;

        bus.interrupts.notifyAboutVBLANK();
        bus.gpu.notifyAboutVBLANK();
    }
}

void Core::emulateBlock() {
    uint32_t cyclesTaken = bus.cpu.cycles;

    for (int i = 0; i < 10; ++i) {
        bus.cpu.step();
    }

    cyclesTaken = bus.cpu.cycles - cyclesTaken;

    bus.gpu.catchUpToCPU(cyclesTaken);
    bus.timers.catchUpToCPU(cyclesTaken);
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

