#include <iostream>

#include "psx/core.h"

int main(int argc, char *argv[]) {
    PSX::Core core;
    core.readBIOS("BIOS/SCPH1001.BIN");

    try {
        while (true) {
            core.step();
        }
    } catch (const std::runtime_error &e) {
        std::cout << std::endl;
        std::cout << "Excecution halted at exception: " << e.what() << std::endl;
        std::cout << core << std::endl;
    }

    return 0;
}
