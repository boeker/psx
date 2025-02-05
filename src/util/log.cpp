#include "log.h"

#include <iostream>

namespace util {

bool Log::logEnabled = false;
bool Log::busLogEnabled = false;

void Log::log(const std::string &message, Type type) {
    switch (type) {
        case Type::WARNING:
            std::cerr << "Warning: " << message << std::endl;
            break;
        case Type::DMA:
        //case Type::DMA_WRITE:
            std::cerr << "[DMA] " << message << std::endl;
            break;
        case Type::GPU:
        //case Type::GPU_IO:
        case Type::GPU_VRAM:
            std::cerr << "[GPU] " <<  message << std::endl;;
            break;
        case Type::INTERRUPTS:
            std::cerr << "[INT] " <<  message << std::endl;;
            break;
        case Type::EXCEPTION:
            std::cerr <<  "[EXC] " << message << std::endl;
            break;
        case Type::CDROM:
        case Type::MDEC:
        case Type::PERIPHERAL:
        case Type::TIMERS:
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
        case Type::BUS:
            if (busLogEnabled) {
                std::cerr << message << std::endl;
            }
        default:
            break;
    }
}

}
