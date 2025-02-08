#include "core.h"

#include <format>
#include <iostream>

namespace PSX {

Core::Core() {
    reset();

    bus.bios.readFromFile("BIOS/SCPH1001.BIN");
}

void Core::reset() {
    bus.reset();
}

void Core::emulateBlock() {
    uint32_t cyclesTaken = bus.cpu.cycles;

    for (int i = 0; i < 10; ++i) {
        bus.cpu.step();
    }

    cyclesTaken = bus.cpu.cycles - cyclesTaken;

    bus.gpu.catchUpToCPU(cyclesTaken);

    if (bus.cpu.cycles >= CPU_VBLANK_FREQUENCY) {
        bus.cpu.cycles -= CPU_VBLANK_FREQUENCY;

        bus.interrupts.notifyAboutVBLANK();
    }
}

void Core::run() {
    try {
        while (true) {
            emulateBlock();
        }

    } catch (const std::runtime_error &e) {
        std::cout << std::endl;
        std::cout << "Execution halted at exception: " << e.what() << std::endl;
        std::cout << bus << std::endl;
    }
}

}

