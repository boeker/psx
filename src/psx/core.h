#ifndef PSX_CORE_H
#define PSX_CORE_H

#include "bus.h"

namespace PSX {

class Core {
public:
    Bus bus;

public:
    Core();
    void reset(); 

    void step();
    void run();
};

}

#endif
