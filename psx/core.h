#ifndef PSX_CORE_H
#define PSX_CORE_H

#include "bus.h"

namespace PSX {

class Renderer;

class Core {
public:
    Bus bus;

public:
    Core();
    void reset();

    void setRenderer(Renderer *renderer);
    void emulateStep();
    void emulateBlock();
    void emulateUntilVBLANK();
    void run();
};

}

#endif
