#include <iostream>

#include "psx/core.h"

int main(int argc, char *argv[]) {
    PSX::Core core;
    core.readBIOS("BIOS/SCPH1001.BIN");

    while (true) {
        core.step();
    }

    return 0;
}
