#include <iostream>

#include "psx/core.h"

int main(int argc, char *argv[]) {
    PSX::Core core;
    core.readBIOS("BIOS/SCPH1001.BIN");

    core.step();

    return 0;
}
