#ifndef PSX_CORE_H
#define PSX_CORE_H

#include "bus.h"

namespace PSX {

class Renderer;

class Core {
public:
    Bus bus;

public:
    Core(Renderer *renderer);
    void reset();

    void emulateBlock();
    void run();

private:
    Renderer *renderer;
};

}

#endif
