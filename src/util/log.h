#ifndef UTIL_LOG_H
#define UTIL_LOG_H

#include <string>

namespace util {

class Log {
public:
    enum Type {
        CPU,
        MEMORY,
        REGISTER_READ,
        REGISTER_WRITE,
        REGISTER_PC_READ,
        REGISTER_PC_WRITE,
        CP0_REGISTER_READ,
        CP0_REGISTER_WRITE,
        CDROM,
        GPU,
        GPU_WRITE,
        GPUSTAT,
        SPU,
        MDEC,
        DMA,
        TIMERS,
        PERIPHERAL,
        INTERRUPTS,
        EXCEPTION,
        MISC,
        WARNING,
    };

    static bool logEnabled;

    static void log(const std::string &message, Type type = MISC);
};

}

#endif
