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

void Core::step() {
    bus.cpu.step();
}

void Core::run() {
    try {
        while (true) {
            step();
        }

    } catch (const std::runtime_error &e) {
        std::cout << std::endl;
        std::cout << "Excecution halted at exception: " << e.what() << std::endl;
        std::cout << bus << std::endl;
    }
}

}

