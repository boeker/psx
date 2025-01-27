#include "log.h"

#include <iostream>

namespace util {

bool Log::logEnabled = false;

void Log::log(const std::string &message, Type type) {
    switch (type) {
        case Type::WARNING:
            std::cerr <<  message << "!" << std::endl;
            break;
        case Type::DMA:
        case Type::TIMERS:
        case Type::INTERRUPTS:
        case Type::CDROM:
        case Type::MDEC:
        case Type::PERIPHERAL:
        case Type::EXCEPTION:
            std::cerr <<  message << std::endl;
            break;
        case Type::GPU:
            std::cerr <<  message << std::endl;
            break;
        case Type::SPU:
        case Type::CPU:
        case Type::MEMORY:
        case Type::REGISTER_READ:
        case Type::REGISTER_WRITE:
        case Type::CP0_REGISTER_READ:
        case Type::CP0_REGISTER_WRITE:
        case Type::REGISTER_PC_WRITE:
        case Type::REGISTER_PC_READ:
        case Type::MISC:
            //if (logEnabled) {
            //    std::cerr << message;
            //}
            break;
    }
}

}
