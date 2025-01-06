#include "memory.h"

namespace exceptions {

FileReadError::FileReadError(const std::string &what)
    : runtime_error(what) {
}

FileReadError::FileReadError(const char *what)
    : runtime_error(what) {
}
}
