#include "log.h"

#include <iostream>

namespace util {

void Log::log(const std::string &message, Type type) {
    switch (type) {
        case Type::CPU:
        case Type::MEMORY:
        case Type::MISC:
            std::cerr << message;
            break;
    }
}

}
