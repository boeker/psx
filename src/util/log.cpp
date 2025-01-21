#include "log.h"

#include <iostream>

namespace util {

bool Log::logEnabled = false;

void Log::log(const std::string &message, Type type) {
    switch (type) {
        case Type::CPU:
        case Type::MEMORY:
        case Type::REGISTER_READ:
        case Type::REGISTER_WRITE:
        case Type::CP0_REGISTER_READ:
        case Type::CP0_REGISTER_WRITE:
        case Type::REGISTER_PC_WRITE:
        case Type::MISC:
            if (logEnabled) {
                std::cerr << message;
            }
            break;
        case Type::REGISTER_PC_READ:
            break;
    }
}

}
