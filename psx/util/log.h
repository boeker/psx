#ifndef UTIL_LOG_H
#define UTIL_LOG_H

#include <string>

namespace util {

class Log {
public:
    enum Type {
        BUS,
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
        GPU_IO,
        GPU_VRAM,
        SPU,
        MDEC,
        DMA,
        DMA_WRITE,
        DMA_IO,
        TIMERS,
        PERIPHERAL,
        INTERRUPTS,
        INTERRUPTS_IO,
        EXCEPTION,
        EXCEPTION_VERBOSE,
        MISC,
        WARNING,
    };

    static bool logEnabled;
    static bool busLogEnabled;

    static void log(const std::string &message, Type type = MISC);
};

}

#endif
