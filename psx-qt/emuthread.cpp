#include "emuthread.h"

#include "psx/core.h"

EmuThread::EmuThread() {
}

EmuThread::~EmuThread() {
}

void EmuThread::run() {
    PSX::Core core;

    core.run();
}

