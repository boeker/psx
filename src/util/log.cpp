#include "log.h"

#include <iostream>

namespace util {

bool Log::logEnabled = false;

void Log::log(const std::string &message, Type type) {
    switch (type) {
        case Type::WARNING:
            std::cerr <<  message << "!" << std::endl;
            break;
        case Type::CPU:
        case Type::MEMORY:
        case Type::REGISTER_READ:
        case Type::REGISTER_WRITE:
        case Type::CP0_REGISTER_READ:
        case Type::CP0_REGISTER_WRITE:
        case Type::REGISTER_PC_WRITE:
        case Type::REGISTER_PC_READ:
        case Type::SPU:
        case Type::MISC:
        case Type::GPU:
        case Type::CDROM:
        case Type::MDEC:
        case Type::DMA:
        case Type::TIMER:
        case Type::PERIPHERAL:
        case Type::INTERRUPT:
            //if (logEnabled) {
            //    std::cerr << message;
            //}
            break;
    }
}

}
