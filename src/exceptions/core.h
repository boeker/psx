#ifndef EXCEPTIONS_CORE_H
#define EXCEPTIONS_CORE_H

#include <exception>
#include <stdexcept>
#include <string>

namespace exceptions {

class UnknownOpcodeError : public std::runtime_error {
public:
    explicit UnknownOpcodeError(const std::string &what)
        : std::runtime_error(what) {}
    explicit UnknownOpcodeError(const char *what)
        : std::runtime_error(what) {}
};
}

#endif
